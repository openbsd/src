/*
 * "...we will have peace, when you and all your works have perished--and
 * the works of your dark master to whom you would deliver us.  You are a
 * liar, Saruman, and a corrupter of men's hearts."  --Theoden
 */

#include "EXTERN.h"
#include "perl.h"

void
taint_proper(f, s)
const char *f;
char *s;
{
    char *ug;

    DEBUG_u(PerlIO_printf(Perl_debug_log,
            "%s %d %d %d\n", s, tainted, uid, euid));

    if (tainted) {
	if (euid != uid)
	    ug = " while running setuid";
	else if (egid != gid)
	    ug = " while running setgid";
	else
	    ug = " while running with -T switch";
	if (!unsafe)
	    croak(f, s, ug);
	else if (dowarn)
	    warn(f, s, ug);
    }
}

void
taint_env()
{
    SV** svp;
    MAGIC* mg;
    char** e;
    static char* misc_env[] = {
	"IFS",		/* most shells' inter-field separators */
	"CDPATH",	/* ksh dain bramage #1 */
	"ENV",		/* ksh dain bramage #2 */
	"BASH_ENV",	/* bash dain bramage -- I guess it's contagious */
	NULL
    };

    if (!envgv)
	return;

#ifdef VMS
    int i = 0;
    char name[10 + TYPE_DIGITS(int)] = "DCL$PATH";

    while (1) {
	if (i)
	    (void)sprintf(name,"DCL$PATH;%d", i);
	svp = hv_fetch(GvHVn(envgv), name, strlen(name), FALSE);
	if (!svp || *svp == &sv_undef)
	    break;
	if (SvTAINTED(*svp)) {
	    TAINT;
	    taint_proper("Insecure %s%s", "$ENV{DCL$PATH}");
	}
	if ((mg = mg_find(*svp, 'e')) && MgTAINTEDDIR(mg)) {
	    TAINT;
	    taint_proper("Insecure directory in %s%s", "$ENV{DCL$PATH}");
	}
	i++;
    }
#endif /* VMS */

    svp = hv_fetch(GvHVn(envgv),"PATH",4,FALSE);
    if (svp && *svp) {
	if (SvTAINTED(*svp)) {
	    TAINT;
	    taint_proper("Insecure %s%s", "$ENV{PATH}");
	}
	if ((mg = mg_find(*svp, 'e')) && MgTAINTEDDIR(mg)) {
	    TAINT;
	    taint_proper("Insecure directory in %s%s", "$ENV{PATH}");
	}
    }

#ifndef VMS
    /* tainted $TERM is okay if it contains no metachars */
    svp = hv_fetch(GvHVn(envgv),"TERM",4,FALSE);
    if (svp && *svp && SvTAINTED(*svp)) {
	bool was_tainted = tainted;
	char *t = SvPV(*svp, na);
	char *e = t + na;
	tainted = was_tainted;
	if (t < e && isALNUM(*t))
	    t++;
	while (t < e && (isALNUM(*t) || *t == '-' || *t == ':'))
	    t++;
	if (t < e) {
	    TAINT;
	    taint_proper("Insecure $ENV{%s}%s", "TERM");
	}
    }
#endif /* !VMS */

    for (e = misc_env; *e; e++) {
	svp = hv_fetch(GvHVn(envgv), *e, strlen(*e), FALSE);
	if (svp && *svp != &sv_undef && SvTAINTED(*svp)) {
	    TAINT;
	    taint_proper("Insecure $ENV{%s}%s", *e);
	}
    }
}
