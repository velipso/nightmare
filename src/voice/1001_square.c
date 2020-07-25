// (c) Copyright 2020, Sean Connelly (@velipso), sean.cm
// MIT License
// Project Home: https://github.com/velipso/nightmare

#define NAME(n)                v1001_ ## n
#define UNISON                 1
#define UNISON_DETUNE(u)       1
#define STATIC_FILTER()        biquad_lowpass(&vu->bq, 60.0f + x * 2000.0f, 0)
#define PARAM_FILTER()         biquad_lowpass(&vu->bq, 60.0f + x * 2000.0f, 0)
#define ENVELOPE_MAKE()        envelope_make(&vu->env, 0, 0.01f, 0, 0.1f, 0.5f, 0.2f)
#define ENVELOPE_STEP()        s *= env
#define OSC_SQUARE
#define OVERSAMPLE             8
#define DUTY()                 duty = y * 0.4f + 0.1f

#include "../synth/osc.c"

static const vabout_st NAME(about) = {
	.vuser_size = sizeof(NAME(vst)),
	.buser_size = sizeof(NAME(bst)),
	.voice = {
		.voice_id = 1001,
		.vtype = NM_VT_OSC,
		.vcat = NM_VC_LEAD,
		.name = "Square",
		.xlabel = "Test X", .x = 50,
		.ylabel = "Test Y", .y = 50
	}
};

#undef DUTY
#undef OVERSAMPLE
#undef OSC_SQUARE
#undef ENVELOPE_STEP
#undef ENVELOPE_MAKE
#undef PARAM_FILTER
#undef STATIC_FILTER
#undef UNISON_DETUNE
#undef UNISON
#undef NAME
