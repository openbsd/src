/*	$OpenBSD: crt0.c,v 1.5 2003/01/16 19:15:38 mickey Exp $	*/

/*
 * Copyright (c) 2001 Michael Shalayeff
 * All rights reserved.
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

int	global __asm ("$global$") = 0;
int	sh_func_adrs __asm ("$$sh_func_adrs") = 0;

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: crt0.c,v 1.5 2003/01/16 19:15:38 mickey Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/fcntl.h>
#include <sys/exec.h>
#include <paths.h>

#include "common.h"

typedef char Obj_Entry;

/*
 * Lots of the chunks of this file cobbled together from pieces of
 * other OpenBSD crt files, including the common code.
 */

char	**environ;

extern void	__init(void);
extern void	__fini(void);

#ifdef MCRT0
extern void	monstartup(u_long, u_long);
extern void	_mcleanup(void);
extern unsigned char etext, eprol;
#endif /* MCRT0 */

void __start(char **, void (*)(void), const Obj_Entry *);
static char *__strrchr(const char *p, char ch);

void
__start(sp, cleanup, obj)
	char **sp;
	void (*cleanup)(void);			/* from shared loader */
	const Obj_Entry *obj;			/* from shared loader */
{
	struct ps_strings *arginfo = (struct ps_strings *)sp;
	char **argv, *namep;

	__asm __volatile (".import $global$, data\n\t"
			  "ldil L%%$global$, %%r27\n\t"
			  "ldo	R%%$global$(%%r27), %%r27" ::: "r27");

	argv = arginfo->ps_argvstr;
	environ = arginfo->ps_envstr;
	if ((namep = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(namep, '/')) == NULL)
			__progname = namep;
		else
			__progname++;
	}

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&eprol, (u_long)&etext);
#endif

	__init();

	exit(main(arginfo->ps_nargvstr, argv, environ));
}

static char *
__strrchr(const char *p, char ch)
{
	char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
}

#ifdef MCRT0
__asm (".export eprol, entry\n\t.label eprol");
#endif
