/*	$OpenBSD: init.c,v 1.3 2016/03/21 00:41:13 guenther Exp $ */
/*
 * Copyright (c) 2014,2015 Philip Guenther <guenther@openbsd.org>
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


#define _DYN_LOADER

#include <sys/types.h>
#include <sys/exec_elf.h>
#include <sys/syscall.h>

#include <limits.h>		/* NAME_MAX */
#include <stdlib.h>		/* atexit */
#include <string.h>
#include <unistd.h>		/* _pagesize */

/* XXX should be in an include file shared with csu */
char	***_csu_finish(char **_argv, char **_envp, void (*_cleanup)(void));

/* provide definition for this */
int	_pagesize = 0;

/*
 * In dynamicly linked binaries environ and __progname are overriden by
 * the definitions in ld.so.
 */
char	**environ __attribute__((weak)) = NULL;
char	*__progname __attribute__((weak)) = NULL;


#ifndef PIC
static inline void early_static_init(char **_argv, char **_envp);
#endif /* PIC */


/*
 * extract useful bits from the auxiliary vector and either
 * a) register ld.so's cleanup in dynamic links, or
 * b) init __progname, environ, and the TIB in static links.
 */
char ***
_csu_finish(char **argv, char **envp, void (*cleanup)(void))
{
	AuxInfo	*aux;

#ifndef PIC
	/* static libc in a static link? */
	if (cleanup == NULL)
		early_static_init(argv, envp);
#endif /* !PIC */

	/* Extract useful bits from the auxiliary vector */
	while (*envp++ != NULL)
		;
	for (aux = (void *)envp; aux->au_id != AUX_null; aux++) {
		switch (aux->au_id) {
		case AUX_pagesz:
			_pagesize = aux->au_v;
			break;
		}
	}

	if (cleanup != NULL)
		atexit(cleanup);

	return &environ;
}

#ifndef PIC
/*
 * static libc in a static link?  Then disable kbind and set up
 * __progname and environ
 */
static inline void
early_static_init(char **argv, char **envp)
{
	static char progname_storage[NAME_MAX+1];

	/* disable kbind */
	syscall(SYS_kbind, (void *)NULL, (size_t)0, (long long)0);

	environ = envp;

	/* set up __progname */
	if (*argv != NULL) {		/* NULL ptr if argc = 0 */
		const char *p = strrchr(*argv, '/');

		if (p == NULL)
			p = *argv;
		else
			p++;
		strlcpy(progname_storage, p, sizeof(progname_storage));
	}
	__progname = progname_storage;
}
#endif /* !PIC */
