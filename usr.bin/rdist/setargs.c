/*
 * Copyright (c) 1983 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char RCSid[] = 
"$Id: setargs.c,v 1.1 1996/02/03 12:12:43 dm Exp $";

static char sccsid[] = "@(#)setargs.c";

static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#include "defs.h"

#if	defined(SETARGS)

/*
 * Set process argument functions
 */

#define MAXUSERENVIRON 		40
char 			      **Argv = NULL;
char 			       *LastArgv = NULL;
char 			       *UserEnviron[MAXUSERENVIRON+1];

/*
 * Settup things for using setproctitle()
 */
setargs_settup(argc, argv, envp)
	int			argc;
	char		      **argv;
	char		      **envp;
{
	register int 		i;
	extern char 	      **environ;

  	/* Remember the User Environment */

	for (i = 0; i < MAXUSERENVIRON && envp[i] != NULL; i++)
		UserEnviron[i] = strdup(envp[i]);
	UserEnviron[i] = NULL;
	environ = UserEnviron;

  	/* Save start and extent of argv for setproctitle */
	Argv = argv;
	if (i > 0)
		LastArgv = envp[i-1] + strlen(envp[i-1]);
	else
		LastArgv = argv[argc-1] + strlen(argv[argc-1]);
}

/*
 * Set process title
 */
extern void _setproctitle(msg)
        char *msg;
{
	register int i;
	char *p;
	
	p = Argv[0];

	/* Make ps print "(program)" */
	*p++ = '-';
	
	i = strlen(msg);
	if (i > LastArgv - p - 2) {
		i = LastArgv - p - 2;
		msg[i] = '\0';
	}
	(void) strcpy(p, msg);
	p += i;
	while (p < LastArgv) {
		*p++ = ' ';
	}
}

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_VARARGS
/*
 * Varargs front-end to _setproctitle()
 */
extern void setproctitle(va_alist)
	va_dcl
{
	static char buf[BUFSIZ];
	va_list args;
	char *fmt;

	va_start(args);
	fmt = va_arg(args, char *);
	(void) vsprintf(buf, fmt, args);
	va_end(args);

	_setproctitle(buf);
}
#endif	/* ARG_VARARGS */
#if	defined(ARG_TYPE) && ARG_TYPE == ARG_STDARG
/*
 * Stdarg front-end to _setproctitle()
 */
extern void setproctitle(char *fmt, ...)
{
	static char buf[BUFSIZ];
	va_list args;

	va_start(args, fmt);
	(void) vsprintf(buf, fmt, args);
	va_end(args);

	_setproctitle(buf);
}
#endif	/* ARG_STDARG */
#if	!defined(ARG_TYPE)
/*
 * Non-Varargs front-end to _setproctitle()
 */
/*VARARGS1*/
extern void setproctitle(fmt, a1, a2, a3, a4, a5, a6)
	char *fmt;
{
	static char buf[BUFSIZ];

	(void) sprintf(buf, fmt, a1, a2, a3, a4, a5, a6);

	_setproctitle(buf);
}
#endif	/* !ARG_TYPE */

#endif 	/* SETARGS */
