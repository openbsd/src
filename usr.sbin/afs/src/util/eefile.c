/*	$OpenBSD: eefile.c,v 1.1 1999/04/30 01:59:16 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1999 Kungliga Tekniska Högskolan
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
RCSID("$KTH: eefile.c,v 1.2 1999/03/06 18:02:21 lha Exp $");
#endif

#include <stdio.h>
#include <roken.h>
#include <err.h>
#include "eefile.h"

void
eefopen (const char *name, const char *mode, fileblob *f)
{
     int streamfd;

     asprintf (&f->curname, "%sXXXXXX", name);
     if (f->curname == NULL)
	 err (1, "malloc");

     streamfd = mkstemp(f->curname);
     f->stream = fdopen (streamfd, mode);
     if (f->stream == NULL)
	 err (1, "open %s mode %s", f->curname, mode);
     f->newname = estrdup(name);
}

void
eefclose (fileblob *f)
{
     if (fclose (f->stream))
	 err (1, "close %s", f->curname);
     if (rename(f->curname, f->newname))
	 err (1, "rename %s, %s", f->curname, f->newname);
     free(f->curname);
     free(f->newname);
}

size_t
eefread (void *ptr, size_t size, size_t nitems, fileblob *f)
{
     size_t res;

     res = fread (ptr, size, nitems, f->stream);
     if (res == 0)
	 err (1, "read %s", f->curname);
     return res;
}

size_t
eefwrite (const void *ptr, size_t size, size_t nitems, fileblob *f)
{
     size_t res;

     res = fwrite (ptr, size, nitems, f->stream);
     if (res == 0)
	 err (1, "write %s", f->curname);
     return res;
}
