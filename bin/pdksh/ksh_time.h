/*	$OpenBSD: ksh_time.h,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

/* Wrapper around the ugly time.h,sys/time.h includes/ifdefs */

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else /* TIME_WITH_SYS_TIME */
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif /* TIME_WITH_SYS_TIME */

#ifndef TIME_DECLARED
extern time_t time ARGS((time_t *));
#endif

#ifndef CLK_TCK
# define CLK_TCK 60			/* 60HZ */
#endif
