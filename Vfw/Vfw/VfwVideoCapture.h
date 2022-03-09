#pragma once

#include <Windows.h>
#include <stdio.h>

#include <Vfw.h>

#pragma comment(lib,"vfw32.lib")

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))

enum PixelFormat
{
	PIX_FMT_UYVY422 = 0,
	PIX_FMT_YUYV422 = 1,
	PIX_FMT_YUV420P,
	PIX_FMT_MONOWHITE,
	PIX_FMT_RGB4,
	PIX_FMT_RGB8,
	PIX_FMT_RGB555,
	PIX_FMT_BGR24,
	PIX_FMT_RGB32,
	PIX_FMT_NONE				//找不到这种像素格式,就是被编码了
};

enum CodecID
{
	CODEC_ID_DVVIDEO = 0,
	CODEC_ID_MJPEG = 1,
	CODEC_ID_NONE
};

struct Frame
{
	BYTE*	m_buffer;
	DWORD	m_dwLength;
};

typedef struct FrameList
{
	Frame		frame;
	FrameList*	next;
}FrameList;

class VfwVideoCapture
{
private:
	HWND	m_hCapWnd;
	
	DWORD	m_dwCodecID;
	DWORD   m_dwPixFormat;

	HANDLE	m_hEvent;						//标记FrameList 是否为空
	CRITICAL_SECTION	m_csFrameList;		//操作FrameList

	FrameList*	m_pHead;
	FrameList*	m_pTail;
	DWORD		m_dwCurBufSize;

	static LRESULT CALLBACK frame_cb(HWND hwnd, LPVIDEOHDR vdhdr);
public:
	void	GetDeviceList();

	int		VfwStart(int DeviceId, int width, int height, int fps = 15);
	void	VfwClose();

	void	VfwGetFrame(PBYTE*ppbuffer, DWORD* pLength);

	DWORD	VfwGetCodecID()
	{
		return m_dwCodecID;
	}
	DWORD   VfwGetPixFormat()
	{
		return m_dwPixFormat;
	}

	VfwVideoCapture();
	~VfwVideoCapture();
	
};

