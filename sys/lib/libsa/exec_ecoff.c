/*	$OpenBSD: exec_ecoff.c,v 1.2 1998/07/14 16:51:26 mickey Exp $	*/

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
#include <lib/libsa/exec.h>

int
ecoff_probe(fd, hdr)
	int fd;
	union x_header *hdr;
{
	return !ECOFF_BADMAG(&(hdr->x_ecoff));
}


int
ecoff_load(fd, xp)
	int fd;
	register struct x_param *xp;
{
	register struct ecoff_exechdr *x = &xp->xp_hdr->x_ecoff;

#ifdef EXEC_DEBUG
	printf("\nstruct ecoff_exechdr { f.{%x, %x, %x, %lx, %x, %x, %x}\n"
	       "\ta.{%x, %lx, %lx, %lx, %lx, %lx, %lx, %lx} }\n",
	       x->f.f_magic, x->f.f_nscns, x->f.f_timdat, x->f.f_symptr,
	       x->f.f_nsyms, x->f.f_opthdr, x->f.f_flags,
	       x->a.magic, x->a.tsize, x->a.dsize, x->a.bsize,
	       x->a.entry, x->a.text_start, x->a.data_start, x->a.bss_start);
#endif
	xp->xp_entry = x->a.entry;

	xp->text.size = x->a.tsize;
	xp->data.size = x->a.dsize;
	xp->bss.size = x->a.bsize;
	xp->sym.size = x->f.f_nsyms * sizeof (struct ecoff_extsym);
	xp->str.size = 0;

	xp->text.foff = ECOFF_TXTOFF(x);
	xp->data.foff = xp->text.foff + x->a.tsize;
	xp->bss.foff = 0;
	if (x->f.f_symptr) {
		xp->sym.foff = xp->data.foff + x->a.dsize;
		xp->str.foff = xp->sym.foff + xp->sym.size;
	}

	xp->text.addr = x->a.text_start;
	xp->data.addr = x->a.data_start;
	xp->bss.addr = x->a.bss_start;
	xp->sym.addr = x->f.f_symptr;
	xp->str.addr = xp->sym.addr + xp->sym.size;

	return 0;
}
