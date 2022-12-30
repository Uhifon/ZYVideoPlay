#pragma once
#ifdef __DLLEXPORT
#define __DLL_EXP _declspec(dllexport)    
#else
#define __DLL_EXP _declspec(dllimport)    
#endif 

#include <stdio.h>
#include <string>

using namespace std;
//录像状态
enum RecordStatus
{
	Idle,
	Recording,
	Stop
};

//视频存储格式
enum VideoFormat
{
	MP4,
	AVI,
};

//图片存储格式
enum PictureFormat
{
	JPEG,
	PNG,
	BMP,
};

//播放器状态
enum PlayStatus
{
	Close,
	Playing,
	Pause
};

//滤镜种类
enum FilterType
{
	Time,
	Reticle,
	Text,
	Other
};

//颜色
struct RGBAColor
{
	BYTE R;
	BYTE G;
	BYTE B;
	BYTE A;
};

//OSD字体属性结构体
 struct OSDParameter
{
	char* text;
	int fontSize;        //汉字字号
	int fontFamily;      //0=黑体，1=楷体，2=宋体
	RGBAColor fontColor;      //字体颜色 rgb表示
	RGBAColor backColor;      //字体背景色 rgb表示
	float x;             //字体坐标 x：宽度百分比0-1
	float y;             //字体坐标 y：高度百分比0-1
	bool visible;        //是否可见
};

 //格式
 enum  FrameFormat
 {
	 AUDIO16,
	 RGB32,
	 YV12,
	 UVVY,
	 YUVJ420P
 };

 struct FrameInfo
{
	int width;  					   	//画面宽，单位像素。如果是音频数据则为0；
	int height; 					  	//画面高。如果是音频数据则为0；
	int stamp;  					  	//时标信息，单位毫秒。
	FrameFormat format;                 //数据类型，0：AUDIO16，1：RGB32，2：（YUV420P）YV12，3：YUVJ420P 
	int frameRate; 					    //实时帧率。
};

/**
* 功能描述：打开视频
* @param nPort：播放通道号[0,31] 
* @param url：视频文件或视频流地址
* @param data：图像控件句柄，后台录制时，赋值NULL
*
* @return 成功/失败
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_OpenVideo(int nPort, const char* url, const void* hWnd);

/**
*  功能描述： 开始播放
* @param nPort：播放通道号[0,31] 
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_StartPlay(int nPort);

/**
*  功能描述： 暂停播放
* @param nPort：播放通道号[0,31] 
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_PausePlay(int nPort);


/**
* 功能描述：关闭视频释放资源
* @param nPort：播放通道号[0,31] 
*/
extern "C" __declspec(dllexport)  bool _stdcall VideoPlayer_CloseVideo(int nPort);

/**
* 功能描述：设置文件播放指针的相对位置（百分比）
* @param nPort：播放通道号[0,31] 
* @param pos：播放时间位置  0-100
* @param fReleatePos：0-100
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_SetPlayPos(int nPort, float fReleatePos);

/**
* 功能描述：播放速度控制
* @param nPort：播放通道号[0,31] 
* @param speed：速度  0：正常播放   1：1.25倍速  2：1.5倍速 3：1.75倍速 4:2倍速  5：0.75倍速  6：0.5倍速 7：0.25倍速
*/
extern "C" __declspec(dllexport) void _stdcall VideoPlayer_ChangeSpeed(int nPort, int speed);

/**
* 功能描述：获取当前播放时间
* @param nPort：播放通道号[0,31] 
* @return 播放时间  单位：秒
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_GetPlayedTime(int nPort);

/**
* 功能描述：播放窗口缩放
* @param nPort：播放通道号[0,31] 
* @return 成功/失败
*/
extern "C" __declspec(dllexport)  bool _stdcall VideoPlayer_ResizeWindow(int nPort);

/**
* 功能描述：设置录制存储参数
* @param nPort：
* @param path：录像保存路径包含文件名
* @param fps：帧率,请设置与视频流相同帧率
* @param quality：: 图像质量（0-5） 越高质量越好，占用内存越大，设置为0时，为自动码率
* @param duration：单个文件时长(min),默认60分钟，时间到时将在文件名后加上“_X”标识
* @param format:存储格式,默认为mp4
*
* @return 成功/失败
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_StartRecord(int nPort, const char* path, int fps, float quality=5, int duration = 60, VideoFormat format = VideoFormat::MP4);

/**
* 功能描述：结束录制
* @param nPort：播放通道号[0,31] 
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_StopRecord(int nPort);

/**
* 功能描述：视频截图
* @param nPort：播放通道号[0,31]
* @param path： 图像保存的路径，不含后缀名
* @param format：图像格式，默认为jpeg
*
* @return 成功/失败
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_ShotImage(int nPort, const char* path, PictureFormat format= PictureFormat::JPEG);


/**
* 功能描述：设置某一行的OSD叠加文本
* @param nPort：播放通道号[0,31] 
* @param osdID：OSD文本序号
* @param para：osd属性
*
* @return 成功/失败
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_SetOSDText(int nPort, int osdID, OSDParameter osdParam);

/**
* 功能描述：设置所有的OSD文本（总共20行）
* @param nPort：播放通道号[0,31]
* @param para：osd属性
*
* @return 成功/失败
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_SetAllOSDTexts(int nPort, OSDParameter osdParams[]);

/**
* 功能描述：清除所有OSD文本
* @param nPort：播放通道号[0,31] 
*
* @return 成功/失败
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_ClearOSDText(int nPort);


/**
* 功能描述：设置解码缓存队列大小
* @param nPort：播放通道号[0,31]
* @param size：大小范围【1-20】，默认为5，设置过小会丢帧，设置过大可能会导致画面延迟
*
* @return 成功/失败
*/
extern "C" __declspec(dllexport) void _stdcall VideoPlayer_SetDecodedBufferSize(int nPort, int size);

/**
* 功能描述：单帧往后播放 
* @param nPort：播放通道号[0,31]
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_NextFrame(int nPort);

/**
* 功能描述：单帧往前播放 
* @param nPort：播放通道号[0,31]
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_PrevFrame(int nPort);

/**
* 功能描述：跳转视频到某一时间位置 
* @param nPort：播放通道号[0,31]
* @param pos：视频位置百分比
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_SetPlayPos(int nPort,float pos);

/**
* 功能描述：显示/隐藏当前系统时间
* @param nPort：播放通道号[0,31]
* @param visible：是否显示
* @param xPos：坐标位置X，宽度百分比
* @param yPos：坐标位置Y，高度百分比
* @param fontSize：字体大小，默认50
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_SetDateTime(int nPort,bool visible, float xPos, float yPos,int fontSize=50);
 
 
/**
* 功能描述：当前解码完成数据回调
* @param nPort：播放通道号[0,31]
* @param pBuf：yuv数据指针
* @param nSize：数据大小
* @param pFrameInfo：帧信息
*/
typedef void(_stdcall* DecodeCallBack)(int nPort, const unsigned char* pBuf, int nSize, FrameInfo& pFrameInfo);

/**
* 功能描述：设置解码回调函数
* @param nPort：播放通道号[0,31]
* @param pDecodeProc：回调函数
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_SetDecCallBack(int nPort, DecodeCallBack pDecodeProc);

 

/**
* 功能描述：在显示句柄窗口中绘制文本、图形等
* @param nPort：播放通道号[0,31]
* @param hWnd：视频窗口句柄，用于创建GDI
* @param width：视频窗口宽度
* @param height：视频窗口高度
*/
typedef void(_stdcall* DrawCallBack)(int nPort, const void* hWnd,int width,int height);

extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_SetDrawCallBack(int nPort, DrawCallBack pDrawProc);

 
