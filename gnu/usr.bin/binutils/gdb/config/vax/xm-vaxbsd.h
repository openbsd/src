/* Definitions to make GDB run on a vax under BSD, 4.3 or 4.4. */

/* We have to include these files now, so that GDB will not make
   competing definitions in defs.h.  */
#include <machine/endian.h>
/* This should exist on either 4.3 or 4.4.  4.3 doesn't have limits.h
   or machine/limits.h.  */
#include <sys/param.h>

#include "vax/xm-vax.h"
