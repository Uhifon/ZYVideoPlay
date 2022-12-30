// ZYVideoPlay.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "ZYVideoPlay.h"
#include <thread>
#include <vector>
#include <fstream>
#include <algorithm>
#include "Player.h"
#include "Common.h"
 

Player player[32];
const void* hwdList[32];
 
 
/**
* 功能描述：打开视频
* @param url：视频文件或视频流地址
* @param data：图像控件句柄，后台录制时，赋值NULL
*
* @return 成功/失败
*/
 bool _stdcall VideoPlayer_OpenVideo(int nPort, const char* url, const void* hWnd)
 {
	 player[nPort].Port = nPort;
	 hwdList[nPort] = hWnd;
	 player[nPort].OpenVideo(url, hWnd);
	 return true;
 }

/**
*  功能描述： 开始播放
*/
bool _stdcall VideoPlayer_StartPlay(int nPort)
{
	if (!player[nPort].isInited)
		return false;
	player[nPort].playStatus = PlayStatus::Playing;
	return true;
}


/**
*  功能描述： 暂停播放
*/
bool _stdcall VideoPlayer_PausePlay(int nPort)
{
	if (!player[nPort].isInited)
		return false;
	player[nPort].playStatus = PlayStatus::Pause;
	return true;
}


/**
* 功能描述：关闭视频释放资源
*/
 bool _stdcall VideoPlayer_CloseVideo(int nPort)
 {
	 player[nPort].StopPlay();
	 return true;
 }

 /**
* 功能描述：单帧往后播放
* @param nPort：播放通道号[0,31]
*/
 bool _stdcall VideoPlayer_NextFrame(int nPort)
 {
	 return true;
 }

 /**
* 功能描述：单帧往前播放
* @param nPort：播放通道号[0,31]
*/
 bool _stdcall VideoPlayer_PrevFrame(int nPort)
 {
	 return true;
 }

/**
* 功能描述：跳转视频到某一时间位置 
* @param nPort：播放通道号[0,31]
* @param pos：视频位置百分比
*/
bool _stdcall VideoPlayer_SetPlayPos(int nPort, float pos)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;
	 int ret = player[nPort].xffmpeg->Seek(pos);
	return  ret ==0;
}

/**
* 功能描述：显示/隐藏当前系统时间
* @param nPort：播放通道号[0,31]
* @param visible：是否显示
* @param xPos：坐标位置X，宽度百分比
* @param yPos：坐标位置Y，高度百分比
* @param fontSize：字体大小，默认50
*/
bool _stdcall VideoPlayer_SetDateTime(int nPort, bool visible, float xPos, float yPos,int fontSize)
{
	if (!visible)
	{
		player[nPort].dateTimeFilterDescr = "";
	}
	else
	{
		int x = (int)(xPos * player[nPort].width);
		int y = (int)(yPos * player[nPort].height);
		char str[255] = "\0";  //strftime 第一个参数是 char * 
		string fontStr = Common::GetFontFamily(0);
		//"drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\simhei.ttf':x=0:y=0:fontcolor=white:fontsize=80:text='%{localtime\\:%Y-%m-%d %H\\\\\\:%M\\\\\\:%S}'";
		char* txt = "%{localtime\\:%Y-%m-%d %H\\\\\\:%M\\\\\\:%S}";
		sprintf_s(str, "drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\%s':fontsize=%d:fontcolor='#%02x%02x%02x':text='%s':x=%d:y=%d", fontStr.c_str(), fontSize, 255, 255, 255, txt, x, y);
		player[nPort].dateTimeFilterDescr = str;
	}
	int ret = player[nPort].InitOSD();
	return ret == 0;
}

/**
* 功能描述：播放速度控制
* @param speed：速度   0:0.25倍数 1：0.5倍速  2：正常播放   3：1.25倍速  4：1.5倍速  5：2倍速 6:3倍速  7：4倍速 
*/
void _stdcall VideoPlayer_ChangeSpeed(int nPort, int speed)
{
	switch (speed)
	{
	case 0:player[nPort].playSpeed = 0.25; break;
	case 1:player[nPort].playSpeed = 0.5; break;
	case 2:player[nPort].playSpeed = 1; break;
	case 3:player[nPort].playSpeed = 1.25; break;
	case 4:player[nPort].playSpeed = 1.5; break;
	case 5:player[nPort].playSpeed = 2; break;
	case 6:player[nPort].playSpeed = 3; break;
	case 7:player[nPort].playSpeed = 4; break;
	default:player[nPort].playSpeed = 1; break;
	}
}

//当前播放时间获取
int _stdcall  VideoPlayer_GetPlayTime(int nPort)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;

	AVRational time_base = player[nPort].xffmpeg->inFormatCtx->streams[0]->time_base;
	int now_time = player[nPort].nowPTS * av_q2d(time_base);
	return now_time;
}

/**
* 功能描述：获取当前播放时间
*
* @return 播放时间  单位：秒
*/
bool _stdcall VideoPlayer_GetPlayedTime(int nPort)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;
	return true;

}

/**
* 功能描述：播放窗口缩放
* @param data：窗口句柄，调整大小后的窗口
*
* @return 成功/失败
*/
 bool _stdcall VideoPlayer_ResizeWindow(int nPort)
 {
	 int ret = 0;
	 if (player[nPort].playStatus != PlayStatus::Close)
		 ret = player[nPort].sdlDisplay->ResizeWindow(hwdList[nPort]);
	 else
		 ret = -1;
	 return ret == 0;
 }

 /**
 * 功能描述：设置录制存储参数
 * @param nPort：
 * @param path：录像保存路径包含文件名
 * @param fps：帧率,请设置与视频流相同帧率 
 * @param quality：: 图像质量（0-10） 越高质量越好，占用内存越大，设置为0时，为自动码率
 * @param duration：单个文件时长(min),默认60分钟
 * @param format:存储格式,默认为mp4
 *
 * @return 成功/失败
 */
bool _stdcall VideoPlayer_StartRecord(int nPort, const char* path, int fps, float quality, int duration,VideoFormat format)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;
	if (duration <= 0 || quality < 0 )
	{
		return false;
	}
	player[nPort].encFps = fps;
	player[nPort].maxRecordTimeLength = duration;
    player[nPort].videoQuality = quality;
	player[nPort].recordVideoDir = path;
	if(format== VideoFormat::MP4)
		player[nPort].recordVideoFmt = ".mp4";
	else
		player[nPort].recordVideoFmt = ".avi";
	player[nPort].recordPath = player[nPort].recordVideoDir + player[nPort].recordVideoFmt;
	 
	//初始化编码器
	int ret = 0;
	if (player[nPort].width != 0)
		ret = player[nPort].xffmpeg->InitEncoderCodec(AVCodecID::AV_CODEC_ID_H264, player[nPort].width, player[nPort].height, player[nPort].encFps, quality);
	if (ret < 0)
		return false;
	player[nPort].StartRecord();
	return true;
}


/**
* 功能描述：结束录制
*/
bool _stdcall VideoPlayer_StopRecord(int nPort)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;
	player[nPort].StopRecord();
	return true;
}

/**
* 功能描述：视频截图
* @param nPort：播放通道号[0,32]
* @param path： 图像保存的路径，不含后缀名
* @param format：图像格式
*
* @return 成功/失败
*/
bool _stdcall VideoPlayer_ShotImage(int nPort, const char* path, PictureFormat format)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;
	string path1(path);
	if (format == PictureFormat::JPEG)
		path1 += ".jpg";
	else if (format == PictureFormat::PNG)
		path1 += ".png";
	else if (format == PictureFormat::BMP)
		path1 += ".bmp";
	return player[nPort].CatchImage(path1);
}

/**
* 功能描述：设置叠加OSD文本
* @param nPort：
* @param OsdID：OSD文本序号
* @param param：显示的文本参数
*
* @return 成功/失败
*/
bool _stdcall VideoPlayer_SetOSDText(int nPort, int osdID,  OSDParameter osdParam)
{
	if (player[nPort].playStatus == PlayStatus::Close )
		return false;
	if (osdParam.fontSize == 0)
		osdParam.fontSize = 30;
	string fontStr = Common::GetFontFamily(osdParam.fontFamily);
 
	if (osdParam.text != string())
	{
		string txt = osdParam.text;
		Common::string_replase(txt, ":", "\\\:");
		Common::string_replase(txt, "/", "\\\/");
		string str = Common::GBK_2_UTF8(txt);
		char filters_descr[255] = { 0 };
		int x = (int)(osdParam.x * player[nPort].width);
		int y = (int)(osdParam.y * player[nPort].height);
		if (osdParam.backColor.A == 0) //透明背景
			sprintf_s(filters_descr, "drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\%s':fontsize=%d:fontcolor='#%02x%02x%02x':text='%s':x=%d:y=%d", fontStr.c_str(), osdParam.fontSize, osdParam.fontColor.R, osdParam.fontColor.G, osdParam.fontColor.B, str.c_str(), x, y);
		else  //非透明背景
			sprintf_s(filters_descr, "drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\%s':fontsize=%d:fontcolor='#%02x%02x%02x':box=1:boxcolor='#%02x%02x%02x':text='%s':x=%d:y=%d", fontStr.c_str(), osdParam.fontSize, osdParam.fontColor.R, osdParam.fontColor.G, osdParam.fontColor.B, osdParam.backColor.R, osdParam.backColor.G, osdParam.backColor.B, str.c_str(), x, y);
		player[nPort].filterParams[osdID]->descr = filters_descr;
	}
	else
		player[nPort].filterParams[osdID]->descr = "";
	player[nPort].filterParams[osdID]->osdID = osdID;
	player[nPort].filterParams[osdID]->visible = osdParam.visible;
 	int ret = player[nPort].InitOSD();
	return ret == 0;
}


/**
* 功能描述：设置所有的OSD文本（总共20行）
* @param nPort：播放通道号[0,31]
* @param para：osd属性
*
* @return 成功/失败
*/
bool _stdcall VideoPlayer_SetAllOSDTexts(int nPort, OSDParameter osdParams[])
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;
	if (osdParams == NULL)
		return false;

	for (int i = 0; i < FILTER_PARA_SIZE; i++)
	{
		OSDParameter osdParam = osdParams[i];
		if (osdParam.visible)
		{
			if (osdParam.fontSize == 0)
				osdParam.fontSize = 30;
			string fontStr = Common::GetFontFamily(osdParam.fontFamily);
			if (osdParam.text != string())
			{
				string txt = osdParam.text;
				Common::string_replase(txt, ":", "\\\:");
				Common::string_replase(txt, "/", "\\\/");
				string str = Common::GBK_2_UTF8(txt);
				char filters_descr[255] = { 0 };
				int x = (int)(osdParam.x * player[nPort].width);
				int y = (int)(osdParam.y * player[nPort].height);
				if (osdParam.backColor.A == 0) //透明背景
					sprintf_s(filters_descr, "drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\%s':fontsize=%d:fontcolor='#%02x%02x%02x':text='%s':x=%d:y=%d", fontStr.c_str(), osdParam.fontSize, osdParam.fontColor.R, osdParam.fontColor.G, osdParam.fontColor.B, str.c_str(), x, y);
				else   //非透明背景
					sprintf_s(filters_descr, "drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\%s':fontsize=%d:fontcolor='#%02x%02x%02x':box=1:boxcolor='#%02x%02x%02x':text='%s':x=%d:y=%d", fontStr.c_str(), osdParam.fontSize, osdParam.fontColor.R, osdParam.fontColor.G, osdParam.fontColor.B, osdParam.backColor.R, osdParam.backColor.G, osdParam.backColor.B, str.c_str(), x, y);
				player[nPort].filterParams[i]->descr = filters_descr;
			}
			else
				player[nPort].filterParams[i]->descr = "";
			player[nPort].filterParams[i]->osdID = i;
			player[nPort].filterParams[i]->visible = osdParam.visible;
		}
		else
		{
			player[nPort].filterParams[i]->descr = "";
			player[nPort].filterParams[i]->osdID = i;
			player[nPort].filterParams[i]->visible = false;
		}
		
	}	 
	int ret = player[nPort].InitOSD();
	return ret == 0;
}

/**
* 功能描述：清除OSD文本
* @param nPort：
* @param OsdID：OSD文本序号
*
* @return 成功/失败
*/
bool _stdcall VideoPlayer_ClearOSDText(int nPort)
{
	for (int i = 0; i < FILTER_PARA_SIZE; i++)
	{
		delete player[nPort].filterParams[i];
		player[nPort].filterParams[i] = NULL;
	    player[nPort].filterParams[i] = new FilterParam;
	}
	int ret = player[nPort].InitOSD();
	return ret == 0;
}

/**
* 功能描述：设置解码缓存队列大小
* @param nPort：播放通道号[0,31]
* @param size：大小范围【1-25】，默认为10，设置过小会丢帧，设置过大可能会导致画面延迟
*
* @return 成功/失败
*/
void _stdcall VideoPlayer_SetDecodedBufferSize(int nPort, int size)
{
	player[nPort].decodedBufferSize = size;
}

bool _stdcall VideoPlayer_SetDecCallBack(int nPort, DecodeCallBack pDecodeProc)
{
	player[nPort].decodeCallBack = pDecodeProc;
	return true;
}

bool _stdcall VideoPlayer_SetDrawCallBack(int nPort, DrawCallBack pDrawProc)
{
	player[nPort].drawCallBack = pDrawProc;
	return true;
}