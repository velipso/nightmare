// (c) Copyright 2020, Sean Connelly (@velipso), sean.cm
// MIT License
// Project Home: https://github.com/velipso/nightmare

#define NAME(n)                v1002_ ## n
#define UNISON                 5
#define UNISON_DETUNE(u)       (1 + ((u & 1) ? -1 : 1) * 0.003f * u * u * (y + 0.01f))
#define STATIC_FILTER()        biquad_lowpass(&vu->bq, 60.0f + x * 2000.0f, 0)
#define PARAM_FILTER()         biquad_lowpass(&vu->bq, 60.0f + x * 2000.0f, 0)
#define ENVELOPE_MAKE()        envelope_make(&vu->env, 0, 0.1f, 0, 0.1f, 0.5f, 0.2f)
#define ENVELOPE_STEP()        s *= env
#define OSC_SAW
#define OVERSAMPLE             2
#define DUTY()

#include "../synth/osc.c"

VABOUT_POLY(
	1002,
	NM_VC_PAD,
	"Saw",
	"Test X:", 50,
	"Test Y:", 50
);

#undef DUTY
#undef OVERSAMPLE
#undef OSC_SAW
#undef ENVELOPE_STEP
#undef ENVELOPE_MAKE
#undef PARAM_FILTER
#undef STATIC_FILTER
#undef UNISON_DETUNE
#undef UNISON
#undef NAME
