/* $OpenBSD: log.c,v 1.66 2026/10/03 01:31:30 deraadt Exp $	 */
/* $EOM: log.c,v 1.30 2000/09/29 08:19:23 niklas Exp $	 */

/*
 * Copyright (c) 1998, 1999, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 1999, 2000, 2001, 2003 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/types.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <unistd.h>

#include "conf.h"
#include "isakmp_num.h"
#include "log.h"
#include "monitor.h"
#include "util.h"

static void	_log_print(int, int, const char *, va_list, int, int);

static FILE	*log_output;

int		verbose_logging = 0;
static int	log_level[LOG_ENDCLASS];

void
log_init(int debug)
{
	if (debug)
		log_output = stderr;
	else
		log_to(0);	/* syslog */
}

void
log_reinit(void)
{
	struct conf_list *logging;
	struct conf_list_node *logclass;
	int		class, level;

	logging = conf_get_list("General", "Logverbose");
	if (logging) {
		verbose_logging = 1;
		conf_free_list(logging);
	}
	logging = conf_get_list("General", "Loglevel");
	if (!logging)
		return;

	for (logclass = TAILQ_FIRST(&logging->fields); logclass;
	    logclass = TAILQ_NEXT(logclass, link)) {
		if (sscanf(logclass->field, "%d=%d", &class, &level) != 2) {
			if (sscanf(logclass->field, "A=%d", &level) == 1)
				for (class = 0; class < LOG_ENDCLASS; class++)
					log_debug_cmd(class, level);
			else {
				log_print("init: invalid logging class or "
				    "level: %s", logclass->field);
				continue;
			}
		} else
			log_debug_cmd(class, level);
	}
	conf_free_list(logging);
}

void
log_to(FILE *f)
{
	if (!log_output && f)
		closelog();
	log_output = f;
	if (!f)
		openlog("isakmpd", LOG_PID | LOG_CONS, LOG_DAEMON);
}

FILE *
log_current(void)
{
	return log_output;
}

static char *
_log_get_class(int error_class)
{
	/* XXX For test purposes. To be removed later on?  */
	static char	*class_text[] = LOG_CLASSES_TEXT;

	if (error_class < 0)
		return "Dflt";
	else if (error_class >= LOG_ENDCLASS)
		return "Unkn";
	else
		return class_text[error_class];
}

static void
_log_print(int error, int syslog_level, const char *fmt, va_list ap,
    int class, int level)
{
	char		buffer[LOG_SIZE], nbuf[LOG_SIZE + 32];
	static const char fallback_msg[] =
	    "write to log file failed (errno %d), redirecting to syslog";
	int		len;
	struct tm      *tm;
	struct timeval  now;
	time_t          t;

	len = vsnprintf(buffer, sizeof buffer, fmt, ap);
	if (len > 0 && len < (int) sizeof buffer - 1 && error)
		snprintf(buffer + len, sizeof buffer - len, ": %s",
		    strerror(errno));
	if (log_output) {
		gettimeofday(&now, 0);
		t = now.tv_sec;
		if ((tm = localtime(&t)) == NULL) {
			/* Invalid time, use the epoch. */
			t = 0;
			tm = localtime(&t);
		}
		if (class >= 0)
			snprintf(nbuf, sizeof nbuf,
			    "%02d%02d%02d.%06ld %s %02d ",
			    tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec,
			    _log_get_class(class), level);
		else /* LOG_PRINT (-1) or LOG_REPORT (-2) */
			snprintf(nbuf, sizeof nbuf, "%02d%02d%02d.%06ld %s ",
			    tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec,
			    class == LOG_PRINT ? "Default" : "Report>");
		strlcat(nbuf, buffer, sizeof nbuf);
		strlcat(nbuf, getuid() ? "" : " [priv]", LOG_SIZE + 32);
		strlcat(nbuf, "\n", sizeof nbuf);

		if (fwrite(nbuf, strlen(nbuf), 1, log_output) == 0) {
			/* Report fallback.  */
			syslog(LOG_ALERT, fallback_msg, errno);
			fprintf(log_output, fallback_msg, errno);

			/*
			 * Close log_output to prevent isakmpd from locking
			 * the file.  We may need to explicitly close stdout
			 * to do this properly.
			 * XXX - Figure out how to match two FILE *'s and
			 * rewrite.
			 */
			if (fileno(log_output) != -1 &&
			    fileno(stdout) == fileno(log_output))
				fclose(stdout);
			fclose(log_output);

			/* Fallback to syslog.  */
			log_to(0);

			/* (Re)send current message to syslog().  */
			syslog(class == LOG_REPORT ? LOG_ALERT :
			    syslog_level, "%s", buffer);
		}
	} else
		syslog(class == LOG_REPORT ? LOG_ALERT : syslog_level, "%s",
		    buffer);
}

void
log_debug(int cls, int level, const char *fmt, ...)
{
	va_list         ap;

	/*
	 * If we are not debugging this class, or the level is too low, just
	 * return.
	 */
	if (cls >= 0 && (log_level[cls] == 0 || level > log_level[cls]))
		return;
	va_start(ap, fmt);
	_log_print(0, LOG_INFO, fmt, ap, cls, level);
	va_end(ap);
}

void
log_debug_buf(int cls, int level, const char *header, const u_int8_t *buf,
    size_t sz)
{
	size_t	i, j;
	char	s[73];

	/*
	 * If we are not debugging this class, or the level is too low, just
	 * return.
	 */
	if (cls >= 0 && (log_level[cls] == 0 || level > log_level[cls]))
		return;

	log_debug(cls, level, "%s:", header);
	for (i = j = 0; i < sz;) {
		snprintf(s + j, sizeof s - j, "%02x", buf[i++]);
		j += strlen(s + j);
		if (i % 4 == 0) {
			if (i % 32 == 0) {
				s[j] = '\0';
				log_debug(cls, level, "%s", s);
				j = 0;
			} else
				s[j++] = ' ';
		}
	}
	if (j) {
		s[j] = '\0';
		log_debug(cls, level, "%s", s);
	}
}

void
log_debug_cmd(int cls, int level)
{
	if (cls < 0 || cls >= LOG_ENDCLASS) {
		log_print("log_debug_cmd: invalid debugging class %d", cls);
		return;
	}
	if (level < 0) {
		log_print("log_debug_cmd: invalid debugging level %d for "
		    "class %d", level, cls);
		return;
	}
	if (level == log_level[cls])
		log_print("log_debug_cmd: log level unchanged for class %d",
		    cls);
	else {
		log_print("log_debug_cmd: log level changed from %d to %d "
		    "for class %d", log_level[cls], level, cls);
		log_level[cls] = level;
	}
}

void
log_debug_toggle(void)
{
	static int	log_level_copy[LOG_ENDCLASS], toggle = 0;

	if (!toggle) {
		LOG_DBG((LOG_MISC, 50, "log_debug_toggle: "
		    "debug levels cleared"));
		memcpy(&log_level_copy, &log_level, sizeof log_level);
		bzero(&log_level, sizeof log_level);
	} else {
		memcpy(&log_level, &log_level_copy, sizeof log_level);
		LOG_DBG((LOG_MISC, 50, "log_debug_toggle: "
		    "debug levels restored"));
	}
	toggle = !toggle;
}

void
log_print(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	_log_print(0, LOG_NOTICE, fmt, ap, LOG_PRINT, 0);
	va_end(ap);
}

void
log_verbose(const char *fmt, ...)
{
	va_list	ap;

	if (verbose_logging == 0)
		return;

	va_start(ap, fmt);
	_log_print(0, LOG_NOTICE, fmt, ap, LOG_PRINT, 0);
	va_end(ap);
}

void
log_error(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	_log_print(1, LOG_ERR, fmt, ap, LOG_PRINT, 0);
	va_end(ap);
}

void
log_errorx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_log_print(0, LOG_ERR, fmt, ap, LOG_PRINT, 0);
	va_end(ap);
}

void
log_fatal(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	_log_print(1, LOG_CRIT, fmt, ap, LOG_PRINT, 0);
	va_end(ap);
	monitor_exit(1);
}

void
log_fatalx(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	_log_print(0, LOG_CRIT, fmt, ap, LOG_PRINT, 0);
	va_end(ap);
	monitor_exit(1);
}
