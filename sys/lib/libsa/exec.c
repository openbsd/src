/*	$OpenBSD: exec.c,v 1.21 1998/04/27 18:38:25 millert Exp $	*/
/*	$NetBSD: exec.c,v 1.15 1996/10/13 02:29:01 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/reboot.h>
#ifndef INSECURE
#include <sys/stat.h>
#endif

#include "stand.h"

static char *ssym, *esym;

extern u_int opendev;

void
exec(path, loadaddr, howto)
	char *path;
	void *loadaddr;
	int howto;
{
	int io;
#ifndef INSECURE
	struct stat sb;
#endif
	struct exec x;
	u_int i;
	ssize_t sz;
	char *addr;
#ifdef EXEC_DEBUG
	char *daddr, *etxt;
#endif

	io = open(path, 0);
	if (io < 0)
		return;

	(void) fstat(io, &sb);
	if (sb.st_mode & 2)
		printf("non-secure file, check permissions!\n");

	sz = read(io, (char *)&x, sizeof(x));
	if (sz != sizeof(x) || N_BADMAG(x)) {
		errno = EFTYPE;
		return;
	}

#ifdef EXEC_DEBUG
	printf("\nstruct exec {%lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx}\n",
		x.a_midmag, x.a_text, x.a_data, x.a_bss, x.a_syms,
		x.a_entry, x.a_trsize, x.a_drsize);
#endif

        /* Text */
	printf("%u", x.a_text);
	addr = loadaddr;
	sz = x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC) {
		bcopy((char *)&x, addr, sizeof x);
		addr += sizeof x;
		sz -= sizeof x;
	}
	if (read(io, (char *)addr, sz) != sz)
		goto shread;
	addr += sz;
#ifdef EXEC_DEBUG
	printf("\ntext {%x, %x, %x, %x}\n",
	    addr[0], addr[1], addr[2], addr[3]);
	etxt = addr;
#endif
	if (N_GETMAGIC(x) == NMAGIC)
		while ((long)addr & (N_PAGSIZ(x) - 1))
			*addr++ = 0;

        /* Data */
#ifdef EXEC_DEBUG
	daddr = addr;
#endif
	printf("+%u", x.a_data);
	if (read(io, addr, x.a_data) != (ssize_t)x.a_data)
		goto shread;
	addr += x.a_data;

        /* Bss */
	printf("+%u", x.a_bss);
	for (i = 0; i < x.a_bss; i++)
		*addr++ = 0;

        /* Symbols */
	if (x.a_syms) {
		ssym = addr;
		bcopy(&x.a_syms, addr, sizeof(x.a_syms));
		addr += sizeof(x.a_syms);
		printf("+[%u", x.a_syms);
		if (read(io, addr, x.a_syms) != (ssize_t)x.a_syms)
			goto shread;
		addr += x.a_syms;

		if (read(io, &i, sizeof(u_int)) != sizeof(u_int))
			goto shread;

		bcopy(&i, addr, sizeof(u_int));
		if (i) {
			sz = i - sizeof(int);
			addr += sizeof(int);
			if (read(io, addr, sz) != sz)
                	goto shread;
			addr += sz;
		}

		/* and that many bytes of string table */
		printf("+%d]", sz);
		esym = addr;
	} else {
		ssym = 0;
		esym = 0;
	}

	close(io);

	/* and note the end address of all this	*/
	printf(" total=0x%lx", (u_long)addr);

/* XXX - Hack alert!
   This is no good, loadaddr is passed into
   machdep_start(), and it should do whatever
   is needed.

	x.a_entry += (long)loadaddr;
*/
	printf(" start=0x%x\n", x.a_entry);

#ifdef EXEC_DEBUG
        printf("loadaddr=%p etxt=%p daddr=%p ssym=%p esym=%p\n",
	    	loadaddr, etxt, daddr, ssym, esym);
        printf("\n\nReturn to boot...\n");
        getchar();
#endif

	machdep_start((char *)((register_t)x.a_entry), howto, loadaddr, ssym,
	    esym);

	/* exec failed */
	errno = ENOEXEC;
	return;

shread:
	close(io);
	errno = EIO;
	return;
}
