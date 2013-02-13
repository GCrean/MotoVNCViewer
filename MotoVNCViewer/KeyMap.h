#pragma once

#include "SymKey.h"

#include "rfb.h"

// A single key press on the client may result in more than one 
// going to the server.

const unsigned int MaxKeysPerKey = 4;
const CARD32 VoidKeyCode = XK_VoidSymbol;

// keycodes contains the keysyms terminated by an VoidKeyCode.
// The releaseModifiers is a set of ORed flags indicating whether 
// particular modifier-up messages should be sent before the keys 
// and modifier-down after.

const CARD32 KEYMAP_LCONTROL = 0x0001;
const CARD32 KEYMAP_RCONTROL = 0x0002;
const CARD32 KEYMAP_LALT     = 0x0004;
const CARD32 KEYMAP_RALT     = 0x0008;

typedef struct {
    CARD32 keycodes[MaxKeysPerKey];
    CARD32 releaseModifiers;
} KeyActionSpec;

class KeyMap {
public:
	KeyMap();
	KeyActionSpec PCtoX(UINT virtkey, DWORD keyData);
private:
	// CARD32 keymap[256];
	unsigned char buf[4]; // lots of space for now
	BYTE keystate[256];
};
