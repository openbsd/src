/*
 * Cygwin extras
 */

#include "EXTERN.h"
#include "perl.h"
#undef USE_DYNAMIC_LOADING
#include "XSUB.h"

#include <unistd.h>


/* see also Cwd.pm */
static
XS(Cygwin_cwd)
{
    dXSARGS;
    char *cwd;

    if(items != 0)
	Perl_croak(aTHX_ "Usage: Cwd::cwd()");
    if(cwd = getcwd(NULL, 0)) {
	ST(0) = sv_2mortal(newSVpv(cwd, 0));
	safesysfree(cwd);
	XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

void
init_os_extras(void)
{
    char *file = __FILE__;
    dTHX;

    newXS("Cwd::cwd", Cygwin_cwd, file);
}
