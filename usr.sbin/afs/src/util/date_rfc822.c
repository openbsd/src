/*	$OpenBSD: date_rfc822.c,v 1.1.1.1 1998/09/14 21:53:21 art Exp $	*/
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
 * $KTH: date_rfc822.c,v 1.5 1998/02/11 03:43:32 art Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: date_rfc822.c,v 1.5 1998/02/11 03:43:32 art Exp $");
#endif

#include <roken.h>
#include <time.h>
#include "date_rfc822.h"

/*
 * Return current date in RFC822-format with timezone GMT.
 * Memory is allocated with strdup.
 */

char *
date_time2rfc822 (time_t t)
{
     char tmp [80];

     strftime (tmp, sizeof (tmp), "%A, %d-%h-%y %H:%M:%S %Z", gmtime (&t));
     return strdup (tmp);
}

#if 0 /* XXX */
/*
 * Convert from RFC 822 representation of date into a time_t.
 */

time_t
date_rfc8222time (char *s)
{
     struct tm tm;

     strptime (s, "%d-%h-%y %H:%M:%S ", &tm);
     return timegm (&tm);
}
#endif
