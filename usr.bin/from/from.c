/*	$OpenBSD: from.c,v 1.26 2018/08/08 17:52:46 deraadt Exp $	*/
/*	$NetBSD: from.c,v 1.6 1995/09/01 01:39:10 jtc Exp $	*/

/*
 * Copyright (c) 1980, 1988, 1993
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
#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>
#include <string.h>
#include <err.h>
#include <errno.h>

int	match(char *, char *);
char	*mail_spool(char *file, const char *user);

int
main(int argc, char *argv[])
{
	int ch, newline, fflag = 0;
	char *file, *line, *sender, *p;
	size_t linesize = 0;
	ssize_t linelen;
	FILE *fp;

	file = line = sender = NULL;
	while ((ch = getopt(argc, argv, "f:s:")) != -1) {
		switch(ch) {
		case 'f':
			fflag = 1;
			file = optarg;
			break;
		case 's':
			sender = optarg;
			for (p = sender; *p; ++p)
				if (isupper((unsigned char)*p))
					*p = tolower((unsigned char)*p);
			break;
		default:
			fprintf(stderr,
			    "usage: from [-f file] [-s sender] [user]\n");
			exit(EXIT_FAILURE);
		}
	}
	argv += optind;

	if (pledge("stdio unveil rpath getpw", NULL) == -1)
		err(1, "pledge");

	file = mail_spool(file, *argv);

	if (unveil(file, "r") == -1)
		err(1, "unveil");
	if (pledge("stdio rpath getpw", NULL) == -1)
		err(1, "pledge");

	if ((fp = fopen(file, "r")) == NULL) {
		if (!fflag && errno == ENOENT)
			exit(EXIT_SUCCESS);
		err(1, "%s", file);
	}

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	for (newline = 1; (linelen = getline(&line, &linesize, fp)) != -1;) {
		if (*line == '\n') {
			newline = 1;
			continue;
		}
		if (newline && !strncmp(line, "From ", 5) &&
		    (!sender || match(line + 5, sender)))
			printf("%s", line);
		newline = 0;
	}
	free(line);
	if (ferror(fp))
		err(EXIT_FAILURE, "getline");
	fclose(fp);
	exit(EXIT_SUCCESS);
}

char *
mail_spool(char *file, const char *user)
{
	struct passwd *pwd;

	/*
	 * We find the mailbox by:
	 *	1 -f flag
	 *	2 _PATH_MAILDIR/user (from argv)
	 *	2 MAIL environment variable
	 *	3 _PATH_MAILDIR/user (from environment or passwd db)
	 */
	if (file == NULL) {
		if (user == NULL) {
			if ((file = getenv("MAIL")) == NULL) {
				if ((user = getenv("LOGNAME")) == NULL &&
				    (user = getenv("USER")) == NULL) {
					if (!(pwd = getpwuid(getuid())))
						errx(1, "no password file "
						    "entry for you");
					user = pwd->pw_name;
				}
			}
		}
		if (file == NULL) {
			if (asprintf(&file, "%s/%s", _PATH_MAILDIR, user) == -1)
				err(1, NULL);
		}
	}
	return(file);
}

int
match(char *line, char *sender)
{
	char ch, pch, first, *p, *t;

	for (first = *sender++;;) {
		if (isspace((unsigned char)(ch = *line)))
			return(0);
		++line;
		if (isupper((unsigned char)ch))
			ch = tolower((unsigned char)ch);
		if (ch != first)
			continue;
		for (p = sender, t = line;;) {
			if (!(pch = *p++))
				return(1);
			if (isupper((unsigned char)(ch = *t++)))
				ch = tolower((unsigned char)ch);
			if (ch != pch)
				break;
		}
	}
	/* NOTREACHED */
}
