/*	$OpenBSD: sb.c,v 1.16 1998/11/03 21:15:01 downsj Exp $	*/
/*	$NetBSD: sb.c,v 1.57 1998/01/12 09:43:46 thorpej Exp $	*/

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
#include <machine/bus.h>
#include <machine/pio.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/sbreg.h>
#include <dev/isa/sbvar.h>
#include <dev/isa/sbdspvar.h>

struct cfdriver sb_cd = {
	NULL, "sb", DV_DULL
};

struct audio_device sb_device = {
	"SoundBlaster",
	"x",
	"sb"
};

int	sb_getdev __P((void *, struct audio_device *));

/*
 * Define our interface to the higher level audio driver.
 */

struct audio_hw_if sb_hw_if = {
	sbdsp_open,
	sbdsp_close,
	NULL,
	sbdsp_query_encoding,
	sbdsp_set_params,
	sbdsp_round_blocksize,
	NULL,
	sbdsp_dma_init_output,
	sbdsp_dma_init_input,
	sbdsp_dma_output,
	sbdsp_dma_input,
	sbdsp_haltdma,
	sbdsp_haltdma,
	sbdsp_speaker_ctl,
	sb_getdev,
	NULL,
	sbdsp_mixer_set_port,
	sbdsp_mixer_get_port,
	sbdsp_mixer_query_devinfo,
	sb_malloc,
	sb_free,
	sb_round,
        sb_mappage,
	sbdsp_get_props,
	NULL,
	NULL
};

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (sbdebug) printf x
int	sbdebug = 0;
#else
#define DPRINTF(x)
#endif

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
			DPRINTF(("%s: configured dma chan %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_drq8));
			return 0;
		}
	} else {
		if (!SB_DRQ_VALID(sc->sc_drq8)) {
			DPRINTF(("%s: configured dma chan %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_drq8));
			return 0;
		}
	}

        if (0 <= sc->sc_drq16 && sc->sc_drq16 <= 3)
        	/* 
                 * XXX Some ViBRA16 cards seem to have two 8 bit DMA 
                 * channels.  I've no clue how to use them, so ignore
                 * one of them for now.  -- augustss@netbsd.org
                 */
        	sc->sc_drq16 = -1;

	if (ISSB16CLASS(sc)) {
		if (sc->sc_drq16 == -1)
			sc->sc_drq16 = sc->sc_drq8;
		if (!SB16_DRQ_VALID(sc->sc_drq16)) {
			DPRINTF(("%s: configured dma chan %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_drq16));
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
				DPRINTF(("%s: couldn't auto-detect interrupt\n", sc->sc_dev.dv_xname));
				return 0;
			}
		}
		else {
			if (!SB_IRQ_VALID(sc->sc_irq)) {
				DPRINTF(("%s: couldn't auto-detect interrupt\n", sc->sc_dev.dv_xname));
				return 0;
			}
		}
	} else
#endif
	if (ISSBPROCLASS(sc)) {
		if (!SBP_IRQ_VALID(sc->sc_irq)) {
			DPRINTF(("%s: configured irq %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_irq));
			return 0;
		}
	} else {
		if (!SB_IRQ_VALID(sc->sc_irq)) {
			DPRINTF(("%s: configured irq %d invalid\n",
			    sc->sc_dev.dv_xname, sc->sc_irq));
			return 0;
		}
	}

	if (ISSB16CLASS(sc)) {
		int w, r;
#if 0
		DPRINTF(("%s: old drq conf %02x\n", sc->sc_dev.dv_xname,
		    sbdsp_mix_read(sc, SBP_SET_DRQ)));
		DPRINTF(("%s: try drq conf %02x\n", sc->sc_dev.dv_xname,
		    drq_conf[sc->sc_drq16] | drq_conf[sc->sc_drq8]));
#endif
		w = drq_conf[sc->sc_drq16] | drq_conf[sc->sc_drq8];
		sbdsp_mix_write(sc, SBP_SET_DRQ, w);
		r = sbdsp_mix_read(sc, SBP_SET_DRQ) & 0xeb;
		if (r != w) {
			DPRINTF(("%s: setting drq mask %02x failed, got %02x\n", sc->sc_dev.dv_xname, w, r));
			return 0;
		}
#if 0
		DPRINTF(("%s: new drq conf %02x\n", sc->sc_dev.dv_xname,
		    sbdsp_mix_read(sc, SBP_SET_DRQ)));
#endif

#if 0
		DPRINTF(("%s: old irq conf %02x\n", sc->sc_dev.dv_xname,
		    sbdsp_mix_read(sc, SBP_SET_IRQ)));
		DPRINTF(("%s: try irq conf %02x\n", sc->sc_dev.dv_xname,
		    irq_conf[sc->sc_irq]));
#endif
		w = irq_conf[sc->sc_irq];
		sbdsp_mix_write(sc, SBP_SET_IRQ, w);
		r = sbdsp_mix_read(sc, SBP_SET_IRQ) & 0x0f;
		if (r != w) {
			DPRINTF(("%s: setting irq mask %02x failed, got %02x\n",
			    sc->sc_dev.dv_xname, w, r));
			return 0;
		}
#if 0
		DPRINTF(("%s: new irq conf %02x\n", sc->sc_dev.dv_xname,
		    sbdsp_mix_read(sc, SBP_SET_IRQ)));
#endif
	}

	return 1;
}


void
sbattach(sc)
	struct sbdsp_softc *sc;
{
	sc->sc_ih = isa_intr_establish(sc->sc_ic, sc->sc_irq, IST_EDGE,
	    IPL_AUDIO, sbdsp_intr, sc, sc->sc_dev.dv_xname);

	sbdsp_attach(sc);

	audio_attach_mi(&sb_hw_if, 0, sc, &sc->sc_dev);
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
sb_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	struct sbdsp_softc *sc = addr;
	static char *names[] = SB_NAMES;
	char *config;

	if (sc->sc_model == SB_JAZZ)
		strncpy(retp->name, "MV Jazz16", sizeof(retp->name));
	else
		strncpy(retp->name, "SoundBlaster", sizeof(retp->name));
	sprintf(retp->version, "%d.%02d", 
		SBVER_MAJOR(sc->sc_version),
		SBVER_MINOR(sc->sc_version));
	if (0 <= sc->sc_model && sc->sc_model < sizeof names / sizeof names[0])
		config = names[sc->sc_model];
	else
		config = "??";
	strncpy(retp->config, config, sizeof(retp->config));
		
	return 0;
}
