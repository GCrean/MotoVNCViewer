#include "MotoVNCViewer.h"

static SOCKET	sSocket = INVALID_SOCKET;
static HANDLE	hCloseThread = NULL;
static HANDLE	hVNCThread = NULL;
static LPVOID	lpBitmapData = NULL;
static LPBYTE	lpRecvBuffer = NULL;
static LPVOID	lpBufferBase = NULL;
static HANDLE	hBitmap = NULL;
static HANDLE	hOldBitmap = NULL;
static HDC		hClientDC;
static DWORD	dwClientHeight;
static DWORD	dwClientWidth;
static WORD		wBPP;
static DWORD	dwBytesPerPixel;
static DWORD	dwBufferSize;
static DWORD	dwPitch;
static BOOL		bConnected = FALSE;

const rfbPixelFormat vnc8bitFormat = {8, 8, 1, 1, 7,7,3, 0,3,6,0,0};
const rfbPixelFormat vnc16bitFormat = {16, 16, 1, 1, 63, 31, 31, 0,6,11,0,0};

//Prototypes
static void SetPixels(DWORD dwXPos,DWORD dwYPos,DWORD dwWidth,DWORD dwHeight);
static BOOL ProcessRawEncoding(rfbFramebufferUpdateRectHeader *lpuh);
static BOOL ProcessHexTileEncoding(rfbFramebufferUpdateRectHeader *lpuh);

/****************************************************************************************************/
static BOOL CreateImageBitmap()
{
	rfbServerInitMsg	si = {0};
	rfbSetPixelFormatMsg spf = {0};
	LPBITMAPINFO		lpBitmapInfo = NULL;
	LPDWORD				lpColorTable = NULL;

	//Recieve VNC Parameters
	if (!RecvBuffer(sSocket,(LPBYTE)&si,sizeof(si))) return FALSE;

	//Recieve The Server Name
	LPBYTE lpTemp= new BYTE[Swap32IfLE(si.nameLength)];
	RecvBuffer(sSocket,lpTemp,Swap32IfLE(si.nameLength));
	delete lpTemp;

	//Set Pixel Format
	spf.type = rfbSetPixelFormat;
	spf.format = vnc16bitFormat;
    spf.format.redMax = Swap16IfLE(spf.format.redMax);
    spf.format.greenMax = Swap16IfLE(spf.format.greenMax);
    spf.format.blueMax = Swap16IfLE(spf.format.blueMax);
	spf.format.bigEndian = 0;
	if (!SendBuffer(sSocket,(LPBYTE)&spf,sizeof(spf))) return FALSE;

	//Create working Buffer
	lpBitmapInfo = (LPBITMAPINFO) LocalAlloc(LPTR,sizeof(BITMAPINFO) + (4 * sizeof(DWORD)));
	if (!lpBitmapInfo) return FALSE;
	lpColorTable = (LPDWORD) lpBitmapInfo->bmiColors;

	dwClientHeight										= Swap16IfLE(si.framebufferHeight);
	dwClientWidth										= Swap16IfLE(si.framebufferWidth);
	dwPitch												= dwClientWidth + ((dwClientWidth / 8) % 4);
	lpBitmapInfo->bmiHeader.biSize						= sizeof(BITMAPINFOHEADER);
	lpBitmapInfo->bmiHeader.biCompression				= BI_BITFIELDS;
	lpBitmapInfo->bmiHeader.biPlanes					= 1;
	lpBitmapInfo->bmiHeader.biBitCount = wBPP			= spf.format.bitsPerPixel;
	lpBitmapInfo->bmiHeader.biWidth						= dwPitch;
	lpBitmapInfo->bmiHeader.biHeight					= -dwClientHeight;

	lpColorTable[0] = Swap16IfLE(spf.format.redMax) << spf.format.redShift;
	lpColorTable[1] = Swap16IfLE(spf.format.greenMax) << spf.format.greenShift;
	lpColorTable[2] = Swap16IfLE(spf.format.blueMax) << spf.format.blueShift;

	//Allocate a screen buffer
	HDC hDc = GetDC(NULL);
	hClientDC = CreateCompatibleDC(hDc);
	hBitmap = CreateDIBSection( hDc, lpBitmapInfo, DIB_RGB_COLORS, (void**)&lpBitmapData, NULL, 0 );
	ReleaseDC(NULL,hDc);
	hOldBitmap = SelectObject(hClientDC,hBitmap);
	LocalFree(lpBitmapInfo);

	//Allocate Recv Buffer
	dwBytesPerPixel = (wBPP + 7) >> 3;
	dwBufferSize = dwClientHeight * (dwClientWidth * dwBytesPerPixel);
	lpBufferBase = VirtualAlloc(NULL,dwBufferSize,MEM_RESERVE,PAGE_NOACCESS);
	lpRecvBuffer = (LPBYTE) VirtualAlloc(lpBufferBase,dwBufferSize,MEM_COMMIT, PAGE_READWRITE);

	return (hBitmap != NULL) ? TRUE : FALSE;
}
/****************************************************************************************************/
static BOOL Negotiate()
{
	CHAR				szVersion[13] = {0};
	DWORD				dwAuthType =0;
	rfbClientInitMsg	ci={1};

	//Recieve Server Version
	if (!RecvBuffer(sSocket,(LPBYTE)szVersion,12)) return FALSE;
	strcpy(szVersion,"RFB 003.003\x0a");
	if (!SendBuffer(sSocket,(LPBYTE)szVersion,12)) return FALSE;

	//Get Auth Type
	if (!RecvBuffer(sSocket,(LPBYTE)&dwAuthType,sizeof(DWORD))) return FALSE;
	if (!SendBuffer(sSocket,(LPBYTE)&ci,sizeof(ci))) return FALSE;

	return TRUE;
}
/****************************************************************************************************/
static DWORD VNCThread(LPVOID lpParameter)
{
sockaddr_in		ConnectAddr;
DWORD			dwValue;
fd_set			fdWrite;
timeval			tvTimeOut = {0};
DWORD			dwEncoding;
DWORD			dwReTry=0;
HKEY			hKey;
rfbFramebufferUpdateRequestMsg	rfbUpdate ={0};
rfbFramebufferUpdateRectHeader	uh;
rfbSetEncodingsMsg				rfbEncoding ={0};
rfbServerToClientMsg			*Msg = NULL;

	//Loop until we are told to quit.
	while (WaitForSingleObject(hCloseThread,dwReTry) == WAIT_TIMEOUT)
	{
		//Read IP Address From Registry.
		if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE,L"Software\\Motorola\\VNCViewer",NULL,NULL,REG_OPTION_NON_VOLATILE,NULL,NULL,&hKey,NULL))
		{
			DWORD dwSz=sizeof(szAddress);
			memset (szAddress,0,dwSz);
			RegQueryValueEx(hKey,L"IPAddress",NULL,NULL,(LPBYTE)&szAddress,&dwSz);
			sprintf(szAddress,"%ls",szAddress);
			dwSz = sizeof(dwPort);
			RegQueryValueEx(hKey,L"IPPort",NULL,NULL,(LPBYTE)&dwPort,&dwSz);
			RegCloseKey(hKey);
		}

		//If No IP Address, Just report it
		if (szAddress[0] == L'\0')
		{
			SetMessage(L"Not Configured");
			dwReTry = 5000;
			continue;
		}

		//Display Message
		SetMessage(L"Connecting");

		//Create non-blocking socket
		sSocket = socket(AF_INET,SOCK_STREAM,0);
		dwValue=1;
		ioctlsocket(sSocket,FIONBIO,&dwValue);
		
		//Try to connect To VNC Server
		ConnectAddr.sin_family = AF_INET;
		ConnectAddr.sin_port = htons((u_short)dwPort);
		ConnectAddr.sin_addr.s_addr = inet_addr(szAddress);

		FD_ZERO(&fdWrite);
		FD_SET(sSocket,&fdWrite);
		tvTimeOut.tv_sec = 1;
		
		//Attempt to connect
		connect(sSocket,(sockaddr *)&ConnectAddr,sizeof(sockaddr_in));
		if (select(0,NULL,&fdWrite,NULL,&tvTimeOut) == 	SOCKET_ERROR) goto Cleanup;

		if (!FD_ISSET(sSocket,&fdWrite)) goto Cleanup;

		//Display Message
		SetMessage(L"Negotiating");
		if (!Negotiate()) goto Cleanup;
		if (!CreateImageBitmap()) goto Cleanup;

		//Set Encoding
		rfbEncoding.type = rfbSetEncodings;
		rfbEncoding.nEncodings = Swap16IfLE(2);
		if (!SendBuffer(sSocket,(LPBYTE)&rfbEncoding,sizeof(rfbEncoding))) goto Cleanup;
		dwEncoding = rfbEncodingHextile; 
		if (!SendBuffer(sSocket,(LPBYTE)&dwEncoding,sizeof(dwEncoding))) goto Cleanup;
		dwEncoding = rfbEncodingRaw; 
		if (!SendBuffer(sSocket,(LPBYTE)&dwEncoding,sizeof(dwEncoding))) goto Cleanup;

		//Request Full Frame Update
		rfbUpdate.type = rfbFramebufferUpdateRequest;
		rfbUpdate.h = Swap16IfLE(dwClientHeight);
		rfbUpdate.w = Swap16IfLE(dwClientWidth);
		rfbUpdate.incremental = FALSE;
		if (!SendBuffer(sSocket,(LPBYTE)&rfbUpdate,sizeof(rfbUpdate))) goto Cleanup;

		//Display Message
		SetMessage(L"Waiting...");

		//Allocate Message Buffer
		Msg = (rfbServerToClientMsg *) LocalAlloc(LPTR,sizeof(rfbServerToClientMsg));
		bConnected = TRUE;
		while (RecvBuffer(sSocket,(LPBYTE)Msg,1))
		{
			switch(Msg->type)
			{
				//Update Screen Buffer
				case rfbFramebufferUpdate:
					{
						if (!RecvBuffer(sSocket,(LPBYTE)Msg+1,sizeof(rfbFramebufferUpdateMsg)-1)) goto Cleanup;
						//Recieve the Rectangles
						for (DWORD dwRects=0;dwRects < Swap16IfLE(Msg->fu.nRects); dwRects++)
						{
							//Get The Header
							if (!RecvBuffer(sSocket,(LPBYTE)&uh,sizeof(uh))) goto Cleanup;
							uh.r.x = Swap16IfLE(uh.r.x);
							uh.r.y = Swap16IfLE(uh.r.y);
							uh.r.w = Swap16IfLE(uh.r.w);
							uh.r.h = Swap16IfLE(uh.r.h);
							uh.encoding = Swap32IfLE(uh.encoding);
							switch (uh.encoding)
							{
								case rfbEncodingRaw:
									if (!ProcessRawEncoding(&uh)) goto Cleanup;
									break;

								case rfbEncodingHextile:
									if (!ProcessHexTileEncoding(&uh)) goto Cleanup;
									break;
							}
						}
						UpdateScreen(hClientDC,dwClientWidth,dwClientHeight);
						rfbUpdate.incremental = TRUE;
						if (!SendBuffer(sSocket,(LPBYTE)&rfbUpdate,sizeof(rfbUpdate))) goto Cleanup;
						break;
					}

				//Update Colour Map Entries
				case rfbSetColourMapEntries:
					if (!RecvBuffer(sSocket,(LPBYTE)Msg+1,sizeof(rfbSetColourMapEntriesMsg) -1)) goto Cleanup;
					break;

				//Bell
				case rfbBell:
					MessageBeep(MAXDWORD);
					break;

				//Copy Server Clipboard
				case rfbServerCutText:
					if (!RecvBuffer(sSocket, (LPBYTE)Msg + 1, sizeof(rfbServerCutTextMsg)-1)) break;
					OpenClipboard(GetDesktopWindow());
					EmptyClipboard();	
					if (Swap32IfLE(Msg->sct.length) != 0)
					{
						LPBYTE lpClip = (LPBYTE) LocalAlloc(LPTR,(Swap32IfLE(Msg->sct.length) +1 )* 2);
						LPBYTE lpWorking = (LPBYTE) LocalAlloc(LPTR,Swap32IfLE(Msg->sct.length));
						if (!RecvBuffer(sSocket, (LPBYTE)lpWorking, Swap32IfLE(Msg->sct.length))) break;
						MultiByteToWideChar(CP_ACP,0,(LPCSTR)lpWorking,Swap32IfLE(Msg->sct.length),(LPWSTR)lpClip,(Swap32IfLE(Msg->sct.length) +1 )* 2);
						LocalFree(lpWorking);
						SetClipboardData(CF_UNICODETEXT,lpClip);
					}
					CloseClipboard();
					break;
			}
		}

Cleanup:
		//Cleanup Session
		dwReTry = RETRY_TIMER;
		bConnected = FALSE;
		ClearScreen();
		LocalFree(Msg);
		VirtualFree(lpRecvBuffer,dwBufferSize,MEM_DECOMMIT);
		VirtualFree(lpBufferBase,0,MEM_RELEASE);
		SelectObject(hClientDC,hOldBitmap);
		DeleteObject(hBitmap);
		DeleteDC(hClientDC);
		closesocket(sSocket);
		lpBufferBase = NULL;
		lpRecvBuffer = NULL;
		Msg = NULL;
		hBitmap=NULL;
		sSocket=INVALID_SOCKET;
		dwReTry = RETRY_TIMER;
		SetMessage(NULL);
	}

	return 0;
}
/****************************************************************************************************/
BOOL SendMouseEvent(DWORD dwFlags,DWORD dwXPos,DWORD dwYPos)
{
	rfbPointerEventMsg	pe;

	//Connected ?
	if(!bConnected) return FALSE;

	pe.type = rfbPointerEvent;
	pe.buttonMask = (CARD8)dwFlags;
	double dXPos = (double)dwClientWidth * (double)((double)dwXPos / (double)dwWidth);
	double dYPos = (double)dwClientHeight * (double)((double)dwYPos / (double)dwHeight);
	pe.x = Swap16IfLE((DWORD)dXPos);
	pe.y = Swap16IfLE((DWORD)dYPos);

	return SendBuffer(sSocket,(LPBYTE)&pe,sizeof(pe));
}
/****************************************************************************************************/
static void SetPixels(DWORD dwXPos,DWORD dwYPos,DWORD dwWidth,DWORD dwHeight)
{
	LPWORD lpStartLine = (LPWORD)lpBitmapData + (dwXPos + (dwYPos * dwPitch));
	LPWORD lpInBuffer = (LPWORD) lpRecvBuffer; 

	while (dwHeight--)
	{
		DWORD dwCount = dwWidth;
		DWORD dwPos = 0;
		while (dwCount--) lpStartLine[dwPos++] = *lpInBuffer++;
		lpStartLine += dwPitch;
	}
}
/****************************************************************************************************/
static BOOL ProcessRawEncoding(rfbFramebufferUpdateRectHeader *lpuh)
{
	DWORD dwPixels = lpuh->r.w * lpuh->r.h;
	DWORD dwBytes = dwPixels * dwBytesPerPixel;
	if (!RecvBuffer(sSocket,lpRecvBuffer,dwBytes)) return FALSE;
	SetPixels(lpuh->r.x,lpuh->r.y,lpuh->r.w,lpuh->r.h);
	return TRUE;
}
/****************************************************************************************************/
static BOOL ProcessHexTileEncoding(rfbFramebufferUpdateRectHeader *lpuh)
{
	DWORD dwBytes = 16 * 16 * dwBytesPerPixel;
	int rx = lpuh->r.x;
	int ry = lpuh->r.y;
	int rw = lpuh->r.w;
	int rh = lpuh->r.h;
	int w;
	int h;
	int iHeight = ry + rh;
	int iWidth = rx + rw;
	CARD8 subencoding;
	CARD8 nSubrects;

	for (int y=ry; y < iHeight; y+=16)
	{
		for (int x=rx; x < iWidth; x+=16)
		{
			w = h = 16;
			if (rx+rw - x < 16) w = rx+rw - x;  
            if (ry+rh - y < 16) h = ry+rh - y;  
			if (!RecvBuffer(sSocket,&subencoding,sizeof(CARD8))) return FALSE;
			
			//Normal HexTile
			if (subencoding & rfbHextileRaw)
			{
				if (!RecvBuffer(sSocket,lpRecvBuffer,w * h * dwBytesPerPixel)) return FALSE;
				SetPixels(x,y,w,h);
				continue;
			}
		}
	}

	return TRUE;
}
/****************************************************************************************************/
BOOL InitVNCViewer()
{
	hCloseThread = CreateEvent(NULL,TRUE,FALSE,NULL);
	hVNCThread = CreateThread(NULL,NULL,&VNCThread,NULL,NULL,NULL);
	return TRUE;
}
/****************************************************************************************************/
void DeinitVNCViewer()
{
	SetEvent(hCloseThread);
	closesocket(sSocket);
	sSocket = INVALID_SOCKET;

	if(WaitForSingleObject(hVNCThread,THREAD_TIMEOUT) == WAIT_TIMEOUT) TerminateThread(hVNCThread,0xdeadc0de);
	CloseHandle(hVNCThread);
	CloseHandle(hCloseThread);
}
