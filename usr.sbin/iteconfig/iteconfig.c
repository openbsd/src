/* $OpenBSD: iteconfig.c,v 1.3 2000/01/19 16:33:00 espie Exp $ */
/*	$NetBSD: iteconfig.c,v 1.4.6.1 1996/06/04 16:48:24 is Exp $	*/
/* Copyright (c) 1999 Marc Espie
 * All rights reserved.
 *
 * This code is derived from software developped by Christian E. Hopps.
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
 *      This product includes software developed by Marc Espie.
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
 *
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps
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
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/queue.h>

#if !defined(amiga) && !defined(atari)
#error "This source is not suitable for this architecture!"
#endif

#if defined(amiga)
#include <amiga/dev/grfabs_reg.h>
#include <amiga/dev/viewioctl.h>
#include <amiga/dev/iteioctl.h>
#endif /* defined(amiga)	*/

#if defined(atari)
#include <atari/dev/grfabs_reg.h>
#include <atari/dev/viewioctl.h>
#include <atari/dev/iteioctl.h>
#endif /* defined(atari)	*/

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#include "pathnames.h"

void	printcmap __P((colormap_t *, int));
void	usage __P((void));
void	xioctl __P((int, int, void *));
colormap_t *xgetcmap __P((int, int));
long	xstrtol __P((char *));
int main __P((int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct itewinsize is, newis;
	struct itebell ib, newib;
	int bt;
	struct winsize ws;
	char *file = _PATH_CONSOLE;
	int ch, fd, iflag, max_colors;
	int use_is, use_ib;

	max_colors = use_is = use_ib = iflag = 0;

		/* need two passes through options */
	while ((ch = getopt(argc, argv, "B:D:H:P:T:V:W:X:Y:b:d:f:h:ip:t:v:w:x:y:"))
	    != -1) {
		switch (tolower(ch)) {
		case 'd':
		case 'h':
		case 'w':
		case 'x':
		case 'y':
			use_is = 1;
			break;
		case 'f':
			file = optarg;
			break;
		case 'p':
		case 't':
		case 'v':
			use_ib = 1;
			break;
		case 'b':
		case 'i':
			break;
		default:
			usage();
		}
	}

	fd = open(file, O_RDONLY, O_NONBLOCK);
	if(fd == -1)
		err(1, "open \"%s\"", file);

	if (argc > optind) 
		use_is = 1;

	if (use_is) {
		xioctl(fd, ITEIOCGWINSZ, &is);
		memcpy(&newis, &is, sizeof is);
		max_colors = 1 << is.depth;
	}
	if (use_ib) {
		xioctl(fd, ITEIOCGBELL, &ib);
		memcpy(&newib, &ib, sizeof ib);
	}

	optind = 1;
	optreset = 1;
	
		
	while ((ch = getopt(argc, argv, "B:D:H:P:T:V:W:X:Y:b:d:f:h:ip:t:v:w:x:y:"))
	    != -1) {
		switch (tolower(ch)) {
		case 'i':
			iflag = 1;
			break;
		case 'f':
			break;
		case 'd':
			newis.depth = xstrtol(optarg);
			break;
		case 'h':
			newis.height = xstrtol(optarg);
			break;
		case 'w':
			newis.width = xstrtol(optarg);
			break;
		case 'x':
			newis.x = xstrtol(optarg);
			break;
		case 'y':
			newis.y = xstrtol(optarg);
			break;
		case 'p':
			newib.pitch = xstrtol(optarg);
			break;
		case 't':
			newib.msec = xstrtol(optarg);
			break;
		case 'v':
			newib.volume = xstrtol(optarg);
			break;
		case 'b':
#ifdef  ITEIOCSBLKTIME
			bt = xstrtol(optarg);
			xioctl(fd, ITEIOCSBLKTIME, &bt);
			break;
#else
			/*FALLTHRU*/
#endif
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (use_is && memcmp(&is, &newis, sizeof is) != 0) {
		xioctl(fd, ITEIOCSWINSZ, &newis);
		xioctl(fd, ITEIOCGWINSZ, &is);
		max_colors = 1 << is.depth;
	}
	if (use_ib && memcmp(&ib, &newib, sizeof ib) != 0) {
		xioctl(fd, ITEIOCSBELL, &newib);
		xioctl(fd, ITEIOCGBELL, &ib);
	}
	
	argc -= optind;
	argv += optind;


	if (argc) {
		int i;
		long val;
		colormap_t *cm;
		/*
		 * get, set and get colors again
		 */
		cm = xgetcmap(fd, max_colors);
		for (i = 0; i < argc; i++) {
			val = xstrtol(argv[i]);
			if (i >= max_colors) {
				warnx("warning: too many colors");
				break;
			}
			cm->entry[i] = val;
		}
		xioctl(fd, VIOCSCMAP, cm);
		free(cm);
	}

	/* do tty stuff to get it to register the changes. */
	xioctl(fd, TIOCGWINSZ, &ws);

	if (iflag) {
		printf("tty size: rows %d cols %d\n", ws.ws_row, ws.ws_col);
		if (use_is || ioctl(fd, ITEIOCGWINSZ, &is) != -1) {
			printf("ite size: w: %d  h: %d  d: %d  [x: %d  y: %d]\n",
			    is.width, is.height, is.depth, is.x, is.y);
			max_colors = 1 << is.depth;
		}
		if (use_ib || ioctl(fd, ITEIOCGBELL, &ib) != -1)
			printf("ite bell: vol: %d  millisec: %d  pitch: %d\n",
			    ib.volume, ib.msec, ib.pitch);
#ifdef ITEIOCGBLKTIME
		if (ioctl(fd, ITEIOCGBLKTIME, &bt) != -1) {
			printf("ite screenblanker: ");
			if (bt != 0)
				printf("%d seconds\n", bt);
			else
				printf("off\n");
		}
#endif
		if (max_colors) {
			colormap_t *cm;
			cm = xgetcmap(fd, max_colors);
			printcmap(cm, ws.ws_col);
			free(cm);
		}
	}
	close(fd);
	exit(0);
}

void
xioctl(fd, cmd, addr)
	int fd, cmd;
	void *addr;
{
	if (ioctl(fd, cmd, addr) == -1) 
		err(1, "ioctl");
}

long
xstrtol(s)
	char *s;
{
	long rv;

	rv = strtol(s, NULL, 0);
	if (errno == ERANGE && (rv == LONG_MIN || rv == LONG_MAX))
		err(1, "bad format: \"%s\"", s);
	return(rv);
}

colormap_t *
xgetcmap(fd, ncolors)
	int fd;
	int ncolors;
{
	colormap_t *cm;

	cm = malloc(sizeof(colormap_t) + ncolors * sizeof(u_long));
	if (cm == NULL)
		err(1, "malloc");
	cm->first = 0;
	cm->size = ncolors;
	cm->entry = (u_long *) & cm[1];
	xioctl(fd, VIOCGCMAP, cm);
	return(cm);
}

void
printcmap(cm, ncols)
	colormap_t *cm;
	int ncols;
{
	int i, nel;

	switch (cm->type) {
	case CM_MONO:
		printf("monochrome");
		return;
	case CM_COLOR:
		printf("color levels: red: %d  green: %d  blue: %d",
		    cm->red_mask + 1, cm->green_mask + 1, cm->blue_mask + 1);
		break;
	case CM_GREYSCALE:
		printf("greyscale levels: %d", cm->grey_mask + 1);
		break;
	}
	printf("\n");
	
	nel = ncols / 11 - 1;
	for (i = 0; i < cm->size; i++) {
		printf("0x%08lx ", cm->entry[i]);
		if ((i + 1) % nel == 0)
			printf("\n");
	}
	if ((i + 1) % nel)
		printf("\n");
}

void
usage()
{
	fprintf(stderr, "%s\n\t\t%s%s\n\t\t%s\n",
	    "usage: iteconfig [-i] [-f file] [-v volume] [-p pitch] [-t msec]",
#ifdef ITEIOCSBLKTIME
	    "[-b timeout]",
#else
	    "",
#endif
	    "[-w width] [-h height] [-d depth] [-x off] [-y off]",
	    "[color ...]");
	exit(1);
}
