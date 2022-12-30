#include "stdafx.h"
#include "SDLDisplay.h"

SDLDisplay::SDLDisplay()
{
	widthTemp = 0;
	heightTemp = 0;
	hwndTemp = NULL;
	mux = SDL_CreateMutex();
	screen = NULL;
	sdlRenderer = NULL;
	sdlTexture = NULL;
	data_count = 0;

	 
	 audio_len = 0;
	 pAudio_pos = NULL;


	//初始化SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
	{
		printf("SDL: could not initialize SDL - %s\n", SDL_GetError());
	}
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d");   //指定渲染方式   "direct3d","opengl","opengles2","opengles","metal","software"
	if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"))//D3D 支持
	{
		printf("Warning: Linear texture filtering not enabled!");
	}
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");   //放大算法
	SDL_EventState(SDL_WINDOWEVENT, SDL_IGNORE);       //解决改变控件大小时，卡主的问题

}



SDLDisplay::~SDLDisplay()
{

}

 
 //填充音频设备缓冲区的回调函数
void fill_audio_buffer(void* userdata, Uint8* stream, int len)
{
	SDLDisplay* sdl = (SDLDisplay*)userdata;
	SDL_memset(stream, 0, len);
	// 判断是否有读到数据
	if (sdl->audio_len == 0)
		return;
	 
	len = (len > sdl->audio_len ? sdl->audio_len : len);
	SDL_MixAudio(stream, sdl->pAudio_pos, len, SDL_MIX_MAXVOLUME); //对音乐数据进行混音
	sdl->pAudio_pos += len;
	sdl->audio_len -= len;

}
 
//初始化音频设备
int SDLDisplay::InitAudio(int out_framesize,int out_sample_rate,int out_channels)
{
	//音频播放参数设置
	SDL_AudioSpec audioSpec;
	audioSpec.freq = out_sample_rate; //DSP频率 相当于采样率 ,音频数据的采样率。常用的有48000,44100等
	audioSpec.format = AUDIO_S16SYS; //格式
	audioSpec.channels = out_channels; //通道数
	audioSpec.silence = 0; //静音值，一般为0
	audioSpec.samples = out_framesize;
	audioSpec.callback = fill_audio_buffer; //回调
	audioSpec.userdata = this; //用户数据
	int a = SDL_OpenAudio(&audioSpec, nullptr); //打开音频设备，第二个参数为播放音频的设备，NULL表示默认设备
	if (a < 0)
	{
		printf("Can not open audio! %d", a);
		return -1;
	}
	SDL_PauseAudio(0);
	return 0;
}


//播放音频
int SDLDisplay::PlayAudio(int audio_buffersize,  uint8_t** audio_data)
{
	audio_chunk = *audio_data;
	data_count += audio_buffersize;
	audio_len = audio_buffersize;
	pAudio_pos = audio_chunk;
	while (audio_len>0)
	{
		Sleep(1);
	}
	return 0;
}

#pragma region //SDL播放
//初始化SDL
//width height视频源的分辨率
int SDLDisplay::CreateSDLWindow(int width, int height, const void* data)
{
	SDL_LockMutex(mux);
	hwndTemp = data;
	widthTemp = width;
	heightTemp = height;

	//创建SDL窗口 根据父窗体句柄 绑定
	screen = SDL_CreateWindowFrom(data);
	if (!screen)
	{
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		SDL_DestroyWindow(screen);
		SDL_UnlockMutex(mux);
		return -1;
	}
	 
	SDL_ShowWindow(screen);  //SDL_quit会将窗体隐藏
 
	//定义渲染器
	//sdlRenderer = SDL_CreateRenderer(
	//	screen,//渲染的目标窗口。
	//	-1, //打算初始化的渲染设备的索引。设置“ - 1”则初始化默认的渲染设备。
	//	SDL_RENDERER_ACCELERATED	//支持以下值（位于SDL_RendererFlags定义中） 而xp中只支持SDL_RENDERER_SOFTWARE （使用软件渲染）
	//		//SDL_RENDERER_SOFTWARE ：使用软件渲染
	//		//SDL_RENDERER_ACCELERATED ：使用硬件加速
	//		//SDL_RENDERER_PRESENTVSYNC：和显示器的刷新率同步
	//		//SDL_RENDERER_TARGETTEXTURE ：渲染器支持渲染到纹理
	//	);  

	sdlRenderer = SDL_CreateRenderer(
		screen, 
		-1,  
		NULL
	);

	if (!sdlRenderer)
	{
		printf("SDL: could not create renderer - %s\n", SDL_GetError());
		SDL_DestroyRenderer(sdlRenderer);
		SDL_UnlockMutex(mux);
		return -1;
	}
 
	//定义纹理
	sdlTexture = SDL_CreateTexture(sdlRenderer,						//目标渲染器
		SDL_PIXELFORMAT_YV12,          //纹理的格式  SDL_PIXELFORMAT_BGRA32 SDL_PIXELFORMAT_IYUV
		SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING,   //变化频繁
		width,
		height);
	if (!sdlTexture)
	{
		printf("SDL: could not create texture  - %s\n", SDL_GetError());
		SDL_DestroyTexture(sdlTexture);
		SDL_UnlockMutex(mux);
		return -1;
	}	
	SDL_UnlockMutex(mux);
	return 0;
}


//缩放窗体
int SDLDisplay::ResizeWindow(const void* data)
{
	  hwndTemp = data;
	  SDL_LockMutex(mux);
	 //SDL_SetWindowSize(screen, width, height);
	  CloseSDL();
	  CreateSDLWindow(widthTemp, heightTemp,data);
	  SDL_UnlockMutex(mux);
	  return 0;
}


//SDL窗体显示YUV格式  
int SDLDisplay::UpdateSDLWindowYUV(AVFrame* frame,int width,int height)
{
	if (width != widthTemp || heightTemp != height)
	{
		widthTemp = width;
		heightTemp = height;
		CloseSDL();
		CreateSDLWindow(width, height, hwndTemp);
	}
	if (frame->linesize[0] < 0)
		return -1;
	SDL_LockMutex(mux);
	int ret = SDL_UpdateYUVTexture(sdlTexture, //目标纹理
		NULL, //更新像素的矩形区域。设置为NULL的时候更新整个区域
		frame->data[0],    //像素数据
		frame->linesize[0],//一行像素数据的字节数。
		frame->data[1],
		frame->linesize[1],
		frame->data[2],
		frame->linesize[2]);
	ret = SDL_RenderClear(sdlRenderer);  //清空渲染器
	//SDL_RenderCopy( sdlRenderer, sdlTexture, &sdlRect, &sdlRect );  
	ret = SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);//将目标输出到显卡
	SDL_RenderPresent(sdlRenderer);//显示窗口	
	//SDL_Surface * surface = SDL_GetWindowSurface(screen);//更新窗口之前或者当时得到surface指针 ，不在此处调用
    //SDL_SaveBMP(surface, "D:\\123.bmp");  //保存屏幕截图 ，保存到的为黑屏  //初始化渲染器的时候有两种方式，硬件渲染和软件渲染，硬件渲染保存的为黑屏
	SDL_UnlockMutex(mux);
	return ret;
}

//SDL窗体显示rgb格式，效率更高  5ms左右
int SDLDisplay::UpdateSDLWindowRGB(AVFrame* frame, int width, int height)
{
	if (width != widthTemp || heightTemp != height)
	{
		widthTemp = width;
		heightTemp = height;
		CloseSDL();
		CreateSDLWindow(width, height, hwndTemp);
	}
	if (frame->linesize[0] < 0)
		return -1;
	SDL_LockMutex(mux);
	int ret = SDL_UpdateTexture(sdlTexture,
		NULL,
		frame->data[0],
		frame->linesize[0]);
	ret = SDL_RenderClear(sdlRenderer);
	//SDL_RenderCopy( sdlRenderer, sdlTexture, &sdlRect, &sdlRect );  
	ret = SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
	SDL_RenderPresent(sdlRenderer);
	SDL_UnlockMutex(mux);
	return ret;
}

void SDLDisplay::CloseSDL()
{
	SDL_LockMutex(mux);
	if (screen) // 销毁window
	{
		SDL_DestroyWindow(screen);
		screen = NULL;
	}
	if (sdlRenderer) // 销毁渲染器
	{
		SDL_DestroyRenderer(sdlRenderer);
		sdlRenderer = NULL;
	}
	if (sdlTexture) // 销毁纹理
	{
		SDL_DestroyTexture(sdlTexture);
		sdlTexture = NULL;
	}
	//SDL_Quit();//清空所有SDL占用资源，并退出。 
	SDL_UnlockMutex(mux);
}

#pragma endregion
