/*	$NetBSD: rawwrite.c,v 1.2 1996/01/07 22:06:24 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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

#include <osbind.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "libtos.h"

#define	SECT_SIZE	512		/* Sector size			*/
#define	NSECT_DD	18		/* Sectors per track 720Kb	*/
#define	NSECT_HD	36		/* Sectors per track 1.44Mb	*/
#define	NTRK		80		/* Number of tracks		*/

static void help    PROTO((void));
static void usage   PROTO((void));
static void brwrite PROTO((char *, int));

char	buf[NSECT_HD * SECT_SIZE];
int	h_flag = 0;	/* Show help					*/
int	v_flag = 0;	/* Verbose (a dot for each track copied)	*/
int	V_flag = 0;	/* Show version					*/
char	*progname;

const char version[] = "$Revision: 1.1 $";

int
main(argc, argv)
int	argc;
char	*argv[];
{
	extern	int	optind;
	extern	char	*optarg;
	int		ch;
	char		*infile;
	int		fd;
	int		i;
	int		nsect;

	progname = argv[0];
	init_toslib(argv[0]);

	while ((ch = getopt(argc, argv, "hvVwo:")) != EOF) {
		switch (ch) {
			case 'h':
				h_flag = 1;
				break;
			case 'o':
				redirect_output(optarg);
				break;
			case 'v':
				v_flag = 1;
				break;
			case 'V':
				V_flag = 1;
				break;
			case 'w':
				set_wait_for_key();
				break;
			default :
				usage();
				break;
		}
	}
	if (h_flag)
		help();
	if (V_flag)
		eprintf("%s\r\n", version);

	if (optind >= argc)
		usage();

	infile = argv[optind];
	nsect  = NSECT_DD;

	if ((fd = open(infile, O_RDONLY)) < 0)
		fatal(-1, "Cannot open '%s'\n", infile);

	for (i = 0; i < NTRK; i++) {
		if (read(fd, buf, nsect * SECT_SIZE) != (nsect * SECT_SIZE))
		    fatal(-1, "\n\rRead error on '%s'\n", infile);
		if (v_flag) {
			if (i && !(i % 40))
				eprintf("\r\n");
			eprintf(".");
		}
		brwrite(buf, i);
	}
	close(fd);
	if (v_flag)
		eprintf("\r\n");
	xexit(0);
}

static void
brwrite(buf, trk)
char	*buf;
int	trk;
{
	static u_char	trbuf[NSECT_DD * SECT_SIZE * 2];
	static u_int	sideno  = 0;

	for (sideno = 0; sideno < 2; sideno++) {
		if (Flopfmt(trbuf, 0, 0, NSECT_DD/2, trk, sideno, 1, 0x87654321,
																 0xe5e5))
			fatal(-1, "Format error");
		if (Flopwr(buf, 0, 0, 1, trk, sideno, NSECT_DD/2))
			fatal(-1, "Write error");
		buf += (NSECT_DD/2) * SECT_SIZE;
	}
}
static void
usage()
{
	eprintf("Usage: %s [-hvVw] [-o <log-file>] <infile>\r\n", progname);
	xexit(1);
}

static void
help()
{
	eprintf("\r
write a raw floppy-image to disk\r
\r
Usage: %s [-hvVw] [-o <log-file>] <infile>\r
\r
Description of options:\r
\r
\t-h  What your getting right now.\r
\t-o  Write output to both <output file> and stdout.\r
\t-v  Show a '.' for each track written.\r
\t-V  Print program version.\r
\t-w  Wait for a keypress before exiting.\r
", progname);
	xexit(0);
}
