/*	$OpenBSD: catopen.c,v 1.14 2011/07/12 21:31:20 matthew Exp $ */
/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define _NLS_PRIVATE

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <nl_types.h>

#define NLS_DEFAULT_PATH "/usr/share/nls/%L/%N.cat:/usr/share/nls/%N/%L"
#define NLS_DEFAULT_LANG "C"

static nl_catd load_msgcat(const char *);

/* ARGSUSED */
nl_catd
_catopen(const char *name, int oflag)
{
	char tmppath[PATH_MAX];
	char *nlspath;
	char *lang;
	char *s, *t;
	const char *u;
	nl_catd catd;
		
	if (name == NULL || *name == '\0')
		return (nl_catd) -1;

	/* absolute or relative path? */
	if (strchr(name, '/'))
		return load_msgcat(name);

	if (issetugid() != 0 || (nlspath = getenv("NLSPATH")) == NULL)
		nlspath = NLS_DEFAULT_PATH;
	if ((lang = getenv("LANG")) == NULL)
		lang = NLS_DEFAULT_LANG;

	s = nlspath;
	t = tmppath;	
	do {
		while (*s && *s != ':') {
			if (*s == '%') {
				switch (*(++s)) {
				case 'L':	/* locale */
					u = lang;
					while (*u && t < tmppath + PATH_MAX-1)
						*t++ = *u++;
					break;
				case 'N':	/* name */
					u = name;
					while (*u && t < tmppath + PATH_MAX-1)
						*t++ = *u++;
					break;
				case 'l':	/* lang */
				case 't':	/* territory */
				case 'c':	/* codeset */
					break;
				default:
					if (t < tmppath + PATH_MAX-1)
						*t++ = *s;
				}
			} else {
				if (t < tmppath + PATH_MAX-1)
					*t++ = *s;
			}
			s++;
		}

		*t = '\0';
		catd = load_msgcat(tmppath);
		if (catd != (nl_catd) -1)
			return catd;

		if (*s)
			s++;
		t = tmppath;
	} while (*s);

	return (nl_catd) -1;
}

static nl_catd
load_msgcat(const char *path)
{
	struct stat st;
	nl_catd catd;
	void *data;
	int fd;

	if ((fd = open(path, O_RDONLY)) == -1)
		return (nl_catd) -1;

	if (fstat(fd, &st) != 0) {
		close (fd);
		return (nl_catd) -1;
	}

	data = mmap(0, (size_t) st.st_size, PROT_READ, MAP_SHARED, fd, (off_t)0);
	close (fd);

	if (data == MAP_FAILED) {
		return (nl_catd) -1;
	}

	if (ntohl(((struct _nls_cat_hdr *) data)->__magic) != _NLS_MAGIC) {
		munmap(data, (size_t) st.st_size);
		return (nl_catd) -1;
	}

	if ((catd = malloc(sizeof (*catd))) == 0) {
		munmap(data, (size_t) st.st_size);
		return (nl_catd) -1;
	}

	catd->__data = data;
	catd->__size = st.st_size;
	return catd;
}
