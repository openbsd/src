/*	$OpenBSD: ce4231.c,v 1.10 2002/08/16 19:02:17 jason Exp $	*/

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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
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

#include <dev/ic/ad1848reg.h>
#include <dev/ic/cs4231reg.h>

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

#define	CS_TIMEOUT	90000

#define	CS_PC_LINEMUTE	XCTL0_ENABLE
#define	CS_PC_HDPHMUTE	XCTL1_ENABLE
#define	CS_AFS_PI	0x10

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
size_t	ce4231_round_buffersize(void *, int, size_t);
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
	ce4231_round_buffersize,
	0,
	ce4231_get_props,
	ce4231_trigger_output,
	ce4231_trigger_input
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
	sc->sc_bustag = ea->ea_bustag;
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

	sc->sc_cih = bus_intr_establish(ea->ea_bustag, ea->ea_intrs[0],
	    IPL_AUDIO, 0, ce4231_cintr, sc);
	if (sc->sc_cih == NULL) {
		printf(": couldn't establish capture interrupt\n");
		return;
	}
	sc->sc_pih = bus_intr_establish(ea->ea_bustag, ea->ea_intrs[1],
	    IPL_AUDIO, 0, ce4231_pintr, sc);
	if (sc->sc_pih == NULL) {
		printf(": couldn't establish play interrupt1\n");
		return;
	}

	/* XXX what if prom has already mapped?! */

	if (ebus_bus_map(ea->ea_bustag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[0]), ea->ea_regs[0].size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_cshandle) != 0) {
		printf(": couldn't map cs4231 registers\n");
		return;
	}

	if (ebus_bus_map(ea->ea_bustag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[1]), ea->ea_regs[1].size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_pdmahandle) != 0) {
		printf(": couldn't map dma1 registers\n");
		return;
	}

	if (ebus_bus_map(ea->ea_bustag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[2]), ea->ea_regs[2].size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_cdmahandle) != 0) {
		printf(": couldn't map dma2 registers\n");
		return;
	}

	if (ebus_bus_map(ea->ea_bustag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[3]), ea->ea_regs[3].size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_auxhandle) != 0) {
		printf(": couldn't map aux registers\n");
		return;
	}

	printf(": nvaddrs %d\n", ea->ea_nvaddrs);

	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);

	audio_attach_mi(&ce4231_sa_hw_if, sc, &sc->sc_dev);

	/* Default to speaker, unmuted, reasonable volume */
	sc->sc_out_port = CSPORT_SPEAKER;
	sc->sc_mute[CSPORT_SPEAKER] = 1;
	sc->sc_mute[CSPORT_MONITOR] = 1;
	sc->sc_volume[CSPORT_SPEAKER].left = 192;
	sc->sc_volume[CSPORT_SPEAKER].right = 192;

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

	if (selected == -1) {
		printf("%s: can't find speed\n", sc->sc_dev.dv_xname);
		selected = 3;
	}

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

	ce4231_write(sc, SP_PIN_CONTROL,
	    ce4231_read(sc, SP_PIN_CONTROL) | INTERRUPT_ENABLE);

	return (0);
}

void
ce4231_setup_output(sc)
	struct ce4231_softc *sc;
{
	u_int8_t pc, mi, rm, lm;

	pc = ce4231_read(sc, SP_PIN_CONTROL) | CS_PC_HDPHMUTE | CS_PC_LINEMUTE;

	mi = ce4231_read(sc, CS_MONO_IO_CONTROL) | MONO_OUTPUT_MUTE;

	lm = ce4231_read(sc, SP_LEFT_OUTPUT_CONTROL);
	lm &= ~OUTPUT_ATTEN_BITS;
	lm |= ((~(sc->sc_volume[CSPORT_SPEAKER].left >> 2)) &
	    OUTPUT_ATTEN_BITS) | OUTPUT_MUTE;

	rm = ce4231_read(sc, SP_RIGHT_OUTPUT_CONTROL);
	rm &= ~OUTPUT_ATTEN_BITS;
	rm |= ((~(sc->sc_volume[CSPORT_SPEAKER].right >> 2)) &
	    OUTPUT_ATTEN_BITS) | OUTPUT_MUTE;

	if (sc->sc_mute[CSPORT_MONITOR]) {
		lm &= ~OUTPUT_MUTE;
		rm &= ~OUTPUT_MUTE;
	}

	switch (sc->sc_out_port) {
	case CSPORT_HEADPHONE:
		if (sc->sc_mute[CSPORT_SPEAKER])
			pc &= ~CS_PC_HDPHMUTE;
		break;
	case CSPORT_SPEAKER:
		if (sc->sc_mute[CSPORT_SPEAKER])
			mi &= ~MONO_OUTPUT_MUTE;
		break;
	case CSPORT_LINEOUT:
		if (sc->sc_mute[CSPORT_SPEAKER])
			pc &= ~CS_PC_LINEMUTE;
		break;
	}

	ce4231_write(sc, SP_LEFT_OUTPUT_CONTROL, lm);
	ce4231_write(sc, SP_RIGHT_OUTPUT_CONTROL, rm);
	ce4231_write(sc, SP_PIN_CONTROL, pc);
	ce4231_write(sc, CS_MONO_IO_CONTROL, mi);
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
ce4231_set_params(addr, setmode, usemode, p, r)
	void *addr;
	int setmode, usemode;
	struct audio_params *p, *r;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)addr;
	int err, bits, enc;
	void (*pswcode)(void *, u_char *, int cnt);
	void (*rswcode)(void *, u_char *, int cnt);

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
		bits = FMT_ULAW >> 5;
		break;
	case AUDIO_ENCODING_ALAW:
		bits = FMT_ALAW >> 5;
		break;
	case AUDIO_ENCODING_ADPCM:
		bits = FMT_ADPCM >> 5;
		break;
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision == 16)
			bits = FMT_TWOS_COMP >> 5;
		else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (p->precision == 16)
			bits = FMT_TWOS_COMP_BE >> 5;
		else
			return (EINVAL);
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (p->precision == 8)
			bits = FMT_PCM8 >> 5;
		else
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	if (p->channels != 1 && p->channels != 2)
		return (EINVAL);

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
	return (blk & (-4));
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

	switch (cp->dev) {
	case CSAUDIO_DAC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			ce4231_write(sc, SP_LEFT_AUX1_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    LINE_INPUT_ATTEN_BITS);
		else if (cp->un.value.num_channels == 2) {
			ce4231_write(sc, SP_LEFT_AUX1_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] &
			    LINE_INPUT_ATTEN_BITS);
			ce4231_write(sc, SP_RIGHT_AUX1_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] &
			    LINE_INPUT_ATTEN_BITS);
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_LINE_IN_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			ce4231_write(sc, CS_LEFT_LINE_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    AUX_INPUT_ATTEN_BITS);
		else if (cp->un.value.num_channels == 2) {
			ce4231_write(sc, CS_LEFT_LINE_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] &
			    AUX_INPUT_ATTEN_BITS);
			ce4231_write(sc, CS_RIGHT_LINE_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] &
			    AUX_INPUT_ATTEN_BITS);
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MIC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
#if 0
			ce4231_write(sc, CS_MONO_IO_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    MONO_INPUT_ATTEN_BITS);
#endif
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_CD_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
			ce4231_write(sc, SP_LEFT_AUX2_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] &
			    LINE_INPUT_ATTEN_BITS);
		} else if (cp->un.value.num_channels == 2) {
			ce4231_write(sc, SP_LEFT_AUX2_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] &
			    LINE_INPUT_ATTEN_BITS);
			ce4231_write(sc, SP_RIGHT_AUX2_CONTROL,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] &
			    LINE_INPUT_ATTEN_BITS);
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MONITOR_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			ce4231_write(sc, SP_DIGITAL_MIX,
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

		ce4231_setup_output(sc);
		error = 0;
		break;
	case CSAUDIO_OUTPUT:
		if (cp->un.ord != CSPORT_LINEOUT &&
		    cp->un.ord != CSPORT_SPEAKER &&
		    cp->un.ord != CSPORT_HEADPHONE)
			return (EINVAL);
		sc->sc_out_port = cp->un.ord;
		ce4231_setup_output(sc);
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
		ce4231_setup_output(sc);
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
ce4231_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct ce4231_softc *sc = (struct ce4231_softc *)addr;
	int error = EINVAL;

	DPRINTF(("ce4231_get_port: port=%d type=%d\n", cp->dev, cp->type));

	switch (cp->dev) {
	case CSAUDIO_DAC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]=
			    ce4231_read(sc, SP_LEFT_AUX1_CONTROL) &
			    LINE_INPUT_ATTEN_BITS;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    ce4231_read(sc, SP_LEFT_AUX1_CONTROL) &
			    LINE_INPUT_ATTEN_BITS;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    ce4231_read(sc, SP_RIGHT_AUX1_CONTROL) &
			    LINE_INPUT_ATTEN_BITS;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_LINE_IN_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1)
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    ce4231_read(sc, CS_LEFT_LINE_CONTROL) & AUX_INPUT_ATTEN_BITS;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    ce4231_read(sc, CS_LEFT_LINE_CONTROL) & AUX_INPUT_ATTEN_BITS;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    ce4231_read(sc, CS_RIGHT_LINE_CONTROL) & AUX_INPUT_ATTEN_BITS;
		} else
			break;
		error = 0;
		break;
	case CSAUDIO_MIC_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;
		if (cp->un.value.num_channels == 1) {
#if 0
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			    ce4231_read(sc, CS_MONO_IO_CONTROL) &
			    MONO_INPUT_ATTEN_BITS;
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
			    ce4231_read(sc, SP_LEFT_AUX2_CONTROL) &
			    LINE_INPUT_ATTEN_BITS;
		else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
			    ce4231_read(sc, SP_LEFT_AUX2_CONTROL) &
			    LINE_INPUT_ATTEN_BITS;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
			    ce4231_read(sc, SP_RIGHT_AUX2_CONTROL) &
			    LINE_INPUT_ATTEN_BITS;
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
		    ce4231_read(sc, SP_DIGITAL_MIX) >> 2;
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

size_t
ce4231_round_buffersize(addr, direction, size)
	void *addr;
	int direction;
	size_t size;
{
	return (size);
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

#endif /* NAUDIO > 0 */
