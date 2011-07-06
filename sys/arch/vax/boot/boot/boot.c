/*	$OpenBSD: boot.c,v 1.20 2011/07/06 18:32:59 miod Exp $ */
/*	$NetBSD: boot.c,v 1.18 2002/05/31 15:58:26 ragge Exp $ */
/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)boot.c	7.15 (Berkeley) 5/4/91
 */

#include <sys/param.h>
#include <sys/reboot.h>

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/loadfile.h>

#include "machine/rpb.h"
#include "machine/sid.h"

#include "vaxstand.h"

/*
 * Boot program... arguments passed in r10 and r11 determine
 * whether boot stops to ask for system name and which device
 * boot comes from.
 */

char line[100];
int	bootdev, debug;
extern	unsigned opendev;

void	usage(char *), boot(char *), halt(char *);
void	Xmain(void);
void	autoconf(void);
int	getsecs(void);
int	setjmp(int *);
int	testkey(void);

const struct vals {
	char	*namn;
	void	(*func)(char *);
	char	*info;
} val[] = {
	{"?", usage, "Show this help menu"},
	{"help", usage, "Same as '?'"},
	{"boot", boot, "Load and execute file"},
	{"halt", halt, "Halts the system"},
	{0, 0},
};

int jbuf[10];
int sluttid, senast, skip, askname;
int mcheck_silent;
struct rpb bootrpb;

void
Xmain(void)
{
	int io;
	int j, nu;
	char transition = '\010';
	u_long marks[MARK_MAX];

	io = 0;
	skip = 1;
	autoconf();

	/*
	 * Some VAXstation 4000 PROMs slowly erase the whole screen with \010
	 * if running with glass console - at least VS4000/60 and VS4000/VLC;
	 * this is probably the LCG PROM at fault. Use a different transition
	 * pattern, it's not as nice but it does not take 3(!) seconds to
	 * display...
	 */
	if (((vax_boardtype == VAX_BTYP_46 &&
	      (vax_siedata & 0xff) == VAX_VTYP_46) ||
	     (vax_boardtype == VAX_BTYP_48 &&
	      ((vax_siedata >> 8) & 0xff) == VAX_STYP_48)) &&
	    (vax_confdata & 0x100) == 0)
		transition = ' ';

	askname = bootrpb.rpb_bootr5 & RB_ASKNAME;
	printf("\n\r>> OpenBSD/vax boot [%s] <<\n", "1.16");
	printf(">> Press enter to autoboot now, or any other key to abort:  ");
	sluttid = getsecs() + 5;
	senast = 0;
	skip = 0;
	setjmp(jbuf);
	for (;;) {
		nu = sluttid - getsecs();
		if (senast != nu)
			printf("%c%d", transition, nu);
		if (nu <= 0)
			break;
		senast = nu;
		if ((j = (testkey() & 0177))) {
			skip = 1;
			if (j != 10 && j != 13) {
				printf("\nPress '?' for help");
				askname = 1;
			}
			break;
		}
	}
	skip = 1;
	printf("\n");

	if (setjmp(jbuf))
		askname = 1;

	/* First try to autoboot */
	if (askname == 0) {
		int err;

		errno = 0;
		printf("> boot bsd\n");
		marks[MARK_START] = 0;
		err = loadfile("bsd", marks,
		    LOAD_KERNEL|COUNT_KERNEL);
		if (err == 0) {
			machdep_start((char *)marks[MARK_ENTRY],
					      marks[MARK_NSYM],
				      (void *)marks[MARK_START],
				      (void *)marks[MARK_SYM],
				      (void *)marks[MARK_END]);
		}
		printf("bsd: boot failed: %s\n", strerror(errno));
	}

	/* If any key pressed, or autoboot failed, go to conversational boot */
	for (;;) {
		const struct vals *v = &val[0];
		char *c, *d;

		printf("> ");
		gets(line);

		c = line;
		while (*c == ' ')
			c++;

		if (c[0] == 0)
			continue;

		if ((d = strchr(c, ' ')))
			*d++ = 0;

		while (v->namn) {
			if (strcmp(v->namn, c) == 0)
				break;
			v++;
		}
		if (v->namn)
			(*v->func)(d);
		else
			printf("Unknown command: %s\n", c);
	}
}

void
halt(char *hej)
{
	asm("halt");
}

void
boot(char *arg)
{
	char *fn = "bsd";
	int howto, err;
	u_long marks[MARK_MAX];

	if (arg) {
		while (*arg == ' ')
			arg++;

		if (*arg != '-') {
			fn = arg;
			if ((arg = strchr(arg, ' '))) {
				*arg++ = 0;
				while (*arg == ' ')
					arg++;
			} else
				goto load;
		}
		if (*arg != '-') {
fail:			printf("usage: boot [filename] [-acsd]\n");
			return;
		}

		howto = 0;

		while (*++arg) {
			if (*arg == 'a')
				howto |= RB_ASKNAME;
			else if (*arg == 'c')
				howto |= RB_CONFIG;
			else if (*arg == 'd')
				howto |= RB_KDB;
			else if (*arg == 's')
				howto |= RB_SINGLE;
			else
				goto fail;
		}
		bootrpb.rpb_bootr5 = howto;
	}
load:  
	marks[MARK_START] = 0;
	err = loadfile(fn, marks, LOAD_KERNEL|COUNT_KERNEL);
	if (err == 0) {
		machdep_start((char *)marks[MARK_ENTRY],
				      marks[MARK_NSYM],
			      (void *)marks[MARK_START],
			      (void *)marks[MARK_SYM],
			      (void *)marks[MARK_END]);
	}
	printf("Boot failed: %s\n", strerror(errno));
}

void
usage(char *hej)
{
	const struct vals *v = &val[0];
	int i;

	printf("Commands:\n");
	while (v->namn) {
		printf("%s ", v->namn);
		for (i = 1 + strlen(v->namn); (i & 7) != 0; i++)
			printf(" ");
		printf("%s\n", v->info);
		v++;
	}
}
