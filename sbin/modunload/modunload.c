/*	$OpenBSD: modunload.c,v 1.14 2003/09/19 17:36:03 deraadt Exp $	*/
/*	$NetBSD: modunload.c,v 1.9 1995/05/28 05:23:05 jtc Exp $	*/

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

static void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-i id] [-n name] [-p postunload]\n",
		__progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c, devfd;
	long modnum = -1;
	char *modname = NULL;
	char *endptr, *post = NULL;
	struct lmc_unload ulbuf;

	while ((c = getopt(argc, argv, "i:n:p:")) != -1) {
		switch (c) {
		case 'i':
			modnum = strtol(optarg, &endptr, 0);
			if (modnum < 0 || modnum > INT_MAX || *endptr != '\0')
                                errx(1, "not a valid number");
			break;
		case 'n':
			modname = optarg;
			break;
		case 'p':
			post = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0 || (modnum == -1 && modname == NULL))
		usage();


	/*
	 * Open the virtual device device driver for exclusive use (needed
	 * to ioctl() to retrive the loaded module(s) status).
	 */
	if ((devfd = open(_PATH_LKM, O_RDWR, 0)) == -1)
		err(2, "%s", _PATH_LKM);

	/*
	 * Unload the requested module.
	 */
	ulbuf.name = modname;
	ulbuf.id = (int)modnum;

	if (ioctl(devfd, LMUNLOAD, &ulbuf) == -1) {
		switch (errno) {
		case EINVAL:
			errx(3, "id out of range");
		case ENOENT:
			errx(3, "no such module");
		default:
			err(5, "LMUNLOAD");
		}
	}

	/*
	 * Execute the post-unload program.
	 */
	if (post) {
		execl(post, post, (char *)NULL);
		err(16, "can't exec `%s'", post);
	}
	exit(0);
}
