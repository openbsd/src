/*	$OpenBSD: modstat.c,v 1.20 2003/01/18 23:30:20 deraadt Exp $	*/

/*
 * Copyright (c) 1993 Terrence R. Lambert.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/lkm.h>

#include <a.out.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

#define POINTERSIZE	((int)(2 * sizeof(void*)))

static char *type_names[] = {
	"SYSCALL",
	"VFS",
	"DEV",
	"EXEC",
	"MISC"
};

static void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-i id] [-n name]\n", __progname);
	exit(1);
}

static int
dostat(int devfd, int modnum, char *modname)
{
	char name[MAXLKMNAME];
	struct lmc_stat	sbuf;

	bzero(&name, sizeof name);
	bzero(&sbuf, sizeof sbuf);
	sbuf.id = modnum;
	sbuf.name = name;

	if (modname != NULL) {
		if (strlen(modname) >= sizeof(name))
			return 4;
		strlcpy(sbuf.name, modname, sizeof(name));
	}

	if (ioctl(devfd, LMSTAT, &sbuf) == -1) {
		switch (errno) {
		case EINVAL:		/* out of range */
			return 2;
		case ENOENT:		/* no such entry */
			return 1;
		default:		/* other error (EFAULT, etc) */
			warn("LMSTAT");
			return 4;
		}
	}

	/* Decode this stat buffer... */
	printf("%-7s %3d %3ld %0*lx %04lx %0*lx %3ld %s\n",
	    type_names[sbuf.type], sbuf.id, sbuf.offset, POINTERSIZE,
	    (long)sbuf.area, (long)sbuf.size, POINTERSIZE,
	    (long)sbuf.private, (long)sbuf.ver, sbuf.name);

	return 0;
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c, modnum = -1;
	char *modname = NULL;
	char *endptr;
	int devfd;

	while ((c = getopt(argc, argv, "i:n:")) != -1) {
		switch (c) {
		case 'i':
			modnum = (int)strtol(optarg, &endptr, 0);
			if (modnum < 0 || modnum > INT_MAX || *endptr != '\0')
				errx(1, "%s: not a valid number", optarg);
			break;
		case 'n':
			modname = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	/*
	 * Open the virtual device device driver for exclusive use (needed
	 * to ioctl() to retrive the loaded module(s) status).
	 */
	if ((devfd = open(_PATH_LKM, O_RDONLY)) == -1)
		err(2, "%s", _PATH_LKM);

	setegid(getgid());
	setgid(getgid());

	printf("Type     Id Off %-*s Size %-*s Rev Module Name\n",
	    POINTERSIZE, "Loadaddr", POINTERSIZE, "Info");

	if (modnum != -1 || modname != NULL) {
		if (dostat(devfd, modnum, modname))
			exit(3);
		exit(0);
	}

	/* Start at 0 and work up until we receive EINVAL. */
	for (modnum = 0; dostat(devfd, modnum, NULL) < 2; modnum++)
		;

	exit(0);
}
