/*	$NetBSD: mount_tcfs.c,v 1.5 1997/09/16 12:31:02 lukem Exp $	*/

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

#include <sys/cdefs.h>


#include <sys/types.h>
#include <miscfs/tcfs/tcfs.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "mntopts.h"

#define ALTF_LABEL	0x1
#define ALTF_CIPHER	0x2

const struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ "label", 0, ALTF_LABEL, 1},
	{ "cipher", 0, ALTF_CIPHER, 1},
	{ NULL }
};

int	main __P((int, char *[]));
int	subdir __P((const char *, const char *));
void	usage __P((void));
int	tcfs_mount_getcipher __P((char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct tcfs_args args;
	int ch, mntflags, altflags;
	char target[MAXPATHLEN];

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
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	if (realpath(argv[0], target) == 0)
		err(1, "%s", target);

	if (subdir(target, argv[1]) || subdir(argv[1], target))
		errx(1, "%s (%s) and %s are not distinct paths",
		    argv[0], target, argv[1]);

	if (args.cipher_num == -1) { 
		printf("cipher number not found for filesystem %s\n", argv[1]);
		exit(1);
	}
        args.target = target;
	
	if (mount(MOUNT_TCFS, argv[1], mntflags, &args))
		err(1, "%s", "");
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
usage()
{
	(void)fprintf(stderr,
		"usage: mount_tcfs [-o options] target_fs mount_point\n");
	exit(1);
}
