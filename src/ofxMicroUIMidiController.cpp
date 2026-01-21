#include "ofxMicroUIMidiController.h"

ofxMicroUIMidiController::ofxMicroUIMidiController(ofxMicroUISoftware * _soft, string device) : soft(_soft) {
	set(device);
	setUI(*_soft->_ui);
	ofAddListener(_u->uiEventMaster, this, &ofxMicroUIMidiController::uiEventMaster);
	ofAddListener(ofEvents().update, this, &ofxMicroUIMidiController::onUpdate);
}

void ofxMicroUIMidiController::onUpdate(ofEventArgs &data) {
	// if (!empty(changePreset)) {
	// 	_u->loadPreset(changePreset);
	// 	changePreset.clear();
	// }
	//
	// Process ALL pending presets from MIDI thread
//	std::string preset;
//	while(presetChannel.tryReceive(preset)){
//		_u->loadPreset(preset);
//	}

 	ofxMidiMessage msg;
    while(threadMidiMessage.tryReceive(msg)) {
    	parseMidiMessage(msg);
    }
}
void ofxMicroUIMidiController::newMidiMessage(ofxMidiMessage& msg) {
	threadMidiMessage.send(msg); // Non-blocking, copies the message
}

void ofxMicroUIMidiController::parseMidiMessage(ofxMidiMessage& msg) {
//        cout << "newMidiMessage " << endl;
	frameAction = ofGetFrameNum();
//        cout << frameAction << endl;
//        bool debug = true;
	bool debug = false;
	if (debug) {
		cout << "channel:" +ofToString(msg.channel) << endl;
		cout << "pitch:"   +ofToString(msg.pitch) << endl;
		cout << "control:" +ofToString(msg.control) << endl;
		cout << "status:"  +ofToString(msg.status) << endl;
		cout << "-------" << endl;
	}

	string index = ofToString(msg.channel) + " " + ofToString(msg.pitch) + " " + ofToString(msg.control);
	string index2 = index + " " + ofToString(msg.status);

	if ( midiControllerMap.find(index) != midiControllerMap.end()) {
		// action
		elementListMidiController *te = &midiControllerMap[index];
		ofxMicroUI * _ui { te->ui == "master" ? _u :  &_u->uis[te->ui] };

		// aqui apenas os controles que somente acontecem no note on
		if (msg.status == 144) {
			if (te->tipo == "bool") {
				if (_ui->getToggle(te->nome) != NULL) {
					_ui->getToggle(te->nome)->flip();
					if (_ui->pBool[te->nome]) {
						sendNote(msg.channel, msg.pitch, 3);
					} else {
						sendNote(msg.channel, msg.pitch, 0);
					}
				}
			}

			else if (te->tipo == "preset") {
				if (lastPresetPitch >= 0) {
					sendNote(lastPresetChannel, lastPresetPitch, 0); // 1 green 3 red
				}
				cout << "midi preset " << msg.pitch << endl;
				sendNote(msg.channel, msg.pitch, 1); // 1 green 3 red 5 yellow
//				_u->getElement("presets")->set(te->nome);
//				_u->loadPreset(te->nome);
//				soft->changePresetChannel.send(te->nome);
				soft->loadPreset(te->nome);

				lastPresetChannel = msg.channel;
				lastPresetPitch = msg.pitch;
			}

			else if (te->tipo == "bang") {
				cout << "BANG! " << te->nome << endl;
				ofxMicroUI::booleano * e = _ui->getToggle(te->nome);
				if (e != NULL) {
					e->set(true);
				}
//					ofxMicroUI::bang * e = ((ofxMicroUI::bang*)_ui->getElement(te->nome));
//					if (e != NULL) {
//						e->bang();
////						e->set(true);
//					}
			}
		}

		if (te->tipo == "radio") {
			if (te->valor == "") {
				ofxMicroUI::radio * r = _ui->getRadio(te->nome);
				int nElements = r->elements.size();
				int valor = ofMap(msg.value, 0, 127, 0, nElements);
				r->set(valor);
			}
			else {
				//_ui->futureCommands.push_back(future(te->ui, te->nome, "radioSet", te->valor));
			}
		}

		else if (te->tipo == "float" || te->tipo == "int") {
			ofxMicroUI::slider * s = (ofxMicroUI::slider*)_ui->getElement(te->nome);
			if (s != NULL) {
				float valor = ofMap(msg.value, 0, 127, s->min, s->max);
				s->set(valor);
			} else {
				cout << te->nome << endl;
				cout << "NULL" << endl;
			}
		}

		else if (te->tipo == "hold") {
			ofxMicroUI::hold * e = (ofxMicroUI::hold*)_ui->getElement(te->nome);
			if (e != NULL) {
				e->set(msg.status == 144);
				sendNote(msg.channel, msg.pitch, msg.status == 144 ? 1 : 0);
			}
		}


		else if (te->tipo == "savePresetNumber") {
			if (_u != NULL) {
				cout << te->tipo << endl;
//				holdPresetNumber = _u->getPresetNumber();
			}
		}

		else if (te->tipo == "restorePresetNumber") {
			if (_u != NULL) {
				cout << te->tipo << endl;
//				_u->futureCommands.push_back(future("master", "presets", "loadAllPresets", holdPresetNumber));
			}
		}

		// REMOVER?
		else if (te->tipo == "presetHold") {
			if (_u != NULL) {
				sendNote(lastPresetChannel, lastPresetPitch, 0); // 1 green 3 red 5 yellow
//				_u->futureCommands.push_back(future("master", "presets", "loadPresetHold", ofToInt(te->nome)));
//				_u->nextPreset.push_back(ofToInt(te->nome));
				sendNote(msg.channel, msg.pitch, 3); // 1 green 3 red 5 yellow
				lastPresetChannel = msg.channel;
				lastPresetPitch = msg.pitch;
				//http://community.akaipro.com/akai_professional/topics/midi-information-for-apc-mini
				//127 = verde
			}
		}

		// REMOVER?
		else if (te->tipo == "presetRelease") {
			if (_u != NULL) {
				// TODO XAXA
				// 1 green 3 red 5 yellow
				sendNote(lastPresetChannel, lastPresetPitch, 0);

//				_u->futureCommands.push_back(future("master", "presets", "loadPresetRelease", ofToInt(te->nome)));
				//_u->nextPreset.push_back(ofToInt(te->nome));
				sendNote(msg.channel, msg.pitch, 3); // 1 green 3 red 5 yellow
				lastPresetChannel = msg.channel;
				lastPresetPitch = msg.pitch;
			}
		}
	} else {
		// discard note off ?
		if (msg.status != 128) {
			cout << index << "\t\t" << index2 << endl;
		}
	}
	midiKeys[msg.pitch] = msg.status == 144;
	midiMessage = msg;
}



void ofxMicroUIMidiController::set(const string & midiDevice) {
	connected = midiControllerIn.openPort(midiDevice);
	cout << "ofxMicroUIMidiController setup :: " + midiDevice + " :: ";
	cout << (connected ? "connected" : "not found") << endl;
	if (connected) {
		midiControllerOut.openPort(midiDevice); // by number
		midiControllerIn.ignoreTypes(false, false, false);
	//	ofxMidi::setConnectionListener(this);
		midiControllerIn.addListener(this);

		string fileName { folder + midiDevice + ".txt" };

		if (fs::exists(ofToDataPath(fileName)) && midiControllerIn.isOpen()) {
			for (auto & m : ofxMicroUI::textToVector(fileName)) {
				if (m != "" && m.substr(0,1) != "#") {
					elementListMidiController te;
					vector <string> cols = ofSplitString(m, "\t");
					te.ui 	= cols[1];
					te.tipo = cols[2];
					te.nome = cols[3];
					if (cols.size() > 4) {
						te.valor = cols[4];
					}
					vector <string> vals = ofSplitString(cols[0], " ");
					int channel = ofToInt(vals[0]);
					int pitch 	= ofToInt(vals[1]);
					te.channel = channel;
					te.pitch = pitch;

					// isso aqui da igual a index = cols[0]
					string index = cols[0];


					midiControllerMap[index] = te;
					elements.push_back(&midiControllerMap[index]);
				}
			}
		}

		ofAddListener(ofEvents().exit, this, &ofxMicroUIMidiController::onExit);
	}
}

void ofxMicroUIMidiController::checkElement(const ofxMicroUI::element & e) {
	if (e.name == "presets" && e._ui->uiName == "master") {
//		cout << "OWWW presets and master" << endl;
//		for (auto & m : midiControllerMap) {
//			if (m.second.nome == e._ui->pString["presets"]) {
//				if (lastPresetChannel != 0 || lastPresetPitch != 0) {
//					sendNote(lastPresetChannel, lastPresetPitch, 0); // 1 green 3 red
//				}
//				sendNote(m.second.channel, m.second.pitch, 1); // 1 green 3 red
//				lastPresetChannel = m.second.channel;
//				lastPresetPitch = m.second.pitch;
//			}
//		}
	}

//	for (auto & m : midiControllerMap) {
//		if (m.second.nome == e.name && m.second.ui == e._ui->uiName) {
//			if (m.second.tipo == "bool") {
//				if (e._ui->pBool[e.name]) { // *e.b
//					sendNote(m.second.channel, m.second.pitch, 5);
//				} else {
//					sendNote(m.second.channel, m.second.pitch, 0);
//				}
//			}
//		}
//	}
}

void ofxMicroUIMidiController::uiEvent(ofxMicroUI::element & e) {
	if (frameAction != ofGetFrameNum()) {
		checkElement(e);
	}
}

void ofxMicroUIMidiController::uiEventMaster(string & s) {
	if (s == "setup") {
		frameAction = ofGetFrameNum();
		for (auto & e : _u->elements) {
			checkElement(*e);
		}
		for (auto & u : _u->uis) {
			for (auto & e : u.second.elements) {
				checkElement(*e);
			}
		}
	}
}


//ofxMicroUIMidiController::ofxMicroUIMidiController() {}



//#endif
