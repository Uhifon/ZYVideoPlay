#include "stdafx.h"
#include "ShotImage.h"
#include <Windows.h>
#include <fstream>
#include <SDL.h>
 
ShotImage::ShotImage()
{
}

ShotImage::~ShotImage()
{
	
}

 
///YUV420格式存储为图片
// outJpegFileName：输出的jpeg文件名称
//quaulity: 输出的jpeg图像质量，有效范围为0-100
int ShotImage::Yuv420p2Jpeg(const char* savePath, unsigned char* yData, unsigned char* uData, unsigned char* vData, int image_width, int image_height, int quality)
{
	try
	{
		struct jpeg_compress_struct cinfo;
		struct jpeg_error_mgr jerr;
		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_compress(&cinfo);

		FILE * outfile;    // target file  
		if ((outfile = fopen(savePath, "wb")) == NULL) {
			fprintf(stderr, "can't open %s\n", savePath);
			exit(1);
		}
		jpeg_stdio_dest(&cinfo, outfile);

		cinfo.image_width = image_width;  // image width and height, in pixels  
		cinfo.image_height = image_height;
		cinfo.input_components = 3;    // # of color components per pixel  
		cinfo.in_color_space = JCS_YCbCr;  //colorspace of input image  
		jpeg_set_defaults(&cinfo);
		jpeg_set_quality(&cinfo, quality, TRUE);

		//  
		//  cinfo.raw_data_in = TRUE;  
		cinfo.jpeg_color_space = JCS_YCbCr;
		cinfo.comp_info[0].h_samp_factor = 2;
		cinfo.comp_info[0].v_samp_factor = 2;
		jpeg_start_compress(&cinfo, TRUE);

		JSAMPROW row_pointer[1];

		unsigned char *yuvbuf;
		if ((yuvbuf = (unsigned char *)malloc(image_width * 3)) != NULL)
			memset(yuvbuf, 0, image_width * 3);

		int j = 0;
		while (cinfo.next_scanline < cinfo.image_height)
		{
			int idx = 0;
			for (int i = 0; i<image_width; i++)
			{
				yuvbuf[idx++] = yData[i + j * image_width];
				yuvbuf[idx++] = uData[j / 2 * image_width / 2 + (i / 2)];
				yuvbuf[idx++] = vData[j / 2 * image_width / 2 + (i / 2)];
			}
			row_pointer[0] = yuvbuf;
			jpeg_write_scanlines(&cinfo, row_pointer, 1);
			j++;
		}
		jpeg_finish_compress(&cinfo);
		jpeg_destroy_compress(&cinfo);
		fclose(outfile);
		free(yuvbuf);
		return 0;
	}
	catch(exception ex)
	{
		return -1;
	}
}

//BGR转bmp
//bpp 图像位数
int ShotImage::Bgr2Bmp(const char* filename, uint8_t* pRGBBuffer, int width, int height, int bpp)
{
	BITMAPFILEHEADER bmpheader;
	BITMAPINFOHEADER bmpinfo;
	FILE* fp = NULL;

	fp = fopen(filename, "wb");
	if (fp == NULL)
	{
		return FALSE;
	}

	bmpheader.bfType = ('M' << 8) | 'B';
	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpheader.bfSize = bmpheader.bfOffBits + width * height * bpp / 8;

	bmpinfo.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.biWidth = width;
	bmpinfo.biHeight = 0 - height;
	bmpinfo.biPlanes = 1;
	bmpinfo.biBitCount = bpp;
	bmpinfo.biCompression = BI_RGB;
	bmpinfo.biSizeImage = 0;
	bmpinfo.biXPelsPerMeter = 100;
	bmpinfo.biYPelsPerMeter = 100;
	bmpinfo.biClrUsed = 0;
	bmpinfo.biClrImportant = 0;

	fwrite(&bmpheader, sizeof(BITMAPFILEHEADER), 1, fp);
	fwrite(&bmpinfo, sizeof(BITMAPINFOHEADER), 1, fp);
	fwrite(pRGBBuffer, width * height * bpp / 8, 1, fp);
	fclose(fp);
	fp = NULL;

	return TRUE;
}
 

///AVFrame转为图片存储
bool ShotImage::ConvertImage(AVFrame* frame, const char* path)
{
	int ret = -1;
	if (frame->format == AVPixelFormat::AV_PIX_FMT_YUV420P)
	{
		uint8_t* pY = (uint8_t*)malloc(frame->width*frame->height );
		uint8_t* pU = (uint8_t*)malloc(frame->width*frame->height / 4);
		uint8_t* pV = (uint8_t*)malloc(frame->width*frame->height / 4);
		memcpy(pY, frame->data[0], frame->linesize[0] * frame->height);
		memcpy(pU, frame->data[1], frame->linesize[1] * frame->height / 2);
		memcpy(pV, frame->data[2], frame->linesize[2] * frame->height / 2);
		ret = Yuv420p2Jpeg(path, pY, pU,pV, frame->width, frame->height, 80);
		free(pY);
		free(pU);
		free(pV);
	}
	if (frame->format == AVPixelFormat::AV_PIX_FMT_BGR8)
		 ret = Bgr2Bmp(path, frame->data[0], frame->width, frame->height, 24);

	return ret == 0;
}
 