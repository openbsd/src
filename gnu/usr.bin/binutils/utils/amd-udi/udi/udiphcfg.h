/* This file just picks the correct udiphxxx.h depending on the host */
/* The two hosts that are now defined are SUN and MSDOS */

#ifdef __hpux
#include "udiphsun.h"
#endif

#ifdef sun
#include "udiphsun.h"
#else

#ifdef MSDOS
#include "udiphdos.h"
#endif
#endif

