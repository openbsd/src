/*	$OpenBSD: efile.c,v 1.2 1999/04/30 01:59:16 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: efile.c,v 1.5 1999/02/27 11:04:50 assar Exp $");
#endif

#include <stdio.h>
#include "efile.h"

FILE *
efopen (const char *name, const char *mode)
{
     FILE *tmp;

     tmp = fopen (name, mode);
     if (tmp == NULL)
	 err (1, "open %s mode %s", name, mode);
     return tmp;
}

void
efclose (FILE *f)
{
     if (fclose (f))
	 err (1, "close");
}

size_t
efread (void *ptr, size_t size, size_t nitems, FILE *stream)
{
     size_t res;

     res = fread (ptr, size, nitems, stream);
     if (res == 0)
	 err (1, "read");
     return res;
}

size_t
efwrite (const void *ptr, size_t size, size_t nitems, FILE *stream)
{
     size_t res;

     res = fwrite (ptr, size, nitems, stream);
     if (res == 0)
	 err (1, "write");
     return res;
}
