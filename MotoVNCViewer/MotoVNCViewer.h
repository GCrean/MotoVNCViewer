#pragma once
#include <Windows.h>
#include <aygshell.h>
#include <Winsock.h>
#include "DbgAPI.h"
#include "ddraw.h"
#include "rfb.h"

#pragma comment(lib,"AygShell")
#pragma comment(lib,"ws2")

#define MOTOWNDCLASSNAME		L"MotoVNCClass"
#define	RETRY_TIMER				2000
#define THREAD_TIMEOUT			5000

//Helper
#define GETMASK(Len,Offset)			(((1 << Len) - 1) << Offset);
#define CONVCOLOUR(r,g,b) (USHORT)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3))
#define SCALE(s) (iScale * (s))
#define ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))

//DirectDraw Function Prototype
typedef LONG (*DIRECTDRAWCREATE)(LPGUID, LPUNKNOWN *, LPUNKNOWN *);

//Structures
struct DecodeWindowMessage
{                             
    UINT Code;                                  
    LRESULT (*Fxn)(HWND, UINT, WPARAM, LPARAM);
};

extern DWORD dwWidth;
extern DWORD dwHeight;
extern CHAR szAddress[40];
extern DWORD dwPort;

void ClearScreen();
void UpdateScreen(HDC hDCBackdrop,DWORD dwClientWidth,DWORD dwClientHeight);
void SetMessage(LPCWSTR lpszMessage);

//VNCParser.cpp
BOOL InitVNCViewer();
void DeinitVNCViewer();
BOOL SendMouseEvent(DWORD dwFlags,DWORD dwXPos,DWORD dwYPos);

//Utils.cpp
BOOL SendBuffer(SOCKET sSocket,LPBYTE bBuffer,DWORD dwLength);
BOOL RecvBuffer(SOCKET sSocket,LPBYTE bBuffer,DWORD dwLength);
HFONT BuildFont(int iFontSize, BOOL bBold, BOOL bItalic);

//Primitive.s
extern "C" DWORD TxCalcGradiant(DWORD dwStart,DWORD dwEnd,DWORD dwBlend);