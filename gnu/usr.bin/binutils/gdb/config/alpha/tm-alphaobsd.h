/* GDB target definitions for Alpha running OpenBSD. */

#include "alpha/tm-alpha.h"

#undef START_INFERIOR_TRAPS_EXPECTED
#define START_INFERIOR_TRAPS_EXPECTED 2

#undef NO_SINGLE_STEP
#define NO_SINGLE_STEP
#undef CANNOT_STEP_BREAKPOINT 
