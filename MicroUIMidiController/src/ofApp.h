#pragma once

#include "ofMain.h"
#include "ofxMicroUI.h"
#include "ofxMicroUIMidiController.h"

class ofApp : public ofBaseApp{
		public:
			void setup();
			void update();
			void draw();
			void keyPressed(int key);

	ofxMicroUI u;
	ofxMicroUI * ui = &u.uis["ui"];
	ofxMicroUI * uiC = &u.uis["scene"];
	string & scene = ui->pString["scene"];

	ofxMicroUISoftware soft { &u, 1 };
	ofFbo * fbo = soft.getFboFinal();

	ofxMicroUIMidiController midiController = { &soft, "APC MINI" };
};
