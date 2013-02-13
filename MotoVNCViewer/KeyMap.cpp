#include "MotoVNCViewer.h"

// Define the keymap structure
typedef struct vncKeymapping_struct {
	UINT wincode;
	UINT Xcode;
} vncKeymapping;

static const vncKeymapping keymap[] = {
	{VK_BACK,		XK_BackSpace},
	{VK_TAB,		XK_Tab},
	{VK_CLEAR,		XK_Clear},
	{VK_RETURN,		XK_Return},
	{VK_LSHIFT,		XK_Shift_L},
	{VK_RSHIFT,		XK_Shift_R},
    {VK_SHIFT,      XK_Shift_L},
	{VK_LCONTROL,	XK_Control_L},
	{VK_RCONTROL,	XK_Control_R},
    {VK_CONTROL,	XK_Control_L},
    {VK_LMENU,		XK_Alt_L},
    {VK_RMENU,		XK_Alt_R},
    {VK_MENU,		XK_Alt_L},
    {VK_PAUSE,		XK_Pause},
    {VK_CAPITAL,	XK_Caps_Lock},
    {VK_ESCAPE,		XK_Escape},
    {VK_SPACE,		XK_space},
    {VK_PRIOR,		XK_Page_Up},
    {VK_NEXT,		XK_Page_Down},
    {VK_END,		XK_End},
    {VK_HOME,		XK_Home},
    {VK_LEFT,		XK_Left},
    {VK_UP,			XK_Up},
    {VK_RIGHT,		XK_Right},
    {VK_DOWN,		XK_Down},
    {VK_SELECT,		XK_Select},
    {VK_EXECUTE,	XK_Execute},
    {VK_SNAPSHOT,	XK_Print},
    {VK_INSERT,		XK_Insert},
    {VK_DELETE,		XK_Delete},
    {VK_HELP,		XK_Help},
    {VK_NUMPAD0,	XK_KP_0},
    {VK_NUMPAD1,	XK_KP_1},
    {VK_NUMPAD2,	XK_KP_2},
    {VK_NUMPAD3,	XK_KP_3},
    {VK_NUMPAD4,	XK_KP_4},
    {VK_NUMPAD5,	XK_KP_5},
    {VK_NUMPAD6,	XK_KP_6},
    {VK_NUMPAD7,	XK_KP_7},
    {VK_NUMPAD8,	XK_KP_8},
    {VK_NUMPAD9,	XK_KP_9},
    {VK_MULTIPLY,	XK_KP_Multiply},
    {VK_ADD,		XK_KP_Add},
    {VK_SEPARATOR,	XK_KP_Separator},   // often comma
    {VK_SUBTRACT,	XK_KP_Subtract},
    {VK_DECIMAL,	XK_KP_Decimal},
    {VK_DIVIDE,		XK_KP_Divide},
    {VK_F1,			XK_F1},
    {VK_F2,			XK_F2},
    {VK_F3,			XK_F3},
    {VK_F4,			XK_F4},
    {VK_F5,			XK_F5},
    {VK_F6,			XK_F6},
    {VK_F7,			XK_F7},
    {VK_F8,			XK_F8},
    {VK_F9,			XK_F9},
    {VK_F10,		XK_F10},
    {VK_F11,		XK_F11},
    {VK_F12,		XK_F12},
    {VK_F13,		XK_F13},
    {VK_F14,		XK_F14},
    {VK_F15,		XK_F15},
    {VK_F16,		XK_F16},
    {VK_F17,		XK_F17},
    {VK_F18,		XK_F18},
    {VK_F19,		XK_F19},
    {VK_F20,		XK_F20},
    {VK_F21,		XK_F21},
    {VK_F22,		XK_F22},
    {VK_F23,		XK_F23},
    {VK_F24,		XK_F24},
    {VK_NUMLOCK,	XK_Num_Lock},
    {VK_SCROLL,		XK_Scroll_Lock}
};


KeyMap::KeyMap() {
};


KeyActionSpec KeyMap::PCtoX(UINT virtkey, DWORD keyData) { 
	UINT numkeys = 0;
    
    KeyActionSpec kas;
    kas.releaseModifiers = 0;

    bool extended = ((keyData & 0x1000000) != 0);
    
    if (extended) { 
        switch (virtkey) {
        case VK_MENU :
            virtkey = VK_RMENU; break;
        case VK_CONTROL:
            virtkey = VK_RCONTROL; break;
        }
    }
    
    // We try looking it up in our table
    UINT mapsize = sizeof(keymap) / sizeof(vncKeymapping);
        
    // Look up the desired code in the table
    for (UINT i = 0; i < mapsize; i++)
    {
        if (keymap[i].wincode == virtkey) {
            kas.keycodes[numkeys++] = keymap[i].Xcode;
            break;
        }
    }
    
    if (numkeys != 0) UINT key = kas.keycodes[numkeys-1];

    kas.keycodes[numkeys] = VoidKeyCode;
	return kas;
};
