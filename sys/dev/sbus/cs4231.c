/*	$OpenBSD: cs4231.c,v 1.1 2001/09/30 00:45:17 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 * Driver for CS4231 based audio found in some sun4m systems (cs4231)
 * based on ideas from the S/Linux project and the NetBSD project.
 */

#include "audio.h"
#if NAUDIO > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/auconv.h>

#include <dev/sbus/sbusvar.h>
#include <dev/sbus/cs4231reg.h>
#include <dev/sbus/cs4231var.h>

#define	CSAUDIO_DAC_LVL		0
#define	CSAUDIO_LINE_IN_LVL	1
#define	CSAUDIO_MIC_LVL		2
#define	CSAUDIO_CD_LVL		3
#define	CSAUDIO_MONITOR_LVL	4
#define	CSAUDIO_OUTPUT_LVL	5
#define	CSAUDIO_LINE_IN_MUTE	6
#define	CSAUDIO_DAC_MUTE	7
#define	CSAUDIO_CD_MUTE		8
#define	CSAUDIO_MIC_MUTE	9
#define	CSAUDIO_MONITOR_MUTE	10
#define	CSAUDIO_OUTPUT_MUTE	11
#define	CSAUDIO_REC_LVL		12
#define	CSAUDIO_RECORD_SOURCE	13
#define	CSAUDIO_OUTPUT		14
#define	CSAUDIO_INPUT_CLASS	15
#define	CSAUDIO_OUTPUT_CLASS	16
#define	CSAUDIO_RECORD_CLASS	17
#define	CSAUDIO_MONITOR_CLASS	18

#define	CSPORT_AUX2		0
#define	CSPORT_AUX1		1
#define	CSPORT_DAC		2
#define	CSPORT_LINEIN		3
#define	CSPORT_MONO		4
#define	CSPORT_MONITOR		5
#define	CSPORT_SPEAKER		6
#define	CSPORT_LINEOUT		7
#define	CSPORT_HEADPHONE	8

#define MIC_IN_PORT	0
#define LINE_IN_PORT	1
#define AUX1_IN_PORT	2
#define DAC_IN_PORT	3

#ifdef AUDIO_DEBUG
#define	DPRINTF(x)	printf x
#else
#define	DPRINTF(x)
#endif

#define CS_WRITE(sc,r,v)	\
    bus_space_write_1(sc->sc_bustag, sc->sc_regs, r, v)
#define	CS_READ(sc,r)		\
    bus_space_read_1(sc->sc_bustag, sc->sc_regs, r)

#define	APC_WRITE(sc,r,v)	\
    bus_space_write_4(sc->sc_bustag, sc->sc_regs, r, v)
#define	APC_READ(sc,r)		\
    bus_space_read_4(sc->sc_bustag, sc->sc_regs, r)

int	cs4231_match	__P((struct device *, void *, void *));
void	cs4231_attach	__P((struct device *, struct device *, void *));
int	cs4231_intr	__P((void *));

void	cs4231_wait		__P((struct cs4231_softc *));
int	cs4231_set_speed	__P((struct cs4231_softc *, u_long *));
void	cs4231_mute_monitor	__P((struct cs4231_softc *, int));
void	cs4231_setup_output	__P((struct cs4231_softc *sc));

void		cs4231_write	__P((struct cs4231_softc *, u_int8_t, u_int8_t));
u_int8_t	cs4231_read	__P((struct cs4231_softc *, u_int8_t));

/* Audio interface */
int	cs4231_open		__P((void *, int));
void	cs4231_close		__P((void *));
int	cs4231_query_encoding	__P((void *, struct audio_encoding *));
int	cs4231_set_params	__P((void *, int, int, struct audio_params *,
    struct audio_params *));
int	cs4231_round_blocksize	__P((void *, int));
int	cs4231_commit_settings	__P((void *));
int	cs4231_halt_output	__P((void *));
int	cs4231_halt_input	__P((void *));
int	cs4231_getdev		__P((void *, struct audio_device *));
int	cs4231_set_port		__P((void *, mixer_ctrl_t *));
int	cs4231_get_port		__P((void *, mixer_ctrl_t *));
int	cs4231_query_devinfo	__P((void *addr, mixer_devinfo_t *));
void *	cs4231_alloc		__P((void *, u_long, int, int));
void	cs4231_free		__P((void *, void *, int));
u_long	cs4231_round_buffersize	__P((void *, u_long));
int	cs4231_get_props	__P((void *));
int	cs4231_trigger_output __P((void *, void *, void *, int,
    void (*intr)__P((void *)), void *arg, struct audio_params *));
int	cs4231_trigger_input __P((void *, void *, void *, int,
    void (*intr)__P((void *)), void *arg, struct audio_params *));

struct audio_hw_if cs4231_sa_hw_if = {
	cs4231_open,
	cs4231_close,
	0,
	cs4231_query_encoding,
	cs4231_set_params,
	cs4231_round_blocksize,
	cs4231_commit_settings,
	0,
	0,
	0,
	0,
	cs4231_halt_output,
	cs4231_halt_input,
	0,
	cs4231_getdev,
	0,
	cs4231_set_port,
	cs4231_get_port,
	cs4231_query_devinfo,
	cs4231_alloc,
	cs4231_free,
	cs4231_round_buffersize,
	0,
	cs4231_get_props,
	cs4231_trigger_output,
	cs4231_trigger_input
};

struct cfattach audiocs_ca = {
	sizeof (struct cs4231_softc), cs4231_match, cs4231_attach
};

struct cfdriver audiocs_cd = {
	NULL, "audiocs", DV_DULL
};

struct audio_device cs4231_device = {
	"SUNW,CS4231",
	"b",
	"onboard1",		/* XXX unknown for ultra */
};

int
cs4231_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct sbus_attach_args *sa = aux;

	return (strcmp("SUNW,CS4231", sa->sa_name) == 0);
}

void    
cs4231_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	struct cs4231_softc *sc = (struct cs4231_softc *)self;
	int node;
	u_int32_t sbusburst, burst;

	node = sa->sa_node;

	/* Pass on the bus tags */
	sc->sc_bustag = sa->sa_bustag;
	sc->sc_dmatag = sa->sa_dmatag;

	/* Make sure things are sane. */
	if (sa->sa_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", sa->sa_nintr);
		return;
	}
	if (sa->sa_nreg != 1) {
		printf(": expected 1 register set, got %d\n",
		    sa->sa_nreg);
		return;
	}

	if (bus_intr_establish(sa->sa_bustag, sa->sa_pri, IPL_AUDIO, 0,
	    cs4231_intr, sc) == NULL) {
		printf(": couldn't establish interrupt, pri %d\n", sa->sa_pri);
		return;
	}

	if (sbus_bus_map(sa->sa_bustag,
	    (bus_type_t)sa->sa_reg[0].sbr_slot,
	    (bus_addr_t)sa->sa_reg[0].sbr_offset,
	    (bus_size_t)sa->sa_reg[0].sbr_size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_regs) != 0) {
		printf(": couldn't map registers\n", self->dv_xname);
		return;
	}

	sbus_establish(&sc->sc_sd, &sc->sc_dev);

	sbusburst = ((struct sbus_softc *)parent)->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1;	/* 1->16 */
	burst = getpropint(node, "burst-sizes", -1);
	if (burst == -1)
		burst = sbusburst;
	sc->sc_burst = burst & sbusburst;

	printf(": pri %d\n", sa->sa_pri);

	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);

	audio_attach_mi(&cs4231_sa_hw_if, sc, &sc->sc_dev);

	/* Default to speaker, unmuted, reasonable volume */
	sc->sc_out_port = CSPORT_SPEAKER;
	sc->sc_mute[CSPORT_SPEAKER] = 1;
	sc->sc_volume[CSPORT_SPEAKER].left = 192;
	sc->sc_volume[CSPORT_SPEAKER].right = 192;
}

/*
 * Write to one of the indirect registers of cs4231.
 */
void
cs4231_write(sc, r, v)
	struct cs4231_softc *sc;
	u_int8_t r, v;
{
	CS_WRITE(sc, CS4231_IAR, r);
	CS_WRITE(sc, CS4231_IDR, v);
}

/*
 * Read from one of the indirect registers of cs4231.
 */
u_int8_t
cs4231_read(sc, r)
	struct cs4231_softc *sc;
	u_int8_t r;
{
	CS_WRITE(sc, CS4231_IAR, r);
	return (CS_READ(sc, CS4231_IDR));
}

void
cs4231_mute_monitor(sc, mute)
	struct cs4231_softc *sc;
	int mute;
{
	u_int8_t lv, rv;

	lv = cs4231_read(sc, CS_IAR_LDACOUT);
	rv = cs4231_read(sc, CS_IAR_RDACOUT);
	if (mute) {
		lv |= CS_LDACOUT_LDM;
		rv |= CS_RDACOUT_RDM;
	} else {
		lv &= ~CS_LDACOUT_LDM;
		rv &= ~CS_RDACOUT_RDM;
	}
	cs4231_write(sc, CS_IAR_LDACOUT, lv);
	cs4231_write(sc, CS_IAR_RDACOUT, rv);
}

int
cs4231_set_speed(sc, argp)
	struct cs4231_softc *sc;
	u_long *argp;

{
	/*
	 * The available speeds are in the following table. Keep the speeds in
	 * the increasing order.
	 */
	typedef struct {
		int speed;
		u_char bits;
	} speed_struct;
	u_long arg = *argp;

	static speed_struct speed_table[] = {
		{5510,	(0 << 1) | CS_FSPB_C2SL_XTAL2},
		{5510,	(0 << 1) | CS_FSPB_C2SL_XTAL2},
		{6620,	(7 << 1) | CS_FSPB_C2SL_XTAL2},
		{8000,	(0 << 1) | CS_FSPB_C2SL_XTAL1},
		{9600,	(7 << 1) | CS_FSPB_C2SL_XTAL1},
		{11025,	(1 << 1) | CS_FSPB_C2SL_XTAL2},
		{16000,	(1 << 1) | CS_FSPB_C2SL_XTAL1},
		{18900,	(2 << 1) | CS_FSPB_C2SL_XTAL2},
		{22050,	(3 << 1) | CS_FSPB_C2SL_XTAL2},
		{27420,	(2 << 1) | CS_FSPB_C2SL_XTAL1},
		{32000,	(3 << 1) | CS_FSPB_C2SL_XTAL1},
		{33075,	(6 << 1) | CS_FSPB_C2SL_XTAL2},
		{33075,	(4 << 1) | CS_FSPB_C2SL_XTAL2},
		{44100,	(5 << 1) | CS_FSPB_C2SL_XTAL2},
		{48000,	(6 << 1) | CS_FSPB_C2SL_XTAL1},
	};

	int i, n, selected = -1;

	n = sizeof(speed_table) / sizeof(speed_struct);

	if (arg < speed_table[0].speed)
		selected = 0;
	if (arg > speed_table[n - 1].speed)
		selected = n - 1;

	for (i = 1; selected == -1 && i < n; i++) {
		if (speed_table[i].speed == arg)
			selected = i;
		else if (speed_table[i].speed > arg) {
			int diff1, diff2;

			diff1 = arg - speed_table[i - 1].speed;
			diff2 = speed_table[i].speed - arg;
			if (diff1 < diff2)
				selected = i - 1;
			else
				selected = i;
		}
	}

	if (selected == -1) {
		printf("%s: can't find speed\n", sc->sc_dev.dv_xname);
		selected = 3;
	}

	sc->sc_speed_bits = speed_table[selected].bits;
	sc->sc_need_commit = 1;
	*argp = speed_table[selected].speed;

	return (0);
}

void
cs4231_wait(sc)
	struct cs4231_softc *sc;
{
	int tries;
	u_int8_t ir;

	DELAY(100);

	CS_WRITE(sc, CS4231_IAR, ~(CS_IAR_MCE));
	tries = CS_TIMEOUT;
	while (1) {
		ir = CS_READ(sc, CS4231_IAR);
		if (ir != CS_IAR_INIT)
			break;
		if (--tries == 0)
			break;
		DELAY(100);
	}
	if (!tries)
		printf("%s: waited too long to reset iar\n",
		    sc->sc_dev.dv_xname);

	CS_WRITE(sc, CS4231_IAR, CS_IAR_ERRINIT);
	tries = CS_TIMEOUT;
	while (1) {
		ir = CS_READ(sc, CS4231_IDR);
		if (ir != CS_ERRINIT_ACI)
			break;
		if (--tries == 0)
			break;
		DELAY(100);
	}
	if (!tries)
		printf("%s: waited too long to reset errinit\n",
		    sc->sc_dev.dv_xname);
}

/*
 * Audio interface functions
 */
int
cs4231_open(addr, flags)
	void *addr;
	int flags;
{
	struct cs4231_softc *sc = addr;
	u_int8_t reg;

	if (sc->sc_open)
		return (EBUSY);
	sc->sc_open = 1;
	sc->sc_locked = 0;
	sc->sc_rintr = 0;
	sc->sc_rarg = 0;
	sc->sc_pintr = 0;
	sc->sc_parg = 0;

	APC_WRITE(sc, APC_CSR, APC_CSR_RESET);
	DELAY(10);
	APC_WRITE(sc, APC_CSR, 0);
	DELAY(10);
	APC_WRITE(sc, APC_CSR, APC_READ(sc, APC_CSR) | APC_CSR_CODEC_RESET);

	DELAY(20);

	APC_WRITE(sc, APC_CSR, APC_READ(sc, APC_CSR) & (~APC_CSR_CODEC_RESET));
	CS_WRITE(sc, CS4231_IAR, CS_READ(sc, CS4231_IAR) | CS_IAR_MCE);

	cs4231_wait(sc);

	cs4231_write(sc, CS_IAR_MCE | CS_IAR_MODEID, CS_MODEID_MODE2);

	reg = cs4231_read(sc, CS_IAR_VID);
	if ((reg & CS_VID_CHIP_MASK) == CS_VID_CHIP_CS4231) {
		switch (reg & CS_VID_VER_MASK) {
		case CS_VID_VER_CS4231A:
		case CS_VID_VER_CS4231:
		case CS_VID_VER_CS4232:
			break;
		default:
			printf("%s: unknown CS version: %d\n",
			    sc->sc_dev.dv_xname, reg & CS_VID_VER_MASK);
		}
	}
	else {
		printf("%s: unknown CS chip/version: %d/%d\n",
		    sc->sc_dev.dv_xname, reg & CS_VID_CHIP_MASK,
		    reg & CS_VID_VER_MASK);
	}

	/* XXX TODO: setup some defaults */
	CS_WRITE(sc, CS4231_IAR, ~(CS_IAR_MCE));
	cs4231_wait(sc);

	reg = cs4231_read(sc, CS_IAR_MCE | CS_IAR_IC);
	reg &= ~(CS_IC_CAL_CONV);
	cs4231_write(sc, CS_IAR_MCE | CS_IAR_IC, reg);
	CS_WRITE(sc, CS4231_IAR, ~(CS_IAR_MCE));
	cs4231_wait(sc);

	cs4231_setup_output(sc);
	return (0);
}

void
cs4231_setup_output(sc)
	struct cs4231_softc *sc;
{
	u_int8_t r;

	r = cs4231_read(sc, CS_IAR_PC);
	r |= CS_PC_HDPHMUTE | CS_PC_LINEMUTE;
	cs4231_write(sc, CS_IAR_PC, r);

	r = cs4231_read(sc, CS_IAR_MONO);
	r |= CS_MONO_MOM;
	cs4231_write(sc, CS_IAR_MONO, r);

	switch (sc->sc_out_port) {
	case CSPORT_HEADPHONE:
		if (sc->sc_mute[CSPORT_SPEAKER]) {
			r = cs4231_read(sc, CS_IAR_PC);
			r &= ~CS_PC_HDPHMUTE;
			cs4231_write(sc, CS_IAR_PC, r);
		}
		break;
	case CSPORT_SPEAKER:
		if (sc->sc_mute[CSPORT_SPEAKER]) {
			r = cs4231_read(sc, CS_IAR_MONO);
			r &= ~CS_MONO_MOM;
			cs4231_write(sc, CS_IAR_MONO, r);
		}
		break;
	case CSPORT_LINEOUT:
		if (sc->sc_mute[CSPORT_SPEAKER]) {
			r = cs4231_read(sc, CS_IAR_PC);
			r &= ~CS_PC_LINEMUTE;
			cs4231_write(sc, CS_IAR_PC, r);
		}
		break;
	}

	r = cs4231_read(sc, CS_IAR_LDACOUT);
	r &= ~CS_LDACOUT_LDA_MASK;
	r |= (~(sc->sc_volume[CSPORT_SPEAKER].left >> 2)) &
	    CS_LDACOUT_LDA_MASK;
	cs4231_write(sc, CS_IAR_LDACOUT, r);

	r = cs4231_read(sc, CS_IAR_RDACOUT);
	r &= ~CS_RDACOUT_RDA_MASK;
	r |= (~(sc->sc_volume[CSPORT_SPEAKER].right >> 2)) &
	    CS_RDACOUT_RDA_MASK;
	cs4231_write(sc, CS_IAR_RDACOUT, r);
}

void
cs4231_close(addr)
	void *addr;
{
	struct cs4231_softc *sc = addr;

	cs4231_halt_input(sc);
	cs4231_halt_output(sc);
	sc->sc_open = 0;
}

int
cs4231_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
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
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 3:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 4:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 8:
		strcpy(fp->name, AudioEadpcm);
		fp->encoding = AUDIO_ENCODING_ADPCM;
		fp->precision = 8;
		fp->flags = 0;
		break;
	default:
		err = EINVAL;
	}
	return (err);
}

int
cs4231_set_params(addr, setmode, usemode, p, r)
	void *addr;
	int setmode, usemode;
	struct audio_params *p, *r;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	int err, bits, enc;
	void (*pswcode) __P((void *, u_char *, int cnt));
	void (*rswcode) __P((void *, u_char *, int cnt));

	enc = p->encoding;
	pswcode = rswcode = 0;
	switch (enc) {
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision == 8) {
			enc = AUDIO_ENCODING_ULINEAR_LE;
			pswcode = rswcode = change_sign8;
		}
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (p->precision == 16) {
			enc = AUDIO_ENCODING_SLINEAR_LE;
			pswcode = rswcode = change_sign16;
		}
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		if (p->precision == 16) {
			enc = AUDIO_ENCODING_SLINEAR_BE;
			pswcode = rswcode = change_sign16;
		}
		break;
	}

	switch (enc) {
	case AUDIO_ENCODING_ULAW:
		bits = CS_CDF_FMT_ULAW >> 5;
		break;
	case AUDIO_ENCODING_ALAW:
		bits = CS_CDF_FMT_ALAW >> 5;
		break;
	case AUDIO_ENCODING_ADPCM:
		bits = CS_CDF_FMT_ADPCM >> 5;
		break;
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision == 16)
			bits = CS_CDF_FMT_LINEAR_LE >> 5;
		else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (p->precision == 16)
			bits = CS_CDF_FMT_LINEAR_BE >> 5;
		else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (p->precision == 8)
			bits = CS_CDF_FMT_ULINEAR >> 5;
		else
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	if (p->channels != 1 && p->channels != 2)
		return (EINVAL);

	err = cs4231_set_speed(sc, &p->sample_rate);
	if (err)
		return (err);

	p->sw_code = pswcode;
	r->sw_code = rswcode;

	sc->sc_format_bits = bits;
	sc->sc_channels = p->channels;
	sc->sc_precision = p->precision;
	sc->sc_need_commit = 1;
	return (0);
}

int
cs4231_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	return (blk & (-4));
}

int
cs4231_commit_settings(addr)
	void *addr;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	int s, tries;
	u_int8_t fs;

	if (sc->sc_need_commit == 0)
		return (0);

	s = splaudio();

	cs4231_mute_monitor(sc, 1);

	fs = sc->sc_speed_bits | (sc->sc_format_bits << 5);
	if (sc->sc_channels == 2)
		fs |= CS_FSPB_SM_STEREO;

	cs4231_write(sc, CS_IAR_MCE | CS_IAR_FSPB, fs);
	CS_READ(sc, CS4231_IDR);
	CS_READ(sc, CS4231_IDR);
	tries = 100000;
	while (1) {
		if (CS_READ(sc, CS4231_IAR) != CS_IAR_INIT)
			break;
		if (--tries == 0)
			break;
		DELAY(10);
	}
	if (tries == 0) {
		printf("%s: timeout committing fspb\n", sc->sc_dev.dv_xname);
		splx(s);
		return (0);
	}

	cs4231_write(sc, CS_IAR_MCE | CS_IAR_CDF, fs);
	CS_READ(sc, CS4231_IDR);
	CS_READ(sc, CS4231_IDR);
	tries = 100000;
	while (1) {
		if (CS_READ(sc, CS4231_IAR) != CS_IAR_INIT)
			break;
		if (--tries == 0)
			break;
		DELAY(10);
	}
	if (tries == 0) {
		printf("%s: timeout committing cdf\n", sc->sc_dev.dv_xname);
		splx(s);
		return (0);
	}

	cs4231_wait(sc);

	cs4231_mute_monitor(sc, 0);

	splx(s);

	sc->sc_need_commit = 0;
	return (0);
}

int
cs4231_halt_output(addr)
	void *addr;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;

	APC_WRITE(sc, APC_CSR, APC_READ(sc, APC_CSR) &
	    ~(APC_CSR_EI | APC_CSR_GIE | APC_CSR_PIE |
	      APC_CSR_EIE | APC_CSR_PDMA_GO | APC_CSR_PMIE));
	cs4231_write(sc, CS_IAR_IC,
	    cs4231_read(sc, CS_IAR_IC) & (~CS_IC_PEN));
	sc->sc_locked = 0;
	return (0);
}

int
cs4231_halt_input(addr)
	void *addr;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;

	APC_WRITE(sc, APC_CSR, APC_CSR_CAPTURE_PAUSE);
	cs4231_write(sc, CS_IAR_IC,
	    cs4231_read(sc, CS_IAR_IC) & (~CS_IC_CEN));
	sc->sc_locked = 0;
	return (0);
}

int
cs4231_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	*retp = cs4231_device;
	return (0);
}

int
cs4231_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	int error = EINVAL;

	DPRINTF(("cs4231_set_port: port=%d type=%d\n", cp->dev, cp->type));

	switch (cp->dev) {
	case CSAUDIO_DAC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cs4231_write(sc, CS_IAR_LACIN1,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    CS_LACIN1_GAIN_MASK);
		else if (cp->un.value.num_channels == 2) {
			cs4231_write(sc, CS_IAR_LACIN1,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] &
			    CS_LACIN1_GAIN_MASK);
			cs4231_write(sc, CS_IAR_RACIN1,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] &
			    CS_RACIN1_GAIN_MASK);
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_LINE_IN_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cs4231_write(sc, CS_IAR_LLI,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    CS_LLI_GAIN_MASK);
		else if (cp->un.value.num_channels == 2) {
			cs4231_write(sc, CS_IAR_LLI,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] &
			    CS_LLI_GAIN_MASK);
			cs4231_write(sc, CS_IAR_RLI,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] &
			    CS_RLI_GAIN_MASK);
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MIC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
#if 0
			cs4231_write(sc, CS_IAR_MONO,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    CS_MONO_MIA_MASK);
#endif
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_CD_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			cs4231_write(sc, CS_IAR_LACIN2,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    CS_LACIN2_GAIN_MASK);
		} else if (cp->un.value.num_channels == 2) {
			cs4231_write(sc, CS_IAR_LACIN2,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] &
			    CS_LACIN2_GAIN_MASK);
			cs4231_write(sc, CS_IAR_RACIN2,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] &
			    CS_RACIN2_GAIN_MASK);
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cs4231_write(sc, CS_IAR_LOOP,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] << 2);
		else
			break;
		error = 0;
		break;
	case CSAUDIO_OUTPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_volume[CSPORT_SPEAKER].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			sc->sc_volume[CSPORT_SPEAKER].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		}
		else if (cp->un.value.num_channels == 2) {
			sc->sc_volume[CSPORT_SPEAKER].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_volume[CSPORT_SPEAKER].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		}
		else
			break;

		cs4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_OUTPUT:
		if (cp->un.ord != CSPORT_LINEOUT &&
		    cp->un.ord != CSPORT_SPEAKER &&
		    cp->un.ord != CSPORT_HEADPHONE)
			return (EINVAL);
		sc->sc_out_port = cp->un.ord;
		cs4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_LINE_IN_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_LINEIN] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_DAC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_AUX1] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_CD_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_AUX2] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_MIC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_MONO] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_MONITOR_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_MONITOR] = cp->un.ord ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_OUTPUT_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_SPEAKER] = cp->un.ord ? 1 : 0;
		cs4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_REC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		break;
	case CSAUDIO_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		break;
	}

	return (error);
}

int
cs4231_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	int error = EINVAL;

	DPRINTF(("cs4231_get_port: port=%d type=%d\n", cp->dev, cp->type));

	switch (cp->dev) {
	case CSAUDIO_DAC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]=
			    cs4231_read(sc, CS_IAR_LACIN1) &
			    CS_LACIN1_GAIN_MASK;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    cs4231_read(sc, CS_IAR_LACIN1) &
			    CS_LACIN1_GAIN_MASK;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    cs4231_read(sc, CS_IAR_RACIN1) &
			    CS_RACIN1_GAIN_MASK;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_LINE_IN_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    cs4231_read(sc, CS_IAR_LLI) & CS_LLI_GAIN_MASK;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    cs4231_read(sc, CS_IAR_LLI) & CS_LLI_GAIN_MASK;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    cs4231_read(sc, CS_IAR_RLI) & CS_RLI_GAIN_MASK;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MIC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
#if 0
			sc->sc_regs->iar = CS_IAR_MONO;
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    cs4231_read(sc, CS_IAR_MONO) &
			    CS_MONO_MIA_MASK;
#endif
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_CD_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    cs4231_read(sc, CS_IAR_LACIN2) &
			    CS_LACIN2_GAIN_MASK;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    cs4231_read(sc, CS_IAR_LACIN2) &
			    CS_LACIN2_GAIN_MASK;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    cs4231_read(sc, CS_IAR_RACIN2) &
			    CS_RACIN2_GAIN_MASK;
		}
		else
			break;
		error = 0;
		break;
	case CSAUDIO_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels != 1)
			break;
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
		    cs4231_read(sc, CS_IAR_LOOP) >> 2;
		error = 0;
		break;
	case CSAUDIO_OUTPUT_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    sc->sc_volume[CSPORT_SPEAKER].left;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    sc->sc_volume[CSPORT_SPEAKER].left;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    sc->sc_volume[CSPORT_SPEAKER].right;
		}
		else
			break;
		error = 0;
		break;
	case CSAUDIO_LINE_IN_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_LINEIN] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_DAC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_AUX1] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_CD_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_AUX2] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_MIC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_MONO] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_MONITOR_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_MONITOR] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_OUTPUT_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = sc->sc_mute[CSPORT_SPEAKER] ? 1 : 0;
		error = 0;
		break;
	case CSAUDIO_REC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    AUDIO_MIN_GAIN;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    AUDIO_MIN_GAIN;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    AUDIO_MIN_GAIN;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM) break;
		cp->un.ord = MIC_IN_PORT;
		error = 0;
		break;
	case CSAUDIO_OUTPUT:
		if (cp->type != AUDIO_MIXER_ENUM) break;
		cp->un.ord = sc->sc_out_port;
		error = 0;
		break;
	default:
		printf("Invalid kind!\n");
	}
	return (error);
}

int
cs4231_query_devinfo(addr, dip)
	void *addr;
	mixer_devinfo_t *dip;
{
	int err = 0;

	switch (dip->index) {
	case CSAUDIO_MIC_LVL:		/* mono/microphone mixer */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_MIC_MUTE;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_DAC_LVL:		/* dacout */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_DAC_MUTE;
		strcpy(dip->label.name, AudioNdac);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_LINE_IN_LVL:	/* line */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_LINE_IN_MUTE;
		strcpy(dip->label.name, AudioNline);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_CD_LVL:		/* cd */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_CD_MUTE;
		strcpy(dip->label.name, AudioNcd);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_MONITOR_LVL:	/* monitor level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_MONITOR_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_MONITOR_MUTE;
		strcpy(dip->label.name, AudioNmonitor);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_OUTPUT_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_OUTPUT_MUTE;
		strcpy(dip->label.name, AudioNoutput);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_LINE_IN_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = CSAUDIO_LINE_IN_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_DAC_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = CSAUDIO_DAC_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_CD_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = CSAUDIO_CD_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_MIC_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = CSAUDIO_MIC_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_MONITOR_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = CSAUDIO_MONITOR_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_OUTPUT_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = CSAUDIO_OUTPUT_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;

	mute:
		strcpy(dip->label.name, AudioNmute);
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNon);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNoff);
		dip->un.e.member[1].ord = 1;
		break;
	case CSAUDIO_REC_LVL:		/* record level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_RECORD_SOURCE;
		strcpy(dip->label.name, AudioNrecord);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case CSAUDIO_RECORD_SOURCE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = CSAUDIO_REC_LVL;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->un.e.num_mem = 3;
		strcpy(dip->un.e.member[0].label.name, AudioNcd);
		dip->un.e.member[0].ord = DAC_IN_PORT;
		strcpy(dip->un.e.member[1].label.name, AudioNmicrophone);
		dip->un.e.member[1].ord = MIC_IN_PORT;
		strcpy(dip->un.e.member[2].label.name, AudioNdac);
		dip->un.e.member[2].ord = AUX1_IN_PORT;
		strcpy(dip->un.e.member[3].label.name, AudioNline);
		dip->un.e.member[3].ord = LINE_IN_PORT;
		break;
	case CSAUDIO_OUTPUT:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_MONITOR_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNoutput);
		dip->un.e.num_mem = 3;
		strcpy(dip->un.e.member[0].label.name, AudioNspeaker);
		dip->un.e.member[0].ord = CSPORT_SPEAKER;
		strcpy(dip->un.e.member[1].label.name, AudioNline);
		dip->un.e.member[1].ord = CSPORT_LINEOUT;
		strcpy(dip->un.e.member[2].label.name, AudioNheadphone);
		dip->un.e.member[2].ord = CSPORT_HEADPHONE;
		break;
	case CSAUDIO_INPUT_CLASS:	/* input class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		break;
	case CSAUDIO_OUTPUT_CLASS:	/* output class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		break;
	case CSAUDIO_MONITOR_CLASS:	/* monitor class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_MONITOR_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCmonitor);
		break;
	case CSAUDIO_RECORD_CLASS:	/* record class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		break;
	default:
		err = ENXIO;
	}

	return (err);
}

u_long
cs4231_round_buffersize(addr, size)
	void *addr;
	u_long size;
{
	return (size);
}

int
cs4231_get_props(addr)
	void *addr;
{
	return (AUDIO_PROP_FULLDUPLEX);
}

/*
 * Hardware interrupt handler
 */
int
cs4231_intr(v)
	void *v;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)v;
	u_int32_t csr;
	u_int8_t reg, status;
	struct cs_dma *p;
	int r = 0;

	csr = APC_READ(sc, APC_CSR);
	status = CS_READ(sc, CS4231_STS);
	if (status & (CS_STATUS_INT | CS_STATUS_SER)) {
		reg = cs4231_read(sc, CS_IAR_AFS);
		if (reg & CS_AFS_PI) {
			cs4231_write(sc, CS_IAR_PBLB, 0xff);
			cs4231_write(sc, CS_IAR_PBUB, 0xff);
		}
		CS_WRITE(sc, CS4231_STS, 0);
	}

	APC_WRITE(sc, APC_CSR, csr);

	if (csr & (APC_CSR_PI|APC_CSR_PMI|APC_CSR_PIE|APC_CSR_PD))
		r = 1;

	if (csr & APC_CSR_PM) {
		u_long nextaddr, togo;

		p = sc->sc_nowplaying;
		togo = sc->sc_playsegsz - sc->sc_playcnt;
		if (togo == 0) {
			nextaddr = (u_int32_t)p->dmamap->dm_segs[0].ds_addr;
			sc->sc_playcnt = togo = sc->sc_blksz;
		} else {
			nextaddr = APC_READ(sc, APC_PNVA) + sc->sc_blksz;
			if (togo > sc->sc_blksz)
				togo = sc->sc_blksz;
			sc->sc_playcnt += togo;
		}

		APC_WRITE(sc, APC_PNVA, nextaddr);
		APC_WRITE(sc, APC_PNC, togo);

		if (sc->sc_pintr != NULL)
			(*sc->sc_pintr)(sc->sc_parg);
		r = 1;
	}

	if (csr & APC_CSR_CI) {
		if (sc->sc_rintr != NULL) {
			r = 1;
			(*sc->sc_rintr)(sc->sc_rarg);
		}
	}

	return (r);
}

void *
cs4231_alloc(addr, size, pool, flags)
	void *addr;
	u_long size;
	int pool;
	int flags;
{
	struct cs4231_softc *sc = (struct cs4231_softc *)addr;
	bus_dma_tag_t dmat = sc->sc_dmatag;
	struct cs_dma *p;

	p = (struct cs_dma *)malloc(sizeof(struct cs_dma), pool, flags);
	if (p == NULL)
		goto fail;

	if (bus_dmamap_create(dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &p->dmamap) != 0)
		goto fail;

	p->size = size;

	if (bus_dmamem_alloc(dmat, size, 64*1024, 0, p->segs,
	    sizeof(p->segs)/sizeof(p->segs[0]), &p->nsegs,
	    BUS_DMA_NOWAIT) != 0)
		goto fail1;

	if (bus_dmamem_map(dmat, p->segs, p->nsegs, p->size,
	    &p->addr, BUS_DMA_NOWAIT | BUS_DMA_COHERENT) != 0)
		goto fail2;

	if (bus_dmamap_load(dmat, p->dmamap, p->addr, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto fail3;

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;
	return (p->addr);

fail3:
	bus_dmamem_unmap(dmat, p->addr, p->size);
fail2:
	bus_dmamem_free(dmat, p->segs, p->nsegs);
fail1:
	bus_dmamap_destroy(dmat, p->dmamap);
fail:
	return (NULL);
}

void
cs4231_free(addr, ptr, pool)
	void *addr;
	void *ptr;
	int pool;
{
	struct cs4231_softc *sc = addr;
	bus_dma_tag_t dmat = sc->sc_dmatag;
	struct cs_dma *p, **pp;

	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &(*pp)->next) {
		if (p->addr != ptr)
			continue;
		bus_dmamap_unload(dmat, p->dmamap);
		bus_dmamem_unmap(dmat, p->addr, p->size);
		bus_dmamem_free(dmat, p->segs, p->nsegs);
		bus_dmamap_destroy(dmat, p->dmamap);
		*pp = p->next;
		free(p, pool);
		return;
	}
	printf("%s: attempt to free rogue pointer\n", sc->sc_dev.dv_xname);
}

int
cs4231_trigger_output(addr, start, end, blksize, intr, arg, param)
	void *addr, *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct cs4231_softc *sc = addr;
	struct cs_dma *p;
	u_int32_t csr;
	vaddr_t n;

	if (sc->sc_locked != 0) {
		printf("%s: trigger_output: already running\n",
		    sc->sc_dev.dv_xname);
		return (EINVAL);
	}

	sc->sc_locked = 1;
	sc->sc_pintr = intr;
	sc->sc_parg = arg;

	for (p = sc->sc_dmas; p->addr != start; p = p->next)
		/*EMPTY*/;
	if (p == NULL) {
		printf("%s: trigger_output: bad addr: %x\n",
		    sc->sc_dev.dv_xname, start);
		return (EINVAL);
	}

	n = (char *)end - (char *)start;

	/*
	 * Do only `blksize' at a time, so audio_pint() is kept
	 * synchronous with us...
	 */
	sc->sc_blksz = blksize;
	sc->sc_nowplaying = p;
	sc->sc_playsegsz = n;

	if (n > sc->sc_blksz)
		n = sc->sc_blksz;

	sc->sc_playcnt = n;

	csr = APC_READ(sc, APC_CSR);

	APC_WRITE(sc, APC_PNVA, (u_long)p->dmamap->dm_segs[0].ds_addr);
	APC_WRITE(sc, APC_PNC, (u_long)n);

	if ((csr & APC_CSR_PDMA_GO) == 0 || (csr & APC_CSR_PPAUSE) != 0) {
		APC_WRITE(sc, APC_CSR,
		    APC_READ(sc, APC_CSR) & ~(APC_CSR_PIE | APC_CSR_PPAUSE));
		APC_WRITE(sc, APC_CSR, APC_READ(sc, APC_CSR) |
		    APC_CSR_EI | APC_CSR_GIE | APC_CSR_PIE | APC_CSR_EIE |
		    APC_CSR_PMIE | APC_CSR_PDMA_GO);
		cs4231_write(sc, CS_IAR_PBLB, 0xff);
		cs4231_write(sc, CS_IAR_PBUB, 0xff);
		cs4231_write(sc, CS_IAR_IC,
		    cs4231_read(sc, CS_IAR_IC) | CS_IC_PEN);
	}
	return (0);
}

int
cs4231_trigger_input(addr, start, end, blksize, intr, arg, param)
	void *addr, *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	return (ENXIO);
}

#endif /* NAUDIO > 0 */
