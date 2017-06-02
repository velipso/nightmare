// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#ifndef NIGHTMARE__H
#define NIGHTMARE__H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

//
// memory management
//

typedef void *(*nm_alloc_func)(size_t size);
typedef void *(*nm_realloc_func)(void *ptr, size_t size);
typedef void (*nm_free_func)(void *ptr);
extern nm_alloc_func nm_alloc;
extern nm_realloc_func nm_realloc;
extern nm_free_func nm_free;

//
// enums
//

typedef enum nm_event_type_enum nm_event_type;
typedef enum nm_patchcat_enum nm_patchcat;
typedef enum nm_patch_enum nm_patch;
typedef enum nm_notemsg_enum nm_notemsg;

//
// structures
//

typedef struct nm_event_struct nm_event_st, *nm_event;
typedef struct nm_wevent_struct nm_wevent_st, *nm_wevent;
typedef struct nm_midi_struct nm_midi_st, *nm_midi;
typedef struct nm_sample_struct nm_sample_st, *nm_sample;
typedef struct nm_note_struct nm_note_st, *nm_note;
typedef struct nm_defpatchinf_struct nm_defpatchinf_st, *nm_defpatchinf;
typedef struct nm_defvoiceinf_struct nm_defvoiceinf_st, *nm_defvoiceinf;
typedef struct nm_voice_struct nm_voice_st, *nm_voice;
typedef struct nm_channel_struct nm_channel_st, *nm_channel;
typedef struct nm_ctx_struct nm_ctx_st, *nm_ctx;

//
// function pointers
//

typedef void (*nm_warn_func)(const char *warning, void *user);
typedef bool (*nm_synth_patch_setup_func)(nm_ctx ctx, void *synth, nm_patch patch, void *patchinf);
typedef bool (*nm_synth_render_func)(nm_ctx ctx, nm_voice voice, int len, nm_sample samples);
typedef size_t (*nm_fwrite_func)(const void *restrict src, size_t size, size_t nitems,
	void *restrict user);

//
// API
//

// functions that return bool will return true on success and false for failure (out of memory)
nm_ctx      nm_ctx_new(uint16_t ticks_per_quarternote, uint16_t channels, int voices,
	int samples_per_sec, void *synth, size_t sizeof_patchinf, size_t sizeof_voiceinf,
	nm_synth_patch_setup_func f_patch_setup, nm_synth_render_func f_render);
bool        nm_ismidi(uint8_t data[8]);
bool        nm_midi_newfile(nm_ctx ctx, const char *file, nm_warn_func f_warn, void *user);
bool        nm_midi_newbuffer(nm_ctx ctx, uint64_t size, uint8_t *data, nm_warn_func f_warn,
	void *user);
const char *nm_patch_str(nm_patch p);
nm_patchcat nm_patch_category(nm_patch p);
const char *nm_patchcat_str(nm_patchcat pc);
bool        nm_ev_nop(nm_ctx ctx, uint32_t tick);
bool        nm_ev_reset(nm_ctx ctx, uint32_t tick, uint16_t ticks_per_quarternote);
bool        nm_ev_noteon(nm_ctx ctx, uint32_t tick, uint16_t channel, uint8_t note, float vel);
bool        nm_ev_notemod(nm_ctx ctx, uint32_t tick, uint16_t channel, uint8_t note, float mod);
bool        nm_ev_noteoff(nm_ctx ctx, uint32_t tick, uint16_t channel, uint8_t note);
bool        nm_ev_chanmod(nm_ctx ctx, uint32_t tick, uint16_t channel, float mod);
bool        nm_ev_chanbend(nm_ctx ctx, uint32_t tick, uint16_t channel, float bend);
bool        nm_ev_tempo(nm_ctx ctx, uint32_t tick, uint8_t num, uint8_t den, float tempo);
bool        nm_ev_patch(nm_ctx ctx, uint32_t tick, uint16_t channel, nm_patch patch);
void        nm_defpatch(nm_patch patch, uint8_t wave, float peak, float attack, float decay,
	float sustain, float harmonic1, float harmonic2, float harmonic3, float harmonic4);
bool        nm_ctx_bake(nm_ctx ctx, uint32_t ticks);
bool        nm_ctx_bakeall(nm_ctx ctx);
void        nm_ctx_savemidi(nm_ctx ctx, void *user, nm_fwrite_func f_fwrite);
void        nm_ctx_clear(nm_ctx ctx);
void        nm_ctx_process(nm_ctx ctx, int sample_len, nm_sample samples);
void        nm_ctx_dumpev(nm_ctx ctx);
int         nm_ctx_eventsleft(nm_ctx ctx);
int         nm_ctx_voicesleft(nm_ctx ctx);
void        nm_ctx_free(nm_ctx ctx);

//
// enum details
//

enum nm_patchcat_enum {
	NM_PIANO,  // Piano
	NM_CHROM,  // Chromatic Percussion
	NM_ORGAN,  // Organ
	NM_GUITAR, // Guitar
	NM_BASS,   // Bass
	NM_STRING, // Strings & Orchestral
	NM_ENSEM,  // Ensemble
	NM_BRASS,  // Brass
	NM_REED,   // Reed
	NM_PIPE,   // Pipe
	NM_LEAD,   // Synth Lead
	NM_PAD,    // Synth Pad
	NM_SFX1,   // Sound Effects 1
	NM_ETHNIC, // Ethnic
	NM_PERC,   // Percussive
	NM_SFX2,   // Sound Effects 2
	NM_PERSND, // Percussion Sound Set
	NM__PATCHCAT_END
};

// X(enum, code, name), where code is a hex number in the format of:
// 0xPQQRR   P = 1 for Melody instrument, 2 for Percussion Sound Set instrument
//          QQ = Program Change code
//          RR = Bank code
#define NM_EACH_PATCH(X)                                                     \
	X(PIANO_ACGR    , 0x10000, "Acoustic Grand Piano"                  )  \
	X(PIANO_ACGR_WI , 0x10001, "Acoustic Grand Piano (wide)"           )  \
	X(PIANO_ACGR_DK , 0x10002, "Acoustic Grand Piano (dark)"           )  \
	X(PIANO_BRAC    , 0x10100, "Bright Acoustic Piano"                 )  \
	X(PIANO_BRAC_WI , 0x10101, "Bright Acoustic Piano (wide)"          )  \
	X(PIANO_ELGR    , 0x10200, "Electric Grand Piano"                  )  \
	X(PIANO_ELGR_WI , 0x10201, "Electric Grand Piano (wide)"           )  \
	X(PIANO_HOTO    , 0x10300, "Honky-tonk Piano"                      )  \
	X(PIANO_HOTO_WI , 0x10301, "Honky-tonk Piano (wide)"               )  \
	X(PIANO_ELE1    , 0x10400, "Electric Piano 1"                      )  \
	X(PIANO_ELE1_DT , 0x10401, "Electric Piano 1 (detuned)"            )  \
	X(PIANO_ELE1_VM , 0x10402, "Electric Piano 1 (velocity mix)"       )  \
	X(PIANO_ELE1_60 , 0x10403, "Electric Piano 1 (60's)"               )  \
	X(PIANO_ELE2    , 0x10500, "Electric Piano 2"                      )  \
	X(PIANO_ELE2_DT , 0x10501, "Electric Piano 2 (detuned)"            )  \
	X(PIANO_ELE2_VM , 0x10502, "Electric Piano 2 (velocity mix)"       )  \
	X(PIANO_ELE2_LE , 0x10503, "Electric Piano 2 (legend)"             )  \
	X(PIANO_ELE2_PH , 0x10504, "Electric Piano 2 (phase)"              )  \
	X(PIANO_HARP    , 0x10600, "Harpsichord"                           )  \
	X(PIANO_HARP_OM , 0x10601, "Harpsichord (octave mix)"              )  \
	X(PIANO_HARP_WI , 0x10602, "Harpsichord (wide)"                    )  \
	X(PIANO_HARP_KO , 0x10603, "Harpsichord (with key off)"            )  \
	X(PIANO_CLAV    , 0x10700, "Clavi"                                 )  \
	X(PIANO_CLAV_PU , 0x10701, "Clavi (pulse)"                         )  \
	X(CHROM_CELE    , 0x10800, "Celesta"                               )  \
	X(CHROM_GLOC    , 0x10900, "Glockenspiel"                          )  \
	X(CHROM_MUBO    , 0x10A00, "Music Box"                             )  \
	X(CHROM_VIPH    , 0x10B00, "Vibraphone"                            )  \
	X(CHROM_VIPH_WI , 0x10B01, "Vibraphone (wide)"                     )  \
	X(CHROM_MARI    , 0x10C00, "Marimba"                               )  \
	X(CHROM_MARI_WI , 0x10C01, "Marimba (wide)"                        )  \
	X(CHROM_XYLO    , 0x10D00, "Xylophone"                             )  \
	X(CHROM_BELL_TU , 0x10E00, "Bells (tubular)"                       )  \
	X(CHROM_BELL_CH , 0x10E01, "Bells (church)"                        )  \
	X(CHROM_BELL_CA , 0x10E02, "Bells (carillon)"                      )  \
	X(CHROM_DULC    , 0x10F00, "Dulcimer"                              )  \
	X(ORGAN_DRAW    , 0x11000, "Drawbar Organ"                         )  \
	X(ORGAN_DRAW_DT , 0x11001, "Drawbar Organ (detuned)"               )  \
	X(ORGAN_DRAW_60 , 0x11002, "Drawbar Organ (60's)"                  )  \
	X(ORGAN_DRAW_AL , 0x11003, "Drawbar Organ (alternative)"           )  \
	X(ORGAN_PERC    , 0x11100, "Percussive Organ"                      )  \
	X(ORGAN_PERC_DT , 0x11101, "Percussive Organ (detuned)"            )  \
	X(ORGAN_PERC_2  , 0x11102, "Percussive Organ 2"                    )  \
	X(ORGAN_ROCK    , 0x11200, "Rock Organ"                            )  \
	X(ORGAN_CHUR    , 0x11300, "Church Organ"                          )  \
	X(ORGAN_CHUR_OM , 0x11301, "Church Organ (octave mix)"             )  \
	X(ORGAN_CHUR_DT , 0x11302, "Church Organ (detuned)"                )  \
	X(ORGAN_REED    , 0x11400, "Reed Organ"                            )  \
	X(ORGAN_REED_PU , 0x11401, "Reed Organ (puff)"                     )  \
	X(ORGAN_ACCO    , 0x11500, "Accordion"                             )  \
	X(ORGAN_ACCO_2  , 0x11501, "Accordion 2"                           )  \
	X(ORGAN_HARM    , 0x11600, "Harmonica"                             )  \
	X(ORGAN_TANG    , 0x11700, "Tango Accordion"                       )  \
	X(GUITAR_NYLO   , 0x11800, "Nylon Acoustic Guitar"                 )  \
	X(GUITAR_NYLO_UK, 0x11801, "Nylon Acoustic Guitar (ukulele)"       )  \
	X(GUITAR_NYLO_KO, 0x11802, "Nylon Acoustic Guitar (key off)"       )  \
	X(GUITAR_NYLO_AL, 0x11803, "Nylon Acoustic Guitar (alternative)"   )  \
	X(GUITAR_STEE   , 0x11900, "Steel Acoustic Guitar"                 )  \
	X(GUITAR_STEE_12, 0x11901, "Steel Acoustic Guitar (12-string)"     )  \
	X(GUITAR_STEE_MA, 0x11902, "Steel Acoustic Guitar (mandolin)"      )  \
	X(GUITAR_STEE_BS, 0x11903, "Steel Acoustic Guitar (body sound)"    )  \
	X(GUITAR_JAZZ   , 0x11A00, "Jazz Electric Guitar"                  )  \
	X(GUITAR_JAZZ_PS, 0x11A01, "Jazz Electric Guitar (pedal steel)"    )  \
	X(GUITAR_CLEA   , 0x11B00, "Clean Electric Guitar"                 )  \
	X(GUITAR_CLEA_DT, 0x11B01, "Clean Electric Guitar (detuned)"       )  \
	X(GUITAR_CLEA_MT, 0x11B02, "Clean Electric Guitar (midtone)"       )  \
	X(GUITAR_MUTE   , 0x11C00, "Muted Electric Guitar"                 )  \
	X(GUITAR_MUTE_FC, 0x11C01, "Muted Electric Guitar (funky cutting)" )  \
	X(GUITAR_MUTE_VS, 0x11C02, "Muted Electric Guitar (velo-sw)"       )  \
	X(GUITAR_MUTE_JM, 0x11C03, "Muted Electric Guitar (jazz man)"      )  \
	X(GUITAR_OVER   , 0x11D00, "Overdriven Guitar"                     )  \
	X(GUITAR_OVER_PI, 0x11D01, "Overdriven Guitar (pinch)"             )  \
	X(GUITAR_DIST   , 0x11E00, "Distortion Guitar"                     )  \
	X(GUITAR_DIST_FB, 0x11E01, "Distortion Guitar (feedback)"          )  \
	X(GUITAR_DIST_RH, 0x11E02, "Distortion Guitar (rhythm)"            )  \
	X(GUITAR_HARM   , 0x11F00, "Guitar Harmonics"                      )  \
	X(GUITAR_HARM_FB, 0x11F01, "Guitar Harmonics (feedback)"           )  \
	X(BASS_ACOU     , 0x12000, "Acoustic Bass"                         )  \
	X(BASS_FING     , 0x12100, "Finger Electric Bass"                  )  \
	X(BASS_FING_SL  , 0x12101, "Finger Electric Bass (slap)"           )  \
	X(BASS_PICK     , 0x12200, "Pick Electric Bass"                    )  \
	X(BASS_FRET     , 0x12300, "Fretless Bass"                         )  \
	X(BASS_SLP1     , 0x12400, "Slap Bass 1"                           )  \
	X(BASS_SLP2     , 0x12500, "Slap Bass 2"                           )  \
	X(BASS_SYN1     , 0x12600, "Synth Bass 1"                          )  \
	X(BASS_SYN1_WA  , 0x12601, "Synth Bass 1 (warm)"                   )  \
	X(BASS_SYN1_RE  , 0x12602, "Synth Bass 1 (resonance)"              )  \
	X(BASS_SYN1_CL  , 0x12603, "Synth Bass 1 (clavi)"                  )  \
	X(BASS_SYN1_HA  , 0x12604, "Synth Bass 1 (hammer)"                 )  \
	X(BASS_SYN2     , 0x12700, "Synth Bass 2"                          )  \
	X(BASS_SYN2_AT  , 0x12701, "Synth Bass 2 (attack)"                 )  \
	X(BASS_SYN2_RU  , 0x12702, "Synth Bass 2 (rubber)"                 )  \
	X(BASS_SYN2_AP  , 0x12703, "Synth Bass 2 (attack pulse)"           )  \
	X(STRING_VILN   , 0x12800, "Violin"                                )  \
	X(STRING_VILN_SA, 0x12801, "Violin (slow attack)"                  )  \
	X(STRING_VILA   , 0x12900, "Viola"                                 )  \
	X(STRING_CELL   , 0x12A00, "Cello"                                 )  \
	X(STRING_CONT   , 0x12B00, "Contrabass"                            )  \
	X(STRING_TREM   , 0x12C00, "Tremolo Strings"                       )  \
	X(STRING_PIZZ   , 0x12D00, "Pizzicato Strings"                     )  \
	X(STRING_HARP   , 0x12E00, "Orchestral Harp"                       )  \
	X(STRING_HARP_YC, 0x12E01, "Orchestral Harp (yang chin)"           )  \
	X(STRING_TIMP   , 0x12F00, "Timpani"                               )  \
	X(ENSEM_STR1    , 0x13000, "String Ensembles 1"                    )  \
	X(ENSEM_STR1_SB , 0x13001, "String Ensembles 1 (strings and brass)")  \
	X(ENSEM_STR1_60 , 0x13002, "String Ensembles 1 (60s strings)"      )  \
	X(ENSEM_STR2    , 0x13100, "String Ensembles 2"                    )  \
	X(ENSEM_SYN1    , 0x13200, "SynthStrings 1"                        )  \
	X(ENSEM_SYN1_AL , 0x13201, "SynthStrings 1 (alternative)"          )  \
	X(ENSEM_SYN2    , 0x13300, "SynthStrings 2"                        )  \
	X(ENSEM_CHOI    , 0x13400, "Choir Aahs"                            )  \
	X(ENSEM_CHOI_AL , 0x13401, "Choir Aahs (alternative)"              )  \
	X(ENSEM_VOIC    , 0x13500, "Voice Oohs"                            )  \
	X(ENSEM_VOIC_HM , 0x13501, "Voice Oohs (humming)"                  )  \
	X(ENSEM_SYVO    , 0x13600, "Synth Voice"                           )  \
	X(ENSEM_SYVO_AN , 0x13601, "Synth Voice (analog)"                  )  \
	X(ENSEM_ORHI    , 0x13700, "Orchestra Hit"                         )  \
	X(ENSEM_ORHI_BP , 0x13701, "Orchestra Hit (bass hit plus)"         )  \
	X(ENSEM_ORHI_6  , 0x13702, "Orchestra Hit (6th)"                   )  \
	X(ENSEM_ORHI_EU , 0x13703, "Orchestra Hit (euro)"                  )  \
	X(BRASS_TRUM    , 0x13800, "Trumpet"                               )  \
	X(BRASS_TRUM_DS , 0x13801, "Trumpet (dark soft)"                   )  \
	X(BRASS_TROM    , 0x13900, "Trombone"                              )  \
	X(BRASS_TROM_AL , 0x13901, "Trombone (alternative)"                )  \
	X(BRASS_TROM_BR , 0x13902, "Trombone (bright)"                     )  \
	X(BRASS_TUBA    , 0x13A00, "Tuba"                                  )  \
	X(BRASS_MUTR    , 0x13B00, "Muted Trumpet"                         )  \
	X(BRASS_MUTR_AL , 0x13B01, "Muted Trumpet (alternative)"           )  \
	X(BRASS_FRHO    , 0x13C00, "French Horn"                           )  \
	X(BRASS_FRHO_WA , 0x13C01, "French Horn (warm)"                    )  \
	X(BRASS_BRSE    , 0x13D00, "Brass Section"                         )  \
	X(BRASS_BRSE_OM , 0x13D01, "Brass Section (octave mix)"            )  \
	X(BRASS_SBR1    , 0x13E00, "Synth Brass 1"                         )  \
	X(BRASS_SBR1_AL , 0x13E01, "Synth Brass 1 (alternative)"           )  \
	X(BRASS_SBR1_AN , 0x13E02, "Synth Brass 1 (analog)"                )  \
	X(BRASS_SBR1_JU , 0x13E03, "Synth Brass 1 (jump)"                  )  \
	X(BRASS_SBR2    , 0x13F00, "Synth Brass 2"                         )  \
	X(BRASS_SBR2_AL , 0x13F01, "Synth Brass 2 (alternative)"           )  \
	X(BRASS_SBR2_AN , 0x13F02, "Synth Brass 2 (analog)"                )  \
	X(REED_SOSA     , 0x14000, "Soprano Sax"                           )  \
	X(REED_ALSA     , 0x14100, "Alto Sax"                              )  \
	X(REED_TESA     , 0x14200, "Tenor Sax"                             )  \
	X(REED_BASA     , 0x14300, "Baritone Sax"                          )  \
	X(REED_OBOE     , 0x14400, "Oboe"                                  )  \
	X(REED_ENHO     , 0x14500, "English Horn"                          )  \
	X(REED_BASS     , 0x14600, "Bassoon"                               )  \
	X(REED_CLAR     , 0x14700, "Clarinet"                              )  \
	X(PIPE_PICC     , 0x14800, "Piccolo"                               )  \
	X(PIPE_FLUT     , 0x14900, "Flute"                                 )  \
	X(PIPE_RECO     , 0x14A00, "Recorder"                              )  \
	X(PIPE_PAFL     , 0x14B00, "Pan Flute"                             )  \
	X(PIPE_BLBO     , 0x14C00, "Blown Bottle"                          )  \
	X(PIPE_SHAK     , 0x14D00, "Shakuhachi"                            )  \
	X(PIPE_WHIS     , 0x14E00, "Whistle"                               )  \
	X(PIPE_OCAR     , 0x14F00, "Ocarina"                               )  \
	X(LEAD_OSC1     , 0x15000, "Oscilliator 1"                         )  \
	X(LEAD_OSC1_SQ  , 0x15001, "Oscilliator 1 (square)"                )  \
	X(LEAD_OSC1_SI  , 0x15002, "Oscilliator 1 (sine)"                  )  \
	X(LEAD_OSC2     , 0x15100, "Oscilliator 2"                         )  \
	X(LEAD_OSC2_SA  , 0x15101, "Oscilliator 2 (sawtooth)"              )  \
	X(LEAD_OSC2_SP  , 0x15102, "Oscilliator 2 (saw + pulse)"           )  \
	X(LEAD_OSC2_DS  , 0x15103, "Oscilliator 2 (double sawtooth)"       )  \
	X(LEAD_OSC2_AN  , 0x15104, "Oscilliator 2 (sequenced analog)"      )  \
	X(LEAD_CALL     , 0x15200, "Calliope"                              )  \
	X(LEAD_CHIF     , 0x15300, "Chiff"                                 )  \
	X(LEAD_CHAR     , 0x15400, "Charang"                               )  \
	X(LEAD_CHAR_WL  , 0x15401, "Charang (wire lead)"                   )  \
	X(LEAD_VOIC     , 0x15500, "Voice"                                 )  \
	X(LEAD_FIFT     , 0x15600, "Fifths"                                )  \
	X(LEAD_BALE     , 0x15700, "Bass + Lead"                           )  \
	X(LEAD_BALE_SW  , 0x15701, "Bass + Lead (soft wrl)"                )  \
	X(PAD_NEAG      , 0x15800, "New Age"                               )  \
	X(PAD_WARM      , 0x15900, "Warm"                                  )  \
	X(PAD_WARM_SI   , 0x15901, "Warm (sine)"                           )  \
	X(PAD_POLY      , 0x15A00, "Polysynth"                             )  \
	X(PAD_CHOI      , 0x15B00, "Choir"                                 )  \
	X(PAD_CHOI_IT   , 0x15B01, "Choir (itopia)"                        )  \
	X(PAD_BOWE      , 0x15C00, "Bowed"                                 )  \
	X(PAD_META      , 0x15D00, "Metallic"                              )  \
	X(PAD_HALO      , 0x15E00, "Halo"                                  )  \
	X(PAD_SWEE      , 0x15F00, "Sweep"                                 )  \
	X(SFX1_RAIN     , 0x16000, "Rain"                                  )  \
	X(SFX1_SOTR     , 0x16100, "Soundtrack"                            )  \
	X(SFX1_CRYS     , 0x16200, "Crystal"                               )  \
	X(SFX1_CRYS_MA  , 0x16201, "Crystal (mallet)"                      )  \
	X(SFX1_ATMO     , 0x16300, "Atmosphere"                            )  \
	X(SFX1_BRIG     , 0x16400, "Brightness"                            )  \
	X(SFX1_GOBL     , 0x16500, "Goblins"                               )  \
	X(SFX1_ECHO     , 0x16600, "Echoes"                                )  \
	X(SFX1_ECHO_BE  , 0x16601, "Echoes (bell)"                         )  \
	X(SFX1_ECHO_PA  , 0x16602, "Echoes (pan)"                          )  \
	X(SFX1_SCFI     , 0x16700, "Sci-Fi"                                )  \
	X(ETHNIC_SITA   , 0x16000, "Sitar"                                 )  \
	X(ETHNIC_SITA_BE, 0x16001, "Sitar (bend)"                          )  \
	X(ETHNIC_BANJ   , 0x16100, "Banjo"                                 )  \
	X(ETHNIC_SHAM   , 0x16200, "Shamisen"                              )  \
	X(ETHNIC_KOTO   , 0x16300, "Koto"                                  )  \
	X(ETHNIC_KOTO_TA, 0x16301, "Koto (taisho)"                         )  \
	X(ETHNIC_KALI   , 0x16400, "Kalimba"                               )  \
	X(ETHNIC_BAPI   , 0x16500, "Bag Pipe"                              )  \
	X(ETHNIC_FIDD   , 0x16600, "Fiddle"                                )  \
	X(ETHNIC_SHAN   , 0x16700, "Shanai"                                )  \
	X(PERC_TIBE     , 0x17000, "Tinkle Bell"                           )  \
	X(PERC_AGOG     , 0x17100, "Agogo"                                 )  \
	X(PERC_STDR     , 0x17200, "Steel Drums"                           )  \
	X(PERC_WOOD     , 0x17300, "Woodblock"                             )  \
	X(PERC_WOOD_CA  , 0x17301, "Woodblock (castanets)"                 )  \
	X(PERC_TADR     , 0x17400, "Taiko Drum"                            )  \
	X(PERC_TADR_CB  , 0x17401, "Taiko Drum (concert bass)"             )  \
	X(PERC_METO     , 0x17500, "Melodic Tom"                           )  \
	X(PERC_METO_PO  , 0x17501, "Melodic Tom (power)"                   )  \
	X(PERC_SYDR     , 0x17600, "Synth Drum"                            )  \
	X(PERC_SYDR_RB  , 0x17601, "Synth Drum (rhythm box tom)"           )  \
	X(PERC_SYDR_EL  , 0x17602, "Synth Drum (electric)"                 )  \
	X(PERC_RECY     , 0x17700, "Reverse Cymbal"                        )  \
	X(SFX2_G0_GUFR  , 0x17800, "Guitar Fret Noise"                     )  \
	X(SFX2_G0_GUCU  , 0x17801, "Guitar Cutting Noise"                  )  \
	X(SFX2_G0_STSL  , 0x17802, "Acoustic Bass String Slap"             )  \
	X(SFX2_G1_BRNO  , 0x17900, "Breath Noise"                          )  \
	X(SFX2_G1_FLKC  , 0x17901, "Flute Key Click"                       )  \
	X(SFX2_G2_SEAS  , 0x17A00, "Seashore"                              )  \
	X(SFX2_G2_RAIN  , 0x17A01, "Rain"                                  )  \
	X(SFX2_G2_THUN  , 0x17A02, "Thunder"                               )  \
	X(SFX2_G2_WIND  , 0x17A03, "Wind"                                  )  \
	X(SFX2_G2_STRE  , 0x17A04, "Stream"                                )  \
	X(SFX2_G2_BUBB  , 0x17A05, "Bubble"                                )  \
	X(SFX2_G3_BTW1  , 0x17B00, "Bird Tweet 1"                          )  \
	X(SFX2_G3_DOG   , 0x17B01, "Dog"                                   )  \
	X(SFX2_G3_HOGA  , 0x17B02, "Horse Gallop"                          )  \
	X(SFX2_G3_BTW2  , 0x17B03, "Bird Tweet 2"                          )  \
	X(SFX2_G4_TEL1  , 0x17C00, "Telephone Ring 1"                      )  \
	X(SFX2_G4_TEL2  , 0x17C01, "Telephone Ring 2"                      )  \
	X(SFX2_G4_DOCR  , 0x17C02, "Door Creaking"                         )  \
	X(SFX2_G4_DOOR  , 0x17C03, "Door"                                  )  \
	X(SFX2_G4_SCRA  , 0x17C04, "Scratch"                               )  \
	X(SFX2_G4_WICH  , 0x17C05, "Wind Chime"                            )  \
	X(SFX2_G5_HELI  , 0x17D00, "Helicopter"                            )  \
	X(SFX2_G5_CAEN  , 0x17D01, "Car Engine"                            )  \
	X(SFX2_G5_CAST  , 0x17D02, "Car Stop"                              )  \
	X(SFX2_G5_CAPA  , 0x17D03, "Car Pass"                              )  \
	X(SFX2_G5_CACR  , 0x17D04, "Car Crash"                             )  \
	X(SFX2_G5_SIRE  , 0x17D05, "Siren"                                 )  \
	X(SFX2_G5_TRAI  , 0x17D06, "Train"                                 )  \
	X(SFX2_G5_JETP  , 0x17D07, "Jetplane"                              )  \
	X(SFX2_G5_STAR  , 0x17D08, "Starship"                              )  \
	X(SFX2_G5_BUNO  , 0x17D09, "Burst Noise"                           )  \
	X(SFX2_G6_APPL  , 0x17E00, "Applause"                              )  \
	X(SFX2_G6_LAUG  , 0x17E01, "Laughing"                              )  \
	X(SFX2_G6_SCRE  , 0x17E02, "Screaming"                             )  \
	X(SFX2_G6_PUNC  , 0x17E03, "Punch"                                 )  \
	X(SFX2_G6_HEBE  , 0x17E04, "Heart Beat"                            )  \
	X(SFX2_G6_FOOT  , 0x17E05, "Footsteps"                             )  \
	X(SFX2_G7_GUSH  , 0x17F00, "Gun Shot"                              )  \
	X(SFX2_G7_MAGU  , 0x17F01, "Machine Gun"                           )  \
	X(SFX2_G7_LAGU  , 0x17F02, "Laser Gun"                             )  \
	X(SFX2_G7_EXPL  , 0x17F03, "Explosion"                             )  \
	X(PERSND_STAN   , 0x20000, "PSS Standard"                          )  \
	X(PERSND_ROOM   , 0x20800, "PSS Room"                              )  \
	X(PERSND_POWE   , 0x21000, "PSS Power"                             )  \
	X(PERSND_ELEC   , 0x21800, "PSS Electronic"                        )  \
	X(PERSND_ANLG   , 0x21900, "PSS Analog"                            )  \
	X(PERSND_JAZZ   , 0x22000, "PSS Jazz"                              )  \
	X(PERSND_BRUS   , 0x22800, "PSS Brush"                             )  \
	X(PERSND_ORCH   , 0x23000, "PSS Orchestra"                         )  \
	X(PERSND_SNFX   , 0x23800, "PSS Sound Effects"                     )
;
enum nm_patch_enum {
	#define X(en, code, str) NM_ ## en,
	NM_EACH_PATCH(X)
	#undef X
	NM__PATCH_END
};

enum nm_event_type_enum {
	NM_EV_NOP,
	NM_EV_RESET,
	NM_EV_NOTEON,
	NM_EV_NOTEMOD,
	NM_EV_NOTEOFF,
	NM_EV_CHANMOD,
	NM_EV_CHANBEND,
	NM_EV_TEMPO,
	NM_EV_PATCH
};

//
// structure details
//

struct nm_event_struct {
	uint32_t tick;
	uint8_t type; // nm_event_type
	uint8_t data1;
	uint16_t channel;
	union {
		float data2f;
		uint32_t data2i;
	} u;
};

struct nm_wevent_struct {
	nm_wevent next;
	nm_event_st ev;
};

struct nm_defpatchinf_struct {
	float peak;
	float attack;
	float decay;
	float sustain;
	float harmonic1;
	float harmonic2;
	float harmonic3;
	float harmonic4;
	float (*f_wave)(float i);
};

struct nm_defvoiceinf_struct {
	float fade;
	float dfade;
};

struct nm_voice_struct {
	nm_voice next;
	nm_synth_render_func f_render;
	void *synth;
	void *patchinf;
	void *voiceinf;
	nm_patch patch;
	uint8_t note;
	uint16_t channel;
	int samptot;
	int cyctot;
	float vel;
	float mod;
	float cyc;
	float dcyc;
	bool down;
	bool released;
};

struct nm_channel_struct {
	nm_patch patch;
	float bend;
	float mod;
};

struct nm_ctx_struct {
	void *synth;
	nm_synth_patch_setup_func f_patch_setup;
	nm_synth_render_func f_render;
	nm_event events;
	nm_wevent wevents; // the wevent at the start of the linked-list
	nm_wevent last_wevent; // the wevent at the end of the linked-list
	nm_wevent ins_wevent; // the last wevent that was inserted
	int ev_size;
	int ev_read;
	int ev_write;
	float tempo;
	uint8_t ts_num;
	uint8_t ts_den;
	uint16_t ticks_per_quarternote;
	int samples_per_sec;
	double ticks;
	uint32_t last_bake_ticks;
	double samples_per_tick;
	nm_voice voices_free;
	nm_voice voices_used;
	nm_channel channels;
	uint16_t channel_count;
	uint8_t patchinf_custom[NM__PATCH_END]; // 0 = unallocated/default, 1 = custom synth
	void *patchinf[NM__PATCH_END];
	int notecnt[128];
};

struct nm_sample_struct {
	float L;
	float R;
};

#endif // NIGHTMARE__H
