/*	$NetBSD: ttyflags.c,v 1.6 1995/08/13 05:24:03 cgd Exp $	*/

/*
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1994 Christopher G. Demetriou\n\
	All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char rcsid[] = "$NetBSD: ttyflags.c,v 1.6 1995/08/13 05:24:03 cgd Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/ioctl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <ttyent.h>
#include <unistd.h>

int change_all __P((void));
int change_ttyflags __P((struct ttyent *));
int change_ttys __P((char **));
void usage __P((void));

int nflag, vflag;

/*
 * Ttyflags sets the device-specific tty flags, based on the contents
 * of /etc/ttys.  It can either set all of the ttys' flags, or set
 * the flags of the ttys specified on the command line.
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	int aflag, ch, rval;

	aflag = nflag = vflag = 0;
	while ((ch = getopt(argc, argv, "anv")) != EOF)
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'n':		/* undocumented */
			nflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (aflag && argc != 0)
		usage();

	rval = 0;

	if (setttyent() == 0)
		err(1, "setttyent");

	if (aflag)
		rval = change_all();
	else
		rval = change_ttys(argv);

	if (endttyent() == 0)
		warn("endttyent");

	exit(rval);
}

/*
 * Change all /etc/ttys entries' flags.
 */
int
change_all()
{
	struct ttyent *tep;
	int rval;

	rval = 0;
	for (tep = getttyent(); tep != NULL; tep = getttyent())
		if (change_ttyflags(tep))
			rval = 1;
	return (rval);
}

/*
 * Change the specified ttys' flags.
 */
int
change_ttys(ttylist)
	char **ttylist;
{
	struct ttyent *tep;
	int rval;

	rval = 0;
	for (; *ttylist != NULL; ttylist++) {
		tep = getttynam(*ttylist);
		if (tep == NULL) {
			warnx("couldn't find an entry in %s for \"%s\"",
			    _PATH_TTYS, *ttylist);
			rval = 1;
			continue;
		}

		if (change_ttyflags(tep))
			rval = 1;
	}
	return (rval);
}

/*
 * Acutually do the work; find out what the new flags value should be,
 * open the device, and change the flags.
 */
int
change_ttyflags(tep)
	struct ttyent *tep;
{
	int fd, flags, rval, st;
	char path[PATH_MAX];

	st = tep->ty_status;
	flags = rval = 0;

	/* Convert ttyent.h flags into ioctl flags. */
	if (st & TTY_LOCAL)
		flags |= TIOCFLAG_CLOCAL;
	if (st & TTY_RTSCTS)
		flags |= TIOCFLAG_CRTSCTS;
	if (st & TTY_SOFTCAR)
		flags |= TIOCFLAG_SOFTCAR;
	if (st & TTY_MDMBUF)
		flags |= TIOCFLAG_MDMBUF;

	/* Find the full device path name. */
	(void)snprintf(path, sizeof path, "%s%s", _PATH_DEV, tep->ty_name);

	if (vflag)
		warnx("setting flags on %s to %0x", path, flags);
	if (nflag)
		return (0);

	/* Open the device NON-BLOCKING, set the flags, and close it. */
	if ((fd = open(path, O_RDONLY | O_NONBLOCK, 0)) == -1) {
		if (!(errno == ENXIO ||
		      (errno == ENOENT && (st & TTY_ON) == 0)))
			rval = 1;
		if (rval || vflag)
			warn("open %s", path);
		return (rval);
	}
	if (ioctl(fd, TIOCSFLAGS, &flags) == -1)
		if (errno != ENOTTY || vflag) {
			warn("TIOCSFLAGS on %s", path);
			rval = (errno != ENOTTY);
		}
	if (close(fd) == -1) {
		warn("close %s", path);
		return (1);
	}
	return (rval);
}

/*
 * Print usage information when a bogus set of arguments is given.
 */
void
usage()
{
	(void)fprintf(stderr, "usage: ttyflags [-v] [-a | tty ... ]\n");
	exit(1);
}
