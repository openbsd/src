/*	$OpenBSD: midisyn.c,v 1.5 2005/11/21 18:16:38 millert Exp $	*/
/*	$NetBSD: midisyn.c,v 1.5 1998/11/25 22:17:07 augustss Exp $	*/

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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/audioio.h>
#include <sys/midiio.h>
#include <sys/device.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/midivar.h>
#include <dev/midisynvar.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (midisyndebug) printf x
#define DPRINTFN(n,x)	if (midisyndebug >= (n)) printf x
int	midisyndebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

int	midisyn_findvoice(midisyn *, int, int);
void	midisyn_freevoice(midisyn *, int);
int	midisyn_allocvoice(midisyn *, u_int32_t, u_int32_t);
u_int32_t midisyn_note_to_freq(int);
u_int32_t midisyn_finetune(u_int32_t, int, int, int);

int	midisyn_open(void *, int, 
		     void (*iintr)(void *, int),
		     void (*ointr)(void *), void *arg);
void	midisyn_close(void *);
int	midisyn_output(void *, int);
void	midisyn_getinfo(void *, struct midi_info *);
int	midisyn_ioctl(void *, u_long, caddr_t, int, struct proc *);

struct midi_hw_if midisyn_hw_if = {
	midisyn_open,
	midisyn_close,
	midisyn_output,
	midisyn_getinfo,
	midisyn_ioctl,
};

static int midi_lengths[] = { 3,3,3,3,2,2,3,1 };
/* Number of bytes in a MIDI command, including status */
#define MIDI_LENGTH(d) (midi_lengths[((d) >> 4) & 7])

int
midisyn_open(addr, flags, iintr, ointr, arg)
	void *addr;
	int flags;
	void (*iintr)(void *, int);
	void (*ointr)(void *);
	void *arg;
{
	midisyn *ms = addr;

	DPRINTF(("midisyn_open: ms=%p ms->mets=%p\n", ms, ms->mets));
	if (ms->mets->open)
		return (ms->mets->open(ms, flags));
	else
		return (0);
}

void
midisyn_close(addr)
	void *addr;
{
	midisyn *ms = addr;
	struct midisyn_methods *fs;
	int v;

	DPRINTF(("midisyn_close: ms=%p ms->mets=%p\n", ms, ms->mets));
	fs = ms->mets;
	for (v = 0; v < ms->nvoice; v++)
		if (ms->voices[v].inuse) {
			fs->noteoff(ms, v, 0, 0);
			midisyn_freevoice(ms, v);
		}
	if (fs->close)
		fs->close(ms);
}

void
midisyn_getinfo(addr, mi)
	void *addr;
	struct midi_info *mi;
{
	midisyn *ms = addr;

	mi->name = ms->name;
	mi->props = 0;
}

int
midisyn_ioctl(maddr, cmd, addr, flag, p)
	void *maddr;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	midisyn *ms = maddr;

	if (ms->mets->ioctl)
		return (ms->mets->ioctl(ms, cmd, addr, flag, p));
	else
		return (EINVAL);
}

int
midisyn_findvoice(ms, chan, note)
	midisyn *ms;
	int chan, note;
{
	u_int cn;
	int v;

	if (!(ms->flags & MS_DOALLOC))
		return (chan);
	cn = MS_CHANNOTE(chan, note);
	for (v = 0; v < ms->nvoice; v++)
		if (ms->voices[v].chan_note == cn && ms->voices[v].inuse)
			return (v);
	return (-1);
}

void
midisyn_attach(sc, ms)
	struct midi_softc *sc;
	midisyn *ms;
{
	if (ms->flags & MS_DOALLOC) {
		ms->voices = malloc(ms->nvoice * sizeof (struct voice), 
				    M_DEVBUF, M_WAITOK);
		memset(ms->voices, 0, ms->nvoice * sizeof (struct voice));
		ms->seqno = 1;
		if (ms->mets->allocv == 0)
			ms->mets->allocv = &midisyn_allocvoice;
	}
	sc->hw_if = &midisyn_hw_if;
	sc->hw_hdl = ms;
	DPRINTF(("midisyn_attach: ms=%p\n", sc->hw_hdl));
}

void
midisyn_freevoice(ms, voice)
	midisyn *ms;
	int voice;
{
	if (!(ms->flags & MS_DOALLOC))
		return;
	ms->voices[voice].inuse = 0;
}

int
midisyn_allocvoice(ms, chan, note)
	midisyn *ms;
	u_int32_t chan, note;
{
	int bestv, v;
	u_int bestseq, s;

	if (!(ms->flags & MS_DOALLOC))
		return (chan);
	/* Find a free voice, or if no free voice is found the oldest. */
	bestv = 0;
	bestseq = ms->voices[0].seqno + (ms->voices[0].inuse ? 0x40000000 : 0);
	for (v = 1; v < ms->nvoice; v++) {
		s = ms->voices[v].seqno;
		if (ms->voices[v].inuse)
			s += 0x40000000;
		if (s < bestseq) {
			bestseq = s;
			bestv = v;
		}
	}
	DPRINTFN(10,("midisyn_allocvoice: v=%d seq=%d cn=%x inuse=%d\n",
		     bestv, ms->voices[bestv].seqno, 
		     ms->voices[bestv].chan_note,
		     ms->voices[bestv].inuse));
#ifdef AUDIO_DEBUG
	if (ms->voices[bestv].inuse)
		DPRINTFN(1,("midisyn_allocvoice: steal %x\n", 
			    ms->voices[bestv].chan_note));
#endif
	ms->voices[bestv].chan_note = MS_CHANNOTE(chan, note);
	ms->voices[bestv].seqno = ms->seqno++;
	ms->voices[bestv].inuse = 1;
	return (bestv);
}

int
midisyn_output(addr, b)
	void *addr;
	int b;
{
	midisyn *ms = addr;
	u_int8_t status, chan;
	int voice = 0;		/* initialize to keep gcc quiet */
	struct midisyn_methods *fs;
	u_int32_t note, vel;

	DPRINTF(("midisyn_output: ms=%p b=0x%02x\n", ms, b));
	fs = ms->mets;
	if (ms->pos < 0) {
		/* Doing SYSEX */
		DPRINTF(("midisyn_output: sysex 0x%02x\n", b));
		if (fs->sysex)
			fs->sysex(ms, b);
		if (b == MIDI_SYSEX_END)
			ms->pos = 0;
		return (0);
	}
	if (ms->pos == 0 && !MIDI_IS_STATUS(b))
		ms->pos++;	/* repeat last status byte */
	ms->buf[ms->pos++] = b;
	status = ms->buf[0];
	if (ms->pos < MIDI_LENGTH(status))
		return (0);
	/* Decode the MIDI command */
	chan = MIDI_GET_CHAN(status);
	note = ms->buf[1];
	if (ms->flags & MS_FREQXLATE)
		note = midisyn_note_to_freq(note);
	vel = ms->buf[2];
	switch (MIDI_GET_STATUS(status)) {
	case MIDI_NOTEOFF:
		voice = midisyn_findvoice(ms, chan, ms->buf[1]);
		if (voice >= 0) {
			fs->noteoff(ms, voice, note, vel);
			midisyn_freevoice(ms, voice);
		}
		break;
	case MIDI_NOTEON:
		voice = fs->allocv(ms, chan, ms->buf[1]);
		fs->noteon(ms, voice, note, vel);
		break;
	case MIDI_KEY_PRESSURE:
		if (fs->keypres) {
			voice = midisyn_findvoice(ms, voice, ms->buf[1]);
			if (voice >= 0)
				fs->keypres(ms, voice, note, vel);
		}
		break;
	case MIDI_CTL_CHANGE:
		if (fs->ctlchg)
			fs->ctlchg(ms, chan, ms->buf[1], vel);
		break;
	case MIDI_PGM_CHANGE:
		if (fs->pgmchg)
			fs->pgmchg(ms, chan, ms->buf[1]);
		break;
	case MIDI_CHN_PRESSURE:
		if (fs->chnpres) {
			voice = midisyn_findvoice(ms, chan, ms->buf[1]);
			if (voice >= 0)
				fs->chnpres(ms, voice, note);
		}
		break;
	case MIDI_PITCH_BEND:
		if (fs->pitchb) {
			voice = midisyn_findvoice(ms, chan, ms->buf[1]);
			if (voice >= 0)
				fs->pitchb(ms, chan, note, vel);
		}
		break;
	case MIDI_SYSTEM_PREFIX:
		if (fs->sysex)
			fs->sysex(ms, status);
		ms->pos = -1;
		return (0);
	}
	ms->pos = 0;
	return (0);
}

/*
 * Convert a MIDI note to the corresponding frequency.
 * The frequency is scaled by 2^16.
 */
u_int32_t
midisyn_note_to_freq(note)
	int note;
{
	int o, n, f;
#define BASE_OCTAVE 5
	static u_int32_t notes[] = {
		17145893, 18165441, 19245614, 20390018, 21602472, 22887021,
		24247954, 25689813, 27217409, 28835840, 30550508, 32367136
	};


	o = note / 12;
	n = note % 12;

	f = notes[n];

	if (o < BASE_OCTAVE)
		f >>= (BASE_OCTAVE - o);
	else if (o > BASE_OCTAVE)
		f <<= (o - BASE_OCTAVE);
	return (f);
}

u_int32_t
midisyn_finetune(base_freq, bend, range, vibrato_cents)
	u_int32_t base_freq;
	int bend;
	int range;
	int vibrato_cents;
{
	static u_int16_t semitone_tuning[24] = 
	{
/*   0 */ 10000, 10595, 11225, 11892, 12599, 13348, 14142, 14983, 
/*   8 */ 15874, 16818, 17818, 18877, 20000, 21189, 22449, 23784, 
/*  16 */ 25198, 26697, 28284, 29966, 31748, 33636, 35636, 37755
	};
	static u_int16_t cent_tuning[100] =
	{
/*   0 */ 10000, 10006, 10012, 10017, 10023, 10029, 10035, 10041, 
/*   8 */ 10046, 10052, 10058, 10064, 10070, 10075, 10081, 10087, 
/*  16 */ 10093, 10099, 10105, 10110, 10116, 10122, 10128, 10134, 
/*  24 */ 10140, 10145, 10151, 10157, 10163, 10169, 10175, 10181, 
/*  32 */ 10187, 10192, 10198, 10204, 10210, 10216, 10222, 10228, 
/*  40 */ 10234, 10240, 10246, 10251, 10257, 10263, 10269, 10275, 
/*  48 */ 10281, 10287, 10293, 10299, 10305, 10311, 10317, 10323, 
/*  56 */ 10329, 10335, 10341, 10347, 10353, 10359, 10365, 10371, 
/*  64 */ 10377, 10383, 10389, 10395, 10401, 10407, 10413, 10419, 
/*  72 */ 10425, 10431, 10437, 10443, 10449, 10455, 10461, 10467, 
/*  80 */ 10473, 10479, 10485, 10491, 10497, 10503, 10509, 10515, 
/*  88 */ 10521, 10528, 10534, 10540, 10546, 10552, 10558, 10564, 
/*  96 */ 10570, 10576, 10582, 10589
	};
	u_int32_t amount;
	int negative, semitones, cents, multiplier;

	if (range == 0)
		return base_freq;

	if (base_freq == 0)
		return base_freq;

	if (range >= 8192)
		range = 8192;

	bend = bend * range / 8192;
	bend += vibrato_cents;

	if (bend == 0)
		return base_freq;

	if (bend < 0) {
		bend = -bend;
		negative = 1;
	} else 
		negative = 0;

	if (bend > range)
		bend = range;

	multiplier = 1;
	while (bend > 2399) {
		multiplier *= 4;
		bend -= 2400;
	}

	semitones = bend / 100;
	if (semitones > 23)
		semitones = 23;
	cents = bend % 100;

	amount = semitone_tuning[semitones] * multiplier * cent_tuning[cents]
		/ 10000;

	if (negative)
		return (base_freq * 10000 / amount);	/* Bend down */
	else
		return (base_freq * amount / 10000);	/* Bend up */
}

