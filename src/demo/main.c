// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#include "SDL.h"
#include "../nightmare.h"

int main(){
	SDL_Init(SDL_INIT_EVERYTHING);
	nm_test();
	SDL_Quit();
	return 0;
}
