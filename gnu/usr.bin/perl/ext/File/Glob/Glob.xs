#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "bsd_glob.h"

#define MY_CXT_KEY "File::Glob::_guts" XS_VERSION

typedef struct {
    int		x_GLOB_ERROR;
} my_cxt_t;

START_MY_CXT

#define GLOB_ERROR	(MY_CXT.x_GLOB_ERROR)

#include "const-c.inc"

#ifdef WIN32
#define errfunc		NULL
#else
int
errfunc(const char *foo, int bar) {
  return !(bar == ENOENT || bar == ENOTDIR);
}
#endif

MODULE = File::Glob		PACKAGE = File::Glob

BOOT:
{
    MY_CXT_INIT;
}

void
doglob(pattern,...)
    char *pattern
PROTOTYPE: $;$
PREINIT:
    glob_t pglob;
    int i;
    int retval;
    int flags = 0;
    SV *tmp;
PPCODE:
    {
	dMY_CXT;

	/* allow for optional flags argument */
	if (items > 1) {
	    flags = (int) SvIV(ST(1));
	}

	/* call glob */
	retval = bsd_glob(pattern, flags, errfunc, &pglob);
	GLOB_ERROR = retval;

	/* return any matches found */
	EXTEND(sp, pglob.gl_pathc);
	for (i = 0; i < pglob.gl_pathc; i++) {
	    /* printf("# bsd_glob: %s\n", pglob.gl_pathv[i]); */
	    tmp = sv_2mortal(newSVpvn(pglob.gl_pathv[i],
				      strlen(pglob.gl_pathv[i])));
	    TAINT;
	    SvTAINT(tmp);
	    PUSHs(tmp);
	}

	bsd_globfree(&pglob);
    }

INCLUDE: const-xs.inc
