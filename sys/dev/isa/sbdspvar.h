/*	$NetBSD: sbdspvar.h,v 1.7 1995/11/10 05:01:08 mycroft Exp $	*/

/*
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

#define SB_MIC_PORT	0
#define SB_SPEAKER	1
#define SB_LINE_IN_PORT	2
#define SB_DAC_PORT	3
#define SB_FM_PORT	4
#define SB_CD_PORT	5
#define SB_MASTER_VOL	6
#define SB_TREBLE	7
#define SB_BASS		8
#define SB_NDEVS	9

#define SB_OUTPUT_MODE	9
#define 	SB_SPKR_MONO	0
#define 	SB_SPKR_STEREO	1

#define	SB_RECORD_SOURCE 10

#define SB_INPUT_CLASS	11
#define SB_OUTPUT_CLASS	12
#define SB_RECORD_CLASS	13


/*
 * Software state, per SoundBlaster card.
 * The soundblaster has multiple functionality, which we must demultiplex.
 * One approach is to have one major device number for the soundblaster card,
 * and use different minor numbers to indicate which hardware function
 * we want.  This would make for one large driver.  Instead our approach
 * is to partition the design into a set of drivers that share an underlying
 * piece of hardware.  Most things are hard to share, for example, the audio
 * and midi ports.  For audio, we might want to mix two processes' signals,
 * and for midi we might want to merge streams (this is hard due to
 * running status).  Moreover, we should be able to re-use the high-level
 * modules with other kinds of hardware.  In this module, we only handle the
 * most basic communications with the sb card.
 */
struct sbdsp_softc {
	struct	device sc_dev;		/* base device */
	struct	isadev sc_id;		/* ISA device */
	void	*sc_ih;			/* interrupt vectoring */

	int	sc_iobase;		/* I/O port base address */
	int	sc_irq;			/* interrupt */
	int	sc_drq;			/* DMA */

	u_short	sc_open;		/* reference count of open calls */
	u_short	sc_locked;		/* true when doing HS DMA  */
 	u_short	sc_adacmode;		/* low/high speed mode indicator */

	u_long	sc_irate;		/* Sample rate for input */
	u_long	sc_orate;		/* ...and output */

	u_int	gain[SB_NDEVS];		/* kept in SB levels: right/left each
					   in a nibble */
	
	u_int	encoding;		/* ulaw/linear -- keep track */

	u_int	out_port;		/* output port */
	u_int	in_port;		/* input port */

	u_int	spkr_state;		/* non-null is on */
	
#define SB_ADAC_LS 0
#define SB_ADAC_HS 1
 	u_short	sc_adactc;		/* current adac time constant */
	u_long	sc_interrupts;		/* number of interrupts taken */
	void	(*sc_intr)(void*);	/* dma completion intr handler */
	void	(*sc_mintr)(void*, int);/* midi input intr handler */
	void	*sc_arg;		/* arg for sc_intr() */

	int	dmaflags;
	caddr_t	dmaaddr;
	vm_size_t	dmacnt;
	int	sc_last_hsw_size;	/* last HS dma size */
	int	sc_last_hsr_size;	/* last HS dma size */
	int	sc_chans;		/* # of channels */
	char	sc_dmain_inprogress;	/* DMA input in progress? */
	char	sc_dmaout_inprogress;	/* DMA output in progress? */

	u_int	sc_model;		/* DSP model */
#define SBVER_MAJOR(v)	((v)>>8)
#define SBVER_MINOR(v)	((v)&0xff)
};

#define ISSBPRO(sc) \
	(SBVER_MAJOR((sc)->sc_model) == 3)

#define ISSBPROCLASS(sc) \
	(SBVER_MAJOR((sc)->sc_model) > 2)

#define ISSB16CLASS(sc) \
      (SBVER_MAJOR((sc)->sc_model) > 3)


#ifdef _KERNEL
int	sbdsp_open __P((struct sbdsp_softc *, dev_t, int));
void	sbdsp_close __P((void *));

int	sbdsp_probe __P((struct sbdsp_softc *));
void	sbdsp_attach __P((struct sbdsp_softc *));

int	sbdsp_set_in_gain __P((void *, u_int, u_char));
int	sbdsp_set_in_gain_real __P((void *, u_int, u_char));
int	sbdsp_get_in_gain __P((void *));
int	sbdsp_set_out_gain __P((void *, u_int, u_char));
int	sbdsp_set_out_gain_real __P((void *, u_int, u_char));
int	sbdsp_get_out_gain __P((void *));
int	sbdsp_set_monitor_gain __P((void *, u_int));
int	sbdsp_get_monitor_gain __P((void *));
int	sbdsp_set_in_sr __P((void *, u_long));
int	sbdsp_set_in_sr_real __P((void *, u_long));
u_long	sbdsp_get_in_sr __P((void *));
int	sbdsp_set_out_sr __P((void *, u_long));
int	sbdsp_set_out_sr_real __P((void *, u_long));
u_long	sbdsp_get_out_sr __P((void *));
int	sbdsp_query_encoding __P((void *, struct audio_encoding *));
int	sbdsp_set_encoding __P((void *, u_int));
int	sbdsp_get_encoding __P((void *));
int	sbdsp_set_precision __P((void *, u_int));
int	sbdsp_get_precision __P((void *));
int	sbdsp_set_channels __P((void *, int));
int	sbdsp_get_channels __P((void *));
int	sbdsp_round_blocksize __P((void *, int));
int	sbdsp_set_out_port __P((void *, int));
int	sbdsp_get_out_port __P((void *));
int	sbdsp_set_in_port __P((void *, int));
int	sbdsp_get_in_port __P((void *));
int	sbdsp_get_avail_in_ports __P((void *));
int	sbdsp_get_avail_out_ports __P((void *));
int	sbdsp_speaker_ctl __P((void *, int));
int	sbdsp_commit_settings __P((void *));

int	sbdsp_dma_output __P((void *, void *, int, void (*)(), void*));
int	sbdsp_dma_input __P((void *, void *, int, void (*)(), void*));

int	sbdsp_haltdma __P((void *));
int	sbdsp_contdma __P((void *));

u_int	sbdsp_get_silence __P((int));
void	sbdsp_compress __P((int, u_char *, int));
void	sbdsp_expand __P((int, u_char *, int));

int	sbdsp_reset __P((struct sbdsp_softc *));
void	sbdsp_spkron __P((struct sbdsp_softc *));
void	sbdsp_spkroff __P((struct sbdsp_softc *));

int	sbdsp_wdsp(int iobase, int v);
int	sbdsp_rdsp(int iobase);

int	sbdsp_intr __P((void *));
short	sbversion __P((struct sbdsp_softc *));

int	sbdsp_set_sr __P((struct sbdsp_softc *, u_long *, int));
int	sbdsp_setfd __P((void *, int));

void	sbdsp_mix_write __P((struct sbdsp_softc *, int, int));
int	sbdsp_mix_read __P((struct sbdsp_softc *, int));

int	sbdsp_mixer_set_port __P((void *, mixer_ctrl_t *));
int	sbdsp_mixer_get_port __P((void *, mixer_ctrl_t *));
int	sbdsp_mixer_query_devinfo __P((void *, mixer_devinfo_t *));

#endif
