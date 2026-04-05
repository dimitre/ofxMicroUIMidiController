# ofxMicroUIMidiController - MIDI Learn Feature

## Overview
MIDI Learn feature for ofxMicroUIMidiController, inspired by Ableton Live's implementation. Allows quick mapping of MIDI controllers to UI elements without manually editing text files.

---

## Current State

### Existing Bidirectional MIDI (Working)
- Mapping via text files (e.g., `APC MINI.txt`)
- Format: `<channel> <pitch> <control> <status> <ui_name> <type> <element_name> [value]`
- Bidirectional feedback: UI changes → LED updates on APC Mini
- Supports: bool, float, int, radio, preset, bang, hold

### Hardware
- Primary: **APC Mini**
- 64 button grid (8x8) with RGB LEDs
- 16 side buttons (8 right, 8 bottom) with LEDs
- 9 faders (CC messages)
- Shift button

---

## Goals

1. **Quick Mapping**: Map MIDI controls to UI elements without file editing
2. **Visual Feedback**: Show which elements are mapped and in learn mode
3. **Persistent Storage**: Save mappings back to file or new format
4. **Maintain Bidirectional**: Keep existing LED feedback functionality

---

## User Flow

### Enter Learn Mode
```
[Click "MIDI" button or press 'M']
         ↓
   learnMode = true
   UI shows "LEARN MODE" indicator
```

### Map a Control
```
[Click UI element "volume"]           [Move APC Mini knob]
         ↓                                       ↓
   Element highlights yellow                   ↓
   waiting for MIDI...              Capture: CC 48, Ch 1
         ↓                                       ↓
         └────────── Mapping created ───────────┘
         ↓
   Auto-detect type: float
   Save: 176 1 48 0 master float volume
   Element returns to normal
   LED feedback active immediately
```

### Exit Learn Mode
```
[Click "MIDI" button again or press 'M']
         ↓
   learnMode = false
   All highlights removed
```

---

## Design Decisions

### Open Questions

#### 1. Save Behavior
- [ ] **Auto-save**: Immediately append to `APC MINI.txt`
- [ ] **Manual save**: Keep in memory, "Save Mappings" button
- [ ] **Hybrid**: Auto-save to separate `APC MINI_learned.txt`

#### 2. Unlearning / Clearing
- [ ] Right-click mapped element → "Clear MIDI"
- [ ] "Clear All Mappings" button
- [ ] Separate "Unlearn Mode" (like Ableton)
- [ ] Keyboard shortcut (Delete key on selected mapping)

#### 3. Visual Feedback in UI
- [ ] Small LED dot on mapped elements
- [ ] Different border color for mapped elements
- [ ] Tooltip showing MIDI CC/note info
- [ ] Separate "MIDI Mappings" window/panel

#### 4. Conflict Handling
- [ ] **Block**: Prevent duplicate mappings
- [ ] **Swap**: Move old mapping to new element
- [ ] **Allow**: Multiple elements per MIDI control
- [ ] **Ask**: Dialog to choose action

#### 5. Multiple Controllers
- [ ] Support multiple APC Minis?
- [ ] Separate mapping files per device?
- [ ] Device selection in learn mode?

#### 6. Learn Mode UI
- [ ] Global "MIDI" button in UI
- [ ] Per-element right-click menu
- [ ] Keyboard shortcut only
- [ ] All of the above

---

## Implementation Notes

### Data Structures

```cpp
// Existing (keep compatibility)
struct elementListMidiController {
    string ui;
    string tipo;
    string nome;
    string valor = "";
    int channel;
    int pitch;
};

// New: Learn state
struct LearnState {
    bool active = false;
    ofxMicroUI::element* element = nullptr;
    string uiName;
    uint64_t startFrame;  // Timeout if no MIDI received
};

// New: Visual indicator
struct MappingVisual {
    ofColor mappedColor = ofColor(0, 255, 0);      // Green dot
    ofColor learningColor = ofColor(255, 255, 0);  // Yellow highlight
    float dotSize = 4;
    ofVec2f dotOffset = {2, 2};
};
```

### File Format Considerations

#### Option A: Extend existing format
```
# Learned mappings appended with timestamp
176 1 48 0 master float volume  # learned 2026-04-05 09:32:00
```

#### Option B: JSON format (new file)
```json
{
  "device": "APC MINI",
  "mappings": [
    {
      "channel": 1,
      "pitch": 48,
      "control": 0,
      "status": 176,
      "ui": "master",
      "type": "float",
      "name": "volume",
      "learned": true,
      "date": "2026-04-05T09:32:00"
    }
  ]
}
```

#### Option C: Hybrid (backward compatible)
- Keep `.txt` for hand-edited mappings
- `.json` for learned mappings (loaded after, overrides)

### Decision: XML Format Only

**Rationale:**
- MIDI Learn becomes the primary mapping interface
- No need to maintain hand-edited text files
- ofXml built into openFrameworks (no extra dependencies)
- YAML would require yaml-cpp library (heavier binary)

**File naming:** `{midiDeviceName}.xml` (e.g., `APC MINI.xml`)

**Format:**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<midiMappings device="APC MINI" version="1.0">
    <mapping channel="1" pitch="0" control="48" status="176"
             ui="master" type="float" name="volume"
             learned="true" date="2026-04-05T09:32:00"/>
    <mapping channel="1" pitch="56" control="0" status="144"
             ui="master" type="preset" name="0"
             learned="true" date="2026-04-05T09:33:00"/>
</midiMappings>
```

**Migration:** Old `.txt` files can be manually converted once, then Learn mode takes over.

---

## APC Mini Specifics

### Button Grid (8x8)
- Notes: 0-63 (bottom-left to top-right)
- Status: Note On (144) / Note Off (128)
- LED feedback: `sendNoteOn(channel, pitch, color)`
- Colors: 0=off, 1=green, 3=red, 5=yellow, etc.

### Side Buttons
- Right side: 64-71 (bottom to top)
- Bottom: 82-89 (left to right)
- Shift: 98

### Faders
- CC 48-56 (left to right)
- CC 56 = master fader

### Learn Mode LED Behavior
```cpp
// When in learn mode: gentle pulsing on all buttons?
// When element selected: blink that element's mapped control?
// When mapping complete: brief flash of confirmation
```

---

## UI Integration with ofxMicroUI

### Option 1: External Button
```cpp
// In ofApp
ofxMicroUI midiLearnUI;
midiLearnUI.createFromText("learnbutton.txt");
// Contains: toggle midiLearn
```

### Option 2: Integrated into ofxMicroUI
```cpp
// Auto-add "MIDI" button to every UI
// Requires modification to ofxMicroUI core
```

### Option 3: Right-click Context Menu
```cpp
// On element right-click:
// - MIDI Learn
// - Clear MIDI Mapping
// - Show MIDI Info
```

---

## Highlight Implementation Approaches

### Approach A: Minimal Change to ofxMicroUI (Recommended)
Add only `_lastClickedElement` pointer to ofxMicroUI, draw highlight in `ofxMicroUIMidiController`.

**Changes to ofxMicroUI.h:**
```cpp
// Add to ofxMicroUI class
ofxMicroUI::element* _lastClickedElement = nullptr;
```

**Changes to ofxMicroUI.cpp (in mouseUI):**
```cpp
// When element is clicked
if (pressed) {
    _lastClickedElement = e;  // Track which element
    if (_masterUI != nullptr) {
        _masterUI->_lastClickedUI = this;
    }
}
```

**Highlight drawing in ofxMicroUIMidiController:**
```cpp
void ofxMicroUIMidiController::drawLearnModeOverlay() {
    if (!learnState.active) return;
    
    // Draw "LEARN MODE" text somewhere
    ofDrawBitmapString("MIDI LEARN MODE", 10, 20);
    
    // Highlight last clicked element
    if (_u->_lastClickedElement != nullptr) {
        ofPushStyle();
        ofNoFill();
        ofSetLineWidth(3);
        ofSetColor(255, 255, 0);  // Yellow highlight
        ofDrawRectangle(_u->_lastClickedElement->rect);
        ofPopStyle();
    }
}
```

**Pros:**
- Minimal intrusion into ofxMicroUI
- ofxMicroUIMidiController controls the visual style
- Easy to change highlight color/behavior

**Cons:**
- Highlight drawn after UI FBO (might need separate pass)
- Slight coupling (ofxMicroUIMidiController needs to know about _lastClickedElement)

---

### Approach B: Overlay FBO
Create a separate FBO in ofxMicroUIMidiController for highlights, drawn on top of UI.

```cpp
class ofxMicroUIMidiController {
    ofFbo highlightFbo;
    
    void drawHighlights() {
        highlightFbo.begin();
        ofClear(0, 0);
        
        // Draw all mapped elements (green dot)
        for (auto& m : midiControllerMap) {
            drawMappedIndicator(m.second);
        }
        
        // Draw learning element highlight
        if (learnState.active && learnState.element != nullptr) {
            drawLearningHighlight(learnState.element);
        }
        
        highlightFbo.end();
        highlightFbo.draw(0, 0);
    }
};
```

**Pros:**
- Zero changes to ofxMicroUI
- Full control over rendering

**Cons:**
- Another FBO to manage
- Coordinate sync issues if UI moves

---

### Approach C: Modify Element Drawing (Invasive)
Add draw callback/hook to elements.

```cpp
// In ofxMicroUIElements.h
class element {
    std::function<void()> customDraw = nullptr;
    void draw() {
        drawElement();
        if (useLabel) drawLabel();
        if (customDraw) customDraw();  // Hook for MIDI controller
    }
};
```

**Pros:**
- Highlight integrated into element's draw cycle
- No coordinate issues

**Cons:**
- Significant change to ofxMicroUI
- All elements affected

---

## Decision: Keep State in ofxMicroUIMidiController, Track Element in ofxMicroUISoftware

**Key Insight:** `ofxMicroUISoftware` already manages all UIs. It should track the last clicked element across all UIs, then `ofxMicroUIMidiController` can read it from there.

**Architecture:**
```
ofxMicroUIMidiController (learn mode state + MIDI handling)
         │
         ├── uses ──► ofxMicroUISoftware * soft
         │                    │
         │                    ├── ofxMicroUI * _ui (master)
         │                    │       └── _lastClickedUI (existing)
         │                    ├── unordered_map uis (sub-UIs)
         │                    └── _lastClickedElement (NEW - tracks element)
         │                    └── _lastClickedUIName (NEW - which UI)
         │
         └── draws highlights ──► on top of UI FBOs
```

**ofxMicroUISoftware changes:**
```cpp
// ofxMicroUISoftware.h
class ofxMicroUISoftware {
    // ... existing ...
    ofxMicroUI::element* _lastClickedElement = nullptr;  // NEW
    string _lastClickedUIName;  // NEW - "master" or sub-UI name
};

// ofxMicroUISoftware.cpp - in uiEvents or mouse handler
void ofxMicroUISoftware::uiEvents(ofxMicroUI::element& e) {
    // Track last clicked element from any UI
    _lastClickedElement = &e;
    _lastClickedUIName = e._ui->uiName;
}
```

**ofxMicroUIMidiController reads from software:**
```cpp
void ofxMicroUIMidiController::update() {
    if (!learnMode) return;
    
    if (learnElement == nullptr && soft->_lastClickedElement != nullptr) {
        // New element clicked - start learning
        startLearning(soft->_lastClickedElement, soft->_lastClickedUIName);
        
        // Clear so we don't re-trigger
        soft->_lastClickedElement = nullptr;
    }
}
```

**ofxMicroUIMidiController implementation:**
```cpp
class ofxMicroUIMidiController {
    ofxMicroUISoftware * soft = nullptr;
    ofxMicroUI * _u = nullptr;  // master UI (from soft)
    
    // Learn mode state
    bool learnMode = false;
    ofxMicroUI::element* learnElement = nullptr;
    string learnUIName;
    
public:
    void toggleLearnMode();
    void update();  // Check for clicked element
    void draw();    // Draw highlights
    
private:
    void startLearning(ofxMicroUI::element* e, const string& uiName);
    void finishLearning(const ofxMidiMessage& msg);
    void drawLearnOverlay();
    void drawMappedIndicators();
};
```

**Usage flow:**
```cpp
// 1. Toggle learn mode (key 'M' or UI button)
midiController.toggleLearnMode();

// 2. User clicks element -> ofxMicroUI sets _lastClickedElement

// 3. ofxMicroUIMidiController::update() detects it
void ofxMicroUIMidiController::update() {
    if (!learnMode) return;
    
    if (learnElement == nullptr) {
        // Check all UIs for last clicked element
        if (_u->_lastClickedElement != nullptr) {
            startLearning(_u->_lastClickedElement, "master");
        }
        // Also check sub-UIs...
    }
    
    // Check for right-click on mapped elements to remove
}

// 4. User moves MIDI control -> mapping created
void ofxMicroUIMidiController::newMidiMessage(ofxMidiMessage& msg) {
    if (learnMode && learnElement != nullptr) {
        finishLearning(msg);  // Create mapping
        learnElement = nullptr;  // Ready for next
    }
}

// 5. Draw modal overlay (auto-registered via ofEvents)
void ofxMicroUIMidiController::onDraw(ofEventArgs& args) {
    if (!learnMode) return;
    
    ofPushStyle();
    
    // Modal background - dim everything
    ofFill();
    ofSetColor(0, 0, 0, 150);  // Semi-transparent black
    ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
    
    // Banner at top
    ofSetColor(255, 255, 0);
    ofDrawBitmapString("MIDI LEARN MODE", 20, 40);
    
    // Highlight currently learning element (yellow) or just mapped (green)
    // Draw these ON TOP of the dim overlay so they pop
    if (learnElement != nullptr) {
        ofNoFill();
        ofSetLineWidth(4);
        ofSetColor(255, 255, 0);  // Yellow = waiting for MIDI
        ofDrawRectangle(learnElement->rect);
        
        // Fill with slight yellow tint
        ofFill();
        ofSetColor(255, 255, 0, 30);
        ofDrawRectangle(learnElement->rect);
    } else if (justMappedElement != nullptr) {
        ofNoFill();
        ofSetLineWidth(3);
        ofSetColor(0, 255, 0);  // Green = successfully mapped
        ofDrawRectangle(justMappedElement->rect);
        
        ofFill();
        ofSetColor(0, 255, 0, 30);
        ofDrawRectangle(justMappedElement->rect);
    }
    
    // Rainbow MIDI bar
    drawMidiRainbowBar();
    
    // Status bar at bottom
    drawStatusBar();
    
    ofPopStyle();
}

// Constructor registers events automatically
ofxMicroUIMidiController::ofxMicroUIMidiController(ofxMicroUISoftware* _soft, string device) 
    : soft(_soft) {
    // ... existing setup ...
    ofAddListener(ofEvents().update, this, &ofxMicroUIMidiController::onUpdate);
    ofAddListener(ofEvents().draw, this, &ofxMicroUIMidiController::onDraw);  // NEW
    ofAddListener(ofEvents().exit, this, &ofxMicroUIMidiController::onExit);
}
```

---

## Learn Mode UI Design

### Modal Overlay

When learn mode is active, the entire window gets a **semi-transparent black overlay** (modal effect):

```
┌─────────────────────────────────────────────────────────┐
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│ ░░  ┌─────────┐  ┌─────────┐  ┌─────────┐          ░░ │
│ ░░  │ slider  │  │ toggle  │  │  radio  │          ░░ │  ← UI visible
│ ░░  └─────────┘  └─────────┘  └─────────┘          ░░ │    but dimmed
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│                                                         │
│   MIDI LEARN MODE                                       │  ← Bright text
│                                                         │
│   ╔═════════════════════════════════════════════════╗   │
│   ║ ▓▓▓▓░░░▓▓▓░░░░▓▓░░▓▓▓▓▓░░░░▓▓▓░░░▓▓░░░▓▓▓▓▓▓▓▓ ║   │  ← Rainbow bar
│   ╚═════════════════════════════════════════════════╝   │
│                                                         │
│   Click element → Move MIDI | Right-click: remove       │  ← Status
│   [Enter] Save | [Esc] Cancel                           │
└─────────────────────────────────────────────────────────┘
        ↑
   Selected element has
   yellow/green border
   (drawn on top of overlay)
```

### Visual States (Learn Mode Only)

| State | Visual | Meaning | Action |
|-------|--------|---------|--------|
| Learning | Yellow border (4px) + slight yellow fill | Waiting for MIDI input | Move MIDI control |
| Just Mapped | Green border (3px) + slight green fill | Successfully mapped | Right-click to remove |
| Modal BG | Black @ 60% opacity | Learn mode active | - |

**Effect:** UI elements stay visible for reference but are clearly "in the background". Selected/mapped elements pop with bright borders on top of the overlay.

### One-to-One Mapping Rule

**One MIDI control maps to exactly one UI element.**

When mapping:
```
[MIDI CC 48] already mapped to "filter"
         ↓
[User maps CC 48 to "volume"]
         ↓
   Old mapping removed: "filter" no longer mapped
   New mapping created: "volume" mapped to CC 48
```

This prevents confusion and simplifies removal (just right-click the mapped element).

### Status Bar

Simple text at bottom of screen:

```
┌─────────────────────────────────────────────────────────┐
│                                                         │
│  [UI elements with highlights]                          │
│                                                         │
│  Click element → Move MIDI control | Right-click: remove│
│  [Enter] Save | [Esc] Cancel                            │
└─────────────────────────────────────────────────────────┘
```

No buttons - just keyboard shortcuts:
- **Enter** = Save mappings to file
- **Escape** = Cancel (discard changes)

---

## MIDI Mapping Visualization (Rainbow Mode)

When learn mode is active, show a **rainbow HSV bar** that visualizes all MIDI mappings:

```
┌─────────────────────────────────────────────────────────┐
│  MIDI LEARN MODE                                        │
│                                                         │
│  ┌─────────────────────────────────────────────────┐    │
│  │ [UI elements with yellow/green highlights]      │    │
│  └─────────────────────────────────────────────────┘    │
│                                                         │
│  ╔═══════════════════════════════════════════════════╗  │
│  ║ ▓▓▓▓░░░▓▓▓░░░░▓▓░░▓▓▓▓▓░░░░▓▓▓░░░▓▓░░░▓▓▓▓▓▓▓▓ ║  │
│  ║ red    yellow   green   cyan     blue    magenta ║  │
│  ╚═══════════════════════════════════════════════════╝  │
│  0    16    32    48    64    80    96   112   127     │
│  CC values (hue mapped to CC number)                    │
│                                                         │
│  Click element → Move MIDI | Right-click: remove        │
│  [Enter] Save | [Esc] Cancel                            │
└─────────────────────────────────────────────────────────┘
```

### Rainbow Bar Design

```cpp
void ofxMicroUIMidiController::drawMidiRainbowBar() {
    float barY = ofGetHeight() - 60;
    float barHeight = 20;
    float barX = 10;
    float barWidth = ofGetWidth() - 20;
    
    // Draw full rainbow gradient (CC 0-127 mapped to hue 0-255)
    for (int cc = 0; cc < 128; cc++) {
        float x = ofMap(cc, 0, 127, barX, barX + barWidth);
        float w = barWidth / 128.0f;
        
        // Hue based on CC number
        ofColor c = ofColor::fromHsb(ofMap(cc, 0, 127, 0, 255), 200, 255);
        
        // Check if this CC is mapped
        bool isMapped = isCCMapped(cc);
        
        if (isMapped) {
            // Mapped CC: bright, saturated
            ofSetColor(c);
            ofFill();
            ofDrawRectangle(x, barY, w + 1, barHeight);
            
            // White border for visibility
            ofNoFill();
            ofSetColor(255);
            ofSetLineWidth(1);
            ofDrawRectangle(x, barY, w + 1, barHeight);
        } else {
            // Unmapped CC: dark, subtle
            ofSetColor(c.r * 0.2, c.g * 0.2, c.b * 0.2);
            ofFill();
            ofDrawRectangle(x, barY, w + 1, barHeight);
        }
    }
    
    // Draw CC number labels
    ofSetColor(200);
    for (int cc = 0; cc <= 127; cc += 16) {
        float x = ofMap(cc, 0, 127, barX, barX + barWidth);
        ofDrawBitmapString(ofToString(cc), x, barY + barHeight + 12);
    }
    
    // Highlight currently selected/learned CC
    if (currentLearnCC >= 0) {
        float x = ofMap(currentLearnCC, 0, 127, barX, barX + barWidth);
        ofNoFill();
        ofSetColor(255);
        ofSetLineWidth(2);
        ofDrawRectangle(x - 2, barY - 2, barWidth / 128.0f + 4, barHeight + 4);
    }
}
```

### Color Mapping (CC Only)

Hue mapped to CC for maximum distinction:

| CC | Hue | Color |
|----|-----|-------|
| 0 | 0° | Red |
| 21 | 60° | Yellow |
| 42 | 120° | Green |
| 64 | 180° | Cyan |
| 85 | 240° | Blue |
| 106 | 300° | Magenta |
| 127 | 340° | Pink/Red |

```cpp
// Map CC 0-127 to full hue range 0-360
float hue = ofMap(cc, 0, 127, 0, 360);
ofColor c = ofColor::fromHsb(hue, 255, 255);
```

Adjacent CCs have ~2.8° hue difference - visible but subtle.

### Benefits

1. **Immediate overview** - See all mapped CCs at a glance
2. **Pattern recognition** - Notice if mappings are clustered or spread  
3. **Find gaps** - Easily spot available CCs (dim sections) for new mappings
4. **Visual feedback** - After mapping, see the new CC light up in the bar

### New User Experience

**First time (no mappings):**
```
╔═══════════════════════════════════════════════════╗
║ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ ║  <- All dim (unmapped)
╚═══════════════════════════════════════════════════╝
```

**After mapping volume → CC 48:**
```
╔═══════════════════════════════════════════════════╗
║ ░░░░░░░░░░░░░░░░▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░ ║  <- CC 48 bright green
╚═══════════════════════════════════════════════════╝
                  ^^^^^
                  CC 48 (cyan/green)
```

**After mapping 3 more controls:**
```
╔═══════════════════════════════════════════════════╗
║ ░░░▓▓▓▓░░░░░░░░░▓▓▓▓▓░░░░░░░▓▓▓░░░░░░░░░▓▓▓▓░░░░ ║  <- 4 mapped CCs visible
╚═══════════════════════════════════════════════════╝
     ^^^^           ^^^^^        ^^^           ^^^^
     CC 16          CC 48       CC 72          CC 108
```

### Future: Channel Visualization

If needed later, could use brightness/saturation for channel:

```cpp
// Channel 1-16 affects brightness
float brightness = ofMap(channel, 1, 16, 255, 100);
ofColor c = ofColor::fromHsb(hue, 255, brightness);
```

For now: **CC only, full hue range** - simple and effective for APC Mini (mostly Ch 1).

### X Button for Removing Mappings

```cpp
struct MappedElementUI {
    ofxMicroUI::element* element;
    ofRectangle xButtonRect;  // Small X on the right side
    string mappingKey;        // For removing from midiControllerMap
};

vector<MappedElementUI> mappedElementsForCurrentMidi;

void ofxMicroUIMidiController::drawMappedElements() {
    for (auto& m : mappedElementsForCurrentMidi) {
        // Green border around mapped element
        ofNoFill();
        ofSetLineWidth(2);
        ofSetColor(0, 255, 0);
        ofDrawRectangle(m.element->rect);
        
        // X button on right side of element
        ofRectangle& r = m.element->rect;
        m.xButtonRect = ofRectangle(r.x + r.width - 12, r.y + 2, 10, 10);
        
        ofFill();
        ofSetColor(255, 0, 0);  // Red X button
        ofDrawRectangle(m.xButtonRect);
        
        ofSetColor(255);
        ofDrawBitmapString("x", m.xButtonRect.x + 3, m.xButtonRect.y + 8);
    }
}

void ofxMicroUIMidiController::checkRemoveMappingClicks() {
    // Check if mouse clicked any X button
    for (auto& m : mappedElementsForCurrentMidi) {
        if (m.xButtonRect.inside(ofGetMouseX(), ofGetMouseY())) {
            if (ofGetMousePressed(0)) {
                removeMapping(m.mappingKey);
                rebuildMappedElementsList();
            }
        }
    }
}
```

### Right-click to Remove (Selected)

Right-click on a mapped element shows context menu:

```cpp
void ofxMicroUIMidiController::mousePressed(int x, int y, int button) {
    if (!learnMode) return;
    
    if (button == 2) {  // Right click
        ofxMicroUI::element* e = findElementAt(x, y);
        if (e != nullptr && isMapped(e)) {
            removeMapping(e);  // Immediate removal
            // Or: showContextMenu(e);  // "Remove MIDI Mapping" / "Cancel"
        }
    }
}
```

**Decision:** Right-click is cleaner, no UI clutter from X buttons.

---

## Save / Cancel Buttons

In learn mode, show two buttons at the bottom (or top) of the screen:

```
┌─────────────────────────────────────────────────────────┐
│  MIDI LEARN MODE                                        │
│                                                         │
│  [UI elements with highlights]                          │
│                                                         │
│                              [ SAVE ]  [ CANCEL ]       │
└─────────────────────────────────────────────────────────┘
```

### Button Behavior

| Button | Action |
|--------|--------|
| **SAVE** | Save all learned mappings to file, exit learn mode |
| **CANCEL** | Discard all changes made in this learn session, exit learn mode |

### Implementation

```cpp
class ofxMicroUIMidiController {
    // ... existing ...
    
    // Learn mode buttons
    ofRectangle saveButtonRect;
    ofRectangle cancelButtonRect;
    
    // Track changes for cancel functionality
    vector<elementListMidiController> mappingsBackup;  // Before learn mode
    vector<elementListMidiController> newMappings;     // Added in learn mode
    
    void enterLearnMode() {
        learnMode = true;
        // Backup current mappings
        mappingsBackup = getCurrentMappings();
        newMappings.clear();
    }
    
    void saveLearnMode() {
        // Append newMappings to file
        saveLearnedMappings();
        learnMode = false;
        learnElement = nullptr;
    }
    
    void cancelLearnMode() {
        // Restore from backup
        restoreMappings(mappingsBackup);
        learnMode = false;
        learnElement = nullptr;
    }
    
    void drawStatusBar() {
        string status;
        if (learnElement == nullptr) {
            status = "Click element to map | Right-click mapped element to remove | [Enter] Save | [Esc] Cancel";
        } else {
            status = "Move MIDI control to map '" + learnElement->name + "' | [Esc] Cancel";
        }
        
        ofSetColor(255, 255, 0);
        ofDrawBitmapString(status, 10, ofGetHeight() - 20);
    }
};
```

---

## Updated Data Structures

```cpp
class ofxMicroUIMidiController {
    // ... existing ...
    
    // Learn mode state
    bool learnMode = false;
    ofxMicroUI::element* learnElement = nullptr;
    string learnUIName;
    
    // For showing existing mappings
    struct MappedUI {
        ofxMicroUI::element* element;
        ofRectangle xButton;
        string key;  // midiControllerMap key
    };
    vector<MappedUI> currentMappings;
    
    // Last MIDI message (for finding mappings to same control)
    ofxMidiMessage lastMidiMessage;
    
    void rebuildCurrentMappings();  // Update list when MIDI changes
    void drawCurrentMappings();
    void checkXButtonClicks();
};
```

### Flow with Existing Mappings Display

```
[Enter Learn Mode]
         ↓
[Click element "volume"]
         ↓
   learnElement = volume
   No MIDI yet, so no existing mappings shown
         ↓
[Move APC Mini knob (CC 48)]
         ↓
   Check: Are there other elements mapped to CC 48?
   Yes: "filter" also mapped to CC 48
         ↓
   Show both with green borders + X buttons:
   ┌─────────────┐  ┌─────────────┐
   │ volume  [x] │  │ filter  [x] │
   └─────────────┘  └─────────────┘
      (learning)     (also CC 48)
         ↓
   User can click [x] to remove either mapping
```

**Pros:**
- Minimal change to ofxMicroUI (none!)
- Single `_lastClickedElement` in software manages all UIs
- Clean separation: Software tracks clicks, MIDI controller uses them
- No UI iteration needed

**Cons:**
- ofxMicroUISoftware needs element tracking
- Slight coupling between software and MIDI controller


---

## Type Detection for MIDI Learn

Auto-detect element type using `dynamic_cast` (same pattern as existing code):

```cpp
string detectElementType(ofxMicroUI::element* e) {
    if (dynamic_cast<ofxMicroUI::slider*>(e)) {
        // Check if int or float slider
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
    // Note: "preset" and "bang" are special - usually mapped manually
    return "unknown";
}
```

### Applying MIDI Values (Existing Pattern)

Same logic as current `parseMidiMessage()`:

```cpp
void applyMidiValue(const string& tipo, ofxMicroUI::element* e, 
                    float midiValue, const ofxMidiMessage& msg) {
    if (tipo == "float" || tipo == "int") {
        ofxMicroUI::slider* s = dynamic_cast<ofxMicroUI::slider*>(e);
        if (s) {
            float valor = ofMap(midiValue, 0, 127, s->min, s->max);
            s->set(valor);
        }
    }
    else if (tipo == "bool") {
        ofxMicroUI::toggle* t = dynamic_cast<ofxMicroUI::toggle*>(e);
        if (t && msg.status == 144) {  // Note on
            t->flip();
        }
    }
    else if (tipo == "radio") {
        ofxMicroUI::radio* r = dynamic_cast<ofxMicroUI::radio*>(e);
        if (r) {
            int nElements = r->elements.size();
            int index = ofMap(midiValue, 0, 127, 0, nElements - 1);
            r->set(index);
        }
    }
    else if (tipo == "hold") {
        ofxMicroUI::hold* h = dynamic_cast<ofxMicroUI::hold*>(e);
        if (h) {
            h->set(msg.status == 144);  // true on note on, false on note off
        }
    }
    // etc...
}
```

---

## Code Structure

```cpp
class ofxMicroUIMidiController {
    // ... existing members ...
    
public:
    // Learn mode control
    void toggleLearnMode();
    void setLearnMode(bool active);
    bool isLearnMode() { return learnState.active; }
    
    // Visual feedback
    void onDraw(ofEventArgs& args);  // Auto-registered overlay
    
    // Mapping management
    void startLearning(ofxMicroUI::element* e, const string& uiName);
    void finishLearning(const ofxMidiMessage& msg);
    void clearMapping(const string& uiName, const string& elementName);
    void clearAllMappings();
    
    // Type detection
    string detectElementType(ofxMicroUI::element* e);
    void applyMidiValue(const string& tipo, ofxMicroUI::element* e, 
                        float midiValue, const ofxMidiMessage& msg);
    
    // Persistence
    void saveLearnedMappings();
    void loadLearnedMappings();
    
private:
    LearnState learnState;
    string learnedMappingsFile = "APC MINI.xml";
    
    // Track which elements have mappings (for visual feedback)
    map<string, bool> hasMapping;  // key: "uiName/elementName"
};
```

---

## Future Ideas

- [ ] **MIDI Feedback Intensity**: Scale LED brightness to value
- [ ] **Shift+Click**: Fine-tune mapping (set min/max range)
- [ ] **MIDI Thru**: Pass unmapped messages to other apps
- [ ] **OSC Integration**: Same learn mode for OSC messages
- [ ] **Relative Mode**: Support endless encoders (infinite rotation)

---

## Decisions Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-04-05 | Created MIDILEARN.md | Organize design before coding |
| 2026-04-05 | Use XML for learned mappings | ofXml built into OF, no yaml-cpp dependency |
| 2026-04-05 | Track `_lastClickedElement` in ofxMicroUISoftware | Software manages all UIs, single source of truth |
| 2026-04-05 | Modal overlay with rainbow bar | Visual clarity, Ableton-style UX |
| 2026-04-05 | Right-click to remove mappings | Clean UI, no X buttons |
| 2026-04-05 | One-to-one MIDI mapping | Prevents confusion, simpler removal |
| 2026-04-05 | Enter/Esc for save/cancel | Keyboard-driven, no buttons needed |
| 2026-04-05 | XML only, drop .txt support | MIDI Learn becomes primary interface |
| 2026-04-05 | Auto-registered draw via ofEvents | No explicit draw() call needed in ofApp |
| 2026-04-05 | Use dynamic_cast for type detection | Consistent with existing ofxMicroUI patterns |
| 2026-04-05 | Rainbow bar shows mapped CCs only | Unmapped = dim, mapped = bright with hue |
| 2026-04-05 | Store device name in mappings | Foundation for multi-device support |

---

## Implementation Status

### Completed ✅
- [x] Add `_lastClickedElement` to ofxMicroUI
- [x] Add LearnState and methods to ofxMicroUIMidiController.h
- [x] Implement type detection (detectElementType)
- [x] Implement learn mode methods (startLearning, finishLearning)
- [x] Implement XML save/load methods
- [x] Implement onDraw with modal overlay and rainbow bar
- [x] Implement right-click removal
- [x] Update example project
- [x] Fix compilation errors (constructor, const correctness, variable names)

### Pending 🔄
- [ ] Test build with chalet buildrun
- [ ] Test with actual APC Mini hardware
- [ ] Verify XML loading on startup
- [ ] Fine-tune visual feedback timing

## Next Steps

1. Build and verify no compilation errors
2. Test learn mode workflow with hardware
3. Verify mappings persist after save/load
4. Iterate based on usage feedback
