/*	$OpenBSD: stripboot.c,v 1.1 1997/09/14 12:54:27 downsj Exp $	*/

/*
 * Copyright (c) 1990, 1993
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
 *	@(#)mkboot.c	8.1 (Berkeley) 7/15/93
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mkboot.c	7.2 (Berkeley) 12/16/90";
#endif
static char rcsid[] = "$OpenBSD: stripboot.c,v 1.1 1997/09/14 12:54:27 downsj Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <a.out.h>

#include "volhdr.h"

#include <stdio.h>
#include <ctype.h>

/*
 * This does nothing but strip the a.out header from a boot file ala mkboot.
 */

main(argc, argv)
	int argc;
	char **argv;
{
	int from, to;
	int n, tcnt, dcnt;
	struct load ld;
	struct exec ex;
	char buf[10240];

	if (argc != 3)
		usage();

	from = open(argv[1], O_RDONLY, 0600);
	if (from < 0) {
		perror(argv[1]);
		exit(1);
	}
	to = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (to < 0) {
		perror(argv[2]);
		exit(1);
	}

	n = read(from, &ex, sizeof(ex));
	if (n != sizeof(ex)) {
		fprintf(stderr, "error reading file header\n");
		exit(1);
	}
	if (N_GETMAGIC(ex) == OMAGIC) {
		tcnt = ex.a_text;
		dcnt = ex.a_data;
	}
	else if (N_GETMAGIC(ex) == NMAGIC) {
		tcnt = (ex.a_text + PGOFSET) & ~PGOFSET;
		dcnt = ex.a_data;
	}
	else {
		fprintf(stderr, "bad magic number\n");
		exit(1);
	}
	ld.address = ex.a_entry;
	ld.count = tcnt + dcnt;
	write(to, &ld, sizeof(ld));
	while (tcnt) {
		n = sizeof(buf);
		if (n > tcnt)
			n = tcnt;
		n = read(from, buf, n);
		if (n < 0) {
			perror("read");
			exit(1);
		}
		if (n == 0) {
			fprintf(stderr, "short read\n");
			exit(1);
		}
		if (write(to, buf, n) < 0) {
			perror("write");
			exit(1);
		}
		tcnt -= n;
	}
	while (dcnt) {
		n = sizeof(buf);
		if (n > dcnt)
			n = dcnt;
		n = read(from, buf, n);
		if (n < 0) {
			perror("read");
			exit(1);
		}
		if (n == 0) {
			fprintf(stderr, "short read\n");
			exit(1);
		}
		if (write(to, buf, n) < 0) {
			perror("write");
			exit(1);
		}
		dcnt -= n;
	}

	close(from);
	close(to);
}

usage()
{
	fprintf(stderr,
		"usage:	 stripboot infile outfile\n");
	exit(1);
}
