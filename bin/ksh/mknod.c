/*	$OpenBSD: mknod.c,v 1.1 2005/10/06 06:39:36 otto Exp $	*/
/*	$NetBSD: mknod.c,v 1.8 1995/08/11 00:08:18 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Fall.
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
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mknod.c	8.1 (Berkeley) 6/5/93";
#else
static const char rcsid[] = "$OpenBSD: mknod.c,v 1.1 2005/10/06 06:39:36 otto Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "sh.h"

int
domknod(int argc, char **argv, mode_t mode)
{
	dev_t dev;
	char *endp;
	u_int major, minor;

	if (argv[1][0] == 'c')
		mode |= S_IFCHR;
	else if (argv[1][0] == 'b')
		mode |= S_IFBLK;
	else {
		bi_errorf("node must be type 'b' or 'c'.");
		return 1;
	}

	major = (long)strtoul(argv[2], &endp, 0);
	if (endp == argv[2] || *endp != '\0') {
		bi_errorf("non-numeric major number.");
		return 1;
	}
	minor = (long)strtoul(argv[3], &endp, 0);
	if (endp == argv[3] || *endp != '\0') {
		bi_errorf("non-numeric minor number.");
		return 1;
	}
	dev = makedev(major, minor);
	if (major(dev) != major || minor(dev) != minor) {
		bi_errorf("major or minor number too large");
		return 1;
	}
	if (mknod(argv[0], mode, dev) < 0) {
		bi_errorf("%s: %s", argv[0], strerror(errno));
		return 1;
	}
	return 0;
}

int
domkfifo(int argc, char **argv, mode_t mode)
{
	int rv = 0;

	if (mkfifo(argv[0], mode) < 0) {
		bi_errorf("%s: %s", argv[0], strerror(errno));
		rv = 1;
	}
	return(rv);
}

