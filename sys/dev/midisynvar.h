/*	$OpenBSD: midisynvar.h,v 1.1 1999/01/02 00:02:37 niklas Exp $	*/
/*	$NetBSD: midisynvar.h,v 1.3 1998/11/25 22:17:07 augustss Exp $	*/

/*
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#ifndef _SYS_DEV_MIDISYNVAR_H_
#define _SYS_DEV_MIDISYNVAR_H_

typedef struct midisyn midisyn;

struct midisyn_methods {
	int  (*open)	__P((midisyn *, int));
	void (*close)   __P((midisyn *));
	int  (*ioctl)   __P((midisyn *, u_long, caddr_t, int, struct proc *));
	int  (*allocv)  __P((midisyn *, u_int32_t, u_int32_t));
	void (*noteon)  __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
	void (*noteoff) __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
	void (*keypres) __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
	void (*ctlchg)  __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
	void (*pgmchg)  __P((midisyn *, u_int32_t, u_int32_t));
	void (*chnpres) __P((midisyn *, u_int32_t, u_int32_t));
	void (*pitchb)  __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
	void (*sysex)   __P((midisyn *, u_int32_t));
};

struct voice {
	u_int chan_note;	/* channel and note */
#define MS_CHANNOTE(chan, note) ((chan) * 256 + (note))
#define MS_GETCHAN(v) ((v)->chan_note >> 8)
	u_int seqno;		/* allocation index (increases with time) */
	u_char inuse;
};

#define MIDI_MAX_CHANS 16

struct midisyn {
	/* Filled by synth driver */
	struct midisyn_methods *mets;
	char name[32];
	int nvoice;
	int flags;
#define MS_DOALLOC	1
#define MS_FREQXLATE	2
	void *data;

	/* Used by midisyn driver */
	u_int8_t buf[3];
	int pos;
	struct voice *voices;
	u_int seqno;
	u_int16_t pgms[MIDI_MAX_CHANS];
};

#define MS_GETPGM(ms, vno) ((ms)->pgms[MS_GETCHAN(&(ms)->voices[vno])])

struct midi_softc;

extern struct midi_hw_if midisyn_hw_if;

void	midisyn_attach __P((struct midi_softc *, midisyn *));

#define MIDISYN_FREQ_TO_HZ(f) ((f) >> 16)

#endif /* _SYS_DEV_MIDISYNVAR_H_ */
