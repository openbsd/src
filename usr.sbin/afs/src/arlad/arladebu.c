/*
 * Copyright (c) 1995 - 2000, 2002 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <roken.h>
#include <parse_units.h>
#include <fs.h>
#include "arladeb.h"

RCSID("$arla: arladebu.c,v 1.5 2002/07/24 06:01:07 lha Exp $");

#define all (ADEBERROR | ADEBWARN | ADEBDISCONN | ADEBFBUF |		\
	     ADEBMSG | ADEBKERNEL | ADEBCLEANER | ADEBCALLBACK |	\
	     ADEBCM | ADEBVOLCACHE | ADEBFCACHE | ADEBINIT |		\
	     ADEBCONN | ADEBMISC | ADEBVLOG)

struct units arla_deb_units[] = {
    { "all",		all},
    { "almost-all",	all & ~ADEBCLEANER},
    { "venuslog",	ADEBVLOG },
    { "errors",		ADEBERROR },
    { "warnings",	ADEBWARN },
    { "disconn",	ADEBDISCONN },
    { "fbuf",		ADEBFBUF },
    { "messages",	ADEBMSG },
    { "kernel",		ADEBKERNEL },
    { "cleaner",	ADEBCLEANER },
    { "callbacks",	ADEBCALLBACK },
    { "cache-manager",	ADEBCM },
    { "volume-cache",	ADEBVOLCACHE },
    { "file-cache",	ADEBFCACHE },
    { "initialization",	ADEBINIT },
    { "connection",	ADEBCONN },
    { "miscellaneous",	ADEBMISC },
    { "invalidator",	ADEBINVALIDATOR },
    { "default",	ARLA_DEFAULT_LOG },
    { "none",		0 },
    { NULL }
};

void
arla_log_print_levels (FILE *f)
{
    print_flags_table (arla_deb_units, f);
}
