/*	$OpenBSD: snscore.c,v 1.10 2009/11/13 19:54:09 jsg Exp $	*/
/*	$NetBSD: snscore.c,v 1.5 1995/04/24 12:25:43 cgd Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
 */

#include <sys/types.h>
#include <err.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

#define MAXPLAYERS 256

struct player	{
	uid_t	uids;
	short	scores;
	char	*name;
} players[MAXPLAYERS], temp;

void
snscore(int fd, int topn)
{
	uid_t	uid;
	short	score;
	int	noplayers;
	int	i, j, notsorted;
	char	*q;
	struct	passwd	*p;

	if (fd < 0) {
		fd = open(_PATH_RAWSCORES, O_RDONLY, 0);
		if (fd < 0)
			errx(1, "Couldn't open raw scorefile");
	}

	lseek(fd, 0, SEEK_SET);
	printf("%sSnake scores to date:\n", topn > 0 ? "Top " : "");
	/* read(fd, &whoallbest, sizeof(uid_t));
	 * read(fd, &allbest, sizeof(short));   SCOREFILE FORMAT CHANGE
	 */
	noplayers = 0;
	for (uid = 0; ; uid++) {
		if (read(fd, &score, sizeof(short)) == 0)
			break;
		if (score > 0) {
			if (noplayers >= MAXPLAYERS)
				errx(2, "Too many entries in scorefile!");
			players[noplayers].uids = uid;
			players[noplayers].scores = score;
			p = getpwuid(uid);
			if (p == NULL)
				continue;
			q = p -> pw_name;
			if ((players[noplayers].name = strdup(q)) == NULL)
				err(1, "strdup");

			noplayers++;
		}
	}

	/* bubble sort scores */
	for (notsorted = 1; notsorted; ) {
		notsorted = 0;
		for (i = 0; i < noplayers - 1; i++)
			if (players[i].scores < players[i + 1].scores) {
				temp = players[i];
				players[i] = players[i + 1];
				players[i + 1] = temp;
				notsorted++;
			}
	}

	if ((topn > 0) && (topn < noplayers))
		noplayers = topn;
	j = 1;
	for (i = 0; i < noplayers; i++) {
		printf("%d:\t$%d\t%s\n", j, players[i].scores, players[i].name);
		if (i < noplayers - 1 &&
		    players[i].scores > players[i + 1].scores)
			j = i + 2;
	}
	if (noplayers == 0)
		printf("None.\n");
}
