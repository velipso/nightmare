// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#ifndef NIGHTMARE__H

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
typedef struct nm_midi_struct nm_midi_st, *nm_midi;
typedef struct nm_sample_struct nm_sample_st, *nm_sample;
typedef struct nm_note_struct nm_note_st, *nm_note;
typedef struct nm_voice_struct nm_voice_st;
typedef struct nm_ctx_struct nm_ctx_st, *nm_ctx;
typedef struct nm_channel_struct nm_channel_st, *nm_channel;

//
// function pointers
//

typedef void (*nm_warn_func)(const char *warning, void *user);

//
// API
//

// old api

nm_midi     nm_midi_newfile(const char *file, nm_warn_func f_warn, void *user);
nm_midi     nm_midi_newbuffer(uint64_t size, uint8_t *data, nm_warn_func f_warn, void *user);
const char *nm_event_type_str(nm_event_type type);
const char *nm_patch_str(nm_patch p);
nm_patchcat nm_patch_category(nm_patch p);
const char *nm_patchcat_str(nm_patchcat pc);
nm_ctx      nm_ctx_new(nm_midi midi, int track, int voices, int samples_per_sec);
int         nm_ctx_process(nm_ctx ctx, int sample_len, nm_sample samples);
void        nm_ctx_free(nm_ctx ctx);
void        nm_midi_free(nm_midi midi);

//
// enum details
//

enum nm_event_type_enum {
	NM_NOTEOFF,
	NM_NOTEON,
	NM_NOTEPRESSURE,
	NM_CONTROLCHANGE,
	NM_PROGRAMCHANGE,
	NM_CHANNELPRESSURE,
	NM_PITCHBEND,
	NM_TEMPO,
	NM_TIMESIG
};

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
	X(NM_PIANO_ACGR    , 0x10000, "Acoustic Grand Piano"                  )  \
	X(NM_PIANO_ACGR_WI , 0x10001, "Acoustic Grand Piano (wide)"           )  \
	X(NM_PIANO_ACGR_DK , 0x10002, "Acoustic Grand Piano (dark)"           )  \
	X(NM_PIANO_BRAC    , 0x10100, "Bright Acoustic Piano"                 )  \
	X(NM_PIANO_BRAC_WI , 0x10101, "Bright Acoustic Piano (wide)"          )  \
	X(NM_PIANO_ELGR    , 0x10200, "Electric Grand Piano"                  )  \
	X(NM_PIANO_ELGR_WI , 0x10201, "Electric Grand Piano (wide)"           )  \
	X(NM_PIANO_HOTO    , 0x10300, "Honky-tonk Piano"                      )  \
	X(NM_PIANO_HOTO_WI , 0x10301, "Honky-tonk Piano (wide)"               )  \
	X(NM_PIANO_ELE1    , 0x10400, "Electric Piano 1"                      )  \
	X(NM_PIANO_ELE1_DT , 0x10401, "Electric Piano 1 (detuned)"            )  \
	X(NM_PIANO_ELE1_VM , 0x10402, "Electric Piano 1 (velocity mix)"       )  \
	X(NM_PIANO_ELE1_60 , 0x10403, "Electric Piano 1 (60's)"               )  \
	X(NM_PIANO_ELE2    , 0x10500, "Electric Piano 2"                      )  \
	X(NM_PIANO_ELE2_DT , 0x10501, "Electric Piano 2 (detuned)"            )  \
	X(NM_PIANO_ELE2_VM , 0x10502, "Electric Piano 2 (velocity mix)"       )  \
	X(NM_PIANO_ELE2_LE , 0x10503, "Electric Piano 2 (legend)"             )  \
	X(NM_PIANO_ELE2_PH , 0x10504, "Electric Piano 2 (phase)"              )  \
	X(NM_PIANO_HARP    , 0x10600, "Harpsichord"                           )  \
	X(NM_PIANO_HARP_OM , 0x10601, "Harpsichord (octave mix)"              )  \
	X(NM_PIANO_HARP_WI , 0x10602, "Harpsichord (wide)"                    )  \
	X(NM_PIANO_HARP_KO , 0x10603, "Harpsichord (with key off)"            )  \
	X(NM_PIANO_CLAV    , 0x10700, "Clavi"                                 )  \
	X(NM_PIANO_CLAV_PU , 0x10701, "Clavi (pulse)"                         )  \
	X(NM_CHROM_CELE    , 0x10800, "Celesta"                               )  \
	X(NM_CHROM_GLOC    , 0x10900, "Glockenspiel"                          )  \
	X(NM_CHROM_MUBO    , 0x10A00, "Music Box"                             )  \
	X(NM_CHROM_VIPH    , 0x10B00, "Vibraphone"                            )  \
	X(NM_CHROM_VIPH_WI , 0x10B01, "Vibraphone (wide)"                     )  \
	X(NM_CHROM_MARI    , 0x10C00, "Marimba"                               )  \
	X(NM_CHROM_MARI_WI , 0x10C01, "Marimba (wide)"                        )  \
	X(NM_CHROM_XYLO    , 0x10D00, "Xylophone"                             )  \
	X(NM_CHROM_BELL_TU , 0x10E00, "Bells (tubular)"                       )  \
	X(NM_CHROM_BELL_CH , 0x10E01, "Bells (church)"                        )  \
	X(NM_CHROM_BELL_CA , 0x10E02, "Bells (carillon)"                      )  \
	X(NM_CHROM_DULC    , 0x10F00, "Dulcimer"                              )  \
	X(NM_ORGAN_DRAW    , 0x11000, "Drawbar Organ"                         )  \
	X(NM_ORGAN_DRAW_DT , 0x11001, "Drawbar Organ (detuned)"               )  \
	X(NM_ORGAN_DRAW_60 , 0x11002, "Drawbar Organ (60's)"                  )  \
	X(NM_ORGAN_DRAW_AL , 0x11003, "Drawbar Organ (alternative)"           )  \
	X(NM_ORGAN_PERC    , 0x11100, "Percussive Organ"                      )  \
	X(NM_ORGAN_PERC_DT , 0x11101, "Percussive Organ (detuned)"            )  \
	X(NM_ORGAN_PERC_2  , 0x11102, "Percussive Organ 2"                    )  \
	X(NM_ORGAN_ROCK    , 0x11200, "Rock Organ"                            )  \
	X(NM_ORGAN_CHUR    , 0x11300, "Church Organ"                          )  \
	X(NM_ORGAN_CHUR_OM , 0x11301, "Church Organ (octave mix)"             )  \
	X(NM_ORGAN_CHUR_DT , 0x11302, "Church Organ (detuned)"                )  \
	X(NM_ORGAN_REED    , 0x11400, "Reed Organ"                            )  \
	X(NM_ORGAN_REED_PU , 0x11401, "Reed Organ (puff)"                     )  \
	X(NM_ORGAN_ACCO    , 0x11500, "Accordion"                             )  \
	X(NM_ORGAN_ACCO_2  , 0x11501, "Accordion 2"                           )  \
	X(NM_ORGAN_HARM    , 0x11600, "Harmonica"                             )  \
	X(NM_ORGAN_TANG    , 0x11700, "Tango Accordion"                       )  \
	X(NM_GUITAR_NYLO   , 0x11800, "Nylon Acoustic Guitar"                 )  \
	X(NM_GUITAR_NYLO_UK, 0x11801, "Nylon Acoustic Guitar (ukulele)"       )  \
	X(NM_GUITAR_NYLO_KO, 0x11802, "Nylon Acoustic Guitar (key off)"       )  \
	X(NM_GUITAR_NYLO_AL, 0x11803, "Nylon Acoustic Guitar (alternative)"   )  \
	X(NM_GUITAR_STEE   , 0x11900, "Steel Acoustic Guitar"                 )  \
	X(NM_GUITAR_STEE_12, 0x11901, "Steel Acoustic Guitar (12-string)"     )  \
	X(NM_GUITAR_STEE_MA, 0x11902, "Steel Acoustic Guitar (mandolin)"      )  \
	X(NM_GUITAR_STEE_BS, 0x11903, "Steel Acoustic Guitar (body sound)"    )  \
	X(NM_GUITAR_JAZZ   , 0x11A00, "Jazz Electric Guitar"                  )  \
	X(NM_GUITAR_JAZZ_PS, 0x11A01, "Jazz Electric Guitar (pedal steel)"    )  \
	X(NM_GUITAR_CLEA   , 0x11B00, "Clean Electric Guitar"                 )  \
	X(NM_GUITAR_CLEA_DT, 0x11B01, "Clean Electric Guitar (detuned)"       )  \
	X(NM_GUITAR_CLEA_MT, 0x11B02, "Clean Electric Guitar (midtone)"       )  \
	X(NM_GUITAR_MUTE   , 0x11C00, "Muted Electric Guitar"                 )  \
	X(NM_GUITAR_MUTE_FC, 0x11C01, "Muted Electric Guitar (funky cutting)" )  \
	X(NM_GUITAR_MUTE_VS, 0x11C02, "Muted Electric Guitar (velo-sw)"       )  \
	X(NM_GUITAR_MUTE_JM, 0x11C03, "Muted Electric Guitar (jazz man)"      )  \
	X(NM_GUITAR_OVER   , 0x11D00, "Overdriven Guitar"                     )  \
	X(NM_GUITAR_OVER_PI, 0x11D01, "Overdriven Guitar (pinch)"             )  \
	X(NM_GUITAR_DIST   , 0x11E00, "Distortion Guitar"                     )  \
	X(NM_GUITAR_DIST_FB, 0x11E01, "Distortion Guitar (feedback)"          )  \
	X(NM_GUITAR_DIST_RH, 0x11E02, "Distortion Guitar (rhythm)"            )  \
	X(NM_GUITAR_HARM   , 0x11F00, "Guitar Harmonics"                      )  \
	X(NM_GUITAR_HARM_FB, 0x11F01, "Guitar Harmonics (feedback)"           )  \
	X(NM_BASS_ACOU     , 0x12000, "Acoustic Bass"                         )  \
	X(NM_BASS_FING     , 0x12100, "Finger Electric Bass"                  )  \
	X(NM_BASS_FING_SL  , 0x12101, "Finger Electric Bass (slap)"           )  \
	X(NM_BASS_PICK     , 0x12200, "Pick Electric Bass"                    )  \
	X(NM_BASS_FRET     , 0x12300, "Fretless Bass"                         )  \
	X(NM_BASS_SLP1     , 0x12400, "Slap Bass 1"                           )  \
	X(NM_BASS_SLP2     , 0x12500, "Slap Bass 2"                           )  \
	X(NM_BASS_SYN1     , 0x12600, "Synth Bass 1"                          )  \
	X(NM_BASS_SYN1_WA  , 0x12601, "Synth Bass 1 (warm)"                   )  \
	X(NM_BASS_SYN1_RE  , 0x12602, "Synth Bass 1 (resonance)"              )  \
	X(NM_BASS_SYN1_CL  , 0x12603, "Synth Bass 1 (clavi)"                  )  \
	X(NM_BASS_SYN1_HA  , 0x12604, "Synth Bass 1 (hammer)"                 )  \
	X(NM_BASS_SYN2     , 0x12700, "Synth Bass 2"                          )  \
	X(NM_BASS_SYN2_AT  , 0x12701, "Synth Bass 2 (attack)"                 )  \
	X(NM_BASS_SYN2_RU  , 0x12702, "Synth Bass 2 (rubber)"                 )  \
	X(NM_BASS_SYN2_AP  , 0x12703, "Synth Bass 2 (attack pulse)"           )  \
	X(NM_STRING_VILN   , 0x12800, "Violin"                                )  \
	X(NM_STRING_VILN_SA, 0x12801, "Violin (slow attack)"                  )  \
	X(NM_STRING_VILA   , 0x12900, "Viola"                                 )  \
	X(NM_STRING_CELL   , 0x12A00, "Cello"                                 )  \
	X(NM_STRING_CONT   , 0x12B00, "Contrabass"                            )  \
	X(NM_STRING_TREM   , 0x12C00, "Tremolo Strings"                       )  \
	X(NM_STRING_PIZZ   , 0x12D00, "Pizzicato Strings"                     )  \
	X(NM_STRING_HARP   , 0x12E00, "Orchestral Harp"                       )  \
	X(NM_STRING_HARP_YC, 0x12E01, "Orchestral Harp (yang chin)"           )  \
	X(NM_STRING_TIMP   , 0x12F00, "Timpani"                               )  \
	X(NM_ENSEM_STR1    , 0x13000, "String Ensembles 1"                    )  \
	X(NM_ENSEM_STR1_SB , 0x13001, "String Ensembles 1 (strings and brass)")  \
	X(NM_ENSEM_STR1_60 , 0x13002, "String Ensembles 1 (60s strings)"      )  \
	X(NM_ENSEM_STR2    , 0x13100, "String Ensembles 2"                    )  \
	X(NM_ENSEM_SYN1    , 0x13200, "SynthStrings 1"                        )  \
	X(NM_ENSEM_SYN1_AL , 0x13201, "SynthStrings 1 (alternative)"          )  \
	X(NM_ENSEM_SYN2    , 0x13300, "SynthStrings 2"                        )  \
	X(NM_ENSEM_CHOI    , 0x13400, "Choir Aahs"                            )  \
	X(NM_ENSEM_CHOI_AL , 0x13401, "Choir Aahs (alternative)"              )  \
	X(NM_ENSEM_VOIC    , 0x13500, "Voice Oohs"                            )  \
	X(NM_ENSEM_VOIC_HM , 0x13501, "Voice Oohs (humming)"                  )  \
	X(NM_ENSEM_SYVO    , 0x13600, "Synth Voice"                           )  \
	X(NM_ENSEM_SYVO_AN , 0x13601, "Synth Voice (analog)"                  )  \
	X(NM_ENSEM_ORHI    , 0x13700, "Orchestra Hit"                         )  \
	X(NM_ENSEM_ORHI_BP , 0x13701, "Orchestra Hit (bass hit plus)"         )  \
	X(NM_ENSEM_ORHI_6  , 0x13702, "Orchestra Hit (6th)"                   )  \
	X(NM_ENSEM_ORHI_EU , 0x13703, "Orchestra Hit (euro)"                  )  \
	X(NM_BRASS_TRUM    , 0x13800, "Trumpet"                               )  \
	X(NM_BRASS_TRUM_DS , 0x13801, "Trumpet (dark soft)"                   )  \
	X(NM_BRASS_TROM    , 0x13900, "Trombone"                              )  \
	X(NM_BRASS_TROM_AL , 0x13901, "Trombone (alternative)"                )  \
	X(NM_BRASS_TROM_BR , 0x13902, "Trombone (bright)"                     )  \
	X(NM_BRASS_TUBA    , 0x13A00, "Tuba"                                  )  \
	X(NM_BRASS_MUTR    , 0x13B00, "Muted Trumpet"                         )  \
	X(NM_BRASS_MUTR_AL , 0x13B01, "Muted Trumpet (alternative)"           )  \
	X(NM_BRASS_FRHO    , 0x13C00, "French Horn"                           )  \
	X(NM_BRASS_FRHO_WA , 0x13C01, "French Horn (warm)"                    )  \
	X(NM_BRASS_BRSE    , 0x13D00, "Brass Section"                         )  \
	X(NM_BRASS_BRSE_OM , 0x13D01, "Brass Section (octave mix)"            )  \
	X(NM_BRASS_SBR1    , 0x13E00, "Synth Brass 1"                         )  \
	X(NM_BRASS_SBR1_AL , 0x13E01, "Synth Brass 1 (alternative)"           )  \
	X(NM_BRASS_SBR1_AN , 0x13E02, "Synth Brass 1 (analog)"                )  \
	X(NM_BRASS_SBR1_JU , 0x13E03, "Synth Brass 1 (jump)"                  )  \
	X(NM_BRASS_SBR2    , 0x13F00, "Synth Brass 2"                         )  \
	X(NM_BRASS_SBR2_AL , 0x13F01, "Synth Brass 2 (alternative)"           )  \
	X(NM_BRASS_SBR2_AN , 0x13F02, "Synth Brass 2 (analog)"                )  \
	X(NM_REED_SOSA     , 0x14000, "Soprano Sax"                           )  \
	X(NM_REED_ALSA     , 0x14100, "Alto Sax"                              )  \
	X(NM_REED_TESA     , 0x14200, "Tenor Sax"                             )  \
	X(NM_REED_BASA     , 0x14300, "Baritone Sax"                          )  \
	X(NM_REED_OBOE     , 0x14400, "Oboe"                                  )  \
	X(NM_REED_ENHO     , 0x14500, "English Horn"                          )  \
	X(NM_REED_BASS     , 0x14600, "Bassoon"                               )  \
	X(NM_REED_CLAR     , 0x14700, "Clarinet"                              )  \
	X(NM_PIPE_PICC     , 0x14800, "Piccolo"                               )  \
	X(NM_PIPE_FLUT     , 0x14900, "Flute"                                 )  \
	X(NM_PIPE_RECO     , 0x14A00, "Recorder"                              )  \
	X(NM_PIPE_PAFL     , 0x14B00, "Pan Flute"                             )  \
	X(NM_PIPE_BLBO     , 0x14C00, "Blown Bottle"                          )  \
	X(NM_PIPE_SHAK     , 0x14D00, "Shakuhachi"                            )  \
	X(NM_PIPE_WHIS     , 0x14E00, "Whistle"                               )  \
	X(NM_PIPE_OCAR     , 0x14F00, "Ocarina"                               )  \
	X(NM_LEAD_OSC1     , 0x15000, "Oscilliator 1"                         )  \
	X(NM_LEAD_OSC1_SQ  , 0x15001, "Oscilliator 1 (square)"                )  \
	X(NM_LEAD_OSC1_SI  , 0x15002, "Oscilliator 1 (sine)"                  )  \
	X(NM_LEAD_OSC2     , 0x15100, "Oscilliator 2"                         )  \
	X(NM_LEAD_OSC2_SA  , 0x15101, "Oscilliator 2 (sawtooth)"              )  \
	X(NM_LEAD_OSC2_SP  , 0x15102, "Oscilliator 2 (saw + pulse)"           )  \
	X(NM_LEAD_OSC2_DS  , 0x15103, "Oscilliator 2 (double sawtooth)"       )  \
	X(NM_LEAD_OSC2_AN  , 0x15104, "Oscilliator 2 (sequenced analog)"      )  \
	X(NM_LEAD_CALL     , 0x15200, "Calliope"                              )  \
	X(NM_LEAD_CHIF     , 0x15300, "Chiff"                                 )  \
	X(NM_LEAD_CHAR     , 0x15400, "Charang"                               )  \
	X(NM_LEAD_CHAR_WL  , 0x15401, "Charang (wire lead)"                   )  \
	X(NM_LEAD_VOIC     , 0x15500, "Voice"                                 )  \
	X(NM_LEAD_FIFT     , 0x15600, "Fifths"                                )  \
	X(NM_LEAD_BALE     , 0x15700, "Bass + Lead"                           )  \
	X(NM_LEAD_BALE_SW  , 0x15701, "Bass + Lead (soft wrl)"                )  \
	X(NM_PAD_NEAG      , 0x15800, "New Age"                               )  \
	X(NM_PAD_WARM      , 0x15900, "Warm"                                  )  \
	X(NM_PAD_WARM_SI   , 0x15901, "Warm (sine)"                           )  \
	X(NM_PAD_POLY      , 0x15A00, "Polysynth"                             )  \
	X(NM_PAD_CHOI      , 0x15B00, "Choir"                                 )  \
	X(NM_PAD_CHOI_IT   , 0x15B01, "Choir (itopia)"                        )  \
	X(NM_PAD_BOWE      , 0x15C00, "Bowed"                                 )  \
	X(NM_PAD_META      , 0x15D00, "Metallic"                              )  \
	X(NM_PAD_HALO      , 0x15E00, "Halo"                                  )  \
	X(NM_PAD_SWEE      , 0x15F00, "Sweep"                                 )  \
	X(NM_SFX1_RAIN     , 0x16000, "Rain"                                  )  \
	X(NM_SFX1_SOTR     , 0x16100, "Soundtrack"                            )  \
	X(NM_SFX1_CRYS     , 0x16200, "Crystal"                               )  \
	X(NM_SFX1_CRYS_MA  , 0x16201, "Crystal (mallet)"                      )  \
	X(NM_SFX1_ATMO     , 0x16300, "Atmosphere"                            )  \
	X(NM_SFX1_BRIG     , 0x16400, "Brightness"                            )  \
	X(NM_SFX1_GOBL     , 0x16500, "Goblins"                               )  \
	X(NM_SFX1_ECHO     , 0x16600, "Echoes"                                )  \
	X(NM_SFX1_ECHO_BE  , 0x16601, "Echoes (bell)"                         )  \
	X(NM_SFX1_ECHO_PA  , 0x16602, "Echoes (pan)"                          )  \
	X(NM_SFX1_SCFI     , 0x16700, "Sci-Fi"                                )  \
	X(NM_ETHNIC_SITA   , 0x16000, "Sitar"                                 )  \
	X(NM_ETHNIC_SITA_BE, 0x16001, "Sitar (bend)"                          )  \
	X(NM_ETHNIC_BANJ   , 0x16100, "Banjo"                                 )  \
	X(NM_ETHNIC_SHAM   , 0x16200, "Shamisen"                              )  \
	X(NM_ETHNIC_KOTO   , 0x16300, "Koto"                                  )  \
	X(NM_ETHNIC_KOTO_TA, 0x16301, "Koto (taisho)"                         )  \
	X(NM_ETHNIC_KALI   , 0x16400, "Kalimba"                               )  \
	X(NM_ETHNIC_BAPI   , 0x16500, "Bag Pipe"                              )  \
	X(NM_ETHNIC_FIDD   , 0x16600, "Fiddle"                                )  \
	X(NM_ETHNIC_SHAN   , 0x16700, "Shanai"                                )  \
	X(NM_PERC_TIBE     , 0x17000, "Tinkle Bell"                           )  \
	X(NM_PERC_AGOG     , 0x17100, "Agogo"                                 )  \
	X(NM_PERC_STDR     , 0x17200, "Steel Drums"                           )  \
	X(NM_PERC_WOOD     , 0x17300, "Woodblock"                             )  \
	X(NM_PERC_WOOD_CA  , 0x17301, "Woodblock (castanets)"                 )  \
	X(NM_PERC_TADR     , 0x17400, "Taiko Drum"                            )  \
	X(NM_PERC_TADR_CB  , 0x17401, "Taiko Drum (concert bass)"             )  \
	X(NM_PERC_METO     , 0x17500, "Melodic Tom"                           )  \
	X(NM_PERC_METO_PO  , 0x17501, "Melodic Tom (power)"                   )  \
	X(NM_PERC_SYDR     , 0x17600, "Synth Drum"                            )  \
	X(NM_PERC_SYDR_RB  , 0x17601, "Synth Drum (rhythm box tom)"           )  \
	X(NM_PERC_SYDR_EL  , 0x17602, "Synth Drum (electric)"                 )  \
	X(NM_PERC_RECY     , 0x17700, "Reverse Cymbal"                        )  \
	X(NM_SFX2_G0_GUFR  , 0x17800, "Guitar Fret Noise"                     )  \
	X(NM_SFX2_G0_GUCU  , 0x17801, "Guitar Cutting Noise"                  )  \
	X(NM_SFX2_G0_STSL  , 0x17802, "Acoustic Bass String Slap"             )  \
	X(NM_SFX2_G1_BRNO  , 0x17900, "Breath Noise"                          )  \
	X(NM_SFX2_G1_FLKC  , 0x17901, "Flute Key Click"                       )  \
	X(NM_SFX2_G2_SEAS  , 0x17A00, "Seashore"                              )  \
	X(NM_SFX2_G2_RAIN  , 0x17A01, "Rain"                                  )  \
	X(NM_SFX2_G2_THUN  , 0x17A02, "Thunder"                               )  \
	X(NM_SFX2_G2_WIND  , 0x17A03, "Wind"                                  )  \
	X(NM_SFX2_G2_STRE  , 0x17A04, "Stream"                                )  \
	X(NM_SFX2_G2_BUBB  , 0x17A05, "Bubble"                                )  \
	X(NM_SFX2_G3_BTW1  , 0x17B00, "Bird Tweet 1"                          )  \
	X(NM_SFX2_G3_DOG   , 0x17B01, "Dog"                                   )  \
	X(NM_SFX2_G3_HOGA  , 0x17B02, "Horse Gallop"                          )  \
	X(NM_SFX2_G3_BTW2  , 0x17B03, "Bird Tweet 2"                          )  \
	X(NM_SFX2_G4_TEL1  , 0x17C00, "Telephone Ring 1"                      )  \
	X(NM_SFX2_G4_TEL2  , 0x17C01, "Telephone Ring 2"                      )  \
	X(NM_SFX2_G4_DOCR  , 0x17C02, "Door Creaking"                         )  \
	X(NM_SFX2_G4_DOOR  , 0x17C03, "Door"                                  )  \
	X(NM_SFX2_G4_SCRA  , 0x17C04, "Scratch"                               )  \
	X(NM_SFX2_G4_WICH  , 0x17C05, "Wind Chime"                            )  \
	X(NM_SFX2_G5_HELI  , 0x17D00, "Helicopter"                            )  \
	X(NM_SFX2_G5_CAEN  , 0x17D01, "Car Engine"                            )  \
	X(NM_SFX2_G5_CAST  , 0x17D02, "Car Stop"                              )  \
	X(NM_SFX2_G5_CAPA  , 0x17D03, "Car Pass"                              )  \
	X(NM_SFX2_G5_CACR  , 0x17D04, "Car Crash"                             )  \
	X(NM_SFX2_G5_SIRE  , 0x17D05, "Siren"                                 )  \
	X(NM_SFX2_G5_TRAI  , 0x17D06, "Train"                                 )  \
	X(NM_SFX2_G5_JETP  , 0x17D07, "Jetplane"                              )  \
	X(NM_SFX2_G5_STAR  , 0x17D08, "Starship"                              )  \
	X(NM_SFX2_G5_BUNO  , 0x17D09, "Burst Noise"                           )  \
	X(NM_SFX2_G6_APPL  , 0x17E00, "Applause"                              )  \
	X(NM_SFX2_G6_LAUG  , 0x17E01, "Laughing"                              )  \
	X(NM_SFX2_G6_SCRE  , 0x17E02, "Screaming"                             )  \
	X(NM_SFX2_G6_PUNC  , 0x17E03, "Punch"                                 )  \
	X(NM_SFX2_G6_HEBE  , 0x17E04, "Heart Beat"                            )  \
	X(NM_SFX2_G6_FOOT  , 0x17E05, "Footsteps"                             )  \
	X(NM_SFX2_G7_GUSH  , 0x17F00, "Gun Shot"                              )  \
	X(NM_SFX2_G7_MAGU  , 0x17F01, "Machine Gun"                           )  \
	X(NM_SFX2_G7_LAGU  , 0x17F02, "Laser Gun"                             )  \
	X(NM_SFX2_G7_EXPL  , 0x17F03, "Explosion"                             )  \
	X(NM_PERSND_STAN   , 0x20000, "PSS Standard"                          )  \
	X(NM_PERSND_ROOM   , 0x20800, "PSS Room"                              )  \
	X(NM_PERSND_POWE   , 0x21000, "PSS Power"                             )  \
	X(NM_PERSND_ELEC   , 0x21800, "PSS Electronic"                        )  \
	X(NM_PERSND_ANLG   , 0x21900, "PSS Analog"                            )  \
	X(NM_PERSND_JAZZ   , 0x22000, "PSS Jazz"                              )  \
	X(NM_PERSND_BRUS   , 0x22800, "PSS Brush"                             )  \
	X(NM_PERSND_ORCH   , 0x23000, "PSS Orchestra"                         )  \
	X(NM_PERSND_SNFX   , 0x23800, "PSS Sound Effects"                     )
;
enum nm_patch_enum {
	#define X(en, code, str) en,
	NM_EACH_PATCH(X)
	#undef X
	NM__PATCH_END
};

enum nm_notemsg_enum {
	NM_MSG_NOTEOFF,
	NM_MSG_PITCHBEND,
	NM_MSG_PRESSURE
};

//
// structure details
//

struct nm_note_struct {
	int note;
	float freq;
	float cycle_pos;
	float dcycle_pos;
	float hit_velocity;
	float hold_pressure;
	float release_velocity;
	float fade;
	float dfade;
	bool hit;
	bool down;
	bool release;
};

struct nm_channel_struct {
	nm_note_st notes[128];
	float pressure;
	float pitch_bend;
};

struct nm_voice_struct {
	nm_voice_st *next;
	void *user;
};

struct nm_ctx_struct {
	nm_midi midi;
	double ticks;
	double samples_per_tick;
	uint64_t samples_done;
	nm_channel_st *chans;
	nm_voice_st *voice_free;
	nm_voice_st *voice_used;
	nm_event ev;
	double samples_per_sec;
	int notedowns[128];
	bool ignore_timesig;
};

struct nm_sample_struct {
	float L;
	float R;
};

struct nm_event_struct {
	nm_event_st *next;
	nm_event_type type;
	uint64_t tick;
	union {
		struct { int channel; int note; float velocity; } noteoff;
		struct { int channel; int note; float velocity; } noteon;
		struct { int channel; int note; float pressure; } notepressure;
		struct { int channel; int control; int value;   } controlchange;
		struct { int channel; int patch;                } programchange;
		struct { int channel; float pressure;           } channelpressure;
		struct { int channel; float bend;               } pitchbend;
		struct { int tempo;                             } tempo;
		struct { int num; int den; int cc; int dd;      } timesig;
	} u;
};

struct nm_midi_struct {
	nm_event *tracks;
	int track_count;
	int ticks_per_q;
	int max_channels;
};

//
// new api
//

// temporarily putting this here until I flesh it out completely
// ...

typedef bool (*nm_synth_patch_setup_func)(nm_ctx ctx, void *synth, nm_patch patch, void *patchinf);
typedef bool (*nm_synth_render_func)(nm_ctx ctx, void *synth, int len, nm_sample samples,
	void *patchinf, float vel, float mod, int sampdone, int cycdone, float cycpos, float dcyc);

typedef enum nm_event2_type_enum nm_event2_type;

typedef struct nm_event2_struct nm_event2_st, *nm_event2;
typedef struct nm_channel2_struct nm_channel2_st, *nm_channel2;
typedef struct nm_voice2_struct nm_voice2_st, *nm_voice2;
typedef struct nm_ctx2_struct nm_ctx2_st, *nm_ctx2;

enum nm_event2_type_enum {
	NM_EV_RESET,
	NM_EV_NOTEON,
	NM_EV_NOTEBEND,
	NM_EV_NOTEMOD,
	NM_EV_NOTEOFF,
	NM_EV_TEMPO,
	NM_EV_PATCH
};

struct nm_event2_struct {
	uint32_t tick;
	uint8_t type; // nm_event2_type
	uint8_t channel;
	uint16_t data1;
	float data2;
};

struct nm_voice2_struct {
	nm_voice2 next;
	nm_synth_render_func f_render;
	void *patchinf;
	nm_patch patch;
	int sampdone;
	int cycdone;
	float vel;
	float cycpos;
	float dcyc;
};

struct nm_ctx2_struct {
	void *synth;
	nm_synth_patch_setup_func f_patch_setup;
	nm_synth_render_func f_render;
	nm_event2 events;
	int ev_size;
	int ev_read;
	int ev_write;
	int ticks_per_quarternote;
	int samples_per_sec;
	int usec_per_quarternote;
	double ticks;
	double samples_per_tick;
	nm_voice2 voices_free;
	nm_voice2 voices_used;
	nm_patch *channel_patch;
	int channels;
	uintptr_t patchinf[NM__PATCH_END];
	// 0 = uninitialized, 1 = default synth, 2 = zero length custom synth
	// 3+ = pointer to data of size sizeof_patchinf
};

nm_ctx2 nm_ctx2_new(int channels, int voices, int samples_per_sec, void *synth,
	size_t sizeof_patchinf, nm_synth_patch_setup_func f_patch_setup, nm_synth_render_func f_render);
void    nm_ctx2_bake(nm_ctx2 ctx, uint32_t ticks);
void    nm_ctx2_process(nm_ctx2 ctx, int sample_len, nm_sample samples);
void    nm_ctx2_free(nm_ctx2 ctx);

// ...
// end temp

#endif // NIGHTMARE__H
