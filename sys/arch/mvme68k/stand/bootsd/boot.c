/*	$NetBSD$	*/

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
 *
 * from: @(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/reboot.h>
#include <a.out.h>
#include <machine/prom.h>
#include "stand.h"

void reset_twiddle __P((void));
void copyunix __P((int io, char *addr));
void parse_args __P((struct mvmeprom_args *pargs));

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
struct nlist	*nlp, *enlp;
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

struct mvmeprom_args *bugargs;

int
main(pp)
	struct mvmeprom_args *pp;
{
	struct exec x;
	char *file;
	void *addr;
	int io, i;

	printf(">> NetBSD sdboot [%s]\n", version);

	bugargs = pp;
	parse_args(pp);
	file = kernel.kname;

	if ((io = open(file, 0)) < 0) {
		printf("Can't open %s: %s\n", file, strerror(errno));
		mvmeprom_return();
	}
	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) ||
	    N_BADMAG(x)) {
		printf("Bad format\n");
		return (0);
	}
	/* Make load address start of page which containes "start" */
	addr = (void *)(x.a_entry & ~0x0FFF);
	lseek(io, 0, SEEK_SET);

	reset_twiddle();

	printf("booting %s load address 0x%x\n", file, addr);
	copyunix(io, addr);
	return (0);
}

/*ARGSUSED*/
void
copyunix(io, addr)
	int io;
	char *addr;
{
	struct exec x;
	int i;
	void (*entry)() = (void (*)())addr;

	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) ||
	    N_BADMAG(x)) {
		printf("Bad format\n");
		return;
	}

	reset_twiddle();
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
	reset_twiddle();
	printf("+%x", x.a_data);
	if (read(io, addr, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;
	reset_twiddle();
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
		reset_twiddle();

		if (read(io, &strtablen, sizeof(int)) != sizeof(int))
			goto shread;
		reset_twiddle();

		bcopy(&strtablen, addr, sizeof(int));
		if (i = strtablen) {
			i -= sizeof(int);
			addr += sizeof(int);
			{
				int cnt;
				cnt = read(io, addr, i);
				if (cnt != i)
				    /*
				    goto shread;
				    */printf("symwarn");
			}
			reset_twiddle();
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
#if 0
	if (kernel.bflags & RB_HALT) {
		mvmeprom_return();
	}
#endif
	if (((u_long)entry &0xf) == 0x2) {
		(entry)(bugargs, &kernel);
	} else {
                /* is type fixing anything like price fixing? */
                typedef (* kernel_start)(int, int, void *,void *, void *);
                kernel_start addr; 
                addr = (void *)entry;
                (addr)(kernel.bflags,0,kernel.esym,kernel.smini,kernel.emini);
	}
	return;
shread:
	printf("Short read\n");
}
#define NO_TWIDDLE_FUNC

#ifndef NO_TWIDDLE_FUNC
static int tw_on;
static int tw_pos;
static char tw_chars[] = "|/-\\";
#endif

void
reset_twiddle()
{
#ifndef NO_TWIDDLE_FUNC
	if (tw_on)
		putchar('\b');
	tw_on = 0;
	tw_pos = 0;
#endif
}

#ifndef NO_TWIDDLE_FUNC
void
twiddle()
{
	if (tw_on)
		putchar('\b');
	else
		tw_on = 1;
	putchar(tw_chars[tw_pos++]);
	tw_pos %= (sizeof(tw_chars) - 1);
}
#endif

void
_rtt()
{
	mvmeprom_return();
}
void
parse_args(pargs)
	struct mvmeprom_args *pargs;
{
	char *ptr = pargs->arg_start;
	char *name = "/netbsd";
	char c;
	int howto = 0;

	if (pargs->arg_start != pargs->arg_end) {
		while (c = *ptr) {
			while (c == ' ')
				c = *++ptr;
			if (!c)
				return;
			if (c == '-')
				while ((c = *++ptr) && c != ' ') {
					if (c == 'a')
						howto |= RB_ASKNAME;
					else if (c == 'b')
						howto |= RB_HALT;
					else if (c == 'y')
						howto |= RB_NOSYM;
					else if (c == 'd')
						howto |= RB_KDB;
					else if (c == 'm')
						howto |= RB_MINIROOT;
					else if (c == 'r')
						howto |= RB_DFLTROOT;
					else if (c == 's')
						howto |= RB_SINGLE;
				}
			else {
				name = ptr;
				while ((c = *++ptr) && c != ' ');
				if (c)
					*ptr++ = 0;
			}
		}
#if 0
		if (RB_NOSYM & howto) printf("RB_NOSYM\n\r");
		if (RB_AUTOBOOT & howto) printf("RB_AUTOBOOT\n\r");
		if (RB_SINGLE & howto) printf("RB_SINGLE\n\r");
		if (RB_NOSYNC & howto) printf("RB_NOSYNC\n\r");
		if (RB_HALT & howto) printf("RB_HALT\n\r");
		if (RB_DFLTROOT & howto) printf("RB_DFLTROOT\n\r");
		if (RB_KDB & howto) printf("RB_KDB\n\r");
		if (RB_RDONLY & howto) printf("RB_RDONLY\n\r");
		if (RB_DUMP & howto) printf("RB_DUMP\n\r");
		if (RB_MINIROOT & howto) printf("RB_MINIROOT\n\r");
#endif
	}
	kernel.bflags = howto;
	kernel.kname = name;
}
