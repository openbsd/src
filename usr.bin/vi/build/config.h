/*	$OpenBSD: config.h,v 1.11 2014/11/10 21:40:11 tedu Exp $	*/

/* Define if you want a debugging version. */
/* #undef DEBUG */

/* Define if you have fcntl(2) style locking. */
/* #undef HAVE_LOCK_FCNTL */

/* Define if you have flock(2) style locking. */
#define HAVE_LOCK_FLOCK 1
