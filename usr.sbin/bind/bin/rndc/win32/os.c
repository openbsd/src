/*
 * Copyright (C) 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: os.c,v 1.2 2001/08/08 23:26:58 gson Exp $ */

#include <config.h>

#include <rndc/os.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

int
set_user(FILE *fd, const char *user) {
	return (0);
}

/*
 * Note that the error code EOPNOTSUPP does not exist
 * on win32 so we are forced to fall back to using
 * ENOENT for now. WSAEOPNOTSUPP does exist but it
 * should only be used for sockets.
 */

FILE *
safe_create(const char *filename) {
	int fd;
	FILE *f;
        struct stat sb;

        if (stat(filename, &sb) == -1) {
                if (errno != ENOENT)
			return (NULL);
        } else if ((sb.st_mode & S_IFREG) == 0) {
		errno = ENOENT;
		return (NULL);
	}

	fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd == -1)
		return (NULL);
	f = fdopen(fd, "w");
	if (f == NULL)
		close(fd);
	return (f);
}
