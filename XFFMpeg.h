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

	/*����*/
	int OpenInput(const string url);
	void ClearBuffer();//���buffer
	void CloseInput();
	int InitDecoderCodec();
	int InitAduioDecoderCodec();
	int ReadPacket(AVPacket* pkt);
	 
	int DecodeVideo(AVPacket* pkt, AVFrame* yuvFrame);
	int DecodeAudio(AVPacket* pkt, vector<AVFrame*>& frames);

	/*����*/
	int OpenOutput(const string path);
	void CloseOutput();
	int InitEncoderCodec(AVCodecID codecID, int width, int height, int fps, float quality);
	int Encode(AVFrame *pFrame, AVPacket* outPkt);
	int WritePacket(AVPacket* packet);
	int FinishWrite();


	/*ת��*/
	int InitSwsContext(AVCodecContext* videoCtx, AVPixelFormat dstFmt, int width, int height);
	int FormatConvert(AVFrame* srcFrame, AVFrame* dstFrame);
	void SwsFree();

	/*�˾�1*/
	int InitFilters(const char* filtersDescr);
	int FilterFrame(AVFrame* srcFrame, AVFrame* dstFrame);
	void CloseFilter();

	/*�˾�2*/
	int InitFilters2(const char* filtersDescr);
	int FilterFrame2(AVFrame* srcFrame, AVFrame* dstFrame);
	void CloseFilter2();

	//��ת 0-1
	int Seek(float pos);
 
	//�رղ��ͷ�������Դ
	void Close();

 
public:
	AVInputFormat* inFmt = NULL;
	AVDictionary* decOpitons;

	int videoIndex, audioIndex;     //����Ƶ������
	int srcWidth, srcHeight;        //ԭ��Ƶ�ֱ���
	double fps;        //֡��
	int encFps;        //����֡��
	int totalTime;  //��ʱ�� ��λ����
	int bitrate;    //����
	AVCodecID videCodecID;  //��Ƶ�����ʽ
	AVCodecContext* encoderCtx ;    //������������
	AVCodecContext* decoderCtx;    //������������
	AVCodecContext* audioDecoderCtx ;    //��Ƶ������������
	bool isNetUrl;   //�Ƿ�Ϊ������
	bool isUDPStream;  //�Ƿ�ΪUDP��
	//bool isFinished; //��ȡ֡��ʱ
	AVFormatContext* inFormatCtx;  //�����ļ������� 
	string videoUrl;
	bool m_NextFrameIsKey  ;   //�Ƿ�����Ϊ�ؼ�֡
	int64_t	m_tStart;
	int timeout;  //��ʱ�ж�ʱ������ ��λs
private:
	AVDictionary* encOpitons;
	long long  pktCount ;
	char errBuffer[1024];
	AVPixelFormat dstFormat ;   //ת����Ŀ���ʽ
	AVPixelFormat srcFormat;     //ԭʼ��ʽ
	AVFormatContext	*outFormatCtx; //����ļ������� 
	AVCodec* decoderCodec;         //������
	AVCodec* audioDecoderCodec ;         //��Ƶ������
	AVCodec* encoderCodec ;         //������
	SwsContext* sCtx ;              //��ʽת����
 
	AVStream* in_stream;
	AVStream* out_stream;
	  
	//�˾�
	AVFilterGraph* filterGraph ;
	AVFilterContext* bufferSrcCtx ;
	AVFilterContext* bufferSinkCtx ;  
	AVFilterInOut *inputs ;
	AVFilterInOut *outputs ;
 
	//�˾�2
	AVFilterGraph* filterGraph2;
	AVFilterContext* bufferSrcCtx2;
	AVFilterContext* bufferSinkCtx2;  
	AVFilterInOut* inputs2;
	AVFilterInOut* outputs2;
};

