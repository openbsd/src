/*	$OpenBSD: client.c,v 1.1 2015/10/31 12:19:41 millert Exp $	*/

/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "cron.h"

/* int in_file(const char *string, FILE *file, int error)
 *	return TRUE if one of the lines in file matches string exactly,
 *	FALSE if no lines match, and error on error.
 */
static int
in_file(const char *string, FILE *file, int error)
{
	char line[MAX_TEMPSTR];
	char *endp;

	if (fseek(file, 0L, SEEK_SET))
		return (error);
	while (fgets(line, MAX_TEMPSTR, file)) {
		if (line[0] != '\0') {
			endp = &line[strlen(line) - 1];
			if (*endp != '\n')
				return (error);
			*endp = '\0';
			if (0 == strcmp(line, string))
				return (TRUE);
		}
	}
	if (ferror(file))
		return (error);
	return (FALSE);
}

/* int allowed(const char *username, const char *allow_file, const char *deny_file)
 *	returns TRUE if (allow_file exists and user is listed)
 *	or (deny_file exists and user is NOT listed).
 *	root is always allowed.
 */
int
allowed(const char *username, const char *allow_file, const char *deny_file)
{
	FILE	*fp;
	int	isallowed;

	if (strcmp(username, "root") == 0)
		return (TRUE);
	isallowed = FALSE;
	if ((fp = fopen(allow_file, "r")) != NULL) {
		isallowed = in_file(username, fp, FALSE);
		fclose(fp);
	} else if ((fp = fopen(deny_file, "r")) != NULL) {
		isallowed = !in_file(username, fp, FALSE);
		fclose(fp);
	}
	return (isallowed);
}

/* void poke_daemon(const char *spool_dir, unsigned char cookie)
 *	touches spool_dir and sends a poke to the cron daemon if running.
 */
void
poke_daemon(const char *spool_dir, unsigned char cookie)
{
	int sock = -1;
	struct sockaddr_un s_un;

	(void) utime(spool_dir, NULL);		/* old poke method */

	bzero(&s_un, sizeof(s_un));
	if (snprintf(s_un.sun_path, sizeof s_un.sun_path, "%s/%s",
	      SPOOL_DIR, CRONSOCK) >= sizeof(s_un.sun_path)) {
		fprintf(stderr, "%s: %s/%s: path too long\n",
		    ProgramName, SPOOL_DIR, CRONSOCK);
		return;
	}
	s_un.sun_family = AF_UNIX;
	(void) signal(SIGPIPE, SIG_IGN);
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0 &&
	    connect(sock, (struct sockaddr *)&s_un, sizeof(s_un)) == 0)
		write(sock, &cookie, 1);
	else
		fprintf(stderr, "%s: warning, cron does not appear to be "
		    "running.\n", ProgramName);
	if (sock >= 0)
		close(sock);
	(void) signal(SIGPIPE, SIG_DFL);
}
