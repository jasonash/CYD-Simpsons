// User input: the resistive touch panel for now, the knob switches later.
// A small task on core 0 polls the panel and turns presses into events;
// the player loop drains them between frames.
#pragma once

namespace input {

enum Event {
    NONE = 0,
    TAP,    // press and release, shorter than the hold time
    HOLD,   // press held past the hold time (fires once, on the threshold)
};

// Start the polling task. Safe to call once.
void begin();

// Take the oldest pending event, or NONE. Cheap; call from any task.
Event poll();

// True while the panel is pressed. Cheap.
bool pressed();

}  // namespace input
