/*	$OpenBSD: ite_tvrx.c,v 1.2 2006/08/17 06:31:10 miod Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Mark Davies of the Department of Computer
 * Science, Victoria University of Wellington, New Zealand.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * from: Utah $Hdr: ite_hy.c 1.1 92/01/22$
 *
 *	@(#)ite_hy.c	8.1 (Berkeley) 6/10/93
 */

#ifdef ITECONSOLE
#include <sys/param.h>

#include "samachdep.h"
#include "itevar.h"

void
tvrx_init(struct ite_data *ip)
{
	int width;

	ip->bmv = ite_dio_windowmove1bpp;
	ite_fontinfo(ip);
	width = ((ip->ftwidth + 7) / 8) * 8;
	ip->cpl = (ip->fbwidth - ip->dwidth) / width;
	ip->cblanky  = ip->fonty + ((128 / ip->cpl) +1) * ip->ftheight;

	/*
	 * Clear the framebuffer on all planes.
	 */
	ite_dio_windowmove1bpp(ip, 0, 0, 0, 0, ip->fbheight, ip->fbwidth,
	    RR_CLEAR);

	ite_fontinit1bpp(ip);

	/*
	 * Stash the inverted cursor.
	 */
	ite_dio_windowmove1bpp(ip, charY(ip, ' '), charX1bpp(ip, ' '),
	    ip->cblanky, ip->cblankx, ip->ftheight, ip->ftwidth,
	    RR_COPYINVERTED);
}
#endif
