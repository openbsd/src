/*
 * Copyright (c) 1994 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
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
 *	This product includes software developed by SigmaSoft, Th.  Lockert.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: arch.c,v 1.4 1999/08/20 09:20:39 millert Exp $";
#endif /* not lint */

#include <sys/param.h>

#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>

static void usage __P((void));

static int machine;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct utsname uts;
	char *arch;
	char *opts;
	int c;
	int short_form = 0;
	extern char *__progname;

	setlocale(LC_ALL, "");

	machine = strcmp(__progname, "machine") == 0;
	if (machine) {
		arch = MACHINE;
		opts = "a";
		short_form++;
	} else {
		arch = MACHINE_ARCH;
		opts = "ks";
	}
	while ((c = getopt(argc, argv, opts)) != -1)
		switch (c) {
			case 'a':
				arch = MACHINE_ARCH;
				break;
			case 'k':
				arch = MACHINE;
				break;
			case 's':
				short_form++;
				break;
			default:
				usage();
				/* NOTREACHED */
		}
	if (optind != argc) {
		usage();
		/* NOTREACHED */
	}
	if (!short_form) {
		fputs("OpenBSD", stdout);
		fputc('.', stdout);
	}
	fputs(arch, stdout);
	fputc('\n', stdout);
	exit(0);
}

static void
usage()
{
	if (machine)
		fprintf(stderr, "usage: machine [-a]\n");
	else
		fprintf(stderr, "usage: arch [-ks]\n");
	exit(1);
}
