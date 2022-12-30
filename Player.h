#pragma once
#include <SDL.h> 
#include "XFFMpeg.h"
#include "SDLDisplay.h"
#include <thread>
#include "ZYVideoPlay.h"
#include <list>
#include <mutex>
#include <string>

#define FILTER_PARA_SIZE 20 

using namespace std;

struct FilterParam
{
	//osd���
	int osdID;
	//�˾�����
    string descr;
	//��ʾ���
	bool visible;
};

 
class Player
{
	
public:
	Player();
	~Player();
	static string EnumToString(AVCodecID ci);
	bool OpenVideo(const char* url, const void* hWnd);
	void StartPlay(const void* hwd, int width, int height);
	void StopPlay();
	void StartRecord();
    void StopRecord();
	void GetDataThread();
	void DecodeVideoThread();
	void DecodeAudioThread();
	void FrameProcessingThread();
	void RecordThread();
	int InitOSD();
	bool CombineOSDCommand(string& descr);
	int  Filtering(AVFrame* srcFrame, AVFrame* dstFrame,int filterNum);
	void ClearBuffer();
	bool CatchImage(string path);
	typedef void(_stdcall* DisconnectCallBack)();
	void  RegisterDisconnectCallBack(DisconnectCallBack pDisconnectProc);
private:
	//�߳����
	thread* getDataThread; //��ȡ���߳�
	thread* decodeVideoThread;//��Ƶ�����߳�
	thread* decodeAudiohread; //��Ƶ�����߳�
	thread* frameProcessingThread; //ͼ�����߳�
	thread* recordThread; //¼���߳�

	mutex packetDataMutex;//��Ƶ����
	mutex audioPacketMutex;//��Ƶ����
	mutex decodeDataMutex;//������	
	mutex  filterMutex; //�˾�1��
	mutex  filterMutex2; //�˾�2��
	mutex recordMutex;  //¼����
 
	//buffer
	list<AVPacket*> packetBuffer;//��Ŷ�ȡ���������Ƶ��
	list<AVPacket*> audioPacketBuffer;//��Ŷ�ȡ���������Ƶ��
	list<AVFrame*> recordBuffer;//������Ŵ����������	
	list<AVFrame*> decodedBuffer;//����Ѿ������֡

	//�ַ��������
	int filterFlag;  //ѡ���˵ڼ����˾���Ϊ0ʱ��δѡ���˾�
	string strOSD;       //�µ��ַ����Ӵ���
	string odlDescr;	 //�ɵ��ַ����Ӵ���
	int filter_change;   //�˾������Ƿ���
  
	int textNum;  //�ı�����

	
	bool creatFileFlag;
	bool finishedFileFlag;
	 
	void ReopenStream();
public:
	//��Ƶ����
	const void* playHandle; //���Ŵ��ھ��

	//��Ƶ¼��
	bool bRecord;
	int Port;
	XFFMpeg* xffmpeg;
    PlayStatus playStatus;  //����״̬
	SDLDisplay* sdlDisplay;
	DecodeCallBack decodeCallBack;
	DrawCallBack drawCallBack;
	
	 
    bool isInited;//��ʼ��׼�����
	int width;
	int height;
	int totalTime;
	int bitrate;
	string encodeFormat;
	int frameCnt;
	int realFPS;
	int fps;

    FilterParam* filterParams[FILTER_PARA_SIZE];//ˮӡ���˾��б� 
	string	dateTimeFilterDescr;  //ʱ��ˮӡ

	int decodedBufferSize;

	//¼��
	int64_t pts, dts;
	int last_pts;
	int last_dts;
	long startTime;            //¼��ʼʱ��
	string recordPath;         //¼����Ƶ·��+�ļ���
	long fileTime;             //��ǰ¼���ļ�ʱ�� ms
	int videoQuality;        //��Ƶ����
	int encFps;                //����֡��
	int maxRecordTimeLength;   //¼�񵥸��ļ���С  min
	int RecordType;            //¼��ʽ��0��ǰ̨/1����̨
	string recordVideoDir;
	string recordVideoFmt;

	int playSpeed;
	int nowPTS;

	//��ͼ·��
	bool bShotting;
	string shotImagePath;

	unsigned char* yuv_buffer;
	int video_decode_size ;
};

