/*	$OpenBSD: ksh_times.h,v 1.2 1996/10/01 02:05:41 downsj Exp $	*/

#ifndef KSH_TIMES_H
# define KSH_TIMES_H

/* Needed for clock_t on some systems (ie, NeXT in non-posix mode) */
#include "ksh_time.h"

#include <sys/times.h>

#ifdef TIMES_BROKEN
extern clock_t	ksh_times ARGS((struct tms *));
#else /* TIMES_BROKEN */
# define ksh_times times
#endif /* TIMES_BROKEN */

#ifdef HAVE_TIMES
extern clock_t	times ARGS((struct tms *));
#endif /* HAVE_TIMES */
#endif /* KSH_TIMES_H */
