/*	$OpenBSD: auvia.c,v 1.29 2004/12/07 03:17:42 jsg Exp $ */
/*	$NetBSD: auvia.c,v 1.7 2000/11/15 21:06:33 jdolecek Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tyler C. Sarna
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * VIA Technologies VT82C686A Southbridge Audio Driver
 *
 * Documentation links:
 *
 * ftp://ftp.alsa-project.org/pub/manuals/via/686a.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/audioio.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97.h>

#include <dev/pci/auviavar.h>

struct auvia_dma {
	struct auvia_dma *next;
	caddr_t addr;
	size_t size;
	bus_dmamap_t map;
	bus_dma_segment_t seg;
};

struct auvia_dma_op {
	u_int32_t ptr;
	u_int32_t flags;
#define AUVIA_DMAOP_EOL		0x80000000
#define AUVIA_DMAOP_FLAG	0x40000000
#define AUVIA_DMAOP_STOP	0x20000000
#define AUVIA_DMAOP_COUNT(x)	((x)&0x00FFFFFF)
};

/* rev. H and later seem to support only fixed rate 44.1 kHz */
#define	AUVIA_FIXED_RATE	44100

int	auvia_match(struct device *, void *, void *);
void	auvia_attach(struct device *, struct device *, void *);
int	auvia_open(void *, int);
void	auvia_close(void *);
int	auvia_query_encoding(void *addr, struct audio_encoding *fp);
int	auvia_set_params(void *, int, int, struct audio_params *,
	struct audio_params *);
int	auvia_round_blocksize(void *, int);
int	auvia_halt_output(void *);
int	auvia_halt_input(void *);
int	auvia_getdev(void *, struct audio_device *);
int	auvia_set_port(void *, mixer_ctrl_t *);
int	auvia_get_port(void *, mixer_ctrl_t *);
int	auvia_query_devinfo(void *, mixer_devinfo_t *);
void *	auvia_malloc(void *, int, size_t, int, int);
void	auvia_free(void *, void *, int);
size_t	auvia_round_buffersize(void *, int, size_t);
paddr_t	auvia_mappage(void *, void *, off_t, int);
int	auvia_get_props(void *);
int	auvia_build_dma_ops(struct auvia_softc *, struct auvia_softc_chan *,
	struct auvia_dma *, void *, void *, int);
int	auvia_trigger_output(void *, void *, void *, int, void (*)(void *),
	void *, struct audio_params *);
int	auvia_trigger_input(void *, void *, void *, int, void (*)(void *),
	void *, struct audio_params *);

int	auvia_intr(void *);

struct  cfdriver auvia_cd = {
	NULL, "auvia", DV_DULL
};

struct cfattach auvia_ca = {
	sizeof (struct auvia_softc), auvia_match, auvia_attach
};

#define AUVIA_PCICONF_JUNK	0x40
#define		AUVIA_PCICONF_ENABLES	 0x00FF0000	/* reg 42 mask */
#define		AUVIA_PCICONF_ACLINKENAB 0x00008000	/* ac link enab */
#define		AUVIA_PCICONF_ACNOTRST	 0x00004000	/* ~(ac reset) */
#define		AUVIA_PCICONF_ACSYNC	 0x00002000	/* ac sync */
#define		AUVIA_PCICONF_ACVSR	 0x00000800	/* var. samp. rate */
#define		AUVIA_PCICONF_ACSGD	 0x00000400	/* SGD enab */
#define		AUVIA_PCICONF_ACFM	 0x00000200	/* FM enab */
#define		AUVIA_PCICONF_ACSB	 0x00000100	/* SB enab */
#define		AUVIA_PCICONF_PRIVALID	 0x00000001	/* primary codec rdy */

#define AUVIA_PLAY_BASE			0x00
#define AUVIA_RECORD_BASE		0x10

#define	AUVIA_RP_STAT			0x00
#define		AUVIA_RPSTAT_INTR		0x03
#define AUVIA_RP_CONTROL		0x01
#define		AUVIA_RPCTRL_START		0x80
#define		AUVIA_RPCTRL_TERMINATE		0x40
#define		AUVIA_RPCTRL_AUTOSTART		0x20
/* The following are 8233 specific */
#define		AUVIA_RPCTRL_STOP		0x04
#define		AUVIA_RPCTRL_EOL		0x02
#define		AUVIA_RPCTRL_FLAG		0x01
#define AUVIA_RP_MODE			0x02
#define		AUVIA_RPMODE_INTR_FLAG		0x01
#define		AUVIA_RPMODE_INTR_EOL		0x02
#define		AUVIA_RPMODE_STEREO		0x10
#define		AUVIA_RPMODE_16BIT		0x20
#define		AUVIA_RPMODE_AUTOSTART		0x80
#define	AUVIA_RP_DMAOPS_BASE		0x04

#define	VIA8233_RP_DXS_LVOL		0x02
#define	VIA8233_RP_DXS_RVOL		0x03
#define	VIA8233_RP_RATEFMT		0x08
#define		VIA8233_RATEFMT_48K	0xfffff
#define		VIA8233_RATEFMT_STEREO	0x00100000
#define		VIA8233_RATEFMT_16BIT	0x00200000

#define VIA_RP_DMAOPS_COUNT		0x0C

#define	AUVIA_CODEC_CTL			0x80
#define		AUVIA_CODEC_READ		0x00800000
#define		AUVIA_CODEC_BUSY		0x01000000
#define		AUVIA_CODEC_PRIVALID		0x02000000
#define		AUVIA_CODEC_INDEX(x)		((x)<<16)

#define TIMEOUT	50

struct audio_hw_if auvia_hw_if = {
	auvia_open,
	auvia_close,
	NULL, /* drain */
	auvia_query_encoding,
	auvia_set_params,
	auvia_round_blocksize,
	NULL, /* commit_settings */
	NULL, /* init_output */
	NULL, /* init_input */
	NULL, /* start_output */
	NULL, /* start_input */
	auvia_halt_output,
	auvia_halt_input,
	NULL, /* speaker_ctl */
	auvia_getdev,
	NULL, /* setfd */
	auvia_set_port,
	auvia_get_port,
	auvia_query_devinfo,
	auvia_malloc,
	auvia_free,
	auvia_round_buffersize,
	auvia_mappage,
	auvia_get_props,
	auvia_trigger_output,
	auvia_trigger_input
};

int	auvia_attach_codec(void *, struct ac97_codec_if *);
int	auvia_write_codec(void *, u_int8_t, u_int16_t);
int	auvia_read_codec(void *, u_int8_t, u_int16_t *);
void	auvia_reset_codec(void *);
int	auvia_waitready_codec(struct auvia_softc *sc);
int	auvia_waitvalid_codec(struct auvia_softc *sc);

const struct pci_matchid auvia_devices[] = {
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT82C686A_AC97 },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8233_AC97 },
};

int
auvia_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, auvia_devices,
	    sizeof(auvia_devices)/sizeof(auvia_devices[0])));
}


void
auvia_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct auvia_softc *sc = (struct auvia_softc *) self;
	const char *intrstr = NULL;
	struct mixer_ctrl ctl;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t pt = pa->pa_tag;
	pci_intr_handle_t ih;
	bus_size_t iosize;
	pcireg_t pr;
	int r, i;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_VIATECH_VT8233_AC97)
		sc->sc_flags |= AUVIA_FLAGS_VT8233;

	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_IO, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &iosize, 0)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pc = pc;
	sc->sc_pt = pt;

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}
	intrstr = pci_intr_string(pc, ih);

	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, auvia_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}

	printf(": %s\n", intrstr);

	/* disable SBPro compat & others */
	pr = pci_conf_read(pc, pt, AUVIA_PCICONF_JUNK);

	pr &= ~AUVIA_PCICONF_ENABLES; /* clear compat function enables */
	/* XXX what to do about MIDI, FM, joystick? */

	pr |= (AUVIA_PCICONF_ACLINKENAB | AUVIA_PCICONF_ACNOTRST |
	    AUVIA_PCICONF_ACVSR | AUVIA_PCICONF_ACSGD);

	pr &= ~(AUVIA_PCICONF_ACFM | AUVIA_PCICONF_ACSB);

	pci_conf_write(pc, pt, AUVIA_PCICONF_JUNK, pr);

	sc->host_if.arg = sc;
	sc->host_if.attach = auvia_attach_codec;
	sc->host_if.read = auvia_read_codec;
	sc->host_if.write = auvia_write_codec;
	sc->host_if.reset = auvia_reset_codec;

	if ((r = ac97_attach(&sc->host_if)) != 0) {
		printf("%s: can't attach codec (error 0x%X)\n",
		    sc->sc_dev.dv_xname, r);
		pci_intr_disestablish(pc, sc->sc_ih);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}

	/* disable mutes */
	for (i = 0; i < 4; i++) {
		static struct {
			char *class, *device;
		} d[] = {
			{ AudioCoutputs, AudioNmaster},
			{ AudioCinputs, AudioNdac},
			{ AudioCinputs, AudioNcd},
			{ AudioCrecord, AudioNvolume},
		};

		ctl.type = AUDIO_MIXER_ENUM;
		ctl.un.ord = 0;

		ctl.dev = sc->codec_if->vtbl->get_portnum_by_name(sc->codec_if,
		    d[i].class, d[i].device, AudioNmute);
		auvia_set_port(sc, &ctl);
	}

	/* set a reasonable default volume */

	ctl.type = AUDIO_MIXER_VALUE;
	ctl.un.value.num_channels = 2;
	ctl.un.value.level[AUDIO_MIXER_LEVEL_LEFT] = \
	ctl.un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = 199;

	ctl.dev = sc->codec_if->vtbl->get_portnum_by_name(sc->codec_if,
	    AudioCoutputs, AudioNmaster, NULL);
	auvia_set_port(sc, &ctl);

	audio_attach_mi(&auvia_hw_if, sc, &sc->sc_dev);
}


int
auvia_attach_codec(void *addr, struct ac97_codec_if *cif)
{
	struct auvia_softc *sc = addr;

	sc->codec_if = cif;

	return 0;
}


void
auvia_reset_codec(void *addr)
{
	int i;
	struct auvia_softc *sc = addr;
	pcireg_t r;

	/* perform a codec cold reset */

	r = pci_conf_read(sc->sc_pc, sc->sc_pt, AUVIA_PCICONF_JUNK);

	r &= ~AUVIA_PCICONF_ACNOTRST;	/* enable RESET (active low) */
	pci_conf_write(sc->sc_pc, sc->sc_pt, AUVIA_PCICONF_JUNK, r);
	delay(2);

	r |= AUVIA_PCICONF_ACNOTRST;	/* disable RESET (inactive high) */
	pci_conf_write(sc->sc_pc, sc->sc_pt, AUVIA_PCICONF_JUNK, r);
	delay(200);

	for (i = 500000; i != 0 && !(pci_conf_read(sc->sc_pc, sc->sc_pt,
		AUVIA_PCICONF_JUNK) & AUVIA_PCICONF_PRIVALID); i--)
		DELAY(1);
	if (i == 0)
		printf("%s: codec reset timed out\n", sc->sc_dev.dv_xname);
}


int
auvia_waitready_codec(struct auvia_softc *sc)
{
	int i;

	/* poll until codec not busy */
	for (i = 0; (i < TIMEOUT) && (bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	     AUVIA_CODEC_CTL) & AUVIA_CODEC_BUSY); i++)
		delay(1);

	if (i >= TIMEOUT) {
		printf("%s: codec busy\n", sc->sc_dev.dv_xname);
		return 1;
	}

	return 0;
}


int
auvia_waitvalid_codec(struct auvia_softc *sc)
{
	int i;

	/* poll until codec valid */
	for (i = 0; (i < TIMEOUT) && !(bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	     AUVIA_CODEC_CTL) & AUVIA_CODEC_PRIVALID); i++)
		delay(1);

	if (i >= TIMEOUT) {
		printf("%s: codec invalid\n", sc->sc_dev.dv_xname);
		return 1;
	}

	return 0;
}


int
auvia_write_codec(void *addr, u_int8_t reg, u_int16_t val)
{
	struct auvia_softc *sc = addr;

	if (auvia_waitready_codec(sc))
		return 1;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, AUVIA_CODEC_CTL,
	    AUVIA_CODEC_PRIVALID | AUVIA_CODEC_INDEX(reg) | val);

	return 0;
}


int
auvia_read_codec(void *addr, u_int8_t reg, u_int16_t *val)
{
	struct auvia_softc *sc = addr;

	if (auvia_waitready_codec(sc))
		return 1;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, AUVIA_CODEC_CTL,
	    AUVIA_CODEC_PRIVALID | AUVIA_CODEC_READ | AUVIA_CODEC_INDEX(reg));

	if (auvia_waitready_codec(sc))
		return 1;

	if (auvia_waitvalid_codec(sc))
		return 1;

	*val = bus_space_read_2(sc->sc_iot, sc->sc_ioh, AUVIA_CODEC_CTL);

	return 0;
}


int
auvia_open(void *addr, int flags)
{
	return 0;
}


void
auvia_close(void *addr)
{
	struct auvia_softc *sc = addr;

	auvia_halt_output(sc);
	auvia_halt_input(sc);

	sc->sc_play.sc_intr = NULL;
	sc->sc_record.sc_intr = NULL;
}


int
auvia_query_encoding(void *addr, struct audio_encoding *fp)
{
	switch (fp->index) {
	case 0:
		strlcpy(fp->name, AudioEulinear, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return (0);
	case 1:
		strlcpy(fp->name, AudioEmulaw, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 2:
		strlcpy(fp->name, AudioEalaw, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 3:
		strlcpy(fp->name, AudioEslinear, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 4:
		strlcpy(fp->name, AudioEslinear_le, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return (0);
	case 5:
		strlcpy(fp->name, AudioEulinear_le, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 6:
		strlcpy(fp->name, AudioEslinear_be, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 7:
		strlcpy(fp->name, AudioEulinear_be, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	default:
		return (EINVAL);
	}
}


int
auvia_set_params(void *addr, int setmode, int usemode,
    struct audio_params *play, struct audio_params *rec)
{
	struct auvia_softc *sc = addr;
	struct audio_params *p;
	u_int16_t regval;
	int mode, base;

	/* for mode in (RECORD, PLAY) */
	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		if (mode == AUMODE_PLAY) {
			p = play;
			base = AUVIA_PLAY_BASE;
		} else {
			p = rec;
			base = AUVIA_RECORD_BASE;
		}

		if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
			u_int32_t v = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    base + VIA8233_RP_RATEFMT) & ~(VIA8233_RATEFMT_48K
			    | VIA8233_RATEFMT_STEREO | VIA8233_RATEFMT_16BIT);

			v |= VIA8233_RATEFMT_48K *
			    (p->sample_rate / 20) / (48000 / 20);

			if (p->channels == 2)
				v |= VIA8233_RATEFMT_STEREO;
			if (p->precision == 16)
				v |= VIA8233_RATEFMT_16BIT;

			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    base + VIA8233_RP_RATEFMT, v);
		}

		if (ac97_set_rate(sc->codec_if, p, mode))
			return (EINVAL);

		if ((p->precision != 8 && p->precision != 16) ||
		    (p->channels != 1 && p->channels != 2))
			return (EINVAL);

		p->factor = 1;
		p->sw_code = 0;
		switch (p->encoding) {
		case AUDIO_ENCODING_SLINEAR_BE:
			if (p->precision == 16)
				p->sw_code = swap_bytes;
			else
				p->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (p->precision != 16)
				p->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (p->precision == 16)
				p->sw_code = mode == AUMODE_PLAY?
				    swap_bytes_change_sign16_le :
				    change_sign16_swap_bytes_le;
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (p->precision == 16)
				p->sw_code = change_sign16_le;
			break;
		case AUDIO_ENCODING_ULAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = mulaw_to_slinear16_le;
			} else
				p->sw_code = ulinear8_to_mulaw;
			break;
		case AUDIO_ENCODING_ALAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = alaw_to_slinear16_le;
			} else
				p->sw_code = ulinear8_to_alaw;
			break;
		default:
			return (EINVAL);
		}

		regval = (p->channels == 2 ? AUVIA_RPMODE_STEREO : 0)
			| (p->precision * p->factor == 16 ?
				AUVIA_RPMODE_16BIT : 0)
			| AUVIA_RPMODE_INTR_FLAG | AUVIA_RPMODE_INTR_EOL
			| AUVIA_RPMODE_AUTOSTART;

		if (mode == AUMODE_PLAY)
			sc->sc_play.sc_reg = regval;
		else
			sc->sc_record.sc_reg = regval;
	}

	return 0;
}


int
auvia_round_blocksize(void *addr, int blk)
{
	return (blk & -32);
}


int
auvia_halt_output(void *addr)
{
	struct auvia_softc *sc = addr;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AUVIA_PLAY_BASE + AUVIA_RP_CONTROL, AUVIA_RPCTRL_TERMINATE);

	return 0;
}


int
auvia_halt_input(void *addr)
{
	struct auvia_softc *sc = addr;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AUVIA_RECORD_BASE + AUVIA_RP_CONTROL, AUVIA_RPCTRL_TERMINATE);

	return 0;
}


int
auvia_getdev(void *addr, struct audio_device *retp)
{
	struct auvia_softc *sc = addr;

	if (retp) {
		strncpy(retp->name,
		    sc->sc_flags & AUVIA_FLAGS_VT8233? "VIA VT8233" :
		    "VIA VT82C686A", sizeof(retp->name));
		strncpy(retp->version, sc->sc_revision, sizeof(retp->version));
		strncpy(retp->config, "auvia", sizeof(retp->config));
	}

	return 0;
}


int
auvia_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct auvia_softc *sc = addr;

	return (sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp));
}


int
auvia_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct auvia_softc *sc = addr;

	return (sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp));
}


int
auvia_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct auvia_softc *sc = addr;

	return (sc->codec_if->vtbl->query_devinfo(sc->codec_if, dip));
}


void *
auvia_malloc(void *addr, int direction, size_t size, int pool, int flags)
{
	struct auvia_softc *sc = addr;
	struct auvia_dma *p;
	int error;
	int rseg;

	p = malloc(sizeof(*p), pool, flags);
	if (!p)
		return 0;

	p->size = size;
	if ((error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &p->seg,
	    1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate dma, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_alloc;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &p->seg, rseg, size, &p->addr,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map dma, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_map;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &p->map)) != 0) {
		printf("%s: unable to create dma map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_create;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, p->map, p->addr, size, NULL,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load dma map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_load;
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;

	return p->addr;


fail_load:
	bus_dmamap_destroy(sc->sc_dmat, p->map);
fail_create:
	bus_dmamem_unmap(sc->sc_dmat, p->addr, size);
fail_map:
	bus_dmamem_free(sc->sc_dmat, &p->seg, 1);
fail_alloc:
	free(p, pool);
	return 0;
}


void
auvia_free(void *addr, void *ptr, int pool)
{
	struct auvia_softc *sc = addr;
	struct auvia_dma **pp, *p;

	for (pp = &(sc->sc_dmas); (p = *pp) != NULL; pp = &p->next)
		if (p->addr == ptr) {
			bus_dmamap_unload(sc->sc_dmat, p->map);
			bus_dmamap_destroy(sc->sc_dmat, p->map);
			bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
			bus_dmamem_free(sc->sc_dmat, &p->seg, 1);

			*pp = p->next;
			free(p, pool);
			return;
		}

	panic("auvia_free: trying to free unallocated memory");
}


size_t
auvia_round_buffersize(void *addr, int direction, size_t size)
{
	return size;
}


paddr_t
auvia_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct auvia_softc *sc = addr;
	struct auvia_dma *p;

	if (off < 0)
		return -1;

	for (p = sc->sc_dmas; p && p->addr != mem; p = p->next)
		;

	if (!p)
		return -1;

	return bus_dmamem_mmap(sc->sc_dmat, &p->seg, 1, off, prot,
	    BUS_DMA_WAITOK);
}


int
auvia_get_props(void *addr)
{
	return (AUDIO_PROP_MMAP |  AUDIO_PROP_INDEPENDENT |
	    AUDIO_PROP_FULLDUPLEX);
}


int
auvia_build_dma_ops(struct auvia_softc *sc, struct auvia_softc_chan *ch,
    struct auvia_dma *p, void *start, void *end, int blksize)
{
	struct auvia_dma_op *op;
	struct auvia_dma *dp;
	bus_addr_t s;
	size_t l;
	int segs;

	s = p->map->dm_segs[0].ds_addr;
	l = ((char *)end - (char *)start);
	segs = (l + blksize - 1) / blksize;

	if (segs > (ch->sc_dma_op_count)) {
		/* if old list was too small, free it */
		if (ch->sc_dma_ops)
			auvia_free(sc, ch->sc_dma_ops, M_DEVBUF);

		ch->sc_dma_ops = auvia_malloc(sc, 0,
		    sizeof(struct auvia_dma_op) * segs, M_DEVBUF, M_WAITOK);

		for (dp = sc->sc_dmas; dp &&
		     dp->addr != (void *)(ch->sc_dma_ops); dp = dp->next)
			;

		if (!dp)
			panic("%s: build_dma_ops: where'd my memory go??? "
			    "address (%p)", sc->sc_dev.dv_xname,
			    ch->sc_dma_ops);

		ch->sc_dma_op_count = segs;
		ch->sc_dma_ops_dma = dp;
	}

	dp = ch->sc_dma_ops_dma;
	op = ch->sc_dma_ops;

	while (l) {
		op->ptr = htole32(s);
		l = l - blksize;
		/* if last block */
		op->flags = htole32((l? AUVIA_DMAOP_FLAG : AUVIA_DMAOP_EOL) | blksize);
		s += blksize;
		op++;
	}

	return 0;
}


int
auvia_trigger_output(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, struct audio_params *param)
{
	struct auvia_softc *sc = addr;
	struct auvia_softc_chan *ch = &(sc->sc_play);
	struct auvia_dma *p;

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		;

	if (!p)
		panic("auvia_trigger_output: request with bad start "
		    "address (%p)", start);

	if (auvia_build_dma_ops(sc, ch, p, start, end, blksize)) {
		return 1;
	}

	ch->sc_intr = intr;
	ch->sc_arg = arg;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
	    AUVIA_PLAY_BASE + AUVIA_RP_DMAOPS_BASE,
	    ch->sc_dma_ops_dma->map->dm_segs[0].ds_addr);

	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AUVIA_PLAY_BASE + AUVIA_RP_MODE, ch->sc_reg);

	if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_PLAY_BASE + VIA8233_RP_DXS_LVOL, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_PLAY_BASE + VIA8233_RP_DXS_RVOL, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_PLAY_BASE + AUVIA_RP_CONTROL,
		    AUVIA_RPCTRL_START | AUVIA_RPCTRL_AUTOSTART |
		    AUVIA_RPCTRL_STOP  | AUVIA_RPCTRL_EOL | AUVIA_RPCTRL_FLAG);
	} else
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_PLAY_BASE + AUVIA_RP_CONTROL, AUVIA_RPCTRL_START);

	return 0;
}


int
auvia_trigger_input(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, struct audio_params *param)
{
	struct auvia_softc *sc = addr;
	struct auvia_softc_chan *ch = &(sc->sc_record);
	struct auvia_dma *p;

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		;

	if (!p)
		panic("auvia_trigger_input: request with bad start "
		    "address (%p)", start);

	if (auvia_build_dma_ops(sc, ch, p, start, end, blksize))
		return 1;

	ch->sc_intr = intr;
	ch->sc_arg = arg;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
	    AUVIA_RECORD_BASE + AUVIA_RP_DMAOPS_BASE,
	    ch->sc_dma_ops_dma->map->dm_segs[0].ds_addr);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AUVIA_RECORD_BASE + AUVIA_RP_MODE, ch->sc_reg);

	if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_RECORD_BASE + VIA8233_RP_DXS_LVOL, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_RECORD_BASE + VIA8233_RP_DXS_RVOL, 0);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_RECORD_BASE + AUVIA_RP_CONTROL,
		    AUVIA_RPCTRL_START | AUVIA_RPCTRL_AUTOSTART |
		    AUVIA_RPCTRL_STOP  | AUVIA_RPCTRL_EOL | AUVIA_RPCTRL_FLAG);
	} else
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_RECORD_BASE + AUVIA_RP_CONTROL, AUVIA_RPCTRL_START);

	return 0;
}


int
auvia_intr(void *arg)
{
	struct auvia_softc *sc = arg;
	u_int8_t r;
	int i = 0;

	r = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
	    AUVIA_RECORD_BASE + AUVIA_RP_STAT);
	if (r & AUVIA_RPSTAT_INTR) {
		if (sc->sc_record.sc_intr)
			sc->sc_record.sc_intr(sc->sc_record.sc_arg);

		/* clear interrupts */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_RECORD_BASE + AUVIA_RP_STAT, AUVIA_RPSTAT_INTR);

		i++;
	}
	r = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
	    AUVIA_PLAY_BASE + AUVIA_RP_STAT);
	if (r & AUVIA_RPSTAT_INTR) {
		if (sc->sc_play.sc_intr)
			sc->sc_play.sc_intr(sc->sc_play.sc_arg);

		/* clear interrupts */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    AUVIA_PLAY_BASE + AUVIA_RP_STAT, AUVIA_RPSTAT_INTR);

		i++;
	}

	return (i? 1 : 0);
}
