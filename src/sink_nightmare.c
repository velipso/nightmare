// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#include "sink_nightmare.h"

typedef struct {
	nm_ctx ctx;
	uint16_t channel;
	uint16_t tpq;
} nm_user_st, *nm_user;

static sink_val L_patchname(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double idx;
	if (!sink_arg_num(ctx, size, args, 0, &idx))
		return SINK_NIL;
	int i = (int)idx;
	if (i != idx || i < 0 || i >= NM__PATCH_END)
		return SINK_NIL;
	return sink_str_newcstr(ctx, nm_patch_str(i));
}

static sink_val L_patchcat(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double idx;
	if (!sink_arg_num(ctx, size, args, 0, &idx))
		return SINK_NIL;
	int i = (int)idx;
	if (i < 0 || i >= NM__PATCH_END)
		return SINK_NIL;
	switch (nm_patch_category(i)){
		case NM_PIANO : return sink_str_newcstr(ctx, "Piano");
		case NM_CHROM : return sink_str_newcstr(ctx, "Chromatic Percussion");
		case NM_ORGAN : return sink_str_newcstr(ctx, "Organ");
		case NM_GUITAR: return sink_str_newcstr(ctx, "Guitar");
		case NM_BASS  : return sink_str_newcstr(ctx, "Bass");
		case NM_STRING: return sink_str_newcstr(ctx, "Strings & Orchestral");
		case NM_ENSEM : return sink_str_newcstr(ctx, "Ensemble");
		case NM_BRASS : return sink_str_newcstr(ctx, "Brass");
		case NM_REED  : return sink_str_newcstr(ctx, "Reed");
		case NM_PIPE  : return sink_str_newcstr(ctx, "Pipe");
		case NM_LEAD  : return sink_str_newcstr(ctx, "Synth Lead");
		case NM_PAD   : return sink_str_newcstr(ctx, "Synth Pad");
		case NM_SFX1  : return sink_str_newcstr(ctx, "Sound Effects 1");
		case NM_ETHNIC: return sink_str_newcstr(ctx, "Ethnic");
		case NM_PERC  : return sink_str_newcstr(ctx, "Percussive");
		case NM_SFX2  : return sink_str_newcstr(ctx, "Sound Effects 2");
		case NM_PERSND: return sink_str_newcstr(ctx, "Percussion Sound Set");
		case NM__PATCHCAT_END: break;
	}
	return SINK_NIL;
}

static sink_val L_mastervolume(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	double vol;
	if (!sink_arg_num(ctx, size, args, 1, &vol))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	if (t != ticks)
		return sink_abortcstr(ctx, "Invalid ticks; expecting integer");
	if (!nm_ev_mastvol(u->ctx, t, (float)vol))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

static sink_val L_masterpan(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	double pan;
	if (!sink_arg_num(ctx, size, args, 1, &pan))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	if (t != ticks)
		return sink_abortcstr(ctx, "Invalid ticks; expecting integer");
	if (!nm_ev_mastpan(u->ctx, t, (float)pan))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

typedef struct {
	int note; // 0 to 11, -1 for unknown
	int octave; // -1 to 9, -2 for unknown
} parse_note_st;
static inline parse_note_st parse_note(const uint8_t *data){
	parse_note_st ret = { -1, -2 };
	int state = 0;
	int i = 0;
	while (data[i]){
		uint8_t ch = data[i++];
		switch (state){
			case 0: // searching for note
				if      (ch == 'c' || ch == 'C'){ ret.note =  0; state = 1; }
				else if (ch == 'd' || ch == 'D'){ ret.note =  2; state = 1; }
				else if (ch == 'e' || ch == 'E'){ ret.note =  4; state = 1; }
				else if (ch == 'f' || ch == 'F'){ ret.note =  5; state = 1; }
				else if (ch == 'g' || ch == 'G'){ ret.note =  7; state = 1; }
				else if (ch == 'a' || ch == 'A'){ ret.note =  9; state = 1; }
				else if (ch == 'b' || ch == 'B'){ ret.note = 11; state = 1; }
				else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
					/* do nothing */;
				else
					return ret;
				break;

			case 1: // searching for sharp, flat, or octave
				if (ch == '#' || ch == 's' || ch == 'S' || ch == '+'){
					ret.note = (ret.note + 1) % 12;
					state = 2;
				}
				else if (ch == 'b' || ch == 'f' || ch == 'F' || ch == '-'){
					ret.note = (ret.note + 11) % 12;
					state = 2;
				}
				else if (ch == 'n' || ch == 'N' || (ch >= '0' && ch <= '9')){
					if (ch == 'n' || ch == 'N')
						ret.octave = -1;
					else
						ret.octave = ch - '0';
					return ret;
				}
				else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
					/* do nothing */;
				else
					return ret;
				break;

			case 2: // octave
				if (ch == 'n' || ch == 'N' || (ch >= '0' && ch <= '9')){
					if (ch == 'n' || ch == 'N')
						ret.octave = -1;
					else
						ret.octave = ch - '0';
					return ret;
				}
				else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
					/* do nothing */;
				else
					return ret;
				break;
		}
	}
	return ret;
}

static sink_val L_note(sink_ctx ctx, int size, sink_val *args, nm_user u){
	sink_val cn = SINK_NIL;
	sink_val co;
	if (size == 1){
		if (sink_isstr(args[0])){
			// convert string to note index
			sink_str str = sink_caststr(ctx, args[0]);
			parse_note_st pn = parse_note(str->bytes);
			if (pn.note < 0 || pn.octave < -1)
				return SINK_NIL;
			int idx = pn.note + (pn.octave + 1) * 12;
			if (idx >= 128)
				return SINK_NIL;
			return sink_num(idx);
		}
		else if (sink_isnum(args[0])){
			// convert note index to component list
			double idx = sink_castnum(args[0]);
			int i = (int)idx;
			if (i < 0 || i >= 128 || i != idx)
				return SINK_NIL;
			sink_val res[2];
			res[1] = sink_num((int)(i / 12) - 1);
			switch (i % 12){
				case  0: res[0] = sink_str_newcstr(ctx, "C" ); break;
				case  1: res[0] = sink_str_newcstr(ctx, "C#"); break;
				case  2: res[0] = sink_str_newcstr(ctx, "D" ); break;
				case  3: res[0] = sink_str_newcstr(ctx, "D#"); break;
				case  4: res[0] = sink_str_newcstr(ctx, "E" ); break;
				case  5: res[0] = sink_str_newcstr(ctx, "F" ); break;
				case  6: res[0] = sink_str_newcstr(ctx, "F#"); break;
				case  7: res[0] = sink_str_newcstr(ctx, "G" ); break;
				case  8: res[0] = sink_str_newcstr(ctx, "G#"); break;
				case  9: res[0] = sink_str_newcstr(ctx, "A" ); break;
				case 10: res[0] = sink_str_newcstr(ctx, "A#"); break;
				case 11: res[0] = sink_str_newcstr(ctx, "B" ); break;
			}
			return sink_list_newblob(ctx, 2, res);
		}
		else if (sink_islist(args[0])){
			sink_list ls = sink_castlist(ctx, args[0]);
			if (ls->size == 2){
				cn = ls->vals[0];
				co = ls->vals[1];
			}
		}
	}
	else if (size == 2 && sink_isstr(args[0]) && sink_isnum(args[1])){
		cn = args[0];
		co = args[1];
	}

	if (!sink_isnil(cn)){
		// convert components to note index
		if (!sink_isstr(cn) || !sink_isnum(co))
			return SINK_NIL;
		double oct = sink_castnum(co);
		int o = (int)oct;
		if (o < -1 || o > 9 || o != oct)
			return SINK_NIL;
		sink_str str = sink_caststr(ctx, cn);
		parse_note_st pn = parse_note(str->bytes);
		if (pn.note < 0 || pn.octave >= -1)
			return SINK_NIL;
		int n = (o + 1) * 12 + pn.note;
		if (n >= 128)
			return SINK_NIL;
		return sink_num(n);
	}

	return sink_abortcstr(ctx, "Invalid arguments to `music.note`");
}

static sink_val L_channel(sink_ctx ctx, int size, sink_val *args, nm_user u){
	if (size >= 1){
		double chan;
		if (!sink_arg_num(ctx, size, args, 1, &chan))
			return SINK_NIL;
		uint16_t c = (uint16_t)chan;
		if (c != chan || c >= u->ctx->channel_count)
			return sink_abortformat(ctx, "Invalid channel (0 to %d)", u->ctx->channel_count - 1);
		u->channel = c;
	}
	return sink_num(u->channel);
}

static sink_val L_volume(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	double vol;
	if (!sink_arg_num(ctx, size, args, 1, &vol))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	if (t != ticks)
		return sink_abortcstr(ctx, "Invalid ticks; expecting integer");
	if (!nm_ev_chanvol(u->ctx, t, u->channel, (float)vol))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

static sink_val L_expression(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	double exp;
	if (!sink_arg_num(ctx, size, args, 1, &exp))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	if (t != ticks)
		return sink_abortcstr(ctx, "Invalid ticks; expecting integer");
	if (exp < 0 || exp > 1)
		return sink_abortcstr(ctx, "Invalid expression; must be in range [0, 1]");
	if (!nm_ev_chanexp(u->ctx, t, u->channel, (float)exp))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

static sink_val L_pan(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	double pan;
	if (!sink_arg_num(ctx, size, args, 1, &pan))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	if (t != ticks)
		return sink_abortcstr(ctx, "Invalid ticks; expecting integer");
	if (!nm_ev_chanpan(u->ctx, t, u->channel, (float)pan))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

static sink_val L_reset(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	if (t != ticks)
		return sink_abortcstr(ctx, "Invalid ticks; expecting integer");
	if (size > 1){
		double tpq;
		if (!sink_arg_num(ctx, size, args, 1, &tpq))
			return SINK_NIL;
		uint16_t t2 = (uint16_t)tpq;;
		if (t2 != tpq)
			return sink_abortcstr(ctx, "Invalid ticks per quarternote; expecting integer");
		u->tpq = t2;
	}
	if (!nm_ev_reset(u->ctx, t, u->tpq))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

static sink_val L_play(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	double duration;
	if (!sink_arg_num(ctx, size, args, 1, &duration))
		return SINK_NIL;
	double note;
	if (!sink_arg_num(ctx, size, args, 2, &note))
		return SINK_NIL;
	double vel = 1;
	if (size >= 4 && !sink_arg_num(ctx, size, args, 3, &vel))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	uint32_t d = (uint32_t)duration;
	uint8_t n = ((uint8_t)note) & 0x7F;
	if (t != ticks || d != duration || n != note)
		return sink_abortcstr(ctx, "Invalid numbers; expecting integers");
	if (vel <= 0 || vel > 1)
		return sink_abortcstr(ctx, "Invalid velocity, must be between (0, 1]");
	if (!nm_ev_noteon(u->ctx, t, u->channel, n, vel) ||
		!nm_ev_noteoff(u->ctx, t + d, u->channel, n))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

static sink_val L_bend(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	double bend;
	if (!sink_arg_num(ctx, size, args, 1, &bend))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	if (t != ticks)
		return sink_abortcstr(ctx, "Invalid ticks; expecting integer");
	if (!nm_ev_chanbend(u->ctx, t, u->channel, (float)bend))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

static sink_val L_tempo(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	double num;
	if (!sink_arg_num(ctx, size, args, 1, &num))
		return SINK_NIL;
	double den;
	if (!sink_arg_num(ctx, size, args, 2, &den))
		return SINK_NIL;
	double tempo;
	if (!sink_arg_num(ctx, size, args, 3, &tempo))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	uint8_t n = ((uint8_t)num) & 0x7F;
	uint8_t d = (uint8_t)den;
	float tm = (float)tempo;
	if (t != ticks || n != num || d != den || isnan(tempo) || tempo <= 0 || tempo > 1000)
		return sink_abortcstr(ctx, "Invalid parameters, out of range");
	if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32 && d != 64 && d != 128)
		return sink_abortcstr(ctx, "Invalid denominator, must be power of 2 between 1-128");
	if (!nm_ev_tempo(u->ctx, t, n, d, tm))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

static sink_val L_patch(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	double patch;
	if (!sink_arg_num(ctx, size, args, 1, &patch))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	uint8_t p = (uint8_t)patch;
	if (t != ticks || p != patch)
		return sink_abortcstr(ctx, "Invalid numbers; expecting integers");
	if (!nm_ev_patch(u->ctx, t, u->channel, p))
		return sink_abortcstr(ctx, "Failed to insert event");
	return sink_bool(true);
}

static sink_val L_defpatch(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double patch, wave, peak, attack, decay, sustain, h1, h2, h3, h4;
	if (!sink_arg_num(ctx, size, args, 0, &patch  ) ||
		!sink_arg_num(ctx, size, args, 1, &wave   ) ||
		!sink_arg_num(ctx, size, args, 2, &peak   ) ||
		!sink_arg_num(ctx, size, args, 3, &attack ) ||
		!sink_arg_num(ctx, size, args, 4, &decay  ) ||
		!sink_arg_num(ctx, size, args, 5, &sustain) ||
		!sink_arg_num(ctx, size, args, 6, &h1     ) ||
		!sink_arg_num(ctx, size, args, 7, &h2     ) ||
		!sink_arg_num(ctx, size, args, 8, &h3     ) ||
		!sink_arg_num(ctx, size, args, 9, &h4     ))
		return SINK_NIL;
	int p = (int)patch;
	if (p < 0 || p >= 256 || p != patch)
		return sink_abortcstr(ctx, "Invalid patch (expecting 0-255)");
	uint8_t w = (uint8_t)wave;
	if (w < 0 || w >= 4 || w != wave)
		return sink_abortcstr(ctx, "Invalid wave (expecting 0-3)");
	nm_defpatch(p, w, peak, attack, decay, sustain, h1, h2, h3, h4);
	return sink_bool(true);
}

static sink_val L_bake(sink_ctx ctx, int size, sink_val *args, nm_user u){
	double ticks;
	if (!sink_arg_num(ctx, size, args, 0, &ticks))
		return SINK_NIL;
	uint32_t t = (uint32_t)ticks;
	if (t != ticks)
		return sink_abortcstr(ctx, "Invalid ticks; expecting integers");
	if (!nm_ctx_bake(u->ctx, t))
		return sink_abortcstr(ctx, "Failed to bake notes");
	return SINK_ASYNC;
}

static sink_val L_bakeall(sink_ctx ctx, int size, sink_val *args, nm_user u){
	if (!nm_ctx_bakeall(u->ctx))
		return sink_abortcstr(ctx, "Failed to bake notes");
	return SINK_ASYNC;
}

static sink_val L_savemidi(sink_ctx ctx, int size, sink_val *args, nm_user u){
	return SINK_NIL;
}

void sink_nightmare_scr(sink_scr scr){
	sink_scr_incbody(scr, "nightmare",
		"namespace music;"
		"enum "
		#define X(en, code, name) #en ", "
		NM_EACH_PATCH(X)
		#undef X
		"_PATCH_END;"
		"enum " // notes
			"Cn1=0x00,Csn1=0x01,Dbn1=0x01,Dn1=0x02,Dsn1=0x03,Ebn1=0x03,En1=0x04,Fn1=0x05,Fsn1=0x06,"
			"Gbn1=0x06,Gn1=0x07,Gsn1=0x08,Abn1=0x08,An1=0x09,Asn1=0x0A,Bbn1=0x0A,Bn1=0x0B,"
			"C0=0x0C,Cs0=0x0D,Db0=0x0D,D0=0x0E,Ds0=0x0F,Eb0=0x0F,E0=0x10,F0=0x11,Fs0=0x12,"
			"Gb0=0x12,G0=0x13,Gs0=0x14,Ab0=0x14,A0=0x15,As0=0x16,Bb0=0x16,B0=0x17,"
			"C1=0x18,Cs1=0x19,Db1=0x19,D1=0x1A,Ds1=0x1B,Eb1=0x1B,E1=0x1C,F1=0x1D,Fs1=0x1E,"
			"Gb1=0x1E,G1=0x1F,Gs1=0x20,Ab1=0x20,A1=0x21,As1=0x22,Bb1=0x22,B1=0x23,"
			"C2=0x24,Cs2=0x25,Db2=0x25,D2=0x26,Ds2=0x27,Eb2=0x27,E2=0x28,F2=0x29,Fs2=0x2A,"
			"Gb2=0x2A,G2=0x2B,Gs2=0x2C,Ab2=0x2C,A2=0x2D,As2=0x2E,Bb2=0x2E,B2=0x2F,"
			"C3=0x30,Cs3=0x31,Db3=0x31,D3=0x32,Ds3=0x33,Eb3=0x33,E3=0x34,F3=0x35,Fs3=0x36,"
			"Gb3=0x36,G3=0x37,Gs3=0x38,Ab3=0x38,A3=0x39,As3=0x3A,Bb3=0x3A,B3=0x3B,"
			"C4=0x3C,Cs4=0x3D,Db4=0x3D,D4=0x3E,Ds4=0x3F,Eb4=0x3F,E4=0x40,F4=0x41,Fs4=0x42,"
			"Gb4=0x42,G4=0x43,Gs4=0x44,Ab4=0x44,A4=0x45,As4=0x46,Bb4=0x46,B4=0x47,"
			"C5=0x48,Cs5=0x49,Db5=0x49,D5=0x4A,Ds5=0x4B,Eb5=0x4B,E5=0x4C,F5=0x4D,Fs5=0x4E,"
			"Gb5=0x4E,G5=0x4F,Gs5=0x50,Ab5=0x50,A5=0x51,As5=0x52,Bb5=0x52,B5=0x53,"
			"C6=0x54,Cs6=0x55,Db6=0x55,D6=0x56,Ds6=0x57,Eb6=0x57,E6=0x58,F6=0x59,Fs6=0x5A,"
			"Gb6=0x5A,G6=0x5B,Gs6=0x5C,Ab6=0x5C,A6=0x5D,As6=0x5E,Bb6=0x5E,B6=0x5F,"
			"C7=0x60,Cs7=0x61,Db7=0x61,D7=0x62,Ds7=0x63,Eb7=0x63,E7=0x64,F7=0x65,Fs7=0x66,"
			"Gb7=0x66,G7=0x67,Gs7=0x68,Ab7=0x68,A7=0x69,As7=0x6A,Bb7=0x6A,B7=0x6B,"
			"C8=0x6C,Cs8=0x6D,Db8=0x6D,D8=0x6E,Ds8=0x6F,Eb8=0x6F,E8=0x70,F8=0x71,Fs8=0x72,"
			"Gb8=0x72,G8=0x73,Gs8=0x74,Ab8=0x74,A8=0x75,As8=0x76,Bb8=0x76,B8=0x77,"
			"C9=0x78,Cs9=0x79,Db9=0x79,D9=0x7A,Ds9=0x7B,Eb9=0x7B,E9=0x7C,F9=0x7D,Fs9=0x7E,"
			"Gb9=0x7E,G9=0x7F;"
		"declare patchname    'nightmare.patchname'   ;"
		"declare patchcat     'nightmare.patchcat'    ;"
		"declare mastervolume 'nightmare.mastervolume';"
		"declare masterpan    'nightmare.masterpan'   ;"
		"declare note         'nightmare.note'        ;"
		"declare channel      'nightmare.channel'     ;"
		"declare volume       'nightmare.volume'      ;"
		"declare expression   'nightmare.expression'  ;"
		"declare pan          'nightmare.pan'         ;"
		"declare reset        'nightmare.reset'       ;"
		"declare play         'nightmare.play'        ;"
		"declare bend         'nightmare.bend'        ;"
		"declare tempo        'nightmare.tempo'       ;"
		"declare patch        'nightmare.patch'       ;"
		"declare defpatch     'nightmare.defpatch'    ;"
		"declare bake         'nightmare.bake'        ;"
		"declare bakeall      'nightmare.bakeall'     ;"
		"end"
	);
}

bool sink_nightmare_ctx(sink_ctx ctx, nm_ctx nctx){
	nm_user u = nm_alloc(sizeof(nm_user_st));
	if (u == NULL)
		return false;
	sink_ctx_cleanup(ctx, u, nm_free);
	u->ctx = nctx;
	u->channel = 0;
	u->tpq = nctx->ticks_per_quarternote;
	sink_ctx_native(ctx, "nightmare.patchname"   , u, (sink_native_func)L_patchname   );
	sink_ctx_native(ctx, "nightmare.patchcat"    , u, (sink_native_func)L_patchcat    );
	sink_ctx_native(ctx, "nightmare.mastervolume", u, (sink_native_func)L_mastervolume);
	sink_ctx_native(ctx, "nightmare.masterpan"   , u, (sink_native_func)L_masterpan   );
	sink_ctx_native(ctx, "nightmare.note"        , u, (sink_native_func)L_note        );
	sink_ctx_native(ctx, "nightmare.channel"     , u, (sink_native_func)L_channel     );
	sink_ctx_native(ctx, "nightmare.volume"      , u, (sink_native_func)L_volume      );
	sink_ctx_native(ctx, "nightmare.expression"  , u, (sink_native_func)L_expression  );
	sink_ctx_native(ctx, "nightmare.pan"         , u, (sink_native_func)L_pan         );
	sink_ctx_native(ctx, "nightmare.reset"       , u, (sink_native_func)L_reset       );
	sink_ctx_native(ctx, "nightmare.play"        , u, (sink_native_func)L_play        );
	sink_ctx_native(ctx, "nightmare.bend"        , u, (sink_native_func)L_bend        );
	sink_ctx_native(ctx, "nightmare.tempo"       , u, (sink_native_func)L_tempo       );
	sink_ctx_native(ctx, "nightmare.patch"       , u, (sink_native_func)L_patch       );
	sink_ctx_native(ctx, "nightmare.defpatch"    , u, (sink_native_func)L_defpatch    );
	sink_ctx_native(ctx, "nightmare.bake"        , u, (sink_native_func)L_bake        );
	sink_ctx_native(ctx, "nightmare.bakeall"     , u, (sink_native_func)L_bakeall     );
	sink_ctx_native(ctx, "nightmare.savemidi"    , u, (sink_native_func)L_savemidi    );
	return true;
}
