#ifndef PERL_BEOS_BEOSISH_H
#define PERL_BEOS_BEOSISH_H

#include "../unixish.h"

#undef  waitpid
#define waitpid beos_waitpid

pid_t beos_waitpid(pid_t process_id, int *status_location, int options);

/* This seems to be protoless. */
char *gcvt(double value, int num_digits, char *buffer);

/* flock support, if available */
#ifdef HAS_FLOCK

#include <flock.h>

#undef close
#define close flock_close

#undef dup2
#define dup2 flock_dup2

#endif /* HAS_FLOCK */


#undef kill
#define kill beos_kill
int beos_kill(pid_t pid, int sig);

#undef sigaction
#define sigaction(sig, act, oact) beos_sigaction((sig), (act), (oact))
int beos_sigaction(int sig, const struct sigaction *act,
                   struct sigaction *oact);

#endif

