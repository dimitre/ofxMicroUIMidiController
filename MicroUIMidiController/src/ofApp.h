#pragma once

#include "ofMain.h"
#include "ofxMicroUI.h"
#include "ofxMicroUIMidiController.h"

class ofApp : public ofBaseApp{
	public:
		void setup();
		void update();
		void draw();

	ofxMicroUI u;
	ofxMicroUI * ui = &u.uis["ui"];
	ofxMicroUI * uiC = &u.uis["scene"];
	string & scene = ui->pString["scene"];

	ofxMicroUISoftware soft { &u, "MidiController" };
	ofFbo * fbo = &soft.fbo;

	ofxMicroUIMidiController midiController = { &soft, "APC MINI" };
};
