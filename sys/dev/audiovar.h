/*	$NetBSD: audiovar.h,v 1.3 1995/05/08 22:01:44 brezak Exp $	*/

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
 *	From: Header: audiovar.h,v 1.3 93/07/18 14:07:25 mccanne Exp  (LBL)
 */

/*
 * Initial/default block duration is both configurable and patchable.
 */
#ifndef AUDIO_BLK_MS
#define AUDIO_BLK_MS	20	/* 20 ms */
#endif

#ifndef AUDIO_BACKLOG
#define AUDIO_BACKLOG	3	/* 60 ms */
#endif

/*
 * Use a single page as the size of the audio ring buffers, so that
 * the data won't cross a page boundary.  This way the dma carried out
 * in the hardware module will be efficient (i.e., at_dma() won't have
 * to make a copy).
 */
#ifndef AU_RING_SIZE
#define AU_RING_SIZE		NBPG
#endif

#define AU_RING_MOD(k)		((k) & (AU_RING_SIZE - 1))
#define AU_RING_EMPTY(rp) 	((rp)->hp == (rp)->tp)
#define AU_RING_FULL(rp)	(AU_RING_MOD((rp)->tp + 1) == (rp)->hp)
#define AU_RING_LEN(rp)		(AU_RING_MOD((rp)->tp - (rp)->hp))
#define AU_RING_INIT(rp)	{					  \
					 (rp)->nblk = (rp)->au_stamp = 0; \
					 (rp)->hp = (rp)->tp = (rp)->bp;  \
				}

struct audio_buffer {
	u_char	*hp;		/* head */
	u_char	*tp;		/* tail */
	u_char	*bp;		/* start of buffer */
	u_char	*ep;		/* end of buffer */
	
	int	nblk;		/* number of active blocks in buffer */
	int	maxblk;		/* max # of active blocks in buffer */
	u_long	au_stamp;	/* number of audio samples read/written */

	u_short	cb_pause;	/* io paused */
	u_long	cb_drops;	/* missed samples from over/underrun */
	u_long	cb_pdrops;	/* paused samples */
};

/*
 * Software state, per audio device.
 */
struct audio_softc {
	void	*hw_hdl;		/* Hardware driver handle */
	struct	audio_hw_if *hw_if; /* Hardware interface */
	u_char	sc_open;	/* single use device */
#define AUOPEN_READ	0x01
#define AUOPEN_WRITE	0x02
	u_char	sc_mode;	/* bitmask for RECORD/PLAY */

	struct	selinfo sc_wsel; /* write selector */
	struct	selinfo sc_rsel; /* read selector */

	/* Sleep channels for reading and writing. */
	int	sc_rchan;
	int	sc_wchan;

	/* Ring buffers, separate for record and play. */
	struct	audio_buffer rr; /* Record ring */
	struct	audio_buffer pr; /* Play ring */
	
	u_char	sc_rbus;	/* input dma in progress */
	u_char	sc_pbus;	/* output dma in progress */

	u_long	sc_wseek;	/* timestamp of last frame written */
	u_long	sc_rseek;	/* timestamp of last frame read */

	int	sc_blksize;	/* recv block (chunk) size in bytes */
	int	sc_smpl_in_blk;	/* # samples in a block  */
	int	sc_50ms;	/* # of samples for 50ms? */
	int	sc_backlog;	/* # blks of xmit backlog to generate */
	int	sc_lowat;	/* xmit low water mark (for wakeup) */
	int	sc_hiwat;	/* xmit high water mark (for wakeup) */

	int	sc_rblks;	/* number of phantom record blocks */
	int	sc_pencoding;	/* current encoding; play */
	int	sc_rencoding;	/* current encoding; record */
};
