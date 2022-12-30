#pragma once
#include <iostream>
#include <string>
#include <vector>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavfilter/avfilter.h>
#include <libavutil/opt.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h> 
}

 
using namespace std;

class XFFMpeg
{
public:
	XFFMpeg();
	~XFFMpeg();
	void InitRegister();

	/*解码*/
	int OpenInput(const string url);
	void ClearBuffer();//清空buffer
	void CloseInput();
	int InitDecoderCodec();
	int InitAduioDecoderCodec();
	int ReadPacket(AVPacket* pkt);
	 
	int DecodeVideo(AVPacket* pkt, AVFrame* yuvFrame);
	int DecodeAudio(AVPacket* pkt, vector<AVFrame*>& frames);

	/*编码*/
	int OpenOutput(const string path);
	void CloseOutput();
	int InitEncoderCodec(AVCodecID codecID, int width, int height, int fps, float quality);
	int Encode(AVFrame *pFrame, AVPacket* outPkt);
	int WritePacket(AVPacket* packet);
	int FinishWrite();


	/*转码*/
	int InitSwsContext(AVCodecContext* videoCtx, AVPixelFormat dstFmt, int width, int height);
	int FormatConvert(AVFrame* srcFrame, AVFrame* dstFrame);
	void SwsFree();

	/*滤镜1*/
	int InitFilters(const char* filtersDescr);
	int FilterFrame(AVFrame* srcFrame, AVFrame* dstFrame);
	void CloseFilter();

	/*滤镜2*/
	int InitFilters2(const char* filtersDescr);
	int FilterFrame2(AVFrame* srcFrame, AVFrame* dstFrame);
	void CloseFilter2();

	//跳转 0-1
	int Seek(float pos);
 
	//关闭并释放所有资源
	void Close();

 
public:
	AVInputFormat* inFmt = NULL;
	AVDictionary* decOpitons;

	int videoIndex, audioIndex;     //音视频索引号
	int srcWidth, srcHeight;        //原视频分辨率
	double fps;        //帧率
	int encFps;        //编码帧率
	int totalTime;  //总时长 单位：秒
	int bitrate;    //码率
	AVCodecID videCodecID;  //视频编码格式
	AVCodecContext* encoderCtx ;    //编码器上下文
	AVCodecContext* decoderCtx;    //解码器上下文
	AVCodecContext* audioDecoderCtx ;    //音频解码器上下文
	bool isNetUrl;   //是否为网络流
	bool isUDPStream;  //是否为UDP流
	//bool isFinished; //获取帧超时
	AVFormatContext* inFormatCtx;  //输入文件上下文 
	string videoUrl;
	bool m_NextFrameIsKey  ;   //是否设置为关键帧
	int64_t	m_tStart;
	int timeout;  //超时中断时间设置 单位s
private:
	AVDictionary* encOpitons;
	long long  pktCount ;
	char errBuffer[1024];
	AVPixelFormat dstFormat ;   //转换的目标格式
	AVPixelFormat srcFormat;     //原始格式
	AVFormatContext	*outFormatCtx; //输出文件上下文 
	AVCodec* decoderCodec;         //解码器
	AVCodec* audioDecoderCodec ;         //音频解码器
	AVCodec* encoderCodec ;         //编码器
	SwsContext* sCtx ;              //格式转换器
 
	AVStream* in_stream;
	AVStream* out_stream;
	  
	//滤镜
	AVFilterGraph* filterGraph ;
	AVFilterContext* bufferSrcCtx ;
	AVFilterContext* bufferSinkCtx ;  
	AVFilterInOut *inputs ;
	AVFilterInOut *outputs ;
 
	//滤镜2
	AVFilterGraph* filterGraph2;
	AVFilterContext* bufferSrcCtx2;
	AVFilterContext* bufferSinkCtx2;  
	AVFilterInOut* inputs2;
	AVFilterInOut* outputs2;
};

