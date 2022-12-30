#include "stdafx.h"
#include "XFFMpeg.h"
#include "Common.h"


XFFMpeg::XFFMpeg()
{
	pktCount = 0;
	encoderCtx = NULL;
	decoderCtx = NULL;
	audioDecoderCtx = NULL;
	inFormatCtx = NULL;
	outFormatCtx = NULL;
	decoderCodec = NULL;
	audioDecoderCodec = NULL;
	encoderCodec = NULL;
	sCtx = NULL;
	filterGraph = NULL;
	bufferSrcCtx = NULL;
	bufferSinkCtx = NULL;
	inputs = NULL;
	outputs = NULL;

	filterGraph2 = NULL;
	bufferSrcCtx2 = NULL;
	bufferSinkCtx2 = NULL;
	inputs2 = NULL;
	outputs2 = NULL;

	encOpitons = NULL;
	decOpitons = NULL;

	in_stream = NULL;
	out_stream = NULL;
	videoUrl = "";

	timeout = 10;
}

XFFMpeg::~XFFMpeg()
{
}

int ff_lockmgr_callback(void** mutex, enum AVLockOp op)
{
	switch (op)
	{
	case AV_LOCK_CREATE:///< Create a mutex  
	{
		CRITICAL_SECTION* cs = (CRITICAL_SECTION*)av_malloc(sizeof(CRITICAL_SECTION));
		if (!cs)
		{
			return -1;
		}
		memset(cs, 0, sizeof(CRITICAL_SECTION));
		InitializeCriticalSection(cs);
		*(CRITICAL_SECTION**)mutex = cs;
	}
	break;
	case AV_LOCK_OBTAIN:///< Lock the mutex  
	{
		if (mutex && *(CRITICAL_SECTION**)mutex)
		{
			::EnterCriticalSection(*(CRITICAL_SECTION**)mutex);
		}
	}
	break;
	case AV_LOCK_RELEASE:///< Unlock the mutex  
	{
		if (mutex && *(CRITICAL_SECTION**)mutex)
		{
			::LeaveCriticalSection(*(CRITICAL_SECTION**)mutex);
		}
	}
	break;
	case AV_LOCK_DESTROY:///< Free mutex resources  
	{
		if (mutex && *(CRITICAL_SECTION**)mutex)
		{
			::DeleteCriticalSection(*(CRITICAL_SECTION**)mutex);
			av_free(*(CRITICAL_SECTION**)mutex);
			*(CRITICAL_SECTION**)mutex = NULL;
		}
	}
	break;
	default:
		break;
	}
	return 0;
}

void ff_log_callback(void* avcl, int level, const char* fmt, va_list vl)
{
	char log[1024];
	vsnprintf(log, sizeof(log), fmt, vl);
	OutputDebugStringA(log);
}

//注册 初始化库
void XFFMpeg::InitRegister()
{
	av_register_all();
	avfilter_register_all();
	avformat_network_init();
	//在vc的调试输出窗口中看见所有的ffmpeg日志
	//av_log_set_level(AV_LOG_ERROR);
   // av_log_set_callback(ff_log_callback);   
	//使用多个线程同时播放多个视频源的时候，在调用avcodec_open / close的时候，可能导致失败，
	//这个可以查阅ffmpeg的源码分析其中的原因，失败的主要原因是在调用此2函数时，
	//ffmpeg为了确保该2函数为原子操作，在avcodec_open / close两函数的开头和结尾处使用了
	//一个变量entangled_thread_counter来记录当前函数是否已经有其他线程进入，如果有其他线程正在此2函数内运行，则会调用失败。
	//  av_lockmgr_register(ff_lockmgr_callback);
}

#pragma region //解封装、解码、播放

//获取CPU核心数
int getCPUNumber()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors;
}


 //超时中断av_read_frame
static int check_read_interrupt(void* ctx)
{
	XFFMpeg* p = (XFFMpeg*)ctx;
	int64_t t = time(NULL);
	if (t - p->m_tStart > p->timeout) // time(NULL)获取当前系统时间,单位为秒
		return 1;
	else
		return 0;
}


//打开视频文件 视频流或者视频文件
int XFFMpeg::OpenInput(const string url)
{
	if (url.length() == 0)
		return -1;
	videoUrl = url;

	inFormatCtx = avformat_alloc_context();
	//av_read_frame的超时设置，如果没有设置timeout，那么掉线后，av_read_frame会阻塞
	//设置av_read_frame的超时时间，可 check_read_interrupt中动态修改
	//inFormatCtx->interrupt_callback.callback = check_read_interrupt;  //超时中断
	//inFormatCtx->interrupt_callback.opaque = this;
	m_tStart = time(NULL);

	//inFormatCtx->pb;
	// 设置参数
	decOpitons = NULL;
	int r = url.find_first_of(":");
	string flag = url.substr(0, r);
	isNetUrl = false;
	isUDPStream = false;
	int ret = -1;
	
	if (flag.compare("udp") == 0 || flag.compare("rtsp") == 0 || flag.compare("rtmp") == 0 || flag.compare("http") == 0)
	{
		if (flag.compare("udp") == 0)
			isUDPStream = true;
		isNetUrl = true;
		//stimeout:(rtsp us)的单位是us
		//timeout:(http:ms udp:s)的单位是ms
		if (flag.compare("rtsp") == 0)
		{
			//ret = av_dict_set(&decOpitons, "rtsp_transport", "udp", 0);  //udp丢包导致马赛克
			ret = av_dict_set(&decOpitons, "rtsp_transport", "tcp", 0);  //
		}
		ret = av_dict_set(&decOpitons, "stimeout", "20000000", 0);  //us 设置超时断开，在进行连接时是阻塞状态，若没有设置超时断开则会一直去阻塞获取数据，单位是微秒。 10s=10000000us

		// 设置“buffer_size”缓存容量  根据码率大小调整
		ret = av_dict_set(&decOpitons, "buffer_size", "500000000", 0);  //bite 50000000

		// probesize的单位是字节。 探测深度大小，读取文件信息   
		ret = av_dict_set(&decOpitons, "probesize", "50000000", 0); //set probing size 

		 //指定分析多少微秒以探测输入和probesize谁先达到都停止 ,default 5,000,000 µs = 5 s
		ret = av_dict_set(&decOpitons, "analyzeduration", "20000000", 0);  //specify how many microseconds are analyzed to probe the input

		//设置网络延时时间，默认设置的时间比较短，如果网络连不上，会直接断掉
		//延时时间设置大一点，在一些网络情况不是特别好的时候，非常有效果
		//如果局域网的健康可以不用设置网络延时时间
		ret = av_dict_set(&decOpitons, "max_delay", "5000", 0); //毫秒

		//设置低延时 以下两句效果一样
		ret = av_dict_set(&decOpitons, "fflags", "nobuffer", 0);
		//inFormatCtx->flags = AVFMT_FLAG_NOBUFFER;  //不显示探测的码流


		//设置av_read_frame的超时时间，不可动态设置
		//av_dict_set(&decOpitons, "rw_timeout", "10000", 0);//单位:ms  
	}

	//打开文件
	ret = avformat_open_input(&inFormatCtx, url.c_str(), inFmt, &decOpitons);
	if (ret != 0)                //udp://127.0.0.1:7000/
	{
		av_dict_free(&decOpitons);
		//printf("Couldn't open input stream.\n");
		avformat_close_input(&inFormatCtx);
		return -20;
	}

	//av_dict_free(&decOpitons);

	videoIndex = -1;
	audioIndex = -1;
	//找到音视频索引
	videoIndex = av_find_best_stream(inFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	audioIndex = av_find_best_stream(inFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (videoIndex < 0)
	{
		//printf("Can not fund video stream!\n");
		avformat_close_input(&inFormatCtx);
		return -1;
	}

	//获取视频流信息
	//if (!isNetUrl)  
	//{
	if (avformat_find_stream_info(inFormatCtx, NULL) < 0)
	{
		//printf("Couldn't find stream information.\n");
		avformat_close_input(&inFormatCtx);
		return -1;
	}
	fps = av_q2d(inFormatCtx->streams[videoIndex]->avg_frame_rate);
	/*}
	else
	{
		getResolution(info);
		inFormatCtx->streams[videoIndex]->codec->width = info.width;
		inFormatCtx->streams[videoIndex]->codec->height = info.height;
		fps = info.fps;
	}*/

	srcWidth = inFormatCtx->streams[videoIndex]->codec->width;
	srcHeight = inFormatCtx->streams[videoIndex]->codec->height;
	decoderCtx = inFormatCtx->streams[videoIndex]->codec;
	 
	if(audioIndex>=0)
		audioDecoderCtx = inFormatCtx->streams[audioIndex]->codec;

	if (videoIndex < 0 || srcWidth <= 0 || srcHeight <= 0)
	{
		//printf("Couldn't find stream information.\n");
		avformat_close_input(&inFormatCtx);
		return -21;
	}

	if (inFormatCtx->duration != AV_NOPTS_VALUE) {
		int hours, mins, secs, us;
		int64_t duration = inFormatCtx->duration + 5000;
		secs = duration / AV_TIME_BASE;
		us = duration % AV_TIME_BASE;
		mins = secs / 60;
		secs %= 60;
		hours = mins / 60;
		mins %= 60;
		//printf("%02d:%02d:%02d.%02d\n", hours, mins, secs, (100 * us) / AV_TIME_BASE);
		totalTime = duration / AV_TIME_BASE;
	}
	/*totalTime = inFormatCtx->streams[videoIndex]->duration / (AV_TIME_BASE);*/
	bitrate = decoderCtx->bit_rate;
	videCodecID = decoderCtx->codec_id;
 
	return 0;
}

//释放输入文件内容
void XFFMpeg::CloseInput()
{
	if (inFormatCtx)
		/*avformat_close_input
		（1）调用AVInputFormat的read_close()方法关闭输入流
		（2）调用avformat_free_context()释放AVFormatContext
		（3）调用avio_close()关闭并且释放AVIOContext*/
		avformat_close_input(&inFormatCtx);

	//if (decoderCtx)
	//{
		//avcodec_close(decoderCtx);
		 //avcodec_free_context(&decoderCtx);
	//}
	//if (decoderCodec)
	//	av_free(decoderCodec);

}

//初始化解码器
int XFFMpeg::InitDecoderCodec()
{
	if (!inFormatCtx)//若未打开视频
	{
		return -1;
	}
	//打开解码器AV_CODEC_ID_H264
	decoderCodec = avcodec_find_decoder(decoderCtx->codec_id);
	if (decoderCodec == NULL)
	{
		//printf("Codec not found.\n");
		avcodec_free_context(&decoderCtx);
		return -1;
	}

	//配置解码器上下文的参数
	 //avcodec_parameters_to_context(decoderCtx, inFormatCtx->streams[videoIndex]->codecpar);//将流的参数复制到解码器
	if (decoderCtx->pix_fmt == AVPixelFormat::AV_PIX_FMT_NONE)
		decoderCtx->pix_fmt = AVPixelFormat::AV_PIX_FMT_YUV420P;
	decoderCtx->thread_safe_callbacks = 1;
	decoderCtx->thread_count = getCPUNumber();  //默认1，0为调用最大线程数来进行软解。
	decoderCtx->flags = AV_CODEC_FLAG_LOW_DELAY;  //低延时 

	if (avcodec_open2(decoderCtx, decoderCodec, NULL) < 0)
	{
		//printf("Could not open codec.\n");
		avcodec_free_context(&decoderCtx);
		return -1;
	}
	return 0;
}

//初始化音频解码器
int XFFMpeg::InitAduioDecoderCodec()
{
	if (!inFormatCtx)//若未打开视频
	{
		return -1;
	}
	//打开解码器AV_CODEC_ID_AAC
	audioDecoderCodec = avcodec_find_decoder(audioDecoderCtx->codec_id);
	if (audioDecoderCodec == NULL)
	{
		//printf("Codec not found.\n");
		avcodec_free_context(&audioDecoderCtx);
		return -1;
	}

	if (avcodec_open2(audioDecoderCtx, audioDecoderCodec, NULL) < 0)
	{
		//printf("Could not open codec.\n");
		avcodec_free_context(&audioDecoderCtx);
		return -1;
	}
	return 0;
}


//读取帧包 pkt
int XFFMpeg::ReadPacket(AVPacket* pkt)
{
	int ret = 0;
	if (!inFormatCtx)
	{
		return -1;
	}
	m_tStart = time(NULL);
	ret = av_read_frame(inFormatCtx, pkt);	 
	//返回值：AVERROR_EOF等
	if (ret < 0)
	{
		cout << "read frame fail! code:" << ret << endl;
		return ret;
	}
	/*	else
		   cout << "read frame ok! pts=" << pkt->pts << endl;*/
	return 0;
}


//解码视频包Packet 获得frame  不管成功与否都释放pkt
int XFFMpeg::DecodeVideo(AVPacket* pkt, AVFrame* yuvFrame)
{
	if (!inFormatCtx)
	{
		av_packet_unref(pkt);
		return -1;
	}
	if (pkt->stream_index != videoIndex)
	{
		av_packet_unref(pkt);
		return -1;
	}

	//判断是否为关键帧I
	if (pkt->flags & AV_PKT_FLAG_KEY)
	{
		//在解码I帧之前清空解码器缓存，为了降低UDP丢包等导致IDE缓冲帧数增加
	 	// avcodec_flush_buffers(decoderCtx);
	}
 
	//发送之前读取的视频帧pkt到ffmepg，放到解码队列中 分析H264解码
	int re = avcodec_send_packet(decoderCtx, pkt);
	if (re != 0)
	{
		av_packet_unref(pkt);
		return -1;
	}
	//将成功的解码队列中取出1个frame,基本上所有解码器解码之后得到的图像数据都是YUV420的格式
	re = avcodec_receive_frame(decoderCtx, yuvFrame);
	if (re == AVERROR_EOF)
	{
		/*while (true)
		{
			re = avcodec_receive_frame(decoderCtx, yuvFrame);
		}*/
		avcodec_flush_buffers(decoderCtx);// 解码器已冲洗，解码中所有帧已取出
	}
	if (re != 0 && yuvFrame->width == 0)
	{
		av_packet_unref(pkt);
		return -1;
	}
	av_packet_unref(pkt);
	return re;
}

//解码音频
int XFFMpeg::DecodeAudio(AVPacket* pkt, vector<AVFrame*>& frames)
{
	int ret, data_size;

	/* send the packet with the compressed data to the decoder */
	ret = avcodec_send_packet(audioDecoderCtx, pkt);
	if (ret < 0) {
		fprintf(stderr, "Error submitting the packet to the decoder\n");
		return -1;
	}
	//通常视频帧只用被avcodec_receive_frame()一次，而音频帧需要被avcodec_receive_frame()多次。
	/* read all the output frames (in general there may be any number of them */
	while (ret >= 0) 
	{
		AVFrame* frame = av_frame_alloc();
		ret = avcodec_receive_frame(audioDecoderCtx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			av_frame_free(&frame);
			break;
		}
		else if (ret < 0) 
		{
			av_frame_free(&frame);
			fprintf(stderr, "Error during decoding\n");
			break;
		}

		//1) AAC:
		//nb_samples和frame_size = 1024
		//一帧数据量：1024 * 2 * av_get_bytes_per_sample(s16) = 4096个字节。
		//会编码：88200 / (1024 * 2 * av_get_bytes_per_sample(s16)) = 21.5帧数据
		data_size = av_get_bytes_per_sample(audioDecoderCtx->sample_fmt);
		if (data_size < 0)
		{
			av_frame_free(&frame);
			/* This should not occur, checking just for paranoia */
			fprintf(stderr, "Failed to calculate data size\n");
			break;
		}
		frames.push_back(frame);
	}
	if (frames.size()>0)
		return 0;
	else
		return -1;
}

//清空buffer
void XFFMpeg::ClearBuffer()
{
	if (outFormatCtx)
		avio_flush(outFormatCtx->pb);
	if (decoderCtx)
		avcodec_flush_buffers(decoderCtx);
	if (inFormatCtx)
		avformat_flush(inFormatCtx);
	if (encoderCtx)
		avcodec_flush_buffers(encoderCtx);
}

void XFFMpeg::Close()
{
	/*cout << "close video" << endl;*/
	SwsFree();
	CloseInput();
	CloseFilter();
	CloseFilter2();
	CloseOutput();
}

#pragma endregion

#pragma region  //编码、封装、保存
//初始化编码器 codecID流格式
int XFFMpeg::InitEncoderCodec(AVCodecID codecID, int width, int height, int fps, float quality)
{
	int ret = 0;
	encFps = fps;

	//编码质量设置
	bitrate = (int)(width * height);
	if (quality <= 0)
		bitrate = 0;
	if (quality > 5)
		quality = 5;
	unsigned int br = (int)(bitrate / 5.0f * quality);

	//查找编码器
	encoderCodec = avcodec_find_encoder(codecID);
	if (NULL == encoderCodec)
	{
		//printf("%s", "avcodec_find_encoder failed");
		return  -1;
	}
	//编码器参数
	encoderCtx = avcodec_alloc_context3(encoderCodec);
	in_stream = inFormatCtx->streams[videoIndex];
	//将输入流的编码参数copy到AVCodecContext结构体中，并且重新拷贝一份extradata内容，涉及到的视频的关键参数有format, width, height, codec_type等
	ret = avcodec_parameters_to_context(encoderCtx, in_stream->codecpar);
	encoderCtx->codec_tag = 0;
	
	//以下修改输出流的部分参数
	encoderCtx->gop_size = fps;			 //连续的画面组大小  I帧间隔 每多少帧插入一个I帧，I帧越少视频越小 I帧编码耗时最短  耗时：P>B>I
	encoderCtx->has_b_frames = 1;			 //设置为0不使用B帧 B帧越多图片越小
	encoderCtx->max_b_frames = 3;            //连续的B帧的数量，一般设置不超过三帧，这里设置三帧
	encoderCtx->codec_id = codecID;       //编码器编码的数据类型  
	encoderCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	encoderCtx->time_base.num = 1;
	encoderCtx->time_base.den = fps;
	encoderCtx->framerate.num = 1;
	encoderCtx->framerate.den = fps;
	encoderCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	encoderCtx->width = width;
	encoderCtx->height = height;
	encoderCtx->thread_safe_callbacks = 1;
    encoderCtx->thread_count = getCPUNumber();  //默认1，0为调用最大线程数来进行软解。
 
	//固定码率CBR设置如下 输出文件大小可控
	//encoderCtx->bit_rate = br;     
	//encoderCtx->rc_max_rate = br;
	//encoderCtx->rc_min_rate = br;
 //	encoderCtx->rc_buffer_size = br;  // 用于设置码率控制缓冲器的大小 根据希望比特率获得多大的可变性而设置，默认为maxrate的2倍，如果想限制流的比特率，可设置为maxrate的一半
 //   encoderCtx->bit_rate_tolerance = br; //固定允许的码率误差，数值越大视频越小
	//encoderCtx->rc_initial_buffer_occupancy = encoderCtx->rc_buffer_size*3/4;
	//encoderCtx->rc_buffer_aggressivity = 1;
	//encoderCtx->rc_initial_cplx = 0.5;

	//可变码率 VBR 的设置 输出文件大小不可控
	/*encoderCtx->flags |= AV_CODEC_FLAG_QSCALE;
	encoderCtx->rc_min_rate = br/3*2;
	encoderCtx->rc_max_rate = br/3*4;
	encoderCtx->bit_rate = br;*/

	//平均码率 ABR 输出文件大小可控
	encoderCtx->bit_rate = br; // 若设置了bit_rate采用abr（平均码率）控制方式，没设置时按照crf（限制码率因子）方式 
	 

	//设置编码字典参数
	encOpitons = NULL;
	ret = av_dict_set(&encOpitons, "crf", "18", 0);  //crf默认23，18~28是一个合理的范围。18被认为是视觉无损的（从技术角度上看当然还是有损的），它的输出视频质量和输入视频相当。
	ret = av_dict_set(&encOpitons, "preset", "veryfast", 0);  //调节编码速度和质量的平衡，参数有:ultrafast\superfast\veryfast\faster\fast\medium\slow\slower\veryslower\placebo	
	ret = av_dict_set(&encOpitons, "tune", "zerolatency", 0); //配合视频的类型和视觉优化的参数zerolatency：零延迟  film:电影、真人类型   animation：动画  grain：需要保留大量的grain时使用 stillimage:静态图像编码，psnr：为提高psnr做了优化，ssim：为提高ssim做了优化
	//ret = av_dict_set(&encOpitons, "profile", "main", 0);  //一旦使用该选项，就无法进行无损编码 (--qp 0 or --crf 0)。画质设置1、Baseline Profile：基本画质。支持I/P 帧，只支持无交错（Progressive）和CAVLC；
														   /*2、Extended profile：进阶画质。支持I / P / B / SP / SI 帧，只支持无交错（Progressive）和CAVLC；(用的少)
															3、Main profile：主流画质。提供I / P / B 帧，支持无交错（Progressive）和交错（Interlaced），
															也支持CAVLC 和CABAC 的支持；
															4、High profile：高级画质。在main Profile 的基础上增加了8x8内部预测、自定义量化、 无损视频编码和更多的YUV 格式；*/
															//打开编码器
	ret = avcodec_open2(encoderCtx, encoderCodec, &encOpitons);
	av_dict_free(&encOpitons);
	if (ret < 0)
	{
		//printf("%s", "open codec failed");
		avcodec_close(encoderCtx);
		return  ret;
	}
	return ret;
}


//初始化输出上下文 写文件头 
int XFFMpeg::OpenOutput(const string fileName)
{
	int ret = 0;
	pktCount = 0;
	//【1】初始化一个用于输出的AVFormatContent
	ret = avformat_alloc_output_context2(&outFormatCtx, NULL, NULL, fileName.c_str());
	if (ret < 0)
	{
		//cout << "avformat_alloc_output_context2 fail!" << endl;
		return -1;
		avformat_close_input(&outFormatCtx);
	}

	//【2】创建输出流
	out_stream = avformat_new_stream(outFormatCtx, encoderCodec); //avformat_new_stream之后便在 AVFormatContext 里增加了 AVStream 通道（相关的index已经被设置了）。之后，我们就可以自行设置 AVStream 的一些参数信息。例如 : codec_id , format ,bit_rate ,width , height ... ...
	if (!out_stream)
	{
		//printf("Failed allocating output stream.\n");
		ret = AVERROR_UNKNOWN;
		return -1;
	}
	//out_stream->time_base.num = 1;
	//out_stream->time_base.den = fps;

	//【3】复制一份编码器的配置给输出流
	avcodec_parameters_from_context(out_stream->codecpar, encoderCtx);

	if (outFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
		encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;   //视频预览

	//【4】创建输出视频文件
	ret = avio_open2(&outFormatCtx->pb, fileName.c_str(), AVIO_FLAG_WRITE, NULL, NULL);
	if (ret < 0)
	{
		//cout << "avio_open2 fail!" << endl;
		avformat_close_input(&outFormatCtx);
		return -1;
	}

	//【5】写文件头（对于某些没有文件头的封装格式，不需要此函数。比如说MPEG2TS） 
	AVDictionary* opt = NULL;
	av_dict_set_int(&opt, "video_track_timescale", encFps, 0);//设置所有视频的时间计算方式 设置帧率 //要是没有设置这个值的话，就采用我们AVStream的time_base的分母
	//av_dump_format(outFormatCtx, 0, "head.txt", 1);
	ret = avformat_write_header(outFormatCtx, &opt);
	if (ret < 0)
	{
		//cout << "avformat_write_header fail!" << endl;
		avformat_close_input(&outFormatCtx);
		return -1;

	}
	return ret;

}



//编码函数 传入FRAME 传出packet,不管成功与否都释放frame
int XFFMpeg::Encode(AVFrame* pFrame, AVPacket* outPkt)
{
	int ret;
	if (!encoderCtx)
	{
		av_frame_free(&pFrame);
		return -1;
	}
	if (m_NextFrameIsKey)
	{
		m_NextFrameIsKey = false;
		pFrame->pict_type = AV_PICTURE_TYPE_I;//指定录像第一帧必须是关键帧
	}
	ret = avcodec_send_frame(encoderCtx, pFrame);
	av_frame_free(&pFrame);
	if (ret < 0 && ret != AVERROR_EOF)
	{
		//cout << "avcodec_send_frame fail!" << endl;
		return ret;
	}
	ret = avcodec_receive_packet(encoderCtx, outPkt);
	if (ret < 0 && ret != AVERROR(EAGAIN))
	{
		//cout << "avcodec_receive_packet fail!" << endl;
		return ret;
	}
	/*int gotPicture;
	ret = avcodec_encode_video2(encoderCtx, outPkt, pFrame, &gotPicture);
	av_frame_free(&pFrame);
	if (ret < 0 && gotPicture != 0)
	{
		cout << "avcodec_encode_video2 fail!" << endl;
		SDL_UnlockMutex(encodeMux);
		return ret;
	}*/
	return 0;
}


void XFFMpeg::CloseOutput()
{
	if (outFormatCtx)
	{
		if (outFormatCtx->pb)
			avio_close(outFormatCtx->pb);   //关闭视频文件
		avformat_free_context(outFormatCtx);
		outFormatCtx = NULL;
	}
	if (encoderCtx)
	{
		avcodec_close(encoderCtx);
		avcodec_free_context(&encoderCtx);
	}
	if (encoderCodec)
	{
		av_free(encoderCodec);
		encoderCodec = NULL;
	}
}


 

//写入文件
//（1）循环调用interleave_packet()以及write_packet()，将还未输出的AVPacket输出出来。
//（2）调用AVOutputFormat的write_trailer()，输出文件尾。
int XFFMpeg::WritePacket(AVPacket* pkt)
{
	if (!outFormatCtx)
		return -1;
	if (pkt->stream_index != videoIndex) //确保处理的是视频流
		return -1;
	//DTS(Decoding Time Stamp, 解码时间戳)，表示压缩帧的解码时间。
	//PTS(Presentation Time Stamp, 显示时间戳)，表示将压缩帧解码后得到的原始帧的显示时间。
	//不含B帧的视频，其DTS和PTS是相同
	//av_rescale_q_rnd则是将以 "时钟基c" 表示的 数值a 转换成以 "时钟基b" 来表示。

	//AVRational time_base = outFormatCtx->streams[0]->time_base;
	//AVRational r_frame_rate1 = encoderCtx->framerate;
	//AVRational time_base_q = { 1, AV_TIME_BASE };
	//int64_t calc_duration = (double)(AV_TIME_BASE) * (1 / av_q2d(r_frame_rate1));
	//enc_pkt.pts = count * (out_stream->time_base.den) / ((out_stream->time_base.num) * fps);
	//pkt->pts = av_rescale_q(pktCount * calc_duration, time_base_q, time_base);
	//pkt->dts = pkt->pts;
	//pkt->duration = (out_stream->time_base.den) / ((out_stream->time_base.num) * fps);
	//pkt->duration = av_rescale_q(calc_duration, time_base_q, time_base);
	//pkt->pos = -1;

	pkt->pts = pkt->dts = pktCount * encFps / 1 / encFps;
	pktCount++;


	//将数据包写入输出媒体文件以确保正确交错。
	//av_write_frame 直接将包写进Mux，没有缓存和重新排序，一切都需要用户自己设置。
	//av_interleaved_write_frame 将对packet进行缓存和pts检查,这是区别于av_write_frame 的地方
	int ret = av_write_frame(outFormatCtx, pkt);
	if (ret < 0)
		cout << "av_interleaved_write_frame fail!" << endl;
	return ret;
}

//结束写文件
int XFFMpeg::FinishWrite()
{
	if (!outFormatCtx)
		return -1;
	//输出文件尾
	int ret = av_write_trailer(outFormatCtx);
	if (ret < 0)
		cout << "av_write_trailer fail!" << endl;

	pktCount = 0;
	return ret;
}

#pragma endregion

#pragma region //格式转换
//定义格式转换上下文
int XFFMpeg::InitSwsContext(AVCodecContext* videoCtx, AVPixelFormat dstFmt, int width, int height)
{
	if (width == 0 || height == 0)
		return -1;

	dstFormat = dstFmt;
	srcFormat = videoCtx->pix_fmt;
	if (srcFormat == AVPixelFormat::AV_PIX_FMT_NONE)
	{
		srcFormat = AVPixelFormat::AV_PIX_FMT_YUV420P;
		//return -1;
	}

	int srcW = videoCtx->width;
	int srcH = videoCtx->height;
	if (videoCtx->width != width || videoCtx->height != height)
	{
		srcW = width;
		srcH = height;
	}
	//sws_getContext 可以用于多路码流转换，为每个不同的码流都指定一个不同的转换上下文，而 sws_getCachedContext 只能用于一路码流转换。
	if (!sCtx)
		sCtx = sws_getContext(srcW, /* 输入图像的宽度 */
			srcH, /* 输入图像的高度 */
			srcFormat,/* 输入图像的像素格式 */
			width,/* 输出图像的宽度 */
			height,/* 输出图像的高度 */
			dstFormat, /* 输出图像的像素格式 */
			SWS_FAST_BILINEAR,/* 选择缩放算法(只有当输入输出图像大小不同时有效),一般选择SWS_FAST_BILINEAR */
			NULL, NULL, NULL);
	return 0;
}



//转换格式,不管成功与否都释放srcFrame
int XFFMpeg::FormatConvert(AVFrame* srcFrame, AVFrame* dstFrame)
{

	uint8_t* pszBGRBuffer = NULL;
	int nBGRFrameSize;
	nBGRFrameSize = av_image_get_buffer_size(dstFormat, srcFrame->width, srcFrame->height, 1);
	pszBGRBuffer = (uint8_t*)av_malloc(nBGRFrameSize);
	av_image_fill_arrays(dstFrame->data, dstFrame->linesize, pszBGRBuffer, dstFormat, srcFrame->width, srcFrame->height, 1);
	//avpicture_alloc((AVPicture*)dstFrame, dstFormat, srcWidth, srcHeight);  //旧接口

	dstFrame->width = srcFrame->width;
	dstFrame->height = srcFrame->height;
	dstFrame->format = dstFormat;
	int height = sws_scale(sCtx,
		(uint8_t**)srcFrame->data,
		srcFrame->linesize,
		0,
		srcHeight,  //srcSliceY=0，srcSliceH=height，表示一次性处理完整个图像,这种设置是为了多线程并行，例如可以创建两个线程，第一个线程处理 [0, h/2-1]行，第二个线程处理 [h/2, h-1]行。并行处理加快速度
		dstFrame->data,//输出的每个颜色通道数据指针
		dstFrame->linesize);//每个颜色通道行字节数

	if (height <= 0)
	{
		cout << "sws_scale fail!" << endl;
	}
	av_frame_free(&srcFrame);

	return height;
}

//释放格式转换器
void XFFMpeg::SwsFree()
{
	if (sCtx)
	{
		sws_freeContext(sCtx);
		sCtx = NULL;
	}
}

  
#pragma endregion

#pragma region //滤镜、字符叠加

//初始化filter，输出为AV_PIX_FMT_YUV420P
int XFFMpeg::InitFilters(const char* filtersDescr)
{

	string str = Common::GBK_2_UTF8((string)filtersDescr);
	char args[512];
	int ret = 0;
	const AVFilter* buffersrc = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");
	outputs = avfilter_inout_alloc();
	inputs = avfilter_inout_alloc();

	//为FilterGraph分配内存
	filterGraph = avfilter_graph_alloc();
	if (!outputs || !inputs || !filterGraph)
	{
		ret = AVERROR(ENOMEM);
		goto end;
	}
	if(srcWidth>0)
	//args为描述输入帧序列的参数信息：
	sprintf_s(args,
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		srcWidth, srcHeight, srcFormat, decoderCtx->time_base.num, decoderCtx->time_base.den, decoderCtx->sample_aspect_ratio.num, decoderCtx->sample_aspect_ratio.den);

	//创建filter输入端 bufferSrcCtx的初始化需要args作为参数，否则无法正常初始化
	ret = avfilter_graph_create_filter(&bufferSrcCtx, buffersrc, "in", args, NULL, filterGraph);
	if (ret < 0)
	{
		cout << "Cannot create buffer source\n";
		goto end;
	}

	//创建filter输出端
	ret = avfilter_graph_create_filter(&bufferSinkCtx, buffersink, "out", NULL, NULL, filterGraph);
	if (ret < 0)
	{
		cout << "Cannot create buffer sink\n";
		goto end;
	}
	// 因为后面显示视频帧时有sws_scale()进行图像格式转换，故此处不设置滤镜输出格式也可
	// 设置输出像素格式为pix_fmts[]中指定的格式(如果要用SDL显示，则这些格式应是SDL支持格式)
	enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
	ret = av_opt_set_int_list(bufferSinkCtx, "pix_fmts", pix_fmts, -1, AV_OPT_SEARCH_CHILDREN);
	if (ret < 0)
	{
		cout << "Cannot set output pixel format\n";
		goto end;
	}
	outputs->name = av_strdup("in");
	outputs->filter_ctx = bufferSrcCtx;
	outputs->pad_idx = 0;
	outputs->next = NULL;
	inputs->name = av_strdup("out");
	inputs->filter_ctx = bufferSinkCtx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	//将一串通过字符串描述的Graph添加到FilterGraph中
	// hflip,crop=1280:1024
	//filtersDescr = "drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\simhei.ttf':x=0:y=0:fontcolor=white:fontsize=80:text='%{localtime\\:%Y-%m-%d %H\\\\\\:%M\\\\\\:%S}'";
	if ((ret = avfilter_graph_parse_ptr(filterGraph, str.c_str(), &inputs, &outputs, NULL)) < 0)
	{
		cout << "parse ptr error\n";
		goto end;
	}

	//检查FilterGraph的配置并格式化画布
	if ((ret = avfilter_graph_config(filterGraph, NULL)) < 0)
	{
		cout << "parse config error\n";
		goto end;
	}
	return ret;
end:
	CloseFilter();
	return -1;
}

//叠加字符，更新Frame
int XFFMpeg::FilterFrame(AVFrame* srcFrame, AVFrame* dstFrame)
{
	if (!filterGraph)
		return -1;
	//	cout << "av_buffersrc_add_frame" << endl;
	int ret = 0;
	//向FilterGraph中加入一个AVFrame
	//ret=  av_buffersrc_add_frame_flags(bufferSrcCtx, srcFrame, AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT);
	ret = av_buffersrc_add_frame(bufferSrcCtx, srcFrame);
	if (ret < 0)
		cout << "Error while feeding the filtergraph\n";
	//从FilterGraph中取出一个AVFrame
	ret = av_buffersink_get_frame(bufferSinkCtx, dstFrame);
	return ret;
}

//关闭滤镜
void XFFMpeg::CloseFilter()
{
	if (inputs)
		avfilter_inout_free(&inputs);
	if (outputs)
		avfilter_inout_free(&outputs);
	if (filterGraph)
		avfilter_graph_free(&filterGraph); //added by ws for avfilter
}

//初始化filter，输出为AV_PIX_FMT_YUV420P
int XFFMpeg::InitFilters2(const char* filtersDescr)
{
	char args[512];
	int ret = 0;
	const AVFilter* buffersrc = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");
	outputs2 = avfilter_inout_alloc();
	inputs2 = avfilter_inout_alloc();

	//为FilterGraph分配内存
	filterGraph2 = avfilter_graph_alloc();
	if (!outputs2 || !inputs2 || !filterGraph2)
	{
		ret = AVERROR(ENOMEM);
		goto end;
	}
	//args为描述输入帧序列的参数信息：
	sprintf_s(args,
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		srcWidth, srcHeight, srcFormat, decoderCtx->time_base.num, decoderCtx->time_base.den, decoderCtx->sample_aspect_ratio.num, decoderCtx->sample_aspect_ratio.den);

	//创建filter输入端 bufferSrcCtx的初始化需要args作为参数，否则无法正常初始化
	ret = avfilter_graph_create_filter(&bufferSrcCtx2, buffersrc, "in", args, NULL, filterGraph2);
	if (ret < 0)
	{
		cout << "Cannot create buffer source\n";
		goto end;
	}

	//创建filter输出端
	ret = avfilter_graph_create_filter(&bufferSinkCtx2, buffersink, "out", NULL, NULL, filterGraph2);
	if (ret < 0)
	{
		cout << "Cannot create buffer sink\n";
		goto end;
	}
	// 因为后面显示视频帧时有sws_scale()进行图像格式转换，故此处不设置滤镜输出格式也可
	// 设置输出像素格式为pix_fmts[]中指定的格式(如果要用SDL显示，则这些格式应是SDL支持格式)
	enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
	ret = av_opt_set_int_list(bufferSinkCtx2, "pix_fmts", pix_fmts, -1, AV_OPT_SEARCH_CHILDREN);
	if (ret < 0)
	{
		cout << "Cannot set output pixel format\n";
		goto end;
	}
	outputs2->name = av_strdup("in");
	outputs2->filter_ctx = bufferSrcCtx2;
	outputs2->pad_idx = 0;
	outputs2->next = NULL;
	inputs2->name = av_strdup("out");
	inputs2->filter_ctx = bufferSinkCtx2;
	inputs2->pad_idx = 0;
	inputs2->next = NULL;

	//将一串通过字符串描述的Graph添加到FilterGraph中
	// hflip,crop=1280:1024
	//filtersDescr = "drawtext=fontfile='C\\:\\\\Windows\\\\Fonts\\\\simhei.ttf':x=0:y=0:fontcolor=white:fontsize=80:text='%{localtime\\:%Y-%m-%d %H\\\\\\:%M\\\\\\:%S}'";
	if ((ret = avfilter_graph_parse_ptr(filterGraph2, filtersDescr, &inputs2, &outputs2, NULL)) < 0)
	{
		cout << "parse ptr error\n";
		goto end;
	}

	//检查FilterGraph的配置并格式化画布
	if ((ret = avfilter_graph_config(filterGraph2, NULL)) < 0)
	{
		cout << "parse config error\n";
		goto end;
	}
	return ret;
end:
	CloseFilter2();
	return -1;
}

 
int XFFMpeg::FilterFrame2(AVFrame* srcFrame, AVFrame* dstFrame)
{
	if (!filterGraph2)
		return -1;
	//	cout << "av_buffersrc_add_frame" << endl;
	int ret = 0;
	//向FilterGraph中加入一个AVFrame
	//ret=  av_buffersrc_add_frame_flags(bufferSrcCtx, srcFrame, AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT);
	ret = av_buffersrc_add_frame(bufferSrcCtx2, srcFrame);
	if (ret < 0)
		cout << "Error while feeding the filtergraph\n";
	//从FilterGraph中取出一个AVFrame
	ret = av_buffersink_get_frame(bufferSinkCtx2, dstFrame);
	return ret;
}

//关闭滤镜
void XFFMpeg::CloseFilter2()
{

	if (inputs2)
		avfilter_inout_free(&inputs2);
	if (outputs2)
		avfilter_inout_free(&outputs2);
	if (filterGraph2)
		avfilter_graph_free(&filterGraph2); //added by ws for avfilter
}

#pragma endregion

//跳转
int XFFMpeg::Seek(float pos)
{
	if (!inFormatCtx)//未打开视频
	{
		return false;
	}

	int64_t stamp = 0;
	stamp =  pos * inFormatCtx->streams[videoIndex]->duration;//当前它实际的位置
	int re = av_seek_frame(inFormatCtx, videoIndex, stamp, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);//将视频移至到当前点击滑动条位置
	avcodec_flush_buffers(decoderCtx);//刷新缓冲,清理掉
	if (re > 0)
		return true;
	return false;
}


