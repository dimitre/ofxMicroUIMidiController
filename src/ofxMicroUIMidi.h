#pragma once

//#ifdef USEMIDI
#include "ofEvents.h"
#include "ofxMidi.h"
#include "ofxMicroUISoftware.h"

class ofxMicroUIMidi : public ofxMidiListener {
public:
	ofxMicroUIMidi(ofxMicroUISoftware * _soft, const std::string & midiDevice);
//	~ofxMicroUIMidi() {};
	void onUpdate(ofEventArgs &data);
	void newMidiMessage(ofxMidiMessage& msg);
	void parseMidiMessage(ofxMidiMessage& msg);
	
	ofThreadChannel<ofxMidiMessage> threadMidiMessage;

	ofxMidiIn 	midiIn;
	ofxMidiOut	midiOut;
};
