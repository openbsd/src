/*	$OpenBSD: main.c,v 1.3 1999/01/18 06:20:53 pjanzen Exp $	*/
/*	$NetBSD: main.c,v 1.3 1995/04/22 10:37:01 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.2 (Berkeley) 4/28/95";
#else
static char rcsid[] = "$OpenBSD: main.c,v 1.3 1999/01/18 06:20:53 pjanzen Exp $";
#endif
#endif /* not lint */

#include "extern.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

/*ARGSUSED*/
int
main(argc, argv)
	int argc;
	char **argv;
{
	char *p;
	int i;
	int fd;

	gid = getgid();
	egid = getegid();
	setegid(gid);

	fd = open("/dev/null", O_RDONLY);
	if (fd < 3)
		exit(1);
	close(fd);

	srandom(getpid());
	if ((p = strrchr(*argv, '/')))
		p++;
	else
		p = *argv;
	if (strcmp(p, "driver") == 0 || strcmp(p, "saildriver") == 0)
		mode = MODE_DRIVER;
	else if (strcmp(p, "sail.log") == 0)
		mode = MODE_LOGGER;
	else
		mode = MODE_PLAYER;
	while ((p = *++argv) && *p == '-')
		switch (p[1]) {
		case 'd':
			mode = MODE_DRIVER;
			break;
		case 's':
			mode = MODE_LOGGER;
			break;
		case 'D':
			debug++;
			break;
		case 'x':
			randomize++;
			break;
		case 'l':
			longfmt++;
			break;
		case 'b':
			nobells++;
			break;
		default:
			errx(1, "unknown flag %s", p);
		}
	if (*argv)
		game = atoi(*argv);
	else
		game = -1;
	if ((i = setjmp(restart)))
		mode = i;
	switch (mode) {
	case MODE_PLAYER:
		return pl_main();
	case MODE_DRIVER:
		return dr_main();
	case MODE_LOGGER:
		return lo_main();
	default:
		warnx("Unknown mode %d", mode);
		abort();
	}
	/*NOTREACHED*/
}
