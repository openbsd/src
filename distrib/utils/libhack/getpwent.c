/*	$OpenBSD: getpwent.c,v 1.2 2000/03/01 22:10:12 todd Exp $	*/
/*	$NetBSD: getpwent.c,v 1.2 1995/10/13 18:10:27 gwr Exp $	*/

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

/*
 * Smaller replacement for: libc/gen/getpwent.c
 * Needed by programs like: rsh, rlogin
 */

#include <stdio.h>
#include <string.h>
#include <pwd.h>

#define	PWNULL	(struct passwd *)0
#define MAXFIELD 8

static char *pw_file = "/etc/passwd";
static FILE *pw_fp;
static char  pw_line[128];
static struct passwd pw_ent;

/*
 * Open passwd file if necessary, and
 * get the next entry.
 */
struct passwd *
getpwent()
{
	char *fv[MAXFIELD];
	char *p;
	int   fc;

	/* Open passwd file if not already. */
	if (pw_fp == NULL)
		pw_fp = fopen(pw_file, "r");
	/* Still NULL.  No passwd file? */
	if (pw_fp == NULL)
		return PWNULL;

readnext:
	/* Read the next line... */
	if (fgets(pw_line, sizeof(pw_line), pw_fp) == NULL)
		return PWNULL;

	/* ...and parse it. */
	p = pw_line;
	fc = 0;
	while (fc < MAXFIELD) {
		fv[fc] = strsep(&p, ":\n");
		if (fv[fc] == NULL)
			break;
		fc++;
	}

	/* Need at least 0..5 */
	if (fc < 6)
		goto readnext;
	while (fc < MAXFIELD)
		fv[fc++] = "";

	/* Build the pw entry... */
	pw_ent.pw_name   = fv[0];
	pw_ent.pw_passwd = fv[1];
	pw_ent.pw_uid = atoi(fv[2]);
	pw_ent.pw_gid = atoi(fv[3]);
	pw_ent.pw_gecos = fv[4];
	pw_ent.pw_dir   = fv[5];
	pw_ent.pw_shell = fv[6];

	return (&pw_ent);
}

/* internal for setpwent() */
int
setpassent(stayopen)
	int stayopen;
{
	if (pw_fp)
		rewind(pw_fp);
}

/* rewind to the beginning. */
void
setpwent()
{
	(void) setpassent(0);
}

/* done with the passwd file */
void
endpwent()
{
	if (pw_fp) {
		fclose(pw_fp);
		pw_fp = NULL;
	}
}

struct passwd *
getpwnam(name)
	const char *name;
{
	struct passwd *pw;

	setpwent();
	while ((pw = getpwent()) != PWNULL)
		if (!strcmp(pw->pw_name, name))
			break;

	endpwent();
	return(pw);
}

struct passwd *
getpwuid(uid)
	uid_t uid;
{
	struct passwd *pw;

	setpwent();
	while ((pw = getpwent()) != PWNULL)
		if (pw->pw_uid == uid)
			break;

	endpwent();
	return(pw);
}

#ifdef	TEST_MAIN
main() {
	struct passwd *pw;

	printf("#name, password, uid, gid, comment, dir, shell\n");

	while ((pw = getpwent()) != NULL) {
		printf("%s:%s:", pw->pw_name, pw->pw_passwd);
		printf("%d:%d:", pw->pw_uid, pw->pw_gid);
		printf("%s:", pw->pw_gecos);
		printf("%s:", pw->pw_dir);
		printf("%s\n", pw->pw_shell);
	}
}
#endif
