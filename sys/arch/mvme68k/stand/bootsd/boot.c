/*	$Id: boot.c,v 1.6 1996/02/16 00:13:18 rahnds Exp $ */

/*-
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
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
 * from: @(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/reboot.h>
#include <a.out.h>
#include <machine/prom.h>
#include "stand.h"

void copyunix __P((int io, char *addr));
void parse_args __P((void));

int debug;
int netif_debug;
#define RB_NOSYM 0x400

/*
 * Boot device is derived from ROM provided information.
 */
extern char	*version;
u_long		esym;
char		*strtab;
int		strtablen;
#if 0
struct nlist  *nlp, *enlp;
#endif

struct kernel {
	void	*entry;
	void	*symtab;
	void	*esym;
	int	bflags;
	int	bdev;
	char	*kname;
	void	*smini;
	void	*emini;
	u_int	end_loaded;
} kernel;

extern struct mvmeprom_args bugargs;

int
main()
{
	struct exec x;
	char *file;
	void *addr;
	int io, i;

	printf(">> OpenBSD sdboot [%s]\n", version);

	parse_args();
	file = kernel.kname;

	io = open(file, 0);
	if (io < 0) {
		printf("Can't open %s: %s\n", file, strerror(errno));
		mvmeprom_return();
	}
	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) || N_BADMAG(x)) {
		printf("Bad format\n");
		return (0);
	}
	/* Make load address start of page which containes "start" */
	addr = (void *)(x.a_entry & ~0x0FFF);
	lseek(io, 0, SEEK_SET);

	/*printf("load %s to 0x%x\n", file, addr);*/
	copyunix(io, addr);
	return (0);
}

/*ARGSUSED*/
void
copyunix(io, addr)
	int io;
	char *addr;
{
	void (*entry)() = (void (*)())addr;
	struct exec x;
	int i, cnt;

	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) || N_BADMAG(x)) {
		printf("Bad format\n");
		return;
	}

	printf("%x", x.a_text);
	if (N_GETMAGIC(x) == ZMAGIC) {
		kernel.entry = entry = (void *)x.a_entry;
		lseek(io, 0, SEEK_SET);
	}
	if (read(io, (char *)addr, x.a_text) != x.a_text)
		goto shread;
	addr += x.a_text;
	if (N_GETMAGIC(x) == NMAGIC)
		while ((int)addr & CLOFSET)
			*addr++ = 0;
	printf("+%x", x.a_data);
	if (read(io, addr, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;
	printf("+%x", x.a_bss);
	for (i = 0; i < x.a_bss; i++)
		*addr++ = 0;
	if (x.a_syms != 0 && !(kernel.bflags & RB_NOSYM)) {
		bcopy(&x.a_syms, addr, sizeof(x.a_syms));
		addr += sizeof(x.a_syms);
#if 0
		nlp = (struct nlist *)addr;
#endif
		printf("+[%x+", x.a_syms);
		if (read(io, addr, x.a_syms) != x.a_syms)
			goto shread;
		addr += x.a_syms;
#if 0
		enlp = (struct nlist *)(strtab = addr);
#endif

		if (read(io, &strtablen, sizeof(int)) != sizeof(int))
			goto shread;

		bcopy(&strtablen, addr, sizeof(int));
		if (i = strtablen) {
			i -= sizeof(int);
			addr += sizeof(int);
			cnt = read(io, addr, i);
			if (cnt != i)
				printf("symwarn"); /* goto shread; */
			addr += i;
		}
		printf("%x]", i);
		esym = KERNBASE +
			(((int)addr + sizeof(int) - 1) & ~(sizeof(int) - 1));
		kernel.symtab = (void *) x.a_syms;
		kernel.esym = addr;
	} else {
		kernel.symtab = 0;
		kernel.esym = 0;
	}

#if 0
	while (nlp < enlp) {
		register int strx = nlp->n_un.n_strx;
		if (strx > strtablen)
			continue;
		if (strcmp(strtab+strx, "_esym") == 0) {
			*(int*)(nlp->n_value - KERNBASE) = esym;
			break;
		}
		nlp++;
	}
#endif

	kernel.bdev = 0;
	kernel.end_loaded = (u_int)addr;
	kernel.smini = 0;
	kernel.emini = 0;
	kernel.kname = 0;

	printf("=%x\n", (u_int)addr - (u_int)entry);	/* XXX wrong? */

#if 0
printf("entry %x\n",kernel.entry);
printf("symtab %x\n",kernel.symtab);
printf("esym %x\n",kernel.esym);
printf("bflags %x\n",kernel.bflags);
printf("bdev %x\n",kernel.bdev);
printf("kname %x\n",kernel.kname);
printf("smini %x\n",kernel.smini);
printf("emini %x\n",kernel.emini);
printf("end_loaded %x\n",kernel.end_loaded);
#endif

	printf("start at 0x%x\n", (int)entry);
	if (((u_long)entry &0xf) == 0x2) {
		(entry)(&bugargs, &kernel);
	} else {
		/* is type fixing anything like price fixing? */
		typedef (* kernel_start) __P((int, int, void *,void *, void *));
		kernel_start addr; 
		addr = (void *)entry;
		(addr)(kernel.bflags, 0, kernel.esym, kernel.smini, kernel.emini);
	}
	return;

shread:
	printf("short read\n");
}

struct flags {
	char c;
	short bit;
} bf[] = {
	{ 'a', RB_ASKNAME },
	{ 'b', RB_HALT },
	{ 'y', RB_NOSYM },
	{ 'd', RB_KDB },
	{ 'm', RB_MINIROOT },
	{ 'r', RB_DFLTROOT },
	{ 's', RB_SINGLE },
};

void
parse_args()
{
	char *name = "/bsd", *ptr;
	int i, howto = 0;
	char c;

	if (bugargs.arg_start != bugargs.arg_end) {
		ptr = bugargs.arg_start;
		while (c = *ptr) {
			while (c == ' ')
				c = *++ptr;
			if (c == '\0')
				return;
			if (c != '-') {
				name = ptr;
				while ((c = *++ptr) && c != ' ')
					;
				if (c)
					*ptr++ = 0;
				continue;
			}
			while ((c = *++ptr) && c != ' ') {
				for (i = 0; i < sizeof(bf)/sizeof(bf[0]); i++)
					if (bf[i].c == c) {
						howto |= bf[i].bit;
					}
			}
		}
	}
	kernel.bflags = howto;
	kernel.kname = name;
}
