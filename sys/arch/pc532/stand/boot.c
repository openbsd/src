/*	$NetBSD: boot.c,v 1.5 1995/11/30 00:59:06 jtc Exp $	*/

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
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#ifndef lint
static char rcsid[] = "$NetBSD: boot.c,v 1.5 1995/11/30 00:59:06 jtc Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/reboot.h>
#include <a.out.h>
#include "stand.h"
#include "samachdep.h"

/*
 * Boot program... bits in `howto' determine whether boot stops to
 * ask for system name.	 Boot device is derived from ROM provided
 * information.
 */

extern	unsigned opendev;
extern	int noconsole;
extern	int testing;

char *ssym, *esym;

char *name;
char *names[] = {
	"/netbsd", "/onetbsd", "/netbsd.old",
};
#define NUMNAMES	(sizeof(names)/sizeof(char *))
#define LOADADDR	((char *)0x2000)

static int bdev, bctlr, bunit, bpart;

main()
{
	int currname = 0;
	int io;

	cninit();
	scsiinit();

	printf("\n>> NetBSD BOOT pc532 [$Revision: 1.3 $]\n");

	bdev  = B_TYPE(bootdev);
	bctlr = B_CONTROLLER(bootdev);
	bunit = B_UNIT(bootdev);
	bpart = B_PARTITION(bootdev);

	for (;;) {
		name = names[currname++];
		if (currname == NUMNAMES)
		    currname = 0;

		if (!noconsole) {
		    howto = 0;
		    getbootdev(&howto);
		}
		else
		    printf(": %s\n", name);

		io = open(name, 0);

		if (io >= 0) {
			copyunix(howto, opendev, io);
			close(io);
		}
		else
		    printf("boot: %s\n", strerror(errno));
	}
}

/*ARGSUSED*/
copyunix(howto, devtype, io)
	register int howto;	/* boot flags */
	register u_int devtype;	/* boot device */
	register int io;
{
	struct exec x;
	int i;
	register char *load;	/* load addr for unix */
	register char *addr;
	char *file;
	int dev, ctlr, unit, part;

	/* XXX use devtype? */
	dev = B_TYPE(opendev);
	ctlr = B_CONTROLLER(opendev);
	unit = B_UNIT(opendev);
	part = B_PARTITION(opendev);
	/* get the file name part of name */
	devparse(name, &i, &i, &i, &i, &i, &file);

	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) || N_BADMAG(x)) {
		printf("Bad format\n");
		return;
	}
	addr = LOADADDR;	/* Always load at LOADADDR */
	load = (char *)(x.a_entry & 0x00ffff00); /* XXX make less magical? */
	printf("Booting %s%d%c:%s @ 0x%x\n",
	    devsw[dev].dv_name, unit + (8*ctlr), 'a' + part, file, load);

	if (testing) {
		load = addr = alloc(2*1024*1024); /* XXX stat the file? */
		if (!addr) {
			printf("alloc failed\n");
			exit(1);
		}
	}

	/* Text */
	printf("%d", x.a_text);
	if (N_GETMAGIC(x) == ZMAGIC && lseek(io, 0, SEEK_SET) == -1)
		goto shread;
	if (read(io, addr, x.a_text) != x.a_text)
		goto shread;
	addr += x.a_text;
	if (N_GETMAGIC(x) == NMAGIC)
		while ((int)addr & CLOFSET)
			*addr++ = 0;
	/* Data */
	printf("+%d", x.a_data);
	if (read(io, addr, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;

	/* Bss */
	printf("+%d", x.a_bss);
	bzero( addr, x.a_bss );
	addr += x.a_bss;

	/* Symbols */
	ssym = load + (addr - LOADADDR);
	bcopy(&x.a_syms, addr, sizeof(x.a_syms));
	addr += sizeof(x.a_syms);
	printf(" [%d+", x.a_syms);
	if (x.a_syms && read(io, addr, x.a_syms) != x.a_syms)
		goto shread;
	addr += x.a_syms;

	/* read size of string table */
	i = 0;
	if (x.a_syms && read(io, &i, sizeof(int)) != sizeof(int))
		goto shread;

	/* read strings */
	printf("%d]", i);
	bcopy(&i, addr, sizeof(int));
	if (i) {
		i -= sizeof(int);
		addr += sizeof(int);
		if (read(io, addr, i) != i)
		    goto shread;
		addr += i;
	}

	if (load != LOADADDR) {
		bcopy(LOADADDR, load, addr - LOADADDR);
		addr = load + (addr - LOADADDR);
	}
#define	round_to_size(x,t) \
	(((int)(x) + sizeof(t) - 1) & ~(sizeof(t) - 1))
	esym = (char *)round_to_size(addr - load,int);
#undef round_to_size

	/* and note the end address of all this	*/
	printf(" total=0x%x", addr);

	x.a_entry &= 0xffffff;
	printf(" start 0x%x\n", x.a_entry);

#ifdef DEBUG
	printf("ssym=0x%x esym=0x%x\n", ssym, esym);
	printf("\n\nReturn to boot...\n");
	getchar();
#endif

	if (!testing) {
#ifdef __GNUC__
		/* do NOT change order!!
		 * the following are passed as args, and are in registers
		 * clobbered by the last two movd's!!!
		 */
		asm("	movd %0,r5" : : "g" (load));
		asm("	movd %0,r6" : : "g" (devtype));
		asm("	movd %0,r7" : : "g" (howto));

		/* magic value for locore.s to look for (3253232532) */
		asm("	movd %0,r3" : : "i" (0xc1e86394));
		asm("	movd %0,r4" : : "g" (esym));
#endif /* __GNUC__ */
		(*((int (*)()) x.a_entry))();
	}
	return;
shread:
	printf("Short read\n");
	return;
}

char line[100];

getbootdev(howto)
     int *howto;
{
	char c, *ptr = line;

	printf("Boot: [[[%s%d%c:]%s][-abdrs]] :- ",
	    devsw[bdev].dv_name, bunit + (8 * bctlr), 'a'+bpart, name);

	if (tgets(line)) {
		while (c = *ptr) {
			while (c == ' ')
				c = *++ptr;
			if (!c)
				return;
			if (c == '-')
				while ((c = *++ptr) && c != ' ')
					switch (c) {
					case 'a':
						*howto |= RB_ASKNAME;
						continue;
					case 'b':
						*howto |= RB_HALT;
						continue;
					case 'd':
						*howto |= RB_KDB;
						continue;
					case 'r':
						*howto |= RB_DFLTROOT;
						continue;
					case 's':
						*howto |= RB_SINGLE;
						continue;
					}
			else {
				name = ptr;
				while ((c = *++ptr) && c != ' ');
				if (c)
					*ptr++ = 0;
			}
		}
	} else
		printf("\n");
}
