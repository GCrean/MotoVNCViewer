#include "MotoVNCViewer.h"

BOOL SendBuffer(SOCKET sSocket,LPBYTE bBuffer,DWORD dwLength)
{
	DWORD	dwSent = 0;
	DWORD	dwResult;
	fd_set	fdSet;
	timeval	tvTimeOut = {0};

	//Is Valid Handle ?
	if (sSocket == INVALID_SOCKET) return FALSE;

	FD_ZERO(&fdSet);
	FD_SET(sSocket,&fdSet);
	tvTimeOut.tv_sec = 1;
	while (dwSent != dwLength)
	{
		select(0,NULL,&fdSet,NULL,&tvTimeOut);
		if (!FD_ISSET(sSocket,&fdSet)) return FALSE;

		dwResult = send(sSocket,(char *)bBuffer + dwSent,dwLength - dwSent,0);
		if ((dwResult == SOCKET_ERROR) || (dwResult == 0)) return FALSE;
		dwSent += dwResult;
	}
	return TRUE;
}
/************************************************************************************************/
BOOL RecvBuffer(SOCKET sSocket,LPBYTE bBuffer,DWORD dwLength)
{
	DWORD	dwRecv = 0;
	DWORD	dwResult;
	fd_set	fdSet;
	int iRes;

	//Is Valid Handle ?
	if (sSocket == INVALID_SOCKET) return FALSE;
	FD_ZERO(&fdSet);
	FD_SET(sSocket,&fdSet);
	while (dwRecv != dwLength)
	{
		iRes = select(0,&fdSet,NULL,NULL,NULL);
		if (iRes == SOCKET_ERROR) return FALSE;
		if (!FD_ISSET(sSocket,&fdSet)) return FALSE;

		dwResult = recv(sSocket,(char *)bBuffer + dwRecv,dwLength - dwRecv,0);
		if ((dwResult == SOCKET_ERROR) || (dwResult == 0)) return FALSE;
		dwRecv += dwResult;
	}
	return TRUE;
}
/************************************************************************************************/
HFONT BuildFont(int iFontSize, BOOL bBold, BOOL bItalic)
{
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	lf.lfHeight = iFontSize;
	lf.lfWidth = 0;
	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	lf.lfWeight = bBold ? 600 : 500;
	lf.lfItalic = bItalic;
	lf.lfUnderline = FALSE;
	lf.lfStrikeOut = FALSE;
	lf.lfCharSet = EASTEUROPE_CHARSET;
	lf.lfOutPrecision = OUT_RASTER_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = CLEARTYPE_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	wcscpy(lf.lfFaceName, L"Tahoma");
	return CreateFontIndirect(&lf);
}