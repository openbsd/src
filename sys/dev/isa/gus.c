/*	$NetBSD: gus.c,v 1.3 1995/11/10 04:30:44 mycroft Exp $	*/

/*
 * Copyright (c) 1994, 1995 Ken Hornstein.  All rights reserved.
 * Copyright (c) 1995 John T. Kohl.  All rights reserved.
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
 *	This product includes software developed by Ken Hornstein.
 * 4. The name of the authors may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *
 * TODO:
 *	. figure out why mixer activity while sound is playing causes problems
 *	  (phantom interrupts?)
 *  	. figure out a better deinterleave strategy that avoids sucking up
 *	  CPU, memory and cache bandwidth.  (Maybe a special encoding?
 *	  Maybe use the double-speed sampling/hardware deinterleave trick
 *	  from the GUS SDK?)  A 486/33 isn't quite fast enough to keep
 *	  up with 44.1kHz 16-bit stereo output without some drop-outs.
 *	. use CS4231 for 16-bit sampling, for a-law and mu-law playback.
 *	. actually test full-duplex sampling(recording) and playback.
 */

/*
 * Gravis UltraSound driver
 *
 * For more detailed information, see the GUS developers' kit
 * available on the net at:
 *
 * ftp://freedom.nmsu.edu/pub/ultrasound/gravis/util/
 * 	gusdkXXX.zip (developers' kit--get rev 2.22 or later)
 *		See ultrawrd.doc inside--it's MS Word (ick), but it's the bible
 *
 */

/*
 * The GUS Max has a slightly strange set of connections between the CS4231
 * and the GF1 and the DMA interconnects.  It's set up so that the CS4231 can
 * be playing while the GF1 is loading patches from the system.
 *
 * Here's a recreation of the DMA interconnect diagram:
 *
 *       GF1
 *   +---------+				 digital
 *   |         |  record			 ASIC
 *   |         |--------------+
 *   |         |              |		       +--------+
 *   |         | play (dram)  |      +----+    |	|
 *   |         |--------------(------|-\  |    |   +-+  |
 *   +---------+              |      |  >-|----|---|C|--|------  dma chan 1
 *                            |  +---|-/  |    |   +-+ 	|
 *                            |  |   +----+    |    |   |
 *                            |	 |   +----+    |    |   |
 *   +---------+        +-+   +--(---|-\  |    |    |   |
 *   |         | play   |8|      |   |  >-|----|----+---|------  dma chan 2
 *   | ---C----|--------|/|------(---|-/  |    |        |
 *   |    ^    |record  |1|      |   +----+    |	|
 *   |    |    |   /----|6|------+   	       +--------+
 *   | ---+----|--/     +-+
 *   +---------+
 *     CS4231   	8-to-16 bit bus conversion, if needed
 *
 *
 * "C" is an optional combiner.
 *
 */

#include "gus.h"
#if NGUS > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/cpufunc.h>
#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <i386/isa/icu.h>

#include <dev/ic/ics2101reg.h>
#include <dev/ic/cs4231reg.h>
#include <dev/ic/ad1848reg.h>
#include <dev/isa/ics2101var.h>
#include <dev/isa/ad1848var.h>
#include "gusreg.h"

/*
 * Software state of a single "voice" on the GUS
 */

struct gus_voice {

	/*
	 * Various control bits
	 */

	unsigned char voccntl;	/* State of voice control register */
	unsigned char volcntl;	/* State of volume control register */
	unsigned char pan_pos;	/* Position of volume panning (4 bits) */
	int rate;		/* Sample rate of voice being played back */

	/*
	 * Address of the voice data into the GUS's DRAM.  20 bits each
	 */

	u_long start_addr;	/* Starting address of voice data loop area */
	u_long end_addr;	/* Ending address of voice data loop */
	u_long current_addr;	/* Beginning address of voice data
				   (start playing here) */

	/*
	 * linear volume values for the GUS's volume ramp.  0-511 (9 bits).
	 * These values must be translated into the logarithmic values using
	 * gus_log_volumes[]
	 */

	int start_volume;	/* Starting position of volume ramp */
	int current_volume;	/* Current position of volume on volume ramp */
	int end_volume;		/* Ending position of volume on volume ramp */
};

/*
 * Software state of GUS
 */

struct gus_softc {
	struct device sc_dev;		/* base device */
	struct isadev sc_id;		/* ISA device */
	void *sc_ih;			/* interrupt vector */

	int sc_iobase;			/* I/O base address */
	int sc_irq;			/* IRQ used */
	int sc_drq;			/* DMA channel for play */
	int sc_recdrq;			/* DMA channel for recording */

	int sc_flags;			/* Various flags about the GUS */
#define GUS_MIXER_INSTALLED	0x01	/* An ICS mixer is installed */
#define GUS_LOCKED		0x02	/* GUS is busy doing multi-phase DMA */
#define GUS_CODEC_INSTALLED	0x04	/* CS4231 installed/MAX */
#define GUS_PLAYING		0x08	/* GUS is playing a voice */
#define GUS_DMAOUT_ACTIVE	0x10	/* GUS is busy doing audio DMA */
#define GUS_DMAIN_ACTIVE	0x20	/* GUS is busy sampling  */
#define GUS_OPEN		0x100	/* GUS is open */
	int sc_dsize;			/* Size of GUS DRAM */
	int sc_voices;			/* Number of active voices */
	u_char sc_revision;		/* Board revision of GUS */
	u_char sc_mixcontrol;		/* Value of GUS_MIX_CONTROL register */

	u_long sc_orate;		/* Output sampling rate */
	u_long sc_irate;		/* Input sampling rate */

	int sc_encoding;		/* Current data encoding type */
	int sc_precision;		/* # of bits of precision */
	int sc_channels;		/* Number of active channels */
	int sc_blocksize;		/* Current blocksize */
	int sc_chanblocksize;		/* Current blocksize for each in-use
					   channel */
	short sc_nbufs;			/* how many on-GUS bufs per-channel */
	short sc_bufcnt;		/* how many need to be played */
	void *sc_deintr_buf;		/* deinterleave buffer for stereo */

	int sc_ogain;			/* Output gain control */
	u_char sc_out_port;		/* Current out port (generic only) */
	u_char sc_in_port;		/* keep track of it when no codec */

	void (*sc_dmaoutintr) __P((void*)); /* DMA completion intr handler */
	void *sc_outarg;		/* argument for sc_dmaoutintr() */
	u_char *sc_dmaoutaddr;		/* for isa_dmadone */
	u_long sc_gusaddr;		/* where did we just put it? */
	int sc_dmaoutcnt;		/* for isa_dmadone */

	void (*sc_dmainintr) __P((void*)); /* DMA completion intr handler */
	void *sc_inarg;			/* argument for sc_dmaoutintr() */
	u_char *sc_dmainaddr;		/* for isa_dmadone */
	int sc_dmaincnt;		/* for isa_dmadone */

	struct stereo_dma_intr {
		void (*intr)__P((void *));
		void *arg;
		u_char *buffer;
		u_long dmabuf;
		int size;
		int flags;
	} sc_stereo;

	/*
	 * State information for linear audio layer
	 */

	int sc_dmabuf;			/* Which ring buffer we're DMA'ing to */
	int sc_playbuf;			/* Which ring buffer we're playing */

	/*
	 * Voice information array.  All voice-specific information is stored
	 * here
	 */

	struct gus_voice sc_voc[32];	/* Voice data for each voice */
	union {
		struct ics2101_softc sc_mixer_u;
		struct ad1848_softc sc_codec_u;
	} u;
#define sc_mixer u.sc_mixer_u
#define sc_codec u.sc_codec_u
};

struct ics2101_volume {
	u_char left;
	u_char right;
};

#define HAS_CODEC(sc) ((sc)->sc_flags & GUS_CODEC_INSTALLED)
#define HAS_MIXER(sc) ((sc)->sc_flags & GUS_MIXER_INSTALLED)

/*
 * Mixer devices for ICS2101
 */
/* MIC IN mute, line in mute, line out mute are first since they can be done
   even if no ICS mixer. */
#define GUSICS_MIC_IN_MUTE		0
#define GUSICS_LINE_IN_MUTE		1
#define GUSICS_MASTER_MUTE		2
#define GUSICS_CD_MUTE			3
#define GUSICS_DAC_MUTE			4
#define GUSICS_MIC_IN_LVL		5
#define GUSICS_LINE_IN_LVL		6
#define GUSICS_CD_LVL			7
#define GUSICS_DAC_LVL			8
#define GUSICS_MASTER_LVL		9

#define GUSICS_RECORD_SOURCE		10

/* Classes */
#define GUSICS_INPUT_CLASS		11
#define GUSICS_OUTPUT_CLASS		12
#define GUSICS_RECORD_CLASS		13

/*
 * Mixer & MUX devices for CS4231
 */
#define GUSMAX_MIX_IN			0 /* input to MUX from mixer output */
#define GUSMAX_MONO_LVL			1 /* mic input to MUX;
					     also mono mixer input */
#define GUSMAX_DAC_LVL			2 /* input to MUX; also mixer input */
#define GUSMAX_LINE_IN_LVL		3 /* input to MUX; also mixer input */
#define GUSMAX_CD_LVL			4 /* mixer input only */
#define GUSMAX_MONITOR_LVL		5 /* digital mix (?) */
#define GUSMAX_OUT_LVL			6 /* output level. (?) */
#define GUSMAX_SPEAKER_LVL		7 /* pseudo-device for mute */
#define GUSMAX_LINE_IN_MUTE		8 /* pre-mixer */
#define GUSMAX_DAC_MUTE			9 /* pre-mixer */
#define GUSMAX_CD_MUTE			10 /* pre-mixer */
#define GUSMAX_MONO_MUTE		11 /* pre-mixer--microphone/mono */
#define GUSMAX_MONITOR_MUTE		12 /* post-mixer level/mute */
#define GUSMAX_SPEAKER_MUTE		13 /* speaker mute */

#define GUSMAX_REC_LVL			14 /* post-MUX gain */

#define GUSMAX_RECORD_SOURCE		15

/* Classes */
#define GUSMAX_INPUT_CLASS		16
#define GUSMAX_RECORD_CLASS		17
#define GUSMAX_MONITOR_CLASS		18
#define GUSMAX_OUTPUT_CLASS		19

#ifdef AUDIO_DEBUG
#define GUSPLAYDEBUG	/*XXX*/
extern void Dprintf __P((const char *, ...));
#define DPRINTF(x)	if (gusdebug) Dprintf x
#define DMAPRINTF(x)	if (gusdmadebug) Dprintf x
int	gusdebug = 0;
int	gusdmadebug = 0;
#else
#define DPRINTF(x)
#define DMAPRINTF(x)
#endif
int	gus_dostereo = 1;

#define NDMARECS 2048
#ifdef GUSPLAYDEBUG
int	gusstats = 0;
struct dma_record {
    struct timeval tv;
    u_long gusaddr;
    caddr_t bsdaddr;
    u_short count;
    u_char channel;
    u_char direction;
} dmarecords[NDMARECS];

int dmarecord_index = 0;
#endif

/*
 * local routines
 */

int	gusopen __P((dev_t, int));
void	gusclose __P((void *));
void	gusmax_close __P((void *));
int	gusprobe ()/*__P((struct device *, struct device *, void *))*/;
void	gusattach __P((struct device *, struct device *, void *));
int	gusintr __P((void *));
int	gus_set_in_gain __P((caddr_t, u_int, u_char));
int	gus_get_in_gain __P((caddr_t));
int	gus_set_out_gain __P((caddr_t, u_int, u_char));
int	gus_get_out_gain __P((caddr_t));
int 	gus_set_in_sr __P((void *, u_long));
u_long 	gus_get_in_sr __P((void *));
int 	gusmax_set_in_sr __P((void *, u_long));
u_long 	gusmax_get_in_sr __P((void *));
int 	gus_set_out_sr __P((void *, u_long));
u_long 	gus_get_out_sr __P((void *));
int 	gusmax_set_out_sr __P((void *, u_long));
u_long 	gusmax_get_out_sr __P((void *));
int	gus_set_encoding __P((void *, u_int));
int	gus_get_encoding __P((void *));
int	gusmax_set_encoding __P((void *, u_int));
int	gusmax_get_encoding __P((void *));
int	gus_set_precision __P((void *, u_int));
int	gus_get_precision __P((void *));
int	gusmax_set_precision __P((void *, u_int));
int	gusmax_get_precision __P((void *));
int	gus_set_channels __P((void *, int));
int	gus_get_channels __P((void *));
int	gusmax_set_channels __P((void *, int));
int	gusmax_get_channels __P((void *));
int	gus_round_blocksize __P((void *, int));
int	gus_set_out_port __P((void *, int));
int	gus_get_out_port __P((void *));
int	gus_set_in_port __P((void *, int));
int	gus_get_in_port __P((void *));
int	gus_commit_settings __P((void *));
int	gus_dma_output __P((void *, void *, int, void (*)(), void *));
int	gus_dma_input __P((void *, void *, int, void (*)(), void *));
int	gus_halt_out_dma __P((void *));
int	gus_halt_in_dma __P((void *));
int	gus_cont_out_dma __P((void *));
int	gus_cont_in_dma __P((void *));
int	gus_speaker_ctl __P((void *, int));
int	gusmax_set_precision __P((void *, u_int));
int	gusmax_get_precision __P((void *));
int	gusmax_round_blocksize __P((void *, int));
int	gusmax_commit_settings __P((void *));
int	gusmax_dma_output __P((void *, void *, int, void (*)(), void *));
int	gusmax_dma_input __P((void *, void *, int, void (*)(), void *));
int	gusmax_halt_out_dma __P((void *));
int	gusmax_halt_in_dma __P((void *));
int	gusmax_cont_out_dma __P((void *));
int	gusmax_cont_in_dma __P((void *));
int	gusmax_speaker_ctl __P((void *, int));
int	gusmax_set_out_port __P((void *, int));
int	gusmax_get_out_port __P((void *));
int	gusmax_set_in_port __P((void *, int));
int	gusmax_get_in_port __P((void *));
int	gus_getdev __P((void *, struct audio_device *));

static void	gus_deinterleave __P((struct gus_softc *, void *, int));
static void	gus_expand __P((void *, int, u_char *, int));
static void	gusmax_expand __P((void *, int, u_char *, int));

static int	gus_mic_ctl __P((void *, int));
static int	gus_linein_ctl __P((void *, int));
static int	gus_test_iobase __P((int));
static void	guspoke __P((int, long, u_char));
static void	gusdmaout __P((struct gus_softc *, int, u_long, caddr_t, int));
static void	gus_init_cs4231 __P((struct gus_softc *));
static void	gus_init_ics2101 __P((struct gus_softc *));

static void	gus_set_chan_addrs __P((struct gus_softc *));
static void	gusreset __P((struct gus_softc *, int));
static void	gus_set_voices __P((struct gus_softc *, int));
static void	gus_set_volume __P((struct gus_softc *, int, int));
static void	gus_set_samprate __P((struct gus_softc *, int, int));
static void	gus_set_recrate __P((struct gus_softc *, u_long));
static void	gus_start_voice __P((struct gus_softc *, int, int)),
		gus_stop_voice __P((struct gus_softc *, int, int)),
		gus_set_endaddr __P((struct gus_softc *, int, u_long)),
		gus_set_curaddr __P((struct gus_softc *, int, u_long));
static u_long	gus_get_curaddr __P((struct gus_softc *, int));
static int	gus_dmaout_intr __P((struct gus_softc *));
static void	gus_dmaout_dointr __P((struct gus_softc *));
static void	gus_dmaout_timeout __P((void *));
static int	gus_dmain_intr __P((struct gus_softc *));
static int	gus_voice_intr __P((struct gus_softc *));
static void	gus_start_playing __P((struct gus_softc *, int));
static void	gus_continue_playing __P((struct gus_softc *, int));
static u_char guspeek __P((int, u_long));
static unsigned long convert_to_16bit();
static int	gus_setfd __P((void *, int));
static int	gus_mixer_set_port __P((void *, mixer_ctrl_t *));
static int	gus_mixer_get_port __P((void *, mixer_ctrl_t *));
static int	gusmax_mixer_set_port __P((void *, mixer_ctrl_t *));
static int	gusmax_mixer_get_port __P((void *, mixer_ctrl_t *));
static int	gus_mixer_query_devinfo __P((void *, mixer_devinfo_t *));
static int	gusmax_mixer_query_devinfo __P((void *, mixer_devinfo_t *));
static int	gus_query_encoding __P((void *, struct audio_encoding *));

static void	gusics_master_mute __P((struct ics2101_softc *, int));
static void	gusics_dac_mute __P((struct ics2101_softc *, int));
static void	gusics_mic_mute __P((struct ics2101_softc *, int));
static void	gusics_linein_mute __P((struct ics2101_softc *, int));
static void	gusics_cd_mute __P((struct ics2101_softc *, int));

/*
 * ISA bus driver routines
 */

struct cfdriver guscd = {
	NULL, "gus", gusprobe, gusattach, DV_DULL, sizeof(struct gus_softc)
};


/*
 * A mapping from IRQ/DRQ values to the values used in the GUS's internal
 * registers.  A zero means that the referenced IRQ/DRQ is invalid
 */

static int gus_irq_map[] = {
	IRQUNK, IRQUNK, 1, 3, IRQUNK, 2, IRQUNK, 4, IRQUNK, 1, IRQUNK, 5, 6,
	IRQUNK, IRQUNK, 7
};
static int gus_drq_map[] = {
	DRQUNK, 1, DRQUNK, 2, DRQUNK, 3, 4, 5
};

/*
 * A list of valid base addresses for the GUS
 */

static int gus_base_addrs[] = {
	0x210, 0x220, 0x230, 0x240, 0x250, 0x260
};
static int gus_addrs = sizeof(gus_base_addrs) / sizeof(gus_base_addrs[0]);

/*
 * Maximum frequency values of the GUS based on the number of currently active
 * voices.  Since the GUS samples a voice every 1.6 us, the maximum frequency
 * is dependent on the number of active voices.  Yes, it is pretty weird.
 */

static int gus_max_frequency[] = {
		44100,		/* 14 voices */
		41160,		/* 15 voices */
		38587,		/* 16 voices */
		36317,		/* 17 voices */
		34300,		/* 18 voices */
		32494,		/* 19 voices */
		30870,		/* 20 voices */
		29400,		/* 21 voices */
		28063,		/* 22 voices */
		26843,		/* 23 voices */
		25725,		/* 24 voices */
		24696,		/* 25 voices */
		23746,		/* 26 voices */
		22866,		/* 27 voices */
		22050,		/* 28 voices */
		21289,		/* 29 voices */
		20580,		/* 30 voices */
		19916,		/* 31 voices */
		19293		/* 32 voices */
};
/*
 * A mapping of linear volume levels to the logarithmic volume values used
 * by the GF1 chip on the GUS.  From GUS SDK vol1.c.
 */

static unsigned short gus_log_volumes[512] = {
 0x0000,
 0x0700, 0x07ff, 0x0880, 0x08ff, 0x0940, 0x0980, 0x09c0, 0x09ff, 0x0a20,
 0x0a40, 0x0a60, 0x0a80, 0x0aa0, 0x0ac0, 0x0ae0, 0x0aff, 0x0b10, 0x0b20,
 0x0b30, 0x0b40, 0x0b50, 0x0b60, 0x0b70, 0x0b80, 0x0b90, 0x0ba0, 0x0bb0,
 0x0bc0, 0x0bd0, 0x0be0, 0x0bf0, 0x0bff, 0x0c08, 0x0c10, 0x0c18, 0x0c20,
 0x0c28, 0x0c30, 0x0c38, 0x0c40, 0x0c48, 0x0c50, 0x0c58, 0x0c60, 0x0c68,
 0x0c70, 0x0c78, 0x0c80, 0x0c88, 0x0c90, 0x0c98, 0x0ca0, 0x0ca8, 0x0cb0,
 0x0cb8, 0x0cc0, 0x0cc8, 0x0cd0, 0x0cd8, 0x0ce0, 0x0ce8, 0x0cf0, 0x0cf8,
 0x0cff, 0x0d04, 0x0d08, 0x0d0c, 0x0d10, 0x0d14, 0x0d18, 0x0d1c, 0x0d20,
 0x0d24, 0x0d28, 0x0d2c, 0x0d30, 0x0d34, 0x0d38, 0x0d3c, 0x0d40, 0x0d44,
 0x0d48, 0x0d4c, 0x0d50, 0x0d54, 0x0d58, 0x0d5c, 0x0d60, 0x0d64, 0x0d68,
 0x0d6c, 0x0d70, 0x0d74, 0x0d78, 0x0d7c, 0x0d80, 0x0d84, 0x0d88, 0x0d8c,
 0x0d90, 0x0d94, 0x0d98, 0x0d9c, 0x0da0, 0x0da4, 0x0da8, 0x0dac, 0x0db0,
 0x0db4, 0x0db8, 0x0dbc, 0x0dc0, 0x0dc4, 0x0dc8, 0x0dcc, 0x0dd0, 0x0dd4,
 0x0dd8, 0x0ddc, 0x0de0, 0x0de4, 0x0de8, 0x0dec, 0x0df0, 0x0df4, 0x0df8,
 0x0dfc, 0x0dff, 0x0e02, 0x0e04, 0x0e06, 0x0e08, 0x0e0a, 0x0e0c, 0x0e0e,
 0x0e10, 0x0e12, 0x0e14, 0x0e16, 0x0e18, 0x0e1a, 0x0e1c, 0x0e1e, 0x0e20,
 0x0e22, 0x0e24, 0x0e26, 0x0e28, 0x0e2a, 0x0e2c, 0x0e2e, 0x0e30, 0x0e32,
 0x0e34, 0x0e36, 0x0e38, 0x0e3a, 0x0e3c, 0x0e3e, 0x0e40, 0x0e42, 0x0e44,
 0x0e46, 0x0e48, 0x0e4a, 0x0e4c, 0x0e4e, 0x0e50, 0x0e52, 0x0e54, 0x0e56,
 0x0e58, 0x0e5a, 0x0e5c, 0x0e5e, 0x0e60, 0x0e62, 0x0e64, 0x0e66, 0x0e68,
 0x0e6a, 0x0e6c, 0x0e6e, 0x0e70, 0x0e72, 0x0e74, 0x0e76, 0x0e78, 0x0e7a,
 0x0e7c, 0x0e7e, 0x0e80, 0x0e82, 0x0e84, 0x0e86, 0x0e88, 0x0e8a, 0x0e8c,
 0x0e8e, 0x0e90, 0x0e92, 0x0e94, 0x0e96, 0x0e98, 0x0e9a, 0x0e9c, 0x0e9e,
 0x0ea0, 0x0ea2, 0x0ea4, 0x0ea6, 0x0ea8, 0x0eaa, 0x0eac, 0x0eae, 0x0eb0,
 0x0eb2, 0x0eb4, 0x0eb6, 0x0eb8, 0x0eba, 0x0ebc, 0x0ebe, 0x0ec0, 0x0ec2,
 0x0ec4, 0x0ec6, 0x0ec8, 0x0eca, 0x0ecc, 0x0ece, 0x0ed0, 0x0ed2, 0x0ed4,
 0x0ed6, 0x0ed8, 0x0eda, 0x0edc, 0x0ede, 0x0ee0, 0x0ee2, 0x0ee4, 0x0ee6,
 0x0ee8, 0x0eea, 0x0eec, 0x0eee, 0x0ef0, 0x0ef2, 0x0ef4, 0x0ef6, 0x0ef8,
 0x0efa, 0x0efc, 0x0efe, 0x0eff, 0x0f01, 0x0f02, 0x0f03, 0x0f04, 0x0f05,
 0x0f06, 0x0f07, 0x0f08, 0x0f09, 0x0f0a, 0x0f0b, 0x0f0c, 0x0f0d, 0x0f0e,
 0x0f0f, 0x0f10, 0x0f11, 0x0f12, 0x0f13, 0x0f14, 0x0f15, 0x0f16, 0x0f17,
 0x0f18, 0x0f19, 0x0f1a, 0x0f1b, 0x0f1c, 0x0f1d, 0x0f1e, 0x0f1f, 0x0f20,
 0x0f21, 0x0f22, 0x0f23, 0x0f24, 0x0f25, 0x0f26, 0x0f27, 0x0f28, 0x0f29,
 0x0f2a, 0x0f2b, 0x0f2c, 0x0f2d, 0x0f2e, 0x0f2f, 0x0f30, 0x0f31, 0x0f32,
 0x0f33, 0x0f34, 0x0f35, 0x0f36, 0x0f37, 0x0f38, 0x0f39, 0x0f3a, 0x0f3b,
 0x0f3c, 0x0f3d, 0x0f3e, 0x0f3f, 0x0f40, 0x0f41, 0x0f42, 0x0f43, 0x0f44,
 0x0f45, 0x0f46, 0x0f47, 0x0f48, 0x0f49, 0x0f4a, 0x0f4b, 0x0f4c, 0x0f4d,
 0x0f4e, 0x0f4f, 0x0f50, 0x0f51, 0x0f52, 0x0f53, 0x0f54, 0x0f55, 0x0f56,
 0x0f57, 0x0f58, 0x0f59, 0x0f5a, 0x0f5b, 0x0f5c, 0x0f5d, 0x0f5e, 0x0f5f,
 0x0f60, 0x0f61, 0x0f62, 0x0f63, 0x0f64, 0x0f65, 0x0f66, 0x0f67, 0x0f68,
 0x0f69, 0x0f6a, 0x0f6b, 0x0f6c, 0x0f6d, 0x0f6e, 0x0f6f, 0x0f70, 0x0f71,
 0x0f72, 0x0f73, 0x0f74, 0x0f75, 0x0f76, 0x0f77, 0x0f78, 0x0f79, 0x0f7a,
 0x0f7b, 0x0f7c, 0x0f7d, 0x0f7e, 0x0f7f, 0x0f80, 0x0f81, 0x0f82, 0x0f83,
 0x0f84, 0x0f85, 0x0f86, 0x0f87, 0x0f88, 0x0f89, 0x0f8a, 0x0f8b, 0x0f8c,
 0x0f8d, 0x0f8e, 0x0f8f, 0x0f90, 0x0f91, 0x0f92, 0x0f93, 0x0f94, 0x0f95,
 0x0f96, 0x0f97, 0x0f98, 0x0f99, 0x0f9a, 0x0f9b, 0x0f9c, 0x0f9d, 0x0f9e,
 0x0f9f, 0x0fa0, 0x0fa1, 0x0fa2, 0x0fa3, 0x0fa4, 0x0fa5, 0x0fa6, 0x0fa7,
 0x0fa8, 0x0fa9, 0x0faa, 0x0fab, 0x0fac, 0x0fad, 0x0fae, 0x0faf, 0x0fb0,
 0x0fb1, 0x0fb2, 0x0fb3, 0x0fb4, 0x0fb5, 0x0fb6, 0x0fb7, 0x0fb8, 0x0fb9,
 0x0fba, 0x0fbb, 0x0fbc, 0x0fbd, 0x0fbe, 0x0fbf, 0x0fc0, 0x0fc1, 0x0fc2,
 0x0fc3, 0x0fc4, 0x0fc5, 0x0fc6, 0x0fc7, 0x0fc8, 0x0fc9, 0x0fca, 0x0fcb,
 0x0fcc, 0x0fcd, 0x0fce, 0x0fcf, 0x0fd0, 0x0fd1, 0x0fd2, 0x0fd3, 0x0fd4,
 0x0fd5, 0x0fd6, 0x0fd7, 0x0fd8, 0x0fd9, 0x0fda, 0x0fdb, 0x0fdc, 0x0fdd,
 0x0fde, 0x0fdf, 0x0fe0, 0x0fe1, 0x0fe2, 0x0fe3, 0x0fe4, 0x0fe5, 0x0fe6,
 0x0fe7, 0x0fe8, 0x0fe9, 0x0fea, 0x0feb, 0x0fec, 0x0fed, 0x0fee, 0x0fef,
 0x0ff0, 0x0ff1, 0x0ff2, 0x0ff3, 0x0ff4, 0x0ff5, 0x0ff6, 0x0ff7, 0x0ff8,
 0x0ff9, 0x0ffa, 0x0ffb, 0x0ffc, 0x0ffd, 0x0ffe, 0x0fff};

#define SELECT_GUS_REG(port,x) outb(port+GUS_REG_SELECT,x)
#define WHICH_GUS_REG(port) inb(port+GUS_REG_SELECT)
#define ADDR_HIGH(x) (unsigned int) ((x >> 7L) & 0x1fffL)
#define ADDR_LOW(x) (unsigned int) ((x & 0x7fL) << 9L)

#define GUS_MIN_VOICES 14	/* Minimum possible number of voices */
#define GUS_MAX_VOICES 32	/* Maximum possible number of voices */
#define GUS_VOICE_LEFT 0	/* Voice used for left (and mono) playback */
#define GUS_VOICE_RIGHT 1	/* Voice used for right playback */
#define GUS_MEM_OFFSET 32	/* Offset into GUS memory to begin of buffer */
#define GUS_BUFFER_MULTIPLE 1024	/* Audio buffers are multiples of this */
#define	GUS_MEM_FOR_BUFFERS	131072	/* use this many bytes on-GUS */
#define	GUS_LEFT_RIGHT_OFFSET	(sc->sc_nbufs * sc->sc_chanblocksize + GUS_MEM_OFFSET)

#define GUS_PREC_BYTES (sc->sc_precision >> 3) /* precision to bytes */

/* splgus() must be splaudio() */

#define splgus splaudio

/*
 * Interface to higher level audio driver
 */

struct audio_hw_if gus_hw_if = {
	gusopen,
	gusclose,
	NULL,				/* drain */
	gus_set_in_sr,
	gus_get_in_sr,
	gus_set_out_sr,
	gus_get_out_sr,

	gus_query_encoding,
	gus_set_encoding,
	gus_get_encoding,

	gus_set_precision,
	gus_get_precision,

	gus_set_channels,
	gus_get_channels,

	gus_round_blocksize,

	gus_set_out_port,
	gus_get_out_port,
	gus_set_in_port,
	gus_get_in_port,

	gus_commit_settings,

	ad1848_get_silence,

	gus_expand,
	mulaw_compress,

	gus_dma_output,
	gus_dma_input,
	gus_halt_out_dma,
	gus_halt_in_dma,
	gus_cont_out_dma,
	gus_cont_in_dma,

	gus_speaker_ctl,

	gus_getdev,
	gus_setfd,
	gus_mixer_set_port,
	gus_mixer_get_port,
	gus_mixer_query_devinfo,
	1,				/* full-duplex */
	0,
};


/*
 * Some info about the current audio device
 */

struct audio_device gus_device = {
	"UltraSound",
	"",
	"gus",
};

#define FLIP_REV	5		/* This rev has flipped mixer chans */


int
gusprobe(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct gus_softc *sc = (void *) self;
	register struct isa_attach_args *ia = aux;
	struct cfdata *cf = sc->sc_dev.dv_cfdata;
	register int iobase = ia->ia_iobase;
	int recdrq = cf->cf_flags;

	int i;
	unsigned char s1, s2;

	/*
	 * Before we do anything else, make sure requested IRQ and DRQ are
	 * valid for this card.
	 */

	if (gus_irq_map[ia->ia_irq] == IRQUNK) {
		printf("gus: invalid irq %d, card not probed\n", ia->ia_irq);
		return(0);
	}

	if (gus_drq_map[ia->ia_drq] == DRQUNK) {
		printf("gus: invalid drq %d, card not probed\n", ia->ia_drq);
		return(0);
	}

	if (recdrq != 0x00) {
		if (recdrq > 7 || gus_drq_map[recdrq] == DRQUNK) {
		   printf("gus: invalid flag given for second DMA channel (0x%x), card not probed\n", recdrq);
		   return(0);
	        }
	} else
		recdrq = ia->ia_drq;

	if (iobase == IOBASEUNK) {
		int i;
		for(i = 0; i < gus_addrs; i++)
			if (gus_test_iobase(gus_base_addrs[i])) {
				iobase = gus_base_addrs[i];
				goto done;
			}
		return 0;
	} else if (! gus_test_iobase(iobase))
			return 0;

done:
	sc->sc_iobase = iobase;
	sc->sc_irq = ia->ia_irq;
	sc->sc_drq = ia->ia_drq;
	sc->sc_recdrq = recdrq;

	ia->ia_iobase = sc->sc_iobase;
	ia->ia_iosize = 16;		/* XXX */
	return(1);
}

/*
 * Test to see if a particular I/O base is valid for the GUS.  Return true
 * if it is.
 */

static int
gus_test_iobase (int iobase)
{
	int i = splgus();
	u_char s1, s2;

	/*
	 * Reset GUS to an initial state before we do anything.
	 */

	delay(500);

 	SELECT_GUS_REG(iobase, GUSREG_RESET);
 	outb(iobase+GUS_DATA_HIGH, 0x00);

 	delay(500);

	SELECT_GUS_REG(iobase, GUSREG_RESET);
 	outb(iobase+GUS_DATA_HIGH, GUSMASK_MASTER_RESET);

 	delay(500);

	splx(i);

	/*
	 * See if we can write to the board's memory
	 */

 	s1 = guspeek(iobase, 0L);
 	s2 = guspeek(iobase, 1L);

 	guspoke(iobase, 0L, 0xaa);
 	guspoke(iobase, 1L, 0x55);

 	if ((i=(int)guspeek(iobase, 0L)) != 0xaa) {
		return(0);
	}

	guspoke(iobase, 0L, s1);
	guspoke(iobase, 1L, s2);

	return 1;
}

/*
 * Setup the GUS for use; called shortly after probe
 */

void
gusattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct gus_softc *sc = (void *) self;
	register struct isa_attach_args *ia = aux;
	register int port = ia->ia_iobase;
	int		s,i;
	register unsigned char	c,d,m;

	/*
	 * Figure out our board rev, and see if we need to initialize the
	 * mixer
	 */

 	delay(500);

 	c = inb(port+GUS_BOARD_REV);
	if (c != 0xff)
		sc->sc_revision = c;
	else
		sc->sc_revision = 0;


 	SELECT_GUS_REG(port, GUSREG_RESET);
 	outb(port+GUS_DATA_HIGH, 0x00);

	gusreset(sc, GUS_MAX_VOICES); /* initialize all voices */
	gusreset(sc, GUS_MIN_VOICES); /* then set to just the ones we use */

	/*
	 * Setup the IRQ and DRQ lines in software, using values from
	 * config file
	 */

	m = GUSMASK_LINE_IN|GUSMASK_LINE_OUT;		/* disable all */

	c = ((unsigned char) gus_irq_map[ia->ia_irq]) | GUSMASK_BOTH_RQ;

	if (sc->sc_recdrq == sc->sc_drq)
		d = (unsigned char) (gus_drq_map[sc->sc_drq] |
				GUSMASK_BOTH_RQ);
	else
		d = (unsigned char) (gus_drq_map[sc->sc_drq] |
				gus_drq_map[sc->sc_recdrq] << 3);

	/*
	 * Program the IRQ and DMA channels on the GUS.  Note that we hardwire
	 * the GUS to only use one IRQ channel, but we give the user the
	 * option of using two DMA channels (the other one given by the flags
	 * option in the config file).  Two DMA channels are needed for full-
	 * duplex operation.
	 *
	 * The order of these operations is very magical.
	 */

	disable_intr();

	outb(port+GUS_REG_CONTROL, GUS_REG_IRQCTL);
	outb(port+GUS_MIX_CONTROL, m);
	outb(port+GUS_IRQCTL_CONTROL, 0x00);
	outb(port+0x0f, 0x00);

	outb(port+GUS_MIX_CONTROL, m);
	outb(port+GUS_DMA_CONTROL, d | 0x80); /* magic reset? */

	outb(port+GUS_MIX_CONTROL, m | GUSMASK_CONTROL_SEL);
	outb(port+GUS_IRQ_CONTROL, c);

	outb(port+GUS_MIX_CONTROL, m);
	outb(port+GUS_DMA_CONTROL, d);

	outb(port+GUS_MIX_CONTROL, m | GUSMASK_CONTROL_SEL);
	outb(port+GUS_IRQ_CONTROL, c);

	outb(port+GUS_VOICE_SELECT, 0x00);

	/* enable line in, line out.  leave mic disabled. */
	outb(port+GUS_MIX_CONTROL,
	     (m | GUSMASK_LATCHES) & ~(GUSMASK_LINE_OUT|GUSMASK_LINE_IN));
	outb(port+GUS_VOICE_SELECT, 0x00);

	enable_intr();

	sc->sc_mixcontrol =
		(m | GUSMASK_LATCHES) & ~(GUSMASK_LINE_OUT|GUSMASK_LINE_IN);


 	if (sc->sc_revision >= 5 && sc->sc_revision <= 9) {
 		sc->sc_flags |= GUS_MIXER_INSTALLED;
 		gus_init_ics2101(sc);
	}
	if (sc->sc_revision >= 0xa) {
		gus_init_cs4231(sc);
	}

 	SELECT_GUS_REG(port, GUSREG_RESET);
 	/*
 	 * Check to see how much memory we have on this card; see if any
 	 * "mirroring" occurs.  We're assuming at least 256K already exists
 	 * on the card; otherwise the initial probe would have failed
 	 */

	guspoke(port, 0L, 0x00);
	for(i = 1; i < 1024; i++) {
		unsigned long loc;
		unsigned char val;

		/*
		 * See if we've run into mirroring yet
		 */

		if (guspeek(port, 0L) != 0)
			break;

		loc = i << 10;

		guspoke(port, loc, 0xaa);
		if (guspeek(port, loc) != 0xaa)
			break;
	}

	sc->sc_dsize = i;
	sprintf(gus_device.version, "3.%d", sc->sc_revision);

	printf("\n <Gravis UltraSound version 3.%d, %dKB DRAM, ",
	       sc->sc_revision, sc->sc_dsize);
	if (HAS_MIXER(sc))
		printf("ICS2101 mixer, ");
	if (HAS_CODEC(sc))
		printf("%s codec/mixer, ", sc->sc_codec.chip_name);
	if (sc->sc_recdrq == sc->sc_drq) {
		printf("half-duplex");
		gus_hw_if.full_duplex = 0;
	} else {
		printf("full-duplex, record drq %d", sc->sc_recdrq);
		gus_hw_if.full_duplex = 1;
	}

	printf(">\n");

	/*
	 * Setup a default interrupt handler
	 */

	/* XXX we shouldn't have to use splgus == splclock, nor should
	 * we use ISA_IPL_CLOCK.
	 */
	sc->sc_ih = isa_intr_establish(ia->ia_irq, ISA_IST_EDGE, ISA_IPL_AUDIO,
				       gusintr, sc /* sc->sc_gusdsp */);

	/*
	 * Set some default values
	 */

	sc->sc_irate = sc->sc_orate = 44100;
	sc->sc_encoding = AUDIO_ENCODING_LINEAR;
	sc->sc_precision = 16;
	sc->sc_voc[GUS_VOICE_LEFT].voccntl |= GUSMASK_DATA_SIZE16;
	sc->sc_voc[GUS_VOICE_RIGHT].voccntl |= GUSMASK_DATA_SIZE16;
	sc->sc_channels = 1;
	sc->sc_ogain = 340;
	gus_commit_settings(sc);

	/*
	 * We always put the left channel full left & right channel
	 * full right.
	 * For mono playback, we set up both voices playing the same buffer.
	 */
	outb(sc->sc_iobase+GUS_VOICE_SELECT, (unsigned char) GUS_VOICE_LEFT);
	SELECT_GUS_REG(sc->sc_iobase, GUSREG_PAN_POS);
	outb(sc->sc_iobase+GUS_DATA_HIGH, GUS_PAN_FULL_LEFT);

	outb(sc->sc_iobase+GUS_VOICE_SELECT, (unsigned char) GUS_VOICE_RIGHT);
	SELECT_GUS_REG(sc->sc_iobase, GUSREG_PAN_POS);
	outb(sc->sc_iobase+GUS_DATA_HIGH, GUS_PAN_FULL_RIGHT);

	/*
	 * Attach to the generic audio layer
	 */

	if (audio_hardware_attach(&gus_hw_if, HAS_CODEC(sc) ? (void *)&sc->sc_codec : (void *)sc) != 0)
		printf("gus: could not attach to audio pseudo-device driver\n");
}

int
gusopen(dev, flags)
	dev_t dev;
	int flags;
{
	int unit = AUDIOUNIT(dev);
	struct gus_softc *sc;

	DPRINTF(("gusopen() called\n"));

	if (unit >= guscd.cd_ndevs)
		return ENXIO;
	sc = guscd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (sc->sc_flags & GUS_OPEN)
		return EBUSY;

	/*
	 * Some initialization
	 */

	sc->sc_flags |= GUS_OPEN;
	sc->sc_dmabuf = 0;
	sc->sc_playbuf = -1;
	sc->sc_bufcnt = 0;
	sc->sc_voc[GUS_VOICE_LEFT].start_addr = GUS_MEM_OFFSET - 1;
	sc->sc_voc[GUS_VOICE_LEFT].current_addr = GUS_MEM_OFFSET;

	if (HAS_CODEC(sc)) {
		ad1848_open(&sc->sc_codec, dev, flags);
		sc->sc_codec.aux1_mute = 0;
		ad1848_mute_aux1(&sc->sc_codec, 0); /* turn on DAC output */
		if (flags & FREAD) {
			sc->sc_codec.mono_mute = 0;
			cs4231_mute_mono(&sc->sc_codec, 0);
		}
	} else if (flags & FREAD) {
		/* enable/unmute the microphone */
		if (HAS_MIXER(sc)) {
			gusics_mic_mute(&sc->sc_mixer, 0);
		} else
			gus_mic_ctl(sc, SPKR_ON);
	}
	if (sc->sc_nbufs == 0)
	    gus_round_blocksize(sc, GUS_BUFFER_MULTIPLE); /* default blksiz */
	return 0;
}

static void
gusmax_expand(hdl, encoding, buf, count)
	void *hdl;
	int encoding;
	u_char *buf;
	int count;
{
	register struct ad1848_softc *ac = hdl;

	gus_expand(ac->parent, encoding, buf, count);
}

static void
gus_expand(hdl, encoding, buf, count)
	void *hdl;
	int encoding;
	u_char *buf;
	int count;
{
	struct gus_softc *sc = hdl;

	mulaw_expand(NULL, encoding, buf, count);
	/*
	 * If we need stereo deinterleaving, do it now.
	 */
	if (sc->sc_channels == 2)
		gus_deinterleave(sc, (void *)buf, count);
}

static void
gus_deinterleave(sc, buf, size)
	register struct gus_softc *sc;
	void *buf;
	int size;
{
	/* deinterleave the stereo data.  We can use sc->sc_deintr_buf
	   for scratch space. */
	register int i;

	/*
	 * size is in bytes.
	 */
	if (sc->sc_precision == 16) {
		register u_short *dei = sc->sc_deintr_buf;
		register u_short *sbuf = buf;
		size >>= 1;		/* bytecnt to shortcnt */
		/* copy 2nd of each pair of samples to the staging area, while
		   compacting the 1st of each pair into the original area. */
		for (i = 0; i < size/2-1; i++)  {
			dei[i] = sbuf[i*2+1];
			sbuf[i+1] = sbuf[i*2+2];
		}
		/*
		 * this has copied one less sample than half of the
		 * buffer.  The first sample of the 1st stream was
		 * already in place and didn't need copying.
		 * Therefore, we've moved all of the 1st stream's
		 * samples into place.  We have one sample from 2nd
		 * stream in the last slot of original area, not
		 * copied to the staging area (But we don't need to!).
		 * Copy the remainder of the original stream into place.
		 */
		bcopy(dei, &sbuf[size/2], i * sizeof(short));
	} else {
		register u_char *dei = sc->sc_deintr_buf;
		register u_char *sbuf = buf;
		for (i = 0; i < size/2-1; i++)  {
			dei[i] = sbuf[i*2+1];
			sbuf[i+1] = sbuf[i*2+2];
		}
		bcopy(dei, &sbuf[size/2], i);
	}
}

/*
 * Actually output a buffer to the DSP chip
 */

int
gusmax_dma_output(addr, buf, size, intr, arg)
	void * addr;
	void *buf;
	int size;
	void (*intr)();
	void *arg;
{
	register struct ad1848_softc *ac = addr;
	return gus_dma_output(ac->parent, buf, size, intr, arg);
}

/*
 * called at splgus() from interrupt handler.
 */
void
stereo_dmaintr(void *arg)
{
    struct gus_softc *sc = arg;
    struct stereo_dma_intr *sa = &sc->sc_stereo;

    DMAPRINTF(("stereo_dmaintr"));

    /*
     * Put other half in its place, then call the real interrupt routine :)
     */

    sc->sc_dmaoutintr = sa->intr;
    sc->sc_outarg = sa->arg;

#ifdef GUSPLAYDEBUG
    if (gusstats) {
      microtime(&dmarecords[dmarecord_index].tv);
      dmarecords[dmarecord_index].gusaddr = sa->dmabuf;
      dmarecords[dmarecord_index].bsdaddr = sa->buffer;
      dmarecords[dmarecord_index].count = sa->size;
      dmarecords[dmarecord_index].channel = 1;
      dmarecords[dmarecord_index].direction = 1;
      dmarecord_index = ++dmarecord_index % NDMARECS;
    }
#endif

    gusdmaout(sc, sa->flags, sa->dmabuf, (caddr_t) sa->buffer, sa->size);

    sa->flags = 0;
    sa->dmabuf = 0;
    sa->buffer = 0;
    sa->size = 0;
    sa->intr = 0;
    sa->arg = 0;
}

/*
 * Start up DMA output to the card.
 * Called at splgus/splaudio already, either from intr handler or from
 * generic audio code.
 */
int
gus_dma_output(addr, buf, size, intr, arg)
	void * addr;
	void *buf;
	int size;
	void (*intr)();
	void *arg;
{
	struct gus_softc *sc = addr;
	u_char *buffer = buf;
	u_long boarddma;
	int i, flags;

	DMAPRINTF(("gus_dma_output %d @ %x\n", size, buf));

	if (size != sc->sc_blocksize) {
	    DPRINTF(("gus_dma_output reqsize %d not sc_blocksize %d\n",
		     size, sc->sc_blocksize));
	    return EINVAL;
	}

	flags = GUSMASK_DMA_WRITE;
	if (sc->sc_precision == 16)
	    flags |= GUSMASK_DMA_DATA_SIZE; 
	/* pcm16 is signed, mulaw & pcm8 are unsigned */
	if (sc->sc_encoding == AUDIO_ENCODING_ULAW ||
	    sc->sc_encoding == AUDIO_ENCODING_PCM8)
	    flags |= GUSMASK_DMA_INVBIT;

	if (sc->sc_channels == 2) {
		if (sc->sc_precision == 16) {
			if (size & 3) {
				DPRINTF(("gus_dma_output: unpaired 16bit samples"));
				size &= 3;
			}
		} else if (size & 1) {
			DPRINTF(("gus_dma_output: unpaired samples"));
			size &= 1;
		}
		if (size == 0)
			return 0;
		size >>= 1;

		boarddma = size * sc->sc_dmabuf + GUS_MEM_OFFSET;

		sc->sc_stereo.intr = intr;
		sc->sc_stereo.arg = arg;
		sc->sc_stereo.size = size;
		sc->sc_stereo.dmabuf = boarddma + GUS_LEFT_RIGHT_OFFSET;
		sc->sc_stereo.buffer = buffer + size;
		sc->sc_stereo.flags = flags;
		if (gus_dostereo) {
		  intr = stereo_dmaintr;
		  arg = sc;
		}
	} else
		boarddma = size * sc->sc_dmabuf + GUS_MEM_OFFSET;


	sc->sc_flags |= GUS_LOCKED;
	sc->sc_dmaoutintr = intr;
	sc->sc_outarg = arg;

#ifdef GUSPLAYDEBUG
	if (gusstats) {
	  microtime(&dmarecords[dmarecord_index].tv);
	  dmarecords[dmarecord_index].gusaddr = boarddma;
	  dmarecords[dmarecord_index].bsdaddr = buffer;
	  dmarecords[dmarecord_index].count = size;
	  dmarecords[dmarecord_index].channel = 0;
	  dmarecords[dmarecord_index].direction = 1;
	  dmarecord_index = ++dmarecord_index % NDMARECS;
	}
#endif

	gusdmaout(sc, flags, boarddma, (caddr_t) buffer, size);

	return 0;
}

void
gusmax_close(addr)
	void *addr;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
/*	ac->aux1_mute = 1;
	ad1848_mute_aux1(ac, 1);	/* turn off DAC output */
	ad1848_close(ac);
	gusclose(sc);
}

/*
 * Close out device stuff.  Called at splgus() from generic audio layer.
 */
void
gusclose(addr)
	void *addr;
{
	struct gus_softc *sc = addr;

        DPRINTF(("gus_close: sc=0x%x\n", sc));


/*	if (sc->sc_flags & GUS_DMAOUT_ACTIVE) */ {
		gus_halt_out_dma(sc);
	}
/*	if (sc->sc_flags & GUS_DMAIN_ACTIVE) */ {
		gus_halt_in_dma(sc);
	}
	sc->sc_flags &= ~(GUS_OPEN|GUS_LOCKED|GUS_DMAOUT_ACTIVE|GUS_DMAIN_ACTIVE);

	if (sc->sc_deintr_buf) {
		FREE(sc->sc_deintr_buf, M_DEVBUF);
		sc->sc_deintr_buf = NULL;
	}
	/* turn off speaker, etc. */

	/* make sure the voices shut up: */
	gus_stop_voice(sc, GUS_VOICE_LEFT, 1);
	gus_stop_voice(sc, GUS_VOICE_RIGHT, 0);
}

/*
 * Service interrupts.  Farm them off to helper routines if we are using the
 * GUS for simple playback/record
 */

#ifdef DIAGNOSTIC
int gusintrcnt;
int gusdmaintrcnt;
int gusvocintrcnt;
#endif

int
gusintr(arg)
	void *arg;
{
	register struct gus_softc *sc = arg;
	unsigned char intr;
	register int port = sc->sc_iobase;
	int retval = 0;

	DPRINTF(("gusintr\n"));
#ifdef DIAGNOSTIC
	gusintrcnt++;
#endif
	if (HAS_CODEC(sc))
		retval = ad1848_intr(&sc->sc_codec);
	if ((intr = inb(port+GUS_IRQ_STATUS)) & GUSMASK_IRQ_DMATC) {
		DMAPRINTF(("gusintr dma flags=%x\n", sc->sc_flags));
#ifdef DIAGNOSTIC
		gusdmaintrcnt++;
#endif
		retval += gus_dmaout_intr(sc);
		if (sc->sc_flags & GUS_DMAIN_ACTIVE) {
		    SELECT_GUS_REG(port, GUSREG_SAMPLE_CONTROL);
		    intr = inb(port+GUS_DATA_HIGH);
		    if (intr & GUSMASK_SAMPLE_DMATC) {
			retval += gus_dmain_intr(sc);
		    }
		}
	}
	if (intr & (GUSMASK_IRQ_VOICE | GUSMASK_IRQ_VOLUME)) {
		DMAPRINTF(("gusintr voice flags=%x\n", sc->sc_flags));
#ifdef DIAGNOSTIC
		gusvocintrcnt++;
#endif
		retval += gus_voice_intr(sc);
	}
	if (retval)
		return 1;
	return retval;
}

int gus_bufcnt[GUS_MEM_FOR_BUFFERS / GUS_BUFFER_MULTIPLE];
int gus_restart;				/* how many restarts? */
int gus_stops;				/* how many times did voice stop? */
int gus_falsestops;			/* stopped but not done? */
int gus_continues;

struct playcont {
	struct timeval tv;
	u_int playbuf;
	u_int dmabuf;
	u_char bufcnt;
	u_char vaction;
	u_char voccntl;
	u_char volcntl;
	u_long curaddr;
	u_long endaddr;
} playstats[NDMARECS];

int playcntr;

static void
gus_dmaout_timeout(arg)
     void *arg;
{
    register struct gus_softc *sc = arg;
    register int port = sc->sc_iobase;
    int s;

    printf("%s: dmaout timeout\n", sc->sc_dev.dv_xname);
    /*
     * Stop any DMA.
     */

    s = splgus();
    SELECT_GUS_REG(port, GUSREG_DMA_CONTROL);
    outb(sc->sc_iobase+GUS_DATA_HIGH, 0);

/*    isa_dmaabort(sc->sc_drq);		/* XXX we will dmadone below? */

    gus_dmaout_dointr(sc);
    splx(s);
}


/*
 * Service DMA interrupts.  This routine will only get called if we're doing
 * a DMA transfer for playback/record requests from the audio layer.
 */

static int
gus_dmaout_intr(sc)
	struct gus_softc *sc;
{
	register int port = sc->sc_iobase;

	/*
	 * If we got a DMA transfer complete from the GUS DRAM, then deal
	 * with it.
	 */

	SELECT_GUS_REG(port, GUSREG_DMA_CONTROL);
 	if (inb(port+GUS_DATA_HIGH) & GUSMASK_DMA_IRQPEND) {
	    untimeout(gus_dmaout_timeout, sc);
	    gus_dmaout_dointr(sc);
	    return 1;
	}
	return 0;
}

static void
gus_dmaout_dointr(sc)
	struct gus_softc *sc;
{
	register int port = sc->sc_iobase;

	/* sc->sc_dmaoutcnt - 1 because DMA controller counts from zero?. */
	isa_dmadone(B_WRITE,
		    sc->sc_dmaoutaddr,
		    sc->sc_dmaoutcnt - 1,
		    sc->sc_drq);
	sc->sc_flags &= ~GUS_DMAOUT_ACTIVE;  /* pending DMA is done */
	DMAPRINTF(("gus_dmaout_dointr %d @ %x\n", sc->sc_dmaoutcnt,
		   sc->sc_dmaoutaddr));

	/*
	 * to prevent clicking, we need to copy last sample
	 * from last buffer to scratch area just before beginning of
	 * buffer.  However, if we're doing formats that are converted by
	 * the card during the DMA process, we need to pick up the converted
	 * byte rather than the one we have in memory.
	 */
	if (sc->sc_dmabuf == sc->sc_nbufs - 1) {
	  register int i;
	  switch (sc->sc_encoding) {
	  case AUDIO_ENCODING_PCM16:
	    /* we have the native format */
	    for (i = 1; i <= 2; i++)
	      guspoke(port, sc->sc_gusaddr -
		      (sc->sc_nbufs - 1) * sc->sc_chanblocksize - i,
		      sc->sc_dmaoutaddr[sc->sc_dmaoutcnt-i]);
	    break;
	  case AUDIO_ENCODING_PCM8:
	  case AUDIO_ENCODING_ULAW:
	    /* we need to fetch the translated byte, then stuff it. */
	    guspoke(port, sc->sc_gusaddr -
		    (sc->sc_nbufs - 1) * sc->sc_chanblocksize - 1,
		    guspeek(port,
			    sc->sc_gusaddr + sc->sc_chanblocksize - 1));
	    break;
	  }
	}
	/*
	 * If this is the first half of stereo, "ignore" this one
	 * and copy out the second half.
	 */
	if (sc->sc_dmaoutintr == stereo_dmaintr) {
	    (*sc->sc_dmaoutintr)(sc->sc_outarg);
	    return;
	}
	/*
	 * If the voice is stopped, then start it.  Reset the loop
	 * and roll bits.  Call the audio layer routine, since if
	 * we're starting a stopped voice, that means that the next
	 * buffer can be filled
	 */

	sc->sc_flags &= ~GUS_LOCKED;
	if (sc->sc_voc[GUS_VOICE_LEFT].voccntl &
	    GUSMASK_VOICE_STOPPED) {
	    if (sc->sc_flags & GUS_PLAYING) {
		printf("%s: playing yet stopped?\n", sc->sc_dev.dv_xname);
	    }
	    sc->sc_bufcnt++; /* another yet to be played */
	    gus_start_playing(sc, sc->sc_dmabuf);
	    gus_restart++;
	} else {
	    /*
	     * set the sound action based on which buffer we
	     * just transferred.  If we just transferred buffer 0
	     * we want the sound to loop when it gets to the nth
	     * buffer; if we just transferred
	     * any other buffer, we want the sound to roll over
	     * at least one more time.  The voice interrupt
	     * handlers will take care of accounting &
	     * setting control bits if it's not caught up to us
	     * yet.
	     */
	    if (++sc->sc_bufcnt == 2) {
		/*
		 * XXX 
		 * If we're too slow in reaction here,
		 * the voice could be just approaching the
		 * end of its run.  It should be set to stop,
		 * so these adjustments might not DTRT.
		 */
		if (sc->sc_dmabuf == 0 &&
		    sc->sc_playbuf == sc->sc_nbufs - 1) {
		    /* player is just at the last buf, we're at the
		       first.  Turn on looping, turn off rolling. */
		    sc->sc_voc[GUS_VOICE_LEFT].voccntl |= GUSMASK_LOOP_ENABLE;
		    sc->sc_voc[GUS_VOICE_LEFT].volcntl &= ~GUSMASK_VOICE_ROLL;
		    playstats[playcntr].vaction = 3;
		} else {
		    /* player is at previous buf:
		       turn on rolling, turn off looping */
		    sc->sc_voc[GUS_VOICE_LEFT].voccntl &= ~GUSMASK_LOOP_ENABLE;
		    sc->sc_voc[GUS_VOICE_LEFT].volcntl |= GUSMASK_VOICE_ROLL;
		    playstats[playcntr].vaction = 4;
		}
#ifdef GUSPLAYDEBUG
		if (gusstats) {
		  microtime(&playstats[playcntr].tv);
		  playstats[playcntr].endaddr = sc->sc_voc[GUS_VOICE_LEFT].end_addr;
		  playstats[playcntr].voccntl = sc->sc_voc[GUS_VOICE_LEFT].voccntl;
		  playstats[playcntr].volcntl = sc->sc_voc[GUS_VOICE_LEFT].volcntl;
		  playstats[playcntr].playbuf = sc->sc_playbuf;
		  playstats[playcntr].dmabuf = sc->sc_dmabuf;
		  playstats[playcntr].bufcnt = sc->sc_bufcnt;
		  playstats[playcntr].curaddr = gus_get_curaddr(sc, GUS_VOICE_LEFT);
		  playcntr = ++playcntr % NDMARECS;
		}
#endif
		outb(port+GUS_VOICE_SELECT, GUS_VOICE_LEFT);
		SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);
		outb(port+GUS_DATA_HIGH, sc->sc_voc[GUS_VOICE_LEFT].voccntl);
		SELECT_GUS_REG(port, GUSREG_VOLUME_CONTROL);
		outb(port+GUS_DATA_HIGH, sc->sc_voc[GUS_VOICE_LEFT].volcntl);
	    }
	}
	gus_bufcnt[sc->sc_bufcnt-1]++;
	/*
	 * flip to the next DMA buffer
	 */

	sc->sc_dmabuf = ++sc->sc_dmabuf % sc->sc_nbufs; 
	/*
	 * See comments below about DMA admission control strategy.
	 * We can call the upper level here if we have an
	 * idle buffer (not currently playing) to DMA into.
	 */
	if (sc->sc_dmaoutintr && sc->sc_bufcnt < sc->sc_nbufs) {
	    /* clean out to prevent double calls */
	    void (*pfunc) __P((void *)) = sc->sc_dmaoutintr;
	    void *arg = sc->sc_outarg;

	    sc->sc_outarg = 0;
	    sc->sc_dmaoutintr = 0;
	    (*pfunc)(arg);
	}
}

/*
 * Service voice interrupts
 */

static int
gus_voice_intr(sc)
	struct gus_softc *sc;
{
	register int port = sc->sc_iobase;
	int ignore = 0, voice, rval = 0;
	unsigned long addr;
	unsigned char intr, status;

	/*
	 * The point of this may not be obvious at first.  A voice can
	 * interrupt more than once; according to the GUS SDK we are supposed
	 * to ignore multiple interrupts for the same voice.
	 */

	while(1) {
		SELECT_GUS_REG(port, GUSREG_IRQ_STATUS);
		intr = inb(port+GUS_DATA_HIGH);

		if ((intr & (GUSMASK_WIRQ_VOLUME | GUSMASK_WIRQ_VOICE))
			== (GUSMASK_WIRQ_VOLUME | GUSMASK_WIRQ_VOICE))
			/*
			 * No more interrupts, time to return
			 */
		 	return rval;

		if ((intr & GUSMASK_WIRQ_VOICE) == 0) {

		    /*
		     * We've got a voice interrupt.  Ignore previous
		     * interrupts by the same voice.
		     */

		    rval = 1;
		    voice = intr & GUSMASK_WIRQ_VOICEMASK;

		    if ((1 << voice) & ignore)
			break;

		    ignore |= 1 << voice;

		    /*
		     * If the voice is stopped, then force it to stop
		     * (this stops it from continuously generating IRQs)
		     */

		    SELECT_GUS_REG(port, GUSREG_VOICE_CNTL+0x80);
		    status = inb(port+GUS_DATA_HIGH);
		    if (status & GUSMASK_VOICE_STOPPED) {
			if (voice != GUS_VOICE_LEFT) {
			    DMAPRINTF(("%s: spurious voice %d stop?\n",
				       sc->sc_dev.dv_xname, voice));
			    gus_stop_voice(sc, voice, 0);
			    continue;
			}
			gus_stop_voice(sc, voice, 1);
			/* also kill right voice */
			gus_stop_voice(sc, GUS_VOICE_RIGHT, 0);
			sc->sc_bufcnt--; /* it finished a buffer */
			if (sc->sc_bufcnt > 0) {
			    /*
			     * probably a race to get here: the voice
			     * stopped while the DMA code was just trying to
			     * get the next buffer in place. 
			     * Start the voice again.
			     */
			    printf("%s: stopped voice not drained? (%x)\n",
				   sc->sc_dev.dv_xname, sc->sc_bufcnt);
			    gus_falsestops++;

			    sc->sc_playbuf = ++sc->sc_playbuf % sc->sc_nbufs;
			    gus_start_playing(sc, sc->sc_playbuf);
			} else if (sc->sc_bufcnt < 0) {
#ifdef DDB
			    printf("negative bufcnt in stopped voice\n");
			    Debugger();
#else
			    panic("negative bufcnt in stopped voice");
#endif
			} else {
			    sc->sc_playbuf = -1; /* none are active */
			    gus_stops++;
			}
			/* fall through to callback and admit another
			   buffer.... */
		    } else if (sc->sc_bufcnt != 0) {
			/*
			 * This should always be taken if the voice
			 * is not stopped.
			 */
			gus_continues++;
			gus_continue_playing(sc, voice);
		    }
		    /*
		     * call the upper level to send on down another
		     * block. We do admission rate control as follows:
		     *
		     * When starting up output (in the first N
		     * blocks), call the upper layer after the DMA is
		     * complete (see above in gus_dmaout_intr()).
		     *
		     * When output is already in progress and we have
		     * no more GUS buffers to use for DMA, the DMA
		     * output routines do not call the upper layer.
		     * Instead, we call the DMA completion routine
		     * here, after the voice interrupts indicating
		     * that it's finished with a buffer.
		     *
		     * However, don't call anything here if the DMA
		     * output flag is set, (which shouldn't happen)
		     * because we'll squish somebody else's DMA if
		     * that's the case.  When DMA is done, it will
		     * call back if there is a spare buffer.
		     */
		    if (sc->sc_dmaoutintr && !(sc->sc_flags & GUS_LOCKED)) {
			if (sc->sc_dmaoutintr == stereo_dmaintr)
			    printf("gusdmaout botch?\n");
			else {
			    /* clean out to avoid double calls */
			    void (*pfunc)() = sc->sc_dmaoutintr;
			    void *arg = sc->sc_outarg;

			    sc->sc_outarg = 0;
			    sc->sc_dmaoutintr = 0;
			    (*pfunc)(arg);
			}
		    }
		}

		/*
		 * Ignore other interrupts for now
		 */
	}
}

static void
gus_start_playing(sc, bufno)
struct gus_softc *sc;
int bufno;
{
    register int port = sc->sc_iobase;
    /*
     * Start the voices playing, with buffer BUFNO.
     */

    /*
     * Loop or roll if we have buffers ready.
     */

    if (sc->sc_bufcnt == 1) {
	sc->sc_voc[GUS_VOICE_LEFT].voccntl &= ~(GUSMASK_LOOP_ENABLE);
	sc->sc_voc[GUS_VOICE_LEFT].volcntl &= ~(GUSMASK_VOICE_ROLL);
    } else {
	if (bufno == sc->sc_nbufs - 1) {
	    sc->sc_voc[GUS_VOICE_LEFT].voccntl |= GUSMASK_LOOP_ENABLE;
	    sc->sc_voc[GUS_VOICE_LEFT].volcntl &= ~(GUSMASK_VOICE_ROLL);
	} else {
	    sc->sc_voc[GUS_VOICE_LEFT].voccntl &= ~GUSMASK_LOOP_ENABLE;
	    sc->sc_voc[GUS_VOICE_LEFT].volcntl |= GUSMASK_VOICE_ROLL;
	}
    }

    outb(port+GUS_VOICE_SELECT, GUS_VOICE_LEFT);

    SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);
    outb(port+GUS_DATA_HIGH, sc->sc_voc[GUS_VOICE_LEFT].voccntl);

    SELECT_GUS_REG(port, GUSREG_VOLUME_CONTROL);
    outb(port+GUS_DATA_HIGH, sc->sc_voc[GUS_VOICE_LEFT].volcntl);

    sc->sc_voc[GUS_VOICE_LEFT].current_addr =
	GUS_MEM_OFFSET + sc->sc_chanblocksize * bufno;
    sc->sc_voc[GUS_VOICE_LEFT].end_addr =
	sc->sc_voc[GUS_VOICE_LEFT].current_addr + sc->sc_chanblocksize - 1;
    sc->sc_voc[GUS_VOICE_RIGHT].current_addr =
	sc->sc_voc[GUS_VOICE_LEFT].current_addr + 
	(gus_dostereo && sc->sc_channels == 2 ? GUS_LEFT_RIGHT_OFFSET : 0);
    /*
     * set up right channel to just loop forever, no interrupts,
     * starting at the buffer we just filled.  We'll feed it data
     * at the same time as left channel.
     */
    sc->sc_voc[GUS_VOICE_RIGHT].voccntl |= GUSMASK_LOOP_ENABLE;
    sc->sc_voc[GUS_VOICE_RIGHT].volcntl &= ~(GUSMASK_VOICE_ROLL);

#ifdef GUSPLAYDEBUG
    if (gusstats) {
      microtime(&playstats[playcntr].tv);
      playstats[playcntr].curaddr = sc->sc_voc[GUS_VOICE_LEFT].current_addr;

      playstats[playcntr].voccntl = sc->sc_voc[GUS_VOICE_LEFT].voccntl;
      playstats[playcntr].volcntl = sc->sc_voc[GUS_VOICE_LEFT].volcntl;
      playstats[playcntr].endaddr = sc->sc_voc[GUS_VOICE_LEFT].end_addr;
      playstats[playcntr].playbuf = bufno;
      playstats[playcntr].dmabuf = sc->sc_dmabuf;
      playstats[playcntr].bufcnt = sc->sc_bufcnt;
      playstats[playcntr].vaction = 5;
      playcntr = ++playcntr % NDMARECS;
    }
#endif

    outb(port+GUS_VOICE_SELECT, GUS_VOICE_RIGHT);
    SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);
    outb(port+GUS_DATA_HIGH, sc->sc_voc[GUS_VOICE_RIGHT].voccntl);
    SELECT_GUS_REG(port, GUSREG_VOLUME_CONTROL);
    outb(port+GUS_DATA_HIGH, sc->sc_voc[GUS_VOICE_RIGHT].volcntl);

    gus_start_voice(sc, GUS_VOICE_RIGHT, 0);
    gus_start_voice(sc, GUS_VOICE_LEFT, 1);
    if (sc->sc_playbuf == -1)
	/* mark start of playing */
	sc->sc_playbuf = bufno;
}

static void
gus_continue_playing(sc, voice)
register struct gus_softc *sc;
int voice;
{
    register int port = sc->sc_iobase;

    /*
     * stop this voice from interrupting while we work.
     */

    SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);
    outb(port+GUS_DATA_HIGH, sc->sc_voc[voice].voccntl & ~(GUSMASK_VOICE_IRQ));

    /* 
     * update playbuf to point to the buffer the hardware just started
     * playing
     */
    sc->sc_playbuf = ++sc->sc_playbuf % sc->sc_nbufs;
    
    /*
     * account for buffer just finished
     */
    if (--sc->sc_bufcnt == 0) {
	DPRINTF(("gus: bufcnt 0 on continuing voice?\n"));
    }
    if (sc->sc_playbuf == sc->sc_dmabuf && (sc->sc_flags & GUS_LOCKED))
	printf("continue into active dmabuf?\n");

    /*
     * Select the end of the buffer based on the currently active
     * buffer, [plus extra contiguous buffers (if ready)].
     */

    /* 
     * set endpoint at end of buffer we just started playing.
     *
     * The total gets -1 because end addrs are one less than you might
     * think (the end_addr is the address of the last sample to play)
     */
    gus_set_endaddr(sc, voice, GUS_MEM_OFFSET +
		    sc->sc_chanblocksize * (sc->sc_playbuf + 1) - 1);

    if (sc->sc_bufcnt < 2) {
	/*
	 * Clear out the loop and roll flags, and rotate the currently
	 * playing buffer.  That way, if we don't manage to get more
	 * data before this buffer finishes, we'll just stop.
	 */
	sc->sc_voc[voice].voccntl &= ~GUSMASK_LOOP_ENABLE;
	sc->sc_voc[voice].volcntl &= ~GUSMASK_VOICE_ROLL;
	playstats[playcntr].vaction = 0;
    } else {
	/*
	 * We have some buffers to play.  set LOOP if we're on the
	 * last buffer in the ring, otherwise set ROLL.
	 */
	if (sc->sc_playbuf == sc->sc_nbufs - 1) {
	    sc->sc_voc[voice].voccntl |= GUSMASK_LOOP_ENABLE;
	    sc->sc_voc[voice].volcntl &= ~GUSMASK_VOICE_ROLL;
	    playstats[playcntr].vaction = 1;
	} else {
	    sc->sc_voc[voice].voccntl &= ~GUSMASK_LOOP_ENABLE;
	    sc->sc_voc[voice].volcntl |= GUSMASK_VOICE_ROLL;
	    playstats[playcntr].vaction = 2;
	}
    }
#ifdef GUSPLAYDEBUG
    if (gusstats) {
      microtime(&playstats[playcntr].tv);
      playstats[playcntr].curaddr = gus_get_curaddr(sc, voice);

      playstats[playcntr].voccntl = sc->sc_voc[voice].voccntl;
      playstats[playcntr].volcntl = sc->sc_voc[voice].volcntl;
      playstats[playcntr].endaddr = sc->sc_voc[voice].end_addr;
      playstats[playcntr].playbuf = sc->sc_playbuf;
      playstats[playcntr].dmabuf = sc->sc_dmabuf;
      playstats[playcntr].bufcnt = sc->sc_bufcnt;
      playcntr = ++playcntr % NDMARECS;
    }
#endif

    /*
     * (re-)set voice parameters.  This will reenable interrupts from this
     * voice.
     */

    SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);
    outb(port+GUS_DATA_HIGH, sc->sc_voc[voice].voccntl);
    SELECT_GUS_REG(port, GUSREG_VOLUME_CONTROL);
    outb(port+GUS_DATA_HIGH, sc->sc_voc[voice].volcntl);
}

/*
 * Send/receive data into GUS's DRAM using DMA.  Called at splgus()
 */

static void
gusdmaout(sc, flags, gusaddr, buffaddr, length)
	struct gus_softc *sc;
	int flags, length;
	unsigned long gusaddr;
	caddr_t buffaddr;
{
	register unsigned char c = (unsigned char) flags;
	register int port = sc->sc_iobase;
	int s;

	DMAPRINTF(("gusdmaout flags=%x scflags=%x\n", flags, sc->sc_flags));

	sc->sc_gusaddr = gusaddr;

	/*
	 * If we're using a 16 bit DMA channel, we have to jump through some
	 * extra hoops; this includes translating the DRAM address a bit
	 */

	if (sc->sc_drq >= 4) {
		c |= GUSMASK_DMA_WIDTH;
		gusaddr = convert_to_16bit(gusaddr);
	}

	/*
	 * Add flag bits that we always set - fast DMA, enable IRQ
	 */

	c |= GUSMASK_DMA_ENABLE | GUSMASK_DMA_R0 | GUSMASK_DMA_IRQ;

	/*
	 * Make sure the GUS _isn't_ setup for DMA
	 */

 	SELECT_GUS_REG(port, GUSREG_DMA_CONTROL);
	outb(port+GUS_DATA_HIGH, 0);

	/*
	 * Tell the PC DMA controller to start doing DMA
	 */

	sc->sc_dmaoutaddr = (u_char *) buffaddr;
	sc->sc_dmaoutcnt = length;
	isa_dmastart(B_WRITE, buffaddr, length, sc->sc_drq);

	/*
	 * Set up DMA address - use the upper 16 bits ONLY
	 */

	sc->sc_flags |= GUS_DMAOUT_ACTIVE;

 	SELECT_GUS_REG(port, GUSREG_DMA_START);
 	outw(port+GUS_DATA_LOW, (int) (gusaddr >> 4));

 	/*
 	 * Tell the GUS to start doing DMA
 	 */

 	SELECT_GUS_REG(port, GUSREG_DMA_CONTROL);
	outb(port+GUS_DATA_HIGH, c);

	/*
	 * XXX If we don't finish in one second, give up...
	 */
	untimeout(gus_dmaout_timeout, sc); /* flush old one, if there is one */
	timeout(gus_dmaout_timeout, sc, hz);
}

/*
 * Start a voice playing on the GUS.  Called from interrupt handler at
 * splgus().
 */

static void
gus_start_voice(sc, voice, intrs)
	struct gus_softc *sc;
	int voice;
	int intrs;
{
	register int port = sc->sc_iobase;
	unsigned long start;
	unsigned long current;
	unsigned long end;

	/*
	 * Pick all the values for the voice out of the gus_voice struct
	 * and use those to program the voice
	 */

 	start = sc->sc_voc[voice].start_addr;
 	current = sc->sc_voc[voice].current_addr;
 	end = sc->sc_voc[voice].end_addr;

 	/*
	 * If we're using 16 bit data, mangle the addresses a bit
	 */

	if (sc->sc_voc[voice].voccntl & GUSMASK_DATA_SIZE16) {
	        /* -1 on start so that we get onto sample boundary--other
		   code always sets it for 1-byte rollover protection */
		start = convert_to_16bit(start-1);
		current = convert_to_16bit(current);
		end = convert_to_16bit(end);
	}

	/*
	 * Select the voice we want to use, and program the data addresses
	 */

	outb(port+GUS_VOICE_SELECT, (unsigned char) voice);

	SELECT_GUS_REG(port, GUSREG_START_ADDR_HIGH);
	outw(port+GUS_DATA_LOW, ADDR_HIGH(start));
	SELECT_GUS_REG(port, GUSREG_START_ADDR_LOW);
	outw(port+GUS_DATA_LOW, ADDR_LOW(start));

	SELECT_GUS_REG(port, GUSREG_CUR_ADDR_HIGH);
	outw(port+GUS_DATA_LOW, ADDR_HIGH(current));
	SELECT_GUS_REG(port, GUSREG_CUR_ADDR_LOW);
	outw(port+GUS_DATA_LOW, ADDR_LOW(current));

	SELECT_GUS_REG(port, GUSREG_END_ADDR_HIGH);
	outw(port+GUS_DATA_LOW, ADDR_HIGH(end));
	SELECT_GUS_REG(port, GUSREG_END_ADDR_LOW);
	outw(port+GUS_DATA_LOW, ADDR_LOW(end));

	/*
	 * (maybe) enable interrupts, disable voice stopping
	 */

	if (intrs) {
		sc->sc_flags |= GUS_PLAYING; /* playing is about to start */
		sc->sc_voc[voice].voccntl |= GUSMASK_VOICE_IRQ;
		DMAPRINTF(("gus voice playing=%x\n", sc->sc_flags));
	} else
		sc->sc_voc[voice].voccntl &= ~GUSMASK_VOICE_IRQ;
	sc->sc_voc[voice].voccntl &= ~(GUSMASK_VOICE_STOPPED |
		GUSMASK_STOP_VOICE);

	/*
	 * Tell the GUS about it.  Note that we're doing volume ramping here
	 * from 0 up to the set volume to help reduce clicks.
	 */

	SELECT_GUS_REG(port, GUSREG_START_VOLUME);
	outb(port+GUS_DATA_HIGH, 0x00);
	SELECT_GUS_REG(port, GUSREG_END_VOLUME);
	outb(port+GUS_DATA_HIGH, sc->sc_voc[voice].current_volume >> 4);
	SELECT_GUS_REG(port, GUSREG_CUR_VOLUME);
	outw(port+GUS_DATA_LOW, 0x00);
	SELECT_GUS_REG(port, GUSREG_VOLUME_RATE);
	outb(port+GUS_DATA_HIGH, 63);

	SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);
	outb(port+GUS_DATA_HIGH, sc->sc_voc[voice].voccntl);
	SELECT_GUS_REG(port, GUSREG_VOLUME_CONTROL);
	outb(port+GUS_DATA_HIGH, 0x00);
	delay(50);
	SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);
	outb(port+GUS_DATA_HIGH, sc->sc_voc[voice].voccntl);
	SELECT_GUS_REG(port, GUSREG_VOLUME_CONTROL);
	outb(port+GUS_DATA_HIGH, 0x00);

}

/*
 * Stop a given voice.  called at splgus()
 */

static void
gus_stop_voice(sc, voice, intrs_too)
	struct gus_softc *sc;
	int voice;
	int intrs_too;
{
	register int port = sc->sc_iobase;

	sc->sc_voc[voice].voccntl |= GUSMASK_VOICE_STOPPED |
		GUSMASK_STOP_VOICE;
	if (intrs_too) {
	  sc->sc_voc[voice].voccntl &= ~(GUSMASK_VOICE_IRQ);
	  /* no more DMA to do */
	  sc->sc_flags &= ~GUS_PLAYING;
	}
	DMAPRINTF(("gusintr voice notplaying=%x\n", sc->sc_flags));

	guspoke(port, 0L, 0);

	outb(port+GUS_VOICE_SELECT, (unsigned char) voice);

	SELECT_GUS_REG(port, GUSREG_CUR_VOLUME);
	outw(port+GUS_DATA_LOW, 0x0000);
	SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);
	outb(port+GUS_DATA_HIGH, sc->sc_voc[voice].voccntl);
	delay(100);
	SELECT_GUS_REG(port, GUSREG_CUR_VOLUME);
	outw(port+GUS_DATA_LOW, 0x0000);
	SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);
	outb(port+GUS_DATA_HIGH, sc->sc_voc[voice].voccntl);

	SELECT_GUS_REG(port, GUSREG_CUR_ADDR_HIGH);
	outw(port+GUS_DATA_LOW, 0x0000);
	SELECT_GUS_REG(port, GUSREG_CUR_ADDR_LOW);
	outw(port+GUS_DATA_LOW, 0x0000);

}


/*
 * Set the volume of a given voice.  Called at splgus().
 */
static void
gus_set_volume(sc, voice, volume)
	struct gus_softc *sc;
	int voice, volume;
{
	register int port = sc->sc_iobase;
	unsigned int gusvol;

	gusvol = gus_log_volumes[volume < 512 ? volume : 511];

	sc->sc_voc[voice].current_volume = gusvol;

	outb(port+GUS_VOICE_SELECT, (unsigned char) voice);

	SELECT_GUS_REG(port, GUSREG_START_VOLUME);
	outb(port+GUS_DATA_HIGH, (unsigned char) (gusvol >> 4));

	SELECT_GUS_REG(port, GUSREG_END_VOLUME);
	outb(port+GUS_DATA_HIGH, (unsigned char) (gusvol >> 4));

	SELECT_GUS_REG(port, GUSREG_CUR_VOLUME);
	outw(port+GUS_DATA_LOW, gusvol << 4);
	delay(500);
	outw(port+GUS_DATA_LOW, gusvol << 4);

}

/*
 * Interface to the audio layer - set the data encoding type
 */

int
gusmax_set_encoding(addr, encoding)
	void * addr;
	u_int encoding;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	(void) ad1848_set_encoding(ac, encoding);
	return gus_set_encoding(sc, encoding);
}

int
gus_set_encoding(addr, encoding)
	void * addr;
	u_int encoding;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_set_encoding called\n"));

	/* XXX todo: add alaw for codec */
	if (encoding != AUDIO_ENCODING_ULAW &&
	    encoding != AUDIO_ENCODING_PCM16 &&
	    encoding != AUDIO_ENCODING_PCM8)
		return EINVAL;

	if (encoding != AUDIO_ENCODING_PCM16)
		sc->sc_precision = 8;       /* XXX force it. */

	sc->sc_encoding = encoding;

	if (sc->sc_precision == 8) {
		sc->sc_voc[GUS_VOICE_LEFT].voccntl &= ~GUSMASK_DATA_SIZE16;
		sc->sc_voc[GUS_VOICE_RIGHT].voccntl &= ~GUSMASK_DATA_SIZE16;
	} else {
		sc->sc_voc[GUS_VOICE_LEFT].voccntl |= GUSMASK_DATA_SIZE16;
		sc->sc_voc[GUS_VOICE_RIGHT].voccntl |= GUSMASK_DATA_SIZE16;
	}
	return 0;
}

int
gusmax_set_channels(addr, channels)
	void * addr;
	int channels;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	(void) ad1848_set_channels(ac, channels);
	return gus_set_channels(sc, channels);
}

int
gus_set_channels(addr, channels)
	void * addr;
	int channels;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_set_channels called\n"));

	if (channels != 1 && channels != 2)
		return EINVAL;

	sc->sc_channels = channels;

	return 0;
}

/*
 * Interface to the audio layer - set the data precision
 */

int
gusmax_set_precision(addr, bits)
	void * addr;
	u_int bits;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;

	(void) ad1848_set_precision(ac, bits);
	return gus_set_precision(sc, bits);
}


int
gus_set_precision(addr, bits)
	void * addr;
	u_int bits;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_set_precision called\n"));

	if (bits != 8 && bits != 16)
		return EINVAL;

	if (sc->sc_encoding != AUDIO_ENCODING_PCM16 && bits != 8)
		/* If we're doing PCM8 or MULAW, it must be 8 bits. */
		return EINVAL;

	sc->sc_precision = bits;

	if (bits == 16) {
		sc->sc_voc[GUS_VOICE_LEFT].voccntl |= GUSMASK_DATA_SIZE16;
		sc->sc_voc[GUS_VOICE_RIGHT].voccntl |= GUSMASK_DATA_SIZE16;
	} else {
		sc->sc_voc[GUS_VOICE_LEFT].voccntl &= ~GUSMASK_DATA_SIZE16;
		sc->sc_voc[GUS_VOICE_RIGHT].voccntl &= ~GUSMASK_DATA_SIZE16;
	}
	return 0;
}

/*
 * Interface to the audio layer - set the blocksize to the correct number
 * of units
 */

int
gusmax_round_blocksize(addr, blocksize)
	void * addr;
	int blocksize;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;

/*	blocksize = ad1848_round_blocksize(ac, blocksize);*/
	return gus_round_blocksize(sc, blocksize);
}

int
gus_round_blocksize(addr, blocksize)
	void * addr;
	int blocksize;
{
	register struct gus_softc *sc = addr;
	register unsigned long i;

	DPRINTF(("gus_round_blocksize called\n"));

	if (sc->sc_encoding == AUDIO_ENCODING_ULAW && blocksize > 32768)
		blocksize = 32768;
	else if (blocksize > 65536)
		blocksize = 65536;

	if ((blocksize % GUS_BUFFER_MULTIPLE) != 0)
		blocksize = (blocksize / GUS_BUFFER_MULTIPLE + 1) *
			GUS_BUFFER_MULTIPLE;

	/* set up temporary buffer to hold the deinterleave, if necessary
	   for stereo output */
	if (sc->sc_deintr_buf) {
		FREE(sc->sc_deintr_buf, M_DEVBUF);
		sc->sc_deintr_buf = NULL;
	}
	MALLOC(sc->sc_deintr_buf, void *, blocksize>>1, M_DEVBUF, M_WAITOK);

	sc->sc_blocksize = blocksize;
	/* multi-buffering not quite working yet. */
	sc->sc_nbufs = /*GUS_MEM_FOR_BUFFERS / blocksize*/ 2;

	gus_set_chan_addrs(sc);

	return blocksize;
}

/*
 * Interfaces to the audio layer - return values from the software config
 * struct
 */

int
gusmax_get_encoding(addr)
	void * addr;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	return gus_get_encoding(sc);
}

int
gus_get_encoding(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_get_encoding called\n"));

	/* XXX TODO: codec stuff */
	return sc->sc_encoding;
}

int
gusmax_get_channels(addr)
	void * addr;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	return gus_get_channels(sc);
}

int
gus_get_channels(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_get_channels called\n"));

	return sc->sc_channels;
}

u_long
gus_get_in_sr(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_get_in_sr called\n"));
	return sc->sc_irate;
}

u_long
gusmax_get_in_sr(addr)
	void * addr;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	return gus_get_in_sr(sc);
}

u_long
gusmax_get_out_sr(addr)
	void * addr;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	return gus_get_out_sr(sc);
}

u_long
gus_get_out_sr(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_get_out_sr called\n"));
	return sc->sc_orate;
}

int
gusmax_get_precision(addr)
	void * addr;
{
	register struct ad1848_softc *sc = addr;
	return gus_get_precision(sc->parent);
}

int
gus_get_precision(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_get_precision called\n"));

	return sc->sc_precision;
}

int
gus_get_out_gain(addr)
	caddr_t addr;
{
	register struct gus_softc *sc = (struct gus_softc *) addr;

	DPRINTF(("gus_get_out_gain called\n"));
	return sc->sc_ogain / 2;
}

/*
 * Interface to the audio layer - set the sample rate of the output voices
 */

int
gusmax_set_out_sr(addr, rate)
	void * addr;
	u_long rate;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	(void) ad1848_set_out_sr(ac, rate);
	return gus_set_out_sr(sc, rate);
}

int
gus_set_out_sr(addr, rate)
	void * addr;
	u_long rate;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_set_out_sr called\n"));

	if (rate > gus_max_frequency[sc->sc_voices - GUS_MIN_VOICES])
		rate = gus_max_frequency[sc->sc_voices - GUS_MIN_VOICES];

	sc->sc_orate = rate;

	return 0;
}

static inline void gus_set_voices(sc, voices)
struct gus_softc *sc;
int voices;
{
	register int port = sc->sc_iobase;
	/*
	 * Select the active number of voices
	 */

	SELECT_GUS_REG(port, GUSREG_ACTIVE_VOICES);
	outb(port+GUS_DATA_HIGH, (voices-1) | 0xc0);

	sc->sc_voices = voices;
}

/*
 * Actually set the settings of various values on the card
 */

int
gusmax_commit_settings(addr)
	void * addr;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;

	(void) ad1848_commit_settings(ac);
	return gus_commit_settings(sc);
}

/*
 * Commit the settings.  Called at normal IPL.
 */
int
gus_commit_settings(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;
	int s;

	DPRINTF(("gus_commit_settings called (gain = %d)\n",sc->sc_ogain));


	s = splgus();

	gus_set_recrate(sc, sc->sc_irate);
	gus_set_volume(sc, GUS_VOICE_LEFT, sc->sc_ogain);
	gus_set_volume(sc, GUS_VOICE_RIGHT, sc->sc_ogain);
	gus_set_samprate(sc, GUS_VOICE_LEFT, sc->sc_orate);
	gus_set_samprate(sc, GUS_VOICE_RIGHT, sc->sc_orate);
	splx(s);
	gus_set_chan_addrs(sc);

	return 0;
}

static void
gus_set_chan_addrs(sc)
struct gus_softc *sc;
{
	/*
	 * We use sc_nbufs * blocksize bytes of storage in the on-board GUS
	 * ram. 
	 * For mono, each of the sc_nbufs buffers is DMA'd to in one chunk,
	 * and both left & right channels play the same buffer.
	 *
	 * For stereo, each channel gets a contiguous half of the memory,
	 * and each has sc_nbufs buffers of size blocksize/2.
	 * Stereo data are deinterleaved in main memory before the DMA out
	 * routines are called to queue the output.
	 *
	 * The blocksize per channel is kept in sc_chanblocksize.
	 */
	if (sc->sc_channels == 2)
	    sc->sc_chanblocksize = sc->sc_blocksize/2;
	else
	    sc->sc_chanblocksize = sc->sc_blocksize;

	sc->sc_voc[GUS_VOICE_LEFT].start_addr = GUS_MEM_OFFSET - 1;
	sc->sc_voc[GUS_VOICE_RIGHT].start_addr =
	    (gus_dostereo && sc->sc_channels == 2 ? GUS_LEFT_RIGHT_OFFSET : 0)
	      + GUS_MEM_OFFSET - 1;
	sc->sc_voc[GUS_VOICE_RIGHT].current_addr =
	    sc->sc_voc[GUS_VOICE_RIGHT].start_addr + 1;
	sc->sc_voc[GUS_VOICE_RIGHT].end_addr =
	    sc->sc_voc[GUS_VOICE_RIGHT].start_addr +
	    sc->sc_nbufs * sc->sc_chanblocksize;

}

/*
 * Set the sample rate of the given voice.  Called at splgus().
 */

static void
gus_set_samprate(sc, voice, freq)
	struct gus_softc *sc;
	int voice, freq;
{
	register int port = sc->sc_iobase;
	unsigned int fc;
	unsigned long temp, f = (unsigned long) freq;

	/*
	 * calculate fc based on the number of active voices;
	 * we need to use longs to preserve enough bits
	 */

	temp = (unsigned long) gus_max_frequency[sc->sc_voices-GUS_MIN_VOICES];

 	fc = (unsigned int)(((f << 9L) + (temp >> 1L)) / temp);

 	fc <<= 1;


	/*
	 * Program the voice frequency, and set it in the voice data record
	 */

	outb(port+GUS_VOICE_SELECT, (unsigned char) voice);
	SELECT_GUS_REG(port, GUSREG_FREQ_CONTROL);
	outw(port+GUS_DATA_LOW, fc);

	sc->sc_voc[voice].rate = freq;

}

/*
 * Interface to the audio layer - set the recording sampling rate
 */

int
gusmax_set_in_sr(addr, rate)
	void * addr;
	u_long rate;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	(void) ad1848_set_in_sr(ac, rate);
	return gus_set_in_sr(sc, rate);
}


int
gus_set_in_sr(addr, rate)
	void *addr;
	u_long rate;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gus_set_in_sr called\n"));

	sc->sc_irate = rate;

	return 0;
}
/*
 * Set the sample rate of the recording frequency.  Formula is from the GUS
 * SDK.  Called at splgus().
 */

static void
gus_set_recrate(sc, rate)
	struct gus_softc *sc;
	u_long rate;
{
	register int port = sc->sc_iobase;
	u_char realrate;
	int s;
	DPRINTF(("gus_set_recrate %lu\n", rate));

/*	realrate = 9878400/(16*(rate+2)); /* formula from GUS docs */
	realrate = (9878400 >> 4)/rate - 2; /* formula from code, sigh. */

	SELECT_GUS_REG(port, GUSREG_SAMPLE_FREQ);
 	outb(port+GUS_DATA_HIGH, realrate);
}

/*
 * Interface to the audio layer - turn the output on or off.  Note that some
 * of these bits are flipped in the register
 */

int
gusmax_speaker_ctl(addr, newstate)
	void * addr;
	int newstate;
{
	register struct ad1848_softc *sc = addr;
	return gus_speaker_ctl(sc->parent, newstate);
}

int
gus_speaker_ctl(addr, newstate)
	void * addr;
	int newstate;
{
	register struct gus_softc *sc = (struct gus_softc *) addr;

	/* Line out bit is flipped: 0 enables, 1 disables */
	if ((newstate == SPKR_ON) &&
	    (sc->sc_mixcontrol & GUSMASK_LINE_OUT)) {
		sc->sc_mixcontrol &= ~GUSMASK_LINE_OUT;
		outb(sc->sc_iobase+GUS_MIX_CONTROL, sc->sc_mixcontrol);
	}
	if ((newstate == SPKR_OFF) &&
	    (sc->sc_mixcontrol & GUSMASK_LINE_OUT) == 0) {
		sc->sc_mixcontrol |= GUSMASK_LINE_OUT;
		outb(sc->sc_iobase+GUS_MIX_CONTROL, sc->sc_mixcontrol);
	}

	return 0;
}

static int
gus_linein_ctl(addr, newstate)
	void * addr;
	int newstate;
{
	register struct gus_softc *sc = (struct gus_softc *) addr;

	/* Line in bit is flipped: 0 enables, 1 disables */
	if ((newstate == SPKR_ON) &&
	    (sc->sc_mixcontrol & GUSMASK_LINE_IN)) {
		sc->sc_mixcontrol &= ~GUSMASK_LINE_IN;
		outb(sc->sc_iobase+GUS_MIX_CONTROL, sc->sc_mixcontrol);
	}
	if ((newstate == SPKR_OFF) &&
	    (sc->sc_mixcontrol & GUSMASK_LINE_IN) == 0) {
		sc->sc_mixcontrol |= GUSMASK_LINE_IN;
		outb(sc->sc_iobase+GUS_MIX_CONTROL, sc->sc_mixcontrol);
	}

	return 0;
}

static int
gus_mic_ctl(addr, newstate)
	void * addr;
	int newstate;
{
	register struct gus_softc *sc = (struct gus_softc *) addr;

	/* Mic bit is normal: 1 enables, 0 disables */
	if ((newstate == SPKR_ON) &&
	    (sc->sc_mixcontrol & GUSMASK_MIC_IN) == 0) {
		sc->sc_mixcontrol |= GUSMASK_MIC_IN;
		outb(sc->sc_iobase+GUS_MIX_CONTROL, sc->sc_mixcontrol);
	}
	if ((newstate == SPKR_OFF) &&
	    (sc->sc_mixcontrol & GUSMASK_MIC_IN)) {
		sc->sc_mixcontrol &= ~GUSMASK_MIC_IN;
		outb(sc->sc_iobase+GUS_MIX_CONTROL, sc->sc_mixcontrol);
	}

	return 0;
}

/*
 * Set the end address of a give voice.  Called at splgus()
 */

static void
gus_set_endaddr(sc, voice, addr)
	struct gus_softc *sc;
	int voice;
	unsigned long addr;
{
	register int port = sc->sc_iobase;

	sc->sc_voc[voice].end_addr = addr;

	if (sc->sc_voc[voice].voccntl & GUSMASK_DATA_SIZE16)
		addr = convert_to_16bit(addr);

	SELECT_GUS_REG(port, GUSREG_END_ADDR_HIGH);
	outw(port+GUS_DATA_LOW, ADDR_HIGH(addr));
	SELECT_GUS_REG(port, GUSREG_END_ADDR_LOW);
	outw(port+GUS_DATA_LOW, ADDR_LOW(addr));

}

#if 0
/*
 * Set current address.  called at splgus()
 */
static void
gus_set_curaddr(sc, voice, addr)
	struct gus_softc *sc;
	int voice;
	unsigned long addr;
{
	register int port = sc->sc_iobase;

	sc->sc_voc[voice].current_addr = addr;

	if (sc->sc_voc[voice].voccntl & GUSMASK_DATA_SIZE16)
		addr = convert_to_16bit(addr);

	outb(port+GUS_VOICE_SELECT, (unsigned char) voice);

	SELECT_GUS_REG(port, GUSREG_CUR_ADDR_HIGH);
	outw(port+GUS_DATA_LOW, ADDR_HIGH(addr));
	SELECT_GUS_REG(port, GUSREG_CUR_ADDR_LOW);
	outw(port+GUS_DATA_LOW, ADDR_LOW(addr));

}
#endif

/*
 * Get current GUS playback address.  Called at splgus().
 */
static unsigned long
gus_get_curaddr(sc, voice)
	struct gus_softc *sc;
	int voice;
{
	register int port = sc->sc_iobase;
	unsigned long addr;

	outb(port+GUS_VOICE_SELECT, (unsigned char) voice);
	SELECT_GUS_REG(port, GUSREG_CUR_ADDR_HIGH|GUSREG_READ);
	addr = (inw(port+GUS_DATA_LOW) & 0x1fff) << 7;
	SELECT_GUS_REG(port, GUSREG_CUR_ADDR_LOW|GUSREG_READ);
	addr |= (inw(port+GUS_DATA_LOW) >> 9L) & 0x7f;

	if (sc->sc_voc[voice].voccntl & GUSMASK_DATA_SIZE16)
	    addr = (addr & 0xc0000) | ((addr & 0x1ffff) << 1); /* undo 16-bit change */
	DPRINTF(("gus voice %d curaddr %d end_addr %d\n",
		 voice, addr, sc->sc_voc[voice].end_addr));
	/* XXX sanity check the address? */

	return(addr);
}

/*
 * Convert an address value to a "16 bit" value - why this is necessary I
 * have NO idea
 */

static unsigned long
convert_to_16bit(address)
	unsigned long address;
{
	unsigned long old_address;

	old_address = address;
	address >>= 1;
	address &= 0x0001ffffL;
	address |= (old_address & 0x000c0000L);

	return (address);
}

/*
 * Write a value into the GUS's DRAM
 */

static void
guspoke(port, address, value)
	int port;
	long address;
	unsigned char value;
{

	/*
	 * Select the DRAM address
	 */

 	SELECT_GUS_REG(port, GUSREG_DRAM_ADDR_LOW);
 	outw(port+GUS_DATA_LOW, (unsigned int) (address & 0xffff));
 	SELECT_GUS_REG(port, GUSREG_DRAM_ADDR_HIGH);
 	outb(port+GUS_DATA_HIGH, (unsigned char) ((address >> 16) & 0xff));

	/*
	 * Actually write the data
	 */

	outb(port+GUS_DRAM_DATA, value);
}

/*
 * Read a value from the GUS's DRAM
 */

static unsigned char
guspeek(port, address)
	int port;
	u_long address;
{

	/*
	 * Select the DRAM address
	 */

 	SELECT_GUS_REG(port, GUSREG_DRAM_ADDR_LOW);
 	outw(port+GUS_DATA_LOW, (unsigned int) (address & 0xffff));
 	SELECT_GUS_REG(port, GUSREG_DRAM_ADDR_HIGH);
 	outb(port+GUS_DATA_HIGH, (unsigned char) ((address >> 16) & 0xff));

	/*
	 * Read in the data from the board
	 */

	return (unsigned char) inb(port+GUS_DRAM_DATA);
}

/*
 * Reset the Gravis UltraSound card, completely
 */

static void
gusreset(sc, voices)
	struct gus_softc *sc;
	int voices;
{
	register int port = sc->sc_iobase;
	int i,s;

	s = splgus();

	/*
	 * Reset the GF1 chip
	 */

	SELECT_GUS_REG(port, GUSREG_RESET);
	outb(port+GUS_DATA_HIGH, 0x00);

	delay(500);

	/*
	 * Release reset
	 */

	SELECT_GUS_REG(port, GUSREG_RESET);
	outb(port+GUS_DATA_HIGH, GUSMASK_MASTER_RESET);

	delay(500);

	/*
	 * Reset MIDI port as well
	 */

	outb(GUS_MIDI_CONTROL,MIDI_RESET);

	delay(500);

	outb(GUS_MIDI_CONTROL,0x00);

	/*
	 * Clear interrupts
	 */

	SELECT_GUS_REG(port, GUSREG_DMA_CONTROL);
	outb(port+GUS_DATA_HIGH, 0x00);
	SELECT_GUS_REG(port, GUSREG_TIMER_CONTROL);
	outb(port+GUS_DATA_HIGH, 0x00);
	SELECT_GUS_REG(port, GUSREG_SAMPLE_CONTROL);
	outb(port+GUS_DATA_HIGH, 0x00);

	gus_set_voices(sc, voices);

	inb(port+GUS_IRQ_STATUS);
	SELECT_GUS_REG(port, GUSREG_DMA_CONTROL);
	inb(port+GUS_DATA_HIGH);
	SELECT_GUS_REG(port, GUSREG_SAMPLE_CONTROL);
	inb(port+GUS_DATA_HIGH);
	SELECT_GUS_REG(port, GUSREG_IRQ_STATUS);
	inb(port+GUS_DATA_HIGH);

	/*
	 * Reset voice specific information
	 */

	for(i = 0; i < voices; i++) {
		outb(port+GUS_VOICE_SELECT, (unsigned char) i);

		SELECT_GUS_REG(port, GUSREG_VOICE_CNTL);

		sc->sc_voc[i].voccntl = GUSMASK_VOICE_STOPPED |
			GUSMASK_STOP_VOICE;

		outb(port+GUS_DATA_HIGH, sc->sc_voc[i].voccntl);

		sc->sc_voc[i].volcntl = GUSMASK_VOLUME_STOPPED |
				GUSMASK_STOP_VOLUME;

		SELECT_GUS_REG(port, GUSREG_VOLUME_CONTROL);
		outb(port+GUS_DATA_HIGH, sc->sc_voc[i].volcntl);

		delay(100);

		gus_set_samprate(sc, i, 8000);
		SELECT_GUS_REG(port, GUSREG_START_ADDR_HIGH);
		outw(port+GUS_DATA_LOW, 0x0000);
		SELECT_GUS_REG(port, GUSREG_START_ADDR_LOW);
		outw(port+GUS_DATA_LOW, 0x0000);
		SELECT_GUS_REG(port, GUSREG_END_ADDR_HIGH);
		outw(port+GUS_DATA_LOW, 0x0000);
		SELECT_GUS_REG(port, GUSREG_END_ADDR_LOW);
		outw(port+GUS_DATA_LOW, 0x0000);
		SELECT_GUS_REG(port, GUSREG_VOLUME_RATE);
		outb(port+GUS_DATA_HIGH, 0x01);
		SELECT_GUS_REG(port, GUSREG_START_VOLUME);
		outb(port+GUS_DATA_HIGH, 0x10);
		SELECT_GUS_REG(port, GUSREG_END_VOLUME);
		outb(port+GUS_DATA_HIGH, 0xe0);
		SELECT_GUS_REG(port, GUSREG_CUR_VOLUME);
		outw(port+GUS_DATA_LOW, 0x0000);

		SELECT_GUS_REG(port, GUSREG_CUR_ADDR_HIGH);
		outw(port+GUS_DATA_LOW, 0x0000);
		SELECT_GUS_REG(port, GUSREG_CUR_ADDR_LOW);
		outw(port+GUS_DATA_LOW, 0x0000);
		SELECT_GUS_REG(port, GUSREG_PAN_POS);
		outb(port+GUS_DATA_HIGH, 0x07);
	}

	/*
	 * Clear out any pending IRQs
	 */

	inb(port+GUS_IRQ_STATUS);
	SELECT_GUS_REG(port, GUSREG_DMA_CONTROL);
	inb(port+GUS_DATA_HIGH);
	SELECT_GUS_REG(port, GUSREG_SAMPLE_CONTROL);
	inb(port+GUS_DATA_HIGH);
	SELECT_GUS_REG(port, GUSREG_IRQ_STATUS);
	inb(port+GUS_DATA_HIGH);

	SELECT_GUS_REG(port, GUSREG_RESET);
	outb(port+GUS_DATA_HIGH, GUSMASK_MASTER_RESET | GUSMASK_DAC_ENABLE |
		GUSMASK_IRQ_ENABLE);

	splx(s);
}


static void
gus_init_cs4231(sc)
	struct gus_softc *sc;
{
	register int port = sc->sc_iobase;
	u_char ctrl;

	ctrl = (port & 0xf0) >> 4;	/* set port address middle nibble */
	/*
	 * The codec is a bit weird--swapped dma channels.
	 */
	ctrl |= GUS_MAX_CODEC_ENABLE;
	if (sc->sc_drq >= 4)
		ctrl |= GUS_MAX_RECCHAN16;
	if (sc->sc_recdrq >= 4)
		ctrl |= GUS_MAX_PLAYCHAN16;

	outb(port+GUS_MAX_CTRL, ctrl);

	sc->sc_codec.sc_iobase = port+GUS_MAX_CODEC_BASE;

	if (ad1848_probe(&sc->sc_codec) == 0) {
		sc->sc_flags &= ~GUS_CODEC_INSTALLED;
	} else {
		struct ad1848_volume vol = {AUDIO_MAX_GAIN, AUDIO_MAX_GAIN};
		struct audio_hw_if gusmax_hw_if = {
			gusopen,
			gusmax_close,
			NULL,				/* drain */
			gusmax_set_in_sr,
			gusmax_get_in_sr,
			gusmax_set_out_sr,
			gusmax_get_out_sr,

			ad1848_query_encoding, /* query encoding */
			gusmax_set_encoding,
			gusmax_get_encoding,

			gusmax_set_precision,
			gusmax_get_precision,

			gusmax_set_channels,
			gusmax_get_channels,

			gusmax_round_blocksize,

			gusmax_set_out_port,
			gusmax_get_out_port,
			gusmax_set_in_port,
			gusmax_get_in_port,

			gusmax_commit_settings,

			ad1848_get_silence,

			gusmax_expand,	/* XXX use codec */
			mulaw_compress,

			gusmax_dma_output,
			gusmax_dma_input,
			gusmax_halt_out_dma,
			gusmax_halt_in_dma,
			gusmax_cont_out_dma,
			gusmax_cont_in_dma,

			gusmax_speaker_ctl,

			gus_getdev,
			gus_setfd,
			gusmax_mixer_set_port,
			gusmax_mixer_get_port,
			gusmax_mixer_query_devinfo,
			1,				/* full-duplex */
			0,
		};
		sc->sc_flags |= GUS_CODEC_INSTALLED;
		sc->sc_codec.parent = sc;
		sc->sc_codec.sc_drq = sc->sc_recdrq;
		sc->sc_codec.sc_recdrq = sc->sc_drq;
		gus_hw_if = gusmax_hw_if;
		/* enable line in and mic in the GUS mixer; the codec chip
		   will do the real mixing for them. */
		sc->sc_mixcontrol &= ~GUSMASK_LINE_IN; /* 0 enables. */
		sc->sc_mixcontrol |= GUSMASK_MIC_IN; /* 1 enables. */
		outb(sc->sc_iobase+GUS_MIX_CONTROL, sc->sc_mixcontrol);
		
		ad1848_attach(&sc->sc_codec);
		/* turn on pre-MUX microphone gain. */
		ad1848_set_mic_gain(&sc->sc_codec, &vol);
	}
}


/*
 * Return info about the audio device, for the AUDIO_GETINFO ioctl
 */

int
gus_getdev(addr, dev)
	void * addr;
	struct audio_device *dev;
{
	*dev = gus_device;
	return 0;
}

/*
 * stubs (XXX)
 */

int
gus_set_in_gain(addr, gain, balance)
	caddr_t addr;
	u_int gain;
	u_char balance;
{
	DPRINTF(("gus_set_in_gain called\n"));
	return 0;
}

int
gus_get_in_gain(addr)
	caddr_t addr;
{
	DPRINTF(("gus_get_in_gain called\n"));
	return 0;
}

int
gusmax_set_out_port(addr, port)
	void * addr;
	int port;
{
	register struct ad1848_softc *sc = addr;
	return gus_set_out_port(sc->parent, port);
}

int
gus_set_out_port(addr, port)
	void * addr;
	int port;
{
	register struct gus_softc *sc = addr;
	DPRINTF(("gus_set_out_port called\n"));
	sc->sc_out_port = port;

	return 0;
}

int
gusmax_get_out_port(addr)
	void * addr;
{
	register struct ad1848_softc *sc = addr;
	return gus_get_out_port(sc->parent);
}

int
gus_get_out_port(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;
	DPRINTF(("gus_get_out_port() called\n"));
	return sc->sc_out_port;
}

int
gusmax_set_in_port(addr, port)
	void * addr;
	int port;
{
	register struct ad1848_softc *sc = addr;
	DPRINTF(("gusmax_set_in_port: %d\n", port));

	switch(port) {
	case GUSMAX_MONO_LVL:
		port = MIC_IN_PORT;
		break;
	case GUSMAX_LINE_IN_LVL:
		port = LINE_IN_PORT;
		break;
	case GUSMAX_DAC_LVL:
		port = AUX1_IN_PORT;
		break;
	case GUSMAX_MIX_IN:
		port = DAC_IN_PORT;
		break;
	default:
		return(EINVAL);
		/*NOTREACHED*/
	}
	return(ad1848_set_rec_port(sc, port));
}

int
gusmax_get_in_port(addr)
	void * addr;
{
	register struct ad1848_softc *sc = addr;
	int port = GUSMAX_MONO_LVL;
    
	switch(ad1848_get_rec_port(sc)) {
	case MIC_IN_PORT:
		port = GUSMAX_MONO_LVL;
		break;
	case LINE_IN_PORT:
		port = GUSMAX_LINE_IN_LVL;
		break;
	case DAC_IN_PORT:
		port = GUSMAX_MIX_IN;
		break;
	case AUX1_IN_PORT:
		port = GUSMAX_DAC_LVL;
		break;
	}

	DPRINTF(("gusmax_get_in_port: %d\n", port));

	return(port);
}

int
gus_set_in_port(addr, port)
	void * addr;
	int port;
{
	register struct gus_softc *sc = addr;
	DPRINTF(("gus_set_in_port called\n"));
	/*
	 * On the GUS with ICS mixer, the ADC input is after the mixer stage,
	 * so we can't set the input port.
	 *
	 * On the GUS with CS4231 codec/mixer, see gusmax_set_in_port().
	 */
	sc->sc_in_port = port;

	return 0;
}


int
gus_get_in_port(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;
	DPRINTF(("gus_get_in_port called\n"));
	return sc->sc_in_port;
}


int
gusmax_dma_input(addr, buf, size, callback, arg)
	void * addr;
	void *buf;
	int size;
	void (*callback)();
	void *arg;
{
	register struct ad1848_softc *sc = addr;
	return gus_dma_input(sc->parent, buf, size, callback, arg);
}

/*
 * Start sampling the input source into the requested DMA buffer.
 * Called at splgus(), either from top-half or from interrupt handler.
 */
int
gus_dma_input(addr, buf, size, callback, arg)
	void * addr;
	void *buf;
	int size;
	void (*callback)();
	void *arg;
{
	register struct gus_softc *sc = addr;
	register int port = sc->sc_iobase;
	register u_char dmac;
	DMAPRINTF(("gus_dma_input called\n"));
    
	/*
	 * Sample SIZE bytes of data from the card, into buffer at BUF.
	 */

	if (sc->sc_precision == 16)
	    return EINVAL;		/* XXX */

	/* set DMA modes */
	dmac = GUSMASK_SAMPLE_IRQ|GUSMASK_SAMPLE_START;
	if (sc->sc_recdrq >= 4)
		dmac |= GUSMASK_SAMPLE_DATA16;
	if (sc->sc_encoding == AUDIO_ENCODING_ULAW ||
	    sc->sc_encoding == AUDIO_ENCODING_PCM8)
	    dmac |= GUSMASK_SAMPLE_INVBIT;
	if (sc->sc_channels == 2)
	    dmac |= GUSMASK_SAMPLE_STEREO;
	isa_dmastart(B_READ, (caddr_t) buf, size, sc->sc_recdrq);

	DMAPRINTF(("gus_dma_input isa_dmastarted\n"));
	sc->sc_flags |= GUS_DMAIN_ACTIVE;
	sc->sc_dmainintr = callback;
	sc->sc_inarg = arg;
	sc->sc_dmaincnt = size;
	sc->sc_dmainaddr = buf;

	SELECT_GUS_REG(port, GUSREG_SAMPLE_CONTROL);
	outb(port+GUS_DATA_HIGH, dmac);	/* Go! */


	DMAPRINTF(("gus_dma_input returning\n"));

	return 0;
}

static int
gus_dmain_intr(sc)
	struct gus_softc *sc;
{
        void (*callback) __P((void *));
	void *arg;

	DMAPRINTF(("gus_dmain_intr called\n"));
	if (sc->sc_dmainintr) {
	    isa_dmadone(B_READ, sc->sc_dmainaddr, sc->sc_dmaincnt - 1,
			sc->sc_recdrq);
	    callback = sc->sc_dmainintr;
	    arg = sc->sc_inarg;

	    sc->sc_dmainaddr = 0;
	    sc->sc_dmaincnt = 0;
	    sc->sc_dmainintr = 0;
	    sc->sc_inarg = 0;

	    sc->sc_flags &= ~GUS_DMAIN_ACTIVE;
	    DMAPRINTF(("calling dmain_intr callback %x(%x)\n", callback, arg));
	    (*callback)(arg);
	    return 1;
	} else {
	    DMAPRINTF(("gus_dmain_intr false?\n"));
	    return 0;			/* XXX ??? */
	}
}

int
gusmax_halt_out_dma(addr)
	void * addr;
{
	register struct ad1848_softc *sc = addr;
	return gus_halt_out_dma(sc->parent);
}


int
gusmax_halt_in_dma(addr)
	void * addr;
{
	register struct ad1848_softc *sc = addr;
	return gus_halt_in_dma(sc->parent);
}

int
gusmax_cont_out_dma(addr)
	void * addr;
{
	register struct ad1848_softc *sc = addr;
	return gus_cont_out_dma(sc->parent);
}

int
gusmax_cont_in_dma(addr)
	void * addr;
{
	register struct ad1848_softc *sc = addr;
	return gus_cont_in_dma(sc->parent);
}

/*
 * Stop any DMA output.  Called at splgus().
 */
int
gus_halt_out_dma(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;
	register int port = sc->sc_iobase;

	DMAPRINTF(("gus_halt_out_dma called\n"));
	/*
	 * Make sure the GUS _isn't_ setup for DMA
	 */

 	SELECT_GUS_REG(port, GUSREG_DMA_CONTROL);
	outb(sc->sc_iobase+GUS_DATA_HIGH, 0);

	untimeout(gus_dmaout_timeout, sc);
	isa_dmaabort(sc->sc_drq);
	sc->sc_flags &= ~(GUS_DMAOUT_ACTIVE|GUS_LOCKED);
	sc->sc_dmaoutintr = 0;
	sc->sc_outarg = 0;
	sc->sc_dmaoutaddr = 0;
	sc->sc_dmaoutcnt = 0;
	sc->sc_dmabuf = 0;
	sc->sc_bufcnt = 0;
	sc->sc_playbuf = -1;
	/* also stop playing */
	gus_stop_voice(sc, GUS_VOICE_LEFT, 1);
	gus_stop_voice(sc, GUS_VOICE_RIGHT, 0);

	return 0;
}

/*
 * Stop any DMA output.  Called at splgus().
 */
int
gus_halt_in_dma(addr)
	void * addr;
{
	register struct gus_softc *sc = addr;
	register int port = sc->sc_iobase;
	DMAPRINTF(("gus_halt_in_dma called\n"));

	/*
	 * Make sure the GUS _isn't_ setup for DMA
	 */

 	SELECT_GUS_REG(port, GUSREG_SAMPLE_CONTROL);
	outb(port+GUS_DATA_HIGH,
	     inb(port+GUS_DATA_HIGH) & ~(GUSMASK_SAMPLE_START|GUSMASK_SAMPLE_IRQ));

	isa_dmaabort(sc->sc_recdrq);
	sc->sc_flags &= ~GUS_DMAIN_ACTIVE;
	sc->sc_dmainintr = 0;
	sc->sc_inarg = 0;
	sc->sc_dmainaddr = 0;
	sc->sc_dmaincnt = 0;

	return 0;
}

int
gus_cont_out_dma(addr)
	void * addr;
{
	DPRINTF(("gus_cont_out_dma called\n"));
	return EOPNOTSUPP;
}

int
gus_cont_in_dma(addr)
	void * addr;
{
	DPRINTF(("gus_cont_in_dma called\n"));
	return EOPNOTSUPP;
}


static int
gus_setfd(addr, flag)
	void *addr;
	int flag;
{
    if (gus_hw_if.full_duplex == 0)
	 return ENOTTY;

    return(0);				/* nothing fancy to do. */
}

static inline int
gus_to_vol(cp, vol)
	mixer_ctrl_t *cp;
	struct ad1848_volume *vol;
{
	if (cp->un.value.num_channels == 1) {
		vol->left = vol->right = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		return(1);
	}
	else if (cp->un.value.num_channels == 2) {
		vol->left  = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
		vol->right = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		return(1);
	}
	return(0);
}

static inline int
gus_from_vol(cp, vol)
	mixer_ctrl_t *cp;
	struct ad1848_volume *vol;
{
	if (cp->un.value.num_channels == 1) {
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = vol->left;
		return(1);
	}
	else if (cp->un.value.num_channels == 2) {
		cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = vol->left;
		cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = vol->right;
		return(1);
	}
	return(0);
}

static int
gusmax_mixer_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	struct ad1848_volume vol;
	u_char eq;
	int error = EINVAL;
    
	DPRINTF(("gusmax_mixer_get_port: port=%d\n", cp->dev));

	switch (cp->dev) {
#if 0 /* use mono level instead */
	case GUSMAX_MIC_IN_LVL:	/* Microphone */
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = ad1848_get_mic_gain(ac, &vol);
			if (!error)
				gus_from_vol(cp, &vol);
		}
		break;
#endif

	case GUSMAX_DAC_LVL:		/* dac out */
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = ad1848_get_aux1_gain(ac, &vol);
			if (!error)
				gus_from_vol(cp, &vol);
		}
		break;

	case GUSMAX_LINE_IN_LVL:	/* line in */
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = cs4231_get_linein_gain(ac, &vol);
			if (!error)
				gus_from_vol(cp, &vol);
		}
		break;

	case GUSMAX_MONO_LVL:	/* mono */
		if (cp->type == AUDIO_MIXER_VALUE &&
		    cp->un.value.num_channels == 1) {
			error = cs4231_get_mono_gain(ac, &vol);
			if (!error)
				gus_from_vol(cp, &vol);
		}
		break;

	case GUSMAX_CD_LVL:	/* CD */
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = ad1848_get_aux2_gain(ac, &vol);
			if (!error)
				gus_from_vol(cp, &vol);
		}
		break;

	case GUSMAX_MONITOR_LVL:	/* monitor level */
		if (cp->type == AUDIO_MIXER_VALUE &&
		    cp->un.value.num_channels == 1) {
			error = ad1848_get_mon_gain(ac, &vol);
			if (!error)
				cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
					vol.left;
		}
		break;

	case GUSMAX_OUT_LVL:	/* output level */
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = ad1848_get_out_gain(ac, &vol);
			if (!error)
				gus_from_vol(cp, &vol);
		}
		break;

	case GUSMAX_SPEAKER_LVL:	/* fake speaker for mute naming */
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (sc->sc_mixcontrol & GUSMASK_LINE_OUT)
				vol.left = vol.right = AUDIO_MAX_GAIN;
			else
				vol.left = vol.right = AUDIO_MIN_GAIN;
			error = 0;
			gus_from_vol(cp, &vol);
		}
		break;

	case GUSMAX_LINE_IN_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = ac->line_mute;
			error = 0;
		}
		break;


	case GUSMAX_DAC_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = ac->aux1_mute;
			error = 0;
		}
		break;

	case GUSMAX_CD_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = ac->aux2_mute;
			error = 0;
		}
		break;

	case GUSMAX_MONO_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = ac->mono_mute;
			error = 0;
		}
		break;

	case GUSMAX_MONITOR_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = ac->mon_mute;
			error = 0;
		}
		break;

	case GUSMAX_SPEAKER_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = sc->sc_mixcontrol & GUSMASK_LINE_OUT ? 1 : 0;
			error = 0;
		}
		break;

	case GUSMAX_REC_LVL:		/* record level */
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = ad1848_get_rec_gain(ac, &vol);
			if (!error)
				gus_from_vol(cp, &vol);
		}
		break;

	case GUSMAX_RECORD_SOURCE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = ad1848_get_rec_port(ac);
			error = 0;
		}
		break;

	default:
		error = ENXIO;
		break;
	}

	return(error);
}

static int
gus_mixer_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	register struct gus_softc *sc = addr;
	register struct ics2101_softc *ic = &sc->sc_mixer;
	struct ad1848_volume vol;
	int error = EINVAL;
	u_int mute;

	DPRINTF(("gus_mixer_get_port: dev=%d type=%d\n", cp->dev, cp->type));

	if (!HAS_MIXER(sc) && cp->dev > GUSICS_MASTER_MUTE)
		return ENXIO;
    
	switch (cp->dev) {

	case GUSICS_MIC_IN_MUTE:	/* Microphone */
		if (cp->type == AUDIO_MIXER_ENUM) {
			if (HAS_MIXER(sc))
				cp->un.ord = ic->sc_mute[GUSMIX_CHAN_MIC][ICSMIX_LEFT];
			else
				cp->un.ord =
				    sc->sc_mixcontrol & GUSMASK_MIC_IN ? 0 : 1;
			error = 0;
		}
		break;

	case GUSICS_LINE_IN_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			if (HAS_MIXER(sc))
				cp->un.ord = ic->sc_mute[GUSMIX_CHAN_LINE][ICSMIX_LEFT];
			else
				cp->un.ord =
				    sc->sc_mixcontrol & GUSMASK_LINE_IN ? 1 : 0;
			error = 0;
		}
		break;

	case GUSICS_MASTER_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			if (HAS_MIXER(sc))
				cp->un.ord = ic->sc_mute[GUSMIX_CHAN_MASTER][ICSMIX_LEFT];
			else
				cp->un.ord =
				    sc->sc_mixcontrol & GUSMASK_LINE_OUT ? 1 : 0;
			error = 0;
		}
		break;

	case GUSICS_DAC_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = ic->sc_mute[GUSMIX_CHAN_DAC][ICSMIX_LEFT];
			error = 0;
		}
		break;

	case GUSICS_CD_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = ic->sc_mute[GUSMIX_CHAN_CD][ICSMIX_LEFT];
			error = 0;
		}
		break;

	case GUSICS_MASTER_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			vol.left = ic->sc_setting[GUSMIX_CHAN_MASTER][ICSMIX_LEFT];
			vol.right = ic->sc_setting[GUSMIX_CHAN_MASTER][ICSMIX_RIGHT];
			if (gus_from_vol(cp, &vol))
				error = 0;
		}
		break;

	case GUSICS_MIC_IN_LVL:	/* Microphone */
		if (cp->type == AUDIO_MIXER_VALUE) {
			vol.left = ic->sc_setting[GUSMIX_CHAN_MIC][ICSMIX_LEFT];
			vol.right = ic->sc_setting[GUSMIX_CHAN_MIC][ICSMIX_RIGHT];
			if (gus_from_vol(cp, &vol))
				error = 0;
		}
		break;
	
	case GUSICS_LINE_IN_LVL:	/* line in */
		if (cp->type == AUDIO_MIXER_VALUE) {
			vol.left = ic->sc_setting[GUSMIX_CHAN_LINE][ICSMIX_LEFT];
			vol.right = ic->sc_setting[GUSMIX_CHAN_LINE][ICSMIX_RIGHT];
			if (gus_from_vol(cp, &vol))
				error = 0;
		}
		break;


	case GUSICS_CD_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			vol.left = ic->sc_setting[GUSMIX_CHAN_CD][ICSMIX_LEFT];
			vol.right = ic->sc_setting[GUSMIX_CHAN_CD][ICSMIX_RIGHT];
			if (gus_from_vol(cp, &vol))
				error = 0;
		}
		break;

	case GUSICS_DAC_LVL:		/* dac out */
		if (cp->type == AUDIO_MIXER_VALUE) {
			vol.left = ic->sc_setting[GUSMIX_CHAN_DAC][ICSMIX_LEFT];
			vol.right = ic->sc_setting[GUSMIX_CHAN_DAC][ICSMIX_RIGHT];
			if (gus_from_vol(cp, &vol))
				error = 0;
		}
		break;


	case GUSICS_RECORD_SOURCE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			/* Can't set anything else useful, sigh. */
			 cp->un.ord = 0;
		}
		break;

	default:
		return ENXIO;
	    /*NOTREACHED*/
	}
	return error;
}

static void
gusics_master_mute(ic, mute)
	struct ics2101_softc *ic;
	int mute;
{
	ics2101_mix_mute(ic, GUSMIX_CHAN_MASTER, ICSMIX_LEFT, mute);
	ics2101_mix_mute(ic, GUSMIX_CHAN_MASTER, ICSMIX_RIGHT, mute);
}

static void
gusics_mic_mute(ic, mute)
	struct ics2101_softc *ic;
	int mute;
{
	ics2101_mix_mute(ic, GUSMIX_CHAN_MIC, ICSMIX_LEFT, mute);
	ics2101_mix_mute(ic, GUSMIX_CHAN_MIC, ICSMIX_RIGHT, mute);
}

static void
gusics_linein_mute(ic, mute)
	struct ics2101_softc *ic;
	int mute;
{
	ics2101_mix_mute(ic, GUSMIX_CHAN_LINE, ICSMIX_LEFT, mute);
	ics2101_mix_mute(ic, GUSMIX_CHAN_LINE, ICSMIX_RIGHT, mute);
}

static void
gusics_cd_mute(ic, mute)
	struct ics2101_softc *ic;
	int mute;
{
	ics2101_mix_mute(ic, GUSMIX_CHAN_CD, ICSMIX_LEFT, mute);
	ics2101_mix_mute(ic, GUSMIX_CHAN_CD, ICSMIX_RIGHT, mute);
}

static void
gusics_dac_mute(ic, mute)
	struct ics2101_softc *ic;
	int mute;
{
	ics2101_mix_mute(ic, GUSMIX_CHAN_DAC, ICSMIX_LEFT, mute);
	ics2101_mix_mute(ic, GUSMIX_CHAN_DAC, ICSMIX_RIGHT, mute);
}

static int
gusmax_mixer_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;
	struct ad1848_volume vol;
	int error = EINVAL;
    
	DPRINTF(("gusmax_mixer_set_port: dev=%d type=%d\n", cp->dev, cp->type));

	switch (cp->dev) {
#if 0
	case GUSMAX_MIC_IN_LVL:	/* Microphone */
		if (cp->type == AUDIO_MIXER_VALUE &&
		    cp->un.value.num_channels == 1) {
			/* XXX enable/disable pre-MUX fixed gain */
			if (gus_to_vol(cp, &vol))
				error = ad1848_set_mic_gain(ac, &vol);
		}
		break;
#endif
	
	case GUSMAX_DAC_LVL:		/* dac out */
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol))
				error = ad1848_set_aux1_gain(ac, &vol);
		}
		break;

	case GUSMAX_LINE_IN_LVL:	/* line in */
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol))
				error = cs4231_set_linein_gain(ac, &vol);
		}
		break;

	case GUSMAX_MONO_LVL:	/* mic/mono in */
		if (cp->type == AUDIO_MIXER_VALUE &&
		    cp->un.value.num_channels == 1) {
			if (gus_to_vol(cp, &vol))
				error = cs4231_set_mono_gain(ac, &vol);
		}
		break;

	case GUSMAX_CD_LVL:	/* CD: AUX2 */
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol))
				error = ad1848_set_aux2_gain(ac, &vol);
		}
		break;

	case GUSMAX_MONITOR_LVL:
		if (cp->type == AUDIO_MIXER_VALUE &&
		    cp->un.value.num_channels == 1) {
			vol.left  = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			error = ad1848_set_mon_gain(ac, &vol);
		}
		break;

	case GUSMAX_OUT_LVL:	/* output volume */
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol))
				error = ad1848_set_out_gain(ac, &vol);
		}
		break;

	case GUSMAX_SPEAKER_LVL:
		if (cp->type == AUDIO_MIXER_VALUE &&
		    cp->un.value.num_channels == 1) {
			if (gus_to_vol(cp, &vol)) {
				gus_speaker_ctl(sc, vol.left > AUDIO_MIN_GAIN ?
						SPKR_ON : SPKR_OFF);
				error = 0;
			}
		}
		break;

	case GUSMAX_LINE_IN_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			ac->line_mute = cp->un.ord ? 1 : 0;
			DPRINTF(("line mute %d\n", cp->un.ord));
			cs4231_mute_line(ac, ac->line_mute);
			gus_linein_ctl(sc, ac->line_mute ? SPKR_OFF : SPKR_ON);
			error = 0;
		}
		break;

	case GUSMAX_DAC_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			ac->aux1_mute = cp->un.ord ? 1 : 0;
			DPRINTF(("dac mute %d\n", cp->un.ord));
			ad1848_mute_aux1(ac, ac->aux1_mute);
			error = 0;
		}
		break;

	case GUSMAX_CD_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			ac->aux2_mute = cp->un.ord ? 1 : 0;
			DPRINTF(("cd mute %d\n", cp->un.ord));
			ad1848_mute_aux2(ac, ac->aux2_mute);
			error = 0;
		}
		break;

	case GUSMAX_MONO_MUTE:	/* Microphone */
		if (cp->type == AUDIO_MIXER_ENUM) {
			ac->mono_mute = cp->un.ord ? 1 : 0;
			DPRINTF(("mono mute %d\n", cp->un.ord));
			cs4231_mute_mono(ac, ac->mono_mute);
			gus_mic_ctl(sc, ac->mono_mute ? SPKR_OFF : SPKR_ON);
			error = 0;
		}
		break;

	case GUSMAX_MONITOR_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			ac->mon_mute = cp->un.ord ? 1 : 0;
			DPRINTF(("mono mute %d\n", cp->un.ord));
			cs4231_mute_monitor(ac, ac->mon_mute);
			error = 0;
		}
		break;

	case GUSMAX_SPEAKER_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			gus_speaker_ctl(sc, cp->un.ord ? SPKR_OFF : SPKR_ON);
			error = 0;
		}
		break;

	case GUSMAX_REC_LVL:		/* record level */
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol))
				error = ad1848_set_rec_gain(ac, &vol);
		}
		break;
	
	case GUSMAX_RECORD_SOURCE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			error = ad1848_set_rec_port(ac, cp->un.ord);
		}
		break;

	default:
		return ENXIO;
	    /*NOTREACHED*/
    }
    return error;
}

static int
gus_mixer_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	register struct gus_softc *sc = addr;
	register struct ics2101_softc *ic = &sc->sc_mixer;
	struct ad1848_volume vol;
	int error = EINVAL;
	u_int mute;

	DPRINTF(("gus_mixer_set_port: dev=%d type=%d\n", cp->dev, cp->type));

	if (!HAS_MIXER(sc) && cp->dev > GUSICS_MASTER_MUTE)
		return ENXIO;
    
	switch (cp->dev) {

	case GUSICS_MIC_IN_MUTE:	/* Microphone */
		if (cp->type == AUDIO_MIXER_ENUM) {
			DPRINTF(("mic mute %d\n", cp->un.ord));
			if (HAS_MIXER(sc)) {
				gusics_mic_mute(ic, cp->un.ord);
			}
			gus_mic_ctl(sc, cp->un.ord ? SPKR_OFF : SPKR_ON);
			error = 0;
		}
		break;

	case GUSICS_LINE_IN_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			DPRINTF(("linein mute %d\n", cp->un.ord));
			if (HAS_MIXER(sc)) {
				gusics_linein_mute(ic, cp->un.ord);
			}
			gus_linein_ctl(sc, cp->un.ord ? SPKR_OFF : SPKR_ON);
			error = 0;
		}
		break;

	case GUSICS_MASTER_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			DPRINTF(("master mute %d\n", cp->un.ord));
			if (HAS_MIXER(sc)) {
				gusics_master_mute(ic, cp->un.ord);
			}
			gus_speaker_ctl(sc, cp->un.ord ? SPKR_OFF : SPKR_ON);
			error = 0;
		}
		break;

	case GUSICS_DAC_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			gusics_dac_mute(ic, cp->un.ord);
			error = 0;
		}
		break;

	case GUSICS_CD_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			gusics_cd_mute(ic, cp->un.ord);
			error = 0;
		}
		break;

	case GUSICS_MASTER_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol)) {
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_MASTER,
						      ICSMIX_LEFT,
						      vol.left);
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_MASTER,
						      ICSMIX_RIGHT,
						      vol.right);
				error = 0;
			}
		}
		break;

	case GUSICS_MIC_IN_LVL:	/* Microphone */
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol)) {
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_MIC,
						      ICSMIX_LEFT,
						      vol.left);
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_MIC,
						      ICSMIX_RIGHT,
						      vol.right);
				error = 0;
			}
		}
		break;
	
	case GUSICS_LINE_IN_LVL:	/* line in */
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol)) {
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_LINE,
						      ICSMIX_LEFT,
						      vol.left);
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_LINE,
						      ICSMIX_RIGHT,
						      vol.right);
				error = 0;
			}
		}
		break;


	case GUSICS_CD_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol)) {
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_CD,
						      ICSMIX_LEFT,
						      vol.left);
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_CD,
						      ICSMIX_RIGHT,
						      vol.right);
				error = 0;
			}
		}
		break;

	case GUSICS_DAC_LVL:		/* dac out */
		if (cp->type == AUDIO_MIXER_VALUE) {
			if (gus_to_vol(cp, &vol)) {
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_DAC,
						      ICSMIX_LEFT,
						      vol.left);
				ics2101_mix_attenuate(ic,
						      GUSMIX_CHAN_DAC,
						      ICSMIX_RIGHT,
						      vol.right);
				error = 0;
			}
		}
		break;


	case GUSICS_RECORD_SOURCE:
		if (cp->type == AUDIO_MIXER_ENUM && cp->un.ord == 0) {
			/* Can't set anything else useful, sigh. */
			error = 0;
		}
		break;

	default:
		return ENXIO;
	    /*NOTREACHED*/
	}
	return error;
}

static int
gusmax_mixer_query_devinfo(addr, dip)
	void *addr;
	register mixer_devinfo_t *dip;
{
	register struct ad1848_softc *ac = addr;
	register struct gus_softc *sc = ac->parent;

	DPRINTF(("gusmax_query_devinfo: index=%d\n", dip->index));

	switch(dip->index) {
	case GUSMAX_MIX_IN:	/* mixed MUX input */
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = GUSMAX_INPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmixerout);
		dip->un.e.num_mem = 0;		/* XXX */
		break;

#if 0
    case GUSMAX_MIC_IN_LVL:	/* Microphone */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = GUSMAX_MIC_IN_MUTE;
	strcpy(dip->label.name, AudioNmicrophone);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;
#endif

    case GUSMAX_MONO_LVL:	/* mono/microphone mixer */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = GUSMAX_MONO_MUTE;
	strcpy(dip->label.name, AudioNmicrophone);
	dip->un.v.num_channels = 1;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case GUSMAX_DAC_LVL:		/*  dacout */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = GUSMAX_DAC_MUTE;
	strcpy(dip->label.name, AudioNdac);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case GUSMAX_LINE_IN_LVL:	/* line */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = GUSMAX_LINE_IN_MUTE;
	strcpy(dip->label.name, AudioNline);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case GUSMAX_CD_LVL:		/* cd */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = GUSMAX_CD_MUTE;
	strcpy(dip->label.name, AudioNcd);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;


    case GUSMAX_MONITOR_LVL:	/* monitor level */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = GUSMAX_MONITOR_CLASS;
	dip->next = GUSMAX_MONITOR_MUTE;
	dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNmonitor);
	dip->un.v.num_channels = 1;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case GUSMAX_OUT_LVL:		/* cs4231 output volume: not useful? */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = GUSMAX_MONITOR_CLASS;
	dip->prev = dip->next = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNoutput);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case GUSMAX_SPEAKER_LVL:		/* fake speaker volume */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = GUSMAX_MONITOR_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = GUSMAX_SPEAKER_MUTE;
	strcpy(dip->label.name, AudioNspeaker);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case GUSMAX_LINE_IN_MUTE:
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = GUSMAX_LINE_IN_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;
	
    case GUSMAX_DAC_MUTE:
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = GUSMAX_DAC_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;

    case GUSMAX_CD_MUTE:
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = GUSMAX_CD_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;
	
    case GUSMAX_MONO_MUTE:
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = GUSMAX_MONO_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;

    case GUSMAX_MONITOR_MUTE:
	dip->mixer_class = GUSMAX_OUTPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = GUSMAX_MONITOR_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;

    case GUSMAX_SPEAKER_MUTE:
	dip->mixer_class = GUSMAX_OUTPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = GUSMAX_SPEAKER_LVL;
	dip->next = AUDIO_MIXER_LAST;
    mute:
	strcpy(dip->label.name, AudioNmute);
	dip->un.e.num_mem = 2;
	strcpy(dip->un.e.member[0].label.name, AudioNoff);
	dip->un.e.member[0].ord = 0;
	strcpy(dip->un.e.member[1].label.name, AudioNon);
	dip->un.e.member[1].ord = 1;
	break;
	
    case GUSMAX_REC_LVL:	/* record level */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = GUSMAX_RECORD_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = GUSMAX_RECORD_SOURCE;
	strcpy(dip->label.name, AudioNrecord);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case GUSMAX_RECORD_SOURCE:
	dip->mixer_class = GUSMAX_RECORD_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = GUSMAX_REC_LVL;
	dip->next = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNsource);
	dip->un.e.num_mem = 4;
	strcpy(dip->un.e.member[0].label.name, AudioNoutput);
	dip->un.e.member[0].ord = GUSMAX_MIX_IN;
	strcpy(dip->un.e.member[1].label.name, AudioNmicrophone);
	dip->un.e.member[1].ord = GUSMAX_MONO_LVL;
	strcpy(dip->un.e.member[2].label.name, AudioNdac);
	dip->un.e.member[2].ord = GUSMAX_DAC_LVL;
	strcpy(dip->un.e.member[3].label.name, AudioNline);
	dip->un.e.member[3].ord = GUSMAX_LINE_IN_LVL;
	break;

    case GUSMAX_INPUT_CLASS:			/* input class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = GUSMAX_INPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCInputs);
	break;

    case GUSMAX_OUTPUT_CLASS:			/* output class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = GUSMAX_OUTPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCOutputs);
	break;

    case GUSMAX_MONITOR_CLASS:			/* monitor class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = GUSMAX_MONITOR_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCMonitor);
	break;
	    
    case GUSMAX_RECORD_CLASS:			/* record source class */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = GUSMAX_RECORD_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCRecord);
	break;

    default:
	return ENXIO;
	/*NOTREACHED*/
    }
    DPRINTF(("AUDIO_MIXER_DEVINFO: name=%s\n", dip->label.name));
	return 0;
}

static int
gus_mixer_query_devinfo(addr, dip)
	void *addr;
	register mixer_devinfo_t *dip;
{
	register struct gus_softc *sc = addr;

	DPRINTF(("gusmax_query_devinfo: index=%d\n", dip->index));

	if (!HAS_MIXER(sc) && dip->index > GUSICS_MASTER_MUTE)
		return ENXIO;

	switch(dip->index) {

	case GUSICS_MIC_IN_LVL:	/* Microphone */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = GUSICS_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = GUSICS_MIC_IN_MUTE;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case GUSICS_LINE_IN_LVL:	/* line */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = GUSICS_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = GUSICS_LINE_IN_MUTE;
		strcpy(dip->label.name, AudioNline);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case GUSICS_CD_LVL:		/* cd */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = GUSICS_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = GUSICS_CD_MUTE;
		strcpy(dip->label.name, AudioNcd);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case GUSICS_DAC_LVL:		/*  dacout */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = GUSICS_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = GUSICS_DAC_MUTE;
		strcpy(dip->label.name, AudioNdac);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case GUSICS_MASTER_LVL:		/*  master output */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = GUSICS_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = GUSICS_MASTER_MUTE;
		strcpy(dip->label.name, AudioNvolume);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;


	case GUSICS_LINE_IN_MUTE:
		dip->mixer_class = GUSICS_INPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = GUSICS_LINE_IN_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	
	case GUSICS_DAC_MUTE:
		dip->mixer_class = GUSICS_INPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = GUSICS_DAC_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;

	case GUSICS_CD_MUTE:
		dip->mixer_class = GUSICS_INPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = GUSICS_CD_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;
	
	case GUSICS_MIC_IN_MUTE:
		dip->mixer_class = GUSICS_INPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = GUSICS_MIC_IN_LVL;
		dip->next = AUDIO_MIXER_LAST;
		goto mute;

	case GUSICS_MASTER_MUTE:
		dip->mixer_class = GUSICS_OUTPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = GUSICS_MASTER_LVL;
		dip->next = AUDIO_MIXER_LAST;
mute:
		strcpy(dip->label.name, AudioNmute);
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		break;
	
	case GUSICS_RECORD_SOURCE:
		dip->mixer_class = GUSICS_RECORD_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->un.e.num_mem = 1;
		strcpy(dip->un.e.member[0].label.name, AudioNoutput);
		dip->un.e.member[0].ord = GUSICS_MASTER_LVL;
		break;

	case GUSICS_INPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = GUSICS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCInputs);
		break;

	case GUSICS_OUTPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = GUSICS_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCOutputs);
		break;

	case GUSICS_RECORD_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = GUSICS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCRecord);
		break;

	default:
		return ENXIO;
	/*NOTREACHED*/
	}
	DPRINTF(("AUDIO_MIXER_DEVINFO: name=%s\n", dip->label.name));
	return 0;
}

static int
gus_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
	register struct gus_softc *sc = addr;

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEmulaw);
		fp->format_id = AUDIO_ENCODING_ULAW;
		break;
	case 1:
		strcpy(fp->name, AudioEpcm16);
		fp->format_id = AUDIO_ENCODING_PCM16;
		break;
	case 2:
		strcpy(fp->name, AudioEpcm8);
		fp->format_id = AUDIO_ENCODING_PCM8;
		break;
	default:
		return(EINVAL);
		/*NOTREACHED*/
	}
	return (0);
}

/*
 * Setup the ICS mixer in "transparent" mode: reset everything to a sensible
 * level.  Levels as suggested by GUS SDK code.
 */

static void
gus_init_ics2101(sc)
	struct gus_softc *sc;
{
	register int port = sc->sc_iobase;
	register struct ics2101_softc *ic = &sc->sc_mixer;
	sc->sc_mixer.sc_selio = port+GUS_MIXER_SELECT;
	sc->sc_mixer.sc_dataio = port+GUS_MIXER_DATA;
	sc->sc_mixer.sc_flags = (sc->sc_revision == 5) ? ICS_FLIP : 0;

	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_MIC,
			      ICSMIX_LEFT,
			      ICSMIX_MIN_ATTN);
	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_MIC,
			      ICSMIX_RIGHT,
			      ICSMIX_MIN_ATTN);
	/*
	 * Start with microphone muted by the mixer...
	 */
	gusics_mic_mute(ic, 1);

	/* ... and enabled by the GUS master mix control */
	gus_mic_ctl(sc, SPKR_ON);

	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_LINE,
			      ICSMIX_LEFT,
			      ICSMIX_MIN_ATTN);
	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_LINE,
			      ICSMIX_RIGHT,
			      ICSMIX_MIN_ATTN);

	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_CD,
			      ICSMIX_LEFT,
			      ICSMIX_MIN_ATTN);
	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_CD,
			      ICSMIX_RIGHT,
			      ICSMIX_MIN_ATTN);

	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_DAC,
			      ICSMIX_LEFT,
			      ICSMIX_MIN_ATTN);
	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_DAC,
			      ICSMIX_RIGHT,
			      ICSMIX_MIN_ATTN);

	ics2101_mix_attenuate(ic,
			      ICSMIX_CHAN_4,
			      ICSMIX_LEFT,
			      ICSMIX_MAX_ATTN);
	ics2101_mix_attenuate(ic,
			      ICSMIX_CHAN_4,
			      ICSMIX_RIGHT,
			      ICSMIX_MAX_ATTN);

	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_MASTER,
			      ICSMIX_LEFT,
			      ICSMIX_MIN_ATTN);
	ics2101_mix_attenuate(ic,
			      GUSMIX_CHAN_MASTER,
			      ICSMIX_RIGHT,
			      ICSMIX_MIN_ATTN);
	/* unmute other stuff: */
	gusics_cd_mute(ic, 0);
	gusics_dac_mute(ic, 0);
	gusics_linein_mute(ic, 0);
	return;
}


#endif /* NGUS */
