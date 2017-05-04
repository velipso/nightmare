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

void midi_warn(const char *msg, void *user){
	printf("> %s\n", msg);
}

void process_midi(const char *file){
	printf("%s\n", &file[32]);
	nm_midi midi = nm_midi_newfile(file, midi_warn, NULL);
	nm_midi_free(midi);
}

int main(int argc, char **argv){
	//SDL_Init(SDL_INIT_AUDIO);
	//SDL_Quit();

	fs_readdir("/Users/sean/Downloads/midi/data", each_midi);
	//process_midi("/Users/sean/Downloads/midi/data/f/For_Those_About_To_Rock.MID");
	//process_midi("/Users/sean/Downloads/midi/data/c/cantina13.mid");
	//process_midi("/Users/sean/Downloads/midi/data/q/qfg4open.mid");
	//process_midi("/Users/sean/Downloads/midi/data/m/mario.mid");
	//process_midi("/Users/sean/Downloads/midi/data/0/010Ratanakosin.mid");
	//process_midi("/Users/sean/Downloads/midi/data/0/001.mid");
	//process_midi("/Users/sean/Downloads/midi/data/0/00warcraft2.mid");
	//process_midi("/Users/sean/Downloads/midi/data/8/82-01-medley.mid");

	// "/Users/sean/Downloads/midi/data/f/For_Those_About_To_Rock.MID"
	//     has an 0-size MTrk which is followed by track data that goes until EOF

	return 0;
}
