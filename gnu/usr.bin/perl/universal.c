/*    universal.c
 *
 *    Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
 *    2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * '"The roots of those mountains must be roots indeed; there must be
 *   great secrets buried there which have not been discovered since the
 *   beginning."'                   --Gandalf, relating Gollum's history
 *
 *     [p.54 of _The Lord of the Rings_, I/ii: "The Shadow of the Past"]
 */

/* This file contains the code that implements the functions in Perl's
 * UNIVERSAL package, such as UNIVERSAL->can().
 *
 * It is also used to store XS functions that need to be present in
 * miniperl for a lack of a better place to put them. It might be
 * clever to move them to seperate XS files which would then be pulled
 * in by some to-be-written build process.
 */

#include "EXTERN.h"
#define PERL_IN_UNIVERSAL_C
#include "perl.h"

#ifdef USE_PERLIO
#include "perliol.h" /* For the PERLIO_F_XXX */
#endif

static HV *
S_get_isa_hash(pTHX_ HV *const stash)
{
    dVAR;
    struct mro_meta *const meta = HvMROMETA(stash);

    PERL_ARGS_ASSERT_GET_ISA_HASH;

    if (!meta->isa) {
	AV *const isa = mro_get_linear_isa(stash);
	if (!meta->isa) {
	    HV *const isa_hash = newHV();
	    /* Linearisation didn't build it for us, so do it here.  */
	    SV *const *svp = AvARRAY(isa);
	    SV *const *const svp_end = svp + AvFILLp(isa) + 1;
	    const HEK *const canon_name = HvNAME_HEK(stash);

	    while (svp < svp_end) {
		(void) hv_store_ent(isa_hash, *svp++, &PL_sv_undef, 0);
	    }

	    (void) hv_common(isa_hash, NULL, HEK_KEY(canon_name),
			     HEK_LEN(canon_name), HEK_FLAGS(canon_name),
			     HV_FETCH_ISSTORE, &PL_sv_undef,
			     HEK_HASH(canon_name));
	    (void) hv_store(isa_hash, "UNIVERSAL", 9, &PL_sv_undef, 0);

	    SvREADONLY_on(isa_hash);

	    meta->isa = isa_hash;
	}
    }
    return meta->isa;
}

/*
 * Contributed by Graham Barr  <Graham.Barr@tiuk.ti.com>
 * The main guts of traverse_isa was actually copied from gv_fetchmeth
 */

STATIC bool
S_isa_lookup(pTHX_ HV *stash, const char * const name)
{
    dVAR;
    const struct mro_meta *const meta = HvMROMETA(stash);
    HV *const isa = meta->isa ? meta->isa : S_get_isa_hash(aTHX_ stash);
    STRLEN len = strlen(name);
    const HV *our_stash;

    PERL_ARGS_ASSERT_ISA_LOOKUP;

    if (hv_common(isa, NULL, name, len, 0 /* No "UTF-8" flag possible with only
					     a char * argument*/,
		  HV_FETCH_ISEXISTS, NULL, 0)) {
	/* Direct name lookup worked.  */
	return TRUE;
    }

    /* A stash/class can go by many names (ie. User == main::User), so 
       we use the name in the stash itself, which is canonical.  */
    our_stash = gv_stashpvn(name, len, 0);

    if (our_stash) {
	HEK *const canon_name = HvNAME_HEK(our_stash);

	if (hv_common(isa, NULL, HEK_KEY(canon_name), HEK_LEN(canon_name),
		      HEK_FLAGS(canon_name),
		      HV_FETCH_ISEXISTS, NULL, HEK_HASH(canon_name))) {
	    return TRUE;
	}
    }

    return FALSE;
}

/*
=head1 SV Manipulation Functions

=for apidoc sv_derived_from

Returns a boolean indicating whether the SV is derived from the specified class
I<at the C level>.  To check derivation at the Perl level, call C<isa()> as a
normal Perl method.

=cut
*/

bool
Perl_sv_derived_from(pTHX_ SV *sv, const char *const name)
{
    dVAR;
    HV *stash;

    PERL_ARGS_ASSERT_SV_DERIVED_FROM;

    SvGETMAGIC(sv);

    if (SvROK(sv)) {
	const char *type;
        sv = SvRV(sv);
        type = sv_reftype(sv,0);
	if (type && strEQ(type,name))
	    return TRUE;
	stash = SvOBJECT(sv) ? SvSTASH(sv) : NULL;
    }
    else {
        stash = gv_stashsv(sv, 0);
    }

    return stash ? isa_lookup(stash, name) : FALSE;
}

/*
=for apidoc sv_does

Returns a boolean indicating whether the SV performs a specific, named role.
The SV can be a Perl object or the name of a Perl class.

=cut
*/

#include "XSUB.h"

bool
Perl_sv_does(pTHX_ SV *sv, const char *const name)
{
    const char *classname;
    bool does_it;
    SV *methodname;
    dSP;

    PERL_ARGS_ASSERT_SV_DOES;

    ENTER;
    SAVETMPS;

    SvGETMAGIC(sv);

    if (!SvOK(sv) || !(SvROK(sv) || (SvPOK(sv) && SvCUR(sv))
	    || (SvGMAGICAL(sv) && SvPOKp(sv) && SvCUR(sv)))) {
	LEAVE;
	return FALSE;
    }

    if (sv_isobject(sv)) {
	classname = sv_reftype(SvRV(sv),TRUE);
    } else {
	classname = SvPV_nolen(sv);
    }

    if (strEQ(name,classname)) {
	LEAVE;
	return TRUE;
    }

    PUSHMARK(SP);
    XPUSHs(sv);
    mXPUSHs(newSVpv(name, 0));
    PUTBACK;

    methodname = newSVpvs_flags("isa", SVs_TEMP);
    /* ugly hack: use the SvSCREAM flag so S_method_common
     * can figure out we're calling DOES() and not isa(),
     * and report eventual errors correctly. --rgs */
    SvSCREAM_on(methodname);
    call_sv(methodname, G_SCALAR | G_METHOD);
    SPAGAIN;

    does_it = SvTRUE( TOPs );
    FREETMPS;
    LEAVE;

    return does_it;
}

PERL_XS_EXPORT_C void XS_UNIVERSAL_isa(pTHX_ CV *cv);
PERL_XS_EXPORT_C void XS_UNIVERSAL_can(pTHX_ CV *cv);
PERL_XS_EXPORT_C void XS_UNIVERSAL_DOES(pTHX_ CV *cv);
PERL_XS_EXPORT_C void XS_UNIVERSAL_VERSION(pTHX_ CV *cv);
XS(XS_version_new);
XS(XS_version_stringify);
XS(XS_version_numify);
XS(XS_version_normal);
XS(XS_version_vcmp);
XS(XS_version_boolean);
#ifdef HASATTRIBUTE_NORETURN
XS(XS_version_noop) __attribute__noreturn__;
#else
XS(XS_version_noop);
#endif
XS(XS_version_is_alpha);
XS(XS_version_qv);
XS(XS_version_is_qv);
XS(XS_utf8_is_utf8);
XS(XS_utf8_valid);
XS(XS_utf8_encode);
XS(XS_utf8_decode);
XS(XS_utf8_upgrade);
XS(XS_utf8_downgrade);
XS(XS_utf8_unicode_to_native);
XS(XS_utf8_native_to_unicode);
XS(XS_Internals_SvREADONLY);
XS(XS_Internals_SvREFCNT);
XS(XS_Internals_hv_clear_placehold);
XS(XS_PerlIO_get_layers);
XS(XS_Internals_hash_seed);
XS(XS_Internals_rehash_seed);
XS(XS_Internals_HvREHASH);
XS(XS_re_is_regexp); 
XS(XS_re_regname);
XS(XS_re_regnames);
XS(XS_re_regnames_count);
XS(XS_re_regexp_pattern);
XS(XS_Tie_Hash_NamedCapture_FETCH);
XS(XS_Tie_Hash_NamedCapture_STORE);
XS(XS_Tie_Hash_NamedCapture_DELETE);
XS(XS_Tie_Hash_NamedCapture_CLEAR);
XS(XS_Tie_Hash_NamedCapture_EXISTS);
XS(XS_Tie_Hash_NamedCapture_FIRSTK);
XS(XS_Tie_Hash_NamedCapture_NEXTK);
XS(XS_Tie_Hash_NamedCapture_SCALAR);
XS(XS_Tie_Hash_NamedCapture_flags);

void
Perl_boot_core_UNIVERSAL(pTHX)
{
    dVAR;
    static const char file[] = __FILE__;

    newXS("UNIVERSAL::isa",             XS_UNIVERSAL_isa,         file);
    newXS("UNIVERSAL::can",             XS_UNIVERSAL_can,         file);
    newXS("UNIVERSAL::DOES",            XS_UNIVERSAL_DOES,        file);
    newXS("UNIVERSAL::VERSION", 	XS_UNIVERSAL_VERSION, 	  file);
    {
	/* register the overloading (type 'A') magic */
	PL_amagic_generation++;
	/* Make it findable via fetchmethod */
	newXS("version::()", XS_version_noop, file);
	newXS("version::new", XS_version_new, file);
	newXS("version::parse", XS_version_new, file);
	newXS("version::(\"\"", XS_version_stringify, file);
	newXS("version::stringify", XS_version_stringify, file);
	newXS("version::(0+", XS_version_numify, file);
	newXS("version::numify", XS_version_numify, file);
	newXS("version::normal", XS_version_normal, file);
	newXS("version::(cmp", XS_version_vcmp, file);
	newXS("version::(<=>", XS_version_vcmp, file);
	newXS("version::vcmp", XS_version_vcmp, file);
	newXS("version::(bool", XS_version_boolean, file);
	newXS("version::boolean", XS_version_boolean, file);
	newXS("version::(nomethod", XS_version_noop, file);
	newXS("version::noop", XS_version_noop, file);
	newXS("version::is_alpha", XS_version_is_alpha, file);
	newXS("version::qv", XS_version_qv, file);
	newXS("version::declare", XS_version_qv, file);
	newXS("version::is_qv", XS_version_is_qv, file);
    }
    newXS("utf8::is_utf8", XS_utf8_is_utf8, file);
    newXS("utf8::valid", XS_utf8_valid, file);
    newXS("utf8::encode", XS_utf8_encode, file);
    newXS("utf8::decode", XS_utf8_decode, file);
    newXS("utf8::upgrade", XS_utf8_upgrade, file);
    newXS("utf8::downgrade", XS_utf8_downgrade, file);
    newXS("utf8::native_to_unicode", XS_utf8_native_to_unicode, file);
    newXS("utf8::unicode_to_native", XS_utf8_unicode_to_native, file);
    newXSproto("Internals::SvREADONLY",XS_Internals_SvREADONLY, file, "\\[$%@];$");
    newXSproto("Internals::SvREFCNT",XS_Internals_SvREFCNT, file, "\\[$%@];$");
    newXSproto("Internals::hv_clear_placeholders",
               XS_Internals_hv_clear_placehold, file, "\\%");
    newXSproto("PerlIO::get_layers",
               XS_PerlIO_get_layers, file, "*;@");
    /* Providing a Regexp::DESTROY fixes #21347. See test in t/op/ref.t  */
    CvFILE(newCONSTSUB(get_hv("Regexp::", GV_ADD), "DESTROY", NULL))
	= (char *)file;
    newXSproto("Internals::hash_seed",XS_Internals_hash_seed, file, "");
    newXSproto("Internals::rehash_seed",XS_Internals_rehash_seed, file, "");
    newXSproto("Internals::HvREHASH", XS_Internals_HvREHASH, file, "\\%");
    newXSproto("re::is_regexp", XS_re_is_regexp, file, "$");
    newXSproto("re::regname", XS_re_regname, file, ";$$");
    newXSproto("re::regnames", XS_re_regnames, file, ";$");
    newXSproto("re::regnames_count", XS_re_regnames_count, file, "");
    newXSproto("re::regexp_pattern", XS_re_regexp_pattern, file, "$");
    newXS("Tie::Hash::NamedCapture::FETCH", XS_Tie_Hash_NamedCapture_FETCH, file);
    newXS("Tie::Hash::NamedCapture::STORE", XS_Tie_Hash_NamedCapture_STORE, file);
    newXS("Tie::Hash::NamedCapture::DELETE", XS_Tie_Hash_NamedCapture_DELETE, file);
    newXS("Tie::Hash::NamedCapture::CLEAR", XS_Tie_Hash_NamedCapture_CLEAR, file);
    newXS("Tie::Hash::NamedCapture::EXISTS", XS_Tie_Hash_NamedCapture_EXISTS, file);
    newXS("Tie::Hash::NamedCapture::FIRSTKEY", XS_Tie_Hash_NamedCapture_FIRSTK, file);
    newXS("Tie::Hash::NamedCapture::NEXTKEY", XS_Tie_Hash_NamedCapture_NEXTK, file);
    newXS("Tie::Hash::NamedCapture::SCALAR", XS_Tie_Hash_NamedCapture_SCALAR, file);
    newXS("Tie::Hash::NamedCapture::flags", XS_Tie_Hash_NamedCapture_flags, file);
}

/*
=for apidoc croak_xs_usage

A specialised variant of C<croak()> for emitting the usage message for xsubs

    croak_xs_usage(cv, "eee_yow");

works out the package name and subroutine name from C<cv>, and then calls
C<croak()>. Hence if C<cv> is C<&ouch::awk>, it would call C<croak> as:

    Perl_croak(aTHX_ "Usage %s::%s(%s)", "ouch" "awk", "eee_yow");

=cut
*/

void
Perl_croak_xs_usage(pTHX_ const CV *const cv, const char *const params)
{
    const GV *const gv = CvGV(cv);

    PERL_ARGS_ASSERT_CROAK_XS_USAGE;

    if (gv) {
	const char *const gvname = GvNAME(gv);
	const HV *const stash = GvSTASH(gv);
	const char *const hvname = stash ? HvNAME_get(stash) : NULL;

	if (hvname)
	    Perl_croak(aTHX_ "Usage: %s::%s(%s)", hvname, gvname, params);
	else
	    Perl_croak(aTHX_ "Usage: %s(%s)", gvname, params);
    } else {
	/* Pants. I don't think that it should be possible to get here. */
	Perl_croak(aTHX_ "Usage: CODE(0x%"UVxf")(%s)", PTR2UV(cv), params);
    }
}

XS(XS_UNIVERSAL_isa)
{
    dVAR;
    dXSARGS;

    if (items != 2)
	croak_xs_usage(cv, "reference, kind");
    else {
	SV * const sv = ST(0);
	const char *name;

	SvGETMAGIC(sv);

	if (!SvOK(sv) || !(SvROK(sv) || (SvPOK(sv) && SvCUR(sv))
		    || (SvGMAGICAL(sv) && SvPOKp(sv) && SvCUR(sv))))
	    XSRETURN_UNDEF;

	name = SvPV_nolen_const(ST(1));

	ST(0) = boolSV(sv_derived_from(sv, name));
	XSRETURN(1);
    }
}

XS(XS_UNIVERSAL_can)
{
    dVAR;
    dXSARGS;
    SV   *sv;
    const char *name;
    SV   *rv;
    HV   *pkg = NULL;

    if (items != 2)
	croak_xs_usage(cv, "object-ref, method");

    sv = ST(0);

    SvGETMAGIC(sv);

    if (!SvOK(sv) || !(SvROK(sv) || (SvPOK(sv) && SvCUR(sv))
		|| (SvGMAGICAL(sv) && SvPOKp(sv) && SvCUR(sv))))
	XSRETURN_UNDEF;

    name = SvPV_nolen_const(ST(1));
    rv = &PL_sv_undef;

    if (SvROK(sv)) {
        sv = MUTABLE_SV(SvRV(sv));
        if (SvOBJECT(sv))
            pkg = SvSTASH(sv);
    }
    else {
        pkg = gv_stashsv(sv, 0);
    }

    if (pkg) {
	GV * const gv = gv_fetchmethod_autoload(pkg, name, FALSE);
        if (gv && isGV(gv))
	    rv = sv_2mortal(newRV(MUTABLE_SV(GvCV(gv))));
    }

    ST(0) = rv;
    XSRETURN(1);
}

XS(XS_UNIVERSAL_DOES)
{
    dVAR;
    dXSARGS;
    PERL_UNUSED_ARG(cv);

    if (items != 2)
	Perl_croak(aTHX_ "Usage: invocant->DOES(kind)");
    else {
	SV * const sv = ST(0);
	const char *name;

	name = SvPV_nolen_const(ST(1));
	if (sv_does( sv, name ))
	    XSRETURN_YES;

	XSRETURN_NO;
    }
}

XS(XS_UNIVERSAL_VERSION)
{
    dVAR;
    dXSARGS;
    HV *pkg;
    GV **gvp;
    GV *gv;
    SV *sv;
    const char *undef;
    PERL_UNUSED_ARG(cv);

    if (SvROK(ST(0))) {
        sv = MUTABLE_SV(SvRV(ST(0)));
        if (!SvOBJECT(sv))
            Perl_croak(aTHX_ "Cannot find version of an unblessed reference");
        pkg = SvSTASH(sv);
    }
    else {
        pkg = gv_stashsv(ST(0), 0);
    }

    gvp = pkg ? (GV**)hv_fetchs(pkg, "VERSION", FALSE) : NULL;

    if (gvp && isGV(gv = *gvp) && (sv = GvSV(gv)) && SvOK(sv)) {
        SV * const nsv = sv_newmortal();
        sv_setsv(nsv, sv);
        sv = nsv;
	if ( !sv_derived_from(sv, "version"))
	    upg_version(sv, FALSE);
        undef = NULL;
    }
    else {
        sv = &PL_sv_undef;
        undef = "(undef)";
    }

    if (items > 1) {
	SV *req = ST(1);

	if (undef) {
	    if (pkg) {
		const char * const name = HvNAME_get(pkg);
		Perl_croak(aTHX_
			   "%s does not define $%s::VERSION--version check failed",
			   name, name);
	    } else {
		Perl_croak(aTHX_
			     "%s defines neither package nor VERSION--version check failed",
			     SvPVx_nolen_const(ST(0)) );
	     }
	}

	if ( !sv_derived_from(req, "version")) {
	    /* req may very well be R/O, so create a new object */
	    req = sv_2mortal( new_version(req) );
	}

	if ( vcmp( req, sv ) > 0 ) {
	    if ( hv_exists(MUTABLE_HV(SvRV(req)), "qv", 2 ) ) {
		Perl_croak(aTHX_ "%s version %"SVf" required--"
		       "this is only version %"SVf"", HvNAME_get(pkg),
		       SVfARG(vnormal(req)),
		       SVfARG(vnormal(sv)));
	    } else {
		Perl_croak(aTHX_ "%s version %"SVf" required--"
		       "this is only version %"SVf"", HvNAME_get(pkg),
		       SVfARG(vstringify(req)),
		       SVfARG(vstringify(sv)));
	    }
	}

    }

    if ( SvOK(sv) && sv_derived_from(sv, "version") ) {
	ST(0) = vstringify(sv);
    } else {
	ST(0) = sv;
    }

    XSRETURN(1);
}

XS(XS_version_new)
{
    dVAR;
    dXSARGS;
    if (items > 3)
	croak_xs_usage(cv, "class, version");
    SP -= items;
    {
        SV *vs = ST(1);
	SV *rv;
	const char * const classname =
	    sv_isobject(ST(0)) /* get the class if called as an object method */
		? HvNAME(SvSTASH(SvRV(ST(0))))
		: (char *)SvPV_nolen(ST(0));

	if ( items == 1 || ! SvOK(vs) ) { /* no param or explicit undef */
	    /* create empty object */
	    vs = sv_newmortal();
	    sv_setpvs(vs, "0");
	}
	else if ( items == 3 ) {
	    vs = sv_newmortal();
	    Perl_sv_setpvf(aTHX_ vs,"v%s",SvPV_nolen_const(ST(2)));
	}

	rv = new_version(vs);
	if ( strcmp(classname,"version") != 0 ) /* inherited new() */
	    sv_bless(rv, gv_stashpv(classname, GV_ADD));

	mPUSHs(rv);
	PUTBACK;
	return;
    }
}

XS(XS_version_stringify)
{
     dVAR;
     dXSARGS;
     if (items < 1)
	 croak_xs_usage(cv, "lobj, ...");
     SP -= items;
     {
	  SV *	lobj;

	  if (sv_derived_from(ST(0), "version")) {
	       lobj = SvRV(ST(0));
	  }
	  else
	       Perl_croak(aTHX_ "lobj is not of type version");

	  mPUSHs(vstringify(lobj));

	  PUTBACK;
	  return;
     }
}

XS(XS_version_numify)
{
     dVAR;
     dXSARGS;
     if (items < 1)
	 croak_xs_usage(cv, "lobj, ...");
     SP -= items;
     {
	  SV *	lobj;

	  if (sv_derived_from(ST(0), "version")) {
	       lobj = SvRV(ST(0));
	  }
	  else
	       Perl_croak(aTHX_ "lobj is not of type version");

	  mPUSHs(vnumify(lobj));

	  PUTBACK;
	  return;
     }
}

XS(XS_version_normal)
{
     dVAR;
     dXSARGS;
     if (items < 1)
	 croak_xs_usage(cv, "lobj, ...");
     SP -= items;
     {
	  SV *	lobj;

	  if (sv_derived_from(ST(0), "version")) {
	       lobj = SvRV(ST(0));
	  }
	  else
	       Perl_croak(aTHX_ "lobj is not of type version");

	  mPUSHs(vnormal(lobj));

	  PUTBACK;
	  return;
     }
}

XS(XS_version_vcmp)
{
     dVAR;
     dXSARGS;
     if (items < 1)
	 croak_xs_usage(cv, "lobj, ...");
     SP -= items;
     {
	  SV *	lobj;

	  if (sv_derived_from(ST(0), "version")) {
	       lobj = SvRV(ST(0));
	  }
	  else
	       Perl_croak(aTHX_ "lobj is not of type version");

	  {
	       SV	*rs;
	       SV	*rvs;
	       SV * robj = ST(1);
	       const IV	 swap = (IV)SvIV(ST(2));

	       if ( ! sv_derived_from(robj, "version") )
	       {
		    robj = new_version(SvOK(robj) ? robj : newSVpvs("0"));
	       }
	       rvs = SvRV(robj);

	       if ( swap )
	       {
		    rs = newSViv(vcmp(rvs,lobj));
	       }
	       else
	       {
		    rs = newSViv(vcmp(lobj,rvs));
	       }

	       mPUSHs(rs);
	  }

	  PUTBACK;
	  return;
     }
}

XS(XS_version_boolean)
{
    dVAR;
    dXSARGS;
    if (items < 1)
	croak_xs_usage(cv, "lobj, ...");
    SP -= items;
    if (sv_derived_from(ST(0), "version")) {
	SV * const lobj = SvRV(ST(0));
	SV * const rs = newSViv( vcmp(lobj,new_version(newSVpvs("0"))) );
	mPUSHs(rs);
	PUTBACK;
	return;
    }
    else
	Perl_croak(aTHX_ "lobj is not of type version");
}

XS(XS_version_noop)
{
    dVAR;
    dXSARGS;
    if (items < 1)
	croak_xs_usage(cv, "lobj, ...");
    if (sv_derived_from(ST(0), "version"))
	Perl_croak(aTHX_ "operation not supported with version object");
    else
	Perl_croak(aTHX_ "lobj is not of type version");
#ifndef HASATTRIBUTE_NORETURN
    XSRETURN_EMPTY;
#endif
}

XS(XS_version_is_alpha)
{
    dVAR;
    dXSARGS;
    if (items != 1)
	croak_xs_usage(cv, "lobj");
    SP -= items;
    if (sv_derived_from(ST(0), "version")) {
	SV * const lobj = ST(0);
	if ( hv_exists(MUTABLE_HV(SvRV(lobj)), "alpha", 5 ) )
	    XSRETURN_YES;
	else
	    XSRETURN_NO;
	PUTBACK;
	return;
    }
    else
	Perl_croak(aTHX_ "lobj is not of type version");
}

XS(XS_version_qv)
{
    dVAR;
    dXSARGS;
    PERL_UNUSED_ARG(cv);
    SP -= items;
    {
	SV * ver = ST(0);
	SV * rv;
	const char * classname = "";
	if ( items == 2 && SvOK(ST(1)) ) {
	    /* getting called as object or class method */
	    ver = ST(1);
	    classname = 
		sv_isobject(ST(0)) /* class called as an object method */
		    ? HvNAME_get(SvSTASH(SvRV(ST(0))))
		    : (char *)SvPV_nolen(ST(0));
	}
	if ( !SvVOK(ver) ) { /* not already a v-string */
	    rv = sv_newmortal();
	    sv_setsv(rv,ver); /* make a duplicate */
	    upg_version(rv, TRUE);
	} else {
	    rv = sv_2mortal(new_version(ver));
	}
	if ( items == 2 && strcmp(classname,"version") ) { /* inherited new() */
	    sv_bless(rv, gv_stashpv(classname, GV_ADD));
	}
	PUSHs(rv);
    }
    PUTBACK;
    return;
}

XS(XS_version_is_qv)
{
    dVAR;
    dXSARGS;
    if (items != 1)
	croak_xs_usage(cv, "lobj");
    SP -= items;
    if (sv_derived_from(ST(0), "version")) {
	SV * const lobj = ST(0);
	if ( hv_exists(MUTABLE_HV(SvRV(lobj)), "qv", 2 ) )
	    XSRETURN_YES;
	else
	    XSRETURN_NO;
	PUTBACK;
	return;
    }
    else
	Perl_croak(aTHX_ "lobj is not of type version");
}

XS(XS_utf8_is_utf8)
{
     dVAR;
     dXSARGS;
     if (items != 1)
	 croak_xs_usage(cv, "sv");
     else {
	SV * const sv = ST(0);
	SvGETMAGIC(sv);
	    if (SvUTF8(sv))
		XSRETURN_YES;
	    else
		XSRETURN_NO;
     }
     XSRETURN_EMPTY;
}

XS(XS_utf8_valid)
{
     dVAR;
     dXSARGS;
     if (items != 1)
	 croak_xs_usage(cv, "sv");
    else {
	SV * const sv = ST(0);
	STRLEN len;
	const char * const s = SvPV_const(sv,len);
	if (!SvUTF8(sv) || is_utf8_string((const U8*)s,len))
	    XSRETURN_YES;
	else
	    XSRETURN_NO;
    }
     XSRETURN_EMPTY;
}

XS(XS_utf8_encode)
{
    dVAR;
    dXSARGS;
    if (items != 1)
	croak_xs_usage(cv, "sv");
    sv_utf8_encode(ST(0));
    XSRETURN_EMPTY;
}

XS(XS_utf8_decode)
{
    dVAR;
    dXSARGS;
    if (items != 1)
	croak_xs_usage(cv, "sv");
    else {
	SV * const sv = ST(0);
	const bool RETVAL = sv_utf8_decode(sv);
	ST(0) = boolSV(RETVAL);
	sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

XS(XS_utf8_upgrade)
{
    dVAR;
    dXSARGS;
    if (items != 1)
	croak_xs_usage(cv, "sv");
    else {
	SV * const sv = ST(0);
	STRLEN	RETVAL;
	dXSTARG;

	RETVAL = sv_utf8_upgrade(sv);
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}

XS(XS_utf8_downgrade)
{
    dVAR;
    dXSARGS;
    if (items < 1 || items > 2)
	croak_xs_usage(cv, "sv, failok=0");
    else {
	SV * const sv = ST(0);
        const bool failok = (items < 2) ? 0 : (int)SvIV(ST(1));
        const bool RETVAL = sv_utf8_downgrade(sv, failok);

	ST(0) = boolSV(RETVAL);
	sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

XS(XS_utf8_native_to_unicode)
{
 dVAR;
 dXSARGS;
 const UV uv = SvUV(ST(0));

 if (items > 1)
     croak_xs_usage(cv, "sv");

 ST(0) = sv_2mortal(newSViv(NATIVE_TO_UNI(uv)));
 XSRETURN(1);
}

XS(XS_utf8_unicode_to_native)
{
 dVAR;
 dXSARGS;
 const UV uv = SvUV(ST(0));

 if (items > 1)
     croak_xs_usage(cv, "sv");

 ST(0) = sv_2mortal(newSViv(UNI_TO_NATIVE(uv)));
 XSRETURN(1);
}

XS(XS_Internals_SvREADONLY)	/* This is dangerous stuff. */
{
    dVAR;
    dXSARGS;
    SV * const sv = SvRV(ST(0));
    PERL_UNUSED_ARG(cv);

    if (items == 1) {
	 if (SvREADONLY(sv))
	     XSRETURN_YES;
	 else
	     XSRETURN_NO;
    }
    else if (items == 2) {
	if (SvTRUE(ST(1))) {
	    SvREADONLY_on(sv);
	    XSRETURN_YES;
	}
	else {
	    /* I hope you really know what you are doing. */
	    SvREADONLY_off(sv);
	    XSRETURN_NO;
	}
    }
    XSRETURN_UNDEF; /* Can't happen. */
}

XS(XS_Internals_SvREFCNT)	/* This is dangerous stuff. */
{
    dVAR;
    dXSARGS;
    SV * const sv = SvRV(ST(0));
    PERL_UNUSED_ARG(cv);

    if (items == 1)
	 XSRETURN_IV(SvREFCNT(sv) - 1); /* Minus the ref created for us. */
    else if (items == 2) {
         /* I hope you really know what you are doing. */
	 SvREFCNT(sv) = SvIV(ST(1));
	 XSRETURN_IV(SvREFCNT(sv));
    }
    XSRETURN_UNDEF; /* Can't happen. */
}

XS(XS_Internals_hv_clear_placehold)
{
    dVAR;
    dXSARGS;

    if (items != 1)
	croak_xs_usage(cv, "hv");
    else {
	HV * const hv = MUTABLE_HV(SvRV(ST(0)));
	hv_clear_placeholders(hv);
	XSRETURN(0);
    }
}

XS(XS_PerlIO_get_layers)
{
    dVAR;
    dXSARGS;
    if (items < 1 || items % 2 == 0)
	croak_xs_usage(cv, "filehandle[,args]");
#ifdef USE_PERLIO
    {
	SV *	sv;
	GV *	gv;
	IO *	io;
	bool	input = TRUE;
	bool	details = FALSE;

	if (items > 1) {
	     SV * const *svp;
	     for (svp = MARK + 2; svp <= SP; svp += 2) {
		  SV * const * const varp = svp;
		  SV * const * const valp = svp + 1;
		  STRLEN klen;
		  const char * const key = SvPV_const(*varp, klen);

		  switch (*key) {
		  case 'i':
		       if (klen == 5 && memEQ(key, "input", 5)) {
			    input = SvTRUE(*valp);
			    break;
		       }
		       goto fail;
		  case 'o': 
		       if (klen == 6 && memEQ(key, "output", 6)) {
			    input = !SvTRUE(*valp);
			    break;
		       }
		       goto fail;
		  case 'd':
		       if (klen == 7 && memEQ(key, "details", 7)) {
			    details = SvTRUE(*valp);
			    break;
		       }
		       goto fail;
		  default:
		  fail:
		       Perl_croak(aTHX_
				  "get_layers: unknown argument '%s'",
				  key);
		  }
	     }

	     SP -= (items - 1);
	}

	sv = POPs;
	gv = MUTABLE_GV(sv);

	if (!isGV(sv)) {
	     if (SvROK(sv) && isGV(SvRV(sv)))
		  gv = MUTABLE_GV(SvRV(sv));
	     else if (SvPOKp(sv))
		  gv = gv_fetchsv(sv, 0, SVt_PVIO);
	}

	if (gv && (io = GvIO(gv))) {
	     AV* const av = PerlIO_get_layers(aTHX_ input ?
					IoIFP(io) : IoOFP(io));
	     I32 i;
	     const I32 last = av_len(av);
	     I32 nitem = 0;
	     
	     for (i = last; i >= 0; i -= 3) {
		  SV * const * const namsvp = av_fetch(av, i - 2, FALSE);
		  SV * const * const argsvp = av_fetch(av, i - 1, FALSE);
		  SV * const * const flgsvp = av_fetch(av, i,     FALSE);

		  const bool namok = namsvp && *namsvp && SvPOK(*namsvp);
		  const bool argok = argsvp && *argsvp && SvPOK(*argsvp);
		  const bool flgok = flgsvp && *flgsvp && SvIOK(*flgsvp);

		  if (details) {
		      /* Indents of 5? Yuck.  */
		      /* We know that PerlIO_get_layers creates a new SV for
			 the name and flags, so we can just take a reference
			 and "steal" it when we free the AV below.  */
		       XPUSHs(namok
			      ? sv_2mortal(SvREFCNT_inc_simple_NN(*namsvp))
			      : &PL_sv_undef);
		       XPUSHs(argok
			      ? newSVpvn_flags(SvPVX_const(*argsvp),
					       SvCUR(*argsvp),
					       (SvUTF8(*argsvp) ? SVf_UTF8 : 0)
					       | SVs_TEMP)
			      : &PL_sv_undef);
		       XPUSHs(flgok
			      ? sv_2mortal(SvREFCNT_inc_simple_NN(*flgsvp))
			      : &PL_sv_undef);
		       nitem += 3;
		  }
		  else {
		       if (namok && argok)
			    XPUSHs(sv_2mortal(Perl_newSVpvf(aTHX_ "%"SVf"(%"SVf")",
						 SVfARG(*namsvp),
						 SVfARG(*argsvp))));
		       else if (namok)
			   XPUSHs(sv_2mortal(SvREFCNT_inc_simple_NN(*namsvp)));
		       else
			    XPUSHs(&PL_sv_undef);
		       nitem++;
		       if (flgok) {
			    const IV flags = SvIVX(*flgsvp);

			    if (flags & PERLIO_F_UTF8) {
				 XPUSHs(newSVpvs_flags("utf8", SVs_TEMP));
				 nitem++;
			    }
		       }
		  }
	     }

	     SvREFCNT_dec(av);

	     XSRETURN(nitem);
	}
    }
#endif

    XSRETURN(0);
}

XS(XS_Internals_hash_seed)
{
    dVAR;
    /* Using dXSARGS would also have dITEM and dSP,
     * which define 2 unused local variables.  */
    dAXMARK;
    PERL_UNUSED_ARG(cv);
    PERL_UNUSED_VAR(mark);
    XSRETURN_UV(PERL_HASH_SEED);
}

XS(XS_Internals_rehash_seed)
{
    dVAR;
    /* Using dXSARGS would also have dITEM and dSP,
     * which define 2 unused local variables.  */
    dAXMARK;
    PERL_UNUSED_ARG(cv);
    PERL_UNUSED_VAR(mark);
    XSRETURN_UV(PL_rehash_seed);
}

XS(XS_Internals_HvREHASH)	/* Subject to change  */
{
    dVAR;
    dXSARGS;
    PERL_UNUSED_ARG(cv);
    if (SvROK(ST(0))) {
	const HV * const hv = (const HV *) SvRV(ST(0));
	if (items == 1 && SvTYPE(hv) == SVt_PVHV) {
	    if (HvREHASH(hv))
		XSRETURN_YES;
	    else
		XSRETURN_NO;
	}
    }
    Perl_croak(aTHX_ "Internals::HvREHASH $hashref");
}

XS(XS_re_is_regexp)
{
    dVAR; 
    dXSARGS;
    PERL_UNUSED_VAR(cv);

    if (items != 1)
	croak_xs_usage(cv, "sv");

    SP -= items;

    if (SvRXOK(ST(0))) {
        XSRETURN_YES;
    } else {
        XSRETURN_NO;
    }
}

XS(XS_re_regnames_count)
{
    REGEXP *rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;
    SV * ret;
    dVAR; 
    dXSARGS;

    if (items != 0)
	croak_xs_usage(cv, "");

    SP -= items;

    if (!rx)
        XSRETURN_UNDEF;

    ret = CALLREG_NAMED_BUFF_COUNT(rx);

    SPAGAIN;

    if (ret) {
        mXPUSHs(ret);
        PUTBACK;
        return;
    } else {
        XSRETURN_UNDEF;
    }
}

XS(XS_re_regname)
{
    dVAR;
    dXSARGS;
    REGEXP * rx;
    U32 flags;
    SV * ret;

    if (items < 1 || items > 2)
	croak_xs_usage(cv, "name[, all ]");

    SP -= items;

    rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;

    if (!rx)
        XSRETURN_UNDEF;

    if (items == 2 && SvTRUE(ST(1))) {
        flags = RXapif_ALL;
    } else {
        flags = RXapif_ONE;
    }
    ret = CALLREG_NAMED_BUFF_FETCH(rx, ST(0), (flags | RXapif_REGNAME));

    if (ret) {
        mXPUSHs(ret);
        XSRETURN(1);
    }
    XSRETURN_UNDEF;    
}


XS(XS_re_regnames)
{
    dVAR;
    dXSARGS;
    REGEXP * rx;
    U32 flags;
    SV *ret;
    AV *av;
    I32 length;
    I32 i;
    SV **entry;

    if (items > 1)
	croak_xs_usage(cv, "[all]");

    rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;

    if (!rx)
        XSRETURN_UNDEF;

    if (items == 1 && SvTRUE(ST(0))) {
        flags = RXapif_ALL;
    } else {
        flags = RXapif_ONE;
    }

    SP -= items;

    ret = CALLREG_NAMED_BUFF_ALL(rx, (flags | RXapif_REGNAMES));

    SPAGAIN;

    SP -= items;

    if (!ret)
        XSRETURN_UNDEF;

    av = MUTABLE_AV(SvRV(ret));
    length = av_len(av);

    for (i = 0; i <= length; i++) {
        entry = av_fetch(av, i, FALSE);
        
        if (!entry)
            Perl_croak(aTHX_ "NULL array element in re::regnames()");

        mXPUSHs(SvREFCNT_inc_simple_NN(*entry));
    }

    SvREFCNT_dec(ret);

    PUTBACK;
    return;
}

XS(XS_re_regexp_pattern)
{
    dVAR;
    dXSARGS;
    REGEXP *re;

    if (items != 1)
	croak_xs_usage(cv, "sv");

    SP -= items;

    /*
       Checks if a reference is a regex or not. If the parameter is
       not a ref, or is not the result of a qr// then returns false
       in scalar context and an empty list in list context.
       Otherwise in list context it returns the pattern and the
       modifiers, in scalar context it returns the pattern just as it
       would if the qr// was stringified normally, regardless as
       to the class of the variable and any strigification overloads
       on the object.
    */

    if ((re = SvRX(ST(0)))) /* assign deliberate */
    {
        /* Housten, we have a regex! */
        SV *pattern;
        STRLEN left = 0;
        char reflags[6];

        if ( GIMME_V == G_ARRAY ) {
            /*
               we are in list context so stringify
               the modifiers that apply. We ignore "negative
               modifiers" in this scenario.
            */

            const char *fptr = INT_PAT_MODS;
            char ch;
            U16 match_flags = (U16)((RX_EXTFLAGS(re) & PMf_COMPILETIME)
                                    >> RXf_PMf_STD_PMMOD_SHIFT);

            while((ch = *fptr++)) {
                if(match_flags & 1) {
                    reflags[left++] = ch;
                }
                match_flags >>= 1;
            }

            pattern = newSVpvn_flags(RX_PRECOMP(re),RX_PRELEN(re),
				     (RX_UTF8(re) ? SVf_UTF8 : 0) | SVs_TEMP);

            /* return the pattern and the modifiers */
            XPUSHs(pattern);
            XPUSHs(newSVpvn_flags(reflags, left, SVs_TEMP));
            XSRETURN(2);
        } else {
            /* Scalar, so use the string that Perl would return */
            /* return the pattern in (?msix:..) format */
#if PERL_VERSION >= 11
            pattern = sv_2mortal(newSVsv(MUTABLE_SV(re)));
#else
            pattern = newSVpvn_flags(RX_WRAPPED(re), RX_WRAPLEN(re),
				     (RX_UTF8(re) ? SVf_UTF8 : 0) | SVs_TEMP);
#endif
            XPUSHs(pattern);
            XSRETURN(1);
        }
    } else {
        /* It ain't a regexp folks */
        if ( GIMME_V == G_ARRAY ) {
            /* return the empty list */
            XSRETURN_UNDEF;
        } else {
            /* Because of the (?:..) wrapping involved in a
               stringified pattern it is impossible to get a
               result for a real regexp that would evaluate to
               false. Therefore we can return PL_sv_no to signify
               that the object is not a regex, this means that one
               can say

                 if (regex($might_be_a_regex) eq '(?:foo)') { }

               and not worry about undefined values.
            */
            XSRETURN_NO;
        }
    }
    /* NOT-REACHED */
}

XS(XS_Tie_Hash_NamedCapture_FETCH)
{
    dVAR;
    dXSARGS;
    REGEXP * rx;
    U32 flags;
    SV * ret;

    if (items != 2)
	croak_xs_usage(cv, "$key, $flags");

    rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;

    if (!rx || !SvROK(ST(0)))
        XSRETURN_UNDEF;

    SP -= items;

    flags = (U32)INT2PTR(IV,SvIV(SvRV(MUTABLE_SV(ST(0)))));
    ret = CALLREG_NAMED_BUFF_FETCH(rx, ST(1), flags);

    SPAGAIN;

    if (ret) {
        mXPUSHs(ret);
        PUTBACK;
        return;
    }
    XSRETURN_UNDEF;
}

XS(XS_Tie_Hash_NamedCapture_STORE)
{
    dVAR;
    dXSARGS;
    REGEXP * rx;
    U32 flags;

    if (items != 3)
	croak_xs_usage(cv, "$key, $value, $flags");

    rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;

    if (!rx || !SvROK(ST(0))) {
        if (!PL_localizing)
            Perl_croak(aTHX_ "%s", PL_no_modify);
        else
            XSRETURN_UNDEF;
    }

    SP -= items;

    flags = (U32)INT2PTR(IV,SvIV(SvRV(MUTABLE_SV(ST(0)))));
    CALLREG_NAMED_BUFF_STORE(rx,ST(1), ST(2), flags);
}

XS(XS_Tie_Hash_NamedCapture_DELETE)
{
    dVAR;
    dXSARGS;
    REGEXP * rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;
    U32 flags;

    if (items != 2)
	croak_xs_usage(cv, "$key, $flags");

    if (!rx || !SvROK(ST(0)))
        Perl_croak(aTHX_ "%s", PL_no_modify);

    SP -= items;

    flags = (U32)INT2PTR(IV,SvIV(SvRV(MUTABLE_SV(ST(0)))));
    CALLREG_NAMED_BUFF_DELETE(rx, ST(1), flags);
}

XS(XS_Tie_Hash_NamedCapture_CLEAR)
{
    dVAR;
    dXSARGS;
    REGEXP * rx;
    U32 flags;

    if (items != 1)
	croak_xs_usage(cv, "$flags");

    rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;

    if (!rx || !SvROK(ST(0)))
        Perl_croak(aTHX_ "%s", PL_no_modify);

    SP -= items;

    flags = (U32)INT2PTR(IV,SvIV(SvRV(MUTABLE_SV(ST(0)))));
    CALLREG_NAMED_BUFF_CLEAR(rx, flags);
}

XS(XS_Tie_Hash_NamedCapture_EXISTS)
{
    dVAR;
    dXSARGS;
    REGEXP * rx;
    U32 flags;
    SV * ret;

    if (items != 2)
	croak_xs_usage(cv, "$key, $flags");

    rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;

    if (!rx || !SvROK(ST(0)))
        XSRETURN_UNDEF;

    SP -= items;

    flags = (U32)INT2PTR(IV,SvIV(SvRV(MUTABLE_SV(ST(0)))));
    ret = CALLREG_NAMED_BUFF_EXISTS(rx, ST(1), flags);

    SPAGAIN;

	XPUSHs(ret);
	PUTBACK;
	return;
}

XS(XS_Tie_Hash_NamedCapture_FIRSTK)
{
    dVAR;
    dXSARGS;
    REGEXP * rx;
    U32 flags;
    SV * ret;

    if (items != 1)
	croak_xs_usage(cv, "");

    rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;

    if (!rx || !SvROK(ST(0)))
        XSRETURN_UNDEF;

    SP -= items;

    flags = (U32)INT2PTR(IV,SvIV(SvRV(MUTABLE_SV(ST(0)))));
    ret = CALLREG_NAMED_BUFF_FIRSTKEY(rx, flags);

    SPAGAIN;

    if (ret) {
        mXPUSHs(ret);
        PUTBACK;
    } else {
        XSRETURN_UNDEF;
    }

}

XS(XS_Tie_Hash_NamedCapture_NEXTK)
{
    dVAR;
    dXSARGS;
    REGEXP * rx;
    U32 flags;
    SV * ret;

    if (items != 2)
	croak_xs_usage(cv, "$lastkey");

    rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;

    if (!rx || !SvROK(ST(0)))
        XSRETURN_UNDEF;

    SP -= items;

    flags = (U32)INT2PTR(IV,SvIV(SvRV(MUTABLE_SV(ST(0)))));
    ret = CALLREG_NAMED_BUFF_NEXTKEY(rx, ST(1), flags);

    SPAGAIN;

    if (ret) {
        mXPUSHs(ret);
    } else {
        XSRETURN_UNDEF;
    }  
    PUTBACK;
}

XS(XS_Tie_Hash_NamedCapture_SCALAR)
{
    dVAR;
    dXSARGS;
    REGEXP * rx;
    U32 flags;
    SV * ret;

    if (items != 1)
	croak_xs_usage(cv, "");

    rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;

    if (!rx || !SvROK(ST(0)))
        XSRETURN_UNDEF;

    SP -= items;

    flags = (U32)INT2PTR(IV,SvIV(SvRV(MUTABLE_SV(ST(0)))));
    ret = CALLREG_NAMED_BUFF_SCALAR(rx, flags);

    SPAGAIN;

    if (ret) {
        mXPUSHs(ret);
        PUTBACK;
        return;
    } else {
        XSRETURN_UNDEF;
    }
}

XS(XS_Tie_Hash_NamedCapture_flags)
{
    dVAR;
    dXSARGS;

    if (items != 0)
	croak_xs_usage(cv, "");

	mXPUSHu(RXapif_ONE);
	mXPUSHu(RXapif_ALL);
	PUTBACK;
	return;
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
