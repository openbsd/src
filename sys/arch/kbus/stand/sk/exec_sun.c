/*	$OpenBSD: exec_sun.c,v 1.2 2000/03/03 00:54:51 todd Exp $ */

/*-
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed by Theo de Raadt
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <a.out.h>

#include "stand.h"

extern int debug;

extern u_int bootdev;

/*ARGSUSED*/
exec_sun(file, loadaddr, howto)
	char	*file;
	char	*loadaddr;
	int	howto;
{
	register int io;
	struct exec x;
	int cc, magic;
	void (*entry)();
	register char *cp;
	register int *ip;
	int textlen;

#ifdef	DEBUG
	printf("exec_sun: file=%s loadaddr=0x%x\n", file, loadaddr);
#endif

	io = open(file, 0);
	if (io < 0)
		return(-1);

	/*
	 * Read in the exec header, and validate it.
	 */
	if (read(io, (char *)&x, sizeof(x)) != sizeof(x))
		goto shread;
	if (N_BADMAG(x)) {
		errno = EFTYPE;
		goto closeout;
	}

	cp = x.a_entry; /* loadaddr; */
	textlen = x.a_text;
	magic = N_GETMAGIC(x);
	if (magic == ZMAGIC) {
		cp += sizeof(x);
		textlen -= sizeof(x);
	}
	entry = (void (*)())cp;

	printf ("Entry at %x\n", cp);
	/*
	 * Leave a copy of the exec header before the text.
	 * The sun3 kernel uses this to verify that the
	 * symbols were loaded by this boot program.
	 */
	bcopy(&x, cp - sizeof(x), sizeof(x));

	/*
	 * Read in the text segment.
	 */
	printf("%x", x.a_text);
	if (read(io, cp, textlen) != textlen)
		goto shread;
	cp += textlen;

	/*
	 * NMAGIC may have a gap between text and data.
	 */
	if (magic == NMAGIC) {
		register int mask = N_PAGSIZ(x) - 1;
		while ((int)cp & mask)
			*cp++ = 0;
	}

	/*
	 * Read in the data segment.
	 */
	printf("+%x", x.a_data);
	if (read(io, cp, x.a_data) != x.a_data)
		goto shread;
	cp += x.a_data;

	/*
	 * Zero out the BSS section.
	 * (Kernel does not do it itself)
	 */
	printf("+%x", x.a_bss);
	cc = x.a_bss;
	while ((int)cp & 3) {
		*cp++ = 0;
		--cc;
	}
	ip = (int *)cp;
	cp += cc;
	while ((char *)ip < cp)
		*ip++ = 0;

	/*
	 * Read in the symbol table and strings.
	 * (Always set the symtab size word.)
	 */
	*ip++ = x.a_syms;
	cp = (char *)ip;

	if (x.a_syms > 0) {

		/* Symbol table and string table length word. */
		cc = x.a_syms;
		printf("+[%x", cc);
		cc += sizeof(int);	/* strtab length too */
		if (read(io, cp, cc) != cc)
			goto shread;
		cp += x.a_syms;
		ip = (int *)cp;  	/* points to strtab length */
		cp += sizeof(int);

		/* String table.  Length word includes itself. */
		cc = *ip;
		printf("+%x]", cc);
		cc -= sizeof(int);
		if (cc <= 0)
			goto shread;
		if (read(io, cp, cc) != cc)
			goto shread;
		cp += cc;
	}
	printf("=%x\n", cp - loadaddr);
	close(io);

	if (debug) {
		printf("Debug mode - enter c to continue\n");
	}

	printf("Starting program at 0x%x\n", (int)entry);
	(*entry)(howto, bootdev, cp, 0, 0);
	panic("exec returned");

shread:
	printf("exec: short read\n");
	errno = EIO;
closeout:
	close(io);
	return(-1);
}
