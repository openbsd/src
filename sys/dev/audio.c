/*	$NetBSD: audio.c,v 1.14 1996/01/07 06:21:02 mycroft Exp $	*/

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
 */

/*
 * This is a (partially) SunOS-compatible /dev/audio driver for NetBSD.
 * 
 * This code tries to do something half-way sensible with
 * half-duplex hardware, such as with the SoundBlaster hardware.  With
 * half-duplex hardware allowing O_RDWR access doesn't really make
 * sense.  However, closing and opening the device to "turn around the
 * line" is relatively expensive and costs a card reset (which can
 * take some time, at least for the SoundBlaster hardware).  Instead
 * we allow O_RDWR access, and provide an ioctl to set the "mode",
 * i.e. playing or recording.
 *
 * If you write to a half-duplex device in record mode, the data is
 * tossed.  If you read from the device in play mode, you get silence
 * filled buffers at the rate at which samples are naturally
 * generated.
 *
 * If you try to set both play and record mode on a half-duplex
 * device, playing takes precedence.
 */

/*
 * Todo:
 * - Add softaudio() isr processing for wakeup, select and signals.
 * - Allow opens for READ and WRITE (one open each)
 * - Setup for single isr for full-duplex
 * - Add SIGIO generation for changes in the mixer device
 * - Fixup SunOS compat for mixer device changes in ioctl.
 */

#include "audio.h"
#if NAUDIO > 0

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/select.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>

#include <sys/audioio.h>
#include <dev/audiovar.h>
#include <dev/audio_if.h>

#ifdef AUDIO_DEBUG
#include <machine/stdarg.h>
#ifndef TOLOG
#define TOLOG	0x04
#endif
void kprintf __P((const char *fmt, int flags, struct tty *tp, va_list ap));

void
#ifdef __STDC__
Dprintf(const char *fmt, ...)
#else
Dprintf(fmt, va_alist)
	char *fmt;
#endif
{
	va_list ap;

	va_start(ap, fmt);
	kprintf(fmt, TOLOG, NULL, ap);
	va_end(ap);
}

#define DPRINTF(x)	if (audiodebug) Dprintf x
int	audiodebug = 0;
#else
#define DPRINTF(x)
#endif

int naudio;	/* Count of attached hardware drivers */

int audio_blk_ms = AUDIO_BLK_MS;
int audio_backlog = AUDIO_BACKLOG;
int audio_blocksize;

/* A block of silence */
char *auzero_block;

struct audio_softc *audio_softc[NAUDIO];

int	audiosetinfo __P((struct audio_softc *, struct audio_info *));
int	audiogetinfo __P((struct audio_softc *, struct audio_info *));

int	audio_open __P((dev_t, int, int, struct proc *));
int	audio_close __P((dev_t, int, int, struct proc *));
int	audio_read __P((dev_t, struct uio *, int));
int	audio_write __P((dev_t, struct uio *, int));
int	audio_ioctl __P((dev_t, int, caddr_t, int, struct proc *));
int	audio_select __P((dev_t, int, struct proc *));

int	mixer_open __P((dev_t, int, int, struct proc *));
int	mixer_close __P((dev_t, int, int, struct proc *));
int	mixer_ioctl __P((dev_t, int, caddr_t, int, struct proc *));
    
void	audio_init_record __P((struct audio_softc *));
void	audio_init_play __P((struct audio_softc *));
void	audiostartr __P((struct audio_softc *));
void	audiostartp __P((struct audio_softc *));
void	audio_rint __P((struct audio_softc *));
void	audio_pint __P((struct audio_softc *));

int	audio_calc_blksize __P((struct audio_softc *));
void	audio_silence_fill __P((struct audio_softc *, u_char *, int));
int	audio_silence_copyout __P((struct audio_softc *, int, struct uio *));
void	audio_alloc_auzero __P((struct audio_softc *, int));

#ifdef AUDIO_DEBUG
void
audio_printsc(struct audio_softc *sc)
{
	printf("hwhandle %x hw_if %x ", sc->hw_hdl, sc->hw_if);
	printf("open %x mode %x\n", sc->sc_open, sc->sc_mode);
	printf("rchan %x wchan %x ", sc->sc_rchan, sc->sc_wchan);
	printf("rring blk %x pring nblk %x\n", sc->rr.nblk, sc->pr.nblk);
	printf("rbus %x pbus %x ", sc->sc_rbus, sc->sc_pbus);
	printf("blksz %d sib %d ", sc->sc_blksize, sc->sc_smpl_in_blk);
	printf("sp50ms %d backlog %d\n", sc->sc_50ms, sc->sc_backlog);
	printf("hiwat %d lowat %d rblks %d\n", sc->sc_hiwat, sc->sc_lowat,
		sc->sc_rblks);
}
#endif

void
audioattach(num)
	int num;
{
}

/*
 * Called from hardware driver.
 */
int
audio_hardware_attach(hwp, hdlp)
	struct audio_hw_if *hwp;
	void *hdlp;
{
	int *zp, i;
	struct audio_softc *sc;

	if (naudio >= NAUDIO) {
	    DPRINTF(("audio_hardware_attach: not enough audio devices: %d > %d\n",
		     naudio, NAUDIO));
	    return(EINVAL);
	}

	/*
	 * Malloc a softc for the device
	 */
	/* XXX Find the first free slot */
	audio_softc[naudio] = malloc(sizeof(struct audio_softc), M_DEVBUF, M_WAITOK);
	sc = audio_softc[naudio];	
	bzero(sc, sizeof(struct audio_softc));

	/* XXX too paranoid? */
	if (hwp->open == 0 ||
	    hwp->close == 0 ||
	    hwp->set_in_sr == 0 ||
	    hwp->get_in_sr == 0 ||
	    hwp->set_out_sr == 0 ||
	    hwp->get_out_sr == 0 ||
	    hwp->set_encoding == 0 ||
	    hwp->get_encoding == 0 ||
	    hwp->set_precision == 0 ||
	    hwp->get_precision == 0 ||
	    hwp->set_channels == 0 ||
	    hwp->get_channels == 0 ||
	    hwp->round_blocksize == 0 ||
	    hwp->set_out_port == 0 ||
	    hwp->get_out_port == 0 ||
	    hwp->set_in_port == 0 ||
	    hwp->get_in_port == 0 ||
	    hwp->commit_settings == 0 ||
	    hwp->get_silence == 0 ||
	    hwp->start_output == 0 ||
	    hwp->start_input == 0 ||
	    hwp->halt_output == 0 ||
	    hwp->halt_input == 0 ||
	    hwp->cont_output == 0 ||
	    hwp->cont_input == 0 ||
	    hwp->getdev == 0 ||
	    hwp->set_port == 0 ||
	    hwp->get_port == 0 ||
	    hwp->query_devinfo == 0)
		return(EINVAL);

	sc->hw_if = hwp;
	sc->hw_hdl = hdlp;
	
	/*
	 * Alloc DMA play and record buffers
	 */
	sc->rr.bp = malloc(AU_RING_SIZE, M_DEVBUF, M_WAITOK);
	if (sc->rr.bp == 0) {
		return (ENOMEM);
	}
	sc->pr.bp = malloc(AU_RING_SIZE, M_DEVBUF, M_WAITOK);
	if (sc->pr.bp == 0) {
		free(sc->rr.bp, M_DEVBUF);
		return (ENOMEM);
	}

	/*
	 * Set default softc params
	 */
	sc->sc_pencoding = sc->sc_rencoding = AUDIO_ENCODING_LINEAR;

	/*
	 * Return the audio unit number
	 */
	hwp->audio_unit = naudio++;

#ifdef AUDIO_DEBUG
	printf("audio: unit %d attached\n", hwp->audio_unit);
#endif
	
	return(0);
}

int
audio_hardware_detach(hwp)
	struct audio_hw_if *hwp;
{
	struct audio_softc *sc;
	
#ifdef DIAGNOSTIC
	if (!hwp)
	    panic("audio_hardware_detach: null hwp");
	
	if (hwp->audio_unit > naudio)
	    panic("audio_hardware_detach: invalid audio unit");
#endif

	sc = audio_softc[hwp->audio_unit];

	if (hwp != sc->hw_if)
		return(EINVAL);
	
	if (sc->sc_open != 0)
		return(EBUSY);

	sc->hw_if = 0;

	/* Free audio buffers */
	free(sc->rr.bp, M_DEVBUF);
	free(sc->pr.bp, M_DEVBUF);

	free(sc, M_DEVBUF);
	audio_softc[hwp->audio_unit] = NULL;

	return(0);
}

int
audioopen(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	switch(AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	    return audio_open(dev, flags, ifmt, p);
	    /*NOTREACHED*/
	case MIXER_DEVICE:
	    return mixer_open(dev, flags, ifmt, p);
	    /*NOTREACHED*/
	default:
	    break;
	}
	return EIO;
}

/* ARGSUSED */
int
audioclose(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	switch(AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	    return audio_close(dev, flags, ifmt, p);
	    /*NOTREACHED*/
	case MIXER_DEVICE:
	    return mixer_close(dev, flags, ifmt, p);
	    /*NOTREACHED*/
	default:
	    break;
	}
	return EIO;
}

int
audioread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	switch(AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	    return audio_read(dev, uio, ioflag);
	    /*NOTREACHED*/
	case MIXER_DEVICE:
	    return ENODEV;
	    /*NOTREACHED*/
	default:
	    break;
	}
	return EIO;
}

int
audiowrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	switch(AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	    return audio_write(dev, uio, ioflag);
	    /*NOTREACHED*/
	case MIXER_DEVICE:
	    return ENODEV;
	    /*NOTREACHED*/
	default:
	    break;
	}
	return EIO;
}

int
audioioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	switch(AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	    return audio_ioctl(dev, cmd, addr, flag, p);
	    /*NOTREACHED*/
	case MIXER_DEVICE:
	    return mixer_ioctl(dev, cmd, addr, flag, p);
	    /*NOTREACHED*/
	default:
	    break;
	}
	return EIO;
}

int
audioselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	switch(AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	    return audio_select(dev, rw, p);
	    /*NOTREACHED*/
	case MIXER_DEVICE:
	    return 1;
	    /*NOTREACHED*/
	default:
	    break;
	}
	return EIO;
}

/*
 * Audio driver
 */
void
audio_init_ring(rp, blksize)
	struct audio_buffer *rp;
	int blksize;
{
	int nblk = AU_RING_SIZE / blksize;

	rp->ep = rp->bp + nblk * blksize;
	rp->hp = rp->tp = rp->bp;
	rp->maxblk = nblk;
	rp->nblk = 0;
	rp->cb_drops = 0;
	rp->cb_pdrops = 0;
}

void
audio_initbufs(sc)
	struct audio_softc *sc;
{
	int nblk = AU_RING_SIZE / sc->sc_blksize;

	audio_init_ring(&sc->rr, sc->sc_blksize);
	audio_init_ring(&sc->pr, sc->sc_blksize);
	sc->sc_lowat = 1;
	if (nblk == 1)
	  sc->sc_hiwat = 1;
	else
	  sc->sc_hiwat = nblk - sc->sc_lowat;
}

static inline int
audio_sleep_timo(chan, label, timo)
	int *chan;
	char *label;
	int timo;
{
	int st;

	if (!label)
		label = "audio";
	
	*chan = 1;
	st = (tsleep(chan, PWAIT | PCATCH, label, timo));
	*chan = 0;
	if (st != 0) {
	    DPRINTF(("audio_sleep: %d\n", st));
	}
	return (st);
}

static inline int
audio_sleep(chan, label)
	int *chan;
	char *label;
{
    return audio_sleep_timo(chan, label, 0);
}

static inline void
audio_wakeup(chan)
	int *chan;
{
	if (*chan) {
		wakeup(chan);
		*chan = 0;
	}
}

int
audio_open(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;
	int s, error;
	struct audio_hw_if *hw;

	if (unit >= NAUDIO || !audio_softc[unit]) {
	    DPRINTF(("audio_open: invalid device unit - %d\n", unit));
	    return (ENODEV);
	}

	sc = audio_softc[unit];
	hw = sc->hw_if;

	DPRINTF(("audio_open: dev=0x%x flags=0x%x sc=0x%x hdl=0x%x\n", dev, flags, sc, sc->hw_hdl));
	if (hw == 0)		/* Hardware has not attached to us... */
		return (ENXIO);

	if ((sc->sc_open & (AUOPEN_READ|AUOPEN_WRITE)) != 0) /* XXX use flags */
		return (EBUSY);

	if ((error = hw->open(dev, flags)) != 0)
		return (error);

	if (flags&FREAD)
		sc->sc_open |= AUOPEN_READ;
	if (flags&FWRITE)
		sc->sc_open |= AUOPEN_WRITE;

	/*
	 * Multiplex device: /dev/audio (MU-Law) and /dev/sound (linear)
	 * The /dev/audio is always (re)set to 8-bit MU-Law mono
	 * For the other devices, you get what they were last set to.
	 */
	if (ISDEVAUDIO(dev)) {
		/* /dev/audio */
		hw->set_precision(sc->hw_hdl, 8);
		hw->set_encoding(sc->hw_hdl, AUDIO_ENCODING_ULAW);
		hw->set_in_sr(sc->hw_hdl, 8000);
		hw->set_out_sr(sc->hw_hdl, 8000);
		hw->set_channels(sc->hw_hdl, 1);

		sc->sc_pencoding = AUDIO_ENCODING_ULAW;
		sc->sc_rencoding = AUDIO_ENCODING_ULAW;
	}

	/*
	 * Sample rate and precision are supposed to be set to proper
	 * default values by the hardware driver, so that it may give
	 * us these values.
	 */
#ifdef DIAGNOSTIC
	if (hw->get_precision(sc->hw_hdl) == 0)
	    panic("audio_open: hardware driver returned 0 for get_precision");
#endif
	sc->sc_blksize = audio_blocksize = audio_calc_blksize(sc);
	audio_alloc_auzero(sc, sc->sc_blksize);
	    
	sc->sc_smpl_in_blk = audio_blocksize / 
	    (hw->get_precision(sc->hw_hdl) / NBBY);
	sc->sc_50ms = 50 * hw->get_out_sr(sc->hw_hdl) / 1000;
	sc->sc_backlog = audio_backlog;

	audio_initbufs(sc);
	DPRINTF(("audio_open: rr.bp=%x-%x pr.bp=%x-%x\n",
		 sc->rr.bp, sc->rr.ep, sc->pr.bp, sc->pr.ep));
	
	hw->commit_settings(sc->hw_hdl);

	s = splaudio();

	/* nothing read or written yet */
	sc->sc_rseek = 0;
	sc->sc_wseek = 0;

	sc->sc_rchan = 0;
	sc->sc_wchan = 0;

	sc->sc_rbus = 0;
	sc->sc_pbus = 0;

	if ((flags & FWRITE) != 0) {
		audio_init_play(sc);
		/* audio_pint(sc);		??? */
	}
	if ((flags & FREAD) != 0) {
		/* Play takes precedence if HW is half-duplex */
		if (hw->full_duplex || ((flags & FWRITE) == 0)) {
			audio_init_record(sc);
			/* audiostartr(sc); don't start recording until read */
		}
	}
	if (ISDEVAUDIO(dev)) {
	    /* if open only for read or only for write, then set specific mode */
	    if ((flags & (FWRITE|FREAD)) == FWRITE) {
		sc->sc_mode = AUMODE_PLAY;
		sc->pr.cb_pause = 0;
		sc->rr.cb_pause = 1;
		audiostartp(sc);
	    } else if ((flags & (FWRITE|FREAD)) == FREAD) {
		sc->sc_mode = AUMODE_RECORD;
		sc->rr.cb_pause = 0;
		sc->pr.cb_pause = 1;
		audiostartr(sc);
	    }
	}
	splx(s);
	return (0);
}

/*
 * Must be called from task context.
 */
void
audio_init_record(sc)
	struct audio_softc *sc;
{
	int s = splaudio();

	sc->sc_mode |= AUMODE_RECORD;
	if (sc->hw_if->speaker_ctl &&
	    (!sc->hw_if->full_duplex || (sc->sc_mode & AUMODE_PLAY) == 0))
		sc->hw_if->speaker_ctl(sc->hw_hdl, SPKR_OFF);
	splx(s);
}

/*
 * Must be called from task context.
 */
void
audio_init_play(sc)
	struct audio_softc *sc;
{
	int s = splaudio();

	sc->sc_mode |= AUMODE_PLAY;
	sc->sc_rblks = 0;
	if (sc->hw_if->speaker_ctl)
		sc->hw_if->speaker_ctl(sc->hw_hdl, SPKR_ON);
	splx(s);
}

int
audio_drain(sc)
	struct audio_softc *sc;
{
	int error;

	while (sc->pr.nblk > 0) {
		DPRINTF(("audio_drain: nblk=%d\n", sc->pr.nblk));
		/*
		 * XXX
		 * When the process is exiting, it ignores all signals and
		 * we can't interrupt this sleep, so we set a 1-minute
		 * timeout.
		 */
		error = audio_sleep_timo(&sc->sc_wchan, "aud dr", 60*hz);
		if (error != 0)
			return (error);
	}
	return (0);
}

/*
 * Close an audio chip.
 */
/* ARGSUSED */
int
audio_close(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_softc[unit];
	struct audio_hw_if *hw = sc->hw_if;
	int s;

	DPRINTF(("audio_close: unit=%d\n", unit));

	/*
	 * Block until output drains, but allow ^C interrupt.
	 */
	sc->sc_lowat = 0;	/* avoid excessive wakeups */
	s = splaudio();
	/*
	 * If there is pending output, let it drain (unless
	 * the output is paused).
	 */
	if (sc->sc_pbus && sc->pr.nblk > 0 && !sc->pr.cb_pause) {
		if (!audio_drain(sc) && hw->drain)
			(void)hw->drain(sc->hw_hdl);
	}
	
	hw->close(sc->hw_hdl);
	
	if (flags&FREAD)
		sc->sc_open &= ~AUOPEN_READ;
	if (flags&FWRITE)
		sc->sc_open &= ~AUOPEN_WRITE;

	splx(s);
	DPRINTF(("audio_close: done\n"));

	return (0);
}

int
audio_read(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_softc[unit];
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_buffer *cb = &sc->rr;
	u_char *hp;
	int blocksize = sc->sc_blksize;
	int error, s;

	DPRINTF(("audio_read: cc=%d mode=%d rblks=%d\n",
	         uio->uio_resid, sc->sc_mode, sc->sc_rblks));

	if (uio->uio_resid == 0)
		return (0);

	if (uio->uio_resid < blocksize)
		return (EINVAL);

	/* First sample we'll read in sample space */
	sc->sc_rseek = cb->au_stamp - AU_RING_LEN(cb);

	/*
	 * If hardware is half-duplex and currently playing, return
	 * silence blocks based on the number of blocks we have output.
	 */
	if ((!hw->full_duplex) &&
	    (sc->sc_mode & AUMODE_PLAY)) {
		do {
			s = splaudio();
			while (sc->sc_rblks <= 0) {
				DPRINTF(("audio_read: sc_rblks=%d\n", sc->sc_rblks));
				if (ioflag & IO_NDELAY) {
					splx(s);
					return (EWOULDBLOCK);
				}
				error = audio_sleep(&sc->sc_rchan, "aud hr");
				if (error != 0) {
					splx(s);
					return (error);
				}
			}
			splx(s);
			error = audio_silence_copyout(sc, blocksize, uio);
			if (error)
				break;
			--sc->sc_rblks;
		} while (uio->uio_resid >= blocksize);
		return (error);
	}
	error = 0;
	do {
		while (cb->nblk <= 0) {
			if (ioflag & IO_NDELAY) {
				error = EWOULDBLOCK;
				return (error);
			}
			s = splaudio();
			if (!sc->sc_rbus)
				audiostartr(sc);
			error = audio_sleep(&sc->sc_rchan, "aud rd");
			splx(s);
			if (error != 0)
				return (error);
		}
		hp = cb->hp;
		if (hw->sw_decode)
			hw->sw_decode(sc->hw_hdl, sc->sc_rencoding, hp, blocksize);
		error = uiomove(hp, blocksize, uio);
		if (error)
			break;
		hp += blocksize;
		if (hp >= cb->ep)
			hp = cb->bp;
		cb->hp = hp;
		--cb->nblk;
	} while (uio->uio_resid >= blocksize);

	return (error);
}

void
audio_clear(sc)
	struct audio_softc *sc;
{
	int s = splaudio();

	if (sc->sc_rbus || sc->sc_pbus) {
		sc->hw_if->halt_output(sc->hw_hdl);
		sc->hw_if->halt_input(sc->hw_hdl);
		sc->sc_rbus = 0;
		sc->sc_pbus = 0;
	}
	AU_RING_INIT(&sc->rr);
	AU_RING_INIT(&sc->pr);

	splx(s);
}

int
audio_calc_blksize(sc)
	struct audio_softc *sc;
{
	struct audio_hw_if *hw = sc->hw_if;
    	int bs;
	
	bs =  hw->get_out_sr(sc->hw_hdl) * audio_blk_ms / 1000;
	if (bs == 0)
		bs = 1;
	bs *= hw->get_channels(sc->hw_hdl);
	bs *= hw->get_precision(sc->hw_hdl) / NBBY;
	if (bs > AU_RING_SIZE/2)
		bs = AU_RING_SIZE/2;
	bs =  hw->round_blocksize(sc->hw_hdl, bs);
	if (bs > AU_RING_SIZE)
		bs = AU_RING_SIZE;

	return(bs);
}

void
audio_silence_fill(sc, p, n)
	struct audio_softc *sc;
        u_char *p;
        int n;
{
	struct audio_hw_if *hw = sc->hw_if;
	int i;
	u_int auzero;

	auzero = hw->get_silence(sc->sc_pencoding);

	while (--n >= 0)
	    *p++ = auzero;
}

audio_silence_copyout(sc, n, uio)
	struct audio_softc *sc;
	int n;
	struct uio *uio;
{
	struct audio_hw_if *hw = sc->hw_if;
	struct iovec *iov;
	int error = 0;
	u_int auzero;

	auzero = hw->get_silence(sc->sc_rencoding);

        while (n > 0 && uio->uio_resid) {
                iov = uio->uio_iov;
                if (iov->iov_len == 0) {
                        uio->uio_iov++;
                        uio->uio_iovcnt--;
                        continue;
                }
                switch (uio->uio_segflg) {
                case UIO_USERSPACE:
			error = copyout(&auzero, iov->iov_base, 1);
			if (error)
				return (error);
			break;

                case UIO_SYSSPACE:
                        bcopy(&auzero, iov->iov_base, 1);
                        break;
                }
                iov->iov_base++;
                iov->iov_len--;
                uio->uio_resid--;
                uio->uio_offset++;
                n--;
        }
        return (error);
}

void
audio_alloc_auzero(sc, bs)
	struct audio_softc *sc;
	int bs;
{
	struct audio_hw_if *hw = sc->hw_if;
	u_char *p, silence;
	int i;
	
	if (auzero_block)
		free(auzero_block, M_DEVBUF);

	auzero_block = malloc(bs, M_DEVBUF, M_WAITOK);
#ifdef DIAGNOSTIC
	if (auzero_block == 0) {
		panic("audio_alloc_auzero: malloc auzero_block failed");
	}
#endif
	silence = hw->get_silence(sc->sc_pencoding);

	p = auzero_block;
	for (i = 0; i < bs; i++)
		*p++ = silence;

	if (hw->sw_encode)
		hw->sw_encode(sc->hw_hdl, sc->sc_pencoding, auzero_block, bs);
}

    
int
audio_write(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_softc[unit];
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_buffer *cb = &sc->pr;
	u_char *tp;
	int error, s, cc;
	int blocksize = sc->sc_blksize;

	DPRINTF(("audio_write: cc=%d hiwat=%d\n", uio->uio_resid, sc->sc_hiwat));
	/*
	 * If half-duplex and currently recording, throw away data.
	 */
	if (!hw->full_duplex &&
	    (sc->sc_mode & AUMODE_RECORD)) {
		uio->uio_offset += uio->uio_resid;
		uio->uio_resid = 0;
		DPRINTF(("audio_write: half-dpx read busy\n"));
		return (0);
	}
	error = 0;

	while (uio->uio_resid > 0) {
		int watermark = sc->sc_hiwat;
		while (cb->nblk > watermark) {
			DPRINTF(("audio_write: nblk=%d watermark=%d\n", cb->nblk, watermark));
			if (ioflag & IO_NDELAY) {
				error = EWOULDBLOCK;
				return (error);
			}
			error = audio_sleep(&sc->sc_wchan, "aud wr");
			if (error != 0) {
				return (error);
			}
			watermark = sc->sc_lowat;
		}
#if 0
		if (cb->nblk == 0 &&
		    cb->maxblk > sc->sc_backlog &&
		    uio->uio_resid <= blocksize &&
		    (cb->au_stamp - sc->sc_wseek) > sc->sc_50ms) {
			/*
			 * the write is 'small', the buffer is empty
			 * and we have been silent for at least 50ms
			 * so we might be dealing with an application
			 * that writes frames synchronously with
			 * reading them.  If so, we need an output
			 * backlog to cover scheduling delays or
			 * there will be gaps in the sound output.
			 * Also take this opportunity to reset the
			 * buffer pointers in case we ended up on
			 * a bad boundary (odd byte, blksize bytes
			 * from end, etc.).
			 */
		        DPRINTF(("audiowrite: set backlog %d\n", sc->sc_backlog));
			s = splaudio();
			cb->hp = cb->bp;
			cb->nblk = sc->sc_backlog;
			cb->tp = cb->hp + sc->sc_backlog * blocksize;
			splx(s);
			audio_silence_fill(sc, cb->hp, sc->sc_backlog * blocksize);
		}
#endif
		/* Calculate sample number of first sample in block we write */
		s = splaudio();
		sc->sc_wseek = AU_RING_LEN(cb) + cb->au_stamp;
		splx(s);
		
		tp = cb->tp;
		cc = uio->uio_resid;

#ifdef AUDIO_DEBUG
		if (audiodebug > 1) {
		    int left = cb->ep - tp;
		    Dprintf("audio_write: cc=%d tp=0x%x bs=%d nblk=%d left=%d\n", cc, tp, blocksize, cb->nblk, left);
		}
#endif		
#ifdef DIAGNOSTIC
  {
		int towrite = (cc < blocksize)?cc:blocksize;
      
		/* check for an overwrite. Should never happen */
		if ((tp + towrite) > cb->ep) {
			DPRINTF(("audio_write: overwrite tp=0x%x towrite=%d ep=0x%x bs=%d\n",
			         tp, towrite, cb->ep, blocksize));
			printf("audio_write: overwrite tp=0x%x towrite=%d ep=0x%x\n",
			         tp, towrite, cb->ep);
			tp = cb->bp;
		}
  }
#endif
		if (cc < blocksize) {
			error = uiomove(tp, cc, uio);
			if (error == 0) {
				/* fill with audio silence */
				tp += cc;
				cc = blocksize - cc;
				audio_silence_fill(sc, tp, cc);
				DPRINTF(("audio_write: auzero 0x%x %d 0x%x\n",
				         tp, cc, *(int *)tp));
				tp += cc;
			}
		} else {
			error = uiomove(tp, blocksize, uio);
			if (error == 0) {
				tp += blocksize;
			}
		}
		if (error) {
#ifdef AUDIO_DEBUG
		        printf("audio_write:(1) uiomove failed %d; cc=%d tp=0x%x bs=%d\n", error, cc, tp, blocksize);
#endif
			break;
		}		    

		if (hw->sw_encode) {
			hw->sw_encode(sc->hw_hdl, sc->sc_pencoding, cb->tp, blocksize);
		}

		/* wrap the ring buffer if at end */
		s = splaudio();
		if (tp >= cb->ep)
			tp = cb->bp;
		cb->tp = tp;
		++cb->nblk;	/* account for buffer filled */

		/*
		 * If output isn't active, start it up.
		 */
		if (sc->sc_pbus == 0)
			audiostartp(sc);
		splx(s);
	}
	return (error);
}

int
audio_ioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_softc[unit];
	struct audio_hw_if *hw = sc->hw_if;
	int error = 0, s;

	DPRINTF(("audio_ioctl(%d,'%c',%d)\n",
	          IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff));
	switch (cmd) {

	case AUDIO_FLUSH:
		DPRINTF(("AUDIO_FLUSH\n"));
		audio_clear(sc);
		s = splaudio();
		if ((sc->sc_mode & AUMODE_PLAY) && (sc->sc_pbus == 0))
			audiostartp(sc);
		/* Again, play takes precedence on half-duplex hardware */
		if ((sc->sc_mode & AUMODE_RECORD) &&
		    (hw->full_duplex ||
		     ((sc->sc_mode & AUMODE_PLAY) == 0)))
			audiostartr(sc);
		splx(s);
		break;

	/*
	 * Number of read samples dropped.  We don't know where or
	 * when they were dropped.
	 */
	case AUDIO_RERROR:
		*(int *)addr = sc->rr.cb_drops != 0;
		break;

	/*
	 * How many samples will elapse until mike hears the first
	 * sample of what we last wrote?
	 */
	case AUDIO_WSEEK:
		s = splaudio();
		*(u_long *)addr = sc->sc_wseek - sc->pr.au_stamp
				  + AU_RING_LEN(&sc->rr);
		splx(s);
		break;
	case AUDIO_SETINFO:
		DPRINTF(("AUDIO_SETINFO\n"));
		error = audiosetinfo(sc, (struct audio_info *)addr);
		break;

	case AUDIO_GETINFO:
		DPRINTF(("AUDIO_GETINFO\n"));
		error = audiogetinfo(sc, (struct audio_info *)addr);
		break;

	case AUDIO_DRAIN:
		DPRINTF(("AUDIO_DRAIN\n"));
		error = audio_drain(sc);
		if (!error && hw->drain)
		    error = hw->drain(sc->hw_hdl);
		break;

	case AUDIO_GETDEV:
		DPRINTF(("AUDIO_GETDEV\n"));
		error = hw->getdev(sc->hw_hdl, (audio_device_t *)addr);
		break;
		
	case AUDIO_GETENC:
		DPRINTF(("AUDIO_GETENC\n"));
		error = hw->query_encoding(sc->hw_hdl, (struct audio_encoding *)addr);
		break;

	case AUDIO_GETFD:
		DPRINTF(("AUDIO_GETFD\n"));
		*(int *)addr = hw->full_duplex;
		break;

	case AUDIO_SETFD:
		DPRINTF(("AUDIO_SETFD\n"));
		error = hw->setfd(sc->hw_hdl, *(int *)addr);
		break;

	default:
		DPRINTF(("audio_ioctl: unknown ioctl\n"));
		error = EINVAL;
		break;
	}
	DPRINTF(("audio_ioctl(%d,'%c',%d) result %d\n",
	          IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff, error));
	return (error);
}

int
audio_select(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_softc[unit];
	int s = splaudio();

#if 0
	DPRINTF(("audio_select: rw=%d mode=%d rblks=%d rr.nblk=%d\n",
	         rw, sc->sc_mode, sc->sc_rblks, sc->rr.nblk));
#endif
	switch (rw) {

	case FREAD:
		if (sc->sc_mode & AUMODE_PLAY) {
			if (sc->sc_rblks > 0) {
				splx(s);
				return (1);
			}
		} else if (sc->rr.nblk > 0) {
			splx(s);
			return (1);
		}
		selrecord(p, &sc->sc_rsel);
		break;

	case FWRITE:
		/*
		 * Can write if we're recording because it gets preempted.
		 * Otherwise, can write when below low water.
		 * XXX this won't work right if we're in 
		 * record mode -- we need to note that a write
		 * select has happed and flip the speaker.
		 *
		 * XXX The above XXX-comment is SoundBlaster-dependent,
		 * right? Or maybe specific to half-duplex devices?
		 */
		if (sc->sc_mode & AUMODE_RECORD ||
		    sc->pr.nblk < sc->sc_lowat) {
			splx(s);
			return (1);
		}
		selrecord(p, &sc->sc_wsel);
		break;
	}
	splx(s);
	return (0);
}

void
audiostartr(sc)
	struct audio_softc *sc;
{
	int err;
    
    	DPRINTF(("audiostartr: tp=0x%x\n", sc->rr.tp));

	if (err = sc->hw_if->start_input(sc->hw_hdl, sc->rr.tp, sc->sc_blksize,
	    				 audio_rint, (void *)sc)) {
		DPRINTF(("audiostartr failed: %d\n", err));
		audio_clear(sc);
	}
	else
		sc->sc_rbus = 1;
}

void
audiostartp(sc)
	struct audio_softc *sc;
{
	int rval;
    
    	DPRINTF(("audiostartp: hp=0x%x nblk=%d\n", sc->pr.hp, sc->pr.nblk));
    
	if (sc->pr.nblk > 0) {
		u_char *hp = sc->pr.hp;
		if (rval = sc->hw_if->start_output(sc->hw_hdl, hp, sc->sc_blksize,
		    				   audio_pint, (void *)sc)) {
		    	DPRINTF(("audiostartp: failed: %d\n", rval));
		}
		else {
			sc->sc_pbus = 1;
			hp += sc->sc_blksize;
			if (hp >= sc->pr.ep)
				hp = sc->pr.bp;
			sc->pr.hp = hp;
		}
	}
}

/*
 * Called from HW driver module on completion of dma output.
 * Start output of new block, wrap in ring buffer if needed.
 * If no more buffers to play, output zero instead.
 * Do a wakeup if necessary.
 */
void
audio_pint(sc)
	struct audio_softc *sc;
{
	u_char *hp;
	int cc = sc->sc_blksize;
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_buffer *cb = &sc->pr;
	int err;
	
	/*
	 * XXX
	 * if there is only one buffer in the ring, this test
	 * always fails and the output is always silence after the
	 * first block.
	 */
	if (--cb->nblk > 0) {
		hp = cb->hp;
		if (cb->cb_pause) {
		    cb->cb_pdrops++;
#ifdef AUDIO_DEBUG
		    if (audiodebug > 1)
			Dprintf("audio_pint: paused %d\n", cb->cb_pdrops);
#endif
		    goto psilence;
		}
		else {
#ifdef AUDIO_DEBUG
		    if (audiodebug > 1)
		    	Dprintf("audio_pint: hp=0x%x cc=%d\n", hp, cc);
#endif
		    if (err = hw->start_output(sc->hw_hdl, hp, cc,
					       audio_pint, (void *)sc)) {
			    DPRINTF(("audio_pint restart failed: %d\n", err));
			    audio_clear(sc);
		    }
		    else {
		    	    hp += cc;
			    if (hp >= cb->ep)
				    hp = cb->bp;
			    cb->hp = hp;
			    cb->au_stamp += sc->sc_smpl_in_blk;

			    ++sc->sc_rblks;
		    }
		}
	}
	else {
		cb->cb_drops++;
#ifdef AUDIO_DEBUG
		if (audiodebug > 1)
		    Dprintf("audio_pint: drops=%d auzero %d 0x%x\n", cb->cb_drops, cc, *(int *)auzero_block);
#endif
 psilence:
		if (err = hw->start_output(sc->hw_hdl,
		    			   auzero_block, cc,
					   audio_pint, (void *)sc)) {
			DPRINTF(("audio_pint zero failed: %d\n", err));
			audio_clear(sc);
		}
	}

#ifdef AUDIO_DEBUG
	DPRINTF(("audio_pint: mode=%d pause=%d nblk=%d lowat=%d\n",
		sc->sc_mode, cb->cb_pause, cb->nblk, sc->sc_lowat));
#endif
	if ((sc->sc_mode & AUMODE_PLAY) && !cb->cb_pause) {
		if (cb->nblk <= sc->sc_lowat) {
			audio_wakeup(&sc->sc_wchan);
			selwakeup(&sc->sc_wsel);
		}
	}

	/*
	 * XXX
	 * possible to return one or more "phantom blocks" now.
	 * Only in half duplex?
	 */
	if (hw->full_duplex) {
		audio_wakeup(&sc->sc_rchan);
		selwakeup(&sc->sc_rsel);
	}
}

/*
 * Called from HW driver module on completion of dma input.
 * Mark it as input in the ring buffer (fiddle pointers).
 * Do a wakeup if necessary.
 */
void
audio_rint(sc)
	struct audio_softc *sc;
{
	u_char *tp;
	int cc = sc->sc_blksize;
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_buffer *cb = &sc->rr;
	int err;
	
	tp = cb->tp;
	if (cb->cb_pause) {
		cb->cb_pdrops++;
		DPRINTF(("audio_rint: pdrops %d\n", cb->cb_pdrops));
	}
	else {
		tp += cc;
	    	if (tp >= cb->ep)
			tp = cb->bp;
	    	if (++cb->nblk < cb->maxblk) {
#ifdef AUDIO_DEBUG
		    	if (audiodebug > 1)
				Dprintf("audio_rint: tp=0x%x cc=%d\n", tp, cc);
#endif
			if (err = hw->start_input(sc->hw_hdl, tp, cc,
						  audio_rint, (void *)sc)) {
				DPRINTF(("audio_rint: start failed: %d\n", err));
				audio_clear(sc);
			}
			cb->au_stamp += sc->sc_smpl_in_blk;
		    } else {
			/* 
			 * XXX 
			 * How do we count dropped input samples due to overrun?
			 * Start a "dummy DMA transfer" when the input ring buffer
			 * is full and count # of these?  Seems pretty lame to
			 * me, but how else are we going to do this?
			 */
			cb->cb_drops++;
			sc->sc_rbus = 0;
			DPRINTF(("audio_rint: drops %d\n", cb->cb_drops));
	    	}
	    	cb->tp = tp;

	    	audio_wakeup(&sc->sc_rchan);
	    	selwakeup(&sc->sc_rsel);
	}
}

int
audiosetinfo(sc, ai)
	struct audio_softc *sc;
	struct audio_info *ai;
{
	struct audio_prinfo *r = &ai->record, *p = &ai->play;
	int cleared = 0;
	int bsize, bps, error = 0;
	struct audio_hw_if *hw = sc->hw_if;
	mixer_ctrl_t ct;
	
	if (hw == 0)		/* HW has not attached */
		return(ENXIO);

	if (p->sample_rate != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_out_sr(sc->hw_hdl, p->sample_rate);
		if (error != 0)
			return(error);
		sc->sc_50ms = 50 * hw->get_out_sr(sc->hw_hdl) / 1000;
	}
	if (r->sample_rate != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_in_sr(sc->hw_hdl, r->sample_rate);
		if (error != 0)
			return(error);
		sc->sc_50ms = 50 * hw->get_in_sr(sc->hw_hdl) / 1000;
	}
	if (p->encoding != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_encoding(sc->hw_hdl, p->encoding);
		if (error != 0)
			return(error);
		sc->sc_pencoding = p->encoding;
	}	
	if (r->encoding != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_encoding(sc->hw_hdl, r->encoding);
		if (error != 0)
			return(error);
		sc->sc_rencoding = r->encoding;
	}
	if (p->precision != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_precision(sc->hw_hdl, p->precision);
		if (error != 0)
			return(error);
		
		sc->sc_blksize = audio_blocksize = audio_calc_blksize(sc);
		audio_alloc_auzero(sc, sc->sc_blksize);
		bps = hw->get_precision(sc->hw_hdl) / NBBY;
		sc->sc_smpl_in_blk = sc->sc_blksize / bps;
		audio_initbufs(sc);
	}
	if (r->precision != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_precision(sc->hw_hdl, r->precision);
		if (error != 0)
			return(error);
		
		sc->sc_blksize = audio_blocksize = audio_calc_blksize(sc);
		audio_alloc_auzero(sc, sc->sc_blksize);
		bps = hw->get_precision(sc->hw_hdl) / NBBY;
		sc->sc_smpl_in_blk = sc->sc_blksize / bps;
		audio_initbufs(sc);
	}
	if (p->channels != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_channels(sc->hw_hdl, p->channels);
		if (error != 0)
			return(error);

		sc->sc_blksize = audio_blocksize = audio_calc_blksize(sc);
		audio_alloc_auzero(sc, sc->sc_blksize);
		bps = hw->get_precision(sc->hw_hdl) / NBBY;
		sc->sc_smpl_in_blk = sc->sc_blksize / bps;
		audio_initbufs(sc);
	}
	if (r->channels != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_channels(sc->hw_hdl, r->channels);
		if (error != 0)
			return(error);

		sc->sc_blksize = audio_blocksize = audio_calc_blksize(sc);
		audio_alloc_auzero(sc, sc->sc_blksize);
		bps = hw->get_precision(sc->hw_hdl) / NBBY;
		sc->sc_smpl_in_blk = sc->sc_blksize / bps;
		audio_initbufs(sc);
	}
	if (p->port != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_out_port(sc->hw_hdl, p->port);
		if (error != 0)
			return(error);
	}
	if (r->port != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		error = hw->set_in_port(sc->hw_hdl, r->port);
		if (error != 0)
			return(error);
	}
	if (p->gain != ~0) {
		ct.dev = hw->get_out_port(sc->hw_hdl);
		ct.type = AUDIO_MIXER_VALUE;
		ct.un.value.num_channels = 1;
		ct.un.value.level[AUDIO_MIXER_LEVEL_MONO] = p->gain;
		hw->set_port(sc->hw_hdl, &ct);
	}
	if (r->gain != ~0) {
		ct.dev = hw->get_in_port(sc->hw_hdl);
		ct.type = AUDIO_MIXER_VALUE;
		ct.un.value.num_channels = 1;
		ct.un.value.level[AUDIO_MIXER_LEVEL_MONO] = r->gain;
		hw->set_port(sc->hw_hdl, &ct);
	}
	
	if (p->pause != (u_char)~0) {
		sc->pr.cb_pause = p->pause;
		if (!p->pause) {
		    int s = splaudio();
			audiostartp(sc);
		    splx(s);
		}
	}
	if (r->pause != (u_char)~0) {
		sc->rr.cb_pause = r->pause;
		if (!r->pause) {
		    int s = splaudio();
			audiostartr(sc);
		    splx(s);
		}
	}

	if (ai->blocksize != ~0) { /* Explicitly specified, possibly */
				   /* overriding changes done above. */
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		if (ai->blocksize == 0)
			bsize = audio_blocksize;
		else if (ai->blocksize > AU_RING_SIZE/2)
			bsize = AU_RING_SIZE/2;
		else
			bsize = ai->blocksize;
		bsize = hw->round_blocksize(sc->hw_hdl, bsize);
		if (bsize > AU_RING_SIZE)
			bsize = AU_RING_SIZE;
		ai->blocksize = sc->sc_blksize = bsize;
		audio_alloc_auzero(sc, sc->sc_blksize);
		bps = hw->get_precision(sc->hw_hdl) / NBBY;
		sc->sc_smpl_in_blk = bsize / bps;
		audio_initbufs(sc);
	}
	if (ai->hiwat != ~0) {
		if ((unsigned)ai->hiwat > sc->pr.maxblk)
			ai->hiwat = sc->pr.maxblk;
		if (sc->sc_hiwat != 0)
			sc->sc_hiwat = ai->hiwat;
	}
	if (ai->lowat != ~0) {
		if ((unsigned)ai->lowat > sc->pr.maxblk)
			ai->lowat = sc->pr.maxblk;
		sc->sc_lowat = ai->lowat;
	}
	if (ai->backlog != ~0) {
		if ((unsigned)ai->backlog > (sc->pr.maxblk/2))
			ai->backlog = sc->pr.maxblk/2;
		sc->sc_backlog = ai->backlog;
	}
	if (ai->mode != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		sc->sc_mode = ai->mode;
		if (sc->sc_mode & AUMODE_PLAY) {
			audio_init_play(sc);
			if (!hw->full_duplex) /* Play takes precedence */
				sc->sc_mode &= ~(AUMODE_RECORD);
		}
		if (sc->sc_mode & AUMODE_RECORD)
			audio_init_record(sc);
	}
	error = hw->commit_settings(sc->hw_hdl);
	if (error != 0)
		return (error);

	if (cleared) {
		int s = splaudio();
		if (sc->sc_mode & AUMODE_PLAY)
			audiostartp(sc);
		if (sc->sc_mode & AUMODE_RECORD)
			audiostartr(sc);
		splx(s);
	}
	
	return (0);
}

int
audiogetinfo(sc, ai)
	struct audio_softc *sc;
	struct audio_info *ai;
{
	struct audio_prinfo *r = &ai->record, *p = &ai->play;
	struct audio_hw_if *hw = sc->hw_if;
	mixer_ctrl_t ct;
	
	if (hw == 0)		/* HW has not attached */
		return(ENXIO);
	
	p->sample_rate = hw->get_out_sr(sc->hw_hdl);
	r->sample_rate = hw->get_in_sr(sc->hw_hdl);
	p->channels = r->channels = hw->get_channels(sc->hw_hdl);
	p->precision = r->precision = hw->get_precision(sc->hw_hdl);
	p->encoding = hw->get_encoding(sc->hw_hdl);
	r->encoding = hw->get_encoding(sc->hw_hdl);

	r->port = hw->get_in_port(sc->hw_hdl);
	p->port = hw->get_out_port(sc->hw_hdl);

	ct.dev = r->port;
	ct.type = AUDIO_MIXER_VALUE;
	ct.un.value.num_channels = 1;
	if (hw->get_port(sc->hw_hdl, &ct) == 0)
	    r->gain = ct.un.value.level[AUDIO_MIXER_LEVEL_MONO];
	else
	    r->gain = AUDIO_MAX_GAIN/2;

	ct.dev = p->port;
	ct.un.value.num_channels = 1;
	if (hw->get_port(sc->hw_hdl, &ct) == 0)
	    p->gain = ct.un.value.level[AUDIO_MIXER_LEVEL_MONO];
	else
	    p->gain = AUDIO_MAX_GAIN/2;

	p->pause = sc->pr.cb_pause;
	r->pause = sc->rr.cb_pause;
	p->error = sc->pr.cb_drops != 0;
	r->error = sc->rr.cb_drops != 0;

	p->open = ((sc->sc_open & AUOPEN_WRITE) != 0);
	r->open = ((sc->sc_open & AUOPEN_READ) != 0);

	p->samples = sc->pr.au_stamp - sc->pr.cb_pdrops;
	r->samples = sc->rr.au_stamp - sc->rr.cb_pdrops;

	p->seek = sc->sc_wseek;
	r->seek = sc->sc_rseek;

	ai->blocksize = sc->sc_blksize;
	ai->hiwat = sc->sc_hiwat;
	ai->lowat = sc->sc_lowat;
	ai->backlog = sc->sc_backlog;
	ai->mode = sc->sc_mode;

	return (0);
}

/*
 * Mixer driver
 */
int
mixer_open(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;
	struct audio_hw_if *hw;

	if (unit >= NAUDIO || !audio_softc[unit]) {
	    DPRINTF(("mixer_open: invalid device unit - %d\n", unit));
	    return (ENODEV);
	}

	sc = audio_softc[unit];
	hw = sc->hw_if;

	DPRINTF(("mixer_open: dev=%x flags=0x%x sc=0x%x\n", dev, flags, sc));
	if (hw == 0)		/* Hardware has not attached to us... */
		return (ENXIO);

	return (0);
}

/*
 * Close a mixer device
 */
/* ARGSUSED */
int
mixer_close(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	DPRINTF(("mixer_close: unit %d\n", AUDIOUNIT(dev)));
	
	return (0);
}

int
mixer_ioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_softc[unit];
	struct audio_hw_if *hw = sc->hw_if;
	int error = EINVAL;

	DPRINTF(("mixer_ioctl(%d,'%c',%d)\n",
	          IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff));

	switch (cmd) {
	case AUDIO_GETDEV:
	    DPRINTF(("AUDIO_GETDEV\n"));
	    error = hw->getdev(sc->hw_hdl, (audio_device_t *)addr);
	    break;
		
	case AUDIO_MIXER_DEVINFO:
	    DPRINTF(("AUDIO_MIXER_DEVINFO\n"));
	    error = hw->query_devinfo(sc->hw_hdl, (mixer_devinfo_t *)addr);
	    break;

	case AUDIO_MIXER_READ:
	    DPRINTF(("AUDIO_MIXER_READ\n"));
	    error = hw->get_port(sc->hw_hdl, (mixer_ctrl_t *)addr);
	    break;

	case AUDIO_MIXER_WRITE:
	    DPRINTF(("AUDIO_MIXER_WRITE\n"));
	    error = hw->set_port(sc->hw_hdl, (mixer_ctrl_t *)addr);
	    if (error == 0)
		error = hw->commit_settings(sc->hw_hdl);
	    break;

	default:
	    error = EINVAL;
	    break;
	}
	DPRINTF(("mixer_ioctl(%d,'%c',%d) result %d\n",
	          IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff, error));
	return (error);
}
#endif
