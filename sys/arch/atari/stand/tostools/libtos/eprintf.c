/*	$NetBSD: eprintf.c,v 1.1.1.1 1996/01/07 21:50:49 leo Exp $	*/

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

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include "libtos.h"

static int	output_redirected = 0;

void
redirect_output(fn)
	char	*fn;
{
	int	fd;

	fd = creat(fn, 0666);
	if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0)
		fatal(errno, "%s", fn);
	output_redirected = 1;
}

/*
 * Print output to stderr. When output is redirected,
 * also write it to stdout.
 */
int
eprintf(frm)
	char	*frm;
{
	va_list	args;
	int	rv;

	va_start(args, frm);
	rv = veprintf(frm, args);
	va_end(args);
	return(rv);
}

int
veprintf(frm, args)
	char	*frm;
	va_list	args;
{
	int	rv;

	rv = vfprintf(stderr, frm, args);
	if (output_redirected)
		vfprintf(stdout, frm, args);
	return(rv);
}
