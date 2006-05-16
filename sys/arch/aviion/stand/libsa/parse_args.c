/*	$OpenBSD: parse_args.c,v 1.1 2006/05/16 22:48:18 miod Exp $ */

/*-
 * Copyright (c) 1995 Theo de Raadt
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/prom.h>
#include <a.out.h>

#include "stand.h"
#include "libsa.h"

#define KERNEL_NAME "bsd"

int
parse_args(char *line, char **filep)
{
	char *name = NULL, *p;

	/* recognize the special ``halt'' keyword */
	if (strcmp(line, "halt") == 0)
		return (1);

	/*
	 * The command line should be under the form
	 *   devtype(...)filename args
	 * such as
	 *   inen()bsd -s
	 * and we only care about the kernel name here.
	 *
	 * However, if the kernel could not be loaded, and we asked the
	 * user, he may not give the devtype() part - especially since
	 * at the moment we only support inen() anyway.
	 */

	/* search for a set of braces */
	for (p = line; *p != '\0' && *p != '('; p++) ;
	if (*p != '\0') {
		for (p = line; *p != '\0' && *p != ')'; p++) ;
		if (*p != '\0')
			name = ++p;
	}
		
	if (name == NULL)
		name = line;

	/* now insert a NUL before any option */
	for (p = name; *p != '\0' && *p != ' '; p++) ;
	*p = '\0';

	/* no name, use the default */
	if (*name == '\0')
		name = KERNEL_NAME;

	*filep = name;
	return (0);
}
