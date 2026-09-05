// Episode library: the list of playable files on the card and the order
// they come out in. Phase 1 is one channel with a shuffle bag: the whole
// list is shuffled, dealt to exhaustion, then reshuffled, so nothing repeats
// until everything has played once.
#pragma once

#include <stdint.h>

namespace library {

// Scan `dir` for *.avi files (one level, no recursion). Returns the number
// found. Names are kept on the heap; a rescan frees the old list.
int scan(const char* dir);

int count();

// Deal the next episode from the bag. Returns a full POSIX path in a
// buffer owned by the library (valid until the next call), or nullptr if
// the library is empty.
const char* next();

// Path of the episode dealt by the last next() call, or nullptr.
const char* current();

// File name only (no directory) of the current episode, for display.
const char* currentName();

}  // namespace library
