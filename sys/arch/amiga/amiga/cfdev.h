/*	$NetBSD: cfdev.h,v 1.3 1995/04/02 20:38:17 chopps Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
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
#ifndef _CFDEV_H_
#define _CFDEV_H_

struct expanrom {
	u_char	type;
	u_char	prodid;
	u_char	flags;
	u_char	pad;
	u_short	manid;
	u_long	serno;
	u_short	idiagvec;
	u_long	resv;
};

#define	ERT_TYPEMASK	0xc0		/* Board type mask */
#define	ERT_ZORROII	0xc0		/* Zorro II type */
#define	ERT_ZORROIII	0x80		/* Zorro III type */
#define	ERTF_MEMLIST	(1 << 5)	/* Add board to free memory list */
#define	ERT_MEMMASK	0x07		/* Board size */

#define	ERFF_EXTENDED	(1 << 5)	/* Used extended size table */
#define	ERT_Z3_SSMASK	0x0f		/* Zorro III Sub-Size */

struct cfdev {
	u_char	resv0[14];
	u_char	flags;
	u_char	pad;
	struct	expanrom rom;
	caddr_t	addr;
	u_long	size;
	u_char	resv1[28];
};

struct	cfdev *cfdev;
int	ncfdev;

#endif /* _CFDEV_H_ */
