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

const int sample_buffer_size = 4096;

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
	for (int t = 0; t < midi->track_count; t++){
		printf("Track %d\n", t + 1);
		nm_event ev = midi->tracks[t];
		while (ev){
			if (ev->type == NM_NOTEON)
				printf("%08llX Note On  %02X %g\n", ev->tick, ev->u.noteon.note, ev->u.noteon.velocity);
			else if (ev->type == NM_NOTEOFF)
				printf("%08llX Note Off %02X %g\n", ev->tick, ev->u.noteoff.note, ev->u.noteoff.velocity);
			else
				printf("%08llX %s\n", ev->tick, nm_event_type_str(ev->type));
			ev = ev->next;
		}
	}
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

	//fs_readdir("/Users/sean/Downloads/midi/data", each_midi);
	//process_midi("/Users/sean/Downloads/midi/data/f/For_Those_About_To_Rock.MID"); // largest file
	//process_midi("/Users/sean/Downloads/midi/data/c/cantina13.mid");
	//process_midi("/Users/sean/Downloads/midi/data/q/qfg4open.mid");
	//process_midi("/Users/sean/Downloads/midi/data/m/mario.mid");
	//process_midi("/Users/sean/Downloads/midi/data/0/010Ratanakosin.mid");
	//process_midi("/Users/sean/Downloads/midi/data/0/001.mid");
	//process_midi("/Users/sean/Downloads/midi/data/0/00warcraft2.mid");
	//process_midi("/Users/sean/Downloads/midi/data/8/82-01-medley.mid");
	//process_midi("/Users/sean/Downloads/midi/data/z/Zelda3ocarina.mid"); // very small and simple

	// "/Users/sean/Downloads/midi/data/f/For_Those_About_To_Rock.MID"
	//     has an 0-size MTrk which is followed by track data that goes until EOF

	//printf("warnings from %d / %d (%g)\n", tot_warn, tot_all,
	//	(double)tot_warn * 100.0 / (double)tot_all);

	//const char *file = "/Users/sean/Downloads/midi/data/z/Zelda3ocarina.mid";
	const char *file = "/Users/sean/Downloads/midi/data/s/santa_monica.mid";
	nm_midi midi = nm_midi_newfile(file, midi_warn, NULL);
	if (midi == NULL){
		fprintf(stderr, "Failed to load MIDI file\n");
		return 1;
	}
	nm_ctx ctx = nm_ctx_new(midi, 0, 48000);
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
