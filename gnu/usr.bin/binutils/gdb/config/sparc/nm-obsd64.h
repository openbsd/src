#ifndef NM_OBSD64_H
#define NM_OBSD64_H

#define SVR4_SHARED_LIBS

/* Before storing, read all the registers. (see inftarg.c) */
#define CHILD_PREPARE_TO_STORE() \
    read_register_bytes (0, NULL, REGISTER_BYTES)

/* Get generic OpenBSD native definitions. */
#include "nm-obsd.h"

#endif /* NM_OBSD64_H */
