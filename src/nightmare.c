// (c) Copyright 2020, Sean Connelly (@velipso), sean.cm
// MIT License
// Project Home: https://github.com/velipso/nightmare

#include "nightmare.h"
#include <math.h>
#include <string.h>
#include <opusfile.h>

nm_malloc_f  nm_malloc  = malloc;
nm_realloc_f nm_realloc = realloc;
nm_free_f    nm_free    = free;

static const float TAU = 6.283185307179586476925286766559005768394338798750211641949f;
static const int K_BLOCK_SIZE = 200;

typedef struct {
	size_t vuser_size;
	size_t buser_size;
	nm_voice_st voice;
} vabout_st;

//
// UTILITY
//

static inline int clampi(int v, int min, int max){
	return v < min ? min : (v > max ? max : v);
}

static inline float clampf(float v, float min, float max){
	return v < min ? min : (v > max ? max : v);
}

static inline int signi(int v){
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

static inline int absi(int v){
	return v < 0 ? -v : v;
}

static inline float absf(float v){
	return fabs(v);
}

static inline int maxi(int a, int b){
	return a > b ? a : b;
}

static inline float maxf(float a, float b){
	return a > b ? a : b;
}

static inline int mini(int a, int b){
	return a > b ? b : a;
}

static inline float minf(float a, float b){
	return a > b ? b : a;
}

//
// BIQUAD FILTER
//

typedef struct {
	float b0;
	float b1;
	float b2;
	float a1;
	float a2;
	nm_sample_st xn1;
	nm_sample_st xn2;
	nm_sample_st yn1;
	nm_sample_st yn2;
} biquad_st;

static inline void biquad_reset(biquad_st *bq){
	bq->xn1 = (nm_sample_st){ 0, 0 };
	bq->xn2 = (nm_sample_st){ 0, 0 };
	bq->yn1 = (nm_sample_st){ 0, 0 };
	bq->yn2 = (nm_sample_st){ 0, 0 };
}

static inline void biquad_scale(biquad_st *bq, float amt){
	bq->b0 = amt;
	bq->b1 = 0;
	bq->b2 = 0;
	bq->a1 = 0;
	bq->a2 = 0;
}

static inline void biquad_lowpass(biquad_st *bq, float freq, float Q){
	freq /= 24000;
	if (freq >= 1.0f)
		biquad_scale(bq, 1);
	else if (freq <= 0.0f)
		biquad_scale(bq, 0);
	else{
		Q = powf(10.0f, Q * 0.05f); // convert Q from dB to linear
		float theta = TAU * freq;
		float alpha = sinf(theta) / (2.0f * Q);
		float cosw  = cosf(theta);
		float beta  = (1.0f - cosw) * 0.5f;
		float a0inv = 1.0f / (1.0f + alpha);
		bq->b0 = a0inv * beta;
		bq->b1 = a0inv * 2.0f * beta;
		bq->b2 = a0inv * beta;
		bq->a1 = a0inv * -2.0f * cosw;
		bq->a2 = a0inv * (1.0f - alpha);
	}
}

static inline void biquad_highpass(biquad_st *bq, float freq, float Q){
	freq /= 24000;
	if (freq >= 1.0f)
		biquad_scale(bq, 0);
	else if (freq <= 0.0f)
		biquad_scale(bq, 1);
	else{
		Q = powf(10.0f, Q * 0.05f); // convert Q from dB to linear
		float theta = TAU * freq;
		float alpha = sinf(theta) / (2.0f * Q);
		float cosw  = cosf(theta);
		float beta  = (1.0f + cosw) * 0.5f;
		float a0inv = 1.0f / (1.0f + alpha);
		bq->b0 = a0inv * beta;
		bq->b1 = a0inv * -2.0f * beta;
		bq->b2 = a0inv * beta;
		bq->a1 = a0inv * -2.0f * cosw;
		bq->a2 = a0inv * (1.0f - alpha);
	}
}

static inline void biquad_bandpass(biquad_st *bq, float freq, float Q){
	freq /= 24000;
	if (freq <= 0.0f || freq >= 1.0f)
		biquad_scale(bq, 0);
	else if (Q <= 0.0f)
		biquad_scale(bq, 1);
	else{
		float w0    = TAU * freq;
		float alpha = sinf(w0) / (2.0f * Q);
		float k     = cosf(w0);
		float a0inv = 1.0f / (1.0f + alpha);
		bq->b0 = a0inv * alpha;
		bq->b1 = 0;
		bq->b2 = a0inv * -alpha;
		bq->a1 = a0inv * -2.0f * k;
		bq->a2 = a0inv * (1.0f - alpha);
	}
}

static inline void biquad_notch(biquad_st *bq, float freq, float Q){
	freq /= 24000;
	if (freq <= 0.0f || freq >= 1.0f)
		biquad_scale(bq, 1);
	else if (Q <= 0.0f)
		biquad_scale(bq, 0);
	else{
		float w0    = TAU * freq;
		float alpha = sinf(w0) / (2.0f * Q);
		float k     = cosf(w0);
		float a0inv = 1.0f / (1.0f + alpha);
		bq->b0 = a0inv;
		bq->b1 = a0inv * -2.0f * k;
		bq->b2 = a0inv;
		bq->a1 = a0inv * -2.0f * k;
		bq->a2 = a0inv * (1.0f - alpha);
	}
}

static inline void biquad_peaking(biquad_st *bq, float freq, float Q, float gain){
	float A = powf(10.0f, gain * 0.025f); // square root of gain converted from dB to linear
	freq /= 24000;
	if (freq <= 0.0f || freq >= 1.0f)
		biquad_scale(bq, 1);
	else if (Q <= 0.0f)
		biquad_scale(bq, A * A); // scale by A squared
	else{
		float w0    = (float)M_PI * 2.0f * freq;
		float alpha = sinf(w0) / (2.0f * Q);
		float k     = cosf(w0);
		float a0inv = 1.0f / (1.0f + alpha / A);
		bq->b0 = a0inv * (1.0f + alpha * A);
		bq->b1 = a0inv * -2.0f * k;
		bq->b2 = a0inv * (1.0f - alpha * A);
		bq->a1 = a0inv * -2.0f * k;
		bq->a2 = a0inv * (1.0f - alpha / A);
	}
}

static inline void biquad_lowshelf(biquad_st *bq, float freq, float Q, float gain){
	float A = powf(10.0f, gain * 0.025f); // square root of gain converted from dB to linear
	freq /= 24000;
	if (freq <= 0.0f || Q == 0.0f)
		biquad_scale(bq, 1);
	else if (freq >= 1.0f)
		biquad_scale(bq, A * A); // scale by A squared
	else{
		float w0    = TAU * freq;
		float ainn  = (A + 1.0f / A) * (1.0f / Q - 1.0f) + 2.0f;
		if (ainn < 0)
			ainn = 0;
		float alpha = 0.5f * sinf(w0) * sqrtf(ainn);
		float k     = cosf(w0);
		float k2    = 2.0f * sqrtf(A) * alpha;
		float Ap1   = A + 1.0f;
		float Am1   = A - 1.0f;
		float a0inv = 1.0f / (Ap1 + Am1 * k + k2);
		bq->b0 = a0inv * A * (Ap1 - Am1 * k + k2);
		bq->b1 = a0inv * 2.0f * A * (Am1 - Ap1 * k);
		bq->b2 = a0inv * A * (Ap1 - Am1 * k - k2);
		bq->a1 = a0inv * -2.0f * (Am1 + Ap1 * k);
		bq->a2 = a0inv * (Ap1 + Am1 * k - k2);
	}
}

static inline void biquad_highshelf(biquad_st *bq, float freq, float Q, float gain){
	float A = powf(10.0f, gain * 0.025f); // square root of gain converted from dB to linear
	freq /= 24000;
	if (freq >= 1.0f || Q == 0.0f)
		biquad_scale(bq, 1);
	else if (freq <= 0.0f)
		biquad_scale(bq, A * A); // scale by A squared
	else{
		float w0    = TAU * freq;
		float ainn  = (A + 1.0f / A) * (1.0f / Q - 1.0f) + 2.0f;
		if (ainn < 0)
			ainn = 0;
		float alpha = 0.5f * sinf(w0) * sqrtf(ainn);
		float k     = cosf(w0);
		float k2    = 2.0f * sqrtf(A) * alpha;
		float Ap1   = A + 1.0f;
		float Am1   = A - 1.0f;
		float a0inv = 1.0f / (Ap1 - Am1 * k + k2);
		bq->b0 = a0inv * A * (Ap1 + Am1 * k + k2);
		bq->b1 = a0inv * -2.0f * A * (Am1 + Ap1 * k);
		bq->b2 = a0inv * A * (Ap1 + Am1 * k - k2);
		bq->a1 = a0inv * 2.0f * (Am1 - Ap1 * k);
		bq->a2 = a0inv * (Ap1 - Am1 * k - k2);
	}
}

static inline nm_sample_st biquad_step(biquad_st *bq, nm_sample_st in){
	nm_sample_st out = {
		bq->b0 * in.L +
		bq->b1 * bq->xn1.L +
		bq->b2 * bq->xn2.L -
		bq->a1 * bq->yn1.L -
		bq->a2 * bq->yn2.L,
		bq->b0 * in.R +
		bq->b1 * bq->xn1.R +
		bq->b2 * bq->xn2.R -
		bq->a1 * bq->yn1.R -
		bq->a2 * bq->yn2.R
	};
	bq->xn2 = bq->xn1;
	bq->xn1 = in;
	bq->yn2 = bq->yn1;
	bq->yn1 = out;
	return out;
}

//
// OVERSAMPLE
//

typedef struct {
	float b0;
	float a1;
	float a2;
	float xn1;
	float xn2;
	float yn1;
	float yn2;
} oversample_st;

static inline void oversample_make(oversample_st *os, int factor){
	float omega = TAU / (factor * 2.0f);
	float cs = cosf(omega);
	float alpha = sinf(omega) * 1.154700538379252f; // 2/sqrt(3)
	float a0inv = 1.0f / (1.0f + alpha);
	os->b0 = a0inv * (1.0f - cs) * 0.5f;
	os->a1 = a0inv * -2.0f * cs;
	os->a2 = a0inv * (1.0f - alpha);
	os->xn1 = 0;
	os->xn2 = 0;
	os->yn1 = 0;
	os->yn2 = 0;
}

static inline float oversample_step(oversample_st *os, float v){
	float out = os->b0 * (v + os->xn1 * 2.0f + os->xn2) - os->yn1 * os->a1 - os->yn2 * os->a2;
	os->xn2 = os->xn1;
	os->xn1 = v;
	os->yn2 = os->yn1;
	os->yn1 = out;
	return out;
}

//
// ENVELOPE
//

typedef struct {
	float wait;
	float attack;
	float hold;
	float decay;
	float sustain;
	float release;
	float last;
	int sample;
	bool released;
} envelope_st;

static inline void envelope_make(envelope_st *env, float wait, float attack, float hold,
	float decay, float sustain, float release){
	env->wait = wait;
	env->attack = attack;
	env->hold = hold;
	env->decay = decay;
	env->sustain = sustain;
	env->release = release;
	env->last = 0;
	env->sample = 0;
	env->released = false;
}

static inline void envelope_reset(envelope_st *env){
	env->last = 0;
	env->sample = 0;
	env->released = false;
}

static inline float envelope_step(envelope_st *env){
	float time = env->sample++ / 48000.0f;
	float v = 0;
	if (env->released){
		if (time < env->release)
			v = env->last * (1 - time / env->release);
		else
			v = 0;
	}
	else{
		float t2 = env->wait + env->attack;
		float t3 = t2 + env->hold;
		float t4 = t3 + env->decay;
		if (time < env->wait)
			v = 0;
		else if (time < t2)
			v = (time - env->wait) / env->attack;
		else if (time < t3)
			v = 1;
		else if (time < t4)
			v = env->sustain + (1 - env->sustain) * (t4 - time) / env->decay;
		else
			v = env->sustain;
		env->last = v;
	}
	return v;
}

static inline void envelope_release(envelope_st *env){
	env->sample = 0;
	env->released = true;
}

static inline bool envelope_done(envelope_st *env){
	return env->released && env->sample / 48000.0f >= env->release;
}

//
// VOICES
//

#include "voice/1001_square.c"
#include "voice/1002_saw.c"

static const nm_voice_st end_voice = {0};

const nm_voice_st *nm_voices[] = {
	&v1001_about.voice,
	&v1002_about.voice,
	&end_voice
};

//
// API
//

void nm_init(){
	// TODO: load the opus samples into memory

	#ifndef NDEBUG
	// TODO: validate voices are sorted
	// TODO: validate no voices surpass vuser's size
	// TODO: validate no voices surpass buser's size
	#endif
}

void nm_ctx_make(nm_ctx_st *nm){
}

void nm_ctx_destroy(nm_ctx_st *nm){
}

void nm_ctx_render(nm_ctx_st *nm, nm_sample_st *out, size_t outsize){
}

void nm_term(){
	// TODO: unload samples
}
