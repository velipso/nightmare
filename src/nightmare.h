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
	NM_PITCHBEND,
	NM_TEMPO
} nm_event_type;

typedef struct nm_event_struct nm_event_st, *nm_event;
struct nm_event_struct {
	nm_event_st *next;
	nm_event_type type;
	uint64_t tick;
	union {
		struct { int channel; int note; float velocity; } noteoff;
		struct { int channel; int note; float velocity; } noteon;
		struct { int channel; int note; int pressure; } notepressure;
		struct { int channel; int control; int value; } controlchange;
		struct { int channel; int patch;              } programchange;
		struct { int channel; int pressure;           } channelpressure;
		struct { int channel; int bend;               } pitchbend;
		struct { int tempo;                           } tempo;
		struct { int num; int den; int
	} u;
};

typedef struct {
	nm_event *tracks;
	int track_count;
	int ticks_per_q;
	int max_channels;
} nm_midi_st, *nm_midi;

typedef struct {
	int note;
	double freq;
	float hit_velocity;
	float hold_pressure;
	float release_velocity;
	bool pressed;
} nm_note_st;

typedef struct {
	nm_note_st notes[128];
	float pressure;
	float pitch_bend;
} nm_channel_st;

typedef struct {
	nm_midi midi;
	uint64_t ticks;
	double ticks_per_sec;
	nm_channel_st *chans;
	int samples_per_sec;
	bool ignore_timesig;
} nm_ctx_st, *nm_ctx;

typedef void (*nm_midi_warn_func)(const char *warning, void *user);

typedef struct {
	float L;
	float R;
} nm_sample_st;

nm_midi nm_midi_newfile(const char *file, nm_midi_warn_func f_warn, void *warnuser);
nm_midi nm_midi_newbuffer(uint64_t size, uint8_t *data, nm_midi_warn_func f_warn, void *warnuser);
nm_ctx  nm_ctx_new(nm_midi midi, int samples_per_sec);
int     nm_ctx_process(nm_ctx ctx, int sample_len, nm_sample_st *samples);
void    nm_ctx_free(nm_ctx ctx);
void    nm_midi_free(nm_midi midi);

#endif // NIGHTMARE__H
