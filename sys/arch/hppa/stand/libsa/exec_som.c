/*	$OpenBSD: exec_som.c,v 1.1 1999/12/23 04:10:30 mickey Exp $	*/

/*
 * Copyright (c) 1999 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include "libsa.h"
#include <machine/exec.h>
#include <lib/libsa/exec.h>

int
som_probe(fd, hdr)
	int fd;
	union x_header *hdr;
{
	return !SOM_BADMAGIC(&hdr->x_som);
}


int
som_load(fd, xp)
	int fd;
	register struct x_param *xp;
{
	register struct som_filehdr *xf = &xp->xp_hdr->x_som;
	struct som_exec_aux x;

	if (lseek(fd, xf->aux_loc, SEEK_SET) < 0 ||
	    read (fd, &x, sizeof(x)) != sizeof(x)) {
		if (!errno)
			errno = EIO;
		return -1;
	}

	xp->xp_entry = x.a_entry;

	xp->text.size = hppa_round_page(x.a_tsize);
	xp->data.size = hppa_round_page(x.a_dsize);
	xp->bss.size = x.a_bsize;
	xp->sym.size = xf->sym_total * sizeof(struct som_sym);
	xp->str.size = xf->strings_size;

	xp->text.foff = x.a_tfile;
	xp->data.foff = x.a_dfile;
	xp->bss.foff = 0;
	if (xf->sym_total) {
		xp->sym.foff = xf->sym_loc;
		xp->str.foff = xf->strings_loc;
	}

	xp->text.addr = x.a_tmem;
	xp->data.addr = x.a_dmem;
	xp->bss.addr = xp->data.addr + x.a_dsize;
	xp->sym.addr = xp->bss.addr + xp->bss.size;
	xp->str.addr = xp->sym.addr + xp->sym.size;

	return 0;
}

int
som_ldsym(fd, xp)
	int fd;
	register struct x_param *xp;
{
	return -1;
}
