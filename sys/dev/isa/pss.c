/*	$OpenBSD: pss.c,v 1.25 2011/06/29 12:17:40 tedu Exp $ */
/*	$NetBSD: pss.c,v 1.38 1998/01/12 09:43:44 thorpej Exp $	*/

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

/*
 * Copyright (c) 1993 Analog Devices Inc. All rights reserved
 *
 * Portions provided by Marc.Hoffman@analog.com and
 * Greg.Yukna@analog.com .
 *
 */

/*
 * Todo:
 *	- Provide PSS driver to access DSP
 *	- Provide MIDI driver to access MPU
 *	- Finish support for CD drive (Sony and SCSI)
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
#include <machine/intr.h>
#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/ad1848var.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/pssreg.h>

/* XXX Default WSS base */
#define WSS_BASE_ADDRESS 0x0530

/*
 * Mixer devices
 */
#define PSS_MIC_IN_LVL		0
#define PSS_LINE_IN_LVL		1
#define PSS_DAC_LVL		2
#define PSS_REC_LVL		3
#define PSS_MON_LVL		4
#define PSS_MASTER_VOL		5
#define PSS_MASTER_TREBLE	6
#define PSS_MASTER_BASS		7
#define PSS_MIC_IN_MUTE		8
#define PSS_LINE_IN_MUTE	9
#define PSS_DAC_MUTE		10

#define PSS_OUTPUT_MODE		11
#define 	PSS_SPKR_MONO	0
#define 	PSS_SPKR_STEREO	1
#define 	PSS_SPKR_PSEUDO	2
#define 	PSS_SPKR_SPATIAL 3

#define PSS_RECORD_SOURCE	12

/* Classes */
#define PSS_INPUT_CLASS		13
#define PSS_RECORD_CLASS	14
#define PSS_MONITOR_CLASS	15
#define PSS_OUTPUT_CLASS	16


struct pss_softc {
	struct	device sc_dev;		/* base device */
	void	*sc_ih;			/* interrupt vectoring */

	int	sc_iobase;		/* I/O port base address */
	int	sc_drq;			/* dma channel */

	struct	ad1848_softc *ad1848_sc;

	int	out_port;

	struct	ad1848_volume master_volume;
	int	master_mode;

	int	monitor_treble;
	int	monitor_bass;

	int	mic_mute, cd_mute, dac_mute;
};

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (pssdebug) printf x
int	pssdebug = 0;
#else
#define DPRINTF(x)
#endif

int	pssprobe(struct device *, void *, void *);
void	pssattach(struct device *, struct device *, void *);

int	spprobe(struct device *, void *, void *);
void	spattach(struct device *, struct device *, void *);

int	pssintr(void *);

int	pss_speaker_ctl(void *, int);

int	pss_getdev(void *, struct audio_device *);

int	pss_mixer_set_port(void *, mixer_ctrl_t *);
int	pss_mixer_get_port(void *, mixer_ctrl_t *);
int	pss_query_devinfo(void *, mixer_devinfo_t *);

#ifdef PSS_DSP
void	pss_dspwrite(struct pss_softc *, int);
#endif
void	pss_setaddr(int, int);
int	pss_setint(int, int);
int	pss_setdma(int, int);
int	pss_testirq(struct pss_softc *, int);
int	pss_testdma(struct pss_softc *, int);
#ifdef AUDIO_DEBUG
void	pss_dump_regs(struct pss_softc *);
#endif
int	pss_set_master_gain(struct pss_softc *, struct ad1848_volume *);
int	pss_set_master_mode(struct pss_softc *, int);
int	pss_set_treble(struct pss_softc *, u_int);
int	pss_set_bass(struct pss_softc *, u_int);
int	pss_get_master_gain(struct pss_softc *, struct ad1848_volume *);
int	pss_get_master_mode(struct pss_softc *, u_int *);
int	pss_get_treble(struct pss_softc *, u_char *);
int	pss_get_bass(struct pss_softc *, u_char *);

#ifdef AUDIO_DEBUG
void	wss_dump_regs(struct ad1848_softc *);
#endif

/*
 * Define our interface to the higher level audio driver.
 */

struct audio_hw_if pss_audio_if = {
	ad1848_open,
	ad1848_close,
	NULL,
	ad1848_query_encoding,
	ad1848_set_params,
	ad1848_round_blocksize,
	ad1848_commit_settings,
	NULL,
	NULL,
	NULL,
	NULL,
	ad1848_halt_output,
	ad1848_halt_input,
	pss_speaker_ctl,
	pss_getdev,
	NULL,
	pss_mixer_set_port,
	pss_mixer_get_port,
	pss_query_devinfo,
	ad1848_malloc,
	ad1848_free,
	ad1848_round,
	ad1848_mappage,
	ad1848_get_props,
	ad1848_trigger_output,
	ad1848_trigger_input,
	NULL
};


/* Interrupt translation for WSS config */
static u_char wss_interrupt_bits[16] = {
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x08,
    0xff, 0x10, 0x18, 0x20,
    0xff, 0xff, 0xff, 0xff
};
/* ditto for WSS DMA channel */
static u_char wss_dma_bits[4] = {1, 2, 0, 3};

struct cfattach pss_ca = {
	sizeof(struct pss_softc), pssprobe, pssattach
};

struct cfdriver pss_cd = {
	NULL, "pss", DV_DULL, 1
};

struct cfattach sp_ca = {
	sizeof(struct ad1848_softc), spprobe, spattach
};

struct cfdriver sp_cd = {
	NULL, "sp", DV_DULL
};

struct audio_device pss_device = {
	"pss,ad1848",
	"",
	"PSS"
};

#ifdef PSS_DSP
void
pss_dspwrite(sc, data)
	struct pss_softc *sc;
	int data;
{
    int i;
    int pss_base = sc->sc_iobase;

    /*
     * Note! the i<5000000 is an emergency exit. The dsp_command() is sometimes
     * called while interrupts are disabled. This means that the timer is
     * disabled also. However the timeout situation is a abnormal condition.
     * Normally the DSP should be ready to accept commands after just couple of
     * loops.
     */
    for (i = 0; i < 5000000; i++) {
	if (inw(pss_base+PSS_STATUS) & PSS_WRITE_EMPTY) {
	    outw(pss_base+PSS_DATA, data);
	    return;
	}
    }
    printf ("pss: DSP Command (%04x) Timeout.\n", data);
}
#endif /* PSS_DSP */

void
pss_setaddr(addr, configAddr)
	int addr;
	int configAddr;
{
    int val;

    val = inw(configAddr);
    val &= ADDR_MASK;
    val |= (addr << 4);
    outw(configAddr,val);
}

/* pss_setint
 * This function sets the correct bits in the
 * configuration register to
 * enable the chosen interrupt.
 */
int
pss_setint(intNum, configAddress)
	int intNum;
	int configAddress;
{
    int val;

    switch(intNum) {
    case 3:
	val = inw(configAddress);
	val &= INT_MASK;
	val |= INT_3_BITS;
	break;
    case 5:
	val = inw(configAddress);
	val &= INT_MASK;
	val |= INT_5_BITS;
	break;
    case 7:
	val = inw(configAddress);
	val &= INT_MASK;
	val |= INT_7_BITS;
	break;
    case 9:
	val = inw(configAddress);
	val &= INT_MASK;
	val |= INT_9_BITS;
	break;
    case 10:
	val = inw(configAddress);
	val &= INT_MASK;
	val |= INT_10_BITS;
	break;
    case 11:
	val = inw(configAddress);
	val &= INT_MASK;
	val |= INT_11_BITS;
	break;
    case 12:
	val = inw(configAddress);
	val &= INT_MASK;
	val |= INT_12_BITS;
	break;
    default:
	DPRINTF(("pss_setint: invalid irq (%d)\n", intNum));
	return 1;
    }
    outw(configAddress,val);
    return 0;
}

int
pss_setdma(dmaNum, configAddress)
	int dmaNum;
	int configAddress;
{
    int val;

    switch(dmaNum) {
    case 0:
	val = inw(configAddress);
	val &= DMA_MASK;
	val |= DMA_0_BITS;
	break;
    case 1:
	val = inw(configAddress);
	val &= DMA_MASK;
	val |= DMA_1_BITS;
	break;
    case 3:
	val = inw(configAddress);
	val &= DMA_MASK;
	val |= DMA_3_BITS;
	break;
    case 5:
	val = inw(configAddress);
	val &= DMA_MASK;
	val |= DMA_5_BITS;
	break;
    case 6:
	val = inw(configAddress);
	val &= DMA_MASK;
	val |= DMA_6_BITS;
	break;
    case 7:
	val = inw(configAddress);
	val &= DMA_MASK;
	val |= DMA_7_BITS;
	break;
    default:
	DPRINTF(("pss_setdma: invalid drq (%d)\n", dmaNum));
	return 1;
    }
    outw(configAddress, val);
    return 0;
}

/*
 * This function tests an interrupt number to see if
 * it is available. It takes the interrupt button
 * as its argument and returns TRUE if the interrupt
 * is ok.
*/
int
pss_testirq(struct pss_softc *sc, int intNum)
{
    int config = sc->sc_iobase + PSS_CONFIG;
    int val;
    int ret;
    int i;

    /* Set the interrupt bits */
    switch(intNum) {
    case 3:
	val = inw(config);
	val &= INT_MASK;	/* Special: 0 */
	break;
    case 5:
	val = inw(config);
	val &= INT_MASK;
	val |= INT_TEST_BIT | INT_5_BITS;
	break;
    case 7:
	val = inw(config);
	val &= INT_MASK;
	val |= INT_TEST_BIT | INT_7_BITS;
	break;
    case 9:
	val = inw(config);
	val &= INT_MASK;
	val |= INT_TEST_BIT | INT_9_BITS;
	break;
    case 10:
	val = inw(config);
	val &= INT_MASK;
	val |= INT_TEST_BIT | INT_10_BITS;
	break;
    case 11:
	val = inw(config);
	val &= INT_MASK;
	val |= INT_TEST_BIT | INT_11_BITS;
	break;
    case 12:
	val = inw(config);
	val &= INT_MASK;
	val |= INT_TEST_BIT | INT_12_BITS;
	break;
    default:
	DPRINTF(("pss_testirq: invalid irq (%d)\n", intNum));
	return 0;
    }
    outw(config, val);

    /* Check if the interrupt is in use */
    /* Do it a few times in case there is a delay */
    ret = 0;
    for (i = 0; i < 5; i++) {
	val = inw(config);
	if (val & INT_TEST_PASS) {
	    ret = 1;
	    break;
	}
    }

    /* Clear the Test bit and the interrupt bits */
    val = inw(config);
    val &= INT_TEST_BIT_MASK & INT_MASK;
    outw(config, val);
    return(ret);
}

/*
 * This function tests a dma channel to see if
 * it is available. It takes the DMA channel button
 * as its argument and returns TRUE if the channel
 * is ok.
 */
int
pss_testdma(sc, dmaNum)
	struct pss_softc *sc;
	int dmaNum;
{
    int config = sc->sc_iobase + PSS_CONFIG;
    int val;
    int i, ret;

    switch (dmaNum) {
    case 0:
	val = inw(config);
	val &= DMA_MASK;
	val |= DMA_TEST_BIT | DMA_0_BITS;
	break;
    case 1:
	val = inw(config);
	val &= DMA_MASK;
	val |= DMA_TEST_BIT | DMA_1_BITS;
	break;
    case 3:
	val = inw(config);
	val &= DMA_MASK;
	val |= DMA_TEST_BIT | DMA_3_BITS;
	break;
    case 5:
	val = inw(config);
	val &= DMA_MASK;
	val |= DMA_TEST_BIT | DMA_5_BITS;
	break;
    case 6:
	val = inw(config);
	val &= DMA_MASK;
	val |= DMA_TEST_BIT | DMA_6_BITS;
	break;
    case 7:
	val = inw(config);
	val &= DMA_MASK;
	val |= DMA_TEST_BIT | DMA_7_BITS;
	break;
    default:
	DPRINTF(("pss_testdma: invalid drq (%d)\n", dmaNum));
	return 0;
    }
    outw(config, val);

    /* Check if the DMA channel is in use */
    /* Do it a few times in case there is a delay */
    ret = 0;
    for (i = 0; i < 3; i++) {
	val = inw(config);
	if (val & DMA_TEST_PASS) {
	    ret = 1;
	    break;
	}
    }

    /* Clear the Test bit and the DMA bits */
    val = inw(config);
    val &= DMA_TEST_BIT_MASK & DMA_MASK;
    outw(config, val);
    return(ret);
}

#ifdef AUDIO_DEBUG
void
wss_dump_regs(sc)
	struct ad1848_softc *sc;
{

    printf("WSS reg: status=%02x\n",
	   (u_char)inb(sc->sc_iobase-WSS_CODEC+WSS_STATUS));
}

void
pss_dump_regs(sc)
	struct pss_softc *sc;
{

    printf("PSS regs: status=%04x vers=%04x ",
	   (u_short)inw(sc->sc_iobase+PSS_STATUS),
	   (u_short)inw(sc->sc_iobase+PSS_ID_VERS));

    printf("config=%04x wss_config=%04x\n",
	   (u_short)inw(sc->sc_iobase+PSS_CONFIG),
	   (u_short)inw(sc->sc_iobase+PSS_WSS_CONFIG));
}
#endif

/*
 * Probe for the PSS hardware.
 */
int
pssprobe(parent, self, aux)
    struct device *parent;
    void *self;
    void *aux;
{
    struct pss_softc *sc = self;
    struct isa_attach_args *ia = aux;
    int iobase = ia->ia_iobase;

    if (!PSS_BASE_VALID(iobase)) {
	DPRINTF(("pss: configured iobase %x invalid\n", iobase));
	return 0;
    }

    /* Need to probe for iobase when IOBASEUNK {0x220 0x240} */
    if (iobase == IOBASEUNK) {

	iobase = 0x220;
	if ((inw(iobase+PSS_ID_VERS) & 0xff00) == 0x4500)
	    goto pss_found;

	iobase = 0x240;
	if ((inw(iobase+PSS_ID_VERS) & 0xff00) == 0x4500)
	    goto pss_found;

	DPRINTF(("pss: no PSS found (at 0x220 or 0x240)\n"));
	return 0;
    }
    else if ((inw(iobase+PSS_ID_VERS) & 0xff00) != 0x4500) {
	DPRINTF(("pss: not a PSS - %x\n", inw(iobase+PSS_ID_VERS)));
	return 0;
    }

pss_found:
    sc->sc_iobase = iobase;

    /* Clear WSS config */
    pss_setaddr(WSS_BASE_ADDRESS, sc->sc_iobase+PSS_WSS_CONFIG); /* XXX! */
    outb(WSS_BASE_ADDRESS+WSS_CONFIG, 0);

    /* Clear config registers (POR reset state) */
    outw(sc->sc_iobase+PSS_CONFIG, 0);
    outw(sc->sc_iobase+PSS_WSS_CONFIG, 0);
    outw(sc->sc_iobase+SB_CONFIG, 0);
    outw(sc->sc_iobase+MIDI_CONFIG, 0);
    outw(sc->sc_iobase+CD_CONFIG, 0);

    if (ia->ia_irq == IRQUNK) {
	int i;
	for (i = 0; i < 16; i++) {
	    if (pss_testirq(sc, i) != 0)
		break;
	}
	if (i == 16) {
	    DPRINTF(("pss: unable to locate free IRQ channel\n"));
	    return 0;
	}
	else {
	    ia->ia_irq = i;
	    DPRINTF(("pss: found IRQ %d free\n", i));
	}
    }
    else {
	if (pss_testirq(sc, ia->ia_irq) == 0) {
	    DPRINTF(("pss: configured IRQ unavailable (%d)\n", ia->ia_irq));
	    return 0;
	}
    }

    /* XXX Need to deal with DRQUNK */
    if (pss_testdma(sc, ia->ia_drq) == 0) {
	DPRINTF(("pss: configured DMA channel unavailable (%d)\n", ia->ia_drq));
	return 0;
    }

    ia->ia_iosize = PSS_NPORT;

    /* Initialize PSS irq and dma */
    pss_setint(ia->ia_irq, sc->sc_iobase+PSS_CONFIG);
    pss_setdma(sc->sc_drq, sc->sc_iobase+PSS_CONFIG);

    return 1;
}

/*
 * Probe for the Soundport (ad1848)
 */
int
spprobe(parent, match, aux)
    struct device *parent;
    void *match, *aux;
{
    struct ad1848_softc *sc = match;
    struct pss_softc *pc = (void *) parent;
    struct cfdata *cf = (void *)sc->sc_dev.dv_cfdata;
    struct isa_attach_args *ia = aux;
    u_char bits;
    int i;

    sc->sc_iot = ia->ia_iot;
    sc->sc_iobase = cf->cf_iobase + WSS_CODEC;

    /* Set WSS io address */
    pss_setaddr(cf->cf_iobase, pc->sc_iobase+PSS_WSS_CONFIG);

    /* Is there an ad1848 chip at the WSS iobase ? */
    if (ad1848_probe(sc) == 0) {
	DPRINTF(("sp: no ad1848 ? iobase=%x\n", sc->sc_iobase));
	return 0;
    }

    /* Setup WSS interrupt and DMA if auto */
    if (cf->cf_irq == IRQUNK) {

	/* Find unused IRQ for WSS */
	for (i = 0; i < 12; i++) {
	    if (wss_interrupt_bits[i] != 0xff) {
		if (pss_testirq(pc, i))
		    break;
	    }
	}
	if (i == 12) {
	    DPRINTF(("sp: unable to locate free IRQ for WSS\n"));
	    return 0;
	}
	else {
	    cf->cf_irq = i;
	    sc->sc_irq = i;
	    DPRINTF(("sp: found IRQ %d free\n", i));
	}
    }
    else {
	sc->sc_irq = cf->cf_irq;
	if (pss_testirq(pc, sc->sc_irq) == 0) {
	    DPRINTF(("sp: configured IRQ unavailable (%d)\n", sc->sc_irq));
	    return 0;
	}
    }

    if (cf->cf_drq == DRQUNK) {
	/* Find unused DMA channel for WSS */
	for (i = 0; i < 4; i++) {
	    if (wss_dma_bits[i]) {
		if (pss_testdma(pc, i))
		    break;
	    }
	}
	if (i == 4) {
	    DPRINTF(("sp: unable to locate free DMA channel for WSS\n"));
	    return 0;
	}
	else {
	    sc->sc_drq = cf->cf_drq = i;
	    DPRINTF(("sp: found DMA %d free\n", i));
	}
    }
    else {
	if (pss_testdma(pc, sc->sc_drq) == 0) {
	    DPRINTF(("sp: configured DMA channel unavailable (%d)\n", sc->sc_drq));
	    return 0;
	}
	sc->sc_drq = cf->cf_drq;
    }
    sc->sc_recdrq = sc->sc_drq;

    /* Set WSS config registers */
    if ((bits = wss_interrupt_bits[sc->sc_irq]) == 0xff) {
	DPRINTF(("sp: invalid interrupt configuration (irq=%d)\n", sc->sc_irq));
	return 0;
    }

    outb(sc->sc_iobase+WSS_CONFIG, (bits | 0x40));
    if ((inb(sc->sc_iobase+WSS_STATUS) & 0x40) == 0)	/* XXX What do these bits mean ? */
	DPRINTF(("sp: IRQ %x\n", inb(sc->sc_iobase+WSS_STATUS)));

    outb(sc->sc_iobase+WSS_CONFIG, (bits | wss_dma_bits[sc->sc_drq]));

    pc->ad1848_sc = sc;
    sc->parent = pc;

    return 1;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
pssattach(parent, self, aux)
    struct device *parent, *self;
    void *aux;
{
    struct pss_softc *sc = (struct pss_softc *)self;
    struct isa_attach_args *ia = (struct isa_attach_args *)aux;
    int iobase = ia->ia_iobase;
    u_char vers;
    struct ad1848_volume vol = {150, 150};

    sc->sc_iobase = iobase;
    sc->sc_drq = ia->ia_drq;

    /* Setup interrupt handler for PSS */
    sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE, IPL_AUDIO,
	pssintr, sc, sc->sc_dev.dv_xname);

    vers = (inw(sc->sc_iobase+PSS_ID_VERS)&0xff) - 1;
    printf(": ESC614%c\n", (vers > 0)?'A'+vers:' ');

    (void)config_found(self, ia->ia_ic, NULL);		/* XXX */

    sc->out_port = PSS_MASTER_VOL;

    (void)pss_set_master_mode(sc, PSS_SPKR_STEREO);
    (void)pss_set_master_gain(sc, &vol);
    (void)pss_set_treble(sc, AUDIO_MAX_GAIN/2);
    (void)pss_set_bass(sc, AUDIO_MAX_GAIN/2);

    audio_attach_mi(&pss_audio_if, sc->ad1848_sc, &sc->ad1848_sc->sc_dev);
}

void
spattach(parent, self, aux)
    struct device *parent, *self;
    void *aux;
{
    struct ad1848_softc *sc = (struct ad1848_softc *)self;
    struct cfdata *cf = (void *)sc->sc_dev.dv_cfdata;
    isa_chipset_tag_t ic = aux;				/* XXX */
    int iobase = cf->cf_iobase;

    sc->sc_iobase = iobase;
    sc->sc_drq = cf->cf_drq;

    sc->sc_ih = isa_intr_establish(ic, cf->cf_irq, IST_EDGE, IPL_AUDIO,
	ad1848_intr, sc, sc->sc_dev.dv_xname);

    sc->sc_isa = parent->dv_parent;

    ad1848_attach(sc);

    printf("\n");
}

int
pss_set_master_gain(sc, gp)
    struct pss_softc *sc;
    struct ad1848_volume *gp;
{
    DPRINTF(("pss_set_master_gain: %d:%d\n", gp->left, gp->right));

#ifdef PSS_DSP
    if (gp->left > PHILLIPS_VOL_MAX)
	gp->left = PHILLIPS_VOL_MAX;
    if (gp->left < PHILLIPS_VOL_MIN)
	gp->left = PHILLIPS_VOL_MIN;
    if (gp->right > PHILLIPS_VOL_MAX)
	gp->right = PHILLIPS_VOL_MAX;
    if (gp->right < PHILLIPS_VOL_MIN)
	gp->right = PHILLIPS_VOL_MIN;

    pss_dspwrite(sc, SET_MASTER_COMMAND);
    pss_dspwrite(sc, MASTER_VOLUME_LEFT|(PHILLIPS_VOL_CONSTANT + gp->left / PHILLIPS_VOL_STEP));
    pss_dspwrite(sc, SET_MASTER_COMMAND);
    pss_dspwrite(sc, MASTER_VOLUME_RIGHT|(PHILLIPS_VOL_CONSTANT + gp->right / PHILLIPS_VOL_STEP));
#endif

    sc->master_volume = *gp;
    return(0);
}

int
pss_set_master_mode(sc, mode)
    struct pss_softc *sc;
    int mode;
{
    short phillips_mode;

    DPRINTF(("pss_set_master_mode: %d\n", mode));

    if (mode == PSS_SPKR_STEREO)
	phillips_mode = PSS_STEREO;
    else if (mode == PSS_SPKR_PSEUDO)
	phillips_mode = PSS_PSEUDO;
    else if (mode == PSS_SPKR_SPATIAL)
	phillips_mode = PSS_SPATIAL;
    else if (mode == PSS_SPKR_MONO)
	phillips_mode = PSS_MONO;
    else
	return (EINVAL);

#ifdef PSS_DSP
    pss_dspwrite(sc, SET_MASTER_COMMAND);
    pss_dspwrite(sc, MASTER_SWITCH | mode);
#endif

    sc->master_mode = mode;

    return(0);
}

int
pss_set_treble(sc, treb)
    struct pss_softc *sc;
    u_int treb;
{
    DPRINTF(("pss_set_treble: %d\n", treb));

#ifdef PSS_DSP
    if (treb > PHILLIPS_TREBLE_MAX)
	treb = PHILLIPS_TREBLE_MAX;
    if (treb < PHILLIPS_TREBLE_MIN)
	treb = PHILLIPS_TREBLE_MIN;
    pss_dspwrite(sc, SET_MASTER_COMMAND);
    pss_dspwrite(sc, MASTER_TREBLE|(PHILLIPS_TREBLE_CONSTANT + treb / PHILLIPS_TREBLE_STEP));
#endif

    sc->monitor_treble = treb;

    return(0);
}

int
pss_set_bass(sc, bass)
    struct pss_softc *sc;
    u_int bass;
{
    DPRINTF(("pss_set_bass: %d\n", bass));

#ifdef PSS_DSP
    if (bass > PHILLIPS_BASS_MAX)
	bass = PHILLIPS_BASS_MAX;
    if (bass < PHILLIPS_BASS_MIN)
	bass = PHILLIPS_BASS_MIN;
    pss_dspwrite(sc, SET_MASTER_COMMAND);
    pss_dspwrite(sc, MASTER_BASS|(PHILLIPS_BASS_CONSTANT + bass / PHILLIPS_BASS_STEP));
#endif

    sc->monitor_bass = bass;

    return(0);
}

int
pss_get_master_gain(sc, gp)
    struct pss_softc *sc;
    struct ad1848_volume *gp;
{
    *gp = sc->master_volume;
    return(0);
}

int
pss_get_master_mode(sc, mode)
    struct pss_softc *sc;
    u_int *mode;
{
    *mode = sc->master_mode;
    return(0);
}

int
pss_get_treble(sc, tp)
    struct pss_softc *sc;
    u_char *tp;
{
    *tp = sc->monitor_treble;
    return(0);
}

int
pss_get_bass(sc, bp)
    struct pss_softc *sc;
    u_char *bp;
{
    *bp = sc->monitor_bass;
    return(0);
}

int
pss_speaker_ctl(addr, newstate)
    void *addr;
    int newstate;
{
    return(0);
}

int
pssintr(arg)
	void *arg;
{
    struct pss_softc *sc = arg;
    u_short sr;

    sr = inw(sc->sc_iobase+PSS_STATUS);

    DPRINTF(("pssintr: sc=%p st=%x\n", sc, sr));

    /* Acknowledge intr */
    outw(sc->sc_iobase+PSS_IRQ_ACK, 0);

    /* Is it one of ours ? */
    if (sr & (PSS_WRITE_EMPTY|PSS_READ_FULL|PSS_IRQ|PSS_DMQ_TC)) {
	/* XXX do something */
	return 1;
    }

    return 0;
}

int
pss_getdev(addr, retp)
    void *addr;
    struct audio_device *retp;
{
    DPRINTF(("pss_getdev: retp=%p\n", retp));

    *retp = pss_device;
    return 0;
}

static ad1848_devmap_t mappings[] = {
{ PSS_MIC_IN_LVL, AD1848_KIND_LVL, AD1848_AUX2_CHANNEL },
{ PSS_LINE_IN_LVL, AD1848_KIND_LVL, AD1848_AUX1_CHANNEL },
{ PSS_DAC_LVL, AD1848_KIND_LVL, AD1848_DAC_CHANNEL },
{ PSS_MON_LVL, AD1848_KIND_LVL, AD1848_MONO_CHANNEL },
{ PSS_MIC_IN_MUTE, AD1848_KIND_MUTE, AD1848_AUX2_CHANNEL },
{ PSS_LINE_IN_MUTE, AD1848_KIND_MUTE, AD1848_AUX1_CHANNEL },
{ PSS_DAC_MUTE, AD1848_KIND_MUTE, AD1848_DAC_CHANNEL },
{ PSS_REC_LVL, AD1848_KIND_RECORDGAIN, -1 },
{ PSS_RECORD_SOURCE, AD1848_KIND_RECORDSOURCE, -1}
};

static int nummap = sizeof(mappings) / sizeof(mappings[0]);

int
pss_mixer_set_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    struct ad1848_softc *ac = addr;
    struct pss_softc *sc = ac->parent;
    struct ad1848_volume vol;
    int error = ad1848_mixer_set_port(ac, mappings, nummap, cp);

    if (error != ENXIO)
      return (error);

    switch (cp->dev) {
    case PSS_MASTER_VOL:	/* master volume */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    if (ad1848_to_vol(cp, &vol))
		error = pss_set_master_gain(sc, &vol);
	}
	break;

    case PSS_OUTPUT_MODE:
	if (cp->type == AUDIO_MIXER_ENUM)
	    error = pss_set_master_mode(sc, cp->un.ord);
	break;

    case PSS_MASTER_TREBLE:	/* master treble */
	if (cp->type == AUDIO_MIXER_VALUE && cp->un.value.num_channels == 1)
	    error = pss_set_treble(sc, (u_char)cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
	break;

    case PSS_MASTER_BASS:	/* master bass */
	if (cp->type == AUDIO_MIXER_VALUE && cp->un.value.num_channels == 1)
	    error = pss_set_bass(sc, (u_char)cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
	break;

    default:
	    return ENXIO;
	    /*NOTREACHED*/
    }

    return 0;
}

int
pss_mixer_get_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    struct ad1848_softc *ac = addr;
    struct pss_softc *sc = ac->parent;
    struct ad1848_volume vol;
    u_char eq;
    int error = ad1848_mixer_get_port(ac, mappings, nummap, cp);

    if (error != ENXIO)
      return (error);

    error = EINVAL;

    switch (cp->dev) {
    case PSS_MASTER_VOL:	/* master volume */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    error = pss_get_master_gain(sc, &vol);
	    if (!error)
		ad1848_from_vol(cp, &vol);
	}
	break;

    case PSS_MASTER_TREBLE:	/* master treble */
	if (cp->type == AUDIO_MIXER_VALUE && cp->un.value.num_channels == 1) {
	    error = pss_get_treble(sc, &eq);
	    if (!error)
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = eq;
	}
	break;

    case PSS_MASTER_BASS:	/* master bass */
	if (cp->type == AUDIO_MIXER_VALUE && cp->un.value.num_channels == 1) {
	    error = pss_get_bass(sc, &eq);
	    if (!error)
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = eq;
	}
	break;

    case PSS_OUTPUT_MODE:
	if (cp->type == AUDIO_MIXER_ENUM)
	    error = pss_get_master_mode(sc, &cp->un.ord);
	break;

    default:
	error = ENXIO;
	break;
    }

    return(error);
}

int
pss_query_devinfo(addr, dip)
    void *addr;
    mixer_devinfo_t *dip;
{
    DPRINTF(("pss_query_devinfo: index=%d\n", dip->index));

    switch(dip->index) {
    case PSS_MIC_IN_LVL:	/* Microphone */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = PSS_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = PSS_MIC_IN_MUTE;
	strlcpy(dip->label.name, AudioNmicrophone, sizeof dip->label.name);
	dip->un.v.num_channels = 2;
	strlcpy(dip->un.v.units.name, AudioNvolume,
	    sizeof dip->un.v.units.name);
	break;

    case PSS_LINE_IN_LVL:	/* line/CD */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = PSS_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = PSS_LINE_IN_MUTE;
	strlcpy(dip->label.name, AudioNcd, sizeof dip->label.name);
	dip->un.v.num_channels = 2;
	strlcpy(dip->un.v.units.name, AudioNvolume,
	    sizeof dip->un.v.units.name);
	break;

    case PSS_DAC_LVL:		/*  dacout */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = PSS_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = PSS_DAC_MUTE;
	strlcpy(dip->label.name, AudioNdac, sizeof dip->label.name);
	dip->un.v.num_channels = 2;
	strlcpy(dip->un.v.units.name, AudioNvolume,
	    sizeof dip->un.v.units.name);
	break;

    case PSS_REC_LVL:	/* record level */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = PSS_RECORD_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = PSS_RECORD_SOURCE;
	strlcpy(dip->label.name, AudioNrecord, sizeof dip->label.name);
	dip->un.v.num_channels = 2;
	strlcpy(dip->un.v.units.name, AudioNvolume,
	    sizeof dip->un.v.units.name);
	break;

    case PSS_MON_LVL:	/* monitor level */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = PSS_MONITOR_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strlcpy(dip->label.name, AudioNmonitor, sizeof dip->label.name);
	dip->un.v.num_channels = 1;
	strlcpy(dip->un.v.units.name, AudioNvolume,
	    sizeof dip->un.v.units.name);
	break;

    case PSS_MASTER_VOL:	/* master volume */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = PSS_OUTPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = PSS_OUTPUT_MODE;
	strlcpy(dip->label.name, AudioNmaster, sizeof dip->label.name);
	dip->un.v.num_channels = 2;
	strlcpy(dip->un.v.units.name, AudioNvolume,
	    sizeof dip->un.v.units.name);
	break;

    case PSS_MASTER_TREBLE:	/* master treble */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = PSS_OUTPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strlcpy(dip->label.name, AudioNtreble, sizeof dip->label.name);
	dip->un.v.num_channels = 1;
	strlcpy(dip->un.v.units.name, AudioNtreble,
	    sizeof dip->un.v.units.name);
	break;

    case PSS_MASTER_BASS:	/* master bass */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = PSS_OUTPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strlcpy(dip->label.name, AudioNbass, sizeof dip->label.name);
	dip->un.v.num_channels = 1;
	strlcpy(dip->un.v.units.name, AudioNbass, sizeof dip->un.v.units.name);
	break;

    case PSS_OUTPUT_CLASS:			/* output class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = PSS_OUTPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strlcpy(dip->label.name, AudioCoutputs, sizeof dip->label.name);
	break;

    case PSS_INPUT_CLASS:			/* input class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = PSS_INPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strlcpy(dip->label.name, AudioCinputs, sizeof dip->label.name);
	break;

    case PSS_MONITOR_CLASS:			/* monitor class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = PSS_MONITOR_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strlcpy(dip->label.name, AudioCmonitor, sizeof dip->label.name);
	break;

    case PSS_RECORD_CLASS:			/* record source class */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = PSS_RECORD_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strlcpy(dip->label.name, AudioCrecord, sizeof dip->label.name);
	break;

    case PSS_MIC_IN_MUTE:
	dip->mixer_class = PSS_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = PSS_MIC_IN_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;

    case PSS_LINE_IN_MUTE:
	dip->mixer_class = PSS_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = PSS_LINE_IN_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;

    case PSS_DAC_MUTE:
	dip->mixer_class = PSS_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = PSS_DAC_LVL;
	dip->next = AUDIO_MIXER_LAST;
    mute:
	strlcpy(dip->label.name, AudioNmute, sizeof dip->label.name);
	dip->un.e.num_mem = 2;
	strlcpy(dip->un.e.member[0].label.name, AudioNoff,
	    sizeof dip->un.e.member[0].label.name);
	dip->un.e.member[0].ord = 0;
	strlcpy(dip->un.e.member[1].label.name, AudioNon,
	    sizeof dip->un.e.member[1].label.name);
	dip->un.e.member[1].ord = 1;
	break;

    case PSS_OUTPUT_MODE:
	dip->mixer_class = PSS_OUTPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = PSS_MASTER_VOL;
	dip->next = AUDIO_MIXER_LAST;
	strlcpy(dip->label.name, AudioNmode, sizeof dip->label.name);
	dip->un.e.num_mem = 4;
	strlcpy(dip->un.e.member[0].label.name, AudioNmono,
	    sizeof dip->un.e.member[0].label.name);
	dip->un.e.member[0].ord = PSS_SPKR_MONO;
	strlcpy(dip->un.e.member[1].label.name, AudioNstereo,
	    sizeof dip->un.e.member[1].label.name);
	dip->un.e.member[1].ord = PSS_SPKR_STEREO;
	strlcpy(dip->un.e.member[2].label.name, AudioNpseudo,
	    sizeof dip->un.e.member[2].label.name);
	dip->un.e.member[2].ord = PSS_SPKR_PSEUDO;
	strlcpy(dip->un.e.member[3].label.name, AudioNspatial,
	    sizeof dip->un.e.member[3].label.name);
	dip->un.e.member[3].ord = PSS_SPKR_SPATIAL;
	break;

    case PSS_RECORD_SOURCE:
	dip->mixer_class = PSS_RECORD_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = PSS_REC_LVL;
	dip->next = AUDIO_MIXER_LAST;
	strlcpy(dip->label.name, AudioNsource, sizeof dip->label.name);
	dip->un.e.num_mem = 3;
	strlcpy(dip->un.e.member[0].label.name, AudioNmicrophone,
	    sizeof dip->un.e.member[0].label.name);
	dip->un.e.member[0].ord = PSS_MIC_IN_LVL;
	strlcpy(dip->un.e.member[1].label.name, AudioNcd,
	    sizeof dip->un.e.member[1].label.name);
	dip->un.e.member[1].ord = PSS_LINE_IN_LVL;
	strlcpy(dip->un.e.member[2].label.name, AudioNdac,
	    sizeof dip->un.e.member[2].label.name);
	dip->un.e.member[2].ord = PSS_DAC_LVL;
	break;

    default:
	return ENXIO;
	/*NOTREACHED*/
    }
    DPRINTF(("AUDIO_MIXER_DEVINFO: name=%s\n", dip->label.name));

    return 0;
}
