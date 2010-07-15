/*	$OpenBSD: fms.c,v 1.22 2010/07/15 03:43:11 jakemsr Exp $ */
/*	$NetBSD: fms.c,v 1.5.4.1 2000/06/30 16:27:50 simonb Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Witold J. Wnuk.
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
 * Forte Media FM801 Audio Device Driver
 */

#include "radio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/audioio.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97.h>
#if 0
#include <dev/ic/mpuvar.h>
#endif

#include <dev/pci/fmsreg.h>
#include <dev/pci/fmsvar.h>


struct fms_dma {
	struct fms_dma *next;
	caddr_t addr;
	size_t size;
	bus_dmamap_t map;
	bus_dma_segment_t seg;
};



int	fms_match(struct device *, void *, void *);
void	fms_attach(struct device *, struct device *, void *);
int	fms_intr(void *);

int	fms_open(void *, int);
void	fms_close(void *);
int	fms_query_encoding(void *, struct audio_encoding *);
int	fms_set_params(void *, int, int, struct audio_params *, 
			    struct audio_params *);
void	fms_get_default_params(void *, int, struct audio_params *);
int	fms_round_blocksize(void *, int);
int	fms_halt_output(void *);
int	fms_halt_input(void *);
int	fms_getdev(void *, struct audio_device *);
int	fms_set_port(void *, mixer_ctrl_t *);
int	fms_get_port(void *, mixer_ctrl_t *);
int	fms_query_devinfo(void *, mixer_devinfo_t *);
void	*fms_malloc(void *, int, size_t, int, int);
void	fms_free(void *, void *, int);
paddr_t	fms_mappage(void *, void *, off_t, int);
int	fms_get_props(void *);
int	fms_trigger_output(void *, void *, void *, int, void (*)(void *),
			   void *, struct audio_params *);
int	fms_trigger_input(void *, void *, void *, int, void (*)(void *),
			  void *, struct audio_params *);

struct  cfdriver fms_cd = {
	NULL, "fms", DV_DULL
};

struct cfattach fms_ca = {
	sizeof (struct fms_softc), fms_match, fms_attach
};

struct audio_device fms_device = {
	"Forte Media 801",
	"1.0",
	"fms"
};


struct audio_hw_if fms_hw_if = {
	fms_open,
	fms_close,
	NULL,
	fms_query_encoding,
	fms_set_params,
	fms_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	fms_halt_output,
	fms_halt_input,
	NULL,
	fms_getdev,
	NULL,
	fms_set_port,
	fms_get_port,
	fms_query_devinfo,
	fms_malloc,
	fms_free,
	NULL,
	fms_mappage,
	fms_get_props,
	fms_trigger_output,
	fms_trigger_input,
	fms_get_default_params
};

int	fms_attach_codec(void *, struct ac97_codec_if *);
int	fms_read_codec(void *, u_int8_t, u_int16_t *);
int	fms_write_codec(void *, u_int8_t, u_int16_t);
void	fms_reset_codec(void *);

int	fms_allocmem(struct fms_softc *, size_t, size_t,
			  struct fms_dma *);
int	fms_freemem(struct fms_softc *, struct fms_dma *);

int
fms_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_FORTEMEDIA &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_FORTEMEDIA_FM801)
		return (1);
	return (0);
}

void
fms_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct fms_softc *sc = (struct fms_softc *) self;
	struct audio_attach_args aa;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t pt = pa->pa_tag;
	pci_intr_handle_t ih;
	bus_size_t iosize;
	const char *intrstr;
	u_int16_t k1;
	int i;
	
	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_IO, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &iosize, 0)) {
		printf(": can't map i/o space\n");
		return;
	}
	
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, 0x30, 2,
	    &sc->sc_mpu_ioh)) {
		printf(": can't get mpu subregion handle\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}

	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, 0x68, 4,
	    &sc->sc_opl_ioh)) {
		printf(": can't get opl subregion handle\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}
	
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, fms_intr, sc,
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

	sc->sc_dmat = pa->pa_dmat;

	/* Disable legacy audio (SBPro compatibility) */
	pci_conf_write(pc, pt, 0x40, 0);
	
	/* Reset codec and AC'97 */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CTL, 0x0020);
	delay(2);		/* > 1us according to AC'97 documentation */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CTL, 0x0000);
	delay(1);		/* > 168.2ns according to AC'97 documentation */
	
	/* Set up volume */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_PCM_VOLUME, 0x0808);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_FM_VOLUME, 0x0808);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_I2S_VOLUME, 0x0808);
	
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_RECORD_SOURCE, 0x0000);
	
	/* Unmask playback, record and mpu interrupts, mask the rest */
	k1 = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_INTMASK);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_INTMASK, 
	    (k1 & ~(FM_INTMASK_PLAY | FM_INTMASK_REC | FM_INTMASK_MPU)) |
	     FM_INTMASK_VOL);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_INTSTATUS, 
	    FM_INTSTATUS_PLAY | FM_INTSTATUS_REC | FM_INTSTATUS_MPU | 
	    FM_INTSTATUS_VOL);

#if NRADIO > 0
	fmsradio_attach(sc);
#endif /* NRADIO > 0 */
	
	sc->host_if.arg = sc;
	sc->host_if.attach = fms_attach_codec;
	sc->host_if.read = fms_read_codec;
	sc->host_if.write = fms_write_codec;
	sc->host_if.reset = fms_reset_codec;

	if (ac97_attach(&sc->host_if) != 0)
		return;
	
	/* Turn mute off */
	for (i = 0; i < 3; i++) {
		static struct {
			char *class, *device;
		} d[] = {
			{ AudioCoutputs, AudioNmaster },
			{ AudioCinputs, AudioNdac },
			{ AudioCrecord, AudioNvolume }
		};
		struct mixer_ctrl ctl;
		
		ctl.type = AUDIO_MIXER_ENUM;
		ctl.un.ord = 0;
		ctl.dev = sc->codec_if->vtbl->get_portnum_by_name(sc->codec_if,
			d[i].class, d[i].device, AudioNmute);
		fms_set_port(sc, &ctl);
	}

	audio_attach_mi(&fms_hw_if, sc, &sc->sc_dev);

	aa.type = AUDIODEV_TYPE_OPL;
	aa.hwif = NULL;
	aa.hdl = NULL;
	config_found(&sc->sc_dev, &aa, audioprint);

	aa.type = AUDIODEV_TYPE_MPU;
	aa.hwif = NULL;
	aa.hdl = NULL;
	sc->sc_mpu_dev = config_found(&sc->sc_dev, &aa, audioprint);
}

/*
 * Each AC-link frame takes 20.8us, data should be ready in next frame,
 * we allow more than two.
 */
#define TIMO 50
int
fms_read_codec(addr, reg, val)
	void *addr;
	u_int8_t reg;
	u_int16_t *val;
{
	struct fms_softc *sc = addr;
	int i;

	/* Poll until codec is ready */
	for (i = 0; i < TIMO && bus_space_read_2(sc->sc_iot, sc->sc_ioh, 
		 FM_CODEC_CMD) & FM_CODEC_CMD_BUSY; i++)
		delay(1);
	if (i >= TIMO) {
		printf("fms: codec busy\n");
		return 1;
	}

	/* Write register index, read access */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CMD, 
			  reg | FM_CODEC_CMD_READ);
	
	/* Poll until we have valid data */
	for (i = 0; i < TIMO && !(bus_space_read_2(sc->sc_iot, sc->sc_ioh, 
		 FM_CODEC_CMD) & FM_CODEC_CMD_VALID); i++)
		delay(1);
	if (i >= TIMO) {
		printf("fms: no data from codec\n");
		return 1;
	}
	
	/* Read data */
	*val = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_DATA);
	return 0;
}

int
fms_write_codec(addr, reg, val)
	void *addr;
	u_int8_t reg;
	u_int16_t val;
{
	struct fms_softc *sc = addr;
	int i;
	
	/* Poll until codec is ready */
	for (i = 0; i < TIMO && bus_space_read_2(sc->sc_iot, sc->sc_ioh, 
		 FM_CODEC_CMD) & FM_CODEC_CMD_BUSY; i++)
		delay(1);
	if (i >= TIMO) {
		printf("fms: codec busy\n");
		return 1;
	}

	/* Write data */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_DATA, val);
	/* Write index register, write access */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CMD, reg);
	return 0;
}
#undef TIMO

int
fms_attach_codec(addr, cif)
	void *addr;
	struct ac97_codec_if *cif;
{
	struct fms_softc *sc = addr;

	sc->codec_if = cif;
	return 0;
}

/* Cold Reset */
void
fms_reset_codec(addr)
	void *addr;
{
	struct fms_softc *sc = addr;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CTL, 0x0020);
	delay(2);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CTL, 0x0000);
	delay(1);
}

int
fms_intr(arg)
	void *arg;
{
	struct fms_softc *sc = arg;
	u_int16_t istat;
	
	istat = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_INTSTATUS);

	if (istat & FM_INTSTATUS_PLAY) {
		if ((sc->sc_play_nextblk += sc->sc_play_blksize) >= 
		     sc->sc_play_end)
			sc->sc_play_nextblk = sc->sc_play_start;

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, 
		    sc->sc_play_flip++ & 1 ? 
		    FM_PLAY_DMABUF2 : FM_PLAY_DMABUF1, sc->sc_play_nextblk);

		if (sc->sc_pintr)
			sc->sc_pintr(sc->sc_parg);
		else
			printf("unexpected play intr\n");
	}

	if (istat & FM_INTSTATUS_REC) {
		if ((sc->sc_rec_nextblk += sc->sc_rec_blksize) >= 
		     sc->sc_rec_end)
			sc->sc_rec_nextblk = sc->sc_rec_start;

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, 
		    sc->sc_rec_flip++ & 1 ? 
		    FM_REC_DMABUF2 : FM_REC_DMABUF1, sc->sc_rec_nextblk);

		if (sc->sc_rintr)
			sc->sc_rintr(sc->sc_rarg);
		else
			printf("unexpected rec intr\n");
	}
	
#if 0
	if (istat & FM_INTSTATUS_MPU)
		mpu_intr(sc->sc_mpu_dev);
#endif

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_INTSTATUS, 
			  istat & (FM_INTSTATUS_PLAY | FM_INTSTATUS_REC));
	
	return 1;
}

int
fms_open(addr, flags)
	void *addr;
	int flags;	
{
	/* UNUSED struct fms_softc *sc = addr;*/
	
	return 0;
}

void
fms_close(addr)
	void *addr;
{
	/* UNUSED struct fms_softc *sc = addr;*/
}

int
fms_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{

	switch (fp->index) {
	case 0:
		strlcpy(fp->name, AudioEmulaw, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 1:
		strlcpy(fp->name, AudioEslinear_le, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 2:
		strlcpy(fp->name, AudioEulinear, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 3:
		strlcpy(fp->name, AudioEalaw, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 4:
		strlcpy(fp->name, AudioEulinear_le, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 5:
		strlcpy(fp->name, AudioEslinear, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strlcpy(fp->name, AudioEulinear_be, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strlcpy(fp->name, AudioEslinear_be, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	default:
		return EINVAL;
	}
	fp->bps = AUDIO_BPS(fp->precision);
	fp->msb = 1;

	return 0;
}

void
fms_get_default_params(void *addr, int mode, struct audio_params *params)
{
	ac97_get_default_params(params);
}

/*
 * Range below -limit- is set to -rate-
 * What a pity FM801 does not have 24000
 * 24000 -> 22050 sounds rather poor
 */
struct {
	int limit;
	int rate;
} fms_rates[11] = {
	{  6600,  5500 },
	{  8750,  8000 },
	{ 10250,  9600 },
	{ 13200, 11025 },
	{ 17500, 16000 },
	{ 20500, 19200 },
	{ 26500, 22050 },
	{ 35000, 32000 },
	{ 41000, 38400 },
	{ 46000, 44100 },
	{ 48000, 48000 },
	/* anything above -> 48000 */
};

int
fms_set_params(addr, setmode, usemode, play, rec)
	void *addr;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct fms_softc *sc = addr;
	int i;

	if (setmode & AUMODE_PLAY) {
		play->factor = 1;
		play->sw_code = 0;
		switch(play->encoding) {
		case AUDIO_ENCODING_ULAW:
			play->factor = 2;
			play->sw_code = mulaw_to_slinear16_le;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (play->precision == 8)
				play->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (play->precision == 16)
				play->sw_code = change_sign16_le;
			break;
		case AUDIO_ENCODING_ALAW:
			play->factor = 2;
			play->sw_code = alaw_to_slinear16_le;
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (play->precision == 16)
				play->sw_code = swap_bytes;
			else
				play->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (play->precision == 16)
				play->sw_code = change_sign16_swap_bytes_le;
			break;
		default:
			return EINVAL;
		}
		play->bps = AUDIO_BPS(play->precision);
		play->msb = 1;

		for (i = 0; i < 10 && play->sample_rate > fms_rates[i].limit;
		     i++)
			;
		play->sample_rate = fms_rates[i].rate;
		sc->sc_play_reg = (play->channels == 2 ? FM_PLAY_STEREO : 0) |
		    (play->precision * play->factor == 16 ? FM_PLAY_16BIT : 0) |
		    (i << 8);
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
				rec->sw_code = change_sign16_le;
			break;
		case AUDIO_ENCODING_ALAW:
			rec->sw_code = ulinear8_to_alaw;
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (rec->precision == 16)
				rec->sw_code = swap_bytes;
			else
				rec->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (rec->precision == 16)
				rec->sw_code = swap_bytes_change_sign16_le;
			break;
		default:
			return EINVAL;
		}
		rec->bps = AUDIO_BPS(rec->precision);
		rec->msb = 1;

		for (i = 0; i < 10 && rec->sample_rate > fms_rates[i].limit; 
		     i++)
			;
		rec->sample_rate = fms_rates[i].rate;
		sc->sc_rec_reg = 
		    (rec->channels == 2 ? FM_REC_STEREO : 0) | 
		    (rec->precision * rec->factor == 16 ? FM_REC_16BIT : 0) |
		    (i << 8);
	}
	
	return 0;
}

int
fms_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	return (blk + 0xf) & ~0xf;
}

int
fms_halt_output(addr)
	void *addr;
{
	struct fms_softc *sc = addr;
	u_int16_t k1;
	
	k1 = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_PLAY_CTL);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_PLAY_CTL, 
			  (k1 & ~(FM_PLAY_STOPNOW | FM_PLAY_START)) | 
			  FM_PLAY_BUF1_LAST | FM_PLAY_BUF2_LAST);
	
	return 0;
}

int
fms_halt_input(addr)
	void *addr;
{
	struct fms_softc *sc = addr;
	u_int16_t k1;
	
	k1 = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_REC_CTL);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_REC_CTL, 
			  (k1 & ~(FM_REC_STOPNOW | FM_REC_START)) |
			  FM_REC_BUF1_LAST | FM_REC_BUF2_LAST);
	
	return 0;
}

int
fms_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	*retp = fms_device;
	return 0;
}

int
fms_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct fms_softc *sc = addr;

	return (sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp));
}

int
fms_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct fms_softc *sc = addr;
	
	return (sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp));
}

void *
fms_malloc(addr, direction, size, pool, flags)
	void *addr;
	int direction;
	size_t size;
	int pool, flags;
{
	struct fms_softc *sc = addr;
	struct fms_dma *p;
	int error;
	int rseg;
	
	p = malloc(sizeof(*p), pool, flags);
	if (!p)
		return 0;
	
	p->size = size;
	if ((error = bus_dmamem_alloc(sc->sc_dmat, size, NBPG, 0, &p->seg, 1, 
				      &rseg, BUS_DMA_NOWAIT)) != 0) {
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
fms_free(addr, ptr, pool)
	void *addr;
	void *ptr;
	int pool;
{
	struct fms_softc *sc = addr;
	struct fms_dma **pp, *p;

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

	panic("fms_free: trying to free unallocated memory");
}

paddr_t
fms_mappage(addr, mem, off, prot)
	void *addr;
	void *mem;
	off_t off;
	int prot;
{
	struct fms_softc *sc = addr;
	struct fms_dma *p;
	
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
fms_get_props(addr)
	void *addr;
{
	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | 
	       AUDIO_PROP_FULLDUPLEX;
}

int
fms_query_devinfo(addr, dip)
	void *addr;
	mixer_devinfo_t *dip;
{
	struct fms_softc *sc = addr;

	return (sc->codec_if->vtbl->query_devinfo(sc->codec_if, dip));
}

int
fms_trigger_output(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct fms_softc *sc = addr;
	struct fms_dma *p;
	
	sc->sc_pintr = intr;
	sc->sc_parg = arg;
	
	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		;
	
	if (!p)
		panic("fms_trigger_output: request with bad start "
		      "address (%p)", start);

	sc->sc_play_start = p->map->dm_segs[0].ds_addr;
	sc->sc_play_end = sc->sc_play_start + ((char *)end - (char *)start);
	sc->sc_play_blksize = blksize;
	sc->sc_play_nextblk = sc->sc_play_start + sc->sc_play_blksize;	
	sc->sc_play_flip = 0;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_PLAY_DMALEN, blksize - 1);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, FM_PLAY_DMABUF1, 
			  sc->sc_play_start);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, FM_PLAY_DMABUF2, 
			  sc->sc_play_nextblk);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_PLAY_CTL, 
			  FM_PLAY_START | FM_PLAY_STOPNOW | sc->sc_play_reg);
	return 0;
}


int
fms_trigger_input(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct fms_softc *sc = addr;
	struct fms_dma *p;
	
	sc->sc_rintr = intr;
	sc->sc_rarg = arg;
	
	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		;
	
	if (!p)
		panic("fms_trigger_input: request with bad start "
		      "address (%p)", start);

	sc->sc_rec_start = p->map->dm_segs[0].ds_addr;
	sc->sc_rec_end = sc->sc_rec_start + ((char *)end - (char *)start);
	sc->sc_rec_blksize = blksize;
	sc->sc_rec_nextblk = sc->sc_rec_start + sc->sc_rec_blksize;	
	sc->sc_rec_flip = 0;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_REC_DMALEN, blksize - 1);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, FM_REC_DMABUF1, 
			  sc->sc_rec_start);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, FM_REC_DMABUF2, 
			  sc->sc_rec_nextblk);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_REC_CTL, 
			  FM_REC_START | FM_REC_STOPNOW | sc->sc_rec_reg);
	return 0;
}


