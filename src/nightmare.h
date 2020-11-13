// (c) Copyright 2020, Sean Connelly (@velipso), sean.cm
// MIT License
// Project Home: https://github.com/velipso/nightmare

#ifndef NIGHTMARE__H
#define NIGHTMARE__H

#ifndef NM_CLIP_MAX
#define NM_CLIP_MAX      200
#endif

#ifndef NM_NOTES_MAX
#define NM_NOTES_MAX     100
#endif

#ifndef NM_CHANNELS_MAX
#define NM_CHANNELS_MAX  6
#endif

#ifndef NM_BARS_MAX
#define NM_BARS_MAX      100
#endif

#ifndef NM_AVOICES_MAX
#define NM_AVOICES_MAX   (16 + NM_CHANNELS_MAX * 8)
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// k-block size, i.e., number of samples in a rendered block
// do not change or you'll screw everything up :-)
#define NM_K 200

typedef struct {
	float L;
	float R;
} nm_sample_st;

typedef enum {
	NM_VT_POLY,
	NM_VT_MONO,
	NM_VT_SAMPLE
} nm_vtype;

typedef enum {
	NM_VC_BASS,
	NM_VC_BRASS,
	NM_VC_DRUMS,
	NM_VC_KEYS,
	NM_VC_LEAD,
	NM_VC_PAD,
	NM_VC_SFX,
	NM_VC_STRING
} nm_vcat;

typedef struct {
	int voice_id;
	nm_vtype vtype;
	nm_vcat vcat;
	const char *name;
	const char *xlabel;
	const char *ylabel;
	const char *samples[15];
	int x;
	int y;
} nm_voice_st;

typedef enum {
	NM_OS_CHROMATICLOW,
	NM_OS_CHROMATICMID,
	NM_OS_CHROMATICHIGH,
	NM_OS_EMAJOR,
	NM_OS_EMINOR,
	NM_OS_EMAJORPENTATONIC,
	NM_OS_EMINORPENTATONIC
} nm_oscscale;

typedef enum {
	NM_SS_1_4X,
	NM_SS_1_3X,
	NM_SS_1_2X,
	NM_SS_1X,
	NM_SS_2X,
	NM_SS_3X,
	NM_SS_4X
} nm_samplespeed;

typedef struct {
	int x1;
	int y1;
	int x2;
	int y2;
	int velocity;
	int hold;
} nm_note_st;

typedef struct {
	nm_sample_st kbuf[NM_K];
	int kbuf_size;
	int tempo; // stored as number of k-blocks before advancing a 1/16th note
	struct {
		int aid;
		int priority;
		int clip_id;
		void *about;
		float x;
		float y;
		float out;
		uint64_t vdata[100];
	} avoices[NM_AVOICES_MAX];
	struct {
		int voice_id;
		int x;
		int y;
		int out;
		union {
			nm_oscscale oscscale;
			nm_samplespeed samplespeed;
		} u;
		nm_note_st notes[NM_NOTES_MAX];
		int notes_size;
		uint64_t cdata[100];
	} clips[NM_CLIP_MAX];
} nm_ctx_st, *nm_ctx;

extern const nm_voice_st *nm_voices[];

// initialize everything (once)
void nm_init();
void nm_clear(nm_ctx nm);
void nm_render(nm_ctx nm, nm_sample_st *out, size_t outsize);

static inline float nm_getbpm(nm_ctx nm){
	return 3600.0f / nm->tempo;
}

// clip
void nm_clip_setvoice(nm_ctx nm, int clip_id, int voice_id);
void nm_clip_setx(nm_ctx nm, int clip_id, int x);
void nm_clip_sety(nm_ctx nm, int clip_id, int y);
void nm_clip_setxy(nm_ctx nm, int clip_id, int x, int y);
void nm_clip_setout(nm_ctx nm, int clip_id, int out);
void nm_clip_setoscscale(nm_ctx nm, int clip_id, nm_oscscale oscscale);
void nm_clip_setsamplespeed(nm_ctx nm, int clip_id, nm_samplespeed samplespeed);
void nm_clip_setnote(nm_ctx nm, int clip_id, int note_id, nm_note_st note);
void nm_clip_noteon(nm_ctx nm, int clip_id, int note, int velocity);
void nm_clip_noteoff(nm_ctx nm, int clip_id, int note, int velocity);

static inline int nm_clip_getvoice(nm_ctx nm, int clip_id){
	return nm->clips[clip_id].voice_id;
}

static inline int nm_clip_getx(nm_ctx nm, int clip_id){
	return nm->clips[clip_id].x;
}

static inline int nm_clip_gety(nm_ctx nm, int clip_id){
	return nm->clips[clip_id].y;
}

static inline int nm_clip_getout(nm_ctx nm, int clip_id){
	return nm->clips[clip_id].out;
}

static inline nm_oscscale nm_clip_getoscscale(nm_ctx nm, int clip_id){
	return nm->clips[clip_id].u.oscscale;
}

static inline nm_samplespeed nm_clip_getsamplespeed(nm_ctx nm, int clip_id){
	return nm->clips[clip_id].u.samplespeed;
}

static inline int nm_clip_getnotessize(nm_ctx nm, int clip_id){
	return nm->clips[clip_id].notes_size;
}

static inline nm_note_st nm_clip_getnote(nm_ctx nm, int clip_id, int note_id){
	return nm->clips[clip_id].notes[note_id];
}

#endif // NIGHTMARE__H
