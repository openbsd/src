/* Definitions to make GDB run on a vax under Ultrix. */

#include "vax/xm-vax.h"
#define NO_PTRACE_H

/* Old versions of ultrix have fd_set but not the FD_* macros.  */

#define FD_SET(bit,fdsetp) ((fdsetp)->fds_bits[(n) / 32] |= (1 << ((n) % 32)))
#define FD_ZERO(fdsetp) memset (fdsetp, 0, sizeof (*fdsetp))
