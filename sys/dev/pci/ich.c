/*	$OpenBSD: ich.c,v 1.4 2000/11/20 17:48:58 mickey Exp $	*/

/*
 * Copyright (c) 2000 Michael Shalayeff
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
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* #define	ICH_DEBUG */
/*
 * AC'97 audio found on Intel 810/820/440MX chipsets.
 *	http://developer.intel.com/design/chipsets/datashts/290655.htm
 *	http://developer.intel.com/design/chipsets/manuals/298028.htm
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
#define	ICH_NAMBAR	0x10
/* 12.1.11 NABMBAR - native audio bus mastering base address register */
#define	ICH_NABMBAR	0x14

/* table 12-3. native audio bus master control registers */
#define	ICH_BDBAR	0x00	/* 8-byte aligned address */
#define	ICH_CIV		0x04	/* 5 bits current index value */
#define	ICH_LVI		0x05	/* 5 bits last valid index value */
#define		ICH_LVI_MASK	0x1f
#define	ICH_STS		0x06	/* 16 bits status */
#define		ICH_FIFOE	0x10	/* fifo error */
#define		ICH_BCIS	0x08	/* r- buf cmplt int sts; wr ack */
#define		ICH_LVBCI	0x04	/* r- last valid bci, wr ack */
#define		ICH_CELV	0x02	/* current equals last valid */
#define		ICH_DCH		0x01	/* dma halted */
#define		ICH_ISTS_BITS	"\020\01dch\02celv\03lvbci\04bcis\05fifoe"
#define	ICH_PICB	0x08	/* 16 bits */
#define	ICH_PIV		0x0a	/* 5 bits prefetched index value */
#define	ICH_CTRL	0x0b	/* control */
#define		ICH_IOCE	0x10	/* int on completion enable */
#define		ICH_FEIE	0x08	/* fifo error int enable */
#define		ICH_LVBIE	0x04	/* last valid buf int enable */
#define		ICH_RR		0x02	/* 1 - reset regs */
#define		ICH_RPBM	0x01	/* 1 - run, 0 - pause */

#define	ICH_PCMI	0x00
#define	ICH_PCMO	0x10
#define	ICH_MICI	0x20

#define	ICH_GCTRL	0x2c
#define		ICH_SRIE	0x20	/* int when 2ndary codec resume */
#define		ICH_PRIE	0x10	/* int when primary codec resume */
#define		ICH_ACLSO	0x08	/* aclink shut off */
#define		ICH_WRESET	0x04	/* warm reset */
#define		ICH_CRESET	0x02	/* cold reset */
#define		ICH_GIE		0x01	/* gpi int enable */
#define	ICH_GSTS	0x30
#define		ICH_MD3		0x20000	/* pwr-dn semaphore for modem */
#define		ICH_AD3		0x10000	/* pwr-dn semaphore for audio */
#define		ICH_RCS		0x08000	/* read completion status */
#define		ICH_B3S12	0x04000	/* bit 3 of slot 12 */
#define		ICH_B2S12	0x02000	/* bit 2 of slot 12 */
#define		ICH_B1S12	0x01000	/* bit 1 of slot 12 */
#define		ICH_SRI		0x00800	/* secondary resume int */
#define		ICH_PRI		0x00400	/* primary resume int */
#define		ICH_SCR		0x00200	/* secondary codec ready */
#define		ICH_PCR		0x00100	/* primary codec ready */
#define		ICH_MINT	0x00080	/* mic in int */
#define		ICH_POINT	0x00040	/* pcm out int */
#define		ICH_PIINT	0x00020	/* pcm in int */
#define		ICH_MOINT	0x00004	/* modem out int */
#define		ICH_MIINT	0x00002	/* modem in int */
#define		ICH_GSCI	0x00001	/* gpi status change */
#define		ICH_GSTS_BITS	"\020\01gsci\02miict\03moint\06piint\07point\010mint\011pcr\012scr\013pri\014sri\015b1s12\016b2s12\017b3s12\020rcs\021ad3\022md3"
#define	ICH_CAS		0x34	/* 1/8 bit */
#define	ICH_SEMATIMO	1000	/* us */

#define	ICH_MIXER_RESET		0x00
#define	ICH_MIXER_MUTE		0x02
#define	ICH_MIXER_HDFMUTE	0x04
#define	ICH_MIXER_MONOMUTE	0x06
#define	ICH_MIXER_TONE		0x08
#define	ICH_MIXER_BEEPMUTE	0x0a
#define	ICH_MIXER_PHONEMUTE	0x0c
#define	ICH_MIXER_MICMUTE	0x0e
#define	ICH_MIXER_LINEMUTE	0x10
#define	ICH_MIXER_CDMUTE	0x12
#define	ICH_MIXER_VDMUTE	0x14
#define	ICH_MIXER_AUXMUTE	0x16
#define	ICH_MIXER_PCMMUTE	0x18
#define	ICH_MIXER_RECSEL	0x1a
#define	ICH_MIXER_RECGAIN	0x1c
#define	ICH_MIXER_RECGAINMIC	0x1e
#define	ICH_MIXER_GP		0x20
#define	ICH_MIXER_3DCTRL	0x22
#define	ICH_MIXER_RESERVED	0x24
#define	ICH_PM			0x26
#define		ICH_PM_PCMI	0x100
#define		ICH_PM_PCMO	0x200
#define		ICH_PM_MICI	0x400
#define	ICH_EXTAUDIO		0x28
#define	ICH_EXTAUCTRL		0x2a
#define	ICH_PCMRATE		0x2c
#define	ICH_PCM3dRATE		0x2e
#define	ICH_PCMLFERATE		0x30
#define	ICH_PCMLRRATE		0x32
#define	ICH_MICADCRATE		0x34
#define	ICH_CLFEMUTE		0x36
#define	ICH_LR3DMUTE		0x38

/*
 * according to the dev/audiovar.h AU_RING_SIZE is 2^16, what fits
 * in our limits perfectly, i.e. setting it to higher value
 * in your kernel config would improve perfomance, still 2^21 is the max
 */
#define	ICH_DMALIST_MAX	32
#define	ICH_DMASEG_MAX	(65536*2)	/* 64k samples, 2x16 bit samples */
struct ich_dmalist {
	u_int32_t	base;
	u_int32_t	len;
#define	ICH_DMAF_IOC	0x80000000	/* 1-int on complete */
#define	ICH_DMAF_BUP	0x40000000	/* 0-retrans last, 1-transmit 0 */
};

struct ich_dma {
	bus_dmamap_t map;
	caddr_t addr;
	bus_dma_segment_t segs[ICH_DMALIST_MAX];
	int nsegs;
	size_t size;
	struct ich_dma *next;
};

struct ich_softc {
	struct device sc_dev;
	void *sc_ih;

	audio_device_t sc_audev;

	bus_space_tag_t iot;
	bus_space_handle_t mix_ioh;
	bus_space_handle_t aud_ioh;
	bus_dma_tag_t dmat;

	struct ac97_codec_if *codec_if;
	struct ac97_host_if host_if;

	/* dma scatter-gather buffer lists, aligned to 8 bytes */
	struct ich_dmalist *dmalist_pcmo, *dmap_pcmo,
	    dmasto_pcmo[ICH_DMALIST_MAX+1];
	struct ich_dmalist *dmalist_pcmi, *dmap_pcmi,
	    dmasto_pcmi[ICH_DMALIST_MAX+1];;
	struct ich_dmalist *dmalist_mici, *dmap_mici,
	    dmasto_mici[ICH_DMALIST_MAX+1];;
	/* i/o buffer pointers */
	u_int32_t pcmo_start, pcmo_p, pcmo_end;
	int pcmo_blksize, pcmo_fifoe;
	u_int32_t pcmi_start, pcmi_p, pcmi_end;
	int pcmi_blksize, pcmi_fifoe;
	u_int32_t mici_start, mici_p, mici_end;
	int mici_blksize, mici_fifoe;
	struct ich_dma *sc_dmas;

	void (*sc_pintr) __P((void *));
	void *sc_parg;

	void (*sc_rintr) __P((void *));
	void *sc_rarg;
};

#ifdef ICH_DEBUG
#define	DPRINTF(l,x)	do { if (ich_debug & (l)) printf x; } while(0)
int ich_debug = 0xfffe;
#define	ICH_DEBUG_CODECIO	0x0001
#define	ICH_DEBUG_DMA		0x0002
#define	ICH_DEBUG_PARAM		0x0004
#else
#define	DPRINTF(x,y)	/* nothing */
#endif

struct cfdriver	ich_cd = {
	NULL, "ich", DV_DULL
};

int  ich_match __P((struct device *, void *, void *));
void ich_attach __P((struct device *, struct device *, void *));
int  ich_intr __P((void *));

struct cfattach ich_ca = {
	sizeof(struct ich_softc), ich_match, ich_attach
};

static const struct ich_devtype {
	int	product;
	int	options;
} ich_devices[] = {
	{ PCI_PRODUCT_INTEL_82801AA_ACA, 0 },
	{ PCI_PRODUCT_INTEL_82801AB_ACA, 0 },
	{ PCI_PRODUCT_INTEL_82801BA_ACA, 0 },
	{ PCI_PRODUCT_INTEL_82440MX_ACA, 0 },
};

int ich_open __P((void *, int));
void ich_close __P((void *));
int ich_query_encoding __P((void *, struct audio_encoding *));
int ich_set_params __P((void *, int, int, struct audio_params *,
    struct audio_params *));
int ich_round_blocksize __P((void *, int));
int ich_halt_output __P((void *));
int ich_halt_input __P((void *));
int ich_getdev __P((void *, struct audio_device *));
int ich_set_port __P((void *, mixer_ctrl_t *));
int ich_get_port __P((void *, mixer_ctrl_t *));
int ich_query_devinfo __P((void *, mixer_devinfo_t *));
void *ich_allocm __P((void *, u_long, int, int));
void ich_freem __P((void *, void *, int));
u_long ich_round_buffersize __P((void *, u_long));
int ich_mappage __P((void *, void *, int, int));
int ich_get_props __P((void *));
int ich_trigger_output __P((void *, void *, void *, int, void (*)(void *),
    void *, struct audio_params *));
int ich_trigger_input __P((void *, void *, void *, int, void (*)(void *),
    void *, struct audio_params *));

struct audio_hw_if ich_hw_if = {
	ich_open,
	ich_close,
	NULL,			/* drain */
	ich_query_encoding,
	ich_set_params,
	ich_round_blocksize,
	NULL,			/* commit_setting */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	ich_halt_output,
	ich_halt_input,
	NULL,			/* speaker_ctl */
	ich_getdev,
	NULL,			/* getfd */
	ich_set_port,
	ich_get_port,
	ich_query_devinfo,
	ich_allocm,
	ich_freem,
	ich_round_buffersize,
	ich_mappage,
	ich_get_props,
	ich_trigger_output,
	ich_trigger_input
};

int  ich_attach_codec __P((void *, struct ac97_codec_if *));
int  ich_read_codec __P((void *, u_int8_t, u_int16_t *));
int  ich_write_codec __P((void *, u_int8_t, u_int16_t));
void ich_reset_codec __P((void *));

int
ich_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	int i;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return 0;

	for (i = sizeof(ich_devices)/sizeof(ich_devices[0]); i--;)
		if (PCI_PRODUCT(pa->pa_id) == ich_devices[i].product)
			return 1;

	return 0;
}

void
ich_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ich_softc *sc = (struct ich_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	bus_size_t mix_size, aud_size;
	pcireg_t csr;
	const char *intrstr;

	if (pci_mapreg_map(pa, ICH_NAMBAR, PCI_MAPREG_TYPE_IO, 0,
			   &sc->iot, &sc->mix_ioh, NULL, &mix_size)) {
		printf(": can't map codec i/o space\n");
		return;
	}
	if (pci_mapreg_map(pa, ICH_NABMBAR, PCI_MAPREG_TYPE_IO, 0,
			   &sc->iot, &sc->aud_ioh, NULL, &aud_size)) {
		printf(": can't map device i/o space\n");
		bus_space_unmap(sc->iot, sc->mix_ioh, mix_size);
		return;
	}
	sc->dmat = pa->pa_dmat;

	/* enable bus mastering (should not it be mi?) */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
			 pa->pa_intrline, &ih)) {
		printf(": can't map interrupt\n");
		bus_space_unmap(sc->iot, sc->aud_ioh, aud_size);
		bus_space_unmap(sc->iot, sc->mix_ioh, mix_size);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, ich_intr, sc,
				       sc->sc_dev.dv_xname);
	if (!sc->sc_ih) {
		printf(": can't establish interrupt");
		if (intrstr)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(sc->iot, sc->aud_ioh, aud_size);
		bus_space_unmap(sc->iot, sc->mix_ioh, mix_size);
		return;
	}

#ifdef notused
	for (i = sizeof(ich_devices)/sizeof(ich_devices[0]); i--;)
		if (PCI_PRODUCT(pa->pa_id) == ich_devices[i].product)
			break;
#endif

	strcpy(sc->sc_audev.name, "ICH0 AC97");
	sprintf(sc->sc_audev.version, "0x%02x", PCI_REVISION(pa->pa_class));
	strcpy(sc->sc_audev.config, sc->sc_dev.dv_xname);

	printf(" %s: %s\n", intrstr, sc->sc_audev.name);

	/* allocate dma lists */
#define	a(a)	(void*)(((u_long)(a) + sizeof(*(a)) - 1) & ~(sizeof(*(a))-1))
	sc->dmalist_pcmo = sc->dmap_pcmo = a(sc->dmasto_pcmo);
	sc->dmalist_pcmi = sc->dmap_pcmi = a(sc->dmasto_pcmi);
	sc->dmalist_mici = sc->dmap_mici = a(sc->dmasto_mici);
#undef a
	DPRINTF(ICH_DEBUG_DMA, ("ich_attach: lists %p %p %p\n",
	    sc->dmalist_pcmo, sc->dmalist_pcmi, sc->dmalist_mici));

	/* Reset codec and AC'97 */
	ich_reset_codec(sc);

	sc->host_if.arg = sc;
	sc->host_if.attach = ich_attach_codec;
	sc->host_if.read = ich_read_codec;
	sc->host_if.write = ich_write_codec;
	sc->host_if.reset = ich_reset_codec;

	if (ac97_attach(&sc->host_if) != 0) {
		pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
		bus_space_unmap(sc->iot, sc->aud_ioh, aud_size);
		bus_space_unmap(sc->iot, sc->mix_ioh, mix_size);
		return;
	}

	audio_attach_mi(&ich_hw_if, sc, &sc->sc_dev);
}

int
ich_read_codec(v, reg, val)
	void *v;
	u_int8_t reg;
	u_int16_t *val;
{
	struct ich_softc *sc = v;
	int i;

	/* wait for an access semaphore */
	for (i = ICH_SEMATIMO; i-- &&
	    bus_space_read_1(sc->iot, sc->aud_ioh, ICH_CAS) & 1; DELAY(1));

	if (i > 0) {
		*val = bus_space_read_2(sc->iot, sc->mix_ioh, reg);
		DPRINTF(ICH_DEBUG_CODECIO,
		    ("ich_read_codec(%x, %x)\n", reg, *val));

		return 0;
	} else {
		DPRINTF(ICH_DEBUG_CODECIO,
		    ("%s: read_codec timeout\n", sc->sc_dev.dv_xname));
		return -1;
	}
}

int
ich_write_codec(v, reg, val)
	void *v;
	u_int8_t reg;
	u_int16_t val;
{
	struct ich_softc *sc = v;
	int i;

	DPRINTF(ICH_DEBUG_CODECIO, ("ich_write_codec(%x, %x)\n", reg, val));

	/* wait for an access semaphore */
	for (i = ICH_SEMATIMO; i-- &&
	    bus_space_read_1(sc->iot, sc->aud_ioh, ICH_CAS) & 1; DELAY(1));

	if (i > 0) {
		bus_space_write_2(sc->iot, sc->mix_ioh, reg, val);
		return 0;
	} else {
		DPRINTF(ICH_DEBUG_CODECIO,
		    ("%s: write_codec timeout\n", sc->sc_dev.dv_xname));
		return -1;
	}
}

int
ich_attach_codec(v, cif)
	void *v;
	struct ac97_codec_if *cif;
{
	struct ich_softc *sc = v;

	sc->codec_if = cif;
	return 0;
}

void
ich_reset_codec(v)
	void *v;
{
	struct ich_softc *sc = v;
	bus_space_write_4(sc->iot, sc->aud_ioh, ICH_GCTRL, 0);
	DELAY(10);
	bus_space_write_4(sc->iot, sc->aud_ioh, ICH_GCTRL, ICH_CRESET);
}

int
ich_open(v, flags)
	void *v;
	int flags;
{
	return 0;
}

void
ich_close(v)
	void *v;
{
}

int
ich_query_encoding(v, aep)
	void *v;
	struct audio_encoding *aep;
{
	switch (aep->index) {
	case 0:
		strcpy(aep->name, AudioEulinear);
		aep->encoding = AUDIO_ENCODING_ULINEAR;
		aep->precision = 8;
		aep->flags = 0;
		return (0);
	case 1:
		strcpy(aep->name, AudioEmulaw);
		aep->encoding = AUDIO_ENCODING_ULAW;
		aep->precision = 8;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 2:
		strcpy(aep->name, AudioEalaw);
		aep->encoding = AUDIO_ENCODING_ALAW;
		aep->precision = 8;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 3:
		strcpy(aep->name, AudioEslinear);
		aep->encoding = AUDIO_ENCODING_SLINEAR;
		aep->precision = 8;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 4:
		strcpy(aep->name, AudioEslinear_le);
		aep->encoding = AUDIO_ENCODING_SLINEAR_LE;
		aep->precision = 16;
		aep->flags = 0;
		return (0);
	case 5:
		strcpy(aep->name, AudioEulinear_le);
		aep->encoding = AUDIO_ENCODING_ULINEAR_LE;
		aep->precision = 16;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 6:
		strcpy(aep->name, AudioEslinear_be);
		aep->encoding = AUDIO_ENCODING_SLINEAR_BE;
		aep->precision = 16;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 7:
		strcpy(aep->name, AudioEulinear_be);
		aep->encoding = AUDIO_ENCODING_ULINEAR_BE;
		aep->precision = 16;
		aep->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	default:
		return (EINVAL);
	}
}

int
ich_set_params(v, setmode, usemode, play, rec)
	void *v;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct ich_softc *sc = v;
	u_int16_t val, rate;

	if (setmode & AUMODE_PLAY) {
		play->factor = 1;
		play->sw_code = NULL;
		switch(play->encoding) {
		case AUDIO_ENCODING_ULAW:
			play->factor = 2;
			play->sw_code = mulaw_to_slinear16;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (play->precision == 8)
				play->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (play->precision == 16)
				play->sw_code = change_sign16;
			break;
		case AUDIO_ENCODING_ALAW:
			play->factor = 2;
			play->sw_code = alaw_to_slinear16;
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (play->precision == 16)
				play->sw_code = swap_bytes;
			else
				play->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (play->precision == 16)
				play->sw_code = change_sign16_swap_bytes;
			break;
		default:
			return EINVAL;
		}

		ich_read_codec(sc, ICH_PM, &val);
		ich_write_codec(sc, ICH_PM, val | ICH_PM_PCMO);

		ich_write_codec(sc, ICH_PCMRATE, play->sample_rate);
		ich_read_codec(sc, ICH_PCMRATE, &rate);
		play->sample_rate = rate;

		ich_write_codec(sc, ICH_PM, val);
	}

	if (setmode & AUMODE_RECORD) {
		rec->factor = 1;
		rec->sw_code = 0;
		switch(rec->encoding) {
		case AUDIO_ENCODING_ULAW:
			rec->sw_code = ulinear8_to_mulaw;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (rec->precision == 8)
				rec->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (rec->precision == 16)
				rec->sw_code = change_sign16;
			break;
		case AUDIO_ENCODING_ALAW:
			rec->sw_code = ulinear8_to_alaw;
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (play->precision == 16)
				play->sw_code = swap_bytes;
			else
				play->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (play->precision == 16)
				play->sw_code = swap_bytes_change_sign16;
			break;
		default:
			return EINVAL;
		}

		ich_read_codec(sc, ICH_PM, &val);
		ich_write_codec(sc, ICH_PM, val | ICH_PM_PCMI);

		ich_write_codec(sc, ICH_PCMLFERATE, play->sample_rate);
		ich_read_codec(sc, ICH_PCMLFERATE, &rate);
		play->sample_rate = rate;

		ich_write_codec(sc, ICH_PM, val);
	}

	return 0;
}

int
ich_round_blocksize(v, blk)
	void *v;
	int blk;
{
	return blk & ~0x3f;
}

int
ich_halt_output(v)
	void *v;
{
	struct ich_softc *sc = v;

	DPRINTF(ICH_DEBUG_DMA, ("%s: halt_output\n", sc->sc_dev.dv_xname));

	bus_space_write_1(sc->iot, sc->aud_ioh, ICH_PCMO + ICH_CTRL, ICH_RR);

	return 0;
}

int
ich_halt_input(v)
	void *v;
{
	struct ich_softc *sc = v;

	DPRINTF(ICH_DEBUG_DMA,
	    ("%s: halt_input\n", sc->sc_dev.dv_xname));

	/* XXX halt both unless known otherwise */

	bus_space_write_1(sc->iot, sc->aud_ioh, ICH_PCMI + ICH_CTRL, ICH_RR);
	bus_space_write_1(sc->iot, sc->aud_ioh, ICH_MICI + ICH_CTRL, ICH_RR);

	return 0;
}

int
ich_getdev(v, adp)
	void *v;
	struct audio_device *adp;
{
	struct ich_softc *sc = v;
	*adp = sc->sc_audev;
	return 0;
}

int
ich_set_port(v, cp)
	void *v;
	mixer_ctrl_t *cp;
{
	struct ich_softc *sc = v;
	return sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp);
}

int
ich_get_port(v, cp)
	void *v;
	mixer_ctrl_t *cp;
{
	struct ich_softc *sc = v;
	return sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp);
}

int
ich_query_devinfo(v, dp)
	void *v;
	mixer_devinfo_t *dp;
{
	struct ich_softc *sc = v;
	return sc->codec_if->vtbl->query_devinfo(sc->codec_if, dp);
}

void *
ich_allocm(v, size, pool, flags)
	void *v;
	u_long size;
	int pool, flags;
{
	struct ich_softc *sc = v;
	struct ich_dma *p;
	int error;

	if (size > ICH_DMALIST_MAX * ICH_DMASEG_MAX)
		return NULL;

	p = malloc(sizeof(*p), pool, flags);
	if (!p)
		return NULL;

	bzero(p, sizeof(p));

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
ich_freem(v, ptr, pool)
	void *v;
	void *ptr;
	int pool;
{
	struct ich_softc *sc = v;
	struct ich_dma *p;

	for (p = sc->sc_dmas; p->addr != ptr; p = p->next)
		if (p->next == NULL) {
			printf("ich_free: trying to free not allocated memory");
			return;
		}

	bus_dmamap_unload(sc->dmat, p->map);
	bus_dmamap_destroy(sc->dmat, p->map);
	bus_dmamem_unmap(sc->dmat, p->addr, p->size);
	bus_dmamem_free(sc->dmat, p->segs, p->nsegs);
	free(p, pool);
}

u_long
ich_round_buffersize(v, size)
	void *v;
	u_long size;
{
	if (size > ICH_DMALIST_MAX * ICH_DMASEG_MAX)
		size = ICH_DMALIST_MAX * ICH_DMASEG_MAX;

	return size;
}

int
ich_mappage(v, mem, off, prot)
	void *v;
	void *mem;
	int off;
	int prot;
{
	struct ich_softc *sc = v;
	struct ich_dma *p;

	if (off < 0)
		return -1;

	for (p = sc->sc_dmas; p && p->addr != mem; p = p->next);
	if (!p)
		return -1;

	return bus_dmamem_mmap(sc->dmat, p->segs, p->nsegs,
	    off, prot, BUS_DMA_WAITOK);
}

int
ich_get_props(v)
	void *v;
{
	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

int
ich_intr(v)
	void *v;
{
	struct ich_softc *sc = v;
	int ret = 0, sts, gsts, i;

	gsts = bus_space_read_2(sc->iot, sc->aud_ioh, ICH_GSTS);
	DPRINTF(ICH_DEBUG_DMA, ("ich_intr: gsts=%b\n", gsts, ICH_GSTS_BITS));

	if (gsts & ICH_POINT) {
		sts = bus_space_read_2(sc->iot, sc->aud_ioh, ICH_PCMO+ICH_STS);
		DPRINTF(ICH_DEBUG_DMA,
		    ("ich_intr: osts=%b\n", sts, ICH_ISTS_BITS));

		if (sts & ICH_FIFOE) {
			printf("%s: fifo underrun # %u\n",
			    sc->sc_dev.dv_xname, ++sc->pcmo_fifoe);
		}

		i = bus_space_read_1(sc->iot, sc->aud_ioh, ICH_PCMO + ICH_CIV);
		if (sts & (ICH_LVBCI | ICH_CELV)) {
			struct ich_dmalist *q, *qe;

			q = sc->dmap_pcmo;
			qe = &sc->dmalist_pcmo[i];

			while (q != qe) {

				q->base = sc->pcmo_p;
				q->len = (sc->pcmo_blksize / 2) | ICH_DMAF_IOC;
				DPRINTF(ICH_DEBUG_DMA,
				    ("ich_intr: %p, %p = %x @ %p\n",
				    qe, q, sc->pcmo_blksize / 2, sc->pcmo_p));

				sc->pcmo_p += sc->pcmo_blksize;
				if (sc->pcmo_p >= sc->pcmo_end)
					sc->pcmo_p = sc->pcmo_start;

				if (++q == &sc->dmalist_pcmo[ICH_DMALIST_MAX])
					q = sc->dmalist_pcmo;
			}

			sc->dmap_pcmo = q;
			bus_space_write_1(sc->iot, sc->aud_ioh,
			    ICH_PCMO + ICH_LVI,
			    (sc->dmap_pcmo - sc->dmalist_pcmo - 1) &
			    ICH_LVI_MASK);
		}

		if (sts & ICH_BCIS && sc->sc_pintr)
			sc->sc_pintr(sc->sc_parg);

		/* int ack */
		bus_space_write_2(sc->iot, sc->aud_ioh, ICH_PCMO + ICH_STS,
		    sts & (ICH_LVBCI | ICH_CELV | ICH_BCIS | ICH_FIFOE));
		bus_space_write_2(sc->iot, sc->aud_ioh, ICH_GSTS, ICH_POINT);
		ret++;
	}

	if (gsts & ICH_PIINT) {
		sts = bus_space_read_2(sc->iot, sc->aud_ioh, ICH_PCMI+ICH_STS);
		DPRINTF(ICH_DEBUG_DMA,
		    ("ich_intr: ists=%b\n", sts, ICH_ISTS_BITS));

		if (sts & ICH_FIFOE) {
			printf("%s: fifo overrun # %u\n",
			    sc->sc_dev.dv_xname, ++sc->pcmi_fifoe);
		}

		i = bus_space_read_1(sc->iot, sc->aud_ioh, ICH_PCMI + ICH_CIV);
		if (sts & (ICH_LVBCI | ICH_CELV)) {
			struct ich_dmalist *q, *qe;

			q = sc->dmap_pcmi;
			qe = &sc->dmalist_pcmi[i];

			while (q != qe) {

				q->base = sc->pcmi_p;
				q->len = (sc->pcmi_blksize / 2) | ICH_DMAF_IOC;
				DPRINTF(ICH_DEBUG_DMA,
				    ("ich_intr: %p, %p = %x @ %p\n",
				    qe, q, sc->pcmi_blksize / 2, sc->pcmi_p));

				sc->pcmi_p += sc->pcmi_blksize;
				if (sc->pcmi_p >= sc->pcmi_end)
					sc->pcmi_p = sc->pcmi_start;

				if (++q == &sc->dmalist_pcmi[ICH_DMALIST_MAX])
					q = sc->dmalist_pcmi;
			}

			sc->dmap_pcmi = q;
			bus_space_write_1(sc->iot, sc->aud_ioh,
			    ICH_PCMI + ICH_LVI,
			    (sc->dmap_pcmi - sc->dmalist_pcmi - 1) &
			    ICH_LVI_MASK);
		}

		if (sts & ICH_BCIS && sc->sc_rintr)
			sc->sc_rintr(sc->sc_rarg);

		/* int ack */
		bus_space_write_2(sc->iot, sc->aud_ioh, ICH_PCMI + ICH_STS,
		    sts & (ICH_LVBCI | ICH_CELV | ICH_BCIS | ICH_FIFOE));
		bus_space_write_2(sc->iot, sc->aud_ioh, ICH_GSTS, ICH_POINT);
		ret++;
	}

	if (gsts & ICH_MIINT) {
		sts = bus_space_read_2(sc->iot, sc->aud_ioh, ICH_MICI+ICH_STS);
		DPRINTF(ICH_DEBUG_DMA,
		    ("ich_intr: ists=%b\n", sts, ICH_ISTS_BITS));
		if (sts & ICH_FIFOE)
			printf("%s: fifo overrun\n", sc->sc_dev.dv_xname);

		/* TODO mic input dma */

		bus_space_write_2(sc->iot, sc->aud_ioh, ICH_GSTS, ICH_MIINT);
	}

	return ret;
}

int
ich_trigger_output(v, start, end, blksize, intr, arg, param)
	void *v;
	void *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct ich_softc *sc = v;
	struct ich_dmalist *q;

	DPRINTF(ICH_DEBUG_DMA,
	    ("ich_trigger_output(%x, %x, %d, %p, %p, %p)\n",
	    kvtop((caddr_t)start), kvtop((caddr_t)end),
	    blksize, intr, arg, param));

#ifdef DIAGNOSTIC
	{
	struct ich_dma *p;
	for (p = sc->sc_dmas; p && p->addr != start; p = p->next);
	if (!p)
		return -1;
	}
#endif
	sc->sc_pintr = intr;
	sc->sc_parg = arg;

	/*
	 * The logic behind this is:
	 * setup one buffer to play, then LVI dump out the rest
	 * to the scatter-gather chain.
	 */
	sc->pcmo_start = kvtop((caddr_t)start);
	sc->pcmo_p = sc->pcmo_start + blksize;
	sc->pcmo_end = kvtop((caddr_t)end);
	sc->pcmo_blksize = blksize;

	q = sc->dmap_pcmo = sc->dmalist_pcmo;
	q->base = sc->pcmo_start;
	q->len = (blksize / 2) | ICH_DMAF_IOC;
	if (++q == &sc->dmalist_pcmo[ICH_DMALIST_MAX])
		q = sc->dmalist_pcmo;
	sc->dmap_pcmo = q;

	bus_space_write_4(sc->iot, sc->aud_ioh, ICH_PCMO + ICH_BDBAR,
	    kvtop((caddr_t)sc->dmalist_pcmo));
	bus_space_write_1(sc->iot, sc->aud_ioh, ICH_PCMO + ICH_CTRL,
	    ICH_IOCE | ICH_FEIE | ICH_LVBIE | ICH_RPBM);
	bus_space_write_1(sc->iot, sc->aud_ioh, ICH_PCMO + ICH_LVI,
	    (sc->dmap_pcmo - 1 - sc->dmalist_pcmo) & ICH_LVI_MASK);

	return 0;
}

int
ich_trigger_input(v, start, end, blksize, intr, arg, param)
	void *v;
	void *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct ich_softc *sc = v;
	struct ich_dmalist *q;

	DPRINTF(ICH_DEBUG_DMA,
	    ("ich_trigger_input(%x, %x, %d, %p, %p, %p)\n",
	    kvtop((caddr_t)start), kvtop((caddr_t)end),
	    blksize, intr, arg, param));

#ifdef DIAGNOSTIC
	{
	struct ich_dma *p;
	for (p = sc->sc_dmas; p && p->addr != start; p = p->next);
	if (!p)
		return -1;
	}
#endif
	sc->sc_rintr = intr;
	sc->sc_rarg = arg;

	/*
	 * The logic behind this is:
	 * setup one buffer to play, then LVI dump out the rest
	 * to the scatter-gather chain.
	 */
	sc->pcmi_start = kvtop((caddr_t)start);
	sc->pcmi_p = sc->pcmi_start + blksize;
	sc->pcmi_end = kvtop((caddr_t)end);
	sc->pcmi_blksize = blksize;

	q = sc->dmap_pcmi = sc->dmalist_pcmi;
	q->base = sc->pcmi_start;
	q->len = (blksize / 2) | ICH_DMAF_IOC;
	if (++q == &sc->dmalist_pcmi[ICH_DMALIST_MAX])
		q = sc->dmalist_pcmi;
	sc->dmap_pcmi = q;

	bus_space_write_4(sc->iot, sc->aud_ioh, ICH_PCMI + ICH_BDBAR,
	    kvtop((caddr_t)sc->dmalist_pcmi));
	bus_space_write_1(sc->iot, sc->aud_ioh, ICH_PCMI + ICH_CTRL,
	    ICH_IOCE | ICH_FEIE | ICH_LVBIE | ICH_RPBM);
	bus_space_write_1(sc->iot, sc->aud_ioh, ICH_PCMI + ICH_LVI,
	    (sc->dmap_pcmi - 1 - sc->dmalist_pcmi) & ICH_LVI_MASK);

	return 0;
}

