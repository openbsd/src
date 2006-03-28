/*    taint.c
 *
 *    Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "...we will have peace, when you and all your works have perished--and
 * the works of your dark master to whom you would deliver us.  You are a
 * liar, Saruman, and a corrupter of men's hearts."  --Theoden
 */

/* This file contains a few functions for handling data tainting in Perl
 */

#include "EXTERN.h"
#define PERL_IN_TAINT_C
#include "perl.h"

void
Perl_taint_proper(pTHX_ const char *f, const char *s)
{
#if defined(HAS_SETEUID) && defined(DEBUGGING)
#   if Uid_t_size == 1
    {
	const UV  uid = PL_uid;
	const UV euid = PL_euid;

	DEBUG_u(PerlIO_printf(Perl_debug_log,
			       "%s %d %"UVuf" %"UVuf"\n",
			       s, PL_tainted, uid, euid));
    }
#   else
    {
	const IV  uid = PL_uid;
	const IV euid = PL_euid;

	DEBUG_u(PerlIO_printf(Perl_debug_log,
			       "%s %d %"IVdf" %"IVdf"\n",
			       s, PL_tainted, uid, euid));
    }
#   endif
#endif

    if (PL_tainted) {
	const char *ug;

	if (!f)
	    f = PL_no_security;
	if (PL_euid != PL_uid)
	    ug = " while running setuid";
	else if (PL_egid != PL_gid)
	    ug = " while running setgid";
	else if (PL_taint_warn)
            ug = " while running with -t switch";
        else
	    ug = " while running with -T switch";
	if (PL_unsafe || PL_taint_warn) {
            if(ckWARN(WARN_TAINT))
                Perl_warner(aTHX_ packWARN(WARN_TAINT), f, s, ug);
        }
        else {
            Perl_croak(aTHX_ f, s, ug);
        }
    }
}

void
Perl_taint_env(pTHX)
{
    SV** svp;
    MAGIC* mg;
    const char* const *e;
    static const char* const misc_env[] = {
	"IFS",		/* most shells' inter-field separators */
	"CDPATH",	/* ksh dain bramage #1 */
	"ENV",		/* ksh dain bramage #2 */
	"BASH_ENV",	/* bash dain bramage -- I guess it's contagious */
	NULL
    };

    /* Don't bother if there's no *ENV glob */
    if (!PL_envgv)
	return;
    /* If there's no %ENV hash of if it's not magical, croak, because
     * it probably doesn't reflect the actual environment */
    if (!GvHV(PL_envgv) || !(SvRMAGICAL(GvHV(PL_envgv))
	    && mg_find((SV*)GvHV(PL_envgv), PERL_MAGIC_env))) {
	const bool was_tainted = PL_tainted;
	const char * const name = GvENAME(PL_envgv);
	PL_tainted = TRUE;
	if (strEQ(name,"ENV"))
	    /* hash alias */
	    taint_proper("%%ENV is aliased to %s%s", "another variable");
	else
	    /* glob alias: report it in the error message */
	    taint_proper("%%ENV is aliased to %%%s%s", name);
	/* this statement is reached under -t or -U */
	PL_tainted = was_tainted;
    }

#ifdef VMS
    {
    int i = 0;
    char name[10 + TYPE_DIGITS(int)] = "DCL$PATH";

    while (1) {
	if (i)
	    (void)sprintf(name,"DCL$PATH;%d", i);
	svp = hv_fetch(GvHVn(PL_envgv), name, strlen(name), FALSE);
	if (!svp || *svp == &PL_sv_undef)
	    break;
	if (SvTAINTED(*svp)) {
	    TAINT;
	    taint_proper("Insecure %s%s", "$ENV{DCL$PATH}");
	}
	if ((mg = mg_find(*svp, PERL_MAGIC_envelem)) && MgTAINTEDDIR(mg)) {
	    TAINT;
	    taint_proper("Insecure directory in %s%s", "$ENV{DCL$PATH}");
	}
	i++;
    }
  }
#endif /* VMS */

    svp = hv_fetch(GvHVn(PL_envgv),"PATH",4,FALSE);
    if (svp && *svp) {
	if (SvTAINTED(*svp)) {
	    TAINT;
	    taint_proper("Insecure %s%s", "$ENV{PATH}");
	}
	if ((mg = mg_find(*svp, PERL_MAGIC_envelem)) && MgTAINTEDDIR(mg)) {
	    TAINT;
	    taint_proper("Insecure directory in %s%s", "$ENV{PATH}");
	}
    }

#ifndef VMS
    /* tainted $TERM is okay if it contains no metachars */
    svp = hv_fetch(GvHVn(PL_envgv),"TERM",4,FALSE);
    if (svp && *svp && SvTAINTED(*svp)) {
	STRLEN len;
	const bool was_tainted = PL_tainted;
	const char *t = SvPV_const(*svp, len);
	const char * const e = t + len;
	PL_tainted = was_tainted;
	if (t < e && isALNUM(*t))
	    t++;
	while (t < e && (isALNUM(*t) || strchr("-_.+", *t)))
	    t++;
	if (t < e) {
	    TAINT;
	    taint_proper("Insecure $ENV{%s}%s", "TERM");
	}
    }
#endif /* !VMS */

    for (e = misc_env; *e; e++) {
	SV ** const svp = hv_fetch(GvHVn(PL_envgv), *e, strlen(*e), FALSE);
	if (svp && *svp != &PL_sv_undef && SvTAINTED(*svp)) {
	    TAINT;
	    taint_proper("Insecure $ENV{%s}%s", *e);
	}
    }
}

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
