/*	$OpenBSD: eso.c,v 1.8 2000/01/11 13:40:05 deraadt Exp $	*/
/*	$NetBSD: eso.c,v 1.3 1999/08/02 17:37:43 augustss Exp $	*/

/*
 * Copyright (c) 1999 Klaus J. Klein
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ESS Technology Inc. Solo-1 PCI AudioDrive (ES1938/1946) device driver.
 */

#ifdef __OpenBSD__
#define HIDE
#define MATCH_ARG_2_T void *
#else
#define HIDE static
#define MATCH_ARG_2_T struct cfdata *
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/midi_if.h>

#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/mpuvar.h>
#include <dev/ic/i8237reg.h>
#include <dev/pci/esoreg.h>
#include <dev/pci/esovar.h>
#include <dev/audiovar.h>

#include <machine/bus.h>
#include <machine/intr.h>

#ifdef __OpenBSD__
#include <machine/endian.h>
#define htopci(x) htole32(x)
#define pcitoh(x) letoh32(x)
#else
#if BYTE_ORDER == BIG_ENDIAN
#include <machine/bswap.h>
#define htopci(x) bswap32(x)
#define pcitoh(x) bswap32(x)
#else
#define htopci(x) (x)
#define pcitoh(x) (x)
#endif
#endif

#if defined(AUDIO_DEBUG) || defined(DEBUG)
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

struct eso_dma {
	bus_dmamap_t		ed_map;
	caddr_t			ed_addr;
	bus_dma_segment_t	ed_segs[1];
	int			ed_nsegs;
	size_t			ed_size;
	struct eso_dma *	ed_next;
};

#define KVADDR(dma)	((void *)(dma)->ed_addr)
#define DMAADDR(dma)	((dma)->ed_map->dm_segs[0].ds_addr)

/* Autoconfiguration interface */
HIDE int eso_match __P((struct device *, MATCH_ARG_2_T, void *));
HIDE void eso_attach __P((struct device *, struct device *, void *));
HIDE void eso_defer __P((struct device *));

struct cfattach eso_ca = {
	sizeof (struct eso_softc), eso_match, eso_attach
};

#ifdef __OpenBSD__
struct cfdriver eso_cd = {
	NULL, "eso", DV_DULL
};
#endif

/* PCI interface */
HIDE int eso_intr __P((void *));

/* MI audio layer interface */
HIDE int	eso_open __P((void *, int));
HIDE void	eso_close __P((void *));
HIDE int	eso_query_encoding __P((void *, struct audio_encoding *));
HIDE int	eso_set_params __P((void *, int, int, struct audio_params *,
		    struct audio_params *));
HIDE int	eso_round_blocksize __P((void *, int));
HIDE int	eso_halt_output __P((void *));
HIDE int	eso_halt_input __P((void *));
HIDE int	eso_getdev __P((void *, struct audio_device *));
HIDE int	eso_set_port __P((void *, mixer_ctrl_t *));
HIDE int	eso_get_port __P((void *, mixer_ctrl_t *));
HIDE int	eso_query_devinfo __P((void *, mixer_devinfo_t *));
#ifdef __OpenBSD__
void *		eso_allocm __P((void *, u_long, int, int));
#else
HIDE void *	eso_allocm __P((void *, int, size_t, int, int));
#endif
HIDE void	eso_freem __P((void *, void *, int));
#ifdef __OpenBSD__
u_long		eso_round_buffersize __P((void *, u_long));
#else
HIDE size_t	eso_round_buffersize __P((void *, int, size_t));
#endif
HIDE int	eso_mappage __P((void *, void *, int, int));
HIDE int	eso_get_props __P((void *));
HIDE int	eso_trigger_output __P((void *, void *, void *, int,
		    void (*)(void *), void *, struct audio_params *));
HIDE int	eso_trigger_input __P((void *, void *, void *, int,
		    void (*)(void *), void *, struct audio_params *));

HIDE struct audio_hw_if eso_hw_if = {
	eso_open,
	eso_close,
	NULL,			/* drain */
	eso_query_encoding,
	eso_set_params,
	eso_round_blocksize,
	NULL,			/* commit_settings */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	eso_halt_output,
	eso_halt_input,
	NULL,			/* speaker_ctl */
	eso_getdev,
	NULL,			/* setfd */
	eso_set_port,
	eso_get_port,
	eso_query_devinfo,
	eso_allocm,
	eso_freem,
	eso_round_buffersize,
	eso_mappage,
	eso_get_props,
	eso_trigger_output,
	eso_trigger_input
};

HIDE const char * const eso_rev2model[] = {
	"ES1938",
	"ES1946",
	"ES1946 rev E"
};


/*
 * Utility routines
 */
/* Register access etc. */
HIDE uint8_t	eso_read_ctlreg __P((struct eso_softc *, uint8_t));
HIDE uint8_t	eso_read_mixreg __P((struct eso_softc *, uint8_t));
HIDE uint8_t	eso_read_rdr __P((struct eso_softc *));
HIDE int	eso_reset __P((struct eso_softc *));
HIDE void	eso_set_gain __P((struct eso_softc *, unsigned int));
HIDE int	eso_set_recsrc __P((struct eso_softc *, unsigned int));
HIDE void	eso_write_cmd __P((struct eso_softc *, uint8_t));
HIDE void	eso_write_ctlreg __P((struct eso_softc *, uint8_t, uint8_t));
HIDE void	eso_write_mixreg __P((struct eso_softc *, uint8_t, uint8_t));
/* DMA memory allocation */
HIDE int	eso_allocmem __P((struct eso_softc *, size_t, size_t, size_t,
		    int, struct eso_dma *));
HIDE void	eso_freemem __P((struct eso_softc *, struct eso_dma *));


HIDE int
eso_match(parent, match, aux)
	struct device *parent;
	MATCH_ARG_2_T match;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ESSTECH &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ESSTECH_SOLO1)
		return (1);

	return (0);
}

HIDE void
eso_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct eso_softc *sc = (struct eso_softc *)self;
	struct pci_attach_args *pa = aux;
	struct audio_attach_args aa;
	pci_intr_handle_t ih;
	bus_addr_t vcbase;
	const char *intrstring;
	int idx;
	uint8_t a2mode;

	sc->sc_revision = PCI_REVISION(pa->pa_class);

	if (sc->sc_revision <
	    sizeof (eso_rev2model) / sizeof (eso_rev2model[0]))
		printf(": %s", eso_rev2model[sc->sc_revision]);
	else
		printf(": (unknown rev. 0x%02x)", sc->sc_revision);

	/* Map I/O registers. */
	if (pci_mapreg_map(pa, ESO_PCI_BAR_IO, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, NULL)) {
		printf(", can't map I/O space\n");
		return;
	}
	if (pci_mapreg_map(pa, ESO_PCI_BAR_SB, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_sb_iot, &sc->sc_sb_ioh, NULL, NULL)) {
		printf(", can't map SB I/O space\n");
		return;
	}
	if (pci_mapreg_map(pa, ESO_PCI_BAR_VC, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_dmac_iot, &sc->sc_dmac_ioh, &vcbase, &sc->sc_vcsize)) {
		vcbase = 0;
		sc->sc_vcsize = 0x10; /* From the data sheet. */
	}

	if (pci_mapreg_map(pa, ESO_PCI_BAR_MPU, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_mpu_iot, &sc->sc_mpu_ioh, NULL, NULL)) {
		printf(", can't map MPU I/O space\n");
		return;
	}
	if (pci_mapreg_map(pa, ESO_PCI_BAR_GAME, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_game_iot, &sc->sc_game_ioh, NULL, NULL)) {
		printf(", can't map Game I/O space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_dmas = NULL;
	sc->sc_dmac_configured = 0;

	/* Enable bus mastering. */
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/* Reset the device; bail out upon failure. */
	if (eso_reset(sc) != 0) {
		printf(", can't reset\n");
		return;
	}
	
	/* Select the DMA/IRQ policy: DDMA, ISA IRQ emulation disabled. */
	pci_conf_write(pa->pa_pc, pa->pa_tag, ESO_PCI_S1C,
	    pci_conf_read(pa->pa_pc, pa->pa_tag, ESO_PCI_S1C) &
	    ~(ESO_PCI_S1C_IRQP_MASK | ESO_PCI_S1C_DMAP_MASK));

	/* Enable the relevant DMA interrupts. */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ESO_IO_IRQCTL,
	    ESO_IO_IRQCTL_A1IRQ | ESO_IO_IRQCTL_A2IRQ);
	
	/* Set up A1's sample rate generator for new-style parameters. */
	a2mode = eso_read_mixreg(sc, ESO_MIXREG_A2MODE);
	a2mode |= ESO_MIXREG_A2MODE_NEWA1 | ESO_MIXREG_A2MODE_ASYNC;
	eso_write_mixreg(sc, ESO_MIXREG_A2MODE, a2mode);
	
	/* Set mixer regs to something reasonable, needs work. */
	for (idx = 0; idx < ESO_NGAINDEVS; idx++) {
		int v;
		
		switch (idx) {
 		case ESO_MIC_PLAY_VOL:
		case ESO_LINE_PLAY_VOL:
		case ESO_CD_PLAY_VOL:
		case ESO_MONO_PLAY_VOL:
		case ESO_AUXB_PLAY_VOL:
		case ESO_DAC_REC_VOL:
		case ESO_LINE_REC_VOL:
		case ESO_SYNTH_REC_VOL:
		case ESO_CD_REC_VOL:
		case ESO_MONO_REC_VOL:
		case ESO_AUXB_REC_VOL:
		case ESO_SPATIALIZER:
			v = 0;
			break;
		case ESO_MASTER_VOL:
			v = ESO_GAIN_TO_6BIT(AUDIO_MAX_GAIN / 2);
			break;
		default:
			v = ESO_GAIN_TO_4BIT(AUDIO_MAX_GAIN / 2);
			break;
		}
		sc->sc_gain[idx][ESO_LEFT] = sc->sc_gain[idx][ESO_RIGHT] = v;
		eso_set_gain(sc, idx);
	}
	eso_set_recsrc(sc, ESO_MIXREG_ERS_MIC);
	
	/* Map and establish the interrupt. */
	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(", couldn't map interrupt\n");
		return;
	}
	intrstring = pci_intr_string(pa->pa_pc, ih);
#ifdef __OpenBSD__
	sc->sc_ih  = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, eso_intr, sc,
	    sc->sc_dev.dv_xname);
#else
	sc->sc_ih  = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, eso_intr, sc);
#endif
	if (sc->sc_ih == NULL) {
		printf(", couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstring != NULL)
			printf(" at %s", intrstring);
		printf("\n");
		return;
	}
	printf(" %s\n", intrstring);

	/*
	 * Set up the DDMA Control register; a suitable I/O region has been
	 * supposedly mapped in the VC base address register.
	 *
	 * The Solo-1 has an ... interesting silicon bug that causes it to
	 * not respond to I/O space accesses to the Audio 1 DMA controller
	 * if the latter's mapping base address is aligned on a 1K boundary.
	 * As a consequence, it is quite possible for the mapping provided
	 * in the VC BAR to be useless.  To work around this, we defer this
	 * part until all autoconfiguration on our parent bus is completed
	 * and then try to map it ourselves in fulfillment of the constraint.
	 * 
	 * According to the register map we may write to the low 16 bits
	 * only, but experimenting has shown we're safe.
	 * -kjk
	 */
	if (ESO_VALID_DDMAC_BASE(vcbase)) {
		pci_conf_write(pa->pa_pc, pa->pa_tag, ESO_PCI_DDMAC,
		    vcbase | ESO_PCI_DDMAC_DE);
		sc->sc_dmac_configured = 1;

		printf("%s: mapping Audio 1 DMA using VC I/O space at 0x%lx\n",
		    sc->sc_dev.dv_xname, (unsigned long)vcbase);
	} else {
		DPRINTF(("%s: VC I/O space at 0x%lx not suitable, deferring\n",
		    sc->sc_dev.dv_xname, (unsigned long)vcbase));
		sc->sc_pa = *pa;
		config_defer(self, eso_defer);
	}
	
	audio_attach_mi(&eso_hw_if, sc, &sc->sc_dev);

	aa.type = AUDIODEV_TYPE_OPL;
	aa.hwif = NULL;
	aa.hdl = NULL;
	(void)config_found(&sc->sc_dev, &aa, audioprint);

#if 0
	aa.type = AUDIODEV_TYPE_MPU;
	aa.hwif = NULL;
	aa.hdl = NULL;
	sc->sc_mpudev = config_found(&sc->sc_dev, &aa, audioprint);
#endif
}

HIDE void
eso_defer(self)
	struct device *self;
{
	struct eso_softc *sc = (struct eso_softc *)self;
	struct pci_attach_args *pa = &sc->sc_pa;
	bus_addr_t addr, start;

	printf("%s: ", sc->sc_dev.dv_xname);

	/*
	 * This is outright ugly, but since we must not make assumptions
	 * on the underlying allocator's behaviour it's the most straight-
	 * forward way to implement it.  Note that we skip over the first
	 * 1K region, which is typically occupied by an attached ISA bus.
	 */
	for (start = 0x0400; start < 0xffff; start += 0x0400) {
		if (bus_space_alloc(sc->sc_iot,
		    start + sc->sc_vcsize, start + 0x0400 - 1,
		    sc->sc_vcsize, sc->sc_vcsize, 0, 0, &addr,
		    &sc->sc_dmac_ioh) != 0)
			continue;

		pci_conf_write(pa->pa_pc, pa->pa_tag, ESO_PCI_DDMAC,
		    addr | ESO_PCI_DDMAC_DE);
		sc->sc_dmac_iot = sc->sc_iot;
		sc->sc_dmac_configured = 1;
		printf("mapping Audio 1 DMA using I/O space at 0x%lx\n",
		    (unsigned long)addr);

		return;
	}
	
	printf("can't map Audio 1 DMA into I/O space\n");
}

HIDE void
eso_write_cmd(sc, cmd)
	struct eso_softc *sc;
	uint8_t cmd;
{
	int i;

	/* Poll for busy indicator to become clear. */
	for (i = 0; i < ESO_WDR_TIMEOUT; i++) {
		if ((bus_space_read_1(sc->sc_sb_iot, sc->sc_sb_ioh, ESO_SB_RSR)
		    & ESO_SB_RSR_BUSY) == 0) {
			bus_space_write_1(sc->sc_sb_iot, sc->sc_sb_ioh,
			    ESO_SB_WDR, cmd);
			return;
		} else {
			delay(10);
		}
	}

	printf("%s: WDR timeout\n", sc->sc_dev.dv_xname);
	return;
}

/* Write to a controller register */
HIDE void
eso_write_ctlreg(sc, reg, val)
	struct eso_softc *sc;
	uint8_t reg, val;
{

	/* DPRINTF(("ctlreg 0x%02x = 0x%02x\n", reg, val)); */
	
	eso_write_cmd(sc, reg);
	eso_write_cmd(sc, val);
}

/* Read out the Read Data Register */
HIDE uint8_t
eso_read_rdr(sc)
	struct eso_softc *sc;
{
	int i;

	for (i = 0; i < ESO_RDR_TIMEOUT; i++) {
		if (bus_space_read_1(sc->sc_sb_iot, sc->sc_sb_ioh,
		    ESO_SB_RBSR) & ESO_SB_RBSR_RDAV) {
			return (bus_space_read_1(sc->sc_sb_iot,
			    sc->sc_sb_ioh, ESO_SB_RDR));
		} else {
			delay(10);
		}
	}

	printf("%s: RDR timeout\n", sc->sc_dev.dv_xname);
	return (-1);
}


HIDE uint8_t
eso_read_ctlreg(sc, reg)
	struct eso_softc *sc;
	uint8_t reg;
{

	eso_write_cmd(sc, ESO_CMD_RCR);
	eso_write_cmd(sc, reg);
	return (eso_read_rdr(sc));
}

HIDE void
eso_write_mixreg(sc, reg, val)
	struct eso_softc *sc;
	uint8_t reg, val;
{
	int s;

	/* DPRINTF(("mixreg 0x%02x = 0x%02x\n", reg, val)); */
	
	s = splaudio();
	bus_space_write_1(sc->sc_sb_iot, sc->sc_sb_ioh, ESO_SB_MIXERADDR, reg);
	bus_space_write_1(sc->sc_sb_iot, sc->sc_sb_ioh, ESO_SB_MIXERDATA, val);
	splx(s);
}

HIDE uint8_t
eso_read_mixreg(sc, reg)
	struct eso_softc *sc;
	uint8_t reg;
{
	int s;
	uint8_t val;

	s = splaudio();
	bus_space_write_1(sc->sc_sb_iot, sc->sc_sb_ioh, ESO_SB_MIXERADDR, reg);
	val = bus_space_read_1(sc->sc_sb_iot, sc->sc_sb_ioh, ESO_SB_MIXERDATA);
	splx(s);
	
	return (val);
}

HIDE int
eso_intr(hdl)
	void *hdl;
{
	struct eso_softc *sc = hdl;
	uint8_t irqctl;

	irqctl = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ESO_IO_IRQCTL);

	/* If it wasn't ours, that's all she wrote. */
	if ((irqctl & (ESO_IO_IRQCTL_A1IRQ | ESO_IO_IRQCTL_A2IRQ)) == 0)
		return (0);
	
	if (irqctl & ESO_IO_IRQCTL_A1IRQ) {
		/* Clear interrupt. */
		(void)bus_space_read_1(sc->sc_sb_iot, sc->sc_sb_ioh,
		    ESO_SB_RBSR);
	
		if (sc->sc_rintr)
			sc->sc_rintr(sc->sc_rarg);
		else
			wakeup(&sc->sc_rintr);
	}

	if (irqctl & ESO_IO_IRQCTL_A2IRQ) {
		/*
		 * Clear the A2 IRQ latch: the cached value reflects the
		 * current DAC settings with the IRQ latch bit not set.
		 */
		eso_write_mixreg(sc, ESO_MIXREG_A2C2, sc->sc_a2c2);

		if (sc->sc_pintr)
			sc->sc_pintr(sc->sc_parg);
		else
			wakeup(&sc->sc_pintr);
	}

#if 0
	if ((irqctl & ESO_IO_IRQCTL_MPUIRQ) && sc->sc_mpudev != 0)
		mpu_intr(sc->sc_mpudev);
#endif
 
	return (1);
}

/* Perform a software reset, including DMA FIFOs. */
HIDE int
eso_reset(sc)
	struct eso_softc *sc;
{
	int i;

	bus_space_write_1(sc->sc_sb_iot, sc->sc_sb_ioh, ESO_SB_RESET,
	    ESO_SB_RESET_SW | ESO_SB_RESET_FIFO);
	/* `Delay' suggested in the data sheet. */
	(void)bus_space_read_1(sc->sc_sb_iot, sc->sc_sb_ioh, ESO_SB_STATUS);
	bus_space_write_1(sc->sc_sb_iot, sc->sc_sb_ioh, ESO_SB_RESET, 0);

	/* Wait for reset to take effect. */
	for (i = 0; i < ESO_RESET_TIMEOUT; i++) {
		/* Poll for data to become available. */
		if ((bus_space_read_1(sc->sc_sb_iot, sc->sc_sb_ioh,
		    ESO_SB_RBSR) & ESO_SB_RBSR_RDAV) != 0 &&
		    bus_space_read_1(sc->sc_sb_iot, sc->sc_sb_ioh,
			ESO_SB_RDR) == ESO_SB_RDR_RESETMAGIC) {

			/* Activate Solo-1 extension commands. */
			eso_write_cmd(sc, ESO_CMD_EXTENB);
			/* Reset mixer registers. */
			eso_write_mixreg(sc, ESO_MIXREG_RESET,
			    ESO_MIXREG_RESET_RESET);

			return (0);
		} else {
			delay(1000);
		}
	}
	
	printf("%s: reset timeout\n", sc->sc_dev.dv_xname);
	return (-1);
}


/* ARGSUSED */
HIDE int
eso_open(hdl, flags)
	void *hdl;
	int flags;
{
	struct eso_softc *sc = hdl;
	
	DPRINTF(("%s: open\n", sc->sc_dev.dv_xname));

	sc->sc_pintr = NULL;
	sc->sc_rintr = NULL;
	
	return (0);
}

HIDE void
eso_close(hdl)
	void *hdl;
{

	DPRINTF(("%s: close\n", ((struct eso_softc *)hdl)->sc_dev.dv_xname));
}

HIDE int
eso_query_encoding(hdl, fp)
	void *hdl;
	struct audio_encoding *fp;
{
	
	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 2:
		fp->precision = 16;
		if (fp->flags & AUOPEN_READ) {
			strcpy(fp->name, AudioEslinear_be);
			fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
			if (fp->flags & AUOPEN_WRITE)
				fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
			else
				fp->flags = 0;
		} else {
			strcpy(fp->name, AudioEslinear_le);
			fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
			fp->flags = 0;
		}
		break;
	case 3:
		fp->precision = 16;
		if (fp->flags & AUOPEN_READ) {
			strcpy(fp->name, AudioEulinear_be);
			fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
			if (fp->flags & AUOPEN_WRITE)
				fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
			else
				fp->flags = 0;
		} else {
			strcpy(fp->name, AudioEulinear_le);
			fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
			fp->flags = 0;
		}
		break;
	case 4:
		fp->precision = 16;
		if (fp->flags & AUOPEN_READ) {
			strcpy(fp->name, AudioEslinear_le);
			fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		} else {
			strcpy(fp->name, AudioEslinear_be);
			fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		}
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 5:
		fp->precision = 16;
		if (fp->flags & AUOPEN_READ) {
			strcpy(fp->name, AudioEulinear_le);
			fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		} else {
			strcpy(fp->name, AudioEulinear_be);
			fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		}
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

HIDE int
eso_set_params(hdl, setmode, usemode, play, rec)
	void *hdl;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct eso_softc *sc = hdl;
	struct audio_params *p;
	int mode, r[2], rd[2], clk;
	unsigned int srg, fltdiv;
	
	for (mode = AUMODE_RECORD; mode != -1; 
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		p = (mode == AUMODE_PLAY) ? play : rec;

		if (p->sample_rate < ESO_MINRATE ||
		    p->sample_rate > ESO_MAXRATE ||
		    (p->precision != 8 && p->precision != 16) ||
		    (p->channels != 1 && p->channels != 2))
			return (EINVAL);
		
		p->factor = 1;
		p->sw_code = NULL;
		switch (p->encoding) {
		case AUDIO_ENCODING_SLINEAR_BE:
		case AUDIO_ENCODING_ULINEAR_BE:
			if (mode == AUMODE_PLAY && p->precision == 16)
				p->sw_code = swap_bytes;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
		case AUDIO_ENCODING_ULINEAR_LE:
			if (mode == AUMODE_RECORD && p->precision == 16)
				p->sw_code = swap_bytes;
			break;
		case AUDIO_ENCODING_ULAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = mulaw_to_ulinear16;
			} else {
				p->sw_code = ulinear8_to_mulaw;
			}
			break;
		case AUDIO_ENCODING_ALAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = alaw_to_ulinear16;
			} else {
				p->sw_code = ulinear8_to_alaw;
			}
			break;
		default:
			return (EINVAL);
		}

		/*
		 * We'll compute both possible sample rate dividers and pick
		 * the one with the least error.
		 */
#define ABS(x) ((x) < 0 ? -(x) : (x))
		r[0] = ESO_CLK0 /
		    (128 - (rd[0] = 128 - ESO_CLK0 / p->sample_rate));
		r[1] = ESO_CLK1 /
		    (128 - (rd[1] = 128 - ESO_CLK1 / p->sample_rate));

		clk = ABS(p->sample_rate - r[0]) > ABS(p->sample_rate - r[1]);
		srg = rd[clk] | (clk == 1 ? ESO_CLK1_SELECT : 0x00);

		/* Roll-off frequency of 87%, as in the ES1888 driver. */
		fltdiv = 256 - 200279L / p->sample_rate;

		/* Update to reflect the possibly inexact rate. */
		p->sample_rate = r[clk];
	
		if (mode == AUMODE_RECORD) {
			/* Audio 1 */
			DPRINTF(("A1 srg 0x%02x fdiv 0x%02x\n", srg, fltdiv));
			eso_write_ctlreg(sc, ESO_CTLREG_SRG, srg);
			eso_write_ctlreg(sc, ESO_CTLREG_FLTDIV, fltdiv);
		} else {
			/* Audio 2 */
			DPRINTF(("A2 srg 0x%02x fdiv 0x%02x\n", srg, fltdiv));
			eso_write_mixreg(sc, ESO_MIXREG_A2SRG, srg);
			eso_write_mixreg(sc, ESO_MIXREG_A2FLTDIV, fltdiv);
		}
#undef ABS

	}

	return (0);
}

HIDE int
eso_round_blocksize(hdl, blk)
	void *hdl;
	int blk;
{

	return (blk & -32);	/* keep good alignment; at least 16 req'd */
}

HIDE int
eso_halt_output(hdl)
	void *hdl;
{
	struct eso_softc *sc = hdl;
	int error, s;
	
	DPRINTF(("%s: halt_output\n", sc->sc_dev.dv_xname));

	/*
	 * Disable auto-initialize DMA, allowing the FIFO to drain and then
	 * stop.  The interrupt callback pointer is cleared at this
	 * point so that an outstanding FIFO interrupt for the remaining data
	 * will be acknowledged without further processing.
	 *
	 * This does not immediately `abort' an operation in progress (c.f.
	 * audio(9)) but is the method to leave the FIFO behind in a clean
	 * state with the least hair.  (Besides, that item needs to be
	 * rephrased for trigger_*()-based DMA environments.)
	 */
	s = splaudio();
	eso_write_mixreg(sc, ESO_MIXREG_A2C1,
	    ESO_MIXREG_A2C1_FIFOENB | ESO_MIXREG_A2C1_DMAENB);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ESO_IO_A2DMAM,
	    ESO_IO_A2DMAM_DMAENB);

	sc->sc_pintr = NULL;
	error = tsleep(&sc->sc_pintr, PCATCH | PWAIT, "esoho", hz);
	splx(s);
	
	/* Shut down DMA completely. */
	eso_write_mixreg(sc, ESO_MIXREG_A2C1, 0);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ESO_IO_A2DMAM, 0);
	
	return (error == EWOULDBLOCK ? 0 : error);
}

HIDE int
eso_halt_input(hdl)
	void *hdl;
{
	struct eso_softc *sc = hdl;
	int error, s;
	
	DPRINTF(("%s: halt_input\n", sc->sc_dev.dv_xname));

	/* Just like eso_halt_output(), but for Audio 1. */
	s = splaudio();
	eso_write_ctlreg(sc, ESO_CTLREG_A1C2,
	    ESO_CTLREG_A1C2_READ | ESO_CTLREG_A1C2_ADC |
	    ESO_CTLREG_A1C2_DMAENB);
	bus_space_write_1(sc->sc_dmac_iot, sc->sc_dmac_ioh, ESO_DMAC_MODE,
	    DMA37MD_WRITE | DMA37MD_DEMAND);

	sc->sc_rintr = NULL;
	error = tsleep(&sc->sc_rintr, PCATCH | PWAIT, "esohi", hz);
	splx(s);

	/* Shut down DMA completely. */
	eso_write_ctlreg(sc, ESO_CTLREG_A1C2,
	    ESO_CTLREG_A1C2_READ | ESO_CTLREG_A1C2_ADC);
	bus_space_write_1(sc->sc_dmac_iot, sc->sc_dmac_ioh, ESO_DMAC_MASK,
	    ESO_DMAC_MASK_MASK);

	return (error == EWOULDBLOCK ? 0 : error);
}

/* ARGSUSED */
HIDE int
eso_getdev(hdl, retp)
	void *hdl;
	struct audio_device *retp;
{
	struct eso_softc *sc = hdl;

	strncpy(retp->name, "ESS Solo-1", sizeof (retp->name));
#ifdef __OpenBSD__
	/* This does not overflow. */
	sprintf(retp->version, "0x%02x", sc->sc_revision);
#else
	snprintf(retp->version, sizeof (retp->version), "0x%02x",
	    sc->sc_revision);
#endif
	if (sc->sc_revision <=
	    sizeof (eso_rev2model) / sizeof (eso_rev2model[0]))
		strncpy(retp->config, eso_rev2model[sc->sc_revision],
		    sizeof (retp->config));
	else
		strncpy(retp->config, "unknown", sizeof (retp->config));
	
	return (0);
}

HIDE int
eso_set_port(hdl, cp)
	void *hdl;
	mixer_ctrl_t *cp;
{
	struct eso_softc *sc = hdl;
	unsigned int lgain, rgain;
	uint8_t tmp;
	
	switch (cp->dev) {
	case ESO_DAC_PLAY_VOL:
	case ESO_MIC_PLAY_VOL:
	case ESO_LINE_PLAY_VOL:
	case ESO_SYNTH_PLAY_VOL:
	case ESO_CD_PLAY_VOL:
	case ESO_AUXB_PLAY_VOL:
	case ESO_RECORD_VOL:
	case ESO_DAC_REC_VOL:
	case ESO_MIC_REC_VOL:
	case ESO_LINE_REC_VOL:
	case ESO_SYNTH_REC_VOL:
	case ESO_CD_REC_VOL:
	case ESO_AUXB_REC_VOL:
		if (cp->type != AUDIO_MIXER_VALUE)
			return (EINVAL);
		
		/*
		 * Stereo-capable mixer ports: if we get a single-channel
		 * gain value passed in, then we duplicate it to both left
		 * and right channels.
		 */
		switch (cp->un.value.num_channels) {
		case 1:
			lgain = rgain = ESO_GAIN_TO_4BIT(
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			break;
		case 2:
			lgain = ESO_GAIN_TO_4BIT(
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]);
			rgain = ESO_GAIN_TO_4BIT(
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT]);
			break;
		default:
			return (EINVAL);
		}

		sc->sc_gain[cp->dev][ESO_LEFT] = lgain;
		sc->sc_gain[cp->dev][ESO_RIGHT] = rgain;
		eso_set_gain(sc, cp->dev);
		break;

	case ESO_MASTER_VOL:
		if (cp->type != AUDIO_MIXER_VALUE)
			return (EINVAL);

		/* Like above, but a precision of 6 bits. */
		switch (cp->un.value.num_channels) {
		case 1:
			lgain = rgain = ESO_GAIN_TO_6BIT(
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			break;
		case 2:
			lgain = ESO_GAIN_TO_6BIT(
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]);
			rgain = ESO_GAIN_TO_6BIT(
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT]);
			break;
		default:
			return (EINVAL);
		}

		sc->sc_gain[cp->dev][ESO_LEFT] = lgain;
		sc->sc_gain[cp->dev][ESO_RIGHT] = rgain;
		eso_set_gain(sc, cp->dev);
		break;

	case ESO_SPATIALIZER:
		if (cp->type != AUDIO_MIXER_VALUE ||
		    cp->un.value.num_channels != 1)
			return (EINVAL);

		sc->sc_gain[cp->dev][ESO_LEFT] =
		    sc->sc_gain[cp->dev][ESO_RIGHT] =
		    ESO_GAIN_TO_6BIT(
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
		eso_set_gain(sc, cp->dev);
		break;
		
	case ESO_MONO_PLAY_VOL:
	case ESO_MONO_REC_VOL:
		if (cp->type != AUDIO_MIXER_VALUE ||
		    cp->un.value.num_channels != 1)
			return (EINVAL);

		sc->sc_gain[cp->dev][ESO_LEFT] =
		    sc->sc_gain[cp->dev][ESO_RIGHT] =
		    ESO_GAIN_TO_4BIT(
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
		eso_set_gain(sc, cp->dev);
		break;
		
	case ESO_PCSPEAKER_VOL:
		if (cp->type != AUDIO_MIXER_VALUE ||
		    cp->un.value.num_channels != 1)
			return (EINVAL);

		sc->sc_gain[cp->dev][ESO_LEFT] =
		    sc->sc_gain[cp->dev][ESO_RIGHT] =
		    ESO_GAIN_TO_3BIT(
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
		eso_set_gain(sc, cp->dev);
		break;

	case ESO_SPATIALIZER_ENABLE:
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);

		sc->sc_spatializer = (cp->un.ord != 0);

		tmp = eso_read_mixreg(sc, ESO_MIXREG_SPAT);
		if (sc->sc_spatializer)
			tmp |= ESO_MIXREG_SPAT_ENB;
		else
			tmp &= ~ESO_MIXREG_SPAT_ENB;
		eso_write_mixreg(sc, ESO_MIXREG_SPAT,
		    tmp | ESO_MIXREG_SPAT_RSTREL);
		break;
		
	case ESO_MONOOUT_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);

		sc->sc_monooutsrc = cp->un.ord;

		tmp = eso_read_mixreg(sc, ESO_MIXREG_MPM);
		tmp &= ~ESO_MIXREG_MPM_MOMASK;
		tmp |= sc->sc_monooutsrc;
		eso_write_mixreg(sc, ESO_MIXREG_MPM, tmp);
		break;
		
	case ESO_RECORD_MONITOR:
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);

		sc->sc_recmon = (cp->un.ord != 0);
		
		tmp = eso_read_ctlreg(sc, ESO_CTLREG_ACTL);
		if (sc->sc_recmon)
			tmp |= ESO_CTLREG_ACTL_RECMON;
		else
			tmp &= ~ESO_CTLREG_ACTL_RECMON;
		eso_write_ctlreg(sc, ESO_CTLREG_ACTL, tmp);
		break;

	case ESO_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);

		return (eso_set_recsrc(sc, cp->un.ord));

	case ESO_MIC_PREAMP:
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);

		sc->sc_preamp = (cp->un.ord != 0);
		
		tmp = eso_read_mixreg(sc, ESO_MIXREG_MPM);
		tmp &= ~ESO_MIXREG_MPM_RESV0;
		if (sc->sc_preamp)
			tmp |= ESO_MIXREG_MPM_PREAMP;
		else
			tmp &= ~ESO_MIXREG_MPM_PREAMP;
		eso_write_mixreg(sc, ESO_MIXREG_MPM, tmp);
		break;
		
	default:
		return (EINVAL);
	}
	
	return (0);
}

HIDE int
eso_get_port(hdl, cp)
	void *hdl;
	mixer_ctrl_t *cp;
{
	struct eso_softc *sc = hdl;

	switch (cp->dev) {
	case ESO_DAC_PLAY_VOL:
	case ESO_MIC_PLAY_VOL:
	case ESO_LINE_PLAY_VOL:
	case ESO_SYNTH_PLAY_VOL:
	case ESO_CD_PLAY_VOL:
	case ESO_AUXB_PLAY_VOL:
	case ESO_MASTER_VOL:
	case ESO_RECORD_VOL:
	case ESO_DAC_REC_VOL:
	case ESO_MIC_REC_VOL:
	case ESO_LINE_REC_VOL:
	case ESO_SYNTH_REC_VOL:
	case ESO_CD_REC_VOL:
	case ESO_AUXB_REC_VOL:
		/*
		 * Stereo-capable ports: if a single-channel query is made,
		 * just return the left channel's value (since single-channel
		 * settings themselves are applied to both channels).
		 */
		switch (cp->un.value.num_channels) {
		case 1:
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_gain[cp->dev][ESO_LEFT];
			break;
		case 2:
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_gain[cp->dev][ESO_LEFT];
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_gain[cp->dev][ESO_RIGHT];
			break;
		default:
			return (EINVAL);
		}
		break;
		
	case ESO_MONO_PLAY_VOL:
	case ESO_PCSPEAKER_VOL:
	case ESO_MONO_REC_VOL:
	case ESO_SPATIALIZER:
		if (cp->un.value.num_channels != 1)
			return (EINVAL);
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
		    sc->sc_gain[cp->dev][ESO_LEFT];
		break;

	case ESO_RECORD_MONITOR:
		cp->un.ord = sc->sc_recmon;
		break;
		
	case ESO_RECORD_SOURCE:
		cp->un.ord = sc->sc_recsrc;
		break;

	case ESO_MONOOUT_SOURCE:
		cp->un.ord = sc->sc_monooutsrc;
		break;
		
	case ESO_SPATIALIZER_ENABLE:
		cp->un.ord = sc->sc_spatializer;
		break;
		
	case ESO_MIC_PREAMP:
		cp->un.ord = sc->sc_preamp;
		break;

	default:
		return (EINVAL);
	}


	return (0);
	
}

HIDE int
eso_query_devinfo(hdl, dip)
	void *hdl;
	mixer_devinfo_t *dip;
{

	switch (dip->index) {
	case ESO_DAC_PLAY_VOL:
		dip->mixer_class = ESO_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_MIC_PLAY_VOL:
		dip->mixer_class = ESO_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_LINE_PLAY_VOL:
		dip->mixer_class = ESO_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_SYNTH_PLAY_VOL:
		dip->mixer_class = ESO_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNfmsynth);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_MONO_PLAY_VOL:
		dip->mixer_class = ESO_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "mono_in");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_CD_PLAY_VOL:
		dip->mixer_class = ESO_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNcd);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_AUXB_PLAY_VOL:
		dip->mixer_class = ESO_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "auxb");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case ESO_MIC_PREAMP:
		dip->mixer_class = ESO_MICROPHONE_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNpreamp);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		break;
	case ESO_MICROPHONE_CLASS:
		dip->mixer_class = ESO_MICROPHONE_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->type = AUDIO_MIXER_CLASS;
		break;
		
	case ESO_INPUT_CLASS:
		dip->mixer_class = ESO_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		dip->type = AUDIO_MIXER_CLASS;
		break;
		
	case ESO_MASTER_VOL:
		dip->mixer_class = ESO_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmaster);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_PCSPEAKER_VOL:
		dip->mixer_class = ESO_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "pc_speaker");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_MONOOUT_SOURCE:
		dip->mixer_class = ESO_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "mono_out");
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 3;
		strcpy(dip->un.e.member[0].label.name, AudioNmute);
		dip->un.e.member[0].ord = ESO_MIXREG_MPM_MOMUTE;
		strcpy(dip->un.e.member[1].label.name, AudioNdac);
		dip->un.e.member[1].ord = ESO_MIXREG_MPM_MOA2R;
		strcpy(dip->un.e.member[2].label.name, AudioNmixerout);
		dip->un.e.member[2].ord = ESO_MIXREG_MPM_MOREC;
		break;
	case ESO_SPATIALIZER:
		dip->mixer_class = ESO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = ESO_SPATIALIZER_ENABLE;
		strcpy(dip->label.name, AudioNspatial);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, "level");
		break;
	case ESO_SPATIALIZER_ENABLE:
		dip->mixer_class = ESO_OUTPUT_CLASS;
		dip->prev = ESO_SPATIALIZER;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "enable");
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		break;
	
	case ESO_OUTPUT_CLASS:
		dip->mixer_class = ESO_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		dip->type = AUDIO_MIXER_CLASS;
		break;

	case ESO_RECORD_MONITOR:
		dip->mixer_class = ESO_MONITOR_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmute);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		break;
	case ESO_MONITOR_CLASS:
		dip->mixer_class = ESO_MONITOR_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCmonitor);
		dip->type = AUDIO_MIXER_CLASS;
		break;

	case ESO_RECORD_VOL:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNrecord);
		dip->type = AUDIO_MIXER_VALUE;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_RECORD_SOURCE:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 4;
		strcpy(dip->un.e.member[0].label.name, AudioNmicrophone);
		dip->un.e.member[0].ord = ESO_MIXREG_ERS_MIC;
		strcpy(dip->un.e.member[1].label.name, AudioNline);
		dip->un.e.member[1].ord = ESO_MIXREG_ERS_LINE;
		strcpy(dip->un.e.member[2].label.name, AudioNcd);
		dip->un.e.member[2].ord = ESO_MIXREG_ERS_CD;
		strcpy(dip->un.e.member[3].label.name, AudioNmixerout);
		dip->un.e.member[3].ord = ESO_MIXREG_ERS_MIXER;
		break;
	case ESO_DAC_REC_VOL:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_MIC_REC_VOL:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_LINE_REC_VOL:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_SYNTH_REC_VOL:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNfmsynth);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_MONO_REC_VOL:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "mono_in");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 1; /* No lies */
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_CD_REC_VOL:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNcd);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_AUXB_REC_VOL:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "auxb");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case ESO_RECORD_CLASS:
		dip->mixer_class = ESO_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		dip->type = AUDIO_MIXER_CLASS;
		break;
		
	default:
		return (ENXIO);
	}

	return (0);
}

HIDE int
eso_allocmem(sc, size, align, boundary, flags, ed)
	struct eso_softc *sc;
	size_t size;
	size_t align;
	size_t boundary;
	int flags;
	struct eso_dma *ed;
{
	int error, wait;

	wait = (flags & M_NOWAIT) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK;
	ed->ed_size = size;
	
	error = bus_dmamem_alloc(sc->sc_dmat, ed->ed_size, align, boundary,
	    ed->ed_segs, sizeof (ed->ed_segs) / sizeof (ed->ed_segs[0]),
	    &ed->ed_nsegs, wait);
	if (error)
		goto out;

	error = bus_dmamem_map(sc->sc_dmat, ed->ed_segs, ed->ed_nsegs,
	    ed->ed_size, &ed->ed_addr, wait | BUS_DMA_COHERENT);
	if (error)
		goto free;

	error = bus_dmamap_create(sc->sc_dmat, ed->ed_size, 1, ed->ed_size, 0,
	    wait, &ed->ed_map);
	if (error)
		goto unmap;

	error = bus_dmamap_load(sc->sc_dmat, ed->ed_map, ed->ed_addr,
	    ed->ed_size, NULL, wait);
	if (error)
		goto destroy;

	return (0);

 destroy:
	bus_dmamap_destroy(sc->sc_dmat, ed->ed_map);
 unmap:
	bus_dmamem_unmap(sc->sc_dmat, ed->ed_addr, ed->ed_size);
 free:
	bus_dmamem_free(sc->sc_dmat, ed->ed_segs, ed->ed_nsegs);
 out:
	return (error);
}

HIDE void
eso_freemem(sc, ed)
	struct eso_softc *sc;
	struct eso_dma *ed;
{

	bus_dmamap_unload(sc->sc_dmat, ed->ed_map);
	bus_dmamap_destroy(sc->sc_dmat, ed->ed_map);
	bus_dmamem_unmap(sc->sc_dmat, ed->ed_addr, ed->ed_size);
	bus_dmamem_free(sc->sc_dmat, ed->ed_segs, ed->ed_nsegs);
}
	
HIDE void *
#ifdef __OpenBSD__
eso_allocm(hdl, size, type, flags)
#else
eso_allocm(hdl, direction, size, type, flags)
#endif
	void *hdl;
#ifdef __OpenBSD__
	u_long size;
#else
	int direction;
	size_t size;
#endif
	int type, flags;
{
	struct eso_softc *sc = hdl;
	struct eso_dma *ed;
	size_t boundary;
	int error;

	if ((ed = malloc(size, type, flags)) == NULL)
		return (NULL);

	/*
	 * Apparently the Audio 1 DMA controller's current address
	 * register can't roll over a 64K address boundary, so we have to
	 * take care of that ourselves.  The second channel DMA controller
	 * doesn't have that restriction, however.
	 */
#ifdef __OpenBSD__
	boundary = 0x10000;
#else
	if (direction == AUMODE_RECORD)
		boundary = 0x10000;
	else
		boundary = 0;
#endif

	error = eso_allocmem(sc, size, 32, boundary, flags, ed);
	if (error) {
		free(ed, type);
		return (NULL);
	}
	ed->ed_next = sc->sc_dmas;
	sc->sc_dmas = ed;

	return (KVADDR(ed));
}

HIDE void
eso_freem(hdl, addr, type)
	void *hdl;
	void *addr;
	int type;
{
	struct eso_softc *sc;
	struct eso_dma *p, **pp;

	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &p->ed_next) {
		if (KVADDR(p) == addr) {
			eso_freemem(sc, p);
			*pp = p->ed_next;
			free(p, type);
			return;
		}
	}
}

#ifdef __OpenBSD__
u_long
eso_round_buffersize(hdl, bufsize)
#else
HIDE size_t
eso_round_buffersize(hdl, direction, bufsize)
#endif
	void *hdl;
#ifdef __OpenBSD__
	u_long bufsize;
#else
	int direction;
	size_t bufsize;
#endif
{

	/* 64K restriction: ISA at eleven? */
	if (bufsize > 65536)
		bufsize = 65536;

	return (bufsize);
}

HIDE int
eso_mappage(hdl, addr, offs, prot)
	void *hdl;
	void *addr;
	int offs;
	int prot;
{
	struct eso_softc *sc = hdl;
	struct eso_dma *ed;

	if (offs < 0)
		return (-1);
	for (ed = sc->sc_dmas; ed != NULL && KVADDR(ed) == addr;
	     ed = ed->ed_next)
		;
	if (ed == NULL)
		return (-1);
	
	return (bus_dmamem_mmap(sc->sc_dmat, ed->ed_segs, ed->ed_nsegs,
	    offs, prot, BUS_DMA_WAITOK));
}

/* ARGSUSED */
HIDE int
eso_get_props(hdl)
	void *hdl;
{

	return (AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT |
	    AUDIO_PROP_FULLDUPLEX);
}

HIDE int
eso_trigger_output(hdl, start, end, blksize, intr, arg, param)
	void *hdl;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct eso_softc *sc = hdl;
	struct eso_dma *ed;
	uint8_t a2c1;
	
	DPRINTF((
	    "%s: trigger_output: start %p, end %p, blksize %d, intr %p(%p)\n",
	    sc->sc_dev.dv_xname, start, end, blksize, intr, arg));
	DPRINTF(("%s: param: rate %lu, encoding %u, precision %u, channels %u, sw_code %p, factor %d\n",
	    sc->sc_dev.dv_xname, param->sample_rate, param->encoding,
	    param->precision, param->channels, param->sw_code, param->factor));
	
	/* Find DMA buffer. */
	for (ed = sc->sc_dmas; ed != NULL && KVADDR(ed) != start;
	     ed = ed->ed_next)
		;
	if (ed == NULL) {
		printf("%s: trigger_output: bad addr %p\n",
		    sc->sc_dev.dv_xname, start);
		return (EINVAL);
	}
	
	sc->sc_pintr = intr;
	sc->sc_parg = arg;

	/* DMA transfer count (in `words'!) reload using 2's complement. */
	blksize = -(blksize >> 1);
	eso_write_mixreg(sc, ESO_MIXREG_A2TCRLO, blksize & 0xff);
	eso_write_mixreg(sc, ESO_MIXREG_A2TCRHI, blksize >> 8);

	/* Update DAC to reflect DMA count and audio parameters. */
	/* Note: we cache A2C2 in order to avoid r/m/w at interrupt time. */
	if (param->precision * param->factor == 16)
		sc->sc_a2c2 |= ESO_MIXREG_A2C2_16BIT;
	else
		sc->sc_a2c2 &= ~ESO_MIXREG_A2C2_16BIT;
	if (param->channels == 2)
		sc->sc_a2c2 |= ESO_MIXREG_A2C2_STEREO;
	else
		sc->sc_a2c2 &= ~ESO_MIXREG_A2C2_STEREO;
	if (param->encoding == AUDIO_ENCODING_SLINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_LE)
		sc->sc_a2c2 |= ESO_MIXREG_A2C2_SIGNED;
	else
		sc->sc_a2c2 &= ~ESO_MIXREG_A2C2_SIGNED;
	/* Unmask IRQ. */
	sc->sc_a2c2 |= ESO_MIXREG_A2C2_IRQM;
	eso_write_mixreg(sc, ESO_MIXREG_A2C2, sc->sc_a2c2);
	
	/* Set up DMA controller. */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ESO_IO_A2DMAA,
	    htopci(DMAADDR(ed)));
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ESO_IO_A2DMAC,
	    htopci((uint8_t *)end - (uint8_t *)start));
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ESO_IO_A2DMAM,
	    ESO_IO_A2DMAM_DMAENB | ESO_IO_A2DMAM_AUTO);
	
	/* Start DMA. */
	a2c1 = eso_read_mixreg(sc, ESO_MIXREG_A2C1);
	a2c1 &= ~ESO_MIXREG_A2C1_RESV0; /* Paranoia? XXX bit 5 */
	a2c1 |= ESO_MIXREG_A2C1_FIFOENB | ESO_MIXREG_A2C1_DMAENB |
	    ESO_MIXREG_A2C1_AUTO;
	eso_write_mixreg(sc, ESO_MIXREG_A2C1, a2c1);
	
	return (0);
}

HIDE int
eso_trigger_input(hdl, start, end, blksize, intr, arg, param)
	void *hdl;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct eso_softc *sc = hdl;
	struct eso_dma *ed;
	uint8_t actl, a1c1;

	DPRINTF((
	    "%s: trigger_input: start %p, end %p, blksize %d, intr %p(%p)\n",
	    sc->sc_dev.dv_xname, start, end, blksize, intr, arg));
	DPRINTF(("%s: param: rate %lu, encoding %u, precision %u, channels %u, sw_code %p, factor %d\n",
	    sc->sc_dev.dv_xname, param->sample_rate, param->encoding,
	    param->precision, param->channels, param->sw_code, param->factor));

	/*
	 * If we failed to configure the Audio 1 DMA controller, bail here
	 * while retaining availability of the DAC direction (in Audio 2).
	 */
	if (!sc->sc_dmac_configured)
		return (EIO);

	/* Find DMA buffer. */
	for (ed = sc->sc_dmas; ed != NULL && KVADDR(ed) != start;
	     ed = ed->ed_next)
		;
	if (ed == NULL) {
		printf("%s: trigger_output: bad addr %p\n",
		    sc->sc_dev.dv_xname, start);
		return (EINVAL);
	}

	sc->sc_rintr = intr;
	sc->sc_rarg = arg;

	/* Set up ADC DMA converter parameters. */
	actl = eso_read_ctlreg(sc, ESO_CTLREG_ACTL);
	if (param->channels == 2) {
		actl &= ~ESO_CTLREG_ACTL_MONO;
		actl |= ESO_CTLREG_ACTL_STEREO;
	} else {
		actl &= ~ESO_CTLREG_ACTL_STEREO;
		actl |= ESO_CTLREG_ACTL_MONO;
	}
	eso_write_ctlreg(sc, ESO_CTLREG_ACTL, actl);

	/* Set up Transfer Type: maybe move to attach time? */
	eso_write_ctlreg(sc, ESO_CTLREG_A1TT, ESO_CTLREG_A1TT_DEMAND4);

	/* DMA transfer count reload using 2's complement. */
	blksize = -blksize;
	eso_write_ctlreg(sc, ESO_CTLREG_A1TCRLO, blksize & 0xff);
	eso_write_ctlreg(sc, ESO_CTLREG_A1TCRHI, blksize >> 8);

	/* Set up and enable Audio 1 DMA FIFO. */
	a1c1 = ESO_CTLREG_A1C1_RESV1 | ESO_CTLREG_A1C1_FIFOENB;
	if (param->precision * param->factor == 16)
		a1c1 |= ESO_CTLREG_A1C1_16BIT;
	if (param->channels == 2)
		a1c1 |= ESO_CTLREG_A1C1_STEREO;
	else
		a1c1 |= ESO_CTLREG_A1C1_MONO;
	if (param->encoding == AUDIO_ENCODING_SLINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_LE)
		a1c1 |= ESO_CTLREG_A1C1_SIGNED;
	eso_write_ctlreg(sc, ESO_CTLREG_A1C1, a1c1);

	/* Set up ADC IRQ/DRQ parameters. */
	eso_write_ctlreg(sc, ESO_CTLREG_LAIC,
	    ESO_CTLREG_LAIC_PINENB | ESO_CTLREG_LAIC_EXTENB);
	eso_write_ctlreg(sc, ESO_CTLREG_DRQCTL,
	    ESO_CTLREG_DRQCTL_ENB1 | ESO_CTLREG_DRQCTL_EXTENB);

	/* Set up and enable DMA controller. */
	bus_space_write_1(sc->sc_dmac_iot, sc->sc_dmac_ioh, ESO_DMAC_CLEAR, 0);
	bus_space_write_1(sc->sc_dmac_iot, sc->sc_dmac_ioh, ESO_DMAC_MASK,
	    ESO_DMAC_MASK_MASK);
	bus_space_write_1(sc->sc_dmac_iot, sc->sc_dmac_ioh, ESO_DMAC_MODE,
	    DMA37MD_WRITE | DMA37MD_LOOP | DMA37MD_DEMAND);
	bus_space_write_4(sc->sc_dmac_iot, sc->sc_dmac_ioh, ESO_DMAC_DMAA,
	    htopci(DMAADDR(ed)));
	bus_space_write_2(sc->sc_dmac_iot, sc->sc_dmac_ioh, ESO_DMAC_DMAC,
	    htopci((uint8_t *)end - (uint8_t *)start - 1));
	bus_space_write_1(sc->sc_dmac_iot, sc->sc_dmac_ioh, ESO_DMAC_MASK, 0);

	/* Start DMA. */
	eso_write_ctlreg(sc, ESO_CTLREG_A1C2,
	    ESO_CTLREG_A1C2_DMAENB | ESO_CTLREG_A1C2_READ |
	    ESO_CTLREG_A1C2_AUTO | ESO_CTLREG_A1C2_ADC);

	return (0);
}

HIDE int
eso_set_recsrc(sc, recsrc)
	struct eso_softc *sc;
	unsigned int recsrc;
{

	eso_write_mixreg(sc, ESO_MIXREG_ERS, recsrc);
	sc->sc_recsrc = recsrc;
	return (0);
}

HIDE void
eso_set_gain(sc, port)
	struct eso_softc *sc;
	unsigned int port;
{
	uint8_t mixreg, tmp;

	switch (port) {
	case ESO_DAC_PLAY_VOL:
		mixreg = ESO_MIXREG_PVR_A2;
		break;
	case ESO_MIC_PLAY_VOL:
		mixreg = ESO_MIXREG_PVR_MIC;
		break;
	case ESO_LINE_PLAY_VOL:
		mixreg = ESO_MIXREG_PVR_LINE;
		break;
	case ESO_SYNTH_PLAY_VOL:
		mixreg = ESO_MIXREG_PVR_SYNTH;
		break;
	case ESO_CD_PLAY_VOL:
		mixreg = ESO_MIXREG_PVR_CD;
		break;
	case ESO_AUXB_PLAY_VOL:
		mixreg = ESO_MIXREG_PVR_AUXB;
		break;
		    
	case ESO_DAC_REC_VOL:
		mixreg = ESO_MIXREG_RVR_A2;
		break;
	case ESO_MIC_REC_VOL:
		mixreg = ESO_MIXREG_RVR_MIC;
		break;
	case ESO_LINE_REC_VOL:
		mixreg = ESO_MIXREG_RVR_LINE;
		break;
	case ESO_SYNTH_REC_VOL:
		mixreg = ESO_MIXREG_RVR_SYNTH;
		break;
	case ESO_CD_REC_VOL:
		mixreg = ESO_MIXREG_RVR_CD;
		break;
	case ESO_AUXB_REC_VOL:
		mixreg = ESO_MIXREG_RVR_AUXB;
		break;
	case ESO_MONO_PLAY_VOL:
		mixreg = ESO_MIXREG_PVR_MONO;
		break;
	case ESO_MONO_REC_VOL:
		mixreg = ESO_MIXREG_RVR_MONO;
		break;
		
	case ESO_PCSPEAKER_VOL:
		/* Special case - only 3-bit, mono, and reserved bits. */
		tmp = eso_read_mixreg(sc, ESO_MIXREG_PCSVR);
		tmp &= ESO_MIXREG_PCSVR_RESV;
		/* Map bits 7:5 -> 2:0. */
		tmp |= (sc->sc_gain[port][ESO_LEFT] >> 5);
		eso_write_mixreg(sc, ESO_MIXREG_PCSVR, tmp);
		return;

	case ESO_MASTER_VOL:
		/* Special case - separate regs, and 6-bit precision. */
		/* Map bits 7:2 -> 5:0. */
		eso_write_mixreg(sc, ESO_MIXREG_LMVM,
		    sc->sc_gain[port][ESO_LEFT] >> 2);
		eso_write_mixreg(sc, ESO_MIXREG_RMVM,
		    sc->sc_gain[port][ESO_RIGHT] >> 2);
		return;

	case ESO_SPATIALIZER:
		/* Special case - only `mono', and higher precision. */
		eso_write_mixreg(sc, ESO_MIXREG_SPATLVL,
		    sc->sc_gain[port][ESO_LEFT]);
		return;
		
	case ESO_RECORD_VOL:
		/* Very Special case, controller register. */
		eso_write_ctlreg(sc, ESO_CTLREG_RECLVL,ESO_4BIT_GAIN_TO_STEREO(
		   sc->sc_gain[port][ESO_LEFT], sc->sc_gain[port][ESO_RIGHT]));
		return;

	default:
#ifdef DIAGNOSTIC		
		panic("eso_set_gain: bad port %u", port);
		/* NOTREACHED */
#else
		return;
#endif		
		}

	eso_write_mixreg(sc, mixreg, ESO_4BIT_GAIN_TO_STEREO(
	    sc->sc_gain[port][ESO_LEFT], sc->sc_gain[port][ESO_RIGHT]));
}
