/*	$OpenBSD: file.c,v 1.17 2021/12/15 19:22:44 tb Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <zlib.h>

#include "grep.h"

static char	 fname[PATH_MAX];
static char	*lnbuf;
static size_t	 lnbufsize;

#define FILE_STDIO	0
#define FILE_MMAP	1
#define FILE_GZIP	2

struct file {
	int	 type;
	int	 noseek;
	FILE	*f;
	mmf_t	*mmf;
	gzFile	 gzf;
};

#ifndef NOZ
static char *
gzfgetln(gzFile f, size_t *len)
{
	size_t		n;
	int		c;

	for (n = 0; ; ++n) {
		c = gzgetc(f);
		if (c == -1) {
			const char *gzerrstr;
			int gzerr;

			if (gzeof(f))
				break;

			gzerrstr = gzerror(f, &gzerr);
			if (gzerr == Z_ERRNO)
				err(2, "%s", fname);
			else
				errx(2, "%s: %s", fname, gzerrstr);
		}
		if (n >= lnbufsize) {
			lnbufsize *= 2;
			lnbuf = grep_realloc(lnbuf, ++lnbufsize);
		}
		if (c == '\n')
			break;
		lnbuf[n] = c;
	}

	if (gzeof(f) && n == 0)
		return NULL;
	*len = n;
	return lnbuf;
}
#endif

file_t *
grep_fdopen(int fd)
{
	file_t *f;
	struct stat sb;

	if (fd == STDIN_FILENO)
		snprintf(fname, sizeof fname, "(standard input)");
	else if (fname[0] == '\0')
		snprintf(fname, sizeof fname, "(fd %d)", fd);

	if (fstat(fd, &sb) == -1)
		return NULL;
	if (S_ISDIR(sb.st_mode)) {
		errno = EISDIR;
		return NULL;
	}

	f = grep_malloc(sizeof *f);

#ifndef NOZ
	if (Zflag) {
		f->type = FILE_GZIP;
		f->noseek = lseek(fd, 0L, SEEK_SET) == -1;
		if ((f->gzf = gzdopen(fd, "r")) != NULL)
			return f;
	}
#endif
	f->noseek = isatty(fd);
#ifndef SMALL
	/* try mmap first; if it fails, try stdio */
	if (!f->noseek && (f->mmf = mmopen(fd, &sb)) != NULL) {
		f->type = FILE_MMAP;
		return f;
	}
#endif
	f->type = FILE_STDIO;
	if ((f->f = fdopen(fd, "r")) != NULL)
		return f;

	free(f);
	return NULL;
}

file_t *
grep_open(char *path)
{
	file_t *f;
	int fd;

	snprintf(fname, sizeof fname, "%s", path);

	if ((fd = open(fname, O_RDONLY)) == -1)
		return NULL;

	f = grep_fdopen(fd);
	if (f == NULL)
		close(fd);
	return f;
}

int
grep_bin_file(file_t *f)
{
	if (f->noseek)
		return 0;

	switch (f->type) {
	case FILE_STDIO:
		return bin_file(f->f);
#ifndef SMALL
	case FILE_MMAP:
		return mmbin_file(f->mmf);
#endif
#ifndef NOZ
	case FILE_GZIP:
		return gzbin_file(f->gzf);
#endif
	default:
		/* can't happen */
		errx(2, "invalid file type");
	}
}

char *
grep_fgetln(file_t *f, size_t *l)
{
	switch (f->type) {
	case FILE_STDIO:
		if ((*l = getline(&lnbuf, &lnbufsize, f->f)) == -1) {
			if (ferror(f->f))
				err(2, "%s: getline", fname);
			else
				return NULL;
		}
		return lnbuf;
#ifndef SMALL
	case FILE_MMAP:
		return mmfgetln(f->mmf, l);
#endif
#ifndef NOZ
	case FILE_GZIP:
		return gzfgetln(f->gzf, l);
#endif
	default:
		/* can't happen */
		errx(2, "invalid file type");
	}
}

void
grep_close(file_t *f)
{
	switch (f->type) {
	case FILE_STDIO:
		fclose(f->f);
		break;
#ifndef SMALL
	case FILE_MMAP:
		mmclose(f->mmf);
		break;
#endif
#ifndef NOZ
	case FILE_GZIP:
		gzclose(f->gzf);
		break;
#endif
	default:
		/* can't happen */
		errx(2, "invalid file type");
	}
	free(f);
}
