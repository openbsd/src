/*	$OpenBSD: dmesg.c,v 1.8 2000/02/22 19:27:38 deraadt Exp $	*/
/*	$NetBSD: dmesg.c,v 1.8 1995/03/18 14:54:49 cgd Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)dmesg.c	8.1 (Berkeley) 6/5/93";
#else
static char rcsid[] = "$OpenBSD: dmesg.c,v 1.8 2000/02/22 19:27:38 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/cdefs.h>
#include <sys/msgbuf.h>

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

struct nlist nl[] = {
#define	X_MSGBUF	0
	{ "_msgbufp" },
	{ NULL },
};

void usage __P((void));

#define	KREAD(addr, var) \
	kvm_read(kd, addr, &var, sizeof(var)) != sizeof(var)

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int ch, newl, skip, i;
	register char *p, *ep;
	struct msgbuf *bufp, cur;
	char *memf, *nlistf, *bufdata;
	kvm_t *kd;
	char buf[5];

	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, "M:N:")) != -1)
		switch(ch) {
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (memf != NULL || nlistf != NULL) {
		setegid(getgid());
		setgid(getgid());
	}

	/* Read in kernel message buffer, do sanity checks. */
	if ((kd = kvm_open(nlistf, memf, NULL, O_RDONLY, "dmesg")) == NULL)
		exit (1);

	setegid(getgid());
	setgid(getgid());

	if (kvm_nlist(kd, nl) == -1)
		errx(1, "kvm_nlist: %s", kvm_geterr(kd));
	if (nl[X_MSGBUF].n_type == 0)
		errx(1, "%s: msgbufp not found", nlistf ? nlistf : "namelist");
	if (KREAD(nl[X_MSGBUF].n_value, bufp))
		errx(1, "kvm_read: %s: (0x%lx)", kvm_geterr(kd),
		    nl[X_MSGBUF].n_value);
	if (KREAD((long)bufp, cur))
		errx(1, "kvm_read: %s (%0lx)", kvm_geterr(kd),
		    (unsigned long)bufp);
	if (cur.msg_magic != MSG_MAGIC)
		errx(1, "magic number incorrect");
	bufdata = malloc(cur.msg_bufs);
	if (bufdata == NULL)
		errx(1, "couldn't allocate space for buffer data");
	if (kvm_read(kd, (long)&bufp->msg_bufc, bufdata,
	    cur.msg_bufs) != cur.msg_bufs)
		errx(1, "kvm_read: %s", kvm_geterr(kd));
	if (cur.msg_bufx >= cur.msg_bufs)
		cur.msg_bufx = 0;
	kvm_close(kd);

	/*
	 * The message buffer is circular; start at the read pointer, and
	 * go to the write pointer - 1.
	 */
	for (newl = skip = i = 0, p = bufdata + cur.msg_bufx;
	    i < cur.msg_bufs; i++, p++) {
		if (p == bufdata + cur.msg_bufs)
			p = bufdata;
		ch = *p;
		/* Skip "\n<.*>" syslog sequences. */
		if (skip) {
			if (ch == '>')
				newl = skip = 0;
			continue;
		}
		if (newl && ch == '<') {
			skip = 1;
			continue;
		}
		if (ch == '\0')
			continue;
		newl = ch == '\n';
		(void)vis(buf, ch, 0, 0);
		if (buf[1] == 0)
			(void)putchar(buf[0]);
		else
			(void)printf("%s", buf);
	}
	if (!newl)
		(void)putchar('\n');
	exit(0);
}

void
usage()
{
	(void)fprintf(stderr, "usage: dmesg [-M core] [-N system]\n");
	exit(1);
}
