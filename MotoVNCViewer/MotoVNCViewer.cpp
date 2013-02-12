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

//Internal Windows Messages
static LRESULT WndCreate(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndDestory(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndPaint(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndButtonDown(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndButtonUp(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WndButtonMove(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);

// Message dispatch table for WindowProc
static const struct DecodeWindowMessage Messages[] = 
{
	WM_PAINT,WndPaint,
	WM_CREATE,WndCreate,
	WM_DESTROY,WndDestory,
	WM_LBUTTONUP,WndButtonUp,
	WM_LBUTTONDOWN,WndButtonDown,
	WM_MOUSEMOVE,WndButtonMove,
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
static LRESULT WndButtonDown(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	SendMouseEvent(rfbButton1Mask,LOWORD(lParam),HIWORD(lParam));
	return TRUE;
}
/****************************************************************************************************/
static LRESULT WndButtonUp(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	SendMouseEvent(0,LOWORD(lParam),HIWORD(lParam));
	return TRUE;
}
/****************************************************************************************************/
static LRESULT WndButtonMove(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	SendMouseEvent(rfbButton1Mask,LOWORD(lParam),HIWORD(lParam));
	return TRUE;
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

