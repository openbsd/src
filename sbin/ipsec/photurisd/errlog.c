/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
 *
 * This is partly derived from code by Angelos D. Keromytis, kermit@forthnet.gr
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * $Header: /home/cvs/src/sbin/ipsec/photurisd/Attic/errlog.c,v 1.1.1.1 1997/07/18 22:48:49 provos Exp $
 *
 * $Author: provos $
 *
 * $Log: errlog.c,v $
 * Revision 1.1.1.1  1997/07/18 22:48:49  provos
 * initial import of the photuris keymanagement daemon
 *
 * Revision 1.1  1997/05/22 17:34:16  provos
 * Initial revision
 *
 */

#ifndef lint
static char rcsid[] = "$Id: errlog.c,v 1.1.1.1 1997/07/18 22:48:49 provos Exp $";
#endif

#define _ERRLOG_C_

#include <stdio.h>
#include <stdlib.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <errno.h>
#include "buffer.h"
#include "errlog.h"

#ifdef NEED_SNPRINTF
#include "snprintf.h"
#endif

#if defined(sun) || defined(_AIX)
extern char *sys_errlist[];
extern int errno;
#endif

#define LOG_SIZE 200

void _log_error(int flag, char *fmt, va_list ap);

/*
 * crit_error:
 * log the error and exit
 */

void
#if __STDC__
crit_error(int flag, char *fmt, ...)
#else
crit_error(flag, fmt, va_alist)
        int flag;
        char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	_log_error(flag, fmt, ap);
	va_end(ap);
	exit(-1);
}

/*
 * log_error:
 * log an error
 */

void
#if __STDC__
log_error(int flag, char *fmt, ...)
#else
log_error(flag, fmt, va_alist)
        int flag;
        char *fmt;
        va_dcl
#endif
{
     va_list ap;
#if __STDC__
     va_start(ap, fmt);
#else
     va_start(ap);
#endif
     _log_error(flag, fmt, ap);
     va_end(ap);
}

void
_log_error(int flag, char *fmt, va_list ap)
{
     char *buffer = calloc(LOG_SIZE, sizeof(char));
#ifdef __SWR
     FILE f;
#endif
     if(buffer == NULL)
	  return;

#ifdef DEBUG
     sprintf(buffer, "%s: ", (flag ? "Error" : "Warning"));
#else
     buffer[0] = '\0';
#endif

#ifdef __SWR
     f._flags = __SWR | __SSTR;
     f._bf._base = f._p = buffer + strlen(buffer);
     f._bf._size = f._w = LOG_SIZE-1-strlen(buffer);
     vfprintf(&f, fmt, ap);
#else
     vsprintf(buffer+strlen(buffer), fmt, ap);
#endif
     buffer[LOG_SIZE-1] = '\0';
#ifdef DEBUG
     fprintf(stderr, buffer);
     if (flag)
	  fprintf(stderr, " : %s", sys_errlist[errno]);
     fprintf(stderr, ".\n");
#else
     syslog(LOG_WARNING, buffer);
#endif
     free(buffer);

}
