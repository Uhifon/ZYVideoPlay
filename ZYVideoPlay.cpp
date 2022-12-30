// ZYVideoPlay.cpp : ���� DLL Ӧ�ó���ĵ���������
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
* ��������������Ƶ
* @param url����Ƶ�ļ�����Ƶ����ַ
* @param data��ͼ��ؼ��������̨¼��ʱ����ֵNULL
*
* @return �ɹ�/ʧ��
*/
 bool _stdcall VideoPlayer_OpenVideo(int nPort, const char* url, const void* hWnd)
 {
	 player[nPort].Port = nPort;
	 hwdList[nPort] = hWnd;
	 player[nPort].OpenVideo(url, hWnd);
	 return true;
 }

/**
*  ���������� ��ʼ����
*/
bool _stdcall VideoPlayer_StartPlay(int nPort)
{
	if (!player[nPort].isInited)
		return false;
	player[nPort].playStatus = PlayStatus::Playing;
	return true;
}


/**
*  ���������� ��ͣ����
*/
bool _stdcall VideoPlayer_PausePlay(int nPort)
{
	if (!player[nPort].isInited)
		return false;
	player[nPort].playStatus = PlayStatus::Pause;
	return true;
}


/**
* �����������ر���Ƶ�ͷ���Դ
*/
 bool _stdcall VideoPlayer_CloseVideo(int nPort)
 {
	 player[nPort].StopPlay();
	 return true;
 }

 /**
* ������������֡���󲥷�
* @param nPort������ͨ����[0,31]
*/
 bool _stdcall VideoPlayer_NextFrame(int nPort)
 {
	 return true;
 }

 /**
* ������������֡��ǰ����
* @param nPort������ͨ����[0,31]
*/
 bool _stdcall VideoPlayer_PrevFrame(int nPort)
 {
	 return true;
 }

/**
* ������������ת��Ƶ��ĳһʱ��λ�� 
* @param nPort������ͨ����[0,31]
* @param pos����Ƶλ�ðٷֱ�
*/
bool _stdcall VideoPlayer_SetPlayPos(int nPort, float pos)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;
	 int ret = player[nPort].xffmpeg->Seek(pos);
	return  ret ==0;
}

/**
* ������������ʾ/���ص�ǰϵͳʱ��
* @param nPort������ͨ����[0,31]
* @param visible���Ƿ���ʾ
* @param xPos������λ��X����Ȱٷֱ�
* @param yPos������λ��Y���߶Ȱٷֱ�
* @param fontSize�������С��Ĭ��50
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
		char str[255] = "\0";  //strftime ��һ�������� char * 
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
* ���������������ٶȿ���
* @param speed���ٶ�   0:0.25���� 1��0.5����  2����������   3��1.25����  4��1.5����  5��2���� 6:3����  7��4���� 
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

//��ǰ����ʱ���ȡ
int _stdcall  VideoPlayer_GetPlayTime(int nPort)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;

	AVRational time_base = player[nPort].xffmpeg->inFormatCtx->streams[0]->time_base;
	int now_time = player[nPort].nowPTS * av_q2d(time_base);
	return now_time;
}

/**
* ������������ȡ��ǰ����ʱ��
*
* @return ����ʱ��  ��λ����
*/
bool _stdcall VideoPlayer_GetPlayedTime(int nPort)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;
	return true;

}

/**
* �������������Ŵ�������
* @param data�����ھ����������С��Ĵ���
*
* @return �ɹ�/ʧ��
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
 * ��������������¼�ƴ洢����
 * @param nPort��
 * @param path��¼�񱣴�·�������ļ���
 * @param fps��֡��,����������Ƶ����ͬ֡�� 
 * @param quality��: ͼ��������0-10�� Խ������Խ�ã�ռ���ڴ�Խ������Ϊ0ʱ��Ϊ�Զ�����
 * @param duration�������ļ�ʱ��(min),Ĭ��60����
 * @param format:�洢��ʽ,Ĭ��Ϊmp4
 *
 * @return �ɹ�/ʧ��
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
	 
	//��ʼ��������
	int ret = 0;
	if (player[nPort].width != 0)
		ret = player[nPort].xffmpeg->InitEncoderCodec(AVCodecID::AV_CODEC_ID_H264, player[nPort].width, player[nPort].height, player[nPort].encFps, quality);
	if (ret < 0)
		return false;
	player[nPort].StartRecord();
	return true;
}


/**
* ��������������¼��
*/
bool _stdcall VideoPlayer_StopRecord(int nPort)
{
	if (player[nPort].playStatus == PlayStatus::Close)
		return false;
	player[nPort].StopRecord();
	return true;
}

/**
* ������������Ƶ��ͼ
* @param nPort������ͨ����[0,32]
* @param path�� ͼ�񱣴��·����������׺��
* @param format��ͼ���ʽ
*
* @return �ɹ�/ʧ��
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
* �������������õ���OSD�ı�
* @param nPort��
* @param OsdID��OSD�ı����
* @param param����ʾ���ı�����
*
* @return �ɹ�/ʧ��
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
		if (osdParam.backColor.A == 0) //͸������
			sprintf_s(filters_descr, "drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\%s':fontsize=%d:fontcolor='#%02x%02x%02x':text='%s':x=%d:y=%d", fontStr.c_str(), osdParam.fontSize, osdParam.fontColor.R, osdParam.fontColor.G, osdParam.fontColor.B, str.c_str(), x, y);
		else  //��͸������
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
* �����������������е�OSD�ı����ܹ�20�У�
* @param nPort������ͨ����[0,31]
* @param para��osd����
*
* @return �ɹ�/ʧ��
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
				if (osdParam.backColor.A == 0) //͸������
					sprintf_s(filters_descr, "drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\%s':fontsize=%d:fontcolor='#%02x%02x%02x':text='%s':x=%d:y=%d", fontStr.c_str(), osdParam.fontSize, osdParam.fontColor.R, osdParam.fontColor.G, osdParam.fontColor.B, str.c_str(), x, y);
				else   //��͸������
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
* �������������OSD�ı�
* @param nPort��
* @param OsdID��OSD�ı����
*
* @return �ɹ�/ʧ��
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
* �������������ý��뻺����д�С
* @param nPort������ͨ����[0,31]
* @param size����С��Χ��1-25����Ĭ��Ϊ10�����ù�С�ᶪ֡�����ù�����ܻᵼ�»����ӳ�
*
* @return �ɹ�/ʧ��
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