/*	$OpenBSD: exec_aout.c,v 1.1 1998/07/14 03:29:08 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include "libsa.h"
#include <machine/exec.h>
#include <lib/libsa/exec.h>

int
aout_probe(fd, hdr)
	int fd;
	union x_header *hdr;
{
	return !N_BADMAG(hdr->x_aout);
}


int
aout_load(fd, xp)
	int fd;
	register struct x_param *xp;
{
	register struct exec *x = &xp->xp_hdr->x_aout;

#ifdef EXEC_DEBUG
	printf("\nstruct exec {%x, %x, %x, %x, %x, %x, %x, %x}\n",
	       x->a_midmag, x->a_text, x->a_data, x->a_bss, x->a_syms,
	       x->a_entry, x->a_trsize, x->a_drsize);
#endif
	xp->xp_entry = x->a_entry;

	xp->text.foff = N_GETMAGIC(*x) == ZMAGIC? 0: sizeof(*x);
	xp->data.foff = xp->text.foff + x->a_text;
	xp->bss.foff = 0;
	if (x->a_syms) {
		xp->sym.foff = xp->data.foff + x->a_data;
		xp->str.foff = xp->sym.foff + x->a_syms;
	}

	xp->text.addr = 0;
	xp->data.addr = xp->text.addr + x->a_text;
	if (N_GETMAGIC(*x) == NMAGIC)
		xp->data.addr += N_PAGSIZ(x)- (xp->data.addr & (N_PAGSIZ(x)-1));
	xp->bss.addr = xp->data.addr + x->a_data;
	xp->sym.addr = xp->bss.addr + x->a_bss;
	xp->str.addr = xp->sym.addr + x->a_syms;

	xp->text.size = x->a_text;
	xp->data.size = x->a_data;
	xp->bss.size = x->a_bss;
	xp->sym.size = x->a_syms;
	xp->str.size = 0;	/* will be hacked later in exec() */

	return 0;
}
