#include "VfwVideoCapture.h"


static enum PixelFormat vfw_pixfmt(DWORD biCompression, WORD biBitCount)
{
	switch (biCompression) {
	case MKTAG('U', 'Y', 'V', 'Y'):
		return PIX_FMT_UYVY422;
	case MKTAG('Y', 'U', 'Y', '2'):
		return PIX_FMT_YUYV422;
	case MKTAG('I', '4', '2', '0'):
		return PIX_FMT_YUV420P;
	case BI_RGB:
		switch (biBitCount) { /* 1-8 are untested */
		case 1:
			return PIX_FMT_MONOWHITE;
		case 4:
			return PIX_FMT_RGB4;
		case 8:
			return PIX_FMT_RGB8;
		case 16:
			return PIX_FMT_RGB555;
		case 24:
			return PIX_FMT_BGR24;
		case 32:
			return PIX_FMT_RGB32;
		}
	}
	return PIX_FMT_NONE;
}

static enum CodecID vfw_codecid(DWORD biCompression)
{
	switch (biCompression) {
	case MKTAG('d', 'v', 's', 'd'):
		return CODEC_ID_DVVIDEO;
	case MKTAG('M', 'J', 'P', 'G'):
	case MKTAG('m', 'j', 'p', 'g'):
		return CODEC_ID_MJPEG;
	}
	return CODEC_ID_NONE;
}

VfwVideoCapture::VfwVideoCapture()
{
	m_hCapWnd = NULL;
	m_dwCodecID = 0;
	m_dwPixFormat = 0;

	m_dwCurBufSize = 0;
	m_pHead = m_pTail = 0;

	m_hEvent = CreateEvent(0, TRUE, FALSE, NULL);

	InitializeCriticalSection(&m_csFrameList);
}


VfwVideoCapture::~VfwVideoCapture()
{
	if (m_hEvent)
	{
		CloseHandle(&m_hEvent);
		m_hEvent = NULL;
	}
	DeleteCriticalSection(&m_csFrameList);
}

void VfwVideoCapture::GetDeviceList()
{
	TCHAR szName[256];
	TCHAR szVer[256];
	for (int i = 0; i < 10; i++)
	{
		if (capGetDriverDescription(i, szName, 255, szVer, 255))
		{
			wprintf(TEXT("[%d] %s %s\n"), i, szName, szVer);
		}
	}
}
void VfwVideoCapture::VfwClose()
{
	FrameList*pTemp;
	if (m_hCapWnd)
	{
		capSetCallbackOnVideoStream(m_hCapWnd,0);
		capCaptureStop(m_hCapWnd);
		capDriverDisconnect(m_hCapWnd);
		DestroyWindow(m_hCapWnd);
		m_hCapWnd = NULL;
	}
	if (m_hEvent)
	{
		ResetEvent(m_hEvent);
	}
	//清理buffer
	EnterCriticalSection(&m_csFrameList);
	while (m_pHead)
	{
		pTemp = m_pHead;
		m_pHead = m_pHead->next;

		free(pTemp->frame.m_buffer);
		free(pTemp);
	}
	LeaveCriticalSection(&m_csFrameList);
	m_dwCurBufSize = 0;
}

int  VfwVideoCapture::VfwStart(int DeviceId,int width,int height,int fps)
{
	DWORD bisize = 0;
	BITMAPINFO *bi = NULL;
	CAPTUREPARMS cparms = { 0 };
	//创建CaptureWindow
	m_hCapWnd = capCreateCaptureWindow(NULL, 0, 0, 0, 0, 0, 0, 0);
	if (!m_hCapWnd)
	{
		printf("capCreateCaptureWindow failed!\n");
		return -1;
	}
	SetWindowLongPtr(m_hCapWnd, GWLP_USERDATA, (LONG)this);
	//
	
	//
	capPreview(m_hCapWnd, FALSE);
	capOverlay(m_hCapWnd, FALSE);
	//
	if (!capSetCallbackOnVideoStream(m_hCapWnd, frame_cb))
	{
		printf("capSetCallbackOnVideoStream failed!\n");
		goto fail;
	}
	//
	//设置捕获参数
	if (!capDriverConnect(m_hCapWnd, DeviceId))
	{
		printf("capDriverConnect failed!\n");
		DestroyWindow(m_hCapWnd);
		m_hCapWnd = NULL;
		return -1;
	}

	bisize = capGetVideoFormat(m_hCapWnd, 0, 0);
	if (!bisize)
	{
		printf("capGetVideoFormat failed!\n");
		goto fail;
	}

	bi = (BITMAPINFO*)malloc(bisize);
	if (!capGetVideoFormat(m_hCapWnd, bi,bisize))
	{
		printf("capGetVideoFormat failed!\n");
		goto fail;
	}
	
	m_dwPixFormat = vfw_pixfmt(bi->bmiHeader.biCompression, bi->bmiHeader.biBitCount);
	if (m_dwPixFormat == PIX_FMT_NONE)
	{
		m_dwCodecID = vfw_codecid(bi->bmiHeader.biCompression);
		if (m_dwCodecID == CODEC_ID_NONE)
		{
			printf("unsupported format!\n");
			return -1;
		}
	}
	//
	if (width && height)
	{
		bi->bmiHeader.biHeight = height;
		bi->bmiHeader.biWidth = width;
	}
	if (!capSetVideoFormat(m_hCapWnd,bi,bisize))
	{
		printf("capSetVideoFormat failed!\n");
		goto fail;
	}
	if (!capCaptureGetSetup(m_hCapWnd, &cparms, sizeof(cparms)))
	{
		printf("capCaptureGetSetup failed!\n");
		goto fail;
	}

	cparms.fYield = 1; // Spawn a background thread
	//好像没用下面这句.
	cparms.dwRequestMicroSecPerFrame = (1000000 / 15);
	cparms.fAbortLeftMouse = 0;
	cparms.fAbortRightMouse = 0;
	cparms.fCaptureAudio = 0;
	cparms.vKeyAbort = 0;
	//
	if (fps)
		cparms.dwRequestMicroSecPerFrame = 1000000 / fps;
	
	//
	if (!capCaptureSetSetup(m_hCapWnd, &cparms, sizeof(cparms)))
	{
		printf("capCaptureSetSetup failed!\n");
		goto fail;
	}
	//
	ResetEvent(&m_hEvent);
	m_dwCurBufSize = 0;
	//capCaptureSequence不行....
	if (!capCaptureSequenceNoFile(m_hCapWnd))
	{
		printf("capCaptureSequenceNoFile failed!\n");
		goto fail;
	}
	free(bi);
	return 0;
fail:
	free(bi);
	VfwClose();
	return -1;
}

//OnFrameCallbakc是用于preview的
//这里要用OnVideoStreamCallback

LRESULT CALLBACK VfwVideoCapture::frame_cb(HWND hwnd, LPVIDEOHDR vdhdr)
{
	FrameList *pNewFrame = 0;
	
	VfwVideoCapture*pCapture = (VfwVideoCapture*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (pCapture->m_dwCurBufSize >= 1024 * 1024 * 128)
	{
		//不放了,太多了.
		return 0;
	}
	pNewFrame = (FrameList*)malloc(sizeof(FrameList));
	pNewFrame->next = 0;
	pNewFrame->frame.m_buffer = (BYTE*)malloc(vdhdr->dwBytesUsed);
	pNewFrame->frame.m_dwLength = vdhdr->dwBytesUsed;
	//copy frame
	memcpy(pNewFrame->frame.m_buffer, vdhdr->lpData, pNewFrame->frame.m_dwLength);
	//
	
	EnterCriticalSection(&pCapture->m_csFrameList);
	pCapture->m_dwCurBufSize += vdhdr->dwBytesUsed;
	if (pCapture->m_pHead == NULL)
	{
		pCapture->m_pHead = pCapture->m_pTail = pNewFrame;
	}
	else
	{
		pCapture->m_pTail->next = pNewFrame;
		pCapture->m_pTail = pNewFrame;
	}
	SetEvent(pCapture->m_hEvent);
	LeaveCriticalSection(&pCapture->m_csFrameList);
	return 0;
}

void VfwVideoCapture::VfwGetFrame(PBYTE*ppbuffer,DWORD* pLength)
{
	FrameList*pFrame = 0;
	while (true)
	{
		EnterCriticalSection(&m_csFrameList);
		pFrame = m_pHead;
		if (m_pHead == m_pTail)
			m_pHead = m_pTail = NULL;
		else
			m_pHead = m_pHead->next;
		//空了
		if (!pFrame)
			ResetEvent(m_hEvent);
		else
			m_dwCurBufSize -= pFrame->frame.m_dwLength;
		LeaveCriticalSection(&m_csFrameList);

		//等待往链表里面放东西.
		if (!pFrame)
		{
			if (WAIT_TIMEOUT == WaitForSingleObject(m_hEvent, 20000))
			{
				*ppbuffer = 0;
				*pLength = 0;
				return;
			}
		}		
		else
			break;
	}
	*ppbuffer = pFrame->frame.m_buffer;
	*pLength = pFrame->frame.m_dwLength;
	free(pFrame);
}