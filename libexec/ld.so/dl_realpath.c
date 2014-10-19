/*	$OpenBSD: dl_realpath.c,v 1.3 2014/10/19 03:56:28 doug Exp $ */
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

#include <sys/types.h>
#include <limits.h>
#include "archdep.h"

#define ENOENT -2

/*
 * This file was copied from libc/stdlib/realpath.c and modified for ld.so's
 * syscall method which returns -errno.
 */

/*
 * char *dl_realpath(const char *path, char resolved[PATH_MAX]);
 *
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 */
char *
_dl_realpath(const char *path, char *resolved)
{
	struct stat sb;
	const char *p, *s;
	char *q;
	size_t left_len, resolved_len;
	unsigned symlinks;
	int slen, mem_allocated, ret;
	char left[PATH_MAX], next_token[PATH_MAX], symlink[PATH_MAX];

	if (path[0] == '\0') {
		return (NULL);
	}

	if (resolved == NULL) {
		resolved = _dl_malloc(PATH_MAX);
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
		left_len = _dl_strlcpy(left, path + 1, sizeof(left));
	} else {
		if (_dl_getcwd(resolved, PATH_MAX) <= 0) {
			if (mem_allocated)
				_dl_free(resolved);
			else
				_dl_strlcpy(resolved, ".", PATH_MAX);
			return (NULL);
		}
		resolved_len = _dl_strlen(resolved);
		left_len = _dl_strlcpy(left, path, sizeof(left));
	}
	if (left_len >= sizeof(left) || resolved_len >= PATH_MAX) {
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
		p = _dl_strchr(left, '/');
		s = p ? p : left + left_len;
		if (s - left >= sizeof(next_token)) {
			goto err;
		}
		_dl_bcopy(left, next_token, s - left);
		next_token[s - left] = '\0';
		left_len -= s - left;
		if (p != NULL)
			_dl_bcopy(s + 1, left, left_len + 1);
		if (resolved[resolved_len - 1] != '/') {
			if (resolved_len + 1 >= PATH_MAX) {
				goto err;
			}
			resolved[resolved_len++] = '/';
			resolved[resolved_len] = '\0';
		}
		if (next_token[0] == '\0')
			continue;
		else if (_dl_strcmp(next_token, ".") == 0)
			continue;
		else if (_dl_strcmp(next_token, "..") == 0) {
			/*
			 * Strip the last path component except when we have
			 * single "/"
			 */
			if (resolved_len > 1) {
				resolved[resolved_len - 1] = '\0';
				q = _dl_strrchr(resolved, '/') + 1;
				*q = '\0';
				resolved_len = q - resolved;
			}
			continue;
		}

		/*
		 * Append the next path component and lstat() it. If
		 * lstat() fails we still can return successfully if
		 * there are no more path components left.
		 */
		resolved_len = _dl_strlcat(resolved, next_token, PATH_MAX);
		if (resolved_len >= PATH_MAX) {
			goto err;
		}
		if ((ret = _dl_lstat(resolved, &sb)) != 0) {
			if (ret == ENOENT && p == NULL) {
				return (resolved);
			}
			goto err;
		}
		if (S_ISLNK(sb.st_mode)) {
			if (symlinks++ > SYMLOOP_MAX) {
				goto err;
			}
			slen = _dl_readlink(resolved, symlink, sizeof(symlink) - 1);
			if (slen < 0)
				goto err;
			symlink[slen] = '\0';
			if (symlink[0] == '/') {
				resolved[1] = 0;
				resolved_len = 1;
			} else if (resolved_len > 1) {
				/* Strip the last path component. */
				resolved[resolved_len - 1] = '\0';
				q = _dl_strrchr(resolved, '/') + 1;
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
						goto err;
					}
					symlink[slen] = '/';
					symlink[slen + 1] = 0;
				}
				left_len = _dl_strlcat(symlink, left, sizeof(symlink));
				if (left_len >= sizeof(left)) {
					goto err;
				}
			}
			left_len = _dl_strlcpy(left, symlink, sizeof(left));
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
		_dl_free(resolved);
	return (NULL);
}
