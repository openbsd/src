/*	$OpenBSD: cs4231reg.h,v 1.4 2003/06/02 18:40:59 jason Exp $	*/

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
 * Driver for the CS4231 audio in some sun4m systems
 */

/*
 * CS4231 registers from CS web site and Solaris 2.6 includes.
 */
struct cs4231_regs {
	volatile u_int8_t	iar;		/* index address */
	volatile u_int8_t	_padding1[3];	/* reserved */
	volatile u_int8_t	idr;		/* index data */
	volatile u_int8_t	_padding2[3];	/* reserved */
	volatile u_int8_t	status;		/* status */
	volatile u_int8_t	_padding3[3];	/* reserved */
	volatile u_int8_t	pior;		/* PIO data i/o */
	volatile u_int8_t	_padding4[3];	/* reserved */
	volatile u_int32_t	dma_csr;	/* APC control/status */
	volatile u_int32_t	_padding5[3];	/* reserved */
	volatile u_int32_t	dma_cva;	/* capture virtual addr */
	volatile u_int32_t	dma_cc;		/* capture count */
	volatile u_int32_t	dma_cnva;	/* capture next virtual addr */
	volatile u_int32_t	dma_cnc;	/* capture next count */
	volatile u_int32_t	dma_pva;	/* playback virtual addr */
	volatile u_int32_t	dma_pc;		/* playback count */
	volatile u_int32_t	dma_pnva;	/* playback next virtual addr */
	volatile u_int32_t	dma_pnc;	/* playback next count */
};
