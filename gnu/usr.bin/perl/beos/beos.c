#include "beos/beosish.h"

#undef waitpid

#include <sys/wait.h>

/* In BeOS 5.0 the waitpid() seems to misbehave in that the status
 * has the upper and lower bytes swapped compared with the usual
 * POSIX/UNIX implementations.  To undo the surpise effect to the
 * rest of Perl we need this wrapper.  (The rest of BeOS might be
 * surprised because of this, though.) */

pid_t beos_waitpid(pid_t process_id, int *status_location, int options) {
    pid_t got = waitpid(process_id, status_location, options);
    if (status_location)
      *status_location =
	(*status_location & 0x00FF) << 8 |
	(*status_location & 0xFF00) >> 8;
    return got;
}
