/* Wrapper around broken system time.h.  */

#ifndef _PERL_WRAPPER_AROUND_TIME_H
# define _PERL_WRAPPER_AROUND_TIME_H 1

/* Recent versions of the MiNTLib have a macro HAS_TZNAME in 
   time.h resp. sys/time.h.  Wow, I wonder why they didn't
   define HAVE_CONFIG_H ...  */
#ifdef HAS_TZNAME 
# define PERL_HAS_TZNAME HAS_TZNAME
#endif

/* First include the system file.  */
#include_next <time.h> 

#ifdef HAS_TZNAME
# undef HAS_TZNAME
# define HAS_TZNAME PERL_HAS_TZNAME
#endif

#endif

