/*	$OpenBSD: skeyinfo.c,v 1.7 2001/06/17 22:44:51 millert Exp $	*/

/*
 * Copyright (c) 1997, 2001 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <skey.h>
#include <login_cap.h>
#include <bsd_auth.h>

extern char *__progname;

void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct passwd *pw;
	char *challenge, *cp, *name = NULL;
	int ch, verbose = 0;
	auth_session_t *as;

	while ((ch = getopt(argc, argv, "v")) != -1)
		switch(ch) {
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (argc == 1)
		name = argv[0];
	else if (argc > 1)
		usage();

	if (name && getuid() != 0)
		errx(1, "only root may specify an alternate user");

	if (name) {
		if ((pw = getpwnam(name)) == NULL)
			errx(1, "no passwd entry for %s", name);
	} else {
		if ((pw = getpwuid(getuid())) == NULL)
			errx(1, "no passwd entry for uid %u", getuid());
	}

	if ((name = strdup(pw->pw_name)) == NULL)
		err(1, "cannot allocate memory");

	as = auth_userchallenge(name, "skey", NULL, &challenge);
	if (as == NULL || challenge == NULL) {
		auth_close(as);
		errx(1, "unable to retrieve S/Key challenge for %s", name);
	}

	/*
	 * We only want the first line of the challenge so stop after a newline.
	 * If the user wants the full challenge including the hash type
	 * or if the challenge didn't start with 'otp-', print it verbatim.
	 * Otherwise, strip off the first word.
	 */
	if ((cp = strchr(challenge, '\n')))
		*cp = '\0';
	cp = strchr(challenge, ' ');
	if (verbose || *challenge != 'o' || !cp)
		cp = challenge;
	else
		cp++;
	puts(cp);

	auth_close(as);
	exit(0);
}

void
usage()
{
	(void)fprintf(stderr, "Usage: %s [-v] [user]\n", __progname);
	exit(1);
}
