#include "ofxMicroUIMidiController.h"

// MidiDeviceListener implementation
void MidiDeviceListener::newMidiMessage(ofxMidiMessage& msg) {
    if (controller) {
        controller->onMidiFromDevice(msg, deviceName);
    }
}

// Default constructor - opens all MIDI devices
ofxMicroUIMidiController::ofxMicroUIMidiController(ofxMicroUISoftware * _soft) : soft(_soft) {
    setupAllDevices();
    setUI(*_soft->_ui);
    ofAddListener(_u->uiEventMaster, this, &ofxMicroUIMidiController::uiEventMaster);
    ofAddListener(ofEvents().update, this, &ofxMicroUIMidiController::onUpdate);
}

// Legacy constructor - specific device only
ofxMicroUIMidiController::ofxMicroUIMidiController(ofxMicroUISoftware * _soft, string device) : soft(_soft) {
    set(device);
    setUI(*_soft->_ui);
    ofAddListener(_u->uiEventMaster, this, &ofxMicroUIMidiController::uiEventMaster);
    ofAddListener(ofEvents().update, this, &ofxMicroUIMidiController::onUpdate);
}

void ofxMicroUIMidiController::setupAllDevices() {
    // Create temporary instances to query available ports
    ofxMidiIn tempMidiIn;
    ofxMidiOut tempMidiOut;
    
    int numPorts = tempMidiIn.getNumInPorts();
    cout << "ofxMicroUIMidiController: Found " << numPorts << " MIDI input devices" << endl;
    
    for (int i = 0; i < numPorts; i++) {
        string name = tempMidiIn.getInPortName(i);
        
        auto device = std::make_unique<MidiDevice>();
        device->name = name;
        device->listener.controller = this;
        device->listener.deviceName = name;
        
        bool inOk = device->in.openPort(i);
        if (inOk) {
            device->in.ignoreTypes(false, false, false);
            device->in.addListener(&device->listener);
            
            // Try to open matching output port by finding index by name
            int outPort = -1;
            int numOutPorts = tempMidiOut.getNumOutPorts();
            for (int j = 0; j < numOutPorts; j++) {
                if (tempMidiOut.getOutPortName(j) == name) {
                    outPort = j;
                    break;
                }
            }
            if (outPort >= 0) {
                device->out.openPort(outPort);
            }
            
            devices.push_back(std::move(device));
            connected = true;
            cout << "  Opened: " << name << endl;
        } else {
            cout << "  Failed to open: " << name << endl;
        }
    }
    
    if (devices.empty()) {
        cout << "ofxMicroUIMidiController: No MIDI devices connected" << endl;
        connected = false;
    } else {
        // Load mappings
        loadMappingsFromXml();
        ofAddListener(ofEvents().exit, this, &ofxMicroUIMidiController::onExit);
    }
}

void ofxMicroUIMidiController::onMidiFromDevice(ofxMidiMessage& msg, const string& deviceName) {
    // Store which device sent this message
    lastMidiDeviceName = deviceName;
    
    // Forward to thread channel for processing
    threadMidiMessage.send(msg);
}

void ofxMicroUIMidiController::onUpdate(ofEventArgs & /*data*/) {
    ofxMidiMessage msg;
    while(threadMidiMessage.tryReceive(msg)) {
        parseMidiMessageWithLearn(msg);
    }
}

void ofxMicroUIMidiController::newMidiMessage(ofxMidiMessage& msg) {
    // Legacy single device callback - use the first device as source
    if (!devices.empty()) {
        lastMidiDeviceName = devices[0]->name;
    }
    threadMidiMessage.send(msg);
}

void ofxMicroUIMidiController::parseMidiMessage(ofxMidiMessage& msg) {
    frameAction = ofGetFrameNum();
    bool debug = false;
    if (debug) {
        cout << "channel:" +ofToString(msg.channel) << endl;
        cout << "pitch:"   +ofToString(msg.pitch) << endl;
        cout << "control:" +ofToString(msg.control) << endl;
        cout << "status:"  +ofToString(msg.status) << endl;
        cout << "-------" << endl;
    }

    // For CC messages, use control number as the identifier
    // For Note messages, use pitch (note number)
    int midiId = (msg.status == 176) ? msg.control : msg.pitch;
    string index = getMappingKey(msg.channel, midiId, 0);
    string index2 = index + " " + ofToString(msg.status);

    // Check new element-based mapping first
    // We need to find which element (if any) is mapped to this MIDI message
    for (auto& m : elementToMidi) {
        if (m.second.channel == msg.channel && m.second.pitch == midiId) {
            elementListMidiController *te = &m.second;
            ofxMicroUI * _ui { te->ui == "master" ? _u :  &_u->uis[te->ui] };
            
            processMidiForElement(msg, te, _ui);
            break; // One mapping per element, so we can stop
        }
    }
    
    // Also check legacy map for backward compatibility
    if ( midiControllerMap.find(index) != midiControllerMap.end()) {
        elementListMidiController *te = &midiControllerMap[index];
        ofxMicroUI * _ui { te->ui == "master" ? _u :  &_u->uis[te->ui] };
        
        processMidiForElement(msg, te, _ui);
    }
    
    midiKeys[msg.pitch] = msg.status == 144;
    midiMessage = msg;
}

void ofxMicroUIMidiController::processMidiForElement(ofxMidiMessage& msg, elementListMidiController* te, ofxMicroUI* _ui) {
    // Note on actions
    if (msg.status == 144) {
        if (te->tipo == "bool") {
            if (_ui->getToggle(te->nome) != NULL) {
                _ui->getToggle(te->nome)->flip();
                if (_ui->pBool[te->nome]) {
                    sendNoteToDevice(te->device, msg.channel, msg.pitch, 3);
                } else {
                    sendNoteToDevice(te->device, msg.channel, msg.pitch, 0);
                }
            }
        }

        else if (te->tipo == "preset") {
            if (lastPresetPitch >= 0) {
                sendNoteToDevice(te->device, lastPresetChannel, lastPresetPitch, 0);
            }
            cout << "midi preset " << msg.pitch << endl;
            sendNoteToDevice(te->device, msg.channel, msg.pitch, 1);
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
        }
    }

    if (te->tipo == "radio") {
        if (te->valor == "") {
            ofxMicroUI::radio * r = _ui->getRadio(te->nome);
            int nElements = (int)r->elements.size();
            int valor = ofMap(msg.value, 0, 127, 0, nElements);
            r->set(valor);
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
        ofxMicroUI::toggle * t = _ui->getToggle(te->nome);
        if (t != NULL) {
            bool newValue = msg.value >= 64;
            if (t->getVal() != newValue) {
                t->set(newValue);
            }
        }
    }

    else if (te->tipo == "hold") {
        ofxMicroUI::hold * e = (ofxMicroUI::hold*)_ui->getElement(te->nome);
        if (e != NULL) {
            e->set(msg.status == 144);
            sendNoteToDevice(te->device, msg.channel, msg.pitch, msg.status == 144 ? 1 : 0);
        }
    }

    else if (te->tipo == "savePresetNumber") {
        if (_u != NULL) {
            cout << te->tipo << endl;
        }
    }

    else if (te->tipo == "restorePresetNumber") {
        if (_u != NULL) {
            cout << te->tipo << endl;
        }
    }

    else if (te->tipo == "presetHold") {
        if (_u != NULL) {
            sendNoteToDevice(te->device, lastPresetChannel, lastPresetPitch, 0);
            soft->loadPreset(te->nome);
            sendNoteToDevice(te->device, msg.channel, msg.pitch, 3);
            lastPresetChannel = msg.channel;
            lastPresetPitch = msg.pitch;
        }
    }

    else if (te->tipo == "presetRelease") {
        if (_u != NULL) {
            sendNoteToDevice(te->device, lastPresetChannel, lastPresetPitch, 0);
            sendNoteToDevice(te->device, msg.channel, msg.pitch, 3);
            lastPresetChannel = msg.channel;
            lastPresetPitch = msg.pitch;
        }
    }
}

void ofxMicroUIMidiController::set(const string & midiDevice) {
    // Legacy single device setup
    connected = midiControllerIn.openPort(midiDevice);
    cout << "ofxMicroUIMidiController setup :: " + midiDevice + " :: ";
    cout << (connected ? "connected" : "not found") << endl;
    if (connected) {
        midiControllerOut.openPort(midiDevice);
        midiControllerIn.ignoreTypes(false, false, false);
        // midiControllerIn.addListener(this);  // Legacy - listener is now in the device

        // Also add to devices vector for unified handling
        auto device = std::make_unique<MidiDevice>();
        device->name = midiDevice;
        device->listener.controller = this;
        device->listener.deviceName = midiDevice;
        device->in = std::move(midiControllerIn);
        device->out = std::move(midiControllerOut);
        // Re-open since we moved them
        device->in.openPort(midiDevice);
        device->out.openPort(midiDevice);
        device->in.addListener(&device->listener);
        devices.push_back(std::move(device));

        loadMappingsFromXml();
        
        ofAddListener(ofEvents().exit, this, &ofxMicroUIMidiController::onExit);
    }
}

void ofxMicroUIMidiController::checkElement(const ofxMicroUI::element & e) {
    if (e.name == "presets" && e._ui->uiName == "master") {
    }
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
        mappingsBackup = elementToMidi;
        learnElement = nullptr;
        justMappedElement = nullptr;
        
        ofAddListener(ofEvents().draw, this, &ofxMicroUIMidiController::onDraw);
        ofAddListener(ofEvents().keyPressed, this, &ofxMicroUIMidiController::onKeyPressed);
        ofAddListener(ofEvents().mousePressed, this, &ofxMicroUIMidiController::onMousePressed);
        
        _u->_lastClickedElement = nullptr;
        for (auto& ui : _u->uis) {
            ui.second._lastClickedElement = nullptr;
        }
        
        cout << "MIDI Learn Mode: ON" << endl;
    } else {
        ofRemoveListener(ofEvents().draw, this, &ofxMicroUIMidiController::onDraw);
        ofRemoveListener(ofEvents().keyPressed, this, &ofxMicroUIMidiController::onKeyPressed);
        ofRemoveListener(ofEvents().mousePressed, this, &ofxMicroUIMidiController::onMousePressed);
        
        learnElement = nullptr;
        justMappedElement = nullptr;
        
        saveMappingsToXml();
        cout << "MIDI Learn: Saved and exited" << endl;
    }
}

void ofxMicroUIMidiController::onDraw(ofEventArgs& /*args*/) {
    if (!learnMode) return;
    drawLearnModeOverlay();
}

void ofxMicroUIMidiController::onKeyPressed(ofKeyEventArgs& args) {
    if (!learnMode) return;
    
    if (args.key == OF_KEY_RETURN) {
        saveMappingsToXml();
        setLearnMode(false);
        cout << "MIDI Learn: Saved and exited" << endl;
    }
    else if (args.key == OF_KEY_ESC) {
        cancelLearnMode();
        cout << "MIDI Learn: Cancelled" << endl;
    }
    else if (args.key == '0') {
        int count = elementToMidi.size();
        elementToMidi.clear();
        justMappedElement = nullptr;
        learnElement = nullptr;
        cout << "MIDI Learn: Cleared all " << count << " mappings" << endl;
    }
}

void ofxMicroUIMidiController::onMousePressed(ofMouseEventArgs& args) {
    if (!learnMode) return;
    
    ofxMicroUI::element* clickedElement = nullptr;
    string clickedUIName;
    
    for (auto& e : _u->elements) {
        ofRectangle screenRect = getElementScreenRect(e);
        if (screenRect.inside(args.x, args.y)) {
            clickedElement = e;
            clickedUIName = _u->uiName;
            break;
        }
    }
    
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
    
    if (args.button == 2) {
        if (clickedElement) {
            string key = clickedUIName + "/" + clickedElement->name;
            auto it = elementToMidi.find(key);
            if (it != elementToMidi.end()) {
                elementToMidi.erase(it);
                cout << "MIDI Learn: Removed mapping for " << key << endl;
            }
        }
    }
    else if (args.button == 0) {
        if (clickedElement) {
            justMappedElement = nullptr;
        }
        else if (learnElement != nullptr) {
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
    
    justMappedElement = nullptr;
    
    cout << "MIDI Learn: Waiting for MIDI input to map '" << e->name << "' (" << uiName << ")" << endl;
}

void ofxMicroUIMidiController::finishLearning(const ofxMidiMessage& msg) {
    if (!learnElement) return;
    
    int midiId = (msg.status == 176) ? msg.control : msg.pitch;
    
    string tipo = detectElementType(learnElement);
    
    if (tipo == "bool") {
        if (msg.status == 176) {
            tipo = "bool_fader";
        }
    }
    
    // Create mapping keyed by element
    string elementKey = learnUIName + "/" + learnElement->name;
    
    // Remove any existing mapping for this element (one mapping per element)
    auto existing = elementToMidi.find(elementKey);
    if (existing != elementToMidi.end()) {
        cout << "MIDI Learn: Replacing existing mapping for '" << learnElement->name << "'" << endl;
    }
    
    elementListMidiController mapping;
    mapping.device = lastMidiDeviceName;  // The device that sent this message
    mapping.ui = learnUIName;
    mapping.tipo = tipo;
    mapping.nome = learnElement->name;
    mapping.channel = msg.channel;
    mapping.pitch = midiId;
    
    if (tipo == "radio") {
        mapping.valor = "";
    }
    
    elementToMidi[elementKey] = mapping;
    
    cout << "MIDI Learn: Mapped '" << learnElement->name << "' (" << tipo << ") to MIDI ch" 
         << msg.channel << "/" << midiId << " from device '" << lastMidiDeviceName << "'" << endl;
    
    justMappedElement = learnElement;
    justMappedTime = ofGetElapsedTimef();
    
    learnElement = nullptr;
    learnUIName = "";
}

void ofxMicroUIMidiController::cancelLearnMode() {
    elementToMidi = mappingsBackup;
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
    
    ofFill();
    ofSetColor(0, 0, 0, 150);
    ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
    
    drawStatusBar();
    
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
    
    ofxMicroUI::element* activeMidiElement = nullptr;
    float timeSinceMidi = ofGetElapsedTimef() - lastMidiReceiveTime;
    if (timeSinceMidi < MIDI_HIGHLIGHT_DURATION) {
        for (auto& m : elementToMidi) {
            if (m.second.channel == lastReceivedMidi.channel && 
                m.second.pitch == lastReceivedMidi.pitch) {
                activeMidiElement = findElementAcrossUIs(m.second.ui, m.second.nome);
                break;
            }
        }
    }
    
    // Draw all mapped elements with blue tint
    for (auto& m : elementToMidi) {
        ofxMicroUI::element* e = findElementAcrossUIs(m.second.ui, m.second.nome);
        if (e && e != learnElement && e != justMappedElement && e != lastClicked && e != activeMidiElement) {
            ofRectangle rect = getElementScreenRect(e);
            ofFill();
            ofSetColor(0, 150, 255, 40);
            ofDrawRectangle(rect);
        }
    }
    
    // Draw active MIDI element (magenta)
    if (activeMidiElement != nullptr && activeMidiElement != learnElement && activeMidiElement != justMappedElement) {
        ofRectangle rect = getElementScreenRect(activeMidiElement);
        ofFill();
        ofSetColor(255, 0, 255, 80);
        ofDrawRectangle(rect);
    }
    
    // Draw last clicked (white contour)
    if (lastClicked != nullptr && lastClicked != learnElement && lastClicked != justMappedElement) {
        ofRectangle rect = getElementScreenRect(lastClicked);
        ofNoFill();
        ofSetLineWidth(2);
        ofSetColor(255);
        ofDrawRectangle(rect);
    }
    
    // Draw learning element (yellow)
    if (learnElement != nullptr) {
        ofRectangle rect = getElementScreenRect(learnElement);
        ofNoFill();
        ofSetLineWidth(4);
        ofSetColor(255, 255, 0);
        ofDrawRectangle(rect);
        ofFill();
        ofSetColor(255, 255, 0, 30);
        ofDrawRectangle(rect);
    } 
    else if (justMappedElement != nullptr) {
        ofRectangle rect = getElementScreenRect(justMappedElement);
        ofNoFill();
        ofSetLineWidth(3);
        ofSetColor(0, 255, 0);
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
    
    ofSetColor(100, 255, 150);
    
    if (learnElement != nullptr) {
        ofDrawBitmapString("MIDI Learn Mode: Move MIDI control to map '" + learnElement->name + "'", 20, y1);
        ofDrawBitmapString("[Esc] Cancel", 20, y2);
    } else {
        ofDrawBitmapString("MIDI Learn Mode: Click element, then move MIDI control | Right-click mapped: remove", 20, y1);
        ofDrawBitmapString("[Enter] Save  |  [Esc] Cancel  |  [0] Clear all", 20, y2);
    }
}

string ofxMicroUIMidiController::getMappingsFilePath() const {
    return "midi_mappings.xml";
}

void ofxMicroUIMidiController::saveMappingsToXml() {
    string filePath = getMappingsFilePath();
    
    ofXml xml;
    xml.appendChild("midiMappings");
    xml.setAttribute("version", "2.0");
    
    auto root = xml.getChild("midiMappings");
    
    for (auto& m : elementToMidi) {
        auto mappingNode = root.appendChild("mapping");
        mappingNode.setAttribute("device", m.second.device);
        mappingNode.setAttribute("channel", m.second.channel);
        mappingNode.setAttribute("pitch", m.second.pitch);
        mappingNode.setAttribute("ui", m.second.ui);
        mappingNode.setAttribute("tipo", m.second.tipo);
        mappingNode.setAttribute("nome", m.second.nome);
        if (!m.second.valor.empty()) {
            mappingNode.setAttribute("valor", m.second.valor);
        }
    }
    
    if (xml.save(filePath)) {
        cout << "MIDI Learn: Saved " << elementToMidi.size() << " mappings to " << filePath << endl;
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
    
    elementToMidi.clear();
    
    for (auto& mappingNode : root.getChildren("mapping")) {
        elementListMidiController mapping;
        
        if (mappingNode.getAttribute("device")) {
            mapping.device = mappingNode.getAttribute("device").getValue();
        }
        mapping.channel = mappingNode.getAttribute("channel").getIntValue();
        mapping.pitch = mappingNode.getAttribute("pitch").getIntValue();
        mapping.ui = mappingNode.getAttribute("ui").getValue();
        mapping.tipo = mappingNode.getAttribute("tipo").getValue();
        mapping.nome = mappingNode.getAttribute("nome").getValue();
        
        if (mappingNode.getAttribute("valor")) {
            mapping.valor = mappingNode.getAttribute("valor").getValue();
        }
        
        string key = mapping.ui + "/" + mapping.nome;
        elementToMidi[key] = mapping;
    }
    
    cout << "MIDI Learn: Loaded " << elementToMidi.size() << " mappings from " << filePath << endl;
}

string ofxMicroUIMidiController::getMappingKey(int channel, int pitch, int control) const {
    return ofToString(channel) + " " + ofToString(pitch) + " " + ofToString(control);
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

bool ofxMicroUIMidiController::isElementMapped(ofxMicroUI::element* e, string& outKey) {
    if (!e) return false;
    
    string key = e->_ui->uiName + "/" + e->name;
    auto it = elementToMidi.find(key);
    if (it != elementToMidi.end()) {
        outKey = key;
        return true;
    }
    return false;
}

ofRectangle ofxMicroUIMidiController::getElementScreenRect(ofxMicroUI::element* e) {
    if (!e || !e->_ui) return ofRectangle();
    
    ofRectangle rect = e->rect;
    
    rect.x += e->_ui->rectPos.x + e->_ui->_settings->offset.x;
    rect.y += e->_ui->rectPos.y + e->_ui->_settings->offset.y;
    
    return rect;
}

void ofxMicroUIMidiController::parseMidiMessageWithLearn(ofxMidiMessage& msg) {
    if (learnMode) {
        lastReceivedMidi = msg;
        lastMidiReceiveTime = ofGetElapsedTimef();
    }
    
    if (learnMode) {
        if (_u->_lastClickedElement != nullptr && learnElement == nullptr) {
            startLearning(_u->_lastClickedElement, _u->uiName);
            _u->_lastClickedElement = nullptr;
        }
        
        for (auto& ui : _u->uis) {
            if (ui.second._lastClickedElement != nullptr && learnElement == nullptr) {
                startLearning(ui.second._lastClickedElement, ui.second.uiName);
                ui.second._lastClickedElement = nullptr;
            }
        }
        
        if (learnElement != nullptr) {
            if (msg.status == 144 || msg.status == 176) {
                finishLearning(msg);
                return;
            }
        }
    }
    
    parseMidiMessage(msg);
}
