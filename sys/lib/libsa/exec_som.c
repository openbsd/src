/*	$OpenBSD: exec_som.c,v 1.3 1998/07/14 17:17:54 mickey Exp $	*/

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
	struct som_exec_auxhdr x;

	if (lseek(fd, xf->aux_header_location, SEEK_SET) < 0 ||
	    read (fd, &x, sizeof(x)) != sizeof(x)) {
		if (!errno)
			errno = EIO;
		return -1;
	}

	xp->xp_entry = x.exec_entry;

	xp->text.size = hppa_round_page(x.exec_tsize);
	xp->data.size = hppa_round_page(x.exec_dsize);
	xp->bss.size = x.exec_bsize;
	xp->sym.size = xf->symbol_total *
		sizeof(struct som_symbol_dictionary_record);
	xp->str.size = xf->symbol_strings_size;

	xp->text.foff = x.exec_tfile;
	xp->data.foff = x.exec_dfile;
	xp->bss.foff = 0;
	if (xf->symbol_total) {
		xp->sym.foff = xf->symbol_location;
		xp->str.foff = xf->symbol_strings_location;
	}

	xp->text.addr = x.exec_tmem;
	xp->data.addr = x.exec_dmem;
	xp->bss.addr = xp->data.addr + x.exec_dsize;
	xp->sym.addr = xp->bss.addr + xp->bss.size;
	xp->str.addr = xp->sym.addr + xp->sym.size;

	return 0;
}
