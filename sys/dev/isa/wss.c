/*	$NetBSD: wss.c,v 1.7 1995/11/10 04:30:52 mycroft Exp $	*/

/*
 * Copyright (c) 1994 John Brezak
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
#include <sys/buf.h>

#include <machine/cpu.h>
#include <machine/pio.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/ad1848var.h>
#include <dev/isa/wssreg.h>

/*
 * Mixer devices
 */
#define WSS_MIC_IN_LVL		0
#define WSS_LINE_IN_LVL		1
#define WSS_DAC_LVL		2
#define WSS_REC_LVL		3
#define WSS_MON_LVL		4
#define WSS_MIC_IN_MUTE		5
#define WSS_LINE_IN_MUTE	6
#define WSS_DAC_MUTE		7

#define WSS_RECORD_SOURCE	8

/* Classes */
#define WSS_INPUT_CLASS		9
#define WSS_RECORD_CLASS	10
#define WSS_MONITOR_CLASS	11

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (wssdebug) printf x
int	wssdebug = 0;
#else
#define DPRINTF(x)
#endif

struct wss_softc {
	struct	device sc_dev;		/* base device */
	struct	isadev sc_id;		/* ISA device */
	void	*sc_ih;			/* interrupt vectoring */

	struct  ad1848_softc sc_ad1848;
#define wss_irq    sc_ad1848.sc_irq
#define wss_drq    sc_ad1848.sc_drq

	int 	mic_mute, cd_mute, dac_mute;
};

struct audio_device wss_device = {
	"wss,ad1848",
	"",
	"WSS"
};

int	wssprobe();
void	wssattach();
int	wssopen __P((dev_t, int));

int	wss_getdev __P((void *, struct audio_device *));
int	wss_setfd __P((void *, int));

int	wss_set_out_port __P((void *, int));
int	wss_get_out_port __P((void *));
int	wss_set_in_port __P((void *, int));
int	wss_get_in_port __P((void *));
int	wss_mixer_set_port __P((void *, mixer_ctrl_t *));
int	wss_mixer_get_port __P((void *, mixer_ctrl_t *));
int	wss_query_devinfo __P((void *, mixer_devinfo_t *));

/*
 * Define our interface to the higher level audio driver.
 */

struct audio_hw_if wss_hw_if = {
	wssopen,
	ad1848_close,
	NULL,
	ad1848_set_in_sr,
	ad1848_get_in_sr,
	ad1848_set_out_sr,
	ad1848_get_out_sr,
	ad1848_query_encoding,
	ad1848_set_encoding,
	ad1848_get_encoding,
	ad1848_set_precision,
	ad1848_get_precision,
	ad1848_set_channels,
	ad1848_get_channels,
	ad1848_round_blocksize,
	wss_set_out_port,
	wss_get_out_port,
	wss_set_in_port,
	wss_get_in_port,
	ad1848_commit_settings,
	ad1848_get_silence,
	NULL,
	NULL,
	ad1848_dma_output,
	ad1848_dma_input,
	ad1848_halt_out_dma,
	ad1848_halt_in_dma,
	ad1848_cont_out_dma,
	ad1848_cont_in_dma,
	NULL,
	wss_getdev,
	wss_setfd,
	wss_mixer_set_port,
	wss_mixer_get_port,
	wss_query_devinfo,
	0,	/* not full-duplex */
	0
};

#ifndef NEWCONFIG
#define at_dma(flags, ptr, cc, chan)	isa_dmastart(flags, ptr, cc, chan)
#endif

struct cfdriver wsscd = {
	NULL, "wss", wssprobe, wssattach, DV_DULL, sizeof(struct wss_softc)
};

/*
 * Probe for the Microsoft Sound System hardware.
 */
int
wssprobe(parent, self, aux)
    struct device *parent, *self;
    void *aux;
{
    register struct wss_softc *sc = (void *)self;
    register struct isa_attach_args *ia = aux;
    register int iobase = ia->ia_iobase;
    static u_char interrupt_bits[12] = {
	-1, -1, -1, -1, -1, -1, -1, 0x08, -1, 0x10, 0x18, 0x20
    };
    static u_char dma_bits[4] = {1, 2, 0, 3};
    
    if (!WSS_BASE_VALID(ia->ia_iobase)) {
	printf("wss: configured iobase %x invalid\n", ia->ia_iobase);
	return 0;
    }

    sc->sc_ad1848.sc_iobase = iobase;

    /* Is there an ad1848 chip at the WSS iobase ? */
    if (ad1848_probe(&sc->sc_ad1848) == 0)
	return 0;
	
    ia->ia_iosize = WSS_NPORT;

    /* Setup WSS interrupt and DMA */
    if (!WSS_DRQ_VALID(ia->ia_drq)) {
	printf("wss: configured dma chan %d invalid\n", ia->ia_drq);
	return 0;
    }
    sc->wss_drq = ia->ia_drq;

#ifdef NEWCONFIG
    /*
     * If the IRQ wasn't compiled in, auto-detect it.
     */
    if (ia->ia_irq == IRQUNK) {
	ia->ia_irq = isa_discoverintr(ad1848_forceintr, &sc->sc_ad1848);
	if (!WSS_IRQ_VALID(ia->ia_irq)) {
	    printf("wss: couldn't auto-detect interrupt\n");
	    return 0;
	}
    }
    else
#endif
    if (!WSS_IRQ_VALID(ia->ia_irq)) {
	printf("wss: configured interrupt %d invalid\n", ia->ia_irq);
	return 0;
    }

    sc->wss_irq = ia->ia_irq;

    outb(iobase+WSS_CONFIG,
	 (interrupt_bits[ia->ia_irq] | dma_bits[ia->ia_drq]));

    return 1;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
wssattach(parent, self, aux)
    struct device *parent, *self;
    void *aux;
{
    register struct wss_softc *sc = (struct wss_softc *)self;
    struct isa_attach_args *ia = (struct isa_attach_args *)aux;
    register int iobase = ia->ia_iobase;
    int err;
    
    sc->sc_ad1848.sc_recdrq = ia->ia_drq;

#ifdef NEWCONFIG
    isa_establish(&sc->sc_id, &sc->sc_dev);
#endif
    sc->sc_ih = isa_intr_establish(ia->ia_irq, ISA_IST_EDGE, ISA_IPL_AUDIO,
				   ad1848_intr, &sc->sc_ad1848);

    ad1848_attach(&sc->sc_ad1848);
    
    printf(" (vers %d)", inb(iobase+WSS_STATUS) & WSS_VERSMASK);
    printf("\n");

    sc->sc_ad1848.parent = sc;

    if ((err = audio_hardware_attach(&wss_hw_if, &sc->sc_ad1848)) != 0)
	printf("wss: could not attach to audio pseudo-device driver (%d)\n", err);
}

static int
wss_to_vol(cp, vol)
    mixer_ctrl_t *cp;
    struct ad1848_volume *vol;
{
    if (cp->un.value.num_channels == 1) {
	vol->left = vol->right = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
	return(1);
    }
    else if (cp->un.value.num_channels == 2) {
	vol->left  = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
	vol->right = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
	return(1);
    }
    return(0);
}

static int
wss_from_vol(cp, vol)
    mixer_ctrl_t *cp;
    struct ad1848_volume *vol;
{
    if (cp->un.value.num_channels == 1) {
	cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = vol->left;
	return(1);
    }
    else if (cp->un.value.num_channels == 2) {
	cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = vol->left;
	cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = vol->right;
	return(1);
    }
    return(0);
}

int
wssopen(dev, flags)
    dev_t dev;
    int flags;
{
    struct wss_softc *sc;
    int unit = AUDIOUNIT(dev);
    
    if (unit >= wsscd.cd_ndevs)
	return ENODEV;
    
    sc = wsscd.cd_devs[unit];
    if (!sc)
	return ENXIO;
    
    return ad1848_open(&sc->sc_ad1848, dev, flags);
}

int
wss_getdev(addr, retp)
    void *addr;
    struct audio_device *retp;
{
    *retp = wss_device;
    return 0;
}

int
wss_setfd(addr, flag)
    void *addr;
    int flag;
{
    /* Can't do full-duplex */
    return(ENOTTY);
}


int
wss_set_out_port(addr, port)
    void *addr;
    int port;
{
    DPRINTF(("wss_set_out_port:\n"));
    return(EINVAL);
}

int
wss_get_out_port(addr)
    void *addr;
{
    DPRINTF(("wss_get_out_port:\n"));
    return(EINVAL);
}

int
wss_set_in_port(addr, port)
    void *addr;
    int port;
{
    register struct ad1848_softc *ac = addr;
    register struct wss_softc *sc = ac->parent;
	
    DPRINTF(("wss_set_in_port: %d\n", port));

    switch(port) {
    case WSS_MIC_IN_LVL:
	port = MIC_IN_PORT;
	break;
    case WSS_LINE_IN_LVL:
	port = LINE_IN_PORT;
	break;
    case WSS_DAC_LVL:
	port = DAC_IN_PORT;
	break;
    default:
	return(EINVAL);
	/*NOTREACHED*/
    }
    
    return(ad1848_set_rec_port(ac, port));
}

int
wss_get_in_port(addr)
    void *addr;
{
    register struct ad1848_softc *ac = addr;
    register struct wss_softc *sc = ac->parent;
    int port = WSS_MIC_IN_LVL;
    
    switch(ad1848_get_rec_port(ac)) {
    case MIC_IN_PORT:
	port = WSS_MIC_IN_LVL;
	break;
    case LINE_IN_PORT:
	port = WSS_LINE_IN_LVL;
	break;
    case DAC_IN_PORT:
	port = WSS_DAC_LVL;
	break;
    }

    DPRINTF(("wss_get_in_port: %d\n", port));

    return(port);
}

int
wss_mixer_set_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    register struct ad1848_softc *ac = addr;
    register struct wss_softc *sc = ac->parent;
    struct ad1848_volume vol;
    u_char eq;
    int error = EINVAL;
    
    DPRINTF(("wss_mixer_set_port: dev=%d type=%d\n", cp->dev, cp->type));

    switch (cp->dev) {
    case WSS_MIC_IN_LVL:	/* Microphone */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    if (wss_to_vol(cp, &vol))
		error = ad1848_set_aux2_gain(ac, &vol);
	}
	break;
	
    case WSS_MIC_IN_MUTE:	/* Microphone */
	if (cp->type == AUDIO_MIXER_ENUM) {
	    sc->mic_mute = cp->un.ord;
	    DPRINTF(("mic mute %d\n", cp->un.ord));
	    error = 0;
	}
	break;

    case WSS_LINE_IN_LVL:	/* linein/CD */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    if (wss_to_vol(cp, &vol))
		error = ad1848_set_aux1_gain(ac, &vol);
	}
	break;
	
    case WSS_LINE_IN_MUTE:	/* linein/CD */
	if (cp->type == AUDIO_MIXER_ENUM) {
	    sc->cd_mute = cp->un.ord;
	    DPRINTF(("CD mute %d\n", cp->un.ord));
	    error = 0;
	}
	break;

    case WSS_DAC_LVL:		/* dac out */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    if (wss_to_vol(cp, &vol))
		error = ad1848_set_out_gain(ac, &vol);
	}
	break;
	
    case WSS_DAC_MUTE:		/* dac out */
	if (cp->type == AUDIO_MIXER_ENUM) {
	    sc->dac_mute = cp->un.ord;
	    DPRINTF(("DAC mute %d\n", cp->un.ord));
	    error = 0;
	}
	break;

    case WSS_REC_LVL:		/* record level */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    if (wss_to_vol(cp, &vol))
		error = ad1848_set_rec_gain(ac, &vol);
	}
	break;
	
    case WSS_RECORD_SOURCE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    error = ad1848_set_rec_port(ac, cp->un.ord);
	}
	break;

    case WSS_MON_LVL:
	if (cp->type == AUDIO_MIXER_VALUE && cp->un.value.num_channels == 1) {
	    vol.left  = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
	    error = ad1848_set_mon_gain(ac, &vol);
	}
	break;

    default:
	    return ENXIO;
	    /*NOTREACHED*/
    }
    
    return 0;
}

int
wss_mixer_get_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    register struct ad1848_softc *ac = addr;
    register struct wss_softc *sc = ac->parent;
    struct ad1848_volume vol;
    u_char eq;
    int error = EINVAL;
    
    DPRINTF(("wss_mixer_get_port: port=%d\n", cp->dev));

    switch (cp->dev) {
    case WSS_MIC_IN_LVL:	/* Microphone */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    error = ad1848_get_aux2_gain(ac, &vol);
	    if (!error)
		wss_from_vol(cp, &vol);
	}
	break;

    case WSS_MIC_IN_MUTE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    cp->un.ord = sc->mic_mute;
	    error = 0;
	}
	break;

    case WSS_LINE_IN_LVL:	/* linein/CD */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    error = ad1848_get_aux1_gain(ac, &vol);
	    if (!error)
		wss_from_vol(cp, &vol);
	}
	break;

    case WSS_LINE_IN_MUTE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    cp->un.ord = sc->cd_mute;
	    error = 0;
	}
	break;

    case WSS_DAC_LVL:		/* dac out */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    error = ad1848_get_out_gain(ac, &vol);
	    if (!error)
		wss_from_vol(cp, &vol);
	}
	break;

    case WSS_DAC_MUTE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    cp->un.ord = sc->dac_mute;
	    error = 0;
	}
	break;

    case WSS_REC_LVL:		/* record level */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    error = ad1848_get_rec_gain(ac, &vol);
	    if (!error)
		wss_from_vol(cp, &vol);
	}
	break;

    case WSS_RECORD_SOURCE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    cp->un.ord = ad1848_get_rec_port(ac);
	    error = 0;
	}
	break;

    case WSS_MON_LVL:		/* monitor level */
	if (cp->type == AUDIO_MIXER_VALUE && cp->un.value.num_channels == 1) {
	    error = ad1848_get_mon_gain(ac, &vol);
	    if (!error)
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = vol.left;
	}
	break;

    default:
	error = ENXIO;
	break;
    }

    return(error);
}

int
wss_query_devinfo(addr, dip)
    void *addr;
    register mixer_devinfo_t *dip;
{
    register struct ad1848_softc *ac = addr;
    register struct wss_softc *sc = ac->parent;

    DPRINTF(("wss_query_devinfo: index=%d\n", dip->index));

    switch(dip->index) {
    case WSS_MIC_IN_LVL:	/* Microphone */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = WSS_MIC_IN_MUTE;
	strcpy(dip->label.name, AudioNmicrophone);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_LINE_IN_LVL:	/* line/CD */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = WSS_LINE_IN_MUTE;
	strcpy(dip->label.name, AudioNcd);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_DAC_LVL:		/*  dacout */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = WSS_DAC_MUTE;
	strcpy(dip->label.name, AudioNdac);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_REC_LVL:	/* record level */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_RECORD_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = WSS_RECORD_SOURCE;
	strcpy(dip->label.name, AudioNrecord);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_MON_LVL:	/* monitor level */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_MONITOR_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNmonitor);
	dip->un.v.num_channels = 1;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_INPUT_CLASS:			/* input class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCInputs);
	break;

    case WSS_MONITOR_CLASS:			/* monitor class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = WSS_MONITOR_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNmonitor);
	break;
	    
    case WSS_RECORD_CLASS:			/* record source class */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = WSS_RECORD_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNrecord);
	break;
	
    case WSS_MIC_IN_MUTE:
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = WSS_MIC_IN_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;
	
    case WSS_LINE_IN_MUTE:
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = WSS_LINE_IN_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;
	
    case WSS_DAC_MUTE:
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = WSS_DAC_LVL;
	dip->next = AUDIO_MIXER_LAST;
    mute:
	strcpy(dip->label.name, AudioNmute);
	dip->un.e.num_mem = 2;
	strcpy(dip->un.e.member[0].label.name, AudioNoff);
	dip->un.e.member[0].ord = 0;
	strcpy(dip->un.e.member[1].label.name, AudioNon);
	dip->un.e.member[1].ord = 1;
	break;

    case WSS_RECORD_SOURCE:
	dip->mixer_class = WSS_RECORD_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = WSS_REC_LVL;
	dip->next = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNsource);
	dip->un.e.num_mem = 3;
	strcpy(dip->un.e.member[0].label.name, AudioNmicrophone);
	dip->un.e.member[0].ord = WSS_MIC_IN_LVL;
	strcpy(dip->un.e.member[1].label.name, AudioNcd);
	dip->un.e.member[1].ord = WSS_LINE_IN_LVL;
	strcpy(dip->un.e.member[2].label.name, AudioNdac);
	dip->un.e.member[2].ord = WSS_DAC_LVL;
	break;

    default:
	return ENXIO;
	/*NOTREACHED*/
    }
    DPRINTF(("AUDIO_MIXER_DEVINFO: name=%s\n", dip->label.name));

    return 0;
}
