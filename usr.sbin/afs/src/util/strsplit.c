/*	$OpenBSD: strsplit.c,v 1.1.1.1 1998/09/14 21:53:25 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Split a string
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: strsplit.c,v 1.4 1998/02/22 11:22:18 assar Exp $");
#endif

#include <string.h>
#include "strsplit.h"

/* XXX - fix this */
int
strsplit (char *str, char *pat, ...)
{
     va_list args;
     char *p;
     char **sub;
     unsigned hits = 0;

     va_start(args, pat);
     if (pat == NULL)
	  pat = " \t";

     for (p = str; *p; ++hits) {
	  p += strspn (p, pat);
	  sub = va_arg(args, char **);
	  if (*p == '\0' || sub == NULL)
	       break;
	  *sub = p;
	  p += strcspn (p, pat);
	  if (*p != '\0') {
	       *p = '\0';
	       p++;
	  }
     }
     va_end(args);
     return hits;
}

int
vstrsplit (char *str, char *pat, unsigned nsub, char **sub)
{
     unsigned hits = 0;
     char *p;

     if (pat == NULL)
	  pat = " \t";

     for (p = str; *p; ++hits) {
	  p += strspn (p, pat);
	  if (*p == '\0' || sub == NULL || nsub-- == 0)
	       break;
	  *sub++ = p;
	  p += strcspn (p, pat);
	  if (*p != '\0') {
	       *p = '\0';
	       p++;
	  }
     }
     return hits;
}
