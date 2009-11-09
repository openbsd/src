/*	$OpenBSD: sequencer.c,v 1.19 2009/11/09 17:53:39 nicm Exp $	*/
/*	$NetBSD: sequencer.c,v 1.13 1998/11/25 22:17:07 augustss Exp $	*/

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

#include "sequencer.h"
#if NSEQUENCER > 0

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/conf.h>
#include <sys/audioio.h>
#include <sys/midiio.h>
#include <sys/device.h>

#include <dev/midi_if.h>
#include <dev/midivar.h>
#include <dev/sequencervar.h>

#ifndef splaudio
#define splaudio() splbio()	/* XXX found in audio_if.h normally */
#endif

#define ADDTIMEVAL(a, b) ( \
	(a)->tv_sec += (b)->tv_sec, \
	(a)->tv_usec += (b)->tv_usec, \
	(a)->tv_usec >= 1000000 ? ((a)->tv_sec++, (a)->tv_usec -= 1000000) : 0\
	)

#define SUBTIMEVAL(a, b) ( \
	(a)->tv_sec -= (b)->tv_sec, \
	(a)->tv_usec -= (b)->tv_usec, \
	(a)->tv_usec < 0 ? ((a)->tv_sec--, (a)->tv_usec += 1000000) : 0\
	)

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (sequencerdebug) printf x
#define DPRINTFN(n,x)	if (sequencerdebug >= (n)) printf x
int	sequencerdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define SEQ_CMD(b)  ((b)->arr[0])

#define SEQ_EDEV(b)  ((b)->arr[1])
#define SEQ_ECMD(b)  ((b)->arr[2])
#define SEQ_ECHAN(b) ((b)->arr[3])
#define SEQ_ENOTE(b) ((b)->arr[4])
#define SEQ_EPARM(b) ((b)->arr[5])

#define SEQ_EP1(b)   ((b)->arr[4])
#define SEQ_EP2(b)   ((b)->arr[5])

#define SEQ_XCMD(b)  ((b)->arr[1])
#define SEQ_XDEV(b)  ((b)->arr[2])
#define SEQ_XCHAN(b) ((b)->arr[3])
#define SEQ_XNOTE(b) ((b)->arr[4])
#define SEQ_XVEL(b)  ((b)->arr[5])

#define SEQ_TCMD(b)  ((b)->arr[1])
#define SEQ_TPARM(b) ((b)->arr[4])

#define SEQ_NOTE_MAX 128
#define SEQ_NOTE_XXX 255
#define SEQ_VEL_OFF 0

#define RECALC_TICK(t) ((t)->tick = 60 * 1000000L / ((t)->tempo * (t)->timebase))

struct sequencer_softc seqdevs[NSEQUENCER];

void sequencerattach(int);
void seq_reset(struct sequencer_softc *);
int seq_do_command(struct sequencer_softc *, seq_event_rec *);
int seq_do_extcommand(struct sequencer_softc *, seq_event_rec *);
int seq_do_chnvoice(struct sequencer_softc *, seq_event_rec *);
int seq_do_chncommon(struct sequencer_softc *, seq_event_rec *);
int seq_do_timing(struct sequencer_softc *, seq_event_rec *);
int seq_do_local(struct sequencer_softc *, seq_event_rec *);
int seq_do_sysex(struct sequencer_softc *, seq_event_rec *);
int seq_do_fullsize(struct sequencer_softc *, seq_event_rec *,
			 struct uio *);
int seq_timer(struct sequencer_softc *, int, int, seq_event_rec *);
static int seq_input_event(struct sequencer_softc *, seq_event_rec *);
int seq_drain(struct sequencer_softc *);
void seq_startoutput(struct sequencer_softc *);
void seq_timeout(void *);
int seq_to_new(seq_event_rec *, struct uio *);
static int seq_sleep_timo(int *, char *, int);
static int seq_sleep(int *, char *);
static void seq_wakeup(int *);

struct midi_softc;
int midiseq_out(struct midi_dev *, u_char *, u_int, int);
struct midi_dev *midiseq_open(int, int);
void midiseq_close(struct midi_dev *);
void midiseq_reset(struct midi_dev *);
int midiseq_noteon(struct midi_dev *, int, int, int);
int midiseq_noteoff(struct midi_dev *, int, int, int);
int midiseq_keypressure(struct midi_dev *, int, int, int);
int midiseq_pgmchange(struct midi_dev *, int, int);
int midiseq_chnpressure(struct midi_dev *, int, int);
int midiseq_ctlchange(struct midi_dev *, int, int, int);
int midiseq_pitchbend(struct midi_dev *, int, int);
int midiseq_loadpatch(struct midi_dev *, struct sysex_info *,
			   struct uio *);
int midiseq_putc(struct midi_dev *, int);
void midiseq_in(struct midi_dev *, u_char *, int);

void
sequencerattach(int n)
{
}

int
sequenceropen(dev_t dev, int flags, int ifmt, struct proc *p)
{
	int unit = SEQUENCERUNIT(dev);
	struct sequencer_softc *sc;
	struct midi_dev *md;
	int nmidi;

	DPRINTF(("sequenceropen\n"));

	if (unit >= NSEQUENCER)
		return (ENXIO);
	sc = &seqdevs[unit];
	if (sc->isopen)
		return (EBUSY);
	if (SEQ_IS_OLD(dev))
		sc->mode = SEQ_OLD;
	else
		sc->mode = SEQ_NEW;
	sc->isopen++;
	sc->flags = flags & (FREAD|FWRITE);
	sc->rchan = 0;
	sc->wchan = 0;
	sc->pbus = 0;
	sc->async = 0;
	sc->input_stamp = ~0;

	sc->nmidi = 0;
	nmidi = midi_unit_count();

	sc->devs = malloc(nmidi * sizeof(struct midi_dev *),
			  M_DEVBUF, M_WAITOK);
	for (unit = 0; unit < nmidi; unit++) {
		md = midiseq_open(unit, flags);
		if (md) {
			sc->devs[sc->nmidi++] = md;
			md->seq = sc;
		}
	}

	sc->timer.timebase = 100;
	sc->timer.tempo = 60;
	sc->doingsysex = 0;
	RECALC_TICK(&sc->timer);
	sc->timer.last = 0;
	microtime(&sc->timer.start);

	SEQ_QINIT(&sc->inq);
	SEQ_QINIT(&sc->outq);
	sc->lowat = SEQ_MAXQ / 2;
	timeout_set(&sc->timo, seq_timeout, sc);

	seq_reset(sc);

	DPRINTF(("sequenceropen: mode=%d, nmidi=%d\n", sc->mode, sc->nmidi));
	return (0);
}

static int
seq_sleep_timo(int *chan, char *label, int timo)
{
	int st;

	if (!label)
		label = "seq";

	DPRINTFN(5, ("seq_sleep_timo: %p %s %d\n", chan, label, timo));
	*chan = 1;
	st = tsleep(chan, PWAIT | PCATCH, label, timo);
	*chan = 0;
#ifdef MIDI_DEBUG
	if (st != 0)
	    printf("seq_sleep: %d\n", st);
#endif
	return (st);
}

static int
seq_sleep(int *chan, char *label)
{
	return (seq_sleep_timo(chan, label, 0));
}

static void
seq_wakeup(int *chan)
{
	if (*chan) {
		DPRINTFN(5, ("seq_wakeup: %p\n", chan));
		wakeup(chan);
		*chan = 0;
	}
}

int
seq_drain(struct sequencer_softc *sc)
{
	int error;

	DPRINTFN(3, ("seq_drain: %p, len=%d\n", sc, SEQ_QLEN(&sc->outq)));
	seq_startoutput(sc);
	error = 0;
	while(!SEQ_QEMPTY(&sc->outq) && !error)
		error = seq_sleep_timo(&sc->wchan, "seq_dr", 60*hz);
	return (error);
}

void
seq_timeout(void *addr)
{
	struct sequencer_softc *sc = addr;
	DPRINTFN(4, ("seq_timeout: %p\n", sc));
	sc->timeout = 0;
	seq_startoutput(sc);
	if (SEQ_QLEN(&sc->outq) < sc->lowat) {
		seq_wakeup(&sc->wchan);
		selwakeup(&sc->wsel);
		if (sc->async)
			psignal(sc->async, SIGIO);
	}

}

void
seq_startoutput(struct sequencer_softc *sc)
{
	struct sequencer_queue *q = &sc->outq;
	seq_event_rec cmd;

	if (sc->timeout)
		return;
	DPRINTFN(4, ("seq_startoutput: %p, len=%d\n", sc, SEQ_QLEN(q)));
	while(!SEQ_QEMPTY(q) && !sc->timeout) {
		SEQ_QGET(q, cmd);
		seq_do_command(sc, &cmd);
	}
}

int
sequencerclose(dev_t dev, int flags, int ifmt, struct proc *p)
{
	struct sequencer_softc *sc = &seqdevs[SEQUENCERUNIT(dev)];
	int n, s;

	DPRINTF(("sequencerclose: %p\n", sc));

	seq_drain(sc);
	s = splaudio();
	if (sc->timeout) {
		timeout_del(&sc->timo);
		sc->timeout = 0;
	}
	splx(s);

	for (n = 0; n < sc->nmidi; n++)
		midiseq_close(sc->devs[n]);
	free(sc->devs, M_DEVBUF);
	sc->isopen = 0;
	return (0);
}

static int
seq_input_event(struct sequencer_softc *sc, seq_event_rec *cmd)
{
	struct sequencer_queue *q = &sc->inq;

	DPRINTFN(2, ("seq_input_event: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		     cmd->arr[0], cmd->arr[1], cmd->arr[2], cmd->arr[3],
		     cmd->arr[4], cmd->arr[5], cmd->arr[6], cmd->arr[7]));
	if (SEQ_QFULL(q))
		return (ENOMEM);
	SEQ_QPUT(q, *cmd);
	seq_wakeup(&sc->rchan);
	selwakeup(&sc->rsel);
	if (sc->async)
		psignal(sc->async, SIGIO);
	return (0);
}

void
seq_event_intr(void *addr, seq_event_rec *iev)
{
	struct sequencer_softc *sc = addr;
	union {
		u_int32_t l;
		u_int8_t b[4];
	} u;
	u_long t;
	struct timeval now;
	seq_event_rec ev;

	microtime(&now);
	SUBTIMEVAL(&now, &sc->timer.start);
	t = now.tv_sec * 1000000 + now.tv_usec;
	t /= sc->timer.tick;
	if (t != sc->input_stamp) {
		ev.arr[0] = SEQ_TIMING;
		ev.arr[1] = TMR_WAIT_ABS;
		ev.arr[2] = 0;
		ev.arr[3] = 0;
		u.l = t;
		ev.arr[4] = u.b[0];
		ev.arr[5] = u.b[1];
		ev.arr[6] = u.b[2];
		ev.arr[7] = u.b[3];
		seq_input_event(sc, &ev);
		sc->input_stamp = t;
	}
	seq_input_event(sc, iev);
}

int
sequencerread(dev_t dev, struct uio *uio, int ioflag)
{
	struct sequencer_softc *sc = &seqdevs[SEQUENCERUNIT(dev)];
	struct sequencer_queue *q = &sc->inq;
	seq_event_rec ev;
	int error, s;

	DPRINTFN(20, ("sequencerread: %p, count=%d, ioflag=%x\n",
		     sc, uio->uio_resid, ioflag));

	if (sc->mode == SEQ_OLD) {
		DPRINTF(("sequencerread: old read\n"));
		return (EINVAL); /* XXX unimplemented */
	}

	error = 0;
	while (SEQ_QEMPTY(q)) {
		if (ioflag & IO_NDELAY)
			return (EWOULDBLOCK);
		else {
			error = seq_sleep(&sc->rchan, "seq rd");
			if (error)
				return (error);
		}
	}
	s = splaudio();
	while (uio->uio_resid >= sizeof ev && !error && !SEQ_QEMPTY(q)) {
		SEQ_QGET(q, ev);
		error = uiomove((caddr_t)&ev, sizeof ev, uio);
	}
	splx(s);
	return (error);
}

int
sequencerwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct sequencer_softc *sc = &seqdevs[SEQUENCERUNIT(dev)];
	struct sequencer_queue *q = &sc->outq;
	int error;
	seq_event_rec cmdbuf;
	int size;

	DPRINTFN(2, ("sequencerwrite: %p, count=%d\n", sc, uio->uio_resid));

	error = 0;
	size = sc->mode == SEQ_NEW ? sizeof cmdbuf : SEQOLD_CMDSIZE;
	while (uio->uio_resid >= size) {
		error = uiomove((caddr_t)&cmdbuf, size, uio);
		if (error)
			break;
		if (sc->mode == SEQ_OLD)
			if (seq_to_new(&cmdbuf, uio))
				continue;
		if (SEQ_CMD(&cmdbuf) == SEQ_FULLSIZE) {
			/* We do it like OSS does, asynchronously */
			error = seq_do_fullsize(sc, &cmdbuf, uio);
			if (error)
				break;
			continue;
		}
		while (SEQ_QFULL(q)) {
			seq_startoutput(sc);
			if (SEQ_QFULL(q)) {
				if (ioflag & IO_NDELAY)
					return (EWOULDBLOCK);
				error = seq_sleep(&sc->wchan, "seq_wr");
				if (error)
					return (error);
			}
		}
		SEQ_QPUT(q, cmdbuf);
	}
	seq_startoutput(sc);

#ifdef SEQUENCER_DEBUG
	if (error)
		DPRINTFN(2, ("sequencerwrite: error=%d\n", error));
#endif
	return (error);
}

int
sequencerioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct sequencer_softc *sc = &seqdevs[SEQUENCERUNIT(dev)];
	struct synth_info *si;
	struct midi_dev *md;
	int devno;
	int error;
	int t;

	DPRINTFN(2, ("sequencerioctl: %p cmd=0x%08lx\n", sc, cmd));

	error = 0;
	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		break;

	case FIOASYNC:
		if (*(int *)addr) {
			if (sc->async)
				return (EBUSY);
			sc->async = p;
			DPRINTF(("sequencer_ioctl: FIOASYNC %p\n", p));
		} else
			sc->async = 0;
		break;

	case SEQUENCER_RESET:
		seq_reset(sc);
		break;

	case SEQUENCER_PANIC:
		seq_reset(sc);
		/* Do more?  OSS doesn't */
		break;

	case SEQUENCER_SYNC:
		if (sc->flags == FREAD)
			return (0);
		seq_drain(sc);
		error = 0;
		break;

	case SEQUENCER_INFO:
		si = (struct synth_info*)addr;
		devno = si->device;
		if (devno < 0 || devno >= sc->nmidi)
			return (EINVAL);
		md = sc->devs[devno];
		strncpy(si->name, md->name, sizeof si->name);
		si->synth_type = SYNTH_TYPE_MIDI;
		si->synth_subtype = md->subtype;
		si->nr_voices = md->nr_voices;
		si->instr_bank_size = md->instr_bank_size;
		si->capabilities = md->capabilities;
		break;

	case SEQUENCER_NRSYNTHS:
		*(int *)addr = sc->nmidi;
		break;

	case SEQUENCER_NRMIDIS:
		*(int *)addr = sc->nmidi;
		break;

	case SEQUENCER_OUTOFBAND:
		DPRINTFN(3, ("sequencer_ioctl: OOB=%02x %02x %02x %02x %02x %02x %02x %02x\n",
			     *(u_char *)addr, *(u_char *)(addr+1),
			     *(u_char *)(addr+2), *(u_char *)(addr+3),
			     *(u_char *)(addr+4), *(u_char *)(addr+5),
			     *(u_char *)(addr+6), *(u_char *)(addr+7)));
		error = seq_do_command(sc, (seq_event_rec *)addr);
		break;

	case SEQUENCER_TMR_TIMEBASE:
		t = *(int *)addr;
		if (t < 1)
			t = 1;
		if (t > 1000)
			t = 1000;
		sc->timer.timebase = t;
		*(int *)addr = t;
		RECALC_TICK(&sc->timer);
		break;

	case SEQUENCER_TMR_START:
		error = seq_timer(sc, TMR_START, 0, 0);
		break;

	case SEQUENCER_TMR_STOP:
		error = seq_timer(sc, TMR_STOP, 0, 0);
		break;

	case SEQUENCER_TMR_CONTINUE:
		error = seq_timer(sc, TMR_CONTINUE, 0, 0);
		break;

	case SEQUENCER_TMR_TEMPO:
		t = *(int *)addr;
		if (t < 8)
			t = 8;
		if (t > 250)
			t = 250;
		sc->timer.tempo = t;
		*(int *)addr = t;
		RECALC_TICK(&sc->timer);
		break;

	case SEQUENCER_TMR_SOURCE:
		*(int *)addr = SEQUENCER_TMR_INTERNAL;
		break;

	case SEQUENCER_TMR_METRONOME:
		/* noop */
		break;

	case SEQUENCER_THRESHOLD:
		t = SEQ_MAXQ - *(int *)addr / sizeof (seq_event_rec);
		if (t < 1)
			t = 1;
		if (t > SEQ_MAXQ)
			t = SEQ_MAXQ;
		sc->lowat = t;
		break;

	case SEQUENCER_CTRLRATE:
		*(int *)addr = (sc->timer.tempo*sc->timer.timebase + 30) / 60;
		break;

	case SEQUENCER_GETTIME:
	{
		struct timeval now;
		u_long t;
		microtime(&now);
		SUBTIMEVAL(&now, &sc->timer.start);
		t = now.tv_sec * 1000000 + now.tv_usec;
		t /= sc->timer.tick;
		*(int *)addr = t;
		break;
	}

	default:
		DPRINTF(("sequencer_ioctl: unimpl %08lx\n", cmd));
		error = ENOTTY;
		break;
	}
	return (error);
}

int
sequencerpoll(dev_t dev, int events, struct proc *p)
{
	struct sequencer_softc *sc = &seqdevs[SEQUENCERUNIT(dev)];
	int revents = 0;

	DPRINTF(("sequencerpoll: %p rw=0x%x\n", sc, events));

	if (events & (POLLIN | POLLRDNORM)) {
		if (!SEQ_QEMPTY(&sc->inq))
			revents |= events & (POLLIN | POLLRDNORM);
	}
	if (events & (POLLOUT | POLLWRNORM)) {
		if (SEQ_QLEN(&sc->outq) < sc->lowat)
			revents |= events & (POLLOUT | POLLWRNORM);
	}
	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &sc->rsel);
		if (events & (POLLOUT | POLLWRNORM))
			selrecord(p, &sc->wsel);
	}
	return (revents);
}

int
sequencerkqfilter(dev_t dev, struct knote *kn)
{
	return (EPERM);
}

void
seq_reset(struct sequencer_softc *sc)
{
	int i, chn;
	struct midi_dev *md;

	for (i = 0; i < sc->nmidi; i++) {
		md = sc->devs[i];
		midiseq_reset(md);
		for (chn = 0; chn < MAXCHAN; chn++) {
			midiseq_ctlchange(md, chn, MIDI_CTRL_ALLOFF, 0);
			midiseq_ctlchange(md, chn, MIDI_CTRL_RESET, 0);
			midiseq_pitchbend(md, chn, MIDI_BEND_NEUTRAL);
		}
	}
}

int
seq_do_command(struct sequencer_softc *sc, seq_event_rec *b)
{
	int dev;

	DPRINTFN(4, ("seq_do_command: %p cmd=0x%02x\n", sc, SEQ_CMD(b)));

	switch(SEQ_CMD(b)) {
	case SEQ_LOCAL:
		return (seq_do_local(sc, b));
	case SEQ_TIMING:
		return (seq_do_timing(sc, b));
	case SEQ_CHN_VOICE:
		return (seq_do_chnvoice(sc, b));
	case SEQ_CHN_COMMON:
		return (seq_do_chncommon(sc, b));
	case SEQ_SYSEX:
		return (seq_do_sysex(sc, b));
	/* COMPAT */
	case SEQOLD_MIDIPUTC:
		dev = b->arr[2];
		if (dev < 0 || dev >= sc->nmidi)
			return (ENXIO);
		return (midiseq_putc(sc->devs[dev], b->arr[1]));
	default:
		DPRINTF(("seq_do_command: unimpl command %02x\n",
			     SEQ_CMD(b)));
		return (EINVAL);
	}
}

int
seq_do_chnvoice(struct sequencer_softc *sc, seq_event_rec *b)
{
	int cmd, dev, chan, note, parm, voice;
	int error;
	struct midi_dev *md;

	dev = SEQ_EDEV(b);
	if (dev < 0 || dev >= sc->nmidi)
		return (ENXIO);
	md = sc->devs[dev];
	cmd = SEQ_ECMD(b);
	chan = SEQ_ECHAN(b);
	note = SEQ_ENOTE(b);
	parm = SEQ_EPARM(b);
	DPRINTFN(2,("seq_do_chnvoice: cmd=%02x dev=%d chan=%d note=%d parm=%d\n",
		    cmd, dev, chan, note, parm));
	voice = chan;
	if (cmd == MIDI_NOTEON && parm == 0) {
		cmd = MIDI_NOTEOFF;
		parm = MIDI_HALF_VEL;
	}
	switch(cmd) {
	case MIDI_NOTEON:
		DPRINTFN(5, ("seq_do_chnvoice: noteon %p %d %d %d\n",
			     md, voice, note, parm));
		error = midiseq_noteon(md, voice, note, parm);
		break;
	case MIDI_NOTEOFF:
		error = midiseq_noteoff(md, voice, note, parm);
		break;
	case MIDI_KEY_PRESSURE:
		error = midiseq_keypressure(md, voice, note, parm);
		break;
	default:
		DPRINTF(("seq_do_chnvoice: unimpl command %02x\n", cmd));
		error = EINVAL;
		break;
	}
	return (error);
}

int
seq_do_chncommon(struct sequencer_softc *sc, seq_event_rec *b)
{
	int cmd, dev, chan, p1, w14;
	int error;
	struct midi_dev *md;
	union {
		int16_t s;
		u_int8_t b[2];
	} u;

	dev = SEQ_EDEV(b);
	if (dev < 0 || dev >= sc->nmidi)
		return (ENXIO);
	md = sc->devs[dev];
	cmd = SEQ_ECMD(b);
	chan = SEQ_ECHAN(b);
	p1 = SEQ_EP1(b);
	u.b[0] = b->arr[6];
	u.b[1] = b->arr[7];
	w14 = u.s;
	DPRINTFN(2,("seq_do_chncommon: %02x\n", cmd));

	error = 0;
	switch(cmd) {
	case MIDI_PGM_CHANGE:
		error = midiseq_pgmchange(md, chan, p1);
		break;
	case MIDI_CTL_CHANGE:
		if (chan > 15 || p1 > 127)
			return (0); /* EINVAL */
		error = midiseq_ctlchange(md, chan, p1, w14);
		break;
	case MIDI_PITCH_BEND:
		error = midiseq_pitchbend(md, chan, w14);
		break;
	case MIDI_CHN_PRESSURE:
		error = midiseq_chnpressure(md, chan, p1);
		break;
	default:
		DPRINTF(("seq_do_chncommon: unimpl command %02x\n", cmd));
		error = EINVAL;
		break;
	}
	return (error);
}

int
seq_do_timing(struct sequencer_softc *sc, seq_event_rec *b)
{
	union {
		int32_t i;
		u_int8_t b[4];
	} u;

	u.b[0] = b->arr[4];
	u.b[1] = b->arr[5];
	u.b[2] = b->arr[6];
	u.b[3] = b->arr[7];
	return (seq_timer(sc, SEQ_TCMD(b), u.i, b));
}

int
seq_do_local(struct sequencer_softc *sc, seq_event_rec *b)
{
	return (EINVAL);
}

int
seq_do_sysex(struct sequencer_softc *sc, seq_event_rec *b)
{
	int dev, i;
	struct midi_dev *md;
	u_int8_t c, *buf = &b->arr[2];

	dev = SEQ_EDEV(b);
	if (dev < 0 || dev >= sc->nmidi)
		return (ENXIO);
	DPRINTF(("seq_do_sysex: dev=%d\n", dev));
	md = sc->devs[dev];

	if (!sc->doingsysex) {
		c = MIDI_SYSEX_START;
		midiseq_out(md, &c, 1, 0);
		sc->doingsysex = 1;
	}

	for (i = 0; i < 6 && buf[i] != 0xff; i++)
		;
	midiseq_out(md, buf, i, 0);
	if (i < 6 || (i > 0 && buf[i-1] == MIDI_SYSEX_END))
		sc->doingsysex = 0;
	return (0);
}

int
seq_timer(struct sequencer_softc *sc, int cmd, int parm, seq_event_rec *b)
{
	struct syn_timer *t = &sc->timer;
	struct timeval when;
	int ticks;
	int error;
	long long usec;

	DPRINTFN(2,("seq_timer: %02x %d\n", cmd, parm));

	error = 0;
	switch(cmd) {
	case TMR_WAIT_REL:
		parm += t->last;
		/* FALLTHROUGH */
	case TMR_WAIT_ABS:
		t->last = parm;
		usec = (long long)parm * (long long)t->tick; /* convert to usec */
		when.tv_sec = usec / 1000000;
		when.tv_usec = usec % 1000000;
		DPRINTFN(4, ("seq_timer: parm=%d, sleep when=%ld.%06ld", parm,
			     when.tv_sec, when.tv_usec));
		ADDTIMEVAL(&when, &t->start); /* abstime for end */
		ticks = hzto(&when);
		DPRINTFN(4, (" when+start=%ld.%06ld, ticks=%d\n",
			     when.tv_sec, when.tv_usec, ticks));
		if (ticks > 0) {
#ifdef DIAGNOSTIC
			if (ticks > 20 * hz) {
				/* Waiting more than 20s */
				printf("seq_timer: funny ticks=%d, usec=%lld, parm=%d, tick=%ld\n",
				       ticks, usec, parm, t->tick);
			}
#endif
			sc->timeout = 1;
			timeout_add(&sc->timo, ticks);
		}
#ifdef SEQUENCER_DEBUG
		else if (ticks < 0)
			DPRINTF(("seq_timer: ticks = %d\n", ticks));
#endif
		break;
	case TMR_START:
		microtime(&t->start);
		t->running = 1;
		break;
	case TMR_STOP:
		microtime(&t->stop);
		t->running = 0;
		break;
	case TMR_CONTINUE:
		microtime(&when);
		SUBTIMEVAL(&when, &t->stop);
		ADDTIMEVAL(&t->start, &when);
		t->running = 1;
		break;
	case TMR_TEMPO:
		/* parm is ticks per minute / timebase */
		if (parm < 8)
			parm = 8;
		if (parm > 360)
			parm = 360;
		t->tempo = parm;
		RECALC_TICK(t);
		break;
	case TMR_ECHO:
		error = seq_input_event(sc, b);
		break;
	case TMR_RESET:
		t->last = 0;
		microtime(&t->start);
		break;
	default:
		DPRINTF(("seq_timer: unknown %02x\n", cmd));
		error = EINVAL;
		break;
	}
	return (error);
}

int
seq_do_fullsize(struct sequencer_softc *sc, seq_event_rec *b, struct uio *uio)
{
	struct sysex_info sysex;
	u_int dev;

#ifdef DIAGNOSTIC
	if (sizeof(seq_event_rec) != SEQ_SYSEX_HDRSIZE) {
		printf("seq_do_fullsize: sysex size ??\n");
		return (EINVAL);
	}
#endif
	memcpy(&sysex, b, sizeof sysex);
	dev = sysex.device_no;
	DPRINTFN(2, ("seq_do_fullsize: fmt=%04x, dev=%d, len=%d\n",
		     sysex.key, dev, sysex.len));
	return (midiseq_loadpatch(sc->devs[dev], &sysex, uio));
}

/* Convert an old sequencer event to a new one. */
int
seq_to_new(seq_event_rec *ev, struct uio *uio)
{
	int cmd, chan, note, parm;
	u_int32_t delay;
	int error;

	cmd = SEQ_CMD(ev);
	chan = ev->arr[1];
	note = ev->arr[2];
	parm = ev->arr[3];
	DPRINTFN(3, ("seq_to_new: 0x%02x %d %d %d\n", cmd, chan, note, parm));

	if (cmd >= 0x80) {
		/* Fill the event record */
		if (uio->uio_resid >= sizeof *ev - SEQOLD_CMDSIZE) {
			error = uiomove(&ev->arr[SEQOLD_CMDSIZE],
					sizeof *ev - SEQOLD_CMDSIZE, uio);
			if (error)
				return (error);
		} else
			return (EINVAL);
	}

	switch(cmd) {
	case SEQOLD_NOTEOFF:
		note = 255;
		SEQ_ECMD(ev) = MIDI_NOTEOFF;
		goto onoff;
	case SEQOLD_NOTEON:
		SEQ_ECMD(ev) = MIDI_NOTEON;
	onoff:
		SEQ_CMD(ev) = SEQ_CHN_VOICE;
		SEQ_EDEV(ev) = 0;
		SEQ_ECHAN(ev) = chan;
		SEQ_ENOTE(ev) = note;
		SEQ_EPARM(ev) = parm;
		break;
	case SEQOLD_WAIT:
		delay = *(u_int32_t *)ev->arr >> 8;
		SEQ_CMD(ev) = SEQ_TIMING;
		SEQ_TCMD(ev) = TMR_WAIT_REL;
		*(u_int32_t *)&ev->arr[4] = delay;
		break;
	case SEQOLD_SYNCTIMER:
		SEQ_CMD(ev) = SEQ_TIMING;
		SEQ_TCMD(ev) = TMR_RESET;
		break;
	case SEQOLD_PGMCHANGE:
		SEQ_ECMD(ev) = MIDI_PGM_CHANGE;
		SEQ_CMD(ev) = SEQ_CHN_COMMON;
		SEQ_EDEV(ev) = 0;
		SEQ_ECHAN(ev) = chan;
		SEQ_EP1(ev) = note;
		break;
	case SEQOLD_MIDIPUTC:
		break;		/* interpret in normal mode */
	case SEQOLD_ECHO:
	case SEQOLD_PRIVATE:
	case SEQOLD_EXTENDED:
	default:
		DPRINTF(("seq_to_new: not impl 0x%02x\n", cmd));
		return (EINVAL);
	/* In case new events show up */
	case SEQ_TIMING:
	case SEQ_CHN_VOICE:
	case SEQ_CHN_COMMON:
	case SEQ_FULLSIZE:
		break;
	}
	return (0);
}

/**********************************************/

void
midiseq_in(struct midi_dev *md, u_char *msg, int len)
{
	int unit = md->unit;
	seq_event_rec ev;
	int status, chan;

	DPRINTFN(2, ("midiseq_in: %p %02x %02x %02x\n",
		     md, msg[0], msg[1], msg[2]));

	status = MIDI_GET_STATUS(msg[0]);
	chan = MIDI_GET_CHAN(msg[0]);
	switch (status) {
	case MIDI_NOTEON:
		if (msg[2] == 0) {
			status = MIDI_NOTEOFF;
			msg[2] = MIDI_HALF_VEL;
		}
		/* FALLTHROUGH */
	case MIDI_NOTEOFF:
	case MIDI_KEY_PRESSURE:
		SEQ_MK_CHN_VOICE(&ev, unit, status, chan, msg[1], msg[2]);
		break;
	case MIDI_CTL_CHANGE:
		SEQ_MK_CHN_COMMON(&ev, unit, status, chan, msg[1], 0, msg[2]);
		break;
	case MIDI_PGM_CHANGE:
	case MIDI_CHN_PRESSURE:
		SEQ_MK_CHN_COMMON(&ev, unit, status, chan, msg[1], 0, 0);
		break;
	case MIDI_PITCH_BEND:
		SEQ_MK_CHN_COMMON(&ev, unit, status, chan, 0, 0,
				  (msg[1] & 0x7f) | ((msg[2] & 0x7f) << 7));
		break;
	default:
		return;
	}
	seq_event_intr(md->seq, &ev);
}

struct midi_dev *
midiseq_open(int unit, int flags)
{
	extern struct cfdriver midi_cd;
	int error;
	struct midi_dev *md;
	struct midi_softc *sc;
	struct midi_info mi;

	DPRINTFN(2, ("midiseq_open: %d %d\n", unit, flags));
	error = midiopen(makedev(0, unit), flags, 0, 0);
	if (error)
		return (0);
	sc = midi_cd.cd_devs[unit];
	sc->seqopen = 1;
	md = malloc(sizeof *md, M_DEVBUF, M_WAITOK | M_ZERO);
	sc->seq_md = md;
	md->msc = sc;
	midi_getinfo(makedev(0, unit), &mi);
	md->unit = unit;
	md->name = mi.name;
	md->subtype = 0;
	md->nr_voices = 128;	/* XXX */
	md->instr_bank_size = 128; /* XXX */
	if (mi.props & MIDI_PROP_CAN_INPUT)
		md->capabilities |= SYNTH_CAP_INPUT;
	return (md);
}

void
midiseq_close(struct midi_dev *md)
{
	DPRINTFN(2, ("midiseq_close: %d\n", md->unit));
	midiclose(makedev(0, md->unit), 0, 0, 0);
	free(md, M_DEVBUF);
}

void
midiseq_reset(struct midi_dev *md)
{
	/* XXX send GM reset? */
	DPRINTFN(3, ("midiseq_reset: %d\n", md->unit));
}

int
midiseq_out(struct midi_dev *md, u_char *buf, u_int cc, int chk)
{
	DPRINTFN(5, ("midiseq_out: m=%p, unit=%d, buf[0]=0x%02x, cc=%d\n",
		     md->msc, md->unit, buf[0], cc));

	/* The MIDI "status" byte does not have to be repeated. */
	if (chk && md->last_cmd == buf[0])
		buf++, cc--;
	else
		md->last_cmd = buf[0];
	return (midi_writebytes(md->unit, buf, cc));
}

int
midiseq_noteon(struct midi_dev *md, int chan, int note, int vel)
{
	u_char buf[3];

	DPRINTFN(6, ("midiseq_noteon 0x%02x %d %d\n",
		     MIDI_NOTEON | chan, note, vel));
	if (chan < 0 || chan > 15 ||
	    note < 0 || note > 127)
		return (EINVAL);
	if (vel < 0) vel = 0;
	if (vel > 127) vel = 127;
	buf[0] = MIDI_NOTEON | chan;
	buf[1] = note;
	buf[2] = vel;
	return (midiseq_out(md, buf, 3, 1));
}

int
midiseq_noteoff(struct midi_dev *md, int chan, int note, int vel)
{
	u_char buf[3];

	if (chan < 0 || chan > 15 ||
	    note < 0 || note > 127)
		return (EINVAL);
	if (vel < 0) vel = 0;
	if (vel > 127) vel = 127;
	buf[0] = MIDI_NOTEOFF | chan;
	buf[1] = note;
	buf[2] = vel;
	return (midiseq_out(md, buf, 3, 1));
}

int
midiseq_keypressure(struct midi_dev *md, int chan, int note, int vel)
{
	u_char buf[3];

	if (chan < 0 || chan > 15 ||
	    note < 0 || note > 127)
		return (EINVAL);
	if (vel < 0) vel = 0;
	if (vel > 127) vel = 127;
	buf[0] = MIDI_KEY_PRESSURE | chan;
	buf[1] = note;
	buf[2] = vel;
	return (midiseq_out(md, buf, 3, 1));
}

int
midiseq_pgmchange(struct midi_dev *md, int chan, int parm)
{
	u_char buf[2];

	if (chan < 0 || chan > 15 ||
	    parm < 0 || parm > 127)
		return (EINVAL);
	buf[0] = MIDI_PGM_CHANGE | chan;
	buf[1] = parm;
	return (midiseq_out(md, buf, 2, 1));
}

int
midiseq_chnpressure(struct midi_dev *md, int chan, int parm)
{
	u_char buf[2];

	if (chan < 0 || chan > 15 ||
	    parm < 0 || parm > 127)
		return (EINVAL);
	buf[0] = MIDI_CHN_PRESSURE | chan;
	buf[1] = parm;
	return (midiseq_out(md, buf, 2, 1));
}

int
midiseq_ctlchange(struct midi_dev *md, int chan, int parm, int w14)
{
	u_char buf[3];

	if (chan < 0 || chan > 15 ||
	    parm < 0 || parm > 127)
		return (EINVAL);
	buf[0] = MIDI_CTL_CHANGE | chan;
	buf[1] = parm;
	buf[2] = w14 & 0x7f;
	return (midiseq_out(md, buf, 3, 1));
}

int
midiseq_pitchbend(struct midi_dev *md, int chan, int parm)
{
	u_char buf[3];

	if (chan < 0 || chan > 15)
		return (EINVAL);
	buf[0] = MIDI_PITCH_BEND | chan;
	buf[1] = parm & 0x7f;
	buf[2] = (parm >> 7) & 0x7f;
	return (midiseq_out(md, buf, 3, 1));
}

int
midiseq_loadpatch(struct midi_dev *md, struct sysex_info *sysex, struct uio *uio)
{
	u_char c, buf[128];
	int i, cc, error;

	if (sysex->key != SEQ_SYSEX_PATCH) {
		DPRINTF(("midiseq_loadpatch: bad patch key 0x%04x\n",
			     sysex->key));
		return (EINVAL);
	}
	if (uio->uio_resid < sysex->len)
		/* adjust length, should be an error */
		sysex->len = uio->uio_resid;

	DPRINTFN(2, ("midiseq_loadpatch: len=%d\n", sysex->len));
	if (sysex->len == 0)
		return (EINVAL);
	error = uiomove(&c, 1, uio);
	if (error)
		return error;
	if (c != MIDI_SYSEX_START)		/* must start like this */
		return (EINVAL);
	error = midiseq_out(md, &c, 1, 0);
	if (error)
		return (error);
	--sysex->len;
	while (sysex->len > 0) {
		cc = sysex->len;
		if (cc > sizeof buf)
			cc = sizeof buf;
		error = uiomove(buf, cc, uio);
		if (error)
			break;
		for(i = 0; i < cc && !MIDI_IS_STATUS(buf[i]); i++)
			;
		error = midiseq_out(md, buf, i, 0);
		if (error)
			break;
		sysex->len -= i;
		if (i != cc)
			break;
	}
	/* Any leftover data in uio is rubbish;
	 * the SYSEX should be one write ending in SYSEX_END.
	 */
	uio->uio_resid = 0;
	c = MIDI_SYSEX_END;
	return (midiseq_out(md, &c, 1, 0));
}

int
midiseq_putc(struct midi_dev *md, int data)
{
	u_char c = data;
	DPRINTFN(4,("midiseq_putc: 0x%02x\n", data));
	return (midiseq_out(md, &c, 1, 0));
}

#include "midi.h"
#if NMIDI == 0
/*
 * If someone has a sequencer, but no midi devices there will
 * be unresolved references, so we provide little stubs.
 */

int
midi_unit_count()
{
	return (0);
}

int
midiopen(dev_t dev, int flags, int ifmt, struct proc *p)
{
	return (ENXIO);
}

struct cfdriver midi_cd;

void
midi_getinfo(dev_t dev, struct midi_info *mi)
{
}

int
midiclose(dev_t dev, int flags, int ifmt, struct proc *p)
{
	return (ENXIO);
}

int
midi_writebytes(int unit, u_char *buf, int cc)
{
	return (ENXIO);
}
#endif /* NMIDI == 0 */

#endif /* NSEQUENCER > 0 */

