/* $OpenBSD: simple_check.c,v 1.1 1999/09/27 21:40:04 espie Exp $ */
/*-
 * Copyright (c) 1999 Marc Espie.
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
 *	This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS 
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "extern.h"

#define CHECKER_STRING "/usr/bin/fgrep \"`cd %s && /bin/sha1 %s`\" /var/db/pkg/SHA1"
#define CHECKER2_STRING "/usr/bin/fgrep \\(%s\\) /var/db/pkg/SHA1"

int 
simple_check(pkg_name)
	const char *pkg_name;
{
	int result;
	char *buffer;
	char *dir, *file;

	dir = dirname(pkg_name);
	file = basename(pkg_name);
	if (dir == NULL || file == NULL)
		return PKG_SIGERROR;

	buffer = malloc(sizeof(CHECKER_STRING)+strlen(dir)+strlen(file));
	if (!buffer)
		return PKG_SIGERROR;
	sprintf(buffer, CHECKER_STRING, dir, file);
	result = system(buffer);
	free(buffer);
	if (result == 0)
		return PKG_GOODSIG;
	buffer = malloc(sizeof(CHECKER2_STRING)+strlen(file));
	if (!buffer)
		return PKG_SIGERROR;
	sprintf(buffer, CHECKER2_STRING, file);
	result = system(buffer);
	free(buffer);
	if (result == 0)
		return PKG_BADSIG;
	else
		return PKG_UNSIGNED;
}
	
	
