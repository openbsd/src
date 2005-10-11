/*	$OpenBSD: snapper.c,v 1.18 2005/10/11 20:43:53 drahn Exp $	*/
/*	$NetBSD: snapper.c,v 1.1 2003/12/27 02:19:34 grant Exp $	*/

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

/*
 * Datasheet is available from
 * http://www.ti.com/sc/docs/products/analog/tas3004.html
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

#ifdef SNAPPER_DEBUG
# define DPRINTF(x) printf x 
#else
# define DPRINTF(x)
#endif

#define	SNAPPER_DMALIST_MAX	32
#define	SNAPPER_DMASEG_MAX	NBPG

struct snapper_dma {
	bus_dmamap_t map;
	caddr_t addr;
	bus_dma_segment_t segs[SNAPPER_DMALIST_MAX];
	int nsegs;
	size_t size;
	struct snapper_dma *next;
};

struct snapper_softc {
	struct device sc_dev;
	int sc_flags;
	int sc_node;

	void (*sc_ointr)(void *);	/* dma completion intr handler */
	void *sc_oarg;			/* arg for sc_ointr() */
	int sc_opages;			/* # of output pages */

	void (*sc_iintr)(void *);	/* dma completion intr handler */
	void *sc_iarg;			/* arg for sc_iintr() */

	u_int sc_record_source;		/* recording source mask */
	u_int sc_output_mask;		/* output source mask */

	u_char *sc_reg;
	struct device *sc_i2c;

	u_int sc_vol_l;
	u_int sc_vol_r;

	bus_dma_tag_t sc_dmat;
	dbdma_regmap_t *sc_odma;
	dbdma_regmap_t *sc_idma;
	struct dbdma_command *sc_odmacmd, *sc_odmap;
	struct dbdma_command *sc_idmacmd, *sc_idmap;
	dbdma_t sc_odbdma, sc_idbdma;

	struct snapper_dma *sc_dmas;
	u_long sc_rate;
};

int snapper_match(struct device *, void *, void *);
void snapper_attach(struct device *, struct device *, void *);
void snapper_defer(struct device *);
int snapper_intr(void *);
int snapper_open(void *, int);
void snapper_close(void *);
int snapper_query_encoding(void *, struct audio_encoding *);
int snapper_set_params(void *, int, int, struct audio_params *,
    struct audio_params *);
int snapper_round_blocksize(void *, int);
int snapper_halt_output(void *);
int snapper_halt_input(void *);
int snapper_getdev(void *, struct audio_device *);
int snapper_set_port(void *, mixer_ctrl_t *);
int snapper_get_port(void *, mixer_ctrl_t *);
int snapper_query_devinfo(void *, mixer_devinfo_t *);
size_t snapper_round_buffersize(void *, int, size_t);
paddr_t snapper_mappage(void *, void *, off_t, int);
int snapper_get_props(void *);
int snapper_trigger_output(void *, void *, void *, int, void (*)(void *),
    void *, struct audio_params *);
int snapper_trigger_input(void *, void *, void *, int, void (*)(void *),
    void *, struct audio_params *);
void snapper_set_volume(struct snapper_softc *, int, int);
int snapper_set_rate(struct snapper_softc *, int);
void snapper_config(struct snapper_softc *sc, int node, struct device *parent);
struct snapper_mode *snapper_find_mode(u_int, u_int, u_int);
void snapper_cs16mts(void *, u_char *, int);

int tas3004_write(struct snapper_softc *, u_int, const void *);
static int gpio_read(char *);
static void gpio_write(char *, int);
void snapper_mute_speaker(struct snapper_softc *, int);
void snapper_mute_headphone(struct snapper_softc *, int);
int snapper_cint(void *);
int tas3004_init(struct snapper_softc *);
void snapper_init(struct snapper_softc *, int);
void *snapper_allocm(void *h, int dir, size_t size, int type, int flags);

static void mono16_to_stereo16(void *, u_char *, int);
static void swap_bytes_mono16_to_stereo16(void *, u_char *, int);

/* XXX */
int ki2c_setmode(struct device *, int);
int ki2c_write(struct device *, int, int, const void *, int);
void ki2c_writereg(struct device *, int, u_int);


struct cfattach snapper_ca = {
	sizeof(struct snapper_softc), snapper_match, snapper_attach
};
struct cfdriver snapper_cd = {
	NULL, "snapper", DV_DULL
};

struct audio_hw_if snapper_hw_if = {
	snapper_open,
	snapper_close,
	NULL,
	snapper_query_encoding,
	snapper_set_params,
	snapper_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	snapper_halt_output,
	snapper_halt_input,
	NULL,
	snapper_getdev,
	NULL,
	snapper_set_port,
	snapper_get_port,
	snapper_query_devinfo,
	snapper_allocm,		/* allocm */
	NULL,
	snapper_round_buffersize,
	snapper_mappage,
	snapper_get_props,
	snapper_trigger_output,
	snapper_trigger_input,
};

struct audio_device snapper_device = {
	"SNAPPER",
	"",
	"snapper"
};

static u_char *amp_mute;
static u_char *headphone_mute;
static u_char *audio_hw_reset;
static u_char *headphone_detect;
static int headphone_detect_active;


/* I2S registers */
#define I2S_INT		0x00
#define I2S_FORMAT	0x10
#define I2S_FRAMECOUNT	0x40
#define I2S_FRAMEMATCH	0x50
#define I2S_WORDSIZE	0x60

/* TAS3004 registers */
#define DEQ_MCR1	0x01	/* Main control register 1 (1byte) */
#define DEQ_DRC		0x02	/* Dynamic range compression (6bytes?) */
#define DEQ_VOLUME	0x04	/* Volume (6bytes) */
#define DEQ_TREBLE	0x05	/* Treble control (1byte) */
#define DEQ_BASS	0x06	/* Bass control (1byte) */
#define DEQ_MIXER_L	0x07	/* Mixer left gain (9bytes) */
#define DEQ_MIXER_R	0x08	/* Mixer right gain (9bytes) */
#define DEQ_LB0		0x0a	/* Left biquad 0 (15bytes) */
#define DEQ_LB1		0x0b	/* Left biquad 1 (15bytes) */
#define DEQ_LB2		0x0c	/* Left biquad 2 (15bytes) */
#define DEQ_LB3		0x0d	/* Left biquad 3 (15bytes) */
#define DEQ_LB4		0x0e	/* Left biquad 4 (15bytes) */
#define DEQ_LB5		0x0f	/* Left biquad 5 (15bytes) */
#define DEQ_LB6		0x10	/* Left biquad 6 (15bytes) */
#define DEQ_RB0		0x13	/* Right biquad 0 (15bytes) */
#define DEQ_RB1		0x14	/* Right biquad 1 (15bytes) */
#define DEQ_RB2		0x15	/* Right biquad 2 (15bytes) */
#define DEQ_RB3		0x16	/* Right biquad 3 (15bytes) */
#define DEQ_RB4		0x17	/* Right biquad 4 (15bytes) */
#define DEQ_RB5		0x18	/* Right biquad 5 (15bytes) */
#define DEQ_RB6		0x19	/* Right biquad 6 (15bytes) */
#define DEQ_LLB		0x21	/* Left loudness biquad (15bytes) */
#define DEQ_RLB		0x22	/* Right loudness biquad (15bytes) */
#define DEQ_LLB_GAIN	0x23	/* Left loudness biquad gain (3bytes) */
#define DEQ_RLB_GAIN	0x24	/* Right loudness biquad gain (3bytes) */
#define DEQ_ACR		0x40	/* Analog control register (1byte) */
#define DEQ_MCR2	0x43	/* Main control register 2 (1byte) */

#define DEQ_MCR1_FL	0x80	/* Fast load */
#define DEQ_MCR1_SC	0x40	/* SCLK frequency */
#define  DEQ_MCR1_SC_32	0x00	/*  32fs */
#define  DEQ_MCR1_SC_64	0x40	/*  64fs */
#define DEQ_MCR1_SM	0x30	/* Output serial port mode */
#define  DEQ_MCR1_SM_L	0x00	/*  Left justified */
#define  DEQ_MCR1_SM_R	0x10	/*  Right justified */
#define  DEQ_MCR1_SM_I2S 0x20	/*  I2S */
#define DEQ_MCR1_W	0x03	/* Serial port word length */
#define  DEQ_MCR1_W_16	0x00	/*  16 bit */
#define  DEQ_MCR1_W_18	0x01	/*  18 bit */
#define  DEQ_MCR1_W_20	0x02	/*  20 bit */

#define DEQ_MCR2_DL	0x80	/* Download */
#define DEQ_MCR2_AP	0x02	/* All pass mode */

#define DEQ_ACR_ADM	0x80	/* ADC output mode */
#define DEQ_ACR_LRB	0x40	/* Select B input */
#define DEQ_ACR_DM	0x0c	/* De-emphasis control */
#define  DEQ_ACR_DM_OFF	0x00	/*  off */
#define  DEQ_ACR_DM_48	0x04	/*  fs = 48kHz */
#define  DEQ_ACR_DM_44	0x08	/*  fs = 44.1kHz */
#define DEQ_ACR_INP	0x02	/* Analog input select */
#define  DEQ_ACR_INP_A	0x00	/*  A */
#define  DEQ_ACR_INP_B	0x02	/*  B */
#define DEQ_ACR_APD	0x01	/* Analog power down */

struct tas3004_reg {
	u_char MCR1[1];
	u_char DRC[6];
	u_char VOLUME[6];
	u_char TREBLE[1];
	u_char BASS[1];
	u_char MIXER_L[9];
	u_char MIXER_R[9];
	u_char LB0[15];
	u_char LB1[15];
	u_char LB2[15];
	u_char LB3[15];
	u_char LB4[15];
	u_char LB5[15];
	u_char LB6[15];
	u_char RB0[15];
	u_char RB1[15];
	u_char RB2[15];
	u_char RB3[15];
	u_char RB4[15];
	u_char RB5[15];
	u_char RB6[15];
	u_char LLB[15];
	u_char RLB[15];
	u_char LLB_GAIN[3];
	u_char RLB_GAIN[3];
	u_char ACR[1];
	u_char MCR2[1];
};

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

int
snapper_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct confargs *ca = aux;
	int soundbus, soundchip;
	char compat[32];

	if (strcmp(ca->ca_name, "i2s") != 0)
		return 0;

	if ((soundbus = OF_child(ca->ca_node)) == 0 ||
	    (soundchip = OF_child(soundbus)) == 0)
		return 0;

	bzero(compat, sizeof compat);
	OF_getprop(soundchip, "compatible", compat, sizeof compat);

	if (strcmp(compat, "snapper") != 0 &&
	    strcmp(compat, "AOAKeylargo") != 0)
		return 0;

	return 1;
}

void
snapper_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct snapper_softc *sc = (struct snapper_softc *)self;
	struct confargs *ca = aux;
	int cirq, oirq, iirq, cirq_type, oirq_type, iirq_type;
	int soundbus, intr[6];

	ca->ca_reg[0] += ca->ca_baseaddr;
	ca->ca_reg[2] += ca->ca_baseaddr;
	ca->ca_reg[4] += ca->ca_baseaddr;

	sc->sc_reg = mapiodev(ca->ca_reg[0], ca->ca_reg[1]);

	sc->sc_node = ca->ca_node;

	sc->sc_dmat = ca->ca_dmat;
	sc->sc_odma = mapiodev(ca->ca_reg[2], ca->ca_reg[3]); /* out */
	sc->sc_idma = mapiodev(ca->ca_reg[4], ca->ca_reg[5]); /* in */
	sc->sc_odbdma = dbdma_alloc(sc->sc_dmat, SNAPPER_DMALIST_MAX);
	sc->sc_odmacmd = sc->sc_odbdma->d_addr;
	sc->sc_idbdma = dbdma_alloc(sc->sc_dmat, SNAPPER_DMALIST_MAX);
	sc->sc_idmacmd = sc->sc_idbdma->d_addr;

	soundbus = OF_child(ca->ca_node);
	OF_getprop(soundbus, "interrupts", intr, sizeof intr);
	cirq = intr[0];
	oirq = intr[2];
	iirq = intr[4];
	cirq_type = intr[1] ? IST_LEVEL : IST_EDGE;
	oirq_type = intr[3] ? IST_LEVEL : IST_EDGE;
	iirq_type = intr[5] ? IST_LEVEL : IST_EDGE;

	/* intr_establish(cirq, cirq_type, IPL_AUDIO, snapper_intr, sc); */
	mac_intr_establish(parent, oirq, oirq_type, IPL_AUDIO, snapper_intr,
	    sc, "snapper");
	/* intr_establish(iirq, iirq_type, IPL_AUDIO, snapper_intr, sc); */

	printf(": irq %d,%d,%d\n", cirq, oirq, iirq);

	snapper_config(sc,  sc->sc_node, parent);
	config_defer(self, snapper_defer);
}

void
snapper_defer(struct device *dev)
{
	struct snapper_softc *sc = (struct snapper_softc *)dev;
	struct device *dv;

	TAILQ_FOREACH(dv, &alldevs, dv_list)
		if (strncmp(dv->dv_xname, "ki2c", 4) == 0 &&
		    strncmp(dv->dv_parent->dv_xname, "macobio", 7) == 0)
			sc->sc_i2c = dv;
	if (sc->sc_i2c == NULL) {
		printf("%s: unable to find i2c\n", sc->sc_dev.dv_xname);
		return;
	}
	
	/* XXX If i2c has failed to attach, what should we do? */

	audio_attach_mi(&snapper_hw_if, sc, &sc->sc_dev);

	/* ki2c_setmode(sc->sc_i2c, I2C_STDSUBMODE); */
	snapper_init(sc, sc->sc_node);
}

int
snapper_intr(v)
	void *v;
{
	struct snapper_softc *sc = v;
	struct dbdma_command *cmd = sc->sc_odmap;
#ifndef __OpenBSD__
	int count = sc->sc_opages;
	int status;
#else
	u_int16_t c, status;
#endif

	/* if not set we are not running */
	if (!cmd)
		return (0);
	DPRINTF(("snapper_intr: cmd %x\n", cmd));

#ifndef __OpenBSD__
	/* Fill used buffer(s). */
	while (count-- > 0) {
		if ((dbdma_ld16(&cmd->d_command) & 0x30) == 0x30) {
			status = dbdma_ld16(&cmd->d_status);
			cmd->d_status = 0;
			if (status)	/* status == 0x8400 */
				if (sc->sc_ointr)
					(*sc->sc_ointr)(sc->sc_oarg);
		}
		cmd++;
	}
#else
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
#endif

	return 1;
}

int
snapper_open(h, flags)
	void *h;
	int flags;
{
	return 0;
}

/*
 * Close function is called at splaudio().
 */
void
snapper_close(h)
	void *h;
{
	struct snapper_softc *sc = h;

	snapper_halt_output(sc);
	snapper_halt_input(sc);

	sc->sc_ointr = 0;
	sc->sc_iintr = 0;
}

int
snapper_query_encoding(h, ae)
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
snapper_cs16mts(void *v, u_char *p, int cc)
{
	mono16_to_stereo16(v, p, cc);
	change_sign16_be(v, p, cc * 2);
}

struct snapper_mode {
	u_int encoding;
	u_int precision;
	u_int channels;
	void (*sw_code)(void *, u_char *, int);
	int factor;
} snapper_modes[] = {
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
	{ AUDIO_ENCODING_ULINEAR_BE, 16, 1, snapper_cs16mts, 2 },
	{ AUDIO_ENCODING_ULINEAR_BE, 16, 2, change_sign16_be, 1 }
};


struct snapper_mode *
snapper_find_mode(u_int encoding, u_int precision, u_int channels)
{
	struct snapper_mode *m;
	int i;

	for (i = 0; i < sizeof(snapper_modes)/sizeof(snapper_modes[0]); i++) {
		m = &snapper_modes[i];
		if (m->encoding == encoding &&
		    m->precision == precision &&
		    m->channels == channels)
			return (m);
	}
	return (NULL);
}

int
snapper_set_params(h, setmode, usemode, play, rec)
	void *h;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct snapper_mode *m;
	struct snapper_softc *sc = h;
	struct audio_params *p;
	int mode, rate;

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
			m = snapper_find_mode(p->encoding, p->precision,
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
	p->sample_rate = play->sample_rate;
	rate = p->sample_rate;

	if (snapper_set_rate(sc, rate))
		return EINVAL;
	p->sample_rate = sc->sc_rate;

	return 0;
}

int
snapper_round_blocksize(h, size)
	void *h;
	int size;
{
	if (size < NBPG)
		size = NBPG;
	return size & ~PGOFSET;
}

int
snapper_halt_output(h)
	void *h;
{
	struct snapper_softc *sc = h;

	dbdma_stop(sc->sc_odma);
	dbdma_reset(sc->sc_odma);
	return 0;
}

int
snapper_halt_input(h)
	void *h;
{
	struct snapper_softc *sc = h;

	dbdma_stop(sc->sc_idma);
	dbdma_reset(sc->sc_idma);
	return 0;
}

int
snapper_getdev(h, retp)
	void *h;
	struct audio_device *retp;
{
	*retp = snapper_device;
	return 0;
}

enum {
	SNAPPER_MONITOR_CLASS,
	SNAPPER_OUTPUT_CLASS,
	SNAPPER_RECORD_CLASS,
	SNAPPER_OUTPUT_SELECT,
	SNAPPER_VOL_OUTPUT,
	SNAPPER_INPUT_SELECT,
	SNAPPER_VOL_INPUT,
	SNAPPER_ENUM_LAST
};

int
snapper_set_port(h, mc)
	void *h;
	mixer_ctrl_t *mc;
{
	struct snapper_softc *sc = h;
	int l, r;

	DPRINTF(("snapper_set_port dev = %d, type = %d\n", mc->dev, mc->type));

	l = mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
	r = mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];

	switch (mc->dev) {
	case SNAPPER_OUTPUT_SELECT:
		/* No change necessary? */
		if (mc->un.mask == sc->sc_output_mask)
			return 0;

		snapper_mute_speaker(sc, 1);
		snapper_mute_headphone(sc, 1);
		if (mc->un.mask & 1 << 0)
			snapper_mute_speaker(sc, 0);
		if (mc->un.mask & 1 << 1)
			snapper_mute_headphone(sc, 0);

		sc->sc_output_mask = mc->un.mask;
		return 0;

	case SNAPPER_VOL_OUTPUT:
		snapper_set_volume(sc, l, r);
		return 0;

	case SNAPPER_INPUT_SELECT:
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

	case SNAPPER_VOL_INPUT:
		/* XXX TO BE DONE */
		return 0;
	}

	return ENXIO;
}

int
snapper_get_port(h, mc)
	void *h;
	mixer_ctrl_t *mc;
{
	struct snapper_softc *sc = h;

	DPRINTF(("snapper_get_port dev = %d, type = %d\n", mc->dev, mc->type));

	switch (mc->dev) {
	case SNAPPER_OUTPUT_SELECT:
		mc->un.mask = sc->sc_output_mask;
		return 0;

	case SNAPPER_VOL_OUTPUT:
		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = sc->sc_vol_l;
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = sc->sc_vol_r;
		return 0;

	case SNAPPER_INPUT_SELECT:
		mc->un.mask = sc->sc_record_source;
		return 0;

	case SNAPPER_VOL_INPUT:
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
snapper_query_devinfo(h, dip)
	void *h;
	mixer_devinfo_t *dip;
{
	switch (dip->index) {

	case SNAPPER_OUTPUT_SELECT:
		dip->mixer_class = SNAPPER_MONITOR_CLASS;
		strlcpy(dip->label.name, AudioNoutput, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_SET;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.s.num_mem = 2;
		strlcpy(dip->un.s.member[0].label.name, AudioNspeaker,
		    sizeof(dip->un.s.member[0].label.name));
		dip->un.s.member[0].mask = 1 << 0;
		strlcpy(dip->un.s.member[1].label.name, AudioNheadphone,
		    sizeof(dip->un.s.member[1].label.name));
		dip->un.s.member[1].mask = 1 << 1;
		return 0;

	case SNAPPER_VOL_OUTPUT:
		dip->mixer_class = SNAPPER_MONITOR_CLASS;
		strlcpy(dip->label.name, AudioNmaster, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof(dip->un.v.units.name));
		return 0;

	case SNAPPER_INPUT_SELECT:
		dip->mixer_class = SNAPPER_RECORD_CLASS;
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

	case SNAPPER_VOL_INPUT:
		dip->mixer_class = SNAPPER_RECORD_CLASS;
		strlcpy(dip->label.name, AudioNrecord, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_VALUE;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof(dip->un.v.units.name));
		return 0;

	case SNAPPER_MONITOR_CLASS:
		dip->mixer_class = SNAPPER_MONITOR_CLASS;
		strlcpy(dip->label.name, AudioCmonitor, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case SNAPPER_OUTPUT_CLASS:
		dip->mixer_class = SNAPPER_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioCoutputs,
		    sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;

	case SNAPPER_RECORD_CLASS:
		dip->mixer_class = SNAPPER_RECORD_CLASS;
		strlcpy(dip->label.name, AudioCrecord, sizeof(dip->label.name));
		dip->type = AUDIO_MIXER_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		return 0;
	}

	return ENXIO;
}

size_t
snapper_round_buffersize(h, dir, size)
	void *h;
	int dir;
	size_t size;
{
	if (size > 65536)
		size = 65536;
	return size;
}

paddr_t
snapper_mappage(h, mem, off, prot)
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
snapper_get_props(h)
	void *h;
{
	return AUDIO_PROP_FULLDUPLEX /* | AUDIO_PROP_MMAP */;
}

int
snapper_trigger_output(h, start, end, bsize, intr, arg, param)
	void *h;
	void *start, *end;
	int bsize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct snapper_softc *sc = h;
	struct snapper_dma *p;
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
snapper_trigger_input(h, start, end, bsize, intr, arg, param)
	void *h;
	void *start, *end;
	int bsize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	DPRINTF(("snapper_trigger_input called\n"));

	return 1;
}

void
snapper_set_volume(sc, left, right)
	struct snapper_softc *sc;
	int left, right;
{
	u_char vol[6];

	sc->sc_vol_l = left;
	sc->sc_vol_r = right;

	left <<= 8;	/* XXX for now */
	right <<= 8;

	vol[0] = left >> 16;
	vol[1] = left >> 8;
	vol[2] = left;
	vol[3] = right >> 16;
	vol[4] = right >> 8;
	vol[5] = right;

	tas3004_write(sc, DEQ_VOLUME, vol);
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

void keylargo_fcr_enable(int offset, u_int32_t bits);
void keylargo_fcr_disable(int offset, u_int32_t bits);

int
snapper_set_rate(sc, rate)
	struct snapper_softc *sc;
	int rate;
{
	u_int reg = 0;
	int MCLK;
	int clksrc, mdiv, sdiv;
	int mclk_fs;

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

	/* stereo input and output */
	DPRINTF(("I2SSetDataWordSizeReg 0x%08x -> 0x%08x\n",
	    in32rb(sc->sc_reg + I2S_WORDSIZE), 0x02000200));
	out32rb(sc->sc_reg + I2S_WORDSIZE, 0x02000200);

#define I2SClockOffset 0x3C
#define I2SClockEnable (0x00000001<<12)

	if (sc->sc_rate != rate) {
		keylargo_fcr_disable(I2SClockOffset, I2SClockEnable);
		delay(10000); /* XXX - should wait for clock to stop */
		DPRINTF(("I2SSetSerialFormatReg 0x%x -> 0x%x\n",
		    in32rb(sc->sc_reg + I2S_FORMAT), reg));
		out32rb(sc->sc_reg + I2S_FORMAT, reg);
		keylargo_fcr_enable(I2SClockOffset, I2SClockEnable);
	}

	sc->sc_rate = rate;

	return 0;
}

#define DEQaddr 0x6a

const struct tas3004_reg tas3004_initdata = {
	{ DEQ_MCR1_SC_64 | DEQ_MCR1_SM_I2S | DEQ_MCR1_W_20 },	/* MCR1 */
	{ 1, 0, 0, 0, 0, 0 },					/* DRC */
	{ 0, 0, 0, 0, 0, 0 },					/* VOLUME */
	{ 0x72 },						/* TREBLE */
	{ 0x72 },						/* BASS */
	{ 0x10, 0x00, 0x00, 0, 0, 0, 0, 0, 0 },			/* MIXER_L */
	{ 0x10, 0x00, 0x00, 0, 0, 0, 0, 0, 0 },			/* MIXER_R */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* BIQUAD */
	{ 0, 0, 0 },						/* LLB_GAIN */
	{ 0, 0, 0 },						/* RLB_GAIN */
	{ 0 },							/* ACR */
	{ 0 }							/* MCR2 */
};

const char tas3004_regsize[] = {
	0,					/* 0x00 */
	sizeof tas3004_initdata.MCR1,		/* 0x01 */
	sizeof tas3004_initdata.DRC,		/* 0x02 */
	0,					/* 0x03 */
	sizeof tas3004_initdata.VOLUME,		/* 0x04 */
	sizeof tas3004_initdata.TREBLE,		/* 0x05 */
	sizeof tas3004_initdata.BASS,		/* 0x06 */
	sizeof tas3004_initdata.MIXER_L,	/* 0x07 */
	sizeof tas3004_initdata.MIXER_R,	/* 0x08 */
	0,					/* 0x09 */
	sizeof tas3004_initdata.LB0,		/* 0x0a */
	sizeof tas3004_initdata.LB1,		/* 0x0b */
	sizeof tas3004_initdata.LB2,		/* 0x0c */
	sizeof tas3004_initdata.LB3,		/* 0x0d */
	sizeof tas3004_initdata.LB4,		/* 0x0e */
	sizeof tas3004_initdata.LB5,		/* 0x0f */
	sizeof tas3004_initdata.LB6,		/* 0x10 */
	0,					/* 0x11 */
	0,					/* 0x12 */
	sizeof tas3004_initdata.RB0,		/* 0x13 */
	sizeof tas3004_initdata.RB1,		/* 0x14 */
	sizeof tas3004_initdata.RB2,		/* 0x15 */
	sizeof tas3004_initdata.RB3,		/* 0x16 */
	sizeof tas3004_initdata.RB4,		/* 0x17 */
	sizeof tas3004_initdata.RB5,		/* 0x18 */
	sizeof tas3004_initdata.RB6,		/* 0x19 */
	0,0,0,0, 0,0,
	0,					/* 0x20 */
	sizeof tas3004_initdata.LLB,		/* 0x21 */
	sizeof tas3004_initdata.RLB,		/* 0x22 */
	sizeof tas3004_initdata.LLB_GAIN,	/* 0x23 */
	sizeof tas3004_initdata.RLB_GAIN,	/* 0x24 */
	0,0,0,0, 0,0,0,0, 0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	sizeof tas3004_initdata.ACR,		/* 0x40 */
	0,					/* 0x41 */
	0,					/* 0x42 */
	sizeof tas3004_initdata.MCR2		/* 0x43 */
};

int
tas3004_write(sc, reg, data)
	struct snapper_softc *sc;
	u_int reg;
	const void *data;
{
	int size;

	KASSERT(reg < sizeof tas3004_regsize);
	size = tas3004_regsize[reg];
	KASSERT(size > 0);

	if (ki2c_write(sc->sc_i2c, DEQaddr, reg, data, size))
		return -1;

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

#define headphone_active 0	/* XXX OF */
#define amp_active 0		/* XXX OF */

void
snapper_mute_speaker(sc, mute)
	struct snapper_softc *sc;
	int mute;
{
	u_int x;

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
snapper_mute_headphone(sc, mute)
	struct snapper_softc *sc;
	int mute;
{
	u_int x;

	DPRINTF(("headphonemute %d --> ", gpio_read(headphone_mute)));

	if (mute)
		x = headphone_active;	/* mute */
	else
		x = !headphone_active;	/* unmute */
	if (x != gpio_read(headphone_mute))
		gpio_write(headphone_mute, x);

	DPRINTF(("%d\n", gpio_read(headphone_mute)));
}

int
snapper_cint(v)
	void *v;
{
	struct snapper_softc *sc = v;
	u_int sense;

	sense = *headphone_detect;
	DPRINTF(("headphone detect = 0x%x\n", sense));

	if (((sense & 0x02) >> 1) == headphone_detect_active) {
		DPRINTF(("headphone is inserted\n"));
		snapper_mute_speaker(sc, 1);
		snapper_mute_headphone(sc, 0);
		sc->sc_output_mask = 1 << 1;
	} else {
		DPRINTF(("headphone is NOT inserted\n"));
		snapper_mute_speaker(sc, 0);
		snapper_mute_headphone(sc, 1);
		sc->sc_output_mask = 1 << 0;
	}

	return 1;
}

#define reset_active 0	/* XXX OF */

#define DEQ_WRITE(sc, reg, addr) \
	if (tas3004_write(sc, reg, addr)) goto err

int
tas3004_init(sc)
	struct snapper_softc *sc;
{

	/* No reset port.  Nothing to do. */
	if (audio_hw_reset == NULL)
		goto noreset;

	/* Reset TAS3004. */
	gpio_write(audio_hw_reset, !reset_active);	/* Negate RESET */
	delay(100000);				/* XXX Really needed? */

	gpio_write(audio_hw_reset, reset_active);	/* Assert RESET */
	delay(1);

	gpio_write(audio_hw_reset, !reset_active);	/* Negate RESET */
	delay(10000);

noreset:
	DEQ_WRITE(sc, DEQ_LB0, tas3004_initdata.LB0);
	DEQ_WRITE(sc, DEQ_LB1, tas3004_initdata.LB1);
	DEQ_WRITE(sc, DEQ_LB2, tas3004_initdata.LB2);
	DEQ_WRITE(sc, DEQ_LB3, tas3004_initdata.LB3);
	DEQ_WRITE(sc, DEQ_LB4, tas3004_initdata.LB4);
	DEQ_WRITE(sc, DEQ_LB5, tas3004_initdata.LB5);
	DEQ_WRITE(sc, DEQ_LB6, tas3004_initdata.LB6);
	DEQ_WRITE(sc, DEQ_RB0, tas3004_initdata.RB0);
	DEQ_WRITE(sc, DEQ_RB1, tas3004_initdata.RB1);
	DEQ_WRITE(sc, DEQ_RB1, tas3004_initdata.RB1);
	DEQ_WRITE(sc, DEQ_RB2, tas3004_initdata.RB2);
	DEQ_WRITE(sc, DEQ_RB3, tas3004_initdata.RB3);
	DEQ_WRITE(sc, DEQ_RB4, tas3004_initdata.RB4);
	DEQ_WRITE(sc, DEQ_RB5, tas3004_initdata.RB5);
	DEQ_WRITE(sc, DEQ_MCR1, tas3004_initdata.MCR1);
	DEQ_WRITE(sc, DEQ_MCR2, tas3004_initdata.MCR2);
	DEQ_WRITE(sc, DEQ_DRC, tas3004_initdata.DRC);
	DEQ_WRITE(sc, DEQ_VOLUME, tas3004_initdata.VOLUME);
	DEQ_WRITE(sc, DEQ_TREBLE, tas3004_initdata.TREBLE);
	DEQ_WRITE(sc, DEQ_BASS, tas3004_initdata.BASS);
	DEQ_WRITE(sc, DEQ_MIXER_L, tas3004_initdata.MIXER_L);
	DEQ_WRITE(sc, DEQ_MIXER_R, tas3004_initdata.MIXER_R);
	DEQ_WRITE(sc, DEQ_LLB, tas3004_initdata.LLB);
	DEQ_WRITE(sc, DEQ_RLB, tas3004_initdata.RLB);
	DEQ_WRITE(sc, DEQ_LLB_GAIN, tas3004_initdata.LLB_GAIN);
	DEQ_WRITE(sc, DEQ_RLB_GAIN, tas3004_initdata.RLB_GAIN);
	DEQ_WRITE(sc, DEQ_ACR, tas3004_initdata.ACR);

	return 0;
err:
	printf("%s: tas3004_init failed\n", sc->sc_dev.dv_xname);
	return -1;
}

/* FCR(0x3c) bits */
#define I2S0CLKEN	0x1000
#define I2S0EN		0x2000
#define I2S1CLKEN	0x080000
#define I2S1EN		0x100000

#define FCR3C_BITMASK "\020\25I2S1EN\24I2S1CLKEN\16I2S0EN\15I2S0CLKEN"


void
snapper_config(sc, node, parent)
	struct snapper_softc *sc;
	int node;
	struct device *parent;
{
	int gpio;
	int headphone_detect_intr = -1, headphone_detect_intrtype;

#if 0
#ifdef SNAPPER_DEBUG
	char fcr[32];

	bitmask_snprintf(in32rb(0x8000003c), FCR3C_BITMASK, fcr, sizeof fcr);
	printf("FCR(0x3c) 0x%s\n", fcr);
#endif
#endif

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
	DPRINTF((" headphone-mute %p\n", headphone_mute));
	DPRINTF((" amp-mute %p\n", amp_mute));
	DPRINTF((" headphone-detect %p\n", headphone_detect));
	DPRINTF((" headphone-detect active %x\n", headphone_detect_active));
	DPRINTF((" headphone-detect intr %x\n", headphone_detect_intr));
	DPRINTF((" audio-hw-reset %p\n", audio_hw_reset));

	if (headphone_detect_intr != -1)
		mac_intr_establish(parent, headphone_detect_intr, IST_EDGE,
		    IPL_AUDIO, snapper_cint, sc, "snapper_h");
}
void
snapper_init(sc, node)
	struct snapper_softc *sc;
	int node;
{

	/* "sample-rates" (44100, 48000) */
	snapper_set_rate(sc, 44100);

	/* Enable headphone interrupt? */
	*headphone_detect |= 0x80;
	asm volatile ("eieio" ::: "memory");

	/* i2c_set_port(port); */

#if 1
	/* Enable I2C interrupts. */
#define IER 4
#define I2C_INT_DATA 0x01
#define I2C_INT_ADDR 0x02
#define I2C_INT_STOP 0x04
	ki2c_writereg(sc->sc_i2c, IER,I2C_INT_DATA|I2C_INT_ADDR|I2C_INT_STOP);
#endif

	if (tas3004_init(sc))
		return;

	/* Update headphone status. */
	snapper_cint(sc);

	snapper_set_volume(sc, 80, 80);
}

void *
snapper_allocm(void *h, int dir, size_t size, int type, int flags)
{
	struct snapper_softc *sc = h;
	struct snapper_dma *p;
	int error;

	if (size > SNAPPER_DMALIST_MAX * SNAPPER_DMASEG_MAX)
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
