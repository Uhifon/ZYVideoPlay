#pragma once
#include "SDL.h"
 

#include <iostream>
#include <string>
extern "C"
{
#include <libavutil/frame.h>
}

using namespace std;

class SDLDisplay
{
public:
	SDLDisplay();
	~SDLDisplay();
	int InitAudio(int out_framesize, int out_sample_rate, int out_channels);
	int CreateSDLWindow(int width, int height, const void* data);
	int ResizeWindow(const void* data);
	int UpdateSDLWindowYUV(AVFrame* frame,int width, int height);
	int UpdateSDLWindowRGB(AVFrame* frame, int width, int height);
	int PlayAudio(int audio_buffersize,  uint8_t** audio_data);
	void CloseSDL();
private:
	

	SDL_Window* screen;
	SDL_Renderer* sdlRenderer ;
	SDL_Texture* sdlTexture ; 
	SDL_mutex* mux ;

	int data_count ;


public:
	Uint32 audio_len; //剩余长度
    uint8_t* pAudio_pos; //当前位置
    Uint8  *audio_chunk;
    int widthTemp;
    int heightTemp;
    const void* hwndTemp;
};

