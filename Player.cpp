#include "stdafx.h"
#include "Player.h"
#include <future>
#include <fstream>
#include <direct.h>
#include <io.h>
#include "ShotImage.h"
 

Player::Player()
{
	xffmpeg = new XFFMpeg();
	sdlDisplay = new SDLDisplay();
	playStatus = PlayStatus::Close;
	isInited = false;//��ʼ��׼�����
	width = 0;
	height = 0;
	realFPS = 0;
	fps = 0;
	totalTime = 0;
	bitrate = 0;
	frameCnt = 0;
    encodeFormat = "";
	playHandle = "";
	bShotting = false;
	bRecord = false;
	creatFileFlag = false;
	finishedFileFlag = false;
	yuv_buffer = nullptr;
    video_decode_size = 0;
	decodedBufferSize = 10;
	strOSD = "";
	filterFlag = 0;
	playSpeed = 1;

	for (int i = 0; i < FILTER_PARA_SIZE; i++)
	{
		 filterParams[i] = new FilterParam;
		 filterParams[i]->visible = false;
	}
}

string Player::  EnumToString(AVCodecID ci)
{
	switch (ci)
	{
	case AVCodecID::AV_CODEC_ID_MJPEG:return "MJPEG";
	case AVCodecID::AV_CODEC_ID_H263:return "H263";
	case AVCodecID::AV_CODEC_ID_MPEG4:return "MPEG4";
	case AVCodecID::AV_CODEC_ID_H264:return "H264";
	case AVCodecID::AV_CODEC_ID_H265:return "H265";
	default:break;
	}
	return "";
}

Player::~Player()
{
	 StopPlay();
}

//����Ƶ
bool Player::OpenVideo(const char* url, const void* hWnd)
{
	xffmpeg->InitRegister();
	if (isInited)
		return false;
	//��1���������ļ�
	int ret = xffmpeg->OpenInput(url);
	if (ret < 0)
		return false;

	//��2���򿪽�����
	ret = xffmpeg->InitDecoderCodec();
	if (ret < 0)
		return false;

	//��Ƶ������
	if (xffmpeg->audioIndex >= 0)
	{
		ret = xffmpeg->InitAduioDecoderCodec();
		if (ret < 0)
			return false;
	}

	//��Ƶ��Ϣ	
	width = xffmpeg->srcWidth;
	height = xffmpeg->srcHeight;
	fps = (int)xffmpeg->fps;
	totalTime = xffmpeg->totalTime;
	bitrate = xffmpeg->bitrate;
	encodeFormat = EnumToString(xffmpeg->videCodecID);

	//��3��׼����ʽת����
	if (width != 0 && height != 0)
	{
		width = xffmpeg->srcWidth;
		height = xffmpeg->srcHeight;
	}
	ret = xffmpeg->InitSwsContext(xffmpeg->decoderCtx, AVPixelFormat::AV_PIX_FMT_YUV420P, width, height);
	if (ret < 0)
		return false;

	//��4����ʼ����
	StartPlay(hWnd, width, height);
	if (ret >= 0)
		isInited = true;

}

//��ʼ����
void Player::StartPlay(const void* hwd,int width,int height)
{
	playStatus = PlayStatus::Pause;
	getDataThread = new thread(&Player::GetDataThread, this);
	decodeVideoThread = new thread(&Player::DecodeVideoThread, this);
	frameProcessingThread = new thread(&Player::FrameProcessingThread, this);

	//��ʱ�ص���Ƶ����Ƶδ�������
	/*if (xffmpeg->audioIndex >= 0)
		decodeAudiohread=new thread(&Player::DecodeAudioThread, this);*/
	playHandle = hwd;
	// ��ʼ��SDL
	if (hwd != NULL)
	{
	     sdlDisplay->CreateSDLWindow(width, height, hwd);		 
	}
}

//ֹͣ����
void Player::StopPlay()
{
	playStatus = PlayStatus::Close;
	if (recordThread)
	{
		recordThread->join();
		delete recordThread;
		recordThread = nullptr;
	}	
	if (getDataThread)
	{
		getDataThread->join();
		delete getDataThread;
		getDataThread = nullptr;
	}

	if (decodeVideoThread)
	{
		decodeVideoThread->join();
		delete decodeVideoThread;
		decodeVideoThread = nullptr;
	}
	
	if (frameProcessingThread)
	{
		frameProcessingThread->join();
		delete frameProcessingThread;
		frameProcessingThread = nullptr;
	}
	xffmpeg->Close();
	ClearBuffer();
	 
	isInited = false;//��ʼ��׼�����
	width = 0;
	height = 0;
	fps = 0;
	realFPS = 0;
	totalTime = 0;
	bitrate = 0;
	frameCnt = 0;
	encodeFormat = "";
	//playHandle = "";
	bShotting = false;
	bRecord = false;
	creatFileFlag = false;
	finishedFileFlag = false;
	yuv_buffer = nullptr;
	video_decode_size = 0;

}

//��ʼ¼��
void Player::StartRecord()
{
	bRecord = true;
	recordThread = new thread(&Player::RecordThread, this);
}


//ֹͣ¼��
void Player::StopRecord()
{
	bRecord = false;
	recordThread->join();
	delete recordThread;
	recordThread = nullptr;
}

//���buffer
void Player::ClearBuffer()
{
	xffmpeg->ClearBuffer();
	//��ջ�ȡ��buffer
	for each (AVPacket * pkt in packetBuffer)
		av_packet_unref(pkt);
	packetBuffer.clear();

	//��ձ���Buffer
	for each (AVFrame * frame in recordBuffer)
		av_frame_free(&frame);
	recordBuffer.clear();

	//��ս���buffer
	for each (AVFrame * frame in decodedBuffer)
		av_frame_free(&frame);
	decodedBuffer.clear();
}
 
//�Ͽ����磬��������
void Player::ReopenStream()
{
	int ret = -1;
	while (ret<0)
	{
		xffmpeg->CloseInput();
		ret = xffmpeg->OpenInput(xffmpeg->videoUrl.c_str());
		if (ret >= 0)
		{
			//�򿪽�����
		  	ret = xffmpeg->InitDecoderCodec();
			 if(ret>=0)
					break;
		}
	}
 }


//��Ƶ��/�ļ���ȡ�߳�
void Player::GetDataThread()
{
	long second_tick = 0;  //1��ʱ��
	int second_fps = 0;  //1���ӵ�֡��
	while (playStatus != PlayStatus::Close)
	{
		int ret = 0;
		long t_start = SDL_GetTicks();
		bool first_I_Frame = false; //��һ��I֡
		int failNum = 0; //����ʧ�ܴ���
		while (playStatus != PlayStatus::Close)
		{
			//���ű����ļ�ʱ��buffer����ֹͣread
			if (!xffmpeg->isNetUrl && packetBuffer.size() >= 60)
			{
				Sleep(10);
				continue;
			}
			//long t0 = SDL_GetTicks();

			AVPacket* pkt = av_packet_alloc();
			//��1����ȡ��Ƶ֡  UDP�鲥���벻ͣ�ĵ���av_read_frame
			//std::printf("��ȡ��Ƶ֡...\n");
			ret = xffmpeg->ReadPacket(pkt);   //��ȡ������ʱ������������buffer�������� ����˲���http����
			if (ret != 0)
			{
				failNum++;
				av_free_packet(pkt);
				if (failNum > 10000)
				{
					failNum = 0;
					//playStatus = PlayStatus::Close;
				}
				if (ret == AVERROR_EOF)
				{
					playStatus = PlayStatus::Pause;
					//�����̹߳ر���Ƶ
					 ReopenStream();
					 playStatus = PlayStatus::Playing;
					continue;
				}
				continue;
			}
			else
			{				
				failNum = 0;
				if (xffmpeg->isNetUrl)
				{
					//��ȡ��Ƶ���ĵ�һ��I֡��֮ǰ����������,��Ϊû��I֡�޷�����ɹ�
					if (pkt->flags & AV_PKT_FLAG_KEY)
					{
						first_I_Frame = true;
					}
					else
					{
						if (!first_I_Frame)
						{
							//�ڽ���I֮֡ǰ��ս��������棬Ϊ�˽���UDP�����ȵ���IDE����֡������
							//avcodec_flush_buffers(xffmpeg-> decoderCtx);
							av_free_packet(pkt);
							continue;
						}
					}
				}
			}

			//��Ƶ��
			if (pkt->stream_index == xffmpeg->videoIndex)
			{
				//����buffer
				int n = 60;
				if (xffmpeg->fps > 0)
					n = xffmpeg->fps;
				if (packetBuffer.size() <= n*2) //����2s��Ƶ
				{
					packetDataMutex.lock();
					packetBuffer.push_back(pkt);
					packetDataMutex.unlock();
					//printf("packet buffer size:%d\n", packetBuffer.size());
				}
				else	//������������£�����������Ķ��� ������buffer
				{
					av_free_packet(pkt);
				}


				//����ʵʱ֡�ʣ�1���ӵ�֡��	 
				second_fps++;
				if (second_tick == 0)
				{
					second_tick = SDL_GetTicks();
				}
				else
				{
					long x = SDL_GetTicks() - second_tick;
					if (x >= 1000)//1s����
					{
						second_tick = SDL_GetTicks();
						 realFPS = second_fps;
						//printf("avgFPS:%d\n", second_fps);
						second_fps = 0;
					}
				}
			}
			//��Ƶ��
			else if (pkt->stream_index == xffmpeg->audioIndex)
			{
				//����buffer
				if (audioPacketBuffer.size() <= 60)
				{
					audioPacketMutex.lock();
					audioPacketBuffer.push_back(pkt);
					audioPacketMutex.unlock();
				}
				else	//������������£�����������Ķ��� ������buffer
				{
					av_free_packet(pkt);
				}
			}

			//�������а�ʱ��ʱ������̫��ᵼ�½����޷��������
			if (packetBuffer.size() > 1)
				Sleep(1);
		}	
	}
}

//�ص�������ɵ����� 
//��ΪYUVJ420P��ʽ������YUVJ420P���
//��������YUV420P��YV12�����
int yuvDataCallBack(Player* player, AVFrame* frame)
{
	// ��ʼ����� YUV ����ͼƬ�� buffer �ڴ�ռ�
	if (player->yuv_buffer == nullptr)
	{
		player->video_decode_size = avpicture_get_size((AVPixelFormat)frame->format, frame->width, frame->height);
		player->yuv_buffer = (unsigned char*)malloc(player->video_decode_size);
	}

	// YUV420p��������ɫ��Χ��[16,235]��16��ʾ��ɫ��235��ʾ��ɫ
	// YV12 y1y2y3��ys v1v2v3��vn u1u2u3��un
	if (frame->format == AVPixelFormat::AV_PIX_FMT_YUV420P)
	{	
		// Y
		for (int i = 0; i < frame->height; i++)
		{
			memcpy(player->yuv_buffer + frame->width * i,
				frame->data[0] + frame->linesize[0] * i,
				frame->width);
		}
		// V
		for (int j = 0; j < frame->height / 2; j++)
		{
			memcpy(player->yuv_buffer + frame->width * frame->height + frame->width / 2 * (frame->height / 2) + frame->width / 2 * j,
				frame->data[1] + frame->linesize[1] * j,
				frame->width / 2);
		}
		// U
		for (int k = 0; k < frame->height / 2; k++)
		{
			memcpy(player->yuv_buffer + frame->width * frame->height + frame->width / 2 * k,
				frame->data[2] + frame->linesize[2] * k,
				frame->width / 2);
		}
	}
	FrameInfo frameInfo;
	frameInfo.frameRate = player->fps;
	frameInfo.height = frame->height;
	frameInfo.width = frame->width;
	frameInfo.format = FrameFormat::YV12;
	frameInfo.stamp = frame->pts;
 
	player->decodeCallBack(player->Port, player->yuv_buffer, player->video_decode_size, frameInfo);
	//avpicture_free((AVPicture*)frame);
	av_frame_free(&frame);
	return 0;
}

//��Ƶ�����߳�
void Player::DecodeVideoThread()
{
	int flag1 = 0;
	int flag2 = 0;
	int ret = 0;
	int failNum = 0;
	long t_start = SDL_GetTicks();
	while (playStatus != PlayStatus::Close)
	{
		if (playStatus == PlayStatus::Pause)
		{
			Sleep(10);
			continue;
		}
		if (packetBuffer.size() == 0)
		{
			Sleep(1);
			continue;
		}
		//�����ļ�����ʱ��buffer���˾���ͣ���룬����������
		if (!xffmpeg->isNetUrl && decodedBuffer.size() >= decodedBufferSize)
		{
			Sleep(1);
			continue;
		}

		//ȡ���ݰ�
		AVPacket* pkt ;
		packetDataMutex.lock();
		pkt = packetBuffer.front();
		packetBuffer.pop_front();
		packetDataMutex.unlock();
 
		//����ý��ʱ��buffer����ֱ�Ӷ���δ��������
		if (xffmpeg->isNetUrl && decodedBuffer.size() >= decodedBufferSize)
		{
			//printf("decode buffer full��throw a frame��");
			av_free_packet(pkt);
			continue;
		}

		//int t0 = SDL_GetTicks();
		
		//��ʼ����
		AVFrame* srcFrame = av_frame_alloc(); //������frame	
		ret = xffmpeg->DecodeVideo(pkt, srcFrame);
		if (srcFrame->width == 0)
		{
			//std::printf("decode fail��\n");
			av_frame_free(&srcFrame);
			failNum++;
		}
		else
		{
			failNum = 0;			
			//���������ɵ�����
			if (srcFrame->data != NULL)
			{
				decodeDataMutex.lock();
				decodedBuffer.push_back(srcFrame);
				decodeDataMutex.unlock();
			}
			else
			{
				av_frame_free(&srcFrame);
			}
		}
		if (failNum > 3000)
		{
		//	playStatus = PlayStatus::Close;
			failNum = 0;
		}
	}
}

//��ʼ��OSD
int Player::InitOSD()
{
	try
	{
		int ret = 0;
		bool isNeedFiltered = CombineOSDCommand(strOSD); //�ϲ��˾��ַ�,�����Ƿ���Ҫ�˾�
		if (!isNeedFiltered)
		{
			filterFlag = 0;
			return 0;
		}
		if (odlDescr != strOSD)  //�ַ��������  
		{
			//filterFlagΪ0ʱ��Ϊ�״�ʹ���˾���Ĭ���л�Ϊ��1���˾�
			//��ǰ����ʹ�õ�1���˾�ʱ����ʼ����2���˾���׼��
			//��ǰ����ʹ�õ�2���˾�ʱ����ʼ����1���˾���׼��
			if (filterFlag == 0 || filterFlag == 2)
			{
				filterMutex.lock();
				xffmpeg->CloseFilter();
				ret = xffmpeg->InitFilters(strOSD.data());
				filterFlag = 1;
				filterMutex.unlock();
			}
			else
			{
				filterMutex2.lock();
				xffmpeg->CloseFilter2();
				ret = xffmpeg->InitFilters2(strOSD.data());
				filterFlag = 2;
				filterMutex2.unlock();
			}
			odlDescr = strOSD;
			if (ret < 0)
			{
				return -1;
			}
			return 0;
		}
		return 0;
	}
	catch(exception ex)
	{
		return -1;
	}
	
}

//�ж��Ƿ���ʾ�ַ���descrΪDrawTextָ��
bool Player::CombineOSDCommand(string& descr)
{
	descr = "";
	int ret = 0;
	int num = 0;
	if (dateTimeFilterDescr != "")
	{
		descr += dateTimeFilterDescr;
		num++;
	}
	for (int i = 0; i < FILTER_PARA_SIZE; i++)
	{
		if (!filterParams[i]->visible || filterParams[i]->descr == string()|| filterParams[i]->descr=="")
			continue;
 
		int ch1 =  97 + num;
		char str[10] = {0};
		sprintf(str, "[%c];[%c]", ch1, ch1);
		if(num>=1)
			descr += (string)str;

		descr += filterParams[i]->descr;
		num++;
		
	}
 
	//û�������˾�Ч��
	if (num == 0)
	{
		return false;
	}
	if (num >1)
	{
	    descr = "[in]" + descr + "[out]";
	}
	return true;
}

//�˾����� ���YUV420P��ʽ
//srcFrame:
//dstFrame:
//filterNum:�˾�ѡ�� 1��2
int  Player::Filtering(AVFrame* srcFrame, AVFrame* dstFrame, int filterNum)
{
	int ret = 0;
	/*һ���˾����а���һϵ��˳�����ӵ��˾����˾�֮��ͨ�����ŷָ�����
	һ���˾�ͼ�а���һϵ���˾������˾���֮��ͨ���ֺŸ�����
	�˾�������ʽ��[in_link_1]...[in_link_N]filter_name = arguments[out_link_1]...[out_link_M]*/
	//	sprintf_s(filters_descr, "drawtext=fontfile=FreeSerif.ttf:fontsize=60:fontcolor=blue:text='%s':x=10:y=10", descr.data());	
	if (filterNum == 1)
	{
		filterMutex.lock();
		ret = xffmpeg->FilterFrame(srcFrame, dstFrame);
		filterMutex.unlock();
	}
	else if (filterNum == 2)
	{
		filterMutex2.lock();
		ret = xffmpeg->FilterFrame2(srcFrame, dstFrame);
		filterMutex2.unlock();
	}
	else 
	{
		ret = -1;
	}
	 
	if (ret < 0)
	{
		av_frame_free(&srcFrame);
		return -1;
	}
	av_frame_free(&srcFrame);
	return ret;
}

bool shotSucessful = false;  //��ͼ�ɹ���־
//�첽��ʾ
bool UpdateSDLWindowRGB(void* param,AVFrame* frame,int width,int height)
{	
	Player* player =(Player*) param;
	int ret= player->sdlDisplay->UpdateSDLWindowRGB(frame, width, height);
	ShotImage shotImage;
	if (player->bShotting)
	{
		shotSucessful = false;
		shotSucessful = shotImage.ConvertImage(frame, player->shotImagePath.c_str());
		player->bShotting = false;
	}  
	avpicture_free((AVPicture*)frame);
	av_frame_free(&frame);

	//�ص�
	if (player->drawCallBack != NULL)
	{
		RECT rect;
		GetWindowRect((HWND)player->sdlDisplay->hwndTemp, &rect);
		player->drawCallBack(player->Port, player->sdlDisplay->hwndTemp, rect.right - rect.left, rect.bottom - rect.top);
	}
	return ret ==0 ;
}


//YUV��ʽͼ����ʾ
bool UpdateSDLWindowYUV(void* param, AVFrame* frame, int width, int height,bool mustFreePicture)
{

	Player* player = (Player*)param;
	int ret = player->sdlDisplay->UpdateSDLWindowYUV(frame, width, height);
	ShotImage shotImage;
	if (player->bShotting)
	{
		shotSucessful = false;
		shotSucessful = shotImage.ConvertImage(frame, player->shotImagePath.c_str());
		player->bShotting = false;
	}
	if(mustFreePicture)
		avpicture_free((AVPicture*)frame);
	av_frame_free(&frame);

	//�ص�
	if (player->drawCallBack != NULL)
	{
	    RECT rect;
		GetWindowRect((HWND)player->sdlDisplay->hwndTemp, &rect);
		player->drawCallBack(player->Port, player->sdlDisplay->hwndTemp, rect.right - rect.left, rect.bottom - rect.top);
	}
	return ret == 0;
}


//��ͼ
bool Player::CatchImage(string path)
{
	shotSucessful = false;
	shotImagePath = path;
	bShotting = true;
	SDL_Delay(100);
	return shotSucessful;
}

//�첽��ͼ
//bool CatchImage(AVFrame* frame, string shotImagePath)
//{
//	ShotImage shotImage;
//	return shotImage.ConvertImage(frame, shotImagePath.c_str());
//}

//ͼ�����߳�
void Player::FrameProcessingThread()
{
	int ret;
	int tempW = xffmpeg->srcWidth;
	int tempH = xffmpeg->srcHeight;
	Uint32 showStart = 0;
	bool delayFlag = false;

	//�ж��Ƿ������������˾�
	if (!strOSD.empty())
	{
		InitOSD();
		if (width != 0 && height != 0)
		{
			tempW = width;
			tempH = height;
		}
	}

	try
	{
		while (playStatus != PlayStatus::Close)
		{
			if (decodedBuffer.size() == 0)
			{
				Sleep(1);
				continue;
			}
			int startTime = SDL_GetTicks();

			//��1��ȡFrame
			AVFrame* srcFrame = NULL;
			decodeDataMutex.lock();
			if (decodedBuffer.size() > 0)
				srcFrame = decodedBuffer.front();
			decodedBuffer.pop_front();
			decodeDataMutex.unlock();
			
			//������ʱ����������ͣ״̬����Ҫȡ���ѽ�������ݲ�������������Ƶʱ������������
			if (xffmpeg->isNetUrl && playStatus == PlayStatus::Pause)
			{
				av_frame_free(&srcFrame);
				continue;
			}

			int srcW = srcFrame->width;
			int srcH = srcFrame->height;

			AVFrame* resultFrame = srcFrame;  //������ʾ��ͼ��	

			//��2������OSD�����ı�YUV��ʽ	 
			if (!strOSD.empty()&&filterFlag!=0)
			{
				AVFrame* filterFrame = av_frame_alloc(); //�˾�������frame 	
				ret = Filtering(srcFrame, filterFrame, filterFlag);
				if (ret < 0)
				{
					av_frame_free(&filterFrame);
					continue;
				}
				resultFrame = filterFrame;
			}

			//��3��¼�񱣴�
			if (bRecord)
			{
				if (recordBuffer.size() < 60)
				{
					AVFrame* recordFrame = av_frame_alloc();
					recordFrame = av_frame_clone(resultFrame);
					recordMutex.lock();
					recordBuffer.push_back(recordFrame);
					recordMutex.unlock();
				}
				else
					std::printf("record buffer full��throw a frame��\n");
			}

			//��4��ת����ʽΪAV_PIX_FMT_YUV420P����ʾ
			//�˾������Ѿ�ת��ΪAV_PIX_FMT_YUV420P������Ҫ��ת��
			bool mustFreePicture = false;
			if (resultFrame->format!= AV_PIX_FMT_YUV420P)
			{
				AVFrame* dstFrame = av_frame_alloc();
				ret = xffmpeg->FormatConvert(resultFrame, dstFrame);
				if (ret <= 0)
				{
					avpicture_free((AVPicture*)dstFrame);
					av_frame_free(&dstFrame);
					continue;
				}
				resultFrame = dstFrame;
				mustFreePicture = true;
			}
 
			//������Ƶ�����ٶȿ��ƣ���ʱ
			if (!xffmpeg->isNetUrl)
			{
				int endTime = SDL_GetTicks();
				int delay = 0;
				 
				 delay =(int)(1000 * playSpeed / xffmpeg->fps -(endTime - startTime));
				if (delay>0 )
				{
					Sleep(delay);
				}
				startTime = endTime;
			}

			 nowPTS = resultFrame->pts;

			//��5�����������첽�ص�
			if (decodeCallBack != NULL)
			{
				AVFrame* frame = av_frame_alloc();
				frame = av_frame_clone(resultFrame);
				std::future<int> fe = std::async(std::launch::async, yuvDataCallBack, this, frame);
			}

			//��6����ʾ
			if (sdlDisplay->hwndTemp != NULL)
			{
	/*			RECT rect;
				GetWindowRect((HWND)sdlDisplay->hwndTemp, &rect);
				if (drawCallBack != NULL)
					drawCallBack(Port, sdlDisplay->hwndTemp, rect.right- rect.left, rect.bottom- rect.top);*/

				//std::future<bool> fe = std::async(std::launch::async, UpdateSDLWindowRGB, this, bgrFrame, width, height);
					
				std::future<bool> fe = std::async(std::launch::async, UpdateSDLWindowYUV, this, resultFrame, width, height, mustFreePicture);
			}
			else
			{
				if(mustFreePicture)
					avpicture_free((AVPicture*)resultFrame);
				av_frame_free(&resultFrame);
			}
		}
	}
	catch (exception ex)
	{
		throw ex.what();
	}
}
 

//���뱣����Ƶ
void Player::RecordThread()
{
	int ret = 0;
	fileTime = 0;
	last_dts = 0;
	last_pts = 0;
	startTime = 0;
	int fileNum = 0;
	string start;
	xffmpeg->m_NextFrameIsKey = true;
	try
	{
		while (bRecord)
		{
			if (recordBuffer.size() == 0)
			{
				Sleep(2);
				continue;
			}
			//¼�񱣴�

			 //int t0 = SDL_GetTicks();
			//��0������			
			AVFrame* frame;
			recordMutex.lock();
			frame = recordBuffer.front();
			recordBuffer.pop_front();
			recordMutex.unlock();

			int t1 = SDL_GetTicks();
			AVPacket* pkt = av_packet_alloc();
			ret = xffmpeg->Encode(frame, pkt);
			if (ret < 0)//����ʧ��
			{
				av_free_packet(pkt);
				continue;
			}

			int t2 = SDL_GetTicks();
			//std::printf("encode used time:%d\n", t2 - t1);

			//��1�������µ���Ƶ�ļ�
			if (fileTime == 0)  //����ļ�¼�����һ��
			{
				xffmpeg->m_NextFrameIsKey = true;
				creatFileFlag = true;
				if (fileNum > 0)
					recordPath = recordVideoDir + "-" + to_string(fileNum) + recordVideoFmt;
				ret = xffmpeg->OpenOutput(recordPath);    // �����µ���Ƶ�ļ�
			}

			//��2����¼��ʼд���ļ���ʱ��
			if (startTime == 0)
			{
				startTime = SDL_GetTicks();
			}
			//��3������Ƶ�ļ���д������	
			ret = xffmpeg->WritePacket(pkt);
			av_free_packet(pkt);
			//int t3 = SDL_GetTicks();


			//��4������¼��ʱ��
			fileTime = SDL_GetTicks() - startTime;
			//std::printf("record time length:%d\n", fileTime);
			//��5������¼��ʱ�������趨ֵʱ
			if (fileTime >= maxRecordTimeLength * 60 * 1000)
			{
				fileNum++;
				ret = xffmpeg->FinishWrite();
				finishedFileFlag = true;
				fileTime = 0;
				last_dts = 0;
				last_pts = 0;
				startTime = 0;
				//std::printf("finished record one file!time length:%d\n", fileTime);
			}

		}
		//����¼��ʱ
		ret = xffmpeg->FinishWrite();
		finishedFileFlag = true;
		/*if (ret == 0)
		{
			if (callRecordFinished != NULL && playStatus != DataType::close)
				callRecordFinished(true, recordPath.c_str(), 0);
		}
		else if (ret != 0)
		{
			if (callRecordFinished != NULL && playStatus != DataType::close)
				callRecordFinished(false, "", 0);
		}*/

		xffmpeg->CloseOutput();
		fileTime = 0;
		last_dts = 0;
		last_pts = 0;
		startTime = 0;
		//std::printf("stop record!\n");

		//��ձ���Buffer
		for each (AVFrame * frame in recordBuffer)
			av_frame_free(&frame);
		recordBuffer.clear();
	}
	catch (exception ex)
	{
		string err = "recording error!" + (string)(ex.what());
		throw err;
	}
}

//��Ƶ�����߳�
void Player::DecodeAudioThread()
{
	int ret = 0;
	int failNum = 0;
	long t_start = SDL_GetTicks();

	//����Ƶ�豸
	AVCodecContext* acc = xffmpeg->audioDecoderCtx;
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO; //��Ƶ�����ʽ����
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;//  AUDIO_S16SYS
	int out_framesize = 1024;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	int out_sample_rate = acc->sample_rate;//���������
	sdlDisplay->InitAudio(out_framesize, out_sample_rate, out_channels);
	
	//��Ƶ��ʽת����
	SwrContext *au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,
		out_channel_layout,//���
		out_sample_fmt, //����ǰ��ϣ���ĸ�ʽ
		out_sample_rate,//���
		av_get_default_channel_layout(acc->channels), //in_channel_layout, //����
		acc->sample_fmt,//PCMԴ�ļ��Ĳ�����ʽ
		acc->sample_rate, //����
		0, NULL);
	swr_init(au_convert_ctx);

	uint8_t** audio_data_buffer = NULL;//�洢ת��������ݣ��ٱ���AAC  
	//�������������ڴ�ռ�
	av_samples_alloc_array_and_samples(&audio_data_buffer,
		NULL, out_channels,
		192000 * 2,//aframe->nb_samples,
		out_sample_fmt, 1);

	while (playStatus != PlayStatus::Close)
	{
		if (playStatus == PlayStatus::Pause)
		{
			Sleep(1);
			continue;
		}
		if (audioPacketBuffer.size() == 0)
		{
			Sleep(1);
			continue;
		}

		//ȡ���ݰ�
		AVPacket* pkt;
		audioPacketMutex.lock();
		pkt = audioPacketBuffer.front();
		audioPacketBuffer.pop_front();
		audioPacketMutex.unlock();

		//��ʼ����
		vector<AVFrame*> frames; //������frame	
		ret = xffmpeg->DecodeAudio(pkt, frames);
		av_free_packet(pkt);

		//��Ƶ����
		vector<AVFrame*>::iterator i;  //�������������
		for (i = frames.begin(); i != frames.end(); ++i)
		{
			AVFrame* frame = *i;
			int audio_buffersize = frame->linesize[0];

			////ת��
			int convert_size = swr_convert(au_convert_ctx,
				audio_data_buffer,
				192000,
				(const uint8_t**)frame->data,
				frame->nb_samples);
			sdlDisplay->PlayAudio(audio_buffersize, audio_data_buffer);
			av_frame_free(&frame);
		}
		frames.clear();
	}

	av_freep(audio_data_buffer);
	audio_data_buffer = NULL;
	swr_free(&au_convert_ctx);
	au_convert_ctx = NULL;
}