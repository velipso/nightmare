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

typedef void *(*nm_alloc_func)(size_t sz);
typedef void (*nm_free_func)(void *ptr);

nm_alloc_func nm_alloc = malloc;
nm_free_func nm_free = free;

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

uint32_t read_u32_be(FILE *fp){
	uint32_t c1 = fgetc(fp);
	uint32_t c2 = fgetc(fp);
	uint32_t c3 = fgetc(fp);
	uint32_t c4 = fgetc(fp);
	return (c1 << 24) | (c2 << 16) | (c3 << 8) | c4;
}

uint32_t mem_u16_be(uint8_t *mem){
	uint32_t c1 = mem[0];
	uint32_t c2 = mem[1];
	return (c1 << 8) | c2;
}

uint32_t mem_u32_be(uint8_t *mem){
	uint32_t c1 = mem[0];
	uint32_t c2 = mem[1];
	uint32_t c3 = mem[2];
	uint32_t c4 = mem[3];
	return (c1 << 24) | (c2 << 16) | (c3 << 8) | c4;
}

typedef struct {
	uint32_t type;
	uint32_t size;
	uint8_t *data;
} midi_chunk;

typedef enum {
	MTHD_SINGLE,
	MTHD_PARALLEL,
	MTHD_SEQUENCE
} midi_chunk_mthd_format;

typedef struct {
	midi_chunk_mthd_format format;
	int ticks;
} midi_chunk_mthd;

const char *curfile;

bool is_chunk_type(uint32_t type){
	bool isch =
		type == 0x4D546864 || // MThd
		type == 0x4D54726B || // MTrk
		type == 0x58464948 || // XFIH  XF Information Header
		type == 0x58464B4D || // XFKM  XF Karaoke Message
		false;
	if (!isch && false){
		uint8_t c1 = type & 0xFF;
		uint8_t c2 = (type >> 8) & 0xFF;
		uint8_t c3 = (type >> 16) & 0xFF;
		uint8_t c4 = type >> 24;
		if (c1 >= 0x20 && c1 <= 0x7F &&
			c2 >= 0x20 && c2 <= 0x7F &&
			c3 >= 0x20 && c3 <= 0x7F &&
			c4 >= 0x20 && c4 <= 0x7F)
			printf("Unknown chunk: '%c%c%c%c' %s\n", c4, c3, c2, c1, curfile);
	}
	return isch;
}

bool is_repeat_byte(uint32_t type){
	uint8_t c1 = type & 0xFF;
	uint8_t c2 = (type >> 8) & 0xFF;
	uint8_t c3 = (type >> 16) & 0xFF;
	uint8_t c4 = type >> 24;
	return c1 == c2 && c1 == c3 && c1 == c4;
}

int aligned_chunks = 0;
int misaligned_chunks = 0;
int bad_chunks = 0;
int garbage_end = 0;
int all_files = 0;
int mid_mthd = 0;
int mtrk_chunks = 0;
int64_t total_size = 0;

bool read_midi_chunk(FILE *fp, midi_chunk *chk){
	// first, just try to read the chunk immediately
	uint32_t type = read_u32_be(fp);
	if (is_chunk_type(type))
		aligned_chunks++;
	else{
		// search around for the chunk

		// chop off any repeated bytes
		while (is_repeat_byte(type) && !feof(fp))
			type = read_u32_be(fp);
		if (feof(fp))
			return false;

		// we can move 11 bytes backwards safely
		// -4 to unread the type, then -7 is still safe because the minimum chunk size is 8 bytes
		// and we don't want to be stuck in an infinite loop
		long start = ftell(fp);
		if (start < 11)
			return false;
		start -= 11;
		fseek(fp, start, SEEK_SET);
		uint8_t typewin[32] = {0};
		fread(typewin, 1, 32, fp);
		bool found = false;
		for (int i = 0; i < 28; i++){
			type = mem_u32_be(&typewin[i]);
			if (is_chunk_type(type)){
				misaligned_chunks++;
				found = true;
				fseek(fp, start + i + 4, SEEK_SET);
				break;
			}
		}
		if (!found){
			bad_chunks++;
			return false;
		}
	}

	uint32_t size = read_u32_be(fp);
	if (size > 1000000 || feof(fp))
		size = 0;
	uint8_t *data = NULL;
	if (size > 0){
		data = nm_alloc(sizeof(uint8_t) * size);
		if (data == NULL)
			return false;
		size = fread(data, 1, size, fp);
	}
	chk->type = type;
	chk->size = size;
	chk->data = data;
	return true;
}

bool process_midi_mthd(midi_chunk chk, midi_chunk_mthd *mthd){
	if (chk.size < 6){
		fprintf(stderr, "MThd chunk size too small: %s\n", curfile);
		return false;
	}
	int format = ((int)chk.data[0] << 8) | chk.data[1];
	int division = ((int)chk.data[4] << 8) | chk.data[5];
	if (format != 0 && format != 1 && format != 2){
		fprintf(stderr, "Bad header format %d: %s\n", format, curfile);
		return false;
	}
	if (division & 0x8000){
		fprintf(stderr, "Unsupported time division using SMPTE timings: %s\n", curfile);
		return false;
	}
	mthd->format = format == 0 ? MTHD_SINGLE : (format == 1 ? MTHD_PARALLEL : MTHD_SEQUENCE);
	mthd->ticks = division;
	return true;
}

bool process_midi_mtrk(midi_chunk chk){
	double tempo = 120;
	int timesig_num = 4;
	int timesig_den = 4;
	int pos = 0;
	while (pos < chk.size){
		// read delta as variable int
		int dt = 0;
		int len = 0;
		while (true){
			len++;
			int t = chk.data[pos++];
			if (t & 0x80){
				if (pos >= chk.size)
					return false;
				dt = (dt << 7) | (t & 0x7F);
			}
			else{
				dt = (dt << 7) | t;
				break;
			}
		}
		if (len > 4){
			printf("bad time code\n");
			return false;
		}
		printf("%4d | ", dt);
		if (pos >= chk.size)
			return false;
		int msg = chk.data[pos++];
		if (msg >= 0x80 && msg < 0x90){ // Note-Off
			if (pos + 1 >= chk.size){
				printf("bad note off message, out of data\n");
				return false;
			}
			int note = chk.data[pos++];
			int vel = chk.data[pos++];
			if (note >= 0x80)
				printf("bad note off message, note is too high\n");
			if (vel >= 0x80)
				printf("bad note off message, vel is too high\n");
			printf("%% [%X] note off %02X %02X\n", msg & 0xF, note, vel);
		}
		else if (msg >= 0x90 && msg < 0xA0){ // Note-On
			if (pos + 1 >= chk.size){
				printf("bad note on message, out of data\n");
				return false;
			}
			int note = chk.data[pos++];
			int vel = chk.data[pos++];
			if (note >= 0x80)
				printf("bad note on message, note is too high\n");
			if (vel >= 0x80)
				printf("bad note on message, vel is too high\n");
			printf("%% [%X] note on  %02X %02X\n", msg & 0xF, note, vel);
		}
		else if (msg >= 0xA0 && msg < 0xB0){ // Poly Key Pressure
			if (pos + 1 >= chk.size){
				printf("bad poly key message, out of data\n");
				return false;
			}
			int p1 = chk.data[pos++];
			int p2 = chk.data[pos++];
			if (p1 >= 0x80)
				printf("bad poly key message, p1 too high\n");
			if (p2 >= 0x80)
				printf("bad poly key message, p2 too high\n");
			printf("%% [%X] poly key %02X %02X\n", msg & 0xF, p1, p2);
		}
		else if (msg >= 0xB0 && msg < 0xC0){ // Control Change
			if (pos + 1 >= chk.size){
				printf("bad control message, out of data\n");
				return false;
			}
			int p1 = chk.data[pos++];
			int p2 = chk.data[pos++];
			if (p1 >= 0x80)
				printf("bad control message, p1 too high\n");
			if (p2 >= 0x80)
				printf("bad control message, p2 too high\n");
			printf("%% [%X] control  %02X %02X\n", msg & 0xF, p1, p2);
		}
		else if (msg >= 0xC0 && msg < 0xD0){ // Program Change
			if (pos >= chk.size){
				printf("bad program message, out of data\n");
				return false;
			}
			int patch = chk.data[pos++];
			if (patch >= 0x80)
				printf("bad program message, patch is too high\n");
			printf("%% [%X] program  %02X\n", msg & 0xF, patch);
		}
		else if (msg >= 0xD0 && msg < 0xE0){ // Channel Pressure
			if (pos >= chk.size){
				printf("bad pressure message, out of data\n");
				return false;
			}
			int p = chk.data[pos++];
			if (p >= 0x80)
				printf("bad pressure message, p is too high\n");
			printf("%% [%X] pressure %02X\n", msg & 0xF, p);
		}
		else if (msg >= 0xE0 && msg < 0xF0){ // Pitch Bend
			if (pos + 1 >= chk.size){
				printf("bad pitch b message, out of data\n");
				return false;
			}
			int p1 = chk.data[pos++];
			int p2 = chk.data[pos++];
			if (p1 >= 0x80)
				printf("bad pitch b message, p1 too high\n");
			if (p2 >= 0x80)
				printf("bad pitch b message, p2 too high\n");
			printf("%% [%X] pitch b  %02X %02X\n", msg & 0xF, p1, p2);
		}
		else
			printf("unknown message: %02X\n", msg);
	}
	return true;
}

void process_midi(const char *file){
	FILE *fp = fopen(file, "rb");
	if (fp == NULL){
		fprintf(stderr, "Failed to read file: %s\n", file);
		exit(1);
		return;
	}

	curfile = &file[32];

	fseek(fp, 0L, SEEK_END);
	long filesize = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	midi_chunk chk;
	midi_chunk_mthd mthd;
	if (!read_midi_chunk(fp, &chk)){
		fclose(fp);
		return;
	}
	if (chk.type != 0x4D546864) // MThd
		printf("MThd isn't always first chunk: %s\n", curfile);
	else{
		if (!process_midi_mthd(chk, &mthd)){
			nm_free(chk.data);
			fclose(fp);
			return;
		}
	}
	nm_free(chk.data);

	//
	// at this point, assume we are dealing with a genuine MIDI file
	//

	all_files++;
	total_size += filesize;

	int found_tracks = 0;
	while (true){
		int testc = fgetc(fp);
		if (testc == EOF)
			break;
		ungetc(testc, fp);
		if (!read_midi_chunk(fp, &chk)){
			garbage_end++;
			break;
		}
		if (chk.type == 0x4D546864){
			mid_mthd++;
			midi_chunk_mthd h2;
			process_midi_mthd(chk, &h2);
		}
		else if (chk.type == 0x4D54726B){
			mtrk_chunks++;
			process_midi_mtrk(chk);
		}
		else
			printf("Not processing chunk type %08X: %s\n", chk.type, curfile);
		nm_free(chk.data);
	}
	fclose(fp);
}

int main(){
	//SDL_Init(SDL_INIT_AUDIO);
	//nm_test();
	//SDL_Quit();

	const char *root = "/Users/sean/Downloads/midi/data";
	//fs_readdir(root, each_midi);
	//process_midi("/Users/sean/Downloads/midi/data/f/For_Those_About_To_Rock.MID");
	//process_midi("/Users/sean/Downloads/midi/data/c/cantina13.mid");
	//process_midi("/Users/sean/Downloads/midi/data/q/qfg4open.mid");
	process_midi("/Users/sean/Downloads/midi/data/m/mario.mid");

	printf(
		"-----------------------\n"
		"MIDI Files       : %d\n"
		"Total File Size  : %g GB\n"
		"All chunks       : %d\n"
		"Misaligned       : %d\n"
		"Garbage end      : %d\n"
		"MThd in middle   : %d\n",
		all_files,
		(double)total_size / (1024.0 * 1024.0 * 1024.0),
		aligned_chunks + misaligned_chunks,
		misaligned_chunks,
		garbage_end,
		mid_mthd);

	return 0;
}
