/*	$OpenBSD: auccvar.h,v 1.1 1997/09/18 13:39:43 niklas Exp $	*/
/*	$NetBSD: auccvar.h,v 1.3 1997/07/04 21:00:18 is Exp $	*/

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
 */


#ifndef _AUCCVAR_H_
#define _AUCCVAR_H_

#define AUDIO_BUF_SIZE 8192

/* per channel data */
typedef struct aucc_data {
	u_int	nd_freq;	/* frequency */
	u_int	nd_per;		/* period = clock/freq */
	u_int	nd_volume;	/* 0..63 */
	u_int	nd_busy;	/* 1, if channel is busy */
	u_short	*nd_dma;	/* pointer to dma buffer */ 
	u_int	nd_dmalength;  	/* length of dma data */
	int	nd_mask;	/* mask of active channels, 
				   together with this one */
	void    (*nd_intr) __P((void *)); /* interrupt routine */
	void   *nd_intrdata;	/* interrupt data */
	int	nd_doublebuf;	/* double buffering */
} aucc_data_t;

/* mixer sets */
#define AUCC_CHANNELS 0

/* mixer values */
#define AUCC_VOLUME 1
#define AUCC_OUTPUT_CLASS 2

#endif /* _AUCCVAR_H_ */
