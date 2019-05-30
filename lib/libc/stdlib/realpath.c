/*	$OpenBSD: realpath.c,v 1.25 2019/05/30 13:22:48 deraadt Exp $ */
/*
 * Copyright (c) 2003 Constantin S. Svintsoff <kostik@iclub.nsu.ru>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <stdarg.h>

/* A slightly modified copy of this file exists in libexec/ld.so */

/*
 * char *realpath(const char *path, char resolved[PATH_MAX]);
 *
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 */
static char *
urealpath(const char *path, char *resolved)
{
	const char *p;
	char *q;
	size_t left_len, resolved_len, next_token_len;
	unsigned symlinks;
	int serrno, mem_allocated;
	ssize_t slen;
	int trailingslash = 0;
	char left[PATH_MAX], next_token[PATH_MAX], symlink[PATH_MAX];

	if (path == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	if (path[0] == '\0') {
		errno = ENOENT;
		return (NULL);
	}

	serrno = errno;

	if (resolved == NULL) {
		resolved = malloc(PATH_MAX);
		if (resolved == NULL)
			return (NULL);
		mem_allocated = 1;
	} else
		mem_allocated = 0;

	symlinks = 0;
	if (path[0] == '/') {
		resolved[0] = '/';
		resolved[1] = '\0';
		if (path[1] == '\0')
			return (resolved);
		resolved_len = 1;
		left_len = strlcpy(left, path + 1, sizeof(left));
	} else {
		if (getcwd(resolved, PATH_MAX) == NULL) {
			if (mem_allocated)
				free(resolved);
			else
				strlcpy(resolved, ".", PATH_MAX);
			return (NULL);
		}
		resolved_len = strlen(resolved);
		left_len = strlcpy(left, path, sizeof(left));
	}
	if (left_len >= sizeof(left)) {
		errno = ENAMETOOLONG;
		goto err;
	}

	/*
	 * Iterate over path components in `left'.
	 */
	while (left_len != 0) {
		/*
		 * Extract the next path component and adjust `left'
		 * and its length.
		 */
		p = strchr(left, '/');

		next_token_len = p ? (size_t) (p - left) : left_len;
		memcpy(next_token, left, next_token_len);
		next_token[next_token_len] = '\0';

		if (p != NULL) {
			left_len -= next_token_len + 1;
			memmove(left, p + 1, left_len + 1);
		} else {
			left[0] = '\0';
			left_len = 0;
		}

		if (resolved[resolved_len - 1] != '/') {
			if (resolved_len + 1 >= PATH_MAX) {
				errno = ENAMETOOLONG;
				goto err;
			}
			resolved[resolved_len++] = '/';
			resolved[resolved_len] = '\0';
		}
		if (next_token[0] == '\0')
			continue;
		else if (strcmp(next_token, ".") == 0)
			continue;
		else if (strcmp(next_token, "..") == 0) {
			/*
			 * Strip the last path component except when we have
			 * single "/"
			 */
			if (resolved_len > 1) {
				resolved[resolved_len - 1] = '\0';
				q = strrchr(resolved, '/') + 1;
				*q = '\0';
				resolved_len = q - resolved;
			}
			continue;
		}

		/*
		 * Append the next path component and readlink() it. If
		 * readlink() fails we still can return successfully if
		 * it exists but isn't a symlink, or if there are no more
		 * path components left.
		 */
		resolved_len = strlcat(resolved, next_token, PATH_MAX);
		if (resolved_len >= PATH_MAX) {
			errno = ENAMETOOLONG;
			goto err;
		}
		slen = readlink(resolved, symlink, sizeof(symlink));
		if (slen < 0) {
			switch (errno) {
			case EINVAL:
				/* not a symlink, continue to next component */
				continue;
			case ENOENT:
				if (p == NULL) {
					errno = serrno;
					return (resolved);
				}
				/* FALLTHROUGH */
			default:
				goto err;
			}
		} else if (slen == 0) {
			errno = EINVAL;
			goto err;
		} else if (slen == sizeof(symlink)) {
			errno = ENAMETOOLONG;
			goto err;
		} else {
			if (symlinks++ > SYMLOOP_MAX) {
				errno = ELOOP;
				goto err;
			}

			symlink[slen] = '\0';
			if (symlink[0] == '/') {
				resolved[1] = 0;
				resolved_len = 1;
			} else {
				/* Strip the last path component. */
				q = strrchr(resolved, '/') + 1;
				*q = '\0';
				resolved_len = q - resolved;
			}

			/*
			 * If there are any path components left, then
			 * append them to symlink. The result is placed
			 * in `left'.
			 */
			if (p != NULL) {
				if (symlink[slen - 1] != '/') {
					if (slen + 1 >= sizeof(symlink)) {
						errno = ENAMETOOLONG;
						goto err;
					}
					symlink[slen] = '/';
					symlink[slen + 1] = 0;
				}
				left_len = strlcat(symlink, left, sizeof(symlink));
				if (left_len >= sizeof(symlink)) {
					errno = ENAMETOOLONG;
					goto err;
				}
			}
			left_len = strlcpy(left, symlink, sizeof(left));
		}
	}

	/*
	 * Remove trailing slash except when the resolved pathname
	 * is a single "/".
	 */
	if (resolved_len > 1 && resolved[resolved_len - 1] == '/')
		resolved[resolved_len - 1] = '\0';
	return (resolved);

err:
	if (mem_allocated)
		free(resolved);
	return (NULL);
}

/*
 * Copyright (c) 2019 Bob Beck <beck@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

int __realpath(const char *pathname, char *resolved);
PROTO_NORMAL(__realpath);

/*
 * wrapper for kernel __realpath
 */

char *
realpath(const char *path, char *resolved)
{
	char pbuf[PATH_MAX], rbuf[PATH_MAX], expected[PATH_MAX];
	struct syslog_data sdata = SYSLOG_DATA_INIT;
	int usererrno = 0, kernelerrno = 0, trailingslash = 0, save_errno;
	int kernelonly = (getenv("USE_KERNEL_REALPATH") != NULL);
	ssize_t i;

	rbuf[0] = pbuf[0] = expected[0] = '\0';

	if (!kernelonly) {
		memset(expected, 0, sizeof(expected));
		if (urealpath(path, expected) == NULL) {
			usererrno = errno;
			expected[0] = '\0';
		}
	}

	if (path == NULL) {
		kernelerrno = EINVAL;
		goto out;
	}
	if (path[0] == '\0') {
		kernelerrno = ENOENT;
		goto out;
	}
	if (strlcat(pbuf, path, sizeof(pbuf)) >= sizeof(pbuf)) {
		kernelerrno = ENAMETOOLONG;
		goto out;
	}

	if (pbuf[strlen(pbuf) - 1] == '/')
		trailingslash = 1;

	if (__realpath(pbuf, rbuf) == -1)
		kernelerrno = errno;

	/*
	 * XXX XXX XXX
	 *
	 * The old userland implementation strips trailing slashes.
	 * According to Dr. POSIX, realpathing "/bsd" should be fine,
	 * realpathing "/bsd/" should return ENOTDIR.
	 *
	 * Similar, but *different* to the above, The old userland
	 * implementation allows for realpathing "/nonexistent" but
	 * not "/nonexistent/", Both those should return ENOENT
	 * according to POSIX.
	 *
	 * This hack should go away once we decide to match POSIX.
	 * which we should as soon as is convenient.
	 */
	if (kernelerrno == ENOTDIR) {
		/* Try again without the trailing slash. */
		kernelerrno = 0;
		for (i = strlen(pbuf); i > 1 && pbuf[i - 1] == '/'; i--)
			pbuf[i - 1] = '\0';
		rbuf[0] = '\0';
		if (__realpath(pbuf, rbuf) == -1)
			kernelerrno = errno;
	}

out:
	if (!kernelonly) {
		/* syslog if kernel and userland are different */
		save_errno = errno;
		if (strcmp(rbuf, expected) != 0 || (usererrno == 0 &&
		    kernelerrno != 0))
			syslog_r(LOG_CRIT | LOG_CONS, &sdata,
			    "realpath '%s' -> '%s' errno %d, "
			    "expected '%s' errno %d", path, rbuf,
			    kernelerrno, expected, usererrno);
		errno = save_errno;

		/* use userland result */
		if (usererrno) {
			errno = usererrno;
			return NULL;
		}
		else
			errno = 0;
		if (resolved == NULL)
			resolved = strdup(expected);
		else if (strlcpy(resolved, expected, PATH_MAX) >= PATH_MAX) {
			errno = ENAMETOOLONG;
			return NULL;
		}

	} else {
		/* use kernel result */
		if (kernelerrno) {
			errno = kernelerrno;
			return NULL;
		}
		else
			errno = 0;
		if (resolved == NULL)
			resolved = strdup(rbuf);
		else if (strlcpy(resolved, rbuf, PATH_MAX) >= PATH_MAX) {
			errno = ENAMETOOLONG;
			return NULL;
		}
	}
	return (resolved);
}
