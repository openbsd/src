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
  CODE:
#ifdef HAS_NL_LANGINFO
	{
	  char *s;

	  if ((s = nl_langinfo(code)))
	      RETVAL = newSVpvn(s, strlen(s));
	  else
	      RETVAL = &PL_sv_undef;
	}
#else
	croak("nl_langinfo() not implemented on this architecture");
#endif
  OUTPUT:
	RETVAL
