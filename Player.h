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
	//osd编号
	int osdID;
	//滤镜描述
    string descr;
	//显示与否
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
	//线程相关
	thread* getDataThread; //获取流线程
	thread* decodeVideoThread;//视频解码线程
	thread* decodeAudiohread; //音频解码线程
	thread* frameProcessingThread; //图像处理线程
	thread* recordThread; //录制线程

	mutex packetDataMutex;//视频包锁
	mutex audioPacketMutex;//音频包锁
	mutex decodeDataMutex;//解码锁	
	mutex  filterMutex; //滤镜1锁
	mutex  filterMutex2; //滤镜2锁
	mutex recordMutex;  //录制锁
 
	//buffer
	list<AVPacket*> packetBuffer;//存放读取待解码的视频包
	list<AVPacket*> audioPacketBuffer;//存放读取待解码的音频包
	list<AVFrame*> recordBuffer;//用来存放待编码的数据	
	list<AVFrame*> decodedBuffer;//存放已经解码的帧

	//字符叠加相关
	int filterFlag;  //选择了第几个滤镜，为0时，未选择滤镜
	string strOSD;       //新的字符叠加代码
	string odlDescr;	 //旧的字符叠加代码
	int filter_change;   //滤镜参数是否变更
  
	int textNum;  //文本数量

	
	bool creatFileFlag;
	bool finishedFileFlag;
	 
	void ReopenStream();
public:
	//视频参数
	const void* playHandle; //播放窗口句柄

	//视频录制
	bool bRecord;
	int Port;
	XFFMpeg* xffmpeg;
    PlayStatus playStatus;  //播放状态
	SDLDisplay* sdlDisplay;
	DecodeCallBack decodeCallBack;
	DrawCallBack drawCallBack;
	
	 
    bool isInited;//初始化准备完成
	int width;
	int height;
	int totalTime;
	int bitrate;
	string encodeFormat;
	int frameCnt;
	int realFPS;
	int fps;

    FilterParam* filterParams[FILTER_PARA_SIZE];//水印、滤镜列表 
	string	dateTimeFilterDescr;  //时间水印

	int decodedBufferSize;

	//录像
	int64_t pts, dts;
	int last_pts;
	int last_dts;
	long startTime;            //录像开始时间
	string recordPath;         //录制视频路径+文件名
	long fileTime;             //当前录像文件时长 ms
	int videoQuality;        //视频质量
	int encFps;                //编码帧率
	int maxRecordTimeLength;   //录像单个文件大小  min
	int RecordType;            //录像方式，0：前台/1：后台
	string recordVideoDir;
	string recordVideoFmt;

	int playSpeed;
	int nowPTS;

	//截图路径
	bool bShotting;
	string shotImagePath;

	unsigned char* yuv_buffer;
	int video_decode_size ;
};

