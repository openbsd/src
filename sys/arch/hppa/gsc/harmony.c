/*	$OpenBSD: harmony.c,v 1.3 2003/01/26 21:25:39 jason Exp $	*/

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

#define	HARMONY_NREGS	0x40

#define	HARMONY_ID		0x00
#define	HARMONY_RESET		0x04
#define	HARMONY_CNTL		0x08
#define	HARMONY_GAINCTL		0x0c		/* gain control */
#define	HARMONY_PLAYNXT		0x10		/* play next address */
#define	HARMONY_PLAYCUR		0x14		/* play current address */
#define	HARMONY_CAPTNXT		0x18		/* capture next address */
#define	HARMONY_CAPTCUR		0x1c		/* capture current address */
#define	HARMONY_DSTATUS		0x20		/* device status */
#define	HARMONY_OV		0x24
#define	HARMONY_PIO		0x28
#define	HARMONY_DIAG		0x3c

#define	CNTL_INCNTL		0x80000000
#define	CNTL_FORMAT_MASK	0x000000c0
#define	CNTL_FORMAT_SLINEAR16BE	0x00000000
#define	CNTL_FORMAT_ULAW	0x00000040
#define	CNTL_FORMAT_ALAW	0x00000080
#define	CNTL_CHANS_MASK		0x00000020
#define	CNTL_CHANS_MONO		0x00000000
#define	CNTL_CHANS_STEREO	0x00000020
#define	CNTL_RATE_MASK		0x0000001f
#define	CNTL_RATE_5125		0x00000010
#define	CNTL_RATE_6615		0x00000017
#define	CNTL_RATE_8000		0x00000008
#define	CNTL_RATE_9600		0x0000000f
#define	CNTL_RATE_11025		0x00000011
#define	CNTL_RATE_16000		0x00000009
#define	CNTL_RATE_18900		0x00000012
#define	CNTL_RATE_22050		0x00000013
#define	CNTL_RATE_27428		0x0000000a
#define	CNTL_RATE_32000		0x0000000b
#define	CNTL_RATE_33075		0x00000016
#define	CNTL_RATE_37800		0x00000014
#define	CNTL_RATE_44100		0x00000015
#define	CNTL_RATE_48000		0x0000000e

#define	GAINCTL_INPUT_LEFT_M	0x0000f000
#define	GAINCTL_INPUT_LEFT_S	12
#define	GAINCTL_INPUT_RIGHT_M	0x000f0000
#define	GAINCTL_INPUT_RIGHT_S	16
#define	GAINCTL_MONITOR_M	0x00f00000
#define	GAINCTL_MONITOR_S	20
#define	GAINCTL_OUTPUT_LEFT_M	0x00000fc0
#define	GAINCTL_OUTPUT_LEFT_S	6
#define	GAINCTL_OUTPUT_RIGHT_M	0x0000003f
#define	GAINCTL_OUTPUT_RIGHT_S	0

#define	DSTATUS_INTRENA		0x80000000
#define	DSTATUS_PLAYNXT		0x00000200
#define	DSTATUS_CAPTNXT		0x00000002

#define HARMONY_PORT_INPUT_LVL		0
#define	HARMONY_PORT_OUTPUT_LVL		1
#define	HARMONY_PORT_MONITOR_LVL	2
#define	HARMONY_PORT_INPUT_CLASS	3
#define	HARMONY_PORT_OUTPUT_CLASS	4
#define	HARMONY_PORT_MONITOR_CLASS	5

#define	PLAYBACK_EMPTYS			3	/* playback empty buffers */
#define	CAPTURE_EMPTYS			3	/* capture empty buffers */
#define	HARMONY_BUFSIZE			4096

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
	u_int32_t sc_gainctl;
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
	int sc_playing, sc_capturing, sc_intr_enable;
	struct harmony_channel sc_playback;
};

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

struct audio_device harmony_device = {
	"harmony",
	"gsc",
	"lasi",
};

int harmony_match(struct device *, void *, void *);
void harmony_attach(struct device *, struct device *, void *);
int harmony_intr(void *);
void harmony_intr_enable(struct harmony_softc *);
void harmony_intr_disable(struct harmony_softc *);
void harmony_wait(struct harmony_softc *);
void harmony_set_gainctl(struct harmony_softc *, u_int32_t);
u_int32_t harmony_speed_bits(struct harmony_softc *, u_long *);

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

	/* set default gains */
	sc->sc_gainctl =
	    ((0x2 << GAINCTL_OUTPUT_LEFT_S) & GAINCTL_OUTPUT_LEFT_M) |
	    ((0x2 << GAINCTL_OUTPUT_RIGHT_S) & GAINCTL_OUTPUT_RIGHT_M) |
	    ((0xf << GAINCTL_INPUT_LEFT_S) & GAINCTL_INPUT_LEFT_M) |
	    ((0xf << GAINCTL_INPUT_RIGHT_S) & GAINCTL_INPUT_RIGHT_M) |
	    ((0x2 << GAINCTL_MONITOR_S) & GAINCTL_MONITOR_M) |
	    0x0f000000;

	printf("\n");

	audio_attach_mi(&harmony_sa_hw_if, sc, &sc->sc_dv);
}

/*
 * interrupt handler
 */
int
harmony_intr(vsc)
	void *vsc;
{
	struct harmony_softc *sc = vsc;
	u_int32_t dstatus;
	int r = 0;

	if (sc->sc_intr_enable == 0)
		return (0);

	harmony_intr_disable(sc);
	harmony_wait(sc);

	dstatus = bus_space_read_4(sc->sc_bt, sc->sc_bh, HARMONY_DSTATUS) &
	    (DSTATUS_PLAYNXT | DSTATUS_CAPTNXT);

	if (dstatus & DSTATUS_PLAYNXT) {
		r = 1;
		if (sc->sc_playing == 0) {
			bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_PLAYNXT,
			    sc->sc_playback_paddrs[sc->sc_playback_empty]);
			if (++sc->sc_playback_empty == PLAYBACK_EMPTYS)
				sc->sc_playback_empty = 0;
		} else {
			struct harmony_channel *c = &sc->sc_playback;
			struct harmony_dma *d;
			bus_addr_t nextaddr;
			bus_size_t togo;

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

			bus_space_write_4(sc->sc_bt, sc->sc_bh,
			    HARMONY_PLAYNXT, nextaddr);
			c->c_lastaddr = nextaddr + togo;

			if (c->c_intr != NULL)
				(*c->c_intr)(c->c_intrarg);
		}
	}

	if (dstatus & DSTATUS_CAPTNXT) {
		r = 1;
		bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_CAPTNXT,
		    sc->sc_capture_paddrs[sc->sc_capture_empty]);
		if (++sc->sc_capture_empty == CAPTURE_EMPTYS)
			sc->sc_capture_empty = 0;
	}

	harmony_intr_enable(sc);

	return (r);
}

void
harmony_intr_enable(struct harmony_softc *sc)
{
	harmony_wait(sc);
	sc->sc_intr_enable = 1;
	bus_space_write_4(sc->sc_bt, sc->sc_bh,
	    HARMONY_DSTATUS, DSTATUS_INTRENA);
}

void
harmony_intr_disable(struct harmony_softc *sc)
{
	harmony_wait(sc);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_DSTATUS, 0);
	sc->sc_intr_enable = 0;
}

void
harmony_wait(struct harmony_softc *sc)
{
	int i = 5000;

	for (i = 5000; i > 0; i++)
		if (((bus_space_read_4(sc->sc_bt, sc->sc_bh, HARMONY_CNTL)
		    & CNTL_INCNTL)) == 0)
			return;
	printf("%s: wait timeout\n", sc->sc_dv.dv_xname);
}

void
harmony_set_gainctl(struct harmony_softc *sc, u_int32_t gain)
{
	harmony_wait(sc);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_GAINCTL, gain);
}

int
harmony_open(void *vsc, int flags)
{
	struct harmony_softc *sc = vsc;

	if (sc->sc_open)
		return (EBUSY);
	sc->sc_open = 1;

	/* silence */
	harmony_set_gainctl(sc, GAINCTL_OUTPUT_LEFT_M |
	    GAINCTL_OUTPUT_RIGHT_M | GAINCTL_MONITOR_M);

	/* reset codec */
	harmony_wait(sc);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_RESET, 1);
	DELAY(50000);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_RESET, 0);

	harmony_set_gainctl(sc, sc->sc_gainctl);

	return (0);
}

void
harmony_close(void *vsc)
{
	struct harmony_softc *sc = vsc;

	harmony_halt_input(sc);
	harmony_halt_output(sc);
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
	u_int8_t quietchar;
	int i;

	if (sc->sc_need_commit == 0)
		return (0);

	harmony_wait(sc);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_CNTL, sc->sc_cntlbits);

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
	sc->sc_need_commit = 0;

	return (0);
}

int
harmony_halt_output(void *vsc)
{
	struct harmony_softc *sc = vsc;

	harmony_intr_disable(sc);
	sc->sc_playing = 0;
	return (0);
}

int
harmony_halt_input(void *vsc)
{
	struct harmony_softc *sc = vsc;

	harmony_intr_disable(sc);
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
		if (cp->un.value.num_channels == 1) {
			sc->sc_gainctl &=
			    ~(GAINCTL_INPUT_LEFT_M | GAINCTL_INPUT_RIGHT_M);
			sc->sc_gainctl |=
			    (cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] <<
			    GAINCTL_INPUT_LEFT_S) & GAINCTL_INPUT_LEFT_M;
			sc->sc_gainctl |=
			    (cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] <<
			    GAINCTL_INPUT_RIGHT_S) & GAINCTL_INPUT_RIGHT_M;
		} else if (cp->un.value.num_channels == 2) {
			sc->sc_gainctl &=
			    ~(GAINCTL_INPUT_LEFT_M | GAINCTL_INPUT_RIGHT_M);
			sc->sc_gainctl |=
			    (cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] <<
			    GAINCTL_INPUT_RIGHT_S) & GAINCTL_INPUT_RIGHT_M;
			sc->sc_gainctl |=
			    (cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] <<
			    GAINCTL_INPUT_RIGHT_S) & GAINCTL_INPUT_RIGHT_M;
		} else
			break;
		harmony_set_gainctl(sc, sc->sc_gainctl);
		err = 0;
		break;
	case HARMONY_PORT_OUTPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_gainctl &=
			    ~(GAINCTL_OUTPUT_LEFT_M | GAINCTL_OUTPUT_RIGHT_M);
			sc->sc_gainctl |=
			    (cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] <<
			    GAINCTL_OUTPUT_LEFT_S) & GAINCTL_OUTPUT_LEFT_M;
			sc->sc_gainctl |=
			    (cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] <<
			    GAINCTL_OUTPUT_RIGHT_S) & GAINCTL_OUTPUT_RIGHT_M;
		} else if (cp->un.value.num_channels == 2) {
			sc->sc_gainctl &=
			    ~(GAINCTL_OUTPUT_LEFT_M | GAINCTL_OUTPUT_RIGHT_M);
			sc->sc_gainctl |=
			    (cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] <<
			    GAINCTL_OUTPUT_RIGHT_S) & GAINCTL_OUTPUT_RIGHT_M;
			sc->sc_gainctl |=
			    (cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] <<
			    GAINCTL_OUTPUT_RIGHT_S) & GAINCTL_OUTPUT_RIGHT_M;
		} else
			break;
		harmony_set_gainctl(sc, sc->sc_gainctl);
		err = 0;
		break;
	case HARMONY_PORT_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels != 1)
			break;
		sc->sc_gainctl &= ~GAINCTL_MONITOR_M;
		sc->sc_gainctl |=
		    (cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] <<
		    GAINCTL_MONITOR_S) & GAINCTL_MONITOR_M;
		harmony_set_gainctl(sc, sc->sc_gainctl);
		err = 0;
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
			    (sc->sc_gainctl & GAINCTL_INPUT_LEFT_M) >>
			    GAINCTL_INPUT_LEFT_S;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    (sc->sc_gainctl & GAINCTL_INPUT_LEFT_M) >>
			    GAINCTL_INPUT_LEFT_S;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    (sc->sc_gainctl & GAINCTL_INPUT_RIGHT_M) >>
			    GAINCTL_INPUT_RIGHT_S;
		} else
			break;
		err = 0;
		break;
	case HARMONY_PORT_OUTPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    (sc->sc_gainctl & GAINCTL_OUTPUT_LEFT_M) >>
			    GAINCTL_OUTPUT_LEFT_S;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    (sc->sc_gainctl & GAINCTL_OUTPUT_LEFT_M) >>
			    GAINCTL_OUTPUT_LEFT_S;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    (sc->sc_gainctl & GAINCTL_OUTPUT_RIGHT_M) >>
			    GAINCTL_OUTPUT_RIGHT_S;
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
		    (sc->sc_gainctl & GAINCTL_MONITOR_M) >>
		    GAINCTL_MONITOR_S;
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
	bus_size_t n;

	for (d = sc->sc_dmas; d->d_kva != start; d = d->d_next)
		/*EMPTY*/;
	if (d == NULL) {
		printf("%s: trigger_output: bad addr: %p\n",
		    sc->sc_dv.dv_xname, start);
		return (EINVAL);
	}

	c->c_intr = intr;
	c->c_intrarg = intrarg;

	n = (caddr_t)end - (caddr_t)start;

	c->c_blksz = blksize;
	c->c_current = d;
	c->c_segsz = n;

	if (n > c->c_blksz)
		n = c->c_blksz;
	c->c_cnt = n;

	bus_dmamap_sync(sc->sc_dmat, d->d_map, 0, c->c_blksz,
	    BUS_DMASYNC_PREWRITE);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_PLAYNXT,
	    d->d_map->dm_segs[0].ds_addr);
	c->c_lastaddr = d->d_map->dm_segs[0].ds_addr + n;

	sc->sc_playing = 1;
	harmony_intr_enable(sc);
	return (0);
}

int
harmony_trigger_input(void *vsc, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, struct audio_params *param)
{
	struct harmony_softc *sc = vsc;

	bus_space_write_4(sc->sc_bt, sc->sc_bh, HARMONY_CAPTNXT,
	    sc->sc_capture_paddrs[sc->sc_capture_empty]);
	if (++sc->sc_capture_empty == CAPTURE_EMPTYS)
		sc->sc_capture_empty = 0;
	sc->sc_capturing = 1;
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

struct cfdriver harmony_cd = {
	NULL, "harmony", DV_DULL
};

struct cfattach harmony_ca = {
	sizeof(struct harmony_softc), harmony_match, harmony_attach
};
