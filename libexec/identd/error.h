/*	$OpenBSD: error.h,v 1.4 2002/07/16 10:25:47 deraadt Exp $*/

/*
**
** error.h						 Error handling macros
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 19 Aug 1992
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#ifndef __ERROR_H__
#define __ERROR_H__

#include <syslog.h>

#define ERROR(fmt) \
    ((syslog_flag ? (syslog(LOG_ERR, fmt),0) : 0), \
     (debug_flag ? (fprintf(stderr, "%d , %d : ERROR : X-DBG : ", \
			    lport, fport), \
		    fprintf(stderr, fmt), perror(": "), 0) : \
      (printf("%d , %d : ERROR : UNKNOWN-ERROR\r\n", lport, fport), 0)), \
     fflush(stdout), fflush(stderr), exit(1), 0)


#define ERROR1(fmt,v1) \
    ((syslog_flag ? (syslog(LOG_ERR, fmt, v1),0) : 0), \
     (debug_flag ? (fprintf(stderr, "%d , %d : ERROR : X-DBG : ", \
			    lport, fport), \
		    fprintf(stderr, fmt, v1), perror(": "), 0) : \
      (printf("%d , %d : ERROR : UNKNOWN-ERROR\r\n", lport, fport), 0)), \
     fflush(stdout), fflush(stderr), exit(1), 0)

#endif
