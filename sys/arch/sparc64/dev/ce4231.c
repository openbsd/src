/*	$OpenBSD: ce4231.c,v 1.23 2009/12/01 23:17:11 edd Exp $	*/

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
 * Driver for CS4231 based audio found in some sun4u systems (cs4231)
 * based on ideas from the S/Linux project and the NetBSD project.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/auconv.h>

#include <sparc64/dev/ebusreg.h>
#include <sparc64/dev/ebusvar.h>
#include <sparc64/dev/ce4231var.h>

/* AD1418 provides basic registers, CS4231 extends with more */
#include <dev/ic/ad1848reg.h>
#include <dev/ic/cs4231reg.h>

/* Mixer classes and mixer knobs */
#define	CSAUDIO_DAC_LVL		0
#define	CSAUDIO_LINE_IN_LVL	1
#define	CSAUDIO_MIC_LVL		2
#define	CSAUDIO_CD_LVL		3
#define	CSAUDIO_MONITOR_LVL	4
#define	CSAUDIO_LINE_IN_MUTE	5
#define	CSAUDIO_DAC_MUTE	6
#define	CSAUDIO_CD_MUTE		7
#define	CSAUDIO_MIC_MUTE	8
#define	CSAUDIO_MONITOR_MUTE	9
#define	CSAUDIO_REC_LVL		10
#define	CSAUDIO_RECORD_SOURCE	11
#define	CSAUDIO_INPUT_CLASS	12
#define	CSAUDIO_OUTPUT_CLASS	13
#define	CSAUDIO_RECORD_CLASS	14
/* The dac can output to these destinations, which *do* have mute
 * controls, but share levels with the main DAC
 */
#define CSAUDIO_DAC_MONO_MUTE	15
#define CSAUDIO_DAC_LINE_MUTE	16
#define CSAUDIO_DAC_HDPH_MUTE	17

/* Physical volume/attenuation registers, array indexes for
 * sc->sc_volume/sc->sc_mute
 *
 * Based upon my Sun blade 1000:
 * AUX2 is usually CD in
 * MONO is usually a mic
 */
#define	CSPORT_AUX2		0
#define	CSPORT_AUX1		1
#define	CSPORT_DAC		2
#define	CSPORT_LINEIN		3
#define	CSPORT_MONO		4
#define	CSPORT_MONITOR		5
#define CSPORT_ADC		6

/* Recording sources */
#define LINE_IN_PORT	0
#define AUX1_IN_PORT	1
#define MIC_IN_PORT	2
#define DAC_IN_PORT	3

/* Bits on the ADC reg that determine recording source */
#define CS_REC_SRC_BITS 0xc0

#ifdef AUDIO_DEBUG
#define	DPRINTF(x)	printf x
#else
#define	DPRINTF(x)
#endif

#define	CS_TIMEOUT	90000

#define	CS_PC_LINEMUTE	XCTL1_ENABLE
#define	CS_PC_HDPHMUTE	XCTL1_ENABLE
#define	CS_AFS_PI	0x10
#define MONO_INPUT_MUTE 0x80
#define MIX_MUTE	0x00

/* Read/write CS4231 direct registers */
#define CS_WRITE(sc,r,v)	\
    bus_space_write_1((sc)->sc_bustag, (sc)->sc_cshandle, (r) << 2, (v))
#define	CS_READ(sc,r)		\
    bus_space_read_1((sc)->sc_bustag, (sc)->sc_cshandle, (r) << 2)

/* Read/write EBDMA playback registers */
#define	P_WRITE(sc,r,v)		\
    bus_space_write_4((sc)->sc_bustag, (sc)->sc_pdmahandle, (r), (v))
#define	P_READ(sc,r)		\
    bus_space_read_4((sc)->sc_bustag, (sc)->sc_pdmahandle, (r))

/* Read/write EBDMA capture registers */
#define	C_WRITE(sc,r,v)		\
    bus_space_write_4((sc)->sc_bustag, (sc)->sc_cdmahandle, (r), (v))
#define	C_READ(sc,r)		\
    bus_space_read_4((sc)->sc_bustag, (sc)->sc_cdmahandle, (r))

int	ce4231_match(struct device *, void *, void *);
void	ce4231_attach(struct device *, struct device *, void *);
int	ce4231_cintr(void *);
int	ce4231_pintr(void *);

int	ce4231_set_speed(struct ce4231_softc *, u_long *);
void	ce4231_setup_output(struct ce4231_softc *sc);
void	ce4231_setup_input(struct ce4231_softc *sc);

void		ce4231_write(struct ce4231_softc *, u_int8_t, u_int8_t);
u_int8_t	ce4231_read(struct ce4231_softc *, u_int8_t);

/* Audio interface */
int	ce4231_open(void *, int);
void	ce4231_close(void *);
int	ce4231_query_encoding(void *, struct audio_encoding *);
int	ce4231_set_params(void *, int, int, struct audio_params *,
    struct audio_params *);
int	ce4231_round_blocksize(void *, int);
int	ce4231_commit_settings(void *);
int	ce4231_halt_output(void *);
int	ce4231_halt_input(void *);
int	ce4231_getdev(void *, struct audio_device *);
int	ce4231_set_port(void *, mixer_ctrl_t *);
int	ce4231_get_port(void *, mixer_ctrl_t *);
int	ce4231_query_devinfo(void *addr, mixer_devinfo_t *);
void *	ce4231_alloc(void *, int, size_t, int, int);
void	ce4231_free(void *, void *, int);
int	ce4231_get_props(void *);
int	ce4231_trigger_output(void *, void *, void *, int,
    void (*intr)(void *), void *arg, struct audio_params *);
int	ce4231_trigger_input(void *, void *, void *, int,
    void (*intr)(void *), void *arg, struct audio_params *);

struct audio_hw_if ce4231_sa_hw_if = {
	ce4231_open,
	ce4231_close,
	0,
	ce4231_query_encoding,
	ce4231_set_params,
	ce4231_round_blocksize,
	ce4231_commit_settings,
	0,
	0,
	0,
	0,
	ce4231_halt_output,
	ce4231_halt_input,
	0,
	ce4231_getdev,
	0,
	ce4231_set_port,
	ce4231_get_port,
	ce4231_query_devinfo,
	ce4231_alloc,
	ce4231_free,
	0,
	0,
	ce4231_get_props,
	ce4231_trigger_output,
	ce4231_trigger_input,
	0
};

struct cfattach audioce_ca = {
	sizeof (struct ce4231_softc), ce4231_match, ce4231_attach
};

struct cfdriver audioce_cd = {
	NULL, "audioce", DV_DULL
};

struct audio_device ce4231_device = {
	"SUNW,CS4231",
	"b",
	"onboard1",
};

int
ce4231_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct ebus_attach_args *ea = aux;

	if (!strcmp("SUNW,CS4231", ea->ea_name) ||
	    !strcmp("audio", ea->ea_name))
		return (1);
	return (0);
}

void    
ce4231_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ebus_attach_args *ea = aux;
	struct ce4231_softc *sc = (struct ce4231_softc *)self;
	int node;

	node = ea->ea_node;

	sc->sc_last_format = 0xffffffff;

	/* Pass on the bus tags */
	sc->sc_bustag = ea->ea_memtag;
	sc->sc_dmatag = ea->ea_dmatag;

	/* Make sure things are sane. */
	if (ea->ea_nintrs != 2) {
		printf(": expected 2 interrupts, got %d\n", ea->ea_nintrs);
		return;
	}
	if (ea->ea_nregs != 4) {
		printf(": expected 4 register set, got %d\n",
		    ea->ea_nregs);
		return;
	}

	sc->sc_cih = bus_intr_establish(sc->sc_bustag, ea->ea_intrs[0],
	    IPL_AUDIO, 0, ce4231_cintr, sc, self->dv_xname);
	if (sc->sc_cih == NULL) {
		printf(": couldn't establish capture interrupt\n");
		return;
	}
	sc->sc_pih = bus_intr_establish(sc->sc_bustag, ea->ea_intrs[1],
	    IPL_AUDIO, 0, ce4231_pintr, sc, self->dv_xname);
	if (sc->sc_pih == NULL) {
		printf(": couldn't establish play interrupt1\n");
		return;
	}

	/* XXX what if prom has already mapped?! */

	if (ebus_bus_map(sc->sc_bustag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[0]), ea->ea_regs[0].size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_cshandle) != 0) {
		printf(": couldn't map cs4231 registers\n");
		return;
	}

	if (ebus_bus_map(sc->sc_bustag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[1]), ea->ea_regs[1].size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_pdmahandle) != 0) {
		printf(": couldn't map dma1 registers\n");
		return;
	}

	if (ebus_bus_map(sc->sc_bustag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[2]), ea->ea_regs[2].size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_cdmahandle) != 0) {
		printf(": couldn't map dma2 registers\n");
		return;
	}

	if (ebus_bus_map(sc->sc_bustag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[3]), ea->ea_regs[3].size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_auxhandle) != 0) {
		printf(": couldn't map aux registers\n");
		return;
	}

	printf(": nvaddrs %d\n", ea->ea_nvaddrs);

	audio_attach_mi(&ce4231_sa_hw_if, sc, &sc->sc_dev);

	/* Default to all dacouts unmuted, reasonable volume */
	sc->sc_monoout_enable = 1;
	sc->sc_lineout_enable = 1;
	sc->sc_hdphout_enable = 1;
	sc->sc_mute[CSPORT_DAC] = 1; /* dac itself not muted */
	sc->sc_volume[CSPORT_DAC].left = 192;
	sc->sc_volume[CSPORT_DAC].right = 192;

	/* XXX get real burst... */
	sc->sc_burst = EBDCSR_BURST_8;
}

/*
 * Write to one of the indexed registers of cs4231.
 */
void
ce4231_write(sc, r, v)
	struct ce4231_softc *sc;
	u_int8_t r, v;
{
	CS_WRITE(sc, AD1848_IADDR, r);
	CS_WRITE(sc, AD1848_IDATA, v);
}

/*
 * Read from one of the indexed registers of cs4231.
 */
u_int8_t
ce4231_read(sc, r)
	struct ce4231_softc *sc;
	u_int8_t r;
{
	CS_WRITE(sc, AD1848_IADDR, r);
	return (CS_READ(sc, AD1848_IDATA));
}

int
ce4231_set_speed(sc, argp)
	struct ce4231_softc *sc;
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
		{5510,	(0 << 1) | CLOCK_XTAL2},
		{5510,	(0 << 1) | CLOCK_XTAL2},
		{6620,	(7 << 1) | CLOCK_XTAL2},
		{8000,	(0 << 1) | CLOCK_XTAL1},
		{9600,	(7 << 1) | CLOCK_XTAL1},
		{11025,	(1 << 1) | CLOCK_XTAL2},
		{16000,	(1 << 1) | CLOCK_XTAL1},
		{18900,	(2 << 1) | CLOCK_XTAL2},
		{22050,	(3 << 1) | CLOCK_XTAL2},
		{27420,	(2 << 1) | CLOCK_XTAL1},
		{32000,	(3 << 1) | CLOCK_XTAL1},
		{33075,	(6 << 1) | CLOCK_XTAL2},
		{33075,	(4 << 1) | CLOCK_XTAL2},
		{44100,	(5 << 1) | CLOCK_XTAL2},
		{48000,	(6 << 1) | CLOCK_XTAL1},
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

	if (selected == -1)
		selected = 3;

	sc->sc_speed_bits = speed_table[selected].bits;
	sc->sc_need_commit = 1;
	*argp = speed_table[selected].speed;

	return (0);
}

/*
 * Audio interface functions
 */
int
ce4231_open(addr, flags)
	void *addr;
	int flags;
{
	struct ce4231_softc *sc = addr;
	int tries;

	DPRINTF(("ce4231_open\n"));

	if (sc->sc_open)
		return (EBUSY);

	sc->sc_open = 1;
	sc->sc_locked = 0;
	sc->sc_rintr = 0;
	sc->sc_rarg = 0;
	sc->sc_pintr = 0;
	sc->sc_parg = 0;

	P_WRITE(sc, EBDMA_DCSR, EBDCSR_RESET);
	C_WRITE(sc, EBDMA_DCSR, EBDCSR_RESET);
	P_WRITE(sc, EBDMA_DCSR, sc->sc_burst);
	C_WRITE(sc, EBDMA_DCSR, sc->sc_burst);

	DELAY(20);

	for (tries = CS_TIMEOUT;
	     tries && CS_READ(sc, AD1848_IADDR) == SP_IN_INIT; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout waiting for reset\n", sc->sc_dev.dv_xname);

	/* Turn on cs4231 mode */
	ce4231_write(sc, SP_MISC_INFO,
	    ce4231_read(sc, SP_MISC_INFO) | MODE2);

	ce4231_setup_output(sc);
	ce4231_setup_input(sc);

	ce4231_write(sc, SP_PIN_CONTROL,
	    ce4231_read(sc, SP_PIN_CONTROL) | INTERRUPT_ENABLE);

	return (0);
}

/* Set volume/attenuation values and mute settings for outputs */
void
ce4231_setup_output(sc)
	struct ce4231_softc *sc;
{
	u_int8_t pc, mi, rm = 0, lm = 0;


	/* DAC-output attenuation */
	DPRINTF(("ce4231_setup_output: DAC out = %d, %d\n",
	    sc->sc_volume[CSPORT_DAC].left,
	    sc->sc_volume[CSPORT_DAC].right));
	lm = (255 - sc->sc_volume[CSPORT_DAC].left) >> 2;
	rm = (255 - sc->sc_volume[CSPORT_DAC].right) >> 2;

	/* DAC out mute setting */
	if (!sc->sc_mute[CSPORT_DAC]) {
		DPRINTF(("ce4231_setup_output: DAC out muted\n"));
		lm = lm | OUTPUT_MUTE;
		rm = rm | OUTPUT_MUTE;
	}

	/* Commit DAC-out settings */
	ce4231_write(sc, SP_LEFT_OUTPUT_CONTROL, lm);
	ce4231_write(sc, SP_RIGHT_OUTPUT_CONTROL, rm);

	/* Mono DAC-out mute settings */
	mi = ce4231_read(sc, CS_MONO_IO_CONTROL) & ~MONO_OUTPUT_MUTE;
	if (!sc->sc_monoout_enable) {
		DPRINTF(("ce4231_setup_output: DAC mono output is enabled"));
		mi = mi | MONO_OUTPUT_MUTE;
	}

	/* Merge in mono input settings, as in same 8 bits */
	mi = mi | (ce4231_read(sc, CS_MONO_IO_CONTROL) & MONO_INPUT_MUTE);

	/* Commit mono dacout settings */
	ce4231_write(sc, CS_MONO_IO_CONTROL, mi);

	/* Line and headphone dacout mutes */
	pc = (ce4231_read(sc, SP_PIN_CONTROL) &
	    ~CS_PC_HDPHMUTE) & ~CS_PC_LINEMUTE;
	if (!sc->sc_lineout_enable) {
		DPRINTF(("ce4231_setup_output: DAC line output is enabled"));
		pc = pc | CS_PC_LINEMUTE;
	}
	if (!sc->sc_hdphout_enable) {
		DPRINTF(("ce4231_setup_output: DAC hdph output is enabled"));
		pc = pc | CS_PC_HDPHMUTE;
	}

	/* Commit line/headphone DAC-out settings */
	ce4231_write(sc, SP_PIN_CONTROL, pc);
}

/* Set volume/attenuation values and mute settings for outputs */
void
ce4231_setup_input(sc)
	struct ce4231_softc *sc;
{

	u_int8_t line_l, line_r, mono, aux2_l, aux2_r, monitor,
	    adc_l, adc_r;

	/* Line-in gain */
	DPRINTF(("ce4231_setup_input: line in gain = %d,%d\n",
	    sc->sc_volume[CSPORT_LINEIN].left,
	    sc->sc_volume[CSPORT_LINEIN].right));
	line_l = sc->sc_volume[CSPORT_LINEIN].left >> 3;
	line_r = sc->sc_volume[CSPORT_LINEIN].right >> 3;
	DPRINTF(("ce4231_setup_input: line-in gain (on card) = %d,%d\n",
	    line_l, line_r));

	/* Line-in mute */
	if (!sc->sc_mute[CSPORT_LINEIN]) {
		DPRINTF(("ce4231_setup_input: line-in mute is enabled\n"));
		line_l = line_l | CS_PC_LINEMUTE;
		line_r = line_r| CS_PC_LINEMUTE;
	}

	/* Commit line-in settings to card */
	ce4231_write(sc, CS_LEFT_LINE_CONTROL, line_l);
	ce4231_write(sc, CS_RIGHT_LINE_CONTROL, line_r);

	/* Mono-in attenuation */
	DPRINTF(("ce4231_setup_input: mono-in gain = %d\n",
	    sc->sc_volume[CSPORT_MONO].left));
	mono = (255 - sc->sc_volume[CSPORT_MONO].left) >> 4;
	DPRINTF(("ce4231_setup_input: mono-in attenuation (on card) = %d\n",
	    mono));

	/* Mono-in mute */
	if (!sc->sc_mute[CSPORT_MONO]) {
		DPRINTF(("ce4231_setup_input: mono-in mute is enabled\n"));
		mono = mono | MONO_INPUT_MUTE;
	}

	/* Merge in mono dacout setting, as in the same 8 bits */
	mono = mono | (ce4231_read(sc, CS_MONO_IO_CONTROL) & MONO_OUTPUT_MUTE);

	/* Commit mono-in settings */
	ce4231_write(sc, CS_MONO_IO_CONTROL, mono);

	/* CD/aux2 gain */
	DPRINTF(("ce4231_setup_input: cd/aux2 gain = %d,%d\n",
	    sc->sc_volume[CSPORT_AUX2].left,
	    sc->sc_volume[CSPORT_AUX2].right));
	aux2_l = sc->sc_volume[CSPORT_AUX2].left >> 3;
	aux2_r = sc->sc_volume[CSPORT_AUX2].right >> 3;
	DPRINTF(("ce4231_setup_input: cd/aux2 gain (on card) = %d,%d\n",
	    aux2_l, aux2_r));

	/* CD/aux2-input mute */
	if (!sc->sc_mute[CSPORT_AUX2]) {
		DPRINTF(("ce4231_setup_input: cd/aux2-in mute is enabled\n"));
		aux2_l = aux2_l | AUX_INPUT_MUTE;
		aux2_r = aux2_r | AUX_INPUT_MUTE;
	}

	/* Commit cd/aux2 settings */
	ce4231_write(sc, SP_LEFT_AUX2_CONTROL, aux2_l);
	ce4231_write(sc, SP_RIGHT_AUX2_CONTROL, aux2_r);

	/* Monitor attenuation */
	DPRINTF(("ce4231_setup_input: monitor gain = %d\n",
	    sc->sc_volume[CSPORT_MONITOR].left));
	monitor = (0xff - sc->sc_volume[CSPORT_MONITOR].left) & MIX_ATTEN_MASK;
	DPRINTF(("ce4231_setup_input: monitor attenuation (on card) = %d\n",
	    monitor >> 2));

	/* Monitor mute */
	if (!sc->sc_mute[CSPORT_MONITOR]) {
		monitor |= DIGITAL_MIX1_ENABLE;
	}

	/* Commit monitor settings */
	ce4231_write(sc, SP_DIGITAL_MIX, monitor);

	/* ADC-in gain */
	DPRINTF(("ce4231_setup_input: adc gain = %d,%d\n",
	    sc->sc_volume[CSPORT_ADC].left,
	    sc->sc_volume[CSPORT_ADC].right));
	adc_l = sc->sc_volume[CSPORT_ADC].left >> 4;
	adc_r = sc->sc_volume[CSPORT_ADC].right >> 4;
	DPRINTF(("ce4231_setup_input: adc gain (on card)  = %d,%d\n",
	    adc_l, adc_r));

	/* Record source is one of *_INPUT_PORT */
	adc_l = adc_l | (sc->sc_rec_src << 6);
	adc_r = adc_r | (sc->sc_rec_src << 6);

	ce4231_write(sc, SP_LEFT_INPUT_CONTROL, adc_l);
	ce4231_write(sc, SP_RIGHT_INPUT_CONTROL, adc_r);

	return;
}

void
ce4231_close(addr)
	void *addr;
{
	struct ce4231_softc *sc = addr;

	ce4231_halt_input(sc);
	ce4231_halt_output(sc);
	ce4231_write(sc, SP_PIN_CONTROL,
	    ce4231_read(sc, SP_PIN_CONTROL) & (~INTERRUPT_ENABLE));
	sc->sc_open = 0;
}

int
ce4231_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
	int err = 0;

	switch (fp->index) {
	case 0:
		strlcpy(fp->name, AudioEmulaw, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strlcpy(fp->name, AudioEalaw, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 2:
		strlcpy(fp->name, AudioEslinear_le, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 3:
		strlcpy(fp->name, AudioEulinear, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 4:
		strlcpy(fp->name, AudioEslinear_be, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strlcpy(fp->name, AudioEslinear, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strlcpy(fp->name, AudioEulinear_le, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strlcpy(fp->name, AudioEulinear_be, sizeof fp->name);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 8:
		strlcpy(fp->name, AudioEadpcm, sizeof fp->name);
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
ce4231_set_params(addr, setmode, usemode, p, r)
	void *addr;
	int setmode, usemode;
	struct audio_params *p, *r;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)addr;
	int err, bits, enc = p->encoding;
	void (*pswcode)(void *, u_char *, int cnt) = NULL;
	void (*rswcode)(void *, u_char *, int cnt) = NULL;

	if (p->precision > 16)
		p->precision = 16;
	switch (enc) {
	case AUDIO_ENCODING_ULAW:
		if (p->precision != 8)
			p->precision = 8;
		bits = FMT_ULAW >> 5;
		break;
	case AUDIO_ENCODING_ALAW:
		if (p->precision != 8)
			p->precision = 8;
		bits = FMT_ALAW >> 5;
		break;
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision == 8) {
			bits = FMT_PCM8 >> 5;
			pswcode = rswcode = change_sign8;
		} else
			bits = FMT_TWOS_COMP >> 5;
		break;
	case AUDIO_ENCODING_ULINEAR:
		if (p->precision != 8)
			p->precision = 8;
		bits = FMT_PCM8 >> 5;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (p->precision == 8) {
			bits = FMT_PCM8 >> 5;
			pswcode = rswcode = change_sign8;
		} else
			bits = FMT_TWOS_COMP_BE >> 5;
		break;
	case AUDIO_ENCODING_SLINEAR:
		if (p->precision != 8)
			p->precision = 8;
		bits = FMT_PCM8 >> 5;
		pswcode = rswcode = change_sign8;
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (p->precision == 8)
			bits = FMT_PCM8 >> 5;
		else {
			bits = FMT_TWOS_COMP >> 5;
			pswcode = rswcode = change_sign16_le;
		}
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		if (p->precision == 8)
			bits = FMT_PCM8 >> 5;
		else {
			bits = FMT_TWOS_COMP_BE >> 5;
			pswcode = rswcode = change_sign16_be;
		}
		break;
	case AUDIO_ENCODING_ADPCM:
		if (p->precision != 8)
			p->precision = 8;
		bits = FMT_ADPCM >> 5;
		break;
	default:
		return (EINVAL);
	}

	if (p->channels > 2)
		p->channels = 2;

	err = ce4231_set_speed(sc, &p->sample_rate);
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
ce4231_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	return ((blk + 3) & (-4));
}

int
ce4231_commit_settings(addr)
	void *addr;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)addr;
	int s, tries;
	u_int8_t r, fs;

	if (sc->sc_need_commit == 0)
		return (0);

	fs = sc->sc_speed_bits | (sc->sc_format_bits << 5);
	if (sc->sc_channels == 2)
		fs |= FMT_STEREO;

	if (sc->sc_last_format == fs) {
		sc->sc_need_commit = 0;
		return (0);
	}

	s = splaudio();

	r = ce4231_read(sc, SP_INTERFACE_CONFIG) | AUTO_CAL_ENABLE;
	CS_WRITE(sc, AD1848_IADDR, MODE_CHANGE_ENABLE);
	CS_WRITE(sc, AD1848_IADDR, MODE_CHANGE_ENABLE | SP_INTERFACE_CONFIG);
	CS_WRITE(sc, AD1848_IDATA, r);

	CS_WRITE(sc, AD1848_IADDR, MODE_CHANGE_ENABLE | SP_CLOCK_DATA_FORMAT);
	CS_WRITE(sc, AD1848_IDATA, fs);
	CS_READ(sc, AD1848_IDATA);
	CS_READ(sc, AD1848_IDATA);
	tries = CS_TIMEOUT;
	for (tries = CS_TIMEOUT;
	     tries && CS_READ(sc, AD1848_IADDR) == SP_IN_INIT; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout committing fspb\n", sc->sc_dev.dv_xname);

	CS_WRITE(sc, AD1848_IADDR, MODE_CHANGE_ENABLE | CS_REC_FORMAT);
	CS_WRITE(sc, AD1848_IDATA, fs);
	CS_READ(sc, AD1848_IDATA);
	CS_READ(sc, AD1848_IDATA);
	for (tries = CS_TIMEOUT;
	     tries && CS_READ(sc, AD1848_IADDR) == SP_IN_INIT; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout committing cdf\n", sc->sc_dev.dv_xname);

	CS_WRITE(sc, AD1848_IADDR, 0);
	for (tries = CS_TIMEOUT;
	     tries && CS_READ(sc, AD1848_IADDR) == SP_IN_INIT; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout waiting for !mce\n", sc->sc_dev.dv_xname);

	CS_WRITE(sc, AD1848_IADDR, SP_TEST_AND_INIT);
	for (tries = CS_TIMEOUT;
	     tries && CS_READ(sc, AD1848_IDATA) & AUTO_CAL_IN_PROG; tries--)
		DELAY(10);
	if (tries == 0)
		printf("%s: timeout waiting for autocalibration\n",
		    sc->sc_dev.dv_xname);

	splx(s);

	sc->sc_need_commit = 0;
	return (0);
}

int
ce4231_halt_output(addr)
	void *addr;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)addr;

	P_WRITE(sc, EBDMA_DCSR,
	    P_READ(sc, EBDMA_DCSR) & ~EBDCSR_DMAEN);
	ce4231_write(sc, SP_INTERFACE_CONFIG,
	    ce4231_read(sc, SP_INTERFACE_CONFIG) & (~PLAYBACK_ENABLE));
	sc->sc_locked = 0;
	return (0);
}

int
ce4231_halt_input(addr)
	void *addr;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)addr;

	C_WRITE(sc, EBDMA_DCSR,
	    C_READ(sc, EBDMA_DCSR) & ~EBDCSR_DMAEN);
	ce4231_write(sc, SP_INTERFACE_CONFIG,
	    ce4231_read(sc, SP_INTERFACE_CONFIG) & (~CAPTURE_ENABLE));
	sc->sc_locked = 0;
	return (0);
}

int
ce4231_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	*retp = ce4231_device;
	return (0);
}

int
ce4231_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)addr;
	int error = EINVAL;

	DPRINTF(("ce4231_set_port: port=%d type=%d\n", cp->dev, cp->type));

	/* XXX a lot of duplicated code here, sometime in the future
	 * make a function like:
	 * set_soft_volume(struct ce4231_softc *sc, mixer_ctrl_t *cp);
	 */

	switch (cp->dev) {
	case CSAUDIO_DAC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_volume[CSPORT_DAC].left =
				cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			sc->sc_volume[CSPORT_DAC].right =
				cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else if (cp->un.value.num_channels == 2) {
			sc->sc_volume[CSPORT_DAC].left =
				cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_volume[CSPORT_DAC].right =
				cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		} else
			break;
		ce4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_LINE_IN_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_volume[CSPORT_LINEIN].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			sc->sc_volume[CSPORT_LINEIN].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else if (cp->un.value.num_channels == 2) {
			sc->sc_volume[CSPORT_LINEIN].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_volume[CSPORT_LINEIN].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		} else
			break;
		ce4231_setup_input(sc);
		error = 0;
		break;
	case CSAUDIO_MIC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_volume[CSPORT_MONO].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			sc->sc_volume[CSPORT_MONO].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else if (cp->un.value.num_channels == 2) {
			sc->sc_volume[CSPORT_MONO].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_volume[CSPORT_MONO].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		} else
			break;
		ce4231_setup_input(sc);
		error = 0;
		break;
	case CSAUDIO_CD_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_volume[CSPORT_AUX2].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			sc->sc_volume[CSPORT_AUX2].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else if (cp->un.value.num_channels == 2) {
			sc->sc_volume[CSPORT_AUX2].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_volume[CSPORT_AUX2].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		} else
			break;
		ce4231_setup_input(sc);
		error = 0;
		break;
	case CSAUDIO_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE ||
		    cp->un.value.num_channels != 1)
		    	break;
		sc->sc_volume[CSPORT_MONITOR].left =
		    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		ce4231_setup_input(sc);
		error = 0;

		break;
	case CSAUDIO_LINE_IN_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_LINEIN] = cp->un.ord ? 1 : 0;
		ce4231_setup_input(sc);
		error = 0;
		break;
	case CSAUDIO_DAC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_DAC] = cp->un.ord ? 1 : 0;
		ce4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_CD_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_AUX2] = cp->un.ord ? 1 : 0;
		ce4231_setup_input(sc);
		error = 0;
		break;
	case CSAUDIO_MIC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_MONO] = cp->un.ord ? 1 : 0;
		ce4231_setup_input(sc);
		error = 0;
		break;
	case CSAUDIO_MONITOR_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_mute[CSPORT_MONITOR] = cp->un.ord ? 1 : 0;
		ce4231_setup_input(sc);
		error = 0;
		break;
	case CSAUDIO_DAC_MONO_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_monoout_enable = cp->un.ord ? 1 : 0;
		ce4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_DAC_LINE_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_lineout_enable = cp->un.ord ? 1 : 0;
		ce4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_DAC_HDPH_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_hdphout_enable = cp->un.ord ? 1 : 0;
		ce4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_REC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			sc->sc_volume[CSPORT_ADC].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			sc->sc_volume[CSPORT_ADC].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else if (cp->un.value.num_channels == 2) {
			sc->sc_volume[CSPORT_ADC].left =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			sc->sc_volume[CSPORT_ADC].right =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		} else
			break;
		ce4231_setup_input(sc);
		error = 0;

		break;

	case CSAUDIO_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		sc->sc_rec_src = cp->un.ord;
		ce4231_setup_input(sc);
		error = 0;
		break;
	}

	return (error);
}

int
ce4231_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)addr;
	int error = EINVAL;

	switch (cp->dev) {
	case CSAUDIO_DAC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    255 - ((ce4231_read(sc, SP_LEFT_OUTPUT_CONTROL) &
			    OUTPUT_ATTEN_BITS) << 2);
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    255 - ((ce4231_read(sc, SP_LEFT_OUTPUT_CONTROL) &
			    OUTPUT_ATTEN_BITS) << 2);
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    255 - ((ce4231_read(sc, SP_RIGHT_OUTPUT_CONTROL) &
			    OUTPUT_ATTEN_BITS) << 2);
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_LINE_IN_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    (ce4231_read(sc, CS_LEFT_LINE_CONTROL) &
			    LINE_INPUT_ATTEN_BITS) << 3;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    (ce4231_read(sc, CS_LEFT_LINE_CONTROL) &
			    LINE_INPUT_ATTEN_BITS) << 3;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    (ce4231_read(sc, CS_RIGHT_LINE_CONTROL) &
			    LINE_INPUT_ATTEN_BITS) << 3;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MIC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    (255 - (ce4231_read(sc, CS_MONO_IO_CONTROL) &
			    MONO_INPUT_ATTEN_BITS)) << 4;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_CD_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    (ce4231_read(sc, SP_LEFT_AUX2_CONTROL) &
			    LINE_INPUT_ATTEN_BITS) << 3;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    (ce4231_read(sc, SP_LEFT_AUX2_CONTROL) &
			    LINE_INPUT_ATTEN_BITS) << 3;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    (ce4231_read(sc, SP_RIGHT_AUX2_CONTROL) &
			    LINE_INPUT_ATTEN_BITS) << 3;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels != 1)
			break;
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
		    (0xff & MIX_ATTEN_MASK) -
		    (ce4231_read(sc, SP_DIGITAL_MIX) & MIX_ATTEN_MASK);
		error = 0;
		break;
	case CSAUDIO_LINE_IN_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = ce4231_read(sc, CS_LEFT_LINE_CONTROL) &
		    CS_PC_LINEMUTE ? 0 : 1;
		error = 0;
		break;
	case CSAUDIO_DAC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		cp->un.ord = ce4231_read(sc, SP_LEFT_OUTPUT_CONTROL) &
		    OUTPUT_MUTE ? 0 : 1;
		error = 0;
		break;
	case CSAUDIO_CD_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		//cp->un.ord = sc->sc_mute[CSPORT_AUX2] ? 1 : 0;
		cp->un.ord = ce4231_read(sc, SP_LEFT_AUX2_CONTROL) &
		    AUX_INPUT_MUTE ? 0 : 1;
		error = 0;
		break;
	case CSAUDIO_MIC_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		//cp->un.ord = sc->sc_mute[CSPORT_MONO] ? 1 : 0;
		cp->un.ord = ce4231_read(sc, CS_MONO_IO_CONTROL) &
		    MONO_INPUT_MUTE ? 0 : 1;
		error = 0;
		break;
	case CSAUDIO_MONITOR_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		//cp->un.ord = sc->sc_mute[CSPORT_MONITOR] ? 1 : 0;
		cp->un.ord = ce4231_read(sc, SP_DIGITAL_MIX) &
		    DIGITAL_MIX1_ENABLE ? 0 : 1;
		error = 0;
		break;
	case CSAUDIO_DAC_MONO_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		//cp->un.ord = sc->sc_monoout_enable ? 1 : 0;
		cp->un.ord = ce4231_read(sc, CS_MONO_IO_CONTROL) &
		    MONO_OUTPUT_MUTE ? 0 : 1;
		error = 0;
		break;
	case CSAUDIO_DAC_LINE_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		//cp->un.ord = sc->sc_lineout_enable ? 1 : 0;
		cp->un.ord = ce4231_read(sc, SP_PIN_CONTROL) & CS_PC_LINEMUTE
		    ? 0 : 1;
		error = 0;
		break;
	case CSAUDIO_DAC_HDPH_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;
		//cp->un.ord = sc->sc_hdphout_enable ? 1 : 0;
		cp->un.ord = ce4231_read(sc, SP_PIN_CONTROL) & CS_PC_HDPHMUTE
		    ? 0 : 1;
		error = 0;
		break;
	case CSAUDIO_REC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    ce4231_read(sc, SP_LEFT_INPUT_CONTROL) << 4;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    ce4231_read(sc, SP_LEFT_INPUT_CONTROL) << 4;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    ce4231_read(sc, SP_RIGHT_INPUT_CONTROL) << 4;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			break;

		/* AUX in disabled for now until we know what it does */
		if (cp->un.ord == AUX1_IN_PORT)
			break;

		cp->un.ord = ce4231_read(sc, SP_LEFT_INPUT_CONTROL &
		    CS_REC_SRC_BITS) >> 6;
		error = 0;
		break;
	}
	return (error);
}

int
ce4231_query_devinfo(addr, dip)
	void *addr;
	mixer_devinfo_t *dip;
{
	int err = 0;

	switch (dip->index) {
	case CSAUDIO_MIC_LVL:		/* Mono/microphone mixer */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_MIC_MUTE;
		strlcpy(dip->label.name, AudioNmicrophone,
		    sizeof dip->label.name);
		dip->un.v.num_channels = 1;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_DAC_LVL:		/* DAC out */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_DAC_MUTE;
		strlcpy(dip->label.name, AudioNdac, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_LINE_IN_LVL:	/* Line */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_LINE_IN_MUTE;
		strlcpy(dip->label.name, AudioNline, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_CD_LVL:		/* CD */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_CD_MUTE;
		strlcpy(dip->label.name, AudioNcd, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_LINE_IN_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = CSAUDIO_LINE_IN_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_DAC_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = CSAUDIO_DAC_LVL;
		dip->next = CSAUDIO_DAC_MONO_MUTE;
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
	case CSAUDIO_MONITOR_LVL:	/* Monitor level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_MONITOR_MUTE;
		strlcpy(dip->label.name, AudioNmonitor, sizeof dip->label.name);
		dip->un.v.num_channels = 1;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_MONITOR_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = CSAUDIO_MONITOR_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	case CSAUDIO_DAC_MONO_MUTE: /* The DAC has a mono out, usually spkr */
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = CSAUDIO_DAC_MUTE;
		dip->next = CSAUDIO_DAC_LINE_MUTE;
		/* Custom name, as we already have a mute in this class */
		strlcpy(dip->label.name, "monomute", sizeof "monomute");
		goto mute1;
	case CSAUDIO_DAC_LINE_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = CSAUDIO_DAC_MONO_MUTE;
		dip->next = CSAUDIO_DAC_HDPH_MUTE;
		/* Custom name */
		strlcpy(dip->label.name, "linemute", sizeof "linemute");
		goto mute1;
	case CSAUDIO_DAC_HDPH_MUTE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = CSAUDIO_DAC_LINE_MUTE;
		dip->next = CSAUDIO_MONITOR_LVL;
		/* Custom name */
		strlcpy(dip->label.name, "hdphmute", sizeof "hdphmute");
		goto mute1;
	mute:
		strlcpy(dip->label.name, AudioNmute, sizeof dip->label.name);
	mute1:
		dip->un.e.num_mem = 2;
		strlcpy(dip->un.e.member[0].label.name, AudioNon,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = 0;
		strlcpy(dip->un.e.member[1].label.name, AudioNoff,
		    sizeof dip->un.e.member[1].label.name);
		dip->un.e.member[1].ord = 1;
		break;
	case CSAUDIO_REC_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CSAUDIO_RECORD_SOURCE;
		strlcpy(dip->label.name, AudioNrecord, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;
	case CSAUDIO_RECORD_SOURCE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = CSAUDIO_REC_LVL;
		dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioNsource, sizeof dip->label.name);
		dip->un.e.num_mem = 4;

		strlcpy(dip->un.e.member[0].label.name, AudioNline,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = LINE_IN_PORT;

		strlcpy(dip->un.e.member[1].label.name, "aux",
		    sizeof dip->un.e.member[1].label.name);
		dip->un.e.member[1].ord = AUX1_IN_PORT;

		strlcpy(dip->un.e.member[2].label.name, AudioNmicrophone,
		    sizeof dip->un.e.member[2].label.name);
		dip->un.e.member[2].ord = MIC_IN_PORT;

		strlcpy(dip->un.e.member[3].label.name, AudioNdac,
		    sizeof dip->un.e.member[3].label.name);
		dip->un.e.member[3].ord = DAC_IN_PORT;

		break;
	case CSAUDIO_INPUT_CLASS:	/* Input class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioCinputs, sizeof dip->label.name);
		break;
	case CSAUDIO_OUTPUT_CLASS:	/* Output class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioCoutputs, sizeof dip->label.name);
		break;
	case CSAUDIO_RECORD_CLASS:	/* Record class descriptor */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CSAUDIO_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strlcpy(dip->label.name, AudioCrecord, sizeof dip->label.name);
		break;
	default:
		err = ENXIO;
	}

	return (err);
}

int
ce4231_get_props(addr)
	void *addr;
{
	return (AUDIO_PROP_FULLDUPLEX);
}

/*
 * Hardware interrupt handler
 */
int
ce4231_cintr(v)
	void *v;
{
	return (0);
}

int
ce4231_pintr(v)
	void *v;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)v;
	u_int32_t csr;
	u_int8_t reg, status;
	struct cs_dma *p;
	int r = 0;

	csr = P_READ(sc, EBDMA_DCSR);
	status = CS_READ(sc, AD1848_STATUS);
	if (status & (INTERRUPT_STATUS | SAMPLE_ERROR)) {
		reg = ce4231_read(sc, CS_IRQ_STATUS);
		if (reg & CS_AFS_PI) {
			ce4231_write(sc, SP_LOWER_BASE_COUNT, 0xff);
			ce4231_write(sc, SP_UPPER_BASE_COUNT, 0xff);
		}
		CS_WRITE(sc, AD1848_STATUS, 0);
	}

	P_WRITE(sc, EBDMA_DCSR, csr);

	if (csr & EBDCSR_INT)
		r = 1;

	if ((csr & EBDCSR_TC) || ((csr & EBDCSR_A_LOADED) == 0)) {
		u_long nextaddr, togo;

		p = sc->sc_nowplaying;
		togo = sc->sc_playsegsz - sc->sc_playcnt;
		if (togo == 0) {
			nextaddr = (u_int32_t)p->dmamap->dm_segs[0].ds_addr;
			sc->sc_playcnt = togo = sc->sc_blksz;
		} else {
			nextaddr = sc->sc_lastaddr;
			if (togo > sc->sc_blksz)
				togo = sc->sc_blksz;
			sc->sc_playcnt += togo;
		}

		P_WRITE(sc, EBDMA_DCNT, togo);
		P_WRITE(sc, EBDMA_DADDR, nextaddr);
		sc->sc_lastaddr = nextaddr + togo;

		if (sc->sc_pintr != NULL)
			(*sc->sc_pintr)(sc->sc_parg);
		r = 1;
	}

	return (r);
}

void *
ce4231_alloc(addr, direction, size, pool, flags)
	void *addr;
	int direction;
	size_t size;
	int pool;
	int flags;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)addr;
	bus_dma_tag_t dmat = sc->sc_dmatag;
	struct cs_dma *p;

	p = (struct cs_dma *)malloc(sizeof(struct cs_dma), pool, flags);
	if (p == NULL)
		return (NULL);

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
	free(p, pool);
	return (NULL);
}

void
ce4231_free(addr, ptr, pool)
	void *addr;
	void *ptr;
	int pool;
{
	struct ce4231_softc *sc = addr;
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
ce4231_trigger_output(addr, start, end, blksize, intr, arg, param)
	void *addr, *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	struct ce4231_softc *sc = addr;
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
		printf("%s: trigger_output: bad addr: %p\n",
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

	csr = P_READ(sc, EBDMA_DCSR);
	if (csr & EBDCSR_DMAEN) {
		P_WRITE(sc, EBDMA_DCNT, (u_long)n);
		P_WRITE(sc, EBDMA_DADDR,
		    (u_long)p->dmamap->dm_segs[0].ds_addr);
	} else {
		P_WRITE(sc, EBDMA_DCSR, EBDCSR_RESET);
		P_WRITE(sc, EBDMA_DCSR, sc->sc_burst);

		P_WRITE(sc, EBDMA_DCNT, (u_long)n);
		P_WRITE(sc, EBDMA_DADDR,
		    (u_long)p->dmamap->dm_segs[0].ds_addr);

		P_WRITE(sc, EBDMA_DCSR, sc->sc_burst | EBDCSR_DMAEN |
		    EBDCSR_INTEN | EBDCSR_CNTEN | EBDCSR_NEXTEN);

		ce4231_write(sc, SP_LOWER_BASE_COUNT, 0xff);
		ce4231_write(sc, SP_UPPER_BASE_COUNT, 0xff);
		ce4231_write(sc, SP_INTERFACE_CONFIG,
		    ce4231_read(sc, SP_INTERFACE_CONFIG) | PLAYBACK_ENABLE);
	}
	sc->sc_lastaddr = p->dmamap->dm_segs[0].ds_addr + n;

	return (0);
}

int
ce4231_trigger_input(addr, start, end, blksize, intr, arg, param)
	void *addr, *start, *end;
	int blksize;
	void (*intr)(void *);
	void *arg;
	struct audio_params *param;
{
	return (ENXIO);
}
