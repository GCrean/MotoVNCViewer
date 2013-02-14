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
static BYTE		bColorMode = 0;
static z_stream zstrm = {0};
static rfbPixelFormat vnc8bitFormat = {8, 8, 1, 1, 7,7,3, 0,3,6,0,0};
static rfbPixelFormat vnc16bitFormat = {16, 16, 1, 1, 31, 63, 31, 11,5,0,0,0};
static rfbPixelFormat vncServerFormat = {0};
static rfbPixelFormat *vncSelectedFormat = NULL;

//Prototypes
static void SetPixels(DWORD dwXPos,DWORD dwYPos,DWORD dwWidth,DWORD dwHeight);
static BOOL ProcessRawEncoding(rfbFramebufferUpdateRectHeader *lpuh);

/****************************************************************************************************/
static BOOL CreateImageBitmap()
{
	rfbSetPixelFormatMsg spf = {0};
	LPBITMAPINFO		lpBitmapInfo = NULL;
	LPDWORD				lpColorTable = NULL;

	//Select the required format
	bColorMode =1;
	switch (bColorMode)
	{
		case 1:
			vncSelectedFormat = &vnc16bitFormat;
			break;

		case 2:
			vncSelectedFormat = &vncServerFormat;
			break;

		default:
			vncSelectedFormat = &vnc8bitFormat;
			break;
	}

	//Set Pixel Format
	spf.type = rfbSetPixelFormat;
	memcpy(&spf.format,vncSelectedFormat,sizeof(rfbPixelFormat));
	spf.format.redMax = Swap16IfLE(spf.format.redMax);
	spf.format.greenMax = Swap16IfLE(spf.format.greenMax);
	spf.format.blueMax = Swap16IfLE(spf.format.blueMax);
	spf.format.bigEndian = 0;
	if (!SendBuffer(sSocket,(LPBYTE)&spf,sizeof(spf))) return FALSE;

	//Delete Any Old Bitmaps
	if (hBitmap)
	{
		SelectObject(hClientDC,hOldBitmap);	
		DeleteObject(hBitmap);
	}

	//Create working Buffer
	lpBitmapInfo = (LPBITMAPINFO) LocalAlloc(LPTR,sizeof(BITMAPINFO) + (256 * sizeof(DWORD)));
	if (!lpBitmapInfo) return FALSE;
	lpColorTable = (LPDWORD) lpBitmapInfo->bmiColors;
	dwPitch												= dwClientWidth + (sizeof(DWORD) - (dwClientWidth % sizeof(DWORD)));
	lpBitmapInfo->bmiHeader.biSize						= sizeof(BITMAPINFOHEADER);
	lpBitmapInfo->bmiHeader.biCompression				= BI_BITFIELDS;
	lpBitmapInfo->bmiHeader.biPlanes					= 1;
	lpBitmapInfo->bmiHeader.biBitCount = wBPP			= spf.format.bitsPerPixel;
	lpBitmapInfo->bmiHeader.biWidth						= dwPitch;
	lpBitmapInfo->bmiHeader.biHeight					= -(LONG)dwClientHeight;
	lpColorTable[0] = Swap16IfLE(spf.format.redMax) << spf.format.redShift;
	lpColorTable[1] = Swap16IfLE(spf.format.greenMax) << spf.format.greenShift;
	lpColorTable[2] = Swap16IfLE(spf.format.blueMax) << spf.format.blueShift;

	//8 Bit
	if (wBPP == 8)
	{
		lpBitmapInfo->bmiHeader.biCompression = BI_RGB;
		int p=0;
		for (int b = 0; b <= 3; b++) {
			for (int g = 0; g <= 7; g++) {
				for (int r = 0; r <= 7; r++) {
					lpBitmapInfo->bmiColors[p].rgbRed = r * 255 / 7; 	
					lpBitmapInfo->bmiColors[p].rgbGreen = g * 255 / 7;
					lpBitmapInfo->bmiColors[p].rgbBlue  = b * 255 / 3;
					p++;
				}
			}
		}		
		lpBitmapInfo->bmiHeader.biClrUsed = p;
	}

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
	rfbServerInitMsg	si = {0};

	//Recieve Server Version
	if (!RecvBuffer(sSocket,(LPBYTE)szVersion,12)) return FALSE;
	sprintf(szVersion,rfbProtocolVersionFormat,rfbProtocolMajorVersion,rfbProtocolMinorVersion);
	if (!SendBuffer(sSocket,(LPBYTE)szVersion,12)) return FALSE;

	//Get Auth Type
	if (!RecvBuffer(sSocket,(LPBYTE)&dwAuthType,sizeof(DWORD))) return FALSE;
	if (!SendBuffer(sSocket,(LPBYTE)&ci,sizeof(ci))) return FALSE;

	//Recieve VNC Parameters
	if (!RecvBuffer(sSocket,(LPBYTE)&si,sizeof(si))) return FALSE;
	memcpy(&vncServerFormat,&si.format,sizeof(vncServerFormat));
	dwClientHeight	= Swap16IfLE(si.framebufferHeight);
	dwClientWidth	= Swap16IfLE(si.framebufferWidth);
    vncServerFormat.redMax = Swap16IfLE(vncServerFormat.redMax);
    vncServerFormat.greenMax = Swap16IfLE(vncServerFormat.greenMax);
    vncServerFormat.blueMax = Swap16IfLE(vncServerFormat.blueMax);

	//Recieve The Server Name
	LPBYTE lpTemp= new BYTE[Swap32IfLE(si.nameLength)];
	RecvBuffer(sSocket,lpTemp,Swap32IfLE(si.nameLength));
	delete lpTemp;

	return TRUE;
}
/****************************************************************************************************/
static DWORD VNCThread(LPVOID lpParameter)
{
sockaddr_in		ConnectAddr;
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
			dwSz = sizeof(bColorMode);
			RegQueryValueEx(hKey,L"ColorMode",NULL,NULL,(LPBYTE)&bColorMode,&dwSz);
			RegCloseKey(hKey);
		}

		//If No IP Address, Just report it
		if (szAddress[0] == L'\0')
		{
			SetMessage(L"Not Configured");
			dwReTry = RETRY_CONFIG;
			continue;
		}

		//Display Message
		SetMessage(L"Searching");

		//Create non-blocking socket
		sSocket = socket(AF_INET,SOCK_STREAM,0);
		
		//Try to connect To VNC Server
		ConnectAddr.sin_family = AF_INET;
		ConnectAddr.sin_port = htons((u_short)dwPort);
		ConnectAddr.sin_addr.s_addr = inet_addr(szAddress);

		//Attempt to connect
		if (connect(sSocket,(sockaddr *)&ConnectAddr,sizeof(sockaddr_in)) == SOCKET_ERROR) goto Cleanup;

		//Display Message
		SetMessage(L"Negotiating");
		if (!Negotiate()) goto Cleanup;
		if (!CreateImageBitmap()) goto Cleanup;

		//Set Encoding
		rfbEncoding.type = rfbSetEncodings;
		rfbEncoding.nEncodings = Swap16IfLE(2);
		if (!SendBuffer(sSocket,(LPBYTE)&rfbEncoding,sizeof(rfbEncoding))) goto Cleanup;
		dwEncoding = Swap32IfLE(rfbEncodingZlib); 
		if (!SendBuffer(sSocket,(LPBYTE)&dwEncoding,sizeof(dwEncoding))) goto Cleanup;
		dwEncoding = Swap32IfLE(rfbEncodingRaw); 
		if (!SendBuffer(sSocket,(LPBYTE)&dwEncoding,sizeof(dwEncoding))) goto Cleanup;

		//Request Full Frame Update
		rfbUpdate.type = rfbFramebufferUpdateRequest;
		rfbUpdate.h = Swap16IfLE(dwClientHeight);
		rfbUpdate.w = Swap16IfLE(dwClientWidth);
		rfbUpdate.incremental = FALSE;
		if (!SendBuffer(sSocket,(LPBYTE)&rfbUpdate,sizeof(rfbUpdate))) goto Cleanup;

		//Display Message
		SetMessage(L"Waiting...");
		inflateInit(&zstrm);

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
								case rfbEncodingZlib:
									if (!ProcessRawEncoding(&uh)) goto Cleanup;
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
					MessageBeep(MB_OK);
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
		inflateEnd(&zstrm);
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
BOOL SendKeyEvent(DWORD dwKey,BOOL bDown)
{
	rfbKeyEventMsg ke;

	//Connected ?
	if(!bConnected) return FALSE;

	ke.type = rfbKeyEvent;
    ke.down = bDown ? 1 : 0;
    ke.key = Swap32IfLE(dwKey);
	return SendBuffer(sSocket,(LPBYTE)&ke,sizeof(ke));
}
/****************************************************************************************************/
static void SetPixels(LPVOID lpInBuffer,DWORD dwXPos,DWORD dwYPos,DWORD dwWidth,DWORD dwHeight)
{
	switch(wBPP)
	{
		case 32:
			{
				LPDWORD lpStartLine = (LPDWORD)lpBitmapData + (dwXPos + (dwYPos * dwPitch));
				LPDWORD lpBuffer = (LPDWORD) lpInBuffer;
				while (dwHeight--)
				{
					DWORD dwCount = dwWidth;
					DWORD dwPos = 0;
					while (dwCount--) lpStartLine[dwPos++] = *lpBuffer++;
					lpStartLine += dwPitch;
				}
			}
			break;

		case 16:
			{
				LPWORD lpStartLine = (LPWORD)lpBitmapData + (dwXPos + (dwYPos * dwPitch));
				LPWORD lpBuffer = (LPWORD) lpInBuffer;
				while (dwHeight--)
				{
					DWORD dwCount = dwWidth;
					DWORD dwPos = 0;
					while (dwCount--) lpStartLine[dwPos++] = *lpBuffer++;
					lpStartLine += dwPitch;
				}
			}
			break;

		case 8:
			{
				LPBYTE lpStartLine = (LPBYTE)lpBitmapData + (dwXPos + (dwYPos * dwPitch));
				LPBYTE lpBuffer = (LPBYTE) lpInBuffer;
				while (dwHeight--)
				{
					DWORD dwCount = dwWidth;
					DWORD dwPos = 0;
					while (dwCount--) lpStartLine[dwPos++] = *lpBuffer++;
					lpStartLine += dwPitch;
				}
			}
			break;
	}
}
/****************************************************************************************************/
static BOOL ProcessRawEncoding(rfbFramebufferUpdateRectHeader *lpuh)
{
	DWORD dwPixels;
	DWORD dwBytes;
	if (lpuh->encoding == rfbEncodingZlib)
	{
		LPBYTE lpCompress;
		if (!RecvBuffer(sSocket,(LPBYTE)&dwBytes,sizeof(DWORD))) return FALSE;
		dwBytes = Swap32IfLE(dwBytes);
		lpCompress = (LPBYTE)LocalAlloc(LPTR,dwBytes);
		if (!lpCompress) return FALSE;
		if (!RecvBuffer(sSocket,lpCompress,dwBytes)) { LocalFree(lpCompress); return FALSE; }
		zstrm.avail_in = dwBytes;
		zstrm.next_in = lpCompress;
		zstrm.avail_out = dwBufferSize;
		zstrm.next_out = lpRecvBuffer;
		inflate(&zstrm,Z_FINISH);
		LocalFree(lpCompress);
	}else{
		dwPixels = lpuh->r.w * lpuh->r.h;
		dwBytes = dwPixels * dwBytesPerPixel;
		if (!RecvBuffer(sSocket,lpRecvBuffer,dwBytes)) return FALSE;
	}
	SetPixels(lpRecvBuffer,lpuh->r.x,lpuh->r.y,lpuh->r.w,lpuh->r.h);
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
