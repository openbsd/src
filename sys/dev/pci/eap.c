/*	$NetBSD: eap.c,v 1.6 1998/05/26 13:28:03 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author:      Lennart Augustsson <augustss@cs.chalmers.se>
 *
 * Debugging:   Andreas Gustafsson <gson@araneus.fi>
 *		Charles Hannum     <mycroft@netbsd.org>
 * Testing:     Chuck Cranor       <chuck@maria.wustl.edu>
 *              Phil Nelson        <phil@cs.wwu.edu>
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

/* 
 * Ensoniq AudoiPCI ES1370 + AK4531 driver. 
 * Data sheets can be found at 
 * http://www.ensoniq.com/multimedia/semi_html/html/es1370.zip
 * and
 * http://206.214.38.151/pdf/4531.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <machine/bus.h>

/* NetBSD 1.3 backwards compatibility */
#ifndef BUS_DMA_COHERENT
#define BUS_DMA_COHERENT 0	/* XXX */
struct        cfdriver eap_cd = {
      NULL, "eap", DV_DULL
};
#endif
 
#define	PCI_CBIO		0x10

#define EAP_ICSC		0x00    /* interrupt / chip select control */
#define  EAP_SERR_DISABLE	0x00000001
#define  EAP_CDC_EN		0x00000002
#define  EAP_JYSTK_EN		0x00000004
#define  EAP_UART_EN		0x00000008
#define  EAP_ADC_EN		0x00000010
#define  EAP_DAC2_EN		0x00000020
#define  EAP_DAC1_EN		0x00000040
#define  EAP_BREQ		0x00000080
#define  EAP_XTCL0		0x00000100
#define  EAP_M_CB		0x00000200
#define  EAP_CCB_INTRM		0x00000400
#define  EAP_DAC_SYNC		0x00000800
#define  EAP_WTSRSEL		0x00003000
#define   EAP_WTSRSEL_5		0x00000000
#define   EAP_WTSRSEL_11	0x00001000
#define   EAP_WTSRSEL_22	0x00002000
#define   EAP_WTSRSEL_44	0x00003000
#define  EAP_M_SBB		0x00004000
#define  EAP_MSFMTSEL		0x00008000
#define  EAP_SET_PCLKDIV(n)	(((n)&0x1fff)<<16)
#define  EAP_GET_PCLKDIV(n)	(((n)>>16)&0x1fff)
#define  EAP_PCLKBITS		0x1fff0000
#define  EAP_XTCL1		0x40000000
#define  EAP_ADC_STOP		0x80000000

#define EAP_ICSS		0x04	/* interrupt / chip select status */
#define  EAP_I_ADC		0x00000001
#define  EAP_I_DAC2		0x00000002
#define  EAP_I_DAC1		0x00000004
#define  EAP_I_UART		0x00000008
#define  EAP_I_MCCB		0x00000010
#define  EAP_VC			0x00000060
#define  EAP_CWRIP		0x00000100
#define  EAP_CBUSY		0x00000200
#define  EAP_CSTAT		0x00000400
#define  EAP_INTR		0x80000000

#define EAP_UART_DATA		0x08
#define EAP_UART_STATUS		0x09
#define EAP_UART_CONTROL	0x09
#define EAP_MEMPAGE		0x0c
#define EAP_CODEC		0x10
#define  EAP_SET_CODEC(a,d)	(((a)<<8) | (d))

#define EAP_SIC			0x20
#define  EAP_P1_S_MB		0x00000001
#define  EAP_P1_S_EB		0x00000002
#define  EAP_P2_S_MB		0x00000004
#define  EAP_P2_S_EB		0x00000008
#define  EAP_R1_S_MB		0x00000010
#define  EAP_R1_S_EB		0x00000020
#define  EAP_R1P2_BITS		0x0000003c
#define  EAP_P2_DAC_SEN		0x00000040
#define  EAP_P1_SCT_RLD		0x00000080
#define  EAP_P1_INTR_EN		0x00000100
#define  EAP_P2_INTR_EN		0x00000200
#define  EAP_R1_INTR_EN		0x00000400
#define  EAP_P1_PAUSE		0x00000800
#define  EAP_P2_PAUSE		0x00001000
#define  EAP_P1_LOOP_SEL	0x00002000
#define  EAP_P2_LOOP_SEL	0x00004000
#define  EAP_R1_LOOP_SEL	0x00008000
#define  EAP_SET_P2_ST_INC(i)	((i) << 16)
#define  EAP_SET_P2_END_INC(i)	((i) << 19)
#define  EAP_INC_BITS		0x003f0000

#define EAP_DAC1_CSR		0x24
#define EAP_DAC2_CSR		0x28
#define EAP_ADC_CSR		0x2c
#define  EAP_GET_CURRSAMP(r)	((r) >> 16)

#define EAP_DAC_PAGE		0xc
#define EAP_ADC_PAGE		0xd
#define EAP_UART_PAGE1		0xe
#define EAP_UART_PAGE2		0xf

#define EAP_DAC1_ADDR		0x30
#define EAP_DAC1_SIZE		0x34
#define EAP_DAC2_ADDR		0x38
#define EAP_DAC2_SIZE		0x3c
#define EAP_ADC_ADDR		0x30
#define EAP_ADC_SIZE		0x34
#define  EAP_SET_SIZE(c,s)	(((c)<<16) | (s))

#define EAP_XTAL_FREQ 1411200 /* 22.5792 / 16 MHz */

/* AK4531 registers */
#define AK_MASTER_L		0x00
#define AK_MASTER_R		0x01
#define AK_VOICE_L		0x02
#define AK_VOICE_R		0x03
#define AK_FM_L			0x04
#define AK_FM_R			0x05
#define AK_CD_L			0x06
#define AK_CD_R			0x07
#define AK_LINE_L		0x08
#define AK_LINE_R		0x09
#define AK_AUX_L		0x0a
#define AK_AUX_R		0x0b
#define AK_MONO1		0x0c
#define AK_MONO2		0x0d
#define AK_MIC			0x0e
#define AK_MONO			0x0f
#define AK_OUT_MIXER1		0x10
#define  AK_M_FM_L		0x40
#define  AK_M_FM_R		0x20
#define  AK_M_LINE_L		0x10
#define  AK_M_LINE_R		0x08
#define  AK_M_CD_L		0x04
#define  AK_M_CD_R		0x02
#define  AK_M_MIC		0x01
#define AK_OUT_MIXER2		0x11
#define  AK_M_AUX_L		0x20
#define  AK_M_AUX_R		0x10
#define  AK_M_VOICE_L		0x08
#define  AK_M_VOICE_R		0x04
#define  AK_M_MONO2		0x02
#define  AK_M_MONO1		0x01
#define AK_IN_MIXER1_L		0x12
#define AK_IN_MIXER1_R		0x13
#define AK_IN_MIXER2_L		0x14
#define AK_IN_MIXER2_R		0x15
#define  AK_M_TMIC		0x80
#define  AK_M_TMONO1		0x40
#define  AK_M_TMONO2		0x20
#define  AK_M2_AUX_L		0x10
#define  AK_M2_AUX_R		0x08
#define  AK_M_VOICE		0x04
#define  AK_M2_MONO2		0x02
#define  AK_M2_MONO1		0x01
#define AK_RESET		0x16
#define  AK_PD			0x02
#define  AK_NRST		0x01
#define AK_CS			0x17
#define AK_ADSEL		0x18
#define AK_MGAIN		0x19

#define AK_NPORTS 16

#define VOL_TO_ATT5(v) (0x1f - ((v) >> 3))
#define VOL_TO_GAIN5(v) VOL_TO_ATT5(v)
#define ATT5_TO_VOL(v) ((0x1f - (v)) << 3)
#define GAIN5_TO_VOL(v) ATT5_TO_VOL(v)
#define VOL_0DB 200

#define EAP_MASTER_VOL		0
#define EAP_VOICE_VOL		1
#define EAP_FM_VOL		2
#define EAP_CD_VOL		3
#define EAP_LINE_VOL		4
#define EAP_AUX_VOL		5
#define EAP_MIC_VOL		6
#define	EAP_RECORD_SOURCE 	7
#define EAP_OUTPUT_CLASS	8
#define EAP_RECORD_CLASS	9
#define EAP_INPUT_CLASS		10

#define EAP_NDEVS		11


#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (eapdebug) printf x
#define DPRINTFN(n,x)	if (eapdebug>(n)) printf x
int	eapdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define __BROKEN_INDIRECT_CONFIG
#ifdef __BROKEN_INDIRECT_CONFIG
int	eap_match __P((struct device *, void *, void *));
#else
int	eap_match __P((struct device *, struct cfdata *, void *));
#endif
void	eap_attach __P((struct device *, struct device *, void *));
int	eap_intr __P((void *));

struct eap_dma {
	bus_dmamap_t map;
        caddr_t addr;
        bus_dma_segment_t segs[1];
        int nsegs;
        size_t size;
        struct eap_dma *next;
};
#define DMAADDR(map) ((map)->segs[0].ds_addr)
#define KERNADDR(map) ((void *)((map)->addr))

struct eap_softc {
	struct device sc_dev;		/* base device */
	void *sc_ih;			/* interrupt vectoring */
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_dma_tag_t sc_dmatag;	/* DMA tag */

        struct eap_dma *sc_dmas;

	void	(*sc_pintr)(void *);	/* dma completion intr handler */
	void	*sc_parg;		/* arg for sc_intr() */
	char	sc_prun;

	void	(*sc_rintr)(void *);	/* dma completion intr handler */
	void	*sc_rarg;		/* arg for sc_intr() */
	char	sc_rrun;

	int	sc_sampsize;		/* bytes / sample */

	u_char	sc_port[AK_NPORTS];	/* mirror of the hardware setting */
	u_int	sc_record_source;	/* recording source mask */
};

int	eap_allocmem __P((struct eap_softc *, size_t, size_t, struct eap_dma *));
int	eap_freemem __P((struct eap_softc *, struct eap_dma *));

#define EWRITE2(sc, r, x) bus_space_write_2((sc)->iot, (sc)->ioh, (r), (x))
#define EWRITE4(sc, r, x) bus_space_write_4((sc)->iot, (sc)->ioh, (r), (x))
#define EREAD2(sc, r) bus_space_read_2((sc)->iot, (sc)->ioh, (r))
#define EREAD4(sc, r) bus_space_read_4((sc)->iot, (sc)->ioh, (r))

struct cfattach eap_ca = {
	sizeof(struct eap_softc), eap_match, eap_attach
};

int	eap_open __P((void *, int));
void	eap_close __P((void *));
int	eap_query_encoding __P((void *, struct audio_encoding *));
int	eap_set_params __P((void *, int, int, struct audio_params *, struct audio_params *));
int	eap_round_blocksize __P((void *, int));
int	eap_dma_init_output __P((void *, void *, int));
int	eap_dma_init_input __P((void *, void *, int));
int	eap_dma_output __P((void *, void *, int, void (*)(void *), void*));
int	eap_dma_input __P((void *, void *, int, void (*)(void *), void*));
int	eap_halt_in_dma __P((void *));
int	eap_halt_out_dma __P((void *));
int	eap_getdev __P((void *, struct audio_device *));
int	eap_mixer_set_port __P((void *, mixer_ctrl_t *));
int	eap_mixer_get_port __P((void *, mixer_ctrl_t *));
int	eap_query_devinfo __P((void *, mixer_devinfo_t *));
void   *eap_malloc __P((void *, u_long, int, int));
void	eap_free __P((void *, void *, int));
u_long	eap_round __P((void *, u_long));
int	eap_mappage __P((void *, void *, int, int));
int	eap_get_props __P((void *));
void	eap_write_codec __P((struct eap_softc *sc, int a, int d));
void	eap_set_mixer __P((struct eap_softc *sc, int a, int d));

struct audio_hw_if eap_hw_if = {
	eap_open,
	eap_close,
	NULL,
	eap_query_encoding,
	eap_set_params,
	eap_round_blocksize,
	NULL,
	eap_dma_init_output,
	eap_dma_init_input,
	eap_dma_output,
	eap_dma_input,
	eap_halt_out_dma,
	eap_halt_in_dma,
	NULL,
	eap_getdev,
	NULL,
	eap_mixer_set_port,
	eap_mixer_get_port,
	eap_query_devinfo,
	eap_malloc,
	eap_free,
	eap_round,
	eap_mappage,
	eap_get_props,
};

struct audio_device eap_device = {
	"Ensoniq AudioPCI",
	"",
	"eap"
};

int
eap_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_ENSONIQ)
		return (0);
	if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_ENSONIQ_AUDIOPCI)
		return (0);

	return (1);
}

void
eap_write_codec(sc, a, d)
	struct eap_softc *sc;
	int a, d;
{
	int icss;

	do {
	        icss = EREAD4(sc, EAP_ICSS);
		DPRINTFN(5,("eap: codec %d prog: icss=0x%08x\n", a, icss));
	} while(icss & EAP_CWRIP);
	EWRITE4(sc, EAP_CODEC, EAP_SET_CODEC(a, d));
}

void
eap_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct eap_softc *sc = (struct eap_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	char const *intrstr;
	bus_addr_t iobase;
	bus_size_t iosize;
	pci_intr_handle_t ih;
	pcireg_t csr;
	char devinfo[256];
	mixer_ctrl_t ctl;

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));

	/* Map I/O register */
	if (pci_io_find(pc, pa->pa_tag, PCI_CBIO, &iobase, &iosize)) {
		printf("%s: can't find i/o base\n", sc->sc_dev.dv_xname);
		return;
	}
#if defined(__OpenBSD__)
	if (bus_space_map(sc->iot, iobase, iosize, 0, &sc->ioh)) {
#else
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	      &sc->iot, &sc->ioh, NULL, NULL)) {
#endif
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_dmatag = pa->pa_dmat;

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, eap_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	/* Enable interrupts and looping mode. */
        EWRITE4(sc, EAP_SIC, EAP_P2_INTR_EN | EAP_R1_INTR_EN);
        EWRITE4(sc, EAP_ICSC, EAP_CDC_EN); /* enable the parts we need */

	eap_write_codec(sc, AK_RESET, AK_PD); /* reset codec */
	eap_write_codec(sc, AK_RESET, AK_PD | AK_NRST);	/* normal operation */
	eap_write_codec(sc, AK_CS, 0x0); /* select codec clocks */
	/* Enable all relevant mixer switches. */
	eap_write_codec(sc, AK_OUT_MIXER1, 
			AK_M_FM_L | AK_M_FM_R |
			AK_M_LINE_L | AK_M_LINE_R |
			AK_M_CD_L | AK_M_CD_R);
	eap_write_codec(sc, AK_OUT_MIXER2, 
			AK_M_AUX_L | AK_M_AUX_R |
			AK_M_VOICE_L | AK_M_VOICE_R |
			AK_M_MONO2 | AK_M_MONO1);
	ctl.type = AUDIO_MIXER_VALUE;
	ctl.un.value.num_channels = 1;
	for (ctl.dev = EAP_MASTER_VOL; ctl.dev < EAP_MIC_VOL; ctl.dev++) {
		ctl.un.value.level[AUDIO_MIXER_LEVEL_MONO] = VOL_0DB;
		eap_mixer_set_port(sc, &ctl);
	}
	ctl.un.value.level[AUDIO_MIXER_LEVEL_MONO] = 0;
	eap_mixer_set_port(sc, &ctl); /* set the mic to 0 */
	ctl.dev = EAP_RECORD_SOURCE;
	ctl.type = AUDIO_MIXER_SET;
	ctl.un.mask = 1 << EAP_MIC_VOL;
	eap_mixer_set_port(sc, &ctl);

        audio_attach_mi(&eap_hw_if, 0, sc, &sc->sc_dev);
}

int
eap_intr(p)
	void *p;
{
	struct eap_softc *sc = p;
	u_int32_t intr, sic;

        intr = EREAD4(sc, EAP_ICSS);
        if (!(intr & EAP_INTR))
        	return (0);
	sic = EREAD4(sc, EAP_SIC);
	DPRINTFN(5, ("eap_intr: ICSS=0x%08x, SIC=0x%08x\n", intr, sic));
        if (intr & EAP_I_ADC) {
        	/*
                 * XXX This is a hack!
                 * The EAP chip sometimes generates the recording interrupt
                 * while it is still transferring the data.  To make sure
                 * it has all arrived we busy wait until the count is right.
                 * The transfer we are waiting for is 8 longwords.
                 */
        	int s, nw, n;
                EWRITE4(sc, EAP_MEMPAGE, EAP_ADC_PAGE);
                s = EREAD4(sc, EAP_ADC_CSR);
                nw = ((s & 0xffff) + 1) / 4; /* # of words in DMA */
                n = 0;
                while (((EREAD4(sc, EAP_ADC_SIZE) >> 16) + 8) % nw == 0) {
                	delay(10);
                	if (++n > 100) {
                		printf("eapintr: dma fix timeout");
                                break;
                        }
                }
                /* Continue with normal interrupt handling. */
		EWRITE4(sc, EAP_SIC, sic & ~EAP_R1_INTR_EN);
		EWRITE4(sc, EAP_SIC, sic);
                if (sc->sc_rintr)
	        	sc->sc_rintr(sc->sc_rarg);
        }
        if (intr & EAP_I_DAC2) {
		EWRITE4(sc, EAP_SIC, sic & ~EAP_P2_INTR_EN);
		EWRITE4(sc, EAP_SIC, sic);
                if (sc->sc_pintr)
	        	sc->sc_pintr(sc->sc_parg);
        }
	return (1);
}

int
eap_allocmem(sc, size, align, p)
	struct eap_softc *sc;
	size_t size;
	size_t align;
        struct eap_dma *p;
{
	int error;

	p->size = size;
	error = bus_dmamem_alloc(sc->sc_dmatag, p->size, align, 0,
				 p->segs, sizeof(p->segs)/sizeof(p->segs[0]),
				 &p->nsegs, BUS_DMA_NOWAIT);
	if (error)
		return (error);

	error = bus_dmamem_map(sc->sc_dmatag, p->segs, p->nsegs, p->size, 
			       &p->addr, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error)
		goto free;

	error = bus_dmamap_create(sc->sc_dmatag, p->size, 1, p->size,
				  0, BUS_DMA_NOWAIT, &p->map);
	if (error)
		goto unmap;

	error = bus_dmamap_load(sc->sc_dmatag, p->map, p->addr, p->size, NULL, 
				BUS_DMA_NOWAIT);
	if (error)
		goto destroy;
	return (0);

destroy:
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
free:
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return (error);
}

int
eap_freemem(sc, p)
	struct eap_softc *sc;
        struct eap_dma *p;
{
	bus_dmamap_unload(sc->sc_dmatag, p->map);
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return (0);
}

int
eap_open(addr, flags)
	void *addr;
	int flags;
{
	struct eap_softc *sc = addr;

        DPRINTF(("eap_open: sc=%p\n", sc));

        sc->sc_pintr = 0;
        sc->sc_rintr = 0;

        return (0);
}

/*
 * Close function is called at splaudio().
 */
void
eap_close(addr)
	void *addr;
{
	struct eap_softc *sc = addr;
    
        eap_halt_in_dma(sc);
        eap_halt_out_dma(sc);

        sc->sc_pintr = 0;
        sc->sc_rintr = 0;
}

int
eap_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return (0);
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
        case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return (0);
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	default:
		return (EINVAL);
	}
}

int
eap_set_params(addr, setmode, usemode, p, r)
	void *addr;
	int setmode, usemode;
	struct audio_params *p, *r;
{
	struct eap_softc *sc = addr;
	void (*pswcode) __P((void *, u_char *buf, int cnt));
	void (*rswcode) __P((void *, u_char *buf, int cnt));
        u_int32_t mode, div;
        

        pswcode = rswcode = 0;
        switch (p->encoding) {
        case AUDIO_ENCODING_SLINEAR_BE:
        	if (p->precision == 16)
                	rswcode = pswcode = swap_bytes;
		else
			pswcode = rswcode = change_sign8;
		break;
        case AUDIO_ENCODING_SLINEAR_LE:
        	if (p->precision != 16)
			pswcode = rswcode = change_sign8;
        	break;
        case AUDIO_ENCODING_ULINEAR_BE:
        	if (p->precision == 16) {
			pswcode = swap_bytes_change_sign16;
			rswcode = change_sign16_swap_bytes;
		}
		break;
        case AUDIO_ENCODING_ULINEAR_LE:
        	if (p->precision == 16)
			pswcode = rswcode = change_sign16;
        	break;
        case AUDIO_ENCODING_ULAW:
        	pswcode = mulaw_to_ulinear8;
                rswcode = ulinear8_to_mulaw;
                break;
        case AUDIO_ENCODING_ALAW:
                pswcode = alaw_to_ulinear8;
                rswcode = ulinear8_to_alaw;
                break;
        default:
        	return (EINVAL);
        }
	if (p->precision == 16)
		mode = EAP_P2_S_EB | EAP_R1_S_EB;
	else
		mode = 0;
        if (p->channels == 2)
        	mode |= EAP_P2_S_MB | EAP_R1_S_MB;
	else if (p->channels != 1)
		return (EINVAL);
        if (p->sample_rate < 4000 || p->sample_rate > 50000)
        	return (EINVAL);

	sc->sc_sampsize = p->precision / 8 * p->channels; /* bytes / sample */
        p->sw_code = pswcode;
        r->sw_code = rswcode;

        /* Set the encoding */
        mode |= EREAD4(sc, EAP_SIC) & ~(EAP_R1P2_BITS | EAP_INC_BITS);
	mode |= EAP_SET_P2_ST_INC(0) | EAP_SET_P2_END_INC(p->precision / 8);
        EWRITE4(sc, EAP_SIC, mode);
	DPRINTFN(2, ("eap_set_params: set SIC = 0x%08x\n", mode));

        /* Set the speed */
	DPRINTFN(2, ("eap_set_params: old ICSC = 0x%08x\n", 
		     EREAD4(sc, EAP_ICSC)));
	div = EREAD4(sc, EAP_ICSC) & ~EAP_PCLKBITS;
        div |= EAP_SET_PCLKDIV(EAP_XTAL_FREQ / p->sample_rate - 2);
	div |= EAP_CCB_INTRM;
        EWRITE4(sc, EAP_ICSC, div);
	DPRINTFN(2, ("eap_set_params: set ICSC = 0x%08x\n", div));

        return (0);
}

int
eap_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	return (blk & -32);	/* keep good alignment */
}

int
eap_dma_init_input(addr, buf, cc)
	void *addr;
	void *buf;
	int cc;
{
	struct eap_softc *sc = addr;
	struct eap_dma *p;

	DPRINTF(("eap_dma_init_input: dma start loop input addr=%p cc=%d\n", 
		 buf, cc));
        for (p = sc->sc_dmas; p && KERNADDR(p) != buf; p = p->next)
		;
	if (!p) {
		printf("eap_dma_init_input: bad addr %p\n", buf);
		return (EINVAL);
	}
	EWRITE4(sc, EAP_MEMPAGE, EAP_ADC_PAGE);
	EWRITE4(sc, EAP_ADC_ADDR, DMAADDR(p));
	EWRITE4(sc, EAP_ADC_SIZE, EAP_SET_SIZE(0, cc / 4 - 1));
	DPRINTF(("eap_dma_init_input: ADC_ADDR=0x%x, ADC_SIZE=0x%x\n",
		 (int)DMAADDR(p), EAP_SET_SIZE(0, cc / 4 - 1)));
	return (0);
}

int
eap_dma_init_output(addr, buf, cc)
	void *addr;
	void *buf;
	int cc;
{
	struct eap_softc *sc = addr;
	struct eap_dma *p;

	DPRINTF(("eap: dma start loop output buf=%p cc=%d\n", buf, cc));
        for (p = sc->sc_dmas; p && KERNADDR(p) != buf; p = p->next)
		;
	if (!p) {
		printf("eap_dma_init_output: bad addr %p\n", buf);
		return (EINVAL);
	}
	EWRITE4(sc, EAP_MEMPAGE, EAP_DAC_PAGE);
	EWRITE4(sc, EAP_DAC2_ADDR, DMAADDR(p));
	EWRITE4(sc, EAP_DAC2_SIZE, EAP_SET_SIZE(0, cc / 4 - 1));
	DPRINTF(("eap_dma_init_output: DAC2_ADDR=0x%x, DAC2_SIZE=0x%x\n",
		 (int)DMAADDR(p), EAP_SET_SIZE(0, cc / 4 - 1)));
	return (0);
}

int
eap_dma_output(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr) __P((void *));
	void *arg;
{
	struct eap_softc *sc = addr;
	u_int32_t mode;

	DPRINTFN(sc->sc_prun ? 5 : 1, 
                 ("eap_dma_output: sc=%p buf=%p cc=%d intr=%p(%p)\n", 
                  addr, p, cc, intr, arg));
	sc->sc_pintr = intr;
	sc->sc_parg = arg;
	if (!sc->sc_prun) {
#if defined(DIAGNOSTIC) || defined(AUDIO_DEBUG)
	        if (sc->sc_sampsize == 0) {
        		printf("eap_dma_output: sampsize == 0\n");
                        return EINVAL;
                }
#endif
		EWRITE2(sc, EAP_DAC2_CSR, cc / sc->sc_sampsize - 1);
		DPRINTFN(1, ("eap_dma_output: set DAC2_CSR = %d\n", 
			     cc / sc->sc_sampsize - 1));
		DPRINTFN(1, ("eap_dma_output: old ICSC = 0x%08x\n",
			     EREAD4(sc, EAP_ICSC)));
		mode = EREAD4(sc, EAP_ICSC) & ~EAP_DAC2_EN;
		EWRITE4(sc, EAP_ICSC, mode);
		mode |= EAP_DAC2_EN;
		EWRITE4(sc, EAP_ICSC, mode);
		DPRINTFN(1, ("eap_dma_output: set ICSC = 0x%08x\n", mode));
		sc->sc_prun = 1;
	}
        return (0);
}

int
eap_dma_input(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr) __P((void *));
	void *arg;
{
	struct eap_softc *sc = addr;
	u_int32_t mode;

	DPRINTFN(1, ("eap_dma_input: sc=%p buf=%p cc=%d intr=%p(%p)\n", 
		     addr, p, cc, intr, arg));
	sc->sc_rintr = intr;
	sc->sc_rarg = arg;
	if (!sc->sc_rrun) {
#if defined(DIAGNOSTIC) || defined(AUDIO_DEBUG)
	        if (sc->sc_sampsize == 0) {
        		printf("eap_dma_input: sampsize == 0\n");
                        return EINVAL;
                }
#endif
		EWRITE2(sc, EAP_ADC_CSR, cc / sc->sc_sampsize - 1);
		mode = EREAD4(sc, EAP_ICSC) & ~EAP_ADC_EN;
		EWRITE4(sc, EAP_ICSC, mode);
		mode |= EAP_ADC_EN;
		EWRITE4(sc, EAP_ICSC, mode);
		DPRINTFN(1, ("eap_dma_input: set ICSC = 0x%08x\n", mode));
		sc->sc_rrun = 1;
	}
        return (0);
}

int
eap_halt_out_dma(addr)
	void *addr;
{
	struct eap_softc *sc = addr;
	u_int32_t mode;
	
        DPRINTF(("eap: eap_halt_out_dma\n"));
	mode = EREAD4(sc, EAP_ICSC) & ~EAP_DAC2_EN;
	EWRITE4(sc, EAP_ICSC, mode);
	sc->sc_prun = 0;
        return (0);
}

int
eap_halt_in_dma(addr)
	void *addr;
{
	struct eap_softc *sc = addr;
	u_int32_t mode;
    
        DPRINTF(("eap: eap_halt_in_dma\n"));
	mode = EREAD4(sc, EAP_ICSC) & ~EAP_ADC_EN;
	EWRITE4(sc, EAP_ICSC, mode);
	sc->sc_rrun = 0;
        return (0);
}

int
eap_getdev(addr, retp)
	void *addr;
        struct audio_device *retp;
{
	*retp = eap_device;
        return (0);
}

void
eap_set_mixer(sc, a, d)
	struct eap_softc *sc;
        int a, d;
{
	eap_write_codec(sc, a, d);
        DPRINTFN(1, ("eap_mixer_set_port port 0x%02x = 0x%02x\n", a, d));
}


int
eap_mixer_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct eap_softc *sc = addr;
	int lval, rval, l, r, la, ra;
	int l1, r1, l2, r2, m;

	if (cp->dev == EAP_RECORD_SOURCE) {
		if (cp->type != AUDIO_MIXER_SET)
			return (EINVAL);
		m = sc->sc_record_source = cp->un.mask;
		l1 = l2 = r1 = r2 = 0;
		if (m & (1 << EAP_VOICE_VOL)) 
			l2 |= AK_M_VOICE_L, r2 |= AK_M_VOICE_R;
		if (m & (1 << EAP_FM_VOL)) 
			l1 |= AK_M_FM_L, r1 |= AK_M_FM_R;
		if (m & (1 << EAP_CD_VOL)) 
			l1 |= AK_M_CD_L, r1 |= AK_M_CD_R;
		if (m & (1 << EAP_LINE_VOL)) 
			l1 |= AK_M_LINE_L, r1 |= AK_M_LINE_R;
		if (m & (1 << EAP_AUX_VOL)) 
			l2 |= AK_M_AUX_L, r2 |= AK_M_AUX_R;
		if (m & (1 << EAP_MIC_VOL)) 
			l2 |= AK_M_TMIC, r2 |= AK_M_TMIC;
		eap_set_mixer(sc, AK_IN_MIXER1_L, l1);		
		eap_set_mixer(sc, AK_IN_MIXER1_R, r1);
		eap_set_mixer(sc, AK_IN_MIXER2_L, l2);
		eap_set_mixer(sc, AK_IN_MIXER2_R, r2);
		return (0);
	}
	if (cp->type != AUDIO_MIXER_VALUE)
		return (EINVAL);
	if (cp->un.value.num_channels == 1)
		lval = rval = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
	else if (cp->un.value.num_channels == 2) {
		lval = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
		rval = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
	} else
		return (EINVAL);
	ra = -1;
	switch (cp->dev) {
	case EAP_MASTER_VOL:
		l = VOL_TO_ATT5(lval);
		r = VOL_TO_ATT5(rval);
		la = AK_MASTER_L;
		ra = AK_MASTER_R;
		break;
	case EAP_MIC_VOL:
		if (cp->un.value.num_channels != 1)
			return (EINVAL);
		la = AK_MIC;
		goto lr;
	case EAP_VOICE_VOL:
		la = AK_VOICE_L;
		ra = AK_VOICE_R;
		goto lr;
	case EAP_FM_VOL:
		la = AK_FM_L;
		ra = AK_FM_R;
		goto lr;
	case EAP_CD_VOL:
		la = AK_CD_L;
		ra = AK_CD_R;
		goto lr;
	case EAP_LINE_VOL:
		la = AK_LINE_L;
		ra = AK_LINE_R;
		goto lr;
	case EAP_AUX_VOL:
		la = AK_AUX_L;
		ra = AK_AUX_R;
	lr:
		l = VOL_TO_GAIN5(lval);
		r = VOL_TO_GAIN5(rval);
		break;
	default:
		return (EINVAL);
	}
	eap_set_mixer(sc, la, l);
	sc->sc_port[la] = l;
	if (ra >= 0) {
		eap_set_mixer(sc, ra, r);
		sc->sc_port[ra] = r;
	}
	return (0);
}

int
eap_mixer_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct eap_softc *sc = addr;
	int la, ra, l, r;

	switch (cp->dev) {
	case EAP_RECORD_SOURCE:
		cp->un.mask = sc->sc_record_source;
		return (0);
	case EAP_MASTER_VOL:
		l = ATT5_TO_VOL(sc->sc_port[AK_MASTER_L]);
		r = ATT5_TO_VOL(sc->sc_port[AK_MASTER_R]);
		break;
	case EAP_MIC_VOL:
		if (cp->un.value.num_channels != 1)
			return (EINVAL);
		la = ra = AK_MIC;
		goto lr;
	case EAP_VOICE_VOL:
		la = AK_VOICE_L;
		ra = AK_VOICE_R;
		goto lr;
	case EAP_FM_VOL:
		la = AK_FM_L;
		ra = AK_FM_R;
		goto lr;
	case EAP_CD_VOL:
		la = AK_CD_L;
		ra = AK_CD_R;
		goto lr;
	case EAP_LINE_VOL:
		la = AK_LINE_L;
		ra = AK_LINE_R;
		goto lr;
	case EAP_AUX_VOL:
		la = AK_AUX_L;
		ra = AK_AUX_R;
	lr:
		l = GAIN5_TO_VOL(sc->sc_port[la]);
		r = GAIN5_TO_VOL(sc->sc_port[ra]);
		break;
	default:
		return (EINVAL);
	}
	if (cp->un.value.num_channels == 1)
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = (l+r) / 2;
	else if (cp->un.value.num_channels == 2) {
		cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]  = l;
		cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
	}
	return (0);
}

int
eap_query_devinfo(addr, dip)
	void *addr;
	mixer_devinfo_t *dip;
{
	switch (dip->index) {
	case EAP_MASTER_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_OUTPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmaster);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);
	case EAP_VOICE_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);
	case EAP_FM_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNfmsynth);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);
	case EAP_CD_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNcd);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);
	case EAP_LINE_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);
	case EAP_AUX_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNaux);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);
	case EAP_MIC_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);
	case EAP_RECORD_SOURCE:
		dip->mixer_class = EAP_RECORD_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->type = AUDIO_MIXER_SET;
		dip->un.s.num_mem = 5;
		strcpy(dip->un.s.member[0].label.name, AudioNmicrophone);
		dip->un.s.member[0].mask = 1 << EAP_MIC_VOL;
		strcpy(dip->un.s.member[1].label.name, AudioNcd);
		dip->un.s.member[1].mask = 1 << EAP_CD_VOL;
		strcpy(dip->un.s.member[2].label.name, AudioNline);
		dip->un.s.member[2].mask = 1 << EAP_LINE_VOL;
		strcpy(dip->un.s.member[3].label.name, AudioNfmsynth);
		dip->un.s.member[3].mask = 1 << EAP_FM_VOL;
		strcpy(dip->un.s.member[4].label.name, AudioNaux);
		dip->un.s.member[4].mask = 1 << EAP_AUX_VOL;
		return (0);
	case EAP_OUTPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = EAP_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		return (0);
	case EAP_RECORD_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = EAP_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		return (0);
	case EAP_INPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		return (0);
	}
	return (ENXIO);
}

void *
eap_malloc(addr, size, pool, flags)
	void *addr;
	u_long size;
	int pool;
	int flags;
{
	struct eap_softc *sc = addr;
        struct eap_dma *p;
        int error;

        p = malloc(sizeof(*p), pool, flags);
        if (!p)
                return (0);
        error = eap_allocmem(sc, size, 16, p);
        if (error) {
                free(p, pool);
        	return (0);
        }
        p->next = sc->sc_dmas;
        sc->sc_dmas = p;
	return (KERNADDR(p));
}

void
eap_free(addr, ptr, pool)
	void *addr;
	void *ptr;
	int pool;
{
	struct eap_softc *sc = addr;
        struct eap_dma **p;

        for (p = &sc->sc_dmas; *p; p = &(*p)->next) {
                if (KERNADDR(*p) == ptr) {
                        eap_freemem(sc, *p);
                        *p = (*p)->next;
                        free(*p, pool);
                        return;
                }
        }
}

u_long
eap_round(addr, size)
	void *addr;
	u_long size;
{
	return (size);
}

int
eap_mappage(addr, mem, off, prot)
	void *addr;
        void *mem;
        int off;
	int prot;
{
	struct eap_softc *sc = addr;
        struct eap_dma *p;

        for (p = sc->sc_dmas; p && KERNADDR(p) != mem; p = p->next)
		;
	if (!p)
		return (-1);
	return (bus_dmamem_mmap(sc->sc_dmatag, p->segs, p->nsegs, 
				off, prot, BUS_DMA_WAITOK));
}

int
eap_get_props(addr)
	void *addr;
{
	return (AUDIO_PROP_MMAP | AUDIO_PROP_FULLDUPLEX);
}
