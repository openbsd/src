/* $OpenBSD: ym.c,v 1.17 2010/07/31 11:25:38 ratchov Exp $ */


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

#include "midi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/midi_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/ad1848var.h>
#include <dev/ic/mpuvar.h>
#include <dev/isa/ymvar.h>

/*
 * YAMAHA YMF715x (OPL3 Single-chip Audio System 3; OPL3-SA3)
 * control register description
 *
 * Other ports (SBpro, WSS CODEC, MPU401, OPL3, etc.) are NOT listed here.
 */

/*
 * direct registers
 */

/* offset from the base address */
#define SA3_CTL_INDEX	0	/* Index port (R/W) */
#define SA3_CTL_DATA	1	/* Data register port (R/W) */

#define SA3_CTL_NPORT	2	/* number of ports */

/*
 * indirect registers
 */

#define SA3_PWR_MNG		0x01	/* Power management (R/W) */
#define   SA3_PWR_MNG_ADOWN	0x20	/* Analog Down */
#define   SA3_PWR_MNG_PSV	0x04	/* Power save */
#define   SA3_PWR_MNG_PDN	0x02	/* Power down */
#define   SA3_PWR_MNG_PDX	0x01	/* Oscillation stop */
#define SA3_PWR_MNG_DEFAULT	0x00	/* default value */

#define SA3_SYS_CTL		0x02	/* System control (R/W) */
#define   SA3_SYS_CTL_SBHE	0x80	/* 0: AT-bus, 1: XT-bus */
#define   SA3_SYS_CTL_YMODE	0x30	/* 3D Enhancement mode */
#define     SA3_SYS_CTL_YMODE0	0x00	/* Desktop mode  (speaker 5-12cm) */
#define     SA3_SYS_CTL_YMODE1	0x10	/* Notebook PC mode (1)  (3cm) */
#define     SA3_SYS_CTL_YMODE2	0x20	/* Notebook PC mode (2)  (1.5cm) */
#define     SA3_SYS_CTL_YMODE3	0x30	/* Hi-Fi mode            (16-38cm) */
#define   SA3_SYS_CTL_IDSEL	0x06	/* Specify DSP version of SBPro */
#define     SA3_SYS_CTL_IDSEL0	0x00	/* major 0x03, minor 0x01 */
#define     SA3_SYS_CTL_IDSEL1	0x02	/* major 0x02, minor 0x01 */
#define     SA3_SYS_CTL_IDSEL2	0x04	/* major 0x01, minor 0x05 */
#define     SA3_SYS_CTL_IDSEL3	0x06	/* major 0x00, minor 0x00 */
#define   SA3_SYS_CTL_VZE	0x01	/* ZV */
#define SA3_SYS_CTL_DEFAULT	0x00	/* default value */

#define SA3_IRQ_CONF		0x03	/* Interrupt Channel config (R/W) */
#define   SA3_IRQ_CONF_OPL3_B	0x80	/* OPL3 uses IRQ-B */
#define   SA3_IRQ_CONF_MPU_B	0x40	/* MPU401 uses IRQ-B */
#define   SA3_IRQ_CONF_SB_B	0x20	/* Sound Blaster uses IRQ-B */
#define   SA3_IRQ_CONF_WSS_B	0x10	/* WSS CODEC uses IRQ-B */
#define   SA3_IRQ_CONF_OPL3_A	0x08	/* OPL3 uses IRQ-A */
#define   SA3_IRQ_CONF_MPU_A	0x04	/* MPU401 uses IRQ-A */
#define   SA3_IRQ_CONF_SB_A	0x02	/* Sound Blaster uses IRQ-A */
#define   SA3_IRQ_CONF_WSS_A	0x01	/* WSS CODEC uses IRQ-A */
#define SA3_IRQ_CONF_DEFAULT	(SA3_IRQ_CONF_MPU_B | SA3_IRQ_CONF_SB_B | \
				 SA3_IRQ_CONF_OPL3_A | SA3_IRQ_CONF_WSS_A)

#define SA3_IRQA_STAT		0x04	/* Interrupt (IRQ-A) STATUS (RO) */
#define SA3_IRQB_STAT		0x05	/* Interrupt (IRQ-B) STATUS (RO) */
#define   SA3_IRQ_STAT_MV	0x40	/* Hardware Volume Interrupt */
#define   SA3_IRQ_STAT_OPL3	0x20	/* Internal FM-synthesizer timer */
#define   SA3_IRQ_STAT_MPU	0x10	/* MPU401 Interrupt */
#define   SA3_IRQ_STAT_SB	0x08	/* Sound Blaster Playback Interrupt */
#define   SA3_IRQ_STAT_TI	0x04	/* Timer Flag of CODEC */
#define   SA3_IRQ_STAT_CI	0x02	/* Recording Flag of CODEC */
#define   SA3_IRQ_STAT_PI	0x01	/* Playback Flag of CODEC */

#define SA3_DMA_CONF		0x06	/* DMA configuration (R/W) */
#define   SA3_DMA_CONF_SB_B	0x40	/* Sound Blaster playback uses DMA-B */
#define   SA3_DMA_CONF_WSS_R_B	0x20	/* WSS CODEC recording uses DMA-B */
#define   SA3_DMA_CONF_WSS_P_B	0x10	/* WSS CODEC playback uses DMA-B */
#define   SA3_DMA_CONF_SB_A	0x04	/* Sound Blaster playback uses DMA-A */
#define   SA3_DMA_CONF_WSS_R_A	0x02	/* WSS CODEC recording uses DMA-A */
#define   SA3_DMA_CONF_WSS_P_A	0x01	/* WSS CODEC playback uses DMA-A */
#define SA3_DMA_CONF_DEFAULT	(SA3_DMA_CONF_SB_B | SA3_DMA_CONF_WSS_R_B | \
				 SA3_DMA_CONF_WSS_P_A)

#define SA3_VOL_L		0x07	/* Master Volume Lch (R/W) */
#define SA3_VOL_R		0x08	/* Master Volume Rch (R/W) */
#define   SA3_VOL_MUTE		0x80	/* Mute the channel */
#define   SA3_VOL_MV		0x0f	/* Master Volume bits */
#define     SA3_VOL_MV_0	0x00	/*   0dB (maximum volume) */
#define     SA3_VOL_MV_2	0x01	/*  -2dB */
#define     SA3_VOL_MV_4	0x02	/*  -4dB */
#define     SA3_VOL_MV_6	0x03	/*  -6dB */
#define     SA3_VOL_MV_8	0x04	/*  -8dB */
#define     SA3_VOL_MV_10	0x05	/* -10dB */
#define     SA3_VOL_MV_12	0x06	/* -12dB */
#define     SA3_VOL_MV_14	0x07	/* -14dB (default) */
#define     SA3_VOL_MV_16	0x08	/* -16dB */
#define     SA3_VOL_MV_18	0x09	/* -18dB */
#define     SA3_VOL_MV_20	0x0a	/* -20dB */
#define     SA3_VOL_MV_22	0x0b	/* -22dB */
#define     SA3_VOL_MV_24	0x0c	/* -24dB */
#define     SA3_VOL_MV_26	0x0d	/* -26dB */
#define     SA3_VOL_MV_28	0x0e	/* -28dB */
#define     SA3_VOL_MV_30	0x0f	/* -30dB (minimum volume) */
#define SA3_VOL_DEFAULT		SA3_VOL_MV_14

#define SA3_MIC_VOL		0x09	/* MIC Volume (R/W) */
#define   SA3_MIC_MUTE		0x80	/* Mute Mic Volume */
#define   SA3_MIC_MCV		0x1f	/* Mic volume bits */
#define     SA3_MIC_MCV12	0x00	/* +12.0dB (maximum volume) */
#define     SA3_MIC_MCV10_5	0x01	/* +10.5dB */
#define     SA3_MIC_MCV9	0x02	/*  +9.0dB */
#define     SA3_MIC_MCV7_5	0x03	/*  +7.5dB */
#define     SA3_MIC_MCV6	0x04	/*  +6.0dB */
#define     SA3_MIC_MCV4_5	0x05	/*  +4.5dB */
#define     SA3_MIC_MCV3	0x06	/*  +3.0dB */
#define     SA3_MIC_MCV1_5	0x07	/*  +1.5dB */
#define     SA3_MIC_MCV_0	0x08	/*   0.0dB (default) */
#define     SA3_MIC_MCV_1_5	0x09	/*  -1.5dB */
#define     SA3_MIC_MCV_3_0	0x0a	/*  -3.0dB */
#define     SA3_MIC_MCV_4_5	0x0b	/*  -4.5dB */
#define     SA3_MIC_MCV_6	0x0c	/*  -6.0dB */
#define     SA3_MIC_MCV_7_5	0x0d	/*  -7.5dB */
#define     SA3_MIC_MCV_9	0x0e	/*  -9.0dB */
#define     SA3_MIC_MCV_10_5	0x0f	/* -10.5dB */
#define     SA3_MIC_MCV_12	0x10	/* -12.0dB */
#define     SA3_MIC_MCV_13_5	0x11	/* -13.5dB */
#define     SA3_MIC_MCV_15	0x12	/* -15.0dB */
#define     SA3_MIC_MCV_16_5	0x13	/* -16.5dB */
#define     SA3_MIC_MCV_18	0x14	/* -18.0dB */
#define     SA3_MIC_MCV_19_5	0x15	/* -19.5dB */
#define     SA3_MIC_MCV_21	0x16	/* -21.0dB */
#define     SA3_MIC_MCV_22_5	0x17	/* -22.5dB */
#define     SA3_MIC_MCV_24	0x18	/* -24.0dB */
#define     SA3_MIC_MCV_25_5	0x19	/* -25.5dB */
#define     SA3_MIC_MCV_27	0x1a	/* -27.0dB */
#define     SA3_MIC_MCV_28_5	0x1b	/* -28.5dB */
#define     SA3_MIC_MCV_30	0x1c	/* -30.0dB */
#define     SA3_MIC_MCV_31_5	0x1d	/* -31.5dB */
#define     SA3_MIC_MCV_33	0x1e	/* -33.0dB */
#define     SA3_MIC_MCV_34_5	0x1f	/* -34.5dB (minimum volume) */
#define SA3_MIC_VOL_DEFAULT	(SA3_MIC_MUTE | SA3_MIC_MCV_0)

#define SA3_MISC		0x0a	/* Miscellaneous */
#define   SA3_MISC_VEN		0x80	/* Enable hardware volume control */
#define   SA3_MISC_MCSW		0x10	/* A/D is connected to  0: Rch of Mic,
					   1: loopback of monaural output */
#define   SA3_MISC_MODE		0x08	/* 0: SB mode, 1: WSS mode (RO) */
#define   SA3_MISC_VER		0x07	/* Version of OPL3-SA3 (RO) */
					/*	(4 or 5?) */
/*#define SA3_MISC_DEFAULT	(SA3_MISC_VEN | (4 or 5?)) */

/* WSS DMA Base counters (R/W) used for suspend/resume */
#define SA3_DMA_CNT_PLAY_LOW	0x0b	/* Playback Base Counter (Low) */
#define SA3_DMA_CNT_PLAY_HIGH	0x0c	/* Playback Base Counter (High) */
#define SA3_DMA_CNT_REC_LOW	0x0d	/* Recording Base Counter (Low) */
#define SA3_DMA_CNT_REC_HIGH	0x0e	/* Recording Base Counter (High) */

#define SA3_WSS_INT_SCAN	0x0f	/* WSS Interrupt Scan out/in (R/W) */
#define   SA3_WSS_INT_SCAN_STI	0x04	/* 1: TI = "1" and IRQ active */
#define   SA3_WSS_INT_SCAN_SCI	0x02	/* 1: CI = "1" and IRQ active */
#define   SA3_WSS_INT_SCAN_SPI	0x01	/* 1: PI = "1" and IRQ active */
#define SA3_WSS_INT_DEFAULT	0x00	/* default value */

#define SA3_SB_SCAN		0x10	/* SB Internal State Scan out/in (R/W)*/
#define   SA3_SB_SCAN_SBPDA	0x80	/* Sound Blaster Power Down ack */
#define   SA3_SB_SCAN_SS	0x08	/* Scan Select */
#define   SA3_SB_SCAN_SM	0x04	/* Scan Mode 1: read out, 0: write in */
#define   SA3_SB_SCAN_SE	0x02	/* Scan Enable */
#define   SA3_SB_SCAN_SBPDR	0x01	/* Sound Blaster Power Down Request */
#define SA3_SB_SCAN_DEFAULT	0x00	/* default value */

#define SA3_SB_SCAN_DATA	0x11	/* SB Internal State Scan Data (R/W)*/

#define SA3_DPWRDWN		0x12	/* Digital Partial Power Down (R/W) */
#define   SA3_DPWRDWN_JOY	0x80	/* Joystick power down */
#define   SA3_DPWRDWN_MPU	0x40	/* MPU401 power down */
#define   SA3_DPWRDWN_MCLKO	0x20	/* Master Clock disable */
#define   SA3_DPWRDWN_FM	0x10	/* FM (OPL3) power down */
#define   SA3_DPWRDWN_WSS_R	0x08	/* WSS recording power down */
#define   SA3_DPWRDWN_WSS_P	0x04	/* WSS playback power down */
#define   SA3_DPWRDWN_SB	0x02	/* Sound Blaster power down */
#define   SA3_DPWRDWN_PNP	0x01	/* PnP power down */
#define SA3_DPWRDWN_DEFAULT	0x00	/* default value */

#define SA3_APWRDWN		0x13	/* Analog Partial Power Down (R/W) */
#define   SA3_APWRDWN_FMDAC	0x10	/* FMDAC for OPL3 power down */ 
#define   SA3_APWRDWN_AD	0x08	/* A/D for WSS recording power down */
#define   SA3_APWRDWN_DA	0x04	/* D/A for WSS playback power down */
#define   SA3_APWRDWN_SBDAC	0x02	/* D/A for SB power down */
#define   SA3_APWRDWN_WIDE	0x01	/* Wide Stereo power down */
#define SA3_APWRDWN_DEFAULT	0x00	/* default value */

#define SA3_3D_WIDE		0x14	/* 3D Enhanced control (WIDE) (R/W) */
#define   SA3_3D_WIDE_WIDER	0x70	/* Rch of wide 3D enhanced control */
#define   SA3_3D_WIDE_WIDEL	0x07	/* Lch of wide 3D enhanced control */
#define SA3_3D_WIDE_DEFAULT	0x00	/* default value */

#define SA3_3D_BASS		0x15	/* 3D Enhanced control (BASS) (R/W) */
#define   SA3_3D_BASS_BASSR	0x70	/* Rch of bass 3D enhanced control */
#define   SA3_3D_BASS_BASSL	0x07	/* Lch of bass 3D enhanced control */
#define SA3_3D_BASS_DEFAULT	0x00	/* default value */

#define SA3_3D_TREBLE		0x16	/* 3D Enhanced control (TREBLE) (R/W) */
#define   SA3_3D_TREBLE_TRER	0x70	/* Rch of treble 3D enhanced control */
#define   SA3_3D_TREBLE_TREL	0x07	/* Lch of treble 3D enhanced control */
#define SA3_3D_TREBLE_DEFAULT	0x00	/* default value */

/* common to the 3D enhance registers */
#define   SA3_3D_BITS		0x07
#define   SA3_3D_LSHIFT		0
#define   SA3_3D_RSHIFT		4

#define SA3_HVOL_INTR_CNF	0x17	/* Hardware Volume Intr Channel (R/W) */
#define   SA3_HVOL_INTR_CNF_B	0x20	/* Hardware Volume uses IRQ-B */
#define   SA3_HVOL_INTR_CNF_A	0x10	/* Hardware Volume uses IRQ-A */
#define SA3_HVOL_INTR_CNF_DEFAULT	0x00

#define SA3_MULTI_STAT		0x18	/* Multi-purpose Select Pin Stat (RO) */
#define   SA3_MULTI_STAT_SEL	0x70	/* State of SEL2-0 pins */

int ym_getdev(void *, struct audio_device *);
int ym_mixer_set_port(void *, mixer_ctrl_t *);
int ym_mixer_get_port(void *, mixer_ctrl_t *);
int ym_query_devinfo(void *, mixer_devinfo_t *);
int ym_intr(void *);

static void ym_mute(struct ym_softc *, int, int);
static void ym_set_master_gain(struct ym_softc *, struct ad1848_volume *);
static void ym_set_mic_gain(struct ym_softc *, int);
static void ym_set_3d(struct ym_softc *, mixer_ctrl_t *,
	struct ad1848_volume *, int);

struct audio_hw_if ym_hw_if = {
	ad1848_open,
	ad1848_close,
	NULL,
	ad1848_query_encoding,
	ad1848_set_params,
	ad1848_round_blocksize,
	ad1848_commit_settings,
	NULL,
	NULL,
	NULL,
	NULL,
	ad1848_halt_output,
	ad1848_halt_input,
	NULL,
	ym_getdev,
	NULL,
	ym_mixer_set_port,
	ym_mixer_get_port,
	ym_query_devinfo,
	ad1848_malloc,
	ad1848_free,
	ad1848_round,
	ad1848_mappage,
	ad1848_get_props,
	ad1848_trigger_output,
	ad1848_trigger_input,
	NULL
};


struct cfdriver ym_cd = {
	NULL, "ym", DV_DULL
};

struct audio_device ym_device = {
	"ym,ad1848",
	"",
	"ym"
};

static __inline int ym_read(struct ym_softc *, int);
static __inline void ym_write(struct ym_softc *, int, int);

#if NMIDI > 0
int	ym_mpu401_open(void *, int, void (*iintr)(void *, int),
	    void (*ointr)(void *), void *arg);
void	ym_mpu401_close(void *);
int	ym_mpu401_output(void *, int);
void	ym_mpu401_getinfo(void *, struct midi_info *);

struct midi_hw_if ym_mpu401_hw_if = {
	ym_mpu401_open,
	ym_mpu401_close,
	ym_mpu401_output,
	0,		/* flush */
	ym_mpu401_getinfo,
	0,		/* ioctl */
};
#endif

int
ym_intr(v)
	void   *v;
{
#if NMIDI > 0
	struct ym_softc *sc = v;

	if ( /* XXX && */ sc->sc_hasmpu)
		mpu_intr(&sc->sc_mpu_sc);
#endif
	return ad1848_intr(v);
}

void
ym_attach(sc)
	struct ym_softc *sc;

{
	struct ad1848_volume vol_mid = {220, 220};
#if NMIDI > 0
	struct midi_hw_if *mhw = &ym_mpu401_hw_if;
#endif

	sc->sc_ih = isa_intr_establish(sc->sc_ic, sc->ym_irq, IST_EDGE,
	    IPL_AUDIO, ym_intr, &sc->sc_ad1848, sc->sc_dev.dv_xname);

	ad1848_attach(&sc->sc_ad1848);
	printf("\n");
	sc->sc_ad1848.parent = sc;

	/* Establish chip in well known mode */
	ym_set_master_gain(sc, &vol_mid);
	ym_set_mic_gain(sc, 0);
	sc->master_mute = 0;
	ym_mute(sc, SA3_VOL_L, sc->master_mute);
	ym_mute(sc, SA3_VOL_R, sc->master_mute);

	sc->mic_mute = 1;
	ym_mute(sc, SA3_MIC_VOL, sc->mic_mute);

#if NMIDI > 0
	sc->sc_hasmpu = 0;
	if (sc->sc_mpu_sc.iobase) {
		sc->sc_mpu_sc.iot = sc->sc_iot;
		if (mpu_find(&sc->sc_mpu_sc)) {
			sc->sc_hasmpu = 1;
			mhw = &ym_mpu401_hw_if;
		}
	}
	midi_attach_mi(mhw, sc, &sc->sc_dev);
#endif

	audio_attach_mi(&ym_hw_if, &sc->sc_ad1848, &sc->sc_dev);
}

static __inline int
ym_read(sc, reg)
	struct ym_softc *sc;
	int     reg;
{
	bus_space_write_1(sc->sc_iot, sc->sc_controlioh, SA3_CTL_INDEX,
	    (reg & 0xff));
	return (bus_space_read_1(sc->sc_iot, sc->sc_controlioh, SA3_CTL_DATA));
}

static __inline void
ym_write(sc, reg, data)
	struct ym_softc *sc;
	int     reg;
	int     data;
{
	bus_space_write_1(sc->sc_iot, sc->sc_controlioh, SA3_CTL_INDEX,
	    (reg & 0xff));
	bus_space_write_1(sc->sc_iot, sc->sc_controlioh, SA3_CTL_DATA,
	    (data & 0xff));
}



int
ym_getdev(addr, retp)
	void   *addr;
	struct audio_device *retp;
{
	*retp = ym_device;
	return 0;
}


static ad1848_devmap_t mappings[] = {
	{ YM_MIDI_LVL, AD1848_KIND_LVL, AD1848_AUX2_CHANNEL },
	{ YM_CD_LVL, AD1848_KIND_LVL, AD1848_AUX1_CHANNEL },
	{ YM_DAC_LVL, AD1848_KIND_LVL, AD1848_DAC_CHANNEL },
	{ YM_LINE_LVL, AD1848_KIND_LVL, AD1848_LINE_CHANNEL },
	{ YM_SPEAKER_LVL, AD1848_KIND_LVL, AD1848_MONO_CHANNEL },
	{ YM_MONITOR_LVL, AD1848_KIND_LVL, AD1848_MONITOR_CHANNEL },
	{ YM_MIDI_MUTE, AD1848_KIND_MUTE, AD1848_AUX2_CHANNEL },
	{ YM_CD_MUTE, AD1848_KIND_MUTE, AD1848_AUX1_CHANNEL },
	{ YM_DAC_MUTE, AD1848_KIND_MUTE, AD1848_DAC_CHANNEL },
	{ YM_LINE_MUTE, AD1848_KIND_MUTE, AD1848_LINE_CHANNEL },
	{ YM_SPEAKER_MUTE, AD1848_KIND_MUTE, AD1848_MONO_CHANNEL },
	{ YM_MONITOR_MUTE, AD1848_KIND_MUTE, AD1848_MONITOR_CHANNEL },
	{ YM_REC_LVL, AD1848_KIND_RECORDGAIN, -1 },
	{ YM_RECORD_SOURCE, AD1848_KIND_RECORDSOURCE, -1 }
};

#define NUMMAP	(sizeof(mappings) / sizeof(mappings[0]))


static void
ym_mute(sc, left_reg, mute)
	struct ym_softc *sc;
	int     left_reg;
	int     mute;
{
	u_int8_t reg;

	reg = ym_read(sc, left_reg);
	if (mute)
		ym_write(sc, left_reg, reg | 0x80);
	else
		ym_write(sc, left_reg, reg & ~0x80);
}

static void
ym_set_master_gain(sc, vol)
	struct ym_softc *sc;
	struct ad1848_volume *vol;
{
	u_int   atten;

	sc->master_gain = *vol;

	atten = ((AUDIO_MAX_GAIN - vol->left) * (SA3_VOL_MV + 1)) /
	   (AUDIO_MAX_GAIN + 1);

	ym_write(sc, SA3_VOL_L, (ym_read(sc, SA3_VOL_L) & ~SA3_VOL_MV) | atten);

	atten = ((AUDIO_MAX_GAIN - vol->right) * (SA3_VOL_MV + 1)) /
	   (AUDIO_MAX_GAIN + 1);

	ym_write(sc, SA3_VOL_R, (ym_read(sc, SA3_VOL_R) & ~SA3_VOL_MV) | atten);
}

static void
ym_set_mic_gain(sc, vol)
	struct ym_softc *sc;
	int vol;
{
	u_int   atten;

	sc->mic_gain = vol;

	atten = ((AUDIO_MAX_GAIN - vol) * (SA3_MIC_MCV + 1)) /
	    (AUDIO_MAX_GAIN + 1);

	ym_write(sc, SA3_MIC_VOL,
	    (ym_read(sc, SA3_MIC_VOL) & ~SA3_MIC_MCV) | atten);
}

static void
ym_set_3d(sc, cp, val, reg)
	struct ym_softc *sc;
	mixer_ctrl_t *cp;
	struct ad1848_volume *val;
	int reg;
{
	u_int8_t e;

	ad1848_to_vol(cp, val);

	e = (val->left * (SA3_3D_BITS + 1) + (SA3_3D_BITS + 1) / 2) /
		(AUDIO_MAX_GAIN + 1) << SA3_3D_LSHIFT |
	    (val->right * (SA3_3D_BITS + 1) + (SA3_3D_BITS + 1) / 2) /
		(AUDIO_MAX_GAIN + 1) << SA3_3D_RSHIFT;

	ym_write(sc, reg, e);
}

int
ym_mixer_set_port(addr, cp)
	void   *addr;
	mixer_ctrl_t *cp;
{
	struct ad1848_softc *ac = addr;
	struct ym_softc *sc = ac->parent;
	struct ad1848_volume vol;
	int     error = ad1848_mixer_set_port(ac, mappings, NUMMAP, cp);

	if (error != ENXIO)
		return (error);

	error = 0;

	switch (cp->dev) {
	case YM_OUTPUT_LVL:
		ad1848_to_vol(cp, &vol);
		ym_set_master_gain(sc, &vol);
		break;

	case YM_OUTPUT_MUTE:
		sc->master_mute = (cp->un.ord != 0);
		ym_mute(sc, SA3_VOL_L, sc->master_mute);
		ym_mute(sc, SA3_VOL_R, sc->master_mute);
		break;

	case YM_MIC_LVL:
		if (cp->un.value.num_channels != 1)
			error = EINVAL;
		else
			ym_set_mic_gain(sc,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
		break;

	case YM_MASTER_EQMODE:
		sc->sc_eqmode = cp->un.ord & SA3_SYS_CTL_YMODE;
		ym_write(sc, SA3_SYS_CTL, (ym_read(sc, SA3_SYS_CTL) &
		    ~SA3_SYS_CTL_YMODE) | sc->sc_eqmode);
		break;

	case YM_MASTER_TREBLE:
		ym_set_3d(sc, cp, &sc->sc_treble, SA3_3D_TREBLE);
		break;

	case YM_MASTER_BASS:
		ym_set_3d(sc, cp, &sc->sc_bass, SA3_3D_BASS);
		break;

	case YM_MASTER_WIDE:
		ym_set_3d(sc, cp, &sc->sc_wide, SA3_3D_WIDE);
		break;

	case YM_MIC_MUTE:
		sc->mic_mute = (cp->un.ord != 0);
		ym_mute(sc, SA3_MIC_VOL, sc->mic_mute);
		break;

	default:
		return ENXIO;
		/* NOTREACHED */
	}

	return (error);
}

int
ym_mixer_get_port(addr, cp)
	void   *addr;
	mixer_ctrl_t *cp;
{
	struct ad1848_softc *ac = addr;
	struct ym_softc *sc = ac->parent;

	int     error = ad1848_mixer_get_port(ac, mappings, NUMMAP, cp);

	if (error != ENXIO)
		return (error);

	error = 0;

	switch (cp->dev) {
	case YM_OUTPUT_LVL:
		ad1848_from_vol(cp, &sc->master_gain);
		break;

	case YM_OUTPUT_MUTE:
		cp->un.ord = sc->master_mute;
		break;

	case YM_MIC_LVL:
		if (cp->un.value.num_channels != 1)
			error = EINVAL;
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->mic_gain;	
		break;

	case YM_MASTER_EQMODE:
		cp->un.ord = sc->sc_eqmode;
		break;

	case YM_MASTER_TREBLE:
		ad1848_from_vol(cp, &sc->sc_treble);
		break;

	case YM_MASTER_BASS:
		ad1848_from_vol(cp, &sc->sc_bass);
		break;

	case YM_MASTER_WIDE:
		ad1848_from_vol(cp, &sc->sc_wide);
		break;

	case YM_MIC_MUTE:
		cp->un.ord = sc->mic_mute;
		break;

	default:
		error = ENXIO;
		break;
	}

	return (error);
}

static char *mixer_classes[] = {
	AudioCinputs, AudioCrecord, AudioCoutputs, AudioCmonitor,
	AudioCequalization
};

int
ym_query_devinfo(addr, dip)
	void   *addr;
	mixer_devinfo_t *dip;
{
	static char *mixer_port_names[] = { AudioNmidi, AudioNcd, AudioNdac,
		AudioNline, AudioNspeaker, AudioNmicrophone, AudioNmonitor
	};

	dip->next = dip->prev = AUDIO_MIXER_LAST;

	switch (dip->index) {
	case YM_INPUT_CLASS:	/* input class descriptor */
	case YM_OUTPUT_CLASS:
	case YM_MONITOR_CLASS:
	case YM_RECORD_CLASS:
	case YM_EQ_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = dip->index;
		strlcpy(dip->label.name,
		    mixer_classes[dip->index - YM_INPUT_CLASS],
		    sizeof dip->label.name);
		break;

	case YM_MIDI_LVL:
	case YM_CD_LVL:
	case YM_DAC_LVL:
	case YM_LINE_LVL:
	case YM_SPEAKER_LVL:
	case YM_MIC_LVL:
	case YM_MONITOR_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		if (dip->index == YM_MONITOR_LVL)
			dip->mixer_class = YM_MONITOR_CLASS;
		else
			dip->mixer_class = YM_INPUT_CLASS;

		dip->next = dip->index + 7;

		strlcpy(dip->label.name,
		    mixer_port_names[dip->index - YM_MIDI_LVL],
		    sizeof dip->label.name);

		if (dip->index == YM_SPEAKER_LVL ||
		    dip->index == YM_MIC_LVL)
			dip->un.v.num_channels = 1;
		else
			dip->un.v.num_channels = 2;

		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case YM_MIDI_MUTE:
	case YM_CD_MUTE:
	case YM_DAC_MUTE:
	case YM_LINE_MUTE:
	case YM_SPEAKER_MUTE:
	case YM_MIC_MUTE:
	case YM_MONITOR_MUTE:
		if (dip->index == YM_MONITOR_MUTE)
			dip->mixer_class = YM_MONITOR_CLASS;
		else
			dip->mixer_class = YM_INPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = dip->index - 7;
mute:
		strlcpy(dip->label.name, AudioNmute, sizeof dip->label.name);
		dip->un.e.num_mem = 2;
		strlcpy(dip->un.e.member[0].label.name, AudioNoff,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = 0;
		strlcpy(dip->un.e.member[1].label.name, AudioNon,
		    sizeof dip->un.e.member[1].label.name);
		dip->un.e.member[1].ord = 1;
		break;


	case YM_OUTPUT_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_OUTPUT_CLASS;
		dip->next = YM_OUTPUT_MUTE;
		strlcpy(dip->label.name, AudioNmaster, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case YM_OUTPUT_MUTE:
		dip->mixer_class = YM_OUTPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = YM_OUTPUT_LVL;
		goto mute;

	case YM_REC_LVL:	/* record level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_RECORD_CLASS;
		dip->next = YM_RECORD_SOURCE;
		strlcpy(dip->label.name, AudioNrecord, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;


	case YM_RECORD_SOURCE:
		dip->mixer_class = YM_RECORD_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = YM_REC_LVL;
		strlcpy(dip->label.name, AudioNsource, sizeof dip->label.name);
		dip->un.e.num_mem = 4;
		strlcpy(dip->un.e.member[0].label.name, AudioNmicrophone,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = MIC_IN_PORT;
		strlcpy(dip->un.e.member[1].label.name, AudioNline,
		    sizeof dip->un.e.member[1].label.name);
		dip->un.e.member[1].ord = LINE_IN_PORT;
		strlcpy(dip->un.e.member[2].label.name, AudioNdac,
		    sizeof dip->un.e.member[2].label.name);
		dip->un.e.member[2].ord = DAC_IN_PORT;
		strlcpy(dip->un.e.member[3].label.name, AudioNcd,
		    sizeof dip->un.e.member[3].label.name);
		dip->un.e.member[3].ord = AUX1_IN_PORT;
		break;

	case YM_MASTER_EQMODE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = YM_EQ_CLASS;
		strlcpy(dip->label.name, AudioNmode, sizeof dip->label.name);
		strlcpy(dip->un.v.units.name, AudioNmode,
		    sizeof dip->un.v.units.name);
		dip->un.e.num_mem = 4;
		strlcpy(dip->un.e.member[0].label.name, AudioNdesktop,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = SA3_SYS_CTL_YMODE0;
		strlcpy(dip->un.e.member[1].label.name, AudioNlaptop,
		    sizeof dip->un.e.member[1].label.name);
		dip->un.e.member[1].ord = SA3_SYS_CTL_YMODE1;
		strlcpy(dip->un.e.member[2].label.name, AudioNsubnote,
		    sizeof dip->un.e.member[2].label.name);
		dip->un.e.member[2].ord = SA3_SYS_CTL_YMODE2;
		strlcpy(dip->un.e.member[3].label.name, AudioNhifi,
		    sizeof dip->un.e.member[3].label.name);
		dip->un.e.member[3].ord = SA3_SYS_CTL_YMODE3;
		break;

	case YM_MASTER_TREBLE:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_EQ_CLASS;
		strlcpy(dip->label.name, AudioNtreble, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNtreble,
		    sizeof dip->un.v.units.name);
		break;

	case YM_MASTER_BASS:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_EQ_CLASS;
		strlcpy(dip->label.name, AudioNbass, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNbass,
		    sizeof dip->un.v.units.name);
		break;

	case YM_MASTER_WIDE:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_EQ_CLASS;
		strlcpy(dip->label.name, AudioNsurround,
		    sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNsurround,
		    sizeof dip->un.v.units.name);
		break;

	default:
		return ENXIO;
		/* NOTREACHED */
	}

	return 0;
}
#if NMIDI > 0

#define YMMPU(a) (&((struct ym_softc *)addr)->sc_mpu_sc)

int
ym_mpu401_open(addr, flags, iintr, ointr, arg)
	void   *addr;
	int     flags;
	void    (*iintr)(void *, int);
	void    (*ointr)(void *);
	void   *arg;
{
	return mpu_open(YMMPU(addr), flags, iintr, ointr, arg);
}

int
ym_mpu401_output(addr, d)
	void   *addr;
	int     d;
{
	return mpu_output(YMMPU(addr), d);
}

void
ym_mpu401_close(addr)
	void   *addr;
{
	mpu_close(YMMPU(addr));
}

void
ym_mpu401_getinfo(addr, mi)
	void   *addr;
	struct midi_info *mi;
{
	mi->name = "YM MPU-401 UART";
	mi->props = 0;
}
#endif
