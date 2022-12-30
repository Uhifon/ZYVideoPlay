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
	isInited = false;//初始化准备完成
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

//打开视频
bool Player::OpenVideo(const char* url, const void* hWnd)
{
	xffmpeg->InitRegister();
	if (isInited)
		return false;
	//【1】打开输入文件
	int ret = xffmpeg->OpenInput(url);
	if (ret < 0)
		return false;

	//【2】打开解码器
	ret = xffmpeg->InitDecoderCodec();
	if (ret < 0)
		return false;

	//音频解码器
	if (xffmpeg->audioIndex >= 0)
	{
		ret = xffmpeg->InitAduioDecoderCodec();
		if (ret < 0)
			return false;
	}

	//视频信息	
	width = xffmpeg->srcWidth;
	height = xffmpeg->srcHeight;
	fps = (int)xffmpeg->fps;
	totalTime = xffmpeg->totalTime;
	bitrate = xffmpeg->bitrate;
	encodeFormat = EnumToString(xffmpeg->videCodecID);

	//【3】准备格式转码器
	if (width != 0 && height != 0)
	{
		width = xffmpeg->srcWidth;
		height = xffmpeg->srcHeight;
	}
	ret = xffmpeg->InitSwsContext(xffmpeg->decoderCtx, AVPixelFormat::AV_PIX_FMT_YUV420P, width, height);
	if (ret < 0)
		return false;

	//【4】开始播放
	StartPlay(hWnd, width, height);
	if (ret >= 0)
		isInited = true;

}

//开始播放
void Player::StartPlay(const void* hwd,int width,int height)
{
	playStatus = PlayStatus::Pause;
	getDataThread = new thread(&Player::GetDataThread, this);
	decodeVideoThread = new thread(&Player::DecodeVideoThread, this);
	frameProcessingThread = new thread(&Player::FrameProcessingThread, this);

	//暂时关掉音频，音频未开发完成
	/*if (xffmpeg->audioIndex >= 0)
		decodeAudiohread=new thread(&Player::DecodeAudioThread, this);*/
	playHandle = hwd;
	// 初始化SDL
	if (hwd != NULL)
	{
	     sdlDisplay->CreateSDLWindow(width, height, hwd);		 
	}
}

//停止播放
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
	 
	isInited = false;//初始化准备完成
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

//开始录制
void Player::StartRecord()
{
	bRecord = true;
	recordThread = new thread(&Player::RecordThread, this);
}


//停止录制
void Player::StopRecord()
{
	bRecord = false;
	recordThread->join();
	delete recordThread;
	recordThread = nullptr;
}

//清空buffer
void Player::ClearBuffer()
{
	xffmpeg->ClearBuffer();
	//清空获取包buffer
	for each (AVPacket * pkt in packetBuffer)
		av_packet_unref(pkt);
	packetBuffer.clear();

	//清空编码Buffer
	for each (AVFrame * frame in recordBuffer)
		av_frame_free(&frame);
	recordBuffer.clear();

	//清空解码buffer
	for each (AVFrame * frame in decodedBuffer)
		av_frame_free(&frame);
	decodedBuffer.clear();
}
 
//断开网络，反复打开流
void Player::ReopenStream()
{
	int ret = -1;
	while (ret<0)
	{
		xffmpeg->CloseInput();
		ret = xffmpeg->OpenInput(xffmpeg->videoUrl.c_str());
		if (ret >= 0)
		{
			//打开解码器
		  	ret = xffmpeg->InitDecoderCodec();
			 if(ret>=0)
					break;
		}
	}
 }


//视频流/文件获取线程
void Player::GetDataThread()
{
	long second_tick = 0;  //1秒时钟
	int second_fps = 0;  //1秒钟的帧数
	while (playStatus != PlayStatus::Close)
	{
		int ret = 0;
		long t_start = SDL_GetTicks();
		bool first_I_Frame = false; //第一个I帧
		int failNum = 0; //读包失败次数
		while (playStatus != PlayStatus::Close)
		{
			//播放本地文件时，buffer满了停止read
			if (!xffmpeg->isNetUrl && packetBuffer.size() >= 60)
			{
				Sleep(10);
				continue;
			}
			//long t0 = SDL_GetTicks();

			AVPacket* pkt = av_packet_alloc();
			//【1】获取视频帧  UDP组播必须不停的调用av_read_frame
			//std::printf("读取视频帧...\n");
			ret = xffmpeg->ReadPacket(pkt);   //获取不到包时会阻塞，导致buffer快速消耗 ，因此播放http等流
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
					//开启线程关闭视频
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
					//获取视频流的第一个I帧，之前的数据抛弃,因为没有I帧无法解码成功
					if (pkt->flags & AV_PKT_FLAG_KEY)
					{
						first_I_Frame = true;
					}
					else
					{
						if (!first_I_Frame)
						{
							//在解码I帧之前清空解码器缓存，为了降低UDP丢包等导致IDE缓冲帧数增加
							//avcodec_flush_buffers(xffmpeg-> decoderCtx);
							av_free_packet(pkt);
							continue;
						}
					}
				}
			}

			//视频包
			if (pkt->stream_index == xffmpeg->videoIndex)
			{
				//存入buffer
				int n = 60;
				if (xffmpeg->fps > 0)
					n = xffmpeg->fps;
				if (packetBuffer.size() <= n*2) //缓存2s视频
				{
					packetDataMutex.lock();
					packetBuffer.push_back(pkt);
					packetDataMutex.unlock();
					//printf("packet buffer size:%d\n", packetBuffer.size());
				}
				else	//网络流的情况下，来不及解码的丢弃 不存入buffer
				{
					av_free_packet(pkt);
				}


				//计算实时帧率，1秒钟的帧数	 
				second_fps++;
				if (second_tick == 0)
				{
					second_tick = SDL_GetTicks();
				}
				else
				{
					long x = SDL_GetTicks() - second_tick;
					if (x >= 1000)//1s到达
					{
						second_tick = SDL_GetTicks();
						 realFPS = second_fps;
						//printf("avgFPS:%d\n", second_fps);
						second_fps = 0;
					}
				}
			}
			//音频包
			else if (pkt->stream_index == xffmpeg->audioIndex)
			{
				//存入buffer
				if (audioPacketBuffer.size() <= 60)
				{
					audioPacketMutex.lock();
					audioPacketBuffer.push_back(pkt);
					audioPacketMutex.unlock();
				}
				else	//网络流的情况下，来不及解码的丢弃 不存入buffer
				{
					av_free_packet(pkt);
				}
			}

			//队列中有包时延时，读得太快会导致解码无法进入队列
			if (packetBuffer.size() > 1)
				Sleep(1);
		}	
	}
}

//回调解码完成的数据 
//若为YUVJ420P格式，按照YUVJ420P输出
//其他按照YUV420P（YV12）输出
int yuvDataCallBack(Player* player, AVFrame* frame)
{
	// 初始化存放 YUV 编码图片的 buffer 内存空间
	if (player->yuv_buffer == nullptr)
	{
		player->video_decode_size = avpicture_get_size((AVPixelFormat)frame->format, frame->width, frame->height);
		player->yuv_buffer = (unsigned char*)malloc(player->video_decode_size);
	}

	// YUV420p的像素颜色范围是[16,235]，16表示黑色，235表示白色
	// YV12 y1y2y3…ys v1v2v3…vn u1u2u3…un
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

//视频解码线程
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
		//本地文件播放时，buffer满了就暂停解码，不丢弃数据
		if (!xffmpeg->isNetUrl && decodedBuffer.size() >= decodedBufferSize)
		{
			Sleep(1);
			continue;
		}

		//取数据包
		AVPacket* pkt ;
		packetDataMutex.lock();
		pkt = packetBuffer.front();
		packetBuffer.pop_front();
		packetDataMutex.unlock();
 
		//在流媒体时，buffer满了直接丢弃未解码数据
		if (xffmpeg->isNetUrl && decodedBuffer.size() >= decodedBufferSize)
		{
			//printf("decode buffer full！throw a frame！");
			av_free_packet(pkt);
			continue;
		}

		//int t0 = SDL_GetTicks();
		
		//开始解码
		AVFrame* srcFrame = av_frame_alloc(); //解码后的frame	
		ret = xffmpeg->DecodeVideo(pkt, srcFrame);
		if (srcFrame->width == 0)
		{
			//std::printf("decode fail！\n");
			av_frame_free(&srcFrame);
			failNum++;
		}
		else
		{
			failNum = 0;			
			//缓存解码完成的数据
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

//初始化OSD
int Player::InitOSD()
{
	try
	{
		int ret = 0;
		bool isNeedFiltered = CombineOSDCommand(strOSD); //合并滤镜字符,返回是否需要滤镜
		if (!isNeedFiltered)
		{
			filterFlag = 0;
			return 0;
		}
		if (odlDescr != strOSD)  //字符发生变更  
		{
			//filterFlag为0时，为首次使用滤镜，默认切换为第1个滤镜
			//当前正在使用第1个滤镜时，初始化第2个滤镜做准备
			//当前正在使用第2个滤镜时，初始化第1个滤镜做准备
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

//判断是否显示字符，descr为DrawText指令
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
 
	//没有启用滤镜效果
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

//滤镜处理 输出YUV420P格式
//srcFrame:
//dstFrame:
//filterNum:滤镜选择 1或2
int  Player::Filtering(AVFrame* srcFrame, AVFrame* dstFrame, int filterNum)
{
	int ret = 0;
	/*一个滤镜链中包含一系列顺序连接的滤镜。滤镜之间通过逗号分隔开。
	一个滤镜图中包含一系列滤镜链。滤镜链之间通过分号隔开。
	滤镜描述格式：[in_link_1]...[in_link_N]filter_name = arguments[out_link_1]...[out_link_M]*/
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

bool shotSucessful = false;  //截图成功标志
//异步显示
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

	//回调
	if (player->drawCallBack != NULL)
	{
		RECT rect;
		GetWindowRect((HWND)player->sdlDisplay->hwndTemp, &rect);
		player->drawCallBack(player->Port, player->sdlDisplay->hwndTemp, rect.right - rect.left, rect.bottom - rect.top);
	}
	return ret ==0 ;
}


//YUV格式图像显示
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

	//回调
	if (player->drawCallBack != NULL)
	{
	    RECT rect;
		GetWindowRect((HWND)player->sdlDisplay->hwndTemp, &rect);
		player->drawCallBack(player->Port, player->sdlDisplay->hwndTemp, rect.right - rect.left, rect.bottom - rect.top);
	}
	return ret == 0;
}


//截图
bool Player::CatchImage(string path)
{
	shotSucessful = false;
	shotImagePath = path;
	bShotting = true;
	SDL_Delay(100);
	return shotSucessful;
}

//异步截图
//bool CatchImage(AVFrame* frame, string shotImagePath)
//{
//	ShotImage shotImage;
//	return shotImage.ConvertImage(frame, shotImagePath.c_str());
//}

//图像处理线程
void Player::FrameProcessingThread()
{
	int ret;
	int tempW = xffmpeg->srcWidth;
	int tempH = xffmpeg->srcHeight;
	Uint32 showStart = 0;
	bool delayFlag = false;

	//判断是否已事先设置滤镜
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

			//【1】取Frame
			AVFrame* srcFrame = NULL;
			decodeDataMutex.lock();
			if (decodedBuffer.size() > 0)
				srcFrame = decodedBuffer.front();
			decodedBuffer.pop_front();
			decodeDataMutex.unlock();
			
			//网络流时，若处于暂停状态，需要取出已解码的数据并丢弃；本地视频时，不丢弃数据
			if (xffmpeg->isNetUrl && playStatus == PlayStatus::Pause)
			{
				av_frame_free(&srcFrame);
				continue;
			}

			int srcW = srcFrame->width;
			int srcH = srcFrame->height;

			AVFrame* resultFrame = srcFrame;  //最终显示的图像	

			//【2】叠加OSD，不改变YUV格式	 
			if (!strOSD.empty()&&filterFlag!=0)
			{
				AVFrame* filterFrame = av_frame_alloc(); //滤镜处理后的frame 	
				ret = Filtering(srcFrame, filterFrame, filterFlag);
				if (ret < 0)
				{
					av_frame_free(&filterFrame);
					continue;
				}
				resultFrame = filterFrame;
			}

			//【3】录像保存
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
					std::printf("record buffer full，throw a frame！\n");
			}

			//【4】转换格式为AV_PIX_FMT_YUV420P并显示
			//滤镜过程已经转换为AV_PIX_FMT_YUV420P，不需要再转换
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
 
			//本地视频播放速度控制，延时
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

			//【5】解码数据异步回调
			if (decodeCallBack != NULL)
			{
				AVFrame* frame = av_frame_alloc();
				frame = av_frame_clone(resultFrame);
				std::future<int> fe = std::async(std::launch::async, yuvDataCallBack, this, frame);
			}

			//【6】显示
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
 

//编码保存视频
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
			//录像保存

			 //int t0 = SDL_GetTicks();
			//【0】编码			
			AVFrame* frame;
			recordMutex.lock();
			frame = recordBuffer.front();
			recordBuffer.pop_front();
			recordMutex.unlock();

			int t1 = SDL_GetTicks();
			AVPacket* pkt = av_packet_alloc();
			ret = xffmpeg->Encode(frame, pkt);
			if (ret < 0)//编码失败
			{
				av_free_packet(pkt);
				continue;
			}

			int t2 = SDL_GetTicks();
			//std::printf("encode used time:%d\n", t2 - t1);

			//【1】创建新的视频文件
			if (fileTime == 0)  //如果文件录制完成一段
			{
				xffmpeg->m_NextFrameIsKey = true;
				creatFileFlag = true;
				if (fileNum > 0)
					recordPath = recordVideoDir + "-" + to_string(fileNum) + recordVideoFmt;
				ret = xffmpeg->OpenOutput(recordPath);    // 创建新的视频文件
			}

			//【2】记录开始写入文件的时间
			if (startTime == 0)
			{
				startTime = SDL_GetTicks();
			}
			//【3】向视频文件中写入数据	
			ret = xffmpeg->WritePacket(pkt);
			av_free_packet(pkt);
			//int t3 = SDL_GetTicks();


			//【4】计算录制时长
			fileTime = SDL_GetTicks() - startTime;
			//std::printf("record time length:%d\n", fileTime);
			//【5】单次录制时长到达设定值时
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
		//结束录制时
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

		//清空编码Buffer
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

//音频解码线程
void Player::DecodeAudioThread()
{
	int ret = 0;
	int failNum = 0;
	long t_start = SDL_GetTicks();

	//打开音频设备
	AVCodecContext* acc = xffmpeg->audioDecoderCtx;
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO; //音频输出格式布局
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;//  AUDIO_S16SYS
	int out_framesize = 1024;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	int out_sample_rate = acc->sample_rate;//输出采样率
	sdlDisplay->InitAudio(out_framesize, out_sample_rate, out_channels);
	
	//音频格式转换器
	SwrContext *au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,
		out_channel_layout,//输出
		out_sample_fmt, //编码前你希望的格式
		out_sample_rate,//输出
		av_get_default_channel_layout(acc->channels), //in_channel_layout, //输入
		acc->sample_fmt,//PCM源文件的采样格式
		acc->sample_rate, //输入
		0, NULL);
	swr_init(au_convert_ctx);

	uint8_t** audio_data_buffer = NULL;//存储转换后的数据，再编码AAC  
	//分配样本数据内存空间
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

		//取数据包
		AVPacket* pkt;
		audioPacketMutex.lock();
		pkt = audioPacketBuffer.front();
		audioPacketBuffer.pop_front();
		audioPacketMutex.unlock();

		//开始解码
		vector<AVFrame*> frames; //解码后的frame	
		ret = xffmpeg->DecodeAudio(pkt, frames);
		av_free_packet(pkt);

		//音频播放
		vector<AVFrame*>::iterator i;  //定义正向迭代器
		for (i = frames.begin(); i != frames.end(); ++i)
		{
			AVFrame* frame = *i;
			int audio_buffersize = frame->linesize[0];

			////转码
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