#ifndef NM_OBSD64_H
#define NM_OBSD64_H

/* Get generic OpenBSD native definitions. */
#include "nm-obsd.h"

/* Before storing, read all the registers. (see inftarg.c) */
#define CHILD_PREPARE_TO_STORE() \
    read_register_bytes (0, NULL, REGISTER_BYTES)

/* Compensate for stack bias. */

#define TARGET_READ_SP() \
    sparc64_read_sp()

#define TARGET_READ_FP() \
    sparc64_read_fp()

#define TARGET_WRITE_SP(val) \
    sparc64_write_sp(val)

#define TARGET_WRITE_FP(val) \
    sparc64_write_fp(val)

#endif /* NM_OBSD64_H */
