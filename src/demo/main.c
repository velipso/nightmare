// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#include "SDL.h"
#include "../nightmare.h"
#include "../sink_nightmare.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef SINK_WIN32 // TODO: test on win32
#	include <direct.h> // _getcwd
#	define getcwd _getcwd
#else
#	include <unistd.h> // getcwd
#endif

static volatile bool done = false;
#include <signal.h>

static void catchdone(int dummy){
	fclose(stdin);
	done = true;
}

static inline void catchint(){
	signal(SIGINT, catchdone);
	signal(SIGSTOP, catchdone);
}

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

static sink_fstype fs_sinktype(const char *file, void *user){
	if (fs_isdir(file))
		return SINK_FSTYPE_DIR;
	else if (fs_isfile(file))
		return SINK_FSTYPE_FILE;
	return SINK_FSTYPE_NONE;
}

static bool fs_sinkread(sink_scr scr, const char *file, void *user){
	FILE *fp = fopen(file, "rb");
	if (fp == NULL)
		return false; // `false` indicates that the file couldn't be read
	char buf[5000];
	while (!feof(fp)){
		size_t sz = fread(buf, 1, sizeof(buf), fp);
		if (!sink_scr_write(scr, sz, (const uint8_t *)buf))
			break;
	}
	fclose(fp);
	return true; // `true` indicates that the file was read
}

static sink_inc_st inc = {
	.f_fixpath = NULL,
	.f_fstype = fs_sinktype,
	.f_fsread = fs_sinkread,
	.user = NULL
};

static void process_midi(const char *file);
static void each_midi(const char *file){
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
static void midi_warn(const char *msg, void *user){
	did_warn = true;
	printf("> %s\n", msg);
}

int tot_warn = 0;
int tot_all = 0;
static void process_midi(const char *file){
	did_warn = false;
	printf("%s\n", &file[32]);
	// TODO: anything?
	tot_all++;
	if (did_warn)
		tot_warn++;
}

#ifndef NDEBUG
// memory leak detection
static SDL_sem *mem_lock;
typedef struct m_debug_memlist_struct {
	void *p;
	struct m_debug_memlist_struct *next;
} m_debug_memlist_st, *m_debug_memlist;
static m_debug_memlist mem_list = NULL;

static void *db_alloc(size_t size){
	printf("alloc\n");
	void *p = malloc(size);
	m_debug_memlist m = malloc(sizeof(m_debug_memlist_st));
	m->p = p;
	SDL_SemWait(mem_lock);
	m->next = mem_list;
	mem_list = m;
	SDL_SemPost(mem_lock);
	return p;
}

static void *db_realloc(void *p, size_t size){
	printf("realloc\n");
	void *new_p;
	if (p == NULL){
		m_debug_memlist m = malloc(sizeof(m_debug_memlist_st));
		m->p = new_p = malloc(size);
		SDL_SemWait(mem_lock);
		m->next = mem_list;
		mem_list = m;
		SDL_SemPost(mem_lock);
	}
	else{
		SDL_SemWait(mem_lock);
		m_debug_memlist m = mem_list;
		bool found = false;
		while (m){
			if (m->p == p){
				found = true;
				m->p = new_p = realloc(p, size);
				break;
			}
			m = m->next;
		}
		SDL_SemPost(mem_lock);
		if (!found)
			fprintf(stderr, "Reallocated a pointer that wasn't originally allocated\n");
	}
	return new_p;
}

static void db_free(void *p){
	printf("free\n");
	if (p == NULL)
		return;
	SDL_SemWait(mem_lock);
	m_debug_memlist *m = &mem_list;
	bool found = false;
	while (*m){
		if ((*m)->p == p){
			found = true;
			free(p);
			void *f = *m;
			*m = (*m)->next;
			free(f);
			break;
		}
		m = &(*m)->next;
	}
	SDL_SemPost(mem_lock);
	if (!found)
		fprintf(stderr, "Freeing a pointer that wasn't originally allocated\n");
}

static void db_flush(){
	SDL_SemWait(mem_lock);
	m_debug_memlist m = mem_list;
	int count = 0;
	while (m){
		count++;
		void *f = m;
		m = m->next;
		free(f);
	}
	SDL_SemPost(mem_lock);
	if (count == 0)
		fprintf(stderr, "No memory leaks :-)\n");
	else
		fprintf(stderr, "%d memory leak%s\n", count, count == 1 ? "" : "s");
}
#endif

static SDL_sem *lock_write;
static SDL_sem *lock_read;
static SDL_sem *lock_nm;
static nm_sample_st sample_buffer[sample_buffer_size];
static bool render_done = false;
static int run_mode = 0; // 0 = repl, 1 = midi file, 2 = sink file

static bool hassound(nm_ctx ctx){
	SDL_SemWait(lock_nm);
	bool ret = nm_ctx_eventsleft(ctx) > 0 || nm_ctx_voicesleft(ctx) > 0;
	SDL_SemPost(lock_nm);
	return ret;
}

static int sdl_render_audio(void *data){
	nm_ctx ctx = (nm_ctx)data;
	while (true){
		bool hs = hassound(ctx);
		if (run_mode != 0 && !hs)
			break;
		SDL_SemWait(lock_write);
		memset(sample_buffer, 0, sizeof(nm_sample_st) * sample_buffer_size);
		if (hs){
			SDL_SemWait(lock_nm);
			nm_ctx_process(ctx, sample_buffer_size, sample_buffer);
			SDL_SemPost(lock_nm);
			for (int i = 0; i < 128; i++){
				int cnt = ctx->notecnt[i];
				if      (cnt == 0) printf(" ");
				else if (cnt == 1) printf(".");
				else if (cnt == 2) printf(":");
				else if (cnt == 3) printf("&");
				else if (cnt == 4) printf("#");
				else               printf("@");
			}
			printf("%2X\n", nm_ctx_voicesleft(ctx));
		}
		#ifndef NDEBUG
		// crappy click detection
		#if 0
		float pL = sample_buffer[0].L;
		float pR = sample_buffer[0].R;
		for (int i = 1; i < sample_buffer_size; i++){
			float nL = sample_buffer[i].L;
			float nR = sample_buffer[i].R;
			float dL = fabsf(nL - pL);
			float dR = fabsf(nR - pR);
			if (dL > 0.1f || dR > 0.1f)
				printf("click!\n");
			pL = nL;
			pR = nR;
		}
		#endif
		#endif
		SDL_SemPost(lock_read);
	}
	SDL_SemWait(lock_write);
	render_done = true;
	memset(sample_buffer, 0, sizeof(nm_sample_st) * sample_buffer_size);
	SDL_SemPost(lock_read);
	return 0;
}

static void sdl_copy_audio(void *userdata, Uint8* stream, int len){
	if (render_done){
		memset(stream, 0, len);
		return;
	}
	SDL_SemWait(lock_read);
	memcpy(stream, sample_buffer, sizeof(nm_sample_st) * sample_buffer_size);
	SDL_SemPost(lock_write);
}

// used by sink for memory management
void *nms_alloc(size_t sz){ return nm_alloc(sz); }
void *nms_realloc(void *pt, size_t sz){ return nm_realloc(pt, sz); }
void nms_free(void *pt){ nm_free(pt); }

static inline sink_scr getscr(sink_scr_type type){
	char *cwd = getcwd(NULL, 0);
	sink_scr scr = sink_scr_new(inc, cwd, type);
	free(cwd);
	sink_scr_addpath(scr, ".");
	sink_nightmare_scr(scr);
	return scr;
}

static inline sink_ctx getctx(sink_scr scr, nm_ctx nctx){
	sink_ctx sctx = sink_ctx_new(scr, sink_stdio);
	if (sctx == NULL)
		return sctx;
	if (!sink_nightmare_ctx(sctx, nctx)){
		sink_ctx_free(sctx);
		return NULL;
	}
	return sctx;
}

static inline void printline(int line, int level){
	if (line < 10)
		printf(" %d", line);
	else
		printf("%d", line);
	if (level <= 0)
		printf(": ");
	else{
		printf(".");
		for (int i = 0; i < level; i++)
			printf("..");
		printf(" ");
	}
}

static inline void printscrerr(sink_scr scr){
	const char *err = sink_scr_err(scr);
	fprintf(stderr, "%s\n", err ? err : "Error: Unknown");
}

static inline void printctxerr(sink_ctx ctx){
	const char *err = sink_ctx_err(ctx);
	fprintf(stderr, "%s\n", err ? err : "Error: Unknown");
}

static inline int repl(nm_ctx nctx){
	int res = 0;
	sink_scr scr = getscr(SINK_SCR_REPL);
	sink_ctx ctx = getctx(scr, nctx);
	sink_scr_write(scr, 32, (const uint8_t *)"include 'nightmare';using music;");
	int line = 1;
	int bufsize = 0;
	int bufcount = 200;
	char *buf = malloc(sizeof(char) * bufcount);
	if (buf == NULL){
		sink_ctx_free(ctx);
		sink_scr_free(scr);
		fprintf(stderr, "Out of memory!\n");
		return 1;
	}
	catchint();
	printline(line, sink_scr_level(scr));
	while (!done){
		int ch = fgetc(stdin);
		if (ch == EOF){
			ch = '\n';
			done = true;
		}
		if (bufsize >= bufcount - 1){ // make sure there is always room for two chars
			bufcount += 200;
			buf = realloc(buf, sizeof(char) * bufcount);
			if (buf == NULL){
				sink_ctx_free(ctx);
				sink_scr_free(scr);
				fprintf(stderr, "Out of memory!\n");
				return 1;
			}
		}
		buf[bufsize++] = ch;
		if (ch == '\n'){
			if (!sink_scr_write(scr, bufsize, (uint8_t *)buf))
				printscrerr(scr);
			if (sink_scr_level(scr) <= 0){
				SDL_SemWait(lock_nm);
				switch (sink_ctx_run(ctx)){
					case SINK_RUN_PASS:
						done = true;
						res = 0;
						break;
					case SINK_RUN_FAIL:
						printctxerr(ctx);
						break;
					case SINK_RUN_ASYNC:
						fprintf(stderr, "TODO: REPL invoked async function\n");
						done = true;
						break;
					case SINK_RUN_TIMEOUT:
						fprintf(stderr, "REPL returned timeout (impossible)\n");
						done = true;
						break;
					case SINK_RUN_REPLMORE:
						// do nothing
						break;
				}
				SDL_SemPost(lock_nm);
			}
			if (!done){
				while (hassound(nctx))
					SDL_Delay(50);
				printline(++line, sink_scr_level(scr));
			}
			bufsize = 0;
		}
	}
	free(buf);
	sink_ctx_free(ctx);
	sink_scr_free(scr);
	return res;
}

int main(int argc, char **argv){
	// z/Zelda3ocarina.mid              // very small and simple
	// f/For_Those_About_To_Rock.MID    // 0-size MTrk
	// TODO:
	//  pitch bends (coarse and fine grained)
	//  understand control changes
	//  drums
	//  poly mode
	//  omni mode should perform all notes on/off because GM2 doesn't use omni mode
//	each_midi("/Users/sean/Downloads/midi/data");
//	return 0;

	int res = 1;
	nm_ctx nctx = NULL;
	bool sdl_init = false;
	FILE *fp = NULL;

	#ifndef NDEBUG
	sdl_init = true;
	SDL_Init(SDL_INIT_AUDIO);
	printf("Debug Build\n");
	mem_lock = SDL_CreateSemaphore(1);
	nm_alloc = db_alloc;
	nm_realloc = db_realloc;
	nm_free = db_free;
	#endif

	if (argc >= 2){
		// read start of file to see if it's a MIDI file
		fp = fopen(argv[1], "rb");
		if (fp == NULL){
			fprintf(stderr, "Failed to read file: %s\n", argv[1]);
			printf(
				"Usage:\n"
				"  nightmare                 Sink REPL\n"
				"  nightmare file.mid        Play a MIDI file\n"
				"  nightmare file.sink       Execute a sink script\n");
			goto cleanup;
		}
		uint8_t filedata[8] = {0};
		fread(filedata, 1, 8, fp);
		fclose(fp);
		fp = NULL;
		run_mode = nm_ismidi(filedata) ? 1 : 2;
	}

	// create a nightmare context
	nctx = nm_ctx_new(480, 1024, 32, 48000, NULL, 0, 0, NULL, NULL);
	if (nctx == NULL){
		fprintf(stderr, "Out of memory\n");
		goto cleanup;
	}

	if (run_mode == 1){
		if (!nm_midi_newfile(nctx, argv[1], midi_warn, NULL)){
			fprintf(stderr, "Failed to process midi file: %s\n", argv[1]);
			goto cleanup;
		}
	}
	else if (run_mode == 2){
		sink_scr scr = getscr(SINK_SCR_FILE);
		if (scr == NULL){
			fprintf(stderr, "Out of memory\n");
			goto cleanup;
		}

		if (!sink_scr_loadfile(scr, argv[1])){
			printscrerr(scr);
			sink_scr_free(scr);
			goto cleanup;
		}

		sink_ctx sctx = getctx(scr, nctx);
		if (sctx == NULL){
			fprintf(stderr, "Out of memory\n");
			sink_scr_free(scr);
			goto cleanup;
		}

		sink_run res = sink_ctx_run(sctx);
		if (res == SINK_RUN_FAIL){
			printctxerr(sctx);
			sink_ctx_free(sctx);
			sink_scr_free(scr);
			goto cleanup;
		}

		sink_ctx_free(sctx);
		sink_scr_free(scr);
	}

	if (!nm_ctx_bakeall(nctx)){
		fprintf(stderr, "Out of memory\n");
		goto cleanup;
	}

	//nm_ctx_dumpev(nctx);

	#ifdef NDEBUG
	sdl_init = true;
	SDL_Init(SDL_INIT_AUDIO);
	#endif

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
				lock_nm = SDL_CreateSemaphore(1);
				if (lock_nm == NULL)
					fprintf(stderr, "Failed to create semaphore: %s\n", SDL_GetError());
				else{
					SDL_Thread *thr = SDL_CreateThread(sdl_render_audio, "Nightmare Audio", nctx);
					if (thr == NULL)
						fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
					else{
						SDL_PauseAudioDevice(dev, 0); // begin playing
						if (run_mode == 0){
							res = repl(nctx);
							run_mode = 1;
						}
						SDL_WaitThread(thr, NULL);
					}
					SDL_DestroySemaphore(lock_nm);
				}
				SDL_DestroySemaphore(lock_read);
			}
			SDL_DestroySemaphore(lock_write);
		}
		#ifndef NDEBUG
		printf("hit a key to close audio device\n");
		fgetc(stdin);
		#endif
		SDL_CloseAudioDevice(dev);
	}

	cleanup:
	if (fp)
		fclose(fp);
	if (nctx)
		nm_ctx_free(nctx);
	#ifndef NDEBUG
	db_flush();
	SDL_DestroySemaphore(mem_lock);
	#endif
	if (sdl_init)
		SDL_Quit();
	#ifndef NDEBUG
	printf("hit a key to exit\n");
	fgetc(stdin);
	#endif
	return res;
}
