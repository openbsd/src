/*
 * Copyright (c) 1996, 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <sys/types.h>
#include <sys/param.h>

#include "sudo.h"

#ifndef STDC_HEADERS
#ifdef HAVE_PUTENV
extern int putenv	__P((const char *));
#endif /* HAVE_PUTENV */
#ifdef HAVE_SETENV
extern int setenv	__P((char *, char *, int));
#endif /* HAVE_SETENV */
#endif /* !STDC_HEADERS */

#ifndef lint
static const char rcsid[] = "$Sudo: sudo_setenv.c,v 1.40 1999/07/31 16:19:48 millert Exp $";
#endif /* lint */


/*
 * Add a string of the form "var=val" to the environment.  If we are unable
 * to expand the current environent, return -1, else return 0.
 */
int
sudo_setenv(var, val)
    char *var;
    char *val;
{

#ifdef HAVE_SETENV
    return(setenv(var, val, 1));
#else
    char *envstring, *tmp;

    envstring = tmp = (char *) malloc(strlen(var) + strlen(val) + 2);
    if (envstring == NULL)
	return(-1);

    while ((*tmp++ = *var++))
	;

    *(tmp-1) = '=';

    while ((*tmp++ = *val++))
	;

    return(putenv(envstring));
#endif /* HAVE_SETENV */
}
