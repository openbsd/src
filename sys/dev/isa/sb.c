/*	$OpenBSD: sb.c,v 1.13 1997/08/07 05:27:32 deraadt Exp $	*/
/*	$NetBSD: sb.c,v 1.36 1996/05/12 23:53:33 mycroft Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/pio.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/sbreg.h>
#include <dev/isa/sbvar.h>
#include <dev/isa/sbdspvar.h>

struct sb_softc {
	struct	device sc_dev;		/* base device */
	struct	isadev sc_id;		/* ISA device */
	void	*sc_ih;			/* interrupt vectoring */

	struct	sbdsp_softc sc_sbdsp;
};

struct cfdriver sb_cd = {
	NULL, "sb", DV_DULL
};

struct audio_device sb_device = {
	"SoundBlaster",
	"x",
	"sb"
};

int	sbopen __P((dev_t, int));
int	sb_getdev __P((void *, struct audio_device *));

/*
 * Define our interface to the higher level audio driver.
 */

struct audio_hw_if sb_hw_if = {
	sbopen,
	sbdsp_close,
	NULL,
	sbdsp_set_in_sr,
	sbdsp_get_in_sr,
	sbdsp_set_out_sr,
	sbdsp_get_out_sr,
	sbdsp_query_encoding,
	sbdsp_set_format,
	sbdsp_get_encoding,
	sbdsp_get_precision,
	sbdsp_set_channels,
	sbdsp_get_channels,
	sbdsp_round_blocksize,
	sbdsp_set_out_port,
	sbdsp_get_out_port,
	sbdsp_set_in_port,
	sbdsp_get_in_port,
	sbdsp_commit_settings,
	mulaw_expand,
	mulaw_compress,
	sbdsp_dma_output,
	sbdsp_dma_input,
	sbdsp_haltdma,
	sbdsp_haltdma,
	sbdsp_contdma,
	sbdsp_contdma,
	sbdsp_speaker_ctl,
	sb_getdev,
	sbdsp_setfd,
	sbdsp_mixer_set_port,
	sbdsp_mixer_get_port,
	sbdsp_mixer_query_devinfo,
	0,	/* not full-duplex */
	0
};

/*
 * Probe / attach routines.
 */


int
sbmatch(sc)
	struct sbdsp_softc *sc;
{
	static u_char drq_conf[8] = {
		0x01, 0x02, -1, 0x08, -1, 0x20, 0x40, 0x80
	};

	static u_char irq_conf[11] = {
		-1, -1, 0x01, -1, -1, 0x02, -1, 0x04, -1, 0x01, 0x08
	};

	if (sbdsp_probe(sc) == 0)
		return 0;

	/*
	 * Cannot auto-discover DMA channel.
	 */
	if (ISSBPROCLASS(sc)) {
		if (!SBP_DRQ_VALID(sc->sc_drq8)) {
			printf("%s: configured dma chan %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_drq8);
			return 0;
		}
	} else {
		if (!SB_DRQ_VALID(sc->sc_drq8)) {
			printf("%s: configured dma chan %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_drq8);
			return 0;
		}
	}
	
	if (ISSB16CLASS(sc)) {
		if (sc->sc_drq16 == -1)
			sc->sc_drq16 = sc->sc_drq8;
		if (!SB16_DRQ_VALID(sc->sc_drq16)) {
			printf("%s: configured dma chan %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_drq16);
			return 0;
		}
	} else
		sc->sc_drq16 = sc->sc_drq8;
	
#ifdef NEWCONFIG
	/*
	 * If the IRQ wasn't compiled in, auto-detect it.
	 */
	if (sc->sc_irq == IRQUNK) {
		sc->sc_irq = isa_discoverintr(sbforceintr, sc);
		sbdsp_reset(sc);
		if (ISSBPROCLASS(sc)) {
			if (!SBP_IRQ_VALID(sc->sc_irq)) {
				printf("%s: couldn't auto-detect interrupt\n",
					sc->sc_dev.dv_xname);
				return 0;
			}
		}
		else {
			if (!SB_IRQ_VALID(sc->sc_irq)) {
				printf("%s: couldn't auto-detect interrupt\n");
					sc->sc_dev.dv_xname);
				return 0;
			}
		}
	} else
#endif
	if (ISSBPROCLASS(sc)) {
		if (!SBP_IRQ_VALID(sc->sc_irq)) {
			printf("%s: configured irq %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_irq);
			return 0;
		}
	} else {
		if (!SB_IRQ_VALID(sc->sc_irq)) {
			printf("%s: configured irq %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_irq);
			return 0;
		}
	}

	if (ISSB16CLASS(sc)) {
#if 0
		printf("%s: old drq conf %02x\n", sc->sc_dev.dv_xname,
		    sbdsp_mix_read(sc, SBP_SET_DRQ));
		printf("%s: try drq conf %02x\n", sc->sc_dev.dv_xname,
		    drq_conf[sc->sc_drq16] | drq_conf[sc->sc_drq8]);
#endif
		sbdsp_mix_write(sc, SBP_SET_DRQ,
		    drq_conf[sc->sc_drq16] | drq_conf[sc->sc_drq8]);
#if 0
		printf("%s: new drq conf %02x\n", sc->sc_dev.dv_xname,
		    sbdsp_mix_read(sc, SBP_SET_DRQ));
#endif

#if 0
		printf("%s: old irq conf %02x\n", sc->sc_dev.dv_xname,
		    sbdsp_mix_read(sc, SBP_SET_IRQ));
		printf("%s: try irq conf %02x\n", sc->sc_dev.dv_xname,
		    irq_conf[sc->sc_irq]);
#endif
		sbdsp_mix_write(sc, SBP_SET_IRQ,
		    irq_conf[sc->sc_irq]);
#if 0
		printf("%s: new irq conf %02x\n", sc->sc_dev.dv_xname,
		    sbdsp_mix_read(sc, SBP_SET_IRQ));
#endif
	}

	return 1;
}


void
sbattach(sc)
	struct sbdsp_softc *sc;
{
	int error;

	sc->sc_ih = isa_intr_establish(sc->sc_ic, sc->sc_irq, IST_EDGE,
	    IPL_AUDIO, sbdsp_intr, sc, sc->sc_dev.dv_xname);

	sbdsp_attach(sc);

	if ((error = audio_hardware_attach(&sb_hw_if, sc)) != 0)
		printf("%s: could not attach to audio device driver (%d)\n",
		    sc->sc_dev.dv_xname, error);
}

#ifdef NEWCONFIG
void
sbforceintr(aux)
	void *aux;
{
	static char dmabuf;
	struct sbdsp_softc *sc = aux;

	/*
	 * Set up a DMA read of one byte.
	 * XXX Note that at this point we haven't called 
	 * at_setup_dmachan().  This is okay because it just
	 * allocates a buffer in case it needs to make a copy,
	 * and it won't need to make a copy for a 1 byte buffer.
	 * (I think that calling at_setup_dmachan() should be optional;
	 * if you don't call it, it will be called the first time
	 * it is needed (and you pay the latency).  Also, you might
	 * never need the buffer anyway.)
	 */
	at_dma(DMAMODE_READ, &dmabuf, 1, sc->sc_drq8);
	if (sbdsp_wdsp(sc, SB_DSP_RDMA) == 0) {
		(void)sbdsp_wdsp(sc, 0);
		(void)sbdsp_wdsp(sc, 0);
	}
}
#endif


/*
 * Various routines to interface to higher level audio driver
 */

int
sbopen(dev, flags)
    dev_t dev;
    int flags;
{
    struct sbdsp_softc *sc;
    int unit = AUDIOUNIT(dev);
    
    if (unit >= sb_cd.cd_ndevs)
	return ENODEV;
    
    sc = sb_cd.cd_devs[unit];
    if (!sc)
	return ENXIO;
    
    return sbdsp_open(sc, dev, flags);
}

int
sb_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	struct sbdsp_softc *sc = addr;

	if (sc->sc_model & MODEL_JAZZ16)
		strncpy(retp->name, "MV Jazz16", sizeof(retp->name));
	else
		strncpy(retp->name, "SoundBlaster", sizeof(retp->name));
	sprintf(retp->version, "%d.%02d", 
		SBVER_MAJOR(sc->sc_model),
		SBVER_MINOR(sc->sc_model));
	strncpy(retp->config, "sb", sizeof(retp->config));
		
	return 0;
}
