/*	$OpenBSD: ad1848var.h,v 1.7 1998/05/08 18:37:20 csapuntz Exp $	*/
/*	$NetBSD: ad1848var.h,v 1.22 1998/01/19 22:18:26 augustss Exp $	*/

/*
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define AD1848_NPORT	4

struct ad1848_volume {
	u_char	left;
	u_char	right;
};

struct ad1848_softc {
	struct	device sc_dev;		/* base device */
	struct	isadev sc_id;		/* ISA device */
	void	*sc_ih;			/* interrupt vectoring */
	bus_space_tag_t sc_iot;		/* tag */
	bus_space_handle_t sc_ioh;	/* handle */
	int	sc_iooffs;		/* offset from handle */

	void	*parent;
	struct	device *sc_isa;		/* ISA bus's device */

	u_short	sc_locked;		/* true when doing HS DMA  */
	u_int	sc_lastcc;		/* size of last DMA xfer */
	int	sc_mode;		/* half-duplex record/play */
	
#ifndef NEWCONFIG
	int	sc_dma_flags;
	void	*sc_dma_bp;
	u_int	sc_dma_cnt;
#endif

	char	sc_playrun;		/* running in continuous mode */
	char	sc_recrun;		/* running in continuous mode */
#define NOTRUNNING 0
#define DMARUNNING 1
#define PCMRUNNING 2

	int	sc_irq;			/* interrupt */
	int	sc_drq;			/* DMA */
	int	sc_recdrq;		/* record/capture DMA */
	
	/* We keep track of these */
        struct ad1848_volume gains[6];

	struct ad1848_volume rec_gain;

	int	rec_port;		/* recording port */

	/* ad1848 */
	u_char	MCE_bit;
	char	mic_gain_on;		/* CS4231 only */
        char    mute[6];

	char	*chip_name;
	int	mode;
	
	u_int	precision;		/* 8/16 bits */
	int	channels;
	
	u_char	speed_bits;
	u_char	format_bits;
	u_char	need_commit;

	u_long	sc_interrupts;		/* number of interrupts taken */
	void	(*sc_intr)(void *);	/* dma completion intr handler */
	void	*sc_arg;		/* arg for sc_intr() */

	/* Only used by pss XXX */
	int	sc_iobase;
};

#define MUTE_LEFT       1
#define MUTE_RIGHT      2
#define MUTE_ALL        (MUTE_LEFT | MUTE_RIGHT)
#define MUTE_MONO       MUTE_ALL

/* Don't change this ordering without seriously looking around.
   These are indexes into mute[] array and into a register information
   array */
#define AD1848_AUX2_CHANNEL        0
#define AD1848_AUX1_CHANNEL        1
#define AD1848_DAC_CHANNEL         2
#define AD1848_LINE_CHANNEL        3
#define AD1848_MONO_CHANNEL        4
#define AD1848_MONITOR_CHANNEL     5    /* Doesn't seem to be on all later chips */

/*
 * Ad1848 ports
 */
#define MIC_IN_PORT	0
#define LINE_IN_PORT	1
#define AUX1_IN_PORT	2
#define DAC_IN_PORT	3

#ifdef _KERNEL

#define AD1848_KIND_LVL   0
#define AD1848_KIND_MUTE  1
#define AD1848_KIND_RECORDGAIN 2
#define AD1848_KIND_MICGAIN 3
#define AD1848_KIND_RECORDSOURCE 4

typedef struct ad1848_devmap {
  int  id;
  int  kind;
  int  dev;
} ad1848_devmap_t;

static __inline int ad1848_to_vol __P((mixer_ctrl_t *, struct ad1848_volume *));
static __inline int ad1848_from_vol __P((mixer_ctrl_t *, struct ad1848_volume *));

static __inline int
ad1848_to_vol(cp, vol)
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

static __inline int
ad1848_from_vol(cp, vol)
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


int     ad1848_mixer_get_port __P((struct ad1848_softc *, ad1848_devmap_t *, int cnt, mixer_ctrl_t *));
int     ad1848_mixer_set_port __P((struct ad1848_softc *, ad1848_devmap_t *, int, mixer_ctrl_t *));
int	ad1848_mapprobe __P((struct ad1848_softc *, int));
int	ad1848_probe __P((struct ad1848_softc *));
void	ad1848_unmap __P((struct ad1848_softc *));
void	ad1848_attach __P((struct ad1848_softc *));

int	ad1848_open __P((void *, int));
void	ad1848_close __P((void *));
    
void	ad1848_forceintr __P((struct ad1848_softc *));

int	ad1848_query_encoding __P((void *, struct audio_encoding *));
int	ad1848_set_params __P((void *, int, int, struct audio_params *, struct audio_params *));

int	ad1848_round_blocksize __P((void *, int));

int	ad1848_dma_init_output __P((void *, void *, int));
int	ad1848_dma_init_input __P((void *, void *, int));
int	ad1848_dma_output __P((void *, void *, int, void (*)(void *), void*));
int	ad1848_dma_input __P((void *, void *, int, void (*)(void *), void*));

int	ad1848_commit_settings __P((void *));

int	ad1848_halt_in_dma __P((void *));
int	ad1848_halt_out_dma __P((void *));

int	ad1848_intr __P((void *));

int	ad1848_set_rec_port __P((struct ad1848_softc *, int));
int	ad1848_get_rec_port __P((struct ad1848_softc *));

int	ad1848_set_channel_gain __P((struct ad1848_softc *, int, struct ad1848_volume *));
int	ad1848_get_device_gain __P((struct ad1848_softc *, int, struct ad1848_volume *));
int	ad1848_set_rec_gain __P((struct ad1848_softc *, struct ad1848_volume *));
int	ad1848_get_rec_gain __P((struct ad1848_softc *, struct ad1848_volume *));
/* Note: The mic pre-MUX gain is not a variable gain, it's 20dB or 0dB */
int	ad1848_set_mic_gain __P((struct ad1848_softc *, struct ad1848_volume *));
int	ad1848_get_mic_gain __P((struct ad1848_softc *, struct ad1848_volume *));
void     ad1848_mute_channel __P((struct ad1848_softc *, int device, int mute));

void   *ad1848_malloc __P((void *, unsigned long, int, int));
void	ad1848_free __P((void *, void *, int));
unsigned long ad1848_round __P((void *, unsigned long));
int	ad1848_mappage __P((void *, void *, int, int));

int	ad1848_get_props __P((void *));

#endif
