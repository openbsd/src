/*	$NetBSD: catopen.c,v 1.6.4.2 1996/05/28 22:46:56 jtc Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

nl_catd
_catopen(name, oflag)
	const char *name;
	int oflag;
{
	const char *path;
	struct stat st;
	nl_catd catd;
	int fd;
	void *data;

	struct _nls_cat_hdr *cat_hdr;

	if (name == NULL || *name == '\0')
		return (nl_catd) -1;

	/* absolute or relative path? */
	if (strchr (name, '/')) {
		if (stat (name, &st)) {
			return (nl_catd) -1;
		}
		path = name;
	} else {
		char tmppath[PATH_MAX];
		char *nlspath;
		char *lang;
		char *s, *t;
		
		if ((nlspath = getenv ("NLSPATH")) == NULL) {
			nlspath = NLS_DEFAULT_PATH;
		}
		if ((lang = getenv ("LANG")) == NULL) {
			lang = NLS_DEFAULT_LANG;
		}

		for (s = nlspath, t = tmppath; *s; ) {
			if (*s == '%') {
				if (*(s + 1) == 'L') {
					strcpy(t, lang);
					t += strlen(lang);
					s += 2;
				} else if (*(s + 1) == 'N') {
					strcpy(t, name);
					t += strlen(name);
					s += 2;
				} else {
					*t++ = *s++;
				}
			} else if (*s == ':') {
				*t = '\0';

				if (stat (tmppath, &st) == 0) {
					path = tmppath;
					goto load_msgcat;
				}

				t = tmppath;
			} else {
				*t++ = *s++;
			}
		}

		return (nl_catd) -1;
	}

load_msgcat:
	if ((fd = open (path, O_RDONLY)) == -1)
		return (nl_catd) -1;

	if (fstat(fd, &st) != 0) {
		close (fd);
		return (nl_catd) -1;
	}

	data = mmap(0, (size_t) st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close (fd);

	if (data == (void *) -1) {
		munmap(data, (size_t) st.st_size);
		return (nl_catd) -1;
	}

	if (ntohl(((struct _nls_cat_hdr *) data)->__magic) != _NLS_MAGIC) {
		munmap(data, (size_t) st.st_size);
		return (nl_catd) -1;
	}

	if ((catd = malloc (sizeof (*catd))) == 0) {
		munmap(data, (size_t) st.st_size);
		return (nl_catd) -1;
	}

	catd->__data = data;
	catd->__size = st.st_size;
	return catd;
}
