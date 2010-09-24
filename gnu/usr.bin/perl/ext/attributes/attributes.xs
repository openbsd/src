/*    xsutils.c
 *
 *    Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'Perilous to us all are the devices of an art deeper than we possess
 *  ourselves.'                                            --Gandalf
 *
 *     [p.597 of _The Lord of the Rings_, III/xi: "The Palantír"]
 */


#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/*
 * Contributed by Spider Boardman (spider.boardman@orb.nashua.nh.us).
 */

static int
modify_SV_attributes(pTHX_ SV *sv, SV **retlist, SV **attrlist, int numattrs)
{
    dVAR;
    SV *attr;
    int nret;

    for (nret = 0 ; numattrs && (attr = *attrlist++); numattrs--) {
	STRLEN len;
	const char *name = SvPV_const(attr, len);
	const bool negated = (*name == '-');

	if (negated) {
	    name++;
	    len--;
	}
	switch (SvTYPE(sv)) {
	case SVt_PVCV:
	    switch ((int)len) {
	    case 6:
		switch (name[3]) {
		case 'l':
		    if (memEQ(name, "lvalue", 6)) {
			if (negated)
			    CvFLAGS(MUTABLE_CV(sv)) &= ~CVf_LVALUE;
			else
			    CvFLAGS(MUTABLE_CV(sv)) |= CVf_LVALUE;
			continue;
		    }
		    break;
		case 'h':
		    if (memEQ(name, "method", 6)) {
			if (negated)
			    CvFLAGS(MUTABLE_CV(sv)) &= ~CVf_METHOD;
			else
			    CvFLAGS(MUTABLE_CV(sv)) |= CVf_METHOD;
			continue;
		    }
		    break;
		}
		break;
	    }
	    break;
	default:
	    if (memEQs(name, 6, "shared")) {
			if (negated)
			    Perl_croak(aTHX_ "A variable may not be unshared");
			SvSHARE(sv);
                        continue;
	    }
	    break;
	}
	/* anything recognized had a 'continue' above */
	*retlist++ = attr;
	nret++;
    }

    return nret;
}

MODULE = attributes		PACKAGE = attributes

void
_modify_attrs(...)
  PREINIT:
    SV *rv, *sv;
  PPCODE:

    if (items < 1) {
usage:
	croak_xs_usage(cv, "@attributes");
    }

    rv = ST(0);
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);
    if (items > 1)
	XSRETURN(modify_SV_attributes(aTHX_ sv, &ST(0), &ST(1), items-1));

    XSRETURN(0);

void
_fetch_attrs(...)
  PROTOTYPE: $
  PREINIT:
    SV *rv, *sv;
    cv_flags_t cvflags;
  PPCODE:
    if (items != 1) {
usage:
	croak_xs_usage(cv, "$reference");
    }

    rv = ST(0);
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);

    switch (SvTYPE(sv)) {
    case SVt_PVCV:
	cvflags = CvFLAGS((const CV *)sv);
	if (cvflags & CVf_LVALUE)
	    XPUSHs(newSVpvs_flags("lvalue", SVs_TEMP));
	if (cvflags & CVf_METHOD)
	    XPUSHs(newSVpvs_flags("method", SVs_TEMP));
	break;
    default:
	break;
    }

    PUTBACK;

void
_guess_stash(...)
  PROTOTYPE: $
  PREINIT:
    SV *rv, *sv;
    dXSTARG;
  PPCODE:
    if (items != 1) {
usage:
	croak_xs_usage(cv, "$reference");
    }

    rv = ST(0);
    ST(0) = TARG;
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);

    if (SvOBJECT(sv))
	sv_setpvn(TARG, HvNAME_get(SvSTASH(sv)), HvNAMELEN_get(SvSTASH(sv)));
#if 0	/* this was probably a bad idea */
    else if (SvPADMY(sv))
	sv_setsv(TARG, &PL_sv_no);	/* unblessed lexical */
#endif
    else {
	const HV *stash = NULL;
	switch (SvTYPE(sv)) {
	case SVt_PVCV:
	    if (CvGV(sv) && isGV(CvGV(sv)) && GvSTASH(CvGV(sv)))
		stash = GvSTASH(CvGV(sv));
	    else if (/* !CvANON(sv) && */ CvSTASH(sv))
		stash = CvSTASH(sv);
	    break;
	case SVt_PVGV:
	    if (isGV_with_GP(sv) && GvGP(sv) && GvESTASH(MUTABLE_GV(sv)))
		stash = GvESTASH(MUTABLE_GV(sv));
	    break;
	default:
	    break;
	}
	if (stash)
	    sv_setpvn(TARG, HvNAME_get(stash), HvNAMELEN_get(stash));
    }

    SvSETMAGIC(TARG);
    XSRETURN(1);

void
reftype(...)
  PROTOTYPE: $
  PREINIT:
    SV *rv, *sv;
    dXSTARG;
  PPCODE:
    if (items != 1) {
usage:
	croak_xs_usage(cv, "$reference");
    }

    rv = ST(0);
    ST(0) = TARG;
    SvGETMAGIC(rv);
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);
    sv_setpv(TARG, sv_reftype(sv, 0));
    SvSETMAGIC(TARG);

    XSRETURN(1);
/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
