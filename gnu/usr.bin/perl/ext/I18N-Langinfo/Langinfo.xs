#define PERL_NO_GET_CONTEXT
#define PERL_EXT
#define PERL_EXT_LANGINFO

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef I_LANGINFO
#   define __USE_GNU 1 /* Enables YESSTR, otherwise only __YESSTR. */
#   include <langinfo.h>
#else
#   include <perl_langinfo.h>
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
        RETVAL = sv_langinfo(code);

  OUTPUT:
        RETVAL
