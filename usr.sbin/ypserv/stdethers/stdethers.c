/*	$OpenBSD: stdethers.c,v 1.5 2002/07/19 02:38:40 deraadt Exp $ */

/*
 * Copyright (c) 1995 Mats O Jansson <moj@stacken.kth.se>
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
 *	This product includes software developed by Mats O Jansson
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$OpenBSD: stdethers.c,v 1.5 2002/07/19 02:38:40 deraadt Exp $";
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

char *ProgramName = "stdethers";

extern int   ether_line(char *, struct ether_addr *, char *);
extern char *ether_ntoa(struct ether_addr *);

#ifndef NTOA_FIX
#define	NTOA(x) (char *)ether_ntoa(x)
#else
#define NTOA(x) (char *) working_ntoa((u_char *) x)

/* As of 1995-12-02 NetBSD and OpenBSD has an SunOS 4 incompatible ether_ntoa.
   The code in usr/lib/libc/net/ethers seems to do the correct thing
   when asking YP but not when returning string from ether_ntoa.
 */

char *
working_ntoa(u_char *e)
{
	static char a[] = "xx:xx:xx:xx:xx:xx";

	sprintf(a, "%x:%x:%x:%x:%x:%x",
	    e[0], e[1], e[2], e[3], e[4], e[5]);
	return a;
}
#endif

static int
read_line(FILE *fp, char *buf, int size)
{
	int done = 0;

	do {
		while (fgets(buf, size, fp)) {
			int len = strlen(buf);
			done += len;
			if (len > 1 && buf[len-2] == '\\' &&
			    buf[len-1] == '\n') {
				int ch;

				buf += len - 2;
				size -= len - 2;
				*buf = '\n';
				buf[1] = '\0';

				/*
				 * Skip leading white space on next line
				 */
				while ((ch = getc(fp)) != EOF &&
				    isascii(ch) && isspace(ch))
					;
				(void) ungetc(ch, fp);
			} else
				return done;
		}
	} while (size > 0 && !feof(fp));
	return done;
}

int
main(int argc, char *argv[])
{
	FILE	*data_file;
	char	 data_line[1024];
	int	 usage = 0;
	int	 line_no = 0;
	int	 len;
	char	*p, *k, *v;
	struct ether_addr eth_addr;
	char	 hostname[256];

	if (argc > 2)
		usage++;

	if (usage) {
		fprintf(stderr, "usage: %s [file]\n", ProgramName);
		exit(1);
	}

	if (argc == 2) {
		data_file = fopen(argv[1], "r");
		if (data_file == NULL) {
			fprintf(stderr, "%s: can't open %s\n",
			    ProgramName, argv[1]);
			exit(1);
		}
	} else
		data_file = stdin;

	while (read_line(data_file, data_line, sizeof(data_line))) {
		line_no++;
		len = strlen(data_line);

		if (len > 0) {
			if (data_line[0] == '#')
				continue;
		}

		/*
		 * Check if we have the whole line
		 */
		if (data_line[len-1] != '\n') {
			if (argc == 2) {
				fprintf(stderr,
				    "line %d in \"%s\" is too long",
				    line_no, argv[1]);
			} else {
				fprintf(stderr,
				    "line %d in \"stdin\" is too long",
				    line_no);
			}
		} else
			data_line[len-1] = '\0';

		p = (char *) &data_line;

		k  = p;				/* save start of key */
		while (!isspace(*p))		/* find first "space" */
			p++;
		while (isspace(*p))		/* move over "space" */
			p++;

		v = p;				/* save start of value */
		while (*p != '\0')		/* find end of string */
			p++;

		if (ether_line(data_line, &eth_addr, hostname) == 0) {
			fprintf(stdout, "%s\t%s\n", NTOA(&eth_addr),
			    hostname);
		} else {
			fprintf(stderr, "%s: ignoring line %d: \"%s\"\n",
			    ProgramName, line_no, data_line);
		}
	}
	return(0);
}
