/*    universal.c
 *
 *    Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "The roots of those mountains must be roots indeed; there must be
 * great secrets buried there which have not been discovered since the
 * beginning." --Gandalf, relating Gollum's story
 */

#include "EXTERN.h"
#define PERL_IN_UNIVERSAL_C
#include "perl.h"

#ifdef USE_PERLIO
#include "perliol.h" /* For the PERLIO_F_XXX */
#endif

/*
 * Contributed by Graham Barr  <Graham.Barr@tiuk.ti.com>
 * The main guts of traverse_isa was actually copied from gv_fetchmeth
 */

STATIC SV *
S_isa_lookup(pTHX_ HV *stash, const char *name, HV* name_stash,
             int len, int level)
{
    AV* av;
    GV* gv;
    GV** gvp;
    HV* hv = Nullhv;
    SV* subgen = Nullsv;

    /* A stash/class can go by many names (ie. User == main::User), so 
       we compare the stash itself just in case */
    if (name_stash && (stash == name_stash))
        return &PL_sv_yes;

    if (strEQ(HvNAME(stash), name))
	return &PL_sv_yes;

    if (strEQ(name, "UNIVERSAL"))
	return &PL_sv_yes;

    if (level > 100)
	Perl_croak(aTHX_ "Recursive inheritance detected in package '%s'",
		   HvNAME(stash));

    gvp = (GV**)hv_fetch(stash, "::ISA::CACHE::", 14, FALSE);

    if (gvp && (gv = *gvp) != (GV*)&PL_sv_undef && (subgen = GvSV(gv))
	&& (hv = GvHV(gv)))
    {
	if (SvIV(subgen) == (IV)PL_sub_generation) {
	    SV* sv;
	    SV** svp = (SV**)hv_fetch(hv, name, len, FALSE);
	    if (svp && (sv = *svp) != (SV*)&PL_sv_undef) {
	        DEBUG_o( Perl_deb(aTHX_ "Using cached ISA %s for package %s\n",
				  name, HvNAME(stash)) );
		return sv;
	    }
	}
	else {
	    DEBUG_o( Perl_deb(aTHX_ "ISA Cache in package %s is stale\n",
			      HvNAME(stash)) );
	    hv_clear(hv);
	    sv_setiv(subgen, PL_sub_generation);
	}
    }

    gvp = (GV**)hv_fetch(stash,"ISA",3,FALSE);

    if (gvp && (gv = *gvp) != (GV*)&PL_sv_undef && (av = GvAV(gv))) {
	if (!hv || !subgen) {
	    gvp = (GV**)hv_fetch(stash, "::ISA::CACHE::", 14, TRUE);

	    gv = *gvp;

	    if (SvTYPE(gv) != SVt_PVGV)
		gv_init(gv, stash, "::ISA::CACHE::", 14, TRUE);

	    if (!hv)
		hv = GvHVn(gv);
	    if (!subgen) {
		subgen = newSViv(PL_sub_generation);
		GvSV(gv) = subgen;
	    }
	}
	if (hv) {
	    SV** svp = AvARRAY(av);
	    /* NOTE: No support for tied ISA */
	    I32 items = AvFILLp(av) + 1;
	    while (items--) {
		SV* sv = *svp++;
		HV* basestash = gv_stashsv(sv, FALSE);
		if (!basestash) {
		    if (ckWARN(WARN_MISC))
			Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
		             "Can't locate package %"SVf" for @%s::ISA",
			    sv, HvNAME(stash));
		    continue;
		}
		if (&PL_sv_yes == isa_lookup(basestash, name, name_stash, 
                                             len, level + 1)) {
		    (void)hv_store(hv,name,len,&PL_sv_yes,0);
		    return &PL_sv_yes;
		}
	    }
	    (void)hv_store(hv,name,len,&PL_sv_no,0);
	}
    }
    return &PL_sv_no;
}

/*
=head1 SV Manipulation Functions

=for apidoc sv_derived_from

Returns a boolean indicating whether the SV is derived from the specified
class.  This is the function that implements C<UNIVERSAL::isa>.  It works
for class names as well as for objects.

=cut
*/

bool
Perl_sv_derived_from(pTHX_ SV *sv, const char *name)
{
    char *type;
    HV *stash;
    HV *name_stash;

    stash = Nullhv;
    type = Nullch;

    if (SvGMAGICAL(sv))
        mg_get(sv) ;

    if (SvROK(sv)) {
        sv = SvRV(sv);
        type = sv_reftype(sv,0);
        if (SvOBJECT(sv))
            stash = SvSTASH(sv);
    }
    else {
        stash = gv_stashsv(sv, FALSE);
    }

    name_stash = gv_stashpv(name, FALSE);

    return (type && strEQ(type,name)) ||
            (stash && isa_lookup(stash, name, name_stash, strlen(name), 0) 
             == &PL_sv_yes)
        ? TRUE
        : FALSE ;
}

#include "XSUB.h"

void XS_UNIVERSAL_isa(pTHX_ CV *cv);
void XS_UNIVERSAL_can(pTHX_ CV *cv);
void XS_UNIVERSAL_VERSION(pTHX_ CV *cv);
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
XS(XS_Regexp_DESTROY);
XS(XS_Internals_hash_seed);
XS(XS_Internals_rehash_seed);
XS(XS_Internals_HvREHASH);

void
Perl_boot_core_UNIVERSAL(pTHX)
{
    char *file = __FILE__;

    newXS("UNIVERSAL::isa",             XS_UNIVERSAL_isa,         file);
    newXS("UNIVERSAL::can",             XS_UNIVERSAL_can,         file);
    newXS("UNIVERSAL::VERSION", 	XS_UNIVERSAL_VERSION, 	  file);
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
    newXS("Regexp::DESTROY", XS_Regexp_DESTROY, file);
    newXSproto("Internals::hash_seed",XS_Internals_hash_seed, file, "");
    newXSproto("Internals::rehash_seed",XS_Internals_rehash_seed, file, "");
    newXSproto("Internals::HvREHASH", XS_Internals_HvREHASH, file, "\\%");
}


XS(XS_UNIVERSAL_isa)
{
    dXSARGS;
    SV *sv;
    char *name;
    STRLEN n_a;

    if (items != 2)
	Perl_croak(aTHX_ "Usage: UNIVERSAL::isa(reference, kind)");

    sv = ST(0);

    if (SvGMAGICAL(sv))
	mg_get(sv);

    if (!SvOK(sv) || !(SvROK(sv) || (SvPOK(sv) && SvCUR(sv))
		|| (SvGMAGICAL(sv) && SvPOKp(sv) && SvCUR(sv))))
	XSRETURN_UNDEF;

    name = (char *)SvPV(ST(1),n_a);

    ST(0) = boolSV(sv_derived_from(sv, name));
    XSRETURN(1);
}

XS(XS_UNIVERSAL_can)
{
    dXSARGS;
    SV   *sv;
    char *name;
    SV   *rv;
    HV   *pkg = NULL;
    STRLEN n_a;

    if (items != 2)
	Perl_croak(aTHX_ "Usage: UNIVERSAL::can(object-ref, method)");

    sv = ST(0);

    if (SvGMAGICAL(sv))
	mg_get(sv);

    if (!SvOK(sv) || !(SvROK(sv) || (SvPOK(sv) && SvCUR(sv))
		|| (SvGMAGICAL(sv) && SvPOKp(sv) && SvCUR(sv))))
	XSRETURN_UNDEF;

    name = (char *)SvPV(ST(1),n_a);
    rv = &PL_sv_undef;

    if (SvROK(sv)) {
        sv = (SV*)SvRV(sv);
        if (SvOBJECT(sv))
            pkg = SvSTASH(sv);
    }
    else {
        pkg = gv_stashsv(sv, FALSE);
    }

    if (pkg) {
        GV *gv = gv_fetchmethod_autoload(pkg, name, FALSE);
        if (gv && isGV(gv))
	    rv = sv_2mortal(newRV((SV*)GvCV(gv)));
    }

    ST(0) = rv;
    XSRETURN(1);
}

XS(XS_UNIVERSAL_VERSION)
{
    dXSARGS;
    HV *pkg;
    GV **gvp;
    GV *gv;
    SV *sv;
    char *undef;

    if (SvROK(ST(0))) {
        sv = (SV*)SvRV(ST(0));
        if (!SvOBJECT(sv))
            Perl_croak(aTHX_ "Cannot find version of an unblessed reference");
        pkg = SvSTASH(sv);
    }
    else {
        pkg = gv_stashsv(ST(0), FALSE);
    }

    gvp = pkg ? (GV**)hv_fetch(pkg,"VERSION",7,FALSE) : Null(GV**);

    if (gvp && isGV(gv = *gvp) && SvOK(sv = GvSV(gv))) {
        SV *nsv = sv_newmortal();
        sv_setsv(nsv, sv);
        sv = nsv;
        undef = Nullch;
    }
    else {
        sv = (SV*)&PL_sv_undef;
        undef = "(undef)";
    }

    if (items > 1) {
	STRLEN len;
	SV *req = ST(1);

	if (undef) {
	     if (pkg)
		  Perl_croak(aTHX_
			     "%s does not define $%s::VERSION--version check failed",
			     HvNAME(pkg), HvNAME(pkg));
	     else {
		  char *str = SvPVx(ST(0), len);

		  Perl_croak(aTHX_
			     "%s defines neither package nor VERSION--version check failed", str);
	     }
	}
	if (!SvNIOK(sv) && SvPOK(sv)) {
	    char *str = SvPVx(sv,len);
	    while (len) {
		--len;
		/* XXX could DWIM "1.2.3" here */
		if (!isDIGIT(str[len]) && str[len] != '.' && str[len] != '_')
		    break;
	    }
	    if (len) {
		if (SvNOK(req) && SvPOK(req)) {
		    /* they said C<use Foo v1.2.3> and $Foo::VERSION
		     * doesn't look like a float: do string compare */
		    if (sv_cmp(req,sv) == 1) {
			Perl_croak(aTHX_ "%s v%"VDf" required--"
				   "this is only v%"VDf,
				   HvNAME(pkg), req, sv);
		    }
		    goto finish;
		}
		/* they said C<use Foo 1.002_003> and $Foo::VERSION
		 * doesn't look like a float: force numeric compare */
		(void)SvUPGRADE(sv, SVt_PVNV);
		SvNVX(sv) = str_to_version(sv);
		SvPOK_off(sv);
		SvNOK_on(sv);
	    }
	}
	/* if we get here, we're looking for a numeric comparison,
	 * so force the required version into a float, even if they
	 * said C<use Foo v1.2.3> */
	if (SvNOK(req) && SvPOK(req)) {
	    NV n = SvNV(req);
	    req = sv_newmortal();
	    sv_setnv(req, n);
	}

	if (SvNV(req) > SvNV(sv))
	    Perl_croak(aTHX_ "%s version %s required--this is only version %s",
		       HvNAME(pkg), SvPV_nolen(req), SvPV_nolen(sv));
    }

finish:
    ST(0) = sv;

    XSRETURN(1);
}

XS(XS_utf8_is_utf8)
{
     dXSARGS;
     if (items != 1)
	  Perl_croak(aTHX_ "Usage: utf8::is_utf8(sv)");
     {
	  SV *	sv = ST(0);
	  {
	       if (SvUTF8(sv))
		    XSRETURN_YES;
	       else
		    XSRETURN_NO;
	  }
     }
     XSRETURN_EMPTY;
}

XS(XS_utf8_valid)
{
     dXSARGS;
     if (items != 1)
	  Perl_croak(aTHX_ "Usage: utf8::valid(sv)");
     {
	  SV *	sv = ST(0);
	  {
	       STRLEN len;
	       char *s = SvPV(sv,len);
	       if (!SvUTF8(sv) || is_utf8_string((U8*)s,len))
		    XSRETURN_YES;
	       else
		    XSRETURN_NO;
	  }
     }
     XSRETURN_EMPTY;
}

XS(XS_utf8_encode)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "Usage: utf8::encode(sv)");
    {
	SV *	sv = ST(0);

	sv_utf8_encode(sv);
    }
    XSRETURN_EMPTY;
}

XS(XS_utf8_decode)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "Usage: utf8::decode(sv)");
    {
	SV *	sv = ST(0);
	bool	RETVAL;

	RETVAL = sv_utf8_decode(sv);
	ST(0) = boolSV(RETVAL);
	sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

XS(XS_utf8_upgrade)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "Usage: utf8::upgrade(sv)");
    {
	SV *	sv = ST(0);
	STRLEN	RETVAL;
	dXSTARG;

	RETVAL = sv_utf8_upgrade(sv);
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}

XS(XS_utf8_downgrade)
{
    dXSARGS;
    if (items < 1 || items > 2)
	Perl_croak(aTHX_ "Usage: utf8::downgrade(sv, failok=0)");
    {
	SV *	sv = ST(0);
	bool	failok;
	bool	RETVAL;

	if (items < 2)
	    failok = 0;
	else {
	    failok = (int)SvIV(ST(1));
	}

	RETVAL = sv_utf8_downgrade(sv, failok);
	ST(0) = boolSV(RETVAL);
	sv_2mortal(ST(0));
    }
    XSRETURN(1);
}

XS(XS_utf8_native_to_unicode)
{
 dXSARGS;
 UV uv = SvUV(ST(0));

 if (items > 1)
     Perl_croak(aTHX_ "Usage: utf8::native_to_unicode(sv)");

 ST(0) = sv_2mortal(newSViv(NATIVE_TO_UNI(uv)));
 XSRETURN(1);
}

XS(XS_utf8_unicode_to_native)
{
 dXSARGS;
 UV uv = SvUV(ST(0));

 if (items > 1)
     Perl_croak(aTHX_ "Usage: utf8::unicode_to_native(sv)");

 ST(0) = sv_2mortal(newSViv(UNI_TO_NATIVE(uv)));
 XSRETURN(1);
}

XS(XS_Internals_SvREADONLY)	/* This is dangerous stuff. */
{
    dXSARGS;
    SV *sv = SvRV(ST(0));
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
    dXSARGS;
    SV *sv = SvRV(ST(0));
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
    dXSARGS;
    HV *hv = (HV *) SvRV(ST(0));
    if (items != 1)
	Perl_croak(aTHX_ "Usage: UNIVERSAL::hv_clear_placeholders(hv)");
    hv_clear_placeholders(hv);
    XSRETURN(0);
}

XS(XS_Regexp_DESTROY)
{

}

XS(XS_PerlIO_get_layers)
{
    dXSARGS;
    if (items < 1 || items % 2 == 0)
	Perl_croak(aTHX_ "Usage: PerlIO_get_layers(filehandle[,args])");
#ifdef USE_PERLIO
    {
	SV *	sv;
	GV *	gv;
	IO *	io;
	bool	input = TRUE;
	bool	details = FALSE;

	if (items > 1) {
	     SV **svp;
	     
	     for (svp = MARK + 2; svp <= SP; svp += 2) {
		  SV **varp = svp;
		  SV **valp = svp + 1;
		  STRLEN klen;
		  char *key = SvPV(*varp, klen);

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
	gv = (GV*)sv;

	if (!isGV(sv)) {
	     if (SvROK(sv) && isGV(SvRV(sv)))
		  gv = (GV*)SvRV(sv);
	     else
		  gv = gv_fetchpv(SvPVX(sv), FALSE, SVt_PVIO);
	}

	if (gv && (io = GvIO(gv))) {
	     dTARGET;
	     AV* av = PerlIO_get_layers(aTHX_ input ?
					IoIFP(io) : IoOFP(io));
	     I32 i;
	     I32 last = av_len(av);
	     I32 nitem = 0;
	     
	     for (i = last; i >= 0; i -= 3) {
		  SV **namsvp;
		  SV **argsvp;
		  SV **flgsvp;
		  bool namok, argok, flgok;

		  namsvp = av_fetch(av, i - 2, FALSE);
		  argsvp = av_fetch(av, i - 1, FALSE);
		  flgsvp = av_fetch(av, i,     FALSE);

		  namok = namsvp && *namsvp && SvPOK(*namsvp);
		  argok = argsvp && *argsvp && SvPOK(*argsvp);
		  flgok = flgsvp && *flgsvp && SvIOK(*flgsvp);

		  if (details) {
		       XPUSHs(namok ?
			     newSVpv(SvPVX(*namsvp), 0) : &PL_sv_undef);
		       XPUSHs(argok ?
			     newSVpv(SvPVX(*argsvp), 0) : &PL_sv_undef);
		       if (flgok)
			    XPUSHi(SvIVX(*flgsvp));
		       else
			    XPUSHs(&PL_sv_undef);
		       nitem += 3;
		  }
		  else {
		       if (namok && argok)
			    XPUSHs(Perl_newSVpvf(aTHX_ "%"SVf"(%"SVf")",
					       *namsvp, *argsvp));
		       else if (namok)
			    XPUSHs(Perl_newSVpvf(aTHX_ "%"SVf, *namsvp));
		       else
			    XPUSHs(&PL_sv_undef);
		       nitem++;
		       if (flgok) {
			    IV flags = SvIVX(*flgsvp);

			    if (flags & PERLIO_F_UTF8) {
				 XPUSHs(newSVpvn("utf8", 4));
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
    /* Using dXSARGS would also have dITEM and dSP,
     * which define 2 unused local variables.  */
    dMARK; dAX;
    XSRETURN_UV(PERL_HASH_SEED);
}

XS(XS_Internals_rehash_seed)
{
    /* Using dXSARGS would also have dITEM and dSP,
     * which define 2 unused local variables.  */
    dMARK; dAX;
    XSRETURN_UV(PL_rehash_seed);
}

XS(XS_Internals_HvREHASH)	/* Subject to change  */
{
    dXSARGS;
    if (SvROK(ST(0))) {
	HV *hv = (HV *) SvRV(ST(0));
	if (items == 1 && SvTYPE(hv) == SVt_PVHV) {
	    if (HvREHASH(hv))
		XSRETURN_YES;
	    else
		XSRETURN_NO;
	}
    }
    Perl_croak(aTHX_ "Internals::HvREHASH $hashref");
}
