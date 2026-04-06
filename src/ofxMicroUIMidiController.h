#pragma once

//#ifdef USEMIDI
#include "ofEvents.h"
#include "ofxMidi.h"
#include "ofxMicroUISoftware.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
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

// Forward declaration
class ofxMicroUIMidiController;

// Per-device listener that knows its device name
class MidiDeviceListener : public ofxMidiListener {
public:
    ofxMicroUIMidiController* controller = nullptr;
    string deviceName;
    
    void newMidiMessage(ofxMidiMessage& msg) override;
};

class ofxMicroUIMidiController : public ofxMidiListener {
public:
    ofxMicroUIMidiController() {}
    
    // Default: listen to all MIDI devices
    ofxMicroUIMidiController(ofxMicroUISoftware * _soft);
    
    // Legacy: specific device only
    ofxMicroUIMidiController(ofxMicroUISoftware * _soft, string device);
    
    ~ofxMicroUIMidiController() {};

    ofxMicroUISoftware * soft = nullptr;
    
    // Thread channel for MIDI messages (from any device)
    ofThreadChannel<ofxMidiMessage> threadMidiMessage;

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

    vector <unsigned int> lateralLeds = {
        71, 70, 69, 68, 67, 66, 65, 64, //baixo
        82, 83, 84, 85, 86, 87, 88, 89, //lateral
        98, //quadrado?
    };

    unsigned int colors[3] = { 1,3,5 };

    ofFbo fbo;
    ofPixels pixels;

    void displayFromVals() {
        // Send to the device that was used for mapping (first device with matching name)
        for (auto& device : devices) {
            for (int i=0; i<64; i++) {
                device->out.sendNoteOn(1, apcMiniLeds[i], vals[i]);
            }
            break; // Only send to first device for now
        }
    }

    void lateralRandom() {
        for (auto& device : devices) {
            for (auto & l  : lateralLeds) {
                sendNoteToDevice(device->name, 1, l, ofRandom(0,6));
            }
            break;
        }
    }

    void centerClear() {
        for (auto& device : devices) {
            for (auto & l  : apcMiniLeds) {
                sendNoteToDevice(device->name, 1, l, 0);
            }
        }
    }

    void lateralClear() {
        for (auto& device : devices) {
            for (auto & l  : lateralLeds) {
                sendNoteToDevice(device->name, 1, l, 0);
            }
        }
    }

    void blackout() {
        for (auto& device : devices) {
            for (auto & l  : apcMiniLeds) {
                sendNoteToDevice(device->name, 1, l, 0);
            }
            for (auto & l  : lateralLeds) {
                sendNoteToDevice(device->name, 1, l, 0);
            }
        }
    }

    void allRandom() {
        for (auto& device : devices) {
            int c=0;
            for (auto & l  : apcMiniLeds) {
                sendNoteToDevice(device->name, 1, l, c%6 + 1);
                c++;
            }
            for (auto & l  : lateralLeds) {
                sendNoteToDevice(device->name, 1, l, c%2 + 1);
                c++;
            }
            break; // Only first device for now
        }
    }

    void display() {
        if (devices.empty()) return;
        
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
                // Send to first device
                sendNoteToDevice(devices[0]->name, 1, pitch, indexColor);
            }
        }
    }
#else
    void blackout() {}
#endif

    // Legacy: single device MIDI I/O (for backward compatibility)
    map <int, map<int, int> > sentMidi;
    
    void sendNote(int c, int p, int v) {
        // Legacy: send to first connected device
        if (!devices.empty()) {
            sentMidi[c][p] = v;
            devices[0]->out.sendNoteOn(c, p, v);
        }
    }
    
    // New: send to specific device by name
    void sendNoteToDevice(const string& deviceName, int c, int p, int v) {
        for (auto& device : devices) {
            if (device->name == deviceName) {
                sentMidi[c][p] = v;
                device->out.sendNoteOn(c, p, v);
                return;
            }
        }
    }

    void restoreLights() {
        for (auto& device : devices) {
            for (auto & x : sentMidi) {
                for (auto y : x.second) {
                    sendNoteToDevice(device->name, x.first, y.first, y.second);
                }
            }
        }
    }

    uint64_t frameAction = 0;

    struct elementListMidiController {
    public:
        string device;  // MIDI device name (for output feedback)
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

    // Legacy single device (for backward compatibility)
    ofxMidiIn 	midiControllerIn;
    ofxMidiOut	midiControllerOut;
    
    // Multi-device support
    struct MidiDevice {
        string name;
        ofxMidiIn in;
        ofxMidiOut out;
        MidiDeviceListener listener;
    };
    // Use unique_ptr for stable addresses - vector reallocation won't move the objects
    vector<std::unique_ptr<MidiDevice>> devices;
    
    // Track which device sent the last message (for learn mode)
    string lastMidiDeviceName;

    ofxMidiMessage midiMessage;
    bool midiKeys[4000];

    // Mapping: key is "uiName/elementName" (one mapping per element)
    map <string, elementListMidiController> elementToMidi;
    
    // Legacy map (for backward compatibility during transition)
    map <string, elementListMidiController> midiControllerMap;
    vector <elementListMidiController *> elements;

    string lastString;

    map <string,string>			pString;

    int lastPresetChannel = -1;
    int lastPresetPitch = -1;

    ofFbo * fboMC = NULL;

    int holdPresetNumber = 0;

    ofxMicroUI * _u = NULL;
    void setUI(ofxMicroUI &u) {
        _u = &u;
        ofAddListener(_u->uiEvent,this, &ofxMicroUIMidiController::uiEvent);
        for (auto & uis : _u->uis) {
            ofAddListener(uis.second.uiEvent,this, &ofxMicroUIMidiController::uiEvent);
        }
    }

    void newMidiMessage(ofxMidiMessage& msg);
    void onMidiFromDevice(ofxMidiMessage& msg, const string& deviceName);
    void parseMidiMessage(ofxMidiMessage& msg);
    void parseMidiMessageWithLearn(ofxMidiMessage& msg);
    void processMidiForElement(ofxMidiMessage& msg, elementListMidiController* te, ofxMicroUI* _ui);

    void set(const string & midiDevice);
    void setupAllDevices();

    void uiEventMidi(vector<string> & strings) {
        elementLearn.nome = strings[0];
        elementLearn.ui = strings[1];
        elementLearn.tipo = "float";
    }

    void onExit(ofEventArgs & /*data*/) {
        blackout();
        for (auto& device : devices) {
            device->out.closePort();
            device->in.closePort();
        }
        midiControllerOut.closePort();
        midiControllerIn.closePort();
    }

    void checkElement(const ofxMicroUI::element & e);
    void uiEvent(ofxMicroUI::element & e);
    void uiEventMaster(string & s);
    
    // ============================================================
    // MIDI LEARN FEATURE
    // ============================================================
    
public:
    void toggleLearnMode();
    void setLearnMode(bool active);
    bool isLearnMode() const { return learnMode; }
    
    void onDraw(ofEventArgs& args);
    void onKeyPressed(ofKeyEventArgs& args);
    void onMousePressed(ofMouseEventArgs& args);
    
private:
    bool learnMode = false;
    ofxMicroUI::element* learnElement = nullptr;
    string learnUIName;
    ofxMicroUI::element* justMappedElement = nullptr;
    float justMappedTime = 0;
    
    int currentLearnChannel = -1;
    int currentLearnPitch = -1;
    int currentLearnControl = -1;
    
    ofxMidiMessage lastReceivedMidi;
    float lastMidiReceiveTime = 0;
    static constexpr float MIDI_HIGHLIGHT_DURATION = 0.3f;
    
    map<string, elementListMidiController> mappingsBackup;
    
    void startLearning(ofxMicroUI::element* e, const string& uiName);
    void finishLearning(const ofxMidiMessage& msg);
    void clearMapping(const string& uiName, const string& elementName);
    void cancelLearnMode();
    
    string detectElementType(ofxMicroUI::element* e);
    
    void drawLearnModeOverlay();
    void drawStatusBar();
    
    string getMappingsFilePath() const;
    void saveMappingsToXml();
    void loadMappingsFromXml();
    
    string getMappingKey(int channel, int pitch, int control) const;
    ofxMicroUI::element* findElementAcrossUIs(const string& uiName, const string& elementName);
    bool isElementMapped(ofxMicroUI::element* e, string& outKey);
    ofRectangle getElementScreenRect(ofxMicroUI::element* e);
};

//#endif
