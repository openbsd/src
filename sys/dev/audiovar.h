/*	$OpenBSD: audiovar.h,v 1.13 2010/09/21 20:08:11 jakemsr Exp $	*/
/*	$NetBSD: audiovar.h,v 1.18 1998/03/03 09:16:16 augustss Exp $	*/

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
#define AUDIO_BLK_MS	50	/* 50 ms */
#endif

#ifndef AU_RING_SIZE
#define AU_RING_SIZE		65536
#endif

#define AUMINBUF 512
#define AUMINBLK 32
#define AUMINNOBLK 2
struct audio_ringbuffer {
	int	bufsize;	/* allocated memory */
	int	blksize;	/* I/O block size */
	int	maxblks;	/* no of blocks in ring */
	u_char	*start;		/* start of buffer area */
	u_char	*end;		/* end of buffer area */
	u_char	*inp;		/* input pointer (to buffer) */
	u_char	*outp;		/* output pointer (from buffer) */
	int	used;		/* no of used bytes */
	int	usedlow;	/* start writer when used falls below this */
	int	usedhigh;	/* stop writer when used goes above this */
	u_long	stamp;		/* bytes transferred */
	u_long	stamp_last;	/* old value of bytes transferred */
	u_long	drops;		/* missed samples from over/underrun */
	u_long	pdrops;		/* paused samples */
	char	pause;		/* transfer is paused */
	char	mmapped;	/* device is mmap()-ed */
	u_char	blkset;		/* blksize has been set, for stickiness */
};

#define AUDIO_N_PORTS 4

struct au_mixer_ports {
	int	index;
	int	master;
	int	nports;
	u_char	isenum;
	u_int	allports;
	u_int	aumask[AUDIO_N_PORTS];
	u_int	misel [AUDIO_N_PORTS];
	u_int	miport[AUDIO_N_PORTS];
};

/*
 * Software state, per audio device.
 */
struct audio_softc {
	struct	device dev;
	void	*hw_hdl;	/* Hardware driver handle */
	struct	audio_hw_if *hw_if; /* Hardware interface */
	struct	device *sc_dev;	/* Hardware device struct */
	u_char	sc_open;	/* single use device */
#define AUOPEN_READ	0x01
#define AUOPEN_WRITE	0x02
	u_char	sc_mode;	/* bitmask for RECORD/PLAY */

	struct	selinfo sc_wsel; /* write selector */
	struct	selinfo sc_rsel; /* read selector */
	struct	proc *sc_async_audio;	/* process who wants audio SIGIO */
	struct	mixer_asyncs {
		struct mixer_asyncs *next;
		struct proc *proc;
	} *sc_async_mixer;  /* processes who want mixer SIGIO */

	/* Sleep channels for reading and writing. */
	int	sc_rchan;
	int	sc_wchan;

	/* Ring buffers, separate for record and play. */
	struct	audio_ringbuffer sc_rr; /* Record ring */
	struct	audio_ringbuffer sc_pr; /* Play ring */

	u_char	*sc_sil_start;	/* start of silence in buffer */
	int	sc_sil_count;	/* # of silence bytes */

	u_char	sc_rbus;	/* input dma in progress */
	u_char	sc_pbus;	/* output dma in progress */

	u_char	sc_rqui;	/* input dma quiesced */
	u_char	sc_pqui;	/* output dma quiesced */

	struct	audio_params sc_pparams;	/* play encoding parameters */
	struct	audio_params sc_rparams;	/* record encoding parameters */

	int	sc_eof;		/* EOF, i.e. zero sized write, counter */
	u_long	sc_wstamp;
	u_long	sc_playdrop;

	int	sc_full_duplex;	/* device in full duplex mode */

	struct	au_mixer_ports sc_inports, sc_outports;
	int	sc_monitor_port;

	int     sc_refcnt;
	int     sc_dying;

	int	sc_quiesce;
#define	AUDIO_QUIESCE_START	1
#define	AUDIO_QUIESCE_SILENT	2
	struct timeout sc_resume_to;
	struct workq_task sc_resume_task;
	u_char	sc_mute;

#ifdef AUDIO_INTR_TIME
	u_long	sc_pfirstintr;	/* first time we saw a play interrupt */
	int	sc_pnintr;	/* number of interrupts */
	u_long	sc_plastintr;	/* last time we saw a play interrupt */
	long	sc_pblktime;	/* nominal time between interrupts */
	u_long	sc_rfirstintr;	/* first time we saw a rec interrupt */
	int	sc_rnintr;	/* number of interrupts */
	u_long	sc_rlastintr;	/* last time we saw a rec interrupt */
	long	sc_rblktime;	/* nominal time between interrupts */
#endif
};
