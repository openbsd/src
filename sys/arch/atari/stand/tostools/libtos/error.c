/*	$NetBSD: error.c,v 1.1.1.1 1996/01/07 21:50:49 leo Exp $	*/

/*
 * Copyright (c) 1995 Waldi Ravens.
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
 *        This product includes software developed by Waldi Ravens.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <osbind.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libtos.h"

static void	errmsg PROTO((int, char *, va_list));

const char	*program_name;

void
init_toslib(arg0)
	char	*arg0;
{
	char	*p;

	if (isatty(STDERR_FILENO) && (!*arg0 || !getenv("STDERR")))
		Fforce(STDERR_FILENO, -1);

	if (!(p = strrchr(arg0, '/')))
		p = strrchr(arg0, '\\');
	program_name = p ? ++p : arg0;
}

void
error(err, frm)
	int	err;
	char	*frm;
{
	va_list		args;

	va_start(args, frm);
	errmsg(err, frm, args);
	va_end(args);
}

void
fatal(err, frm)
	int	err;
	char	*frm;
{
	va_list		args;

	va_start(args, frm);
	errmsg(err, frm, args);
	va_end(args);

	xexit(EXIT_FAILURE);
}

static void
errmsg(err, frm, args)
	int	err;
	char	*frm;
	va_list	args;
{
	extern const char *program_name;

	eprintf("%s: ", program_name);
	veprintf(frm, args);

	if (err != -1) {
		char	*es = strerror(err);
		if (es)
			eprintf(": %s", es);
		else
			eprintf(": unknown error %d", err);
	}

	eprintf("\n");
}
