/*	$OpenBSD: htpasswd.c,v 1.17 2018/10/31 07:39:10 mestre Exp $ */
/*
 * Copyright (c) 2014 Florian Obser <florian@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__dead void	usage(void);
void		nag(char*);

extern char *__progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage:\t%s [file] login\n", __progname);
	fprintf(stderr, "\t%s -I [file]\n", __progname);
	exit(1);
}

#define MAXNAG 5
int nagcount;

int
main(int argc, char** argv)
{
	char tmpl[sizeof("/tmp/htpasswd-XXXXXXXXXX")];
	char hash[_PASSWORD_LEN], pass[1024], pass2[1024];
	char *line = NULL, *login = NULL, *tok;
	int c, fd, loginlen, batch = 0;
	FILE *in = NULL, *out = NULL;
	const char *file = NULL;
	size_t linesize = 0;
	ssize_t linelen;
	mode_t old_umask;

	while ((c = getopt(argc, argv, "I")) != -1) {
		switch (c) {
		case 'I':
			batch = 1;
			break;
		default:
			usage();
			/* NOT REACHED */
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if ((batch && argc == 1) || (!batch && argc == 2)) {
		if (unveil(argv[0], "rwc") == -1)
			err(1, "unveil");
		if (unveil("/tmp", "rwc") == -1)
			err(1, "unveil");
	}
	if (pledge("stdio rpath wpath cpath flock tmppath tty", NULL) == -1)
		err(1, "pledge");

	if (batch) {
		if (argc == 1)
			file = argv[0];
		else if (argc > 1)
			usage();
		else if (pledge("stdio", NULL) == -1)
			err(1, "pledge");

		if ((linelen = getline(&line, &linesize, stdin)) == -1)
			err(1, "cannot read login:password from stdin");
		line[linelen-1] = '\0';

		if ((tok = strstr(line, ":")) == NULL)
			errx(1, "cannot find ':' in input");
		*tok++ = '\0';

		if ((loginlen = asprintf(&login, "%s:", line)) == -1)
			err(1, "asprintf");

		if (strlcpy(pass, tok, sizeof(pass)) >= sizeof(pass))
			errx(1, "password too long");
	} else {

		switch (argc) {
		case 1:
			if (pledge("stdio tty", NULL) == -1)
				err(1, "pledge");
			if ((loginlen = asprintf(&login, "%s:", argv[0])) == -1)
				err(1, "asprintf");
			break;
		case 2:
			file = argv[0];
			if ((loginlen = asprintf(&login, "%s:", argv[1])) == -1)
				err(1, "asprintf");
			break;
		default:
			usage();
			/* NOT REACHED */
			break;
		}

		if (!readpassphrase("Password: ", pass, sizeof(pass),
		    RPP_ECHO_OFF))
			err(1, "unable to read password");
		if (!readpassphrase("Retype Password: ", pass2, sizeof(pass2),
		    RPP_ECHO_OFF)) {
			explicit_bzero(pass, sizeof(pass));
			err(1, "unable to read password");
		}
		if (strcmp(pass, pass2) != 0) {
			explicit_bzero(pass, sizeof(pass));
			explicit_bzero(pass2, sizeof(pass2));
			errx(1, "passwords don't match");
		}

		explicit_bzero(pass2, sizeof(pass2));
	}

	if (crypt_newhash(pass, "bcrypt,a", hash, sizeof(hash)) != 0)
		err(1, "can't generate hash");
	explicit_bzero(pass, sizeof(pass));

	if (file == NULL)
		printf("%s%s\n", login, hash);
	else {
		if ((in = fopen(file, "r+")) == NULL) {
			if (errno == ENOENT) {
				old_umask = umask(S_IXUSR|
				    S_IWGRP|S_IRGRP|S_IXGRP|
				    S_IWOTH|S_IROTH|S_IXOTH);
				if ((out = fopen(file, "w")) == NULL)
					err(1, "cannot open password file for"
					    " reading or writing");
				umask(old_umask);
			} else
				err(1, "cannot open password file for"
					" reading or writing");
		} else
			if (flock(fileno(in), LOCK_EX|LOCK_NB) == -1)
				errx(1, "cannot lock password file");

		/* file already exits, copy content and filter login out */
		if (out == NULL) {
			strlcpy(tmpl, "/tmp/htpasswd-XXXXXXXXXX", sizeof(tmpl));
			if ((fd = mkstemp(tmpl)) == -1)
				err(1, "mkstemp");

			if ((out = fdopen(fd, "w+")) == NULL)
				err(1, "cannot open tempfile");

			while ((linelen = getline(&line, &linesize, in))
			    != -1) {
				if (strncmp(line, login, loginlen) != 0) {
					if (fprintf(out, "%s", line) == -1)
						errx(1, "cannot write to temp "
						    "file");
					nag(line);
				}
			}
		}
		if (fprintf(out, "%s%s\n", login, hash) == -1)
			errx(1, "cannot write new password hash");

		/* file already exists, overwrite it */
		if (in != NULL) {
			if (fseek(in, 0, SEEK_SET) == -1)
				err(1, "cannot seek in password file");
			if (fseek(out, 0, SEEK_SET) == -1)
				err(1, "cannot seek in temp file");
			if (ftruncate(fileno(in), 0) == -1)
				err(1, "cannot truncate password file");
			while ((linelen = getline(&line, &linesize, out))
			    != -1)
				if (fprintf(in, "%s", line) == -1)
					errx(1, "cannot write to password "
					    "file");
			if (fclose(in) == EOF)
				err(1, "cannot close password file");
		}
		if (fclose(out) == EOF) {
			if (in != NULL)
				err(1, "cannot close temp file");
			else
				err(1, "cannot close password file");
		}
		if (in != NULL && unlink(tmpl) == -1)
			err(1, "cannot delete temp file (%s)", tmpl);
	}
	if (nagcount >= MAXNAG)
		warnx("%d more logins not using bcryt.", nagcount - MAXNAG);
	exit(0);
}

void
nag(char* line)
{
	const char *tok;
	if (strtok(line, ":") != NULL)
		if ((tok = strtok(NULL, ":")) != NULL)
			if (strncmp(tok, "$2a$", 4) != 0 &&
			     strncmp(tok, "$2b$", 4) != 0) {
				nagcount++;
				if (nagcount <= MAXNAG)
					warnx("%s doesn't use bcrypt."
					    " Update the password.", line);
			}
}
