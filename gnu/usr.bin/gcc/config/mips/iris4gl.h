/* Like iris4-gdb.h, but always inhibits assembler optimization for MIPS as.
   Use this via mips-sgi-iris4loser if you need it.  */

/* Use stabs instead of ECOFF debug format.  */
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

#include "mips/iris4loser.h"
