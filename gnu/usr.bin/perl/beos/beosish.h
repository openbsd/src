#ifndef PERL_BEOS_BEOSISH_H
#define PERL_BEOS_BEOSISH_H

#include "../unixish.h"

#undef  waitpid
#define waitpid beos_waitpid

pid_t beos_waitpid(pid_t process_id, int *status_location, int options);

/* This seems to be protoless. */
char *gcvt(double value, int num_digits, char *buffer);

#endif

