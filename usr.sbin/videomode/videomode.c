/*	$NetBSD: videomode.c,v 1.2 1995/10/09 13:52:49 chopps Exp $	*/

/*
 * Copyright (c) 1995 Christian E. Hopps
 * Copyright (c) 1994 Markus Wild
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
 *      This product includes software developed by Markus Wild
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>

#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <stdio.h>

void dump_mode __P((int));
void dump_vm   __P((struct grfvideo_mode *));
int  get_grf __P((void));
void set_mode __P((int));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int m;
	int c;

	if (argc == 1) {
		dump_mode(0);
		return (0);
	}
	while ((c = getopt(argc, argv, "as:")) != EOF) {
		switch (c) {
		case 'a':
			if (optind < argc)
				usage();
			dump_mode(-1);
			return (0);
		case 's':
			m = atoi(optarg);
			if (m == 0 || optind < argc)
				usage();
			set_mode(m);
			return (0);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();

	dump_mode(atoi(*argv));
	return (0);
}


int
get_grf()
{
	struct stat stb;
	char grfname[80];
	int grffd;

	/* find out on which ite/grf we are */
	if (fstat(0, &stb) == -1)
		err(1, "fstat 0");
	if (((stb.st_mode & S_IFMT) != S_IFCHR) || !isatty(0))
		errx(1, "stdin not a tty");
	if (major(stb.st_rdev) != 13)
		errx(1, "stdin not an ite device");
	(void)sprintf(grfname, "/dev/grf%d", minor(stb.st_rdev) & 0x7);
	if ((grffd = open(grfname, 2)) < 0)
		err(1, "%s", grfname);
	return (grffd);
}

void
dump_mode(m)
	int m;
{
	struct grfvideo_mode vm;
	int num_vm;
	int grffd;

	grffd = get_grf();

	if (ioctl(grffd, GRFGETNUMVM, &num_vm) < 0)
		err(1, "GRFGETNUMVM");
	if (m > 0 && m > num_vm)
		errx(1, "no such mode");
	if (m <= 0) {
		(void)printf("Current mode:\n");
		vm.mode_num = 0;
		if (ioctl(grffd, GRFGETVMODE, &vm) == 0)
			dump_vm(&vm);
		(void)printf("\n");
	}
	if (m >= 0)
		return;
	for (m = 1; m <= num_vm; m++) {
		vm.mode_num = m;
		if (ioctl(grffd, GRFGETVMODE, &vm) == -1)
			break;
		dump_vm(&vm);
	}
}

void
set_mode(m)
	int m;
{
	int grffd;

	grffd = get_grf();
	(void)ioctl(grffd, GRFSETVMODE, &m);
}

void
dump_vm(vm)
	struct grfvideo_mode *vm;
{
	(void)printf("%d: %s\n", vm->mode_num, vm->mode_descr);
	(void)printf("pixel_clock = %u, width = %d, height = %d, depth = %d\n", 
	    vm->pixel_clock, vm->disp_width, vm->disp_height, vm->depth);
}

void
usage()
{
	(void)fprintf(stderr, "usage: videomode [mode]\n");
	(void)fprintf(stderr, "usage: videomode -a\n");
	(void)fprintf(stderr, "usage: videomode -s mode\n");
	exit(0);
}
