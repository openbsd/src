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
 * $OpenBSD: errlog.c,v 1.6 1998/07/10 20:27:42 provos Exp $
 */

#ifndef lint
static char rcsid[] = "$Id: errlog.c,v 1.6 1998/07/10 20:27:42 provos Exp $";
#endif

#define _ERRLOG_C_

#include <stdio.h>
#include <stdlib.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <errno.h>
#include "photuris.h"
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
#ifdef __STDC__
crit_error(int flag, char *fmt, ...)
#else
crit_error(flag, fmt, va_alist)
        int flag;
        char *fmt;
        va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
	fmt = va_arg (ap, char *);
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
#ifdef __STDC__
log_error(int flag, char *fmt, ...)
#else
log_error(flag, fmt, va_alist)
        int flag;
        char *fmt;
        va_dcl
#endif
{
     va_list ap;
#ifdef __STDC__
     va_start(ap, fmt);
#else
     va_start(ap);
     fmt = va_arg (ap, char *);
#endif
     _log_error(flag, fmt, ap);
     va_end(ap);
}

void
_log_error(int flag, char *fmt, va_list ap)
{
     char *buffer = calloc(LOG_SIZE, sizeof(char));

     if(buffer == NULL)
	  return;

     if (!daemon_mode)
	  sprintf(buffer, "%s: ", (flag ? "Error" : "Warning"));
     else
	  buffer[0] = '\0';

     vsnprintf(buffer+strlen(buffer), LOG_SIZE-1, fmt, ap);
     buffer[LOG_SIZE-1] = '\0';

     if (daemon_mode)
	  syslog(LOG_WARNING, buffer);
     else {
	  fprintf(stderr, buffer);
	  if (flag)
	       fprintf(stderr, " : %s", sys_errlist[errno]);
	  fprintf(stderr, ".\n");
     }
     free(buffer);

}
