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
	//printf("* start of mtrk\n");
	int running_status = -1;
	while (pos < chk.size){
		// read delta as variable int
		int dt = 0;
		int len = 0;
		while (true){
			len++;
			int t = chk.data[pos++];
			if (t & 0x80){
				if (pos >= chk.size){
					printf("\nbad time code\n");
					return false;
				}
				dt = (dt << 7) | (t & 0x7F);
			}
			else{
				dt = (dt << 7) | t;
				break;
			}
		}
		if (len > 4){
			printf("\nbad time code\n");
			return false;
		}
		//printf("|%4d | ", dt);
		if (pos >= chk.size){
			printf("\nmissing message\n");
			return false;
		}
		int msg = chk.data[pos++];
		if (msg < 0x80){
			// use running status
			if (running_status < 0){
				printf("\nexpecting running status %02X: %s\n", msg, curfile);
				return false;
			}
			else{
				msg = running_status;
				pos--;
			}
		}
		//printf("%4d %02X\n", dt, msg);
		if (msg >= 0x80 && msg < 0x90){ // Note-Off
			if (pos + 1 >= chk.size){
				printf("\nbad note off message, out of data\n");
				return false;
			}
			running_status = msg;
			int note = chk.data[pos++];
			int vel = chk.data[pos++];
			if (note >= 0x80)
				printf("bad note off message, note is too high\n");
			if (vel >= 0x80)
				printf("\nbad note off message, vel is too high\n");
			//printf("%% [%X] note off %02X %02X\n", msg & 0xF, note, vel);
		}
		else if (msg >= 0x90 && msg < 0xA0){ // Note-On
			if (pos + 1 >= chk.size){
				printf("\nbad note on message, out of data\n");
				return false;
			}
			running_status = msg;
			int note = chk.data[pos++];
			int vel = chk.data[pos++];
			if (note >= 0x80)
				printf("\nbad note on message, note is too high\n");
			if (vel >= 0x80)
				printf("\nbad note on message, vel is too high\n");
			//printf("%% [%X] note on  %02X %02X\n", msg & 0xF, note, vel);
		}
		else if (msg >= 0xA0 && msg < 0xB0){ // Poly Key Pressure
			if (pos + 1 >= chk.size){
				printf("bad poly key message, out of data\n");
				return false;
			}
			running_status = msg;
			int p1 = chk.data[pos++];
			int p2 = chk.data[pos++];
			if (p1 >= 0x80)
				printf("\nbad poly key message, p1 too high\n");
			if (p2 >= 0x80)
				printf("\nbad poly key message, p2 too high\n");
			//printf("%% [%X] poly key %02X %02X\n", msg & 0xF, p1, p2);
		}
		else if (msg >= 0xB0 && msg < 0xC0){ // Control Change
			if (pos + 1 >= chk.size){
				printf("bad control message, out of data\n");
				return false;
			}
			running_status = msg;
			int p1 = chk.data[pos++];
			int p2 = chk.data[pos++];
			if (p1 >= 0x80)
				printf("\nbad control message, p1 too high\n");
			if (p2 >= 0x80)
				printf("\nbad control message, p2 too high\n");
			//printf("%% [%X] control  %02X %02X\n", msg & 0xF, p1, p2);
		}
		else if (msg >= 0xC0 && msg < 0xD0){ // Program Change
			if (pos >= chk.size){
				printf("\nbad program message, out of data\n");
				return false;
			}
			running_status = msg;
			int patch = chk.data[pos++];
			if (patch >= 0x80)
				printf("\n[%08X] bad program message, patch is too high %02X\n", pos, patch);
			//printf("%% [%X] program  %02X\n", msg & 0xF, patch);
		}
		else if (msg >= 0xD0 && msg < 0xE0){ // Channel Pressure
			if (pos >= chk.size){
				printf("\nbad pressure message, out of data\n");
				return false;
			}
			running_status = msg;
			int p = chk.data[pos++];
			if (p >= 0x80)
				printf("\nbad pressure message, p is too high\n");
			//printf("%% [%X] pressure %02X\n", msg & 0xF, p);
		}
		else if (msg >= 0xE0 && msg < 0xF0){ // Pitch Bend
			if (pos + 1 >= chk.size){
				printf("\nbad pitch b message, out of data\n");
				return false;
			}
			running_status = msg;
			int p1 = chk.data[pos++];
			int p2 = chk.data[pos++];
			if (p1 >= 0x80)
				printf("\nbad pitch b message, p1 too high\n");
			if (p2 >= 0x80)
				printf("\nbad pitch b message, p2 too high\n");
			//printf("%% [%X] pitch b  %02X %02X\n", msg & 0xF, p1, p2);
		}
		else if (msg == 0xF0 || msg == 0xF7){ // SysEx Event
			// read length as a variable int
			if (pos >= chk.size){
				printf("\nbad sysex data length\n");
				return false;
			}
			int dl = 0;
			int len = 0;
			while (true){
				len++;
				int t = chk.data[pos++];
				if (t & 0x80){
					if (pos >= chk.size){
						printf("\nbad sysex data length\n");
						return false;
					}
					dl = (dl << 7) | (t & 0x7F);
				}
				else{
					dl = (dl << 7) | t;
					break;
				}
			}
			if (len > 4){
				printf("\nbad sysex data length\n");
				return false;
			}
			if (pos + dl > chk.size){
				printf("\nbad sysex data length, len too large\n");
				return false;
			}
			pos += dl;
		}
		else if (msg == 0xFF){ // Meta Event
			if (pos + 1 >= chk.size){
				printf("\nbad meta event, out of data\n");
				return false;
			}
			int type = chk.data[pos++];
			int len = chk.data[pos++];
			if (pos + len > chk.size){
				printf("\nbad meta event, len too large\n");
				return false;
			}
			switch (type){
				case 0x00: // 02 SSSS           Sequence Number
				case 0x01: // LL text           Generic Text
				case 0x02: // LL text           Copyright Notice
				case 0x03: // LL text           Track Name
				case 0x04: // LL text           Instrument Name
				case 0x05: // LL text           Lyric
				case 0x06: // LL text           Marker
				case 0x07: // LL text           Cue Point
				case 0x20: // 01 NN             Channel Prefix
				case 0x21: // 01 PP             MIDI Port
					break;
				case 0x2F: // 00                End of Track
					if (pos < chk.size)
						printf("\nextra data at end of track: %d bytes\n", chk.size - pos);
					return true;
				case 0x51: // 03 TT TT TT       Set Tempo
				case 0x54: // 05 HH MM SS RR TT SMPTE Offset
				case 0x58: // 04 NN MM LL TT    Time Signature
				case 0x59: // 02 SS MM          Key Signature
				case 0x7F: // LL data           Sequencer-Specific Meta Event
					//printf("%% meta %02X\n", type);
					break;
				default:
					printf("\nunknown meta event %02X: %s\n", type, curfile);
					break;
			}
			pos += len;
		}
		else
			printf("\nunknown message %02X: %s\n", msg, curfile);
	}
	//printf("*\n");
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
	printf("%s\n", curfile);

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
			if (!process_midi_mtrk(chk)){
				printf("file failed: %s\n", curfile);
				exit(1);
				return;
			}
		}
		else
			printf("Not processing chunk type %08X: %s\n", chk.type, curfile);
		nm_free(chk.data);
	}
	fclose(fp);
}

void midi_warn(const char *msg, void *user){
	printf("> %s\n", msg);
}

int main(int argc, char **argv){
	//SDL_Init(SDL_INIT_AUDIO);
	//nm_test();
	//SDL_Quit();

	const char *root = "/Users/sean/Downloads/midi/data";
	//fs_readdir(root, each_midi);
	//process_midi("/Users/sean/Downloads/midi/data/f/For_Those_About_To_Rock.MID");
	//process_midi("/Users/sean/Downloads/midi/data/c/cantina13.mid");
	//process_midi("/Users/sean/Downloads/midi/data/q/qfg4open.mid");
	//process_midi("/Users/sean/Downloads/midi/data/m/mario.mid");
	//process_midi("/Users/sean/Downloads/midi/data/0/010Ratanakosin.mid");
	//process_midi("/Users/sean/Downloads/midi/data/0/001.mid");
	//process_midi("/Users/sean/Downloads/midi/data/0/00warcraft2.mid");

	// "/Users/sean/Downloads/midi/data/f/For_Those_About_To_Rock.MID"
	//     has an 0-size MTrk which is followed by track data that goes until EOF

	nm_midi midi = nm_midi_newfile("/Users/sean/Downloads/midi/data/c/cantina13.mid",
		midi_warn, NULL);
	nm_midi_free(midi);

	return 0;
}
