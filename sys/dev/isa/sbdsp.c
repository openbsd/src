/*	$OpenBSD: sbdsp.c,v 1.5 1996/05/07 07:37:41 deraadt Exp $	*/
/*	$NetBSD: sbdsp.c,v 1.25 1996/04/29 20:03:31 christos Exp $	*/

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

int sbdsp_srtotc __P((struct sbdsp_softc *sc, int sr, int isdac,
		      int *tcp, int *modep));
u_int sbdsp_jazz16_probe __P((struct sbdsp_softc *));

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
#define	SB_8K		0x83	/* 8000 Hz */
#define SBPRO_ADC_LS_MAX	0xd4	/* 22727 Hz */
#define SBPRO_ADC_HS_MAX	0xea	/* 45454 Hz */
#define SBCLA_ADC_LS_MAX	0xb3	/* 12987 Hz */
#define SBCLA_ADC_HS_MAX	0xbd	/* 14925 Hz */
#define SB_DAC_LS_MAX	0xd4	/* 22727 Hz */
#define SB_DAC_HS_MAX	0xea	/* 45454 Hz */

int	sbdsp16_wait __P((int));
void	sbdsp_to __P((void *));
void	sbdsp_pause __P((struct sbdsp_softc *));
int	sbdsp_setrate __P((struct sbdsp_softc *, int, int, int *));
int	sbdsp_tctosr __P((struct sbdsp_softc *, int));
int	sbdsp_set_timeconst __P((struct sbdsp_softc *, int));

#ifdef AUDIO_DEBUG
void sb_printsc __P((struct sbdsp_softc *));
#endif

#ifdef AUDIO_DEBUG
void
sb_printsc(sc)
	struct sbdsp_softc *sc;
{
	int i;
    
	printf("open %d dmachan %d iobase %x\n",
	    sc->sc_open, sc->sc_drq, sc->sc_iobase);
	printf("irate %d itc %d imode %d orate %d otc %d omode %d encoding %x\n",
	    sc->sc_irate, sc->sc_itc, sc->sc_imode,
	    sc->sc_orate, sc->sc_otc, sc->sc_omode, sc->encoding);
	printf("outport %d inport %d spkron %d nintr %d\n",
	    sc->out_port, sc->in_port, sc->spkr_state, sc->sc_interrupts);
	printf("precision %d channels %d intr %x arg %x\n",
	    sc->sc_precision, sc->sc_channels, sc->sc_intr, sc->sc_arg);
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

	if (sbdsp_reset(sc) < 0) {
		DPRINTF(("sbdsp: couldn't reset card\n"));
		return 0;
	}
	/* if flags set, go and probe the jazz16 stuff */
	if (sc->sc_dev.dv_cfdata->cf_flags != 0)
		sc->sc_model = sbdsp_jazz16_probe(sc);
	else
		sc->sc_model = sbversion(sc);

	return 1;
}

/*
 * Try add-on stuff for Jazz16.
 */
u_int
sbdsp_jazz16_probe(sc)
	struct sbdsp_softc *sc;
{
	static u_char jazz16_irq_conf[16] = {
	    -1, -1, 0x02, 0x03,
	    -1, 0x01, -1, 0x04,
	    -1, 0x02, 0x05, -1,
	    -1, -1, -1, 0x06};
	static u_char jazz16_drq_conf[8] = {
	    -1, 0x01, -1, 0x02,
	    -1, 0x03, -1, 0x04};

	u_int rval = sbversion(sc);
	register int iobase = sc->sc_iobase;

	if (jazz16_drq_conf[sc->sc_drq] == (u_char)-1 ||
	    jazz16_irq_conf[sc->sc_irq] == (u_char)-1)
		return rval;		/* give up, we can't do it. */
	outb(JAZZ16_CONFIG_PORT, JAZZ16_WAKEUP);
	delay(10000);			/* delay 10 ms */
	outb(JAZZ16_CONFIG_PORT, JAZZ16_SETBASE);
	outb(JAZZ16_CONFIG_PORT, iobase & 0x70);

	if (sbdsp_reset(sc) < 0)
		return rval;		/* XXX? what else could we do? */

	if (sbdsp_wdsp(iobase, JAZZ16_READ_VER))
		return rval;
	if (sbdsp_rdsp(iobase) != JAZZ16_VER_JAZZ)
		return rval;

	if (sbdsp_wdsp(iobase, JAZZ16_SET_DMAINTR) ||
	    /* set both 8 & 16-bit drq to same channel, it works fine. */
	    sbdsp_wdsp(iobase,
		       (jazz16_drq_conf[sc->sc_drq] << 4) |
		       jazz16_drq_conf[sc->sc_drq]) ||
	    sbdsp_wdsp(iobase, jazz16_irq_conf[sc->sc_irq])) {
		DPRINTF(("sbdsp: can't write jazz16 probe stuff"));
		return rval;
	}
	return (rval | MODEL_JAZZ16);
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
sbdsp_attach(sc)
	struct sbdsp_softc *sc;
{

	/* Set defaults */
	if (ISSB16CLASS(sc))
		sc->sc_irate = sc->sc_orate = 8000;
	else if (ISSBPROCLASS(sc))
		sc->sc_itc = sc->sc_otc = SB_8K;
  	else
		sc->sc_itc = sc->sc_otc = SB_8K;
	sc->sc_precision = 8;
	sc->sc_channels = 1;
	sc->encoding = AUDIO_ENCODING_ULAW;

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
				sbdsp_stereo_vol(SBP_MAXVOL/2, SBP_MAXVOL/2));
		sbdsp_mix_write(sc, SBP_LINE_VOL,
				sbdsp_stereo_vol(SBP_MAXVOL, SBP_MAXVOL));
		for (i = 0; i < SB_NDEVS; i++)
			sc->gain[i] = sbdsp_stereo_vol(SBP_MAXVOL, SBP_MAXVOL);
		sc->in_filter = 0;	/* no filters turned on, please */
	}

	printf(": dsp v%d.%02d%s\n",
	       SBVER_MAJOR(sc->sc_model), SBVER_MINOR(sc->sc_model),
	       ISJAZZ16(sc) ? ": <Jazz16>" : "");

#ifdef notyet
	sbdsp_mix_write(sc, SBP_SET_IRQ, 0x04);
	sbdsp_mix_write(sc, SBP_SET_DRQ, 0x22);

	printf("sbdsp_attach: irq=%02x, drq=%02x\n",
	    sbdsp_mix_read(sc, SBP_SET_IRQ),
	    sbdsp_mix_read(sc, SBP_SET_DRQ));
#else
	if (ISSB16CLASS(sc))
		sc->sc_model = 0x0300;
#endif
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

	if (ISSB16CLASS(sc))
		return (sbdsp_setrate(sc, sr, SB_INPUT_RATE, &sc->sc_irate));
	else
		return (sbdsp_srtotc(sc, sr, SB_INPUT_RATE, &sc->sc_itc, &sc->sc_imode));
}

u_long
sbdsp_get_in_sr(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	if (ISSB16CLASS(sc))
		return (sc->sc_irate);
	else
		return (sbdsp_tctosr(sc, sc->sc_itc));
}

int
sbdsp_set_out_sr(addr, sr)
	void *addr;
	u_long sr;
{
	register struct sbdsp_softc *sc = addr;

	if (ISSB16CLASS(sc))
		return (sbdsp_setrate(sc, sr, SB_OUTPUT_RATE, &sc->sc_orate));
	else
		return (sbdsp_srtotc(sc, sr, SB_OUTPUT_RATE, &sc->sc_otc, &sc->sc_omode));
}

u_long
sbdsp_get_out_sr(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	if (ISSB16CLASS(sc))
		return (sc->sc_orate);
	else
		return (sbdsp_tctosr(sc, sc->sc_otc));
}

int
sbdsp_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
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
		return (EINVAL);
	}
	return (0);
}

int
sbdsp_set_encoding(addr, encoding)
	void *addr;
	u_int encoding;
{
	register struct sbdsp_softc *sc = addr;
	
	switch (encoding) {
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

	return (sc->encoding);
}

int
sbdsp_set_precision(addr, precision)
	void *addr;
	u_int precision;
{
	register struct sbdsp_softc *sc = addr;

	if (ISSB16CLASS(sc) || ISJAZZ16(sc)) {
		if (precision != 16 && precision != 8)
			return (EINVAL);
		sc->sc_precision = precision;
	} else {
		if (precision != 8)
			return (EINVAL);
		sc->sc_precision = precision;
	}

	return (0);
}

int
sbdsp_get_precision(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	return (sc->sc_precision);
}

int
sbdsp_set_channels(addr, channels)
	void *addr;
	int channels;
{
	register struct sbdsp_softc *sc = addr;

	if (ISSBPROCLASS(sc)) {
		if (channels != 1 && channels != 2)
			return (EINVAL);
		sc->sc_channels = channels;
		sc->sc_dmadir = SB_DMA_NONE;
		/*
		 * XXXX
		 * With 2 channels, SBPro can't do more than 22kHz.
		 * No framework to check this.
		 */
	} else {
		if (channels != 1)
			return (EINVAL);
		sc->sc_channels = channels;
	}
	
	return (0);
}

int
sbdsp_get_channels(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;
	
	return (sc->sc_channels);
}

int
sbdsp_set_ifilter(addr, which)
	void *addr;
	int which;
{
	register struct sbdsp_softc *sc = addr;
	int mixval;

	if (ISSBPROCLASS(sc)) {
		mixval = sbdsp_mix_read(sc, SBP_INFILTER) & ~SBP_IFILTER_MASK;
		switch (which) {
		case 0:
			mixval |= SBP_FILTER_OFF;
			break;
		case SBP_TREBLE_EQ:
			mixval |= SBP_FILTER_ON | SBP_IFILTER_HIGH;
			break;
		case SBP_BASS_EQ:
			mixval |= SBP_FILTER_ON | SBP_IFILTER_LOW; 
			break;
		default:
			return (EINVAL);
		}
		sc->in_filter = mixval & SBP_IFILTER_MASK;
		sbdsp_mix_write(sc, SBP_INFILTER, mixval);
		return (0);
	} else
		return (EINVAL);
}

int
sbdsp_get_ifilter(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;
	
	if (ISSBPROCLASS(sc)) {
		sc->in_filter =
		    sbdsp_mix_read(sc, SBP_INFILTER) & SBP_IFILTER_MASK;
		switch (sc->in_filter) {
		case SBP_FILTER_ON|SBP_IFILTER_HIGH:
			return (SBP_TREBLE_EQ);
		case SBP_FILTER_ON|SBP_IFILTER_LOW:
			return (SBP_BASS_EQ);
		case SBP_FILTER_OFF:
		default:
			return (0);
		}
	} else
		return (0);
}

int
sbdsp_set_out_port(addr, port)
	void *addr;
	int port;
{
	register struct sbdsp_softc *sc = addr;
	
	sc->out_port = port; /* Just record it */

	return (0);
}

int
sbdsp_get_out_port(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	return (sc->out_port);
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
			return (EINVAL);
		}
	} else {
		switch (port) {
		case SB_MIC_PORT:
			sbport = SBP_FROM_MIC;
			mixport = SBP_MIC_VOL;
			break;
		default:
			return (EINVAL);
		}
	}	    

	sc->in_port = port;	/* Just record it */

	if (ISSBPROCLASS(sc)) {
		/* record from that port */
		sbdsp_mix_write(sc, SBP_RECORD_SOURCE,
		    SBP_RECORD_FROM(sbport, SBP_FILTER_OFF, SBP_IFILTER_HIGH));
		/* fetch gain from that port */
		sc->gain[port] = sbdsp_mix_read(sc, mixport);
	}

	return (0);
}

int
sbdsp_get_in_port(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	return (sc->in_port);
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

	sc->sc_last_hs_size = 0;

	/* Higher speeds need bigger blocks to avoid popping and silence gaps. */
	if (blk < NBPG/4 || blk > NBPG/2) {
		if (ISSB16CLASS(sc)) {
			if (sc->sc_orate > 8000 || sc->sc_irate > 8000)
				blk = NBPG/2;
		} else {
			if (sc->sc_otc > SB_8K || sc->sc_itc < SB_8K)
				blk = NBPG/2;
		}
	}
	/* don't try to DMA too much at once, though. */
	if (blk > NBPG)
		blk = NBPG;
	if (sc->sc_channels == 2)
		return (blk & ~1); /* must be even to preserve stereo separation */
	else
		return (blk);	/* Anything goes :-) */
}

int
sbdsp_commit_settings(addr)
	void *addr;
{
	register struct sbdsp_softc *sc = addr;

	/* due to potentially unfortunate ordering in the above layers,
	   re-do a few sets which may be important--input gains
	   (adjust the proper channels), number of input channels (hit the
	   record rate and set mode) */

	if (ISSBPRO(sc)) {
		/*
		 * With 2 channels, SBPro can't do more than 22kHz.
		 * Whack the rates down to speed if necessary.
		 * Reset the time constant anyway
		 * because it may have been adjusted with a different number
		 * of channels, which means it might have computed the wrong
		 * mode (low/high speed).
		 */
		if (sc->sc_channels == 2 &&
		    sbdsp_tctosr(sc, sc->sc_itc) > 22727) {
			sbdsp_srtotc(sc, 22727, SB_INPUT_RATE,
				     &sc->sc_itc, &sc->sc_imode);
		} else
			sbdsp_srtotc(sc, sbdsp_tctosr(sc, sc->sc_itc),
				     SB_INPUT_RATE, &sc->sc_itc,
				     &sc->sc_imode);

		if (sc->sc_channels == 2 &&
		    sbdsp_tctosr(sc, sc->sc_otc) > 22727) {
			sbdsp_srtotc(sc, 22727, SB_OUTPUT_RATE,
				     &sc->sc_otc, &sc->sc_omode);
		} else
			sbdsp_srtotc(sc, sbdsp_tctosr(sc, sc->sc_otc),
				     SB_OUTPUT_RATE, &sc->sc_otc,
				     &sc->sc_omode);
	}
	if (ISSB16CLASS(sc) || ISJAZZ16(sc)) {
		if (sc->encoding == AUDIO_ENCODING_ULAW &&
		    sc->sc_precision == 16) {
			sc->sc_precision = 8;
			return EINVAL;	/* XXX what should we really do? */
		}
	}
	/*
	 * XXX
	 * Should wait for chip to be idle.
	 */
	sc->sc_dmadir = SB_DMA_NONE;

	return 0;
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
	if (ISSBPROCLASS(sc) &&
	    sbdsp_wdsp(sc->sc_iobase, SB_DSP_RECORD_MONO) < 0) {
		DPRINTF(("sbdsp_open: can't set mono mode\n"));
		/* we'll readjust when it's time for DMA. */
	}

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
	sc->sc_mintr = 0;
	sbdsp_haltdma(sc);

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

	sc->sc_intr = 0;
	if (sc->sc_dmadir != SB_DMA_NONE) {
		isa_dmaabort(sc->sc_drq);
		sc->sc_dmadir = SB_DMA_NONE;
	}
	sc->sc_last_hs_size = 0;

	/*
	 * See SBK, section 11.3.
	 * We pulse a reset signal into the card.
	 * Gee, what a brilliant hardware design.
	 */
	outb(iobase + SBP_DSP_RESET, 1);
	delay(10);
	outb(iobase + SBP_DSP_RESET, 0);
	delay(30);
	if (sbdsp_rdsp(iobase) != SB_MAGIC)
		return -1;

	return 0;
}

int
sbdsp16_wait(iobase)
	int iobase;
{
	register int i;

	for (i = SBDSP_NPOLL; --i >= 0; ) {
		register u_char x;
		x = inb(iobase + SBP_DSP_WSTAT);
		delay(10);
		if ((x & SB_DSP_BUSY) == 0)
			continue;
		return 0;
	}
	++sberr.wdsp;
	return -1;
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
		register u_char x;
		x = inb(iobase + SBP_DSP_WSTAT);
		delay(10);
		if ((x & SB_DSP_BUSY) != 0)
			continue;
		outb(iobase + SBP_DSP_WRITE, v);
		delay(10);
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
		register u_char x;
		x = inb(iobase + SBP_DSP_RSTAT);
		delay(10);
		if ((x & SB_DSP_READY) == 0)
			continue;
		x = inb(iobase + SBP_DSP_READ);
		delay(10);
		return x;
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

	sbdsp_reset(sc);
	return 0;
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

int
sbdsp_setrate(sc, sr, isdac, ratep)
	register struct sbdsp_softc *sc;
	int sr;
	int isdac;
	int *ratep;
{

	/*
	 * XXXX
	 * More checks here?
	 */
	if (sr < 5000 || sr > 44100)
		return (EINVAL);
	*ratep = sr;
	return (0);
}

/*
 * Convert a linear sampling rate into the DAC time constant.
 * Set *mode to indicate the high/low-speed DMA operation.
 * Because of limitations of the card, not all rates are possible.
 * We return the time constant of the closest possible rate.
 * The sampling rate limits are different for the DAC and ADC,
 * so isdac indicates output, and !isdac indicates input.
 */
int
sbdsp_srtotc(sc, sr, isdac, tcp, modep)
	register struct sbdsp_softc *sc;
	int sr;
	int isdac;
	int *tcp, *modep;
{
	int tc, realtc, mode;

	/*
	 * Don't forget to compute which mode we'll be in based on whether
	 * we need to double the rate for stereo on SBPRO.
	 */
	 
	if (sr == 0) {
		tc = SB_LS_MIN;
		mode = SB_ADAC_LS;
		goto out;
	}

	tc = 256 - (1000000 / sr);

	if (sc->sc_channels == 2 && ISSBPRO(sc))
		/* compute based on 2x sample rate when needed */
		realtc = 256 - ( 500000 / sr);
	else
		realtc = tc;
	
	if (tc < SB_LS_MIN) {
		tc = SB_LS_MIN;
		mode = SB_ADAC_LS;	/* NB: 2x minimum speed is still low
					 * speed mode. */
		goto out;
	} else if (isdac) {
		if (realtc <= SB_DAC_LS_MAX)
			mode = SB_ADAC_LS;
		else {
			mode = SB_ADAC_HS;
			if (tc > SB_DAC_HS_MAX)
				tc = SB_DAC_HS_MAX;
		}
	} else {
		int adc_ls_max, adc_hs_max;

		/* XXX use better rounding--compare distance to nearest tc on both
		   sides of requested speed */
		if (ISSBPROCLASS(sc)) {
			adc_ls_max = SBPRO_ADC_LS_MAX;
			adc_hs_max = SBPRO_ADC_HS_MAX;
		} else {
			adc_ls_max = SBCLA_ADC_LS_MAX;
			adc_hs_max = SBCLA_ADC_HS_MAX;
		}
	    
		if (realtc <= adc_ls_max)
			mode = SB_ADAC_LS;
		else {
			mode = SB_ADAC_HS;
			if (tc > adc_hs_max)
				tc = adc_hs_max;
		}
	}

out:
	*tcp = tc;
	*modep = mode;
	return (0);
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
sbdsp_set_timeconst(sc, tc)
	register struct sbdsp_softc *sc;
	int tc;
{
	register int iobase;

	/*
	 * A SBPro in stereo mode uses time constants at double the
	 * actual rate.
	 */
	if (ISSBPRO(sc) && sc->sc_channels == 2)
		tc = 256 - ((256 - tc) / 2);

	DPRINTF(("sbdsp_set_timeconst: sc=%p tc=%d\n", sc, tc));

	iobase = sc->sc_iobase;
	if (sbdsp_wdsp(iobase, SB_DSP_TIMECONST) < 0 ||
	    sbdsp_wdsp(iobase, tc) < 0)
		return (EIO);
	    
	return (0);
}

int
sbdsp_dma_input(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr) __P((void *));
	void *arg;
{
	register struct sbdsp_softc *sc = addr;
	register int iobase;
	
#ifdef AUDIO_DEBUG
	if (sbdspdebug > 1)
		Dprintf("sbdsp_dma_input: cc=%d 0x%x (0x%x)\n", cc, intr, arg);
#endif
	if (sc->sc_channels == 2 && (cc & 1)) {
		DPRINTF(("sbdsp_dma_input: stereo input, odd bytecnt\n"));
		return EIO;
	}

	iobase = sc->sc_iobase;
	if (sc->sc_dmadir != SB_DMA_IN) {
		if (ISSBPRO(sc)) {
			if (sc->sc_channels == 2) {
				if (ISJAZZ16(sc) && sc->sc_precision == 16) {
					if (sbdsp_wdsp(iobase,
						       JAZZ16_RECORD_STEREO) < 0) {
						goto badmode;
					} 
				} else if (sbdsp_wdsp(iobase,
						      SB_DSP_RECORD_STEREO) < 0)
					goto badmode;
				sbdsp_mix_write(sc, SBP_INFILTER,
				    (sbdsp_mix_read(sc, SBP_INFILTER) &
				    ~SBP_IFILTER_MASK) | SBP_FILTER_OFF);
			} else {
				if (ISJAZZ16(sc) && sc->sc_precision == 16) {
					if (sbdsp_wdsp(iobase,
						       JAZZ16_RECORD_MONO) < 0)
					{
						goto badmode;
					}
				} else if (sbdsp_wdsp(iobase, SB_DSP_RECORD_MONO) < 0)
					goto badmode;
				sbdsp_mix_write(sc, SBP_INFILTER,
				    (sbdsp_mix_read(sc, SBP_INFILTER) &
				    ~SBP_IFILTER_MASK) | sc->in_filter);
			}
		}

		if (ISSB16CLASS(sc)) {
			if (sbdsp_wdsp(iobase, SB_DSP16_INPUTRATE) < 0 ||
			    sbdsp_wdsp(iobase, sc->sc_irate >> 8) < 0 ||
			    sbdsp_wdsp(iobase, sc->sc_irate) < 0)
				goto giveup;
		} else
			sbdsp_set_timeconst(sc, sc->sc_itc);
		sc->sc_dmadir = SB_DMA_IN;
	}

	isa_dmastart(DMAMODE_READ, p, cc, sc->sc_drq);
	sc->sc_intr = intr;
	sc->sc_arg = arg;
	sc->dmaflags = DMAMODE_READ;
	sc->dmaaddr = p;
	sc->dmacnt = cc;		/* DMA controller is strange...? */

	if ((ISSB16CLASS(sc) && sc->sc_precision == 16) ||
	    (ISJAZZ16(sc) && sc->sc_drq > 3))
		cc >>= 1;
	--cc;
	if (ISSB16CLASS(sc)) {
		if (sbdsp_wdsp(iobase, sc->sc_precision == 16 ? SB_DSP16_RDMA_16 :
								SB_DSP16_RDMA_8) < 0 ||
		    sbdsp_wdsp(iobase, (sc->sc_precision == 16 ? 0x10 : 0x00) |
				       (sc->sc_channels == 2 ? 0x20 : 0x00)) < 0 ||
		    sbdsp16_wait(iobase) ||
		    sbdsp_wdsp(iobase, cc) < 0 ||
		    sbdsp_wdsp(iobase, cc >> 8) < 0) {
			DPRINTF(("sbdsp_dma_input: SB16 DMA start failed\n"));
			goto giveup;
		}
	} else if (sc->sc_imode == SB_ADAC_LS) {
		if (sbdsp_wdsp(iobase, SB_DSP_RDMA) < 0 ||
		    sbdsp_wdsp(iobase, cc) < 0 ||
		    sbdsp_wdsp(iobase, cc >> 8) < 0) {
		        DPRINTF(("sbdsp_dma_input: LS DMA start failed\n"));
			goto giveup;
		}
	} else {
		if (cc != sc->sc_last_hs_size) {
			if (sbdsp_wdsp(iobase, SB_DSP_BLOCKSIZE) < 0 ||
			    sbdsp_wdsp(iobase, cc) < 0 ||
			    sbdsp_wdsp(iobase, cc >> 8) < 0) {
				DPRINTF(("sbdsp_dma_input: HS DMA start failed\n"));
				goto giveup;
			}
			sc->sc_last_hs_size = cc;
		}
		if (sbdsp_wdsp(iobase, SB_DSP_HS_INPUT) < 0) {
			DPRINTF(("sbdsp_dma_input: HS DMA restart failed\n"));
			goto giveup;
		}
	}
	return 0;

giveup:
	sbdsp_reset(sc);
	return EIO;

badmode:
	DPRINTF(("sbdsp_dma_input: can't set %s mode\n",
		 sc->sc_channels == 2 ? "stereo" : "mono"));
	return EIO;
}

int
sbdsp_dma_output(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr) __P((void *));
	void *arg;
{
	register struct sbdsp_softc *sc = addr;
	register int iobase;
	
#ifdef AUDIO_DEBUG
	if (sbdspdebug > 1)
		Dprintf("sbdsp_dma_output: cc=%d 0x%x (0x%x)\n", cc, intr, arg);
#endif
	if (sc->sc_channels == 2 && (cc & 1)) {
		DPRINTF(("stereo playback odd bytes (%d)\n", cc));
		return EIO;
	}

	iobase = sc->sc_iobase;
	if (sc->sc_dmadir != SB_DMA_OUT) {
		if (ISSBPRO(sc)) {
			/* make sure we re-set stereo mixer bit when we start
			   output. */
			sbdsp_mix_write(sc, SBP_STEREO,
			    (sbdsp_mix_read(sc, SBP_STEREO) & ~SBP_PLAYMODE_MASK) |
			    (sc->sc_channels == 2 ?  SBP_PLAYMODE_STEREO : SBP_PLAYMODE_MONO));
			if (ISJAZZ16(sc)) {
				/* Yes, we write the record mode to set
				   16-bit playback mode. weird, huh? */
				if (sc->sc_precision == 16) {
					sbdsp_wdsp(iobase,
						   sc->sc_channels == 2 ?
						   JAZZ16_RECORD_STEREO :
						   JAZZ16_RECORD_MONO);
				} else {
					sbdsp_wdsp(iobase,
						   sc->sc_channels == 2 ?
						   SB_DSP_RECORD_STEREO :
						   SB_DSP_RECORD_MONO);
				}
			}
		}

		if (ISSB16CLASS(sc)) {
			if (sbdsp_wdsp(iobase, SB_DSP16_OUTPUTRATE) < 0 ||
			    sbdsp_wdsp(iobase, sc->sc_orate >> 8) < 0 ||
			    sbdsp_wdsp(iobase, sc->sc_orate) < 0)
				goto giveup;
		} else
			sbdsp_set_timeconst(sc, sc->sc_otc);
		sc->sc_dmadir = SB_DMA_OUT;
	}

	isa_dmastart(DMAMODE_WRITE, p, cc, sc->sc_drq);
	sc->sc_intr = intr;
	sc->sc_arg = arg;
	sc->dmaflags = DMAMODE_WRITE;
	sc->dmaaddr = p;
	sc->dmacnt = cc;	/* a vagary of how DMA works, apparently. */

	if ((ISSB16CLASS(sc) && sc->sc_precision == 16) ||
	    (ISJAZZ16(sc) && sc->sc_drq > 3))
		cc >>= 1;
	--cc;
	if (ISSB16CLASS(sc)) {
		if (sbdsp_wdsp(iobase, sc->sc_precision == 16 ? SB_DSP16_WDMA_16 :
								SB_DSP16_WDMA_8) < 0 ||
		    sbdsp_wdsp(iobase, (sc->sc_precision == 16 ? 0x10 : 0x00) |
				       (sc->sc_channels == 2 ? 0x20 : 0x00)) < 0 ||
		    sbdsp16_wait(iobase) ||
		    sbdsp_wdsp(iobase, cc) < 0 ||
		    sbdsp_wdsp(iobase, cc >> 8) < 0) {
			DPRINTF(("sbdsp_dma_output: SB16 DMA start failed\n"));
			goto giveup;
		}
	} else if (sc->sc_omode == SB_ADAC_LS) {
		if (sbdsp_wdsp(iobase, SB_DSP_WDMA) < 0 ||
		    sbdsp_wdsp(iobase, cc) < 0 ||
		    sbdsp_wdsp(iobase, cc >> 8) < 0) {
		        DPRINTF(("sbdsp_dma_output: LS DMA start failed\n"));
			goto giveup;
		}
	} else {
		if (cc != sc->sc_last_hs_size) {
			if (sbdsp_wdsp(iobase, SB_DSP_BLOCKSIZE) < 0 ||
			    sbdsp_wdsp(iobase, cc) < 0 ||
			    sbdsp_wdsp(iobase, cc >> 8) < 0) {
				DPRINTF(("sbdsp_dma_output: HS DMA start failed\n"));
				goto giveup;
			}
			sc->sc_last_hs_size = cc;
		}
		if (sbdsp_wdsp(iobase, SB_DSP_HS_OUTPUT) < 0) {
			DPRINTF(("sbdsp_dma_output: HS DMA restart failed\n"));
			goto giveup;
		}
	}
	return 0;

giveup:
	sbdsp_reset(sc);
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
	u_char x;

#ifdef AUDIO_DEBUG
	if (sbdspdebug > 1)
		Dprintf("sbdsp_intr: intr=0x%x\n", sc->sc_intr);
#endif
	if (!isa_dmafinished(sc->sc_drq)) {
		printf("sbdsp_intr: not finished\n");
		return 0;
	}
	sc->sc_interrupts++;
	/* clear interrupt */
#ifdef notyet
	x = sbdsp_mix_read(sc, 0x82);
	x = inb(sc->sc_iobase + 15);
#endif
	x = inb(sc->sc_iobase + SBP_DSP_RSTAT);
	delay(10);
#if 0
	if (sc->sc_mintr != 0) {
		x = sbdsp_rdsp(sc->sc_iobase);
		(*sc->sc_mintr)(sc->sc_arg, x);
	} else
#endif
	if (sc->sc_intr != 0) {
		isa_dmadone(sc->dmaflags, sc->dmaaddr, sc->dmacnt, sc->sc_drq);
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
sbdsp_get_silence(encoding)
	int encoding;
{
#define ULAW_SILENCE	0x7f
#define LINEAR_SILENCE	0
	u_int auzero;
    
	switch (encoding) {
	case AUDIO_ENCODING_ULAW:
		auzero = ULAW_SILENCE; 
		break;
	case AUDIO_ENCODING_PCM16:
	default:
		auzero = LINEAR_SILENCE;
		break;
	}

	return (auzero);
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
	int src, gain;
    
	DPRINTF(("sbdsp_mixer_set_port: port=%d num_channels=%d\n", cp->dev,
	    cp->un.value.num_channels));

	if (!ISSBPROCLASS(sc))
		return EINVAL;

	/*
	 * Everything is a value except for SBPro BASS/TREBLE and
	 * RECORD_SOURCE
	 */
	switch (cp->dev) {
	case SB_SPEAKER:
		cp->dev = SB_MASTER_VOL;
	case SB_MIC_PORT:
	case SB_LINE_IN_PORT:
	case SB_DAC_PORT:
	case SB_FM_PORT:
	case SB_CD_PORT:
	case SB_MASTER_VOL:
		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		/*
		 * All the mixer ports are stereo except for the microphone.
		 * If we get a single-channel gain value passed in, then we
		 * duplicate it to both left and right channels.
		 */

		switch (cp->dev) {
		case SB_MIC_PORT:
			if (cp->un.value.num_channels != 1)
				return EINVAL;

			/* handle funny microphone gain */
			gain = SBP_AGAIN_TO_MICGAIN(cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			break;
		case SB_LINE_IN_PORT:
		case SB_DAC_PORT:
		case SB_FM_PORT:
		case SB_CD_PORT:
		case SB_MASTER_VOL:
			switch (cp->un.value.num_channels) {
			case 1:
				gain = sbdsp_mono_vol(SBP_AGAIN_TO_SBGAIN(cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]));
				break;
			case 2:
				gain = sbdsp_stereo_vol(SBP_AGAIN_TO_SBGAIN(cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]),
							SBP_AGAIN_TO_SBGAIN(cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT]));
				break;
			default:
				return EINVAL;
			}
			break;
		default:
			return EINVAL;
		}

		switch (cp->dev) {
		case SB_MIC_PORT:
			src = SBP_MIC_VOL;
			break;
		case SB_MASTER_VOL:
			src = SBP_MASTER_VOL;
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
		default:
			return EINVAL;
		}

		sbdsp_mix_write(sc, src, gain);
		sc->gain[cp->dev] = gain;
		break;

	case SB_TREBLE:
	case SB_BASS:
	case SB_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		switch (cp->dev) {
		case SB_TREBLE:
			return sbdsp_set_ifilter(addr, cp->un.ord ? SBP_TREBLE_EQ : 0);
		case SB_BASS:
			return sbdsp_set_ifilter(addr, cp->un.ord ? SBP_BASS_EQ : 0);
		case SB_RECORD_SOURCE:
			return sbdsp_set_in_port(addr, cp->un.ord);
		}

		break;

	default:
		return EINVAL;
	}

	return (0);
}

int
sbdsp_mixer_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	register struct sbdsp_softc *sc = addr;
	int gain;
    
	DPRINTF(("sbdsp_mixer_get_port: port=%d", cp->dev));

	if (!ISSBPROCLASS(sc))
		return EINVAL;

	switch (cp->dev) {
	case SB_SPEAKER:
		cp->dev = SB_MASTER_VOL;
	case SB_MIC_PORT:
	case SB_LINE_IN_PORT:
	case SB_DAC_PORT:
	case SB_FM_PORT:
	case SB_CD_PORT:
	case SB_MASTER_VOL:
		gain = sc->gain[cp->dev];

		switch (cp->dev) {
		case SB_MIC_PORT:
			if (cp->un.value.num_channels != 1)
				return EINVAL;

			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = SBP_MICGAIN_TO_AGAIN(gain);
			break;
		case SB_LINE_IN_PORT:
		case SB_DAC_PORT:
		case SB_FM_PORT:
		case SB_CD_PORT:
		case SB_MASTER_VOL:
			switch (cp->un.value.num_channels) {
			case 1:
				cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = SBP_SBGAIN_TO_AGAIN(gain);
				break;
			case 2:
				cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = SBP_LEFTGAIN(gain);
				cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = SBP_RIGHTGAIN(gain);
				break;
			default:
				return EINVAL;
			}
			break;
		}

		break;

	case SB_TREBLE:
	case SB_BASS:
	case SB_RECORD_SOURCE:
		switch (cp->dev) {
		case SB_TREBLE:
			cp->un.ord = sbdsp_get_ifilter(addr) == SBP_TREBLE_EQ;
			return 0;
		case SB_BASS:
			cp->un.ord = sbdsp_get_ifilter(addr) == SBP_BASS_EQ;
			return 0;
		case SB_RECORD_SOURCE:
			cp->un.ord = sbdsp_get_in_port(addr);
			return 0;
		}

		break;

	default:
		return EINVAL;
	}

	return (0);
}

int
sbdsp_mixer_query_devinfo(addr, dip)
	void *addr;
	register mixer_devinfo_t *dip;
{
	register struct sbdsp_softc *sc = addr;

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
		return 0;

	case SB_SPEAKER:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = SB_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNspeaker);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case SB_INPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = SB_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCInputs);
		return 0;

	case SB_OUTPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = SB_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCOutputs);
		return 0;
	}

	if (ISSBPROCLASS(sc)) {
		switch (dip->index) {
		case SB_LINE_IN_PORT:
			dip->type = AUDIO_MIXER_VALUE;
			dip->mixer_class = SB_INPUT_CLASS;
			dip->prev = AUDIO_MIXER_LAST;
			dip->next = AUDIO_MIXER_LAST;
			strcpy(dip->label.name, AudioNline);
			dip->un.v.num_channels = 2;
			strcpy(dip->un.v.units.name, AudioNvolume);
			return 0;

		case SB_DAC_PORT:
			dip->type = AUDIO_MIXER_VALUE;
			dip->mixer_class = SB_INPUT_CLASS;
			dip->prev = AUDIO_MIXER_LAST;
			dip->next = AUDIO_MIXER_LAST;
			strcpy(dip->label.name, AudioNdac);
			dip->un.v.num_channels = 2;
			strcpy(dip->un.v.units.name, AudioNvolume);
			return 0;

		case SB_CD_PORT:
			dip->type = AUDIO_MIXER_VALUE;
			dip->mixer_class = SB_INPUT_CLASS;
			dip->prev = AUDIO_MIXER_LAST;
			dip->next = AUDIO_MIXER_LAST;
			strcpy(dip->label.name, AudioNcd);
			dip->un.v.num_channels = 2;
			strcpy(dip->un.v.units.name, AudioNvolume);
			return 0;

		case SB_FM_PORT:
			dip->type = AUDIO_MIXER_VALUE;
			dip->mixer_class = SB_INPUT_CLASS;
			dip->prev = AUDIO_MIXER_LAST;
			dip->next = AUDIO_MIXER_LAST;
			strcpy(dip->label.name, AudioNfmsynth);
			dip->un.v.num_channels = 2;
			strcpy(dip->un.v.units.name, AudioNvolume);
			return 0;

		case SB_MASTER_VOL:
			dip->type = AUDIO_MIXER_VALUE;
			dip->mixer_class = SB_OUTPUT_CLASS;
			dip->prev = AUDIO_MIXER_LAST;
			dip->next = AUDIO_MIXER_LAST;
			strcpy(dip->label.name, AudioNvolume);
			dip->un.v.num_channels = 2;
			strcpy(dip->un.v.units.name, AudioNvolume);
			return 0;

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
			return 0;

		case SB_BASS:
			dip->type = AUDIO_MIXER_ENUM;
			dip->mixer_class = SB_INPUT_CLASS;
			dip->prev = AUDIO_MIXER_LAST;
			dip->next = AUDIO_MIXER_LAST;
			strcpy(dip->label.name, AudioNbass);
			dip->un.e.num_mem = 2;
			strcpy(dip->un.e.member[0].label.name, AudioNoff);
			dip->un.e.member[0].ord = 0;
			strcpy(dip->un.e.member[1].label.name, AudioNon);
			dip->un.e.member[1].ord = 1;
			return 0;

		case SB_TREBLE:
			dip->type = AUDIO_MIXER_ENUM;
			dip->mixer_class = SB_INPUT_CLASS;
			dip->prev = AUDIO_MIXER_LAST;
			dip->next = AUDIO_MIXER_LAST;
			strcpy(dip->label.name, AudioNtreble);
			dip->un.e.num_mem = 2;
			strcpy(dip->un.e.member[0].label.name, AudioNoff);
			dip->un.e.member[0].ord = 0;
			strcpy(dip->un.e.member[1].label.name, AudioNon);
			dip->un.e.member[1].ord = 1;
			return 0;

		case SB_RECORD_CLASS:			/* record source class */
			dip->type = AUDIO_MIXER_CLASS;
			dip->mixer_class = SB_RECORD_CLASS;
			dip->next = dip->prev = AUDIO_MIXER_LAST;
			strcpy(dip->label.name, AudioCRecord);
			return 0;
		} 
	}

	return ENXIO;
}
