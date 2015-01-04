/*	$OpenBSD: encrypt.c,v 1.36 2015/01/04 02:28:26 deraadt Exp $	*/

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
int	ideal_rounds(void);
void	print_passwd(char *, int, void *);

#define DO_BLF		0

void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: %s [-b rounds] [-c class] [-p | string]\n",
	    __progname);
	exit(1);
}

/*
 * Time how long 8 rounds takes to measure this system's performance.
 * We are aiming for something that takes between 0.25 and 0.5 seconds.
 */
int
ideal_rounds(void)
{
	clock_t before, after;
	int r = 8;
	char buf[_PASSWORD_LEN];
	int duration;

	strlcpy(buf, bcrypt_gensalt(r), _PASSWORD_LEN);
	before = clock();
	crypt("testpassword", buf);
	after = clock();

	duration = after - before;

	/* too quick? slow it down. */
	while (r < 16 && duration <= CLOCKS_PER_SEC / 4) {
		r += 1;
		duration *= 2;
	}
	/* too slow? speed it up. */
	while (r > 4 && duration > CLOCKS_PER_SEC / 2) {
		r -= 1;
		duration /= 2;
	}

	return r;
}


void
print_passwd(char *string, int operation, void *extra)
{
	char buffer[_PASSWORD_LEN];

	if (operation == DO_BLF) {
		int rounds = *(int *)extra;
		if (bcrypt_newhash(string, rounds, buffer, sizeof(buffer)) != 0)
			errx(1, "bcrypt newhash failed");
		fputs(buffer, stdout);
		return;
	} else {
		login_cap_t *lc;
		const char *pref;

		if ((lc = login_getclass(extra)) == NULL)
			errx(1, "unable to get login class `%s'",
			    extra ? (char *)extra : "default");
		pref = login_getcapstr(lc, "localcipher", NULL, NULL);
		if (crypt_newhash(string, pref, buffer, sizeof(buffer)) != 0)
			errx(1, "can't generate hash");
	}

	fputs(buffer, stdout);
}

int
main(int argc, char **argv)
{
	int opt;
	int operation = -1;
	int prompt = 0;
	int rounds;
	void *extra = NULL;		/* Store salt or number of rounds */
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
			if (strcmp(optarg, "a") == 0)
				rounds = ideal_rounds();
			else {
				rounds = strtonum(optarg, 1, INT_MAX, &errstr);
				if (errstr != NULL)
					errx(1, "%s: %s", errstr, optarg);
			}
			extra = &rounds;
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
		memset(argv[optind], 0, strlen(argv[optind]));

		print_passwd(string, operation, extra);

		(void)fputc('\n', stdout);

		/* Wipe our copy, before we free it. */
		memset(string, 0, strlen(string));
		free(string);
	}
	exit(0);
}
