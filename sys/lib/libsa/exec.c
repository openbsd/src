/*	$NetBSD: exec.c,v 1.11 1995/10/20 00:47:47 cgd Exp $	*/

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

#include <string.h>
#include <sys/param.h>
#include <sys/reboot.h>
#ifndef INSECURE
#include <sys/stat.h>
#endif
#include <a.out.h>

#include "stand.h"

static char *ssym, *esym;

extern u_int opendev;

void
exec(path, loadaddr, howto)
	char *path;
	char *loadaddr;
	int howto;
{
	register int io;
#ifndef INSECURE
	struct stat sb;
#endif
	struct exec x;
	int i;
	register char *addr;

	io = open(path, 0);
	if (io < 0)
		return;

#ifndef INSECURE
	(void) fstat(io, &sb);
	if (sb.st_uid || (sb.st_mode & 2)) {
		printf("non-secure file, will not load\n");
		close(io);
		errno = EPERM;
		return;
	}
#endif

	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) ||
	    N_BADMAG(x)) {
		errno = EFTYPE;
		return;
	}

        /* Text */
	printf("%d", x.a_text);
	addr = loadaddr;
#ifdef NO_LSEEK
	if (N_GETMAGIC(x) == ZMAGIC && read(io, (char *)addr, 0x400) == -1)
#else
	if (N_GETMAGIC(x) == ZMAGIC && lseek(io, 0x400, SEEK_SET) == -1)
#endif
		goto shread;
	if (read(io, (char *)addr, x.a_text) != x.a_text)
		goto shread;
	addr += x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC || N_GETMAGIC(x) == NMAGIC)
		while ((long)addr & (N_PAGSIZ(x) - 1))
			*addr++ = 0;

        /* Data */
	printf("+%d", x.a_data);
	if (read(io, addr, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;

        /* Bss */
	printf("+%d", x.a_bss);
	for (i = 0; i < x.a_bss; i++)
		*addr++ = 0;

        /* Symbols */
	ssym = addr;
	bcopy(&x.a_syms, addr, sizeof(x.a_syms));
	addr += sizeof(x.a_syms);
	printf("+[%d", x.a_syms);
	if (read(io, addr, x.a_syms) != x.a_syms)
		goto shread;
	addr += x.a_syms;

	if (read(io, &i, sizeof(int)) != sizeof(int))
		goto shread;

	bcopy(&i, addr, sizeof(int));
	if (i) {
		i -= sizeof(int);
		addr += sizeof(int);
		if (read(io, addr, i) != i)
                	goto shread;
		addr += i;
	}

	/* and that many bytes of (debug symbols?) */
	printf("+%d]", i);

	close(io);

#define	round_to_size(x) \
	(((int)(x) + sizeof(int) - 1) & ~(sizeof(int) - 1))
        esym = (char *)round_to_size(addr - loadaddr);
#undef round_to_size

	/* and note the end address of all this	*/
	printf(" total=0x%lx", (u_long)addr);

	x.a_entry += (long)loadaddr;
	printf(" start=0x%lx\n", x.a_entry);

#ifdef EXEC_DEBUG
        printf("ssym=0x%x esym=0x%x\n", ssym, esym);
        printf("\n\nReturn to boot...\n");
        getchar();
#endif

	machdep_start((char *)x.a_entry, howto, loadaddr, ssym, esym);

	/* exec failed */
	errno = ENOEXEC;
	return;

shread:
	close(io);
	errno = EIO;
	return;
}
