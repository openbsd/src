/*	$OpenBSD: frame.h,v 1.3 2001/08/25 13:33:36 hugh Exp $ */
/*	$NetBSD: frame.h,v 1.2 2000/06/04 19:30:15 matt Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _VAX_FRAME_H_
#define	_VAX_FRAME_H_

/*
 * Description of calls frame on stack. This is the 
 * standard way of making procedure calls on vax systems.
 */
struct callsframe {
	unsigned int	ca_cond;	/* condition handler */
	unsigned int	ca_maskpsw;	/* register mask and saved psw */
	unsigned int	ca_ap;		/* argument pointer */
	unsigned int	ca_fp;		/* frame pointer */
	unsigned int	ca_pc;		/* program counter */
	unsigned int	ca_argno;	/* argument count on stack */
	unsigned int	ca_arg1;	/* first arg on stack */
	/* This can be followed by more arguments */
};

struct icallsframe {
	struct callsframe ica_frame;	/* std call frame */
	unsigned int	ica_r0;		/* interrupt saved r0 */
	unsigned int	ica_r1;		/* interrupt saved r1 */
	unsigned int	ica_r2;		/* interrupt saved r2 */
	unsigned int	ica_r3;		/* interrupt saved r3 */
	unsigned int	ica_r4;		/* interrupt saved r4 */
	unsigned int	ica_r5;		/* interrupt saved r5 */
	unsigned int	ica_pc;		/* interrupt saved pc */
	unsigned int	ica_psl;	/* interrupt saved psl */
};

#endif /* _VAX_FRAME_H */
