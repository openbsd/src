/*	$OpenBSD: mktemp.c,v 1.11 2019/06/27 18:03:37 deraadt Exp $ */
/*
 * Copyright (c) 1996-1998, 2008 Theo de Raadt
 * Copyright (c) 1997, 2008-2009 Todd C. Miller
 * Copyright (c) 2019 Florian Obser <florian@openbsd.org>
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "extern.h"

/*
 * The type of temporary files we can create.
 */
enum	tmpmode {
	MKTEMP_NAME,
	MKTEMP_FILE,
	MKTEMP_DIR,
	MKTEMP_LINK,
	MKTEMP_FIFO,
	MKTEMP_NOD,
	MKTEMP_SOCK
};

/*
 * Characters we'll use for replacement in the template string.
 */
#define TEMPCHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define NUM_CHARS	(sizeof(TEMPCHARS) - 1)

/*
 * The number of template replacement values (foo.XXXXXX = 6) that we
 * require as a minimum for the filename.
 */
#define MIN_X		6

/*
 * The only flags we'll accept for creation of the temporary file.
 */
#define MKOTEMP_FLAGS	(O_APPEND | O_CLOEXEC | O_DSYNC | O_RSYNC | O_SYNC)

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

/*
 * Adapted from libc/stdio/mktemp.c.
 */
static int
mktemp_internalat(int pfd, char *path, int slen, enum tmpmode mode,
	int flags, const char *link, mode_t dev_type, dev_t dev)
{
	char		*start, *cp, *ep;
	const char	 tempchars[] = TEMPCHARS;
	unsigned int	 tries;
	struct stat	 sb;
	struct sockaddr_un sun;
	size_t		 len;
	int		 fd, saved_errno;

	len = strlen(path);
	if (len < MIN_X || slen < 0 || (size_t)slen > len - MIN_X) {
		errno = EINVAL;
		return(-1);
	}
	ep = path + len - slen;

	for (start = ep; start > path && start[-1] == 'X'; start--)
		/* continue */ ;

	if (ep - start < MIN_X) {
		errno = EINVAL;
		return(-1);
	}

	if (flags & ~MKOTEMP_FLAGS) {
		errno = EINVAL;
		return(-1);
	}
	flags |= O_CREAT | O_EXCL | O_RDWR;

	tries = INT_MAX;
	do {
		cp = start;
		do {
			unsigned short rbuf[16];
			unsigned int i;

			/*
			 * Avoid lots of arc4random() calls by using
			 * a buffer sized for up to 16 Xs at a time.
			 */
			arc4random_buf(rbuf, sizeof(rbuf));
			for (i = 0; i < nitems(rbuf) && cp != ep; i++)
				*cp++ = tempchars[rbuf[i] % NUM_CHARS];
		} while (cp != ep);

		switch (mode) {
		case MKTEMP_NAME:
			if (fstatat(pfd, path, &sb, AT_SYMLINK_NOFOLLOW) != 0)
				return(errno == ENOENT ? 0 : -1);
			break;
		case MKTEMP_FILE:
			fd = openat(pfd, path, flags, S_IRUSR|S_IWUSR);
			if (fd != -1 || errno != EEXIST)
				return(fd);
			break;
		case MKTEMP_DIR:
			if (mkdirat(pfd, path, S_IRUSR|S_IWUSR|S_IXUSR) == 0)
				return(0);
			if (errno != EEXIST)
				return(-1);
			break;
		case MKTEMP_LINK:
			if (symlinkat(link, pfd, path) == 0)
				return(0);
			else if (errno != EEXIST)
				return(-1);
			break;
		case MKTEMP_FIFO:
			if (mkfifoat(pfd, path, S_IRUSR|S_IWUSR) == 0)
				return(0);
			else if (errno != EEXIST)
				return(-1);
			break;
		case MKTEMP_NOD:
			if (!(dev_type == S_IFCHR || dev_type == S_IFBLK)) {
				errno = EINVAL;
				return(-1);
			}
			if (mknodat(pfd, path, S_IRUSR|S_IWUSR|dev_type, dev)
			    == 0)
				return(0);
			else if (errno != EEXIST)
				return(-1);
			break;
		case MKTEMP_SOCK:
			memset(&sun, 0, sizeof(sun));
			sun.sun_family = AF_UNIX;
			if ((len = strlcpy(sun.sun_path, link,
			    sizeof(sun.sun_path))) >= sizeof(sun.sun_path)) {
				errno = EINVAL;
				return(-1);
			}
			if (sun.sun_path[len] != '/') {
				if (strlcat(sun.sun_path, "/",
				    sizeof(sun.sun_path)) >=
				    sizeof(sun.sun_path)) {
					errno = EINVAL;
					return(-1);
				}
			}
			if (strlcat(sun.sun_path, path, sizeof(sun.sun_path)) >=
			    sizeof(sun.sun_path)) {
				errno = EINVAL;
				return(-1);
			}
			if ((fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC |
			    SOCK_NONBLOCK, 0)) == -1)
				return -1;
			if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) ==
			    0) {
				close(fd);
				return(0);
			} else if (errno != EEXIST) {
					saved_errno = errno;
					close(fd);
					errno = saved_errno;
					return -1;
			}
			close(fd);
			break;
		}
	} while (--tries);

	errno = EEXIST;
	return(-1);
}

/*
 * A combination of mkstemp(3) and openat(2).
 * On success returns a file descriptor and trailing Xs are overwritten in
 * path to create a unique file name.
 * Returns -1 on failure and sets errno.
 */
int
mkstempat(int fd, char *path)
{
	return mktemp_internalat(fd, path, 0, MKTEMP_FILE, 0, NULL, 0, 0);
}

/*
 * A combination of mkstemp(3) and symlinkat(2).
 * On success returns path with trailing Xs overwritten to create a unique
 * file name.
 * Returns NULL on failure and sets errno.
 */
char *
mkstemplinkat(char *link, int fd, char *path)
{

	if (mktemp_internalat(fd, path, 0, MKTEMP_LINK, 0, link, 0, 0) == -1)
		return NULL;
	return path;
}

/*
 * A combination of mkstemp(3) and mkfifoat(2).
 * On success returns path with trailing Xs overwritten to create a unique
 * file name.
 * Returns NULL on failure and sets errno.
 */
char *
mkstempfifoat(int fd, char *path)
{

	if (mktemp_internalat(fd, path, 0, MKTEMP_FIFO, 0, NULL, 0, 0) == -1)
		return NULL;
	return path;
}

/*
 * A combination of mkstemp(3) and mknodat(2).
 * On success returns path with trailing Xs overwritten to create a unique
 * file name.
 * Returns NULL on failure and sets errno.
 */
char *
mkstempnodat(int fd, char *path, mode_t mode, dev_t dev)
{

	if (mktemp_internalat(fd, path, 0,
	    MKTEMP_NOD, 0, NULL, mode, dev) == -1)
		return NULL;
	return path;
}

/*
 * A combination of mkstemp(3) and bind(2) on a unix domain socket.
 * On success returns path with trailing Xs overwritten to create a unique
 * file name.
 * Returns NULL on failure and sets errno.
 */
char *
mkstempsock(const char *root, char *path)
{

	if (mktemp_internalat(0, path, 0, MKTEMP_SOCK, 0, root, 0, 0) == -1)
		return NULL;
	return path;
}

/*
 * Turn path into a suitable template for mkstemp*at functions and
 * place it into the newly allocated string returned in ret.
 * The caller must free ret.
 * Returns -1 on failure or number of characters output to ret
 * (excluding the final '\0').
 */
int
mktemplate(char **ret, const char *path, int recursive)
{
	int		 n, dirlen;
	const char	*cp;

	if (recursive && (cp = strrchr(path, '/')) != NULL) {
		dirlen = cp - path;
		n = asprintf(ret, "%.*s/.%s.XXXXXXXXXX",
			dirlen, path, path + dirlen + 1);
		if (n == -1) {
			ERR("asprintf");
			*ret = NULL;
		}
	} else if ((n = asprintf(ret, ".%s.XXXXXXXXXX", path)) == -1) {
		ERR("asprintf");
		*ret = NULL;
	}

	return n;
}
