/*    taint.c
 *
 *    Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *    2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * '...we will have peace, when you and all your works have perished--and
 *  the works of your dark master to whom you would deliver us.  You are a
 *  liar, Saruman, and a corrupter of men's hearts.'       --Théoden
 *
 *     [p.580 of _The Lord of the Rings_, III/x: "The Voice of Saruman"]
 */

/* This file contains a few functions for handling data tainting in Perl
 */

#include "EXTERN.h"
#define PERL_IN_TAINT_C
#include "perl.h"

void
Perl_taint_proper(pTHX_ const char *f, const char *const s)
{
#if defined(HAS_SETEUID) && defined(DEBUGGING)
    dVAR;

    PERL_ARGS_ASSERT_TAINT_PROPER;

    {
	const Uid_t  uid = PerlProc_getuid();
	const Uid_t euid = PerlProc_geteuid();

	DEBUG_u(PerlIO_printf(Perl_debug_log,
			       "%s %d %"Uid_t_f" %"Uid_t_f"\n",
			       s, TAINT_get, uid, euid));
    }
#endif

    if (TAINT_get) {
	const char *ug;

	if (!f)
	    f = PL_no_security;
	if (PerlProc_getuid() != PerlProc_geteuid())
	    ug = " while running setuid";
	else if (PerlProc_getgid() != PerlProc_getegid())
	    ug = " while running setgid";
	else if (TAINT_WARN_get)
            ug = " while running with -t switch";
        else
	    ug = " while running with -T switch";

        /* XXX because taint_proper adds extra format args, we can't
         * get the caller to check properly; o we just silence the warning
         * and hope the callers aren't naughty */
        GCC_DIAG_IGNORE(-Wformat-nonliteral);
	if (PL_unsafe || TAINT_WARN_get) {
	    Perl_ck_warner_d(aTHX_ packWARN(WARN_TAINT), f, s, ug);
        }
        else {
            Perl_croak(aTHX_ f, s, ug);
        }
        GCC_DIAG_RESTORE;

    }
}

void
Perl_taint_env(pTHX)
{
    dVAR;
    SV** svp;
    MAGIC* mg;
    const char* const *e;
    static const char* const misc_env[] = {
	"IFS",		/* most shells' inter-field separators */
	"CDPATH",	/* ksh dain bramage #1 */
	"ENV",		/* ksh dain bramage #2 */
	"BASH_ENV",	/* bash dain bramage -- I guess it's contagious */
#ifdef WIN32
	"PERL5SHELL",	/* used for system() on Windows */
#endif
	NULL
    };

    /* Don't bother if there's no *ENV glob */
    if (!PL_envgv)
	return;
    /* If there's no %ENV hash or if it's not magical, croak, because
     * it probably doesn't reflect the actual environment */
    if (!GvHV(PL_envgv) || !(SvRMAGICAL(GvHV(PL_envgv))
	    && mg_find((const SV *)GvHV(PL_envgv), PERL_MAGIC_env))) {
	const bool was_tainted = TAINT_get;
	const char * const name = GvENAME(PL_envgv);
	TAINT;
	if (strEQ(name,"ENV"))
	    /* hash alias */
	    taint_proper("%%ENV is aliased to %s%s", "another variable");
	else
	    /* glob alias: report it in the error message */
	    taint_proper("%%ENV is aliased to %%%s%s", name);
	/* this statement is reached under -t or -U */
	TAINT_set(was_tainted);
#ifdef NO_TAINT_SUPPORT
        PERL_UNUSED_VAR(was_tainted);
#endif
    }

#ifdef VMS
    {
    int i = 0;
    char name[10 + TYPE_DIGITS(int)] = "DCL$PATH";
    STRLEN len = 8; /* strlen(name)  */

    while (1) {
	if (i)
	    len = my_sprintf(name,"DCL$PATH;%d", i);
	svp = hv_fetch(GvHVn(PL_envgv), name, len, FALSE);
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

    svp = hv_fetchs(GvHVn(PL_envgv),"PATH",FALSE);
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
    svp = hv_fetchs(GvHVn(PL_envgv),"TERM",FALSE);
    if (svp && *svp && SvTAINTED(*svp)) {
	STRLEN len;
	const bool was_tainted = TAINT_get;
	const char *t = SvPV_const(*svp, len);
	const char * const e = t + len;

	TAINT_set(was_tainted);
#ifdef NO_TAINT_SUPPORT
        PERL_UNUSED_VAR(was_tainted);
#endif
	if (t < e && isWORDCHAR(*t))
	    t++;
	while (t < e && (isWORDCHAR(*t) || strchr("-_.+", *t)))
	    t++;
	if (t < e) {
	    TAINT;
	    taint_proper("Insecure $ENV{%s}%s", "TERM");
	}
    }
#endif /* !VMS */

    for (e = misc_env; *e; e++) {
	SV * const * const svp = hv_fetch(GvHVn(PL_envgv), *e, strlen(*e), FALSE);
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
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
