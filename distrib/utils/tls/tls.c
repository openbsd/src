/*	$OpenBSD: tls.c,v 1.2 2000/03/01 22:10:13 todd Exp $	*/
/*	$NetBSD: tls.c,v 1.1.1.1 1995/10/08 23:08:47 gwr Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
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

#include <dirent.h>
#include <stdio.h>
#include <time.h>

int iflag;

void show_long(char *fname);

main(argc, argv)
	int argc;
	char **argv;
{
	DIR *dfp;
	struct dirent *d;

	/* If given an arg, just cd there first. */
	if (argc > 1) {
		if (chdir(argv[1])) {
			perror(argv[1]);
			exit(1);
		}
	}
	if (argc > 2)
		fprintf(stderr, "extra args ignored\n");

	dfp = opendir(".");
	if (dfp == NULL) {
		perror("opendir");
		return;
	}

	while ((d = readdir(dfp)) != NULL)
		show_long(d->d_name);

	closedir(dfp);
	exit(0);
}

/* XXX - This is system dependent... */
char ifmt_name[16] = {
	'?',	/* 0: nothing */
	'P',	/* 1: fifo (pipe) */
	'C',	/* 2: chr device */
	'?',	/* 3: ? */
	'D',	/* 4: dir */
	'?',	/* 5: ? */
	'B',	/* 6: blk device */
	'?',	/* 7: ? */
	'F',	/* 8: file */
	'?',	/* 9: ? */
	'L',	/* A: link */
	'?',	/* B: ? */
	'S',	/* C: socket */
	'?',	/* D: ? */
	'W',	/* E: whiteout */
	'?' 	/* F: ? */
};

void
show_long(fname)
	char *fname;
{
	struct stat st;
	int ifmt;
	char ifmt_c;
	char *date;

	if (lstat(fname, &st)) {
		perror(fname);
		return;
	}
	ifmt = (st.st_mode >> 12) & 15;
	ifmt_c = ifmt_name[ifmt];

	if (iflag) {
		/* inode number */
		printf("%6d ",  st.st_ino);
	}

	/* fmt/mode */
	printf("%c:",   ifmt_c);
	printf("%04o ", st.st_mode & 07777);

	/* nlinks, uid, gid */
	printf("%2d ",   st.st_nlink);
	printf("%4d ",  st.st_uid);
	printf("%4d ",  st.st_gid);

	/* size or major/minor */
	if ((ifmt_c == 'B') || (ifmt_c == 'C')) {
		printf("%2d, ", major(st.st_rdev));
		printf("%3d ",  minor(st.st_rdev));
	} else {
		printf("%7d ",  (int) st.st_size);
	}

	/* date */
	date = ctime(&st.st_mtime);
	date += 4;	/* skip day-of-week */
	date[12] = '\0';	/* to the minute */
	printf("%s ", date);

	/* name */
	printf("%s", fname);

	if (ifmt_c == 'L') {
		char linkto[256];
		int n;

		n = readlink(fname, linkto, sizeof(linkto)-1);
		if (n < 0) {
			perror(fname);
			return;
		}
		linkto[n] = '\0';
		printf(" -> %s", linkto);
	}
	printf("\n");
}
