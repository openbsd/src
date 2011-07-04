/* $OpenBSD: crt0.c,v 1.1 2011/07/04 05:42:11 pirofti Exp $ */

/*
 * Copyright (c) 2011 Paul Irofti <pirofti@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include <sys/param.h>
#include <sys/exec.h>
#include <stdlib.h>

static char	*_strrchr(char *, char);

char	**environ;
char	*__progname = "";

char __progname_storage[NAME_MAX+1];

#ifdef MCRT0
extern void	monstartup(u_long, u_long);
extern void	_mcleanup(void);
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */

__asm ("1:						\
	{ .mii					\
	  mov		r15=@gprel(1b)	\n	\
	  mov		r16=ip	;;	\n	\
	  sub		gp=r16,r15	\n	\
	;;					\
	} ");

void
___start(int argc, char **argv, char **envp, void (*cleanup)(void),
    const void *obj, struct ps_strings *ps_strings)
{
	char *namep;
	register struct kframe *kfp;
	char *s;

        environ = envp;

	if ((namep = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(namep, '/')) == NULL)
			__progname = namep;
		else
			++__progname;
		for (s = __progname_storage; *__progname &&
		    s < &__progname_storage[sizeof __progname_storage - 1]; )
			*s++ = *__progname++;
		*s = '\0';
		__progname = __progname_storage;
	}

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

	__init();

	exit(main(argc, argv, environ));
}


static char *
_strrchr(p, ch)
register char *p, ch;
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
/* NOTREACHED */
}

#ifdef MCRT0
asm ("  .text");
asm ("_eprol:");
#endif
