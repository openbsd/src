/*	$OpenBSD: strmatch.c,v 1.1.1.1 1998/09/14 21:53:25 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
RCSID("$KTH: strmatch.c,v 1.5 1998/07/22 03:23:45 assar Exp $");
#endif

#include "strmatch.h"

/*
 * Return 1 iff `str' matches the pattern in `pat'.
 * The pattern consists of ordinary characters that must match exactly,
 * ? that match one character and * that match zero of more characters.
 * \ may be used to quite characters.
 */

int
strmatch (const char *pat, const char *str)
{
     int c1, c2;

     while ((c1 = *pat++) != '\0') {
	  c2 = *str;
	  if (c1 == '\\')
	       if ((c1 = *pat++) != c2)
		    return 1;
	  if (c1 == c2 || c1 == '?')
	       ;
	  else if (c1 == '*') {
	       if (*pat == '\0')
		    return 0;
	       while (*str) {
		    if (strmatch (pat, str) == 0)
			 return 0;
		    ++str;
	       }
	       return 1;
	  } else
	       return 1;
	  ++str;
     }
     return *str != c1;
}
