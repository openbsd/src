/* Is the type time_t defined in <time.h>?  */
#undef HAVE_TIME_T_IN_TIME_H

/* Is the type time_t defined in <sys/types.h>?  */
#undef HAVE_TIME_T_IN_TYPES_H

/* Does <utime.h> define struct utimbuf?  */
#undef HAVE_GOOD_UTIME_H

/* Whether fprintf must be declared even if <stdio.h> is included.  */
#undef NEED_DECLARATION_FPRINTF

/* Do we need to use the b modifier when opening binary files?  */
#undef USE_BINARY_FOPEN
