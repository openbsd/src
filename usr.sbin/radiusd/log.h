/*	$OpenBSD: log.h,v 1.1 2015/07/21 04:06:04 yasuoka Exp $	*/

#ifndef _LOG_H
#define	_LOG_H	1

#include <sys/cdefs.h>
#include <stdarg.h>	/* for va_list */

extern int log_debug_use_syslog;

__BEGIN_DECLS
void		 log_init (int);
void		 logit(int, const char *, ...)
		    __attribute__((__format__ (__syslog__, 2, 3)));
void		 vlog(int, const char *, va_list)
		    __attribute__((__format__ (__syslog__, 2, 0)));
void		 log_warn(const char *, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		 log_warnx(const char *, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		 log_info(const char *, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		 log_debug(const char *, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		 fatal(const char *);
void		 fatalx(const char *);
__END_DECLS

#endif
