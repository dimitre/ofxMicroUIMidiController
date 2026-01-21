#include "ofxMicroUIMidi.h"

ofxMicroUIMidi::ofxMicroUIMidi(ofxMicroUISoftware * _soft, const std::string & midiDevice) {
	bool connected = midiIn.openPort(midiDevice);
	cout << "ofxMicroUIMidiController setup :: " + midiDevice + " :: ";
	cout << (connected ? "connected" : "not found") << endl;
	if (connected) {
		midiOut.openPort(midiDevice); // by number
		midiIn.ignoreTypes(false, false, false);
		//	ofxMidi::setConnectionListener(this);
		midiIn.addListener(this);
	}

//	ofAddListener(_u->uiEventMaster, this, &ofxMicroUIMidi::uiEventMaster);
	ofAddListener(ofEvents().update, this, &ofxMicroUIMidi::onUpdate);
}

void ofxMicroUIMidi::onUpdate(ofEventArgs &data) {
 	ofxMidiMessage msg;
    while(threadMidiMessage.tryReceive(msg)) {
    	parseMidiMessage(msg);
    }
}

void ofxMicroUIMidi::newMidiMessage(ofxMidiMessage& msg) {
	threadMidiMessage.send(msg);
}

void ofxMicroUIMidi::parseMidiMessage(ofxMidiMessage& msg) {
}
