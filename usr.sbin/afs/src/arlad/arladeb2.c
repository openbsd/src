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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <roken.h>
#include "volcache.h"
#include "ko.h"

#include "arladeb.h"

RCSID("$Id: arladeb2.c,v 1.1 2000/09/11 14:40:40 art Exp $");

void
arla_warnx_with_fid (unsigned level, const VenusFid *fid, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_vwarnx_with_fid (level, fid, fmt, args);
    va_end(args);
}

void
arla_vwarnx_with_fid (unsigned level, const VenusFid *fid, const char *fmt,
		      va_list args)
{
    char *s;
    const char *cellname;
    const char *volname;

    cellname = cell_num2name (fid->Cell);
    if (cellname == NULL)
	cellname = "<unknown>";
    volname = volcache_getname (fid->fid.Volume, fid->Cell);
    if (volname == NULL)
	volname = "<unknown>";

    vasprintf (&s, fmt, args);
    if (s == NULL) {
	log_log (arla_log_unit, level,
		 "Sorry, no memory to print `%s'...", fmt);
	return;
    }
    log_log (arla_log_unit, level,
	     "volume %s (%ld) in cell %s (%ld): %s",
	     volname, (unsigned long)fid->fid.Volume, cellname,
	     (long)fid->Cell, s);
    free (s);
}
