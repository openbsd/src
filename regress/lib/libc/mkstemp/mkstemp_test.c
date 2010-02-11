/*
 * Copyright (c) 2010  Philip Guenther <guenther@openbsd.org>
 *
 * Public domain.
 *
 * Verify that mkstemp() doesn't overrun or underrun the template buffer
 * and that it can generate names that don't contain any X's
 */

#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_TEMPLATE_LEN	10
#define MAX_TRIES		100

long pg;

void
try(char *p, const char *prefix, int len)
{
	struct stat sb, fsb;
	char *q;
	size_t plen = strlen(prefix);
	int tries, fd;

	for (tries = 0; tries < MAX_TRIES; tries++) {
		memcpy(p, prefix, plen);
		memset(p + plen, 'X', len);
		p[plen + len] = '\0';
		fd = mkstemp(p);
		if (fd < 0)
			err(1, "mkstemp");
		if (stat(p, &sb))
			err(1, "stat(%s)", p);
		if (fstat(fd, &fsb))
			err(1, "fstat(%d==%s)", fd, p);
		if (sb.st_dev != fsb.st_dev || sb.st_ino != fsb.st_ino)
			errx(1, "stat mismatch");
		close(fd);
		for (q = p + plen; *q != 'X'; q++) {
			if (*q == '\0') {
				if (q != p + plen + len)
					errx(1, "unexpected truncation");
				return;
			}
		}
		if (q >= p + plen + len)
			errx(1, "overrun?");
	}
	errx(1, "exceeded MAX_TRIES");
}

int
main(void)
{
	struct stat sb, fsb;
	char cwd[MAXPATHLEN + 1];
	char *p;
	size_t clen;
	int i;

	pg = sysconf(_SC_PAGESIZE);
	if (getcwd(cwd, sizeof cwd - 1) == NULL)
		err(1, "getcwd");
	clen = strlen(cwd);
	cwd[clen++] = '/';
	cwd[clen] = '\0';
	p = mmap(NULL, pg * 3, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	if (p == NULL)
		err(1, "mmap");
	if (mprotect(p, pg, PROT_NONE) || mprotect(p + pg * 2, pg, PROT_NONE))
		err(1, "mprotect");
	p += pg;

	i = MAX_TEMPLATE_LEN + 1;
	while (i-- > 1) {
		/* try first at the start of a page, no prefix */
		try(p, "", i);
		/* now at the end of the page, no prefix */
		try(p + pg - i - 1, "", i);
		/* start of the page, prefixed with the cwd */
		try(p, cwd, i);
		/* how about at the end of the page, prefixed with cwd? */
		try(p + pg - i - 1 - clen, cwd, i);
	}

	return 0;
}
