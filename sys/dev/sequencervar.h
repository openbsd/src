/*	$OpenBSD: sequencervar.h,v 1.1 1999/01/02 00:02:39 niklas Exp $	*/
/*	$NetBSD: sequencervar.h,v 1.5 1998/11/25 22:17:07 augustss Exp $	*/

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

struct midi_softc;

struct syn_timer {
	struct	timeval start, stop;
	int	tempo, timebase;
	u_long	last;
	u_long	tick;
	int	running;
};

#define SEQ_MAXQ 256
struct sequencer_queue {
	seq_event_rec buf[SEQ_MAXQ];
	u_int	in;		/* input index in buf */
	u_int	out;		/* output index in buf */
	u_int	count;		/* filled slots in buf */
};
#define SEQ_QINIT(q) ((q)->in = (q)->out = (q)->count = 0)
#define SEQ_QEMPTY(q) ((q)->count == 0)
#define SEQ_QFULL(q) ((q)->count >= SEQ_MAXQ)
#define SEQ_QPUT(q, e) ((q)->buf[(q)->in++] = (e), (q)->in %= SEQ_MAXQ, (q)->count++)
#define SEQ_QGET(q, e) ((e) = (q)->buf[(q)->out++], (q)->out %= SEQ_MAXQ, (q)->count--)
#define SEQ_QLEN(q) ((q)->count)

struct sequencer_softc;

#define MAXCHAN 16
struct midi_dev {
	char	*name;
	int	subtype;
	int	capabilities;
	int	nr_voices;
	int	instr_bank_size;
	int	unit;
	u_char	last_cmd;
	struct	sequencer_softc *seq;
	struct	midi_softc *msc;
};

struct sequencer_softc {
	struct	device dev;
	struct	device *sc_dev;	/* Hardware device struct */
	int	isopen;		/* Open indicator */
	int	flags;		/* Open flags */
	int	mode;
#define SEQ_OLD 0
#define SEQ_NEW 1
	int	rchan, wchan;
	int	pbus;
	struct	selinfo wsel;	/* write selector */
	struct	selinfo rsel;	/* read selector */
	struct	proc *async;	/* process who wants audio SIGIO */

	char	doingsysex;	/* doing a SEQ_SYSEX */

	int	nmidi;		/* number of MIDI devices */
	struct	midi_dev **devs;
	struct	syn_timer timer;

	struct	sequencer_queue outq; /* output event queue */
	u_int	lowat;		/* output queue low water mark */
	char	timeout;	/* timeout has been set */

	struct	sequencer_queue inq; /* input event queue */
	u_long	input_stamp;
};

void seq_event_intr __P((void *, seq_event_rec *));

#define SEQUENCERUNIT(d) ((d) & 0x7f)
#define SEQ_IS_OLD(d) ((d) & 0x80)

