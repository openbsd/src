/*	$OpenBSD: init.c,v 1.4 2016/05/07 19:05:22 guenther Exp $ */
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

#ifndef PIC
#include <sys/mman.h>
#endif

#include <tib.h>
#include <limits.h>		/* NAME_MAX */
#include <stdlib.h>		/* atexit */
#include <string.h>
#include <unistd.h>

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
static inline void setup_static_tib(Elf_Phdr *_phdr, int _phnum);
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
	Elf_Phdr *phdr = NULL;
	int phnum = 0;

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
#ifndef PIC
		case AUX_phdr:
			phdr = (void *)aux->au_v;
			break;
		case AUX_phnum:
			phnum = aux->au_v;
			break;
#endif /* !PIC */
		}
	}

#ifndef PIC
	/* static libc in a static link? */
	if (cleanup == NULL)
		setup_static_tib(phdr, phnum);
#endif /* !PIC */

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

/*
 * static TLS handling
 */
#define ELF_ROUND(x,malign)	(((x) + (malign)-1) & ~((malign)-1))

/* for static binaries, the location and size of the TLS image */
static void		*static_tls;
static size_t		static_tls_fsize;

size_t			_static_tls_size = 0;

static inline void
setup_static_tib(Elf_Phdr *phdr, int phnum)
{
	struct tib *tib;
	char *base;
	int i;

	if (phdr != NULL) {
		for (i = 0; i < phnum; i++) {
			if (phdr[i].p_type != PT_TLS)
				continue;
			if (phdr[i].p_memsz == 0)
				break;
			if (phdr[i].p_memsz < phdr[i].p_filesz)
				break;		/* invalid */
#if TLS_VARIANT == 1
			_static_tls_size = phdr[i].p_memsz;
#elif TLS_VARIANT == 2
			/*
			 * variant 2 places the data before the TIB
			 * so we need to round up to the alignment
			 */
			_static_tls_size = ELF_ROUND(phdr[i].p_memsz,
			    phdr[i].p_align);
#endif
			if (phdr[i].p_vaddr != 0 && phdr[i].p_filesz != 0) {
				static_tls = (void *)phdr[i].p_vaddr;
				static_tls_fsize = phdr[i].p_filesz;
			}
			break;
		}
	}

	/*
	 * We call getpagesize() here instead of using _pagesize because
	 * there's no aux-vector in non-PIE static links, so _pagesize
	 * might not be set yet.  If so getpagesize() will get the value.
	 */
	base = mmap(NULL, ELF_ROUND(_static_tls_size + sizeof *tib,
	    getpagesize()), PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
# if TLS_VARIANT == 1
	tib = (struct tib *)base;
# elif TLS_VARIANT == 2
	tib = (struct tib *)(base + _static_tls_size);
# endif

	_static_tls_init(base);
	TIB_INIT(tib, NULL, NULL);
	tib->tib_tid = getthrid();
	TCB_SET(TIB_TO_TCB(tib));
#if ! TCB_HAVE_MD_GET
	_libc_single_tcb = TIB_TO_TCB(tib);
#endif
}

void
_static_tls_init(char *base)
{
	if (_static_tls_size) {
#if TLS_VARIANT == 1
		base += sizeof(struct tib);
#endif
		if (static_tls != NULL)
			memcpy(base, static_tls, static_tls_fsize);
		memset(base + static_tls_fsize, 0,
		    _static_tls_size - static_tls_fsize);
	}
}
#endif /* !PIC */
