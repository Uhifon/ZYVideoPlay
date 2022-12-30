#pragma once
#ifdef __DLLEXPORT
#define __DLL_EXP _declspec(dllexport)    
#else
#define __DLL_EXP _declspec(dllimport)    
#endif 

#include <stdio.h>
#include <string>

using namespace std;
//¼��״̬
enum RecordStatus
{
	Idle,
	Recording,
	Stop
};

//��Ƶ�洢��ʽ
enum VideoFormat
{
	MP4,
	AVI,
};

//ͼƬ�洢��ʽ
enum PictureFormat
{
	JPEG,
	PNG,
	BMP,
};

//������״̬
enum PlayStatus
{
	Close,
	Playing,
	Pause
};

//�˾�����
enum FilterType
{
	Time,
	Reticle,
	Text,
	Other
};

//��ɫ
struct RGBAColor
{
	BYTE R;
	BYTE G;
	BYTE B;
	BYTE A;
};

//OSD�������Խṹ��
 struct OSDParameter
{
	char* text;
	int fontSize;        //�����ֺ�
	int fontFamily;      //0=���壬1=���壬2=����
	RGBAColor fontColor;      //������ɫ rgb��ʾ
	RGBAColor backColor;      //���屳��ɫ rgb��ʾ
	float x;             //�������� x����Ȱٷֱ�0-1
	float y;             //�������� y���߶Ȱٷֱ�0-1
	bool visible;        //�Ƿ�ɼ�
};

 //��ʽ
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
	int width;  					   	//�������λ���ء��������Ƶ������Ϊ0��
	int height; 					  	//����ߡ��������Ƶ������Ϊ0��
	int stamp;  					  	//ʱ����Ϣ����λ���롣
	FrameFormat format;                 //�������ͣ�0��AUDIO16��1��RGB32��2����YUV420P��YV12��3��YUVJ420P 
	int frameRate; 					    //ʵʱ֡�ʡ�
};

/**
* ��������������Ƶ
* @param nPort������ͨ����[0,31] 
* @param url����Ƶ�ļ�����Ƶ����ַ
* @param data��ͼ��ؼ��������̨¼��ʱ����ֵNULL
*
* @return �ɹ�/ʧ��
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_OpenVideo(int nPort, const char* url, const void* hWnd);

/**
*  ���������� ��ʼ����
* @param nPort������ͨ����[0,31] 
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_StartPlay(int nPort);

/**
*  ���������� ��ͣ����
* @param nPort������ͨ����[0,31] 
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_PausePlay(int nPort);


/**
* �����������ر���Ƶ�ͷ���Դ
* @param nPort������ͨ����[0,31] 
*/
extern "C" __declspec(dllexport)  bool _stdcall VideoPlayer_CloseVideo(int nPort);

/**
* ���������������ļ�����ָ������λ�ã��ٷֱȣ�
* @param nPort������ͨ����[0,31] 
* @param pos������ʱ��λ��  0-100
* @param fReleatePos��0-100
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_SetPlayPos(int nPort, float fReleatePos);

/**
* ���������������ٶȿ���
* @param nPort������ͨ����[0,31] 
* @param speed���ٶ�  0����������   1��1.25����  2��1.5���� 3��1.75���� 4:2����  5��0.75����  6��0.5���� 7��0.25����
*/
extern "C" __declspec(dllexport) void _stdcall VideoPlayer_ChangeSpeed(int nPort, int speed);

/**
* ������������ȡ��ǰ����ʱ��
* @param nPort������ͨ����[0,31] 
* @return ����ʱ��  ��λ����
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_GetPlayedTime(int nPort);

/**
* �������������Ŵ�������
* @param nPort������ͨ����[0,31] 
* @return �ɹ�/ʧ��
*/
extern "C" __declspec(dllexport)  bool _stdcall VideoPlayer_ResizeWindow(int nPort);

/**
* ��������������¼�ƴ洢����
* @param nPort��
* @param path��¼�񱣴�·�������ļ���
* @param fps��֡��,����������Ƶ����ͬ֡��
* @param quality��: ͼ��������0-5�� Խ������Խ�ã�ռ���ڴ�Խ������Ϊ0ʱ��Ϊ�Զ�����
* @param duration�������ļ�ʱ��(min),Ĭ��60���ӣ�ʱ�䵽ʱ�����ļ�������ϡ�_X����ʶ
* @param format:�洢��ʽ,Ĭ��Ϊmp4
*
* @return �ɹ�/ʧ��
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_StartRecord(int nPort, const char* path, int fps, float quality=5, int duration = 60, VideoFormat format = VideoFormat::MP4);

/**
* ��������������¼��
* @param nPort������ͨ����[0,31] 
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_StopRecord(int nPort);

/**
* ������������Ƶ��ͼ
* @param nPort������ͨ����[0,31]
* @param path�� ͼ�񱣴��·����������׺��
* @param format��ͼ���ʽ��Ĭ��Ϊjpeg
*
* @return �ɹ�/ʧ��
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_ShotImage(int nPort, const char* path, PictureFormat format= PictureFormat::JPEG);


/**
* ��������������ĳһ�е�OSD�����ı�
* @param nPort������ͨ����[0,31] 
* @param osdID��OSD�ı����
* @param para��osd����
*
* @return �ɹ�/ʧ��
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_SetOSDText(int nPort, int osdID, OSDParameter osdParam);

/**
* �����������������е�OSD�ı����ܹ�20�У�
* @param nPort������ͨ����[0,31]
* @param para��osd����
*
* @return �ɹ�/ʧ��
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_SetAllOSDTexts(int nPort, OSDParameter osdParams[]);

/**
* �����������������OSD�ı�
* @param nPort������ͨ����[0,31] 
*
* @return �ɹ�/ʧ��
*/
extern "C" __declspec(dllexport) bool _stdcall VideoPlayer_ClearOSDText(int nPort);


/**
* �������������ý��뻺����д�С
* @param nPort������ͨ����[0,31]
* @param size����С��Χ��1-20����Ĭ��Ϊ5�����ù�С�ᶪ֡�����ù�����ܻᵼ�»����ӳ�
*
* @return �ɹ�/ʧ��
*/
extern "C" __declspec(dllexport) void _stdcall VideoPlayer_SetDecodedBufferSize(int nPort, int size);

/**
* ������������֡���󲥷� 
* @param nPort������ͨ����[0,31]
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_NextFrame(int nPort);

/**
* ������������֡��ǰ���� 
* @param nPort������ͨ����[0,31]
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_PrevFrame(int nPort);

/**
* ������������ת��Ƶ��ĳһʱ��λ�� 
* @param nPort������ͨ����[0,31]
* @param pos����Ƶλ�ðٷֱ�
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_SetPlayPos(int nPort,float pos);

/**
* ������������ʾ/���ص�ǰϵͳʱ��
* @param nPort������ͨ����[0,31]
* @param visible���Ƿ���ʾ
* @param xPos������λ��X����Ȱٷֱ�
* @param yPos������λ��Y���߶Ȱٷֱ�
* @param fontSize�������С��Ĭ��50
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_SetDateTime(int nPort,bool visible, float xPos, float yPos,int fontSize=50);
 
 
/**
* ������������ǰ����������ݻص�
* @param nPort������ͨ����[0,31]
* @param pBuf��yuv����ָ��
* @param nSize�����ݴ�С
* @param pFrameInfo��֡��Ϣ
*/
typedef void(_stdcall* DecodeCallBack)(int nPort, const unsigned char* pBuf, int nSize, FrameInfo& pFrameInfo);

/**
* �������������ý���ص�����
* @param nPort������ͨ����[0,31]
* @param pDecodeProc���ص�����
*/
extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_SetDecCallBack(int nPort, DecodeCallBack pDecodeProc);

 

/**
* ��������������ʾ��������л����ı���ͼ�ε�
* @param nPort������ͨ����[0,31]
* @param hWnd����Ƶ���ھ�������ڴ���GDI
* @param width����Ƶ���ڿ��
* @param height����Ƶ���ڸ߶�
*/
typedef void(_stdcall* DrawCallBack)(int nPort, const void* hWnd,int width,int height);

extern "C" __declspec(dllexport) bool  _stdcall VideoPlayer_SetDrawCallBack(int nPort, DrawCallBack pDrawProc);

 
