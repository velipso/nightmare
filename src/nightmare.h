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

typedef enum {
	NM_NOTEOFF,
	NM_NOTEON,
	NM_NOTEPRESSURE,
	NM_CONTROLCHANGE,
	NM_PROGRAMCHANGE,
	NM_CHANNELPRESSURE,
	NM_PITCHBEND
} nm_event_type;

typedef struct nm_event_struct nm_event_st, *nm_event;
struct nm_event_struct {
	nm_event_st *next;
	nm_event_type type;
	uint64_t tick;
	union {
		struct { int channel; int note; int velocity; } noteoff;
		struct { int channel; int note; int velocity; } noteon;
		struct { int channel; int note; int pressure; } notepressure;
		struct { int channel; int control; int value; } controlchange;
		struct { int channel; int patch;              } programchange;
		struct { int channel; int pressure;           } channelpressure;
		struct { int channel; int bend;               } pitchbend;
	} u;
};

typedef struct {
	nm_event_st *events;
} nm_track_st, *nm_track;

typedef struct {
	nm_track_st *tracks;
	int track_count;
	int max_channels;
} nm_midi_st, *nm_midi;

typedef struct {
	int tempo;
	int ticks_per_q;
	int timesig_num;
	int timesig_den;
	uint64_t ticks;
} nm_ctx_st, *nm_ctx;

typedef void (*nm_midi_warn_func)(const char *warning, void *user);

nm_midi nm_midi_newfile(const char *file, nm_midi_warn_func f_warn, void *warnuser);
nm_midi nm_midi_newbuffer(uint64_t size, uint8_t *data, nm_midi_warn_func f_warn, void *warnuser);
void    nm_midi_free(nm_midi midi);

#endif // NIGHTMARE__H
