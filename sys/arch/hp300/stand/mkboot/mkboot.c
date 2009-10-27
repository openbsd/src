/*	$OpenBSD: mkboot.c,v 1.6 2009/10/27 23:59:34 deraadt Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/file.h>
#include <a.out.h>

#include "volhdr.h"

#include <stdio.h>
#include <ctype.h>

#define LIF_NUMDIR	8

#define LIF_VOLSTART	0
#define LIF_VOLSIZE	sizeof(struct lifvol)
#define LIF_DIRSTART	512
#define LIF_DIRSIZE	(LIF_NUMDIR * sizeof(struct lifdir))
#define LIF_FILESTART	8192

#define btolifs(b)	(((b) + (SECTSIZE - 1)) / SECTSIZE)
#define lifstob(s)	((s) * SECTSIZE)

int lpflag;
int loadpoint;
struct load ld;
struct lifvol lifv;
struct lifdir lifd[LIF_NUMDIR];
struct exec ex;
char buf[10240];

void	bcddate(int, char *);
char *	lifname(char *);
void	putfile(int, int);
void	usage(void);

/*
 * Old Format:
 *	sector 0:	LIF volume header (40 bytes)
 *	sector 1:	<unused>
 *	sector 2:	LIF directory (8 x 32 == 256 bytes)
 *	sector 3-:	LIF file 0, LIF file 1, etc.
 * where sectors are 256 bytes.
 *
 * New Format:
 *	sector 0:	LIF volume header (40 bytes)
 *	sector 1:	<unused>
 *	sector 2:	LIF directory (8 x 32 == 256 bytes)
 *	sector 3:	<unused>
 *	sector 4-31:	disklabel (~300 bytes right now)
 *	sector 32-:	LIF file 0, LIF file 1, etc.
 */
int
main(argc, argv)
	int argc;
	char **argv;
{
	int ac;
	char **av;
	int from1, from2, from3, to;
	int n;
	char *n1, *n2, *n3;

	ac = --argc;
	av = ++argv;
	if (ac == 0)
		usage();
	if (!strcmp(av[0], "-l")) {
		av++;
		ac--;
		if (ac == 0)
			usage();
		sscanf(av[0], "0x%x", &loadpoint);
		lpflag++;
		av++;
		ac--;
	}
	if (ac == 0)
		usage();
	from1 = open(av[0], O_RDONLY, 0);
	if (from1 < 0) {
		perror("open");
		exit(1);
	}
	n1 = av[0];
	av++;
	ac--;
	if (ac == 0)
		usage();
	if (ac > 1) {
		from2 = open(av[0], O_RDONLY, 0);
		if (from2 < 0) {
			perror("open");
			exit(1);
		}
		n2 = av[0];
		av++;
		ac--;
		if (ac > 1) {
			from3 = open(av[0], O_RDONLY, 0);
			if (from3 < 0) {
				perror("open");
				exit(1);
			}
			n3 = av[0];
			av++;
			ac--;
		} else
			from3 = -1;
	} else
		from2 = from3 = -1;
	to = open(av[0], O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (to < 0) {
		perror("open");
		exit(1);
	}
	/* clear possibly unused directory entries */
	bcopy("          ", lifd[1].dir_name, 10);
	lifd[1].dir_type = -1;
	lifd[1].dir_addr = 0;
	lifd[1].dir_length = 0;
	lifd[1].dir_flag = 0xFF;
	lifd[1].dir_exec = 0;
	lifd[7] = lifd[6] = lifd[5] = lifd[4] = lifd[3] = lifd[2] = lifd[1];
	/* record volume info */
	lifv.vol_id = VOL_ID;
	bcopy("BOOT44", lifv.vol_label, 6);
	lifv.vol_addr = btolifs(LIF_DIRSTART);
	lifv.vol_oct = VOL_OCT;
	lifv.vol_dirsize = btolifs(LIF_DIRSIZE);
	lifv.vol_version = 1;
	/* output bootfile one */
	lseek(to, LIF_FILESTART, 0);
	putfile(from1, to);
	n = btolifs(ld.count + sizeof(ld));
	bcopy(lifname(n1), lifd[0].dir_name, 10);
	lifd[0].dir_type = DIR_TYPE;
	lifd[0].dir_addr = btolifs(LIF_FILESTART);
	lifd[0].dir_length = n;
	bcddate(from1, lifd[0].dir_toc);
	lifd[0].dir_flag = DIR_FLAG;
	lifd[0].dir_exec = lpflag? loadpoint + ex.a_entry : ex.a_entry;
	lifv.vol_length = lifd[0].dir_addr + lifd[0].dir_length;
	/* if there is an optional second boot program, output it */
	if (from2 >= 0) {
		lseek(to, LIF_FILESTART+lifstob(n), 0);
		putfile(from2, to);
		n = btolifs(ld.count + sizeof(ld));
		bcopy(lifname(n2), lifd[1].dir_name, 10);
		lifd[1].dir_type = DIR_TYPE;
		lifd[1].dir_addr = lifv.vol_length;
		lifd[1].dir_length = n;
		bcddate(from2, lifd[1].dir_toc);
		lifd[1].dir_flag = DIR_FLAG;
		lifd[1].dir_exec = lpflag? loadpoint + ex.a_entry : ex.a_entry;
		lifv.vol_length = lifd[1].dir_addr + lifd[1].dir_length;
	}
	/* ditto for three */
	if (from3 >= 0) {
		lseek(to, LIF_FILESTART+lifstob(lifd[0].dir_length+n), 0);
		putfile(from3, to);
		n = btolifs(ld.count + sizeof(ld));
		bcopy(lifname(n3), lifd[2].dir_name, 10);
		lifd[2].dir_type = DIR_TYPE;
		lifd[2].dir_addr = lifv.vol_length;
		lifd[2].dir_length = n;
		bcddate(from3, lifd[2].dir_toc);
		lifd[2].dir_flag = DIR_FLAG;
		lifd[2].dir_exec = lpflag? loadpoint + ex.a_entry : ex.a_entry;
		lifv.vol_length = lifd[2].dir_addr + lifd[2].dir_length;
	}
	/* output volume/directory header info */
	lseek(to, LIF_VOLSTART, 0);
	write(to, &lifv, LIF_VOLSIZE);
	lseek(to, LIF_DIRSTART, 0);
	write(to, lifd, LIF_DIRSIZE);
	return (0);
}

void
putfile(int from, int to)
{
	int n, tcnt, dcnt;

	n = read(from, &ex, sizeof(ex));
	if (n != sizeof(ex)) {
		fprintf(stderr, "error reading file header\n");
		exit(1);
	}
	if (N_GETMAGIC(ex) == OMAGIC) {
		tcnt = ex.a_text;
		dcnt = ex.a_data;
	}
	else if (N_GETMAGIC(ex) == NMAGIC) {
		tcnt = (ex.a_text + PGOFSET) & ~PGOFSET;
		dcnt = ex.a_data;
	}
	else {
		fprintf(stderr, "bad magic number\n");
		exit(1);
	}
	ld.address = lpflag ? loadpoint : ex.a_entry;
	ld.count = tcnt + dcnt;
	write(to, &ld, sizeof(ld));
	while (tcnt) {
		n = sizeof(buf);
		if (n > tcnt)
			n = tcnt;
		n = read(from, buf, n);
		if (n < 0) {
			perror("read");
			exit(1);
		}
		if (n == 0) {
			fprintf(stderr, "short read\n");
			exit(1);
		}
		if (write(to, buf, n) < 0) {
			perror("write");
			exit(1);
		}
		tcnt -= n;
	}
	while (dcnt) {
		n = sizeof(buf);
		if (n > dcnt)
			n = dcnt;
		n = read(from, buf, n);
		if (n < 0) {
			perror("read");
			exit(1);
		}
		if (n == 0) {
			fprintf(stderr, "short read\n");
			exit(1);
		}
		if (write(to, buf, n) < 0) {
			perror("write");
			exit(1);
		}
		dcnt -= n;
	}
}

void
usage()
{
	fprintf(stderr,
		"usage:	 mkboot [-l loadpoint] prog1 [ prog2 ] outfile\n");
	exit(1);
}

char *
lifname(str)
	char *str;
{
	static char lname[10] = "SYS_XXXXX";
	int i;

	for (i = 4; i < 10; i++) {
		if (islower(*str))
			lname[i] = toupper(*str);
		else if (isalnum(*str) || *str == '_')
			lname[i] = *str;
		else
			break;
		str++;
	}
	for ( ; i < 10; i++)
		lname[i] = ' ';
	return(lname);
}

#include <sys/stat.h>
#include <time.h>	/* XXX */

void
bcddate(fd, toc)
	int fd;
	char *toc;
{
	struct stat statb;
	struct tm *tm;

	fstat(fd, &statb);
	tm = localtime(&statb.st_ctime);
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
