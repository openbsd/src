/*	$OpenBSD: i2s.c,v 1.5 2005/12/11 20:56:01 kettenis Exp $	*/
/*	$NetBSD: i2s.c,v 1.1 2003/12/27 02:19:34 grant Exp $	*/

/*-
 * Copyright (c) 2002 Tsubai Masanari.  All rights reserved.
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
#include <dev/ofw/openfirm.h>
#include <macppc/dev/dbdma.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/pio.h>

#include <macppc/dev/i2svar.h>

#ifdef I2S_DEBUG
# define DPRINTF(x) printf x 
#else
# define DPRINTF(x)
#endif

struct i2s_mode *i2s_find_mode(u_int, u_int, u_int);
void i2s_cs16mts(void *, u_char *, int);

static int gpio_read(char *);
static void gpio_write(char *, int);
void i2s_mute_speaker(struct i2s_softc *, int);
void i2s_mute_headphone(struct i2s_softc *, int);
void i2s_mute_lineout(struct i2s_softc *, int);
int i2s_cint(void *);
u_char *i2s_gpio_map(struct i2s_softc *, char *, int *);
void i2s_init(struct i2s_softc *, int);

static void mono16_to_stereo16(void *, u_char *, int);
static void swap_bytes_mono16_to_stereo16(void *, u_char *, int);

/* XXX */
void keylargo_fcr_enable(int, u_int32_t);
void keylargo_fcr_disable(int, u_int32_t);

struct cfdriver i2s_cd = {
	NULL, "i2s", DV_DULL
};

static u_char *amp_mute;
static u_char *headphone_mute;
static u_char *lineout_mute;
static u_char *audio_hw_reset;
static u_char *headphone_detect;
static int headphone_detect_active;
static u_char *lineout_detect;
static int lineout_detect_active;


/* I2S registers */
#define I2S_INT		0x00
#define I2S_FORMAT	0x10
#define I2S_FRAMECOUNT	0x40
#define I2S_FRAMEMATCH	0x50
#define I2S_WORDSIZE	0x60

/* I2S_INT register definitions */
#define I2SClockOffset		0x3c
#define I2S_INT_CLKSTOPPEND	0x01000000

/* FCR(0x3c) bits */
#define I2S0CLKEN	0x1000
#define I2S0EN		0x2000
#define I2S1CLKEN	0x080000
#define I2S1EN		0x100000

/* GPIO bits */
#define GPIO_OUTSEL	0xf0	/* Output select */
		/*	0x00	GPIO bit0 is output
			0x10	media-bay power
			0x20	reserved
			0x30	MPIC */

#define GPIO_ALTOE	0x08	/* Alternate output enable */
		/*	0x00	Use DDR
			0x08	Use output select */

#define GPIO_DDR	0x04	/* Data direction */
#define GPIO_DDR_OUTPUT	0x04	/* Output */
#define GPIO_DDR_INPUT	0x00	/* Input */

#define GPIO_LEVEL	0x02	/* Pin level (RO) */

#define	GPIO_DATA	0x01	/* Data */

void
i2s_attach(struct device *parent, struct i2s_softc *sc, struct confargs *ca)
{
	int cirq, oirq, iirq, cirq_type, oirq_type, iirq_type;
	u_int32_t reg[6], intr[6];

	sc->sc_node = OF_child(ca->ca_node);
	sc->sc_baseaddr = ca->ca_baseaddr;

	OF_getprop(sc->sc_node, "reg", reg, sizeof reg);
	reg[0] += sc->sc_baseaddr;
	reg[2] += sc->sc_baseaddr;
	reg[4] += sc->sc_baseaddr;

	sc->sc_reg = mapiodev(reg[0], reg[1]);

	sc->sc_dmat = ca->ca_dmat;
	sc->sc_odma = mapiodev(reg[2], reg[3]); /* out */
	sc->sc_idma = mapiodev(reg[4], reg[5]); /* in */
	sc->sc_odbdma = dbdma_alloc(sc->sc_dmat, I2S_DMALIST_MAX);
	sc->sc_odmacmd = sc->sc_odbdma->d_addr;
	sc->sc_idbdma = dbdma_alloc(sc->sc_dmat, I2S_DMALIST_MAX);
	sc->sc_idmacmd = sc->sc_idbdma->d_addr;

	OF_getprop(sc->sc_node, "interrupts", intr, sizeof intr);
	cirq = intr[0];
	oirq = intr[2];
	iirq = intr[4];
	cirq_type = intr[1] ? IST_LEVEL : IST_EDGE;
	oirq_type = intr[3] ? IST_LEVEL : IST_EDGE;
	iirq_type = intr[5] ? IST_LEVEL : IST_EDGE;

	/* intr_establish(cirq, cirq_type, IPL_AUDIO, i2s_intr, sc); */
	mac_intr_establish(parent, oirq, oirq_type, IPL_AUDIO, i2s_intr,
	    sc, "i2s");
	/* intr_establish(iirq, iirq_type, IPL_AUDIO, i2s_intr, sc); */

	printf(": irq %d,%d,%d\n", cirq, oirq, iirq);

	i2s_set_rate(sc, 44100);
	i2s_gpio_init(sc, ca->ca_node, parent);
}

int
i2s_intr(v)
	void *v;
{
	struct i2s_softc *sc = v;
	struct dbdma_command *cmd = sc->sc_odmap;
	u_int16_t c, status;

	/* if not set we are not running */
	if (!cmd)
		return (0);
	DPRINTF(("i2s_intr: cmd %x\n", cmd));

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

	return 1;
}

int
i2s_open(h, flags)
	void *h;
	int flags;
{
	return 0;
}

/*
 * Close function is called at splaudio().
 */
void
i2s_close(h)
	void *h;
{
	struct i2s_softc *sc = h;

	i2s_halt_output(sc);
	i2s_halt_input(sc);

	sc->sc_ointr = 0;
	sc->sc_iintr = 0;
}

int
i2s_query_encoding(h, ae)
	void *h;
	struct audio_encoding *ae;
{
	int err = 0;

	switch (ae->index) {
	case 0:
		strlcpy(ae->name, AudioEslinear, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_SLINEAR;
		ae->precision = 16;
		ae->flags = 0;
		break;
	case 1:
		strlcpy(ae->name, AudioEslinear_be, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_SLINEAR_BE;
		ae->precision = 16;
		ae->flags = 0;
		break;
	case 2:
		strlcpy(ae->name, AudioEslinear_le, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_SLINEAR_LE;
		ae->precision = 16;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 3:
		strlcpy(ae->name, AudioEulinear_be, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_ULINEAR_BE;
		ae->precision = 16;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 4:
		strlcpy(ae->name, AudioEulinear_le, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_ULINEAR_LE;
		ae->precision = 16;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 5:
		strlcpy(ae->name, AudioEmulaw, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_ULAW;
		ae->precision = 8;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strlcpy(ae->name, AudioEalaw, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_ALAW;
		ae->precision = 8;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strlcpy(ae->name, AudioEslinear, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_SLINEAR;
		ae->precision = 8;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 8:
		strlcpy(ae->name, AudioEulinear, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_ULINEAR;
		ae->precision = 8;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	default:
		err = EINVAL;
		break;
	}
	return (err);
}

static void
mono16_to_stereo16(v, p, cc)
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

static void
swap_bytes_mono16_to_stereo16(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	swap_bytes(v, p, cc);
	mono16_to_stereo16(v, p, cc);
}

void
i2s_cs16mts(void *v, u_char *p, int cc)
{
	mono16_to_stereo16(v, p, cc);
	change_sign16_be(v, p, cc * 2);
}

struct i2s_mode {
	u_int encoding;
	u_int precision;
	u_int channels;
	void (*sw_code)(void *, u_char *, int);
	int factor;
} i2s_modes[] = {
	{ AUDIO_ENCODING_SLINEAR_LE,  8, 1, linear8_to_linear16_be_mts, 4 },
	{ AUDIO_ENCODING_SLINEAR_LE,  8, 2, linear8_to_linear16_be, 2 },
	{ AUDIO_ENCODING_SLINEAR_LE, 16, 1, swap_bytes_mono16_to_stereo16, 2 },
	{ AUDIO_ENCODING_SLINEAR_LE, 16, 2, swap_bytes, 1 },
	{ AUDIO_ENCODING_SLINEAR_BE,  8, 1, linear8_to_linear16_be_mts, 4 },
	{ AUDIO_ENCODING_SLINEAR_BE,  8, 2, linear8_to_linear16_be, 2 },
	{ AUDIO_ENCODING_SLINEAR_BE, 16, 1, mono16_to_stereo16, 2 },
	{ AUDIO_ENCODING_SLINEAR_BE, 16, 2, NULL, 1 },
	{ AUDIO_ENCODING_ULINEAR_LE,  8, 1, ulinear8_to_linear16_be_mts, 4 },
	{ AUDIO_ENCODING_ULINEAR_LE,  8, 2, ulinear8_to_linear16_be, 2 },
	{ AUDIO_ENCODING_ULINEAR_LE, 16, 1, change_sign16_swap_bytes_le_mts, 2 },
	{ AUDIO_ENCODING_ULINEAR_LE, 16, 2, swap_bytes_change_sign16_be, 1 },
	{ AUDIO_ENCODING_ULINEAR_BE,  8, 1, ulinear8_to_linear16_be_mts, 4 },
	{ AUDIO_ENCODING_ULINEAR_BE,  8, 2, ulinear8_to_linear16_be, 2 },
	{ AUDIO_ENCODING_ULINEAR_BE, 16, 1, i2s_cs16mts, 2 },
	{ AUDIO_ENCODING_ULINEAR_BE, 16, 2, change_sign16_be, 1 }
};


struct i2s_mode *
i2s_find_mode(u_int encoding, u_int precision, u_int channels)
{
	struct i2s_mode *m;
	int i;

	for (i = 0; i < sizeof(i2s_modes)/sizeof(i2s_modes[0]); i++) {
		m = &i2s_modes[i];
		if (m->encoding == encoding &&
		    m->precision == precision &&
		    m->channels == channels)
			return (m);
	}
	return (NULL);
}

int
i2s_set_params(h, setmode, usemode, play, rec)
	void *h;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct i2s_mode *m;
	struct i2s_softc *sc = h;
	struct audio_params *p;
	int mode;

	p = play; /* default to play */

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

		switch (p->encoding) {
		case AUDIO_ENCODING_SLINEAR_LE:
		case AUDIO_ENCODING_SLINEAR_BE:
		case AUDIO_ENCODING_ULINEAR_LE:
		case AUDIO_ENCODING_ULINEAR_BE:
			m = i2s_find_mode(p->encoding, p->precision,
			    p->channels);
			if (m == NULL) {
				printf("mode not found: %u/%u/%u\n",
				    p->encoding, p->precision, p->channels);
				return (EINVAL);
			}
			p->factor = m->factor;
			p->sw_code = m->sw_code;
			break;

		case AUDIO_ENCODING_ULAW:
			if (mode == AUMODE_PLAY) {
				if (p->channels == 1) {
					p->factor = 4;
					p->sw_code = mulaw_to_slinear16_be_mts;
					break;
				}
				if (p->channels == 2) {
					p->factor = 2;
					p->sw_code = mulaw_to_slinear16_be;
					break;
				}
			} else
				break; /* XXX */
			return (EINVAL);

		case AUDIO_ENCODING_ALAW:
			if (mode == AUMODE_PLAY) {
				if (p->channels == 1) {
					p->factor = 4;
					p->sw_code = alaw_to_slinear16_be_mts;
					break;
				}
				if (p->channels == 2) {
					p->factor = 2;
					p->sw_code = alaw_to_slinear16_be;
					break;
				}
			} else
				break; /* XXX */
			return (EINVAL);

		default:
			return (EINVAL);
		}
	}

	/* Set the speed */
	if (i2s_set_rate(sc, play->sample_rate))
		return EINVAL;

	p->sample_rate = sc->sc_rate;

	return 0;
}

int
i2s_round_blocksize(h, size)
	void *h;
	int size;
{
	if (size < NBPG)
		size = NBPG;
	return size & ~PGOFSET;
}

int
i2s_halt_output(h)
	void *h;
{
	struct i2s_softc *sc = h;

	dbdma_stop(sc->sc_odma);
	dbdma_reset(sc->sc_odma);
	return 0;
}

int
i2s_halt_input(h)
	void *h;
{
	struct i2s_softc *sc = h;

	dbdma_stop(sc->sc_idma);
	dbdma_reset(sc->sc_idma);
	return 0;
}

enum {
	I2S_MONITOR_CLASS,
	I2S_OUTPUT_CLASS,
	I2S_RECORD_CLASS,
	I2S_OUTPUT_SELECT,
	I2S_VOL_OUTPUT,
	I2S_INPUT_SELECT,
	I2S_VOL_INPUT,
	I2S_BASS,
	I2S_TREBLE,
	I2S_ENUM_LAST
};

int
i2s_set_port(h, mc)
	void *h;
	mixer_ctrl_t *mc;
{
	struct i2s_softc *sc = h;
	int l, r;

	DPRINTF(("i2s_set_port dev = %d, type = %d\n", mc->dev, mc->type));

	l = mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
	r = mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];

	switch (mc->dev) {
	case I2S_OUTPUT_SELECT:
		/* No change necessary? */
		if (mc->un.mask == sc->sc_output_mask)
			return 0;

		i2s_mute_speaker(sc, 1);
		i2s_mute_headphone(sc, 1);
		i2s_mute_lineout(sc, 1);
		if (mc->un.mask & 1 << 0)
			i2s_mute_speaker(sc, 0);
		if (mc->un.mask & 1 << 1)
			i2s_mute_headphone(sc, 0);
		if (mc->un.mask & 1 << 2)
			i2s_mute_lineout(sc, 0);

		sc->sc_output_mask = mc->un.mask;
		return 0;

	case I2S_VOL_OUTPUT:
		(*sc->sc_setvolume)(sc, l, r);
		return 0;

	case I2S_BASS:
		if (sc->sc_setbass != NULL)
			(*sc->sc_setbass)(sc, l);
		return (0);

	case I2S_TREBLE:
		if (sc->sc_settreble != NULL)
			(*sc->sc_settreble)(sc, l);
		return (0);

	case I2S_INPUT_SELECT:
		/* no change necessary? */
		if (mc->un.mask == sc->sc_record_source)
			return 0;
		switch (mc->un.mask) {
		case 1 << 0: /* CD */
		case 1 << 1: /* microphone */
		case 1 << 2: /* line in */
			/* XXX TO BE DONE */
			break;
		default: /* invalid argument */
			return EINVAL;
		}
		sc->sc_record_source = mc->un.mask;
		return 0;

	case I2S_VOL_INPUT:
		/* XXX TO BE DONE */
		return 0;
	}

	return ENXIO;
}

int
i2s_get_port(h, mc)
	void *h;
	mixer_ctrl_t *mc;
{
	struct i2s_softc *sc = h;

	DPRINTF(("i2s_get_port dev = %d, type = %d\n", mc->dev, mc->type));

	switch (mc->dev) {
	case I2S_OUTPUT_SELECT:
		mc->un.mask = sc->sc_output_mask;
		return 0;

	case I2S_VOL_OUTPUT:
		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = sc->sc_vol_l;
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = sc->sc_vol_r;
		return 0;

	case I2S_INPUT_SELECT:
		mc->un.mask = sc->sc_record_source;
		return 0;

	case I2S_BASS:
		mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->sc_bass;
		return (0);

	case I2S_TREBLE:
		mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->sc_treble;
		return (0);

	case I2S_VOL_INPUT:
		/* XXX TO BE DONE */
		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = 0;
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = 0;
		return 0;

	default:
		return ENXIO;
	}

	return 0;
}

int
i2s_query_devinfo(h, dip)
	void *h;
	mixer_devinfo_t *dip;
{
	int n = 0;

	switch (dip->index) {

	case I2S_OUTPUT_SELECT:
		dip->mixer_class = I2S_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioNselect, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_SET;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->un.s.member[n].label.name, AudioNspeaker,
		    sizeof(dip->un.s.member[n].label.name));
		dip->un.s.member[n++].mask = 1 << 0;
		if (headphone_mute) {
			strlcpy(dip->un.s.member[n].label.name,
			    AudioNheadphone,
			    sizeof(dip->un.s.member[n].label.name));
			dip->un.s.member[n++].mask = 1 << 1;
		}
		if (lineout_mute) {
			strlcpy(dip->un.s.member[n].label.name,	AudioNline,
			    sizeof(dip->un.s.member[n].label.name));
			dip->un.s.member[n++].mask = 1 << 2;
		}
		dip->un.s.num_mem = n;
		return 0;

	case I2S_VOL_OUTPUT:
		dip->mixer_class = I2S_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioNmaster, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof(dip->un.v.units.name));
		return 0;

	case I2S_INPUT_SELECT:
		dip->mixer_class = I2S_RECORD_CLASS;
		strlcpy(dip->label.name, AudioNsource, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_SET;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.s.num_mem = 3;
		strlcpy(dip->un.s.member[0].label.name, AudioNcd,
		    sizeof(dip->un.s.member[0].label.name));
		dip->un.s.member[0].mask = 1 << 0;
		strlcpy(dip->un.s.member[1].label.name, AudioNmicrophone,
		    sizeof(dip->un.s.member[1].label.name));
		dip->un.s.member[1].mask = 1 << 1;
		strlcpy(dip->un.s.member[2].label.name, AudioNline,
		    sizeof(dip->un.s.member[2].label.name));
		dip->un.s.member[2].mask = 1 << 2;
		return 0;

	case I2S_VOL_INPUT:
		dip->mixer_class = I2S_RECORD_CLASS;
		strlcpy(dip->label.name, AudioNrecord, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof(dip->un.v.units.name));
		return 0;

	case I2S_MONITOR_CLASS:
		dip->mixer_class = I2S_MONITOR_CLASS;
		strlcpy(dip->label.name, AudioCmonitor, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case I2S_OUTPUT_CLASS:
		dip->mixer_class = I2S_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioCoutputs,
		    sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case I2S_RECORD_CLASS:
		dip->mixer_class = I2S_RECORD_CLASS;
		strlcpy(dip->label.name, AudioCrecord, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case I2S_BASS:
		dip->mixer_class = I2S_MONITOR_CLASS;
		strlcpy(dip->label.name, AudioNbass, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 1;
		return (0);

	case I2S_TREBLE:
		dip->mixer_class = I2S_MONITOR_CLASS;
		strlcpy(dip->label.name, AudioNtreble, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 1;
		return (0);

	}

	return ENXIO;
}

size_t
i2s_round_buffersize(h, dir, size)
	void *h;
	int dir;
	size_t size;
{
	if (size > 65536)
		size = 65536;
	return size;
}

paddr_t
i2s_mappage(h, mem, off, prot)
	void *h;
	void *mem;
	off_t off;
	int prot;
{
	if (off < 0)
		return -1;
	return -1;	/* XXX */
}

int
i2s_get_props(h)
	void *h;
{
	return AUDIO_PROP_FULLDUPLEX /* | AUDIO_PROP_MMAP */;
}

int
i2s_trigger_output(h, start, end, bsize, intr, arg, param)
	void *h;
	void *start, *end;
	int bsize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct i2s_softc *sc = h;
	struct i2s_dma *p;
	struct dbdma_command *cmd = sc->sc_odmacmd;
	vaddr_t spa, pa, epa;
	int c;

	DPRINTF(("trigger_output %p %p 0x%x\n", start, end, bsize));

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
i2s_trigger_input(h, start, end, bsize, intr, arg, param)
	void *h;
	void *start, *end;
	int bsize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	DPRINTF(("i2s_trigger_input called\n"));

	return 1;
}

#define CLKSRC_49MHz	0x80000000	/* Use 49152000Hz Osc. */
#define CLKSRC_45MHz	0x40000000	/* Use 45158400Hz Osc. */
#define CLKSRC_18MHz	0x00000000	/* Use 18432000Hz Osc. */
#define MCLK_DIV	0x1f000000	/* MCLK = SRC / DIV */
#define  MCLK_DIV1	0x14000000	/*  MCLK = SRC */
#define  MCLK_DIV3	0x13000000	/*  MCLK = SRC / 3 */
#define  MCLK_DIV5	0x12000000	/*  MCLK = SRC / 5 */
#define SCLK_DIV	0x00f00000	/* SCLK = MCLK / DIV */
#define  SCLK_DIV1	0x00800000
#define  SCLK_DIV3	0x00900000
#define SCLK_MASTER	0x00080000	/* Master mode */
#define SCLK_SLAVE	0x00000000	/* Slave mode */
#define SERIAL_FORMAT	0x00070000
#define  SERIAL_SONY	0x00000000
#define  SERIAL_64x	0x00010000
#define  SERIAL_32x	0x00020000
#define  SERIAL_DAV	0x00040000
#define  SERIAL_SILICON	0x00050000

// rate = fs = LRCLK
// SCLK = 64*LRCLK (I2S)
// MCLK = 256fs (typ. -- changeable)

// MCLK = clksrc / mdiv
// SCLK = MCLK / sdiv
// rate = SCLK / 64    ( = LRCLK = fs)

int
i2s_set_rate(sc, rate)
	struct i2s_softc *sc;
	int rate;
{
	u_int reg = 0;
	int MCLK;
	int clksrc, mdiv, sdiv;
	int mclk_fs;
	int timo;

	/* sanify */
	if (rate > 48000)
		rate = 48000;
	else if (rate < 8000)
		rate = 8000;

	switch (rate) {
	case 8000:
		clksrc = 18432000;		/* 18MHz */
		reg = CLKSRC_18MHz;
		mclk_fs = 256;
		break;

	case 44100:
		clksrc = 45158400;		/* 45MHz */
		reg = CLKSRC_45MHz;
		mclk_fs = 256;
		break;

	case 48000:
		clksrc = 49152000;		/* 49MHz */
		reg = CLKSRC_49MHz;
		mclk_fs = 256;
		break;

	default:
		return EINVAL;
	}

	MCLK = rate * mclk_fs;
	mdiv = clksrc / MCLK;			// 4
	sdiv = mclk_fs / 64;			// 4

	switch (mdiv) {
	case 1:
		reg |= MCLK_DIV1;
		break;
	case 3:
		reg |= MCLK_DIV3;
		break;
	case 5:
		reg |= MCLK_DIV5;
		break;
	default:
		reg |= ((mdiv / 2 - 1) << 24) & 0x1f000000;
		break;
	}

	switch (sdiv) {
	case 1:
		reg |= SCLK_DIV1;
		break;
	case 3:
		reg |= SCLK_DIV3;
		break;
	default:
		reg |= ((sdiv / 2 - 1) << 20) & 0x00f00000;
		break;
	}

	reg |= SCLK_MASTER;	/* XXX master mode */

	reg |= SERIAL_64x;

	if (sc->sc_rate == rate)
		return (0);

	/* stereo input and output */
	DPRINTF(("I2SSetDataWordSizeReg 0x%08x -> 0x%08x\n",
	    in32rb(sc->sc_reg + I2S_WORDSIZE), 0x02000200));
	out32rb(sc->sc_reg + I2S_WORDSIZE, 0x02000200);

	/* Clear CLKSTOPPEND */
	out32rb(sc->sc_reg + I2S_INT, I2S_INT_CLKSTOPPEND);

	keylargo_fcr_disable(I2SClockOffset, I2S0CLKEN);

	/* Wait until clock is stopped */
	for (timo = 1000; timo > 0; timo--) {
		if (in32rb(sc->sc_reg + I2S_INT) & I2S_INT_CLKSTOPPEND)
			goto done;
		delay(1);
	}

	printf("i2s_set_rate: timeout\n");

done:
	DPRINTF(("I2SSetSerialFormatReg 0x%x -> 0x%x\n",
	    in32rb(sc->sc_reg + I2S_FORMAT), reg));
	out32rb(sc->sc_reg + I2S_FORMAT, reg);

	keylargo_fcr_enable(I2SClockOffset, I2S0CLKEN);

	sc->sc_rate = rate;

	return 0;
}

int
gpio_read(addr)
	char *addr;
{
	if (*addr & GPIO_DATA)
		return 1;
	return 0;
}

void
gpio_write(addr, val)
	char *addr;
	int val;
{
	u_int data = GPIO_DDR_OUTPUT;

	if (val)
		data |= GPIO_DATA;
	*addr = data;
	asm volatile ("eieio" ::: "memory");
}

#define amp_active 0		/* XXX OF */
#define headphone_active 0	/* XXX OF */
#define lineout_active 0	/* XXX OF */

void
i2s_mute_speaker(sc, mute)
	struct i2s_softc *sc;
	int mute;
{
	u_int x;

	if (amp_mute == NULL)
		return;

	DPRINTF(("ampmute %d --> ", gpio_read(amp_mute)));

	if (mute)
		x = amp_active;		/* mute */
	else
		x = !amp_active;	/* unmute */
	if (x != gpio_read(amp_mute))
		gpio_write(amp_mute, x);

	DPRINTF(("%d\n", gpio_read(amp_mute)));
}

void
i2s_mute_headphone(sc, mute)
	struct i2s_softc *sc;
	int mute;
{
	u_int x;

	if (headphone_mute == NULL)
		return;

	DPRINTF(("headphonemute %d --> ", gpio_read(headphone_mute)));

	if (mute)
		x = headphone_active;	/* mute */
	else
		x = !headphone_active;	/* unmute */
	if (x != gpio_read(headphone_mute))
		gpio_write(headphone_mute, x);

	DPRINTF(("%d\n", gpio_read(headphone_mute)));
}

void
i2s_mute_lineout(sc, mute)
	struct i2s_softc *sc;
	int mute;
{
	u_int x;

	if (lineout_mute == NULL)
		return;

	DPRINTF(("lineout %d --> ", gpio_read(lineout_mute)));

	if (mute)
		x = lineout_active;	/* mute */
	else
		x = !lineout_active;	/* unmute */
	if (x != gpio_read(lineout_mute))
		gpio_write(lineout_mute, x);

	DPRINTF(("%d\n", gpio_read(lineout_mute)));
}

int
i2s_cint(v)
	void *v;
{
	struct i2s_softc *sc = v;
	u_int sense;

	sc->sc_output_mask = 0;
	i2s_mute_speaker(sc, 1);
	i2s_mute_headphone(sc, 1);
	i2s_mute_lineout(sc, 1);

	if (headphone_detect)
		sense = *headphone_detect;
	else
		sense = !headphone_detect_active << 1;
	DPRINTF(("headphone detect = 0x%x\n", sense));

	if (((sense & 0x02) >> 1) == headphone_detect_active) {
		DPRINTF(("headphone is inserted\n"));
		sc->sc_output_mask |= 1 << 1;
		i2s_mute_headphone(sc, 0);
	} else {
		DPRINTF(("headphone is NOT inserted\n"));
	}

	if (lineout_detect)
		sense = *lineout_detect;
	else
		sense = !lineout_detect_active << 1;
	DPRINTF(("lineout detect = 0x%x\n", sense));

	if (((sense & 0x02) >> 1) == lineout_detect_active) {
		DPRINTF(("lineout is inserted\n"));
		sc->sc_output_mask |= 1 << 2;
		i2s_mute_lineout(sc, 0);
	} else {
		DPRINTF(("lineout is NOT inserted\n"));
	}

	if (sc->sc_output_mask == 0) {
		sc->sc_output_mask |= 1 << 0;
		i2s_mute_speaker(sc, 0);
	}

	return 1;
}

u_char *
i2s_gpio_map(struct i2s_softc *sc, char *name, int *irq)
{
	u_int32_t reg[2];
	u_int32_t intr[2];
	int gpio;

	if (OF_getprop(sc->sc_node, name, &gpio,
            sizeof(gpio)) != sizeof(gpio) ||
	    OF_getprop(gpio, "reg", &reg[0],
	    sizeof(reg[0])) != sizeof(reg[0]) ||
	    OF_getprop(OF_parent(gpio), "reg", &reg[1],
	    sizeof(reg[1])) != sizeof(reg[1]))
		return NULL;

	if (irq && OF_getprop(gpio, "interrupts",
	    intr, sizeof(intr)) == sizeof(intr)) {
		*irq = intr[0];
	}

	return mapiodev(sc->sc_baseaddr + reg[0] + reg[1], 1);
}

void
i2s_gpio_init(sc, node, parent)
	struct i2s_softc *sc;
	int node;
	struct device *parent;
{
	int gpio;
	int headphone_detect_intr = -1, headphone_detect_intrtype;
	int lineout_detect_intr = -1;

	/* Map gpios. */
	amp_mute = i2s_gpio_map(sc, "platform-amp-mute", NULL);
	headphone_mute = i2s_gpio_map(sc, "platform-headphone-mute", NULL);
	headphone_detect = i2s_gpio_map(sc, "platform-headphone-detect",
	    &headphone_detect_intr);
	lineout_mute = i2s_gpio_map(sc, "platform-lineout-mute", NULL);
	lineout_detect = i2s_gpio_map(sc, "platform-lineout-detect",
	    &lineout_detect_intr);
	audio_hw_reset = i2s_gpio_map(sc, "platform-hw-reset", NULL);

	gpio = OF_getnodebyname(OF_parent(node), "gpio");
	DPRINTF((" /gpio 0x%x\n", gpio));
	gpio = OF_child(gpio);
	while (gpio) {
		char name[64], audio_gpio[64];
		int intr[2];
		paddr_t addr;

		bzero(name, sizeof name);
		bzero(audio_gpio, sizeof audio_gpio);
		addr = 0;
		OF_getprop(gpio, "name", name, sizeof name);
		OF_getprop(gpio, "audio-gpio", audio_gpio, sizeof audio_gpio);
		OF_getprop(gpio, "AAPL,address", &addr, sizeof addr);
		/* printf("0x%x %s %s\n", gpio, name, audio_gpio); */

		/* gpio5 */
		if (headphone_mute == NULL &&
		    strcmp(audio_gpio, "headphone-mute") == 0)
			headphone_mute = mapiodev(addr,1);

		/* gpio6 */
		if (amp_mute == NULL &&
		    strcmp(audio_gpio, "amp-mute") == 0)
			amp_mute = mapiodev(addr,1);

		/* extint-gpio15 */
		if (headphone_detect == NULL &&
		    strcmp(audio_gpio, "headphone-detect") == 0) {
			headphone_detect = mapiodev(addr,1);
			OF_getprop(gpio, "audio-gpio-active-state",
			    &headphone_detect_active, 4);
			OF_getprop(gpio, "interrupts", intr, 8);
			headphone_detect_intr = intr[0];
			headphone_detect_intrtype = intr[1];
		}

		/* gpio11 (keywest-11) */
		if (audio_hw_reset == NULL &&
		    strcmp(audio_gpio, "audio-hw-reset") == 0)
			audio_hw_reset = mapiodev(addr,1);

		gpio = OF_peer(gpio);
	}
	DPRINTF((" amp-mute %p\n", amp_mute));
	DPRINTF((" headphone-mute %p\n", headphone_mute));
	DPRINTF((" headphone-detect %p\n", headphone_detect));
	DPRINTF((" headphone-detect active %x\n", headphone_detect_active));
	DPRINTF((" headphone-detect intr %x\n", headphone_detect_intr));
	DPRINTF((" lineout-mute %p\n", lineout_mute));
	DPRINTF((" lineout-detect %p\n", lineout_detect));
	DPRINTF((" lineout-detect active %x\n", lineout_detect_active));
	DPRINTF((" lineout-detect intr %x\n", lineout_detect_intr));
	DPRINTF((" audio-hw-reset %p\n", audio_hw_reset));

	if (headphone_detect_intr != -1)
		mac_intr_establish(parent, headphone_detect_intr, IST_EDGE,
		    IPL_AUDIO, i2s_cint, sc, "i2s_h");

	if (lineout_detect_intr != -1)
		mac_intr_establish(parent, lineout_detect_intr, IST_EDGE,
		    IPL_AUDIO, i2s_cint, sc, "i2s_l");

	/* Enable headphone interrupt? */
	*headphone_detect |= 0x80;
	asm volatile("eieio");

	/* Update headphone status. */
	i2s_cint(sc);
}

void *
i2s_allocm(void *h, int dir, size_t size, int type, int flags)
{
	struct i2s_softc *sc = h;
	struct i2s_dma *p;
	int error;

	if (size > I2S_DMALIST_MAX * I2S_DMASEG_MAX)
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

#define reset_active 0

int
deq_reset(struct i2s_softc *sc)
{
	if (audio_hw_reset == NULL)
		return (-1);

	gpio_write(audio_hw_reset, !reset_active);
	delay(1000000);

	gpio_write(audio_hw_reset, reset_active);
	delay(1);

	gpio_write(audio_hw_reset, !reset_active);
	delay(10000);

	return (0);
}
