/* Beginning of modification history */
/* Written 02-08-13 by PG */
/* End of modification history */

/* This header conforms to IEEE Std 1003.1-2001 */

#ifndef   _INCLUDED_SYSLOG_H
#define   _INCLUDED_SYSLOG_H

/* values of the "logopt" option of openlog */

#define   LOG_PID         1
#define   LOG_CONS        2
#define   LOG_NDELAY      4
#define   LOG_ODELAY      8
#define   LOG_NOWAIT     16

/* values of the "facility" argument of openlog
      and of the "priority" argument of syslog */

#define   LOG_KERN    0
#define   LOG_USER    (1<<3)
#define   LOG_MAIL    (2<<3)
#define   LOG_NEWS    (3<<3)
#define   LOG_UUCP    (4<<3)
#define   LOG_DAEMON  (5<<3)
#define   LOG_AUTH    (6<<3)
#define   LOG_CRON    (7<<3)
#define   LOG_LPR     (8<<3)
#define   LOG_LOCAL0  (9<<3)
#define   LOG_LOCAL1 (10<<3)
#define   LOG_LOCAL2 (11<<3)
#define   LOG_LOCAL3 (12<<3)
#define   LOG_LOCAL4 (13<<3)
#define   LOG_LOCAL5 (14<<3)
#define   LOG_LOCAL6 (15<<3)
#define   LOG_LOCAL7 (16<<3)

/* macro for constructing "maskpri" arg to setlogmask */

#define   LOG_MASK(p) (1 << (p))

/* values of the "priority" argument of syslog */

#define   LOG_EMERG      0
#define   LOG_ALERT      1
#define   LOG_CRIT       2
#define   LOG_ERR        3
#define   LOG_WARNING    4
#define   LOG_NOTICE     5
#define   LOG_INFO       6
#define   LOG_DEBUG      7

#undef __P
#ifdef __PROTOTYPES__
#define __P(args) args
#else
#define __P(args) ()
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void    closelog __P((void));
extern void    openlog __P((const char *ident, int logopt,
                    int facility));
extern int     setlogmask __P((int maskpri));
extern void    syslog __P((int priority, const char * message, ...));

#ifdef __cplusplus
}
#endif

#endif /* _INCLUDED_SYSLOG_H */
