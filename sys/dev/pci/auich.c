/*	$OpenBSD: auich.c,v 1.65 2007/09/17 00:50:46 krw Exp $	*/

/*
 * Copyright (c) 2000,2001 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* #define	AUICH_DEBUG */
/*
 * AC'97 audio found on Intel 810/815/820/440MX chipsets.
 *	http://developer.intel.com/design/chipsets/datashts/290655.htm
 *	http://developer.intel.com/design/chipsets/manuals/298028.htm
 *	http://www.intel.com/design/chipsets/datashts/290716.htm
 *	http://www.intel.com/design/chipsets/datashts/290744.htm
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

#include <dev/ic/ac97.h>

/* 12.1.10 NAMBAR - native audio mixer base address register */
#define	AUICH_NAMBAR	0x10
/* 12.1.11 NABMBAR - native audio bus mastering base address register */
#define	AUICH_NABMBAR	0x14
#define	AUICH_CFG	0x41
#define	AUICH_CFG_IOSE	0x01
/* ICH4/ICH5/ICH6/ICH7 native audio mixer BAR */
#define	AUICH_MMBAR	0x18
/* ICH4/ICH5/ICH6/ICH7 native bus mastering BAR */
#define	AUICH_MBBAR	0x1c
#define	AUICH_S2CR	0x10000000	/* tertiary codec ready */

/* table 12-3. native audio bus master control registers */
#define	AUICH_BDBAR	0x00	/* 8-byte aligned address */
#define	AUICH_CIV		0x04	/* 5 bits current index value */
#define	AUICH_LVI		0x05	/* 5 bits last valid index value */
#define		AUICH_LVI_MASK	0x1f
#define	AUICH_STS		0x06	/* 16 bits status */
#define		AUICH_FIFOE	0x10	/* fifo error */
#define		AUICH_BCIS	0x08	/* r- buf cmplt int sts; wr ack */
#define		AUICH_LVBCI	0x04	/* r- last valid bci, wr ack */
#define		AUICH_CELV	0x02	/* current equals last valid */
#define		AUICH_DCH		0x01	/* dma halted */
#define		AUICH_ISTS_BITS	"\020\01dch\02celv\03lvbci\04bcis\05fifoe"
#define	AUICH_PICB	0x08	/* 16 bits */
#define	AUICH_PIV		0x0a	/* 5 bits prefetched index value */
#define	AUICH_CTRL	0x0b	/* control */
#define		AUICH_IOCE	0x10	/* int on completion enable */
#define		AUICH_FEIE	0x08	/* fifo error int enable */
#define		AUICH_LVBIE	0x04	/* last valid buf int enable */
#define		AUICH_RR		0x02	/* 1 - reset regs */
#define		AUICH_RPBM	0x01	/* 1 - run, 0 - pause */

#define	AUICH_PCMI	0x00
#define	AUICH_PCMO	0x10
#define	AUICH_MICI	0x20

#define	AUICH_GCTRL	0x2c
#define		AUICH_SSM_78	0x40000000	/* S/PDIF slots 7 and 8 */
#define		AUICH_SSM_69	0x80000000	/* S/PDIF slots 6 and 9 */
#define		AUICH_SSM_1011	0xc0000000	/* S/PDIF slots 10 and 11 */
#define		AUICH_POM16	0x000000	/* PCM out precision 16bit */
#define		AUICH_POM20	0x400000	/* PCM out precision 20bit */
#define		AUICH_PCM246_MASK 0x300000
#define		AUICH_PCM2	0x000000	/* 2ch output */
#define		AUICH_PCM4	0x100000	/* 4ch output */
#define		AUICH_PCM6	0x200000	/* 6ch output */
#define		AUICH_S2RIE	0x40	/* int when tertiary codec resume */
#define		AUICH_SRIE	0x20	/* int when 2ndary codec resume */
#define		AUICH_PRIE	0x10	/* int when primary codec resume */
#define		AUICH_ACLSO	0x08	/* aclink shut off */
#define		AUICH_WRESET	0x04	/* warm reset */
#define		AUICH_CRESET	0x02	/* cold reset */
#define		AUICH_GIE		0x01	/* gpi int enable */
#define	AUICH_GSTS	0x30
#define		AUICH_MD3		0x20000	/* pwr-dn semaphore for modem */
#define		AUICH_AD3		0x10000	/* pwr-dn semaphore for audio */
#define		AUICH_RCS		0x08000	/* read completion status */
#define		AUICH_B3S12	0x04000	/* bit 3 of slot 12 */
#define		AUICH_B2S12	0x02000	/* bit 2 of slot 12 */
#define		AUICH_B1S12	0x01000	/* bit 1 of slot 12 */
#define		AUICH_SRI		0x00800	/* secondary resume int */
#define		AUICH_PRI		0x00400	/* primary resume int */
#define		AUICH_SCR		0x00200	/* secondary codec ready */
#define		AUICH_PCR		0x00100	/* primary codec ready */
#define		AUICH_MINT	0x00080	/* mic in int */
#define		AUICH_POINT	0x00040	/* pcm out int */
#define		AUICH_PIINT	0x00020	/* pcm in int */
#define		AUICH_MOINT	0x00004	/* modem out int */
#define		AUICH_MIINT	0x00002	/* modem in int */
#define		AUICH_GSCI	0x00001	/* gpi status change */
#define		AUICH_GSTS_BITS	"\020\01gsci\02miict\03moint\06piint\07point\010mint\011pcr\012scr\013pri\014sri\015b1s12\016b2s12\017b3s12\020rcs\021ad3\022md3"
#define	AUICH_CAS		0x34	/* 1/8 bit */
#define	AUICH_SEMATIMO		1000	/* us */
#define	AUICH_RESETIMO		500000	/* us */

#define	ICH_SIS_NV_CTL	0x4c	/* some SiS/NVIDIA register.  From Linux */
#define		ICH_SIS_CTL_UNMUTE	0x01	/* un-mute the output */

/*
 * according to the dev/audiovar.h AU_RING_SIZE is 2^16, what fits
 * in our limits perfectly, i.e. setting it to higher value
 * in your kernel config would improve perfomance, still 2^21 is the max
 */
#define	AUICH_DMALIST_MAX	32
#define	AUICH_DMASEG_MAX	(65536*2)	/* 64k samples, 2x16 bit samples */
struct auich_dmalist {
	u_int32_t	base;
	u_int32_t	len;
#define	AUICH_DMAF_IOC	0x80000000	/* 1-int on complete */
#define	AUICH_DMAF_BUP	0x40000000	/* 0-retrans last, 1-transmit 0 */
};

#define	AUICH_FIXED_RATE 48000

struct auich_dma {
	bus_dmamap_t map;
	caddr_t addr;
	bus_dma_segment_t segs[AUICH_DMALIST_MAX];
	int nsegs;
	size_t size;
	struct auich_dma *next;
};

struct auich_softc {
	struct device sc_dev;
	void *sc_ih;

	audio_device_t sc_audev;

	bus_space_tag_t iot;
	bus_space_tag_t iot_mix;
	bus_space_handle_t mix_ioh;
	bus_space_handle_t aud_ioh;
	bus_dma_tag_t dmat;

	struct ac97_codec_if *codec_if;
	struct ac97_host_if host_if;

	/* dma scatter-gather buffer lists, aligned to 8 bytes */
	struct auich_dmalist *dmalist_pcmo, *dmap_pcmo;
	struct auich_dmalist *dmalist_pcmi, *dmap_pcmi;
	struct auich_dmalist *dmalist_mici, *dmap_mici;

	bus_dmamap_t dmalist_map;
	bus_dma_segment_t dmalist_seg[2];
	caddr_t dmalist_kva;
	bus_addr_t dmalist_pcmo_pa;
	bus_addr_t dmalist_pcmi_pa;
	bus_addr_t dmalist_mici_pa;

	/* i/o buffer pointers */
	u_int32_t pcmo_start, pcmo_p, pcmo_end;
	int pcmo_blksize, pcmo_fifoe;
	u_int32_t pcmi_start, pcmi_p, pcmi_end;
	int pcmi_blksize, pcmi_fifoe;
	u_int32_t mici_start, mici_p, mici_end;
	int mici_blksize, mici_fifoe;
	struct auich_dma *sc_dmas;

	void (*sc_pintr)(void *);
	void *sc_parg;

	void (*sc_rintr)(void *);
	void *sc_rarg;

	void *powerhook;
	int suspend;
	u_int16_t ext_ctrl;
	int sc_sample_size;
	int sc_sts_reg;
	int sc_ignore_codecready;
	int flags;
	int sc_ac97rate;
};

#ifdef AUICH_DEBUG
#define	DPRINTF(l,x)	do { if (auich_debug & (l)) printf x; } while(0)
int auich_debug = 0xfffe;
#define	AUICH_DEBUG_CODECIO	0x0001
#define	AUICH_DEBUG_DMA		0x0002
#define	AUICH_DEBUG_PARAM	0x0004
#else
#define	DPRINTF(x,y)	/* nothing */
#endif

struct cfdriver	auich_cd = {
	NULL, "auich", DV_DULL
};

int  auich_match(struct device *, void *, void *);
void auich_attach(struct device *, struct device *, void *);
int  auich_intr(void *);

struct cfattach auich_ca = {
	sizeof(struct auich_softc), auich_match, auich_attach
};

static const struct auich_devtype {
	int	vendor;
	int	product;
	int	options;
	char	name[8];
} auich_devices[] = {
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_6300ESB_ACA,	0, "ESB" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_6321ESB_ACA,	0, "ESB2" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801AA_ACA,	0, "ICH" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801AB_ACA,	0, "ICH0" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801BA_ACA,	0, "ICH2" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801CA_ACA,	0, "ICH3" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801DB_ACA,	0, "ICH4" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801EB_ACA,	0, "ICH5" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801FB_ACA,	0, "ICH6" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801GB_ACA,	0, "ICH7" },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82440MX_ACA,	0, "440MX" },
	{ PCI_VENDOR_SIS,	PCI_PRODUCT_SIS_7012_ACA,	0, "SiS7012" },
	{ PCI_VENDOR_NVIDIA,	PCI_PRODUCT_NVIDIA_NFORCE_ACA,	0, "nForce" },
	{ PCI_VENDOR_NVIDIA,	PCI_PRODUCT_NVIDIA_NFORCE2_ACA,	0, "nForce2" },
	{ PCI_VENDOR_NVIDIA,	PCI_PRODUCT_NVIDIA_NFORCE2_400_ACA,
	    0, "nForce2" },
	{ PCI_VENDOR_NVIDIA,	PCI_PRODUCT_NVIDIA_NFORCE3_ACA,	0, "nForce3" },
	{ PCI_VENDOR_NVIDIA,	PCI_PRODUCT_NVIDIA_NFORCE3_250_ACA,
	    0, "nForce3" },
	{ PCI_VENDOR_NVIDIA,	PCI_PRODUCT_NVIDIA_NFORCE4_AC,	0, "nForce4" },
	{ PCI_VENDOR_NVIDIA,	PCI_PRODUCT_NVIDIA_MCP04_AC97,	0, "MCP04" },
	{ PCI_VENDOR_NVIDIA,	PCI_PRODUCT_NVIDIA_MCP51_ACA,	0, "MCP51" },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_PBC768_ACA,	0, "AMD768" },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_8111_ACA,	0, "AMD8111" },
};

int auich_open(void *, int);
void auich_close(void *);
int auich_query_encoding(void *, struct audio_encoding *);
int auich_set_params(void *, int, int, struct audio_params *,
    struct audio_params *);
int auich_round_blocksize(void *, int);
int auich_halt_output(void *);
int auich_halt_input(void *);
int auich_getdev(void *, struct audio_device *);
int auich_set_port(void *, mixer_ctrl_t *);
int auich_get_port(void *, mixer_ctrl_t *);
int auich_query_devinfo(void *, mixer_devinfo_t *);
void *auich_allocm(void *, int, size_t, int, int);
void auich_freem(void *, void *, int);
size_t auich_round_buffersize(void *, int, size_t);
paddr_t auich_mappage(void *, void *, off_t, int);
int auich_get_props(void *);
int auich_trigger_output(void *, void *, void *, int, void (*)(void *),
    void *, struct audio_params *);
int auich_trigger_input(void *, void *, void *, int, void (*)(void *),
    void *, struct audio_params *);

void auich_powerhook(int, void *);

struct audio_hw_if auich_hw_if = {
	auich_open,
	auich_close,
	NULL,			/* drain */
	auich_query_encoding,
	auich_set_params,
	auich_round_blocksize,
	NULL,			/* commit_setting */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	auich_halt_output,
	auich_halt_input,
	NULL,			/* speaker_ctl */
	auich_getdev,
	NULL,			/* getfd */
	auich_set_port,
	auich_get_port,
	auich_query_devinfo,
	auich_allocm,
	auich_freem,
	auich_round_buffersize,
	auich_mappage,
	auich_get_props,
	auich_trigger_output,
	auich_trigger_input
};

int  auich_attach_codec(void *, struct ac97_codec_if *);
int  auich_read_codec(void *, u_int8_t, u_int16_t *);
int  auich_write_codec(void *, u_int8_t, u_int16_t);
void auich_reset_codec(void *);
enum ac97_host_flags auich_flags_codec(void *);
unsigned int auich_calibrate(struct auich_softc *);

int
auich_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	int i;

	for (i = sizeof(auich_devices)/sizeof(auich_devices[0]); i--;)
		if (PCI_VENDOR(pa->pa_id) == auich_devices[i].vendor &&
		    PCI_PRODUCT(pa->pa_id) == auich_devices[i].product)
			return 1;

	return 0;
}

void
auich_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct auich_softc *sc = (struct auich_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	bus_size_t mix_size, aud_size;
	pcireg_t csr;
	const char *intrstr;
	u_int32_t status;
	bus_size_t dmasz;
	int i, segs;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801DB_ACA ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801EB_ACA ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801FB_ACA ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801GB_ACA)) {
		/*
		 * Use native mode for ICH4/ICH5/ICH6/ICH7
		 */
		if (pci_mapreg_map(pa, AUICH_MMBAR, PCI_MAPREG_TYPE_MEM, 0,
		    &sc->iot_mix, &sc->mix_ioh, NULL, &mix_size, 0)) {
			csr = pci_conf_read(pa->pa_pc, pa->pa_tag, AUICH_CFG);
			pci_conf_write(pa->pa_pc, pa->pa_tag, AUICH_CFG,
			    csr | AUICH_CFG_IOSE);
			if (pci_mapreg_map(pa, AUICH_NAMBAR, PCI_MAPREG_TYPE_IO,
			    0, &sc->iot_mix, &sc->mix_ioh, NULL, &mix_size, 0)) {
				printf(": can't map codec mem/io space\n");
				return;
			}
		}

		if (pci_mapreg_map(pa, AUICH_MBBAR, PCI_MAPREG_TYPE_MEM, 0,
		    &sc->iot, &sc->aud_ioh, NULL, &aud_size, 0)) {
			csr = pci_conf_read(pa->pa_pc, pa->pa_tag, AUICH_CFG);
			pci_conf_write(pa->pa_pc, pa->pa_tag, AUICH_CFG,
			    csr | AUICH_CFG_IOSE);
			if (pci_mapreg_map(pa, AUICH_NABMBAR,
			    PCI_MAPREG_TYPE_IO, 0, &sc->iot,
			    &sc->aud_ioh, NULL, &aud_size, 0)) {
				printf(": can't map device mem/io space\n");
				bus_space_unmap(sc->iot_mix, sc->mix_ioh, mix_size);
				return;
			}
		}
	} else {
		if (pci_mapreg_map(pa, AUICH_NAMBAR, PCI_MAPREG_TYPE_IO,
		    0, &sc->iot_mix, &sc->mix_ioh, NULL, &mix_size, 0)) {
			printf(": can't map codec i/o space\n");
			return;
		}

		if (pci_mapreg_map(pa, AUICH_NABMBAR, PCI_MAPREG_TYPE_IO,
		    0, &sc->iot, &sc->aud_ioh, NULL, &aud_size, 0)) {
			printf(": can't map device i/o space\n");
			bus_space_unmap(sc->iot_mix, sc->mix_ioh, mix_size);
			return;
		}
	}
	sc->dmat = pa->pa_dmat;

	/* allocate dma memory */
	dmasz = AUICH_DMALIST_MAX * 3 * sizeof(struct auich_dma);
	segs = 1;
	if (bus_dmamem_alloc(sc->dmat, dmasz, PAGE_SIZE, 0, sc->dmalist_seg,
	    segs, &segs, BUS_DMA_NOWAIT)) {
		printf(": failed to alloc dmalist\n");
		return;
	}
	if (bus_dmamem_map(sc->dmat, sc->dmalist_seg, segs, dmasz,
	    &sc->dmalist_kva, BUS_DMA_NOWAIT)) {
		printf(": failed to map dmalist\n");
		bus_dmamem_free(sc->dmat, sc->dmalist_seg, segs);
		return;
	}
	if (bus_dmamap_create(sc->dmat, dmasz, segs, dmasz, 0, BUS_DMA_NOWAIT,
	    &sc->dmalist_map)) {
		printf(": failed to create dmalist map\n");
		bus_dmamem_unmap(sc->dmat, sc->dmalist_kva, dmasz);
		bus_dmamem_free(sc->dmat, sc->dmalist_seg, segs);
		return;
	}
	if (bus_dmamap_load_raw(sc->dmat, sc->dmalist_map, sc->dmalist_seg,
	    segs, dmasz, BUS_DMA_NOWAIT)) {
		printf(": failed to load dmalist map: %d segs %lu size\n",
		    segs, (u_long)dmasz);
		bus_dmamap_destroy(sc->dmat, sc->dmalist_map);
		bus_dmamem_unmap(sc->dmat, sc->dmalist_kva, dmasz);
		bus_dmamem_free(sc->dmat, sc->dmalist_seg, segs);
		return;
	}

	if (pci_intr_map(pa, &ih)) {
		bus_space_unmap(sc->iot, sc->aud_ioh, aud_size);
		bus_space_unmap(sc->iot_mix, sc->mix_ioh, mix_size);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, auich_intr,
				       sc, sc->sc_dev.dv_xname);
	if (!sc->sc_ih) {
		printf(": can't establish interrupt");
		if (intrstr)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(sc->iot, sc->aud_ioh, aud_size);
		bus_space_unmap(sc->iot_mix, sc->mix_ioh, mix_size);
		return;
	}

	for (i = sizeof(auich_devices)/sizeof(auich_devices[0]); i--;)
		if (PCI_PRODUCT(pa->pa_id) == auich_devices[i].product)
			break;

	snprintf(sc->sc_audev.name, sizeof sc->sc_audev.name, "%s AC97",
		 auich_devices[i].name);
	snprintf(sc->sc_audev.version, sizeof sc->sc_audev.version, "0x%02x",
		 PCI_REVISION(pa->pa_class));
	strlcpy(sc->sc_audev.config, sc->sc_dev.dv_xname,
		sizeof sc->sc_audev.config);

	printf(": %s, %s\n", intrstr, sc->sc_audev.name);

	/* SiS 7012 needs special handling */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SIS &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SIS_7012_ACA) {
		sc->sc_sts_reg = AUICH_PICB;
		sc->sc_sample_size = 1;
		/* un-mute output */
		bus_space_write_4(sc->iot, sc->aud_ioh, ICH_SIS_NV_CTL,
		    bus_space_read_4(sc->iot, sc->aud_ioh, ICH_SIS_NV_CTL) |
		    ICH_SIS_CTL_UNMUTE);
	} else {
		sc->sc_sts_reg = AUICH_STS;
		sc->sc_sample_size = 2;
	}

	sc->dmalist_pcmo = (struct auich_dmalist *)(sc->dmalist_kva +
	    (0 * sizeof(struct auich_dmalist) + AUICH_DMALIST_MAX));
	sc->dmalist_pcmo_pa = sc->dmalist_map->dm_segs[0].ds_addr +
	    (0 * sizeof(struct auich_dmalist) + AUICH_DMALIST_MAX);

	sc->dmalist_pcmi = (struct auich_dmalist *)(sc->dmalist_kva +
	    (1 * sizeof(struct auich_dmalist) + AUICH_DMALIST_MAX));
	sc->dmalist_pcmi_pa = sc->dmalist_map->dm_segs[0].ds_addr +
	    (1 * sizeof(struct auich_dmalist) + AUICH_DMALIST_MAX);

	sc->dmalist_mici = (struct auich_dmalist *)(sc->dmalist_kva +
	    (2 * sizeof(struct auich_dmalist) + AUICH_DMALIST_MAX));
	sc->dmalist_mici_pa = sc->dmalist_map->dm_segs[0].ds_addr +
	    (2 * sizeof(struct auich_dmalist) + AUICH_DMALIST_MAX);

	DPRINTF(AUICH_DEBUG_DMA, ("auich_attach: lists %p %p %p\n",
	    sc->dmalist_pcmo, sc->dmalist_pcmi, sc->dmalist_mici));

	/* Reset codec and AC'97 */
	auich_reset_codec(sc);
	status = bus_space_read_4(sc->iot, sc->aud_ioh, AUICH_GSTS);
	if (!(status & AUICH_PCR)) {	/* reset failure */
		if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
		    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801DB_ACA ||
		     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801EB_ACA ||
		     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801FB_ACA ||
		     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801GB_ACA)) {
			/* MSI 845G Max never return AUICH_PCR */
			sc->sc_ignore_codecready = 1;
		} else {
			printf("%s: reset failed!\n", sc->sc_dev.dv_xname);
			return;
		}
	}

	sc->host_if.arg = sc;
	sc->host_if.attach = auich_attach_codec;
	sc->host_if.read = auich_read_codec;
	sc->host_if.write = auich_write_codec;
	sc->host_if.reset = auich_reset_codec;
	sc->host_if.flags = auich_flags_codec;
	if (sc->sc_dev.dv_cfdata->cf_flags & 0x0001)
		sc->flags = AC97_HOST_SWAPPED_CHANNELS;

	if (ac97_attach(&sc->host_if) != 0) {
		pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
		bus_space_unmap(sc->iot, sc->aud_ioh, aud_size);
		bus_space_unmap(sc->iot_mix, sc->mix_ioh, mix_size);
		return;
	}

	audio_attach_mi(&auich_hw_if, sc, &sc->sc_dev);

	/* Watch for power changes */
	sc->suspend = PWR_RESUME;
	sc->powerhook = powerhook_establish(auich_powerhook, sc);

	sc->sc_ac97rate = -1;
}

int
auich_read_codec(v, reg, val)
	void *v;
	u_int8_t reg;
	u_int16_t *val;
{
	struct auich_softc *sc = v;
	int i;

	/* wait for an access semaphore */
	for (i = AUICH_SEMATIMO; i-- &&
	    bus_space_read_1(sc->iot, sc->aud_ioh, AUICH_CAS) & 1; DELAY(1));

	if (!sc->sc_ignore_codecready && i < 0) {
		DPRINTF(AUICH_DEBUG_CODECIO,
		    ("%s: read_codec timeout\n", sc->sc_dev.dv_xname));
		return (-1);
	}

	*val = bus_space_read_2(sc->iot_mix, sc->mix_ioh, reg);
	DPRINTF(AUICH_DEBUG_CODECIO, ("%s: read_codec(%x, %x)\n",
	    sc->sc_dev.dv_xname, reg, *val));
	return (0);
}

int
auich_write_codec(v, reg, val)
	void *v;
	u_int8_t reg;
	u_int16_t val;
{
	struct auich_softc *sc = v;
	int i;

	/* wait for an access semaphore */
	for (i = AUICH_SEMATIMO; i-- &&
	    bus_space_read_1(sc->iot, sc->aud_ioh, AUICH_CAS) & 1; DELAY(1));

	if (sc->sc_ignore_codecready || i >= 0) {
		DPRINTF(AUICH_DEBUG_CODECIO, ("%s: write_codec(%x, %x)\n",
		    sc->sc_dev.dv_xname, reg, val));
		bus_space_write_2(sc->iot_mix, sc->mix_ioh, reg, val);
		return (0);
	} else {
		DPRINTF(AUICH_DEBUG_CODECIO,
		    ("%s: write_codec timeout\n", sc->sc_dev.dv_xname));
		return (-1);
	}
}

int
auich_attach_codec(v, cif)
	void *v;
	struct ac97_codec_if *cif;
{
	struct auich_softc *sc = v;

	sc->codec_if = cif;
	return 0;
}

void
auich_reset_codec(v)
	void *v;
{
	struct auich_softc *sc = v;
	u_int32_t control;
	int i;

	control = bus_space_read_4(sc->iot, sc->aud_ioh, AUICH_GCTRL);
	control &= ~(AUICH_ACLSO | AUICH_PCM246_MASK);
	control |= (control & AUICH_CRESET) ? AUICH_WRESET : AUICH_CRESET;
	bus_space_write_4(sc->iot, sc->aud_ioh, AUICH_GCTRL, control);

	for (i = AUICH_RESETIMO; i-- &&
	    !(bus_space_read_4(sc->iot, sc->aud_ioh, AUICH_GSTS) & AUICH_PCR);
	    DELAY(1));

	if (i < 0)
		DPRINTF(AUICH_DEBUG_CODECIO,
		    ("%s: reset_codec timeout\n", sc->sc_dev.dv_xname));
}

enum ac97_host_flags
auich_flags_codec(void *v)
{
	struct auich_softc *sc = v;

	return (sc->flags);
}

int
auich_open(v, flags)
	void *v;
	int flags;
{
	struct auich_softc *sc = v;

	if (sc->sc_ac97rate == -1)
		sc->sc_ac97rate = auich_calibrate(sc);
	return 0;
}

void
auich_close(v)
	void *v;
{
}

int
auich_query_encoding(v, aep)
	void *v;
	struct audio_encoding *aep;
{
	switch (aep->index) {
	case 0:
		strlcpy(aep->name, AudioEulinear, sizeof aep->name);
		aep->encoding = AUDIO_ENCODING_ULINEAR;
		aep->precision = 8;
		aep->flags = 0;
		return (0);
	case 1:
		strlcpy(aep->name, AudioEmulaw, sizeof aep->name);
		aep->encoding = AUDIO_ENCODING_ULAW;
		aep->precision = 8;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 2:
		strlcpy(aep->name, AudioEalaw, sizeof aep->name);
		aep->encoding = AUDIO_ENCODING_ALAW;
		aep->precision = 8;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 3:
		strlcpy(aep->name, AudioEslinear, sizeof aep->name);
		aep->encoding = AUDIO_ENCODING_SLINEAR;
		aep->precision = 8;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 4:
		strlcpy(aep->name, AudioEslinear_le, sizeof aep->name);
		aep->encoding = AUDIO_ENCODING_SLINEAR_LE;
		aep->precision = 16;
		aep->flags = 0;
		return (0);
	case 5:
		strlcpy(aep->name, AudioEulinear_le, sizeof aep->name);
		aep->encoding = AUDIO_ENCODING_ULINEAR_LE;
		aep->precision = 16;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 6:
		strlcpy(aep->name, AudioEslinear_be, sizeof aep->name);
		aep->encoding = AUDIO_ENCODING_SLINEAR_BE;
		aep->precision = 16;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 7:
		strlcpy(aep->name, AudioEulinear_be, sizeof aep->name);
		aep->encoding = AUDIO_ENCODING_ULINEAR_BE;
		aep->precision = 16;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	default:
		return (EINVAL);
	}
}

int
auich_set_params(v, setmode, usemode, play, rec)
	void *v;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct auich_softc *sc = v;
	int error;
	u_int orate;
	u_int adj_rate;

	if (setmode & AUMODE_PLAY) {
		play->factor = 1;
		play->sw_code = NULL;
		switch(play->encoding) {
		case AUDIO_ENCODING_ULAW:
			switch (play->channels) {
			case 1:
				play->factor = 4;
				play->sw_code = mulaw_to_slinear16_mts;
				break;
			case 2:
				play->factor = 2;
				play->sw_code = mulaw_to_slinear16;
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			switch (play->precision) {
			case 8:
				switch (play->channels) {
				case 1:
					play->factor = 4;
					play->sw_code = linear8_to_linear16_mts;
					break;
				case 2:
					play->factor = 2;
					play->sw_code = linear8_to_linear16;
					break;
				default:
					return (EINVAL);
				}
				break;
			case 16:
				switch (play->channels) {
				case 1:
					play->factor = 2;
					play->sw_code = noswap_bytes_mts;
					break;
				case 2:
					break;
				default:
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			switch (play->precision) {
			case 8:
				switch (play->channels) {
				case 1:
					play->factor = 4;
					play->sw_code = ulinear8_to_linear16_mts;
					break;
				case 2:
					play->factor = 2;
					play->sw_code = ulinear8_to_linear16;
					break;
				default:
					return (EINVAL);
				}
				break;
			case 16:
				switch (play->channels) {
				case 1:
					play->factor = 2;
					play->sw_code = change_sign16_mts;
					break;
				case 2:
					play->sw_code = change_sign16;
					break;
				default:
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ALAW:
			switch (play->channels) {
			case 1:
				play->factor = 4;
				play->sw_code = alaw_to_slinear16_mts;
				break;
			case 2:
				play->factor = 2;
				play->sw_code = alaw_to_slinear16;
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			switch (play->precision) {
			case 8:
				switch (play->channels) {
				case 1:
					play->factor = 4;
					play->sw_code = linear8_to_linear16_mts;
					break;
				case 2:
					play->factor = 2;
					play->sw_code = linear8_to_linear16;
					break;
				default:
					return (EINVAL);
				}
				break;
			case 16:
				switch (play->channels) {
				case 1:
					play->factor = 2;
					play->sw_code = swap_bytes_mts;
					break;
				case 2:
					play->sw_code = swap_bytes;
					break;
				default:
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			switch (play->precision) {
			case 8:
				switch (play->channels) {
				case 1:
					play->factor = 4;
					play->sw_code = ulinear8_to_linear16_mts;
					break;
				case 2:
					play->factor = 2;
					play->sw_code = ulinear8_to_linear16;
					break;
				default:
					return (EINVAL);
				}
				break;
			case 16:
				switch (play->channels) {
				case 1:
					play->factor = 2;
					play->sw_code = change_sign16_swap_bytes_mts;
					break;
				case 2:
					play->sw_code = change_sign16_swap_bytes;
					break;
				default:
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}
			break;
		default:
			return (EINVAL);
		}

		orate = adj_rate = play->sample_rate;
		if (sc->sc_ac97rate != 0)
			adj_rate = orate * AUICH_FIXED_RATE / sc->sc_ac97rate;
		play->sample_rate = adj_rate;
		error = ac97_set_rate(sc->codec_if, play, AUMODE_PLAY);
		if (play->sample_rate == adj_rate)
			play->sample_rate = orate;
		if (error)
			return (error);
	}

	if (setmode & AUMODE_RECORD) {
		rec->factor = 1;
		rec->sw_code = 0;
		switch(rec->encoding) {
		case AUDIO_ENCODING_ULAW:
			rec->sw_code = slinear16_to_mulaw_le;
			rec->factor = 2;
			break;
		case AUDIO_ENCODING_ALAW:
			rec->sw_code = slinear16_to_alaw_le;
			rec->factor = 2;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			switch (rec->precision) {
			case 8:
				rec->sw_code = linear16_to_linear8_le;
				rec->factor = 2;
				break;
			case 16:
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			switch (rec->precision) {
			case 8:
				rec->sw_code = linear16_to_ulinear8_le;
				rec->factor = 2;
				break;
			case 16:
				rec->sw_code = change_sign16_le;
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			switch (rec->precision) {
			case 8:
				rec->sw_code = linear16_to_linear8_le;
				rec->factor = 2;
				break;
			case 16:
				rec->sw_code = swap_bytes;
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			switch (rec->precision) {
			case 8:
				rec->sw_code = linear16_to_ulinear8_le;
				rec->factor = 2;
				break;
			case 16:
				rec->sw_code = change_sign16_swap_bytes_le;
				break;
			default:
				return (EINVAL);
			}
			break;
		default:
			return (EINVAL);
		}

		orate = rec->sample_rate;
		if (sc->sc_ac97rate != 0)
			rec->sample_rate = orate * AUICH_FIXED_RATE /
			    sc->sc_ac97rate;
		error = ac97_set_rate(sc->codec_if, rec, AUMODE_RECORD);
		rec->sample_rate = orate;
		if (error)
			return (error);
	}

	return (0);
}

int
auich_round_blocksize(v, blk)
	void *v;
	int blk;
{
	return (blk + 0x3f) & ~0x3f;
}

int
auich_halt_output(v)
	void *v;
{
	struct auich_softc *sc = v;

	DPRINTF(AUICH_DEBUG_DMA, ("%s: halt_output\n", sc->sc_dev.dv_xname));

	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_PCMO + AUICH_CTRL, AUICH_RR);

	return 0;
}

int
auich_halt_input(v)
	void *v;
{
	struct auich_softc *sc = v;

	DPRINTF(AUICH_DEBUG_DMA,
	    ("%s: halt_input\n", sc->sc_dev.dv_xname));

	/* XXX halt both unless known otherwise */

	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_CTRL, AUICH_RR);
	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_MICI + AUICH_CTRL, AUICH_RR);

	return 0;
}

int
auich_getdev(v, adp)
	void *v;
	struct audio_device *adp;
{
	struct auich_softc *sc = v;
	*adp = sc->sc_audev;
	return 0;
}

int
auich_set_port(v, cp)
	void *v;
	mixer_ctrl_t *cp;
{
	struct auich_softc *sc = v;
	return sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp);
}

int
auich_get_port(v, cp)
	void *v;
	mixer_ctrl_t *cp;
{
	struct auich_softc *sc = v;
	return sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp);
}

int
auich_query_devinfo(v, dp)
	void *v;
	mixer_devinfo_t *dp;
{
	struct auich_softc *sc = v;
	return sc->codec_if->vtbl->query_devinfo(sc->codec_if, dp);
}

void *
auich_allocm(v, direction, size, pool, flags)
	void *v;
	int direction;
	size_t size;
	int pool, flags;
{
	struct auich_softc *sc = v;
	struct auich_dma *p;
	int error;

	if (size > AUICH_DMALIST_MAX * AUICH_DMASEG_MAX)
		return NULL;

	p = malloc(sizeof(*p), pool, flags | M_ZERO);
	if (!p)
		return NULL;

	p->size = size;
	if ((error = bus_dmamem_alloc(sc->dmat, p->size, NBPG, 0, p->segs,
	    1, &p->nsegs, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate dma, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		free(p, pool);
		return NULL;
	}

	if ((error = bus_dmamem_map(sc->dmat, p->segs, p->nsegs, p->size,
	    &p->addr, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map dma, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		bus_dmamem_free(sc->dmat, p->segs, p->nsegs);
		free(p, pool);
		return NULL;
	}

	if ((error = bus_dmamap_create(sc->dmat, p->size, 1,
	    p->size, 0, BUS_DMA_NOWAIT, &p->map)) != 0) {
		printf("%s: unable to create dma map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		bus_dmamem_unmap(sc->dmat, p->addr, size);
		bus_dmamem_free(sc->dmat, p->segs, p->nsegs);
		free(p, pool);
		return NULL;
	}

	if ((error = bus_dmamap_load(sc->dmat, p->map, p->addr, p->size,
	    NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load dma map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		bus_dmamap_destroy(sc->dmat, p->map);
		bus_dmamem_unmap(sc->dmat, p->addr, size);
		bus_dmamem_free(sc->dmat, p->segs, p->nsegs);
		free(p, pool);
		return NULL;
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;

	return p->addr;
}

void
auich_freem(v, ptr, pool)
	void *v;
	void *ptr;
	int pool;
{
	struct auich_softc *sc = v;
	struct auich_dma *p;

	for (p = sc->sc_dmas; p->addr != ptr; p = p->next)
		if (p->next == NULL) {
			printf("auich_freem: trying to free not allocated memory");
			return;
		}

	bus_dmamap_unload(sc->dmat, p->map);
	bus_dmamap_destroy(sc->dmat, p->map);
	bus_dmamem_unmap(sc->dmat, p->addr, p->size);
	bus_dmamem_free(sc->dmat, p->segs, p->nsegs);
	free(p, pool);
}

size_t
auich_round_buffersize(v, direction, size)
	void *v;
	int direction;
	size_t size;
{
	if (size > AUICH_DMALIST_MAX * AUICH_DMASEG_MAX)
		size = AUICH_DMALIST_MAX * AUICH_DMASEG_MAX;

	return size;
}

paddr_t
auich_mappage(v, mem, off, prot)
	void *v;
	void *mem;
	off_t off;
	int prot;
{
	struct auich_softc *sc = v;
	struct auich_dma *p;

	if (off < 0)
		return -1;

	for (p = sc->sc_dmas; p && p->addr != mem; p = p->next);
	if (!p)
		return -1;

	return bus_dmamem_mmap(sc->dmat, p->segs, p->nsegs,
	    off, prot, BUS_DMA_WAITOK);
}

int
auich_get_props(v)
	void *v;
{
	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

int
auich_intr(v)
	void *v;
{
	struct auich_softc *sc = v;
	int ret = 0, sts, gsts, i;

	gsts = bus_space_read_2(sc->iot, sc->aud_ioh, AUICH_GSTS);
	DPRINTF(AUICH_DEBUG_DMA, ("auich_intr: gsts=%b\n", gsts, AUICH_GSTS_BITS));

	if (gsts & AUICH_POINT) {
		sts = bus_space_read_2(sc->iot, sc->aud_ioh,
		    AUICH_PCMO + sc->sc_sts_reg);
		DPRINTF(AUICH_DEBUG_DMA,
		    ("auich_intr: osts=%b\n", sts, AUICH_ISTS_BITS));

#ifdef AUICH_DEBUG
		if (sts & AUICH_FIFOE) {
			printf("%s: fifo underrun # %u\n",
			    sc->sc_dev.dv_xname, ++sc->pcmo_fifoe);
		}
#endif
		i = bus_space_read_1(sc->iot, sc->aud_ioh, AUICH_PCMO + AUICH_CIV);
		if (sts & (AUICH_LVBCI | AUICH_CELV)) {
			struct auich_dmalist *q, *qe;

			q = sc->dmap_pcmo;
			qe = &sc->dmalist_pcmo[i];

			while (q != qe) {

				q->base = sc->pcmo_p;
				q->len = (sc->pcmo_blksize /
				    sc->sc_sample_size) | AUICH_DMAF_IOC;
				DPRINTF(AUICH_DEBUG_DMA,
				    ("auich_intr: %p, %p = %x @ %p\n",
				    qe, q, sc->pcmo_blksize /
				    sc->sc_sample_size, sc->pcmo_p));

				sc->pcmo_p += sc->pcmo_blksize;
				if (sc->pcmo_p >= sc->pcmo_end)
					sc->pcmo_p = sc->pcmo_start;

				if (++q == &sc->dmalist_pcmo[AUICH_DMALIST_MAX])
					q = sc->dmalist_pcmo;
			}

			sc->dmap_pcmo = q;
			bus_space_write_1(sc->iot, sc->aud_ioh,
			    AUICH_PCMO + AUICH_LVI,
			    (sc->dmap_pcmo - sc->dmalist_pcmo - 1) &
			    AUICH_LVI_MASK);
		}

		if (sts & AUICH_BCIS && sc->sc_pintr)
			sc->sc_pintr(sc->sc_parg);

		/* int ack */
		bus_space_write_2(sc->iot, sc->aud_ioh,
		    AUICH_PCMO + sc->sc_sts_reg, sts &
		    (AUICH_LVBCI | AUICH_CELV | AUICH_BCIS | AUICH_FIFOE));
		bus_space_write_2(sc->iot, sc->aud_ioh, AUICH_GSTS, AUICH_POINT);
		ret++;
	}

	if (gsts & AUICH_PIINT) {
		sts = bus_space_read_2(sc->iot, sc->aud_ioh,
		    AUICH_PCMI + sc->sc_sts_reg);
		DPRINTF(AUICH_DEBUG_DMA,
		    ("auich_intr: ists=%b\n", sts, AUICH_ISTS_BITS));

#ifdef AUICH_DEBUG
		if (sts & AUICH_FIFOE) {
			printf("%s: in fifo overrun # %u\n",
			    sc->sc_dev.dv_xname, ++sc->pcmi_fifoe);
		}
#endif
		i = bus_space_read_1(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_CIV);
		if (sts & (AUICH_LVBCI | AUICH_CELV)) {
			struct auich_dmalist *q, *qe;

			q = sc->dmap_pcmi;
			qe = &sc->dmalist_pcmi[i];

			while (q != qe) {

				q->base = sc->pcmi_p;
				q->len = (sc->pcmi_blksize /
				    sc->sc_sample_size) | AUICH_DMAF_IOC;
				DPRINTF(AUICH_DEBUG_DMA,
				    ("auich_intr: %p, %p = %x @ %p\n",
				    qe, q, sc->pcmi_blksize /
				    sc->sc_sample_size, sc->pcmi_p));

				sc->pcmi_p += sc->pcmi_blksize;
				if (sc->pcmi_p >= sc->pcmi_end)
					sc->pcmi_p = sc->pcmi_start;

				if (++q == &sc->dmalist_pcmi[AUICH_DMALIST_MAX])
					q = sc->dmalist_pcmi;
			}

			sc->dmap_pcmi = q;
			bus_space_write_1(sc->iot, sc->aud_ioh,
			    AUICH_PCMI + AUICH_LVI,
			    (sc->dmap_pcmi - sc->dmalist_pcmi - 1) &
			    AUICH_LVI_MASK);
		}

		if (sts & AUICH_BCIS && sc->sc_rintr)
			sc->sc_rintr(sc->sc_rarg);

		/* int ack */
		bus_space_write_2(sc->iot, sc->aud_ioh,
		    AUICH_PCMI + sc->sc_sts_reg, sts &
		    (AUICH_LVBCI | AUICH_CELV | AUICH_BCIS | AUICH_FIFOE));
		bus_space_write_2(sc->iot, sc->aud_ioh, AUICH_GSTS, AUICH_PIINT);
		ret++;
	}

	if (gsts & AUICH_MIINT) {
		sts = bus_space_read_2(sc->iot, sc->aud_ioh,
		    AUICH_MICI + sc->sc_sts_reg);
		DPRINTF(AUICH_DEBUG_DMA,
		    ("auich_intr: ists=%b\n", sts, AUICH_ISTS_BITS));
#ifdef AUICH_DEBUG
		if (sts & AUICH_FIFOE)
			printf("%s: mic fifo overrun\n", sc->sc_dev.dv_xname);
#endif

		/* TODO mic input dma */

		bus_space_write_2(sc->iot, sc->aud_ioh, AUICH_GSTS, AUICH_MIINT);
	}

	return ret;
}

int
auich_trigger_output(v, start, end, blksize, intr, arg, param)
	void *v;
	void *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct auich_softc *sc = v;
	struct auich_dmalist *q;
	struct auich_dma *p;

	DPRINTF(AUICH_DEBUG_DMA,
	    ("auich_trigger_output(%x, %x, %d, %p, %p, %p)\n",
	    start, end, blksize, intr, arg, param));

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next);
	if (!p)
		return -1;

	sc->sc_pintr = intr;
	sc->sc_parg = arg;

	/*
	 * The logic behind this is:
	 * setup one buffer to play, then LVI dump out the rest
	 * to the scatter-gather chain.
	 */
	sc->pcmo_start = p->segs->ds_addr;
	sc->pcmo_p = sc->pcmo_start + blksize;
	sc->pcmo_end = sc->pcmo_start + ((char *)end - (char *)start);
	sc->pcmo_blksize = blksize;

	q = sc->dmap_pcmo = sc->dmalist_pcmo;
	q->base = sc->pcmo_start;
	q->len = (blksize / sc->sc_sample_size) | AUICH_DMAF_IOC;
	if (++q == &sc->dmalist_pcmo[AUICH_DMALIST_MAX])
		q = sc->dmalist_pcmo;
	sc->dmap_pcmo = q;

	bus_space_write_4(sc->iot, sc->aud_ioh, AUICH_PCMO + AUICH_BDBAR,
	    sc->dmalist_pcmo_pa);
	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_PCMO + AUICH_CTRL,
	    AUICH_IOCE | AUICH_FEIE | AUICH_LVBIE | AUICH_RPBM);
	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_PCMO + AUICH_LVI,
	    (sc->dmap_pcmo - 1 - sc->dmalist_pcmo) & AUICH_LVI_MASK);

	return 0;
}

int
auich_trigger_input(v, start, end, blksize, intr, arg, param)
	void *v;
	void *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct auich_softc *sc = v;
	struct auich_dmalist *q;
	struct auich_dma *p;

	DPRINTF(AUICH_DEBUG_DMA,
	    ("auich_trigger_input(%x, %x, %d, %p, %p, %p)\n",
	    start, end, blksize, intr, arg, param));

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next);
	if (!p)
		return -1;

	sc->sc_rintr = intr;
	sc->sc_rarg = arg;

	/*
	 * The logic behind this is:
	 * setup one buffer to play, then LVI dump out the rest
	 * to the scatter-gather chain.
	 */
	sc->pcmi_start = p->segs->ds_addr;
	sc->pcmi_p = sc->pcmi_start + blksize;
	sc->pcmi_end = sc->pcmi_start + ((char *)end - (char *)start);
	sc->pcmi_blksize = blksize;

	q = sc->dmap_pcmi = sc->dmalist_pcmi;
	q->base = sc->pcmi_start;
	q->len = (blksize / sc->sc_sample_size) | AUICH_DMAF_IOC;
	if (++q == &sc->dmalist_pcmi[AUICH_DMALIST_MAX])
		q = sc->dmalist_pcmi;
	sc->dmap_pcmi = q;

	bus_space_write_4(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_BDBAR,
	    sc->dmalist_pcmi_pa);
	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_CTRL,
	    AUICH_IOCE | AUICH_FEIE | AUICH_LVBIE | AUICH_RPBM);
	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_LVI,
	    (sc->dmap_pcmi - 1 - sc->dmalist_pcmi) & AUICH_LVI_MASK);

	return 0;
}

void
auich_powerhook(why, self)
	int why;
	void *self;
{
	struct auich_softc *sc = (struct auich_softc *)self;

	if (why != PWR_RESUME) {
		/* Power down */
		DPRINTF(1, ("auich: power down\n"));
		sc->suspend = why;
		auich_read_codec(sc, AC97_REG_EXT_AUDIO_CTRL, &sc->ext_ctrl);

	} else {
		/* Wake up */
		DPRINTF(1, ("auich: power resume\n"));
		if (sc->suspend == PWR_RESUME) {
			printf("%s: resume without suspend?\n",
			    sc->sc_dev.dv_xname);
			sc->suspend = why;
			return;
		}
		sc->suspend = why;
		auich_reset_codec(sc);
		DELAY(1000);
		(sc->codec_if->vtbl->restore_ports)(sc->codec_if);
		auich_write_codec(sc, AC97_REG_EXT_AUDIO_CTRL, sc->ext_ctrl);
	}
}



/* -------------------------------------------------------------------- */
/* Calibrate card (some boards are overclocked and need scaling) */

unsigned int
auich_calibrate(struct auich_softc *sc)
{
	struct timeval t1, t2;
	u_int8_t ociv, nciv;
	u_int32_t wait_us, actual_48k_rate, bytes, ac97rate;
	void *temp_buffer;
	struct auich_dma *p;
	int i;

	ac97rate = AUICH_FIXED_RATE;
	/*
	 * Grab audio from input for fixed interval and compare how
	 * much we actually get with what we expect.  Interval needs
	 * to be sufficiently short that no interrupts are
	 * generated.
	 */

	/* Setup a buffer */
	bytes = 16000;
	temp_buffer = auich_allocm(sc, AUMODE_RECORD, bytes, M_DEVBUF,
	    M_NOWAIT);
	if (temp_buffer == NULL)
		return (ac97rate);
	for (p = sc->sc_dmas; p && p->addr != temp_buffer; p = p->next)
		;
	if (p == NULL) {
		printf("auich_calibrate: bad address %p\n", temp_buffer);
		return (ac97rate);
	}

	for (i = 0; i < AUICH_DMALIST_MAX; i++) {
		sc->dmalist_pcmi[i].base = p->map->dm_segs[0].ds_addr;
		sc->dmalist_pcmi[i].len = bytes / sc->sc_sample_size;
	}

	/*
	 * our data format is stereo, 16 bit so each sample is 4 bytes.
	 * assuming we get 48000 samples per second, we get 192000 bytes/sec.
	 * we're going to start recording with interrupts disabled and measure
	 * the time taken for one block to complete.  we know the block size,
	 * we know the time in microseconds, we calculate the sample rate:
	 *
	 * actual_rate [bps] = bytes / (time [s] * 4)
	 * actual_rate [bps] = (bytes * 1000000) / (time [us] * 4)
	 * actual_rate [Hz] = (bytes * 250000) / time [us]
	 */

	/* prepare */
	ociv = bus_space_read_1(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_CIV);
	nciv = ociv;
	bus_space_write_4(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_BDBAR,
	    sc->dmalist_pcmi_pa);
	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_LVI,
			  (0 - 1) & AUICH_LVI_MASK);

	/* start */
	microuptime(&t1);
	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_CTRL,
	    AUICH_RPBM);

	/* wait */
	while (nciv == ociv) {
		microuptime(&t2);
		if (t2.tv_sec - t1.tv_sec > 1)
			break;
		nciv = bus_space_read_1(sc->iot, sc->aud_ioh,
					AUICH_PCMI + AUICH_CIV);
	}
	microuptime(&t2);

	/* reset */
	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_PCMI + AUICH_CTRL, AUICH_RR);
	bus_space_write_1(sc->iot, sc->aud_ioh, AUICH_MICI + AUICH_CTRL, AUICH_RR);
	DELAY(100);

	/* turn time delta into us */
	wait_us = ((t2.tv_sec - t1.tv_sec) * 1000000) + t2.tv_usec - t1.tv_usec;

#if 0
	auich_freem(sc, temp_buffer, M_DEVBUF);
#endif

	if (nciv == ociv) {
		printf("%s: ac97 link rate calibration timed out after %d us\n",
		       sc->sc_dev.dv_xname, wait_us);
		return (ac97rate);
	}

	actual_48k_rate = (bytes * 250000) / wait_us;

	if (actual_48k_rate <= 48500)
		ac97rate = AUICH_FIXED_RATE;
	else
		ac97rate = actual_48k_rate;

	printf("%s: measured ac97 link rate at %d Hz",
	       sc->sc_dev.dv_xname, actual_48k_rate);
	if (ac97rate != actual_48k_rate)
		printf(", will use %d Hz", ac97rate);
	printf("\n");

	return (ac97rate);
}
