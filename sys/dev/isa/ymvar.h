/*	$OpenBSD: ymvar.h,v 1.5 2002/03/14 01:26:57 millert Exp $	*/
/*	$NetBSD: wssvar.h,v 1.1 1998/01/19 22:18:25 augustss Exp $	*/

/*
 * Copyright (c) 1998 Constantine Sapuntzakis. All rights reserved.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Mixer devices
 */
#define YM_MIDI_LVL		0
#define YM_CD_LVL		1
#define YM_DAC_LVL		2
#define YM_LINE_LVL		3
#define YM_SPEAKER_LVL		4
#define YM_MIC_LVL		5
#define YM_MONITOR_LVL		6
#define YM_MIDI_MUTE		7
#define YM_CD_MUTE		8
#define YM_DAC_MUTE		9
#define YM_LINE_MUTE		10
#define YM_SPEAKER_MUTE		11
#define YM_MIC_MUTE		12
#define YM_MONITOR_MUTE		13

#define YM_REC_LVL		14
#define YM_RECORD_SOURCE	15

#define YM_OUTPUT_LVL		16
#define YM_OUTPUT_MUTE		17

#define	YM_MASTER_EQMODE	18
#define	YM_MASTER_TREBLE	19
#define	YM_MASTER_BASS		20
#define	YM_MASTER_WIDE		21

/* Classes - don't change this without looking at mixer_classes array */
#define	YM_CLASS_TOP		22
#define YM_INPUT_CLASS		(YM_CLASS_TOP + 0)
#define YM_RECORD_CLASS		(YM_CLASS_TOP + 1) 
#define YM_OUTPUT_CLASS		(YM_CLASS_TOP + 2)
#define YM_MONITOR_CLASS	(YM_CLASS_TOP + 3)
#define YM_EQ_CLASS		(YM_CLASS_TOP + 4)

#define	AudioNmode		"mode"
#define	AudioNdesktop		"desktop"
#define	AudioNlaptop		"laptop"
#define	AudioNsubnote		"subnote"
#define	AudioNhifi		"hifi"

struct ym_softc {
	struct	device sc_dev;		/* base device */
	struct	isadev sc_id;		/* ISA device */
	void	*sc_ih;			/* interrupt vectoring */
	bus_space_tag_t sc_iot;		/* tag */
	bus_space_handle_t sc_ioh;	/* handle */
	isa_chipset_tag_t sc_ic;

        bus_space_handle_t sc_controlioh;

	struct  ad1848_softc sc_ad1848;
#define ym_irq    sc_ad1848.sc_irq
#define ym_drq    sc_ad1848.sc_drq
#define ym_recdrq sc_ad1848.sc_recdrq

        int  master_mute, mic_mute;
        struct ad1848_volume master_gain;
	u_int8_t mic_gain;

	/* 3D enhancement */
	u_int8_t sc_eqmode;
	struct ad1848_volume sc_treble, sc_bass, sc_wide;
#if NMIDI > 0
	int	sc_hasmpu;
	struct	mpu_softc sc_mpu_sc;	/* MPU401 Uart state */
#endif
};

void    ym_attach(struct ym_softc *);
