// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

#include "nightmare.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include <string.h>

nm_alloc_func   nm_alloc   = malloc;
nm_realloc_func nm_realloc = realloc;
nm_free_func    nm_free    = free;

//     |----| attack
// ___
//  |  |    ^
//  |  |   / \
// peak|  /   \______     ___
//  |  | /           \     | sustain as fraction of peak
// _|_ |/_____________\_  _|_
//
//          |--|  +  |-|  decay
static struct {
	float peak;      // max volume (0-1)
	float attack;    // seconds
	float decay;     // seconds
	float sustain;   // fraction of peak
	float harmonic1; // 1st harmonic volume (fraction of peak), freq * 2
	float harmonic2; // 2nd, freq * 3
	float harmonic3; // 3rd, freq * 4
	float harmonic4; // 4th, freq * 5
	uint8_t wave;    // sine(0), saw(1), square(2), triangle(3)
} melodicpatch[256] = {
	/* NM_PIANO_ACGR    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 1 },
	/* NM_PIANO_ACGR_WI */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ACGR_DK */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_BRAC    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_BRAC_WI */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELGR    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELGR_WI */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_HOTO    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_HOTO_WI */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELE1    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELE1_DT */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELE1_VM */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELE1_60 */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELE2    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELE2_DT */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELE2_VM */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELE2_LE */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_ELE2_PH */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_HARP    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_HARP_OM */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_HARP_WI */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_HARP_KO */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_CLAV    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIANO_CLAV_PU */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_CELE    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_GLOC    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_MUBO    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_VIPH    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_VIPH_WI */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_MARI    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_MARI_WI */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_XYLO    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_BELL_TU */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_BELL_CH */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_BELL_CA */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_CHROM_DULC    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_DRAW    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_DRAW_DT */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_DRAW_60 */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_DRAW_AL */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_PERC    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_PERC_DT */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_PERC_2  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_ROCK    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_CHUR    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_CHUR_OM */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_CHUR_DT */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_REED    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_REED_PU */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_ACCO    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_ACCO_2  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_HARM    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ORGAN_TANG    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_NYLO   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_NYLO_UK*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_NYLO_KO*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_NYLO_AL*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_STEE   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_STEE_12*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_STEE_MA*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_STEE_BS*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_JAZZ   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_JAZZ_PS*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_CLEA   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_CLEA_DT*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_CLEA_MT*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_MUTE   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_MUTE_FC*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_MUTE_VS*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_MUTE_JM*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_OVER   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_OVER_PI*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_DIST   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_DIST_FB*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_DIST_RH*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_HARM   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_GUITAR_HARM_FB*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_ACOU     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_FING     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_FING_SL  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_PICK     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_FRET     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SLP1     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SLP2     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SYN1     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SYN1_WA  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SYN1_RE  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SYN1_CL  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SYN1_HA  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SYN2     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SYN2_AT  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SYN2_RU  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BASS_SYN2_AP  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_VILN   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_VILN_SA*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_VILA   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_CELL   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_CONT   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_TREM   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_PIZZ   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_HARP   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_HARP_YC*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_STRING_TIMP   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_STR1    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_STR1_SB */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_STR1_60 */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_STR2    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_SYN1    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_SYN1_AL */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_SYN2    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_CHOI    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_CHOI_AL */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_VOIC    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_VOIC_HM */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_SYVO    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_SYVO_AN */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_ORHI    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_ORHI_BP */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_ORHI_6  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ENSEM_ORHI_EU */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_TRUM    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_TRUM_DS */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_TROM    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_TROM_AL */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_TROM_BR */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_TUBA    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_MUTR    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_MUTR_AL */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_FRHO    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_FRHO_WA */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_BRSE    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_BRSE_OM */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_SBR1    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_SBR1_AL */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_SBR1_AN */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_SBR1_JU */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_SBR2    */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_SBR2_AL */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_BRASS_SBR2_AN */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_REED_SOSA     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_REED_ALSA     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_REED_TESA     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_REED_BASA     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_REED_OBOE     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_REED_ENHO     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_REED_BASS     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_REED_CLAR     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIPE_PICC     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIPE_FLUT     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIPE_RECO     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIPE_PAFL     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIPE_BLBO     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIPE_SHAK     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIPE_WHIS     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PIPE_OCAR     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_OSC1     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_OSC1_SQ  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_OSC1_SI  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_OSC2     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_OSC2_SA  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_OSC2_SP  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_OSC2_DS  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_OSC2_AN  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_CALL     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_CHIF     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_CHAR     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_CHAR_WL  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_VOIC     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_FIFT     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_BALE     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_LEAD_BALE_SW  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_NEAG      */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_WARM      */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_WARM_SI   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_POLY      */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_CHOI      */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_CHOI_IT   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_BOWE      */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_META      */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_HALO      */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PAD_SWEE      */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_RAIN     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_SOTR     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_CRYS     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_CRYS_MA  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_ATMO     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_BRIG     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_GOBL     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_ECHO     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_ECHO_BE  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_ECHO_PA  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX1_SCFI     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_SITA   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_SITA_BE*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_BANJ   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_SHAM   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_KOTO   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_KOTO_TA*/ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_KALI   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_BAPI   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_FIDD   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_ETHNIC_SHAN   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_TIBE     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_AGOG     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_STDR     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_WOOD     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_WOOD_CA  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_TADR     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_TADR_CB  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_METO     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_METO_PO  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_SYDR     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_SYDR_RB  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_SYDR_EL  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_PERC_RECY     */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G0_GUFR  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G0_GUCU  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G0_STSL  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G1_BRNO  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G1_FLKC  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G2_SEAS  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G2_RAIN  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G2_THUN  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G2_WIND  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G2_STRE  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G2_BUBB  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G3_BTW1  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G3_DOG   */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G3_HOGA  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G3_BTW2  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G4_TEL1  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G4_TEL2  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G4_DOCR  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G4_DOOR  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G4_SCRA  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G4_WICH  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_HELI  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_CAEN  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_CAST  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_CAPA  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_CACR  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_SIRE  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_TRAI  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_JETP  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_STAR  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G5_BUNO  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G6_APPL  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G6_LAUG  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G6_SCRE  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G6_PUNC  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G6_HEBE  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G6_FOOT  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G7_GUSH  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G7_MAGU  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G7_LAGU  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
	/* NM_SFX2_G7_EXPL  */ { 1.0f, 0.1f, 0.5f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0 },
};

static inline void recalc_spt(nm_ctx ctx){
	ctx->samples_per_tick =
		((double)ctx->samples_per_sec * (double)ctx->usec_per_quarternote) /
		(1000000.0 * (double)ctx->ticks_per_quarternote);
}

static inline void reset_tempo(nm_ctx ctx, uint16_t ticks_per_quarternote){
	ctx->ticks_per_quarternote = ticks_per_quarternote;
	ctx->usec_per_quarternote = 500000;
	recalc_spt(ctx);
}

nm_ctx nm_ctx_new(uint16_t ticks_per_quarternote, uint16_t channels, int voices,
	int samples_per_sec, void *synth, size_t sizeof_patchinf, size_t sizeof_voiceinf,
	nm_synth_patch_setup_func f_patch_setup, nm_synth_render_func f_render){
	nm_ctx ctx = nm_alloc(sizeof(nm_ctx_st));
	if (ctx == NULL)
		return NULL;

	ctx->events = NULL;
	ctx->channels = NULL;
	ctx->voices_free = NULL;
	ctx->voices_used = NULL;
	memset(ctx->patchinf, 0, sizeof(void *) * NM__PATCH_END);
	ctx->wevents = NULL;
	ctx->last_wevent = NULL;
	ctx->ins_wevent = NULL;

	ctx->synth = synth;
	ctx->f_patch_setup = f_patch_setup;
	ctx->f_render = f_render;

	ctx->ev_read = 0;
	ctx->ev_write = 0;
	ctx->ev_size = 200;
	ctx->events = nm_alloc(sizeof(nm_event_st) * ctx->ev_size);
	if (ctx->events == NULL)
		goto cleanup;

	ctx->samples_per_sec = samples_per_sec;
	ctx->ticks = 0;
	reset_tempo(ctx, ticks_per_quarternote);

	ctx->channel_count = channels;
	ctx->channels = nm_alloc(sizeof(nm_channel_st) * channels);
	if (ctx->channels == NULL)
		goto cleanup;
	for (int i = 0; i < channels; i++){
		ctx->channels[i].patch = NM_PIANO_ACGR; // TODO: what is proper intialization?
		ctx->channels[i].mod = 0;
		ctx->channels[i].bend = 0;
	}

	if (sizeof_voiceinf < sizeof(nm_defvoiceinf_st))
		sizeof_voiceinf = sizeof(nm_defvoiceinf_st);
	for (int i = 0; i < voices; i++){
		nm_voice vc = nm_alloc(sizeof(nm_voice_st));
		if (vc == NULL)
			goto cleanup;
		vc->next = ctx->voices_free;
		ctx->voices_free = vc;
		vc->voiceinf = nm_alloc(sizeof_voiceinf);
		if (vc->voiceinf == NULL)
			goto cleanup;
	}

	memset(ctx->patchinf_status, 0, sizeof(uint8_t) * NM__PATCH_END);

	if (sizeof_patchinf < sizeof(nm_defpatchinf_st))
		sizeof_patchinf = sizeof(nm_defpatchinf_st);
	for (int i = 0; i < NM__PATCH_END; i++){
		ctx->patchinf[i] = nm_alloc(sizeof_patchinf);
		if (ctx->patchinf[i] == NULL){
			return NULL;
		}
	}

	memset(ctx->notecnt, 0, sizeof(int) * 128);

	return ctx;

	cleanup:
	for (int i = 0; i < NM__PATCH_END; i++){
		if (ctx->patchinf[i])
			nm_free(ctx->patchinf[i]);
	}
	nm_voice vc = ctx->voices_free;
	while (vc){
		nm_voice del = vc;
		vc = vc->next;
		if (del->voiceinf)
			nm_free(del->voiceinf);
		nm_free(del);
	}
	if (ctx->channels)
		nm_free(ctx->channels);
	if (ctx->events)
		nm_free(ctx->events);
	nm_free(ctx);
	return NULL;
}

static void warn(nm_warn_func f_warn, void *user, const char *fmt, ...){
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
	f_warn(buf, user);
	nm_free(buf);
}

bool nm_ismidi(uint8_t data[8]){
	return
		data[0] == 'M' && data[1] == 'T' && data[2] == 'h' && data[3] == 'd' &&
		data[4] ==  0  && data[5] ==  0  && data[6] ==  0  && data[7] >= 6;
}

bool nm_midi_newfile(nm_ctx ctx, const char *file, nm_warn_func f_warn, void *user){
	FILE *fp = fopen(file, "rb");
	if (fp == NULL)
		return false;
	fseek(fp, 0, SEEK_END);
	uint64_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t *data = nm_alloc(sizeof(uint8_t) * size);
	if (data == NULL){
		fclose(fp);
		return false;
	}
	if (fread(data, 1, size, fp) != size){
		nm_free(data);
		fclose(fp);
		return false;
	}
	fclose(fp);
	bool res = nm_midi_newbuffer(ctx, size, data, f_warn, user);
	nm_free(data);
	return res;
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

static inline const char *ss(int num){
	return num == 1 ? "" : "s";
}

bool nm_midi_newbuffer(nm_ctx ctx, uint64_t size, uint8_t *data, nm_warn_func f_warn, void *user){
	if (size < 14 ||
		data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd' ||
		data[4] !=  0  || data[5] !=  0  || data[6] !=  0  || data[7] < 6){
		warn(f_warn, user, "Invalid header");
		return false;
	}
	uint64_t pos = 0;
	uint32_t ticks = 0;
	bool found_header = false;
	bool ignore_timesig;
	int hd_format;
	int hd_track_ch;
	uint16_t hd_ticks_per_q;
	int track_count = 0;
	int max_channels = 0;
	int track_i = 0;
	int running_status;
	chunk_st chk;
	while (pos < size){
		if (!read_chunk(pos, size, data, &chk)){
			if (!found_header){
				warn(f_warn, user, "Invalid header");
				return false;
			}
			uint64_t dif = size - pos;
			warn(f_warn, user, "Unrecognized data (%llu byte%s) at end of file", dif, ss(dif));
			return true;
		}
		if (chk.alignment != 0)
			warn(f_warn, user, "Chunk misaligned by %d byte%s", chk.alignment, ss(chk.alignment));
		uint64_t orig_size = chk.data_size;
		if (chk.data_start + chk.data_size > size){
			uint64_t offset = chk.data_start + chk.data_size - size;
			warn(f_warn, user, "Chunk ends %llu byte%s too early", offset, ss(offset));
			chk.data_size -= offset;
		}
		pos = chk.data_start + chk.data_size;
		switch (chk.type){
			case 0: { // MThd
				if (found_header)
					warn(f_warn, user, "Multiple header chunks present");
				found_header = true;
				if (orig_size != 6){
					warn(f_warn, user,
						"Header chunk has non-standard size %llu byte%s (expecting 6 bytes)",
						orig_size, ss(orig_size));
				}
				if (chk.data_size >= 2){
					hd_format = ((int)data[chk.data_start + 0] << 8) | data[chk.data_start + 1];
					if (hd_format != 0 && hd_format != 1 && hd_format != 2){
						warn(f_warn, user, "Header reports bad format (%d)", hd_format);
						hd_format = 1;
					}
				}
				else{
					warn(f_warn, user, "Header missing format");
					hd_format = 1;
				}
				if (chk.data_size >= 4){
					hd_track_ch = ((int)data[chk.data_start + 2] << 8) | data[chk.data_start + 3];
					if (hd_format == 0 && hd_track_ch != 1){
						warn(f_warn, user,
							"Format 0 expecting 1 track chunk, header is reporting %d chunks",
							hd_track_ch);
					}
				}
				else{
					warn(f_warn, user, "Header missing track chunk count");
					hd_track_ch = -1;
				}
				if (chk.data_size >= 6){
					uint32_t t = ((int)data[chk.data_start + 4] << 8) | data[chk.data_start + 5];
					if (t & 0x8000){
						warn(f_warn, user, "Unsupported timing format (SMPTE)");
						hd_ticks_per_q = 1;
					}
					else
						hd_ticks_per_q = t;
				}
				else{
					warn(f_warn, user, "Header missing division");
					hd_ticks_per_q = 1;
				}
			} break;

			case 1: { // MTrk
				if (hd_format == 0 && track_i > 0){
					warn(f_warn, user, "Format 0 expecting 1 track chunk, found more than one");
					hd_format = 1;
				}
				int chan_base = 0;
				if (track_i == 0 || hd_format == 2){
					ignore_timesig = false;
					nm_ev_reset(ctx, ticks, hd_ticks_per_q);
				}
				if (hd_format == 1){
					ticks = 0;
					chan_base = track_i * 16;
				}
				max_channels = chan_base + 16;
				if (max_channels > ctx->channel_count){
					warn(f_warn, user, "Too many simultaneous tracks, ignoring track %d", track_i);
					goto mtrk_end;
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
							warn(f_warn, user, "Invalid timestamp in track %d",
								track_i);
							goto mtrk_end;
						}
						int t = data[p++];
						if (t & 0x80){
							if (p >= p_end){
								warn(f_warn, user, "Invalid timestamp in track %d",
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

					ticks += dt;

					if (p >= p_end){
						warn(f_warn, user, "Missing message in track %d", track_i);
						goto mtrk_end;
					}

					// read msg
					int msg = data[p++];
					if (msg < 0x80){
						// use running status
						if (running_status < 0){
							warn(f_warn, user, "Invalid message %02X in track %d", msg,
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
							warn(f_warn, user, "Bad Note-Off message (out of data) in track %d",
								track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t note = data[p++];
						uint8_t vel = data[p++];
						if (note >= 0x80){
							warn(f_warn, user, "Bad Note-Off message (invalid note %02X) in "
								"track %d", note, track_i);
							note ^= 0x80;
						}
						if (vel >= 0x80){
							warn(f_warn, user, "Bad Note-Off message (invalid velocity %02X) "
								"in track %d", vel, track_i);
							vel ^= 0x80;
						}
						nm_ev_noteoff(ctx, ticks, chan_base + (msg & 0xF), note);
					}
					else if (msg >= 0x90 && msg < 0xA0){ // Note On
						if (p + 1 >= p_end){
							warn(f_warn, user, "Bad Note-On message (out of data) in track %d",
								track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t note = data[p++];
						uint8_t vel = data[p++];
						if (note >= 0x80){
							warn(f_warn, user, "Bad Note-On message (invalid note %02X) in "
								"track %d", note, track_i);
							note ^= 0x80;
						}
						if (vel >= 0x80){
							warn(f_warn, user, "Bad Note-On message (invalid velocity %02X) "
								"in track %d", vel, track_i);
							vel ^= 0x80;
						}
						if (vel == 0)
							nm_ev_noteoff(ctx, ticks, chan_base + (msg & 0xF), note);
						else{
							nm_ev_noteon(ctx, ticks, chan_base + (msg & 0xF), note,
								vel == 0x40 ? 0.5f : (float)vel / 127.0f);
						}
					}
					else if (msg >= 0xA0 && msg < 0xB0){ // Note Pressure
						if (p + 1 >= p_end){
							warn(f_warn, user, "Bad Note Pressure message (out of data) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t note = data[p++];
						uint8_t pressure = data[p++];
						if (note >= 0x80){
							warn(f_warn, user, "Bad Note Pressure message (invalid note %02X) "
								"in track %d", note, track_i);
							note ^= 0x80;
						}
						if (pressure >= 0x80){
							warn(f_warn, user, "Bad Note Pressure message (invalid pressure "
								"%02X) in track %d", pressure, track_i);
							pressure ^= 0x80;
						}
						nm_ev_notemod(ctx, ticks, chan_base + (msg & 0xF), note,
							pressure == 0x40 ? 0.5f : (float)pressure / 127.0f);
					}
					else if (msg >= 0xB0 && msg < 0xC0){ // Control Change
						if (p + 1 >= p_end){
							warn(f_warn, user, "Bad Control Change message (out of data) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t ctrl = data[p++];
						uint8_t val = data[p++];
						if (ctrl >= 0x80){
							warn(f_warn, user, "Bad Control Change message (invalid control "
								" %02X) in track %d", ctrl, track_i);
							ctrl ^= 0x80;
						}
						if (val >= 0x80){
							warn(f_warn, user, "Bad Control Change message (invalid value %02X) "
								"in track %d", val, track_i);
							val ^= 0x80;
						}
						// TODO: deal with ctrl/val
					}
					else if (msg >= 0xC0 && msg < 0xD0){ // Program Change
						if (p >= p_end){
							warn(f_warn, user, "Bad Program Change message (out of data) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t patch = data[p++];
						if (patch >= 0x80){
							warn(f_warn, user, "Bad Program Change message (invalid patch "
								"%02X) in track %d", patch, track_i);
							patch ^= 0x80;
						}
						// TODO: deal with nm_ev_patch (mapping patch+bank to nm_patch)
					}
					else if (msg >= 0xD0 && msg < 0xE0){ // Channel Pressure
						if (p >= p_end){
							warn(f_warn, user, "Bad Channel Pressure message (out of data) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						uint8_t pressure = data[p++];
						if (pressure >= 0x80){
							warn(f_warn, user, "Bad Channel Pressure message (invalid pressure "
								"%02X) in track %d", pressure, track_i);
							pressure ^= 0x80;
						}
						nm_ev_chanmod(ctx, ticks, chan_base + (msg & 0xF),
							pressure == 0x40 ? 0.5f : (float)pressure / 127.0f);
					}
					else if (msg >= 0xE0 && msg < 0xF0){ // Pitch Bend
						if (p + 1 >= p_end){
							warn(f_warn, user, "Bad Pitch Bend message (out of data) in track "
								"%d", track_i);
							goto mtrk_end;
						}
						running_status = msg;
						int p1 = data[p++];
						int p2 = data[p++];
						if (p1 >= 0x80){
							warn(f_warn, user, "Bad Pitch Bend message (invalid lower bits "
								"%02X) in track %d", p1, track_i);
							p1 ^= 0x80;
						}
						if (p2 >= 0x80){
							warn(f_warn, user, "Bad Pitch Bend message (invalid higher bits "
								"%02X) in track %d", p2, track_i);
							p2 ^= 0x80;
						}
						uint16_t bend = p1 | (p2 << 7);
						float bendf;
						if (bend < 0x2000)
							bendf = (float)(bend - 0x2000) / 8192.0f;
						else if (bend == 0x2000)
							bendf = 0;
						else
							bendf = (float)(bend - 0x2000) / 8191.0f;
						nm_ev_chanbend(ctx, ticks, chan_base + (msg & 0xF), bendf);
					}
					else if (msg == 0xF0 || msg == 0xF7){ // SysEx Event
						running_status = -1; // TODO: validate we should clear this
						// read length as a variable int
						int dl = 0;
						int len = 0;
						while (true){
							if (p >= p_end){
								warn(f_warn, user, "Bad SysEx Event (out of data) in track %d",
									track_i);
								goto mtrk_end;
							}
							len++;
							if (len >= 5){
								warn(f_warn, user, "Bad SysEx Event (invalid data length) in "
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
							warn(f_warn, user, "Bad SysEx Event (data length too large) in "
								"track %d", track_i);
							goto mtrk_end;
						}
						/*
						if (data[p] == 0x7E){
							printf("SysEx:");
							for (int i = 0; i < dl; i++)
								printf(" %02X", data[p + i]);
							printf("\n");
						}
						else if (data[p] == 0x7F){
							printf("SysEx RT:");
							for (int i = 0; i < dl; i++)
								printf(" %02X", data[p + i]);
							printf("\n");
						}
						if (data[p + dl - 1] != 0xF7){
							printf("SysEx Packeted:");
							for (int i = 0; i < dl; i++)
								printf(" %02X", data[p + i]);
							printf("\n");
						}
						*/
						p += dl;
					}
					else if (msg == 0xFF){ // Meta Event
						running_status = -1; // TODO: validate we should clear this
						if (p + 1 >= p_end){
							warn(f_warn, user, "Bad Meta Event (out of data) in track %d",
								track_i);
							goto mtrk_end;
						}
						int type = data[p++];
						int len = data[p++];
						if (p + len > p_end){
							warn(f_warn, user, "Bad Meta Event (data length too large) in "
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
									warn(f_warn, user, "Expecting zero-length data for End of "
										"Track message for track %d", track_i);
								}
								if (p < p_end){
									uint64_t pd = p_end - p;
									warn(f_warn, user, "Extra data at end of track %d: %llu "
										"byte%s", track_i, pd, pd == 1 ? "" : "s");
								}
								goto mtrk_end;
							case 0x51: // 03 TT TT TT       Set Tempo
								if (len < 3){
									warn(f_warn, user, "Missing data for Set Tempo event in "
										"track %d", track_i);
								}
								else{
									if (len > 3){
										warn(f_warn, user, "Extra %d byte%s for Set Tempo "
											"event in track %d", len - 3, len - 3 == 1 ? "" : "s",
											track_i);
									}
									int tempo = ((int)data[p + 0] << 16) | ((int)data[p + 1] << 8) |
										data[p + 2];
									if (tempo == 0){
										warn(f_warn, user, "Invalid tempo (0) for track %d",
											track_i);
									}
									else{
										ignore_timesig = true;
										nm_ev_tempo(ctx, ticks, tempo);
									}
								}
								break;
							case 0x54: // 05 HH MM SS RR TT SMPTE Offset
								break;
							case 0x58: // 04 NN MM LL TT    Time Signature
								if (len < 4){
									warn(f_warn, user, "Missing data for Time Signature event "
										"in track %d", track_i);
								}
								else{
									if (len > 4){
										warn(f_warn, user, "Extra %d byte%s for Time Signature "
											"event in track %d", len - 4, len - 4 == 1 ? "" : "s",
											track_i);
									}
									if (!ignore_timesig){
										nm_ev_tempo(ctx, ticks, (uint32_t)
											(1000000.0f / powf(2.0f, 3.0f - data[p + 1])));
									}
								}
								break;
							case 0x59: // 02 SS MM          Key Signature
							case 0x7F: // LL data           Sequencer-Specific Meta Event
								break;
							default:
								warn(f_warn, user, "Unknown Meta Event %02X in track %d",
									type, track_i);
								break;
						}
						p += len;
					}
					else{
						warn(f_warn, user, "Unknown message type %02X in track %d",
							msg, track_i);
						goto mtrk_end;
					}
				}
				warn(f_warn, user, "Track %d ended before receiving End of Track message",
					track_i);
				mtrk_end:
				nm_ev_nop(ctx, ticks);
				track_i++;
			} break;

			case 2: // XFIH
			case 3: // XFKM
				break;

			default:
				warn(f_warn, user, "Fatal Error: Illegal Chunk Type");
				exit(1);
				return false;
		}
	}
	if (found_header && hd_track_ch != track_i){
		warn(f_warn, user, "Mismatch between reported track count (%d) and actual track "
			"count (%d)", hd_track_ch, track_i);
	}
	return true;
}

const char *nm_patch_str(nm_patch p){
	switch (p){
		#define X(en, code, str) case NM_ ## en: return str;
		NM_EACH_PATCH(X)
		#undef X
		case NM__PATCH_END: break;
	}
	return "Unknown";
}

static int nm_patch_code(nm_patch p){
	switch (p){
		#define X(en, code, str) case NM_ ## en: return code;
		NM_EACH_PATCH(X)
		#undef X
		case NM__PATCH_END: break;
	}
	return -1;
}

nm_patchcat nm_patch_category(nm_patch p){
	if      (p >= NM_PIANO_ACGR   && p <= NM_PIANO_CLAV_PU ) return NM_PIANO;
	else if (p >= NM_CHROM_CELE   && p <= NM_CHROM_DULC    ) return NM_CHROM;
	else if (p >= NM_ORGAN_DRAW   && p <= NM_ORGAN_TANG    ) return NM_ORGAN;
	else if (p >= NM_GUITAR_NYLO  && p <= NM_GUITAR_HARM_FB) return NM_GUITAR;
	else if (p >= NM_BASS_ACOU    && p <= NM_BASS_SYN2_AP  ) return NM_BASS;
	else if (p >= NM_STRING_VILN  && p <= NM_STRING_TIMP   ) return NM_STRING;
	else if (p >= NM_ENSEM_STR1   && p <= NM_ENSEM_ORHI_EU ) return NM_ENSEM;
	else if (p >= NM_BRASS_TRUM   && p <= NM_BRASS_SBR2_AN ) return NM_BRASS;
	else if (p >= NM_REED_SOSA    && p <= NM_REED_CLAR     ) return NM_REED;
	else if (p >= NM_PIPE_PICC    && p <= NM_PIPE_OCAR     ) return NM_PIPE;
	else if (p >= NM_LEAD_OSC1    && p <= NM_LEAD_BALE_SW  ) return NM_LEAD;
	else if (p >= NM_PAD_NEAG     && p <= NM_PAD_SWEE      ) return NM_PAD;
	else if (p >= NM_SFX1_RAIN    && p <= NM_SFX1_SCFI     ) return NM_SFX1;
	else if (p >= NM_ETHNIC_SITA  && p <= NM_ETHNIC_SHAN   ) return NM_ETHNIC;
	else if (p >= NM_PERC_TIBE    && p <= NM_PERC_RECY     ) return NM_PERC;
	else if (p >= NM_SFX2_G0_GUFR && p <= NM_SFX2_G7_EXPL  ) return NM_SFX2;
	else if (p >= NM_PERSND_STAN  && p <= NM_PERSND_SNFX   ) return NM_PERSND;
	return NM__PATCHCAT_END;
}

const char *nm_patchcat_str(nm_patchcat pc){
	switch (pc){
		case NM_PIANO : return "Piano";
		case NM_CHROM : return "Chromatic Percussion";
		case NM_ORGAN : return "Organ";
		case NM_GUITAR: return "Guitar";
		case NM_BASS  : return "Bass";
		case NM_STRING: return "Strings & Orchestral";
		case NM_ENSEM : return "Ensemble";
		case NM_BRASS : return "Brass";
		case NM_REED  : return "Reed";
		case NM_PIPE  : return "Pipe";
		case NM_LEAD  : return "Synth Lead";
		case NM_PAD   : return "Synth Pad";
		case NM_SFX1  : return "Sound Effects 1";
		case NM_ETHNIC: return "Ethnic";
		case NM_PERC  : return "Percussive";
		case NM_SFX2  : return "Sound Effects 2";
		case NM_PERSND: return "Percussion Sound Set";
		case NM__PATCHCAT_END: break;
	}
	return "Unknown";
}

static inline void wev_insert(nm_ctx ctx, uint32_t tick, nm_wevent wev){
	wev->ev.tick = tick;
	if (ctx->wevents == NULL){
		// first event
		wev->next = NULL;
		ctx->wevents = ctx->last_wevent = ctx->ins_wevent = wev;
	}
	else if (tick < ctx->wevents->ev.tick){
		// insert at start
		wev->next = ctx->wevents;
		ctx->wevents = wev;
	}
	else if (tick < ctx->ins_wevent->ev.tick){
		// between start and ins_wevent, so search forwards from start
		nm_wevent prev = ctx->wevents;
		while (tick >= prev->next->ev.tick)
			prev = prev->next;
		wev->next = prev->next;
		prev->next = wev;
		ctx->ins_wevent = wev;
	}
	else if (tick == ctx->ins_wevent->ev.tick){
		// insert immediately after ins_wevent
		wev->next = ctx->ins_wevent->next;
		ctx->ins_wevent->next = wev;
		if (ctx->ins_wevent == ctx->last_wevent)
			ctx->last_wevent = wev;
		ctx->ins_wevent = wev;
	}
	else if (tick < ctx->last_wevent->ev.tick){
		// between ins_wevent and last, so search forwards from ins_wevent
		nm_wevent prev = ctx->ins_wevent;
		while (tick >= prev->next->ev.tick)
			prev = prev->next;
		wev->next = prev->next;
		prev->next = wev;
		ctx->ins_wevent = wev;
	}
	else{
		// after last_wevent
		wev->next = NULL;
		ctx->last_wevent->next = wev;
		ctx->last_wevent = wev;
	}
}

bool nm_ev_nop(nm_ctx ctx, uint32_t tick){
	if (ctx->wevents && ctx->wevents->ev.tick == tick)
		return true;
	nm_wevent wev = nm_alloc(sizeof(nm_wevent_st));
	if (wev == NULL)
		return false;
	wev->ev.type = NM_EV_NOP;
	wev_insert(ctx, tick, wev);
	return true;
}

bool nm_ev_reset(nm_ctx ctx, uint32_t tick, uint16_t ticks_per_quarternote){
	nm_wevent wev = nm_alloc(sizeof(nm_wevent_st));
	if (wev == NULL)
		return false;
	wev->ev.type = NM_EV_RESET;
	wev->ev.u.data2i = ticks_per_quarternote;
	wev_insert(ctx, tick, wev);
	return true;
}

bool nm_ev_noteon(nm_ctx ctx, uint32_t tick, uint16_t channel, uint8_t note, float vel){
	if (channel >= ctx->channel_count || note >= 0x80)
		return false;
	nm_wevent wev = nm_alloc(sizeof(nm_wevent_st));
	if (wev == NULL)
		return false;
	wev->ev.type = NM_EV_NOTEON;
	wev->ev.channel = channel;
	wev->ev.data1 = note;
	wev->ev.u.data2f = vel;
	wev_insert(ctx, tick, wev);
	return true;
}

bool nm_ev_notemod(nm_ctx ctx, uint32_t tick, uint16_t channel, uint8_t note, float mod){
	if (channel >= ctx->channel_count || note >= 0x80)
		return false;
	nm_wevent wev = nm_alloc(sizeof(nm_wevent_st));
	if (wev == NULL)
		return false;
	wev->ev.type = NM_EV_NOTEMOD;
	wev->ev.channel = channel;
	wev->ev.data1 = note;
	wev->ev.u.data2f = mod;
	wev_insert(ctx, tick, wev);
	return true;
}

bool nm_ev_noteoff(nm_ctx ctx, uint32_t tick, uint16_t channel, uint8_t note){
	if (channel >= ctx->channel_count || note >= 0x80)
		return false;
	nm_wevent wev = nm_alloc(sizeof(nm_wevent_st));
	if (wev == NULL)
		return false;
	wev->ev.type = NM_EV_NOTEOFF;
	wev->ev.channel = channel;
	wev->ev.data1 = note;
	wev_insert(ctx, tick, wev);
	return true;
}

bool nm_ev_chanmod(nm_ctx ctx, uint32_t tick, uint16_t channel, float mod){
	if (channel >= ctx->channel_count)
		return false;
	nm_wevent wev = nm_alloc(sizeof(nm_wevent_st));
	if (wev == NULL)
		return false;
	wev->ev.type = NM_EV_CHANMOD;
	wev->ev.channel = channel;
	wev->ev.u.data2f = mod;
	wev_insert(ctx, tick, wev);
	return true;
}

bool nm_ev_chanbend(nm_ctx ctx, uint32_t tick, uint16_t channel, float bend){
	if (channel >= ctx->channel_count)
		return false;
	nm_wevent wev = nm_alloc(sizeof(nm_wevent_st));
	if (wev == NULL)
		return false;
	wev->ev.type = NM_EV_CHANBEND;
	wev->ev.channel = channel;
	wev->ev.u.data2f = bend;
	wev_insert(ctx, tick, wev);
	return true;
}

bool nm_ev_tempo(nm_ctx ctx, uint32_t tick, uint32_t usec_per_quarternote){
	nm_wevent wev = nm_alloc(sizeof(nm_wevent_st));
	if (wev == NULL)
		return false;
	wev->ev.type = NM_EV_TEMPO;
	wev->ev.u.data2i = usec_per_quarternote;
	wev_insert(ctx, tick, wev);
	return true;
}

bool nm_ev_patch(nm_ctx ctx, uint32_t tick, uint16_t channel, nm_patch patch){
	if (channel >= ctx->channel_count)
		return false;
	nm_wevent wev = nm_alloc(sizeof(nm_wevent_st));
	if (wev == NULL)
		return false;
	wev->ev.type = NM_EV_PATCH;
	wev->ev.channel = channel;
	wev->ev.u.data2i = patch;
	wev_insert(ctx, tick, wev);
	return true;
}

void nm_defpatch(nm_patch patch, uint8_t wave, float peak, float attack, float decay,
	float sustain, float harmonic1, float harmonic2, float harmonic3, float harmonic4){
	melodicpatch[patch].wave      = wave     ;
	melodicpatch[patch].peak      = peak     ;
	melodicpatch[patch].attack    = attack   ;
	melodicpatch[patch].decay     = decay    ;
	melodicpatch[patch].sustain   = sustain  ;
	melodicpatch[patch].harmonic1 = harmonic1;
	melodicpatch[patch].harmonic2 = harmonic2;
	melodicpatch[patch].harmonic3 = harmonic3;
	melodicpatch[patch].harmonic4 = harmonic4;
}

bool nm_ctx_bake(nm_ctx ctx, uint32_t ticks){
	// count how many events we need to insert
	int evtot = 0;
	nm_wevent wev = ctx->wevents;
	uint32_t last_tick = 0;
	while (wev && wev->ev.tick <= ticks){
		evtot++;
		last_tick = wev->ev.tick;
		wev = wev->next;
	}
	if (last_tick < ticks){
		// need to insert a nop at the last tick to ensure the time gets rendered
		evtot++;
		nm_ev_nop(ctx, last_tick);
	}

	// see how many slots we have left
	// note we have to subtract one from what's left because we can't technically fill up the buffer
	// completely because we interpret ev_read == ev_write to be an empty buffer
	int evleft;
	if (ctx->ev_read < ctx->ev_write)
		evleft = ctx->ev_read + ctx->ev_size - ctx->ev_write - 1;
	else if (ctx->ev_read == ctx->ev_write)
		evleft = ctx->ev_size - 1;
	else
		evleft = ctx->ev_read - ctx->ev_write - 1;

	// check if we have enough room
	if (evleft < evtot){
		// we don't, so allocate more room
		int newsize = ctx->ev_size + evtot - evleft + 200;
		ctx->events = nm_realloc(ctx->events, sizeof(nm_event_st) * newsize);
		if (ctx->events == NULL)
			return false;
		if (ctx->ev_read > ctx->ev_write){
			// read is after write, therefore we need to slide the remaining read events down to
			// the end of the buffer
			// start  : [e|W| | |R|e|e]
			// realloc: [e|W| | |R|e|e| | | ]
			// slide  : [e|W| | | | | |R|e|e]
			int rightlen = ctx->ev_size - ctx->ev_read;
			memmove(&ctx->events[newsize - rightlen], &ctx->events[ctx->ev_read],
				sizeof(nm_event_st) * rightlen);
			ctx->ev_read = newsize - rightlen;
		}
		ctx->ev_size = newsize;
	}

	// copy over the events
	wev = ctx->wevents;
	bool found_ins = false;
	for (int i = 0; i < evtot; i++){
		if (wev == ctx->ins_wevent)
			found_ins = true;
		ctx->events[ctx->ev_write] = wev->ev;
		nm_wevent del = wev;
		wev = wev->next;
		nm_free(del);
		ctx->ev_write = (ctx->ev_write + 1) % ctx->ev_size;
	}

	// update wevents/ins_wevent/last_wevent
	ctx->wevents = wev;
	if (found_ins)
		ctx->ins_wevent = wev;
	if (wev == NULL)
		ctx->last_wevent = NULL;

	return true;
}

bool nm_ctx_bakeall(nm_ctx ctx){
	if (ctx->last_wevent == NULL)
		return true;
	return nm_ctx_bake(ctx, ctx->last_wevent->ev.tick);
}

void nm_ctx_clear(nm_ctx ctx){
	nm_wevent wev = ctx->wevents;
	while (wev){
		nm_wevent del = wev;
		wev = wev->next;
		nm_free(del);
	}
	ctx->wevents = ctx->last_wevent = ctx->ins_wevent = NULL;
}

static float wave_sine(float i){
	return sinf(M_PI * 2.0f * i);
}

static float wave_saw(float i){
	return i;
}

static float wave_square(float i){
	return i < 0.5f ? -1.0f : 1.0f;
}

static float wave_triangle(float i){
	if (i < 0.25f)
		return i * 4.0f;
	else if (i < 0.75f)
		return 2.0f - (i - 0.25f) * 4.0f;
	return (i - 0.75f) * 4.0f - 1.0f;
}

static inline float wave_harmonic(float i, float h1, float h2, float h3, float h4,
	float (*f_wave)(float i)){
	float s0 = f_wave(i);
	float s1 = f_wave(fmodf(2.0f * i, 1.0f));
	float s2 = f_wave(fmodf(3.0f * i, 1.0f));
	float s3 = f_wave(fmodf(4.0f * i, 1.0f));
	float s4 = f_wave(fmodf(5.0f * i, 1.0f));
	return s0 + h1 * s1 + h2 * s2 + h3 * s3 + h4 * s4;
}

static void render_sect(nm_ctx ctx, int len, nm_sample samples){
	nm_voice vc = ctx->voices_used;
	nm_voice *vc_prev = &ctx->voices_used;
	while (vc){
		bool keep;
		if (vc->f_render)
			keep = vc->f_render(ctx, vc, len, samples);
		else{
			// default additive synthesizer
			nm_defvoiceinf vi = vc->voiceinf;
			nm_defpatchinf pi = vc->patchinf;
			float fade = vi->fade;
			float dfade = vi->dfade;
			float peak = pi->peak;
			float attack = pi->attack;
			float decay = pi->decay;
			float sustain = pi->sustain;
			float h1 = pi->harmonic1;
			float h2 = pi->harmonic2;
			float h3 = pi->harmonic3;
			float h4 = pi->harmonic4;
			bool sustaining = vc->down;
			float (*f_wave)(float i) = pi->f_wave;
			if (vc->samptot == 0){
				fade = 0;
				dfade = 1.0f / (attack * ctx->samples_per_sec);
				vi->dfade = dfade;
			}
			if (vc->released){
				dfade = -1.0f / (decay * ctx->samples_per_sec);
				vi->dfade = dfade;
			}
			if (fade > 0 || dfade > 0){
				float amp = vc->vel * peak;
				sustain *= amp;
				float cyc = vc->cyc;
				float dcyc = vc->dcyc;
				for (int a = 0; a < len; a++){
					float v = fade * fade *
						wave_harmonic(fmodf(cyc + a * dcyc, 1.0f), h1, h2, h3, h4, f_wave);
					if (dfade > 0){
						fade += dfade;
						if (fade >= amp){
							dfade = -1.0f / (decay * ctx->samples_per_sec);
							vi->dfade = dfade;
						}
					}
					else if (fade > 0 && (!sustaining || fade > sustain)){
						fade += dfade;
						if (fade <= 0)
							fade = 0;
					}
					samples[a].L += v;
					samples[a].R += v;
				}
				vi->fade = fade;
			}
			keep = fade > 0 || dfade > 0;
		}
		if (keep){
			// advance vc stats
			vc->samptot += len;
			vc->cyc += len * vc->dcyc;
			vc->cyctot += (int)vc->cyc;
			vc->cyc = fmodf(vc->cyc, 1.0f);
			vc->released = false;
			vc_prev = &vc->next;
			vc = vc->next;
		}
		else{
			// move vc over to voices_free
			nm_voice vcn = vc->next;
			vc->next = ctx->voices_free;
			ctx->voices_free = vc;
			*vc_prev = vcn;
			vc = vcn;
		}
	}
}

void nm_ctx_process(nm_ctx ctx, int sample_len, nm_sample samples){
	int total_out = 0;
	while (ctx->ev_read != ctx->ev_write){
		nm_event ev = &ctx->events[ctx->ev_read];
		double next_ev_sample = round(((double)ev->tick - ctx->ticks) * ctx->samples_per_tick);
		if (next_ev_sample > sample_len - total_out)
			break;

		if (next_ev_sample > 0){
			// render before the event
			render_sect(ctx, next_ev_sample, &samples[total_out]);
			total_out += next_ev_sample;
			ctx->ticks += next_ev_sample / ctx->samples_per_tick;
		}

		// process event
		if (ev->type == NM_EV_NOP)
			/* do nothing */;
		else if (ev->type == NM_EV_RESET){
			reset_tempo(ctx, ev->u.data2i);
			// silent all voices
			nm_voice vc = ctx->voices_used;
			while (vc){
				nm_voice next = vc->next;
				vc->next = ctx->voices_free;
				ctx->voices_free = vc;
				vc = next;
			}
		}
		else if (ev->type == NM_EV_NOTEON){
			nm_voice vc = ctx->voices_free;
			if (vc){
				// allocate a voice by moving it from voices_free to voices_used
				ctx->voices_free = vc->next;
				vc->next = ctx->voices_used;
				ctx->voices_used = vc;

				// initialize voice
				nm_channel chan = &ctx->channels[ev->channel];
				nm_patch pt = chan->patch;
				uint8_t pstat = ctx->patchinf_status[pt];
				// 0 = unallocated, 1 = default synth, 2 = custom synth
				if (pstat == 0){
					// attempt to allocate patch
					pstat = 1;
					if (ctx->f_patch_setup){
						if (ctx->f_patch_setup(ctx, ctx->synth, pt, ctx->patchinf[pt]))
							pstat = 2;
					}
					if (pstat == 1){
						// intiailize patchinf to default synth info
						nm_defpatchinf pi = ctx->patchinf[pt];
						pi->peak      = melodicpatch[pt].peak;
						pi->attack    = melodicpatch[pt].attack;
						pi->decay     = melodicpatch[pt].decay;
						pi->sustain   = melodicpatch[pt].sustain;
						pi->harmonic1 = melodicpatch[pt].harmonic1;
						pi->harmonic2 = melodicpatch[pt].harmonic2;
						pi->harmonic3 = melodicpatch[pt].harmonic3;
						pi->harmonic4 = melodicpatch[pt].harmonic4;
						if (melodicpatch[pt].wave == 0)
							pi->f_wave = wave_sine;
						else if (melodicpatch[pt].wave == 1)
							pi->f_wave = wave_saw;
						else if (melodicpatch[pt].wave == 2)
							pi->f_wave = wave_square;
						else
							pi->f_wave = wave_triangle;
					}
					ctx->patchinf_status[pt] = pstat;
				}
				vc->f_render = pstat == 1 ? NULL : ctx->f_render;
				vc->synth = ctx->synth;
				vc->patchinf = ctx->patchinf[pt];
				vc->patch = pt;
				vc->note = ev->data1;
				vc->channel = ev->channel;
				vc->samptot = 0;
				vc->cyctot = 0;
				vc->vel = ev->u.data2f;
				vc->mod = 0;
				vc->cyc = 0;
				// TODO: wire frequency of note to a tuning table
				float freq = 440.0f * powf(2.0f, ((float)ev->data1 - 69.0f) / 12.0f);
				vc->dcyc = 1.0f / ((float)ctx->samples_per_sec / freq);
				vc->down = true;
				vc->released = false;
				ctx->notecnt[ev->data1]++;
			}
		}
		else if (ev->type == NM_EV_NOTEMOD){
			// TODO: this
		}
		else if (ev->type == NM_EV_NOTEOFF){
			// search for the right note/channel and turn it off
			nm_voice vc = ctx->voices_used;
			while (vc){
				if (vc->channel == ev->channel && vc->note == ev->data1 && vc->down){
					vc->down = false;
					vc->released = true;
					ctx->notecnt[ev->data1]--;
					break;
				}
				vc = vc->next;
			}
		}
		else if (ev->type == NM_EV_CHANMOD){
			// TODO: this
		}
		else if (ev->type == NM_EV_CHANBEND){
			// TODO: this
		}
		else if (ev->type == NM_EV_TEMPO){
			ctx->usec_per_quarternote = ev->u.data2i;
			recalc_spt(ctx);
		}
		else if (ev->type == NM_EV_PATCH){
			// TODO: this
		}

		// advance read index
		ctx->ev_read = (ctx->ev_read + 1) % ctx->ev_size;
	}
	// the next event is after these samples, so just render the rest and return
	render_sect(ctx, sample_len - total_out, &samples[total_out]);
	ctx->ticks += (double)(sample_len - total_out) / ctx->samples_per_tick;
}

void nm_ctx_dumpev(nm_ctx ctx){
	int i = ctx->ev_read;
	int cnt = 1;
	while (i != ctx->ev_write){
		nm_event ev = &ctx->events[i];
		// print event
		if (ev->type == NM_EV_NOP)
			printf("%3d. [%08d] NOP\n", cnt, ev->tick);
		else if (ev->type == NM_EV_RESET)
			printf("%3d. [%08d] RESET    %u\n", cnt, ev->tick, ev->u.data2i);
		else if (ev->type == NM_EV_NOTEON){
			printf("%3d. [%08d] NOTEON   (%02X) %02X %g\n", cnt, ev->tick, ev->channel, ev->data1,
				ev->u.data2f);
		}
		else if (ev->type == NM_EV_NOTEMOD){
			printf("%3d. [%08d] NOTEMOD  (%02X) %02X %g\n", cnt, ev->tick, ev->channel, ev->data1,
				ev->u.data2f);
		}
		else if (ev->type == NM_EV_NOTEOFF)
			printf("%3d. [%08d] NOTEOFF  (%02X) %02X\n", cnt, ev->tick, ev->channel, ev->data1);
		else if (ev->type == NM_EV_CHANMOD)
			printf("%3d. [%08d] CHANMOD  (%02X) %g\n", cnt, ev->tick, ev->channel, ev->u.data2f);
		else if (ev->type == NM_EV_CHANBEND)
			printf("%3d. [%08d] CHANBEND (%02X) %g\n", cnt, ev->tick, ev->channel, ev->u.data2f);
		else if (ev->type == NM_EV_TEMPO)
			printf("%3d. [%08d] TEMPO    %u\n", cnt, ev->tick, ev->u.data2i);
		else if (ev->type == NM_EV_PATCH)
			printf("%3d. [%08d] PATCH    (%02X) %u\n", cnt, ev->tick, ev->channel, ev->u.data2i);
		else
			printf("%3d. Unknown event: %02X\n", cnt, ev->type);
		cnt++;
		i = (i + 1) % ctx->ev_size;
	}
}

int nm_ctx_eventsleft(nm_ctx ctx){
	if (ctx->ev_read == ctx->ev_write)
		return 0;
	if (ctx->ev_read > ctx->ev_write)
		return ctx->ev_size - ctx->ev_read + ctx->ev_write;
	return ctx->ev_write - ctx->ev_read;
}

int nm_ctx_voicesleft(nm_ctx ctx){
	int cnt = 0;
	nm_voice vc = ctx->voices_used;
	while (vc){
		cnt++;
		vc = vc->next;
	}
	return cnt;
}

void nm_ctx_free(nm_ctx ctx){
	nm_free(ctx->events);
	nm_wevent wev = ctx->wevents;
	while (wev){
		nm_wevent del = wev;
		wev = wev->next;
		nm_free(del);
	}
	nm_voice vc = ctx->voices_free;
	while (vc){
		nm_voice del = vc;
		vc = vc->next;
		nm_free(del->voiceinf);
		nm_free(del);
	}
	vc = ctx->voices_used;
	while (vc){
		nm_voice del = vc;
		vc = vc->next;
		nm_free(del->voiceinf);
		nm_free(del);
	}
	nm_free(ctx->channels);
	for (int i = 0; i < NM__PATCH_END; i++)
		nm_free(ctx->patchinf[i]);
	nm_free(ctx);
}
