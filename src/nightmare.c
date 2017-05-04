// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#include "nightmare.h"
#include <stdio.h>
#include <stdarg.h>

nm_alloc_func nm_alloc = malloc;
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
	if (fp == NULL){
		warn(f_warn, warnuser, "Failed to open file: %s", file);
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	uint64_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t *data = nm_alloc(sizeof(uint8_t) * size);
	if (data == NULL){
		if (f_warn)
			f_warn("Out of memory", warnuser);
		return NULL;
	}
	if (fread(data, 1, size, fp) != size){
		warn(f_warn, warnuser, "Failed to read contents of file: %s", file);
		nm_free(data);
		return NULL;
	}
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
		// search around for the chunk
		// chop off any repeated bytes
		uint64_t p_orig = p;
		while (data[p] == data[p + 1] && data[p] == data[p + 2] && data[p] == data[p + 3]){
			p += 4;
			if (p + 4 > size)
				return false;
		}
		// rewind 7 bytes and search forward 32 bytes
		p = p < 7 ? 0 : p - 7;
		for (int i = 0; i < 28 && p + 4 <= size && type < 0; i++, p++)
			type = chunk_type(data[p + 0], data[p + 1], data[p + 2], data[p + 3]);
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
	int actual_track_chunks = 0;
	int ticks_per_q;
	int tempo;
	int timesig_num;
	int timesig_den;
	int running_status;
	chunk_st chk;
	while (pos < size){
		if (!read_chunk(pos, size, data, &chk)){
			if (midi == NULL)
				warn(f_warn, warnuser, "Invalid header");
			else
				warn(f_warn, warnuser, "Unrecognized data at end of file");
			return midi;
		}
		if (chk.alignment != 0){
			warn(f_warn, warnuser, "Chunk misaligned by %d byte%s", chk.alignment,
				chk.alignment == 1 ? "" : "s");
		}
		if (midi == NULL){
			midi = nm_alloc(sizeof(nm_midi_st));
			if (midi == NULL){
				if (f_warn)
					f_warn("Out of memory", warnuser);
				return NULL;
			}
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
					ticks_per_q = ((int)data[chk.data_start + 2] << 8) | data[chk.data_start + 3];
					if (ticks_per_q & 0x8000){
						warn(f_warn, warnuser, "Unsupported timing format (SMPTE)");
						//TODO: set ticks_per_q to sane value
					}
				}
				else{
					warn(f_warn, warnuser, "Header missing division");
					//TODO: set ticks_per_q to sane value
				}
			} break;

			case 1: { // MTrk
				actual_track_chunks++;
				if (format == 0 && actual_track_chunks > 1){
					warn(f_warn, warnuser, "Format 0 expecting 1 track chunk, found more than one");
					format = 1;
				}
				if (actual_track_chunks == 1 || format == 2){
					tempo = 120;
					timesig_num = 4;
					timesig_den = 4;
				}
				running_status = -1;


				uint64_t p = chk.data_start;
				uint64_t p_end = chk.data_start + chk.data_size;
				while (p < p_end){
					// read delta as variable int
					int dt = 0;
					int len = 0;
					while (true){
						len++;
						if (len >= 5){
							warn(f_warn, warnuser, "Invalid timestamp in track %d",
								actual_track_chunks);
							goto mtrk_end;
						}
						int t = data[p++];
						if (t & 0x80){
							if (p >= p_end){
								warn(f_warn, warnuser, "Invalid timestamp in track %d",
									actual_track_chunks);
								goto mtrk_end;
							}
							dt = (dt << 7) | (t & 0x7F);
						}
						else{
							dt = (dt << 7) | t;
							break;
						}
					}

					if (p >= p_end){
						warn(f_warn, warnuser, "Missing message in track %d", actual_track_chunks);
						goto mtrk_end;
					}

					// read msg
					int msg = data[p++];
					if (msg < 0x80){
						// use running status
						if (running_status < 0){
							warn(f_warn, warnuser, "Invalid message %02X in track %d", msg,
								actual_track_chunks);
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
								actual_track_chunks);
							goto mtrk_end;
						}
						running_status = msg;
						int note = data[pos++];
						int vel = data[pos++];
						if (note >= 0x80){
							warn(f_warn, warnuser, "Bad Note-Off message (invalid note %02X) in "
								"track %d", note, actual_track_chunks);
							note ^= 0x80;
						}
						if (vel >= 0x80){
							warn(f_warn, warnuser, "Bad Note-Off message (invalid velocity %02X) "
								"in track %d", vel, actual_track_chunks);
							vel ^= 0x80;
						}
						// TODO: fire('note off', dt, msg & 0xF, note, vel)
					}
					else if (msg >= 0x90 && msg < 0xA0){ // Note-On
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Note-On message (out of data) in track %d",
								actual_track_chunks);
							goto mtrk_end;
						}
						running_status = msg;
						int note = data[pos++];
						int vel = data[pos++];
						if (note >= 0x80){
							warn(f_warn, warnuser, "Bad Note-On message (invalid note %02X) in "
								"track %d", note, actual_track_chunks);
							note ^= 0x80;
						}
						if (vel >= 0x80){
							warn(f_warn, warnuser, "Bad Note-On message (invalid velocity %02X) "
								"in track %d", vel, actual_track_chunks);
							vel ^= 0x80;
						}
						// TODO: fire('note on', dt, msg & 0xF, note, vel)
					}
					else if (msg >= 0xA0 && msg < 0xB0){ // Note Pressure
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Note Pressure message (out of data) in "
								"track %d", actual_track_chunks);
							goto mtrk_end;
						}
						running_status = msg;
						int note = data[pos++];
						int pressure = data[pos++];
						if (note >= 0x80){
							warn(f_warn, warnuser, "Bad Note Pressure message (invalid note %02X) "
								"in track %d", note, actual_track_chunks);
							note ^= 0x80;
						}
						if (pressure >= 0x80){
							warn(f_warn, warnuser, "Bad Note Pressure message (invalid pressure "
								"%02X) in track %d", pressure, actual_track_chunks);
							pressure ^= 0x80;
						}
						// TODO: fire('note pressure', dt, msg & 0xF, note, pressure)
					}
					else if (msg >= 0xB0 && msg < 0xC0){ // Control Change
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Control Change message (out of data) in "
								"track %d", actual_track_chunks);
							goto mtrk_end;
						}
						running_status = msg;
						int ctrl = data[pos++];
						int val = data[pos++];
						if (ctrl >= 0x80){
							warn(f_warn, warnuser, "Bad Control Change message (invalid control "
								" %02X) in track %d", ctrl, actual_track_chunks);
							ctrl ^= 0x80;
						}
						if (val >= 0x80){
							warn(f_warn, warnuser, "Bad Control Change message (invalid value %02X) "
								"in track %d", val, actual_track_chunks);
							val ^= 0x80;
						}
						// TODO: further validate ctrl->val relationship based on ctrl
						// TODO: fire('control change', dt, msg & 0xF, ctrl, val)
					}
					else if (msg >= 0xC0 && msg < 0xD0){ // Program Change
						if (p >= p_end){
							warn(f_warn, warnuser, "Bad Program Change message (out of data) in "
								"track %d", actual_track_chunks);
							goto mtrk_end;
						}
						running_status = msg;
						int patch = data[p++];
						if (patch >= 0x80){
							warn(f_warn, warnuser, "Bad Program Change message (invalid patch "
								"%02X) in track %d", patch, actual_track_chunks);
							patch ^= 0x80;
						}
						// TODO: fire('program change', dt, msg & 0xF, patch)
					}
					else if (msg >= 0xD0 && msg < 0xE0){ // Channel Pressure
						if (p >= p_end){
							warn(f_warn, warnuser, "Bad Channel Pressure message (out of data) in "
								"track %d", actual_track_chunks);
							goto mtrk_end;
						}
						running_status = msg;
						int pressure = data[p++];
						if (pressure >= 0x80){
							warn(f_warn, warnuser, "Bad Channel Pressure message (invalid pressure "
								"%02X) in track %d", pressure, actual_track_chunks);
							pressure ^= 0x80;
						}
						// TODO: fire('channel pressure', dt, msg & 0xF, pressure)
					}
					else if (msg >= 0xE0 && msg < 0xF0){ // Pitch Bend
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Pitch Bend message (out of data) in track "
								"%d", actual_track_chunks);
							goto mtrk_end;
						}
						running_status = msg;
						int p1 = data[p++];
						int p2 = data[p++];
						if (p1 >= 0x80){
							warn(f_warn, warnuser, "Bad Pitch Bend message (invalid lower bits "
								"%02X) in track %d", p1, actual_track_chunks);
							p1 ^= 0x80;
						}
						if (p2 >= 0x80){
							warn(f_warn, warnuser, "Bad Pitch Bend message (invalid higher bits "
								"%02X) in track %d", p2, actual_track_chunks);
							p2 ^= 0x80;
						}
						// TODO: fire('pitch bend', dt, msg & 0xF, p1 | (p2 << 7))
					}
					else if (msg == 0xF0 || msg == 0xF7){ // SysEx Event
						running_status = -1; // TODO: validate we should clear this
						// read length as a variable int
						int dl = 0;
						int len = 0;
						while (true){
							if (p >= p_end){
								warn(f_warn, warnuser, "Bad SysEx Event (out of data) in track %d",
									actual_track_chunks);
								goto mtrk_end;
							}
							len++;
							if (len >= 5){
								warn(f_warn, warnuser, "Bad SysEx Event (invalid data length) in "
									"track %d", actual_track_chunks);
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
								"track %d", actual_track_chunks);
							goto mtrk_end;
						}
						// TODO: deal with SysEx event, deal with joining packets together
						p += dl;
					}
					else if (msg == 0xFF){ // Meta Event
						running_status = -1; // TODO: validate we should clear this
						if (p + 1 >= p_end){
							warn(f_warn, warnuser, "Bad Meta Event (out of data) in track %d",
								actual_track_chunks);
							goto mtrk_end;
						}
						int type = data[p++];
						int len = data[p++];
						if (p + len > p_end){
							warn(f_warn, warnuser, "Bad Meta Event (data length too large) in "
								"track %d", actual_track_chunks);
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
								break;
							case 0x2F: // 00                End of Track
								if (p < p_end){
									uint64_t pd = p_end - p;
									warn(f_warn, warnuser, "Extra data at end of track %d: %llu "
										"byte%s", actual_track_chunks, pd, pd == 1 ? "" : "s");
								}
								goto mtrk_end;
							case 0x51: // 03 TT TT TT       Set Tempo
							case 0x54: // 05 HH MM SS RR TT SMPTE Offset
							case 0x58: // 04 NN MM LL TT    Time Signature
							case 0x59: // 02 SS MM          Key Signature
							case 0x7F: // LL data           Sequencer-Specific Meta Event
								//printf("%% meta %02X\n", type);
								break;
							default:
								warn(f_warn, warnuser, "Unknown Meta Event %02X in track %d",
									type, actual_track_chunks);
								break;
						}
						p += len;
					}
					else{
						warn(f_warn, warnuser, "Unknown message type %02X in track %d",
							msg, actual_track_chunks);
						goto mtrk_end;
					}
				}
				mtrk_end:;
			} break;

			case 2: { // XFIH
				printf("TODO: XFIH\n");
			} break;

			case 3: { // XFKM
				printf("TODO: XFKM\n");
			} break;

			default:
				warn(f_warn, warnuser, "Fatal Error: Illegal Chunk Type");
				exit(1);
				return NULL;
		}
	}
	return midi;
}

void nm_midi_free(nm_midi midi){
}
