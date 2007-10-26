/*	$OpenBSD: azalia.c,v 1.37 2007/10/26 02:17:23 deanna Exp $	*/
/*	$NetBSD: azalia.c,v 1.20 2006/05/07 08:31:44 kent Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by TAMURA Kent
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
 * High Definition Audio Specification
 *	ftp://download.intel.com/standards/hdaudio/pdf/HDAudio_03.pdf
 *
 *
 * TO DO:
 *  - S/PDIF
 *  - power hook
 *  - multiple codecs (needed?)
 *  - multiple streams (needed?)
 */

#include <sys/cdefs.h>
#ifdef NETBSD_GOOP
__KERNEL_RCSID(0, "$NetBSD: azalia.c,v 1.15 2005/09/29 04:14:03 kent Exp $");
#endif

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <uvm/uvm_param.h>
#include <dev/audio_if.h>
#include <dev/auconv.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/azalia.h>

typedef struct audio_params audio_params_t;
#ifndef BUS_DMA_NOCACHE
#define	BUS_DMA_NOCACHE 0
#endif
#define	auconv_delete_encodings(x...)
#define	auconv_query_encoding(x...)	(EINVAL)
#define	auconv_create_encodings(x...)	(0)

struct audio_format {
	void *driver_data;
	int32_t mode;
	u_int encoding;
	u_int validbits;
	u_int precision;
	u_int channels;
	u_int channel_mask;
#define	AUFMT_UNKNOWN_POSITION		0U
#define	AUFMT_FRONT_LEFT		0x00001U /* USB audio compatible */
#define	AUFMT_FRONT_RIGHT		0x00002U /* USB audio compatible */
#define	AUFMT_FRONT_CENTER		0x00004U /* USB audio compatible */
#define	AUFMT_LOW_FREQUENCY		0x00008U /* USB audio compatible */
#define	AUFMT_BACK_LEFT			0x00010U /* USB audio compatible */
#define	AUFMT_BACK_RIGHT		0x00020U /* USB audio compatible */
#define	AUFMT_FRONT_LEFT_OF_CENTER	0x00040U /* USB audio compatible */
#define	AUFMT_FRONT_RIGHT_OF_CENTER	0x00080U /* USB audio compatible */
#define	AUFMT_BACK_CENTER		0x00100U /* USB audio compatible */
#define	AUFMT_SIDE_LEFT			0x00200U /* USB audio compatible */
#define	AUFMT_SIDE_RIGHT		0x00400U /* USB audio compatible */
#define	AUFMT_TOP_CENTER		0x00800U /* USB audio compatible */
#define	AUFMT_TOP_FRONT_LEFT		0x01000U
#define	AUFMT_TOP_FRONT_CENTER		0x02000U
#define	AUFMT_TOP_FRONT_RIGHT		0x04000U
#define	AUFMT_TOP_BACK_LEFT		0x08000U
#define	AUFMT_TOP_BACK_CENTER		0x10000U
#define	AUFMT_TOP_BACK_RIGHT		0x20000U

#define	AUFMT_MONAURAL		AUFMT_FRONT_CENTER
#define	AUFMT_STEREO		(AUFMT_FRONT_LEFT | AUFMT_FRONT_RIGHT)
#define	AUFMT_SURROUND4		(AUFMT_STEREO | AUFMT_BACK_LEFT \
				| AUFMT_BACK_RIGHT)
#define	AUFMT_DOLBY_5_1		(AUFMT_SURROUND4 | AUFMT_FRONT_CENTER \
				| AUFMT_LOW_FREQUENCY)

	/**
	 * 0: frequency[0] is lower limit, and frequency[1] is higher limit.
	 * 1-16: frequency[0] to frequency[frequency_type-1] are valid.
	 */
	u_int frequency_type;

#define	AUFMT_MAX_FREQUENCIES	16
	/**
	 * sampling rates
	 */
	u_int frequency[AUFMT_MAX_FREQUENCIES];
};

#define	AUFMT_INVALIDATE(fmt)	(fmt)->mode |= 0x80000000
#define	AUFMT_VALIDATE(fmt)	(fmt)->mode &= 0x7fffffff
#define	AUFMT_IS_VALID(fmt)	(((fmt)->mode & 0x80000000) == 0)

/* ----------------------------------------------------------------
 * ICH6/ICH7 constant values
 * ---------------------------------------------------------------- */

/* PCI registers */
#define ICH_PCI_HDBARL	0x10
#define ICH_PCI_HDBARU	0x14
#define ICH_PCI_HDCTL	0x40
#define		ICH_PCI_HDCTL_CLKDETCLR		0x08
#define		ICH_PCI_HDCTL_CLKDETEN		0x04
#define		ICH_PCI_HDCTL_CLKDETINV		0x02
#define		ICH_PCI_HDCTL_SIGNALMODE	0x01

/* internal types */

typedef struct {
	bus_dmamap_t map;
	caddr_t addr;		/* kernel virtual address */
	bus_dma_segment_t segments[1];
	size_t size;
} azalia_dma_t;
#define AZALIA_DMA_DMAADDR(p)	((p)->map->dm_segs[0].ds_addr)

typedef struct {
	struct azalia_t *az;
	int regbase;
	int number;
	int dir;		/* AUMODE_PLAY or AUMODE_RECORD */
	uint32_t intr_bit;
	azalia_dma_t bdlist;
	azalia_dma_t buffer;
	void (*intr)(void*);
	void *intr_arg;
} stream_t;
#define STR_READ_1(s, r)	\
	bus_space_read_1((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r)
#define STR_READ_2(s, r)	\
	bus_space_read_2((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r)
#define STR_READ_4(s, r)	\
	bus_space_read_4((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r)
#define STR_WRITE_1(s, r, v)	\
	bus_space_write_1((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r, v)
#define STR_WRITE_2(s, r, v)	\
	bus_space_write_2((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r, v)
#define STR_WRITE_4(s, r, v)	\
	bus_space_write_4((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r, v)

typedef struct azalia_t {
	struct device dev;
	struct device *audiodev;

	pci_chipset_tag_t pc;
	void *ih;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t map_size;
	bus_dma_tag_t dmat;
	pcireg_t pciid;
	uint32_t subid;

	codec_t codecs[15];
	int ncodecs;		/* number of codecs */
	int codecno;		/* index of the using codec */

	azalia_dma_t corb_dma;
	int corb_size;
	azalia_dma_t rirb_dma;
	int rirb_size;
	int rirb_rp;
#define UNSOLQ_SIZE	256
	rirb_entry_t *unsolq;
	int unsolq_wp;
	int unsolq_rp;
	boolean_t unsolq_kick;

	boolean_t ok64;
	int nistreams, nostreams, nbstreams;
	stream_t pstream;
	stream_t rstream;
} azalia_t;
#define XNAME(sc)		((sc)->dev.dv_xname)
#define AZ_READ_1(z, r)		bus_space_read_1((z)->iot, (z)->ioh, HDA_##r)
#define AZ_READ_2(z, r)		bus_space_read_2((z)->iot, (z)->ioh, HDA_##r)
#define AZ_READ_4(z, r)		bus_space_read_4((z)->iot, (z)->ioh, HDA_##r)
#define AZ_WRITE_1(z, r, v)	bus_space_write_1((z)->iot, (z)->ioh, HDA_##r, v)
#define AZ_WRITE_2(z, r, v)	bus_space_write_2((z)->iot, (z)->ioh, HDA_##r, v)
#define AZ_WRITE_4(z, r, v)	bus_space_write_4((z)->iot, (z)->ioh, HDA_##r, v)


/* prototypes */
int	azalia_pci_match(struct device *, void *, void *);
void	azalia_pci_attach(struct device *, struct device *, void *);
int	azalia_pci_activate(struct device *, enum devact);
int	azalia_pci_detach(struct device *, int);
int	azalia_intr(void *);
int	azalia_attach(azalia_t *);
void	azalia_attach_intr(struct device *);
int	azalia_init_corb(azalia_t *);
int	azalia_delete_corb(azalia_t *);
int	azalia_init_rirb(azalia_t *);
int	azalia_delete_rirb(azalia_t *);
int	azalia_set_command(azalia_t *, nid_t, int, uint32_t,
	uint32_t);
int	azalia_get_response(azalia_t *, uint32_t *);
void	azalia_rirb_kick_unsol_events(azalia_t *);
void	azalia_rirb_intr(azalia_t *);
int	azalia_alloc_dmamem(azalia_t *, size_t, size_t, azalia_dma_t *);
int	azalia_free_dmamem(const azalia_t *, azalia_dma_t*);

int	azalia_codec_init(codec_t *);
int	azalia_codec_delete(codec_t *);
void	azalia_codec_add_bits(codec_t *, int, uint32_t, int);
void	azalia_codec_add_format(codec_t *, int, int, int, uint32_t,
	int32_t);
int	azalia_codec_comresp(const codec_t *, nid_t, uint32_t,
	uint32_t, uint32_t *);
int	azalia_codec_connect_stream(codec_t *, int, uint16_t, int);

int	azalia_widget_init(widget_t *, const codec_t *, int);
int	azalia_widget_init_audio(widget_t *, const codec_t *);
int	azalia_widget_print_audio(const widget_t *, const char *);
int	azalia_widget_init_pin(widget_t *, const codec_t *);
int	azalia_widget_print_pin(const widget_t *);
int	azalia_widget_init_connection(widget_t *, const codec_t *);

int	azalia_stream_init(stream_t *, azalia_t *, int, int, int);
int	azalia_stream_delete(stream_t *, azalia_t *);
int	azalia_stream_reset(stream_t *);
int	azalia_stream_start(stream_t *, void *, void *, int,
	void (*)(void *), void *, uint16_t);
int	azalia_stream_halt(stream_t *);
int	azalia_stream_intr(stream_t *, uint32_t);

int	azalia_open(void *, int);
void	azalia_close(void *);
int	azalia_query_encoding(void *, audio_encoding_t *);
int	azalia_set_params(void *, int, int, audio_params_t *,
	audio_params_t *);
int	azalia_round_blocksize(void *, int);
int	azalia_halt_output(void *);
int	azalia_halt_input(void *);
int	azalia_getdev(void *, struct audio_device *);
int	azalia_set_port(void *, mixer_ctrl_t *);
int	azalia_get_port(void *, mixer_ctrl_t *);
int	azalia_query_devinfo(void *, mixer_devinfo_t *);
void	*azalia_allocm(void *, int, size_t, int, int);
void	azalia_freem(void *, void *, int);
size_t	azalia_round_buffersize(void *, int, size_t);
int	azalia_get_props(void *);
int	azalia_trigger_output(void *, void *, void *, int,
	void (*)(void *), void *, audio_params_t *);
int	azalia_trigger_input(void *, void *, void *, int,
	void (*)(void *), void *, audio_params_t *);

int	azalia_params2fmt(const audio_params_t *, uint16_t *);
int azalia_create_encodings(struct audio_format *, int,
    struct audio_encoding_set **);

/* variables */
struct cfattach azalia_ca = {
	sizeof(azalia_t), azalia_pci_match, azalia_pci_attach,
	azalia_pci_detach, azalia_pci_activate
};

struct cfdriver azalia_cd = {
	NULL, "azalia", DV_DULL
};

struct audio_hw_if azalia_hw_if = {
	azalia_open,
	azalia_close,
	NULL,			/* drain */
	azalia_query_encoding,
	azalia_set_params,
	azalia_round_blocksize,
	NULL,			/* commit_settings */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	azalia_halt_output,
	azalia_halt_input,
	NULL,			/* speaker_ctl */
	azalia_getdev,
	NULL,			/* setfd */
	azalia_set_port,
	azalia_get_port,
	azalia_query_devinfo,
	azalia_allocm,
	azalia_freem,
	azalia_round_buffersize,
	NULL,			/* mappage */
	azalia_get_props,
	azalia_trigger_output,
	azalia_trigger_input,
};

static const char *pin_colors[16] = {
	"unknown", "black", "gray", "blue",
	"green", "red", "orange", "yellow",
	"purple", "pink", "col0a", "col0b",
	"col0c", "col0d", "white", "other"};
#ifdef AZALIA_DEBUG
static const char *pin_devices[16] = {
	"line-out", AudioNspeaker, AudioNheadphone, AudioNcd,
	"SPDIF-out", "digital-out", "modem-line", "modem-handset",
	"line-in", AudioNaux, AudioNmicrophone, "telephony",
	"SPDIF-in", "digital-in", "dev0e", "other"};
static const char *pin_conn[4] = {
	"jack", "none", "fixed", "combined"};
static const char *pin_conntype[16] = {
	"unknown", "1/8", "1/4", "atapi", "rca", "optical",
	"digital", "analog", "din", "xlr", "rj-11", "combination",
	"con0c", "con0d", "con0e", "other"};
static const char *pin_geo[15] = {
	"n/a", "rear", "front", "left",
	"right", "top", "bottom", "spec0", "spec1", "spec2",
	"loc0a", "loc0b", "loc0c", "loc0d", "loc0f"};
static const char *pin_chass[4] = {
	"external", "internal", "separate", "other"};
#endif

/* ================================================================
 * PCI functions
 * ================================================================ */

#define PCI_ID_CODE0(v, p)	PCI_ID_CODE(PCI_VENDOR_##v, PCI_PRODUCT_##v##_##p)
#define PCIID_NVIDIA_MCP51	PCI_ID_CODE0(NVIDIA, MCP51_HDA)
#define PCIID_NVIDIA_MCP55	PCI_ID_CODE0(NVIDIA, MCP55_HDA)
#define PCIID_ALI_M5461		PCI_ID_CODE0(ALI, M5461)
#define PCIID_VIATECH_HDA	PCI_ID_CODE0(VIATECH, HDA)

int
azalia_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa;

	pa = aux;
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_MULTIMEDIA
	    && PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MULTIMEDIA_HDAUDIO)
		return 1;
	return 0;
}

void
azalia_pci_attach(struct device *parent, struct device *self, void *aux)
{
	azalia_t *sc;
	struct pci_attach_args *pa;
	pcireg_t v;
	pci_intr_handle_t ih;
	const char *intrrupt_str;

	sc = (azalia_t*)self;
	pa = aux;

	sc->dmat = pa->pa_dmat;

	v = pci_conf_read(pa->pa_pc, pa->pa_tag, ICH_PCI_HDBARL);
	v &= PCI_MAPREG_TYPE_MASK | PCI_MAPREG_MEM_TYPE_MASK;
	if (pci_mapreg_map(pa, ICH_PCI_HDBARL, v, 0,
			   &sc->iot, &sc->ioh, NULL, &sc->map_size, 0)) {
		printf(": can't map device i/o space\n");
		return;
	}

	/* enable back-to-back */
	v = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    v | PCI_COMMAND_BACKTOBACK_ENABLE);

	v = pci_conf_read(pa->pa_pc, pa->pa_tag, 0x44);
	pci_conf_write(pa->pa_pc, pa->pa_tag, 0x44, v & (~0x7));

	/* interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		return;
	}
	sc->pc = pa->pa_pc;
	intrrupt_str = pci_intr_string(pa->pa_pc, ih);
	sc->ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, azalia_intr,
	    sc, sc->dev.dv_xname);
	if (sc->ih == NULL) {
		printf(": can't establish interrupt");
		if (intrrupt_str != NULL)
			printf(" at %s", intrrupt_str);
		printf("\n");
		return;
	}
	printf(": %s\n", intrrupt_str);

	sc->pciid = pa->pa_id;

	if (azalia_attach(sc)) {
		printf("%s: initialization failure\n", XNAME(sc));
		azalia_pci_detach(self, 0);
		return;
	}
	sc->subid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	azalia_attach_intr(self);
}

int
azalia_pci_activate(struct device *self, enum devact act)
{
	azalia_t *sc;
	int ret;

	sc = (azalia_t*)self;
	ret = 0;
	switch (act) {
	case DVACT_ACTIVATE:
		return ret;
	case DVACT_DEACTIVATE:
		if (sc->audiodev != NULL)
			ret = config_deactivate(sc->audiodev);
		return ret;
	}
	return EOPNOTSUPP;
}

int
azalia_pci_detach(struct device *self, int flags)
{
	azalia_t *az;
	int i;

	DPRINTF(("%s\n", __func__));
	az = (azalia_t*)self;
	if (az->audiodev != NULL) {
		config_detach(az->audiodev, flags);
		az->audiodev = NULL;
	}

	DPRINTF(("%s: delete streams\n", __func__));
	azalia_stream_delete(&az->rstream, az);
	azalia_stream_delete(&az->pstream, az);

	DPRINTF(("%s: delete codecs\n", __func__));
	for (i = 0; i < az->ncodecs; i++) {
		azalia_codec_delete(&az->codecs[i]);
	}
	az->ncodecs = 0;

	DPRINTF(("%s: delete CORB and RIRB\n", __func__));
	azalia_delete_corb(az);
	azalia_delete_rirb(az);

	DPRINTF(("%s: delete PCI resources\n", __func__));
	if (az->ih != NULL) {
		pci_intr_disestablish(az->pc, az->ih);
		az->ih = NULL;
	}
	if (az->map_size != 0) {
		bus_space_unmap(az->iot, az->ioh, az->map_size);
		az->map_size = 0;
	}
	return 0;
}

int
azalia_intr(void *v)
{
	azalia_t *az = v;
	int ret = 0;
	uint32_t intsts;
	uint8_t rirbsts, rirbctl;

	intsts = AZ_READ_4(az, INTSTS);
	if (intsts == 0)
		return (0);

	AZ_WRITE_4(az, INTSTS, intsts);

	ret += azalia_stream_intr(&az->pstream, intsts);
	ret += azalia_stream_intr(&az->rstream, intsts);

	rirbctl = AZ_READ_1(az, RIRBCTL);
	rirbsts = AZ_READ_1(az, RIRBSTS);

	if (intsts & HDA_INTCTL_CIE) {
		if (rirbctl & HDA_RIRBCTL_RINTCTL) {
			if (rirbsts & HDA_RIRBSTS_RINTFL)
				azalia_rirb_intr(az);
		}
	}

	return (1);
}

/* ================================================================
 * HDA controller functions
 * ================================================================ */

int
azalia_attach(azalia_t *az)
{
	int i, n;
	uint32_t gctl;
	uint16_t gcap;
	uint16_t statests;

	printf("%s: host: High Definition Audio rev. %d.%d\n",
	    XNAME(az), AZ_READ_1(az, VMAJ), AZ_READ_1(az, VMIN));
	gcap = AZ_READ_2(az, GCAP);
	az->nistreams = HDA_GCAP_ISS(gcap);
	az->nostreams = HDA_GCAP_OSS(gcap);
	az->nbstreams = HDA_GCAP_BSS(gcap);
	az->ok64 = (gcap & HDA_GCAP_64OK) != 0;
	DPRINTF(("%s: host: %d output, %d input, and %d bidi streams\n",
	    XNAME(az), az->nostreams, az->nistreams, az->nbstreams));

	/* 4.2.2 Starting the High Definition Audio Controller */
	DPRINTF(("%s: resetting\n", __func__));
	gctl = AZ_READ_4(az, GCTL);
	AZ_WRITE_4(az, GCTL, gctl & ~HDA_GCTL_CRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		if ((AZ_READ_4(az, GCTL) & HDA_GCTL_CRST) == 0)
			break;
	}
	DPRINTF(("%s: reset counter = %d\n", __func__, i));
	if (i <= 0) {
		printf("%s: reset failure\n", XNAME(az));
		return ETIMEDOUT;
	}
	DELAY(1000);
	gctl = AZ_READ_4(az, GCTL);
	AZ_WRITE_4(az, GCTL, gctl | HDA_GCTL_CRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		if (AZ_READ_4(az, GCTL) & HDA_GCTL_CRST)
			break;
	}
	DPRINTF(("%s: reset counter = %d\n", __func__, i));
	if (i <= 0) {
		printf("%s: reset-exit failure\n", XNAME(az));
		return ETIMEDOUT;
	}

	/* enable unsolicited response */
	gctl = AZ_READ_4(az, GCTL);
	AZ_WRITE_4(az, GCTL, gctl | HDA_GCTL_UNSOL);

	/* 4.3 Codec discovery */
	DELAY(1000);
	statests = AZ_READ_2(az, STATESTS);
	for (i = 0, n = 0; i < 15; i++) {
		if ((statests >> i) & 1) {
			DPRINTF(("%s: found a codec at #%d\n", XNAME(az), i));
			az->codecs[n].address = i;
			az->codecs[n++].az = az;
		}
	}
	az->ncodecs = n;
	if (az->ncodecs < 1) {
		printf("%s: No HD-Audio codecs\n", XNAME(az));
		return -1;
	}
	return 0;
}

void
azalia_attach_intr(struct device *self)
{
	azalia_t *az;
	int err, i, c;

	az = (azalia_t*)self;

	AZ_WRITE_2(az, STATESTS, HDA_STATESTS_SDIWAKE);
	AZ_WRITE_1(az, RIRBSTS, HDA_RIRBSTS_RINTFL | HDA_RIRBSTS_RIRBOIS);
	AZ_WRITE_4(az, INTSTS, HDA_INTSTS_CIS | HDA_INTSTS_GIS);
	AZ_WRITE_4(az, DPLBASE, 0);
	AZ_WRITE_4(az, DPUBASE, 0);

	/* 4.4.1 Command Outbound Ring Buffer */
	if (azalia_init_corb(az))
		goto err_exit;
	/* 4.4.2 Response Inbound Ring Buffer */
	if (azalia_init_rirb(az))
		goto err_exit;

	AZ_WRITE_4(az, INTCTL,
	    AZ_READ_4(az, INTCTL) | HDA_INTCTL_CIE | HDA_INTCTL_GIE);

	c = -1;
	for (i = 0; i < az->ncodecs; i++) {
		err = azalia_codec_init(&az->codecs[i]);
		if (!err && c < 0)
			c = i;
	}
	if (c < 0)
		goto err_exit;
	/* Use the first audio codec */
	az->codecno = c;
	DPRINTF(("%s: using the #%d codec\n", XNAME(az), az->codecno));

	if (azalia_stream_init(&az->pstream, az, az->nistreams + 0,
	    1, AUMODE_PLAY))
		goto err_exit;
	if (azalia_stream_init(&az->rstream, az, 0, 2, AUMODE_RECORD))
		goto err_exit;

	az->audiodev = audio_attach_mi(&azalia_hw_if, az, &az->dev);
	return;
err_exit:
	azalia_pci_detach(self, 0);
	return;
}

int
azalia_init_corb(azalia_t *az)
{
	int entries, err, i;
	uint16_t corbrp, corbwp;
	uint8_t corbsize, cap, corbctl;

	/* stop the CORB */
	corbctl = AZ_READ_1(az, CORBCTL);
	if (corbctl & HDA_CORBCTL_CORBRUN) { /* running? */
		AZ_WRITE_1(az, CORBCTL, corbctl & ~HDA_CORBCTL_CORBRUN);
		for (i = 5000; i >= 0; i--) {
			DELAY(10);
			corbctl = AZ_READ_1(az, CORBCTL);
			if ((corbctl & HDA_CORBCTL_CORBRUN) == 0)
				break;
		}
		if (i <= 0) {
			printf("%s: CORB is running\n", XNAME(az));
			return EBUSY;
		}
	}

	/* determine CORB size */
	corbsize = AZ_READ_1(az, CORBSIZE);
	cap = corbsize & HDA_CORBSIZE_CORBSZCAP_MASK;
	corbsize &= ~HDA_CORBSIZE_CORBSIZE_MASK;
	if (cap & HDA_CORBSIZE_CORBSZCAP_256) {
		entries = 256;
		corbsize |= HDA_CORBSIZE_CORBSIZE_256;
	} else if (cap & HDA_CORBSIZE_CORBSZCAP_16) {
		entries = 16;
		corbsize |= HDA_CORBSIZE_CORBSIZE_16;
	} else if (cap & HDA_CORBSIZE_CORBSZCAP_2) {
		entries = 2;
		corbsize |= HDA_CORBSIZE_CORBSIZE_2;
	} else {
		printf("%s: Invalid CORBSZCAP: 0x%2x\n", XNAME(az), cap);
		return -1;
	}

	err = azalia_alloc_dmamem(az, entries * sizeof(corb_entry_t),
	    128, &az->corb_dma);
	if (err) {
		printf("%s: can't allocate CORB buffer\n", XNAME(az));
		return err;
	}
	AZ_WRITE_4(az, CORBLBASE, (uint32_t)AZALIA_DMA_DMAADDR(&az->corb_dma));
	AZ_WRITE_4(az, CORBUBASE, PTR_UPPER32(AZALIA_DMA_DMAADDR(&az->corb_dma)));
	AZ_WRITE_1(az, CORBSIZE, corbsize);
	az->corb_size = entries;

	DPRINTF(("%s: CORB allocation succeeded.\n", __func__));

	/* reset CORBRP */
	corbrp = AZ_READ_2(az, CORBRP);
	AZ_WRITE_2(az, CORBRP, corbrp | HDA_CORBRP_CORBRPRST);
	AZ_WRITE_2(az, CORBRP, corbrp & ~HDA_CORBRP_CORBRPRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		corbrp = AZ_READ_2(az, CORBRP);
		if ((corbrp & HDA_CORBRP_CORBRPRST) == 0)
			break;
	}
	if (i <= 0) {
		printf("%s: CORBRP reset failure\n", XNAME(az));
		return -1;
	}
	DPRINTF(("%s: CORBWP=%d; size=%d\n", __func__,
		 AZ_READ_2(az, CORBRP) & HDA_CORBRP_CORBRP, az->corb_size));

	/* clear CORBWP */
	corbwp = AZ_READ_2(az, CORBWP);
	AZ_WRITE_2(az, CORBWP, corbwp & ~HDA_CORBWP_CORBWP);

	/* Run! */
	corbctl = AZ_READ_1(az, CORBCTL);
	AZ_WRITE_1(az, CORBCTL, corbctl | HDA_CORBCTL_CORBRUN);
	return 0;
}

int
azalia_delete_corb(azalia_t *az)
{
	int i;
	uint8_t corbctl;

	if (az->corb_dma.addr == NULL)
		return 0;
	/* stop the CORB */
	corbctl = AZ_READ_1(az, CORBCTL);
	AZ_WRITE_1(az, CORBCTL, corbctl & ~HDA_CORBCTL_CORBRUN);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		corbctl = AZ_READ_1(az, CORBCTL);
		if ((corbctl & HDA_CORBCTL_CORBRUN) == 0)
			break;
	}
	azalia_free_dmamem(az, &az->corb_dma);
	return 0;
}

int
azalia_init_rirb(azalia_t *az)
{
	int entries, err, i;
	uint16_t rirbwp;
	uint8_t rirbsize, cap, rirbctl;

	/* stop the RIRB */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	if (rirbctl & HDA_RIRBCTL_RIRBDMAEN) { /* running? */
		AZ_WRITE_1(az, RIRBCTL, rirbctl & ~HDA_RIRBCTL_RIRBDMAEN);
		for (i = 5000; i >= 0; i--) {
			DELAY(10);
			rirbctl = AZ_READ_1(az, RIRBCTL);
			if ((rirbctl & HDA_RIRBCTL_RIRBDMAEN) == 0)
				break;
		}
		if (i <= 0) {
			printf("%s: RIRB is running\n", XNAME(az));
			return EBUSY;
		}
	}

	/* determine RIRB size */
	rirbsize = AZ_READ_1(az, RIRBSIZE);
	cap = rirbsize & HDA_RIRBSIZE_RIRBSZCAP_MASK;
	rirbsize &= ~HDA_RIRBSIZE_RIRBSIZE_MASK;
	if (cap & HDA_RIRBSIZE_RIRBSZCAP_256) {
		entries = 256;
		rirbsize |= HDA_RIRBSIZE_RIRBSIZE_256;
	} else if (cap & HDA_RIRBSIZE_RIRBSZCAP_16) {
		entries = 16;
		rirbsize |= HDA_RIRBSIZE_RIRBSIZE_16;
	} else if (cap & HDA_RIRBSIZE_RIRBSZCAP_2) {
		entries = 2;
		rirbsize |= HDA_RIRBSIZE_RIRBSIZE_2;
	} else {
		printf("%s: Invalid RIRBSZCAP: 0x%2x\n", XNAME(az), cap);
		return -1;
	}

	err = azalia_alloc_dmamem(az, entries * sizeof(rirb_entry_t),
	    128, &az->rirb_dma);
	if (err) {
		printf("%s: can't allocate RIRB buffer\n", XNAME(az));
		return err;
	}
	AZ_WRITE_4(az, RIRBLBASE, (uint32_t)AZALIA_DMA_DMAADDR(&az->rirb_dma));
	AZ_WRITE_4(az, RIRBUBASE, PTR_UPPER32(AZALIA_DMA_DMAADDR(&az->rirb_dma)));
	AZ_WRITE_1(az, RIRBSIZE, rirbsize);
	az->rirb_size = entries;

	DPRINTF(("%s: RIRB allocation succeeded.\n", __func__));

	/* setup the unsolicited response queue */
	az->unsolq_rp = 0;
	az->unsolq_wp = 0;
	az->unsolq_kick = FALSE;
	az->unsolq = malloc(sizeof(rirb_entry_t) * UNSOLQ_SIZE,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (az->unsolq == NULL) {
		DPRINTF(("%s: can't allocate unsolicited response queue.\n",
		    XNAME(az)));
		azalia_free_dmamem(az, &az->rirb_dma);
		return ENOMEM;
	}

	/* reset the write pointer */
	rirbwp = AZ_READ_2(az, RIRBWP);
	AZ_WRITE_2(az, RIRBWP, rirbwp | HDA_RIRBWP_RIRBWPRST);

	/* clear the read pointer */
	az->rirb_rp = AZ_READ_2(az, RIRBWP) & HDA_RIRBWP_RIRBWP;
	DPRINTF(("%s: RIRBRP=%d, size=%d\n", __func__, az->rirb_rp, az->rirb_size));

	AZ_WRITE_2(az, RINTCNT, 1);

	/* Run! */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	AZ_WRITE_1(az, RIRBCTL, rirbctl |
	    HDA_RIRBCTL_RIRBDMAEN | HDA_RIRBCTL_RINTCTL);

	return (0);
}

int
azalia_delete_rirb(azalia_t *az)
{
	int i;
	uint8_t rirbctl;

	if (az->unsolq != NULL) {
		free(az->unsolq, M_DEVBUF);
		az->unsolq = NULL;
	}
	if (az->rirb_dma.addr == NULL)
		return 0;
	/* stop the RIRB */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	AZ_WRITE_1(az, RIRBCTL, rirbctl & ~HDA_RIRBCTL_RIRBDMAEN);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		rirbctl = AZ_READ_1(az, RIRBCTL);
		if ((rirbctl & HDA_RIRBCTL_RIRBDMAEN) == 0)
			break;
	}
	azalia_free_dmamem(az, &az->rirb_dma);
	return 0;
}

int
azalia_set_command(azalia_t *az, int caddr, nid_t nid, uint32_t control,
		   uint32_t param)
{
	corb_entry_t *corb;
	int  wp;
	uint32_t verb;
	uint16_t corbwp;
	uint8_t rirbctl;

#ifdef DIAGNOSTIC
	if ((AZ_READ_1(az, CORBCTL) & HDA_CORBCTL_CORBRUN) == 0) {
		printf("%s: CORB is not running.\n", XNAME(az));
		return -1;
	}
#endif
	verb = (caddr << 28) | (nid << 20) | (control << 8) | param;
	corbwp = AZ_READ_2(az, CORBWP);
	wp = corbwp & HDA_CORBWP_CORBWP;
	corb = (corb_entry_t*)az->corb_dma.addr;
	if (++wp >= az->corb_size)
		wp = 0;
	corb[wp] = verb;

	/* disable RIRB interrupts */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	if (rirbctl & HDA_RIRBCTL_RINTCTL) {
		AZ_WRITE_1(az, RIRBCTL, rirbctl & ~HDA_RIRBCTL_RINTCTL);
		azalia_rirb_intr(az);
	}

	AZ_WRITE_2(az, CORBWP, (corbwp & ~HDA_CORBWP_CORBWP) | wp);
#if 0
	DPRINTF(("%s: caddr=%d nid=%d control=0x%x param=0x%x verb=0x%8.8x wp=%d\n",
		 __func__, caddr, nid, control, param, verb, wp));
#endif
	return 0;
}

int
azalia_get_response(azalia_t *az, uint32_t *result)
{
	const rirb_entry_t *rirb;
	int i;
	uint16_t wp;
	uint8_t rirbctl;

#ifdef DIAGNOSTIC
	if ((AZ_READ_1(az, RIRBCTL) & HDA_RIRBCTL_RIRBDMAEN) == 0) {
		printf("%s: RIRB is not running.\n", XNAME(az));
		return -1;
	}
#endif
	for (i = 5000; i >= 0; i--) {
		wp = AZ_READ_2(az, RIRBWP) & HDA_RIRBWP_RIRBWP;
		if (az->rirb_rp != wp)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		printf("%s: RIRB time out\n", XNAME(az));
		return ETIMEDOUT;
	}
	rirb = (rirb_entry_t*)az->rirb_dma.addr;
	for (;;) {
		if (++az->rirb_rp >= az->rirb_size)
			az->rirb_rp = 0;
		if (rirb[az->rirb_rp].resp_ex & RIRB_RESP_UNSOL) {
			az->unsolq[az->unsolq_wp].resp = rirb[az->rirb_rp].resp;
			az->unsolq[az->unsolq_wp++].resp_ex = rirb[az->rirb_rp].resp_ex;
			az->unsolq_wp %= UNSOLQ_SIZE;
		} else
			break;
	}
	if (result != NULL)
		*result = rirb[az->rirb_rp].resp;

	azalia_rirb_kick_unsol_events(az);
#if 0
	for (i = 0; i < 16 /*az->rirb_size*/; i++) {
		DPRINTF(("rirb[%d] 0x%8.8x:0x%8.8x ", i, rirb[i].resp, rirb[i].resp_ex));
		if ((i % 2) == 1)
			DPRINTF(("\n"));
	}
#endif

	/* re-enable RIRB interrupts */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	AZ_WRITE_1(az, RIRBCTL, rirbctl | HDA_RIRBCTL_RINTCTL);

	return 0;
}

void
azalia_rirb_kick_unsol_events(azalia_t *az)
{
	if (az->unsolq_kick)
		return;
	az->unsolq_kick = TRUE;
	while (az->unsolq_rp != az->unsolq_wp) {
		int i;
		int tag;
		codec_t *codec;
		i = RIRB_RESP_CODEC(az->unsolq[az->unsolq_rp].resp_ex);
		tag = RIRB_UNSOL_TAG(az->unsolq[az->unsolq_rp].resp);
		codec = &az->codecs[i];
		DPRINTF(("%s: codec#=%d tag=%d\n", __func__, i, tag));
		az->unsolq_rp++;
		az->unsolq_rp %= UNSOLQ_SIZE;
		if (codec->unsol_event != NULL)
			codec->unsol_event(codec, tag);
	}
	az->unsolq_kick = FALSE;
}

void
azalia_rirb_intr(azalia_t *az)
{
	const rirb_entry_t *rirb;
	uint16_t wp, rp;
	uint8_t rirbsts;

	rirbsts = AZ_READ_1(az, RIRBSTS);

	wp = AZ_READ_2(az, RIRBWP) & HDA_RIRBWP_RIRBWP;
	if (rp == wp)
		return;		/* interrupted but no data in RIRB */
	rirb = (rirb_entry_t*)az->rirb_dma.addr;
	while (az->rirb_rp != wp) {
		if (++az->rirb_rp >= az->rirb_size)
			az->rirb_rp = 0;
		if (rirb[az->rirb_rp].resp_ex & RIRB_RESP_UNSOL) {
			az->unsolq[az->unsolq_wp].resp = rirb[az->rirb_rp].resp;
			az->unsolq[az->unsolq_wp++].resp_ex = rirb[az->rirb_rp].resp_ex;
			az->unsolq_wp %= UNSOLQ_SIZE;
		} else {
			break;
		}
	}

	azalia_rirb_kick_unsol_events(az);

	AZ_WRITE_1(az, RIRBSTS,
	    rirbsts | HDA_RIRBSTS_RIRBOIS | HDA_RIRBSTS_RINTFL);
}

int
azalia_alloc_dmamem(azalia_t *az, size_t size, size_t align, azalia_dma_t *d)
{
	int err;
	int nsegs;

	d->size = size;
	err = bus_dmamem_alloc(az->dmat, size, align, 0, d->segments, 1,
	    &nsegs, BUS_DMA_NOWAIT);
	if (err)
		return err;
	if (nsegs != 1)
		goto free;
	err = bus_dmamem_map(az->dmat, d->segments, 1, size,
	    &d->addr, BUS_DMA_NOWAIT | BUS_DMA_COHERENT | BUS_DMA_NOCACHE);
	if (err)
		goto free;
	err = bus_dmamap_create(az->dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &d->map);
	if (err)
		goto unmap;
	err = bus_dmamap_load(az->dmat, d->map, d->addr, size,
	    NULL, BUS_DMA_NOWAIT);
	if (err)
		goto destroy;

	if (!az->ok64 && PTR_UPPER32(AZALIA_DMA_DMAADDR(d)) != 0) {
		azalia_free_dmamem(az, d);
		return -1;
	}
	return 0;

destroy:
	bus_dmamap_destroy(az->dmat, d->map);
unmap:
	bus_dmamem_unmap(az->dmat, d->addr, size);
free:
	bus_dmamem_free(az->dmat, d->segments, 1);
	d->addr = NULL;
	return err;
}

int
azalia_free_dmamem(const azalia_t *az, azalia_dma_t* d)
{
	if (d->addr == NULL)
		return 0;
	bus_dmamap_unload(az->dmat, d->map);
	bus_dmamap_destroy(az->dmat, d->map);
	bus_dmamem_unmap(az->dmat, d->addr, d->size);
	bus_dmamem_free(az->dmat, d->segments, 1);
	d->addr = NULL;
	return 0;
}

/* ================================================================
 * HDA codec functions
 * ================================================================ */

int
azalia_codec_init(codec_t *this)
{
	uint32_t rev, id, result;
	int err, addr, n, i;
	const char *vendor;

	this->comresp = azalia_codec_comresp;
	addr = this->address;
	DPRINTF(("%s: information of codec[%d] follows:\n",
	    XNAME(this->az), addr));
	/* codec vendor/device/revision */
	err = this->comresp(this, CORB_NID_ROOT, CORB_GET_PARAMETER,
	    COP_REVISION_ID, &rev);
	if (err)
		return err;
	err = this->comresp(this, CORB_NID_ROOT, CORB_GET_PARAMETER,
	    COP_VENDOR_ID, &id);
	if (err)
		return err;
	this->vid = id;
	this->subid = this->az->subid;
	azalia_codec_init_vtbl(this);

	printf("%s: codec:", XNAME(this->az));
	if (this->name == NULL) {
		vendor = pci_findvendor(id >> 16);
		if (vendor == NULL)
			printf(" 0x%04x/0x%04x", id >> 16, id & 0xffff);
		else
			printf(" %s/0x%04x", vendor, id & 0xffff);
	} else
		printf(" %s", this->name);
	printf(" (rev. %u.%u), HDA version %u.%u\n",
	    COP_RID_REVISION(rev), COP_RID_STEPPING(rev),
	    COP_RID_MAJ(rev), COP_RID_MIN(rev));

	/* identify function nodes */
	err = this->comresp(this, CORB_NID_ROOT, CORB_GET_PARAMETER,
	    COP_SUBORDINATE_NODE_COUNT, &result);
	if (err)
		return err;
	this->nfunctions = COP_NSUBNODES(result);
	if (COP_NSUBNODES(result) <= 0) {
		printf("%s: No function groups\n", XNAME(this->az));
		return -1;
	}
	/* iterate function nodes and find an audio function */
	n = COP_START_NID(result);
	DPRINTF(("%s: nidstart=%d #functions=%d\n",
	    __func__, n, this->nfunctions));
	this->audiofunc = -1;
	for (i = 0; i < this->nfunctions; i++) {
		err = this->comresp(this, n + i, CORB_GET_PARAMETER,
		    COP_FUNCTION_GROUP_TYPE, &result);
		if (err)
			continue;
		DPRINTF(("%s: FTYPE result = 0x%8.8x\n", __func__, result));
		if (COP_FTYPE(result) == COP_FTYPE_AUDIO) {
			this->audiofunc = n + i;
			break;	/* XXX multiple audio functions? */
		} else if (COP_FTYPE(result) == COP_FTYPE_MODEM) {
			printf("%s: codec[%d]: No support for modem function groups\n",
			    XNAME(this->az), addr);
		}
	}
	if (this->audiofunc < 0) {
		printf("%s: codec[%d]: No audio function groups\n",
		    XNAME(this->az), addr);
		return -1;
	}

	/* power the audio function */
	this->comresp(this, this->audiofunc, CORB_SET_POWER_STATE, CORB_PS_D0, &result);
	DELAY(100);

	/* check widgets in the audio function */
	err = this->comresp(this, this->audiofunc,
	    CORB_GET_PARAMETER, COP_SUBORDINATE_NODE_COUNT, &result);
	if (err)
		return err;
	DPRINTF(("%s: There are %d widgets in the audio function.\n",
	   __func__, COP_NSUBNODES(result)));
	this->wstart = COP_START_NID(result);
	if (this->wstart < 2) {
		printf("%s: invalid node structure\n", XNAME(this->az));
		return -1;
	}
	this->wend = this->wstart + COP_NSUBNODES(result);
	this->w = malloc(sizeof(widget_t) * this->wend, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->w == NULL) {
		printf("%s: out of memory\n", XNAME(this->az));
		return ENOMEM;
	}

	/* query the base parameters */
	this->comresp(this, this->audiofunc, CORB_GET_PARAMETER,
	    COP_STREAM_FORMATS, &result);
	this->w[this->audiofunc].d.audio.encodings = result;
	this->comresp(this, this->audiofunc, CORB_GET_PARAMETER,
	    COP_PCM, &result);
	this->w[this->audiofunc].d.audio.bits_rates = result;
	this->comresp(this, this->audiofunc, CORB_GET_PARAMETER,
	    COP_INPUT_AMPCAP, &result);
	this->w[this->audiofunc].inamp_cap = result;
	this->comresp(this, this->audiofunc, CORB_GET_PARAMETER,
	    COP_OUTPUT_AMPCAP, &result);
	this->w[this->audiofunc].outamp_cap = result;
#ifdef AZALIA_DEBUG
	azalia_widget_print_audio(&this->w[this->audiofunc], "\t");
	result = this->w[this->audiofunc].inamp_cap;
	DPRINTF(("\tinamp: mute=%u size=%u steps=%u offset=%u\n",
	    (result & COP_AMPCAP_MUTE) != 0, COP_AMPCAP_STEPSIZE(result),
	    COP_AMPCAP_NUMSTEPS(result), COP_AMPCAP_OFFSET(result)));
	result = this->w[this->audiofunc].outamp_cap;
	DPRINTF(("\toutamp: mute=%u size=%u steps=%u offset=%u\n",
	    (result & COP_AMPCAP_MUTE) != 0, COP_AMPCAP_STEPSIZE(result),
	    COP_AMPCAP_NUMSTEPS(result), COP_AMPCAP_OFFSET(result)));
	this->comresp(this, this->audiofunc, CORB_GET_PARAMETER,
	    COP_GPIO_COUNT, &result);
	DPRINTF(("\tgpio: wake=%u unsol=%u gpis=%u gpos=%u gpios=%u\n",
	    (result & COP_GPIO_WAKE) != 0, (result & COP_GPIO_UNSOL) != 0,
	    COP_GPIO_GPIS(result), COP_GPIO_GPOS(result),
	    COP_GPIO_GPIOS(result)));
#endif

	strlcpy(this->w[CORB_NID_ROOT].name, "root",
	    sizeof(this->w[CORB_NID_ROOT].name));
	strlcpy(this->w[this->audiofunc].name, "hdaudio",
	    sizeof(this->w[this->audiofunc].name));
	FOR_EACH_WIDGET(this, i) {
		err = azalia_widget_init(&this->w[i], this, i);
		if (err)
			return err;
	}

	err = this->init_dacgroup(this);
	if (err)
		return err;
#ifdef AZALIA_DEBUG
	for (i = 0; i < this->dacs.ngroups; i++) {
		DPRINTF(("%s: dacgroup[%d]:", __func__, i));
		for (n = 0; n < this->dacs.groups[i].nconv; n++) {
			DPRINTF((" %2.2x", this->dacs.groups[i].conv[n]));
		}
		DPRINTF(("\n"));
	}
#endif

	/* set invalid values for azalia_codec_construct_format() to work */
	this->dacs.cur = -1;
	this->adcs.cur = -1;
	err = azalia_codec_construct_format(this, 0, 0);
	if (err)
		return err;

	return this->mixer_init(this);
}

int
azalia_codec_delete(codec_t *this)
{
	if (this->mixer_delete != NULL)
		this->mixer_delete(this);
	if (this->formats != NULL) {
		free(this->formats, M_DEVBUF);
		this->formats = NULL;
	}
	printf("delete_encodings...\n");
	auconv_delete_encodings(this->encodings);
	this->encodings = NULL;
	return 0;
}

int
azalia_codec_construct_format(codec_t *this, int newdac, int newadc)
{
	const convgroup_t *group;
	uint32_t bits_rates;
	int prev_dac, prev_adc;
	int pvariation, rvariation;
	int nbits, c, chan, i, err;
	nid_t nid;

	prev_dac = this->dacs.cur;
	this->dacs.cur = newdac;
	group = &this->dacs.groups[this->dacs.cur];
	bits_rates = this->w[group->conv[0]].d.audio.bits_rates;
	nbits = 0;
	if (bits_rates & COP_PCM_B8)
		nbits++;
	if (bits_rates & COP_PCM_B16)
		nbits++;
	if (bits_rates & COP_PCM_B20)
		nbits++;
	if (bits_rates & COP_PCM_B24)
		nbits++;
	if (bits_rates & COP_PCM_B32)
		nbits++;
	if (nbits == 0) {
		printf("%s: %s/%d invalid PCM format: 0x%8.8x\n",
		    XNAME(this->az), __FILE__, __LINE__, bits_rates);
		return -1;
	}
	pvariation = group->nconv * nbits;

	prev_adc = this->adcs.cur;
	this->adcs.cur = newadc;
	group = &this->adcs.groups[this->adcs.cur];
	bits_rates = this->w[group->conv[0]].d.audio.bits_rates;
	nbits = 0;
	if (bits_rates & COP_PCM_B8)
		nbits++;
	if (bits_rates & COP_PCM_B16)
		nbits++;
	if (bits_rates & COP_PCM_B20)
		nbits++;
	if (bits_rates & COP_PCM_B24)
		nbits++;
	if (bits_rates & COP_PCM_B32)
		nbits++;
	if (nbits == 0) {
		printf("%s: %s/%d invalid PCM format: 0x%8.8x\n",
		    XNAME(this->az), __FILE__, __LINE__, bits_rates);
		return -1;
	}
	rvariation = group->nconv * nbits;

	if (bits_rates & COP_PCM_R441)
		this->rate = 44100;
	else if (bits_rates & COP_PCM_R480)
		this->rate = 48000;
	else {
		printf("%s: %s/%d invalid PCM format: 0x%8.8x\n",
		    XNAME(this->az), __FILE__, __LINE__, bits_rates);
		return -1;
	}
	if (this->formats != NULL)
		free(this->formats, M_DEVBUF);
	this->nformats = 0;
	this->formats = malloc(sizeof(struct audio_format) *
	    (pvariation + rvariation), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->formats == NULL) {
		printf("%s: out of memory in %s\n",
		    XNAME(this->az), __func__);
		return ENOMEM;
	}

	/* register formats for playback */
	group = &this->dacs.groups[this->dacs.cur];
	nid = group->conv[0];
	chan = 0;
	bits_rates = this->w[nid].d.audio.bits_rates;
	for (c = 0; c < group->nconv; c++) {
		for (chan = 0, i = 0; i <= c; i++)
			chan += WIDGET_CHANNELS(&this->w[group->conv[c]]);
		azalia_codec_add_bits(this, chan, bits_rates, AUMODE_PLAY);
	}

	/* register formats for recording */
	group = &this->adcs.groups[this->adcs.cur];
	nid = group->conv[0];
	chan = 0;
	bits_rates = this->w[nid].d.audio.bits_rates;
	for (c = 0; c < group->nconv; c++) {
		for (chan = 0, i = 0; i <= c; i++)
			chan += WIDGET_CHANNELS(&this->w[group->conv[c]]);
		azalia_codec_add_bits(this, chan, bits_rates, AUMODE_RECORD);
	}

	err = azalia_create_encodings(this->formats, this->nformats,
	    &this->encodings);
	if (err)
		return err;
	return 0;
}

void
azalia_codec_add_bits(codec_t *this, int chan, uint32_t bits_rates, int mode)
{
	if (bits_rates & COP_PCM_B8)
		azalia_codec_add_format(this, chan, 8, 16, bits_rates, mode);
	if (bits_rates & COP_PCM_B16)
		azalia_codec_add_format(this, chan, 16, 16, bits_rates, mode);
	if (bits_rates & COP_PCM_B20)
		azalia_codec_add_format(this, chan, 20, 32, bits_rates, mode);
	if (bits_rates & COP_PCM_B24)
		azalia_codec_add_format(this, chan, 24, 32, bits_rates, mode);
	if (bits_rates & COP_PCM_B32)
		azalia_codec_add_format(this, chan, 32, 32, bits_rates, mode);
}

void
azalia_codec_add_format(codec_t *this, int chan, int valid, int prec,
    uint32_t rates, int32_t mode)
{
	struct audio_format *f;

	f = &this->formats[this->nformats++];
	f->mode = mode;
	f->encoding = AUDIO_ENCODING_SLINEAR_LE;
	if (valid == 8 && prec == 8)
		f->encoding = AUDIO_ENCODING_ULINEAR_LE;
	f->validbits = valid;
	f->precision = prec;
	f->channels = chan;
	switch (chan) {
	case 1:
		f->channel_mask = AUFMT_MONAURAL;
		break;
	case 2:
		f->channel_mask = AUFMT_STEREO;
		break;
	case 4:
		f->channel_mask = AUFMT_SURROUND4;
		break;
	case 6:
		f->channel_mask = AUFMT_DOLBY_5_1;
		break;
	case 8:
		f->channel_mask = AUFMT_DOLBY_5_1
		    | AUFMT_SIDE_LEFT | AUFMT_SIDE_RIGHT;
		break;
	default:
		f->channel_mask = 0;
	}
	if (rates & COP_PCM_R80)
		f->frequency[f->frequency_type++] = 8000;
	if (rates & COP_PCM_R110)
		f->frequency[f->frequency_type++] = 11025;
	if (rates & COP_PCM_R160)
		f->frequency[f->frequency_type++] = 16000;
	if (rates & COP_PCM_R220)
		f->frequency[f->frequency_type++] = 22050;
	if (rates & COP_PCM_R320)
		f->frequency[f->frequency_type++] = 32000;
	if (rates & COP_PCM_R441)
		f->frequency[f->frequency_type++] = 44100;
	if (rates & COP_PCM_R480)
		f->frequency[f->frequency_type++] = 48000;
	if (rates & COP_PCM_R882)
		f->frequency[f->frequency_type++] = 88200;
	if (rates & COP_PCM_R960)
		f->frequency[f->frequency_type++] = 96000;
	if (rates & COP_PCM_R1764)
		f->frequency[f->frequency_type++] = 176400;
	if (rates & COP_PCM_R1920)
		f->frequency[f->frequency_type++] = 192000;
	if (rates & COP_PCM_R3840)
		f->frequency[f->frequency_type++] = 384000;
}

int
azalia_codec_comresp(const codec_t *codec, nid_t nid, uint32_t control,
		     uint32_t param, uint32_t* result)
{
	int err;

	err = azalia_set_command(codec->az, codec->address, nid, control, param);
	if (err)
		return err;
	return azalia_get_response(codec->az, result);
}

int
azalia_codec_connect_stream(codec_t *this, int dir, uint16_t fmt, int number)
{
	const convgroup_t *group;
	int i, err, startchan, nchan;
	nid_t nid;
	boolean_t flag222;

	DPRINTF(("%s: fmt=0x%4.4x number=%d\n", __func__, fmt, number));
	err = 0;
	if (dir == AUMODE_RECORD)
		group = &this->adcs.groups[this->adcs.cur];
	else
		group = &this->dacs.groups[this->dacs.cur];
	flag222 = group->nconv >= 3 &&
	    (WIDGET_CHANNELS(&this->w[group->conv[0]]) == 2) &&
	    (WIDGET_CHANNELS(&this->w[group->conv[1]]) == 2) &&
	    (WIDGET_CHANNELS(&this->w[group->conv[2]]) == 2);
	nchan = (fmt & HDA_SD_FMT_CHAN) + 1;
	startchan = 0;
	for (i = 0; i < group->nconv; i++) {
		nid = group->conv[i];

		/* surround and c/lfe handling */
		if (nchan >= 6 && flag222 && i == 1) {
			nid = group->conv[2];
		} else if (nchan >= 6 && flag222 && i == 2) {
			nid = group->conv[1];
		}

		err = this->comresp(this, nid, CORB_SET_CONVERTER_FORMAT, fmt, NULL);
		if (err)
			goto exit;
		err = this->comresp(this, nid, CORB_SET_CONVERTER_STREAM_CHANNEL,
				    (number << 4) | startchan, NULL);
		if (err)
			goto exit;
		if (nchan > 2)
			startchan += WIDGET_CHANNELS(&this->w[nid]);
	}

exit:
	DPRINTF(("%s: leave with %d\n", __func__, err));
	return err;
}

/* ================================================================
 * HDA widget functions
 * ================================================================ */

#define	WIDGETCAP_BITS							\
    "\20\014LRSWAP\013POWER\012DIGITAL"					\
    "\011CONNLIST\010UNSOL\07PROC\06STRIPE\05FORMATOV\04AMPOV\03OUTAMP"	\
    "\02INAMP\01STEREO"

int
azalia_widget_init(widget_t *this, const codec_t *codec, nid_t nid)
{
	uint32_t result;
	int err;

	err = codec->comresp(codec, nid, CORB_GET_PARAMETER,
	    COP_AUDIO_WIDGET_CAP, &result);
	if (err)
		return err;
	this->nid = nid;
	this->widgetcap = result;
	this->type = COP_AWCAP_TYPE(result);
	DPRINTF(("%s: ", XNAME(codec->az)));
	if (this->widgetcap & COP_AWCAP_POWER) {
		codec->comresp(codec, nid, CORB_SET_POWER_STATE, CORB_PS_D0, &result);
		DELAY(100);
	}
	switch (this->type) {
	case COP_AWTYPE_AUDIO_OUTPUT:
		snprintf(this->name, sizeof(this->name), "dac%2.2x", nid);
		DPRINTF(("%s wcap=%b\n", this->name,
		    this->widgetcap, WIDGETCAP_BITS));
		azalia_widget_init_audio(this, codec);
		break;
	case COP_AWTYPE_AUDIO_INPUT:
		snprintf(this->name, sizeof(this->name), "adc%2.2x", nid);
		DPRINTF(("%s wcap=%b\n", this->name,
		    this->widgetcap, WIDGETCAP_BITS));
		azalia_widget_init_audio(this, codec);
		break;
	case COP_AWTYPE_AUDIO_MIXER:
		snprintf(this->name, sizeof(this->name), "mix%2.2x", nid);
		DPRINTF(("%s wcap=%b\n", this->name,
		    this->widgetcap, WIDGETCAP_BITS));
		break;
	case COP_AWTYPE_AUDIO_SELECTOR:
		snprintf(this->name, sizeof(this->name), "sel%2.2x", nid);
		DPRINTF(("%s wcap=%b\n", this->name,
		    this->widgetcap, WIDGETCAP_BITS));
		break;
	case COP_AWTYPE_PIN_COMPLEX:
		azalia_widget_init_pin(this, codec);
		snprintf(this->name, sizeof(this->name), "%s%2.2x",
		    pin_colors[this->d.pin.color], nid);
		DPRINTF(("%s wcap=%b\n", this->name,
		    this->widgetcap, WIDGETCAP_BITS));
		azalia_widget_print_pin(this);
		break;
	case COP_AWTYPE_POWER:
		snprintf(this->name, sizeof(this->name), "pow%2.2x", nid);
		DPRINTF(("%s wcap=%b\n", this->name,
		    this->widgetcap, WIDGETCAP_BITS));
		break;
	case COP_AWTYPE_VOLUME_KNOB:
		snprintf(this->name, sizeof(this->name), "volume%2.2x", nid);
		DPRINTF(("%s wcap=%b\n", this->name,
		    this->widgetcap, WIDGETCAP_BITS));
		err = codec->comresp(codec, nid, CORB_GET_PARAMETER,
		    COP_VOLUME_KNOB_CAPABILITIES, &result);
		if (!err) {
			this->d.volume.cap = result;
			DPRINTF(("\tdelta=%d steps=%d\n",
			    !!(result & COP_VKCAP_DELTA),
			    COP_VKCAP_NUMSTEPS(result)));
		}
		break;
	case COP_AWTYPE_BEEP_GENERATOR:
		snprintf(this->name, sizeof(this->name), "beep%2.2x", nid);
		DPRINTF(("%s wcap=%b\n", this->name,
		    this->widgetcap, WIDGETCAP_BITS));
		break;
	default:
		snprintf(this->name, sizeof(this->name), "widget%2.2x", nid);
		DPRINTF(("%s wcap=%b\n", this->name,
		    this->widgetcap, WIDGETCAP_BITS));
		break;
	}
	azalia_widget_init_connection(this, codec);

	/* amplifier information */
	if (this->widgetcap & COP_AWCAP_INAMP) {
		if (this->widgetcap & COP_AWCAP_AMPOV)
			codec->comresp(codec, nid, CORB_GET_PARAMETER,
			    COP_INPUT_AMPCAP, &this->inamp_cap);
		else
			this->inamp_cap = codec->w[codec->audiofunc].inamp_cap;
		if ((this->widgetcap & COP_AWCAP_AMPOV) ||
		    (this->nid == codec->audiofunc))
			DPRINTF(("\tinamp: mute=%u size=%u steps=%u offset=%u\n",
			    (this->inamp_cap & COP_AMPCAP_MUTE) != 0,
			    COP_AMPCAP_STEPSIZE(this->inamp_cap),
			    COP_AMPCAP_NUMSTEPS(this->inamp_cap),
			    COP_AMPCAP_OFFSET(this->inamp_cap)));
	}
	if (this->widgetcap & COP_AWCAP_OUTAMP) {
		if (this->widgetcap & COP_AWCAP_AMPOV)
			codec->comresp(codec, nid, CORB_GET_PARAMETER,
			    COP_OUTPUT_AMPCAP, &this->outamp_cap);
		else
			this->outamp_cap = codec->w[codec->audiofunc].outamp_cap;
		if ((this->widgetcap & COP_AWCAP_AMPOV) ||
		    (this->nid == codec->audiofunc))
			DPRINTF(("\toutamp: mute=%u size=%u steps=%u offset=%u\n",
			    (this->outamp_cap & COP_AMPCAP_MUTE) != 0,
			    COP_AMPCAP_STEPSIZE(this->outamp_cap),
			    COP_AMPCAP_NUMSTEPS(this->outamp_cap),
			    COP_AMPCAP_OFFSET(this->outamp_cap)));
	}
	if (codec->init_widget != NULL)
		codec->init_widget(codec, this, nid);
	return 0;
}

int
azalia_widget_init_audio(widget_t *this, const codec_t *codec)
{
	uint32_t result;
	int err;

	/* check audio format */
	if (this->widgetcap & COP_AWCAP_FORMATOV) {
		err = codec->comresp(codec, this->nid,
		    CORB_GET_PARAMETER, COP_STREAM_FORMATS, &result);
		if (err)
			return err;
		this->d.audio.encodings = result;
		if (result == 0) { /* quirk for CMI9880.
				    * This must not occuur usually... */
			this->d.audio.encodings =
			    codec->w[codec->audiofunc].d.audio.encodings;
			this->d.audio.bits_rates =
			    codec->w[codec->audiofunc].d.audio.bits_rates;
		} else {
			if ((result & COP_STREAM_FORMAT_PCM) == 0) {
				printf("%s: %s: No PCM support: %x\n",
				    XNAME(codec->az), this->name, result);
				return -1;
			}
			err = codec->comresp(codec, this->nid, CORB_GET_PARAMETER,
			    COP_PCM, &result);
			if (err)
				return err;
			this->d.audio.bits_rates = result;
		}
	} else {
		this->d.audio.encodings =
		    codec->w[codec->audiofunc].d.audio.encodings;
		this->d.audio.bits_rates =
		    codec->w[codec->audiofunc].d.audio.bits_rates;
	}
#ifdef AZALIA_DEBUG
	if (this->widgetcap & COP_AWCAP_FORMATOV)
		azalia_widget_print_audio(this, "\t");
#endif
	return 0;
}

#define	ENCODING_BITS	"\20\3AC3\2FLOAT32\1PCM"
#define	BITSRATES_BITS	"\20\x15""32bit\x14""24bit\x13""20bit"		\
    "\x12""16bit\x11""8bit""\x0c""384kHz\x0b""192kHz\x0a""176.4kHz"	\
    "\x09""96kHz\x08""88.2kHz\x07""48kHz\x06""44.1kHz\x05""32kHz\x04"	\
    "22.05kHz\x03""16kHz\x02""11.025kHz\x01""8kHz"

int
azalia_widget_print_audio(const widget_t *this, const char *lead)
{
	printf("%sencodings=%b\n", lead, this->d.audio.encodings,
	    ENCODING_BITS);
	printf("%sPCM formats=%b\n", lead, this->d.audio.bits_rates,
	    BITSRATES_BITS);
	return 0;
}

int
azalia_widget_init_pin(widget_t *this, const codec_t *codec)
{
	uint32_t result;
	int err;

	err = codec->comresp(codec, this->nid, CORB_GET_CONFIGURATION_DEFAULT,
	    0, &result);
	if (err)
		return err;
	this->d.pin.config = result;
	this->d.pin.sequence = CORB_CD_SEQUENCE(result);
	this->d.pin.association = CORB_CD_ASSOCIATION(result);
	this->d.pin.color = CORB_CD_COLOR(result);
	this->d.pin.device = CORB_CD_DEVICE(result);

	err = codec->comresp(codec, this->nid, CORB_GET_PARAMETER,
	    COP_PINCAP, &result);
	if (err)
		return err;
	this->d.pin.cap = result;

	/* input pin */
	if ((this->d.pin.cap & COP_PINCAP_INPUT) &&
	    (this->d.pin.cap & COP_PINCAP_OUTPUT) == 0) {
		err = codec->comresp(codec, this->nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err == 0) {
			result &= ~CORB_PWC_OUTPUT;
			result |= CORB_PWC_INPUT;
			codec->comresp(codec, this->nid,
			    CORB_SET_PIN_WIDGET_CONTROL, result, NULL);
		}
	}
	/* output pin, or bidirectional pin */
	if (this->d.pin.cap & COP_PINCAP_OUTPUT) {
		err = codec->comresp(codec, this->nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err == 0) {
			result &= ~CORB_PWC_INPUT;
			result |= CORB_PWC_OUTPUT;
			codec->comresp(codec, this->nid,
			    CORB_SET_PIN_WIDGET_CONTROL, result, NULL);
		}
	}
	return 0;
}

#define	PINCAP_BITS	"\20\021EAPD\16VREF100\15VREF80" \
    "\13VREFGND\12VREF50\11VREFHIZ\07BALANCE\06INPUT" \
    "\05OUTPUT\04HEADPHONE\03PRESENCE\02TRIGGER\01IMPEDANCE"

int
azalia_widget_print_pin(const widget_t *this)
{
	DPRINTF(("\tcap=%b\n", this->d.pin.cap, PINCAP_BITS));
	DPRINTF(("\t[%2.2d/%2.2d] ", CORB_CD_ASSOCIATION(this->d.pin.config),
	    CORB_CD_SEQUENCE(this->d.pin.config)));
	DPRINTF(("color=%s ", pin_colors[CORB_CD_COLOR(this->d.pin.config)]));
	DPRINTF(("device=%s ", pin_devices[CORB_CD_DEVICE(this->d.pin.config)]));
	DPRINTF(("conn=%s ", pin_conn[CORB_CD_PORT(this->d.pin.config)]));
	DPRINTF(("conntype=%s\n", pin_conntype[CORB_CD_CONNECTION(this->d.pin.config)]));
	DPRINTF(("\tlocation=%s ", pin_geo[CORB_CD_LOC_GEO(this->d.pin.config)]));
	DPRINTF(("chassis=%s ", pin_chass[CORB_CD_LOC_CHASS(this->d.pin.config)]));
	DPRINTF(("special="));
	if (CORB_CD_LOC_GEO(this->d.pin.config) == CORB_CD_LOC_SPEC0) {
		if (CORB_CD_LOC_CHASS(this->d.pin.config) == CORB_CD_EXTERNAL)
			DPRINTF(("rear-panel"));
		else if (CORB_CD_LOC_CHASS(this->d.pin.config) == CORB_CD_INTERNAL)
			DPRINTF(("riser"));
		else if (CORB_CD_LOC_CHASS(this->d.pin.config) == CORB_CD_LOC_OTHER)
			DPRINTF(("mobile-lid-internal"));
	} else if (CORB_CD_LOC_GEO(this->d.pin.config) == CORB_CD_LOC_SPEC1) {
		if (CORB_CD_LOC_CHASS(this->d.pin.config) == CORB_CD_EXTERNAL)
			DPRINTF(("drive-bay"));
		else if (CORB_CD_LOC_CHASS(this->d.pin.config) == CORB_CD_INTERNAL)
			DPRINTF(("hdmi"));
		else if (CORB_CD_LOC_CHASS(this->d.pin.config) == CORB_CD_LOC_OTHER)
			DPRINTF(("mobile-lid-external"));
	} else if (CORB_CD_LOC_GEO(this->d.pin.config) == CORB_CD_LOC_SPEC2) {
		if (CORB_CD_LOC_CHASS(this->d.pin.config) == CORB_CD_INTERNAL)
			DPRINTF(("atapi"));
	} else
		DPRINTF(("none"));
	DPRINTF(("\n"));

	return 0;
}

int
azalia_widget_init_connection(widget_t *this, const codec_t *codec)
{
	uint32_t result;
	int err;
	boolean_t longform;
	int length, i;

	this->selected = -1;
	if ((this->widgetcap & COP_AWCAP_CONNLIST) == 0)
		return 0;

	err = codec->comresp(codec, this->nid, CORB_GET_PARAMETER,
	    COP_CONNECTION_LIST_LENGTH, &result);
	if (err)
		return err;
	longform = (result & COP_CLL_LONG) != 0;
	length = COP_CLL_LENGTH(result);
	if (length == 0)
		return 0;
	this->nconnections = length;
	this->connections = malloc(sizeof(nid_t) * (length + 3),
	    M_DEVBUF, M_NOWAIT);
	if (this->connections == NULL) {
		printf("%s: out of memory\n", XNAME(codec->az));
		return ENOMEM;
	}
	if (longform) {
		for (i = 0; i < length;) {
			err = codec->comresp(codec, this->nid,
			    CORB_GET_CONNECTION_LIST_ENTRY, i, &result);
			if (err)
				return err;
			this->connections[i++] = CORB_CLE_LONG_0(result);
			this->connections[i++] = CORB_CLE_LONG_1(result);
		}
	} else {
		for (i = 0; i < length;) {
			err = codec->comresp(codec, this->nid,
			    CORB_GET_CONNECTION_LIST_ENTRY, i, &result);
			if (err)
				return err;
			this->connections[i++] = CORB_CLE_SHORT_0(result);
			this->connections[i++] = CORB_CLE_SHORT_1(result);
			this->connections[i++] = CORB_CLE_SHORT_2(result);
			this->connections[i++] = CORB_CLE_SHORT_3(result);
		}
	}
	if (length > 0) {
		DPRINTF(("\tconnections=0x%x", this->connections[0]));
		for (i = 1; i < length; i++) {
			DPRINTF((",0x%x", this->connections[i]));
		}

		err = codec->comresp(codec, this->nid,
		    CORB_GET_CONNECTION_SELECT_CONTROL, 0, &result);
		if (err)
			return err;
		this->selected = CORB_CSC_INDEX(result);
		DPRINTF(("; selected=0x%x\n", this->connections[result]));
	}
	return 0;
}

/* ================================================================
 * Stream functions
 * ================================================================ */

int
azalia_stream_init(stream_t *this, azalia_t *az, int regindex, int strnum, int dir)
{
	int err;

	this->az = az;
	this->regbase = HDA_SD_BASE + regindex * HDA_SD_SIZE;
	this->intr_bit = 1 << regindex;
	this->number = strnum;
	this->dir = dir;

	/* setup BDL buffers */
	err = azalia_alloc_dmamem(az, sizeof(bdlist_entry_t) * HDA_BDL_MAX,
				  128, &this->bdlist);
	if (err) {
		printf("%s: can't allocate a BDL buffer\n", XNAME(az));
		return err;
	}
	return 0;
}

int
azalia_stream_delete(stream_t *this, azalia_t *az)
{
	if (this->bdlist.addr == NULL)
		return 0;
	azalia_free_dmamem(az, &this->bdlist);
	return 0;
}

int
azalia_stream_reset(stream_t *this)
{
	int i;
	uint16_t ctl;
	uint8_t sts;

	/* Make sure RUN bit is zero before resetting */
	ctl = STR_READ_2(this, CTL);
	ctl &= ~HDA_SD_CTL_RUN;
	STR_WRITE_2(this, CTL, ctl);
	DELAY(40);

	/* Start reset and wait for chip to enter. */
	ctl = STR_READ_2(this, CTL);
	STR_WRITE_2(this, CTL, ctl | HDA_SD_CTL_SRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		ctl = STR_READ_2(this, CTL);
		if (ctl & HDA_SD_CTL_SRST)
			break;
	}
	if (i <= 0) {
		printf("%s: stream reset failure 1\n", XNAME(this->az));
		return -1;
	}

	/* Clear reset and wait for chip to finish */
	STR_WRITE_2(this, CTL, ctl & ~HDA_SD_CTL_SRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		ctl = STR_READ_2(this, CTL);
		if ((ctl & HDA_SD_CTL_SRST) == 0)
			break;
	}
	if (i <= 0) {
		printf("%s: stream reset failure 2\n", XNAME(this->az));
		return -1;
	}

	sts = STR_READ_1(this, STS);
	sts |= HDA_SD_STS_DESE | HDA_SD_STS_FIFOE | HDA_SD_STS_BCIS;
	STR_WRITE_1(this, STS, sts);

	return (0);
}

int
azalia_stream_start(stream_t *this, void *start, void *end, int blk,
    void (*intr)(void *), void *arg, uint16_t fmt)
{
	bdlist_entry_t *bdlist;
	bus_addr_t dmaaddr, dmaend;
	int err, index;
	uint32_t intctl;
	uint8_t ctl2;

	this->intr = intr;
	this->intr_arg = arg;

	err = azalia_stream_reset(this);
	if (err) {
		printf("%s: stream reset failed\n", "azalia");
		return err;
	}

	STR_WRITE_4(this, BDPL, 0);
	STR_WRITE_4(this, BDPU, 0);

	/* setup BDL */
	dmaaddr = AZALIA_DMA_DMAADDR(&this->buffer);
	dmaend = dmaaddr + ((caddr_t)end - (caddr_t)start);
	bdlist = (bdlist_entry_t*)this->bdlist.addr;
	for (index = 0; index < HDA_BDL_MAX; index++) {
		bdlist[index].low = htole32(dmaaddr);
		bdlist[index].high = htole32(PTR_UPPER32(dmaaddr));
		bdlist[index].length = htole32(blk);
		bdlist[index].flags = htole32(BDLIST_ENTRY_IOC);
		dmaaddr += blk;
		if (dmaaddr >= dmaend) {
			index++;
			break;
		}
	}

	dmaaddr = AZALIA_DMA_DMAADDR(&this->bdlist);
	STR_WRITE_4(this, BDPL, dmaaddr);
	STR_WRITE_4(this, BDPU, PTR_UPPER32(dmaaddr));
	STR_WRITE_2(this, LVI, (index - 1) & HDA_SD_LVI_LVI);
	ctl2 = STR_READ_1(this, CTL2);
	STR_WRITE_1(this, CTL2,
	    (ctl2 & ~HDA_SD_CTL2_STRM) | (this->number << HDA_SD_CTL2_STRM_SHIFT));
	STR_WRITE_4(this, CBL, ((caddr_t)end - (caddr_t)start));
	STR_WRITE_2(this, FMT, fmt);

	err = azalia_codec_connect_stream(&this->az->codecs[this->az->codecno],
	    this->dir, fmt, this->number);
	if (err)
		return EINVAL;

	intctl = AZ_READ_4(this->az, INTCTL);
	intctl |= this->intr_bit;
	AZ_WRITE_4(this->az, INTCTL, intctl);

	STR_WRITE_1(this, CTL, STR_READ_1(this, CTL) |
	    HDA_SD_CTL_DEIE | HDA_SD_CTL_FEIE | HDA_SD_CTL_IOCE |
	    HDA_SD_CTL_RUN);

	return (0);
}

int
azalia_stream_halt(stream_t *this)
{
	uint16_t ctl;

	ctl = STR_READ_2(this, CTL);
	ctl &= ~(HDA_SD_CTL_DEIE | HDA_SD_CTL_FEIE | HDA_SD_CTL_IOCE | HDA_SD_CTL_RUN);
	STR_WRITE_2(this, CTL, ctl);
	AZ_WRITE_4(this->az, INTCTL,
	    AZ_READ_4(this->az, INTCTL) & ~this->intr_bit);
	return (0);
}

#define	HDA_SD_STS_BITS	"\20\3BCIS\4FIFOE\5DESE\6FIFORDY"

int
azalia_stream_intr(stream_t *this, uint32_t intsts)
{
	u_int8_t sts;

	if ((intsts & this->intr_bit) == 0)
		return (0);

	sts = STR_READ_1(this, STS);
	STR_WRITE_1(this, STS, sts |
	    HDA_SD_STS_DESE | HDA_SD_STS_FIFOE | HDA_SD_STS_BCIS);

	if (sts & (HDA_SD_STS_DESE | HDA_SD_STS_FIFOE))
		printf("%s: stream %d: sts=%b\n", XNAME(this->az),
		    this->number, sts, HDA_SD_STS_BITS);
	if (sts & HDA_SD_STS_BCIS)
		this->intr(this->intr_arg);
	return (1);
}

/* ================================================================
 * MI audio entries
 * ================================================================ */

int
azalia_open(void *v, int flags)
{
	azalia_t *az;
	codec_t *codec;

	DPRINTF(("%s: flags=0x%x\n", __func__, flags));
	az = v;
	codec = &az->codecs[az->codecno];
	codec->running++;
	return 0;
}

void
azalia_close(void *v)
{
	azalia_t *az;
	codec_t *codec;

	DPRINTF(("%s\n", __func__));
	az = v;
	codec = &az->codecs[az->codecno];
	codec->running--;
}

int
azalia_query_encoding(void *v, audio_encoding_t *enc)
{
	azalia_t *az;
	codec_t *codec;
	int i, j;

	az = v;
	codec = &az->codecs[az->codecno];
	for (j = 0, i = 0; j < codec->nformats; j++) {
		if (codec->formats[j].validbits !=
		    codec->formats[j].precision)
			continue;
		if (i == enc->index) {
			enc->encoding = codec->formats[j].encoding;
			enc->precision = codec->formats[j].precision;
			switch (enc->encoding) {
			case AUDIO_ENCODING_SLINEAR_LE:
				strlcpy(enc->name, enc->precision == 8 ?
				    AudioEslinear : AudioEslinear_le,
				    sizeof enc->name);
				break;
			case AUDIO_ENCODING_ULINEAR_LE:
				strlcpy(enc->name, enc->precision == 8 ?
				    AudioEulinear : AudioEulinear_le,
				    sizeof enc->name);
				break;
			default:
				strlcpy(enc->name, "unknown", sizeof enc->name);
				break;
			}
			return (0);
		}
		i++;
	}
	return (EINVAL);
}

int
azalia_set_params(void *v, int smode, int umode, audio_params_t *p,
    audio_params_t *r)
{
	azalia_t *az;
	codec_t *codec;
	void (*pswcode)(void *, u_char *, int) = NULL;
	void (*rswcode)(void *, u_char *, int) = NULL;
	int i, j;

	az = v;
	codec = &az->codecs[az->codecno];
	if (smode & AUMODE_RECORD && r != NULL) {
		if (r->encoding == AUDIO_ENCODING_ULAW) {	 /*XXX*/
			r->encoding = AUDIO_ENCODING_SLINEAR_LE;
			r->precision = 16;
			r->sample_rate = codec->rate;
		}
		for (i = 0; i < codec->nformats; i++) {
			if (r->encoding != codec->formats[i].encoding)
				continue;
			if (r->precision != codec->formats[i].precision)
				continue;
			if (r->channels != codec->formats[i].channels)
				continue;
			break;
		}
		/* find a 2 channel format and emulate mono */
		if (i == codec->nformats && r->channels == 1) {
			r->factor = 2;
			rswcode = linear16_decimator;
			for (i = 0; i < codec->nformats; i++) {
				if (r->encoding != codec->formats[i].encoding)
					continue;
				if (r->precision != codec->formats[i].precision)
					continue;
				if (codec->formats[i].channels != 2)
					continue;
				break;
			}
		}

		if (i == codec->nformats) {
			DPRINTF(("%s: can't find record format %u/%u/%u\n",
			    __func__, r->encoding, r->precision, r->channels));
			return (EINVAL);
		}
		for (j = 0; j < codec->formats[i].frequency_type; j++) {
			if (r->sample_rate != codec->formats[i].frequency[j])
				continue;
			break;
		}
		if (j == codec->formats[i].frequency_type) {
			DPRINTF(("%s: can't find record rate %u\n",
			    __func__, r->sample_rate));
			return (EINVAL);
		}
		r->sw_code = rswcode;
	}
	if (smode & AUMODE_PLAY && p != NULL) {
		if (p->encoding == AUDIO_ENCODING_ULAW) {	 /*XXX*/
			p->encoding = AUDIO_ENCODING_SLINEAR_LE;
			p->precision = 16;
			p->sample_rate = codec->rate;
		}
		for (i = 0; i < codec->nformats; i++) {
			if (p->encoding != codec->formats[i].encoding)
				continue;
			if (p->precision != codec->formats[i].precision)
				continue;
			if (p->channels != codec->formats[i].channels)
				continue;
			break;
		}
		/* find a 2 channel format and emulate mono */
		if (i == codec->nformats && p->channels == 1) {
			p->factor = 2;
			pswcode = noswap_bytes_mts;
			for (i = 0; i < codec->nformats; i++) {
				if (p->encoding != codec->formats[i].encoding)
					continue;
				if (p->precision != codec->formats[i].precision)
					continue;
				if (codec->formats[i].channels != 2)
					continue;
				break;
			}
		}
		if (i == codec->nformats) {
			DPRINTF(("%s: can't find playback format %u/%u/%u\n",
			    __func__, p->encoding, p->precision, p->channels));
			return (EINVAL);
		}
		for (j = 0; j < codec->formats[i].frequency_type; j++) {
			if (p->sample_rate != codec->formats[i].frequency[j])
				continue;
			break;
		}
		if (j == codec->formats[i].frequency_type) {
			DPRINTF(("%s: can't find playback rate %u\n",
			    __func__, p->sample_rate));
			return (EINVAL);
		}
		p->sw_code = pswcode;
	}

	return (0);
}

int
azalia_round_blocksize(void *v, int blk)
{
	azalia_t *az;
	size_t size;

	blk &= ~0x7f;		/* must be multiple of 128 */
	if (blk <= 0)
		blk = 128;
	/* number of blocks must be <= HDA_BDL_MAX */
	az = v;
	size = az->pstream.buffer.size;
#ifdef DIAGNOSTIC
	if (size <= 0) {
		printf("%s: size is 0", __func__);
		return 256;
	}
#endif
	if (size > HDA_BDL_MAX * blk) {
		blk = size / HDA_BDL_MAX;
		if (blk & 0x7f)
			blk = (blk + 0x7f) & ~0x7f;
	}
	DPRINTF(("%s: resultant block size = %d\n", __func__, blk));
	return blk;
}

int
azalia_halt_output(void *v)
{
	azalia_t *az;

	DPRINTF(("%s\n", __func__));
	az = v;
	return azalia_stream_halt(&az->pstream);
}

int
azalia_halt_input(void *v)
{
	azalia_t *az;

	DPRINTF(("%s\n", __func__));
	az = v;
	return azalia_stream_halt(&az->rstream);
}

int
azalia_getdev(void *v, struct audio_device *dev)
{
	azalia_t *az;

	az = v;
	strlcpy(dev->name, "HD-Audio", MAX_AUDIO_DEV_LEN);
	snprintf(dev->version, MAX_AUDIO_DEV_LEN,
	    "%d.%d", AZ_READ_1(az, VMAJ), AZ_READ_1(az, VMIN));
	strlcpy(dev->config, XNAME(az), MAX_AUDIO_DEV_LEN);
	return 0;
}

int
azalia_set_port(void *v, mixer_ctrl_t *mc)
{
	azalia_t *az;
	codec_t *co;

	az = v;
	co = &az->codecs[az->codecno];
	return co->set_port(co, mc);
}

int
azalia_get_port(void *v, mixer_ctrl_t *mc)
{
	azalia_t *az;
	codec_t *co;

	az = v;
	co = &az->codecs[az->codecno];
	return co->get_port(co, mc);
}

int
azalia_query_devinfo(void *v, mixer_devinfo_t *mdev)
{
	azalia_t *az;
	const codec_t *co;

	az = v;
	co = &az->codecs[az->codecno];
	if (mdev->index >= co->nmixers)
		return ENXIO;
	*mdev = co->mixers[mdev->index].devinfo;
	return 0;
}

void *
azalia_allocm(void *v, int dir, size_t size, int pool, int flags)
{
	azalia_t *az;
	stream_t *stream;
	int err;

	az = v;
	stream = dir == AUMODE_PLAY ? &az->pstream : &az->rstream;
	err = azalia_alloc_dmamem(az, size, 128, &stream->buffer);
	if (err) {
		printf("%s: allocm failed\n", az->dev.dv_xname);
		return NULL;
	}
	return stream->buffer.addr;
}

void
azalia_freem(void *v, void *addr, int pool)
{
	azalia_t *az;
	stream_t *stream;

	az = v;
	if (addr == az->pstream.buffer.addr) {
		stream = &az->pstream;
	} else if (addr == az->rstream.buffer.addr) {
		stream = &az->rstream;
	} else {
		return;
	}
	azalia_free_dmamem(az, &stream->buffer);
}

size_t
azalia_round_buffersize(void *v, int dir, size_t size)
{
	size &= ~0x7f;		/* must be multiple of 128 */
	if (size <= 0)
		size = 128;
	return size;
}

int
azalia_get_props(void *v)
{
	return AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

int
azalia_trigger_output(void *v, void *start, void *end, int blk,
    void (*intr)(void *), void *arg, audio_params_t *param)
{
	azalia_t *az;
	int err;
	uint16_t fmt;

	err = azalia_params2fmt(param, &fmt);
	if (err)
		return EINVAL;

	az = v;
	return azalia_stream_start(&az->pstream, start, end, blk, intr, arg, fmt);
}

int
azalia_trigger_input(void *v, void *start, void *end, int blk,
    void (*intr)(void *), void *arg, audio_params_t *param)
{
	azalia_t *az;
	int err;
	uint16_t fmt;

	DPRINTF(("%s: this=%p start=%p end=%p blk=%d {enc=%u %uch %u/%ubit %uHz}\n",
	    __func__, v, start, end, blk, param->encoding, param->channels,
	    param->precision, param->precision, param->sample_rate));

	err = azalia_params2fmt(param, &fmt);
	if (err)
		return EINVAL;

	az = v;
	return azalia_stream_start(&az->rstream, start, end, blk, intr, arg, fmt);
}

/* --------------------------------
 * helpers for MI audio functions
 * -------------------------------- */
int
azalia_params2fmt(const audio_params_t *param, uint16_t *fmt)
{
	uint16_t ret;

	ret = 0;
#ifdef DIAGNOSTIC
	if (param->channels > HDA_MAX_CHANNELS) {
		printf("%s: too many channels: %u\n", __func__,
		    param->channels);
		return EINVAL;
	}
#endif
	/* allow any number of channels in a native format */
	if (param->sw_code == NULL)
		ret |= param->channels - 1;

	/* emulated mono uses 2 channel formats */
	else
		ret |= 1;

	switch (param->precision) {
	case 8:
		ret |= HDA_SD_FMT_BITS_8_16;
		break;
	case 16:
		ret |= HDA_SD_FMT_BITS_16_16;
		break;
	case 32:
		ret |= HDA_SD_FMT_BITS_32_32;
		break;
	}

#if 0
	switch (param->validbits) {
	case 8:
		ret |= HDA_SD_FMT_BITS_8_16;
		break;
	case 16:
		ret |= HDA_SD_FMT_BITS_16_16;
		break;
	case 20:
		ret |= HDA_SD_FMT_BITS_20_32;
		break;
	case 24:
		ret |= HDA_SD_FMT_BITS_24_32;
		break;
	case 32:
		ret |= HDA_SD_FMT_BITS_32_32;
		break;
	default:
		printf("%s: invalid validbits: %u\n", __func__,
		    param->validbits);
	}
#endif

	if (param->sample_rate == 384000) {
		printf("%s: invalid sample_rate: %u\n", __func__,
		    param->sample_rate);
		return EINVAL;
	} else if (param->sample_rate == 192000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X4 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 176400) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X4 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 96000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X2 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 88200) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X2 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 48000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 44100) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 32000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X2 | HDA_SD_FMT_DIV_BY3;
	} else if (param->sample_rate == 22050) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY2;
	} else if (param->sample_rate == 16000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY3;
	} else if (param->sample_rate == 11025) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY4;
	} else if (param->sample_rate == 8000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY6;
	} else {
		printf("%s: invalid sample_rate: %u\n", __func__,
		    param->sample_rate);
		return EINVAL;
	}
	*fmt = ret;
	return 0;
}

int
azalia_create_encodings(struct audio_format *formats, int nformats,
    struct audio_encoding_set **encodings)
{
#if 0
	int i;
	u_int j;

	for (i = 0; i < nformats; i++) {
		printf("format(%d): encoding %u vbits %u prec %u chans %u cmask 0x%x\n",
		    i, formats[i].encoding, formats[i].validbits,
		    formats[i].precision, formats[i].channels,
		    formats[i].channel_mask);
		printf("format(%d) rates:", i);
		for (j = 0; j < formats[i].frequency_type; j++) {
			printf(" %u", formats[i].frequency[j]);
		}
		printf("\n");
	}
#endif
	return (0);
}
