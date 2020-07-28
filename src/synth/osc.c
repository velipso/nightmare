// (c) Copyright 2020, Sean Connelly (@velipso), sean.cm
// MIT License
// Project Home: https://github.com/velipso/nightmare

#if !defined(OSC_CURVE)
	#define __OSC__UNDEF__CURVE__
	#if defined(OSC_SINE)
		#define OSC_CURVE(ang)    sinf(ang * TAU)
	#elif defined(OSC_SQUARE)
		#define OSC_CURVE(ang)    ang < duty ? -1 : 1
	#elif defined(OSC_SAW)
		#define OSC_CURVE(ang)    ang < 0.5f ? 2.0f * ang : -2.0f + 2.0f * ang
	#elif defined(OSC_TRIANGLE)
		#define OSC_CURVE(ang)    ang < 0.25f ? ang * 4.0f : ang < 0.75f ? 2.0f - 4.0f * ang : 4.0f * ang - 4.0f
	#else
		#error Missing oscillator type or curve
	#endif
#endif

typedef struct {
	int note;
	float dang;
} NAME(note_st);

// data per voice
typedef struct {
	biquad_st bq;
	oversample_st os[UNISON];
	envelope_st env;
	float ang[UNISON];
	NAME(note_st) notes[15];
	int nextnote;
} NAME(vst);

// data per clip
typedef struct {
	int dummy;
} NAME(cst);

static void NAME(poly_build)(
	NAME(cst) *cu,
	float out,
	float x,
	float y
){
}

static void NAME(poly_noteon)(
	NAME(vst) *vu, NAME(cst) *cu,
	int note,
	float freq,
	float velocity,
	float x,
	float y
){
	if (vu->nextnote == 0){
		for (int u = 0; u < UNISON; u++)
			vu->ang[u] = (float)u / UNISON;
		STATIC_FILTER();
		ENVELOPE_MAKE();
	#if defined(OVERSAMPLE) && OVERSAMPLE > 1
		for (int u = 0; u < UNISON; u++)
			oversample_make(&vu->os[u], OVERSAMPLE);
	#endif
	}
	vu->notes[vu->nextnote++] = (NAME(note_st)){ note, freq / 48000.0f };
}

static bool NAME(poly_render)(
	nm_sample_st *buf,
	NAME(vst) *vu, NAME(cst) *cu,
	float volume, float dvolume,
	float x, float dx,
	float y, float dy
){
	bool on = vu->nextnote > 0 || !envelope_done(&vu->env);
	float dang[UNISON];
	dang[0] = vu->notes[maxi(0, vu->nextnote - 1)].dang;
	for (int u = 1; u < UNISON; u++)
		dang[u] = dang[0] * UNISON_DETUNE(u);
	if (!on)
		dvolume = -volume / NM_K;
	for (int i = 0; i < NM_K; i++){
		float s = 0;
		float env = envelope_step(&vu->env);
		float duty;
		DUTY();

	#if defined(OVERSAMPLE) && OVERSAMPLE > 1
		#pragma unroll
		for (int u = 0; u < UNISON; u++){
			#pragma unroll
			for (int os = 0; os < OVERSAMPLE; os++){
				float ang = fmodf(vu->ang[u] + os * dang[u] / OVERSAMPLE, 1);
				float sv = OSC_CURVE(ang);
				sv = oversample_step(&vu->os[u], sv);
				if (os == 0)
					s += sv;
			}
		}
	#else
		for (int u = 0; u < UNISON; u++)
			s += OSC_CURVE(vu->ang[u]);
	#endif

		ENVELOPE_STEP();
		nm_sample_st out = { s, s };
		PARAM_FILTER();
		out = biquad_step(&vu->bq, out);

		buf[i].L += volume * out.L;
		buf[i].R += volume * out.R;
		#pragma unroll
		for (int u = 0; u < UNISON; u++)
			vu->ang[u] = fmodf(vu->ang[u] + dang[u], 1);
		volume += dvolume;
		x += dx;
		y += dy;
	}
	return on;
}

static void NAME(poly_noteoff)(
	NAME(vst) *vu, NAME(cst) *cu,
	int note,
	float freq
){
	for (int i = 0; i < vu->nextnote; i++){
		if (vu->notes[i].note == note){
			if (i < vu->nextnote - 1){
				memmove(&vu->notes[i], &vu->notes[i + 1],
					sizeof(NAME(note_st)) * (vu->nextnote - i));
			}
			vu->nextnote--;
			if (vu->nextnote <= 0)
				envelope_release(&vu->env);
			return;
		}
	}
}

#ifdef __OSC__UNDEF__CURVE__
	#undef OSC_CURVE
#endif
