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


	//��ʼ��SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
	{
		printf("SDL: could not initialize SDL - %s\n", SDL_GetError());
	}
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d");   //ָ����Ⱦ��ʽ   "direct3d","opengl","opengles2","opengles","metal","software"
	if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"))//D3D ֧��
	{
		printf("Warning: Linear texture filtering not enabled!");
	}
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");   //�Ŵ��㷨
	SDL_EventState(SDL_WINDOWEVENT, SDL_IGNORE);       //����ı�ؼ���Сʱ������������

}



SDLDisplay::~SDLDisplay()
{

}

 
 //�����Ƶ�豸�������Ļص�����
void fill_audio_buffer(void* userdata, Uint8* stream, int len)
{
	SDLDisplay* sdl = (SDLDisplay*)userdata;
	SDL_memset(stream, 0, len);
	// �ж��Ƿ��ж�������
	if (sdl->audio_len == 0)
		return;
	 
	len = (len > sdl->audio_len ? sdl->audio_len : len);
	SDL_MixAudio(stream, sdl->pAudio_pos, len, SDL_MIX_MAXVOLUME); //���������ݽ��л���
	sdl->pAudio_pos += len;
	sdl->audio_len -= len;

}
 
//��ʼ����Ƶ�豸
int SDLDisplay::InitAudio(int out_framesize,int out_sample_rate,int out_channels)
{
	//��Ƶ���Ų�������
	SDL_AudioSpec audioSpec;
	audioSpec.freq = out_sample_rate; //DSPƵ�� �൱�ڲ����� ,��Ƶ���ݵĲ����ʡ����õ���48000,44100��
	audioSpec.format = AUDIO_S16SYS; //��ʽ
	audioSpec.channels = out_channels; //ͨ����
	audioSpec.silence = 0; //����ֵ��һ��Ϊ0
	audioSpec.samples = out_framesize;
	audioSpec.callback = fill_audio_buffer; //�ص�
	audioSpec.userdata = this; //�û�����
	int a = SDL_OpenAudio(&audioSpec, nullptr); //����Ƶ�豸���ڶ�������Ϊ������Ƶ���豸��NULL��ʾĬ���豸
	if (a < 0)
	{
		printf("Can not open audio! %d", a);
		return -1;
	}
	SDL_PauseAudio(0);
	return 0;
}


//������Ƶ
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

#pragma region //SDL����
//��ʼ��SDL
//width height��ƵԴ�ķֱ���
int SDLDisplay::CreateSDLWindow(int width, int height, const void* data)
{
	SDL_LockMutex(mux);
	hwndTemp = data;
	widthTemp = width;
	heightTemp = height;

	//����SDL���� ���ݸ������� ��
	screen = SDL_CreateWindowFrom(data);
	if (!screen)
	{
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		SDL_DestroyWindow(screen);
		SDL_UnlockMutex(mux);
		return -1;
	}
	 
	SDL_ShowWindow(screen);  //SDL_quit�Ὣ��������
 
	//������Ⱦ��
	//sdlRenderer = SDL_CreateRenderer(
	//	screen,//��Ⱦ��Ŀ�괰�ڡ�
	//	-1, //�����ʼ������Ⱦ�豸�����������á� - 1�����ʼ��Ĭ�ϵ���Ⱦ�豸��
	//	SDL_RENDERER_ACCELERATED	//֧������ֵ��λ��SDL_RendererFlags�����У� ��xp��ֻ֧��SDL_RENDERER_SOFTWARE ��ʹ�������Ⱦ��
	//		//SDL_RENDERER_SOFTWARE ��ʹ�������Ⱦ
	//		//SDL_RENDERER_ACCELERATED ��ʹ��Ӳ������
	//		//SDL_RENDERER_PRESENTVSYNC������ʾ����ˢ����ͬ��
	//		//SDL_RENDERER_TARGETTEXTURE ����Ⱦ��֧����Ⱦ������
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
 
	//��������
	sdlTexture = SDL_CreateTexture(sdlRenderer,						//Ŀ����Ⱦ��
		SDL_PIXELFORMAT_YV12,          //����ĸ�ʽ  SDL_PIXELFORMAT_BGRA32 SDL_PIXELFORMAT_IYUV
		SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING,   //�仯Ƶ��
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


//���Ŵ���
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


//SDL������ʾYUV��ʽ  
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
	int ret = SDL_UpdateYUVTexture(sdlTexture, //Ŀ������
		NULL, //�������صľ�����������ΪNULL��ʱ�������������
		frame->data[0],    //��������
		frame->linesize[0],//һ���������ݵ��ֽ�����
		frame->data[1],
		frame->linesize[1],
		frame->data[2],
		frame->linesize[2]);
	ret = SDL_RenderClear(sdlRenderer);  //�����Ⱦ��
	//SDL_RenderCopy( sdlRenderer, sdlTexture, &sdlRect, &sdlRect );  
	ret = SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);//��Ŀ��������Կ�
	SDL_RenderPresent(sdlRenderer);//��ʾ����	
	//SDL_Surface * surface = SDL_GetWindowSurface(screen);//���´���֮ǰ���ߵ�ʱ�õ�surfaceָ�� �����ڴ˴�����
    //SDL_SaveBMP(surface, "D:\\123.bmp");  //������Ļ��ͼ �����浽��Ϊ����  //��ʼ����Ⱦ����ʱ�������ַ�ʽ��Ӳ����Ⱦ�������Ⱦ��Ӳ����Ⱦ�����Ϊ����
	SDL_UnlockMutex(mux);
	return ret;
}

//SDL������ʾrgb��ʽ��Ч�ʸ���  5ms����
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
	if (screen) // ����window
	{
		SDL_DestroyWindow(screen);
		screen = NULL;
	}
	if (sdlRenderer) // ������Ⱦ��
	{
		SDL_DestroyRenderer(sdlRenderer);
		sdlRenderer = NULL;
	}
	if (sdlTexture) // ��������
	{
		SDL_DestroyTexture(sdlTexture);
		sdlTexture = NULL;
	}
	//SDL_Quit();//�������SDLռ����Դ�����˳��� 
	SDL_UnlockMutex(mux);
}

#pragma endregion
