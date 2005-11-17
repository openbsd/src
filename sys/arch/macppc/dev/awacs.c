/*	$OpenBSD: awacs.c,v 1.18 2005/11/17 02:58:03 brad Exp $	*/
/*	$NetBSD: awacs.c,v 1.4 2001/02/26 21:07:51 wiz Exp $	*/

/*-
 * Copyright (c) 2000 Tsubai Masanari.  All rights reserved.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/audioio.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/auconv.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <macppc/dev/dbdma.h>

#ifdef AWACS_DEBUG
# define DPRINTF printf
#else
# define DPRINTF while (0) printf
#endif

#define	AWACS_DMALIST_MAX	32
#define	AWACS_DMASEG_MAX	NBPG

struct awacs_dma {
	bus_dmamap_t map;
	caddr_t addr;
	bus_dma_segment_t segs[AWACS_DMALIST_MAX];
	int nsegs;
	size_t size;
	struct awacs_dma *next;
};


struct awacs_softc {
	struct device sc_dev;

	void (*sc_ointr)(void *);	/* dma completion intr handler */
	void *sc_oarg;			/* arg for sc_ointr() */

	void (*sc_iintr)(void *);	/* dma completion intr handler */
	void *sc_iarg;			/* arg for sc_iintr() */

	u_int sc_record_source;		/* recording source mask */
	u_int sc_output_mask;		/* output source mask */

	char *sc_reg;
	u_int sc_codecctl0;
	u_int sc_codecctl1;
	u_int sc_codecctl2;
	u_int sc_codecctl4;
	u_int sc_soundctl;

	bus_dma_tag_t sc_dmat;
	struct dbdma_regmap *sc_odma;
	struct dbdma_regmap *sc_idma;
	struct dbdma_command *sc_odmacmd, *sc_odmap;
	struct dbdma_command *sc_idmacmd, *sc_idmap;
	dbdma_t sc_odbdma, sc_idbdma;

	struct awacs_dma *sc_dmas;
};

int awacs_match(struct device *, void *, void *);
void awacs_attach(struct device *, struct device *, void *);
int awacs_intr(void *);
int awacs_tx_intr(void *);
int awacs_rx_intr(void *);

int awacs_open(void *, int);
void awacs_close(void *);
int awacs_query_encoding(void *, struct audio_encoding *);
int awacs_set_params(void *, int, int, struct audio_params *,
			 struct audio_params *);
int awacs_round_blocksize(void *, int);
int awacs_trigger_output(void *, void *, void *, int, void (*)(void *),
			     void *, struct audio_params *);
int awacs_trigger_input(void *, void *, void *, int, void (*)(void *),
			    void *, struct audio_params *);
int awacs_halt_output(void *);
int awacs_halt_input(void *);
int awacs_getdev(void *, struct audio_device *);
int awacs_set_port(void *, mixer_ctrl_t *);
int awacs_get_port(void *, mixer_ctrl_t *);
int awacs_query_devinfo(void *, mixer_devinfo_t *);
size_t awacs_round_buffersize(void *, int, size_t);
paddr_t awacs_mappage(void *, void *, off_t, int);
int awacs_get_props(void *);
void *awacs_allocm(void *, int, size_t, int, int);

static inline u_int awacs_read_reg(struct awacs_softc *, int);
static inline void awacs_write_reg(struct awacs_softc *, int, int);
void awacs_write_codec(struct awacs_softc *, int);
void awacs_set_speaker_volume(struct awacs_softc *, int, int);
void awacs_set_ext_volume(struct awacs_softc *, int, int);
void awacs_set_rate(struct awacs_softc *, struct audio_params *);
void awacs_mono16_to_stereo16(void *, u_char *, int);
void awacs_swap_bytes_mono16_to_stereo16(void *, u_char *, int);
void awacs_cvt_ulinear_mono_16_be(void *, u_char *, int);
void awacs_cvt_ulinear_mono_16_le(void *, u_char *, int);

struct cfattach awacs_ca = {
	sizeof(struct awacs_softc), awacs_match, awacs_attach
};

struct cfdriver awacs_cd = {
	NULL, "awacs", DV_DULL
};

struct audio_hw_if awacs_hw_if = {
	awacs_open,
	awacs_close,
	NULL,			/* drain */
	awacs_query_encoding,
	awacs_set_params,
	awacs_round_blocksize,
	NULL,			/* commit_setting */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	awacs_halt_output,
	awacs_halt_input,
	NULL,			/* speaker_ctl */
	awacs_getdev,
	NULL,			/* getfd */
	awacs_set_port,
	awacs_get_port,
	awacs_query_devinfo,
	awacs_allocm,		/* allocm */
	NULL,			/* freem */
	awacs_round_buffersize,	/* round_buffersize */
	awacs_mappage,
	awacs_get_props,
	awacs_trigger_output,
	awacs_trigger_input
};

struct audio_device awacs_device = {
	"AWACS",
	"",
	"awacs"
};

/* register offset */
#define AWACS_SOUND_CTRL	0x00
#define AWACS_CODEC_CTRL	0x10
#define AWACS_CODEC_STATUS	0x20
#define AWACS_CLIP_COUNT	0x30
#define AWACS_BYTE_SWAP		0x40

/* sound control */
#define AWACS_INPUT_SUBFRAME0	0x00000001
#define AWACS_INPUT_SUBFRAME1	0x00000002
#define AWACS_INPUT_SUBFRAME2	0x00000004
#define AWACS_INPUT_SUBFRAME3	0x00000008

#define AWACS_OUTPUT_SUBFRAME0	0x00000010
#define AWACS_OUTPUT_SUBFRAME1	0x00000020
#define AWACS_OUTPUT_SUBFRAME2	0x00000040
#define AWACS_OUTPUT_SUBFRAME3	0x00000080

#define AWACS_RATE_44100	0x00000000
#define AWACS_RATE_29400	0x00000100
#define AWACS_RATE_22050	0x00000200
#define AWACS_RATE_17640	0x00000300
#define AWACS_RATE_14700	0x00000400
#define AWACS_RATE_11025	0x00000500
#define AWACS_RATE_8820		0x00000600
#define AWACS_RATE_7350		0x00000700
#define AWACS_RATE_MASK		0x00000700

#define AWACS_CTL_CNTRLERR 	(1 << 11)
#define AWACS_CTL_PORTCHG 	(1 << 12)
#define AWACS_INT_CNTRLERR 	(1 << 13)
#define AWACS_INT_PORTCHG 	(1 << 14)

/* codec control */
#define AWACS_CODEC_ADDR0	0x00000000
#define AWACS_CODEC_ADDR1	0x00001000
#define AWACS_CODEC_ADDR2	0x00002000
#define AWACS_CODEC_ADDR4	0x00004000
#define AWACS_CODEC_EMSEL0	0x00000000
#define AWACS_CODEC_EMSEL1	0x00400000
#define AWACS_CODEC_EMSEL2	0x00800000
#define AWACS_CODEC_EMSEL4	0x00c00000
#define AWACS_CODEC_BUSY	0x01000000

/* cc0 */
#define AWACS_DEFAULT_CD_GAIN	0x000000bb
#define AWACS_INPUT_CD		0x00000200
#define AWACS_INPUT_LINE	0x00000400
#define AWACS_INPUT_MICROPHONE	0x00000800
#define AWACS_INPUT_MASK	0x00000e00

/* cc1 */
#define AWACS_MUTE_SPEAKER	0x00000080
#define AWACS_MUTE_HEADPHONE	0x00000200

const struct awacs_speed_tab {
	int rate;
	u_int32_t bits;
} awacs_speeds[] = {
	{  7350, AWACS_RATE_7350 },
	{  8820, AWACS_RATE_8820 },
	{ 11025, AWACS_RATE_11025 },
	{ 14700, AWACS_RATE_14700 },
	{ 17640, AWACS_RATE_17640 },
	{ 22050, AWACS_RATE_22050 },
	{ 29400, AWACS_RATE_29400 },
	{ 44100, AWACS_RATE_44100 },
};

int
awacs_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, "awacs") != 0 &&
	    strcmp(ca->ca_name, "davbus") != 0)
		return 0;

#ifdef DEBUG
	printf("awacs: matched %s nreg %d nintr %d\n",
		ca->ca_name, ca->ca_nreg, ca->ca_nintr);
#endif

	if (ca->ca_nreg < 24 || ca->ca_nintr < 12)
		return 0;

	/* XXX for now
	if (ca->ca_nintr > 12)
		return 0;
	*/

	return 1;
}

void
awacs_attach(struct device *parent, struct device *self, void *aux)
{
	struct awacs_softc *sc = (struct awacs_softc *)self;
	struct confargs *ca = aux;
	int cirq, oirq, iirq;
	int cirq_type, oirq_type, iirq_type;

	ca->ca_reg[0] += ca->ca_baseaddr;
	ca->ca_reg[2] += ca->ca_baseaddr;
	ca->ca_reg[4] += ca->ca_baseaddr;

	sc->sc_reg = mapiodev(ca->ca_reg[0], ca->ca_reg[1]);

	sc->sc_dmat = ca->ca_dmat;
	sc->sc_odma = mapiodev(ca->ca_reg[2], ca->ca_reg[3]); /* out */
	sc->sc_idma = mapiodev(ca->ca_reg[4], ca->ca_reg[5]); /* in */
	sc->sc_odbdma = dbdma_alloc(sc->sc_dmat, AWACS_DMALIST_MAX);
	sc->sc_odmacmd = sc->sc_odbdma->d_addr;
	sc->sc_idbdma = dbdma_alloc(sc->sc_dmat, AWACS_DMALIST_MAX);
	sc->sc_idmacmd = sc->sc_idbdma->d_addr;

	if (ca->ca_nintr == 24) {
		cirq = ca->ca_intr[0];
		oirq = ca->ca_intr[2];
		iirq = ca->ca_intr[4];
		cirq_type = ca->ca_intr[1] ? IST_LEVEL : IST_EDGE;
		oirq_type = ca->ca_intr[3] ? IST_LEVEL : IST_EDGE;
		iirq_type = ca->ca_intr[5] ? IST_LEVEL : IST_EDGE;
	} else {
		cirq = ca->ca_intr[0];
		oirq = ca->ca_intr[1];
		iirq = ca->ca_intr[2];
		cirq_type = oirq_type = iirq_type = IST_LEVEL;
	}
	mac_intr_establish(parent, cirq, cirq_type, IPL_AUDIO, awacs_intr,
	    sc, "awacs");
	mac_intr_establish(parent, oirq, oirq_type, IPL_AUDIO, awacs_tx_intr,
	    sc, "awacs/tx");
	mac_intr_establish(parent, iirq, iirq_type, IPL_AUDIO, awacs_rx_intr,
	    sc, "awacs/rx");

	printf(": irq %d,%d,%d",
		cirq, oirq, iirq);

	sc->sc_soundctl = AWACS_INPUT_SUBFRAME0 | AWACS_OUTPUT_SUBFRAME0 |
	    AWACS_RATE_44100 | AWACS_INT_PORTCHG;
	awacs_write_reg(sc, AWACS_SOUND_CTRL, sc->sc_soundctl);

	sc->sc_codecctl0 = AWACS_CODEC_ADDR0 | AWACS_CODEC_EMSEL0;
	sc->sc_codecctl1 = AWACS_CODEC_ADDR1 | AWACS_CODEC_EMSEL0;
	sc->sc_codecctl2 = AWACS_CODEC_ADDR2 | AWACS_CODEC_EMSEL0;
	sc->sc_codecctl4 = AWACS_CODEC_ADDR4 | AWACS_CODEC_EMSEL0;

	sc->sc_codecctl0 |= AWACS_INPUT_CD | AWACS_DEFAULT_CD_GAIN;
	awacs_write_codec(sc, sc->sc_codecctl0);

	/* Set initial volume[s] */
	awacs_set_speaker_volume(sc, 80, 80);
	awacs_set_ext_volume(sc, 80, 80);

	/* Set loopback (for CD?) */
	/* sc->sc_codecctl1 |= 0x440; */
	sc->sc_codecctl1 |= 0x40;
	awacs_write_codec(sc, sc->sc_codecctl1);

	/* check for headphone present */
	if (awacs_read_reg(sc, AWACS_CODEC_STATUS) & 0x8) {
		/* default output to speakers */
		printf(" headphones");
		sc->sc_output_mask = 1 << 1;
		sc->sc_codecctl1 &= ~AWACS_MUTE_HEADPHONE;
		sc->sc_codecctl1 |= AWACS_MUTE_SPEAKER;
		awacs_write_codec(sc, sc->sc_codecctl1);
	} else {
		/* default output to speakers */
		printf(" speaker");
		sc->sc_output_mask = 1 << 0;
		sc->sc_codecctl1 &= ~AWACS_MUTE_SPEAKER;
		sc->sc_codecctl1 |= AWACS_MUTE_HEADPHONE;
		awacs_write_codec(sc, sc->sc_codecctl1);
	}

	/* default input from CD */
	sc->sc_record_source = 1 << 0;
	sc->sc_codecctl0 &= ~AWACS_INPUT_MASK;
	sc->sc_codecctl0 |= AWACS_INPUT_CD;
	awacs_write_codec(sc, sc->sc_codecctl0);

	/* Enable interrupts and looping mode. */
	/* XXX ... */
	awacs_halt_output(sc);
	awacs_halt_input(sc);
	printf("\n");

	audio_attach_mi(&awacs_hw_if, sc, &sc->sc_dev);
}

u_int
awacs_read_reg(struct awacs_softc *sc, int reg)
{
	char *addr = sc->sc_reg;

	return in32rb(addr + reg);
}

void
awacs_write_reg(struct awacs_softc *sc, int reg, int val)
{
	char *addr = sc->sc_reg;

	out32rb(addr + reg, val);
}

void
awacs_write_codec(struct awacs_softc *sc, int value)
{
	awacs_write_reg(sc, AWACS_CODEC_CTRL, value);
	while (awacs_read_reg(sc, AWACS_CODEC_CTRL) & AWACS_CODEC_BUSY);
}

int
awacs_intr(void *v)
{
	int reason;
	struct awacs_softc *sc = v;
	reason = awacs_read_reg(sc, AWACS_SOUND_CTRL);

	if (reason & AWACS_CTL_CNTRLERR) {
		/* change outputs ?? */
	}
	if (reason & AWACS_CTL_PORTCHG) {
#ifdef DEBUG
		printf("status = %x\n", awacs_read_reg(sc, AWACS_CODEC_STATUS));
#endif

		if (awacs_read_reg(sc, AWACS_CODEC_STATUS) & 0x8) {
			/* default output to speakers */
			sc->sc_output_mask = 1 << 1;
			sc->sc_codecctl1 &= ~AWACS_MUTE_HEADPHONE;
			sc->sc_codecctl1 |= AWACS_MUTE_SPEAKER;
			awacs_write_codec(sc, sc->sc_codecctl1);
		} else {
			/* default output to speakers */
			sc->sc_output_mask = 1 << 0;
			sc->sc_codecctl1 &= ~AWACS_MUTE_SPEAKER;
			sc->sc_codecctl1 |= AWACS_MUTE_HEADPHONE;
			awacs_write_codec(sc, sc->sc_codecctl1);
		}
	}

	awacs_write_reg(sc, AWACS_SOUND_CTRL, reason); /* clear interrupt */
	return 1;
}

int
awacs_tx_intr(void *v)
{
	struct awacs_softc *sc = v;
	struct dbdma_command *cmd = sc->sc_odmap;
	u_int16_t c, status;

	/* if not set we are not running */
	if (!cmd)
		return (0);

	c = in16rb(&cmd->d_command);
	status = in16rb(&cmd->d_status);

	if (c >> 12 == DBDMA_CMD_OUT_LAST)
		sc->sc_odmap = sc->sc_odmacmd;
	else
		sc->sc_odmap++;

	if (c & (DBDMA_INT_ALWAYS << 4)) {
		cmd->d_status = 0;
		if (status)	/* status == 0x8400 */
			if (sc->sc_ointr)
				(*sc->sc_ointr)(sc->sc_oarg);
	}

	return (1);
}
int
awacs_rx_intr(void *v)
{
	struct awacs_softc *sc = v;
	struct dbdma_command *cmd = sc->sc_idmap;
	u_int16_t c, status;

	/* if not set we are not running */
	if (!cmd)
		return (0);

	c = in16rb(&cmd->d_command);
	status = in16rb(&cmd->d_status);

	if (c >> 12 == DBDMA_CMD_IN_LAST)
		sc->sc_idmap = sc->sc_idmacmd;
	else
		sc->sc_idmap++;

	if (c & (DBDMA_INT_ALWAYS << 4)) {
		cmd->d_status = 0;
		if (status)	/* status == 0x8400 */
			if (sc->sc_iintr)
				(*sc->sc_iintr)(sc->sc_iarg);
	}

	return (1);
}

int
awacs_open(void *h, int flags)
{
	return 0;
}

/*
 * Close function is called at splaudio().
 */
void
awacs_close(void *h)
{
	struct awacs_softc *sc = h;

	awacs_halt_output(sc);
	awacs_halt_input(sc);

	sc->sc_ointr = 0;
	sc->sc_iintr = 0;
}

int
awacs_query_encoding(void *h, struct audio_encoding *ae)
{
	switch (ae->index) {
	case 0:
		strlcpy(ae->name, AudioEslinear, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_SLINEAR;
		ae->precision = 16;
		ae->flags = 0;
		break;
	case 1:
		strlcpy(ae->name, AudioEslinear_be, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_SLINEAR_BE;
		ae->precision = 16;
		ae->flags = 0;
		break;
	case 2:
		strlcpy(ae->name, AudioEslinear_le, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_SLINEAR_LE;
		ae->precision = 16;
		ae->flags = 0;
		break;
	case 3:
		strlcpy(ae->name, AudioEmulaw, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_ULAW;
		ae->precision = 8;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 4:
		strlcpy(ae->name, AudioEalaw, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_ALAW;
		ae->precision = 8;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 5:
		strlcpy(ae->name, AudioEulinear, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_ULINEAR;
		ae->precision = 16;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strlcpy(ae->name, AudioEulinear_le, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_ULINEAR_LE;
		ae->precision = 16;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strlcpy(ae->name, AudioEulinear_be, sizeof ae->name);
		ae->encoding = AUDIO_ENCODING_ULINEAR_BE;
		ae->precision = 16;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

void
awacs_mono16_to_stereo16(void *v, u_char *p, int cc)
{
	int x;
	int16_t *src, *dst;

	src = (void *)(p + cc);
	dst = (void *)(p + cc * 2);
	while (cc > 0) {
		x = *--src;
		*--dst = x;
		*--dst = x;
		cc -= 2;
	}
}

void
awacs_swap_bytes_mono16_to_stereo16(void *v, u_char *p, int cc)
{
	swap_bytes(v, p, cc);
	awacs_mono16_to_stereo16(v, p, cc);
}

void
awacs_cvt_ulinear_mono_16_le(void *v, u_char *p, int cc)
{
	swap_bytes_change_sign16_be(v, p, cc);
	awacs_mono16_to_stereo16(v, p, cc);
}

void
awacs_cvt_ulinear_mono_16_be(void *v, u_char *p, int cc)
{
	change_sign16_be(v, p, cc);
	awacs_mono16_to_stereo16(v, p, cc);
}

int
awacs_set_params(void *h, int setmode, int usemode, struct audio_params *play,
    struct audio_params *rec)
{
	struct awacs_softc *sc = h;
	struct audio_params *p;
	int mode;

	/*
	 * This device only has one clock, so make the sample rates match.
	 */
	if (play->sample_rate != rec->sample_rate &&
	    usemode == (AUMODE_PLAY | AUMODE_RECORD)) {
		if (setmode == AUMODE_PLAY) {
			rec->sample_rate = play->sample_rate;
			setmode |= AUMODE_RECORD;
		} else if (setmode == AUMODE_RECORD) {
			play->sample_rate = rec->sample_rate;
			setmode |= AUMODE_PLAY;
		} else
			return EINVAL;
	}

	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		p = mode == AUMODE_PLAY ? play : rec;

		if (p->sample_rate < 4000 || p->sample_rate > 50000 ||
		    (p->precision != 8 && p->precision != 16) ||
		    (p->channels != 1 && p->channels != 2))
			return EINVAL;

		p->factor = 1;
		p->sw_code = NULL;
		awacs_write_reg(sc, AWACS_BYTE_SWAP, 0);

		switch (p->encoding) {

		case AUDIO_ENCODING_SLINEAR_LE:
			if (p->channels == 2 && p->precision == 16) {
				p->sw_code = swap_bytes;
				break;
			}
			if (p->channels == 1 && p->precision == 16) {
				p->factor = 2;
				p->sw_code =
				    awacs_swap_bytes_mono16_to_stereo16;
				break;
			}
			return (EINVAL);
		case AUDIO_ENCODING_SLINEAR_BE:
			if (p->channels == 2 && p->precision == 16)
				break;
			if (p->channels == 1 && p->precision == 16) {
				p->factor = 2;
				p->sw_code = awacs_mono16_to_stereo16;
				break;
			}
			return (EINVAL);
		case AUDIO_ENCODING_ULINEAR_LE:
			if (p->channels == 2 && p->precision == 16) {
				p->sw_code = swap_bytes_change_sign16_be;
				break;
			}
			if (p->channels == 1 && p->precision == 16) {
				p->factor = 2;
				p->sw_code = awacs_cvt_ulinear_mono_16_le;
				break;
			}
			return (EINVAL);
		case AUDIO_ENCODING_ULINEAR_BE:
			if (p->channels == 2 && p->precision == 16) {
				p->sw_code = change_sign16_be;
				break;
			}
			if (p->channels == 1 && p->precision == 16) {
				p->factor = 2;
				p->sw_code = awacs_cvt_ulinear_mono_16_be;
				break;
			}
			return (EINVAL);
		case AUDIO_ENCODING_ULAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = mulaw_to_slinear16_be;
				break;
			} else
				break;	/* XXX */
			return (EINVAL);
		case AUDIO_ENCODING_ALAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = alaw_to_slinear16_be;
				break;
			}
			return (EINVAL);
		default:
			return (EINVAL);
		}
	}

	/* Set the speed */
	awacs_set_rate(sc, p);

	return (0);
}

int
awacs_round_blocksize(void *h, int size)
{
	if (size < PAGE_SIZE)
		size = PAGE_SIZE;
	return (size + PAGE_SIZE / 2) & ~(PGOFSET);
}

int
awacs_halt_output(void *h)
{
	struct awacs_softc *sc = h;

	dbdma_stop(sc->sc_odma);
	dbdma_reset(sc->sc_odma);
	dbdma_stop(sc->sc_odma);
	sc->sc_odmap = NULL;
	return 0;
}

int
awacs_halt_input(void *h)
{
	struct awacs_softc *sc = h;

	dbdma_stop(sc->sc_idma);
	dbdma_reset(sc->sc_idma);
	return 0;
}

int
awacs_getdev(void *h, struct audio_device *retp)
{
	*retp = awacs_device;
	return 0;
}

enum {
	AWACS_OUTPUT_SELECT,
	AWACS_VOL_SPEAKER,
	AWACS_VOL_HEADPHONE,
	AWACS_OUTPUT_CLASS,
	AWACS_MONITOR_CLASS,
	AWACS_INPUT_SELECT,
	AWACS_VOL_INPUT,
	AWACS_INPUT_CLASS,
	AWACS_RECORD_CLASS,
	AWACS_ENUM_LAST
};

int
awacs_set_port(void *h, mixer_ctrl_t *mc)
{
	struct awacs_softc *sc = h;
	int l, r;

	DPRINTF("awacs_set_port dev = %d, type = %d\n", mc->dev, mc->type);

	l = mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
	r = mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];

	switch (mc->dev) {
	case AWACS_OUTPUT_SELECT:
		/* no change necessary? */
		if (mc->un.mask == sc->sc_output_mask)
			return 0;
		sc->sc_codecctl1 |= AWACS_MUTE_SPEAKER | AWACS_MUTE_HEADPHONE;
		if (mc->un.mask & 1 << 0)
			sc->sc_codecctl1 &= ~AWACS_MUTE_SPEAKER;
		if (mc->un.mask & 1 << 1)
			sc->sc_codecctl1 &= ~AWACS_MUTE_HEADPHONE;

		awacs_write_codec(sc, sc->sc_codecctl1);
		sc->sc_output_mask = mc->un.mask;
		return 0;

	case AWACS_VOL_SPEAKER:
		awacs_set_speaker_volume(sc, l, r);
		return 0;

	case AWACS_VOL_HEADPHONE:
		awacs_set_ext_volume(sc, l, r);
		return 0;

	case AWACS_VOL_INPUT:
		sc->sc_codecctl0 &= ~0xff;
		sc->sc_codecctl0 |= (l & 0xf0) | (r >> 4);
		awacs_write_codec(sc, sc->sc_codecctl0);
		return 0;

	case AWACS_INPUT_SELECT:
		/* no change necessary? */
		if (mc->un.mask == sc->sc_record_source)
			return 0;
		switch(mc->un.mask) {
		case 1<<0: /* CD */
			sc->sc_codecctl0 &= ~AWACS_INPUT_MASK;
			sc->sc_codecctl0 |= AWACS_INPUT_CD;
			awacs_write_codec(sc, sc->sc_codecctl0);
			break;
		case 1<<1: /* microphone */
			sc->sc_codecctl0 &= ~AWACS_INPUT_MASK;
			sc->sc_codecctl0 |= AWACS_INPUT_MICROPHONE;
			awacs_write_codec(sc, sc->sc_codecctl0);
			break;
		case 1<<2: /* line in */
			sc->sc_codecctl0 &= ~AWACS_INPUT_MASK;
			sc->sc_codecctl0 |= AWACS_INPUT_LINE;
			awacs_write_codec(sc, sc->sc_codecctl0);
			break;
		default: /* invalid argument */
			return -1;
		}
		sc->sc_record_source = mc->un.mask;
		return 0;
	}

	return ENXIO;
}

int
awacs_get_port(void *h, mixer_ctrl_t *mc)
{
	struct awacs_softc *sc = h;
	int vol, l, r;

	DPRINTF("awacs_get_port dev = %d, type = %d\n", mc->dev, mc->type);

	switch (mc->dev) {
	case AWACS_OUTPUT_SELECT:
		mc->un.mask = sc->sc_output_mask;
		return 0;

	case AWACS_VOL_SPEAKER:
		vol = sc->sc_codecctl4;
		l = (15 - ((vol & 0x3c0) >> 6)) * 16;
		r = (15 - (vol & 0x0f)) * 16;
		mc->un.mask = 1 << 0;
		mc->un.value.num_channels = 2;
		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = l;
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
		return 0;

	case AWACS_VOL_HEADPHONE:
		vol = sc->sc_codecctl2;
		l = (15 - ((vol & 0x3c0) >> 6)) * 16;
		r = (15 - (vol & 0x0f)) * 16;
		mc->un.mask = 1 << 1;
		mc->un.value.num_channels = 2;
		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = l;
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
		return 0;

	case AWACS_INPUT_SELECT:
		mc->un.mask = sc->sc_record_source;
		return 0;

	case AWACS_VOL_INPUT:
		vol = sc->sc_codecctl0 & 0xff;
		l = (vol & 0xf0);
		r = (vol & 0x0f) << 4;
		mc->un.mask = sc->sc_record_source;
		mc->un.value.num_channels = 2;
		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = l;
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
		return 0;

	default:
		return ENXIO;
	}

	return 0;
}

int
awacs_query_devinfo(void *h, mixer_devinfo_t *dip)
{
	DPRINTF("query_devinfo %d\n", dip->index);

	switch (dip->index) {

	case AWACS_OUTPUT_SELECT:
		dip->mixer_class = AWACS_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioNselect, sizeof dip->label.name);
		dip->type = AUDIO_MIXER_SET;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.s.num_mem = 2;
		strlcpy(dip->un.s.member[0].label.name, AudioNspeaker,
		    sizeof dip->un.s.member[0].label.name);
		dip->un.s.member[0].mask = 1 << 0;
		strlcpy(dip->un.s.member[1].label.name, AudioNheadphone,
		    sizeof dip->un.s.member[0].label.name);
		dip->un.s.member[1].mask = 1 << 1;
		return 0;

	case AWACS_VOL_SPEAKER:
		dip->mixer_class = AWACS_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioNspeaker,
		    sizeof dip->label.name);
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		return 0;

	case AWACS_VOL_HEADPHONE:
		dip->mixer_class = AWACS_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioNheadphone,
		    sizeof dip->label.name);
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		return 0;

	case AWACS_INPUT_SELECT:
		dip->mixer_class = AWACS_RECORD_CLASS;
		strlcpy(dip->label.name, AudioNsource, sizeof dip->label.name);
		dip->type = AUDIO_MIXER_SET;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.s.num_mem = 3;
		strlcpy(dip->un.s.member[0].label.name, AudioNcd,
		    sizeof dip->un.s.member[0].label.name);
		dip->un.s.member[0].mask = 1 << 0;
		strlcpy(dip->un.s.member[1].label.name, AudioNmicrophone,
		    sizeof dip->un.s.member[1].label.name);
		dip->un.s.member[1].mask = 1 << 1;
		strlcpy(dip->un.s.member[2].label.name, AudioNline,
		    sizeof dip->un.s.member[2].label.name);
		dip->un.s.member[2].mask = 1 << 2;
		return 0;

	case AWACS_VOL_INPUT:
		dip->mixer_class = AWACS_RECORD_CLASS;
		strlcpy(dip->label.name, AudioNmaster, sizeof dip->label.name);
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		return 0;

	case AWACS_MONITOR_CLASS:
		dip->mixer_class = AWACS_MONITOR_CLASS;
		strlcpy(dip->label.name, AudioCmonitor, sizeof dip->label.name);
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case AWACS_OUTPUT_CLASS:
		dip->mixer_class = AWACS_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioCoutputs, sizeof dip->label.name);
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case AWACS_RECORD_CLASS:
		dip->mixer_class = AWACS_MONITOR_CLASS;
		strlcpy(dip->label.name, AudioCrecord, sizeof dip->label.name);
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;
	}

	return ENXIO;
}

size_t
awacs_round_buffersize(void *h, int dir, size_t size)
{
	size = (size + PGOFSET) & ~(PGOFSET);
	if (size > AWACS_DMALIST_MAX * AWACS_DMASEG_MAX)
		size = AWACS_DMALIST_MAX * AWACS_DMASEG_MAX;
	return (size);
}

void *
awacs_allocm(void *h, int dir, size_t size, int type, int flags)
{
	struct awacs_softc *sc = h;
	struct awacs_dma *p;
	int error;

	if (size > AWACS_DMALIST_MAX * AWACS_DMASEG_MAX)
		return (NULL);

	p = malloc(sizeof(*p), type, flags);
	if (!p)
		return (NULL);
	bzero(p, sizeof(*p));

	/* convert to the bus.h style, not used otherwise */
	if (flags & M_NOWAIT)
		flags = BUS_DMA_NOWAIT;

	p->size = size;
	if ((error = bus_dmamem_alloc(sc->sc_dmat, p->size, NBPG, 0, p->segs,
	    1, &p->nsegs, flags)) != 0) {
		printf("%s: unable to allocate dma, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		free(p, type);
		return NULL;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, p->segs, p->nsegs, p->size,
	    &p->addr, flags | BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map dma, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		bus_dmamem_free(sc->sc_dmat, p->segs, p->nsegs);
		free(p, type);
		return NULL;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, p->size, 1,
	    p->size, 0, flags, &p->map)) != 0) {
		printf("%s: unable to create dma map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		bus_dmamem_unmap(sc->sc_dmat, p->addr, size);
		bus_dmamem_free(sc->sc_dmat, p->segs, p->nsegs);
		free(p, type);
		return NULL;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, p->map, p->addr, p->size,
	    NULL, flags)) != 0) {
		printf("%s: unable to load dma map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		bus_dmamap_destroy(sc->sc_dmat, p->map);
		bus_dmamem_unmap(sc->sc_dmat, p->addr, size);
		bus_dmamem_free(sc->sc_dmat, p->segs, p->nsegs);
		free(p, type);
		return NULL;
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;

	return p->addr;
}

paddr_t
awacs_mappage(void *h, void *mem, off_t off, int prot)
{
	if (off < 0)
		return -1;
	return -1;	/* XXX */
}

int
awacs_get_props(void *h)
{
	return AUDIO_PROP_FULLDUPLEX /* | AUDIO_PROP_MMAP */;
}

int
awacs_trigger_output(void *h, void *start, void *end, int bsize,
    void (*intr)(void *), void *arg, struct audio_params *param)
{
	struct awacs_softc *sc = h;
	struct awacs_dma *p;
	struct dbdma_command *cmd = sc->sc_odmacmd;
	vaddr_t spa, pa, epa;
	int c;

	DPRINTF("trigger_output %p %p 0x%x\n", start, end, bsize);

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next);
	if (!p)
		return -1;

	sc->sc_ointr = intr;
	sc->sc_oarg = arg;
	sc->sc_odmap = sc->sc_odmacmd;

	spa = p->segs[0].ds_addr;
	c = DBDMA_CMD_OUT_MORE;
	for (pa = spa, epa = spa + (end - start);
	    pa < epa; pa += bsize, cmd++) {

		if (pa + bsize == epa)
			c = DBDMA_CMD_OUT_LAST;

		DBDMA_BUILD(cmd, c, 0, bsize, pa, DBDMA_INT_ALWAYS,
			DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	}

	DBDMA_BUILD(cmd, DBDMA_CMD_NOP, 0, 0, 0,
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_ALWAYS);
	dbdma_st32(&cmd->d_cmddep, sc->sc_odbdma->d_paddr);

	dbdma_start(sc->sc_odma, sc->sc_odbdma);

	return 0;
}

int
awacs_trigger_input(void *h, void *start, void *end, int bsize,
    void (*intr)(void *), void *arg, struct audio_params *param)
{
	struct awacs_softc *sc = h;
	struct awacs_dma *p;
	struct dbdma_command *cmd = sc->sc_idmacmd;
	vaddr_t spa, pa, epa;
	int c;

	printf("trigger_input %p %p 0x%x\n", start, end, bsize);

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next);
	if (!p)
		return -1;

	sc->sc_iintr = intr;
	sc->sc_iarg = arg;
	sc->sc_idmap = sc->sc_idmacmd;

	spa = p->segs[0].ds_addr;
	c = DBDMA_CMD_IN_MORE;
	for (pa = spa, epa = spa + (end - start);
	    pa < epa; pa += bsize, cmd++) {

		if (pa + bsize == epa)
			c = DBDMA_CMD_IN_LAST;

		DBDMA_BUILD(cmd, c, 0, bsize, pa, DBDMA_INT_ALWAYS,
			DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	}

	DBDMA_BUILD(cmd, DBDMA_CMD_NOP, 0, 0, 0,
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_ALWAYS);
	dbdma_st32(&cmd->d_cmddep, sc->sc_idbdma->d_paddr);

	dbdma_start(sc->sc_idma, sc->sc_idbdma);

	return 0;
}

void
awacs_set_speaker_volume(struct awacs_softc *sc, int left, int  right)
{
	int lval = 15 - (left  & 0xff) / 16;
	int rval = 15 - (right & 0xff) / 16;

	DPRINTF("speaker_volume %d %d\n", lval, rval);

	sc->sc_codecctl4 &= ~0x3cf;
	sc->sc_codecctl4 |= (lval << 6) | rval;
	awacs_write_codec(sc, sc->sc_codecctl4);
}

void
awacs_set_ext_volume(struct awacs_softc *sc, int left, int  right)
{
	int lval = 15 - (left  & 0xff) / 16;
	int rval = 15 - (right & 0xff) / 16;

	DPRINTF("ext_volume %d %d\n", lval, rval);

	sc->sc_codecctl2 &= ~0x3cf;
	sc->sc_codecctl2 |= (lval << 6) | rval;
	awacs_write_codec(sc, sc->sc_codecctl2);
}

void
awacs_set_rate(struct awacs_softc *sc, struct audio_params *p)
{
	int selected = -1;
	size_t n, i;

	n = sizeof(awacs_speeds)/sizeof(awacs_speeds[0]);

	if (p->sample_rate < awacs_speeds[0].rate)
		selected = 0;
	if (p->sample_rate > awacs_speeds[n - 1].rate)
		selected = n - 1;

	for (i = 1; selected == -1 && i < n; i++) {
		if (p->sample_rate == awacs_speeds[i].rate)
			selected = i;
		else if (p->sample_rate < awacs_speeds[i].rate) {
			u_int diff1, diff2;

			diff1 = p->sample_rate - awacs_speeds[i - 1].rate;
			diff2 = awacs_speeds[i].rate - p->sample_rate;
			selected = (diff1 < diff2) ? i - 1 : i;
		}
	}

	if (selected == -1)
		selected = 0;

	sc->sc_soundctl &= ~AWACS_RATE_MASK;
	sc->sc_soundctl |= awacs_speeds[selected].bits;
	p->sample_rate = awacs_speeds[selected].rate;
	awacs_write_reg(sc, AWACS_SOUND_CTRL, sc->sc_soundctl);
}
