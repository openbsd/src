/*	$OpenBSD: awacs.c,v 1.1 2001/09/01 15:50:00 drahn Exp $	*/
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
#include <sys/types.h>

#include <dev/auconv.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <uvm/uvm.h>

#include <machine/autoconf.h>
#include <machine/pio.h>
#include <macppc/mac/dbdma.h>

#ifdef AWACS_DEBUG
# define DPRINTF printf
#else
# define DPRINTF while (0) printf
#endif

struct awacs_softc {
	struct device sc_dev;

	void (*sc_ointr)(void *);	/* dma completion intr handler */
	void *sc_oarg;			/* arg for sc_ointr() */
	int sc_opages;			/* # of output pages */

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

	struct dbdma_regmap *sc_odma;
	struct dbdma_regmap *sc_idma;
	struct dbdma_command *sc_odmacmd;
	struct dbdma_command *sc_idmacmd;
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
int awacs_mappage(void *, void *, int, int);
int awacs_get_props(void *);

static inline u_int awacs_read_reg(struct awacs_softc *, int);
static inline void awacs_write_reg(struct awacs_softc *, int, int);
void awacs_write_codec(struct awacs_softc *, int);
void awacs_set_speaker_volume(struct awacs_softc *, int, int);
void awacs_set_ext_volume(struct awacs_softc *, int, int);
int awacs_set_rate(struct awacs_softc *, int);
void awacs_mono16_to_stereo16 __P((void *v, u_char *p, int cc));

struct cfattach awacs_ca = {
	sizeof(struct awacs_softc), awacs_match, awacs_attach
};

struct cfdriver awacs_cd = {
	NULL, "awacs", DV_DULL
};

struct audio_hw_if awacs_hw_if = {
	awacs_open,
	awacs_close,
	NULL,
	awacs_query_encoding,
	awacs_set_params,
	awacs_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	awacs_halt_output,
	awacs_halt_input,
	NULL,
	awacs_getdev,
	NULL,
	awacs_set_port,
	awacs_get_port,
	awacs_query_devinfo,
	NULL,
	NULL,
	NULL,
	awacs_mappage,
	awacs_get_props,
	awacs_trigger_output,
	awacs_trigger_input,
	NULL,
	awacs_round_buffersize,

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

int
awacs_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, "awacs") != 0 &&
	    strcmp(ca->ca_name, "davbus") != 0)
		return 0;

	printf("awacs: matched %s nreg %d nintr %d\n",
		ca->ca_name, ca->ca_nreg, ca->ca_nintr);

	if (ca->ca_nreg < 24 || ca->ca_nintr < 12)
		return 0;

	/* XXX for now
	if (ca->ca_nintr > 12)
		return 0;
	*/

	return 1;
}

void
awacs_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct awacs_softc *sc = (struct awacs_softc *)self;
	struct confargs *ca = aux;
	int cirq, oirq, iirq;
	int cirq_type, oirq_type, iirq_type;

	ca->ca_reg[0] += ca->ca_baseaddr;
	ca->ca_reg[2] += ca->ca_baseaddr;
	ca->ca_reg[4] += ca->ca_baseaddr;

	sc->sc_reg = mapiodev(ca->ca_reg[0], ca->ca_reg[1]);

	sc->sc_odma = mapiodev(ca->ca_reg[2], ca->ca_reg[3]); /* out */
	sc->sc_idma = mapiodev(ca->ca_reg[4], ca->ca_reg[5]); /* in */
	sc->sc_odmacmd = dbdma_alloc(20 * sizeof(struct dbdma_command));
	sc->sc_idmacmd = dbdma_alloc(20 * sizeof(struct dbdma_command));

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
#if 0
	/* do not use this for now, since both are tied to same freq
	 * we can service both in the same interrupt, lowering
	 * interrupt load by half
	 */
	mac_intr_establish(parent, iirq, irq_type, IPL_AUDIO, awacs_intr,
		sc, "awacs/rx");
#endif

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
awacs_read_reg(sc, reg)
	struct awacs_softc *sc;
	int reg;
{
	char *addr = sc->sc_reg;

	return in32rb(addr + reg);
}

void
awacs_write_reg(sc, reg, val)
	struct awacs_softc *sc;
	int reg, val;
{
	char *addr = sc->sc_reg;

	out32rb(addr + reg, val);
}

void
awacs_write_codec(sc, value)
	struct awacs_softc *sc;
	int value;
{
	awacs_write_reg(sc, AWACS_CODEC_CTRL, value);
	while (awacs_read_reg(sc, AWACS_CODEC_CTRL) & AWACS_CODEC_BUSY);
}

int
awacs_intr(v)
	void *v;
{
	int reason;
	struct awacs_softc *sc = v;
	reason = awacs_read_reg(sc, AWACS_SOUND_CTRL);

	if (reason & AWACS_CTL_CNTRLERR) {
		/* change outputs ?? */
		printf("should change inputs\n");
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
awacs_tx_intr(v)
	void *v;
{
	struct awacs_softc *sc = v;
	struct dbdma_command *cmd = sc->sc_odmacmd;
	int count = sc->sc_opages;
	int status;

	/* Fill used buffer(s). */
	while (count-- > 0) {
		/* if DBDMA_INT_ALWAYS */
		if (in16rb(&cmd->d_command) & 0x30) {	/* XXX */
			status = in16rb(&cmd->d_status);
			cmd->d_status = 0;
			if (status)	/* status == 0x8400 */
				if (sc->sc_ointr)
					(*sc->sc_ointr)(sc->sc_oarg);
		}
		cmd++;
	}

	return 1;
}

int
awacs_open(h, flags)
	void *h;
	int flags;
{
	return 0;
}

/*
 * Close function is called at splaudio().
 */
void
awacs_close(h)
	void *h;
{
	struct awacs_softc *sc = h;

	awacs_halt_output(sc);
	awacs_halt_input(sc);

	sc->sc_ointr = 0;
	sc->sc_iintr = 0;
}

int
awacs_query_encoding(h, ae)
	void *h;
	struct audio_encoding *ae;
{
	switch (ae->index) {
	case 0:
		strcpy(ae->name, AudioEslinear);
		ae->encoding = AUDIO_ENCODING_SLINEAR;
		ae->precision = 16;
		ae->flags = 0;
		return 0;
	case 1:
		strcpy(ae->name, AudioEslinear_be);
		ae->encoding = AUDIO_ENCODING_SLINEAR_BE;
		ae->precision = 16;
		ae->flags = 0;
		return 0;
	case 2:
		strcpy(ae->name, AudioEslinear_le);
		ae->encoding = AUDIO_ENCODING_SLINEAR_LE;
		ae->precision = 16;
		ae->flags = 0;
		return 0;
	case 3:
		strcpy(ae->name, AudioEmulaw);
		ae->encoding = AUDIO_ENCODING_ULAW;
		ae->precision = 8;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 4:
		strcpy(ae->name, AudioEalaw);
		ae->encoding = AUDIO_ENCODING_ALAW;
		ae->precision = 8;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	default:
		return EINVAL;
	}
}

void
awacs_mono16_to_stereo16(v, p, cc)
	void *v;
	u_char *p;
	int cc;
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

int
awacs_set_params(h, setmode, usemode, play, rec)
	void *h;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct awacs_softc *sc = h;
	struct audio_params *p;
	int mode, rate;

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
		p->sw_code = 0;
		awacs_write_reg(sc, AWACS_BYTE_SWAP, 0);

		switch (p->encoding) {

		case AUDIO_ENCODING_SLINEAR_LE:
			awacs_write_reg(sc, AWACS_BYTE_SWAP, 1);
		case AUDIO_ENCODING_SLINEAR_BE:
			if (p->channels == 1) {
				p->factor = 2;
				p->sw_code = awacs_mono16_to_stereo16;
				break;
			}
			if (p->precision != 16)
				return EINVAL;
				/* p->sw_code = change_sign8; */
			break;

		case AUDIO_ENCODING_ULINEAR_LE:
			awacs_write_reg(sc, AWACS_BYTE_SWAP, 1);
		case AUDIO_ENCODING_ULINEAR_BE:
			if (p->precision == 16)
				p->sw_code = change_sign16_be;
			else
				return EINVAL;
			break;

		case AUDIO_ENCODING_ULAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = mulaw_to_slinear16_be;
			} else
				p->sw_code = ulinear8_to_mulaw;
			break;

		case AUDIO_ENCODING_ALAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = alaw_to_slinear16_be;
			} else
				p->sw_code = ulinear8_to_alaw;
			break;

		default:
			return EINVAL;
		}
	}

	/* Set the speed */
	rate = p->sample_rate;

	awacs_set_rate(sc, rate);

	return 0;
}

int
awacs_round_blocksize(h, size)
	void *h;
	int size;
{
	if (size < NBPG)
		size = NBPG;
	return size & ~PGOFSET;
}

int
awacs_halt_output(h)
	void *h;
{
	struct awacs_softc *sc = h;

	dbdma_stop(sc->sc_odma);
	dbdma_reset(sc->sc_odma);
	dbdma_stop(sc->sc_odma);
	return 0;
}

int
awacs_halt_input(h)
	void *h;
{
	struct awacs_softc *sc = h;

	dbdma_stop(sc->sc_idma);
	dbdma_reset(sc->sc_idma);
	return 0;
}

int
awacs_getdev(h, retp)
	void *h;
	struct audio_device *retp;
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
awacs_set_port(h, mc)
	void *h;
	mixer_ctrl_t *mc;
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
		switch(mc->un.mask) {
		case 1<<0: /* speaker */
			sc->sc_codecctl1 &= ~AWACS_MUTE_SPEAKER;
			sc->sc_codecctl1 |= AWACS_MUTE_HEADPHONE;
			awacs_write_codec(sc, sc->sc_codecctl1);
			break;
		case 1<<1: /* headphones */
			sc->sc_codecctl1 |= AWACS_MUTE_SPEAKER;
			sc->sc_codecctl1 &= ~AWACS_MUTE_HEADPHONE;
			awacs_write_codec(sc, sc->sc_codecctl1);
			break;
		default: /* invalid argument */
			return -1;
		}
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
awacs_get_port(h, mc)
	void *h;
	mixer_ctrl_t *mc;
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
awacs_query_devinfo(h, dip)
	void *h;
	mixer_devinfo_t *dip;
{

	DPRINTF("query_devinfo %d\n", dip->index);

	switch (dip->index) {

	case AWACS_OUTPUT_SELECT:
		dip->mixer_class = AWACS_MONITOR_CLASS;
		strcpy(dip->label.name, AudioNoutput);
		dip->type = AUDIO_MIXER_SET;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.s.num_mem = 2;
		strcpy(dip->un.s.member[0].label.name, AudioNspeaker);
		dip->un.s.member[0].mask = 1 << 0;
		strcpy(dip->un.s.member[1].label.name, AudioNheadphone);
		dip->un.s.member[1].mask = 1 << 1;
		return 0;

	case AWACS_VOL_SPEAKER:
		dip->mixer_class = AWACS_OUTPUT_CLASS;
		strcpy(dip->label.name, AudioNspeaker);
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case AWACS_VOL_HEADPHONE:
		dip->mixer_class = AWACS_OUTPUT_CLASS;
		strcpy(dip->label.name, AudioNheadphone);
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case AWACS_INPUT_SELECT:
		dip->mixer_class = AWACS_MONITOR_CLASS;
		strcpy(dip->label.name, AudioNinput);
		dip->type = AUDIO_MIXER_SET;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.s.num_mem = 3;
		strcpy(dip->un.s.member[0].label.name, AudioNcd);
		dip->un.s.member[0].mask = 1 << 0;
		strcpy(dip->un.s.member[1].label.name, AudioNmicrophone);
		dip->un.s.member[1].mask = 1 << 1;
		strcpy(dip->un.s.member[2].label.name, AudioNline);
		dip->un.s.member[2].mask = 1 << 2;
		return 0;

	case AWACS_VOL_INPUT:
		dip->mixer_class = AWACS_INPUT_CLASS;
		strcpy(dip->label.name, AudioNmaster);
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case AWACS_MONITOR_CLASS:
		dip->mixer_class = AWACS_MONITOR_CLASS;
		strcpy(dip->label.name, AudioCmonitor);
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case AWACS_OUTPUT_CLASS:
		dip->mixer_class = AWACS_OUTPUT_CLASS;
		strcpy(dip->label.name, AudioCoutputs);
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case AWACS_RECORD_CLASS:
		dip->mixer_class = AWACS_MONITOR_CLASS;
		strcpy(dip->label.name, AudioCrecord);
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case AWACS_INPUT_CLASS:
		dip->mixer_class = AWACS_INPUT_CLASS;
		strcpy(dip->label.name, AudioCinputs);
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;
	}

	return ENXIO;
}

size_t
awacs_round_buffersize(h, dir, size)
	void *h;
	int dir;
	size_t size;
{
	if (size > 65536)
		size = 65536;
	return size;
}

int
awacs_mappage(h, mem, off, prot)
	void *h;
	void *mem;
	int off;
	int prot;
{
	if (off < 0)
		return -1;
	return -1;	/* XXX */
}

int
awacs_get_props(h)
	void *h;
{
	return AUDIO_PROP_FULLDUPLEX /* | AUDIO_PROP_MMAP */;
}

int
awacs_trigger_output(h, start, end, bsize, intr, arg, param)
	void *h;
	void *start, *end;
	int bsize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct awacs_softc *sc = h;
	struct dbdma_command *cmd = sc->sc_odmacmd;
	vaddr_t va;
	int i, len, intmode;

	DPRINTF("trigger_output %p %p 0x%x\n", start, end, bsize);

	sc->sc_ointr = intr;
	sc->sc_oarg = arg;
	sc->sc_opages = ((char *)end - (char *)start) / NBPG;

#ifdef DIAGNOSTIC
	if (sc->sc_opages > 16)
		panic("awacs_trigger_output");
#endif

	va = (vaddr_t)start;
	len = 0;
	for (i = sc->sc_opages; i > 0; i--) {
		len += NBPG;
		if (len < bsize)
			intmode = DBDMA_INT_NEVER;
		else {
			len = 0;
			intmode = DBDMA_INT_ALWAYS;
		}

		DBDMA_BUILD(cmd, DBDMA_CMD_OUT_MORE, 0, NBPG, vtophys(va),
			intmode, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
		va += NBPG;
		cmd++;
	}

	DBDMA_BUILD(cmd, DBDMA_CMD_NOP, 0, 0, 0,
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_ALWAYS);
	dbdma_st32(&cmd->d_cmddep, vtophys((vaddr_t)sc->sc_odmacmd));

	dbdma_start(sc->sc_odma, sc->sc_odmacmd);

	return 0;
}

int
awacs_trigger_input(h, start, end, bsize, intr, arg, param)
	void *h;
	void *start, *end;
	int bsize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	printf("awacs_trigger_input called\n");

	return 1;
}

void
awacs_set_speaker_volume(sc, left, right)
	struct awacs_softc *sc;
	int left, right;
{
	int lval = 15 - (left  & 0xff) / 16;
	int rval = 15 - (right & 0xff) / 16;

	DPRINTF("speaker_volume %d %d\n", lval, rval);

	sc->sc_codecctl4 &= ~0x3cf;
	sc->sc_codecctl4 |= (lval << 6) | rval;
	awacs_write_codec(sc, sc->sc_codecctl4);
}

void
awacs_set_ext_volume(sc, left, right)
	struct awacs_softc *sc;
	int left, right;
{
	int lval = 15 - (left  & 0xff) / 16;
	int rval = 15 - (right & 0xff) / 16;

	DPRINTF("ext_volume %d %d\n", lval, rval);

	sc->sc_codecctl2 &= ~0x3cf;
	sc->sc_codecctl2 |= (lval << 6) | rval;
	awacs_write_codec(sc, sc->sc_codecctl2);
}

int
awacs_set_rate(sc, rate)
	struct awacs_softc *sc;
	int rate;
{
	int c;

	switch (rate) {

	case 44100:
		c = AWACS_RATE_44100;
		break;
	case 29400:
		c = AWACS_RATE_29400;
		break;
	case 22050:
		c = AWACS_RATE_22050;
		break;
	case 17640:
		c = AWACS_RATE_17640;
		break;
	case 14700:
		c = AWACS_RATE_14700;
		break;
	case 11025:
		c = AWACS_RATE_11025;
		break;
	case 8820:
		c = AWACS_RATE_8820;
		break;
	case 7350:
		c = AWACS_RATE_7350;
		break;
	default:
		return -1;
	}

	sc->sc_soundctl &= ~AWACS_RATE_MASK;
	sc->sc_soundctl |= c;
	awacs_write_reg(sc, AWACS_SOUND_CTRL, sc->sc_soundctl);

	return 0;
}
