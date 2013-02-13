#include "MotoVNCViewer.h"

//Parameters
HWND					hWnd;											//Main Window Handle
HINSTANCE				hInst;											//Instance Handle
DWORD					dwWidth;										//Screen Width
DWORD					dwHeight;										//Screen Height
INT						iScale;											//Used for Scaling
HFONT					hFont;											//Handle To Font
HINSTANCE				hDDraw;											//Handle For DirectDraw Library
IDirectDraw			*	pdd;											//Direct Draw Interface
DDSURFACEDESC			ddsd;											//Surface Descriptor
IDirectDrawSurface  *	pSurf;											//Primary Surface Interface
IDirectDrawSurface  *	pBackSurf;										//Background Surface Interface
LPCWSTR					szMessage = NULL;								//Displayed Message
CHAR					szAddress[40] = {0};							//IP Address of Server
DWORD					dwPort = 5900;									// IP Port of Server
KeyMap					keymap;

//Internal Windows Messages
static LRESULT WndCreate(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndDestory(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndPaint(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndLButtonDown(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndLButtonUp(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndMouseMove(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndKeyEvent(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndCharEvent(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);

// Message dispatch table for WindowProc
static const struct DecodeWindowMessage Messages[] = 
{
	WM_PAINT,WndPaint,
	WM_CREATE,WndCreate,
	WM_DESTROY,WndDestory,
	WM_LBUTTONUP,WndLButtonUp,
	WM_LBUTTONDOWN,WndLButtonUp,
	WM_MOUSEMOVE,WndMouseMove,
	WM_KEYDOWN,WndKeyEvent,
	WM_KEYUP,WndKeyEvent,
	WM_SYSKEYDOWN,WndKeyEvent,
	WM_SYSKEYUP,WndKeyEvent,
	WM_CHAR,WndCharEvent,
	WM_SYSCHAR,WndCharEvent
};
/****************************************************************************************************/
static LRESULT WndCreate(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	SHFullScreen(hWnd, SHFS_HIDESIPBUTTON | SHFS_HIDETASKBAR | SHFS_HIDESTARTICON);
	hFont = BuildFont(SCALE(30),FALSE,FALSE);
	return 0;
}
/****************************************************************************************************/
static LRESULT WndDestory(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	DeleteObject(hFont);
	PostQuitMessage(0);
	return 0;
}
/****************************************************************************************************/
static LRESULT WndPaint(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
PAINTSTRUCT ps;	

	BeginPaint(hWnd, &ps);
	pSurf->Blt(&ps.rcPaint,pBackSurf,&ps.rcPaint,NULL,0);
	if (szMessage)
	{
		RECT Rct= {0,0,dwWidth,dwHeight};
		SetTextColor(ps.hdc,RGB(255,255,255));
		SetBkMode(ps.hdc,TRANSPARENT);
		HGDIOBJ hOldFont = SelectObject(ps.hdc,hFont);
		DrawText(ps.hdc,szMessage,-1,&Rct,DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		SelectObject(ps.hdc,hOldFont);
	}
	EndPaint(hWnd, &ps);
	return 0;
}
/****************************************************************************************************/
static LRESULT WndLButtonDown(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	SendMouseEvent(rfbButton1Mask,LOWORD(lParam),HIWORD(lParam));
	return TRUE;
}
/****************************************************************************************************/
static LRESULT WndLButtonUp(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	SendMouseEvent(0,LOWORD(lParam),HIWORD(lParam));
	return TRUE;
}
/****************************************************************************************************/
static LRESULT WndMouseMove(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	SendMouseEvent(rfbButton1Mask,LOWORD(lParam),HIWORD(lParam));
	return TRUE;
}
/****************************************************************************************************/
static LRESULT WndKeyEvent(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
KeyActionSpec kas = keymap.PCtoX(wParam, lParam);    
BOOL bDown = ((lParam & 0x80000000l) == 0) ? TRUE : FALSE;

    if (kas.releaseModifiers & KEYMAP_LCONTROL) {
        SendKeyEvent(XK_Control_L, FALSE );
    }
    if (kas.releaseModifiers & KEYMAP_LALT) {
        SendKeyEvent(XK_Alt_L, FALSE );
    }
    if (kas.releaseModifiers & KEYMAP_RCONTROL) {
        SendKeyEvent(XK_Control_R, FALSE );
    }
    if (kas.releaseModifiers & KEYMAP_RALT) {
        SendKeyEvent(XK_Alt_R, FALSE );
    }

    for (int i = 0; kas.keycodes[i] != XK_VoidSymbol && i < MaxKeysPerKey; i++) {
        SendKeyEvent(kas.keycodes[i], bDown );
    }

    if (kas.releaseModifiers & KEYMAP_RALT) {
        SendKeyEvent(XK_Alt_R, TRUE );
    }
    if (kas.releaseModifiers & KEYMAP_RCONTROL) {
        SendKeyEvent(XK_Control_R, TRUE );
    }
    if (kas.releaseModifiers & KEYMAP_LALT) {
        SendKeyEvent(XK_Alt_L, FALSE );
    }
    if (kas.releaseModifiers & KEYMAP_LCONTROL) {
        SendKeyEvent(XK_Control_L, FALSE );
    }

	return 0;
}
/****************************************************************************************************/
static LRESULT WndCharEvent(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
     if (wParam == 0x0D || wParam == 0x20 || wParam == 0x08) return 0;
     if (wParam < 32) wParam += 64;  // map ctrl-keys onto alphabet
     if (wParam > 32 && wParam < 127)
	 {
		SendKeyEvent(wParam & 0xff, TRUE);
        SendKeyEvent(wParam & 0xff, FALSE);
     }
	 return 0;
}
/****************************************************************************************************/
static LRESULT CALLBACK WindowProc (HWND hWnd, UINT wMsg, WPARAM wParam,LPARAM lParam)
{
    for (DWORD dwCount = 0; dwCount < ARRAYSIZE(Messages); dwCount++)
	{
        if (wMsg == Messages[dwCount].Code) return (*Messages[dwCount].Fxn)(hWnd, wMsg, wParam, lParam);
    }
    return DefWindowProc (hWnd, wMsg, wParam, lParam);
}
/****************************************************************************************************/
static BOOL InitMainWindow()
{
	WNDCLASS	WndClass = {0};

	//Register a window class
	WndClass.lpfnWndProc = WindowProc;
	WndClass.style = CS_VREDRAW | CS_HREDRAW;
	WndClass.hInstance = hInst;
	WndClass.lpszClassName = MOTOWNDCLASSNAME;
	
	// Register window class
	if (!RegisterClass(&WndClass)) return FALSE;

	//Get Values
	HDC hDc = GetDC(NULL);
	iScale = int(GetDeviceCaps(hDc,LOGPIXELSX) / 96);
	ReleaseDC(NULL,hDc);
	dwWidth = GetSystemMetrics(SM_CXSCREEN);
	dwHeight = GetSystemMetrics(SM_CYSCREEN);

	hWnd = CreateWindowEx(WS_EX_TOPMOST,MOTOWNDCLASSNAME,NULL,WS_POPUP | WS_VISIBLE,0,0,0,0,NULL,NULL,hInst,NULL);
	SHFullScreen(hWnd,SHFS_HIDESIPBUTTON | SHFS_HIDETASKBAR | SHFS_HIDESTARTICON);
	SetWindowPos(hWnd,HWND_TOPMOST,0,0,dwWidth,dwHeight,SWP_SHOWWINDOW);

	return TRUE;
}
/****************************************************************************************************/
void UpdateScreen(HDC hDCBackdrop,DWORD dwClientWidth,DWORD dwClientHeight)
{
	HDC hDC;
	HRESULT HR;

	//Get Device Contect
	szMessage = NULL;
	HR = pBackSurf->GetDC(&hDC);
	StretchBlt(hDC,0,0,dwWidth,dwHeight,hDCBackdrop,0,0,dwClientWidth,dwClientHeight,SRCCOPY);
	pBackSurf->ReleaseDC(hDC);
	InvalidateRect(hWnd,NULL,FALSE);
}
/****************************************************************************************************/
void ClearScreen()
{
	memset((void *)&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	pBackSurf->Lock(NULL,&ddsd,8,0);
	LPWORD lpBuffer=(LPWORD) ddsd.lpSurface;
	for(DWORD i=0;i<(ddsd.dwWidth * ddsd.dwHeight);i++) *lpBuffer++=CONVCOLOUR(0,0,50);
	pBackSurf->Unlock(NULL);
	InvalidateRect(hWnd,NULL,FALSE);
}
/****************************************************************************************************/
void SetMessage(LPCWSTR lpszMessage)
{
	szMessage = lpszMessage;
	InvalidateRect(hWnd,NULL,FALSE);
}
/****************************************************************************************************/
static BOOL InitDirectDraw()
{
DIRECTDRAWCREATE pDirectDrawCreate;
DDSURFACEDESC ddsd;
HRESULT		hr;

	//Try Direct Draw first
	hDDraw = LoadLibrary(L"DDraw.dll");
	if (hDDraw == NULL) return FALSE;

	//Get Address of Create COM Function
	pDirectDrawCreate = (DIRECTDRAWCREATE) GetProcAddress(hDDraw,L"DirectDrawCreate");
	if (pDirectDrawCreate == NULL) return FALSE;

	//Get Pointer to IDirectDraw Interface
	hr=pDirectDrawCreate(0, (IUnknown **)&pdd, 0);
	if(hr!=DD_OK ) return FALSE;

	//Set Full Screen Co-op Level (This will Fail if compiled for PPC)
	hr = pdd->SetCooperativeLevel(hWnd,DDSCL_FULLSCREEN);
	if(hr != DD_OK ) return FALSE;

	//Create Primary surface
	memset((void *)&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	hr=pdd->CreateSurface(&ddsd, &pSurf, NULL);
	if(hr!=DD_OK ) return FALSE;
	
	//Create Secondary Surface
	memset((void *)&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
	ddsd.dwHeight = dwHeight;
	ddsd.dwWidth = dwWidth;
	hr = pdd->CreateSurface(&ddsd, &pBackSurf, NULL);
	if(hr!=DD_OK ) return FALSE;

	return TRUE;
}
/****************************************************************************************************/
static void DeinitDirectDraw()
{
	if (pdd)				  pdd->Release();
	if (pSurf)				  pSurf->Release();
	if (pBackSurf)			  pBackSurf->Release();
	if (hDDraw)				  FreeLibrary(hDDraw);
}
/****************************************************************************************************/
static void DeinitMainWindow()
{
	DestroyWindow(hWnd);
	UnregisterClass(MOTOWNDCLASSNAME,hInst);
}
/****************************************************************************************************/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE,LPWSTR, INT)
{
	MSG			msg;
	WSADATA		wsaData;

	WSAStartup(MAKELONG(2,0),&wsaData);
	hInst = hInstance;

	//COM Support
	CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);

	//Init Main Window
	InitMainWindow();

	//Init Direct Draw
	InitDirectDraw();

	//Clear Screen
	ClearScreen();

	//Init VNC
	InitVNCViewer();

	//Main Message Pump
	while (GetMessage(&msg,NULL,0,0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	//Deinit VNC
	DeinitVNCViewer();

	//Cleanup Direct Draw
	DeinitDirectDraw();

	//Cleanup Main Window
	DeinitMainWindow();

	return 0;
}
/****************************************************************************************************/
// allocate memory for zlib
extern "C" voidpf zcalloc (voidpf opaque, unsigned items, unsigned size)
{
    if (opaque) items += size - size; 
	return (voidpf)LocalAlloc(LPTR, items * size);
}
/****************************************************************************************************/
// free memory for zlib
extern "C" void  zcfree (voidpf opaque, voidpf ptr)
{
	LocalFree((HLOCAL)ptr);
    if (opaque) return; 
}