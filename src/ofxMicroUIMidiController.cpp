#include "ofxMicroUIMidiController.h"

ofxMicroUIMidiController::ofxMicroUIMidiController(ofxMicroUISoftware * _soft, string device) : soft(_soft) {
	set(device);
	setUI(*_soft->_ui);
	ofAddListener(_u->uiEventMaster, this, &ofxMicroUIMidiController::uiEventMaster);
	ofAddListener(ofEvents().update, this, &ofxMicroUIMidiController::onUpdate);
}

void ofxMicroUIMidiController::onUpdate(ofEventArgs & /*data*/) {
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
    	parseMidiMessageWithLearn(msg);
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

	// For CC messages, use control number as the identifier (same as saving)
	// For Note messages, use pitch (note number)
	int midiId = (msg.status == 176) ? msg.control : msg.pitch;
	string index = getMappingKey(msg.channel, midiId, 0);
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
				int nElements = (int)r->elements.size();
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
		
		else if (te->tipo == "bool_fader") {
			// Boolean controlled by fader with threshold (0-63 = false, 64-127 = true)
			ofxMicroUI::toggle * t = _ui->getToggle(te->nome);
			if (t != NULL) {
				bool newValue = msg.value >= 64;  // Threshold at midpoint
				if (t->getVal() != newValue) {
					t->set(newValue);
				}
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
		// Unmapped MIDI message - ignore
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

		// Old .txt format - commented out, using XML only now
		/*
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
					string index = cols[0];
					midiControllerMap[index] = te;
					elements.push_back(&midiControllerMap[index]);
				}
			}
		}
		*/

		// Load XML mappings (new format)
		loadMappingsFromXml();
		
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

// ============================================================
// MIDI LEARN IMPLEMENTATION
// ============================================================

void ofxMicroUIMidiController::toggleLearnMode() {
	setLearnMode(!learnMode);
}

void ofxMicroUIMidiController::setLearnMode(bool active) {
	if (active == learnMode) return;
	
	learnMode = active;
	
	if (learnMode) {
		// Entering learn mode - backup current mappings
		mappingsBackup = midiControllerMap;
		learnElement = nullptr;
		justMappedElement = nullptr;
		
		// Register draw and input events
		ofAddListener(ofEvents().draw, this, &ofxMicroUIMidiController::onDraw);
		ofAddListener(ofEvents().keyPressed, this, &ofxMicroUIMidiController::onKeyPressed);
		ofAddListener(ofEvents().mousePressed, this, &ofxMicroUIMidiController::onMousePressed);
		
		// Clear any stale clicked elements from before learn mode
		_u->_lastClickedElement = nullptr;
		for (auto& ui : _u->uis) {
			ui.second._lastClickedElement = nullptr;
		}
		
		cout << "MIDI Learn Mode: ON" << endl;
	} else {
		// Exiting learn mode - unregister events
		ofRemoveListener(ofEvents().draw, this, &ofxMicroUIMidiController::onDraw);
		ofRemoveListener(ofEvents().keyPressed, this, &ofxMicroUIMidiController::onKeyPressed);
		ofRemoveListener(ofEvents().mousePressed, this, &ofxMicroUIMidiController::onMousePressed);
		
		learnElement = nullptr;
		justMappedElement = nullptr;
		
		// Save by default when exiting (TAB key behavior)
		saveMappingsToXml();
		cout << "MIDI Learn: Saved and exited" << endl;
	}
}

void ofxMicroUIMidiController::onDraw(ofEventArgs& args) {
	if (!learnMode) return;
	drawLearnModeOverlay();
}

void ofxMicroUIMidiController::onKeyPressed(ofKeyEventArgs& args) {
	if (!learnMode) return;
	
	if (args.key == OF_KEY_RETURN) {
		// Save and exit
		saveMappingsToXml();
		setLearnMode(false);
		cout << "MIDI Learn: Saved and exited" << endl;
	}
	else if (args.key == OF_KEY_ESC) {
		// Cancel and restore backup
		cancelLearnMode();
		cout << "MIDI Learn: Cancelled" << endl;
	}
	else if (args.key == '0') {
		// Clear all mappings
		int count = midiControllerMap.size();
		midiControllerMap.clear();
		justMappedElement = nullptr;
		learnElement = nullptr;
		cout << "MIDI Learn: Cleared all " << count << " mappings" << endl;
	}
}

void ofxMicroUIMidiController::onMousePressed(ofMouseEventArgs& args) {
	if (!learnMode) return;
	
	// Find element under mouse across all UIs
	ofxMicroUI::element* clickedElement = nullptr;
	string clickedUIName;
	
	// Check master UI (with offset)
	for (auto& e : _u->elements) {
		ofRectangle screenRect = getElementScreenRect(e);
		if (screenRect.inside(args.x, args.y)) {
			clickedElement = e;
			clickedUIName = _u->uiName;
			break;
		}
	}
	
	// Check sub-UIs (with offset)
	if (!clickedElement) {
		for (auto& ui : _u->uis) {
			for (auto& e : ui.second.elements) {
				ofRectangle screenRect = getElementScreenRect(e);
				if (screenRect.inside(args.x, args.y)) {
					clickedElement = e;
					clickedUIName = ui.second.uiName;
					break;
				}
			}
			if (clickedElement) break;
		}
	}
	
	// Right-click to remove mapping
	if (args.button == 2) {
		if (clickedElement) {
			string key;
			if (isElementMapped(clickedElement, key)) {
				midiControllerMap.erase(key);
				cout << "MIDI Learn: Removed mapping for " << clickedUIName << "/" << clickedElement->name << endl;
			}
		}
	}
	// Left-click on element or empty space
	else if (args.button == 0) {
		if (clickedElement) {
			// Clicked on a new element - clear the "just mapped" green highlight
			// The actual learning will start via _lastClickedElement in update()
			justMappedElement = nullptr;
		}
		else if (learnElement != nullptr) {
			// Clicked on empty space - deselect
			learnElement = nullptr;
			learnUIName = "";
			justMappedElement = nullptr;
			cout << "MIDI Learn: Deselected element" << endl;
		}
	}
}

void ofxMicroUIMidiController::startLearning(ofxMicroUI::element* e, const string& uiName) {
	learnElement = e;
	learnUIName = uiName;
	currentLearnChannel = -1;
	currentLearnPitch = -1;
	currentLearnControl = -1;
	
	// Clear previous "just mapped" highlight when selecting new element
	justMappedElement = nullptr;
	
	cout << "MIDI Learn: Waiting for MIDI input to map '" << e->name << "' (" << uiName << ")" << endl;
}

void ofxMicroUIMidiController::finishLearning(const ofxMidiMessage& msg) {
	if (!learnElement) return;
	
	// For CC messages, use control number as the identifier
	// For Note messages, use pitch (note number)
	int midiId = (msg.status == 176) ? msg.control : msg.pitch;
	
	// Build mapping key (channel + midiId, control is always 0 in key)
	string key = getMappingKey(msg.channel, midiId, 0);
	
	// One-to-one mapping: remove any existing mapping for this MIDI
	auto it = midiControllerMap.find(key);
	if (it != midiControllerMap.end()) {
		cout << "MIDI Learn: Replaced existing mapping for MIDI " << key << endl;
	}
	
	// Detect element type
	string tipo = detectElementType(learnElement);
	
	// For bool/toggle, check if MIDI is CC (fader) or Note (button)
	if (tipo == "bool") {
		if (msg.status == 176) {  // Control Change (fader)
			tipo = "bool_fader";
		}
		// Note (144) uses default "bool" (flip mode)
	}
	
	// Create new mapping
	elementListMidiController mapping;
	mapping.device = midiControllerIn.getName();  // Store device name
	mapping.ui = learnUIName;
	mapping.tipo = tipo;
	mapping.nome = learnElement->name;
	mapping.channel = msg.channel;
	mapping.pitch = midiId;  // Store the actual CC number or note number
	
	// For radio, we might need valor - detect if needed
	if (tipo == "radio") {
		// Radio by index - no specific valor needed
		mapping.valor = "";
	}
	
	midiControllerMap[key] = mapping;
	
	cout << "MIDI Learn: Mapped '" << learnElement->name << "' (" << tipo << ") to MIDI " << key << endl;
	
	// Show green confirmation
	justMappedElement = learnElement;
	justMappedTime = ofGetElapsedTimef();
	
	// Clear learning state (ready for next)
	learnElement = nullptr;
	learnUIName = "";
}

void ofxMicroUIMidiController::cancelLearnMode() {
	// Restore from backup
	midiControllerMap = mappingsBackup;
	setLearnMode(false);
}

string ofxMicroUIMidiController::detectElementType(ofxMicroUI::element* e) {
	if (dynamic_cast<ofxMicroUI::slider*>(e)) {
		ofxMicroUI::slider* s = dynamic_cast<ofxMicroUI::slider*>(e);
		return s->isInt ? "int" : "float";
	}
	else if (dynamic_cast<ofxMicroUI::toggle*>(e)) {
		return "bool";
	}
	else if (dynamic_cast<ofxMicroUI::radio*>(e)) {
		return "radio";
	}
	else if (dynamic_cast<ofxMicroUI::hold*>(e)) {
		return "hold";
	}
	
	return "unknown";
}

void ofxMicroUIMidiController::drawLearnModeOverlay() {
	if (!learnMode) return;
	
	ofPushStyle();
	
	// Modal background - dim everything
	ofFill();
	ofSetColor(0, 0, 0, 150);  // Semi-transparent black
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
	
	// Draw status (two lines at bottom)
	drawStatusBar();
	
	// Get last clicked element across all UIs
	ofxMicroUI::element* lastClicked = nullptr;
	if (_u->_lastClickedElement != nullptr) {
		lastClicked = _u->_lastClickedElement;
	} else {
		for (auto& ui : _u->uis) {
			if (ui.second._lastClickedElement != nullptr) {
				lastClicked = ui.second._lastClickedElement;
				break;
			}
		}
	}
	
	// Check if we have active MIDI input to highlight
	ofxMicroUI::element* activeMidiElement = nullptr;
	float timeSinceMidi = ofGetElapsedTimef() - lastMidiReceiveTime;
	if (timeSinceMidi < MIDI_HIGHLIGHT_DURATION) {
		string activeKey = getMappingKey(lastReceivedMidi.channel, lastReceivedMidi.pitch, lastReceivedMidi.control);
		auto it = midiControllerMap.find(activeKey);
		if (it != midiControllerMap.end()) {
			activeMidiElement = findElementAcrossUIs(it->second.ui, it->second.nome);
		}
	}
	
	// Draw ALL mapped elements with fill tint only (no contour)
	for (auto& m : midiControllerMap) {
		ofxMicroUI::element* e = findElementAcrossUIs(m.second.ui, m.second.nome);
		if (e && e != learnElement && e != justMappedElement && e != lastClicked && e != activeMidiElement) {
			ofRectangle rect = getElementScreenRect(e);
			
			// Fill tint only, no contour
			ofFill();
			ofSetColor(0, 150, 255, 40);  // Blue tint = mapped
			ofDrawRectangle(rect);
		}
	}
	
	// Highlight active MIDI element (magenta) - receiving MIDI right now
	if (activeMidiElement != nullptr && activeMidiElement != learnElement && activeMidiElement != justMappedElement) {
		ofRectangle rect = getElementScreenRect(activeMidiElement);
		
		// Magenta fill for active MIDI
		ofFill();
		ofSetColor(255, 0, 255, 80);  // Magenta tint = active MIDI
		ofDrawRectangle(rect);
	}
	
	// Highlight last clicked element with contour only (white)
	if (lastClicked != nullptr && lastClicked != learnElement && lastClicked != justMappedElement) {
		ofRectangle rect = getElementScreenRect(lastClicked);
		
		ofNoFill();
		ofSetLineWidth(2);
		ofSetColor(255);  // White contour = last clicked
		ofDrawRectangle(rect);
	}
	
	// Highlight currently learning element (yellow contour + fill)
	if (learnElement != nullptr) {
		ofRectangle rect = getElementScreenRect(learnElement);
		
		ofNoFill();
		ofSetLineWidth(4);
		ofSetColor(255, 255, 0);  // Yellow = waiting for MIDI
		ofDrawRectangle(rect);
		
		// Fill with slight yellow tint
		ofFill();
		ofSetColor(255, 255, 0, 30);
		ofDrawRectangle(rect);
	} 
	else if (justMappedElement != nullptr) {
		// Show green permanently until next mapping
		ofRectangle rect = getElementScreenRect(justMappedElement);
		
		ofNoFill();
		ofSetLineWidth(3);
		ofSetColor(0, 255, 0);  // Green = successfully mapped
		ofDrawRectangle(rect);
		
		ofFill();
		ofSetColor(0, 255, 0, 30);
		ofDrawRectangle(rect);
	}
	
	ofPopStyle();
}

void ofxMicroUIMidiController::drawStatusBar() {
	float y1 = ofGetHeight() - 40;
	float y2 = ofGetHeight() - 20;
	
	// Saturated light green with hint of blue (spring green / mint)
	ofSetColor(100, 255, 150);
	
	if (learnElement != nullptr) {
		// Line 1: Mode + what to do
		ofDrawBitmapString("MIDI Learn Mode: Move MIDI control to map '" + learnElement->name + "'", 20, y1);
		// Line 2: Shortcuts
		ofDrawBitmapString("[Esc] Cancel", 20, y2);
	} else {
		// Line 1: Mode + what to do
		ofDrawBitmapString("MIDI Learn Mode: Click element, then move MIDI control | Right-click mapped: remove", 20, y1);
		// Line 2: Shortcuts
		ofDrawBitmapString("[Enter] Save  |  [Esc] Cancel  |  [0] Clear all", 20, y2);
	}
}

string ofxMicroUIMidiController::getMappingsFilePath() const {
	// Use device name for file
	// Note: getName() is not const, so we use a default name
	string deviceName = "midi_controller";
	// Sanitize filename
	ofStringReplace(deviceName, " ", "_");
	return deviceName + "_mappings.xml";
}

void ofxMicroUIMidiController::saveMappingsToXml() {
	string filePath = getMappingsFilePath();
	
	ofXml xml;
	xml.appendChild("midiMappings");
	xml.setAttribute("device", midiControllerIn.getName());
	xml.setAttribute("version", "1.0");
	
	auto root = xml.getChild("midiMappings");
	
	for (auto& m : midiControllerMap) {
		auto mappingNode = root.appendChild("mapping");
		mappingNode.setAttribute("device", m.second.device);
		mappingNode.setAttribute("channel", m.second.channel);
		mappingNode.setAttribute("pitch", m.second.pitch);
		mappingNode.setAttribute("control", 0);  // Not used in current format
		mappingNode.setAttribute("status", 0);   // Derived from context
		mappingNode.setAttribute("ui", m.second.ui);
		mappingNode.setAttribute("tipo", m.second.tipo);
		mappingNode.setAttribute("nome", m.second.nome);
		if (!m.second.valor.empty()) {
			mappingNode.setAttribute("valor", m.second.valor);
		}
	}
	
	if (xml.save(filePath)) {
		cout << "MIDI Learn: Saved mappings to " << filePath << endl;
	} else {
		cout << "MIDI Learn: Failed to save mappings to " << filePath << endl;
	}
}

void ofxMicroUIMidiController::loadMappingsFromXml() {
	string filePath = getMappingsFilePath();
	
	ofXml xml;
	if (!xml.load(filePath)) {
		cout << "MIDI Learn: No existing mappings file found (" << filePath << ")" << endl;
		return;
	}
	
	auto root = xml.getChild("midiMappings");
	if (!root) {
		cout << "MIDI Learn: Invalid XML structure" << endl;
		return;
	}
	
	// Clear existing mappings
	midiControllerMap.clear();
	
	for (auto& mappingNode : root.getChildren("mapping")) {
		elementListMidiController mapping;
		
		if (mappingNode.getAttribute("device")) {
			mapping.device = mappingNode.getAttribute("device").getValue();
		}
		mapping.channel = mappingNode.getAttribute("channel").getIntValue();
		mapping.pitch = mappingNode.getAttribute("pitch").getIntValue();
		// control and status not stored, derived from context
		mapping.ui = mappingNode.getAttribute("ui").getValue();
		mapping.tipo = mappingNode.getAttribute("tipo").getValue();
		mapping.nome = mappingNode.getAttribute("nome").getValue();
		
		if (mappingNode.getAttribute("valor")) {
			mapping.valor = mappingNode.getAttribute("valor").getValue();
		}
		
		string key = getMappingKey(mapping.channel, mapping.pitch, 0);
		midiControllerMap[key] = mapping;
	}
	
	cout << "MIDI Learn: Loaded " << midiControllerMap.size() << " mappings from " << filePath << endl;
}

string ofxMicroUIMidiController::getMappingKey(int channel, int pitch, int control) const {
	return ofToString(channel) + " " + ofToString(pitch) + " " + ofToString(control);
}

bool ofxMicroUIMidiController::isElementMapped(ofxMicroUI::element* e, string& outKey) {
	if (!e) return false;
	
	for (auto& m : midiControllerMap) {
		if (m.second.nome == e->name && m.second.ui == e->_ui->uiName) {
			outKey = m.first;
			return true;
		}
	}
	return false;
}

// Modify parseMidiMessage to handle learn mode
void ofxMicroUIMidiController::parseMidiMessageWithLearn(ofxMidiMessage& msg) {
	// Track last received MIDI for active feedback in learn mode
	if (learnMode) {
		lastReceivedMidi = msg;
		lastMidiReceiveTime = ofGetElapsedTimef();
	}
	
	// If in learn mode and waiting for element, check for clicked element
	if (learnMode) {
		// Check for clicked element in master UI
		if (_u->_lastClickedElement != nullptr && learnElement == nullptr) {
			startLearning(_u->_lastClickedElement, _u->uiName);
			_u->_lastClickedElement = nullptr;  // Clear after using
		}
		
		// Check sub-UIs
		for (auto& ui : _u->uis) {
			if (ui.second._lastClickedElement != nullptr && learnElement == nullptr) {
				startLearning(ui.second._lastClickedElement, ui.second.uiName);
				ui.second._lastClickedElement = nullptr;
			}
		}
		
		// If learning and MIDI received, finish learning
		if (learnElement != nullptr) {
			// Only use note on or control change
			if (msg.status == 144 || msg.status == 176) {
				finishLearning(msg);
				return;  // Don't process as normal MIDI
			}
		}
	}
	
	// Normal MIDI processing (existing code)
	parseMidiMessage(msg);
}

ofxMicroUI::element* ofxMicroUIMidiController::findElementAcrossUIs(const string& uiName, const string& elementName) {
	ofxMicroUI* targetUI = nullptr;
	
	if (uiName == "master" || uiName == _u->uiName) {
		targetUI = _u;
	} else if (_u->uis.find(uiName) != _u->uis.end()) {
		targetUI = &_u->uis[uiName];
	}
	
	if (targetUI) {
		for (auto& e : targetUI->elements) {
			if (e->name == elementName) {
				return e;
			}
		}
	}
	
	return nullptr;
}

ofRectangle ofxMicroUIMidiController::getElementScreenRect(ofxMicroUI::element* e) {
	if (!e || !e->_ui) return ofRectangle();
	
	ofRectangle rect = e->rect;
	
	// Add UI position and settings offset
	rect.x += e->_ui->rectPos.x + e->_ui->_settings->offset.x;
	rect.y += e->_ui->rectPos.y + e->_ui->_settings->offset.y;
	
	return rect;
}



//#endif
