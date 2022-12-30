#pragma once
#include <io.h>
#include <direct.h>
#include "Common.h"
#include <iostream>

extern "C"
{
#include <libavutil\frame.h>
#include <jpeglib.h>
}

// 32λ����
#ifdef _M_IX86
#pragma comment(lib,"..\\libs\\x86\\jpeg.lib")
#endif 

// 64λ����
#ifdef _M_X64
#pragma comment(lib,"..\\libs\\x64\\jpeg.lib")
#endif 

 

class ShotImage
{
public:
	ShotImage();
	~ShotImage();
	/**
	* ������������AVFrame��BGR24��ʽ��ת��ͼƬ����
	* @param savePath������·��
	* @return �ɹ�/ʧ��
	*/
	bool ConvertImage(AVFrame* frame, const char* path);
private:
	int Yuv420p2Jpeg(const char* savePath, unsigned char* yData, unsigned char* uData, unsigned char* vData, int image_width, int image_height, int quality=80);
	int Bgr2Bmp(const char* savePath, uint8_t* data, int width, int height, int bpp);
};

