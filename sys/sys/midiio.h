/*	$OpenBSD: midiio.h,v 1.2 2002/03/14 03:16:12 millert Exp $	*/
/*	$NetBSD: midiio.h,v 1.7 1998/11/25 22:17:07 augustss Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_MIDIIO_H_
#define _SYS_MIDIIO_H_

/*
 * The API defined here is compatible with the OSS MIDI API except
 * for naming.
 */

#ifndef _POSIX_SOURCE
#define __MIDIIO_UNSET_POSIX_SOURCE
#define _POSIX_SOURCE		/* make sure we don't get all the gunk */
#endif
#include <machine/endian.h>
#ifdef __MIDIIO_UNSET_POSIX_SOURCE
#undef _POSIX_SOURCE
#undef __MIDIIO_UNSET_POSIX_SOURCE
#endif

/*
 * ioctl() commands for /dev/midi##
 */
typedef struct {
	unsigned	char cmd;
	char		nr_args, nr_returns;
	unsigned char	data[30];
} mpu_command_rec;

#define MIDI_PRETIME		_IOWR('m', 0, int)
#define MIDI_MPUMODE		_IOWR('m', 1, int)
#define MIDI_MPUCMD		_IOWR('m', 2, mpu_command_rec)


/* The MPU401 command acknowledge and active sense command */
#define MIDI_ACK	0xfe


/* Sequencer */
#define SEQUENCER_RESET			_IO  ('Q', 0)
#define SEQUENCER_SYNC			_IO  ('Q', 1)
#define SEQUENCER_INFO			_IOWR('Q', 2, struct synth_info)
#define SEQUENCER_CTRLRATE		_IOWR('Q', 3, int)
#define SEQUENCER_GETOUTCOUNT		_IOR ('Q', 4, int)
#define SEQUENCER_GETINCOUNT		_IOR ('Q', 5, int)
/*#define SEQUENCER_PERCMODE		_IOW ('Q', 6, int)*/
/*#define SEQUENCER_TESTMIDI		_IOW ('Q', 8, int)*/
#define SEQUENCER_RESETSAMPLES		_IOW ('Q', 9, int)
#define SEQUENCER_NRSYNTHS		_IOR ('Q',10, int)
#define SEQUENCER_NRMIDIS		_IOR ('Q',11, int)
/*#define SEQUENCER_MIDI_INFO		_IOWR('Q',12, struct midi_info)*/
#define SEQUENCER_THRESHOLD		_IOW ('Q',13, int)
#define SEQUENCER_MEMAVL		_IOWR('Q',14, int)
/*#define SEQUENCER_FM_4OP_ENABLE		_IOW ('Q',15, int)*/
#define SEQUENCER_PANIC			_IO  ('Q',17)
#define SEQUENCER_OUTOFBAND		_IOW ('Q',18, struct seq_event_rec)
#define SEQUENCER_GETTIME		_IOR ('Q',19, int)
/*#define SEQUENCER_ID			_IOWR('Q',20, struct synth_info)*/
/*#define SEQUENCER_CONTROL		_IOWR('Q',21, struct synth_control)*/
/*#define SEQUENCER_REMOVESAMPLE		_IOWR('Q',22, struct remove_sample)*/

#if 0
typedef struct synth_control {
	int	devno;		/* Synthesizer # */
	char	data[4000];	/* Device specific command/data record */
} synth_control;

typedef struct remove_sample {
	int	devno;		/* Synthesizer # */
	int	bankno;		/* MIDI bank # (0=General MIDI) */
	int	instrno;	/* MIDI instrument number */
} remove_sample;
#endif

#define CMDSIZE 8
typedef struct seq_event_rec {
	u_char	arr[CMDSIZE];
} seq_event_rec;

struct synth_info {
	char	name[30];
	int	device;
	int	synth_type;
#define SYNTH_TYPE_FM			0
#define SYNTH_TYPE_SAMPLE		1
#define SYNTH_TYPE_MIDI			2

	int	synth_subtype;
#define SYNTH_SUB_FM_TYPE_ADLIB		0x00
#define SYNTH_SUB_FM_TYPE_OPL3		0x01
#define SYNTH_SUB_MIDI_TYPE_MPU401	0x401

#define SYNTH_SUB_SAMPLE_TYPE_BASIC	0x10
#define SYNTH_SUB_SAMPLE_TYPE_GUS	SAMPLE_TYPE_BASIC

	int	nr_voices;
	int	instr_bank_size;
	u_int	capabilities;	
#define SYNTH_CAP_OPL3			0x00000002
#define SYNTH_CAP_INPUT			0x00000004
};

/* Sequencer timer */
#define SEQUENCER_TMR_TIMEBASE		_IOWR('T', 1, int)
#define SEQUENCER_TMR_START		_IO  ('T', 2)
#define SEQUENCER_TMR_STOP		_IO  ('T', 3)
#define SEQUENCER_TMR_CONTINUE		_IO  ('T', 4)
#define SEQUENCER_TMR_TEMPO		_IOWR('T', 5, int)
#define SEQUENCER_TMR_SOURCE		_IOWR('T', 6, int)
#  define SEQUENCER_TMR_INTERNAL	0x00000001
#if 0
#  define SEQUENCER_TMR_EXTERNAL	0x00000002
#  define SEQUENCER_TMR_MODE_MIDI	0x00000010
#  define SEQUENCER_TMR_MODE_FSK	0x00000020
#  define SEQUENCER_TMR_MODE_CLS	0x00000040
#  define SEQUENCER_TMR_MODE_SMPTE	0x00000080
#endif
#define SEQUENCER_TMR_METRONOME		_IOW ('T', 7, int)
#define SEQUENCER_TMR_SELECT		_IOW ('T', 8, int)


#define MIDI_CTRL_ALLOFF 123
#define MIDI_CTRL_RESET 121
#define MIDI_BEND_NEUTRAL (1<<13)

#define MIDI_NOTEOFF		0x80
#define MIDI_NOTEON		0x90
#define MIDI_KEY_PRESSURE	0xA0
#define MIDI_CTL_CHANGE		0xB0
#define MIDI_PGM_CHANGE		0xC0
#define MIDI_CHN_PRESSURE	0xD0
#define MIDI_PITCH_BEND		0xE0
#define MIDI_SYSTEM_PREFIX	0xF0

#define MIDI_IS_STATUS(d) ((d) >= 0x80)
#define MIDI_IS_COMMON(d) ((d) >= 0xf0)

#define MIDI_SYSEX_START	0xF0
#define MIDI_SYSEX_END		0xF7

#define MIDI_GET_STATUS(d) ((d) & 0xf0)
#define MIDI_GET_CHAN(d) ((d) & 0x0f)

#define MIDI_HALF_VEL 64

#define SEQ_LOCAL		0x80
#define SEQ_TIMING		0x81
#define SEQ_CHN_COMMON		0x92
#define SEQ_CHN_VOICE		0x93
#define SEQ_SYSEX		0x94
#define SEQ_FULLSIZE		0xfd

#define SEQ_MK_CHN_VOICE(e, unit, cmd, chan, key, vel) (\
    (e)->arr[0] = SEQ_CHN_VOICE, (e)->arr[1] = (unit), (e)->arr[2] = (cmd),\
    (e)->arr[3] = (chan), (e)->arr[4] = (key), (e)->arr[5] = (vel),\
    (e)->arr[6] = 0, (e)->arr[7] = 0)
#define SEQ_MK_CHN_COMMON(e, unit, cmd, chan, p1, p2, w14) (\
    (e)->arr[0] = SEQ_CHN_COMMON, (e)->arr[1] = (unit), (e)->arr[2] = (cmd),\
    (e)->arr[3] = (chan), (e)->arr[4] = (p1), (e)->arr[5] = (p2),\
    *(short *)&(e)->arr[6] = (w14))

#if _QUAD_LOWWORD == 1
/* big endian */
#define SEQ_PATCHKEY(id) (0xfd00|id)
#else
/* little endian */
#define SEQ_PATCHKEY(id) ((id<<8)|0xfd)
#endif
struct sysex_info {
	u_int16_t	key;	/* Use SYSEX_PATCH or MAUI_PATCH here */
#define SEQ_SYSEX_PATCH	SEQ_PATCHKEY(0x05)
#define SEQ_MAUI_PATCH	SEQ_PATCHKEY(0x06)
	int16_t	device_no;	/* Synthesizer number */
	int32_t	len;		/* Size of the sysex data in bytes */
	u_char	data[1];	/* Sysex data starts here */
};
#define SEQ_SYSEX_HDRSIZE ((u_long)((struct sysex_info *)0)->data)

typedef unsigned char sbi_instr_data[32];
struct sbi_instrument {
	u_int16_t key;	/* FM_PATCH or OPL3_PATCH */
#define SBI_FM_PATCH	SEQ_PATCHKEY(0x01)
#define SBI_OPL3_PATCH	SEQ_PATCHKEY(0x03)
	int16_t		device;
	int32_t		channel;
	sbi_instr_data	operators;
};

#define TMR_RESET		0
#define TMR_WAIT_REL		1	/* Time relative to the prev time */
#define TMR_WAIT_ABS		2	/* Absolute time since TMR_START */
#define TMR_STOP		3
#define TMR_START		4
#define TMR_CONTINUE		5
#define TMR_TEMPO		6
#define TMR_ECHO		8
#define TMR_CLOCK		9	/* MIDI clock */
#define TMR_SPP			10	/* Song position pointer */
#define TMR_TIMESIG		11	/* Time signature */

/* Old sequencer definitions */
#define SEQOLD_CMDSIZE 4

#define SEQOLD_NOTEOFF		0
#define SEQOLD_NOTEON		1
#define SEQOLD_WAIT		TMR_WAIT_ABS
#define SEQOLD_PGMCHANGE	3
#define SEQOLD_SYNCTIMER	TMR_START
#define SEQOLD_MIDIPUTC		5
#define SEQOLD_ECHO		TMR_ECHO
#define SEQOLD_AFTERTOUCH	9
#define SEQOLD_CONTROLLER	10
#define SEQOLD_PRIVATE		0xfe
#define SEQOLD_EXTENDED		0xff

#endif /* !_SYS_MIDIIO_H_ */
