#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef I_SYSLOG
#include <syslog.h>
#endif

#include "const-c.inc"

MODULE = Sys::Syslog		PACKAGE = Sys::Syslog		

INCLUDE: const-xs.inc

int
LOG_FAC(p)
    INPUT:
	int		p
    CODE:
#ifdef LOG_FAC
	RETVAL = LOG_FAC(p);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_FAC");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL

int
LOG_PRI(p)
    INPUT:
	int		p
    CODE:
#ifdef LOG_PRI
	RETVAL = LOG_PRI(p);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_PRI");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL

int
LOG_MAKEPRI(fac,pri)
    INPUT:
	int		fac
	int		pri
    CODE:
#ifdef LOG_MAKEPRI
	RETVAL = LOG_MAKEPRI(fac,pri);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_MAKEPRI");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL

int
LOG_MASK(pri)
    INPUT:
	int		pri
    CODE:
#ifdef LOG_MASK
	RETVAL = LOG_MASK(pri);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_MASK");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL

int
LOG_UPTO(pri)
    INPUT:
	int		pri
    CODE:
#ifdef LOG_UPTO
	RETVAL = LOG_UPTO(pri);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_UPTO");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL
