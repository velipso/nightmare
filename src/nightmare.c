// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#include "nightmare.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include <string.h>

nm_alloc_func nm_alloc = malloc;
nm_realloc_func nm_realloc = realloc;
nm_free_func nm_free = free;

static void warn(nm_midi_warn_func f_warn, void *warnuser, const char *fmt, ...){
	if (f_warn == NULL)
		return;
	va_list args, args2;
	va_start(args, fmt);
	va_copy(args2, args);
	size_t s = vsnprintf(NULL, 0, fmt, args);
	char *buf = nm_alloc(s + 1);
	vsprintf(buf, fmt, args2);
	va_end(args);
	va_end(args2);
	f_warn(buf, warnuser);
	nm_free(buf);
}

nm_midi nm_midi_newfile(const char *file, nm_midi_warn_func f_warn, void *warnuser){
	FILE *fp = fopen(file, "rb");
	if (fp == NULL)
		return NULL;
	fseek(fp, 0, SEEK_END);
	uint64_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t *data = nm_alloc(sizeof(uint8_t) * size);
	if (data == NULL){
		fclose(fp);
		return NULL;
	}
	if (fread(data, 1, size, fp) != size){
		nm_free(data);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	nm_midi midi = nm_midi_newbuffer(size, data, f_warn, warnuser);
	nm_free(data);
	return midi;
}

// -1 = invalid
//  0 = MThd
//  1 = MTrk
//  2 = XFIH
//  3 = XFKM
static inline int chunk_type(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4){
	if (b1 == 'M' && b2 == 'T'){
		if (b3 == 'h' && b4 == 'd')
			return 0;
		else if (b3 == 'r' && b4 == 'k')
			return 1;
	}
	else if (b1 == 'X' && b2 == 'F'){
		if (b3 == 'I' && b4 == 'H')
			return 2;
		else if (b3 == 'K' && b4 == 'M')
			return 3;
	}
	return -1;
}

typedef struct {
	int type;
	uint32_t data_size;
	uint64_t data_start;
	int alignment;
} chunk_st;

static bool read_chunk(uint64_t p, uint64_t size, uint8_t *data, chunk_st *chk){
	if (p + 8 > size)
		return false;
	int type = chunk_type(data[p + 0], data[p + 1], data[p + 2], data[p + 3]);
	int alignment = 0;
	if (type < 0){
		uint64_t p_orig = p;
		// rewind 7 bytes and search forward until end of data
		p = p < 7 ? 0 : p - 7;
		while (p + 4 <= size){
			type = chunk_type(data[p + 0], data[p + 1], data[p + 2], data[p + 3]);
			if (type >= 0)
				break;
			p++;
		}
		if (type >= 0)
			alignment = (int)(p - p_orig);
	}
	if (type < 0 || p + 8 > size)
		return false;
	chk->type = type;
	chk->data_size =
		((uint32_t)data[p + 4] << 24) |
		((uint32_t)data[p + 5] << 16) |
		((uint32_t)data[p + 6] <<  8) |
		((uint32_t)data[p + 7]);
	chk->data_start = p + 8;
	chk->alignment = alignment;
	return true;
}

static inline nm_event ev_noteoff(uint64_t tick, int chan, uint8_t note, uint8_t vel){
	nm_event ev = nm_alloc(sizeof(nm_event_st));
	if (ev == NULL)
		return NULL;
	ev->next = NULL;
	ev->type = NM_NOTEOFF;
	ev->tick = tick;
	ev->u.noteoff.channel = chan;
	ev->u.noteoff.note = note;
	if (vel == 0x40)
		ev->u.noteoff.velocity = 0.5f;
	else
		ev->u.noteoff.velocity = (float)vel / 127.0f;
	return ev;
}

static inline nm_event ev_noteon(uint64_t tick, int chan, uint8_t note, uint8_t vel){
	if (vel == 0)
		return ev_noteoff(tick, chan, note, 0x40);
	nm_event ev = nm_alloc(sizeof(nm_event_st));
	if (ev == NULL)
		return NULL;
	ev->next = NULL;
	ev->type = NM_NOTEON;
	ev->tick = tick;
	ev->u.noteon.channel = chan;
	ev->u.noteon.note = note;
	if (vel == 0x40)
		ev->u.noteon.velocity = 0.5f;
	else
		ev->u.noteon.velocity = (float)vel / 127.0f;
	return ev;
}

static inline nm_event ev_notepressure(uint64_t tick, int chan, uint8_t note, uint8_t pres){
	nm_event ev = nm_alloc(sizeof(nm_event_st));
	if (ev == NULL)
		return NULL;
	ev->next = NULL;
	ev->type = NM_NOTEPRESSURE;
	ev->tick = tick;
	ev->u.notepressure.channel = chan;
	ev->u.notepressure.note = note;
	ev->u.notepressure.pressure = (float)pres / 127.0f; // TODO: not sure
	return ev;
}

static inline nm_event ev_controlchange(uint64_t tick, int chan, uint8_t ctrl, uint8_t val){
	nm_event ev = nm_alloc(sizeof(nm_event_st));
	if (ev == NULL)
		return NULL;
	ev->next = NULL;
	ev->type = NM_CONTROLCHANGE;
	ev->tick = tick;
	ev->u.controlchange.channel = chan;
	ev->u.controlchange.control = ctrl;
	ev->u.controlchange.value = val;
	return ev;
}

static inline nm_event ev_programchange(uint64_t tick, int chan, uint8_t patch){
	nm_event ev = nm_alloc(sizeof(nm_event_st));
	if (ev == NULL)
		return NULL;
	ev->next = NULL;
	ev->type = NM_PROGRAMCHANGE;
	ev->tick = tick;
	ev->u.programchange.channel = chan;
	ev->u.programchange.patch = patch;
	return ev;
}

static inline nm_event ev_channelpressure(uint64_t tick, int chan, uint8_t pres){
	nm_event ev = nm_alloc(sizeof(nm_event_st));
	if (ev == NULL)
		return NULL;
	ev->next = NULL;
	ev->type = NM_CHANNELPRESSURE;
	ev->tick = tick;
	ev->u.channelpressure.channel = chan;
	ev->u.channelpressure.pressure = (float)pres / 127.0f;
	return ev;
}

static inline nm_event ev_pitchbend(uint64_t tick, int chan, uint16_t bend){
	nm_event ev = nm_alloc(sizeof(nm_event_st));
	if (ev == NULL)
		return NULL;
	ev->next = NULL;
	ev->type = NM_PITCHBEND;
	ev->tick = tick;
	ev->u.pitchbend.channel = chan;
	if (bend < 0x2000)
		ev->u.pitchbend.bend = (float)(bend - 0x2000) / 8192.0f;
	else if (bend == 0x2000)
		ev->u.pitchbend.bend = 0;
	else
		ev->u.pitchbend.bend = (float)(bend - 0x2000) / 8191.0f;
	return ev;
}

static inline nm_event ev_tempo(uint64_t tick, int tempo){
	nm_event ev = nm_alloc(sizeof(nm_event_st));
	if (ev == NULL)
		return NULL;
	ev->next = NULL;
	ev->type = NM_TEMPO;
	ev->tick = tick;
	ev->u.tempo.tempo = tempo;
	return ev;
}

static inline nm_event ev_timesig(uint64_t tick, int num, int den, int cc, int dd){
	nm_event ev = nm_alloc(sizeof(nm_event_st));
	if (ev == NULL)
		return NULL;
	ev->next = NULL;
	ev->type = NM_TIMESIG;
	ev->tick = tick;
	ev->u.timesig.num = num;
	ev->u.timesig.den = den;
	ev->u.timesig.cc = cc;
	ev->u.timesig.dd = dd;
	return ev;
}

nm_midi nm_midi_newbuffer(uint64_t size, uint8_t *data, nm_midi_warn_func f_warn, void *warnuser){
	if (size < 14 ||
		data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd' ||
		data[4] !=  0  || data[5] !=  0  || data[6] !=  0  || data[7] < 6){
		warn(f_warn, warnuser, "Invalid header");
		return NULL;
	}
	nm_midi midi = NULL;
	uint64_t pos = 0;
	bool found_header = false;
	int format;
	int track_chunks;
	int track_i = 0;
	int tempo;
	int timesig_num;
	int timesig_den;
	int running_status;
	chunk_st chk;
	while (pos < size){
		if (!read_chunk(pos, size, data, &chk)){
			if (midi == NULL)
				warn(f_warn, warnuser, "Invalid header");
			else{
				uint64_t dif = size - pos;
				warn(f_warn, warnuser, "Unrecognized data (%llu byte%s) at end of file", dif,
					dif == 1 ? "" : "s");
			}
			return midi;
		}
		if (chk.alignment != 0){
			warn(f_warn, warnuser, "Chunk misaligned by %d byte%s", chk.alignment,
				chk.alignment == 1 ? "" : "s");
		}
		if (midi == NULL){
			midi = nm_alloc(sizeof(nm_midi_st));
			if (midi == NULL)
				return NULL;
			midi->tracks = NULL;
			midi->track_count = 0;
			midi->ticks_per_q = 0;
			midi->max_channels = 0;
		}
		uint64_t orig_size = chk.data_size;
		if (chk.data_start + chk.data_size > size){
			uint64_t offset = chk.data_start + chk.data_size - size;
			warn(f_warn, warnuser, "Chunk ends %llu byte%s too early",
				offset, offset == 1 ? "" : "s");
			chk.data_size -= offset;
		}
		pos = chk.data_start + chk.data_size;
		switch (chk.type){
			case 0: { // MThd
				if (found_header)
					warn(f_warn, warnuser, "Multiple header chunks present");
				found_header = true;
				if (orig_size != 6){
					warn(f_warn, warnuser,
						"Header chunk has non-standard size %llu byte%s (expecting 6 bytes)",
						orig_size, orig_size == 1 ? "" : "s");
				}
				if (chk.data_size >= 2){
					format = ((int)data[chk.data_start + 0] << 8) | data[chk.data_start + 1];
					if (format != 0 && format != 1 && format != 2){
						warn(f_warn, warnuser, "Header reports bad format (%d)", format);
						format = 1;
					}
				}
				else{
					warn(f_warn, warnuser, "Header missing format");
					format = 1;
				}
				if (chk.data_size >= 4){
					track_chunks = ((int)data[chk.data_start + 2] << 8) | data[chk.data_start + 3];
					if (format == 0 && track_chunks != 1){
						warn(f_warn, warnuser,
							"Format 0 expecting 1 track chunk, header is reporting %d chunks",
							track_chunks);
					}
				}
				else{
					warn(f_warn, warnuser, "Header missing track chunk count");
					track_chunks = -1;
				}
				if (chk.data_size >= 6){
					int t = ((int)data[chk.data_start + 4] << 8) | data[chk.data_start + 5];
					if (t & 0x8000){
						warn(f_warn, warnuser, "Unsupported timing format (SMPTE)");
						midi->ticks_per_q = 1;
					}
					else
						midi->ticks_per_q = t;
				}
				else{
					warn(f_warn, warnuser, "Header missing division");
					midi->ticks_per_q = 1;
				}
			} break;

			case 1: { // MTrk
				if (format == 0 && track_i > 0){
					warn(f_warn, warnuser, "Format 0 expecting 1 track chunk, found more than one");
					format = 1;
				}
				int chan_base = 0;
				if (track_i == 0 || format == 2){
					tempo = 120;
					timesig_num = 4;
					timesig_den = 4;
					midi->tracks = nm_realloc(midi->tracks, sizeof(nm_event) * (track_i + 1));
					midi->tracks[track_i] = NULL;
					midi->track_count = track_i + 1;
				}
				if (format == 1)
					chan_base = track_i * 16;
				midi->max_channels = chan_base + 16;
				running_status = -1;
				uint64_t p = chk.data_start;
				uint64_t p_end = chk.data_start + chk.data_size;
				uint64_t tick = 0;
				nm_event ev_last = NULL;
				while (p < p_end){
					// read delta as variable int
					nm_event ev_new = NULL;
					int dt = 0;
					int len = 0;
					while (true){
						len++;
						if (len >= 5){
							warn(f_warn, warnuser, "Invalid timestamp in track %d",
								track_i);
							goto mtrk_end;
						}
						int t = data[p++];
						if (t & 0x80){
							if (p >= p_end){
								warn(f_warn, warnuser, "Invalid timestamp in track %d",
									track_i);
								goto mtrk_end;
							}
							dt = (dt << 7) | (t & 0x7F);
						}
						else{
							dt = (dt << 7) | t;
							break;
						}
					}

					tick += dt;

					if (p >= p_end){
						warn(f_warn, warnuser, "Missing message in track %d", track_i);
						goto mtrk_end;
					}

					// read msg
					int msg = data[p++];
					if (msg < 0x80){
						// use running status
						if (running_status < 0){
							warn(f_warn, warnuser, "Invalid message %02X in track %d", msg,
								track_i);
							goto mtrk_end;
						}
						else{
							msg = running_status;
							p--;
						}
					}

					// interpret msg
					if (msg >= 0x80 && msg < 0x90){ // Note-Off
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Note-Off message (out of data) in track %d",
								track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t note = data[p++];
						uint8_t vel = data[p++];
						if (note >= 0x80){
							warn(f_warn, warnuser, "Bad Note-Off message (invalid note %02X) in "
								"track %d", note, track_i);
							note ^= 0x80;
						}
						if (vel >= 0x80){
							warn(f_warn, warnuser, "Bad Note-Off message (invalid velocity %02X) "
								"in track %d", vel, track_i);
							vel ^= 0x80;
						}
						ev_new = ev_noteoff(tick, chan_base + (msg & 0xF), note, vel);
					}
					else if (msg >= 0x90 && msg < 0xA0){ // Note-On
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Note-On message (out of data) in track %d",
								track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t note = data[p++];
						uint8_t vel = data[p++];
						if (note >= 0x80){
							warn(f_warn, warnuser, "Bad Note-On message (invalid note %02X) in "
								"track %d", note, track_i);
							note ^= 0x80;
						}
						if (vel >= 0x80){
							warn(f_warn, warnuser, "Bad Note-On message (invalid velocity %02X) "
								"in track %d", vel, track_i);
							vel ^= 0x80;
						}
						ev_new = ev_noteon(tick, chan_base + (msg & 0xF), note, vel);
					}
					else if (msg >= 0xA0 && msg < 0xB0){ // Note Pressure
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Note Pressure message (out of data) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t note = data[p++];
						uint8_t pressure = data[p++];
						if (note >= 0x80){
							warn(f_warn, warnuser, "Bad Note Pressure message (invalid note %02X) "
								"in track %d", note, track_i);
							note ^= 0x80;
						}
						if (pressure >= 0x80){
							warn(f_warn, warnuser, "Bad Note Pressure message (invalid pressure "
								"%02X) in track %d", pressure, track_i);
							pressure ^= 0x80;
						}
						ev_new = ev_notepressure(tick, chan_base + (msg & 0xF), note, pressure);
					}
					else if (msg >= 0xB0 && msg < 0xC0){ // Control Change
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Control Change message (out of data) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t ctrl = data[p++];
						uint8_t val = data[p++];
						if (ctrl >= 0x80){
							warn(f_warn, warnuser, "Bad Control Change message (invalid control "
								" %02X) in track %d", ctrl, track_i);
							ctrl ^= 0x80;
						}
						if (val >= 0x80){
							warn(f_warn, warnuser, "Bad Control Change message (invalid value %02X) "
								"in track %d", val, track_i);
							val ^= 0x80;
						}
						ev_new = ev_controlchange(tick, chan_base + (msg & 0xF), ctrl, val);
					}
					else if (msg >= 0xC0 && msg < 0xD0){ // Program Change
						if (p >= p_end){
							warn(f_warn, warnuser, "Bad Program Change message (out of data) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t patch = data[p++];
						if (patch >= 0x80){
							warn(f_warn, warnuser, "Bad Program Change message (invalid patch "
								"%02X) in track %d", patch, track_i);
							patch ^= 0x80;
						}
						ev_new = ev_programchange(tick, chan_base + (msg & 0xF), patch);
					}
					else if (msg >= 0xD0 && msg < 0xE0){ // Channel Pressure
						if (p >= p_end){
							warn(f_warn, warnuser, "Bad Channel Pressure message (out of data) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t pressure = data[p++];
						if (pressure >= 0x80){
							warn(f_warn, warnuser, "Bad Channel Pressure message (invalid pressure "
								"%02X) in track %d", pressure, track_i);
							pressure ^= 0x80;
						}
						ev_new = ev_channelpressure(tick, chan_base + (msg & 0xF), pressure);
					}
					else if (msg >= 0xE0 && msg < 0xF0){ // Pitch Bend
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Pitch Bend message (out of data) in track "
								"%d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						int p1 = data[p++];
						int p2 = data[p++];
						if (p1 >= 0x80){
							warn(f_warn, warnuser, "Bad Pitch Bend message (invalid lower bits "
								"%02X) in track %d", p1, track_i);
							p1 ^= 0x80;
						}
						if (p2 >= 0x80){
							warn(f_warn, warnuser, "Bad Pitch Bend message (invalid higher bits "
								"%02X) in track %d", p2, track_i);
							p2 ^= 0x80;
						}
						ev_new = ev_pitchbend(tick, chan_base + (msg & 0xF), p1 | (p2 << 7));
					}
					else if (msg == 0xF0 || msg == 0xF7){ // SysEx Event
						running_status = -1; // TODO: validate we should clear this
						// read length as a variable int
						int dl = 0;
						int len = 0;
						while (true){
							if (p >= p_end){
								warn(f_warn, warnuser, "Bad SysEx Event (out of data) in track %d",
									track_i);
								goto mtrk_end;
							}
							len++;
							if (len >= 5){
								warn(f_warn, warnuser, "Bad SysEx Event (invalid data length) in "
									"track %d", track_i);
								goto mtrk_end;
							}
							int t = data[p++];
							if (t & 0x80)
								dl = (dl << 7) | (t & 0x7F);
							else{
								dl = (dl << 7) | t;
								break;
							}
						}
						if (p + dl > p_end){
							warn(f_warn, warnuser, "Bad SysEx Event (data length too large) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						// TODO: deal with SysEx event, deal with joining packets together
						p += dl;
					}
					else if (msg == 0xFF){ // Meta Event
						running_status = -1; // TODO: validate we should clear this
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Meta Event (out of data) in track %d",
								track_i);
							goto mtrk_end;
						}
						int type = data[p++];
						int len = data[p++];
						if (p + len > p_end){
							warn(f_warn, warnuser, "Bad Meta Event (data length too large) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						switch (type){
							case 0x00: // 02 SSSS           Sequence Number
							case 0x01: // LL text           Generic Text
							case 0x02: // LL text           Copyright Notice
							case 0x03: // LL text           Track Name
							case 0x04: // LL text           Instrument Name
							case 0x05: // LL text           Lyric
							case 0x06: // LL text           Marker
							case 0x07: // LL text           Cue Point
							case 0x20: // 01 NN             Channel Prefix
							case 0x21: // 01 PP             MIDI Port
							case 0x2E: // ?????             "Track Loop Event"?
								break;
							case 0x2F: // 00                End of Track
								if (len != 0){
									warn(f_warn, warnuser, "Expecting zero-length data for End of "
										"Track message for track %d", track_i);
								}
								if (p < p_end){
									uint64_t pd = p_end - p;
									warn(f_warn, warnuser, "Extra data at end of track %d: %llu "
										"byte%s", track_i, pd, pd == 1 ? "" : "s");
								}
								goto mtrk_end;
							case 0x51: // 03 TT TT TT       Set Tempo
								if (len < 3){
									warn(f_warn, warnuser, "Missing data for Set Tempo event in "
										"track %d", track_i);
								}
								else{
									if (len > 3){
										warn(f_warn, warnuser, "Extra %d byte%s for Set Tempo "
											"event in track %d", len - 3, len - 3 == 1 ? "" : "s",
											track_i);
									}
									int tempo = ((int)data[p + 0] << 16) | ((int)data[p + 1] << 8) |
										data[p + 2];
									if (tempo == 0){
										warn(f_warn, warnuser, "Invalid tempo (0) for track %d",
											track_i);
									}
									else
										ev_new = ev_tempo(tick, tempo);
								}
								break;
							case 0x54: // 05 HH MM SS RR TT SMPTE Offset
								break;
							case 0x58: // 04 NN MM LL TT    Time Signature
								if (len < 4){
									warn(f_warn, warnuser, "Missing data for Time Signature event "
										"in track %d", track_i);
								}
								else{
									if (len > 4){
										warn(f_warn, warnuser, "Extra %d byte%s for Time Signature "
											"event in track %d", len - 4, len - 4 == 1 ? "" : "s",
											track_i);
									}
									ev_new = ev_timesig(tick,
										data[p], data[p + 1], data[p + 2], data[p + 3]);
								}
								break;
							case 0x59: // 02 SS MM          Key Signature
							case 0x7F: // LL data           Sequencer-Specific Meta Event
								break;
							default:
								warn(f_warn, warnuser, "Unknown Meta Event %02X in track %d",
									type, track_i);
								break;
						}
						p += len;
					}
					else{
						warn(f_warn, warnuser, "Unknown message type %02X in track %d",
							msg, track_i);
						goto mtrk_end;
					}
					if (ev_new){
						if (ev_last == NULL){
							if (format == 2)
								midi->tracks[track_i] = (ev_last = ev_new);
							else{
								// insert ev_new into tracks[0]
								if (midi->tracks[0] == NULL)
									midi->tracks[0] = ev_new;
								else{
									nm_event here = midi->tracks[0];
									if (ev_new->tick < here->tick){
										// insert at start
										ev_new->next = here;
										midi->tracks[0] = ev_new;
									}
									else{
										while (here->next && here->next->tick < ev_new->tick)
											here = here->next;
										// insert after here
										ev_new->next = here->next;
										here->next = ev_new;
									}
								}
							}
						}
						else
							ev_last = (ev_last->next = ev_new);
					}
				}
				warn(f_warn, warnuser, "Track %d ended before receiving End of Track message",
					track_i);
				mtrk_end:
				track_i++;
			} break;

			case 2: { // XFIH
				warn(f_warn, warnuser, "TODO: XFIH chunk not implemented");
			} break;

			case 3: { // XFKM
				warn(f_warn, warnuser, "TODO: XFKM chunk not implemented");
			} break;

			default:
				warn(f_warn, warnuser, "Fatal Error: Illegal Chunk Type");
				exit(1);
				return NULL;
		}
	}
	if (found_header && track_chunks != track_i){
		warn(f_warn, warnuser, "Mismatch between reported track count (%d) and actual track "
			"count (%d)", track_chunks, track_i);
	}
	return midi;
}

const char *nm_event_type_str(nm_event_type type){
	switch (type){
		case NM_NOTEOFF:         return "Note Off";
		case NM_NOTEON:          return "Note On";
		case NM_NOTEPRESSURE:    return "Note Pressure";
		case NM_CONTROLCHANGE:   return "Control Change";
		case NM_PROGRAMCHANGE:   return "Program Change";
		case NM_CHANNELPRESSURE: return "Channel Pressure";
		case NM_PITCHBEND:       return "Pitch Bend";
		case NM_TEMPO:           return "Tempo";
		case NM_TIMESIG:         return "Time Signature";
	}
	return "Unknown";
}

nm_ctx nm_ctx_new(nm_midi midi, int track, int samples_per_sec){
	if (track < 0 || track >= midi->track_count)
		return NULL;
	nm_ctx ctx = nm_alloc(sizeof(nm_ctx_st));
	if (ctx == NULL)
		return NULL;
	ctx->midi = midi;
	ctx->ticks = 0;
	ctx->samples_done = 0;
	ctx->samples_per_tick = (double)samples_per_sec / ((double)midi->ticks_per_q * 2.0);
	ctx->samples_per_sec = samples_per_sec;
	ctx->ignore_timesig = false;
	ctx->chans = nm_alloc(sizeof(nm_channel_st) * midi->max_channels);
	if (ctx->chans == NULL){
		nm_free(ctx);
		return NULL;
	}
	for (int ch = 0; ch < midi->max_channels; ch++){
		ctx->chans[ch].pressure = 0;
		ctx->chans[ch].pitch_bend = 0;
		for (int nt = 0; nt < 128; nt++){
			ctx->chans[ch].notes[nt].note = nt;
			ctx->chans[ch].notes[nt].freq = 440.0 * pow(2.0, (nt - 69.0) / 12.0);
			ctx->chans[ch].notes[nt].hit_velocity = 1;
			ctx->chans[ch].notes[nt].hold_pressure = 1;
			ctx->chans[ch].notes[nt].release_velocity = 1;
			ctx->chans[ch].notes[nt].fade = 0;
			ctx->chans[ch].notes[nt].dfade = 0;
			ctx->chans[ch].notes[nt].hit = false;
			ctx->chans[ch].notes[nt].down = false;
			ctx->chans[ch].notes[nt].release = false;
		}
	}
	ctx->ev = midi->tracks[track];
	return ctx;
}

static void render_sect(nm_ctx ctx, uint64_t samples_done, int len, nm_sample_st *samples){
	memset(samples, 0, sizeof(nm_sample_st) * len);
	for (int i = 0; i < ctx->midi->max_channels; i++){
		for (int n = 0; n < 128; n++){
			float fade = ctx->chans[i].notes[n].fade;
			float dfade = ctx->chans[i].notes[n].dfade;
			if (ctx->chans[i].notes[n].hit){
				ctx->chans[i].notes[n].hit = false;
				dfade = 1.0f / (0.1f * ctx->samples_per_sec);
				ctx->chans[i].notes[n].dfade = dfade;
			}
			else if (ctx->chans[i].notes[n].release){
				ctx->chans[i].notes[n].release = false;
				dfade = -1.0f / (0.1f * ctx->samples_per_sec);
				ctx->chans[i].notes[n].dfade = dfade;
			}
			if (fade > 0 || dfade > 0){
				float amp = ctx->chans[i].notes[n].hit_velocity;
				float freq = ctx->chans[i].notes[n].freq;
				for (int a = 0; a < len; a++){
					float v = fade * fade * amp *
						sinf(M_PI * 2.0f * freq * (((double)samples_done) + a) / ctx->samples_per_sec);
					if (fade < 1 && dfade > 0){
						fade += dfade;
						if (fade > 1)
							fade = 1;
					}
					else if (fade > 0 && dfade < 0){
						fade += dfade;
						if (fade < 0)
							fade = 0;
					}
					samples[a].L += v;
					samples[a].R += v;
				}
				ctx->chans[i].notes[n].fade = fade;
			}
		}
	}
}

int nm_ctx_process(nm_ctx ctx, int sample_len, nm_sample_st *samples){
	int total_out = 0;
	nm_event ev = ctx->ev;
	while (ev){
		double next_ev_sample = round(((double)ev->tick - ctx->ticks) * ctx->samples_per_tick);
		if (next_ev_sample > sample_len - total_out){
			// the next event is after these samples, so just render the rest and return
			render_sect(ctx, ctx->samples_done, sample_len - total_out, &samples[total_out]);
			ctx->ticks += (double)(sample_len - total_out) / ctx->samples_per_tick;
			ctx->samples_done += sample_len - total_out;
			return sample_len;
		}

		if (next_ev_sample > 0){
			// render before the event
			render_sect(ctx, ctx->samples_done, next_ev_sample, &samples[total_out]);
			total_out += next_ev_sample;
			ctx->ticks += next_ev_sample / ctx->samples_per_tick;
			ctx->samples_done += next_ev_sample;
		}
		// process event
		switch (ev->type){
			case NM_NOTEOFF: {
				nm_note_st *note = &ctx->chans[ev->u.noteoff.channel].notes[ev->u.noteoff.note];
				if (note->down){
					printf("NOTE OFF %02X\n", ev->u.noteoff.note);
					note->release = true;
					note->down = false;
					note->release_velocity = ev->u.noteoff.velocity;
				}
			} break;
			case NM_NOTEON: {
				nm_note_st *note = &ctx->chans[ev->u.noteon.channel].notes[ev->u.noteon.note];
				if (!note->down){
					printf("NOTE ON  %02X\n", ev->u.noteon.note);
					note->hit = true;
					note->down = true;
					note->hit_velocity = ev->u.noteon.velocity;
				}
			} break;
			case NM_NOTEPRESSURE:
				break;
			case NM_CONTROLCHANGE:
				break;
			case NM_PROGRAMCHANGE:
				break;
			case NM_CHANNELPRESSURE:
				break;
			case NM_PITCHBEND:
				break;
			case NM_TEMPO:
				ctx->ignore_timesig = true;
				ctx->samples_per_tick = (ctx->samples_per_sec * ev->u.tempo.tempo) /
					(1000000.0 * ctx->midi->ticks_per_q);
				printf("TEMPO   samples per tick: %g\n", ctx->samples_per_tick);
				break;
			case NM_TIMESIG:
				if (!ctx->ignore_timesig){
					ctx->samples_per_tick = ctx->samples_per_sec /
						(ctx->midi->ticks_per_q * 2.0 * pow(2.0, 2.0 - ev->u.timesig.den));
					printf("TIMESIG  samples per tick: %g\n", ctx->samples_per_tick);
				}
				break;
		}
		ctx->ev = (ev = ev->next);
	}
	return total_out;
}

void nm_ctx_free(nm_ctx ctx){
	nm_free(ctx->chans);
	nm_free(ctx);
}

void nm_midi_free(nm_midi midi){
	if (midi->tracks){
		for (int i = 0; i < midi->track_count; i++){
			nm_event ev_here = midi->tracks[i];
			while (ev_here){
				nm_event delme = ev_here;
				ev_here = ev_here->next;
				nm_free(delme);
			}
		}
		nm_free(midi->tracks);
	}
	nm_free(midi);
}
