#define PERL_NO_GET_CONTEXT

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef I_LANGINFO
#   define __USE_GNU 1 /* Enables YESSTR, otherwise only __YESSTR. */
#   include <langinfo.h>
#endif

#include "const-c.inc"

MODULE = I18N::Langinfo	PACKAGE = I18N::Langinfo

PROTOTYPES: ENABLE

INCLUDE: const-xs.inc

SV*
langinfo(code)
	int	code
  PROTOTYPE: _
  CODE:
#ifdef HAS_NL_LANGINFO
	RETVAL = newSVpv(nl_langinfo(code), 0);
#else
	croak("nl_langinfo() not implemented on this architecture");
#endif
  OUTPUT:
	RETVAL
