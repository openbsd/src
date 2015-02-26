/*	$OpenBSD: encrypt.c,v 1.40 2015/02/26 17:46:15 tedu Exp $	*/

/*
 * Copyright (c) 1996, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <login_cap.h>
#include <limits.h>

/*
 * Very simple little program, for encrypting passwords from the command
 * line.  Useful for scripts and such.
 */

extern char *__progname;

void	usage(void);

#define DO_BLF		0

void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: %s [-b rounds] [-c class] [-p | string]\n",
	    __progname);
	exit(1);
}

static void
print_passwd(char *string, int operation, char *extra)
{
	char buffer[_PASSWORD_LEN];
	const char *pref;
	char prefbuf[64];

	if (operation == DO_BLF) {
		if (snprintf(prefbuf, sizeof(prefbuf), "blowfish,%s", extra) >=
		    sizeof(prefbuf))
			errx(1, "pref too long");
		pref = prefbuf;
	} else {
		login_cap_t *lc;

		if ((lc = login_getclass(extra)) == NULL)
			errx(1, "unable to get login class `%s'",
			    extra ? (char *)extra : "default");
		pref = login_getcapstr(lc, "localcipher", NULL, NULL);
	}
	if (crypt_newhash(string, pref, buffer, sizeof(buffer)) != 0)
		err(1, "can't generate hash");

	fputs(buffer, stdout);
}

int
main(int argc, char **argv)
{
	int opt;
	int operation = -1;
	int prompt = 0;
	char *extra = NULL;	/* Store login class or number of rounds */
	const char *errstr;

	while ((opt = getopt(argc, argv, "pb:c:")) != -1) {
		switch (opt) {
		case 'p':
			prompt = 1;
			break;
		case 'b':                       /* Blowfish password hash */
			if (operation != -1)
				usage();
			operation = DO_BLF;
			if (strcmp(optarg, "a") != 0) {
				(void)strtonum(optarg, 4, 31, &errstr);
				if (errstr != NULL)
					errx(1, "rounds is %s: %s", errstr,
					    optarg);
			}
			extra = optarg;
			break;
		case 'c':                       /* user login class */
			extra = optarg;
			operation = -1;
			break;
		default:
			usage();
		}
	}

	if (((argc - optind) < 1)) {
		char line[BUFSIZ], *string;

		if (prompt) {
			if ((string = getpass("Enter string: ")) == NULL)
				err(1, "getpass");
			print_passwd(string, operation, extra);
			(void)fputc('\n', stdout);
		} else {
			size_t len;
			/* Encrypt stdin to stdout. */
			while (!feof(stdin) &&
			    (fgets(line, sizeof(line), stdin) != NULL)) {
			    	len = strlen(line);
				if (len == 0 || line[0] == '\n')
					continue;
				if (line[len - 1] == '\n')
                     			line[len - 1] = '\0';

				print_passwd(line, operation, extra);

				(void)fputc('\n', stdout);
			}
		}
	} else {
		char *string;

		/* can't combine -p with a supplied string */
		if (prompt)
			usage();

		/* Perhaps it isn't worth worrying about, but... */
		if ((string = strdup(argv[optind])) == NULL)
			err(1, NULL);
		/* Wipe the argument. */
		explicit_bzero(argv[optind], strlen(argv[optind]));

		print_passwd(string, operation, extra);

		(void)fputc('\n', stdout);

		/* Wipe our copy, before we free it. */
		explicit_bzero(string, strlen(string));
		free(string);
	}
	exit(0);
}
