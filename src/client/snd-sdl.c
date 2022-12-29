/*
 * File: snd-sdl.c
 * Purpose: SDL sound support for TomeNET
 *
 * Written in 2010 by C. Blue, inspired by old angband sdl code.
 * New custom sound structure, ambient sound, background music and weather.
 * New in 2022, I added positional audio. Might migrate to OpenAL in the
 * future to support HRTF for z-plane spatial audio. - C. Blue
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */


#include "angband.h"

/* Summer 2022 I migrated from SDL to SDL2, requiring SDL2_mixer v2.5 or higher
   for Mix_SetPanning() and Mix_GetMusicPosition(). - C. Blue */
#ifdef SOUND_SDL

/* Enable ALMixer, overriding SDL? */
//#define SOUND_AL_SDL

/* allow pressing RETURN key to play any piece of music (like for sfx) -- only reason to disable: It's spoilery ;) */
#define ENABLE_JUKEBOX
/* if a song is selected, don't fadeout current song first, but halt it instantly. this is needed for playing disabled songs.
   Note that enabling this will also allow to iteratively play through all sub-songs of a specific music event, by activating
   the same song over and over, while otherwise if this option is disabled you'd have to wait for it to end and randomly switch
   to other sub-songs.*/
#ifdef ENABLE_JUKEBOX
 #define JUKEBOX_INSTANT_PLAY
#endif

/* Smooth dynamic weather volume change depending on amount of particles,
   instead of just on/off depending on particle count window? */
//#define WEATHER_VOL_PARTICLES  -- not yet that cool

/* Maybe this will be cool: Dynamic weather volume calculated from distances to registered clouds */
#define WEATHER_VOL_CLOUDS

/* Allow user-defined custom volume factor for each sample or song? ([].volume) */
#define USER_VOLUME_SFX
#define USER_VOLUME_MUS

#ifdef SOUND_AL_SDL
#else
/*
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_thread.h>
*/
/* This is the way recommended by the SDL web page */
 #include "SDL.h"
 #include "SDL_mixer.h"
 #include "SDL_thread.h"
#endif

/* completely turn off mixing of disabled audio features (music, sound, weather, all)
   as opposed to just muting their volume? */
//#define DISABLE_MUTED_AUDIO

/* load sounds on the fly if the loader thread has not loaded them yet */
/* disabled because this can cause delays up to a few seconds */
bool on_demand_loading = FALSE;

/* output various status messages about initializing audio */
//#define DEBUG_SOUND

/* Path to sound files */
#define SEGFAULT_HACK
#ifndef SEGFAULT_HACK
static const char *ANGBAND_DIR_XTRA_SOUND;
static const char *ANGBAND_DIR_XTRA_MUSIC; //<-this one in particular segfaults!
#else
char ANGBAND_DIR_XTRA_SOUND[1024];
char ANGBAND_DIR_XTRA_MUSIC[1024];
#endif

/* for threaded caching of audio files */
SDL_Thread *load_audio_thread;
//bool kill_load_audio_thread = FALSE; -- not needed, since the thread_load_audio() never takes really long to complete
SDL_mutex *load_sample_mutex_entrance, *load_song_mutex_entrance;
SDL_mutex *load_sample_mutex, *load_song_mutex;


/* declare */
static void fadein_next_music(void);
static void clear_channel(int c);
static bool my_fexists(const char *fname);

static int thread_load_audio(void *dummy);
static Mix_Chunk* load_sample(int idx, int subidx);
static Mix_Music* load_song(int idx, int subidx);


/* Arbitrary limit of mixer channels */
#define MAX_CHANNELS	32

/* Arbitary limit on number of samples per event [8] */
#define MAX_SAMPLES	100

/* Arbitary limit on number of music songs per situation [3] */
#define MAX_SONGS	100

/* Exponential volume level table
 * evlt[i] = round(0.09921 * exp(2.31 * i / 100))
 */

/* evtl must range from 0..MIX_MAX_VOLUME (an SDL definition, [128]) */


/* --------------------------- Mixing algorithms, pick one --------------------------- */
/* Normal mixing: */
//#define MIXING_NORMAL

/* Like normal mixing, but trying to use the edge cases better for broader volume spectrum - not finely implemented yet, DON'T USE!: */
//#define MIXING_EXTREME

/* Like normal mixing, but trying to use maximum range of slider multiplication: 0..10 = 11 steps, so 121 steps in total [RECOMMENDED]: */
#define MIXING_ULTRA

/* Actually add up volume sliders (Master + Specific) and average them, instead of multiplying them. (Drawback: Individual sliders cannot vary volume far away from master slider): */
//#define MIXING_ADDITIVE
/* ----------------------------------------------------------------------------------- */


#ifdef MIXING_NORMAL /* good, 12 is the lowest value at which the mixer is never completely muted */
static s16b evlt[101] = { 12,
  13,  13,  14,  14,  14,  15,  15,  15,  16,  16,
  16,  17,  17,  18,  18,  18,  19,  19,  20,  20,
  21,  21,  22,  22,  23,  23,  24,  24,  25,  25,
  26,  27,  27,  28,  29,  29,  30,  31,  31,  32,
  33,  34,  34,  35,  36,  37,  38,  38,  39,  40,
  41,  42,  43,  44,  45,  46,  47,  48,  50,  51,
  52,  53,  54,  56,  57,  58,  60,  61,  63,  64,
  65,  67,  69,  70,  72,  73,  75,  77,  79,  81,
  82,  84,  86,  88,  90,  93,  95,  97,  99, 102,
 104, 106, 109, 111, 114, 117, 119, 122, 125, 128
};
#endif
#ifdef MIXING_EXTREME /* barely working: 9 is lowest possible, if we -for rounding errors- take care to always mix volume with all multiplications first, before dividing (otherwise 13 in MIXING_NORMAL is lowest). */
static s16b evlt[101] = { 9,
   9,  9,  9, 10, 10,	 10, 10, 11, 11, 11,
  12, 12, 12, 13, 13,	 13, 14, 14, 14, 15,
  15, 16, 16, 17, 17,	 17, 18, 18, 19, 19,
  20, 21, 21, 22, 22,	 23, 24, 24, 25, 26,
  26, 27, 28, 29, 29,	 30, 31, 32, 33, 34,
  34, 35, 36, 37, 38,	 39, 40, 42, 43, 44,
  45, 46, 48, 49, 50,	 52, 53, 54, 56, 57,
  59, 61, 62, 64, 66,	 67, 69, 71, 73, 75,
  77, 79, 81, 84, 86,	 88, 90, 93, 95, 98,
 101,103,106,109,112,	115,118,121,125,128
};
#endif
#ifdef MIXING_ULTRA /* make full use of the complete slider multiplication spectrum: 0..10 ^ 2 -> 121 values */
/* Purely exponentially we'd get 1 .. 1.04127^120 -> 1..128 (rounded).
   However, it'd again result in lacking diferences for low volume values, so let's try a superlinear mixed approach instead:
   i/3 + 1.038^i	- C. Blue
*/
static s16b evlt[121] = { 1 , //0
1 ,2 ,2 ,2 ,3 ,		3 ,4 ,4 ,4 ,5 ,		//10
5 ,6 ,6 ,6 ,7 ,		7 ,8 ,8 ,8 ,9 ,
9 ,10 ,10 ,10 ,11 ,	11 ,12 ,12 ,13 ,13 ,
14 ,14 ,14 ,15 ,15 ,	16 ,16 ,17 ,17 ,18 ,
18 ,19 ,19 ,20 ,20 ,	21 ,21 ,22 ,23 ,23 ,	//50
24 ,24 ,25 ,25 ,26 ,	27 ,27 ,28 ,29 ,29 ,
30 ,31 ,31 ,32 ,33 ,	34 ,35 ,35 ,36 ,37 ,
38 ,39 ,40 ,40 ,41 ,	42 ,43 ,44 ,45 ,46 ,
48 ,49 ,50 ,51 ,52 ,	53 ,55 ,56 ,57 ,59 ,
60 ,62 ,63 ,65 ,66 ,	68 ,70 ,71 ,73 ,75 ,	//100
77 ,79 ,81 ,83 ,85 ,	87 ,90 ,92 ,95 ,97 ,
100 ,103 ,105 ,108 ,111 ,	114 ,118 ,121 ,124 ,128
};
#endif
#ifdef MIXING_ADDITIVE /* special: for ADDITIVE (and averaging) slider mixing instead of the usual multiplicative! */
static s16b evlt[101] = { 1, //could start on 0 here
/* The values in the beginning up to around 13 are adjusted upwards and more linearly, to ensure that each mixer step (+5 array index) actually causes an audible change.
   The drawback is, that the low-mid range is not really exponential anymore and this will be especially noticable for boosted (>100%) audio, which sounds less boosted
   when the volume sliders' average is around the middle volume range :/ - can't have everything. To perfectly remedy this we'd need finer volume values in SDL. - C. Blue
   Said middle range from hear-testing, subjectively (in added slider step values from 0..10, master+music): 4..7 */
1 ,1 ,1 ,1 ,2 ,		2 ,2 ,2 ,2 ,3 ,
3 ,3 ,3 ,3 ,4 ,		4 ,4 ,4 ,4 ,5 ,
5 ,5 ,5 ,5 ,6 ,		6 ,6 ,6 ,6 ,7 ,
7 ,7 ,7 ,7 ,8 ,		8 ,8 ,8 ,8 ,9 ,
9 ,9 ,9 ,9 ,10,		10 ,10 ,11 ,11 ,11 ,
12 ,12 ,13 ,13 ,14 ,	15 ,15 ,16 ,17 ,18 ,
19 ,20 ,21 ,22 ,23 ,	24 ,25 ,27 ,28 ,29 ,
31 ,32 ,34 ,36 ,38 ,	39 ,41 ,44 ,46 ,48 ,
50 ,53 ,56 ,58 ,61 ,	64 ,68 ,71 ,75 ,78 ,
82 ,86 ,91 ,95 ,100 ,	105 ,110 ,116 ,122 ,128
};
#endif

#if 1 /* Exponential volume scale - mikaelh */
 #if 0 /* Use define */
  #define CALC_MIX_VOLUME(T, V)	(cfg_audio_master * T * evlt[V] * evlt[cfg_audio_master_volume] / MIX_MAX_VOLUME)
 #else /* Make it a function instead - needed for volume 200% boost in the new jukebox/options menu. */
  /* T: 0 or 1 for on/off. V: Volume in range of 0..100 (specific slider, possibly adjusted already with extra volume info).
     boost: 1..200 (percent): Despite it called 'boost' it is actually a volume value from 1 to 200, so could also lower audio (100 = keep normal volume). */
  static int CALC_MIX_VOLUME(int T, int V, int boost) {
  #if defined(MIXING_NORMAL) || defined(MIXING_EXTREME)
	int perc_lower, b_m, b_v;
	int Me, Ve;
	int roothack;
	int Mdiff, Vdiff, totaldiff;
  #endif
	int total;
	int M = cfg_audio_master_volume;

//c_msg_format("V:%d,boost:%d,cfgM:%d,cfgU:%d,*:%d,+:%d", V, boost, cfg_audio_master_volume, cfg_audio_music_volume, V * boost * cfg_audio_master_volume, (V + cfg_audio_master_volume) / 2);
	/* Normal, unboosted audio -- note that anti-boost is applied before evlt[], while actual boost (further below) is applied to evlt[] value. */
  #ifdef MIXING_NORMAL
	if (boost <= 100) return((cfg_audio_master * T * evlt[(V * boost) / 100] * evlt[M]) / MIX_MAX_VOLUME);
  #endif
  #ifdef MIXING_EXTREME
	if (boost <= 100) return(cfg_audio_master * T * evlt[(V * boost * M) / 10000]);
  #endif
  #ifdef MIXING_ULTRA
	if (boost <= 100) {
		total = ((V + 10) * (M + 10) * boost) / 10000 - 1; //0..120 (with boost < 100%, -1 may result)
		if (total < 0) total = 0; //prevent '-1' underflow when boost and volume is all low
//c_msg_format("v_=%d (%d*%d*(%d)=%d - %d)", evlt[total], M+10, V+10, boost, total, M);
		return(cfg_audio_master * T * evlt[total]);
	}
  #endif
  #ifdef MIXING_ADDITIVE
	if (boost <= 100) return(cfg_audio_master * T * evlt[((V + M) * boost) / 200]);
  #endif

  #if defined(MIXING_NORMAL) || defined(MIXING_EXTREME)
	boost -= 100;

	/* To be exact we'd have to balance roots, eg master slider has 3x as much room for boosting as specific slider ->
	   it gets 4rt(boost)^3 while specific slider gets 4rt(boost)^1, but I don't wanna do these calcs here, so I will
	   just hack it for now to use simple division and then subtract 4..11% from the result :-p
	   However, even that hack is inaccurate, as we would like to apply it to evlt[] values, not raw values. However, that requires us to apply
	   the perc_lower root factors to the evlt[] values too, again not to the raw values. BUT.. if we do that, we get anomalities such as a boost
	   value of 110 resulting in slightly LOWER sound than unboosted 100% sound, since the root_hack value doesn't necessarily correspond with
	   the jumps in the evlt[] table.
	   So, for now, I just apply the roothack on evlt[] and we live with the small inaccuracy =P - C. Blue */

	/* If we boost the volume, we have to distribute it on the two sliders <specific audio slider> vs <master audio slider> to be most effective. */
	if (M == 100 && V == 100) {
		/* Extreme case: Cannot boost at all, everything is at max already. */
		Me = evlt[M];
		Ve = evlt[V];
		return((cfg_audio_master * T * Ve * Me) / MIX_MAX_VOLUME);
	} else if (!M) {
		/* Extreme case (just treat it extra cause of rounding issue 99.9 vs 100) */
		roothack = 100;
		//M = (M * (100 + boost)) / 100; //--wrong (always 0)
		M = boost / 10;
	} else if (!V) {
		/* Extreme case (just treat it extra cause of rounding issue 99.9 vs 100) */
		roothack = 100;
		//V = (V * (100 + boost)) / 100; //--wrong (always 0)
		V = boost / 10;
	} else if (M > V) {
		/* Master slider is higher -> less capacity to boost it, instead load boosting on the specific slider more than on the master slider */
		Mdiff = 100 - M;
		Vdiff = 100 - V;
		totaldiff = Mdiff + Vdiff;
		perc_lower = (100 * Mdiff) / totaldiff;
		roothack = 100 - (21 - 1045 / (50 + perc_lower));

		b_m = (boost * perc_lower) / 100;
		b_v = (boost * (100 - perc_lower)) / 100;
		M = (M * (100 + b_m)) / 100;
		V = (V * (100 + b_v)) / 100;
	} else if (M < V) {
		/* Master slider is lower -> more capacity to boost it, so load boosting on the master slider more than on the specific slider */
		Mdiff = 100 - M;
		Vdiff = 100 - V;
		totaldiff = Mdiff + Vdiff;
		perc_lower = (100 * Vdiff) / totaldiff;
		roothack = 100 - (21 - 1045 / (50 + perc_lower));

		b_m = (boost * (100 - perc_lower)) / 100;
		b_v = (boost * perc_lower) / 100;
		M = (M * (100 + b_m)) / 100;
		V = (V * (100 + b_v)) / 100;
	} else {
		/* Both sliders are equal - distribute boost evenly (sqrt(boost)) */
		perc_lower = 50;
		roothack = 100 - (21 - 1045 / (50 + perc_lower));

		b_m = b_v = (boost * perc_lower) / 100;
		M = (M * (100 + b_m)) / 100;
		V = (V * (100 + b_v)) / 100;
	}

	/* Limit against overboosting into invalid values */
	if (M > 100) M = 100;
	if (V > 100) V = 100;

	Me = evlt[M];
	Ve = evlt[V];
	total = ((Me * Ve * roothack) / 100) / MIX_MAX_VOLUME;

  #elif defined(MIXING_ADDITIVE)
	/* A felt duplication of +100% corresponds roughly to 4 slider steps, aka +40 volume */
	boost = (boost - 100) * 4; //0..400
	boost /= 10; //+0..+40

	/* Compensate for less effective boost around low-mids (see evlt[] comment) for added-up slider pos range 4..7 */
	if (M + V >= 40 && M + V <= 70) {
		if (M + V == 40 || M + V == 70) boost += 10; //including 60 too may be disputable~
		else boost += 20;
	}

	/* Boost total volume */
	total = M + V + boost;

	/* Limit against overboosting into invalid values */
	if (total > 200) total = 200;

	/* Map to exponential volume */
	total = evlt[total / 2];
//c_msg_format("vB=%d (%d+%d+%d=%d)", total, M+10, V+10, boost, M + V + boost);

  #else /* MIXING_ULTRA */
	boost = (boost - 100); //+0..+100%
   #if 0 /* multiplicative boosting - doesn't do much at very low volume slider levels */
	boost /= 3;//+0..+33%
	/* Boost total volume */
	//total = ((M + 10) * (V + 10) * (100 + boost)) / 10000 - 1; //strictly rounded down boost, will not boost lowest volume since index is only 1.3 vs 1 -> still rounded down
	total = ((M + 10) * (V + 10) * (100 + boost) + 8400) / 10000 - 1; //will reach at least +1 volume index if at least 150% boosting (50%/3 = 16 ->> 1.16 + .84 = 2)
   #else /* additive boosting */
	/* A felt duplication of +100% corresponds roughly to 4 slider steps */
	boost = (boost * 4) / 10; //+0..+40
	total = ((M + 10 + boost / 2) * (V + 10 + boost / 2)) / 100 - 1; //will reach at least +1 volume index if at least 150% boosting (50%/3 = 16 ->> 1.16 + .84 = 2)
   #endif

	/* Limit against overboosting into invalid values */
	if (total > 120) total = 120;

	/* Map to superlinear volume */
	total = evlt[total];
//c_msg_format("vB=%d (%d+%d+%d)", total, M+10, V+10, 100+boost);
  #endif

	/* Return boosted >100% result */
	return(cfg_audio_master * T * total);
  }
 #endif
#endif
#if 0 /* use linear volume scale -- poly+50 would be best, but already causes mixer outtages if total volume becomes too small (ie < 1).. =_= */
 #define CALC_MIX_VOLUME(T, V)  ((MIX_MAX_VOLUME * (cfg_audio_master ? ((T) ? (V) : 0) : 0) * cfg_audio_master_volume) / 10000)
#endif
#if 0 /* use polynomial volume scale -- '+50' seems best, although some low-low combos already won't produce sound at all due to low mixer resolution. maybe just use linear scale, simply? */
 #define CALC_MIX_VOLUME(T, V)	((MIX_MAX_VOLUME * (cfg_audio_master ? ((T) ? (((V) + 50) * ((V) + 50)) / 100 - 25 : 0) : 0) * (((cfg_audio_master_volume + 50) * (cfg_audio_master_volume + 50)) / 100 - 25)) / 40000)
// #define CALC_MIX_VOLUME(T, V)	((MIX_MAX_VOLUME * (cfg_audio_master ? ((T) ? (((V) + 30) * ((V) + 30)) / 100 - 9 : 0) : 0) * (((cfg_audio_master_volume + 30) * (cfg_audio_master_volume + 30)) / 100 - 9)) / 25600)
// #define CALC_MIX_VOLUME(T, V)	((MIX_MAX_VOLUME * (cfg_audio_master ? ((T) ? (((V) + 10) * ((V) + 10)) / 100 - 1 : 0) : 0) * (((cfg_audio_master_volume + 10) * (cfg_audio_master_volume + 10)) / 100 - 1)) / 14400)
#endif
#if 0 /* use exponential volume scale (whoa) - but let's use a lookup table for efficiency ^^ EDIT: too extreme */
 static int vol[11] = { 0, 1, 2, 4, 6, 10, 16, 26, 42, 68, 100 }; /* this is similar to rounded down 1.6^(0..10) */
 #define CALC_MIX_VOLUME(T, V)	((MIX_MAX_VOLUME * (cfg_audio_master ? ((T) ? vol[(V) / 10] : 0) : 0) * vol[cfg_audio_master_volume / 10]) / 10000)
#endif
#if 0 /* also too extreme, even more so */
 static int vol[11] = { 0, 1, 2, 3, 5, 8, 14, 25, 42, 68, 100 }; /* this is similar to rounded down 1.6^(0..10) */
 #define CALC_MIX_VOLUME(T, V)	((MIX_MAX_VOLUME * (cfg_audio_master ? ((T) ? vol[(V) / 10] : 0) : 0) * vol[cfg_audio_master_volume / 10]) / 10000)
#endif


/* Positional audio - C. Blue
   Keeping it retro-style with this neat sqrt table.. >_>
   Note that it must cover (for pythagoras) the maximum sum of dx^2+dy^2 where distance(0,0,dy,dx) can be up to MAX_SIGHT [20]!
   This is unfortunately full of rounding/approximation issues, so we will just go with dx,dy=14,13 (365) and dx,dy=19,3 (370, max!)
   actually (19,2 but rounding!) as limits and reduce coords beyond this slightly. */
static int fast_sqrt[371] = { /* 0..370, for panning */
    0,1,1,1,2,2,2,2,2,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
    14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,
    18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,19,19,19,19,19,19,19,19,19,19
};

/* Struct representing all data about an event sample */
typedef struct {
	int num;                        /* Number of samples for this event */
	Mix_Chunk *wavs[MAX_SAMPLES];   /* Sample array */
	const char *paths[MAX_SAMPLES]; /* Relative pathnames for samples */
	int current_channel;		/* Channel it's currently being played on, -1 if none; to avoid
					   stacking of the same sound multiple (read: too many) times - ...
					   note that with 4.4.5.4+ this is deprecated - C. Blue */
	int started_timer_tick;		/* global timer tick on which this sample was started (for efficiency) */
	bool disabled;			/* disabled by user? */
	bool config;
	unsigned char volume;		/* volume reduced by user? */
} sample_list;

/* background music */
typedef struct {
	int num;
	Mix_Music *wavs[MAX_SONGS];
	const char *paths[MAX_SONGS];
	bool initial[MAX_SONGS];	/* Is it an 'initial' song? An initial song is played first and only once when a music event gets activated. */
	bool disabled;			/* disabled by user? */
	bool config;
	unsigned char volume;		/* volume reduced by user? */
} song_list;

/* Just need an array of SampInfos */
static sample_list samples[SOUND_MAX_2010];

/* Array of potential channels, for managing that only one
   sound of a kind is played simultaneously, for efficiency - C. Blue */
static int channel_sample[MAX_CHANNELS];
static int channel_type[MAX_CHANNELS];
static int channel_volume[MAX_CHANNELS];
static s32b channel_player_id[MAX_CHANNELS];

/* Music Array */
static song_list songs[MUSIC_MAX];

#ifdef ENABLE_JUKEBOX
/* Jukebox music */
static int jukebox_org = -1, jukebox_playing = -1;
#endif


/*
 * Shut down the sound system and free resources.
 */
static void close_audio(void) {
	size_t i;
	int j;

	/* Kill the loading thread if it's still running */
	if (load_audio_thread) {
		//kill_load_audio_thread = TRUE; -- not needed (see far above)
		SDL_WaitThread(load_audio_thread, NULL);
	}

	Mix_HaltMusic();

	/* Free all the sample data*/
	for (i = 0; i < SOUND_MAX_2010; i++) {
		sample_list *smp = &samples[i];

		/* Nuke all samples */
		for (j = 0; j < smp->num; j++) {
			Mix_FreeChunk(smp->wavs[j]);
			string_free(smp->paths[j]);
		}
	}

	/* Free all the music data*/
	for (i = 0; i < MUSIC_MAX; i++) {
		song_list *mus = &songs[i];

		/* Nuke all samples */
		for (j = 0; j < mus->num; j++) {
			Mix_FreeMusic(mus->wavs[j]);
			string_free(mus->paths[j]);
		}
	}

#ifndef SEGFAULT_HACK
	string_free(ANGBAND_DIR_XTRA_SOUND);
	string_free(ANGBAND_DIR_XTRA_MUSIC);
#endif

	/* Close the audio */
	Mix_CloseAudio();

	SDL_DestroyMutex(load_sample_mutex_entrance);
	SDL_DestroyMutex(load_song_mutex_entrance);
	SDL_DestroyMutex(load_sample_mutex);
	SDL_DestroyMutex(load_song_mutex);

	/* XXX This may conflict with the SDL port */
	SDL_Quit();
}

/* Just for external call when using  = I  to install an audio pack while already running */
void close_audio_sdl(void) {
	close_audio();
}


/*
 * Initialise SDL and open the mixer
 */
static bool open_audio(void) {
	int audio_rate;
	Uint16 audio_format;
	int audio_channels;

	/* Initialize variables */
	if (cfg_audio_rate < 4000) cfg_audio_rate = 4000;
	if (cfg_audio_rate > 48000) cfg_audio_rate = 48000;
	audio_rate = cfg_audio_rate;
	audio_format = AUDIO_S16SYS;
	audio_channels = MIX_DEFAULT_CHANNELS; // this is == 2

	/* Initialize the SDL library */
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
//#ifdef DEBUG_SOUND
		plog_fmt("Couldn't initialize SDL: %s", SDL_GetError());
//		puts(format("Couldn't initialize SDL: %s", SDL_GetError()));
//#endif
		return FALSE;
	}

	/* Try to open the audio */
	if (cfg_audio_buffer < 128) cfg_audio_buffer = 128;
	if (cfg_audio_buffer > 8192) cfg_audio_buffer = 8192;
	if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, cfg_audio_buffer) < 0) {
//#ifdef DEBUG_SOUND
		plog_fmt("Couldn't open mixer: %s", SDL_GetError());
//		puts(format("Couldn't open mixer: %s", SDL_GetError()));
//#endif
		return FALSE;
	}

	if (cfg_max_channels > MAX_CHANNELS) cfg_max_channels = MAX_CHANNELS;
	if (cfg_max_channels < 4) cfg_max_channels = 4;
	Mix_AllocateChannels(cfg_max_channels);
	/* set hook for clearing faded-out weather and for managing sound-fx playback */
	Mix_ChannelFinished(clear_channel);
	/* set hook for fading over to next song */
	Mix_HookMusicFinished(fadein_next_music);

	/* Success */
	return TRUE;
}



/*
 * Read sound.cfg and map events to sounds; then load all the sounds into
 * memory to avoid I/O latency later.
 */
#define MAX_REFERENCES 100
static bool sound_sdl_init(bool no_cache) {
	char path[2048];
	char buffer0[4096], *buffer = buffer0, bufferx[4096];
	FILE *fff;
	int i;
	char out_val[160];
	bool disabled;

	bool events_loaded_semaphore;

	int references = 0;
	int referencer[MAX_REFERENCES];
	bool reference_initial[MAX_REFERENCES];
	char referenced_event[MAX_REFERENCES][40];


	load_sample_mutex_entrance = SDL_CreateMutex();
	load_song_mutex_entrance = SDL_CreateMutex();
	load_sample_mutex = SDL_CreateMutex();
	load_song_mutex = SDL_CreateMutex();

	/* Initialise the mixer  */
	if (!open_audio()) return FALSE;

#ifdef DEBUG_SOUND
	puts(format("sound_sdl_init() opened at %d Hz.", cfg_audio_rate));
#endif

	/* Initialize sound-fx channel management */
	for (i = 0; i < cfg_max_channels; i++) channel_sample[i] = -1;


	/* ------------------------------- Init Sounds */

	/* Build the "sound" path */
	path_build(path, sizeof(path), ANGBAND_DIR_XTRA, cfg_soundpackfolder);
#ifndef SEGFAULT_HACK
	ANGBAND_DIR_XTRA_SOUND = string_make(path);
#else
	strcpy(ANGBAND_DIR_XTRA_SOUND, path);
#endif

	/* Find and open the config file */
#if 0
 #ifdef WINDOWS
	/* On Windows we must have a second config file just to store disabled-state, since we cannot write to Program Files folder after Win XP anymore..
	   So if it exists, let it override the normal config file. */
	if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
		strcpy(path, getenv("HOMEDRIVE"));
		strcat(path, getenv("HOMEPATH"));
		strcat(path, "\\TomeNET-sound.cfg");
	} else path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "TomeNET-sound.cfg"); //paranoia

	fff = my_fopen(path, "r");
	if (!fff) {
		path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "sound.cfg");
		fff = my_fopen(path, "r");
	}
 #else
	path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "sound.cfg");
	fff = my_fopen(path, "r");
 #endif
#else
	path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "sound.cfg");
	fff = my_fopen(path, "r");
#endif

	/* Handle errors */
	if (!fff) {
#if 0
		plog_fmt("Failed to open sound config (%s):\n    %s", path, strerror(errno));
		return FALSE;
#else /* try to use simple default file */
		FILE *fff2;
		char path2[2048];

		path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "sound.cfg.default");
		fff = my_fopen(path, "r");
		if (!fff) {
			plog_fmt("Failed to open default sound config (%s):\n    %s", path, strerror(errno));
			return FALSE;
		}

		path_build(path2, sizeof(path2), ANGBAND_DIR_XTRA_SOUND, "sound.cfg");
		fff2 = my_fopen(path2, "w");
		if (!fff2) {
			plog_fmt("Failed to write sound config (%s):\n    %s", path2, strerror(errno));
			return FALSE;
		}

		while (my_fgets(fff, buffer, sizeof(buffer)) == 0)
			fprintf(fff2, "%s\n", buffer);

		my_fclose(fff2);
		my_fclose(fff);

		/* Try again */
		path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "sound.cfg");
		fff = my_fopen(path, "r");
		if (!fff) {
			plog_fmt("Failed to open sound config (%s):\n    %s", path, strerror(errno));
			return FALSE;
		}
#endif
	}

#ifdef DEBUG_SOUND
	puts("sound_sdl_init() reading sound.cfg:");
#endif

	/* Parse the file */
	/* Lines are always of the form "name = sample [sample ...]" */
	while (my_fgets(fff, buffer0, sizeof(buffer0)) == 0) {
		char *cfg_name;
		cptr lua_name;
		char *sample_list;
		char *search;
		char *cur_token;
		char *next_token;
		int event;
		char *c;

		/* (2022, 4.9.0) strip preceding spaces/tabs */
		c = buffer0;
		while (*c == ' ' || *c == '\t') c++;
		strcpy(bufferx, c);
#if 1 /* linewrap via trailing ' \' */
		/* New (2018): Allow linewrapping via trailing ' \' character sequence right before EOL */
		while (strlen(buffer0) >= 2 && buffer0[strlen(buffer0) - 1] == '\\' && (buffer0[strlen(buffer0) - 2] == ' ' || buffer0[strlen(buffer0) - 2] == ' ')) {
			if (strlen(bufferx) + strlen(buffer0) >= 4096) continue; /* String overflow protection: Discard all that is too much. */
			buffer0[strlen(buffer0) - 2] = 0; /* Discard the '\' and the space (we re-insert the space next, if required) */
			if (my_fgets(fff, buffer0, sizeof(buffer0)) == 0) {
				/* If the continuation of the wrapped line doesn't start on a space, re-insert a space to ensure proper parameter separation */
				if (buffer0[0] != ' ' && buffer0[0] != '\t' && buffer0[0]) strcat(bufferx, " ");
				strcat(bufferx, buffer0);
			}
		}
#endif
		strcpy(buffer0, bufferx);

		/* Lines starting on ';' count as 'provided event' but actually
		   remain silent, as a way of disabling effects/songs without
		   letting the server know. */
		buffer = buffer0;
		if (buffer0[0] == ';') {
			buffer++;
			disabled = TRUE;
		} else disabled = FALSE;

		/* Skip anything not beginning with an alphabetic character */
		if (!buffer[0] || !isalpha((unsigned char)buffer[0])) continue;

		/* Skip meta data that we don't need here -- this is for [title] tag introduced in 4.7.1b+ */
		if (!strncmp(buffer, "packname", 8) || !strncmp(buffer, "author", 6) || !strncmp(buffer, "description", 11)) continue;

		/* Split the line into two: the key, and the rest */

		search = strchr(buffer, '=');
		/* no event name given? */
		if (!search) continue;
		*search = 0;
		/* Event name (key): Trim spaces/tabs */
		while (search[strlen(search) - 1] == ' ' || search[strlen(search) - 1] == '\t') search[strlen(search) - 1] = 0;
		while (search[strlen(search) - 1] == ' ' || search[strlen(search) - 1] == '\t') search[strlen(search) - 1] = 0;

		/* Set the event name */
		cfg_name = buffer;

		/* Make sure this is a valid event name */
		for (event = SOUND_MAX_2010 - 1; event >= 0; event--) {
			sprintf(out_val, "return get_sound_name(%d)", event);
			lua_name = string_exec_lua(0, out_val);
			if (!strlen(lua_name)) continue;
			if (strcmp(cfg_name, lua_name) == 0) break;
		}
		if (event < 0) {
			fprintf(stderr, "Sound effect '%s' not in audio.lua\n", cfg_name);
			continue;
		}

		/* Songs: Trim spaces/tabs */
		c = search + 1;
		while (*c == ' ' || *c == '\t') c++;
		sample_list = c;

		/* no audio filenames listed? */
		if (!sample_list[0]) continue;

		/* Terminate the current token */
		cur_token = sample_list;
		/* Handle sample names within quotes */
		if (cur_token[0] == '\"') {
			cur_token++;
			search = strchr(cur_token, '\"');
			if (search) {
				search[0] = '\0';
				search = strpbrk(search + 1, " \t");
			}
		} else {
			search = strpbrk(cur_token, " \t");
		}
		if (search) {
			search[0] = '\0';
			next_token = search + 1;
		} else {
			next_token = NULL;
		}

		/* Sounds: Trim spaces/tabs */
		if (next_token) while (*next_token == ' ' || *next_token == '\t') next_token++;

		/*
		 * Now we find all the sample names and add them one by one
		*/
		events_loaded_semaphore = FALSE;
		while (cur_token) {
			int num = samples[event].num;

			/* Don't allow too many samples */
			if (num >= MAX_SAMPLES) break;

			/* Build the path to the sample */
			path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, cur_token);
			if (!my_fexists(path)) {
				fprintf(stderr, "Can't find sample '%s'\n", cur_token);
				goto next_token_snd;
			}

			/* Don't load now if we're not caching */
			if (no_cache) {
				/* Just save the path for later */
				samples[event].paths[num] = string_make(path);
			} else {
				/* Load the file now */
				samples[event].wavs[num] = Mix_LoadWAV(path);
				if (!samples[event].wavs[num]) {
					plog_fmt("%s: %s", SDL_GetError(), strerror(errno));
					puts(format("%s: %s (%s)", SDL_GetError(), strerror(errno), path));//DEBUG USE_SOUND_2010
					goto next_token_snd;
				}
			}
			/* Initialize as 'not being played' */
			samples[event].current_channel = -1;

			/* Imcrement the sample count */
			samples[event].num++;
			if (!events_loaded_semaphore) {
				events_loaded_semaphore = TRUE;
				audio_sfx++;
				/* for do_cmd_options_...(): remember that this sample was mentioned in our config file */
				samples[event].config = TRUE;
			}

			next_token_snd:

			/* Figure out next token */
			cur_token = next_token;
			if (next_token) {
				/* Handle song names within quotes */
				if (cur_token[0] == '\"') {
					cur_token++;
					search = strchr(cur_token, '\"');
					if (search) {
						search[0] = '\0';
						search = strpbrk(search + 1, " \t");
					}
				} else {
					/* Try to find a space */
					search = strpbrk(cur_token, " \t");
				}
				/* If we can find one, terminate, and set new "next" */
				if (search) {
					search[0] = '\0';
					next_token = search + 1;
				} else {
					/* Otherwise prevent infinite looping */
					next_token = NULL;
				}

				/* Sounds: Trim spaces/tabs */
				if (next_token) while (*next_token == ' ' || *next_token == '\t') next_token++;
			}
		}

		/* disable this sfx? */
		if (disabled) samples[event].disabled = TRUE;
	}

#ifdef WINDOWS
	/* On Windows we must have a second config file just to store disabled-state, since we cannot write to Program Files folder after Win XP anymore..
	   So if it exists, let it override the normal config file. */
	if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
		strcpy(path, getenv("HOMEDRIVE"));
		strcat(path, getenv("HOMEPATH"));
		strcat(path, "\\TomeNET-nosound.cfg");
	} else path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "TomeNET-nosound.cfg"); //paranoia

	fff = my_fopen(path, "r");
	if (fff) {
		while (my_fgets(fff, buffer0, sizeof(buffer0)) == 0) {
			/* find out event state (disabled/enabled) */
			i = exec_lua(0, format("return get_sound_index(\"%s\")", buffer0));
			/* unknown (different game version/sound pack?) */
			if (i == -1 || !samples[i].config) continue;
			/* disable samples listed in this file */
			samples[i].disabled = TRUE;
		}
	}
	my_fclose(fff);
#endif

#ifdef WINDOWS
	if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
		strcpy(path, getenv("HOMEDRIVE"));
		strcat(path, getenv("HOMEPATH"));
		strcat(path, "\\TomeNET-soundvol.cfg");
	} else path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "TomeNET-soundvol.cfg"); //paranoia
#else
	path_build(path, 1024, ANGBAND_DIR_XTRA_SOUND, "TomeNET-soundvol.cfg");
#endif

	fff = my_fopen(path, "r");
	if (fff) {
		while (my_fgets(fff, buffer0, sizeof(buffer0)) == 0) {
			/* find out event state (disabled/enabled) */
			i = exec_lua(0, format("return get_sound_index(\"%s\")", buffer0));
			if (my_fgets(fff, buffer0, sizeof(buffer0))) break; /* Error, incomplete entry */
			/* unknown (different game version/sound pack?) */
			if (i == -1 || !samples[i].config) continue;
			/* set sample volume in this file */
			samples[i].volume = atoi(buffer0);
		}
	}
	my_fclose(fff);


	/* ------------------------------- Init Music */

	buffer = buffer0;

	/* Build the "music" path */
	path_build(path, sizeof(path), ANGBAND_DIR_XTRA, cfg_musicpackfolder);
#ifndef SEGFAULT_HACK
	ANGBAND_DIR_XTRA_MUSIC = string_make(path);
#else
	strcpy(ANGBAND_DIR_XTRA_MUSIC, path);
#endif

	/* Find and open the config file */
#if 0
 #ifdef WINDOWS
	/* On Windows we must have a second config file just to store disabled-state, since we cannot write to Program Files folder after Win XP anymore..
	   So if it exists, let it override the normal config file. */
	if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
		strcpy(path, getenv("HOMEDRIVE"));
		strcat(path, getenv("HOMEPATH"));
		strcat(path, "\\TomeNET-music.cfg");
	} else path_build(path, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "TomeNET-music.cfg"); //paranoia

	fff = my_fopen(path, "r");
	if (!fff) {
		path_build(path, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "music.cfg");
		fff = my_fopen(path, "r");
	}
 #else
	path_build(path, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "music.cfg");
	fff = my_fopen(path, "r");
 #endif
#else
	path_build(path, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "music.cfg");
	fff = my_fopen(path, "r");
#endif

	/* Handle errors */
	if (!fff) {
#if 0
		plog_fmt("Failed to open music config (%s):\n    %s", path, strerror(errno));
		return FALSE;
#else /* try to use simple default file */
		FILE *fff2;
		char path2[2048];

		path_build(path, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "music.cfg.default");
		fff = my_fopen(path, "r");
		if (!fff) {
			plog_fmt("Failed to open default music config (%s):\n    %s", path, strerror(errno));
			return FALSE;
		}

		path_build(path2, sizeof(path2), ANGBAND_DIR_XTRA_MUSIC, "music.cfg");
		fff2 = my_fopen(path2, "w");
		if (!fff2) {
			plog_fmt("Failed to write music config (%s):\n    %s", path2, strerror(errno));
			return FALSE;
		}

		while (my_fgets(fff, buffer, sizeof(buffer)) == 0)
			fprintf(fff2, "%s\n", buffer);

		my_fclose(fff2);
		my_fclose(fff);

		/* Try again */
		path_build(path, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "music.cfg");
		fff = my_fopen(path, "r");
		if (!fff) {
			plog_fmt("Failed to open music config (%s):\n    %s", path, strerror(errno));
			return FALSE;
		}
#endif
	}

#ifdef DEBUG_SOUND
	puts("sound_sdl_init() reading music.cfg:");
#endif

	/* Parse the file */
	/* Lines are always of the form "name = music [music ...]" */
	while (my_fgets(fff, buffer0, sizeof(buffer0)) == 0) {
		char *cfg_name;
		cptr lua_name;
		char *song_list;
		char *search;
		char *cur_token;
		char *next_token;
		int event;
		bool initial, reference;
		char *c;

		/* (2022, 4.9.0) strip preceding spaces/tabs */
		c = buffer0;
		while (*c == ' ' || *c == '\t') c++;
		strcpy(bufferx, c);
#if 1 /* linewrap via trailing ' \' */
		/* New (2018): Allow linewrapping via trailing ' \' character sequence right before EOL */
		while (strlen(buffer0) >= 2 && buffer0[strlen(buffer0) - 1] == '\\' && (buffer0[strlen(buffer0) - 2] == ' ' || buffer0[strlen(buffer0) - 2] == '\t')) {
			if (strlen(bufferx) + strlen(buffer0) >= 4096) continue; /* String overflow protection: Discard all that is too much. */
			buffer0[strlen(buffer0) - 2] = 0; /* Discard the '\' and the space (we re-insert the space next, if required) */
			if (my_fgets(fff, buffer0, sizeof(buffer0)) == 0) {
				/* If the continuation of the wrapped line doesn't start on a space, re-insert a space to ensure proper parameter separation */
				if (buffer0[0] != ' ' && buffer0[0] != '\t' && buffer0[0]) strcat(bufferx, " ");
				strcat(bufferx, buffer0);
			}
		}
#endif
		strcpy(buffer0, bufferx);

		/* Lines starting on ';' count as 'provided event' but actually
		   remain silent, as a way of disabling effects/songs without
		   letting the server know. */
		buffer = buffer0;
		if (buffer0[0] == ';') {
			buffer++;
			disabled = TRUE;
		} else disabled = FALSE;

		/* Skip anything not beginning with an alphabetic character */
		if (!buffer[0] || !isalpha((unsigned char)buffer[0])) continue;

		/* Skip meta data that we don't need here -- this is for [title] tag introduced in 4.7.1b+ */
		if (!strncmp(buffer, "packname", 8) || !strncmp(buffer, "author", 6) || !strncmp(buffer, "description", 11)) continue;


		/* Split the line into two: the key, and the rest */

		search = strchr(buffer, '=');
		/* no event name given? */
		if (!search) continue;
		*search = 0;
		/* Event name (key): Trim spaces/tabs */
		while (search[strlen(search) - 1] == ' ' || search[strlen(search) - 1] == '\t') search[strlen(search) - 1] = 0;
		while (search[strlen(search) - 1] == ' ' || search[strlen(search) - 1] == '\t') search[strlen(search) - 1] = 0;

		/* Set the event name */
		cfg_name = buffer;

		/* Make sure this is a valid event name */
		for (event = MUSIC_MAX - 1; event >= 0; event--) {
			sprintf(out_val, "return get_music_name(%d)", event);
			lua_name = string_exec_lua(0, out_val);
			if (!strlen(lua_name)) continue;
			if (strcmp(cfg_name, lua_name) == 0) break;
		}
		if (event < 0) {
			fprintf(stderr, "Music event '%s' not in audio.lua\n", cfg_name);
			continue;
		}

		/* Songs: Trim spaces/tabs */
		c = search + 1;
		while (*c == ' ' || *c == '\t') c++;
		song_list = c;

		/* no audio filenames listed? */
		if (!song_list[0]) continue;

		/* Terminate the current token */
		cur_token = song_list;
		/* Handle '!' indicator for 'initial' songs */
		if (cur_token[0] == '!') {
			cur_token++;
			initial = TRUE;
		} else initial = FALSE;
		/* Handle '+' indicator for 'referenced' music events */
		reference = FALSE;
		if (cur_token[0] == '+') {
			reference = TRUE;
			cur_token++;
		}
		/* Handle song names within quotes */
		if (cur_token[0] == '\"') {
			cur_token++;
			search = strchr(cur_token, '\"');
			if (search) {
				search[0] = '\0';
				search = strpbrk(search + 1, " \t");
			}
		} else {
			search = strpbrk(cur_token, " \t");
		}
		if (search) {
			search[0] = '\0';
			next_token = search + 1;
		} else {
			next_token = NULL;
		}

		/* Songs: Trim spaces/tabs */
		if (next_token) while (*next_token == ' ' || *next_token == '\t') next_token++;

		/*
		 * Now we find all the song names and add them one by one
		*/
		events_loaded_semaphore = FALSE;
		while (cur_token) {
			int num = songs[event].num;

			/* Don't allow too many songs */
			if (num >= MAX_SONGS) break;

			/* Handle reference */
			if (reference) {
				/* Too many references already? Skip it */
				if (references >= MAX_REFERENCES) goto next_token_mus;
				/* Remember reference for handling them all later, to avoid cycling reference problems */
				referencer[references] = event;
				reference_initial[references] = initial;
				strcpy(referenced_event[references], cur_token);
				references++;
				goto next_token_mus;
			}

			/* Build the path to the sample */
			path_build(path, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, cur_token);
			if (!my_fexists(path)) {
				fprintf(stderr, "Can't find song '%s'\n", cur_token);
				goto next_token_mus;
			}

			songs[event].initial[num] = initial;

			/* Don't load now if we're not caching */
			if (no_cache) {
				/* Just save the path for later */
				songs[event].paths[num] = string_make(path);
			} else {
				/* Load the file now */
				songs[event].wavs[num] = Mix_LoadMUS(path);
				if (!songs[event].wavs[num]) {
					//puts(format("PRBPATH: lua_name %s, ev %d, num %d, path %s", lua_name, event, num, path));
					plog_fmt("%s: %s", SDL_GetError(), strerror(errno));
					//puts(format("%s: %s", SDL_GetError(), strerror(errno)));//DEBUG USE_SOUND_2010
					goto next_token_mus;
				}
			}

			//puts(format("loaded song %s (ev %d, #%d).", songs[event].paths[num], event, num));//debug
			/* Imcrement the sample count */
			songs[event].num++;
			if (!events_loaded_semaphore) {
				events_loaded_semaphore = TRUE;
				audio_music++;
				/* for do_cmd_options_...(): remember that this sample was mentioned in our config file */
				songs[event].config = TRUE;
			}

			next_token_mus:

			/* Figure out next token */
			cur_token = next_token;
			if (next_token) {
				/* Handle '!' indicator for 'initial' songs */
				if (cur_token[0] == '!') {
					cur_token++;
					initial = TRUE;
				} else initial = FALSE;
				/* Handle '+' indicator for 'referenced' music events */
				reference = FALSE;
				if (cur_token[0] == '+') {
					reference = TRUE;
					cur_token++;
				}
				/* Handle song names within quotes */
				if (cur_token[0] == '\"') {
					cur_token++;
					search = strchr(cur_token, '\"');
					if (search) {
						search[0] = '\0';
						search = strpbrk(search + 1, " \t");
					}
				} else {
					/* Try to find a space */
					search = strpbrk(cur_token, " \t");
				}
				/* If we can find one, terminate, and set new "next" */
				if (search) {
					search[0] = '\0';
					next_token = search + 1;
				} else {
					/* Otherwise prevent infinite looping */
					next_token = NULL;
				}

				/* Songs: Trim spaces/tabs */
				if (next_token) while (*next_token == ' ' || *next_token == '\t') next_token++;
			}
		}

		/* disable this song? */
		if (disabled) songs[event].disabled = TRUE;
	}

	/* Solve all stored references now */
	for (i = 0; i < references; i++) {
		int num, event, event_ref, j;
		cptr lua_name;
		bool initial;
		char cur_token[40];

		strcpy(cur_token, referenced_event[i]);
		/* Make sure this is a valid event name */
		for (event = MUSIC_MAX - 1; event >= 0; event--) {
			sprintf(out_val, "return get_music_name(%d)", event);
			lua_name = string_exec_lua(0, out_val);
			if (!strlen(lua_name)) continue;
			if (strcmp(cur_token, lua_name) == 0) break;
		}
		if (event < 0) {
			fprintf(stderr, "Referenced music event '%s' not in audio.lua\n", cur_token);
			continue;
		}
		event_ref = event;

		/* Handle.. */
		event = referencer[i];
		initial = reference_initial[i];
		num = songs[event].num;

		for (j = 0; j < songs[event_ref].num; j++) {
			/* Don't allow too many songs */
			if (num >= MAX_SONGS) break;

			/* Never reference initial songs */
			if (songs[event_ref].initial[j]) continue;

			songs[event].paths[num] = songs[event_ref].paths[j];
			songs[event].wavs[num] = songs[event_ref].wavs[j];
			songs[event].initial[num] = initial;
			num++;
			songs[event].num = num;
			/* for do_cmd_options_...(): remember that this sample was mentioned in our config file */
			songs[event].config = TRUE;
		}
	}

	/* Close the file */
	my_fclose(fff);

#ifdef WINDOWS
	/* On Windows we must have a second config file just to store disabled-state, since we cannot write to Program Files folder after Win XP anymore..
	   So if it exists, let it override the normal config file. */
	if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
		strcpy(path, getenv("HOMEDRIVE"));
		strcat(path, getenv("HOMEPATH"));
		strcat(path, "\\TomeNET-nomusic.cfg");
	} else path_build(path, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "TomeNET-nomusic.cfg"); //paranoia

	fff = my_fopen(path, "r");
	if (fff) {
		while (my_fgets(fff, buffer0, sizeof(buffer0)) == 0) {
			/* find out event state (disabled/enabled) */
			i = exec_lua(0, format("return get_music_index(\"%s\")", buffer0));
			/* unknown (different game version/music pack?) */
			if (i == -1 || !songs[i].config) continue;
			/* disable songs listed in this file */
			songs[i].disabled = TRUE;
		}
	}
#endif
#ifdef WINDOWS
	if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
		strcpy(path, getenv("HOMEDRIVE"));
		strcat(path, getenv("HOMEPATH"));
		strcat(path, "\\TomeNET-musicvol.cfg");
	} else
#endif
	path_build(path, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "TomeNET-musicvol.cfg"); //paranoia

	fff = my_fopen(path, "r");
	if (fff) {
		while (my_fgets(fff, buffer0, sizeof(buffer0)) == 0) {
			/* find out event state (disabled/enabled) */
			i = exec_lua(0, format("return get_music_index(\"%s\")", buffer0));
			if (my_fgets(fff, buffer0, sizeof(buffer0))) break; /* Error, incomplete entry */
			/* unknown (different game version/music pack?) */
			if (i == -1 || !songs[i].config) continue;
			/* set volume of songs listed in this file */
			songs[i].volume = atoi(buffer0);
		}
	}

#ifdef DEBUG_SOUND
	puts("sound_sdl_init() done.");
#endif

	/* Success */
	return TRUE;
}

/*
 * Play a sound of type "event". Returns FALSE if sound couldn't be played.
 */
static bool play_sound(int event, int type, int vol, s32b player_id, int dist_x, int dist_y) {
	Mix_Chunk *wave = NULL;
	int s, vols = 100;
	bool test = FALSE;

#ifdef DISABLE_MUTED_AUDIO
	if (!cfg_audio_master || !cfg_audio_sound) return TRUE; /* claim that it 'succeeded' */
#endif

#ifdef USER_VOLUME_SFX
	/* Apply user-defined custom volume modifier */
	if (samples[event].volume) vols = samples[event].volume;
#endif

	/* hack: */
	if (type == SFX_TYPE_STOP) {
		/* stop all SFX_TYPE_NO_OVERLAP type sounds? */
		if (event == -1) {
			bool found = FALSE;

			for (s = 0; s < cfg_max_channels; s++) {
				if (channel_type[s] == SFX_TYPE_NO_OVERLAP && channel_player_id[s] == player_id) {
					Mix_FadeOutChannel(s, 450); //250..450 (more realistic timing vs smoother sound (avoid final 'spike'))
					found = TRUE;
				}
			}
			return found;
		}
		/* stop sound of this type? */
		else {
			for (s = 0; s < cfg_max_channels; s++) {
				if (channel_sample[s] == event && channel_player_id[s] == player_id) {
					Mix_FadeOutChannel(s, 450); //250..450 (more realistic timing vs smoother sound (avoid final 'spike'))
					return TRUE;
				}
			}
			return FALSE;
		}
	}

	/* Paranoia */
	if (event < 0 || event >= SOUND_MAX_2010) return FALSE;

	if (samples[event].disabled) return TRUE; /* claim that it 'succeeded' */

	/* Check there are samples for this event */
	if (!samples[event].num) return FALSE;

	/* already playing? allow to prevent multiple sounds of the same kind
	   from being mixed simultaneously, for preventing silliness */
	switch (type) {
	case SFX_TYPE_ATTACK: if (c_cfg.ovl_sfx_attack) break; else test = TRUE;
		break;
	case SFX_TYPE_COMMAND: if (c_cfg.ovl_sfx_command) break; else test = TRUE;
		break;
	case SFX_TYPE_MISC: if (c_cfg.ovl_sfx_misc) break; else test = TRUE;
		break;
	case SFX_TYPE_MON_ATTACK: if (c_cfg.ovl_sfx_mon_attack) break; else test = TRUE;
		break;
	case SFX_TYPE_MON_SPELL: if (c_cfg.ovl_sfx_mon_spell) break; else test = TRUE;
		break;
	case SFX_TYPE_MON_MISC: if (c_cfg.ovl_sfx_mon_misc) break; else test = TRUE;
		break;
	case SFX_TYPE_NO_OVERLAP: /* never overlap! (eg tunnelling) */
		test = TRUE;
		break;
	case SFX_TYPE_WEATHER:
	case SFX_TYPE_AMBIENT:
		/* always allow overlapping, since these should be sent by the
		   server in a completely sensibly timed manner anyway. */
		break;
	default:
		test = TRUE;
	}
	if (test) {
#if 0 /* old method before sounds could've come from other players nearby us, too */
		if (samples[event].current_channel != -1) return TRUE;
#else /* so now we need to allow multiple samples, IF they stem from different sources aka players */
		for (s = 0; s < cfg_max_channels; s++) {
			if (channel_sample[s] == event && channel_player_id[s] == player_id) return TRUE;
		}
#endif
	}

	/* prevent playing duplicate sfx that were initiated very closely
	   together in time, after one each other? (efficiency) */
	if (c_cfg.no_ovl_close_sfx && ticks == samples[event].started_timer_tick) return TRUE;


	/* Choose a random event */
	s = rand_int(samples[event].num);
	wave = samples[event].wavs[s];

	/* Try loading it, if it's not cached */
	if (!wave) {
		if (on_demand_loading || no_cache_audio) {
			if (!(wave = load_sample(event, s))) {
				/* we really failed to load it */
				plog(format("SDL sound load failed (%d, %d).", event, s));
				return FALSE;
			}
		} else {
			/* Fail silently */
			return TRUE;
		}
	}

	/* Actually play the thing */
	/* remember, for efficiency management */
	s = Mix_PlayChannel(-1, wave, 0);
	if (s != -1) {
		channel_sample[s] = event;
		channel_type[s] = type;
		channel_volume[s] = vol;
		channel_player_id[s] = player_id;

		/* HACK - use weather volume for thunder sfx */
		if (type == SFX_TYPE_WEATHER)
			Mix_Volume(s, (CALC_MIX_VOLUME(cfg_audio_weather, (cfg_audio_weather_volume * vol) / 100, vols) * grid_weather_volume) / 100);
		else
		if (type == SFX_TYPE_AMBIENT)
			Mix_Volume(s, (CALC_MIX_VOLUME(cfg_audio_sound, (cfg_audio_sound_volume * vol) / 100, vols) * grid_ambient_volume) / 100);
		else
			/* Note: Linear scaling is used here to allow more precise control at the server end */
			Mix_Volume(s, CALC_MIX_VOLUME(cfg_audio_sound, (cfg_audio_sound_volume * vol) / 100, vols));

		/* Simple stereo-positioned audio, only along the x-coords. TODO: HRTF via OpenAL ^^ - C. Blue */
		if (c_cfg.positional_audio) {
#if 0
			/* Just for the heck of it: Trivial (bad) panning, ignoring any y-plane angle and just use basic left/right distance. */
			if (dist_x < 0) Mix_SetPanning(s, 255, (-255 * 4) / (dist_x - 3));
			else if (dist_x > 0) Mix_SetPanning(s, (255 * 4) / (dist_x + 3), 255);
			else Mix_SetPanning(s, 255, 255);
#else
			/* Best we can do with simple stereo for now without HRTF etc.:
			   We don't have a y-audio-plane (aka z-plane really),
			   but at least we pan according to the correct angle. - C. Blue */

			int dy = ABS(dist_y); //we don't differentiate between in front of us / behind us, need HRTF for that.

			/* Hack: Catch edge case: At dist 20 (MAX_SIGHT) there is a +/- 1 leeway orthogonally
			   due to the way distance() works. This is fine, but we only define roots up to 370 for practical reasons.
			   For this tiny angle we can just assume that we receive the same panning as if we stood slightly closer. */
			if (dist_y * dist_y + dist_x * dist_x > 370) {
				if (dy == MAX_SIGHT) dist_x = 0;
				else dist_y = 0;
			}

			if (!dist_x) Mix_SetPanning(s, 255, 255); //shortcut for both, 90 deg and undefined angle aka 'on us'. */
			else if (!dist_y) { //shortcut for 0 deg and 180 deg (ie sin = 0)
				if (dist_x < 0) Mix_SetPanning(s, 255, 0);
				else Mix_SetPanning(s, 0, 255);
			} else { //all other cases (ie sin != 0) -- and d_real cannot be 0 (for division!)
				int pan_l, pan_r;
				int d_real = fast_sqrt[dist_x * dist_x + dist_y * dist_y]; //wow, for once not just an approximated distance (beyond that integer thingy) ^^
				int sin = (10 * dy) / d_real; //sinus (scaled by *10 for accuracy)

				/* Calculate left/right panning weight from angle:
				   The ear with 'los' toward the event gets 100% of the distance-reduced volume (d),
				   while the other ear gets ABS(sin(a)) * d. */
				if (dist_x < 0) { /* somewhere to our left */
					pan_l = 255;
					pan_r = (255 * sin) / 10;
				} else { /* somewhere to our right */
					pan_l = (255 * sin) / 10;
					pan_r = 255;
				}
				Mix_SetPanning(s, pan_l, pan_r);
			}
#endif
		}
	}
	samples[event].current_channel = s;
	samples[event].started_timer_tick = ticks;

	return TRUE;
}
/* play the 'bell' sound */
#define BELL_REDUCTION 3 /* reduce volume of bell() sounds by this factor */
extern bool sound_bell(void) {
	Mix_Chunk *wave = NULL;
	int s, vols = 100;

	if (bell_sound_idx == -1) return FALSE;
	if (samples[bell_sound_idx].disabled) return TRUE; /* claim that it 'succeeded' */
	if (!samples[bell_sound_idx].num) return FALSE;

	/* already playing? prevent multiple sounds of the same kind from being mixed simultaneously, for preventing silliness */
	if (samples[bell_sound_idx].current_channel != -1) return TRUE;

	/* Choose a random event */
	s = rand_int(samples[bell_sound_idx].num);
	wave = samples[bell_sound_idx].wavs[s];

#ifdef USER_VOLUME_SFX
	/* Apply user-defined custom volume modifier */
	if (samples[bell_sound_idx].volume) vols = samples[bell_sound_idx].volume;
#endif

	/* Try loading it, if it's not cached */
	if (!wave) {
		if (on_demand_loading || no_cache_audio) {
			if (!(wave = load_sample(bell_sound_idx, s))) {
				/* we really failed to load it */
				plog(format("SDL sound load failed (%d, %d).", bell_sound_idx, s));
				return FALSE;
			}
		} else {
			/* Fail silently */
			return TRUE;
		}
	}

	/* Actually play the thing */
	/* remember, for efficiency management */
	s = Mix_PlayChannel(-1, wave, 0);
	if (s != -1) {
		channel_sample[s] = bell_sound_idx;
		channel_type[s] = SFX_TYPE_AMBIENT; /* whatever (just so overlapping is possible) */
		if (c_cfg.paging_max_volume) {
			Mix_Volume(s, MIX_MAX_VOLUME / BELL_REDUCTION);
		} else if (c_cfg.paging_master_volume) {
			Mix_Volume(s, CALC_MIX_VOLUME(1, MIX_MAX_VOLUME / BELL_REDUCTION, vols));
		}
	}
	samples[bell_sound_idx].current_channel = s;

	return TRUE;
}
/* play the 'page' sound */
extern bool sound_page(void) {
	Mix_Chunk *wave = NULL;
	int s, vols = 100;

	if (page_sound_idx == -1) return FALSE;
	if (samples[page_sound_idx].disabled) return TRUE; /* claim that it 'succeeded' */
	if (!samples[page_sound_idx].num) return FALSE;

	/* already playing? prevent multiple sounds of the same kind from being mixed simultaneously, for preventing silliness */
	if (samples[page_sound_idx].current_channel != -1) return TRUE;

	/* Choose a random event */
	s = rand_int(samples[page_sound_idx].num);
	wave = samples[page_sound_idx].wavs[s];

#ifdef USER_VOLUME_SFX
	/* Apply user-defined custom volume modifier */
	if (samples[page_sound_idx].volume) vols = samples[page_sound_idx].volume;
#endif

	/* Try loading it, if it's not cached */
	if (!wave) {
		if (on_demand_loading || no_cache_audio) {
			if (!(wave = load_sample(page_sound_idx, s))) {
				/* we really failed to load it */
				plog(format("SDL sound load failed (%d, %d).", page_sound_idx, s));
				return FALSE;
			}
		} else {
			/* Fail silently */
			return TRUE;
		}
	}

	/* Actually play the thing */
	/* remember, for efficiency management */
	s = Mix_PlayChannel(-1, wave, 0);
	if (s != -1) {
		channel_sample[s] = page_sound_idx;
		channel_type[s] = SFX_TYPE_AMBIENT; /* whatever (just so overlapping is possible) */
		if (c_cfg.paging_max_volume) {
			Mix_Volume(s, MIX_MAX_VOLUME);
		} else if (c_cfg.paging_master_volume) {
			Mix_Volume(s, CALC_MIX_VOLUME(1, MIX_MAX_VOLUME, vols));
		}
	}
	samples[page_sound_idx].current_channel = s;

	return TRUE;
}
/* play the 'warning' sound */
extern bool sound_warning(void) {
	Mix_Chunk *wave = NULL;
	int s, vols = 100;

	if (warning_sound_idx == -1) return FALSE;
	if (samples[warning_sound_idx].disabled) return TRUE; /* claim that it 'succeeded' */
	if (!samples[warning_sound_idx].num) return FALSE;

	/* already playing? prevent multiple sounds of the same kind from being mixed simultaneously, for preventing silliness */
	if (samples[warning_sound_idx].current_channel != -1) return TRUE;

	/* Choose a random event */
	s = rand_int(samples[warning_sound_idx].num);
	wave = samples[warning_sound_idx].wavs[s];

#ifdef USER_VOLUME_SFX
	/* Apply user-defined custom volume modifier */
	if (samples[warning_sound_idx].volume) vols = samples[warning_sound_idx].volume;
#endif

	/* Try loading it, if it's not cached */
	if (!wave) {
		if (on_demand_loading || no_cache_audio) {
			if (!(wave = load_sample(warning_sound_idx, s))) {
				/* we really failed to load it */
				plog(format("SDL sound load failed (%d, %d).", warning_sound_idx, s));
				return FALSE;
			}
		} else {
			/* Fail silently */
			return TRUE;
		}
	}

	/* Actually play the thing */
	/* remember, for efficiency management */
	s = Mix_PlayChannel(-1, wave, 0);
	if (s != -1) {
		channel_sample[s] = warning_sound_idx;
		channel_type[s] = SFX_TYPE_AMBIENT; /* whatever (just so overlapping is possible) */
		/* use 'page' sound volume settings for warning too */
		if (c_cfg.paging_max_volume) {
			Mix_Volume(s, MIX_MAX_VOLUME);
		} else if (c_cfg.paging_master_volume) {
			Mix_Volume(s, CALC_MIX_VOLUME(1, MIX_MAX_VOLUME, vols));
		}
	}
	samples[warning_sound_idx].current_channel = s;

	return TRUE;
}


/* Release weather channel after fading out has been completed */
static void clear_channel(int c) {
	/* weather has faded out, mark it as gone */
	if (c == weather_channel) {
		weather_channel = -1;
		return;
	}

	if (c == ambient_channel) {
		ambient_channel = -1;
		return;
	}

	/* a sample has finished playing, so allow this kind to be played again */
	/* hack: if the sample was the 'paging' sound, reset the channel's volume to be on the safe side */
	if (channel_sample[c] == page_sound_idx || channel_sample[c] == warning_sound_idx)
		Mix_Volume(c, CALC_MIX_VOLUME(cfg_audio_sound, cfg_audio_sound_volume, 100));

	/* HACK - if the sample was a weather sample, which would be thunder, reset the vol too, paranoia */
	if (channel_type[c] == SFX_TYPE_WEATHER)
		Mix_Volume(c, CALC_MIX_VOLUME(cfg_audio_sound, cfg_audio_sound_volume, 100));

	samples[channel_sample[c]].current_channel = -1;
	channel_sample[c] = -1;
}

/* Overlay a weather noise -- not WEATHER_VOL_PARTICLES or WEATHER_VOL_CLOUDS */
static void play_sound_weather(int event) {
	Mix_Chunk *wave = NULL;
	int s, new_wc, vols = 100;

	/* allow halting a muted yet playing sound, before checking for DISABLE_MUTED_AUDIO */
	if (event == -2 && weather_channel != -1) {
#ifdef DEBUG_SOUND
		puts(format("w-2: wco %d, ev %d", weather_channel, event));
#endif
		Mix_HaltChannel(weather_channel);
		weather_current = -1;
		return;
	}

	if (event == -1 && weather_channel != -1) {
#ifdef DEBUG_SOUND
		puts(format("w-1: wco %d, ev %d", weather_channel, event));
#endif

		/* if channel is muted anyway, no need to fade out, just halt it instead.
		   HACK: The reason for this is actually to fix this bug:
		   There was a bug where ambient channel wasn't faded out if it was actually
		   muted at the same time, and so it'd continue being active when unmuted
		   again, instead of having been terminated by the fade-out.  */
		if (!cfg_audio_master || !cfg_audio_weather || !cfg_audio_weather_volume) {
			Mix_HaltChannel(weather_channel);
			return;
		}

		if (Mix_FadingChannel(weather_channel) != MIX_FADING_OUT) {
			if (!weather_channel_volume) Mix_HaltChannel(weather_channel); else //hack: workaround SDL bug that doesn't terminate a sample playing at 0 volume after a FadeOut
			Mix_FadeOutChannel(weather_channel, 2000);
		}
		weather_current = -1;
		return;
	}

#ifdef DISABLE_MUTED_AUDIO
	if (!cfg_audio_master || !cfg_audio_weather) return;
#endif

	/* we're already in this weather? */
	if (weather_channel != -1 && weather_current == event &&
	    Mix_FadingChannel(weather_channel) != MIX_FADING_OUT)
		return;

	/* Paranoia */
	if (event < 0 || event >= SOUND_MAX_2010) return;

	if (samples[event].disabled) {
		weather_current = event;
		weather_current_vol = -1;
		return;
	}

	/* Check there are samples for this event */
	if (!samples[event].num) {
		/* stop previous weather sound */
#if 0 /* stop apruptly */
		if (weather_channel != -1) Mix_HaltChannel(weather_channel);
#else /* fade out */
		if (weather_channel != -1 &&
		    Mix_FadingChannel(weather_channel) != MIX_FADING_OUT) {
			if (!weather_channel_volume) Mix_HaltChannel(weather_channel); else //hack: workaround SDL bug that doesn't terminate a sample playing at 0 volume after a FadeOut
			Mix_FadeOutChannel(weather_channel, 2000);
		}
#endif
		return;
	}

	/* Choose a random event */
	s = rand_int(samples[event].num);
	wave = samples[event].wavs[s];

#ifdef USER_VOLUME_SFX
	/* Apply user-defined custom volume modifier */
	if (samples[event].volume) vols = samples[event].volume;
#endif

	/* Try loading it, if it's not cached */
	if (!wave) {
		if (on_demand_loading || no_cache_audio) {
			if (!(wave = load_sample(event, s))) {
				/* we really failed to load it */
				plog(format("SDL sound load failed (%d, %d).", event, s));
				return;
			}
		} else {
			/* Fail silently */
			return;
		}
	}

	/* Actually play the thing */
#if 1 /* volume glitch paranoia (first fade-in seems to move volume to 100% instead of designated cfg_audio_... */
	new_wc = Mix_PlayChannel(weather_channel, wave, -1);
	if (new_wc != -1) {
		Mix_Volume(new_wc, (CALC_MIX_VOLUME(cfg_audio_weather, cfg_audio_weather_volume, vols) * grid_weather_volume) / 100);

		/* weird bug (see above) apparently STILL occurs for some people (Dj_wolf) - hack it moar: */
		if (cfg_audio_weather)

		if (!weather_resume) new_wc = Mix_FadeInChannel(new_wc, wave, -1, 500);
	}
#else
	if (!weather_resume) new_wc = Mix_FadeInChannel(weather_channel, wave, -1, 500);
#endif

	if (!weather_resume) weather_fading = 1;
	else weather_resume = FALSE;

#ifdef DEBUG_SOUND
	puts(format("old: %d, new: %d, ev: %d", weather_channel, new_wc, event));
#endif

#if 1
	/* added this <if> after weather seemed to glitch sometimes,
	   with its channel becoming unmutable */
	//we didn't play weather so far?
	if (weather_channel == -1) {
		//failed to start playing?
		if (new_wc == -1) return;

		//successfully started playing the first weather
		weather_channel = new_wc;
		weather_current = event;
		weather_current_vol = -1;
		Mix_Volume(weather_channel, (CALC_MIX_VOLUME(cfg_audio_weather, cfg_audio_weather_volume, vols) * grid_weather_volume) / 100);
	} else {
		//failed to start playing?
		if (new_wc == -1) {
			Mix_HaltChannel(weather_channel);
			return;
		//successfully started playing a follow-up weather
		} else {
			//same channel?
			if (new_wc == weather_channel) {
				weather_current = event;
				weather_current_vol = -1;
			//different channel?
			} else {
				Mix_HaltChannel(weather_channel);
				weather_channel = new_wc;
				weather_current = event;
				weather_current_vol = -1;
				Mix_Volume(weather_channel, (CALC_MIX_VOLUME(cfg_audio_weather, cfg_audio_weather_volume, vols) * grid_weather_volume) / 100);
			}
		}
	}
#endif
#if 0
	/* added this <if> after weather seemed to glitch sometimes,
	   with its channel becoming unmutable */
	if (new_wc != weather_channel) {
		if (weather_channel != -1) Mix_HaltChannel(weather_channel);
		weather_channel = new_wc;
	}
	if (weather_channel != -1) { //paranoia? should always be != -1 at this point
		weather_current = event;
		weather_current_vol = -1;
		Mix_Volume(weather_channel, (CALC_MIX_VOLUME(cfg_audio_weather, cfg_audio_weather_volume, vols) * grid_weather_volume) / 100);
	}
#endif

#ifdef DEBUG_SOUND
	puts(format("now: %d, oev: %d, ev: %d", weather_channel, event, weather_current));
#endif
}


/* Overlay a weather noise, with a certain volume -- WEATHER_VOL_PARTICLES */
static void play_sound_weather_vol(int event, int vol) {
	Mix_Chunk *wave = NULL;
	int s, new_wc, vols = 100;

	/* allow halting a muted yet playing sound, before checking for DISABLE_MUTED_AUDIO */
	if (event == -2 && weather_channel != -1) {
#ifdef DEBUG_SOUND
		puts(format("w-2: wco %d, ev %d", weather_channel, event));
#endif
		Mix_HaltChannel(weather_channel);
		weather_current = -1;
		return;
	}

	if (event == -1 && weather_channel != -1) {
#ifdef DEBUG_SOUND
		puts(format("w-1: wco %d, ev %d", weather_channel, event));
#endif

		/* if channel is muted anyway, no need to fade out, just halt it instead.
		   HACK: The reason for this is actually to fix this bug:
		   There was a bug where ambient channel wasn't faded out if it was actually
		   muted at the same time, and so it'd continue being active when unmuted
		   again, instead of having been terminated by the fade-out.  */
		if (!cfg_audio_master || !cfg_audio_weather || !cfg_audio_weather_volume) {
			Mix_HaltChannel(weather_channel);
			return;
		}

		if (Mix_FadingChannel(weather_channel) != MIX_FADING_OUT) {
			if (!weather_channel_volume) Mix_HaltChannel(weather_channel); else //hack: workaround SDL bug that doesn't terminate a sample playing at 0 volume after a FadeOut
			Mix_FadeOutChannel(weather_channel, 2000);
		}
		weather_current = -1;
		return;
	}

#ifdef DISABLE_MUTED_AUDIO
	if (!cfg_audio_master || !cfg_audio_weather) return;
#endif

	/* we're already in this weather? */
	if (weather_channel != -1 && weather_current == event &&
	    Mix_FadingChannel(weather_channel) != MIX_FADING_OUT) {
		int v = (cfg_audio_weather_volume * vol) / 100, n, va = 0;

		/* shift array and calculate average over the last n volume values */
		for (n = 20 - 1; n > 0; n--) {
			va += weather_smooth_avg[n];
			/* and shift array to make room for new value */
			weather_smooth_avg[n] = weather_smooth_avg[n - 1];
		}
		va += weather_smooth_avg[0];//add the last element
		va /= 20;//calculate average
		/* queue new value */
		weather_smooth_avg[0] = v;

		/* change volume though? */
//c_message_add(format("vol %d, v %d", vol, v));
		if (weather_vol_smooth < va - 20) {
			weather_vol_smooth_anti_oscill = va;
//c_message_add(format("smooth++ %d (vol %d)", weather_vol_smooth, vol));
		}
		else if (weather_vol_smooth > va + 20) {
			weather_vol_smooth_anti_oscill = va;
//c_message_add(format("smooth-- %d (vol %d)", weather_vol_smooth, vol));
		}

		if (weather_vol_smooth < weather_vol_smooth_anti_oscill)
			weather_vol_smooth++;
		else if (weather_vol_smooth > weather_vol_smooth_anti_oscill)
				weather_vol_smooth--;

#ifdef USER_VOLUME_SFX
		/* Apply user-defined custom volume modifier */
		if (samples[event].volume) vols = samples[event].volume;
#endif

//c_message_add(format("smooth %d", weather_vol_smooth));
		Mix_Volume(weather_channel, (CALC_MIX_VOLUME(cfg_audio_weather, weather_vol_smooth, vols) * grid_weather_volume) / 100);

		/* Done */
		return;
	}

	/* Paranoia */
	if (event < 0 || event >= SOUND_MAX_2010) return;

	if (samples[event].disabled) {
		weather_current = event; //pretend we play it
		weather_current_vol = vol;
		return;
	}

	/* Check there are samples for this event */
	if (!samples[event].num) return;

#ifdef USER_VOLUME_SFX
	/* Apply user-defined custom volume modifier */
	if (samples[event].volume) vols = samples[event].volume;
#endif

	/* Choose a random event */
	s = rand_int(samples[event].num);
	wave = samples[event].wavs[s];

	/* Try loading it, if it's not cached */
	if (!wave) {
		if (on_demand_loading || no_cache_audio) {
			if (!(wave = load_sample(event, s))) {
				/* we really failed to load it */
				plog(format("SDL sound load failed (%d, %d).", event, s));
				return;
			}
		} else {
			/* Fail silently */
			return;
		}
	}

	/* Actually play the thing */
#if 1 /* volume glitch paranoia (first fade-in seems to move volume to 100% instead of designated cfg_audio_... */
	new_wc = Mix_PlayChannel(weather_channel, wave, -1);
	if (new_wc != -1) {
		weather_vol_smooth = (cfg_audio_weather_volume * vol) / 100; /* set initially, instantly */
		Mix_Volume(new_wc, (CALC_MIX_VOLUME(cfg_audio_weather, weather_vol_smooth, vols) * grid_weather_volume) / 100);

		/* weird bug (see above) apparently might STILL occur for some people - hack it moar: */
		if (cfg_audio_weather)

		if (!weather_resume) new_wc = Mix_FadeInChannel(new_wc, wave, -1, 500);
	}
#else
	if (!weather_resume) new_wc = Mix_FadeInChannel(weather_channel, wave, -1, 500);
#endif

	if (!weather_resume) weather_fading = 1;
	else weather_resume = FALSE;

#ifdef DEBUG_SOUND
	puts(format("old: %d, new: %d, ev: %d", weather_channel, new_wc, event));
#endif

#if 1
	/* added this <if> after weather seemed to glitch sometimes,
	   with its channel becoming unmutable */
	//we didn't play weather so far?
	if (weather_channel == -1) {
		//failed to start playing?
		if (new_wc == -1) return;

		//successfully started playing the first weather
		weather_channel = new_wc;
		weather_current = event;
		weather_current_vol = vol;

		weather_vol_smooth = (cfg_audio_weather_volume * vol) / 100; /* set initially, instantly */
		Mix_Volume(weather_channel, (CALC_MIX_VOLUME(cfg_audio_weather, weather_vol_smooth, vols) * grid_weather_volume) / 100);
	} else {
		//failed to start playing?
		if (new_wc == -1) {
			Mix_HaltChannel(weather_channel);
			return;
		//successfully started playing a follow-up weather
		} else {
			//same channel?
			if (new_wc == weather_channel) {
				weather_current = event;
				weather_current_vol = vol;
			//different channel?
			} else {
				Mix_HaltChannel(weather_channel);
				weather_channel = new_wc;
				weather_current = event;
				weather_current_vol = vol;
				weather_vol_smooth = (cfg_audio_weather_volume * vol) / 100; /* set initially, instantly */
				Mix_Volume(weather_channel, (CALC_MIX_VOLUME(cfg_audio_weather, weather_vol_smooth, vols) * grid_weather_volume) / 100);
			}
		}
	}
#endif
#if 0
	/* added this <if> after weather seemed to glitch sometimes,
	   with its channel becoming unmutable */
	if (new_wc != weather_channel) {
		if (weather_channel != -1) Mix_HaltChannel(weather_channel);
		weather_channel = new_wc;
	}
	if (weather_channel != -1) { //paranoia? should always be != -1 at this point
		weather_current = event;
		weather_current_vol = vol;
		Mix_Volume(weather_channel, (CALC_MIX_VOLUME(cfg_audio_weather, (cfg_audio_weather_volume * vol) / 100, vols) * grid_weather_volume) / 100);
	}
#endif

#ifdef DEBUG_SOUND
	puts(format("now: %d, oev: %d, ev: %d", weather_channel, event, weather_current));
#endif
}

/* make sure volume is set correct after fading-in has been completed (might be just paranoia) */
void weather_handle_fading(void) {
	int vols = 100;

	if (weather_channel == -1) { //paranoia
		weather_fading = 0;
		return;
	}

#ifdef USER_VOLUME_SFX
	/* Apply user-defined custom volume modifier */
	if (weather_current != -1 && samples[weather_current].volume) vols = samples[weather_current].volume;
#endif

	if (Mix_FadingChannel(weather_channel) == MIX_NO_FADING) {
#ifndef WEATHER_VOL_PARTICLES
		Mix_Volume(weather_channel, (CALC_MIX_VOLUME(cfg_audio_weather, cfg_audio_weather_volume, vols) * grid_weather_volume) / 100);
#else
		Mix_Volume(weather_channel, (CALC_MIX_VOLUME(cfg_audio_weather, weather_vol_smooth, vols) * grid_weather_volume) / 100);
#endif
		weather_fading = 0;
		return;
	}
}

/* Overlay an ambient sound effect */
static void play_sound_ambient(int event) {
	Mix_Chunk *wave = NULL;
	int s, new_ac, vols = 100;

#ifdef DEBUG_SOUND
	puts(format("psa: ch %d, ev %d", ambient_channel, event));
#endif

	/* allow halting a muted yet playing sound, before checking for DISABLE_MUTED_AUDIO */
	if (event == -2 && ambient_channel != -1) {
#ifdef DEBUG_SOUND
		puts(format("w-2: wco %d, ev %d", ambient_channel, event));
#endif
		Mix_HaltChannel(ambient_channel);
		ambient_current = -1;
		return;
	}

	if (event == -1 && ambient_channel != -1) {
#ifdef DEBUG_SOUND
		puts(format("w-1: wco %d, ev %d", ambient_channel, event));
#endif

		/* if channel is muted anyway, no need to fade out, just halt it instead.
		   HACK: The reason for this is actually to fix this bug:
		   There was a bug where ambient channel wasn't faded out if it was actually
		   muted at the same time, and so it'd continue being active when unmuted
		   again, instead of having been terminated by the fade-out.  */
		if (!cfg_audio_master || !cfg_audio_sound || !cfg_audio_sound_volume) {
			Mix_HaltChannel(ambient_channel);
			return;
		}

		if (Mix_FadingChannel(ambient_channel) != MIX_FADING_OUT) {
			if (!ambient_channel_volume) Mix_HaltChannel(ambient_channel); else //hack: workaround SDL bug that doesn't terminate a sample playing at 0 volume after a FadeOut
			Mix_FadeOutChannel(ambient_channel, 2000);
		}
		ambient_current = -1;
		return;
	}

#ifdef DISABLE_MUTED_AUDIO
	if (!cfg_audio_master || !cfg_audio_sound) return;
#endif

	/* we're already in this ambient? */
	if (ambient_channel != -1 && ambient_current == event &&
	    Mix_FadingChannel(ambient_channel) != MIX_FADING_OUT)
		return;

	/* Paranoia */
	if (event < 0 || event >= SOUND_MAX_2010) return;

	if (samples[event].disabled) {
		ambient_current = event; //pretend we play it
		return;
	}

	/* Check there are samples for this event */
	if (!samples[event].num) {
		/* stop previous ambient sound */
#if 0 /* stop apruptly */
		if (ambient_channel != -1) Mix_HaltChannel(ambient_channel);
#else /* fade out */
		if (ambient_channel != -1 &&
		    Mix_FadingChannel(ambient_channel) != MIX_FADING_OUT) {
			if (!ambient_channel_volume) Mix_HaltChannel(ambient_channel); else //hack: workaround SDL bug that doesn't terminate a sample playing at 0 volume after a FadeOut
			Mix_FadeOutChannel(ambient_channel, 2000);
		}
#endif
		return;
	}

	/* Choose a random event */
	s = rand_int(samples[event].num);
	wave = samples[event].wavs[s];

#ifdef USER_VOLUME_SFX
	/* Apply user-defined custom volume modifier */
	if (samples[event].volume) vols = samples[event].volume;
#endif

	/* Try loading it, if it's not cached */
	if (!wave) {
#if 0 /* for ambient sounds. we don't drop them as we'd do for "normal " sfx, \
	 because they ought to be looped, so we'd completely lose them. - C. Blue */
		if (on_demand_loading || no_cache_audio) {
#endif
			if (!(wave = load_sample(event, s))) {
				/* we really failed to load it */
				plog(format("SDL sound load failed (%d, %d).", event, s));
				return;
			}
#if 0 /* see above */
		} else {
#ifdef DEBUG_SOUND
			puts(format("on_demand_loading %d, no_cache_audio %d", on_demand_loading, no_cache_audio));
#endif
			/* Fail silently */
			return;
		}
#endif
	}

	/* Actually play the thing */
#if 1 /* volume glitch paranoia (first fade-in seems to move volume to 100% instead of designated cfg_audio_... */
	new_ac = Mix_PlayChannel(ambient_channel, wave, -1);
	if (new_ac != -1) {
		Mix_Volume(new_ac, (CALC_MIX_VOLUME(cfg_audio_sound, cfg_audio_sound_volume, vols) * grid_ambient_volume) / 100);

		/* weird bug (see above) apparently might STILL occur for some people - hack it moar: */
		if (cfg_audio_sound)

		if (!ambient_resume) new_ac = Mix_FadeInChannel(new_ac, wave, -1, 500);
	}
#else
	if (!ambient_resume) new_ac = Mix_FadeInChannel(ambient_channel, wave, -1, 500);
#endif

	if (!ambient_resume) ambient_fading = 1;
	else ambient_resume = FALSE;

#ifdef DEBUG_SOUND
	puts(format("old: %d, new: %d, ev: %d", ambient_channel, new_ac, event));
#endif

#if 1
	/* added this <if> after ambient seemed to glitch sometimes,
	   with its channel becoming unmutable */
	//we didn't play ambient so far?
	if (ambient_channel == -1) {
		//failed to start playing?
		if (new_ac == -1) return;

		//successfully started playing the first ambient
		ambient_channel = new_ac;
		ambient_current = event;
		Mix_Volume(ambient_channel, (CALC_MIX_VOLUME(cfg_audio_sound, cfg_audio_sound_volume, vols) * grid_ambient_volume) / 100);
	} else {
		//failed to start playing?
		if (new_ac == -1) {
			Mix_HaltChannel(ambient_channel);
			return;
		//successfully started playing a follow-up ambient
		} else {
			//same channel?
			if (new_ac == ambient_channel) {
				ambient_current = event;
			//different channel?
			} else {
				Mix_HaltChannel(ambient_channel);
				ambient_channel = new_ac;
				ambient_current = event;
				Mix_Volume(ambient_channel, (CALC_MIX_VOLUME(cfg_audio_sound, cfg_audio_sound_volume, vols) * grid_ambient_volume) / 100);
			}
		}

	}
#endif
#if 0
	/* added this <if> after ambient seemed to glitch sometimes,
	   with its channel becoming unmutable */
	if (new_ac != ambient_channel) {
		if (ambient_channel != -1) Mix_HaltChannel(ambient_channel);
		ambient_channel = new_ac;
	}
	if (ambient_channel != -1) { //paranoia? should always be != -1 at this point
		ambient_current = event;
		Mix_Volume(ambient_channel, (CALC_MIX_VOLUME(cfg_audio_sound, cfg_audio_sound_volume, vols) * grid_ambient_volume) / 100);
	}
#endif

#ifdef DEBUG_SOUND
	puts(format("now: %d, oev: %d, ev: %d", ambient_channel, event, ambient_current));
#endif
}

/* make sure volume is set correct after fading-in has been completed (might be just paranoia) */
void ambient_handle_fading(void) {
	int vols = 100;

	if (ambient_channel == -1) { //paranoia
		ambient_fading = 0;
		return;
	}

#ifdef USER_VOLUME_SFX
	/* Apply user-defined custom volume modifier */
	if (ambient_current != -1 && samples[ambient_current].volume) vols = samples[ambient_current].volume;
#endif

	if (Mix_FadingChannel(ambient_channel) == MIX_NO_FADING) {
		Mix_Volume(ambient_channel, (CALC_MIX_VOLUME(cfg_audio_sound, cfg_audio_sound_volume, vols) * grid_ambient_volume) / 100);
		ambient_fading = 0;
		return;
	}
}

/*
 * Play a music of type "event".
 */
static bool play_music(int event) {
	int n, initials = 0, vols = 100;

	/* Paranoia */
	if (event < -4 || event >= MUSIC_MAX) return FALSE;

	/* Don't play anything, just return "success", aka just keep playing what is currently playing.
	   This is used for when the server sends music that doesn't have an alternative option, but
	   should not stop the current music if it fails to play. */
	if (event == -1) return TRUE;

#ifdef ENABLE_JUKEBOX
	/* Jukebox hack: Don't interrupt current jukebox song, but remember event for later */
	if (jukebox_playing != -1) {
		jukebox_org = event;
		return TRUE;
	}
#endif

	/* We previously failed to play both music and alternative music.
	   Stop currently playing music before returning */
	if (event == -2) {
		if (Mix_PlayingMusic() && Mix_FadingMusic() != MIX_FADING_OUT)
			Mix_FadeOutMusic(500);
		music_cur = -1;
		return TRUE; //whatever..
	}

	/* 'shuffle_music' or 'play_all' option changed? */
	if (event == -3) {
		if (c_cfg.shuffle_music) {
			music_next = -1;
			Mix_FadeOutMusic(500);
		} else if (c_cfg.play_all) {
			music_next = -1;
			Mix_FadeOutMusic(500);
		} else {
			music_next = music_cur; //hack
			music_next_song = music_cur_song;
		}
		return TRUE; //whatever..
	}

#ifdef ATMOSPHERIC_INTRO
	/* New, for title screen -> character screen switch: Halt current music */
	if (event == -4) {
		/* Stop currently playing music though, before returning */
		if (Mix_PlayingMusic() && Mix_FadingMusic() != MIX_FADING_OUT)
			Mix_FadeOutMusic(2000);
		music_cur = -1;
		return TRUE; /* claim that it 'succeeded' */
	}
#endif

	/* Check there are samples for this event */
	if (!songs[event].num) return FALSE;

	/* Special hack for ghost music (4.7.4b+), see handle_music() in util.c */
	if (event == 89 && is_atleast(&server_version, 4, 7, 4, 2, 0, 0)) skip_received_music = TRUE;

#ifdef USER_VOLUME_MUS
	/* Apply user-defined custom volume modifier */
	if (songs[event].volume) vols = songs[event].volume;
#endif

	/* if music event is the same as is already running, don't do anything */
	if (music_cur == event && Mix_PlayingMusic() && Mix_FadingMusic() != MIX_FADING_OUT) {
		/* Just change volume if requested, ie if play_music() is called after a play_music_vol() call: We revert back to 100% volume then. */
		if (music_vol != 100) {
			music_vol = 100;
			Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, vols));
		}
		return TRUE; //pretend we played it
	}

	music_next = event;
	if (music_vol != 100) {
		music_vol = 100;
		Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, vols));
	}
	/* handle 'initial' songs with priority */
	for (n = 0; n < songs[music_next].num; n++) if (songs[music_next].initial[n]) initials++;
	/* no initial songs - just pick a song normally */
	if (!initials) {
		if (!c_cfg.first_song)
			/* Pick one song randomly */
			music_next_song = rand_int(songs[music_next].num);
		else
			/* Start with the first song of the music.cfg entry */
			music_next_song = 0;
	}
	/* pick an initial song first */
	else {
		if (!c_cfg.first_song) {
			/* Pick an initial song randomly */
			initials = randint(initials);
			for (n = 0; n < songs[music_next].num; n++) {
				if (!songs[music_next].initial[n]) continue;
				initials--;
				if (initials) continue;
				music_next_song = n;
				break;
			}
		} else
			/* Start with the first initial song of the music.cfg entry */
			for (n = 0; n < songs[music_next].num; n++) {
				if (!songs[music_next].initial[n]) continue;
				music_next_song = 0;
				break;
			}
	}

	/* new: if upcoming music file is same as currently playing one, don't do anything */
	if (music_cur != -1 && music_cur_song != -1 &&
	    songs[music_next].num /* Check there are samples for this event */
	    && !strcmp(
	     songs[music_next].paths[music_next_song], /* Choose a random event and pretend it's the one that would've gotten picked */
	     songs[music_cur].paths[music_cur_song]
	     )) {
		music_next = event;
		music_next_song = music_cur_song;
		return TRUE;
	}

	/* check if music is already running, if so, fade it out first! */
	if (Mix_PlayingMusic()) {
		if (Mix_FadingMusic() != MIX_FADING_OUT)
			Mix_FadeOutMusic(500);
	} else {
		//play immediately
		fadein_next_music();
	}
	return TRUE;
}
static bool play_music_vol(int event, char vol) {
	int n, initials = 0, vols = 100;

	/* Paranoia */
	if (event < -4 || event >= MUSIC_MAX) return FALSE;

	/* Don't play anything, just return "success", aka just keep playing what is currently playing.
	   This is used for when the server sends music that doesn't have an alternative option, but
	   should not stop the current music if it fails to play. */
	if (event == -1) return TRUE;

#ifdef ENABLE_JUKEBOX
	/* Jukebox hack: Don't interrupt current jukebox song, but remember event for later */
	if (jukebox_playing != -1) {
		jukebox_org = event;
		return TRUE;
	}
#endif

	/* We previously failed to play both music and alternative music.
	   Stop currently playing music before returning */
	if (event == -2) {
		if (Mix_PlayingMusic() && Mix_FadingMusic() != MIX_FADING_OUT)
			Mix_FadeOutMusic(500);
		music_cur = -1;
		return TRUE; //whatever..
	}

	/* 'shuffle_music' or 'play_all' option changed? */
	if (event == -3) {
		if (c_cfg.shuffle_music) {
			music_next = -1;
			Mix_FadeOutMusic(500);
		} else if (c_cfg.play_all) {
			music_next = -1;
			Mix_FadeOutMusic(500);
		} else {
			music_next = music_cur; //hack
			music_next_song = music_cur_song;
		}
		return TRUE; //whatever..
	}

#ifdef ATMOSPHERIC_INTRO
	/* New, for title screen -> character screen switch: Halt current music */
	if (event == -4) {
		/* Stop currently playing music though, before returning */
		if (Mix_PlayingMusic() && Mix_FadingMusic() != MIX_FADING_OUT)
			Mix_FadeOutMusic(2000);
		music_cur = -1;
		return TRUE; /* claim that it 'succeeded' */
	}
#endif

	/* Check there are samples for this event */
	if (!songs[event].num) return FALSE;

	/* Special hack for ghost music (4.7.4b+), see handle_music() in util.c */
	if (event == 89 && is_atleast(&server_version, 4, 7, 4, 2, 0, 0)) skip_received_music = TRUE;

#ifdef USER_VOLUME_MUS
	/* Apply user-defined custom volume modifier */
	if (songs[event].volume) vols = songs[event].volume;
#endif

	/* if music event is the same as is already running, don't do anything */
	if (music_cur == event && Mix_PlayingMusic() && Mix_FadingMusic() != MIX_FADING_OUT) {
		/* Just change volume if requested */
		if (music_vol != vol) Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, (cfg_audio_music_volume * evlt[(int)vol]) / MIX_MAX_VOLUME, vols));
		music_vol = vol;
		return TRUE; //pretend we played it
	}

	music_next = event;
	music_vol = vol;
	Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, (cfg_audio_music_volume * evlt[(int)vol]) / MIX_MAX_VOLUME, vols));
	/* handle 'initial' songs with priority */
	for (n = 0; n < songs[music_next].num; n++) if (songs[music_next].initial[n]) initials++;
	/* no initial songs - just pick a song normally */
	if (!initials) {
		if (!c_cfg.first_song)
			/* Pick one song randomly */
			music_next_song = rand_int(songs[music_next].num);
		else
			/* Start with the first song of the music.cfg entry */
			music_next_song = 0;
	}
	/* pick an initial song first */
	else {
		if (!c_cfg.first_song) {
			/* Pick an initial song randomly */
			initials = randint(initials);
			for (n = 0; n < songs[music_next].num; n++) {
				if (!songs[music_next].initial[n]) continue;
				initials--;
				if (initials) continue;
				music_next_song = n;
				break;
			}
		} else
			/* Start with the first initial song of the music.cfg entry */
			for (n = 0; n < songs[music_next].num; n++) {
				if (!songs[music_next].initial[n]) continue;
				music_next_song = 0;
				break;
			}
	}

	/* new: if upcoming music file is same as currently playing one, don't do anything */
	if (music_cur != -1 && music_cur_song != -1 &&
	    songs[music_next].num /* Check there are samples for this event */
	    && !strcmp(
	     songs[music_next].paths[music_next_song], /* Choose a random event and pretend it's the one that would've gotten picked */
	     songs[music_cur].paths[music_cur_song]
	     )) {
		music_next = event;
		music_next_song = music_cur_song;
		return TRUE;
	}

	/* check if music is already running, if so, fade it out first! */
	if (Mix_PlayingMusic()) {
		if (Mix_FadingMusic() != MIX_FADING_OUT)
			Mix_FadeOutMusic(500);
	} else {
		//play immediately
		fadein_next_music();
	}
	return TRUE;
}

static void fadein_next_music(void) {
	Mix_Music *wave = NULL;

#ifdef DISABLE_MUTED_AUDIO
	if (!cfg_audio_master || !cfg_audio_music) return;
#endif

	/* Catch music_next == -1, this can now happen with shuffle_music or play_all option, since songs are no longer looped if it's enabled */
	if ((c_cfg.shuffle_music || c_cfg.play_all) && music_next == -1) {
		int tries, mcs;
		int n, initials = 0, noinit_map[MAX_SONGS], ni = 0;

		/* We're not currently playing any music? -- This can happen when we quickly exit the game and return to music-less account screen
		   and would cause a segfault when trying to access songs[-1]: */
		if (music_cur == -1) return;

		/* Catch disabled songs */
		if (songs[music_cur].disabled) return;

		if (songs[music_cur].num < 1) return; //paranoia

		/* don't sequence-shuffle 'initial' songs */
		for (n = 0; n < songs[music_cur].num; n++) {
			if (songs[music_cur].initial[n]) {
				initials++;
				continue;
			}
			noinit_map[ni] = n;
			ni++;
		}
		/* no non-initial songs found? Don't play any subsequent music then. */
		if (!ni) return;

		if (c_cfg.shuffle_music) {
			/* stick with music event, but play different song, randomly */
			tries = songs[music_cur].num == 1 ? 1 : 100;
			mcs = music_cur_song;
			while (tries--) {
				mcs = noinit_map[rand_int(ni)];
				if (music_cur_song != mcs) break; //find some other song than then one we've just played, or it's not really 'shuffle'..
			}
		} else {
			/* c_cfg.play_all: */
			/* stick with music event, but play different song, in listed order */
			tries = -1;
			mcs = music_cur_song;
			while (++tries < ni) {
				mcs = noinit_map[tries];
				if (mcs <= music_cur_song) continue;
				break;
			}
			/* We already went through all (non-initial) songs? Start anew then: */
			if (tries == ni) mcs = noinit_map[0];
		}
		music_cur_song = mcs;

		/* Choose the predetermined random event */
		wave = songs[music_cur].wavs[music_cur_song];

		/* Try loading it, if it's not cached */
		if (!wave && !(wave = load_song(music_cur, music_cur_song))) {
			/* we really failed to load it */
			plog(format("SDL music load failed (%d, %d).", music_cur, music_cur_song));
			puts(format("SDL music load failed (%d, %d).", music_cur, music_cur_song));
			return;
		}

		/* Actually play the thing */
		//Mix_PlayMusic(wave, 0);//-1 infinite, 0 once, or n times
		Mix_FadeInMusic(wave, 0, 1000);
		return;
	}

	/* Paranoia */
	if (music_next < 0 || music_next >= MUSIC_MAX) return;

	/* Song file was disabled? (local audio options) */
	if (songs[music_next].disabled) {
		music_cur = music_next;
		music_cur_song = music_next_song;
		music_next = -1; //pretend we play it
		return;
	}

	/* Check there are samples for this event */
	if (songs[music_next].num < music_next_song + 1) return;

	/* Choose the predetermined random event */
	wave = songs[music_next].wavs[music_next_song];

	/* Try loading it, if it's not cached */
	if (!wave && !(wave = load_song(music_next, music_next_song))) {
		/* we really failed to load it */
		plog(format("SDL music load failed (%d, %d).", music_next, music_next_song));
		puts(format("SDL music load failed (%d, %d).", music_next, music_next_song));
		return;
	}

#ifdef USER_VOLUME_MUS
	/* Apply user-defined custom volume modifier */
	if (songs[music_next].volume) Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, songs[music_next].volume));
#endif

	/* Actually play the thing */
//#ifdef DISABLE_MUTED_AUDIO /* now these vars are also used for 'continous' music across music events */
	music_cur = music_next;
	music_cur_song = music_next_song;
//#endif
	music_next = -1;
	/* Actually don't repeat 'initial' songs */
	if (!songs[music_cur].initial[music_cur_song]) {
		//Mix_PlayMusic(wave, c_cfg.shuffle_music || c_cfg.play_all ? 0 : -1);//-1 infinite, 0 once, or n times
		Mix_FadeInMusic(wave, c_cfg.shuffle_music || c_cfg.play_all ? 0 : -1, 1000);
	} else Mix_FadeInMusic(wave, c_cfg.shuffle_music || c_cfg.play_all ? 0 : 0, 1000);
}

#ifdef JUKEBOX_INSTANT_PLAY
static bool play_music_instantly(int event) {
	Mix_Music *wave = NULL;

	Mix_HaltMusic();

	/* We just wanted to top currently playing music, do nothing more and just return. */
	if (event == -2) {
		music_cur = -1;
		return TRUE; //whatever..
	}

	/* Catch disabled songs */
	if (songs[event].disabled) {
		music_cur = -1;
		return TRUE;
	}
	if (songs[event].num < 1) return FALSE; //paranoia

	/* But play different song, iteratingly instead of randomly:
	   We ignore shuffle_music, play_all or 'initial' song type and just go through all songs
	   one by one in their order listed in music.cfg. */
	if (music_cur != event) {
		music_cur = event;
		music_cur_song = 0;
	} else music_cur_song = (music_cur_song + 1) % songs[music_cur].num;

	/* Choose the predetermined random event */
	wave = songs[music_cur].wavs[music_cur_song];

	/* Try loading it, if it's not cached */
	if (!wave && !(wave = load_song(music_cur, music_cur_song))) {
		/* we really failed to load it */
		plog(format("SDL music load failed (%d, %d).", music_cur, music_cur_song));
		puts(format("SDL music load failed (%d, %d).", music_cur, music_cur_song));
		return FALSE;
	}

#ifdef USER_VOLUME_MUS
	/* Apply user-defined custom volume modifier */
	if (songs[event].volume) Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, songs[event].volume));
	else Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, 100));
#endif

	/* Actually play the thing. We loop this specific sub-song infinitely and ignore c_cfg.shuffle_music and c_cfg.play_all (and 'initial' song status) here.
	   To get to hear other sub-songs, the user can press ENTER again to restart this music event with a different sub-song. */
	Mix_PlayMusic(wave, -1);
	return TRUE;
}
#endif


#ifdef DISABLE_MUTED_AUDIO
/* start playing current music again if we reenabled it in the mixer UI after having had it disabled */
static void reenable_music(void) {
	Mix_Music *wave = NULL;

	/* music has changed meanwhile, just play the new one the normal way */
	if (music_next != -1 && music_next != music_cur) {
		fadein_next_music();
		return;
	}

	/* music initialization not yet completed? (at the start of the game) */
	if (music_cur == -1 || music_cur_song == -1) return;

	wave = songs[music_cur].wavs[music_cur_song];

	/* If audio is still being loaded/cached, we might just have to exit here for now */
	if (!wave) return;

#ifdef USER_VOLUME_MUS
	/* Apply user-defined custom volume modifier */
	if (songs[music_cur].volume) Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, songs[music_cur].volume));
#endif

	/* Take up playing again immediately, no fading in */
	Mix_PlayMusic(wave, c_cfg.shuffle_music || c_cfg.play_all ? 0 : -1);
}
#endif

/*
 * Set mixing levels in the SDL module.
 */
static void set_mixing_sdl(void) {
	int vols = 100;

#if 0 /* don't use relative sound-effect specific volumes, transmitted from the server? */
	Mix_Volume(-1, CALC_MIX_VOLUME(cfg_audio_sound, cfg_audio_sound_volume, 100));
#else /* use relative volumes (4.4.5b+) */
	int n;

	/* SFX channels */
	for (n = 0; n < cfg_max_channels; n++) {
		if (n == weather_channel) continue;
		if (n == ambient_channel) continue;

		/* HACK - use weather volume for thunder sfx */
		if (channel_sample[n] != -1 && channel_type[n] == SFX_TYPE_WEATHER)
			Mix_Volume(n, (CALC_MIX_VOLUME(cfg_audio_weather, (cfg_audio_weather_volume * channel_volume[n]) / 100, 100) * grid_weather_volume) / 100);
		else
		/* grid_ambient_volume influences non-looped ambient sfx clips */
		if (channel_sample[n] != -1 && channel_type[n] == SFX_TYPE_AMBIENT)
			Mix_Volume(n, (CALC_MIX_VOLUME(cfg_audio_sound, (cfg_audio_sound_volume * channel_volume[n]) / 100, 100) * grid_ambient_volume) / 100);
		else
		/* Note: Linear scaling is used here to allow more precise control at the server end */
			Mix_Volume(n, CALC_MIX_VOLUME(cfg_audio_sound, (cfg_audio_sound_volume * channel_volume[n]) / 100, 100));

 #ifdef DISABLE_MUTED_AUDIO
		if ((!cfg_audio_master || !cfg_audio_sound) && Mix_Playing(n))
			Mix_HaltChannel(n);
 #endif
	}
#endif

	/* Music channel */
#ifdef USER_VOLUME_MUS
	/* Apply user-defined custom volume modifier */
	if (music_cur != -1 && songs[music_cur].volume) vols = songs[music_cur].volume;
#endif
	//Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume));
	Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, (cfg_audio_music_volume * music_vol) / 100, vols));
#ifdef DISABLE_MUTED_AUDIO
	if (!cfg_audio_master || !cfg_audio_music) {
		if (Mix_PlayingMusic()) Mix_HaltMusic();
	} else if (!Mix_PlayingMusic()) reenable_music();
#endif

	/* SFX channel (weather) */
	if (weather_channel != -1 && Mix_FadingChannel(weather_channel) != MIX_FADING_OUT) {
#ifndef WEATHER_VOL_PARTICLES
		weather_channel_volume = (CALC_MIX_VOLUME(cfg_audio_weather, cfg_audio_weather_volume, 100) * grid_weather_volume) / 100;
		Mix_Volume(weather_channel, weather_channel_volume);
#else
		Mix_Volume(weather_channel, weather_channel_volume);
#endif
	}

	/* SFX channel (ambient) */
	if (ambient_channel != -1 && Mix_FadingChannel(ambient_channel) != MIX_FADING_OUT) {
		ambient_channel_volume = (CALC_MIX_VOLUME(cfg_audio_sound, cfg_audio_sound_volume, 100) * grid_ambient_volume) / 100;
		Mix_Volume(ambient_channel, ambient_channel_volume);
	}

#ifdef DISABLE_MUTED_AUDIO
	/* Halt weather/ambient SFX channel immediately */
	if (!cfg_audio_master || !cfg_audio_weather) {
		weather_resume = TRUE;
		if (weather_channel != -1 && Mix_Playing(weather_channel))
			Mix_HaltChannel(weather_channel);

		/* HACK - use weather volume for thunder sfx */
		for (n = 0; n < cfg_max_channels; n++)
			if (channel_type[n] == SFX_TYPE_WEATHER)
				Mix_HaltChannel(n);
	}

	if (!cfg_audio_master || !cfg_audio_sound) {
		ambient_resume = TRUE;
		if (ambient_channel != -1 && Mix_Playing(ambient_channel))
			Mix_HaltChannel(ambient_channel);
	}
#endif
}

/*
 * Init the SDL sound "module".
 */
errr init_sound_sdl(int argc, char **argv) {
	int i;

	/* Parse args */
	for (i = 1; i < argc; i++) {
		if (prefix(argv[i], "-a")) {
			no_cache_audio = TRUE;
			plog("Audio cache disabled.");
			continue;
		}
	}

#ifdef DEBUG_SOUND
	puts(format("init_sound_sdl() init%s", no_cache_audio == FALSE ? " (cached)" : " (not cached)"));
#endif

	/* Load sound preferences if requested */
#if 0
	if (!sound_sdl_init(no_cache_audio)) {
#else /* never cache audio right at program start, because it creates an annoying delay! */
	if (!sound_sdl_init(TRUE)) {
#endif
		plog("Failed to initialise audio.");

		/* Failure */
		return(1);
	}

	/* Set the mixing hook */
	mixing_hook = set_mixing_sdl;

	/* Enable sound */
	sound_hook = play_sound;

	/* Enable music */
	music_hook = play_music;
	music_hook_vol = play_music_vol;

	/* Enable weather noise overlay */
	sound_weather_hook = play_sound_weather;
	sound_weather_hook_vol = play_sound_weather_vol;

	/* Enable ambient sfx overlay */
	sound_ambient_hook = play_sound_ambient;

	/* clean-up hook */
	atexit(close_audio);

	/* start caching audio in a separate thread to eliminate startup loading time */
	if (!no_cache_audio) {
#ifdef DEBUG_SOUND
		puts("Audio cache: Creating thread..");
#endif
		load_audio_thread = SDL_CreateThread(thread_load_audio, NULL, NULL);
		if (load_audio_thread == NULL) {
#ifdef DEBUG_SOUND
			puts("Audio cache: Thread creation failed.");
#endif
			plog(format("Audio cache: Unable to create thread: %s\n", SDL_GetError()));

			/* load manually instead, with annoying delay ;-p */
			thread_load_audio(NULL);
		}
#ifdef DEBUG_SOUND
		else puts("Audio cache: Thread creation succeeded.");
#endif
	}

#ifdef DEBUG_SOUND
	puts("init_sound_sdl() completed.");
#endif

	/* Success */
	return(0);
}
/* Try to allow re-initializing audio.
   Purpose: Switching between audio packs live, without need for client restart. */
errr re_init_sound_sdl(void) {
	int i, j;


	/* --- exit --- */

	/* Close audio first, then try to open it again */
	close_audio();

	/* Reset variables (closing audio doesn't do this since it assumes program exit anyway) */
	for (i = 0; i < SOUND_MAX_2010; i++) {
		samples[i].num = 0;
		samples[i].config = FALSE;
		samples[i].disabled = FALSE;
		for (j = 0; j < MAX_SAMPLES; j++) {
			samples[i].wavs[j] = NULL;
			samples[i].paths[j] = NULL;
		}
	}
	for (i = 0; i < MUSIC_MAX; i++) {
		songs[i].num = 0;
		songs[i].config = FALSE;
		songs[i].disabled = FALSE;
		for (j = 0; j < MAX_SONGS; j++) {
			songs[i].wavs[j] = NULL;
			songs[i].paths[j] = NULL;
		}
	}

	for (i = 0; i < MAX_CHANNELS; i++) {
		channel_sample[i] = -1;
		channel_type[i] = 0;
		channel_volume[i] = 0;
		channel_player_id[i] = 0;
	}

	audio_sfx = 0;
	audio_music = 0;

	/*void (*mixing_hook)(void);
	bool (*sound_hook)(int sound, int type, int vol, s32b player_id, int dist_x, int dist_y);
	void (*sound_ambient_hook)(int sound_ambient);
	void (*sound_weather_hook)(int sound);
	void (*sound_weather_hook_vol)(int sound, int vol);
	bool (*music_hook)(int music);*/

	//cfg_audio_rate = 44100, cfg_max_channels = 32, cfg_audio_buffer = 1024;

	music_cur = -1;
	music_cur_song = -1;
	music_next = -1;
	music_next_song = -1;

	weather_channel = -1;
	weather_current = -1;
	weather_current_vol = -1;
	weather_channel_volume = 100;

	ambient_channel = -1;
	ambient_current = -1;
	ambient_channel_volume = 100;

	//weather_particles_seen, weather_sound_change, weather_fading, ambient_fading;
	//wind_noticable = FALSE;
#if 1 /* WEATHER_VOL_PARTICLES */
	//weather_vol_smooth, weather_vol_smooth_anti_oscill, weather_smooth_avg[20];
#endif

	//cfg_audio_master = TRUE, cfg_audio_music = TRUE, cfg_audio_sound = TRUE, cfg_audio_weather = TRUE, weather_resume = FALSE, ambient_resume = FALSE;
	//cfg_audio_master_volume = cfg_audio_music_volume = cfg_audio_sound_volume = cfg_audio_weather_volume = AUDIO_VOLUME_DEFAULT;

	//grid_weather_volume = grid_ambient_volume = grid_weather_volume_goal = grid_ambient_volume_goal = 100, grid_weather_volume_step, grid_ambient_volume_step;
	bell_sound_idx = -1, page_sound_idx = -1, warning_sound_idx = -1, rain1_sound_idx = -1, rain2_sound_idx = -1, snow1_sound_idx = -1, snow2_sound_idx = -1, browse_sound_idx = -1, browsebook_sound_idx = -1, thunder_sound_idx = -1, browseinven_sound_idx = -1;

	/* --- init --- */

	//if (no_cache_audio) plog("Audio cache disabled.");

#ifdef DEBUG_SOUND
	puts(format("re_init_sound_sdl() init%s", no_cache_audio == FALSE ? " (cached)" : " (not cached)"));
#endif

	/* Load sound preferences if requested */
#if 0
	if (!sound_sdl_init(no_cache_audio)) {
#else /* never cache audio right at program start, because it creates an annoying delay! */
	if (!sound_sdl_init(TRUE)) {
#endif
		plog("Failed to re-initialise audio.");

		/* Failure */
		return(1);
	}

	/* Set the mixing hook */
	mixing_hook = set_mixing_sdl;

	/* Enable sound */
	sound_hook = play_sound;

	/* Enable music */
	music_hook = play_music;
	music_hook_vol = play_music_vol;

	/* Enable weather noise overlay */
	sound_weather_hook = play_sound_weather;
	sound_weather_hook_vol = play_sound_weather_vol;

	/* Enable ambient sfx overlay */
	sound_ambient_hook = play_sound_ambient;

	/* clean-up hook */
	atexit(close_audio);

	/* start caching audio in a separate thread to eliminate startup loading time */
	if (!no_cache_audio) {
#ifdef DEBUG_SOUND
		puts("Audio cache: Creating thread..");
#endif
		load_audio_thread = SDL_CreateThread(thread_load_audio, NULL, NULL);
		if (load_audio_thread == NULL) {
#ifdef DEBUG_SOUND
			puts("Audio cache: Thread creation failed.");
#endif
			plog(format("Audio cache: Unable to create thread: %s\n", SDL_GetError()));

			/* load manually instead, with annoying delay ;-p */
			thread_load_audio(NULL);
		}
#ifdef DEBUG_SOUND
		else puts("Audio cache: Thread creation succeeded.");
#endif
	}

#ifdef DEBUG_SOUND
	puts("re_init_sound_sdl() completed.");
#endif


	/* --- refresh active audio --- */
	Send_redraw(2);

	/* Success */
	return(0);
}

/* on game termination */
void mixer_fadeall(void) {
	Mix_FadeOutMusic(1500);
	Mix_FadeOutChannel(-1, 1500);
}

//extra code I moved here for USE_SOUND_2010, for porting
//this stuff from angband into here. it's part of angband's z-files.c..- C. Blue

//z-files.c:
static bool my_fexists(const char *fname) {
	FILE *fd;
	/* Try to open it */
	fd = fopen(fname, "rb");
	/* It worked */
	if (fd != NULL) {
		fclose(fd);
		return TRUE;
	} else return FALSE;
}

/* if audioCached is TRUE, load those audio files in a separate
   thread, to avoid startup delay of client - C. Blue */
static int thread_load_audio(void *dummy) {
	(void) dummy; /* suppress compiler warning */
	int idx, subidx;

	/* process all sound fx */
	for (idx = 0; idx < SOUND_MAX_2010; idx++) {
		/* process all files for each sound event */
		for (subidx = 0; subidx < samples[idx].num; subidx++) {
			load_sample(idx, subidx);
		}
	}

	/* process all music */
	for (idx = 0; idx < MUSIC_MAX; idx++) {
		/* process all files for each sound event */
		for (subidx = 0; subidx < songs[idx].num; subidx++) {
			load_song(idx, subidx);
		}
	}

	return(0);
}

/* thread-safe loading of audio files - C. Blue */
static Mix_Chunk* load_sample(int idx, int subidx) {
	const char *filename = samples[idx].paths[subidx];
	Mix_Chunk *wave = NULL;

	SDL_LockMutex(load_sample_mutex_entrance);

	SDL_LockMutex(load_sample_mutex);

	SDL_UnlockMutex(load_sample_mutex_entrance);

	/* paranoia: check if it's already loaded (but how could it..) */
	if (samples[idx].wavs[subidx]) {
#ifdef DEBUG_SOUND
		puts(format("sample already loaded %d, %d: %s.", idx, subidx, filename));
#endif
		SDL_UnlockMutex(load_sample_mutex);
		return(samples[idx].wavs[subidx]);
	}

	/* Try loading it, if it's not yet cached */

	/* Verify it exists */
	if (!my_fexists(filename)) {
#ifdef DEBUG_SOUND
		puts(format("file doesn't exist %d, %d: %s.", idx, subidx, filename));
#endif
		SDL_UnlockMutex(load_sample_mutex);
		return(NULL);
	}

	/* Load */
	wave = Mix_LoadWAV(filename);

	/* Did we get it now? */
	if (wave) {
		samples[idx].wavs[subidx] = wave;
#ifdef DEBUG_SOUND
		puts(format("loaded sample %d, %d: %s.", idx, subidx, filename));
#endif
	} else {
#ifdef DEBUG_SOUND
		puts(format("failed to load sample %d, %d: %s.", idx, subidx, filename));
#endif
		SDL_UnlockMutex(load_sample_mutex);
		return(NULL);
	}

	SDL_UnlockMutex(load_sample_mutex);
	return(wave);
}
static Mix_Music* load_song(int idx, int subidx) {
	const char *filename = songs[idx].paths[subidx];
	Mix_Music *waveMUS = NULL;

	SDL_LockMutex(load_song_mutex_entrance);

	SDL_LockMutex(load_song_mutex);

	SDL_UnlockMutex(load_song_mutex_entrance);

	/* check if it's already loaded */
	if (songs[idx].wavs[subidx]) {
#ifdef DEBUG_SOUND
		puts(format("song already loaded %d, %d: %s.", idx, subidx, filename));
#endif
		SDL_UnlockMutex(load_song_mutex);
		return(songs[idx].wavs[subidx]);
	}

	/* Try loading it, if it's not yet cached */

	/* Verify it exists */
	if (!my_fexists(filename)) {
#ifdef DEBUG_SOUND
		puts(format("file doesn't exist %d, %d: %s.", idx, subidx, filename));
#endif
		SDL_UnlockMutex(load_song_mutex);
		return(NULL);
	}

	/* Load */
	waveMUS = Mix_LoadMUS(filename);

	/* Did we get it now? */
	if (waveMUS) {
		songs[idx].wavs[subidx] = waveMUS;
#ifdef DEBUG_SOUND
		puts(format("loaded song %d, %d: %s.", idx, subidx, filename));
#endif
	} else {
#ifdef DEBUG_SOUND
		puts(format("failed to load song %d, %d: %s.", idx, subidx, filename));
#endif
		SDL_UnlockMutex(load_song_mutex);
		return(NULL);
	}

	SDL_UnlockMutex(load_song_mutex);
	return(waveMUS);
}

/* Display options page UI that allows to comment out sounds easily */
void do_cmd_options_sfx_sdl(void) {
	int i, i2, j, d, vertikal_offset = 3, horiz_offset = 5;
	static int y = 0, j_sel = 0;
	int tmp;
	char ch;
	byte a, a2;
	cptr lua_name;
	bool go = TRUE, dis;
	char buf[1024], buf2[1024], out_val[4096], out_val2[4096], *p, evname[4096];
	FILE *fff, *fff2;
	bool cfg_audio_master_org = cfg_audio_master, cfg_audio_sound_org = cfg_audio_sound;
	bool cfg_audio_music_org = cfg_audio_music, cfg_audio_weather_org = cfg_audio_weather;

	//ANGBAND_DIR_XTRA_SOUND/MUSIC are NULL in quiet_mode!
	if (quiet_mode) {
		c_msg_print("Client is running in quiet mode, sounds are not available.");
		return;
	}
	if (!audio_sfx) {
		c_msg_print("No sound effects available.");
		return;
	}

	/* Build the filename */
	path_build(buf, 1024, ANGBAND_DIR_XTRA_SOUND, "sound.cfg");

	/* Check if the file exists */
	fff = my_fopen(buf, "r");
	if (!fff) {
		c_msg_print("Error: File 'sound.cfg' not found.");
		return;
	}
	fclose(fff);

	/* Clear screen */
	Term_clear();

	/* Interact */
	while (go) {
#ifdef USER_VOLUME_SFX
		Term_putstr(0, 0, -1, TERM_WHITE, "  (<\377ydir\377w/\377y#\377w/\377ys\377w>, \377yt\377w (toggle), \377yy\377w/\377yn\377w (enable/disable), \377yv\377w volume, \377yRETURN\377w (play), \377yESC\377w)");
#else
		Term_putstr(0, 0, -1, TERM_WHITE, "  (<\377ydir\377w/\377y#\377w/\377ys\377w>, \377yt\377w (toggle), \377yy\377w/\377yn\377w (enable/disable), \377yRETURN\377w (play), \377yESC\377w)");
#endif
		Term_putstr(0, 1, -1, TERM_WHITE, "  (\377wAll changes made here will auto-save as soon as you leave this page)");

		/* Display the events */
		for (i = y - 10 ; i <= y + 10 ; i++) {
			if (i < 0 || i >= audio_sfx) {
				Term_putstr(horiz_offset + 5, vertikal_offset + i + 10 - y, -1, TERM_WHITE, "                                                          ");
				continue;
			}

			/* Map events we've listed in our local config file onto audio.lua indices */
			i2 = -1;
			for (j = 0; j < SOUND_MAX_2010; j++) {
				if (!samples[j].config) continue;
				i2++;
				if (i2 == i) break;
			}
			if (j != SOUND_MAX_2010) { //paranoia, should always be false
				/* get event name */
				sprintf(out_val, "return get_sound_name(%d)", j);
				lua_name = string_exec_lua(0, out_val);
			} else lua_name = "<nothing>";
			if (i == y) j_sel = j;

			/* set colour depending on enabled/disabled state */
			//todo - c_cfg.use_color D: yadayada
			if (samples[j].disabled) {
				a = TERM_L_DARK;
				a2 = TERM_UMBER;
			} else {
				a = TERM_WHITE;
				a2 = TERM_YELLOW;
			}

			Term_putstr(horiz_offset + 5, vertikal_offset + i + 10 - y, -1, a2, format("  %3d", i + 1));
			Term_putstr(horiz_offset + 12, vertikal_offset + i + 10 - y, -1, a, "                                                   ");
			Term_putstr(horiz_offset + 12, vertikal_offset + i + 10 - y, -1, a, (char*)lua_name);
			if (j == weather_current || j == ambient_current) {
				if (a != TERM_L_DARK) a = TERM_L_GREEN;
				Term_putstr(horiz_offset + 5, vertikal_offset + i + 10 - y, -1, a, "*");
				Term_putstr(horiz_offset + 12, vertikal_offset + i + 10 - y, -1, a, format("%-40s  (playing)", (char*)lua_name));
			} else
				Term_putstr(horiz_offset + 12, vertikal_offset + i + 10 - y, -1, a, (char*)lua_name);

#ifdef USER_VOLUME_SFX
			if (samples[j].volume && samples[j].volume != 100) {
				if (samples[j].volume < 100) a = TERM_UMBER; else a = TERM_L_UMBER;
				Term_putstr(horiz_offset + 1 + 12 + 36 + 1, vertikal_offset + i + 10 - y, -1, a, format("%2d%%", samples[j].volume));
			}
#endif
		}

		/* display static selector */
		Term_putstr(horiz_offset + 1, vertikal_offset + 10, -1, TERM_SELECTOR, ">>>");
		Term_putstr(horiz_offset + 1 + 12 + 50 + 1, vertikal_offset + 10, -1, TERM_SELECTOR, "<<<");

		/* Place Cursor */
		//Term_gotoxy(20, vertikal_offset + y);
		/* hack: hide cursor */
		Term->scr->cx = Term->wid;
		Term->scr->cu = 1;

		/* Get key */
		ch = inkey();

		/* Analyze */
		switch (ch) {
		case ESCAPE:
			/* Restore real mixer settings */
			cfg_audio_master = cfg_audio_master_org;
			cfg_audio_sound = cfg_audio_sound_org;
			cfg_audio_music = cfg_audio_music_org;
			cfg_audio_weather = cfg_audio_weather_org;

			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);

			/* auto-save */

			/* -- save disabled info -- */

			path_build(buf, 1024, ANGBAND_DIR_XTRA_SOUND, "sound.cfg");
#ifndef WINDOWS
			path_build(buf2, 1024, ANGBAND_DIR_XTRA_SOUND, "sound.$$$");
#else
			if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
				strcpy(buf2, getenv("HOMEDRIVE"));
				strcat(buf2, getenv("HOMEPATH"));
				strcat(buf2, "\\TomeNET-nosound.cfg");
			} else path_build(buf2, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "TomeNET-nosound.cfg"); //paranoia
#endif
			fff = my_fopen(buf, "r");
			fff2 = my_fopen(buf2, "w");
			if (!fff) {
				c_msg_print("Error: File 'sound.cfg' not found.");
				return;
			}
			if (!fff2) {
				c_msg_print("Error: Cannot write to disabled-sound config file.");
				return;
			}
			while (TRUE) {
				if (!fgets(out_val, 4096, fff)) {
					if (ferror(fff)) c_msg_print("Error: Failed to read from file 'sound.cfg'.");
					break;
				}

				p = out_val;
				/* remove comment-character */
				if (*p == ';') p++;

				/* ignores lines that don't start on a letter */
				if (tolower(*p) < 'a' || tolower(*p) > 'z') {
#ifndef WINDOWS
					fputs(out_val, fff2);
#endif
					continue;
				}

				/* extract event name */
				strcpy(evname, p);
				*(strchr(evname, ' ')) = 0;

				/* find out event state (disabled/enabled) */
				j = exec_lua(0, format("return get_sound_index(\"%s\")", evname));
				if (j == -1 || !samples[j].config) {
					/* 'empty' event (no filenames specified), just copy it over same as misc lines */
#ifndef WINDOWS
					fputs(out_val, fff2);
#endif
					continue;
				}

				/* apply new state */
				if (samples[j].disabled) {
#ifndef WINDOWS
					strcpy(out_val2, ";");
					strcat(out_val2, p);
				} else strcpy(out_val2, p);
#else
					strcpy(out_val2, evname);
					strcat(out_val2, "\n");
				} else continue;
#endif

				fputs(out_val2, fff2);
			}
			fclose(fff);
			fclose(fff2);

#if 0
 #if 0 /* cannot overwrite the cfg files in Programs (x86) folder on Windows 7 (+?) */
			rename(buf, format("%s.bak", buf));
			rename(buf2, buf);
 #endif
 #if 1 /* delete target file first instead of 'over-renaming'? Seems to work on my Win 7 box at least. */
			rename(buf, format("%s.bak", buf));
			//fd_kill(file_name);
			remove(buf);
			rename(buf2, buf);
 #endif
 #if 0 /* use a separate file instead? */
			path_build(buf, 1024, ANGBAND_DIR_XTRA_MUSIC, "sound-override.cfg");
			rename(buf2, buf);
 #endif
#endif
#ifndef WINDOWS
			rename(buf, format("%s.bak", buf));
			//fd_kill(file_name);
			remove(buf);
			rename(buf2, buf);
#endif

			/* -- save volume info -- */

			path_build(buf, 1024, ANGBAND_DIR_XTRA_SOUND, "sound.cfg");
#ifndef WINDOWS
			path_build(buf2, 1024, ANGBAND_DIR_XTRA_SOUND, "TomeNET-soundvol.cfg");
#else
			if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
				strcpy(buf2, getenv("HOMEDRIVE"));
				strcat(buf2, getenv("HOMEPATH"));
				strcat(buf2, "\\TomeNET-soundvol.cfg");
			} else path_build(buf2, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "TomeNET-soundvol.cfg"); //paranoia
#endif
			fff = my_fopen(buf, "r");
			fff2 = my_fopen(buf2, "w");
			if (!fff) {
				c_msg_print("Error: File 'sound.cfg' not found.");
				return;
			}
			if (!fff2) {
				c_msg_print("Error: Cannot write to sound volume config file.");
				return;
			}
			while (TRUE) {
				if (!fgets(out_val, 4096, fff)) {
					if (ferror(fff)) c_msg_print("Error: Failed to read from file 'sound.cfg'.");
					break;
				}

				p = out_val;
				/* remove comment-character */
				if (*p == ';') p++;

				/* ignores lines that don't start on a letter */
				if (tolower(*p) < 'a' || tolower(*p) > 'z') continue;

				/* extract event name */
				strcpy(evname, p);
				*(strchr(evname, ' ')) = 0;

				/* find out event state (disabled/enabled) */
				j = exec_lua(0, format("return get_sound_index(\"%s\")", evname));
				if (j == -1 || !samples[j].config) continue;

				/* apply new state */
				if (samples[j].volume) {
					strcpy(out_val2, evname);
					strcat(out_val2, "\n");
					fputs(out_val2, fff2);
					sprintf(out_val2, "%d\n", samples[j].volume);
					fputs(out_val2, fff2);
				}
			}
			fclose(fff);
			fclose(fff2);

			/* -- all done -- */
			go = FALSE;
			break;

#ifdef USER_VOLUME_SFX /* needs work @ actual mixing algo */
		case 'v': {
			//i = c_get_quantity("Enter volume % (1..100): ", -1);
			bool inkey_msg_old = inkey_msg;
			char tmp[80];

			inkey_msg = TRUE;
			Term_putstr(0, 1, -1, TERM_L_BLUE, "                                                                              ");
			Term_putstr(0, 1, -1, TERM_L_BLUE, "  Enter volume % (1..200, other values will reset to 100%): ");
			strcpy(tmp, "100");
			if (!askfor_aux(tmp, 4, 0)) {
				inkey_msg = inkey_msg_old;
				samples[j_sel].volume = 0;
				break;
			}
			inkey_msg = inkey_msg_old;
			i = atoi(tmp);
			if (i < 1 || i == 100) i = 0;
			else if (i > 200) i = 200;
			samples[j_sel].volume = i;
			/* Note: Unlike for music we don't adjust an already playing SFX' volume live here, instead the volume is applied the next time it is played. */
			break;
			}
#endif

		case KTRL('T'):
			/* Take a screenshot */
			xhtml_screenshot("screenshot????", 2);
			break;
		case ':':
			/* specialty: allow chatting from within here */
			cmd_message();
			inkey_msg = TRUE; /* And suppress macros again.. */
			break;

		case 't': //case ' ':
			samples[j_sel].disabled = !samples[j_sel].disabled;
			if (samples[j_sel].disabled) {
				if (j_sel == weather_current && weather_channel != -1 && Mix_Playing(weather_channel)) Mix_HaltChannel(weather_channel);
				if (j_sel == ambient_current && ambient_channel != -1 && Mix_Playing(ambient_channel)) Mix_HaltChannel(ambient_channel);
			} else {
				if (j_sel == weather_current) {
					weather_current = -1; //allow restarting it
					if (weather_current_vol != -1) play_sound_weather(j_sel);
					else play_sound_weather_vol(j_sel, weather_current_vol);
				}
				if (j_sel == ambient_current) {
					ambient_current = -1; //allow restarting it
					play_sound_ambient(j_sel);
				}
			}
			/* actually advance down the list too */
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			y = (y + 1 + audio_sfx) % audio_sfx;
			break;
		case 'y':
			samples[j_sel].disabled = FALSE;
			if (j_sel == weather_current) {
				weather_current = -1; //allow restarting it
				if (weather_current_vol != -1) play_sound_weather(j_sel);
				else play_sound_weather_vol(j_sel, weather_current_vol);
			}
			if (j_sel == ambient_current) {
				ambient_current = -1; //allow restarting it
				play_sound_ambient(j_sel);
			}
			/* actually advance down the list too */
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			y = (y + 1 + audio_sfx) % audio_sfx;
			break;
		case 'n':
			samples[j_sel].disabled = TRUE;
			if (j_sel == weather_current && weather_channel != -1 && Mix_Playing(weather_channel)) Mix_HaltChannel(weather_channel);
			if (j_sel == ambient_current && ambient_channel != -1 && Mix_Playing(ambient_channel)) Mix_HaltChannel(ambient_channel);
			/* actually advance down the list too */
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			y = (y + 1 + audio_sfx) % audio_sfx;
			break;

		case '\r':
			/* Force-enable the mixer to play music */
			if (!cfg_audio_master) {
				cfg_audio_master = TRUE;
				cfg_audio_music = FALSE;
				cfg_audio_weather = FALSE;
			}
			cfg_audio_sound = TRUE;

			dis = samples[j_sel].disabled;
			samples[j_sel].disabled = FALSE;
			sound(j_sel, SFX_TYPE_MISC, 100, 0, 0, 0);
			samples[j_sel].disabled = dis;

			//cfg_audio_sound = cfg_audio_sound_org;
			break;

		case '#':
			tmp = c_get_quantity("Enter index number: ", audio_sfx) - 1;
			if (!tmp) break;
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			y = tmp;
			if (y < 0) y = 0;
			if (y >= audio_sfx) y = audio_sfx - 1;
			break;
		case 's': /* Search for event name */
			{
			char searchstr[MAX_CHARS] = { 0 };

			Term_putstr(0, 0, -1, TERM_WHITE, "  Enter (partial) sound event name: ");
			askfor_aux(searchstr, MAX_CHARS - 1, 0);
			if (!searchstr[0]) break;

			/* Map events we've listed in our local config file onto audio.lua indices */
			i2 = -1;
			for (j = 0; j < SOUND_MAX_2010; j++) {
				if (!samples[j].config) continue;
				i2++;
				/* get event name */
				sprintf(out_val, "return get_sound_name(%d)", j);
				lua_name = string_exec_lua(0, out_val);
				if (!my_strcasestr(lua_name, searchstr)) continue;
				/* match */
				y = i2;
				break;
			}
			break;
			}
		case '9':
		case 'p':
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			y = (y - 10 + audio_sfx) % audio_sfx;
			break;
		case '3':
		case ' ':
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			y = (y + 10 + audio_sfx) % audio_sfx;
			break;
		case '1':
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			y = audio_sfx - 1;
			break;
		case '7':
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			y = 0;
			break;
		case '8':
		case '2':
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			d = keymap_dirs[ch & 0x7F];
			y = (y + ddy[d] + audio_sfx) % audio_sfx;
			break;
		case '\010':
			sound(j_sel, SFX_TYPE_STOP, 100, 0, 0, 0);
			y = (y - 1 + audio_sfx) % audio_sfx;
			break;
		default:
			bell();
		}
	}
}

/* Display options page UI that allows to comment out music easily */
#ifdef ENABLE_JUKEBOX
 #define MUSIC_SKIP 10 /* Jukebox backward/forward skip interval in seconds */
#endif
void do_cmd_options_mus_sdl(void) {
	int i, i2, j, d, vertikal_offset = 3, horiz_offset = 5, song_dur = 0;
	static int y = 0, j_sel = 0; // j_sel = -1; for initially jumping to playing song, see further below
	char ch;
	byte a, a2;
	cptr lua_name;
	bool go = TRUE;
	char buf[1024], buf2[1024], out_val[4096], out_val2[4096], *p, evname[4096];
	FILE *fff, *fff2;
#ifdef ENABLE_JUKEBOX
	bool cfg_audio_master_org = cfg_audio_master, cfg_audio_sound_org = cfg_audio_sound;
	bool cfg_audio_music_org = cfg_audio_music, cfg_audio_weather_org = cfg_audio_weather;
 #ifdef JUKEBOX_INSTANT_PLAY
	bool dis;
 #endif
#endif

	//ANGBAND_DIR_XTRA_SOUND/MUSIC are NULL in quiet_mode!
	if (quiet_mode) {
		c_msg_print("Client is running in quiet mode, music is not available.");
		return;
	}
	if (!audio_music) {
		c_msg_print("No music available.");
		return;
	}

	/* Build the filename */
	path_build(buf, 1024, ANGBAND_DIR_XTRA_MUSIC, "music.cfg");

	/* Check if the file exists */
	fff = my_fopen(buf, "r");
	if (!fff) {
		c_msg_print("Error: File 'music.cfg' not found.");
		return;
	}
	fclose(fff);

#ifdef ENABLE_JUKEBOX
	jukebox_org = music_cur;
	jukebox_screen = TRUE;
#endif

	/* Clear screen */
	Term_clear();

#if 0 /* instead of this, rather add a 'j' key that jumps to the currently playing song */
	/* Initially jump selection cursor to song that is currently being played */
	if (j_sel == -1) {
		for (j = 0; j < MUSIC_MAX; j++) {
			//if (!songs[j].config) continue;
			/* playing atm? */
			if (j != music_cur) continue;
			/* match */
			j_sel = y = j;
			break;
		}
	}
#endif

	/* Interact */
	while (go) {
#ifdef ENABLE_JUKEBOX
 #ifdef USER_VOLUME_MUS
		//Term_putstr(0, 0, -1, TERM_WHITE, " \377ydir\377w/\377y#\377w/\377ys\377w select, \377yc\377w cur., \377yt\377w toggle, \377yy\377w/\377yn\377w on/off, \377yv\377w volume, \377yESC\377w leave, \377BRETURN\377w play");
		Term_putstr(0, 0, -1, TERM_WHITE, " \377ydir\377w/\377y#\377w/\377ys\377w select, \377yc\377w cur., \377yt\377w toggle, \377yy\377w/\377yn\377w on/off, \377yv\377w/\377y+\377w/\377y-\377w volume, \377BRETURN\377w play");
 #else
		Term_putstr(0, 0, -1, TERM_WHITE, " \377ydir\377w/\377y#\377w/\377ys\377w select/search, \377yc\377w current, \377yt\377w toggle, \377yy\377w/\377yn\377w on/off, \377yESC\377w leave, \377BRETURN\377w play");
 #endif
		//Term_putstr(0, 1, -1, TERM_WHITE, "  (\377wAll changes made here will auto-save as soon as you leave this page)");
		//Term_putstr(0, 1, -1, TERM_WHITE, format(" \377wChanges auto-save on leaving this UI.   \377BLEFT\377w Backward %d s, \377BRIGHT\377w Forward %d s", MUSIC_SKIP, MUSIC_SKIP));
		Term_putstr(0, 1, -1, TERM_WHITE, format(" \377yESC \377wLeave and auto-save all changes.   \377BLEFT\377w Backward %d s, \377BRIGHT\377w Forward %d s", MUSIC_SKIP, MUSIC_SKIP));
		curmus_y = -1; //assume not visible (outside of visible song list)
#else
 #ifdef USER_VOLUME_MUS
		Term_putstr(0, 0, -1, TERM_WHITE, "  (<\377ydir\377w/\377y#\377w>, \377yt\377w (toggle), \377yy\377w/\377yn\377w (enable/disable), \377yv\377w (volume), \377yESC\377w (leave))");
 #else
		Term_putstr(0, 0, -1, TERM_WHITE, "  (<\377ydir\377w/\377y#\377w>, \377yt\377w (toggle), \377yy\377w/\377yn\377w (enable/disable), \377yESC\377w (leave))");
 #endif
		Term_putstr(0, 1, -1, TERM_WHITE, "  (\377wAll changes made here will auto-save as soon as you leave this screen)");
#endif

		/* Display the events */
		for (i = y - 10 ; i <= y + 10 ; i++) {
			if (i < 0 || i >= audio_music) {
				Term_putstr(horiz_offset + 5, vertikal_offset + i + 10 - y, -1, TERM_WHITE, "                                                              ");
				continue;
			}

			/* Map events we've listed in our local config file onto audio.lua indices */
			i2 = -1;
			for (j = 0; j < MUSIC_MAX; j++) {
				if (!songs[j].config) continue;
				i2++;
				if (i2 == i) break;
			}
			if (j != MUSIC_MAX) { //paranoia, should always be non-equal aka true
				/* get event name */
				sprintf(out_val, "return get_music_name(%d)", j);
				lua_name = string_exec_lua(0, out_val);
			} else lua_name = "<nothing>";
			if (i == y) j_sel = j;

			/* set colour depending on enabled/disabled state */
			//todo - c_cfg.use_color D: yadayada
			if (songs[j].disabled) {
				a = TERM_L_DARK;
				a2 = TERM_UMBER;
			} else {
				a = TERM_WHITE;
				a2 = TERM_YELLOW;
			}

			Term_putstr(horiz_offset + 5, vertikal_offset + i + 10 - y, -1, a2, format("  %3d", i + 1));
			Term_putstr(horiz_offset + 12, vertikal_offset + i + 10 - y, -1, a, "                                                       ");
			if (j == music_cur) {
				a = (jukebox_playing != -1) ? TERM_L_BLUE : (a != TERM_L_DARK ? TERM_L_GREEN : TERM_L_DARK); /* blue = user-selected jukebox song, l-green = current game music */
				Term_putstr(horiz_offset + 5, vertikal_offset + i + 10 - y, -1, a, "*");
				/* New via SDL2_mixer: Add the timestamp */
				curmus_x = horiz_offset + 12;
				curmus_y = vertikal_offset + i + 10 - y;
				curmus_attr = a;
				if (!song_dur) Term_putstr(curmus_x, curmus_y, -1, curmus_attr, format("%-38s  (     )", (char*)lua_name));
				else Term_putstr(curmus_x, curmus_y, -1, curmus_attr, format("%-38s  (     /%02d:%02d)", (char*)lua_name, song_dur / 60, song_dur % 60));
				update_jukebox_timepos();
			} else
				Term_putstr(horiz_offset + 12, vertikal_offset + i + 10 - y, -1, a, (char*)lua_name);

#ifdef USER_VOLUME_MUS
			if (songs[j].volume && songs[j].volume != 100) {
				if (songs[j].volume < 100) a = TERM_UMBER; else a = TERM_L_UMBER;
				Term_putstr(horiz_offset + 1 + 12 + 36 + 1 - 3, vertikal_offset + i + 10 - y, -1, a, format("%2d%%", songs[j].volume)); //-6 to coexist with the new playtime display
			}
#endif
		}

		/* display static selector */
		Term_putstr(horiz_offset + 1, vertikal_offset + 10, -1, TERM_SELECTOR, ">>>");
		Term_putstr(horiz_offset + 1 + 12 + 50 + 3, vertikal_offset + 10, -1, TERM_SELECTOR, "<<<");

		/* Place Cursor */
		//Term_gotoxy(20, vertikal_offset + y);
		/* hack: hide cursor */
		Term->scr->cx = Term->wid;
		Term->scr->cu = 1;

		/* Get key */
		ch = inkey();

		/* Analyze */
		switch (ch) {
		case ESCAPE:
#ifdef ENABLE_JUKEBOX
			/* Restore real mixer settings */
			cfg_audio_master = cfg_audio_master_org;
			cfg_audio_sound = cfg_audio_sound_org;
			cfg_audio_music = cfg_audio_music_org;
			cfg_audio_weather = cfg_audio_weather_org;

			jukebox_playing = -1;
 #ifdef JUKEBOX_INSTANT_PLAY
			/* Note that this will also insta-halt current music if it happens to be <disabled> and different from our jukebox piece,
			   so no need for us to check here for songs[].disabled explicitely for that particular case.
			   However, if the currently jukeboxed song is the same one as the disabled one we do need to halt it. */
			if (jukebox_org == -1 || songs[jukebox_org].disabled) play_music_instantly(-2);//play_music(-2); -- halt song instantly instead of fading out
			else if (jukebox_org != music_cur) play_music_instantly(jukebox_org);//play_music(jukebox_org); -- switch song instantly instead of fading out+in
 #else
			if (jukebox_org == -1) play_music(-2);
			else if (jukebox_org != music_cur) {
				if (songs[jukebox_org].disabled) play_music(-2);
				else play_music(jukebox_org);
			}
 #endif
			jukebox_org = -1;
			curmus_timepos = -1; //no more song is playing in the jukebox now
			jukebox_screen = FALSE;
#endif

			/* auto-save */

			/* -- save disabled info -- */

			path_build(buf, 1024, ANGBAND_DIR_XTRA_MUSIC, "music.cfg");
#ifndef WINDOWS
			path_build(buf2, 1024, ANGBAND_DIR_XTRA_MUSIC, "music.$$$");
#else
			if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
				strcpy(buf2, getenv("HOMEDRIVE"));
				strcat(buf2, getenv("HOMEPATH"));
				strcat(buf2, "\\TomeNET-nomusic.cfg");
			} else path_build(buf2, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "TomeNET-nomusic.cfg"); //paranoia
#endif
			fff = my_fopen(buf, "r");
			fff2 = my_fopen(buf2, "w");
			if (!fff) {
				c_msg_print("Error: File 'music.cfg' not found.");
				return;
			}
			if (!fff2) {
				c_msg_print("Error: Cannot write to disabled-music config file.");
				return;
			}
			while (TRUE) {
				if (!fgets(out_val, 4096, fff)) {
					if (ferror(fff)) c_msg_print("Error: Failed to read from file 'music.cfg'.");
					break;
				}

				p = out_val;
				/* remove comment-character */
				if (*p == ';') p++;

				/* ignores lines that don't start on a letter */
				if (tolower(*p) < 'a' || tolower(*p) > 'z') {
#ifndef WINDOWS
					fputs(out_val, fff2);
#endif
					continue;
				}

				/* extract event name */
				strcpy(evname, p);
				*(strchr(evname, ' ')) = 0;

				/* find out event state (disabled/enabled) */
				j = exec_lua(0, format("return get_music_index(\"%s\")", evname));
				if (j == -1 || !songs[j].config) {
					/* 'empty' event (no filenames specified), just copy it over same as misc lines */
#ifndef WINDOWS
					fputs(out_val, fff2);
#endif
					continue;
				}

				/* apply new state */
				if (songs[j].disabled) {
#ifndef WINDOWS
					strcpy(out_val2, ";");
					strcat(out_val2, p);
				} else strcpy(out_val2, p);
#else
					strcpy(out_val2, evname);
					strcat(out_val2, "\n");
				} else continue;
#endif

				fputs(out_val2, fff2);
			}
			fclose(fff);
			fclose(fff2);

#if 0
 #if 0 /* cannot overwrite the cfg files in Programs (x86) folder on Windows 7 (+?) */
			rename(buf, format("%s.bak", buf));
			rename(buf2, buf);
 #endif
 #if 1 /* delete target file first instead of 'over-renaming'? Seems to work on my Win 7 box at least. */
			rename(buf, format("%s.bak", buf));
			//fd_kill(file_name);
			remove(buf);
			rename(buf2, buf);
 #endif
 #if 0 /* use a separate file instead? */
			path_build(buf, 1024, ANGBAND_DIR_XTRA_MUSIC, "music-override.cfg");
			rename(buf2, buf);
 #endif
#endif
#ifndef WINDOWS
			rename(buf, format("%s.bak", buf));
			//fd_kill(file_name);
			remove(buf);
			rename(buf2, buf);
#endif

			/* -- save volume info -- */

			path_build(buf, 1024, ANGBAND_DIR_XTRA_MUSIC, "music.cfg");
#ifndef WINDOWS
			path_build(buf2, 1024, ANGBAND_DIR_XTRA_MUSIC, "TomeNET-musicvol.cfg");
#else
			if (!win_dontmoveuser && getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
				strcpy(buf2, getenv("HOMEDRIVE"));
				strcat(buf2, getenv("HOMEPATH"));
				strcat(buf2, "\\TomeNET-musicvol.cfg");
			} else path_build(buf2, sizeof(path), ANGBAND_DIR_XTRA_MUSIC, "TomeNET-musicvol.cfg"); //paranoia
#endif
			fff = my_fopen(buf, "r");
			fff2 = my_fopen(buf2, "w");
			if (!fff) {
				c_msg_print("Error: File 'music.cfg' not found.");
				return;
			}
			if (!fff2) {
				c_msg_print("Error: Cannot write to music volume config file.");
				return;
			}
			while (TRUE) {
				if (!fgets(out_val, 4096, fff)) {
					if (ferror(fff)) c_msg_print("Error: Failed to read from file 'music.cfg'.");
					break;
				}

				p = out_val;
				/* remove comment-character */
				if (*p == ';') p++;

				/* ignores lines that don't start on a letter */
				if (tolower(*p) < 'a' || tolower(*p) > 'z') continue;

				/* extract event name */
				strcpy(evname, p);
				*(strchr(evname, ' ')) = 0;

				/* find out event state (disabled/enabled) */
				j = exec_lua(0, format("return get_music_index(\"%s\")", evname));
				if (j == -1 || !songs[j].config) continue;

				/* apply new state */
				if (songs[j].volume) {
					strcpy(out_val2, evname);
					strcat(out_val2, "\n");
					fputs(out_val2, fff2);
					sprintf(out_val2, "%d\n", songs[j].volume);
					fputs(out_val2, fff2);
				}
			}
			fclose(fff);
			fclose(fff2);

			/* -- all done -- */
			go = FALSE;
			break;

#ifdef USER_VOLUME_MUS
		case 'v': {
			//i = c_get_quantity("Enter volume % (1..100): ", 50);
			bool inkey_msg_old = inkey_msg;
			char tmp[80];

			inkey_msg = TRUE;
			Term_putstr(0, 1, -1, TERM_L_BLUE, "                                                                              ");
			Term_putstr(0, 1, -1, TERM_L_BLUE, "  Enter volume % (1..200, m to max, other values will reset to 100%): ");
			strcpy(tmp, "100");
			if (!askfor_aux(tmp, 4, 0)) {
				inkey_msg = inkey_msg_old;
				songs[j_sel].volume = 0;
				break;
			}
			inkey_msg = inkey_msg_old;
			if (tmp[0] == 'm') {
				for (i = 100; i < 200; i++)
					if (CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, i) == MIX_MAX_VOLUME) break;
			} else {
				i = atoi(tmp);
				if (i < 1 || i == 100) i = 0;
				else if (i > 200) i = 200;
			}
			songs[j_sel].volume = i;

			/* If song is currently playing, adjust volume live.
			   (Note: If the selected song was already playing in-game via play_music_vol() this will ovewrite the volume
			   and cause 'wrong' volume, but when it's actually re-played via play_music_vol() the volume will be correct.) */
			if (!i) i = 100; /* Revert to default volume */
			if (j_sel == music_cur) {
				int vn = CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, i);

				/* QoL: Notify if this high a boost value won't make a difference under the current mixer settings */
				if (i > 100 && vn == MIX_MAX_VOLUME && CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, i - 1) == MIX_MAX_VOLUME)
					c_msg_format("\377w(%d%% boost exceeds maximum possible volume at current mixer settings.)", i);

				Mix_VolumeMusic(vn);
			}
			break;
			}
		case '+':
			i = songs[j_sel].volume;
			if (!i) i = 100;
			i += 10;
			if (i == 11) i = 10; //min is 1, not 0, compensate in this opposite direction
			if (i == 100) i = 0;
			else if (i > 200) i = 200;
			songs[j_sel].volume = i;

			/* If song is currently playing, adjust volume live.
			   (Note: If the selected song was already playing in-game via play_music_vol() this will ovewrite the volume
			   and cause 'wrong' volume, but when it's actually re-played via play_music_vol() the volume will be correct.) */
			if (!i) i = 100; /* Revert to default volume */
			if (j_sel == music_cur) Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, i));
			break;
		case '-':
			i = songs[j_sel].volume;
			if (!i) i = 100;
			i -= 10;
			if (i == 100) i = 0;
			else if (i < 1) i = 1;
			songs[j_sel].volume = i;

			/* If song is currently playing, adjust volume live.
			   (Note: If the selected song was already playing in-game via play_music_vol() this will ovewrite the volume
			   and cause 'wrong' volume, but when it's actually re-played via play_music_vol() the volume will be correct.) */
			if (!i) i = 100; /* Revert to default volume */
			if (j_sel == music_cur) Mix_VolumeMusic(CALC_MIX_VOLUME(cfg_audio_music, cfg_audio_music_volume, i));
			break;
#endif

		case 'c': /* Jump to currently playing song */
			for (j = 0; j < MUSIC_MAX; j++) {
				/* playing atm? */
				if (j != music_cur) continue;
				/* match */
				j_sel = y = j;
				break;
			}
			break;

		case KTRL('T'):
			/* Take a screenshot */
			xhtml_screenshot("screenshot????", 2);
			break;
		case ':':
			/* specialty: allow chatting from within here */
			cmd_message();
			inkey_msg = TRUE; /* And suppress macros again.. */
			break;

		case 't': //case ' ':
			songs[j_sel].disabled = !songs[j_sel].disabled;
			if (songs[j_sel].disabled) {
				if (music_cur == j_sel && Mix_PlayingMusic()) Mix_HaltMusic();
			} else {
				if (music_cur == j_sel) {
					music_cur = -1; //allow restarting it
					play_music(j_sel);
				}
			}
			break;
		case 'y':
			songs[j_sel].disabled = FALSE;
			if (music_cur == j_sel) {
				music_cur = -1; //allow restarting it
				play_music(j_sel);
			}
			break;
		case 'n':
			songs[j_sel].disabled = TRUE;
			if (music_cur == j_sel && Mix_PlayingMusic()) Mix_HaltMusic();
			break;

#ifdef ENABLE_JUKEBOX
		case '\r':
 #ifdef JUKEBOX_INSTANT_PLAY
			dis = songs[j_sel].disabled;
			songs[j_sel].disabled = FALSE;
			jukebox_playing = j_sel;

			/* Force-enable the mixer to play music */
			if (!cfg_audio_master) {
				cfg_audio_master = TRUE;
				cfg_audio_sound = FALSE;
				cfg_audio_weather = FALSE;
			}
			cfg_audio_music = TRUE;

			play_music_instantly(j_sel);
			songs[j_sel].disabled = dis;
 #else
			if (j_sel == music_cur) break;
			if (songs[j_sel].disabled) break;
			jukebox_playing = j_sel;

			/* Force-enable the mixer to play music */
			if (!cfg_audio_master) {
				cfg_audio_master = TRUE;
				cfg_audio_sound = FALSE;
				cfg_audio_weather = FALSE;
			}
			cfg_audio_music = TRUE;

			play_music(j_sel);
 #endif

			/* Hack: Find out song length by trial and error o_O */
			{ int lb, l;
			//Mix_RewindMusic();
			lb = 0;
			l = (99 * 60 + 59) * 2; //asume 99:59 min is max duration of any song
			while (l > 1) {
				l >>= 1;
				Mix_SetMusicPosition(lb + l);

				/* Check for overflow beyond actual song length */
				i = (int)Mix_GetMusicPosition(songs[music_cur].wavs[music_cur_song]);
				/* Too far? */
				if (!i) continue;

				/* We found a minimum duration */
				lb = i;
			}
			song_dur = lb;
			/* Reset position */
			Mix_SetMusicPosition(0);
			}

			curmus_timepos = 0; //song starts to play, at 0 seconds mark ie the beginning
			break;
#endif
		case '#':
			i = c_get_quantity("  Enter music event index number: ", audio_music) - 1;
			if (i < 0) break;
			y = i;
			if (y >= audio_music) y = audio_music - 1;
			break;
		case 's': /* Search for event name */
			{
			char searchstr[MAX_CHARS] = { 0 };

			Term_putstr(0, 0, -1, TERM_WHITE, "  Enter (partial) music event name: ");
			askfor_aux(searchstr, MAX_CHARS - 1, 0);
			if (!searchstr[0]) break;

			/* Map events we've listed in our local config file onto audio.lua indices */
			i2 = -1;
			for (j = 0; j < MUSIC_MAX; j++) {
				if (!songs[j].config) continue;
				i2++;
				/* get event name */
				sprintf(out_val, "return get_music_name(%d)", j);
				lua_name = string_exec_lua(0, out_val);
				if (!my_strcasestr(lua_name, searchstr)) continue;
				/* match */
				y = i2;
				break;
			}
			break;
			}
		case '9':
		case 'p':
			y = (y - 10 + audio_music) % audio_music;
			break;
		case '3':
		case ' ':
			y = (y + 10 + audio_music) % audio_music;
			break;
		case '1':
			y = audio_music - 1;
			break;
		case '7':
			y = 0;
			break;
		case '8':
		case '2':
			d = keymap_dirs[ch & 0x7F];
			y = (y + ddy[d] + audio_music) % audio_music;
			break;
		case '\010':
			y = (y - 1 + audio_music) % audio_music;
			break;

		/* Skip forward/backward -- SDL_mixer (1.2.10) only supports ogg,mp3,mod apparently though, and it's a pita,
		   according to https://www.libsdl.org/projects/SDL_mixer/docs/SDL_mixer_65.html :
			MOD
				The double is cast to Uint16 and used for a pattern number in the module.
				Passing zero is similar to rewinding the song.
			OGG
				Jumps to position seconds from the beginning of the song.
			MP3
				Jumps to position seconds from the current position in the stream.
				So you may want to call Mix_RewindMusic before this.
				Does not go in reverse...negative values do nothing.
			Returns: 0 on success, or -1 if the codec doesn't support this function.
		   ..and worst, the is no way to retrieve the current music position, so we have to track it manually: curmus_timepos.
		*/
		case '4':
			if (curmus_timepos == -1) { /* No song is playing. */
				c_message_add("\377yYou must first press ENTER to play a song explicitely, in order to use seek.");
				break;
			}
			Mix_RewindMusic();
			curmus_timepos -= MUSIC_SKIP; /* Skip backward n seconds. */
			if (curmus_timepos < 0) curmus_timepos = 0;
			else Mix_SetMusicPosition(curmus_timepos);
			curmus_timepos = (int)Mix_GetMusicPosition(songs[music_cur].wavs[music_cur_song]); //paranoia, sync
			break;
		case '6':
			if (curmus_timepos == -1) { /* No song is playing. */
				c_message_add("\377yYou must first press ENTER to play a song explicitely, in order to use seek.");
				break;
			}
			Mix_RewindMusic();
			curmus_timepos += MUSIC_SKIP; /* Skip forward n seconds. */
			Mix_SetMusicPosition(curmus_timepos);
			/* Overflow beyond actual song length? SDL2_mixer will then restart from 0, so adjust tracking accordingly */
			curmus_timepos = (int)Mix_GetMusicPosition(songs[music_cur].wavs[music_cur_song]);
			break;

		default:
			bell();
		}
	}
}

/* Deprecated #if0-branch only: Assume curmus_timepos is not -1, aka a song is actually playing.
   Otherwise: Assume jukebox_screen is TRUE aka we're currently in the jukebox UI. */
void update_jukebox_timepos(void) {
#if 0
	curmus_timepos++; //doesn't catch when we reach the end of the song and have to reset to 0s again, so this should be if0'ed
	/* Update jukebox song time stamp */
	if (curmus_y != -1) Term_putstr(curmus_x + 34 + 7, curmus_y, -1, curmus_attr, format("%02d:%02d", curmus_timepos / 60, curmus_timepos % 60));
#else
	int i;

	/* NOTE: Mix_GetMusicPosition() requires SDL2_mixer v2.6.0, which was released on 2022-07-08 and the first src package actually lacks build info for sdl2-config.
	   That means in case you install SDL2_mixer manually into /usr/local instead of /usr, you'll have to edit the makefile and replace sdl2-config calls with the
	   correctl7 prefixed paths to /usr/local/lib and /usr/local/include instead of /usr/lib and /usr/include. -_-
	   Or you overwrite your repo version at /usr prefix instead. I guess best is to just wait till the SDL2_mixer package is in the official repository.. */
	i = (int)Mix_GetMusicPosition(songs[music_cur].wavs[music_cur_song]); //catches when we reach end of song and restart playing at 0s
	if (curmus_timepos != -1) curmus_timepos = i;
	/* Update jukebox song time stamp */
	if (curmus_y != -1) Term_putstr(curmus_x + 34 + 7, curmus_y, -1, curmus_attr, format("%02d:%02d", i / 60, i % 60));
#endif
	/* Hack: Hide the cursor */
	Term->scr->cx = Term->wid;
	Term->scr->cu = 1;
}

#endif /* SOUND_SDL */
