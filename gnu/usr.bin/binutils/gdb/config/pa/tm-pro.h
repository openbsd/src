/* Parameters for execution on an HP PA-RISC level 0 embedded system.
   This is based on tm-hppab.h.
   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).  */

#define PA_LEVEL_0		/* Disables touching space regs and fp */

/* All the PRO targets use software floating point at the moment.  */
#define SOFT_FLOAT 1

/* It's mostly just the common stuff.  */
#include "pa/tm-hppa.h"

#define GDB_TARGET_IS_PA_ELF
