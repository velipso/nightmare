// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#include "SDL.h"
#include "../nightmare.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

const int sample_buffer_size = 1024;

static bool fs_isdir(const char *dir){
	struct stat buf;
	if (stat(dir, &buf) != 0)
		return 0;
	return S_ISDIR(buf.st_mode);
}

static bool fs_isfile(const char *file){
	FILE *fp = fopen(file, "rb");
	if (fp == NULL)
		return false;
	fclose(fp);
	return true;
}

typedef void (*fs_readdir_each_func)(const char *file);
static void fs_readdir(const char *dir, fs_readdir_each_func f_each){
	DIR *dp;
	struct dirent *ep;
	dp = opendir(dir);
	int rslen = strlen(dir);
	if (rslen >= 4000)
		return;
	char full[4001];
	strcpy(full, dir);
	if (dp != NULL){
		while (true){
			ep = readdir(dp);
			if (ep == NULL)
				break;
			if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
				continue;
			int dlen = strlen(ep->d_name);
			if (rslen + dlen + 1 >= 4000)
				continue;
			full[rslen] = '/';
			strcpy(&full[rslen + 1], ep->d_name);
			f_each(full);
		}
		closedir(dp);
	}
}

void process_midi(const char *file);
void each_midi(const char *file){
	if (fs_isdir(file)){
		fs_readdir(file, each_midi);
		return;
	}
	int len = strlen(file);
	if (strcmp(&file[len - 4], ".mid") == 0 ||
		strcmp(&file[len - 4], ".MID") == 0 ||
		strcmp(&file[len - 4], ".Mid") == 0)
		process_midi(file);
}

bool did_warn;
void midi_warn(const char *msg, void *user){
	did_warn = true;
	printf("> %s\n", msg);
}

int tot_warn = 0;
int tot_all = 0;
void process_midi(const char *file){
	did_warn = false;
	printf("%s\n", &file[32]);
	nm_midi midi = nm_midi_newfile(file, midi_warn, NULL);
	if (midi)
		nm_midi_free(midi);
	tot_all++;
	if (did_warn)
		tot_warn++;
}

#ifndef NDEBUG
static void *db_alloc(size_t sz){
	void *ptr = malloc(sz);
	printf("alloc: %p (%zu)\n", ptr, sz);
	return ptr;
}

static void *db_realloc(void *ptr, size_t sz){
	void *np = realloc(ptr, sz);
	printf("realloc: %p -> %p (%zu)\n", ptr, np, sz);
	return np;
}

static void db_free(void *ptr){
	free(ptr);
	printf("free: %p\n", ptr);
}
#endif

SDL_sem *lock_write;
SDL_sem *lock_read;
nm_sample_st sample_buffer[sample_buffer_size];
bool render_done = false;

int sdl_render_audio(void *data){
	nm_ctx ctx = (nm_ctx)data;
	while (true){
		SDL_SemWait(lock_write);
		int sz = nm_ctx_process(ctx, sample_buffer_size, sample_buffer);
		#ifndef NDEBUG
		// crappy click detection
		if (sz > 0){
			float pL = sample_buffer[0].L;
			float pR = sample_buffer[0].R;
			for (int i = 1; i < sz; i++){
				float nL = sample_buffer[i].L;
				float nR = sample_buffer[i].R;
				float dL = fabsf(nL - pL);
				float dR = fabsf(nR - pR);
				if (dL > 0.1f || dR > 0.1f)
					printf("click!\n");
				pL = nL;
				pR = nR;
			}
		}
		#endif
		if (sz < sample_buffer_size){
			memset(&sample_buffer[sz], 0, sizeof(nm_sample_st) * (sample_buffer_size - sz));
			SDL_SemPost(lock_read);
			break;
		}
		else
			SDL_SemPost(lock_read);
	}
	SDL_SemWait(lock_write);
	render_done = true;
	memset(sample_buffer, 0, sizeof(nm_sample_st) * sample_buffer_size);
	SDL_SemPost(lock_read);
	return 0;
}

void sdl_copy_audio(void *userdata, Uint8* stream, int len){
	if (render_done){
		memset(stream, 0, len);
		return;
	}
	SDL_SemWait(lock_read);
	memcpy(stream, sample_buffer, sizeof(nm_sample_st) * sample_buffer_size);
	SDL_SemPost(lock_write);
}

int main(int argc, char **argv){
	#ifndef NDEBUG
	printf("Debug Build\n");
	nm_alloc = db_alloc;
	nm_realloc = db_realloc;
	nm_free = db_free;
	#endif

//	each_midi("/Users/sean/Downloads/midi/data");
//	return 0;

	if (argc < 2){
		printf("Usage: ./nightmare file.mid\n");
		return 0;
	}
//	process_midi(argv[1]);
//	return 0;

	// z/Zelda3ocarina.mid              // very small and simple
	// f/For_Those_About_To_Rock.MID    // 0-size MTrk

	// TODO:
	//  voice based generation
	//  pitch bends (coarse and fine grained)
	//  understand control changes
	//  synth plugins
	//  drums
	//  poly mode
	//  omni mode should perform all notes on/off because GM2 doesn't use omni mode

	nm_midi midi = nm_midi_newfile(argv[1], midi_warn, NULL);
	if (midi == NULL){
		fprintf(stderr, "Failed to load MIDI file: %s\n", argv[1]);
		return 1;
	}
	nm_ctx ctx = nm_ctx_new(midi, 0, 32, 48000);
	if (ctx == NULL){
		fprintf(stderr, "Out of memory\n");
		return 1;
	}
	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec want, have;
	SDL_AudioDeviceID dev;
	SDL_memset(&want, 0, sizeof(want));
	want.freq = 48000;
	want.format = AUDIO_F32;
	want.channels = 2;
	want.samples = sample_buffer_size;
	want.callback = sdl_copy_audio;
	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (dev == 0)
		fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
	else{
		lock_write = SDL_CreateSemaphore(1);
		if (lock_write == NULL)
			fprintf(stderr, "Failed to create semaphore: %s\n", SDL_GetError());
		else{
			lock_read = SDL_CreateSemaphore(0);
			if (lock_read == NULL)
				fprintf(stderr, "Failed to create semaphore: %s\n", SDL_GetError());
			else{
				SDL_Thread *thr = SDL_CreateThread(sdl_render_audio, "Nightmare Audio", ctx);
				if (thr == NULL)
					fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
				else{
					SDL_PauseAudioDevice(dev, 0); // begin playing
					SDL_WaitThread(thr, NULL);
				}
				SDL_DestroySemaphore(lock_read);
			}
			SDL_DestroySemaphore(lock_write);
		}
		SDL_CloseAudioDevice(dev);
	}
	SDL_Quit();
	nm_ctx_free(ctx);
	nm_midi_free(midi);
	return 0;
}
