/*	$NetBSD: rawwrite.c,v 1.1.1.1 1995/04/06 21:04:54 leo Exp $	*/

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
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#define	SECT_SIZE	512		/* Sector size			*/
#define	NSECT_DD	18		/* Sectors per track 720Kb	*/
#define	NSECT_HD	36		/* Sectors per track 1.44Mb	*/
#define	NTRK		80		/* Number of tracks		*/

static void usage();
static void brwrite();

char	buf[NSECT_HD * SECT_SIZE];
int	vflag = 0;
char	*progname;

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
	while ((ch = getopt(argc, argv, "v")) != EOF) {
		switch(ch) {
			case 'v':
				vflag = 1;
				break;
			default :
				usage();
				break;
		}
	}
	if(optind >= argc)
		usage();

	infile = argv[optind];
	nsect  = NSECT_DD;

	if((fd = open(infile, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: Cannot open '%s'\n", progname, infile);
		exit(1);
	}

	for(i = 0; i < NTRK; i++) {
		if(read(fd, buf, nsect * SECT_SIZE) != (nsect * SECT_SIZE)) {
		    fprintf(stderr, "\nRead error on '%s'\n", progname, infile);
		    exit(1);
		}
		if(vflag) {
			if(i && !(i % 40))
				printf("\n");
			fprintf(stderr, ".");
		}
		brwrite(buf, nsect * i, nsect);
	}
	close(fd);
	if(vflag)
		printf("\n");
}

static void brwrite(buf, blk, cnt)
char	*buf;
int	blk, cnt;
{
	if(Rwabs(3, buf, cnt, blk, 0) != 0) {
		fprintf(stderr, "\n%s: Write error on floppy\n", progname);
		exit(1);
	}
}

static void usage()
{
	fprintf(stderr, "usage: rawwrite [-v] <infile>\n");
	exit(1);
}
