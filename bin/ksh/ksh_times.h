/*	$OpenBSD: ksh_times.h,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

#include <sys/times.h>

#ifdef TIMES_BROKEN
extern clock_t	ksh_times ARGS((struct tms *));
#else /* TIMES_BROKEN */
# define ksh_times times
#endif /* TIMES_BROKEN */

#ifdef HAVE_TIMES
extern clock_t	times ARGS((struct tms *));
#endif /* HAVE_TIMES */
