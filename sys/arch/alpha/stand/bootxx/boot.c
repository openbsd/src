/*	$NetBSD: boot.c,v 1.3 1995/06/28 00:58:58 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include "stand.h"

#include <sys/param.h>
#include <sys/exec.h>

#include <machine/prom.h>

#include "../include/coff.h"
#define KERNEL
#include "../include/pte.h"

#include "libsa/open.c"

#if 0
static void aout_exec __P((int, struct exec *));
#endif
static void coff_exec __P((int, struct exechdr *));
static void loadfile __P((char *));

#if 0
static inline int
aout_exec(fd, aout, entryp)
	int fd;
	struct exec *aout;
	u_int64_t *entryp;
{
	size_t sz;

	/* Check the magic number. */
	if (N_GETMAGIC(*aout) != OMAGIC)
		return (1);

	/* Read in text, data. */
	if (lseek(fd, (off_t)N_TXTOFF(*aout), SEEK_SET) < 0)
		return (1);
	sz = aout->a_text + aout->a_data;
	if (read(fd, aout->a_entry, sz) != sz)
		return (1);

	/* Zero out bss. */
	if (aout->a_bss != 0)
		bzero(aout->a_entry + sz, aout->a_bss);

	ffp_save = aout->a_entry + aout->a_text + aout->a_data + aout->a_bss;
	ffp_save = k0segtophys((ffp_save + PGOFSET & ~PGOFSET)) >> PGSHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

	*entryp = aout->a_entry;
	return (0);
}
#endif

static inline void
coff_exec(fd, coff)
	int fd;
	struct exechdr *coff;
{

	/* Read in text. */
	(void)lseek(fd, N_COFFTXTOFF(coff->f, coff->a), 0);
	if (read(fd, coff->a.text_start, coff->a.tsize) != coff->a.tsize) {
/*puts("text read failed\n");*/
		return;
	}

	/* Read in data. */
	if (coff->a.dsize != 0) {
		if (read(fd,
		    coff->a.data_start, coff->a.dsize) != coff->a.dsize) {
/*puts("data read failed\n");*/
			return;
		}
	}

	/* Zero out bss. */
	if (coff->a.bsize != 0)
		bzero(coff->a.bss_start, coff->a.bsize);

#if 0
	ffp_save = coff->a.text_start + coff->a.tsize;
	if (ffp_save < coff->a.data_start + coff->a.dsize)
		ffp_save = coff->a.data_start + coff->a.dsize;
	if (ffp_save < coff->a.bss_start + coff->a.bsize)
		ffp_save = coff->a.bss_start + coff->a.bsize;
	ffp_save = k0segtophys((ffp_save + PGOFSET & ~PGOFSET)) >> PGSHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */
#endif

	{
		extern int diskdev;
		prom_close(diskdev);
	}
	(*(void (*)())coff->a.entry)();
}

/*
 * Open 'filename', read in program and return the entry point or -1 if error.
 */
static inline void
loadfile(fname)
	char *fname;
{
	struct devices *dp;
	union {
		struct exec aout;
		struct exechdr coff;
	} hdr;
	ssize_t nr;
	int fd, rval;

	/* Open the file. */
	/* rval = 1; */
	if ((fd = open(fname, 0)) < 0) {
/*puts("open failed\n");*/
		return;
	}

	/* Read the exec header. */
	if ((nr = read(fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
/*puts("header read failed\n");*/
		return;
	}

#if 0
	/* Exec a.out or COFF. */
	rval = N_COFFBADMAG(hdr.coff.a) ?
	    aout_exec(fd, &hdr.aout, entryp) :
	    coff_exec(fd, &hdr.coff, entryp);
#endif
	coff_exec(fd, &hdr.coff);
/*puts("coff_exec returned\n");*/
}

void
main()
{

	/* Init prom callback vector. */
	init_prom_calls();

	/* print a banner */
/*	puts("loading /boot...\n");*/

	loadfile("/boot");
/*	puts("couln't load /boot.\n");*/
}
