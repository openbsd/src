/*	$NetBSD: audioio.h,v 1.4 1996/02/17 02:28:56 jtk Exp $	*/

/*
 * Copyright (c) 1991-1993 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _AUDIOIO_H_
#define _AUDIOIO_H_

/*
 * Audio device
 */
struct audio_prinfo {
	u_int	sample_rate;	/* sample rate in bit/s */
	u_int	channels;	/* number of channels, usually 1 or 2 */
	u_int	precision;	/* number of bits/sample */
	u_int	encoding;	/* data encoding (AUDIO_ENCODING_* above) */
	u_int	gain;		/* volume level */
	u_int	port;		/* selected I/O port */
	u_long	seek;		/* BSD extension */
	u_int	ispare[3];
	/* Current state of device: */
	u_int	samples;	/* number of samples */
	u_int	eof;		/* End Of File (zero-size writes) counter */
	u_char	pause;		/* non-zero if paused, zero to resume */
	u_char	error;		/* non-zero if underflow/overflow ocurred */
	u_char	waiting;	/* non-zero if another process hangs in open */
	u_char	cspare[3];
	u_char	open;		/* non-zero if currently open */
	u_char	active;		/* non-zero if I/O is currently active */
};
typedef struct audio_prinfo audio_prinfo_t;

struct audio_info {
	struct	audio_prinfo play;	/* Info for play (output) side */
	struct	audio_prinfo record;	/* Info for record (input) side */
	u_int	__spare;
	/* BSD extensions */
	u_int	blocksize;	/* input blocking threshold */
	u_int	hiwat;		/* output high water mark */
	u_int	lowat;		/* output low water mark */
	u_int	backlog;	/* samples of output backlog to gen. */
	u_int	mode;		/* current device mode */
#define AUMODE_PLAY	0x01
#define AUMODE_RECORD	0x02
#define AUMODE_PLAY_ALL	0x04	/* play all samples--no real-time correction */
};
typedef struct audio_info audio_info_t;

#define AUDIO_INITINFO(p)\
	(void)memset((void *)(p), 0xff, sizeof(struct audio_info))

/*
 * Parameter for the AUDIO_GETDEV ioctl to determine current
 * audio devices.
 */
#define MAX_AUDIO_DEV_LEN       16
typedef struct audio_device {
        char name[MAX_AUDIO_DEV_LEN];
        char version[MAX_AUDIO_DEV_LEN];
        char config[MAX_AUDIO_DEV_LEN];
} audio_device_t;

/*
 * Supported audio encodings
 */
/* Encoding ID's */
#define	AUDIO_ENCODING_NONE	0 /* no encoding assigned */
#define AUDIO_ENCODING_ULAW	1
#define AUDIO_ENCODING_ALAW	2
#define AUDIO_ENCODING_PCM16	3
#define AUDIO_ENCODING_LINEAR	AUDIO_ENCODING_PCM16
#define AUDIO_ENCODING_PCM8	4
#define AUDIO_ENCODING_ADPCM	5

typedef struct audio_encoding {
	int index;
	char name[MAX_AUDIO_DEV_LEN];
	int format_id;
} audio_encoding_t;

/*
 * Audio device operations
 */
#define AUDIO_GETINFO	_IOR('A', 21, struct audio_info)
#define AUDIO_SETINFO	_IOWR('A', 22, struct audio_info)
#define AUDIO_DRAIN	_IO('A', 23)
#define AUDIO_FLUSH	_IO('A', 24)
#define AUDIO_WSEEK	_IOR('A', 25, u_long)
#define AUDIO_RERROR	_IOR('A', 26, int)
#define AUDIO_GETDEV	_IOR('A', 27, struct audio_device)
#define AUDIO_GETENC	_IOWR('A', 28, struct audio_encoding)
#define AUDIO_GETFD	_IOR('A', 29, int)
#define AUDIO_SETFD	_IOWR('A', 30, int)
#define AUDIO_PERROR	_IOR('A', 31, int)

/* 
 * Mixer device
 */
#define AUDIO_MIN_GAIN	0
#define AUDIO_MAX_GAIN	255

typedef struct mixer_level {
	int num_channels;
	u_char level[8];	/* [num_channels] */
} mixer_level_t;
#define AUDIO_MIXER_LEVEL_MONO	0
#define AUDIO_MIXER_LEVEL_LEFT	0
#define AUDIO_MIXER_LEVEL_RIGHT	1

/*
 * Device operations
 */

typedef struct audio_mixer_name {
	char name[MAX_AUDIO_DEV_LEN];
	int msg_id;
} audio_mixer_name_t;

typedef struct mixer_devinfo {
	int index;
	audio_mixer_name_t label;
	int type;
#define AUDIO_MIXER_CLASS	0
#define AUDIO_MIXER_ENUM	1
#define AUDIO_MIXER_SET		2
#define AUDIO_MIXER_VALUE	3
	int mixer_class;
	int next, prev;
#define AUDIO_MIXER_LAST	-1
	union {
		struct audio_mixer_enum {
			int num_mem;
			struct {
				audio_mixer_name_t label;
				int ord;
			} member[32];
		} e;
		struct audio_mixer_set {
			int num_mem;
			struct {
				audio_mixer_name_t label;
				int mask;
			} member[32];
		} s;
		struct audio_mixer_value {
			audio_mixer_name_t units;
			int num_channels;
		} v;
	} un;
} mixer_devinfo_t;


typedef struct mixer_ctrl {
	int dev;
	int type;
	union {
		int ord;		/* enum */
		int mask;		/* set */
		mixer_level_t value;	/* value */
	} un;
} mixer_ctrl_t;

/*
 * Mixer operations
 */
#define AUDIO_MIXER_READ		_IOWR('M', 0, mixer_ctrl_t)
#define AUDIO_MIXER_WRITE		_IOWR('M', 1, mixer_ctrl_t)
#define AUDIO_MIXER_DEVINFO		_IOWR('M', 2, mixer_devinfo_t)

/*
 * Well known device names
 */
#define AudioNmicrophone	"mic"
#define AudioNline	"line"
#define AudioNcd	"CD"
#define AudioNdac	"DAC"
#define AudioNrecord	"record"
#define AudioNvolume	"volume"
#define AudioNmonitor	"monitor"
#define AudioNtreble	"treble"
#define AudioNbass	"bass"
#define AudioNspeaker	"speaker"
#define AudioNheadphone	"headphones"
#define AudioNoutput	"output"
#define AudioNinput	"input"
#define AudioNmaster	"master"
#define AudioNstereo	"stereo"
#define AudioNmono	"mono"
#define AudioNspatial	"spatial"
#define AudioNsurround	"surround"
#define AudioNpseudo	"pseudo"
#define AudioNmute	"mute"
#define AudioNenhanced	"enhanced"
#define AudioNon	"on"
#define AudioNoff	"off"
#define AudioNmode	"mode"
#define AudioNsource	"source"
#define AudioNfmsynth	"fmsynth"
#define AudioNwave	"wave"
#define AudioNmidi	"midi"
#define AudioNmixerout	"mixerout"

#define AudioElinear "linear"
#define AudioEmulaw "mulaw"
#define AudioEalaw "alaw"
#define AudioEpcm16 "PCM-16"
#define AudioEpcm8 "PCM-8"
#define AudioEadpcm "ADPCM"

#define AudioCInputs	"Inputs"
#define AudioCOutputs	"Outputs"
#define AudioCRecord	"Record"
#define AudioCMonitor	"Monitor"
#define AudioCEqualization	"Equalization"

#endif /* _AUDIOIO_H_ */
