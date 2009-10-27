/*	$OpenBSD: battlestar.c,v 1.16 2009/10/27 23:59:23 deraadt Exp $	*/
/*	$NetBSD: battlestar.c,v 1.3 1995/03/21 15:06:47 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1993
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

/*
 * Battlestar - a stellar-tropical adventure game
 *
 * Originally written by His Lordship, Admiral David W. Horatio Riggle,
 * on the Cory PDP-11/70, University of California, Berkeley.
 */

#include "extern.h"
#include "pathnames.h"

int main(int, char *[]);

int
main(int argc, char *argv[])
{
	char    mainbuf[LINELENGTH];
	char   *next;
	gid_t	gid;

	open_score_file();

	/* revoke privs */
	gid = getgid();
	setresgid(gid, gid, gid);

	if (argc < 2)
		initialize(NULL);
	else if (strcmp(argv[1], "-r") == 0)
		initialize((argc > 2) ? argv[2] : DEFAULT_SAVE_FILE);
	else
		initialize(argv[1]);

	newlocation();
	for (;;) {
		stop_cypher = 0;
		next = getcom(mainbuf, sizeof mainbuf, ">-: ",
		    "Please type in something.");
		for (wordcount = 0; next && wordcount < NWORD - 1; wordcount++)
			next = getword(next, words[wordcount], -1);
		parse();
		while (cypher())
			;
	}
}
