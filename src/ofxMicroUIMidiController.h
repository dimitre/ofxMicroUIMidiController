#pragma once

//#ifdef USEMIDI
#include "ofEvents.h"
#include "ofxMidi.h"
#include "ofxMicroUISoftware.h"
#include <string>
#include <vector>
#include <map>
using std::string;
using std::vector;
using std::map;

/*
 //http://community.akaipro.com/akai_professional/topics/midi-information-for-apc-mini
 //127 = verde

 All Notes Off. When an All Notes Off is received, all oscillators will turn off.
 c = 123, v = 0: All Notes Off (See text for description of actual mode commands.)
 c = 124, v = 0: Omni Mode Off
 c = 125, v = 0: Omni Mode On
 c = 126, v = M: Mono Mode On (Poly Off) where M is the number of channels (Omni Off) or 0 (Omni On)
 c = 127, v = 0: Poly Mode On (Mono Off) (Note: These four messages also cause All Notes Off)
 */


#include "ofxMicroUI.h"

class ofxMicroUIMidiController : public ofxMidiListener {
public:
	ofxMicroUIMidiController(ofxMicroUISoftware * _soft, string device);
	~ofxMicroUIMidiController() {};

	// std::string changePreset { "" };
	ofThreadChannel<std::string> presetChannel;

	void onUpdate(ofEventArgs &data);

	unsigned int vals[64] = { 0 };

#ifdef USEMIDICONTROLLERLIGHTS
	unsigned int apcMiniLeds[64] = {
		56, 57, 58, 59, 60, 61, 62, 63,
		48, 49, 50, 51, 52, 53, 54, 55,
		40, 41, 42, 43, 44, 45, 46, 47,
		32, 33, 34, 35, 36, 37, 38, 39,
		24, 25, 26, 27, 28, 29, 30, 31,
		16, 17, 18, 19, 20, 21, 22, 23,
		8,  9,  10, 11, 12, 13, 14, 15,
		0,  1,  2,  3,  4,  5,  6,  7
	};

	/* solved here
	 I had an issue when sending note off to 98 (shift key) before others, so midi was stuck
	 */

	vector <unsigned int> lateralLeds = {
		71, 70, 69, 68, 67, 66, 65, 64, //baixo
		82, 83, 84, 85, 86, 87, 88, 89, //lateral
		98, //quadrado?
	};

//    vector <unsigned int> lateralLeds = {
//        71, 70, 69, 68, 67, 66, 65, 64
//    };


//    unsigned int colors[4] = { 0,1,3,5 };
	unsigned int colors[3] = { 1,3,5 };

	ofFbo fbo;
	ofPixels pixels;

	void displayFromVals() {
		for (int i=0; i<64; i++) {
//            sendNote(1, apcMiniLeds[i], vals[i]); // 1 green 3 red 5 yellow
			midiControllerOut.sendNoteOn(1, apcMiniLeds[i], vals[i]);
		}
	}

	void lateralRandom() {
		for (auto & l  : lateralLeds) {
			sendNote(1, l, ofRandom(0,6)); // 1 green 3 red 5 yellow
		}
	}

	void centerClear() {
		for (auto & l  : apcMiniLeds) {
			sendNote(1, l, 0); // 1 green 3 red 5 yellow
		}
	}

	void lateralClear() {
		for (auto & l  : lateralLeds) {
			sendNote(1, l, 0); // 1 green 3 red 5 yellow
		}
	}

	void blackout() {
		for (auto & l  : apcMiniLeds) {
			sendNote(1, l, 0); // 1 green 3 red 5 yellow
		}
		for (auto & l  : lateralLeds) {
			sendNote(1, l, 0); // 1 green 3 red 5 yellow
		}
	}

	void allRandom() {
		int c=0;
		for (auto & l  : apcMiniLeds) {
			sendNote(1, l, c%6 + 1); // 1 green 3 red 5 yellow
			c++;
		}
		for (auto & l  : lateralLeds) {
			sendNote(1, l, c%2 + 1); // 1 green 3 red 5 yellow
			c++;
		}
	}

		// example
	//    midiController.fbo.begin();
	//    fbo->draw(-fbo->getWidth()*.5, -fbo->getHeight()*.5);
	//    midiController.fbo.end();
	//    midiController.display();

	void display() {
		if (!fbo.isAllocated()) {
			fbo.allocate(8, 8, GL_RGB);
			pixels.allocate(8, 8, GL_RGB);
			fbo.begin();
			ofClear(0,255);
			fbo.end();
		} else {
			fbo.getTexture().readToPixels(pixels);
			for (int i=0; i<64; i++) {
				float luma = pixels.getData()[i*3] / 64.0;
				int indexColor = colors[int(luma)];
				int pitch = apcMiniLeds[i];
				sendNote(1, pitch, indexColor); // 1 green 3 red 5 yellow
			}
		}
	}
#else
	void blackout() {}
#endif





	map <int, map<int, int> > sentMidi;
	void sendNote(int c, int p, int v) {
//        cout << "sendNote :: " << c << " : " << p << " : " << v << endl;
		sentMidi[c][p] = v;
		midiControllerOut.sendNoteOn(c, p, v);
	}

	void restoreLights() {
		for (auto & x : sentMidi) {
			for (auto y : x.second) {
				sendNote(x.first, y.first, y.second);
			}
		}
	}

	uint64_t frameAction = 0;

	// todo: some kind of listener able to connect if device is not found at first.
	struct elementListMidiController {
	public:
		string ui;
		string tipo;
		string nome;
		string valor = "";
		int channel;
		int pitch;
	};

	bool connected = false;

	elementListMidiController elementLearn;
	string folder = "";

	ofxMidiIn 	midiControllerIn;
	ofxMidiOut	midiControllerOut;
	ofxMidiMessage midiMessage;
	bool midiKeys[4000];

	//map <int, map <int, map<int, map<int, elementListMidiController> > > > mapaMidiController;

	map <string, elementListMidiController> midiControllerMap;
	vector <elementListMidiController *> elements;

	string lastString;

	map <string,string>			pString;

	// somente para apagar leds nos presets, nada mais
	int lastPresetChannel;
	int lastPresetPitch;

	ofFbo * fboMC = NULL;

	int holdPresetNumber = 0;

	ofxMicroUI * _u = NULL;
	void setUI(ofxMicroUI &u) {
		_u = &u;
		ofAddListener(_u->uiEvent,this, &ofxMicroUIMidiController::uiEvent);
		for (auto & uis : _u->uis) {
			ofAddListener(uis.second.uiEvent,this, &ofxMicroUIMidiController::uiEvent);
		}
//	this is ios only
//		cout << "ofxMidi::setConnectionListener(this);" << endl;
//		ofxMidi::setConnectionListener(this);
	}


//	//--------------------------------------------------------------
//	void midiInputAdded(string name, bool isNetwork) override {
//		cout << "input added" << endl;
//		cout << name << endl;
//	//	stringstream msg;
//	//	msg << "ofxMidi: input added: " << name << " network: " << isNetwork;
//	}
//
//	//--------------------------------------------------------------
//	void midiInputRemoved(string name, bool isNetwork) override {
//		cout << "input removed" << endl;
//		cout << name << endl;
//	//	stringstream msg;
//	//	msg << "ofxMidi: input removed: " << name << " network: " << isNetwork << endl;
//	}
		//--------------------------------------------------------------
	void newMidiMessage(ofxMidiMessage& msg);
	void set(const string & midiDevice);


	void uiEventMidi(vector<string> & strings) {
		elementLearn.nome = strings[0];
		elementLearn.ui = strings[1];
		elementLearn.tipo = "float";
	}

	void onExit(ofEventArgs &data) {
		blackout();
		midiControllerOut.closePort();
		midiControllerIn.closePort();
	}

	void checkElement(const ofxMicroUI::element & e);
	void uiEvent(ofxMicroUI::element & e);
	void uiEventMaster(string & s);
};


//#endif
