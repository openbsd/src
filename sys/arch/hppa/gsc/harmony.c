/*	$OpenBSD: harmony.c,v 1.8 2003/01/27 23:12:41 jason Exp $	*/

/*
 * Copyright (c) 2003 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Harmony (CS4215/AD1849 LASI) audio interface.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/auconv.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>
#include <machine/bus.h>

#include <hppa/dev/cpudevs.h>
#include <hppa/gsc/gscbusvar.h>
#include <hppa/gsc/harmonyreg.h>

#define HARMONY_PORT_INPUT_LVL		0
#define	HARMONY_PORT_OUTPUT_LVL		1
#define	HARMONY_PORT_MONITOR_LVL	2
#define	HARMONY_PORT_RECORD_SOURCE	3
#define	HARMONY_PORT_INPUT_CLASS	4
#define	HARMONY_PORT_OUTPUT_CLASS	5
#define	HARMONY_PORT_MONITOR_CLASS	6
#define	HARMONY_PORT_RECORD_CLASS	7

#define	HARMONY_IN_MIC			0
#define	HARMONY_IN_LINE			1

#define	PLAYBACK_EMPTYS			3	/* playback empty buffers */
#define	CAPTURE_EMPTYS			3	/* capture empty buffers */
#define	HARMONY_BUFSIZE			4096

struct harmony_volume {
	u_char left, right;
};

struct harmony_empty {
	u_int8_t	playback[PLAYBACK_EMPTYS][HARMONY_BUFSIZE];
	u_int8_t	capture[CAPTURE_EMPTYS][HARMONY_BUFSIZE];
};

struct harmony_dma {
	struct harmony_dma *d_next;
	bus_dmamap_t d_map;
	bus_dma_segment_t d_seg;
	caddr_t d_kva;
	size_t d_size;
};

struct harmony_channel {
	struct harmony_dma *c_current;
	bus_size_t c_segsz;
	bus_size_t c_cnt;
	bus_size_t c_blksz;
	bus_addr_t c_lastaddr;
	void (*c_intr)(void *);
	void *c_intrarg;
};

struct harmony_softc {
	struct device sc_dv;
	bus_dma_tag_t sc_dmat;
	bus_space_tag_t sc_bt;
	bus_space_handle_t sc_bh;
	int sc_open;
	u_int32_t sc_cntlbits;
	int sc_need_commit;
	int sc_playback_empty;
	bus_addr_t sc_playback_paddrs[PLAYBACK_EMPTYS];
	int sc_capture_empty;
	bus_addr_t sc_capture_paddrs[CAPTURE_EMPTYS];
	bus_dmamap_t sc_empty_map;
	bus_dma_segment_t sc_empty_seg;
	int sc_empty_rseg;
	struct harmony_empty *sc_empty_kva;
	struct harmony_dma *sc_dmas;
	int sc_playing, sc_capturing;
	struct harmony_channel sc_playback, sc_capture;
	struct harmony_volume sc_monitor_lvl, sc_input_lvl, sc_output_lvl;
	int sc_in_port;
};

#define	READ_REG(sc, reg)		\
    bus_space_read_4((sc)->sc_bt, (sc)->sc_bh, (reg))
#define	WRITE_REG(sc, reg, val)		\
    bus_space_write_4((sc)->sc_bt, (sc)->sc_bh, (reg), (val))
#define	SYNC_REG(sc, reg, flags)	\
    bus_space_barrier((sc)->sc_bt, (sc)->sc_bh, (reg), sizeof(u_int32_t), \
	(flags))

int     harmony_open(void *, int);
void    harmony_close(void *);
int     harmony_query_encoding(void *, struct audio_encoding *);
int     harmony_set_params(void *, int, int, struct audio_params *,
    struct audio_params *);
int     harmony_round_blocksize(void *, int);
int     harmony_commit_settings(void *);
int     harmony_halt_output(void *);
int     harmony_halt_input(void *);
int     harmony_getdev(void *, struct audio_device *);
int     harmony_set_port(void *, mixer_ctrl_t *);
int     harmony_get_port(void *, mixer_ctrl_t *);
int     harmony_query_devinfo(void *addr, mixer_devinfo_t *);
void *  harmony_allocm(void *, int, size_t, int, int);
void    harmony_freem(void *, void *, int);
size_t  harmony_round_buffersize(void *, int, size_t);
int     harmony_get_props(void *);
int     harmony_trigger_output(void *, void *, void *, int,
    void (*intr)(void *), void *arg, struct audio_params *);
int     harmony_trigger_input(void *, void *, void *, int,
    void (*intr)(void *), void *arg, struct audio_params *);

struct audio_hw_if harmony_sa_hw_if = {
	harmony_open,
	harmony_close,
	NULL,
	harmony_query_encoding,
	harmony_set_params,
	harmony_round_blocksize,
	harmony_commit_settings,
	NULL,
	NULL,
	NULL,
	NULL,
	harmony_halt_output,
	harmony_halt_input,
	NULL,
	harmony_getdev,
	NULL,
	harmony_set_port,
	harmony_get_port,
	harmony_query_devinfo,
	harmony_allocm,
	harmony_freem,
	harmony_round_buffersize,
	NULL,
	harmony_get_props,
	harmony_trigger_output,
	harmony_trigger_input,
};

const struct audio_device harmony_device = {
	"harmony",
	"gsc",
	"lasi",
};

int harmony_match(struct device *, void *, void *);
void harmony_attach(struct device *, struct device *, void *);
int harmony_intr(void *);
void harmony_intr_enable(struct harmony_softc *);
void harmony_intr_disable(struct harmony_softc *);
u_int32_t harmony_speed_bits(struct harmony_softc *, u_long *);
void harmony_set_gainctl(struct harmony_softc *);
void harmony_reset_codec(struct harmony_softc *);
void harmony_start_pb(struct harmony_softc *);
void harmony_start_cp(struct harmony_softc *);

int
harmony_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct gsc_attach_args *ga = aux;

	if (ga->ga_type.iodc_type == HPPA_TYPE_FIO) {
		if (ga->ga_type.iodc_sv_model == HPPA_FIO_A1 ||
		    ga->ga_type.iodc_sv_model == HPPA_FIO_A2NB ||
		    ga->ga_type.iodc_sv_model == HPPA_FIO_A1NB ||
		    ga->ga_type.iodc_sv_model == HPPA_FIO_A2)
			return (1);
	}
	return (0);
}

void
harmony_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct harmony_softc *sc = (struct harmony_softc *)self;
	struct gsc_attach_args *ga = aux;
	int i;

	sc->sc_bt = ga->ga_iot;
	sc->sc_dmat = ga->ga_dmatag;

	if (bus_space_map(sc->sc_bt, ga->ga_hpa, HARMONY_NREGS, 0,
	    &sc->sc_bh) != 0) {
		printf(": couldn't map registers\n");
		return;
	}

	if (bus_dmamem_alloc(sc->sc_dmat, sizeof(struct harmony_empty),
	    PAGE_SIZE, 0, &sc->sc_empty_seg, 1, &sc->sc_empty_rseg,
	    BUS_DMA_NOWAIT) != 0) {
		printf(": couldn't alloc empty memory\n");
		return;
	}
	if (bus_dmamem_map(sc->sc_dmat, &sc->sc_empty_seg, 1,
	    sizeof(struct harmony_empty), (caddr_t *)&sc->sc_empty_kva,
	    BUS_DMA_NOWAIT) != 0) {
		printf(": couldn't map empty memory\n");
		bus_dmamem_free(sc->sc_dmat, &sc->sc_empty_seg,
		    sc->sc_empty_rseg);
		return;
	}
	if (bus_dmamap_create(sc->sc_dmat, sizeof(struct harmony_empty), 1,
	    sizeof(struct harmony_empty), 0, BUS_DMA_NOWAIT,
	    &sc->sc_empty_map) != 0) {
		printf(": can't create empty dmamap\n");
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_empty_kva,
		    sizeof(struct harmony_empty));
		bus_dmamem_free(sc->sc_dmat, &sc->sc_empty_seg,
		    sc->sc_empty_rseg);
		return;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_empty_map, sc->sc_empty_kva,
	    sizeof(struct harmony_empty), NULL, BUS_DMA_NOWAIT) != 0) {
		printf(": can't load empty dmamap\n");
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_empty_map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_empty_kva,
		    sizeof(struct harmony_empty));
		bus_dmamem_free(sc->sc_dmat, &sc->sc_empty_seg,
		    sc->sc_empty_rseg);
		return;
	}

	sc->sc_playback_empty = 0;
	for (i = 0; i < PLAYBACK_EMPTYS; i++)
		sc->sc_playback_paddrs[i] =
		    sc->sc_empty_map->dm_segs[0].ds_addr +
		    offsetof(struct harmony_empty, playback[i][0]);

	sc->sc_capture_empty = 0;
	for (i = 0; i < CAPTURE_EMPTYS; i++)
		sc->sc_capture_paddrs[i] =
		    sc->sc_empty_map->dm_segs[0].ds_addr +
		    offsetof(struct harmony_empty, playback[i][0]);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_empty_map,
	    offsetof(struct harmony_empty, playback[0][0]),
	    PLAYBACK_EMPTYS * HARMONY_BUFSIZE, BUS_DMASYNC_PREWRITE);

	(void)gsc_intr_establish((struct gsc_softc *)parent,
	    IPL_AUDIO, ga->ga_irq, harmony_intr, sc, &sc->sc_dv);

	/* set defaults */
	sc->sc_in_port = HARMONY_IN_LINE;
	sc->sc_input_lvl.left = sc->sc_input_lvl.right = 240;
	sc->sc_output_lvl.left = sc->sc_output_lvl.right = 244;
	sc->sc_monitor_lvl.left = sc->sc_monitor_lvl.right = 208;

	/* reset chip, and push default gain controls */
	harmony_reset_codec(sc);

	printf("\n");

	audio_attach_mi(&harmony_sa_hw_if, sc, &sc->sc_dv);
}

void
harmony_reset_codec(struct harmony_softc *sc)
{
	/* silence */
	WRITE_REG(sc, HARMONY_GAINCTL, GAINCTL_OUTPUT_LEFT_M |
	    GAINCTL_OUTPUT_RIGHT_M | GAINCTL_MONITOR_M);

	/* start reset */
	WRITE_REG(sc, HARMONY_RESET, RESET_RST);

	DELAY(100000);		/* wait at least 0.05 sec */

	harmony_set_gainctl(sc);
	WRITE_REG(sc, HARMONY_RESET, 0);
}

/*
 * interrupt handler
 */
int
harmony_intr(vsc)
	void *vsc;
{
	struct harmony_softc *sc = vsc;
	struct harmony_channel *c;
	u_int32_t dstatus;
	int r = 0;

	harmony_intr_disable(sc);

	dstatus = READ_REG(sc, HARMONY_DSTATUS);

	if (dstatus & DSTATUS_PN) {
		c = &sc->sc_playback;
		r = 1;
		harmony_start_pb(sc);
		if (sc->sc_playing && c->c_intr != NULL)
			(*c->c_intr)(c->c_intrarg);
	}

	if (dstatus & DSTATUS_RN) {
		c = &sc->sc_capture;
		r = 1;
		harmony_start_cp(sc);
		if (sc->sc_capturing && c->c_intr != NULL)
			(*c->c_intr)(c->c_intrarg);
	}

	harmony_intr_enable(sc);

	return (r);
}

void
harmony_intr_enable(struct harmony_softc *sc)
{
	WRITE_REG(sc, HARMONY_DSTATUS, DSTATUS_IE);
	SYNC_REG(sc, HARMONY_DSTATUS, BUS_SPACE_BARRIER_WRITE);
}

void
harmony_intr_disable(struct harmony_softc *sc)
{
	WRITE_REG(sc, HARMONY_DSTATUS, 0);
	SYNC_REG(sc, HARMONY_DSTATUS, BUS_SPACE_BARRIER_WRITE);
}

int
harmony_open(void *vsc, int flags)
{
	struct harmony_softc *sc = vsc;

	if (sc->sc_open)
		return (EBUSY);
	sc->sc_open = 1;
	return (0);
}

void
harmony_close(void *vsc)
{
	struct harmony_softc *sc = vsc;

	harmony_halt_input(sc);
	harmony_halt_output(sc);
	harmony_intr_disable(sc);
	sc->sc_open = 0;
}

int
harmony_query_encoding(void *vsc, struct audio_encoding *fp)
{
	int err = 0;

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 2:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 3:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 4:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	default:
		err = EINVAL;
	}
	return (err);
}

int
harmony_set_params(void *vsc, int setmode, int usemode,
    struct audio_params *p, struct audio_params *r)
{
	struct harmony_softc *sc = vsc;
	u_int32_t bits;
	void (*pswcode)(void *, u_char *, int cnt) = NULL;
	void (*rswcode)(void *, u_char *, int cnt) = NULL;

	switch (p->encoding) {
	case AUDIO_ENCODING_ULAW:
		if (p->precision != 8)
			return (EINVAL);
		bits = CNTL_FORMAT_ULAW;
		break;
	case AUDIO_ENCODING_ALAW:
		if (p->precision != 8)
			return (EINVAL);
		bits = CNTL_FORMAT_ALAW;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (p->precision != 16)
			return (EINVAL);
		bits = CNTL_FORMAT_SLINEAR16BE;
		break;

	/* emulated formats */
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision != 16)
			return (EINVAL);
		bits = CNTL_FORMAT_SLINEAR16BE;
		rswcode = pswcode = swap_bytes;
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		if (p->precision != 16)
			return (EINVAL);
		bits = CNTL_FORMAT_SLINEAR16BE;
		rswcode = pswcode = change_sign16_be;
		break;
	default:
		return (EINVAL);
	}

	if (p->channels == 1)
		bits |= CNTL_CHANS_MONO;
	else if (p->channels == 2)
		bits |= CNTL_CHANS_STEREO;
	else
		return (EINVAL);

	bits |= harmony_speed_bits(sc, &p->sample_rate);
	p->sw_code = pswcode;
	r->sw_code = rswcode;
	sc->sc_cntlbits = bits;
	sc->sc_need_commit = 1;

	return (0);
}

int
harmony_round_blocksize(void *vsc, int blk)
{
	return (HARMONY_BUFSIZE);
}

int
harmony_commit_settings(void *vsc)
{
	struct harmony_softc *sc = vsc;
	u_int32_t reg;
	u_int8_t quietchar;
	int i;

	if (sc->sc_need_commit == 0)
		return (0);

	harmony_intr_disable(sc);

	for (;;) {
		reg = READ_REG(sc, HARMONY_DSTATUS);
		if ((reg & (DSTATUS_PC | DSTATUS_RC)) == 0)
			break;
	}

	WRITE_REG(sc, HARMONY_GAINCTL, GAINCTL_OUTPUT_LEFT_M |
	    GAINCTL_OUTPUT_RIGHT_M | GAINCTL_MONITOR_M);

	/* set the silence character based on the encoding type */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_empty_map,
	    offsetof(struct harmony_empty, playback[0][0]),
	    PLAYBACK_EMPTYS * HARMONY_BUFSIZE, BUS_DMASYNC_POSTWRITE);
	switch (sc->sc_cntlbits & CNTL_FORMAT_MASK) {
	case CNTL_FORMAT_ULAW:
		quietchar = 0x7f;
		break;
	case CNTL_FORMAT_ALAW:
		quietchar = 0x55;
		break;
	case CNTL_FORMAT_SLINEAR16BE:
	default:
		quietchar = 0;
		break;
	}
	for (i = 0; i < PLAYBACK_EMPTYS; i++)
		memset(&sc->sc_empty_kva->playback[i][0],
		    quietchar, HARMONY_BUFSIZE);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_empty_map,
	    offsetof(struct harmony_empty, playback[0][0]),
	    PLAYBACK_EMPTYS * HARMONY_BUFSIZE, BUS_DMASYNC_PREWRITE);

	for (;;) {
		/* Wait for it to come out of control mode */
		reg = READ_REG(sc, HARMONY_CNTL);
		if ((reg & CNTL_C) == 0)
			break;
	}

	bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_CNTL,
	    sc->sc_cntlbits | CNTL_C);

	for (;;) {
		/* Wait for it to come out of control mode */
		reg = READ_REG(sc, HARMONY_CNTL);
		if ((reg & CNTL_C) == 0)
			break;
	}

	harmony_set_gainctl(sc);
	sc->sc_need_commit = 0;

	if (sc->sc_playing || sc->sc_capturing)
		harmony_intr_enable(sc);

	return (0);
}

int
harmony_halt_output(void *vsc)
{
	struct harmony_softc *sc = vsc;

	sc->sc_playing = 0;
	return (0);
}

int
harmony_halt_input(void *vsc)
{
	struct harmony_softc *sc = vsc;

	sc->sc_capturing = 0;
	return (0);
}

int
harmony_getdev(void *vsc, struct audio_device *retp)
{
	*retp = harmony_device;
	return (0);
}

int
harmony_set_port(void *vsc, mixer_ctrl_t *cp)
{
	struct harmony_softc *sc = vsc;
	int err = EINVAL;

	switch (cp->dev) {
	case HARMONY_PORT_INPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			sc->sc_input_lvl.left = sc->sc_input_lvl.right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		else if (cp->un.value.num_channels == 2) {
			sc->sc_input_lvl.left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_input_lvl.right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		} else
			break;
		sc->sc_need_commit = 1;
		err = 0;
		break;
	case HARMONY_PORT_OUTPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			sc->sc_output_lvl.left = sc->sc_output_lvl.right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		else if (cp->un.value.num_channels == 2) {
			sc->sc_output_lvl.left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_output_lvl.right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		} else
			break;
		sc->sc_need_commit = 1;
		err = 0;
		break;
	case HARMONY_PORT_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels != 1)
			break;
		sc->sc_monitor_lvl.left = sc->sc_input_lvl.right =
		    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		sc->sc_need_commit = 1;
		err = 0;
		break;
	case HARMONY_PORT_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		if (cp->un.ord != HARMONY_IN_LINE &&
		    cp->un.ord != HARMONY_IN_MIC)
			break;
		sc->sc_in_port = cp->un.ord;
		err = 0;
		sc->sc_need_commit = 1;
		break;
	}

	return (err);
}

int
harmony_get_port(void *vsc, mixer_ctrl_t *cp)
{
	struct harmony_softc *sc = vsc;
	int err = EINVAL;

	switch (cp->dev) {
	case HARMONY_PORT_INPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_input_lvl.left;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_input_lvl.left;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_input_lvl.right;
		} else
			break;
		err = 0;
		break;
	case HARMONY_PORT_OUTPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_output_lvl.left;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_output_lvl.left;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_output_lvl.right;
		} else
			break;
		err = 0;
		break;
	case HARMONY_PORT_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels != 1)
			break;
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
		    sc->sc_monitor_lvl.left;
		err = 0;
		break;
	case HARMONY_PORT_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_in_port;
		err = 0;
		break;
	}
	return (0);
}

int
harmony_query_devinfo(void *vsc, mixer_devinfo_t *dip)
{
	int err = 0;

	switch (dip->index) {
	case HARMONY_PORT_INPUT_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = HARMONY_PORT_INPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNinput);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case HARMONY_PORT_OUTPUT_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = HARMONY_PORT_OUTPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNoutput);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case HARMONY_PORT_MONITOR_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = HARMONY_PORT_MONITOR_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNoutput);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case HARMONY_PORT_RECORD_SOURCE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = HARMONY_PORT_RECORD_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNmicrophone);
		dip->un.e.member[0].ord = HARMONY_IN_MIC;
		strcpy(dip->un.e.member[1].label.name, AudioNline);
		dip->un.e.member[1].ord = HARMONY_IN_LINE;
		break;
	case HARMONY_PORT_INPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = HARMONY_PORT_INPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		break;
	case HARMONY_PORT_OUTPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = HARMONY_PORT_INPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		break;
	case HARMONY_PORT_MONITOR_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = HARMONY_PORT_INPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCmonitor);
		break;
	case HARMONY_PORT_RECORD_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = HARMONY_PORT_RECORD_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		break;
	default:
		err = ENXIO;
		break;
	}

	return (err);
}

void *
harmony_allocm(void *vsc, int dir, size_t size, int pool, int flags)
{
	struct harmony_softc *sc = vsc;
	struct harmony_dma *d;
	int rseg;

	d = (struct harmony_dma *)malloc(sizeof(struct harmony_dma), pool, flags);
	if (d == NULL)
		goto fail;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0, BUS_DMA_NOWAIT,
	    &d->d_map) != 0)
		goto fail1;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &d->d_seg, 1,
	    &rseg, BUS_DMA_NOWAIT) != 0)
		goto fail2;

	if (bus_dmamem_map(sc->sc_dmat, &d->d_seg, 1, size, &d->d_kva,
	    BUS_DMA_NOWAIT) != 0)
		goto fail3;

	if (bus_dmamap_load(sc->sc_dmat, d->d_map, d->d_kva, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto fail4;

	d->d_next = sc->sc_dmas;
	sc->sc_dmas = d;
	d->d_size = size;
	return (d->d_kva);

fail4:
	bus_dmamem_unmap(sc->sc_dmat, d->d_kva, size);
fail3:
	bus_dmamem_free(sc->sc_dmat, &d->d_seg, 1);
fail2:
	bus_dmamap_destroy(sc->sc_dmat, d->d_map);
fail1:
	free(d, pool);
fail:
	return (NULL);
}

void
harmony_freem(void *vsc, void *ptr, int pool)
{
	struct harmony_softc *sc = vsc;
	struct harmony_dma *d, **dd;

	for (dd = &sc->sc_dmas; (d = *dd) != NULL; dd = &(*dd)->d_next) {
		if (d->d_kva != ptr)
			continue;
		bus_dmamap_unload(sc->sc_dmat, d->d_map);
		bus_dmamem_unmap(sc->sc_dmat, d->d_kva, d->d_size);
		bus_dmamem_free(sc->sc_dmat, &d->d_seg, 1);
		bus_dmamap_destroy(sc->sc_dmat, d->d_map);
		free(d, pool);
		return;
	}
	printf("%s: free rogue pointer\n", sc->sc_dv.dv_xname);
}

size_t
harmony_round_buffersize(void *vsc, int direction, size_t size)
{
	return (size & (size_t)(-HARMONY_BUFSIZE));
}

int
harmony_get_props(void *vsc)
{
	return (AUDIO_PROP_FULLDUPLEX);
}

int
harmony_trigger_output(void *vsc, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, struct audio_params *param)
{
	struct harmony_softc *sc = vsc;
	struct harmony_channel *c = &sc->sc_playback;
	struct harmony_dma *d;

	for (d = sc->sc_dmas; d->d_kva != start; d = d->d_next)
		/*EMPTY*/;
	if (d == NULL) {
		printf("%s: trigger_output: bad addr: %p\n",
		    sc->sc_dv.dv_xname, start);
		return (EINVAL);
	}

	c->c_intr = intr;
	c->c_intrarg = intrarg;
	c->c_blksz = blksize;
	c->c_current = d;
	c->c_segsz = (caddr_t)end - (caddr_t)start;
	c->c_cnt = 0;
	c->c_lastaddr = d->d_map->dm_segs[0].ds_addr;

	sc->sc_playing = 1;
	harmony_start_pb(sc);
	harmony_start_cp(sc);
	harmony_intr_enable(sc);

	return (0);
}

void
harmony_start_pb(struct harmony_softc *sc)
{
	struct harmony_channel *c = &sc->sc_playback;
	struct harmony_dma *d;
	bus_addr_t nextaddr;
	bus_size_t togo;

	if (sc->sc_playing == 0) {
		WRITE_REG(sc, HARMONY_PNXTADD,
		    sc->sc_playback_paddrs[sc->sc_playback_empty]);
		SYNC_REG(sc, HARMONY_PNXTADD, BUS_SPACE_BARRIER_WRITE);
		if (++sc->sc_playback_empty == PLAYBACK_EMPTYS)
			sc->sc_playback_empty = 0;
	} else {
		d = c->c_current;
		togo = c->c_segsz - c->c_cnt;
		if (togo == 0) {
			nextaddr = d->d_map->dm_segs[0].ds_addr;
			c->c_cnt = togo = c->c_blksz;
		} else {
			nextaddr = c->c_lastaddr;
			if (togo > c->c_blksz)
				togo = c->c_blksz;
			c->c_cnt += togo;
		}

		bus_dmamap_sync(sc->sc_dmat, d->d_map,
		    nextaddr - d->d_map->dm_segs[0].ds_addr,
		    c->c_blksz, BUS_DMASYNC_PREWRITE);

		WRITE_REG(sc, HARMONY_PNXTADD, nextaddr);
		SYNC_REG(sc, HARMONY_PNXTADD, BUS_SPACE_BARRIER_WRITE);
		c->c_lastaddr = nextaddr + togo;
	}
}

void
harmony_start_cp(struct harmony_softc *sc)
{
	struct harmony_channel *c = &sc->sc_capture;
	struct harmony_dma *d;
	bus_addr_t nextaddr;
	bus_size_t togo;

	if (sc->sc_capturing == 0) {
		WRITE_REG(sc, HARMONY_RNXTADD,
		    sc->sc_capture_paddrs[sc->sc_capture_empty]);
		if (++sc->sc_capture_empty == CAPTURE_EMPTYS)
			sc->sc_capture_empty = 0;
	} else {
		d = c->c_current;
		togo = c->c_segsz - c->c_cnt;
		if (togo == 0) {
			nextaddr = d->d_map->dm_segs[0].ds_addr;
			c->c_cnt = togo = c->c_blksz;
		} else {
			nextaddr = c->c_lastaddr;
			if (togo > c->c_blksz)
				togo = c->c_blksz;
			c->c_cnt += togo;
		}

		bus_dmamap_sync(sc->sc_dmat, d->d_map,
		    nextaddr - d->d_map->dm_segs[0].ds_addr,
		    c->c_blksz, BUS_DMASYNC_PREWRITE);

		WRITE_REG(sc, HARMONY_RNXTADD, nextaddr);
		bus_space_barrier(sc->sc_bt, sc->sc_bh,
		    HARMONY_RNXTADD, sizeof(u_int32_t),
		    BUS_SPACE_BARRIER_WRITE);
		c->c_lastaddr = nextaddr + togo;
	}
}

int
harmony_trigger_input(void *vsc, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, struct audio_params *param)
{
	struct harmony_softc *sc = vsc;
	struct harmony_channel *c = &sc->sc_capture;
	struct harmony_dma *d;

	for (d = sc->sc_dmas; d->d_kva != start; d = d->d_next)
		/*EMPTY*/;
	if (d == NULL) {
		printf("%s: trigger_input: bad addr: %p\n",
		    sc->sc_dv.dv_xname, start);
		return (EINVAL);
	}

	c->c_intr = intr;
	c->c_intrarg = intrarg;
	c->c_blksz = blksize;
	c->c_current = d;
	c->c_segsz = (caddr_t)end - (caddr_t)start;
	c->c_cnt = 0;
	c->c_lastaddr = d->d_map->dm_segs[0].ds_addr;

	sc->sc_capturing = 1;
	harmony_start_pb(sc);
	harmony_start_cp(sc);
	harmony_intr_enable(sc);
	return (0);
}

static const struct speed_struct {
	u_int32_t speed;
	u_int32_t bits;
} harmony_speeds[] = {
	{ 5125, CNTL_RATE_5125 },
	{ 6615, CNTL_RATE_6615 },
	{ 8000, CNTL_RATE_8000 },
	{ 9600, CNTL_RATE_9600 },
	{ 11025, CNTL_RATE_11025 },
	{ 16000, CNTL_RATE_16000 },
	{ 18900, CNTL_RATE_18900 },
	{ 22050, CNTL_RATE_22050 },
	{ 27428, CNTL_RATE_27428 },
	{ 32000, CNTL_RATE_32000 },
	{ 33075, CNTL_RATE_33075 },
	{ 37800, CNTL_RATE_37800 },
	{ 44100, CNTL_RATE_44100 },
	{ 48000, CNTL_RATE_48000 },
};

u_int32_t
harmony_speed_bits(struct harmony_softc *sc, u_long *speedp)
{
	int i, n, selected = -1;

	n = sizeof(harmony_speeds) / sizeof(harmony_speeds[0]);

	if ((*speedp) <= harmony_speeds[0].speed)
		selected = 0;
	else if ((*speedp) >= harmony_speeds[n - 1].speed)
		selected = n - 1;
	else {
		for (i = 1; selected == -1 && i < n; i++) {
			if ((*speedp) == harmony_speeds[i].speed)
				selected = i;
			else if ((*speedp) < harmony_speeds[i].speed) {
				int diff1, diff2;

				diff1 = (*speedp) - harmony_speeds[i - 1].speed;
				diff2 = harmony_speeds[i].speed - (*speedp);
				if (diff1 < diff2)
					selected = i - 1;
				else
					selected = i;
			}
		}
	}

	if (selected == -1)
		selected = 2;

	*speedp = harmony_speeds[selected].speed;
	return (harmony_speeds[selected].bits);
}

void
harmony_set_gainctl(struct harmony_softc *sc)
{
	/* master (monitor) and playback are inverted */
	u_int32_t bits, mask, val;

	/* XXX leave these bits alone or the chip will not come out of CNTL */
	bits = GAINCTL_HE | GAINCTL_LE | GAINCTL_SE | GAINCTL_IS_MASK;

	/* input level */
	bits |= ((sc->sc_input_lvl.left >> (8 - GAINCTL_INPUT_BITS)) <<
	    GAINCTL_INPUT_LEFT_S) & GAINCTL_INPUT_LEFT_M;
	bits |= ((sc->sc_input_lvl.right >> (8 - GAINCTL_INPUT_BITS)) <<
	    GAINCTL_INPUT_RIGHT_S) & GAINCTL_INPUT_RIGHT_M;

	/* output level (inverted) */
	mask = (1 << GAINCTL_OUTPUT_BITS) - 1;
	val = mask - (sc->sc_output_lvl.left >> (8 - GAINCTL_OUTPUT_BITS));
	bits |= (val << GAINCTL_OUTPUT_LEFT_S) & GAINCTL_OUTPUT_LEFT_M;
	val = mask - (sc->sc_output_lvl.right >> (8 - GAINCTL_OUTPUT_BITS));
	bits |= (val << GAINCTL_OUTPUT_RIGHT_S) & GAINCTL_OUTPUT_RIGHT_M;

	/* monitor level (inverted) */
	mask = (1 << GAINCTL_MONITOR_BITS) - 1;
	val = mask - (sc->sc_monitor_lvl.left >> (8 - GAINCTL_MONITOR_BITS));
	bits |= (val << GAINCTL_MONITOR_S) & GAINCTL_MONITOR_M;

	bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_GAINCTL, bits);
}

struct cfdriver harmony_cd = {
	NULL, "harmony", DV_DULL
};

struct cfattach harmony_ca = {
	sizeof(struct harmony_softc), harmony_match, harmony_attach
};
