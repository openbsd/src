/*	$OpenBSD: mkpath.c,v 1.5 2021/05/17 11:52:10 claudio Exp $ */
/*
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "extern.h"

/* Code taken directly from mkdir(1).

 * mkpath -- create directories.
 *	path     - path
 */
int
mkpath(char *path)
{
	struct stat sb;
	char *slash;
	int done;

	slash = path;

	for (;;) {
		slash += strspn(slash, "/");
		slash += strcspn(slash, "/");

		done = (*slash == '\0');
		*slash = '\0';

		if (mkdir(path, 0777) != 0) {
			int mkdir_errno = errno;

			if (stat(path, &sb) == -1) {
				/* Not there; use mkdir()s errno */
				errno = mkdir_errno;
				return (-1);
			}
			if (!S_ISDIR(sb.st_mode)) {
				/* Is there, but isn't a directory */
				errno = ENOTDIR;
				return (-1);
			}
		}

		if (done)
			break;

		*slash = '/';
	}

	return (0);
}
