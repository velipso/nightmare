// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#ifndef NIGHTMARE__H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef void *(*nm_alloc_func)(size_t size);
typedef void (*nm_free_func)(void *ptr);
extern nm_alloc_func nm_alloc;
extern nm_free_func nm_free;

typedef struct {
} nm_midi_st, *nm_midi;

typedef void (*nm_midi_warn_func)(const char *warning, void *user);

nm_midi nm_midi_newfile(const char *file, nm_midi_warn_func f_warn, void *warnuser);
nm_midi nm_midi_newbuffer(uint64_t size, uint8_t *data, nm_midi_warn_func f_warn, void *warnuser);
void    nm_midi_free(nm_midi midi);

#endif // NIGHTMARE__H
