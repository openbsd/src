/*	$OpenBSD: mount_tcfs.c,v 1.6 2002/03/14 06:51:41 mpech Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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

#include <sys/param.h>
#include <sys/mount.h>
#include <miscfs/tcfs/tcfs.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mntopts.h"

#define ALTF_LABEL	0x1
#define ALTF_CIPHER	0x2

const struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ "label", 0, ALTF_LABEL, 1},
	{ "cipher", 0, ALTF_CIPHER, 1},
	{ NULL }
};

int	subdir(const char *, const char *);
void	tcfs_usage(void);

int
main(argc, argv)
	int argc;
	char * const argv[];
{
	struct tcfs_args args;
	int ch, mntflags, altflags;
	char target[MAXPATHLEN];
	char *fs_name, *errcause;

	mntflags = 0;
	altflags = 0;
	args.cipher_num = -1;
	while ((ch = getopt(argc, argv, "o:")) != -1)
		switch(ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &altflags);
			if (altflags & ALTF_CIPHER) {
				char *p, *cipherfield;

				cipherfield = strstr(optarg, "cipher=") + 7;
				args.cipher_num = strtol(cipherfield, &p, 0);
				if (cipherfield == p)
					args.cipher_num = -1;
			}
			altflags = 0;
			break;
		case '?':
		default:
			tcfs_usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		tcfs_usage();

	if (realpath(argv[0], target) == 0)
		err(1, "%s", target);

	if (subdir(target, argv[1]) || subdir(argv[1], target))
		errx(1, "%s (%s) and %s are not distinct paths",
		    argv[0], target, argv[1]);

	if (args.cipher_num == -1)
		errx(1, "cipher number not found for filesystem %s",
		    argv[1]);

        args.target = target;
	fs_name = argv[1];
	
	if (mount(MOUNT_TCFS, fs_name, mntflags, &args) < 0) {
		switch (errno) {
		case EMFILE:
			errcause = "mount table full";
			break;
		case EOPNOTSUPP:
			errcause = "filesystem not supported by kernel";
			break;
		default:
			errcause = strerror(errno);
			break;
		}
		errx(1, "%s on %s: %s", argv[0], fs_name, errcause);
	}
	exit(0);
}

int
subdir(p, dir)
	const char *p;
	const char *dir;
{
	int l;

	l = strlen(dir);
	if (l <= 1)
		return (1);

	if ((strncmp(p, dir, l) == 0) && (p[l] == '/' || p[l] == '\0'))
		return (1);

	return (0);
}

void
tcfs_usage()
{
	(void)fprintf(stderr, "usage: mount_tcfs [-o options] path node\n");
	exit(1);
}
