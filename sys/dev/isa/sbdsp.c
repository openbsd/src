/*	$NetBSD: sbdsp.c,v 1.14 1995/11/10 05:01:06 mycroft Exp $	*/

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
/*
 * SoundBlaster Pro code provided by John Kohl, based on lots of
 * information he gleaned from Steve Haehnichen <steve@vigra.com>'s
 * SBlast driver for 386BSD and DOS driver code from Daniel Sachs
 * <sachs@meibm15.cen.uiuc.edu>.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/pio.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <i386/isa/icu.h>			/* XXX BROKEN; WHY? */

#include <dev/isa/sbreg.h>
#include <dev/isa/sbdspvar.h>

#ifdef AUDIO_DEBUG
extern void Dprintf __P((const char *, ...));
#define DPRINTF(x)	if (sbdspdebug) Dprintf x
int	sbdspdebug = 0;
#else
#define DPRINTF(x)
#endif

#ifndef SBDSP_NPOLL
#define SBDSP_NPOLL 3000
#endif

struct {
	int wdsp;
	int rdsp;
	int wmidi;
} sberr;

#ifdef AUDIO_DEBUG
void
sb_printsc(struct sbdsp_softc *sc)
{
	int i;
    
	printf("open %d dmachan %d iobase %x locked %d\n", sc->sc_open, sc->sc_drq,
		sc->sc_iobase, sc->sc_locked);
	printf("hispeed %d irate %d orate %d encoding %x\n",
		sc->sc_adacmode, sc->sc_irate, sc->sc_orate, sc->encoding);
	printf("outport %d inport %d spkron %d nintr %d\n",
		sc->out_port, sc->in_port, sc->spkr_state, sc->sc_interrupts);
	printf("tc %x chans %x scintr %x arg %x\n", sc->sc_adactc, sc->sc_chans,
		sc->sc_intr, sc->sc_arg);
	printf("gain: ");
	for (i = 0; i < SB_NDEVS; i++)
	    printf("%d ", sc->gain[i]);
	printf("\n");
}
#endif

/*
 * Probe / attach routines.
 */

/*
 * Probe for the soundblaster hardware.
 */
int
sbdsp_probe(sc)
	struct sbdsp_softc *sc;
{
	register int iobase = sc->sc_iobase;

	if (sbdsp_reset(sc) < 0) {
		DPRINTF(("sbdsp: couldn't reset card\n"));
		return 0;
	}
	sc->sc_model = sbversion(sc);

	return 1;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
sbdsp_attach(sc)
	struct sbdsp_softc *sc;
{
	register int iobase = sc->sc_iobase;

	sc->sc_locked = 0;

	/* Set defaults */
	if (ISSBPROCLASS(sc))
	    sc->sc_irate = sc->sc_orate = 45454;
  	else
	    sc->sc_irate = sc->sc_orate = 14925;
	sc->sc_chans = 1;
	sc->encoding = AUDIO_ENCODING_LINEAR;

	(void) sbdsp_set_in_sr_real(sc, sc->sc_irate);
	(void) sbdsp_set_out_sr_real(sc, sc->sc_orate);

	(void) sbdsp_set_in_port(sc, SB_MIC_PORT);
	(void) sbdsp_set_out_port(sc, SB_SPEAKER);

	if (ISSBPROCLASS(sc)) {
		int i;
	    
		/* set mixer to default levels, by sending a mixer
                   reset command. */
		sbdsp_mix_write(sc, SBP_MIX_RESET, SBP_MIX_RESET);
		/* then some adjustments :) */
		sbdsp_mix_write(sc, SBP_CD_VOL,
				sbdsp_stereo_vol(SBP_MAXVOL, SBP_MAXVOL));
		sbdsp_mix_write(sc, SBP_DAC_VOL,
				sbdsp_stereo_vol(SBP_MAXVOL, SBP_MAXVOL));
		sbdsp_mix_write(sc, SBP_MASTER_VOL,
				sbdsp_stereo_vol(SBP_MAXVOL, SBP_MAXVOL));
		sbdsp_mix_write(sc, SBP_LINE_VOL,
				sbdsp_stereo_vol(SBP_MAXVOL, SBP_MAXVOL));
		for (i = 0; i < SB_NDEVS; i++)
		    sc->gain[i] = sbdsp_stereo_vol(SBP_MAXVOL, SBP_MAXVOL);
	}
	printf(": dsp v%d.%02d\n",
	       SBVER_MAJOR(sc->sc_model), SBVER_MINOR(sc->sc_model));
}

/*
 * Various routines to interface to higher level audio driver
 */

void
sbdsp_mix_write(sc, mixerport, val)
	struct sbdsp_softc *sc;
	int mixerport;
	int val;
{
	int iobase = sc->sc_iobase;
	outb(iobase + SBP_MIXER_ADDR, mixerport);
	delay(10);
	outb(iobase + SBP_MIXER_DATA, val);
	delay(30);
}

int
sbdsp_mix_read(sc, mixerport)
	struct sbdsp_softc *sc;
	int mixerport;
{
	int iobase = sc->sc_iobase;
	outb(iobase + SBP_MIXER_ADDR, mixerport);
	delay(10);
	return inb(iobase + SBP_MIXER_DATA);
}

int
sbdsp_set_in_sr(addr, sr)
	void *addr;
	u_long sr;
{
	register struct sbdsp_softc *sc = addr;

	sc->sc_irate = sr;

	return 0;
}

int
sbdsp_set_in_sr_real(addr, sr)
	void *addr;
	u_long sr;
{
	register struct sbdsp_softc *sc = addr;
	int rval;

	if (rval = sbdsp_set_sr(sc, &sr, SB_INPUT_RATE))
		return rval;
	sc->sc_irate = sr;
	sc->sc_dmain_inprogress = 0;		/* do it again on next DMA out */
	sc->sc_dmaout_inprogress = 0;
	return(0);
}

u_long
sbdsp_get_in_sr(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	return(sc->sc_irate);
}

int
sbdsp_set_out_sr(addr, sr)
	void *addr;
	u_long sr;
{
	register struct sbdsp_softc *sc = addr;

	sc->sc_orate = sr;
	return(0);
}

int
sbdsp_set_out_sr_real(addr, sr)
	void *addr;
	u_long sr;
{
	register struct sbdsp_softc *sc = addr;
	int rval;

	if (rval = sbdsp_set_sr(sc, &sr, SB_OUTPUT_RATE))
		return rval;
	sc->sc_orate = sr;
	sc->sc_dmain_inprogress = 0;		/* do it again on next DMA out */
	return(0);
}

u_long
sbdsp_get_out_sr(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	return(sc->sc_orate);
}

int
sbdsp_query_encoding(addr, fp)
    void *addr;
    struct audio_encoding *fp;
{
    register struct sbdsp_softc *sc = addr;

    switch (fp->index) {
    case 0:
	strcpy(fp->name, AudioEmulaw);
	fp->format_id = AUDIO_ENCODING_ULAW;
	break;
    case 1:
	strcpy(fp->name, AudioEpcm16);
	fp->format_id = AUDIO_ENCODING_PCM16;
	break;
    default:
	return(EINVAL);
	/*NOTREACHED*/
    }
    return (0);
}

int
sbdsp_set_encoding(addr, enc)
	void *addr;
	u_int enc;
{
	register struct sbdsp_softc *sc = addr;
	
	switch(enc){
	case AUDIO_ENCODING_ULAW:
		sc->encoding = AUDIO_ENCODING_ULAW;
		break;
	case AUDIO_ENCODING_LINEAR:
		sc->encoding = AUDIO_ENCODING_LINEAR;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

int
sbdsp_get_encoding(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	return(sc->encoding);
}

int
sbdsp_set_precision(addr, prec)
	void *addr;
	u_int prec;
{

	if (prec != 8)
		return(EINVAL);
	return(0);
}

int
sbdsp_get_precision(addr)
	void *addr;
{
	return(8);
}

int
sbdsp_set_channels(addr, chans)
	void *addr;
	int chans;
{
	register struct sbdsp_softc *sc = addr;
	int rval;

	if (ISSBPROCLASS(sc)) {
		if (chans != 1 && chans != 2)
			return(EINVAL);

		sc->sc_chans = chans;
		if (rval = sbdsp_set_in_sr_real(addr, sc->sc_irate))
		    return rval;
		sbdsp_mix_write(sc, SBP_STEREO,
				(sbdsp_mix_read(sc, SBP_STEREO) & ~SBP_PLAYMODE_MASK) |
				(chans == 2 ? SBP_PLAYMODE_STEREO : SBP_PLAYMODE_MONO));
		/* recording channels needs to be done right when we start
		   DMA recording.  Just record number of channels for now
		   and set stereo when ready. */
	}
	else {
		if (chans != 1)
			return(EINVAL);
		sc->sc_chans = 1;
	}
	
	return(0);
}

int
sbdsp_get_channels(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;
	
#if 0
	/* recording stereo may frob the mixer output */
	if (ISSBPROCLASS(sc)) {
		if ((sbdsp_mix_read(sc, SBP_STEREO) & SBP_PLAYMODE_MASK) == SBP_PLAYMODE_STEREO) {
			sc->sc_chans = 2;
		}
		else {
			sc->sc_chans = 1;
		}
	}
	else {
		sc->sc_chans = 1;
	}
#endif

	return(sc->sc_chans);
}

int
sbdsp_set_out_port(addr, port)
	void *addr;
	int port;
{
	register struct sbdsp_softc *sc = addr;
	
	sc->out_port = port; /* Just record it */

	return(0);
}

int
sbdsp_get_out_port(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	return(sc->out_port);
}


int
sbdsp_set_in_port(addr, port)
	void *addr;
	int port;
{
	register struct sbdsp_softc *sc = addr;
	int mixport, sbport;
	
	if (ISSBPROCLASS(sc)) {
	    switch (port) {
	    case SB_MIC_PORT:
		sbport = SBP_FROM_MIC;
		mixport = SBP_MIC_VOL;
		break;
	    case SB_LINE_IN_PORT:
		sbport = SBP_FROM_LINE;
		mixport = SBP_LINE_VOL;
		break;
	    case SB_CD_PORT:
		sbport = SBP_FROM_CD;
		mixport = SBP_CD_VOL;
		break;
	    case SB_DAC_PORT:
	    case SB_FM_PORT:
	    default:
		return(EINVAL);
		/*NOTREACHED*/
	    }
	}
	else {
	    switch (port) {
	    case SB_MIC_PORT:
		sbport = SBP_FROM_MIC;
		mixport = SBP_MIC_VOL;
		break;
	    default:
		return(EINVAL);
		/*NOTREACHED*/
	    }
	}	    

	sc->in_port = port;	/* Just record it */

	if (ISSBPROCLASS(sc)) {
		/* record from that port */
		sbdsp_mix_write(sc, SBP_RECORD_SOURCE,
				SBP_RECORD_FROM(sbport, SBP_FILTER_OFF,
						SBP_FILTER_HIGH));
		/* fetch gain from that port */
		sc->gain[port] = sbdsp_mix_read(sc, mixport);
	}

	return(0);
}

int
sbdsp_get_in_port(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	return(sc->in_port);
}


int
sbdsp_speaker_ctl(addr, newstate)
	void *addr;
	int newstate;
{
	register struct sbdsp_softc *sc = addr;

	if ((newstate == SPKR_ON) &&
	    (sc->spkr_state == SPKR_OFF)) {
		sbdsp_spkron(sc);
		sc->spkr_state = SPKR_ON;
	}
	if ((newstate == SPKR_OFF) &&
	    (sc->spkr_state == SPKR_ON)) {
		sbdsp_spkroff(sc);
		sc->spkr_state = SPKR_OFF;
	}
	return(0);
}

int
sbdsp_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	register struct sbdsp_softc *sc = addr;

	sc->sc_last_hsr_size = sc->sc_last_hsw_size = 0;

	/* Higher speeds need bigger blocks to avoid popping and silence gaps. */
	if ((sc->sc_orate > 8000 || sc->sc_irate > 8000) &&
	    (blk > NBPG/2 || blk < NBPG/4))
		blk = NBPG/2;
	/* don't try to DMA too much at once, though. */
	if (blk > NBPG) blk = NBPG;
	if (sc->sc_chans == 2)
		return (blk & ~1); /* must be even to preserve stereo separation */
	else
		return(blk);	/* Anything goes :-) */
}

int
sbdsp_commit_settings(addr)
	void *addr;
{
	/* due to potentially unfortunate ordering in the above layers,
	   re-do a few sets which may be important--input gains
	   (adjust the proper channels), number of input channels (hit the
	   record rate and set mode) */

	register struct sbdsp_softc *sc = addr;

	sbdsp_set_out_sr_real(addr, sc->sc_orate);
	sbdsp_set_in_sr_real(addr, sc->sc_irate);

	sc->sc_last_hsw_size = sc->sc_last_hsr_size = 0;
	return(0);
}


int
sbdsp_open(sc, dev, flags)
	register struct sbdsp_softc *sc;
	dev_t dev;
	int flags;
{
        DPRINTF(("sbdsp_open: sc=0x%x\n", sc));

	if (sc->sc_open != 0 || sbdsp_reset(sc) != 0)
		return ENXIO;

	sc->sc_open = 1;
	sc->sc_mintr = 0;
	sc->sc_intr = 0;
	sc->sc_arg = 0;
	sc->sc_locked = 0;
	if (ISSBPROCLASS(sc) &&
	    sbdsp_wdsp(sc->sc_iobase, SB_DSP_RECORD_MONO) < 0) {
		DPRINTF(("sbdsp_open: can't set mono mode\n"));
		/* we'll readjust when it's time for DMA. */
	}
	sc->sc_dmain_inprogress = 0;
	sc->sc_dmaout_inprogress = 0;

	/*
	 * Leave most things as they were; users must change things if
	 * the previous process didn't leave it they way they wanted.
	 * Looked at another way, it's easy to set up a configuration
	 * in one program and leave it for another to inherit.
	 */
	DPRINTF(("sbdsp_open: opened\n"));

	return 0;
}

void
sbdsp_close(addr)
	void *addr;
{
	struct sbdsp_softc *sc = addr;

        DPRINTF(("sbdsp_close: sc=0x%x\n", sc));

	sc->sc_open = 0;
	sbdsp_spkroff(sc);
	sc->spkr_state = SPKR_OFF;
	sc->sc_intr = 0;
	sc->sc_mintr = 0;
	/* XXX this will turn off any dma */
	sbdsp_reset(sc);

	DPRINTF(("sbdsp_close: closed\n"));
}

/*
 * Lower-level routines
 */

/*
 * Reset the card.
 * Return non-zero if the card isn't detected.
 */
int
sbdsp_reset(sc)
	register struct sbdsp_softc *sc;
{
	register int iobase = sc->sc_iobase;

	/*
	 * erase any memory of last transfer size.
	 */
	sc->sc_last_hsr_size = sc->sc_last_hsw_size = 0;
	/*
	 * See SBK, section 11.3.
	 * We pulse a reset signal into the card.
	 * Gee, what a brilliant hardware design.
	 */
	outb(iobase + SBP_DSP_RESET, 1);
	delay(3);
	outb(iobase + SBP_DSP_RESET, 0);
	if (sbdsp_rdsp(iobase) != SB_MAGIC)
		return -1;
	return 0;
}

/*
 * Write a byte to the dsp.
 * XXX We are at the mercy of the card as we use a
 * polling loop and wait until it can take the byte.
 */
int
sbdsp_wdsp(int iobase, int v)
{
	register int i;

	for (i = SBDSP_NPOLL; --i >= 0; ) {
		if ((inb(iobase + SBP_DSP_WSTAT) & SB_DSP_BUSY) != 0) {
			delay(10); continue;
		}
		outb(iobase + SBP_DSP_WRITE, v);
		return 0;
	}
	++sberr.wdsp;
	return -1;
}

/*
 * Read a byte from the DSP, using polling.
 */
int
sbdsp_rdsp(int iobase)
{
	register int i;

	for (i = SBDSP_NPOLL; --i >= 0; ) {
		if ((inb(iobase + SBP_DSP_RSTAT) & SB_DSP_READY) == 0)
			continue;
		return inb(iobase + SBP_DSP_READ);
	}
	++sberr.rdsp;
	return -1;
}

/*
 * Doing certain things (like toggling the speaker) make
 * the SB hardware go away for a while, so pause a little.
 */
void
sbdsp_to(arg)
	void *arg;
{
	wakeup(arg);
}

void
sbdsp_pause(sc)
	struct sbdsp_softc *sc;
{
	extern int hz;

	timeout(sbdsp_to, sbdsp_to, hz/8);
	(void)tsleep(sbdsp_to, PWAIT, "sbpause", 0);
}

/*
 * Turn on the speaker.  The SBK documention says this operation
 * can take up to 1/10 of a second.  Higher level layers should
 * probably let the task sleep for this amount of time after
 * calling here.  Otherwise, things might not work (because
 * sbdsp_wdsp() and sbdsp_rdsp() will probably timeout.)
 *
 * These engineers had their heads up their ass when
 * they designed this card.
 */
void
sbdsp_spkron(sc)
	struct sbdsp_softc *sc;
{
	(void)sbdsp_wdsp(sc->sc_iobase, SB_DSP_SPKR_ON);
	sbdsp_pause(sc);
}

/*
 * Turn off the speaker; see comment above.
 */
void
sbdsp_spkroff(sc)
	struct sbdsp_softc *sc;
{
	(void)sbdsp_wdsp(sc->sc_iobase, SB_DSP_SPKR_OFF);
	sbdsp_pause(sc);
}

/*
 * Read the version number out of the card.  Return major code
 * in high byte, and minor code in low byte.
 */
short
sbversion(sc)
	struct sbdsp_softc *sc;
{
	register int iobase = sc->sc_iobase;
	short v;

	if (sbdsp_wdsp(iobase, SB_DSP_VERSION) < 0)
		return 0;
	v = sbdsp_rdsp(iobase) << 8;
	v |= sbdsp_rdsp(iobase);
	return ((v >= 0) ? v : 0);
}

/*
 * Halt a DMA in progress.  A low-speed transfer can be
 * resumed with sbdsp_contdma().
 */
int
sbdsp_haltdma(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	DPRINTF(("sbdsp_haltdma: sc=0x%x\n", sc));

	if (sc->sc_locked)
		sbdsp_reset(sc);
	else
		(void)sbdsp_wdsp(sc->sc_iobase, SB_DSP_HALT);

	isa_dmaabort(sc->sc_drq);
	sc->dmaaddr = 0;
	sc->dmacnt = 0;
	sc->sc_locked = 0;
	sc->dmaflags = 0;
	sc->sc_dmain_inprogress = sc->sc_dmaout_inprogress = 0;
	return(0);
}

int
sbdsp_contdma(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	DPRINTF(("sbdsp_contdma: sc=0x%x\n", sc));

	/* XXX how do we reinitialize the DMA controller state?  do we care? */
	(void)sbdsp_wdsp(sc->sc_iobase, SB_DSP_CONT);
	return(0);
}

/*
 * Time constant routines follow.  See SBK, section 12.
 * Although they don't come out and say it (in the docs),
 * the card clearly uses a 1MHz countdown timer, as the
 * low-speed formula (p. 12-4) is:
 *	tc = 256 - 10^6 / sr
 * In high-speed mode, the constant is the upper byte of a 16-bit counter,
 * and a 256MHz clock is used:
 *	tc = 65536 - 256 * 10^ 6 / sr
 * Since we can only use the upper byte of the HS TC, the two formulae
 * are equivalent.  (Why didn't they say so?)  E.g.,
 * 	(65536 - 256 * 10 ^ 6 / x) >> 8 = 256 - 10^6 / x
 *
 * The crossover point (from low- to high-speed modes) is different
 * for the SBPRO and SB20.  The table on p. 12-5 gives the following data:
 *
 *				SBPRO			SB20
 *				-----			--------
 * input ls min			4	KHz		4	KHz
 * input ls max			23	KHz		13	KHz
 * input hs max			44.1	KHz		15	KHz
 * output ls min		4	KHz		4	KHz
 * output ls max		23	KHz		23	KHz
 * output hs max		44.1	KHz		44.1	KHz
 */
#define SB_LS_MIN	0x06	/* 4000 Hz */
#define SBPRO_ADC_LS_MAX	0xd4	/* 22727 Hz */
#define SBPRO_ADC_HS_MAX	0xea	/* 45454 Hz */
#define SBCLA_ADC_LS_MAX	0xb3	/* 12987 Hz */
#define SBCLA_ADC_HS_MAX	0xbd	/* 14925 Hz */
#define SB_DAC_LS_MAX	0xd4	/* 22727 Hz */
#define SB_DAC_HS_MAX	0xea	/* 45454 Hz */

/*
 * Convert a linear sampling rate into the DAC time constant.
 * Set *mode to indicate the high/low-speed DMA operation.
 * Because of limitations of the card, not all rates are possible.
 * We return the time constant of the closest possible rate.
 * The sampling rate limits are different for the DAC and ADC,
 * so isdac indicates output, and !isdac indicates input.
 */
int
sbdsp_srtotc(sc, sr, mode, isdac)
	register struct sbdsp_softc *sc;
	int sr;
	int *mode;
	int isdac;
{
	int adc_ls_max, adc_hs_max;
	register int tc;

	if (sr == 0) {
		*mode = SB_ADAC_LS;
		return SB_LS_MIN;
	}
	tc = 256 - 1000000 / sr;
	
	/* XXX use better rounding--compare distance to nearest tc on both
	   sides of requested speed */
	if (ISSBPROCLASS(sc)) {
		adc_ls_max = SBPRO_ADC_LS_MAX;
		adc_hs_max = SBPRO_ADC_HS_MAX;
	}
	else {
		adc_ls_max = SBCLA_ADC_LS_MAX;
		adc_hs_max = SBCLA_ADC_HS_MAX;
	}
	    
	if (tc < SB_LS_MIN) {
		tc = SB_LS_MIN;
		*mode = SB_ADAC_LS;
	} else if (isdac) {
		if (tc <= SB_DAC_LS_MAX)
			*mode = SB_ADAC_LS;
		else {
			*mode = SB_ADAC_HS;
			if (tc > SB_DAC_HS_MAX)
				tc = SB_DAC_HS_MAX;
		}
	} else {
		if (tc <= adc_ls_max)
			*mode = SB_ADAC_LS;
		else {
			*mode = SB_ADAC_HS;
			if (tc > adc_hs_max)
				tc = adc_hs_max;
		}
	}
	return tc;
}

/*
 * Convert a DAC time constant to a sampling rate.
 * See SBK, section 12.
 */
int
sbdsp_tctosr(sc, tc)
	register struct sbdsp_softc *sc;
	int tc;
{
	int adc;

	if (ISSBPROCLASS(sc))
		adc = SBPRO_ADC_HS_MAX;
	else
		adc = SBCLA_ADC_HS_MAX;
	
	if (tc > adc)
		tc = adc;
	
	return (1000000 / (256 - tc));
}

int
sbdsp_set_sr(sc, srp, isdac)
	register struct sbdsp_softc *sc;
	u_long *srp;
	int isdac;
{
	register int tc;
	int mode;
	int sr = *srp;
	register int iobase;

	/*
	 * A SBPro in stereo mode uses time constants at double the
	 * actual rate.
	 */
	if (ISSBPRO(sc) && sc->sc_chans == 2) {
		if (sr > 22727)
			sr = 22727;	/* Can't bounce it...order of
					   operations may yield bogus
					   sr here. */
		sr *= 2;
	}
	else if (!ISSBPROCLASS(sc) && sc->sc_chans != 1)
		return EINVAL;

	tc = sbdsp_srtotc(sc, sr, &mode, isdac);
	DPRINTF(("sbdsp_set_sr: sc=0x%x sr=%d mode=0x%x\n", sc, sr, mode));

	iobase = sc->sc_iobase;
	if (sbdsp_wdsp(iobase, SB_DSP_TIMECONST) < 0 ||
	    sbdsp_wdsp(iobase, tc) < 0)
		return EIO;
	    
	sr = sbdsp_tctosr(sc, tc);
	if (ISSBPRO(sc) && sc->sc_chans == 2)
		*srp = sr / 2;
	else
		*srp = sr;

	sc->sc_adacmode = mode;
	sc->sc_adactc = tc;
	return 0;
}

int
sbdsp_dma_input(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr)();
	void *arg;
{
	register struct sbdsp_softc *sc = addr;
	register int iobase;
	
#ifdef AUDIO_DEBUG
	if (sbdspdebug > 1)
		Dprintf("sbdsp_dma_input: cc=%d 0x%x (0x%x)\n", cc, intr, arg);
#endif
	if (sc->sc_chans == 2 && (cc & 1)) {
		DPRINTF(("sbdsp_dma_input: stereo input, odd bytecnt\n"));
		return EIO;
	}
	iobase = sc->sc_iobase;
	if (ISSBPROCLASS(sc) && !sc->sc_dmain_inprogress) {
		if (sc->sc_chans == 2) {
			if (sbdsp_wdsp(iobase, SB_DSP_RECORD_STEREO) < 0)
				goto badmode;
			sbdsp_mix_write(sc, SBP_STEREO,
					sbdsp_mix_read(sc, SBP_STEREO) & ~SBP_PLAYMODE_MASK);
			sbdsp_mix_write(sc, SBP_INFILTER,
					sbdsp_mix_read(sc, SBP_INFILTER) | SBP_FILTER_OFF);
		}
		else {
			if (sbdsp_wdsp(iobase, SB_DSP_RECORD_MONO) < 0)
				goto badmode;
			sbdsp_mix_write(sc, SBP_STEREO,
					sbdsp_mix_read(sc, SBP_STEREO) & ~SBP_PLAYMODE_MASK);
			sbdsp_mix_write(sc, SBP_INFILTER,
					sc->sc_irate <= 8000 ? 
					sbdsp_mix_read(sc, SBP_INFILTER) & ~SBP_FILTER_MASK :
					sbdsp_mix_read(sc, SBP_INFILTER) | SBP_FILTER_OFF);
		}
		sc->sc_dmain_inprogress = 1;
		sc->sc_last_hsr_size = 0;	/* restarting */
	}
	sc->sc_dmaout_inprogress = 0;

	isa_dmastart(B_READ, p, cc, sc->sc_drq);
	sc->sc_intr = intr;
	sc->sc_arg = arg;
	sc->dmaflags = B_READ;
	sc->dmaaddr = p;
	sc->dmacnt = --cc;		/* DMA controller is strange...? */
	if (sc->sc_adacmode == SB_ADAC_LS) {
		if (sbdsp_wdsp(iobase, SB_DSP_RDMA) < 0 ||
		    sbdsp_wdsp(iobase, cc) < 0 ||
		    sbdsp_wdsp(iobase, cc >> 8) < 0) {
			goto giveup;
		}
	}
	else {
		if (cc != sc->sc_last_hsr_size) {
			if (sbdsp_wdsp(iobase, SB_DSP_BLOCKSIZE) < 0 ||
			    sbdsp_wdsp(iobase, cc) < 0 ||
			    sbdsp_wdsp(iobase, cc >> 8) < 0)
				goto giveup;
		}
		if (sbdsp_wdsp(iobase, SB_DSP_HS_INPUT) < 0)
			goto giveup;
		sc->sc_last_hsr_size = cc;
		sc->sc_locked = 1;
	}
	return 0;

giveup:
	isa_dmaabort(sc->sc_drq);
	sbdsp_reset(sc);
	sc->sc_intr = 0;
	sc->sc_arg = 0;
	return EIO;
badmode:
	DPRINTF(("sbdsp_dma_input: can't set %s mode\n",
		 sc->sc_chans == 2 ? "stereo" : "mono"));
	return EIO;
}

int
sbdsp_dma_output(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr)();
	void *arg;
{
	register struct sbdsp_softc *sc = addr;
	register int iobase;
	
#ifdef AUDIO_DEBUG
	if (sbdspdebug > 1)
		Dprintf("sbdsp_dma_output: cc=%d 0x%x (0x%x)\n", cc, intr, arg);
#endif
	if (sc->sc_chans == 2 && cc & 1) {
		DPRINTF(("stereo playback odd bytes (%d)\n", cc));
		return EIO;
	}

	if (ISSBPROCLASS(sc) && !sc->sc_dmaout_inprogress) {
		/* make sure we re-set stereo mixer bit when we start
		   output. */
		sbdsp_mix_write(sc, SBP_STEREO,
				(sbdsp_mix_read(sc, SBP_STEREO) & ~SBP_PLAYMODE_MASK) |
				(sc->sc_chans == 2 ?
				 SBP_PLAYMODE_STEREO : SBP_PLAYMODE_MONO));
		sc->sc_dmaout_inprogress = 1;
		sc->sc_last_hsw_size = 0;	/* restarting */
	}
	sc->sc_dmain_inprogress = 0;
	isa_dmastart(B_WRITE, p, cc, sc->sc_drq);
	sc->sc_intr = intr;
	sc->sc_arg = arg;
	sc->dmaflags = B_WRITE;
	sc->dmaaddr = p;
	sc->dmacnt = --cc;	/* a vagary of how DMA works, apparently. */

	iobase = sc->sc_iobase;
	if (sc->sc_adacmode == SB_ADAC_LS) {
		if (sbdsp_wdsp(iobase, SB_DSP_WDMA) < 0 ||
		    sbdsp_wdsp(iobase, cc) < 0 ||
		    sbdsp_wdsp(iobase, cc >> 8) < 0) {
		        DPRINTF(("sbdsp_dma_output: LS DMA start failed\n"));
			goto giveup;
		}
	}
	else {
		if (cc != sc->sc_last_hsw_size) {
			if (sbdsp_wdsp(iobase, SB_DSP_BLOCKSIZE) < 0) {
				/* sometimes fails initial startup?? */
				delay(100);
				if (sbdsp_wdsp(iobase, SB_DSP_BLOCKSIZE) < 0) {
					DPRINTF(("sbdsp_dma_output: BLOCKSIZE failed\n"));
					goto giveup;
				}
			}
			if (sbdsp_wdsp(iobase, cc) < 0 ||
			    sbdsp_wdsp(iobase, cc >> 8) < 0) {
				DPRINTF(("sbdsp_dma_output: HS DMA start failed\n"));
				goto giveup;
			}
			sc->sc_last_hsw_size = cc;
		}
		if (sbdsp_wdsp(iobase, SB_DSP_HS_OUTPUT) < 0) {
			delay(100);
			if (sbdsp_wdsp(iobase, SB_DSP_HS_OUTPUT) < 0) {
				DPRINTF(("sbdsp_dma_output: HS DMA restart failed\n"));
				goto giveup;
			}
		}
		sc->sc_locked = 1;
	}

	return 0;

 giveup:
	isa_dmaabort(sc->sc_drq);
	sbdsp_reset(sc);
	sc->sc_intr = 0;
	sc->sc_arg = 0;
	return EIO;
}

/*
 * Only the DSP unit on the sound blaster generates interrupts.
 * There are three cases of interrupt: reception of a midi byte
 * (when mode is enabled), completion of dma transmission, or 
 * completion of a dma reception.  The three modes are mutually
 * exclusive so we know a priori which event has occurred.
 */
int
sbdsp_intr(arg)
	void *arg;
{
	register struct sbdsp_softc *sc = arg;

#ifdef AUDIO_DEBUG
	if (sbdspdebug > 1)
		Dprintf("sbdsp_intr: intr=0x%x\n", sc->sc_intr);
#endif
	sc->sc_interrupts++;
	sc->sc_locked = 0;
	/* clear interrupt */
	inb(sc->sc_iobase + SBP_DSP_RSTAT);
#if 0
	if (sc->sc_mintr != 0) {
		int c = sbdsp_rdsp(sc->sc_iobase);
		(*sc->sc_mintr)(sc->sc_arg, c);
	} else
#endif
	if (sc->sc_intr != 0) {
	    /*
	     * The SBPro used to develop and test this driver often
	     * generated dma underruns--it interrupted to signal
	     * completion of the DMA input recording block, but the
	     * ISA DMA controller didn't think the channel was
	     * finished.  Maybe this is just a bus speed issue, I dunno,
	     * but it seems strange and leads to channel-flipping with stereo
	     * recording.  Sigh.
	     */
		isa_dmadone(sc->dmaflags, sc->dmaaddr, sc->dmacnt,
			    sc->sc_drq);
		sc->dmaflags = 0;
		sc->dmaaddr = 0;
		sc->dmacnt = 0;
		(*sc->sc_intr)(sc->sc_arg);
	}
	else
		return 0;
	return 1;
}

#if 0
/*
 * Enter midi uart mode and arrange for read interrupts
 * to vector to `intr'.  This puts the card in a mode
 * which allows only midi I/O; the card must be reset
 * to leave this mode.  Unfortunately, the card does not
 * use transmit interrupts, so bytes must be output
 * using polling.  To keep the polling overhead to a
 * minimum, output should be driven off a timer.
 * This is a little tricky since only 320us separate
 * consecutive midi bytes.
 */
void
sbdsp_set_midi_mode(sc, intr, arg)
	struct sbdsp_softc *sc;
	void (*intr)();
	void *arg;
{

	sbdsp_wdsp(sc->sc_iobase, SB_MIDI_UART_INTR);
	sc->sc_mintr = intr;
	sc->sc_intr = 0;
	sc->sc_arg = arg;
}

/*
 * Write a byte to the midi port, when in midi uart mode.
 */
void
sbdsp_midi_output(sc, v)
	struct sbdsp_softc *sc;
	int v;
{

	if (sbdsp_wdsp(sc->sc_iobase, v) < 0)
		++sberr.wmidi;
}
#endif

u_int
sbdsp_get_silence(enc)
    int enc;
{
#define ULAW_SILENCE	0x7f
#define LINEAR_SILENCE	0
    u_int auzero;
    
    switch (enc) {
    case AUDIO_ENCODING_ULAW:
	auzero = ULAW_SILENCE; 
	break;
    case AUDIO_ENCODING_PCM16:
    default:
	auzero = LINEAR_SILENCE;
	break;
    }

    return(auzero);
}

int
sbdsp_setfd(addr, flag)
	void *addr;
	int flag;
{
	/* Can't do full-duplex */
	return(ENOTTY);
}

int
sbdsp_mixer_set_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    register struct sbdsp_softc *sc = addr;
    int error = 0;
    int src, gain;
    int left, right;
    
    DPRINTF(("sbdsp_mixer_set_port: port=%d num_channels=%d\n", cp->dev, cp->un.value.num_channels));

    /*
     * Everything is a value except for SBPro special OUTPUT_MODE and
     * RECORD_SOURCE
     */
    if (cp->type != AUDIO_MIXER_VALUE) {
	if (!ISSBPROCLASS(sc) || (cp->dev != SB_OUTPUT_MODE &&
				  cp->dev != SB_RECORD_SOURCE))
	    return EINVAL;
    }
    else {
	/*
	 * All the mixer ports are stereo except for the microphone.
	 * If we get a single-channel gain value passed in, then we
	 * duplicate it to both left and right channels.
	 */
    if (cp->un.value.num_channels == 2) {
	left  = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
	right = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
    }
    else
	    left = right = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
    }    
    
    if (ISSBPROCLASS(sc)) {
	/* The _PORT things are all signal inputs to the mixer.
	 * Here we are tweaking their mixing level.
	 *
	 * We can also tweak the output stage volume (MASTER_VOL)
	 */
	gain = sbdsp_stereo_vol(SBP_AGAIN_TO_SBGAIN(left),
				SBP_AGAIN_TO_SBGAIN(right));
	switch(cp->dev) {
        case SB_MIC_PORT:
	    src = SBP_MIC_VOL;
	    if (cp->un.value.num_channels != 1)
		error = EINVAL;
	    else
		/* handle funny microphone gain */
		gain = SBP_AGAIN_TO_MICGAIN(left);
	    break;
        case SB_LINE_IN_PORT:
	    src = SBP_LINE_VOL;
	    break;
        case SB_DAC_PORT:
	    src = SBP_DAC_VOL;
	    break;
        case SB_FM_PORT:
	    src = SBP_FM_VOL;
	    break;
        case SB_CD_PORT:
	    src = SBP_CD_VOL;
	    break;
	case SB_SPEAKER:
	    cp->dev = SB_MASTER_VOL;
        case SB_MASTER_VOL:
	    src = SBP_MASTER_VOL;
	    break;
#if 0
	case SB_OUTPUT_MODE:
	    if (cp->type == AUDIO_MIXER_ENUM)
		return sbdsp_set_channels(addr, cp->un.ord);
	    /* fall through...carefully! */
#endif
	case SB_RECORD_SOURCE:
	    if (cp->type == AUDIO_MIXER_ENUM)
		return sbdsp_set_in_port(addr, cp->un.ord);
	    /* else fall through: bad input */
        case SB_TREBLE:
        case SB_BASS:
        default:
	    error =  EINVAL;
	    break;
	}
	if (!error)
	sbdsp_mix_write(sc, src, gain);
    }    
    else if (cp->dev != SB_MIC_PORT &&
	     cp->dev != SB_SPEAKER)
	error = EINVAL;

    if (!error)
	sc->gain[cp->dev] = gain;

    return(error);
}

int
sbdsp_mixer_get_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    register struct sbdsp_softc *sc = addr;
    int error = 0;
    int done = 0;
    
    DPRINTF(("sbdsp_mixer_get_port: port=%d", cp->dev));

    if (ISSBPROCLASS(sc))
    switch(cp->dev) {
    case SB_MIC_PORT:
	    if (cp->un.value.num_channels == 1) {
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
		    SBP_MICGAIN_TO_AGAIN(sc->gain[cp->dev]);
		return 0;
    }
	    else
		return EINVAL;
	    break;
	case SB_LINE_IN_PORT:
        case SB_DAC_PORT:
        case SB_FM_PORT:
        case SB_CD_PORT:
        case SB_MASTER_VOL:
	    break;
	case SB_SPEAKER:
	    cp->dev = SB_MASTER_VOL;
	    break;
        default:
	    error =  EINVAL;
	    break;
	}
    else {
	if (cp->un.value.num_channels != 1) /* no stereo on SB classic */
	    error = EINVAL;
    else
	    switch(cp->dev) {
	    case SB_MIC_PORT:
		break;
	    case SB_SPEAKER:
		break;
	    default:
	error = EINVAL;
		break;
	    }
    }
    if (error == 0) {
	if (cp->un.value.num_channels == 1) {
	    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
		SBP_SBGAIN_TO_AGAIN(sc->gain[cp->dev]);
	}
	else if (cp->un.value.num_channels == 2) {
	    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
		SBP_LEFTGAIN(sc->gain[cp->dev]);
	    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
		SBP_RIGHTGAIN(sc->gain[cp->dev]);
	} else
	    return EINVAL;
    }
    return(error);
}

int
sbdsp_mixer_query_devinfo(addr, dip)
    void *addr;
    register mixer_devinfo_t *dip;
{
    register struct sbdsp_softc *sc = addr;
    int done = 0;

    DPRINTF(("sbdsp_mixer_query_devinfo: index=%d\n", dip->index));

    switch (dip->index) {
    case SB_MIC_PORT:
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = SB_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNmicrophone);
	dip->un.v.num_channels = 1;
	strcpy(dip->un.v.units.name, AudioNvolume);
	done = 1;
	break;
    case SB_SPEAKER:
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = SB_OUTPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNspeaker);
	dip->un.v.num_channels = 1;
	strcpy(dip->un.v.units.name, AudioNvolume);
	done = 1;
	break;
    case SB_INPUT_CLASS:
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = SB_INPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCInputs);
	done = 1;
	break;
    case SB_OUTPUT_CLASS:
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = SB_OUTPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCOutputs);
	done = 1;
	break;
    }

    if (!done) {
    if (ISSBPROCLASS(sc))
	switch(dip->index) {
	case SB_LINE_IN_PORT:
	    dip->type = AUDIO_MIXER_VALUE;
	    dip->mixer_class = SB_INPUT_CLASS;
	    dip->prev = AUDIO_MIXER_LAST;
	    dip->next = AUDIO_MIXER_LAST;
	    strcpy(dip->label.name, AudioNline);
	    dip->un.v.num_channels = 2;
	    strcpy(dip->un.v.units.name, AudioNvolume);
	    break;
	case SB_DAC_PORT:
	    dip->type = AUDIO_MIXER_VALUE;
	    dip->mixer_class = SB_OUTPUT_CLASS;
	    dip->prev = AUDIO_MIXER_LAST;
	    dip->next = AUDIO_MIXER_LAST;
	    strcpy(dip->label.name, AudioNdac);
	    dip->un.v.num_channels = 2;
	    strcpy(dip->un.v.units.name, AudioNvolume);
	    break;
	case SB_CD_PORT:
	    dip->type = AUDIO_MIXER_VALUE;
	    dip->mixer_class = SB_INPUT_CLASS;
	    dip->prev = AUDIO_MIXER_LAST;
	    dip->next = AUDIO_MIXER_LAST;
	    strcpy(dip->label.name, AudioNcd);
	    dip->un.v.num_channels = 2;
	    strcpy(dip->un.v.units.name, AudioNvolume);
	    break;
	case SB_FM_PORT:
	    dip->type = AUDIO_MIXER_VALUE;
	    dip->mixer_class = SB_OUTPUT_CLASS;
	    dip->prev = AUDIO_MIXER_LAST;
	    dip->next = AUDIO_MIXER_LAST;
	    strcpy(dip->label.name, AudioNfmsynth);
	    dip->un.v.num_channels = 2;
	    strcpy(dip->un.v.units.name, AudioNvolume);
	    break;
	case SB_MASTER_VOL:
	    dip->type = AUDIO_MIXER_VALUE;
	    dip->mixer_class = SB_OUTPUT_CLASS;
	    dip->prev = AUDIO_MIXER_LAST;
	    dip->next = /*TREBLE, BASS not handled, nor is SB_OUTPUT_MODE*/SB_RECORD_SOURCE;
	    strcpy(dip->label.name, AudioNvolume);
	    dip->un.v.num_channels = 2;
	    strcpy(dip->un.v.units.name, AudioNvolume);
	    break;
#if 0
	case SB_OUTPUT_MODE:
	    dip->mixer_class = SB_OUTPUT_CLASS;
	    dip->type = AUDIO_MIXER_ENUM;
	    dip->prev = SB_MASTER_VOL;
	    dip->next = AUDIO_MIXER_LAST;
	    strcpy(dip->label.name, AudioNmode);
	    dip->un.e.num_mem = 2;
	    strcpy(dip->un.e.member[0].label.name, AudioNmono);
	    dip->un.e.member[0].ord = 1; /* nchans */
	    strcpy(dip->un.e.member[1].label.name, AudioNstereo);
	    dip->un.e.member[1].ord = 2; /* nchans */
	    break;
#endif
	case SB_RECORD_SOURCE:
	    dip->mixer_class = SB_RECORD_CLASS;
	    dip->type = AUDIO_MIXER_ENUM;
	    dip->prev = AUDIO_MIXER_LAST;
	    dip->next = AUDIO_MIXER_LAST;
	    strcpy(dip->label.name, AudioNsource);
	    dip->un.e.num_mem = 3;
	    strcpy(dip->un.e.member[0].label.name, AudioNmicrophone);
	    dip->un.e.member[0].ord = SB_MIC_PORT;
	    strcpy(dip->un.e.member[1].label.name, AudioNcd);
	    dip->un.e.member[1].ord = SB_CD_PORT;
	    strcpy(dip->un.e.member[2].label.name, AudioNline);
	    dip->un.e.member[2].ord = SB_LINE_IN_PORT;
	    break;
	case SB_BASS:
	case SB_TREBLE:
	default:
	    return ENXIO;
	    /*NOTREACHED*/
	} 
    else
	return ENXIO;
    }

    DPRINTF(("AUDIO_MIXER_DEVINFO: name=%s\n", dip->label.name));

    return 0;
}
