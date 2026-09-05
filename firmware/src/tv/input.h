// User input: the resistive touch panel for now, the knob switches later.
// A small task on core 0 polls the panel and turns presses into events;
// the player loop drains them between frames.
#pragma once

namespace input {

enum Event {
    NONE = 0,
    TAP,    // press and release, shorter than the hold time
    HOLD,   // press held past the hold time (fires once, on the threshold)
    PROBE,  // serial 'x': run the noise probe (bench only)
};

// Start the polling task. Safe to call once.
void begin();

// Take the oldest pending event, or NONE. Cheap; call from any task.
Event poll();

// Discard pending events (after a transition, so a brush during the static
// does not fire again).
void flush();

// True while the panel is pressed. Cheap.
bool pressed();

// Screen position (in the current rotation) where the last press started.
// Returns false if there has been no press yet.
bool lastPoint(int* x, int* y);

// Tell the mapper the screen is rotated 180 degrees (settings::flip).
void setFlipped(bool flipped);

}  // namespace input
