/*	$OpenBSD: opl.c,v 1.1 1999/01/02 00:02:40 niklas Exp $	*/
/*	$NetBSD: opl.c,v 1.7 1998/12/08 14:26:56 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * The OPL3 (YMF262) manual can be found at
 * ftp://ftp.yamahayst.com/pub/Fax_Back_Doc/Sound/YMF262.PDF
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/select.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <sys/audioio.h>
#include <sys/midiio.h>
#include <dev/audio_if.h>

#include <dev/midi_if.h>
#include <dev/midivar.h>
#include <dev/midisynvar.h>

#include <dev/ic/oplreg.h>
#include <dev/ic/oplvar.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (opldebug) printf x
#define DPRINTFN(n,x)	if (opldebug >= (n)) printf x
int	opldebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct real_voice {
	u_int8_t voice_num;
	u_int8_t voice_mode; /* 0=unavailable, 2=2 OP, 4=4 OP */
	u_int8_t iooffs; /* I/O port (left or right side) */
	u_int8_t op[4]; /* Operator offsets */
};

struct opl_voice voicetab[] = {
/*       No    I/O offs		OP1	OP2	OP3   OP4	*/
/*	---------------------------------------------------	*/
	{ 0,   OPL_L,	{0x00,	0x03,	0x08, 0x0b}},
	{ 1,   OPL_L,	{0x01,	0x04,	0x09, 0x0c}},
	{ 2,   OPL_L,	{0x02,	0x05,	0x0a, 0x0d}},

	{ 3,   OPL_L,	{0x08,	0x0b,	0x00, 0x00}},
	{ 4,   OPL_L,	{0x09,	0x0c,	0x00, 0x00}},
	{ 5,   OPL_L,	{0x0a,	0x0d,	0x00, 0x00}},

	{ 6,   OPL_L,	{0x10,	0x13,	0x00, 0x00}},
	{ 7,   OPL_L,	{0x11,	0x14,	0x00, 0x00}},
	{ 8,   OPL_L,	{0x12,	0x15,	0x00, 0x00}},

	{ 0,   OPL_R,	{0x00,	0x03,	0x08, 0x0b}},
	{ 1,   OPL_R,	{0x01,	0x04,	0x09, 0x0c}},
	{ 2,   OPL_R,	{0x02,	0x05,	0x0a, 0x0d}},
	{ 3,   OPL_R,	{0x08,	0x0b,	0x00, 0x00}},
	{ 4,   OPL_R,	{0x09,	0x0c,	0x00, 0x00}},
	{ 5,   OPL_R,	{0x0a,	0x0d,	0x00, 0x00}},

	{ 6,   OPL_R,	{0x10,	0x13,	0x00, 0x00}},
	{ 7,   OPL_R,	{0x11,	0x14,	0x00, 0x00}},
	{ 8,   OPL_R,	{0x12,	0x15,	0x00, 0x00}}
};

static void opl_command(struct opl_softc *, int, int, int);
void opl_reset(struct opl_softc *);
void opl_freq_to_fnum (int freq, int *block, int *fnum);

int oplsyn_open __P((midisyn *ms, int));
void oplsyn_close __P((midisyn *));
void oplsyn_reset __P((void *));
void oplsyn_noteon __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
void oplsyn_noteoff __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
void oplsyn_keypressure __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
void oplsyn_ctlchange __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
void oplsyn_pitchbend __P((midisyn *, u_int32_t, u_int32_t, u_int32_t));
void oplsyn_loadpatch __P((midisyn *, struct sysex_info *, struct uio *));


void opl_set_op_reg __P((struct opl_softc *, int, int, int, u_char));
void opl_set_ch_reg __P((struct opl_softc *, int, int, u_char));
void opl_load_patch __P((struct opl_softc *, int));
u_int32_t opl_get_block_fnum __P((int freq));
int opl_calc_vol __P((int regbyte, int volume, int main_vol));

struct cfdriver opl_cd = {
	NULL, "opl", DV_DULL
};

struct midisyn_methods opl3_midi = {
	oplsyn_open,
	oplsyn_close,
	0,
	0,
	oplsyn_noteon,
	oplsyn_noteoff,
	oplsyn_keypressure,
	oplsyn_ctlchange,
	0,
	0,
	oplsyn_pitchbend,
	0
};
	
void
opl_attach(sc)
	struct opl_softc *sc;
{
	int i;

	if (!opl_find(sc)) {
		printf("\nopl: find failed\n");
		return;
	}

	sc->syn.mets = &opl3_midi;
	sprintf(sc->syn.name, "%sYamaha OPL%d", sc->syn.name, sc->model);
	sc->syn.data = sc;
	sc->syn.nvoice = sc->model == OPL_2 ? OPL2_NVOICE : OPL3_NVOICE;
	sc->syn.flags =  MS_DOALLOC | MS_FREQXLATE;
	midisyn_attach(&sc->mididev, &sc->syn);
	
	/* Set up voice table */
	for (i = 0; i < OPL3_NVOICE; i++)
		sc->voices[i] = voicetab[i];

	opl_reset(sc);

	printf(": model OPL%d\n", sc->model);

	midi_attach_mi(&midisyn_hw_if, &sc->syn, &sc->mididev.dev);
}

static void
opl_command(sc, offs, addr, data)
	struct opl_softc *sc;
	int offs;
	int addr, data;
{
	DPRINTFN(4, ("opl_command: sc=%p, offs=%d addr=0x%02x data=0x%02x\n", 
		     sc, offs, addr, data));
	offs += sc->offs;
	bus_space_write_1(sc->iot, sc->ioh, OPL_ADDR+offs, addr);
	if (sc->model == OPL_2)
		delay(10);
	else
		delay(6);
	bus_space_write_1(sc->iot, sc->ioh, OPL_DATA+offs, data);
	if (sc->model == OPL_2)
		delay(30);
	else
		delay(6);
}

int
opl_find(sc)
	struct opl_softc *sc;
{
	u_int8_t status1, status2;

	DPRINTFN(2,("opl_find: ioh=0x%x\n", (int)sc->ioh));
	sc->model = OPL_2;	/* worst case assumtion */

	/* Reset timers 1 and 2 */
	opl_command(sc, OPL_L, OPL_TIMER_CONTROL, 
		    OPL_TIMER1_MASK | OPL_TIMER2_MASK);
	/* Reset the IRQ of the FM chip */
	opl_command(sc, OPL_L, OPL_TIMER_CONTROL, OPL_IRQ_RESET);

	/* get status bits */
	status1 = bus_space_read_1(sc->iot,sc->ioh,OPL_STATUS+OPL_L+sc->offs);

	opl_command(sc, OPL_L, OPL_TIMER1, -2); /* wait 2 ticks */
	opl_command(sc, OPL_L, OPL_TIMER_CONTROL, /* start timer1 */
		    OPL_TIMER1_START | OPL_TIMER2_MASK);
	delay(1000);		/* wait for timer to expire */

	/* get status bits again */
	status2 = bus_space_read_1(sc->iot,sc->ioh,OPL_STATUS+OPL_L+sc->offs);

	opl_command(sc, OPL_L, OPL_TIMER_CONTROL, 
		    OPL_TIMER1_MASK | OPL_TIMER2_MASK);
	opl_command(sc, OPL_L, OPL_TIMER_CONTROL, OPL_IRQ_RESET);

	DPRINTFN(2,("opl_find: %02x %02x\n", status1, status2));

	if ((status1 & OPL_STATUS_MASK) != 0 ||
	    (status2 & OPL_STATUS_MASK) != (OPL_STATUS_IRQ | OPL_STATUS_FT1))
		return (0);

	switch(status1) {
	case 0x00:
	case 0x0f:
		sc->model = OPL_3;
		break;
	case 0x06:
		sc->model = OPL_2;
		break;
	default:
		return 0;
	}

	DPRINTFN(2,("opl_find: OPL%d at 0x%x detected\n", 
		    sc->model, (int)sc->ioh));
	return (1);
}

void 
opl_set_op_reg(sc, base, voice, op, value)
	struct opl_softc *sc;
	int base;
	int voice;
	int op;
	u_char value;
{
	struct opl_voice *v = &sc->voices[voice];
	opl_command(sc, v->iooffs, base + v->op[op], value);
}

void 
opl_set_ch_reg(sc, base, voice, value)
	struct opl_softc *sc;
	int base;
	int voice;
	u_char value;
{
	struct opl_voice *v = &sc->voices[voice];
	opl_command(sc, v->iooffs, base + v->voiceno, value);
}


void 
opl_load_patch(sc, v)
	struct opl_softc *sc;
	int v;
{
	struct opl_operators *p = sc->voices[v].patch;

	opl_set_op_reg(sc, OPL_AM_VIB,          v, 0, p->ops[OO_CHARS+0]);
	opl_set_op_reg(sc, OPL_AM_VIB,          v, 1, p->ops[OO_CHARS+1]);
	opl_set_op_reg(sc, OPL_KSL_LEVEL,       v, 0, p->ops[OO_KSL_LEV+0]);
	opl_set_op_reg(sc, OPL_KSL_LEVEL,       v, 1, p->ops[OO_KSL_LEV+1]);
	opl_set_op_reg(sc, OPL_ATTACK_DECAY,    v, 0, p->ops[OO_ATT_DEC+0]);
	opl_set_op_reg(sc, OPL_ATTACK_DECAY,    v, 1, p->ops[OO_ATT_DEC+1]);
	opl_set_op_reg(sc, OPL_SUSTAIN_RELEASE, v, 0, p->ops[OO_SUS_REL+0]);
	opl_set_op_reg(sc, OPL_SUSTAIN_RELEASE, v, 1, p->ops[OO_SUS_REL+1]);
	opl_set_op_reg(sc, OPL_WAVE_SELECT,     v, 0, p->ops[OO_WAV_SEL+0]);
	opl_set_op_reg(sc, OPL_WAVE_SELECT,     v, 1, p->ops[OO_WAV_SEL+1]);
	opl_set_ch_reg(sc, OPL_FEEDBACK_CONNECTION, v, p->ops[OO_FB_CONN]);
}

#define OPL_FNUM_FAIL 0xffff
u_int32_t
opl_get_block_fnum(freq)
	int freq;
{
	u_int32_t f_num = freq / 3125;
	u_int32_t  block = 0;

	while (f_num > 0x3FF && block < 8) {
		block++;
		f_num >>= 1;
	}

	if (block > 7)
		return (OPL_FNUM_FAIL);
	else
		return ((block << 10) | f_num);
  }


void
opl_reset(sc)
	struct opl_softc *sc;
{
	int i;

	for (i = 1; i <= OPL_MAXREG; i++)
		opl_command(sc, OPL_L, OPL_KEYON_BLOCK + i, 0);

	opl_command(sc, OPL_L, OPL_TEST, OPL_ENABLE_WAVE_SELECT);
	opl_command(sc, OPL_L, OPL_PERCUSSION, 0);
	if (sc->model == OPL_3) {
		opl_command(sc, OPL_R, OPL_MODE, OPL3_ENABLE);
		opl_command(sc, OPL_R,OPL_CONNECTION_SELECT,OPL_NOCONNECTION);
	}

	sc->volume = 64;
}

int
oplsyn_open(ms, flags)
	midisyn *ms;
	int flags;
{
	struct opl_softc *sc = ms->data;

	DPRINTFN(2, ("oplsyn_open: %d\n", flags));

	opl_reset(ms->data);
	if (sc->spkrctl)
		sc->spkrctl(sc->spkrarg, 1);
	return (0);
}

void
oplsyn_close(ms)
	midisyn *ms;
{
	struct opl_softc *sc = ms->data;

	DPRINTFN(2, ("oplsyn_close:\n"));

	/*opl_reset(ms->data);*/
	if (sc->spkrctl)
		sc->spkrctl(sc->spkrarg, 0);
}

#if 0
void
oplsyn_getinfo(addr, sd)
	void *addr;
	struct synth_dev *sd;
{
	struct opl_softc *sc = addr;

	sd->name = sc->model == OPL_2 ? "Yamaha OPL2" : "Yamaha OPL3";
	sd->type = SYNTH_TYPE_FM;
	sd->subtype = sc->model == OPL_2 ? SYNTH_SUB_FM_TYPE_ADLIB 
		: SYNTH_SUB_FM_TYPE_OPL3;
	sd->capabilities = 0;
}
#endif

void
oplsyn_reset(addr)
	void *addr;
{
	struct opl_softc *sc = addr;
	DPRINTFN(3, ("oplsyn_reset:\n"));
	opl_reset(sc);
}

int8_t opl_volume_table[128] =
    {-64, -48, -40, -35, -32, -29, -27, -26,
     -24, -23, -21, -20, -19, -18, -18, -17,
     -16, -15, -15, -14, -13, -13, -12, -12,
     -11, -11, -10, -10, -10, -9, -9, -8,
     -8, -8, -7, -7, -7, -6, -6, -6,
     -5, -5, -5, -5, -4, -4, -4, -4,
     -3, -3, -3, -3, -2, -2, -2, -2,
     -2, -1, -1, -1, -1, 0, 0, 0,
     0, 0, 0, 1, 1, 1, 1, 1,
     1, 2, 2, 2, 2, 2, 2, 2,
     3, 3, 3, 3, 3, 3, 3, 4,
     4, 4, 4, 4, 4, 4, 4, 5,
     5, 5, 5, 5, 5, 5, 5, 5,
     6, 6, 6, 6, 6, 6, 6, 6,
     6, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 8, 8, 8, 8, 8};

int
opl_calc_vol(regbyte, volume, mainvol)
	int regbyte; 
	int volume;
	int mainvol;
{
	int level = ~regbyte & OPL_TOTAL_LEVEL_MASK;

	if (mainvol > 127)
		mainvol = 127;

	volume = (volume * mainvol) / 127;

	if (level)
		level += opl_volume_table[volume];

	if (level > OPL_TOTAL_LEVEL_MASK)
		level = OPL_TOTAL_LEVEL_MASK;
	if (level < 0)
		level = 0;

	return (~level & OPL_TOTAL_LEVEL_MASK);
}

void
oplsyn_noteon(ms, voice, freq, vel)
	midisyn *ms;
	u_int32_t voice, freq, vel;
{
	struct opl_softc *sc = ms->data;
	struct opl_voice *v;
	struct opl_operators *p;
	u_int32_t block_fnum;
	int mult;
	int c_mult, m_mult;
	u_int8_t chars0, chars1, ksl0, ksl1, fbc;
	u_int8_t r20m, r20c, r40m, r40c, rA0, rB0;
	u_int8_t vol0, vol1;

	DPRINTFN(3, ("oplsyn_noteon: %p %d %d\n", sc, voice, 
		     MIDISYN_FREQ_TO_HZ(freq)));

#ifdef DIAGNOSTIC
	if (voice < 0 || voice >= sc->syn.nvoice) {
		printf("oplsyn_noteon: bad voice %d\n", voice);
		return;
	}
#endif
	/* Turn off old note */
	opl_set_op_reg(sc, OPL_KSL_LEVEL,   voice, 0, 0xff);
	opl_set_op_reg(sc, OPL_KSL_LEVEL,   voice, 1, 0xff);
	opl_set_ch_reg(sc, OPL_KEYON_BLOCK, voice,    0);

	v = &sc->voices[voice];
	
	p = &opl2_instrs[MS_GETPGM(ms, voice)];
	v->patch = p;
	opl_load_patch(sc, voice);

	mult = 1;
	for (;;) {
		block_fnum = opl_get_block_fnum(freq / mult);
		if (block_fnum != OPL_FNUM_FAIL)
			break;
		mult *= 2;
		if (mult == 16)
			mult = 15;
	}

	chars0 = p->ops[OO_CHARS+0];
	chars1 = p->ops[OO_CHARS+1];
	m_mult = (chars0 & OPL_MULTIPLE_MASK) * mult;
	c_mult = (chars1 & OPL_MULTIPLE_MASK) * mult;
	if ((block_fnum == OPL_FNUM_FAIL) || (m_mult > 15) || (c_mult > 15)) {
		printf("oplsyn_noteon: frequence out of range %d\n",
		       MIDISYN_FREQ_TO_HZ(freq));
		return;
	}
	r20m = (chars0 &~ OPL_MULTIPLE_MASK) | m_mult;
	r20c = (chars1 &~ OPL_MULTIPLE_MASK) | c_mult;

	/* 2 voice */
	ksl0 = p->ops[OO_KSL_LEV+0];
	ksl1 = p->ops[OO_KSL_LEV+1];
	if (p->ops[OO_FB_CONN] & 0x01) {
		vol0 = opl_calc_vol(ksl0, vel, sc->volume);
		vol1 = opl_calc_vol(ksl1, vel, sc->volume);
	} else {
		vol0 = ksl0;
		vol1 = opl_calc_vol(ksl1, vel, sc->volume);
	}
	r40m = (ksl0 & OPL_KSL_MASK) | vol0;
	r40c = (ksl1 & OPL_KSL_MASK) | vol1;

	rA0  = block_fnum & 0xFF;
	rB0  = (block_fnum >> 8) | OPL_KEYON_BIT;

	v->rB0 = rB0;

	fbc = p->ops[OO_FB_CONN];
	if (sc->model == OPL_3) {
		fbc &= ~OPL_STEREO_BITS;
		/* XXX use pan */
		fbc |= OPL_VOICE_TO_LEFT | OPL_VOICE_TO_RIGHT;
	}
	opl_set_ch_reg(sc, OPL_FEEDBACK_CONNECTION, voice, fbc);

	opl_set_op_reg(sc, OPL_AM_VIB,      voice, 0, r20m);
	opl_set_op_reg(sc, OPL_AM_VIB,      voice, 1, r20c);
	opl_set_op_reg(sc, OPL_KSL_LEVEL,   voice, 0, r40m);
	opl_set_op_reg(sc, OPL_KSL_LEVEL,   voice, 1, r40c);
	opl_set_ch_reg(sc, OPL_FNUM_LOW,    voice,    rA0);
	opl_set_ch_reg(sc, OPL_KEYON_BLOCK, voice,    rB0);
}

void
oplsyn_noteoff(ms, voice, note, vel)
	midisyn *ms;
	u_int32_t voice, note, vel;
{
	struct opl_softc *sc = ms->data;
	struct opl_voice *v;

	DPRINTFN(3, ("oplsyn_noteoff: %p %d %d\n", sc, voice, 
		     MIDISYN_FREQ_TO_HZ(note)));

#ifdef DIAGNOSTIC
	if (voice < 0 || voice >= sc->syn.nvoice) {
		printf("oplsyn_noteoff: bad voice %d\n", voice);
		return;
	}
#endif
	v = &sc->voices[voice];
	opl_set_ch_reg(sc, 0xB0, voice, v->rB0 & ~OPL_KEYON_BIT);
}

void
oplsyn_keypressure(ms, voice, note, vel)
	midisyn *ms;
	u_int32_t voice, note, vel;
{
#ifdef AUDIO_DEBUG
	struct opl_softc *sc = ms->data;
	DPRINTFN(1, ("oplsyn_keypressure: %p %d\n", sc, note));
#endif
}

void
oplsyn_ctlchange(ms, voice, parm, w14)
	midisyn *ms;
	u_int32_t voice, parm, w14;
{
#ifdef AUDIO_DEBUG
	struct opl_softc *sc = ms->data;
	DPRINTFN(1, ("oplsyn_ctlchange: %p %d\n", sc, voice));
#endif
}

void
oplsyn_pitchbend(ms, voice, parm, x)
	midisyn *ms;
	u_int32_t voice, parm, x;
{
#ifdef AUDIO_DEBUG
	struct opl_softc *sc = ms->data;
	DPRINTFN(1, ("oplsyn_pitchbend: %p %d\n", sc, voice));
#endif
}

void
oplsyn_loadpatch(ms, sysex, uio)
	midisyn *ms;
	struct sysex_info *sysex;
	struct uio *uio;
{
#if 0
	struct opl_softc *sc = ms->data;
	struct sbi_instrument ins;

	DPRINTFN(1, ("oplsyn_loadpatch: %p\n", sc));

	memcpy(&ins, sysex, sizeof *sysex);
	if (uio->uio_resid >= sizeof ins - sizeof *sysex)
		return EINVAL;
	uiomove((char *)&ins + sizeof *sysex, sizeof ins - sizeof *sysex, uio);
	/* XXX */
#endif
}
