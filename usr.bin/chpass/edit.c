/*	$OpenBSD: edit.c,v 1.23 2002/07/31 22:08:42 millert Exp $	*/
/*	$NetBSD: edit.c,v 1.6 1996/05/15 21:50:45 jtc Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)edit.c	8.3 (Berkeley) 4/2/94";
#else
static char rcsid[] = "$OpenBSD: edit.c,v 1.23 2002/07/31 22:08:42 millert Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "chpass.h"

int
edit(tempname, pw)
	char *tempname;
	struct passwd *pw;
{
	struct stat begin, end;

	for (;;) {
		if (lstat(tempname, &begin) == -1 || S_ISLNK(begin.st_mode))
			return (EDIT_ERROR);
		pw_edit(1, tempname);
		if (lstat(tempname, &end) == -1 || S_ISLNK(end.st_mode))
			return (EDIT_ERROR);
		if (begin.st_mtime == end.st_mtime &&
		    begin.st_size == end.st_size) {
			warnx("no changes made");
			return (EDIT_NOCHANGE);
		}
		if (verify(tempname, pw))
			break;
		pw_prompt();
	}
	return(EDIT_OK);
}

/*
 * display --
 *	print out the file for the user to edit; strange side-effect:
 *	set conditional flag if the user gets to edit the shell.
 */
void
display(tempname, fd, pw)
	char *tempname;
	int fd;
	struct passwd *pw;
{
	FILE *fp;
	char *bp, *p;
	char chngstr[256];

	if (!(fp = fdopen(fd, "w")))
		pw_error(tempname, 1, 1);

	(void)fprintf(fp,
	    "# Changing user database information for %s.\n", pw->pw_name);
	if (!uid) {
		(void)fprintf(fp, "Login: %s\n", pw->pw_name);
		(void)fprintf(fp, "Encrypted password: %s\n", pw->pw_passwd);
		(void)fprintf(fp, "Uid [#]: %u\n", pw->pw_uid);
		(void)fprintf(fp, "Gid [# or name]: %u\n", pw->pw_gid);
		(void)fprintf(fp, "Change [month day year]: %s\n",
		    ttoa(chngstr, sizeof(chngstr), pw->pw_change));
		(void)fprintf(fp, "Expire [month day year]: %s\n",
		    ttoa(chngstr, sizeof(chngstr), pw->pw_expire));
		(void)fprintf(fp, "Class: %s\n", pw->pw_class);
		(void)fprintf(fp, "Home directory: %s\n", pw->pw_dir);
		(void)fprintf(fp, "Shell: %s\n",
		    *pw->pw_shell ? pw->pw_shell : _PATH_BSHELL);
	}
	/* Only admin can change "restricted" shells. */
	else if (ok_shell(pw->pw_shell))
		/*
		 * Make shell a restricted field.  Ugly with a
		 * necklace, but there's not much else to do.
		 */
		(void)fprintf(fp, "Shell: %s\n",
		    *pw->pw_shell ? pw->pw_shell : _PATH_BSHELL);
	else
		list[E_SHELL].restricted = 1;
	bp = pw->pw_gecos;
	p = strsep(&bp, ",");
	(void)fprintf(fp, "Full Name: %s\n", p ? p : "");
	p = strsep(&bp, ",");
	(void)fprintf(fp, "Office Location: %s\n", p ? p : "");
	p = strsep(&bp, ",");
	(void)fprintf(fp, "Office Phone: %s\n", p ? p : "");
	p = strsep(&bp, ",");
	(void)fprintf(fp, "Home Phone: %s\n", p ? p : "");

	(void)fchown(fd, getuid(), getgid());
	(void)fclose(fp);
}

int
verify(tempname, pw)
	char *tempname;
	struct passwd *pw;
{
	unsigned int len, alen, line;
	static char buf[LINE_MAX];
	struct stat sb;
	char *p, *q;
	ENTRY *ep;
	FILE *fp;

	if (!(fp = fopen(tempname, "r")))
		pw_error(tempname, 1, 1);
	if (fstat(fileno(fp), &sb))
		pw_error(tempname, 1, 1);
	if (sb.st_size == 0) {
		warnx("corrupted temporary file");
		goto bad;
	}
	line = 0;
	while (fgets(buf, sizeof(buf), fp)) {
		line++;
		if (!buf[0] || buf[0] == '#')
			continue;
		if (!(p = strchr(buf, '\n'))) {
			warnx("line %u too long", line);
			goto bad;
		}
		*p = '\0';
		for (ep = list;; ++ep) {
			if (!ep->prompt) {
				warnx("unrecognized field on line %u", line);
				goto bad;
			}
			if (!strncasecmp(buf, ep->prompt, ep->len)) {
				if (ep->restricted && uid) {
					warnx(
					    "you may not change the %s field",
						ep->prompt);
					goto bad;
				}
				if (!(p = strchr(buf, ':'))) {
					warnx("line %u corrupted", line);
					goto bad;
				}
				while (isspace(*++p));
				for (q = p; *q && isprint(*q); q++) {
					if (ep->except && strchr(ep->except,*q))
						break;
				}
				if (*q) {
					warnx(
				   "illegal character in the \"%s\" field",
					    ep->prompt);
					goto bad;
				}
				if ((ep->func)(p, pw, ep)) {
bad:					(void)fclose(fp);
					return (0);
				}
				break;
			}
		}
	}
	(void)fclose(fp);

	if (list[E_NAME].save == NULL)
		list[E_NAME].save = "";
	if (list[E_BPHONE].save == NULL)
		list[E_BPHONE].save = "";
	if (list[E_HPHONE].save == NULL)
		list[E_HPHONE].save = "";
	if (list[E_LOCATE].save == NULL)
		list[E_LOCATE].save = "";

	/* Build the gecos field. */
	len = strlen(list[E_NAME].save) + strlen(list[E_BPHONE].save) +
	    strlen(list[E_HPHONE].save) + strlen(list[E_LOCATE].save) + 4;
	for (alen = 0, p = list[E_NAME].save; *p; p++)
		if (*p == '&')
			alen = alen + strlen(pw->pw_name) - 1;
	if (!(p = malloc(len)))
		err(1, NULL);
	(void)snprintf(p, len, "%s,%s,%s,%s", list[E_NAME].save,
	    list[E_LOCATE].save, list[E_BPHONE].save, list[E_HPHONE].save);
	pw->pw_gecos = p;

	if (snprintf(buf, sizeof(buf),
	    "%s:%s:%u:%u:%s:%ld:%ld:%s:%s:%s",
	    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid, pw->pw_class,
	    (long)pw->pw_change, (long)pw->pw_expire, pw->pw_gecos, pw->pw_dir,
	    pw->pw_shell) >= 1023 ||
	    strlen(buf) + alen >= 1023) {
		warnx("entries too long");
		free(p);
		return (0);
	}
	free(p);

	return (pw_scan(buf, pw, NULL));
}
