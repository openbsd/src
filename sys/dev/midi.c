/*	$OpenBSD: midi.c,v 1.1 1999/01/02 00:02:32 niklas Exp $	*/
/*	$NetBSD: midi.c,v 1.10 1998/12/20 14:26:44 drochner Exp $	*/

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

#include "midi.h"
#include "sequencer.h"

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/select.h>
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

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/midivar.h>

#if NMIDI > 0

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (mididebug) printf x
#define DPRINTFN(n,x)	if (mididebug >= (n)) printf x
int	mididebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

int midi_wait;

void	midi_in __P((void *, int));
void	midi_out __P((void *));
int	midi_start_output __P((struct midi_softc *, int));
int	midi_sleep_timo __P((int *, char *, int));
int	midi_sleep __P((int *, char *));
void	midi_wakeup __P((int *));
void	midi_initbuf __P((struct midi_buffer *));
void	midi_timeout __P((void *));

#define __BROKEN_INDIRECT_CONFIG /* XXX */
#ifdef __BROKEN_INDIRECT_CONFIG
int	midiprobe __P((struct device *, void *, void *));
#else
int	midiprobe __P((struct device *, struct cfdata *, void *));
#endif
void	midiattach __P((struct device *, struct device *, void *));

struct cfattach midi_ca = {
	sizeof(struct midi_softc), midiprobe, midiattach
};

struct cfdriver midi_cd = {
	NULL, "midi", DV_DULL
};

#ifdef MIDI_SAVE
#define MIDI_SAVE_SIZE 100000
int midicnt;
struct {
	int cnt;
	u_char buf[MIDI_SAVE_SIZE];
} midisave;
#define MIDI_GETSAVE		_IOWR('m', 100, int)

#endif

extern struct cfdriver midi_cd;

int
midiprobe(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct audio_attach_args *sa = aux;

	DPRINTFN(6,("midiprobe: type=%d sa=%p hw=%p\n", 
		 sa->type, sa, sa->hwif));
	return ((sa->type == AUDIODEV_TYPE_MIDI) ? 1 : 0);
}

void
midiattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct midi_softc *sc = (void *)self;
	struct audio_attach_args *sa = aux;
	struct midi_hw_if *hwp = sa->hwif;
	void *hdlp = sa->hdl;

	DPRINTFN(6, ("MIDI attach\n"));

#ifdef DIAGNOSTIC
	if (hwp == 0 ||
	    hwp->open == 0 ||
	    hwp->close == 0 ||
	    hwp->output == 0 ||
	    hwp->getinfo == 0) {
		printf("midi: missing method\n");
		return;
	}
#endif
	sc->hw_if = hwp;
	sc->hw_hdl = hdlp;
	midi_attach(sc, parent);
}

void
midi_attach(sc, parent)
	struct midi_softc *sc;
	struct device *parent;
{
	struct midi_info mi;

	sc->isopen = 0;

	midi_wait = MIDI_WAIT * hz / 1000000;
	if (midi_wait == 0)
		midi_wait = 1;

	sc->sc_dev = parent;
	sc->hw_if->getinfo(sc->hw_hdl, &mi);
	sc->props = mi.props;
	printf(": <%s>\n", mi.name);
}

int
midi_unit_count()
{
	return (midi_cd.cd_ndevs);
}

void
midi_initbuf(mb)
	struct midi_buffer *mb;
{
	mb->used = 0;
	mb->usedhigh = MIDI_BUFSIZE;
	mb->end = mb->start + mb->usedhigh;
	mb->inp = mb->outp = mb->start;
}

int
midi_sleep_timo(chan, label, timo)
	int *chan;
	char *label;
	int timo;
{
	int st;

	if (!label)
		label = "midi";

	DPRINTFN(5, ("midi_sleep_timo: %p %s %d\n", chan, label, timo));
	*chan = 1;
	st = tsleep(chan, PWAIT | PCATCH, label, timo);
	*chan = 0;
#ifdef MIDI_DEBUG
	if (st != 0)
		printf("midi_sleep: %d\n", st);
#endif
	return (st);
}

int
midi_sleep(chan, label)
	int *chan;
	char *label;
{
	return (midi_sleep_timo(chan, label, 0));
}

void
midi_wakeup(chan)
	int *chan;
{
	if (*chan) {
		DPRINTFN(5, ("midi_wakeup: %p\n", chan));
		wakeup(chan);
		*chan = 0;
	}
}

static int midi_lengths[] = { 2,2,2,2,1,1,2,0 };
/* Number of bytes in a MIDI command */
#define MIDI_LENGTH(d) (midi_lengths[((d) >> 4) & 7])

void
midi_in(addr, data)
	void *addr;
	int data;
{
	struct midi_softc *sc = addr;
	struct midi_buffer *mb = &sc->inbuf;
	int i;

	if (!sc->isopen)
		return;
	if (data == MIDI_ACK)
		return;
	DPRINTFN(3, ("midi_in: %p 0x%02x\n", sc, data));
	if (!(sc->flags & FREAD))
		return;		/* discard data if not reading */

	switch(sc->in_state) {
	case MIDI_IN_START:
		if (MIDI_IS_STATUS(data)) {
			switch(data) {
			case 0xf0: /* Sysex */
				sc->in_state = MIDI_IN_SYSEX;
				break;
			case 0xf1: /* MTC quarter frame */
			case 0xf3: /* Song select */
				sc->in_state = MIDI_IN_DATA;
				sc->in_msg[0] = data;
				sc->in_pos = 1;
				sc->in_left = 1;
				break;
			case 0xf2: /* Song position pointer */
				sc->in_state = MIDI_IN_DATA;
				sc->in_msg[0] = data;
				sc->in_pos = 1;
				sc->in_left = 2;
				break;
			default:
				if (MIDI_IS_COMMON(data)) {
					sc->in_msg[0] = data;
					sc->in_pos = 1;
					goto deliver;
				} else {
					sc->in_state = MIDI_IN_DATA;
					sc->in_msg[0] = sc->in_status = data;
					sc->in_pos = 1;
					sc->in_left = 
						MIDI_LENGTH(sc->in_status);
				}
				break;
			}
		} else {
			if (MIDI_IS_STATUS(sc->in_status)) {
				sc->in_state = MIDI_IN_DATA;
				sc->in_msg[0] = sc->in_status;
				sc->in_msg[1] = data;
				sc->in_pos = 2;
				sc->in_left = MIDI_LENGTH(sc->in_status) - 1;
			}
		}
		return;
	case MIDI_IN_DATA:
		sc->in_msg[sc->in_pos++] = data;
		if (--sc->in_left <= 0)
			break;	/* deliver data */
		return;
	case MIDI_IN_SYSEX:
		if (data == MIDI_SYSEX_END)
			sc->in_state = MIDI_IN_START;
		return;
	}
deliver:
	sc->in_state = MIDI_IN_START;
#if NSEQUENCER > 0
	if (sc->seqopen) {
		extern void midiseq_in __P((struct midi_dev *,u_char *,int));
		midiseq_in(sc->seq_md, sc->in_msg, sc->in_pos);
		return;
	}
#endif

	if (mb->used + sc->in_pos > mb->usedhigh) {
		DPRINTF(("midi_in: buffer full, discard data=0x%02x\n", 
			 sc->in_msg[0]));
		return;
	}
	for (i = 0; i < sc->in_pos; i++) {
		*mb->inp++ = sc->in_msg[i];
		if (mb->inp >= mb->end)
			mb->inp = mb->start;
		mb->used++;
	}
	midi_wakeup(&sc->rchan);
	selwakeup(&sc->rsel);
	if (sc->async)
		psignal(sc->async, SIGIO);
}

void
midi_out(addr)
	void *addr;
{
	struct midi_softc *sc = addr;

	if (!sc->isopen)
		return;
	DPRINTFN(3, ("midi_out: %p\n", sc));
	midi_start_output(sc, 1);
}

int
midiopen(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc;
	struct midi_hw_if *hw;
	int error;

	if (unit >= midi_cd.cd_ndevs ||
	    (sc = midi_cd.cd_devs[unit]) == NULL)
		return (ENXIO);
	DPRINTF(("midiopen %p\n", sc));

	hw = sc->hw_if;
	if (!hw)
		return (ENXIO);
	if (sc->isopen)
		return (EBUSY);
	sc->in_state = MIDI_IN_START;
	sc->in_status = 0;
	error = hw->open(sc->hw_hdl, flags, midi_in, midi_out, sc);
	if (error)
		return (error);
	sc->isopen++;
	midi_initbuf(&sc->outbuf);
	midi_initbuf(&sc->inbuf);
	sc->flags = flags;
	sc->rchan = 0;
	sc->wchan = 0;
	sc->pbus = 0;
	sc->async = 0;

#ifdef MIDI_SAVE
	if (midicnt != 0) {
		midisave.cnt = midicnt;
		midicnt = 0;
	}
#endif

	return (0);
}

int
midiclose(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_hw_if *hw = sc->hw_if;
	int s, error;

	DPRINTF(("midiclose %p\n", sc));

	midi_start_output(sc, 0);
	error = 0;
	s = splaudio();
	while (sc->outbuf.used > 0 && !error) {
		DPRINTFN(2,("midiclose sleep used=%d\n", sc->outbuf.used));
		error = midi_sleep_timo(&sc->wchan, "mid_dr", 30*hz);
	}
	splx(s);
	sc->isopen = 0;
	hw->close(sc->hw_hdl);
#if NSEQUENCER > 0
	sc->seqopen = 0;
	sc->seq_md = 0;
#endif
	return (0);
}

int
midiread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_buffer *mb = &sc->inbuf;
	int error;
	u_char *outp;
	int used, cc, n, resid;
	int s;

	DPRINTF(("midiread: %p, count=%d\n", sc, uio->uio_resid));

	error = 0;
	resid = uio->uio_resid;
	while (uio->uio_resid == resid && !error) {
		s = splaudio();
		while (mb->used <= 0) {
			if (ioflag & IO_NDELAY) {
				splx(s);
				return (EWOULDBLOCK);
			}
			error = midi_sleep(&sc->rchan, "mid rd");
			if (error) {
				splx(s);
				return (error);
			}
		}
		used = mb->used;
		outp = mb->outp;
		splx(s);
		cc = used;	/* maximum to read */
		n = mb->end - outp;
		if (n < cc)
			cc = n;	/* don't read beyond end of buffer */
		if (uio->uio_resid < cc)
			cc = uio->uio_resid; /* and no more than we want */
		DPRINTFN(3, ("midiread: uiomove cc=%d\n", cc));
		error = uiomove(outp, cc, uio);
		if (error)
			break;
		used -= cc;
		outp += cc;
		if (outp >= mb->end)
			outp = mb->start;
		s = splaudio();
		mb->outp = outp;
		mb->used = used;
		splx(s);
	}
	return (error);
}

void
midi_timeout(arg)
	void *arg;
{
	struct midi_softc *sc = arg;

	DPRINTFN(3,("midi_timeout: %p\n", sc));
	midi_start_output(sc, 1);
}

int
midi_start_output(sc, intr)
	struct midi_softc *sc;
	int intr;
{
	struct midi_buffer *mb = &sc->outbuf;
	u_char *outp;
	int error;
	int s;
	int i, mmax;

	error = 0;
	mmax = sc->props & MIDI_PROP_OUT_INTR ? 1 : MIDI_MAX_WRITE;
	s = splaudio();
	if (sc->pbus && !intr) {
		DPRINTFN(4, ("midi_start_output: busy\n"));
		splx(s);
		return (0);
	}
	sc->pbus = 1;
	for (i = 0; i < mmax && mb->used > 0 && !error; i++) {
		outp = mb->outp;
		splx(s);
		DPRINTFN(4, ("midi_start_output: %p i=%d, data=0x%02x\n", 
			     sc, i, *outp));
#ifdef MIDI_SAVE
		midisave.buf[midicnt] = *outp;
		midicnt = (midicnt + 1) % MIDI_SAVE_SIZE;
#endif
		error = sc->hw_if->output(sc->hw_hdl, *outp++);
		if (outp >= mb->end)
			outp = mb->start;
		s = splaudio();
		mb->outp = outp;
		mb->used--;
	}
	midi_wakeup(&sc->wchan);
	selwakeup(&sc->wsel);
	if (sc->async)
		psignal(sc->async, SIGIO);
	if (mb->used > 0) {
		if (!(sc->props & MIDI_PROP_OUT_INTR))
			timeout(midi_timeout, sc, midi_wait);
	} else
		sc->pbus = 0;
	splx(s);
	return (error);
}

int
midiwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_buffer *mb = &sc->outbuf;
	int error;
	u_char *inp;
	int used, cc, n;
	int s;

	DPRINTFN(2, ("midiwrite: %p, unit=%d, count=%d\n", sc, unit, 
		     uio->uio_resid));

	error = 0;
	while (uio->uio_resid > 0 && !error) {
		s = splaudio();
		if (mb->used >= mb->usedhigh) {
			DPRINTFN(3,("midi_write: sleep used=%d hiwat=%d\n", 
				 mb->used, mb->usedhigh));
			if (ioflag & IO_NDELAY) {
				splx(s);
				return (EWOULDBLOCK);
			}
			error = midi_sleep(&sc->wchan, "mid wr");
			if (error) {
				splx(s);
				return (error);
			}
		}			
		used = mb->used;
		inp = mb->inp;
		splx(s);
		cc = mb->usedhigh - used; 	/* maximum to write */
		n = mb->end - inp;
		if (n < cc)
			cc = n;		/* don't write beyond end of buffer */
		if (uio->uio_resid < cc)
			cc = uio->uio_resid; 	/* and no more than we have */
		error = uiomove(inp, cc, uio);
#ifdef MIDI_DEBUG
		if (error)
		        printf("midi_write:(1) uiomove failed %d; "
			       "cc=%d inp=%p\n",
			       error, cc, inp);
#endif
		if (error)
			break;
		inp = mb->inp + cc;
		if (inp >= mb->end)
			inp = mb->start;
		s = splaudio();
		mb->inp = inp;
		mb->used += cc;
		splx(s);
		error = midi_start_output(sc, 0);
	}
	return (error);
}

/*
 * This write routine is only called from sequencer code and expects
 * a write that is smaller than the MIDI buffer.
 */
int
midi_writebytes(unit, buf, cc)
	int unit;
	u_char *buf;
	int cc;
{
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_buffer *mb = &sc->outbuf;
	int n, s;

	DPRINTFN(2, ("midi_writebytes: %p, unit=%d, cc=%d\n", sc, unit, cc));
	DPRINTFN(3, ("midi_writebytes: %x %x %x\n",buf[0],buf[1],buf[2]));

	s = splaudio();
	if (mb->used + cc >= mb->usedhigh) {
		splx(s);
		return (EWOULDBLOCK);
	}
	n = mb->end - mb->inp;
	if (cc < n)
		n = cc;
	mb->used += cc;
	bcopy(buf, mb->inp, n);
	mb->inp += n;
	if (mb->inp >= mb->end) {
		mb->inp = mb->start;
		cc -= n;
		if (cc > 0) {
			bcopy(buf + n, mb->inp, cc);
			mb->inp += cc;
		}
	}
	splx(s);
	return (midi_start_output(sc, 0));
}

int
midiioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	struct midi_hw_if *hw = sc->hw_if;
	int error;

	DPRINTF(("midiioctl: %p cmd=0x%08lx\n", sc, cmd));
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
			DPRINTF(("midi_ioctl: FIOASYNC %p\n", p));
		} else
			sc->async = 0;
		break;

#if 0
	case MIDI_PRETIME:
		/* XXX OSS
		 * This should set up a read timeout, but that's
		 * why we have poll(), so there's nothing yet. */
		error = EINVAL;
		break;
#endif

#ifdef MIDI_SAVE
	case MIDI_GETSAVE:
		error = copyout(&midisave, *(void **)addr, sizeof midisave);
  		break;
#endif

	default:
		if (hw->ioctl)
			error = hw->ioctl(sc->hw_hdl, cmd, addr, flag, p);
		else
			error = EINVAL;
		break;
	}
	return (error);
}

int
midiselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc = midi_cd.cd_devs[unit];
	int s = splaudio();

	DPRINTF(("midiselect: %p rw=0x%x\n", sc, rw));

	switch (rw) {
	case FREAD:
		if (sc->inbuf.used > 0) {
			splx(s);
			return (1);
		}
		selrecord(p, &sc->rsel);
		break;

	case FWRITE:
		if (sc->outbuf.used < sc->outbuf.usedhigh) {
			splx(s);
			return (1);
		}
		selrecord(p, &sc->wsel);
		break;
	}

	splx(s);
	return (0);
}

void
midi_getinfo(dev, mi)
	dev_t dev;
	struct midi_info *mi;
{
	int unit = MIDIUNIT(dev);
	struct midi_softc *sc;

	if (unit >= midi_cd.cd_ndevs ||
	    (sc = midi_cd.cd_devs[unit]) == NULL)
		return;
	sc->hw_if->getinfo(sc->hw_hdl, mi);
}

#endif /* NMIDI > 0 */

#if NMIDI > 0 || NMIDIBUS > 0

int	audioprint __P((void *, const char *));

void
midi_attach_mi(mhwp, hdlp, dev)
	struct midi_hw_if *mhwp;
	void *hdlp;
	struct device *dev;
{
	struct audio_attach_args arg;

#ifdef DIAGNOSTIC
	if (mhwp == NULL) {
		printf("midi_attach_mi: NULL\n");
		return;
	}
#endif
	arg.type = AUDIODEV_TYPE_MIDI;
	arg.hwif = mhwp;
	arg.hdl = hdlp;
	(void)config_found(dev, &arg, audioprint);
}

#endif /* NMIDI > 0 || NMIDIBUS > 0 */
