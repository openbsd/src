/*	$OpenBSD: isolif.c,v 1.1 1997/09/15 06:20:56 downsj Exp $	*/

/*
 * Copyright (c) 1997, Jason Downs.  All rights reserved.
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
 *      This product includes software developed by Jason Downs for the
 *      OpenBSD system.
 * 4. Neither the name(s) of the author(s) nor the name OpenBSD
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1990, 1993
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
 *
 *	@(#)mkboot.c	8.1 (Berkeley) 7/15/93
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mkboot.c	7.2 (Berkeley) 12/16/90";
#endif
static char rcsid[] = "$OpenBSD: isolif.c,v 1.1 1997/09/15 06:20:56 downsj Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <a.out.h>

#include "volhdr.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define LIF_NUMDIR	8

#define LIF_VOLSTART	0
#define LIF_VOLSIZE	sizeof(struct lifvol)
#define LIF_DIRSTART	512
#define LIF_DIRSIZE	(LIF_NUMDIR * sizeof(struct lifdir))
#define LIF_FILESTART	8192

#define btolifs(b)	(((b) + (SECTSIZE - 1)) / SECTSIZE)
#define lifstob(s)	((s) * SECTSIZE)

#define ISO_BLOCKSIZE	2048

static void usage __P((void));
static char *lifname __P((char *));
static void bcddate __P((char *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, isosblk, isoeblk, rfd, to, apply;
	char *name, *outfile, *rawfile;
	struct lifvol lifv;
	struct lifdir lifd[LIF_NUMDIR];
	struct load ld;

	name = outfile = rawfile = NULL;
	apply = isosblk = isoeblk = 0;
	while ((ch = getopt(argc, argv, "as:e:n:r:o:")) != -1) {
		switch (ch) {
		case 'a':
			apply++;
			break;

		case 's':
			isosblk = atoi(optarg);
			break;

		case 'e':
			isoeblk = atoi(optarg);
			break;

		case 'n':
			name = optarg;
			break;

		case 'r':
			rawfile = optarg;
			break;

		case 'o':
			outfile = optarg;
			break;

		default:
			usage();
		}
	}

	if ((name == NULL) || (outfile == NULL) || (isosblk == 0) ||
	    (isoeblk == 0))
		usage();

	/* Need the ROM's header from the rawfile, unfortunately. */
	rfd = open(rawfile, O_RDONLY, 0600);
	if (rfd < 0) {
		perror(rawfile);
		exit(1);
	}
	if (read(rfd, &ld, sizeof(ld)) != sizeof(ld)) {
		perror("read");
		exit(1);
	}
	close(rfd);

	to = open(outfile, (apply ? O_WRONLY : O_WRONLY | O_TRUNC | O_CREAT),
	    0644);
	if (to < 0) {
		perror(outfile);
		exit(1);
	}

	/* clear possibly unused directory entries */
	strncpy(lifd[1].dir_name, "	     ", 10);
	lifd[1].dir_type = -1;
	lifd[1].dir_addr = 0;
	lifd[1].dir_length = 0;
	lifd[1].dir_flag = 0xFF;
	lifd[1].dir_exec = 0;
	lifd[7] = lifd[6] = lifd[5] = lifd[4] = lifd[3] = lifd[2] = lifd[1];
	/* record volume info */
	lifv.vol_id = VOL_ID;
	strncpy(lifv.vol_label, "BOOT43", 6);
	lifv.vol_addr = btolifs(LIF_DIRSTART);
	lifv.vol_oct = VOL_OCT;
	lifv.vol_dirsize = btolifs(LIF_DIRSIZE);
	lifv.vol_version = 1;
	/* output bootfile one */
	strcpy(lifd[0].dir_name, lifname(name));
	lifd[0].dir_type = DIR_TYPE;
	lifd[0].dir_addr = btolifs(isosblk * ISO_BLOCKSIZE);
	lifd[0].dir_length = btolifs(ld.count + sizeof(ld));
	bcddate(lifd[0].dir_toc);
	lifd[0].dir_flag = DIR_FLAG;
	lifd[0].dir_exec = ld.address;
	lifv.vol_length = lifd[0].dir_addr + lifd[0].dir_length;
	/* output volume/directory header info */
	lseek(to, LIF_VOLSTART, 0);
	write(to, &lifv, LIF_VOLSIZE);
	lseek(to, LIF_DIRSTART, 0);
	write(to, lifd, LIF_DIRSIZE);

	close(to);
	exit(0);
}

static void usage()
{
	fprintf(stderr,
	    "usage: isolif [-a] -s isostart -e isoend -n name -r rawfile -o outfile\n");
	exit(1);
}

static char *
lifname(str)
	char *str;
{
	static char lname[10] = "SYS_XXXXX";
	register int i;

	for (i = 4; i < 9; i++) {
		if (islower(*str))
			lname[i] = toupper(*str);
		else if (isalnum(*str) || *str == '_')
			lname[i] = *str;
		else
			break;
		str++;
	}
	for ( ; i < 10; i++)
		lname[i] = '\0';
	return(lname);
}

#include <sys/stat.h>
#include <time.h>	/* XXX */

static void
bcddate(toc)
	char *toc;
{
	time_t now;
	struct tm *tm;

	time(&now);
	tm = localtime(&now);
	*toc = ((tm->tm_mon+1) / 10) << 4;
	*toc++ |= (tm->tm_mon+1) % 10;
	*toc = (tm->tm_mday / 10) << 4;
	*toc++ |= tm->tm_mday % 10;
	*toc = (tm->tm_year / 10) << 4;
	*toc++ |= tm->tm_year % 10;
	*toc = (tm->tm_hour / 10) << 4;
	*toc++ |= tm->tm_hour % 10;
	*toc = (tm->tm_min / 10) << 4;
	*toc++ |= tm->tm_min % 10;
	*toc = (tm->tm_sec / 10) << 4;
	*toc |= tm->tm_sec % 10;
}
