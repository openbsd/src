#if defined(_WIN32)
#  include <windows.h>
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef USE_PPPORT_H
#  include "ppport.h"
#endif

#ifndef HAVE_SYSLOG
#define HAVE_SYSLOG 1
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#  undef HAVE_SYSLOG
#  include "fallback/syslog.h"
#else
#  if defined(I_SYSLOG) || PATCHLEVEL < 6
#    include <syslog.h>
#  endif
#endif

static SV *ident_svptr;

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

#ifdef HAVE_SYSLOG

void
openlog_xs(ident, option, facility)
    INPUT:
        SV*   ident
        int   option
        int   facility
    PREINIT:
        STRLEN len;
        char*  ident_pv;
    CODE:
        ident_svptr = newSVsv(ident);
        ident_pv    = SvPV(ident_svptr, len);
        openlog(ident_pv, option, facility);

void
syslog_xs(priority, message)
    INPUT:
        int   priority
        const char * message
    CODE:
        syslog(priority, "%s", message);

int
setlogmask_xs(mask)
    INPUT:
        int mask
    CODE:
        RETVAL = setlogmask(mask);
    OUTPUT:
        RETVAL

void
closelog_xs()
    CODE:
        closelog();
        if (SvREFCNT(ident_svptr))
            SvREFCNT_dec(ident_svptr);

#else  /* HAVE_SYSLOG */

void
openlog_xs(ident, option, facility)
    INPUT:
        SV*   ident
        int   option
        int   facility
    CODE:

void
syslog_xs(priority, message)
    INPUT:
        int   priority
        const char * message
    CODE:

int
setlogmask_xs(mask)
    INPUT:
        int mask
    CODE:

void
closelog_xs()
    CODE:

#endif /* HAVE_SYSLOG */
