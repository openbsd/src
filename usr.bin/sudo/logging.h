/*
 * Copyright (c) 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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

#ifndef _LOGGING_H
#define _LOGGING_H

#include <syslog.h>
#ifdef __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

/* Logging types */
#define SLOG_SYSLOG		0x01
#define SLOG_FILE		0x02
#define SLOG_BOTH		0x03

/* Flags for log_error() */
#define MSG_ONLY		0x01
#define USE_ERRNO		0x02
#define NO_MAIL			0x04
#define NO_EXIT			0x08

/*
 * Maximum number of characters to log per entry.  The syslogger
 * will log this much, after that, it truncates the log line.
 * We need this here to make sure that we continue with another   
 * syslog(3) call if the internal buffer is more than 1023 characters.
 */
#ifndef MAXSYSLOGLEN
# define MAXSYSLOGLEN		960
#endif
      
void log_auth			__P((int, int));
void log_error			__P((int flags, const char *fmt, ...));
RETSIGTYPE reapchild		__P((int));

#endif /* _LOGGING_H */
