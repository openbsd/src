/*	$OpenBSD: sha256.c,v 1.1 2009/04/17 03:48:35 deraadt Exp $	*/

/*
 * Copyright (c) 2009 Theo de Raadt <deraadt@openbsd.org>
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
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sha2.h>

int
main(int argc, char *argv[])
{
	u_int8_t results[SHA256_DIGEST_LENGTH];
	size_t nread, nwrite, i;
	char buf[BUFSIZ];
	SHA2_CTX ctx;
	FILE *fp;

	if (argv[1] == NULL) {
		fprintf(stderr, "usage: sha256 outfile\n");
		exit(1);
	}

	fp = fopen(argv[1], "w");

	SHA256Init(&ctx);
	while ((nread = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
		SHA256Update(&ctx, (u_int8_t *)buf, nread);
		for (i = 0; i < nread ; ) {
			nwrite = write(STDOUT_FILENO, buf + i, nread - i);
			if (nwrite == -1)
				exit(1);
			i += nwrite;
		}
	}
	SHA256End(&ctx, results);
	fprintf(fp, "%s\n", results);
	fclose(fp);
	exit(0);
}
