#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define INCL_BASE
#define INCL_REXXSAA
#include <os2emx.h>

static RXSTRING * strs;
static int	  nstrs;
static char *	  trace;

static void
needstrs(int n)
{
    if (n > nstrs) {
	if (strs)
	    free(strs);
	nstrs = 2 * n;
	strs = malloc(nstrs * sizeof(RXSTRING));
    }
}

MODULE = OS2::DLL		PACKAGE = OS2::DLL

BOOT:
    needstrs(8);
    trace = getenv("PERL_REXX_DEBUG");

SV *
_call(name, address, queue="SESSION", ...)
	char *		name
	void *		address
	char *		queue
 CODE:
   {
       ULONG	rc;
       int	argc, i;
       RXSTRING	result;
       UCHAR	resbuf[256];
       RexxFunctionHandler *fcn = address;
       argc = items-3;
       needstrs(argc);
       if (trace)
	   fprintf(stderr, "REXXCALL::_call name: '%s' args:", name);
       for (i = 0; i < argc; ++i) {
	   STRLEN len;
	   char *ptr = SvPV(ST(3+i), len);
	   MAKERXSTRING(strs[i], ptr, len);
	   if (trace)
	       fprintf(stderr, " '%.*s'", len, ptr);
       }
       if (!*queue)
	   queue = "SESSION";
       if (trace)
	   fprintf(stderr, "\n");
       MAKERXSTRING(result, resbuf, sizeof resbuf);
       rc = fcn(name, argc, strs, queue, &result);
       if (trace)
	   fprintf(stderr, "  rc=%X, result='%.*s'\n", rc,
		   result.strlength, result.strptr);
       ST(0) = sv_newmortal();
       if (rc == 0) {
	   if (result.strptr)
	       sv_setpvn(ST(0), result.strptr, result.strlength);
	   else
	       sv_setpvn(ST(0), "", 0);
       }
       if (result.strptr && result.strptr != resbuf)
	   DosFreeMem(result.strptr);
   }

