/*    mathoms.c
 *
 *    Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010,
 *    2011, 2012 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *  Anything that Hobbits had no immediate use for, but were unwilling to
 *  throw away, they called a mathom.  Their dwellings were apt to become
 *  rather crowded with mathoms, and many of the presents that passed from
 *  hand to hand were of that sort.
 *
 *     [p.5 of _The Lord of the Rings_: "Prologue"]
 */



/*
 * This file contains mathoms, various binary artifacts from previous
 * versions of Perl which we cannot completely remove from the core
 * code. There are two reasons functions should be here:
 *
 * 1) A function has been been replaced by a macro within a minor release,
 *    so XS modules compiled against an older release will expect to
 *    still be able to link against the function
 * 2) A function Perl_foo(...) with #define foo Perl_foo(aTHX_ ...)
 *    has been replaced by a macro, e.g. #define foo(...) foo_flags(...,0)
 *    but XS code may still explicitly use the long form, i.e.
 *    Perl_foo(aTHX_ ...)
 *
 * NOTE: ALL FUNCTIONS IN THIS FILE should have an entry with the 'b' flag in
 * embed.fnc.
 *
 * To move a function to this file, simply cut and paste it here, and change
 * its embed.fnc entry to additionally have the 'b' flag.  If, for some reason
 * a function you'd like to be treated as mathoms can't be moved from its
 * current place, simply enclose it between
 *
 * #ifndef NO_MATHOMS
 *    ...
 * #endif
 *
 * and add the 'b' flag in embed.fnc.
 *
 * The compilation of this file can be suppressed; see INSTALL
 *
 * Some blurb for perlapi.pod:

=head1 Obsolete backwards compatibility functions

Some of these are also deprecated.  You can exclude these from
your compiled Perl by adding this option to Configure:
C<-Accflags='-DNO_MATHOMS'>

=cut

 */


#include "EXTERN.h"
#define PERL_IN_MATHOMS_C
#include "perl.h"

#ifdef NO_MATHOMS
/* ..." warning: ISO C forbids an empty source file"
   So make sure we have something in here by processing the headers anyway.
 */
#else

/* ref() is now a macro using Perl_doref;
 * this version provided for binary compatibility only.
 */
OP *
Perl_ref(pTHX_ OP *o, I32 type)
{
    return doref(o, type, TRUE);
}

/*
=for apidoc sv_unref

Unsets the RV status of the SV, and decrements the reference count of
whatever was being referenced by the RV.  This can almost be thought of
as a reversal of C<newSVrv>.  This is C<sv_unref_flags> with the C<flag>
being zero.  See C<L</SvROK_off>>.

=cut
*/

void
Perl_sv_unref(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_UNREF;

    sv_unref_flags(sv, 0);
}

/*
=for apidoc sv_taint

Taint an SV.  Use C<SvTAINTED_on> instead.

=cut
*/

void
Perl_sv_taint(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_TAINT;

    sv_magic((sv), NULL, PERL_MAGIC_taint, NULL, 0);
}

/* sv_2iv() is now a macro using Perl_sv_2iv_flags();
 * this function provided for binary compatibility only
 */

IV
Perl_sv_2iv(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_2IV;

    return sv_2iv_flags(sv, SV_GMAGIC);
}

/* sv_2uv() is now a macro using Perl_sv_2uv_flags();
 * this function provided for binary compatibility only
 */

UV
Perl_sv_2uv(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_2UV;

    return sv_2uv_flags(sv, SV_GMAGIC);
}

/* sv_2nv() is now a macro using Perl_sv_2nv_flags();
 * this function provided for binary compatibility only
 */

NV
Perl_sv_2nv(pTHX_ SV *sv)
{
    return sv_2nv_flags(sv, SV_GMAGIC);
}


/* sv_2pv() is now a macro using Perl_sv_2pv_flags();
 * this function provided for binary compatibility only
 */

char *
Perl_sv_2pv(pTHX_ SV *sv, STRLEN *lp)
{
    PERL_ARGS_ASSERT_SV_2PV;

    return sv_2pv_flags(sv, lp, SV_GMAGIC);
}

/*
=for apidoc sv_2pv_nolen

Like C<sv_2pv()>, but doesn't return the length too.  You should usually
use the macro wrapper C<SvPV_nolen(sv)> instead.

=cut
*/

char *
Perl_sv_2pv_nolen(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_2PV_NOLEN;
    return sv_2pv(sv, NULL);
}

/*
=for apidoc sv_2pvbyte_nolen

Return a pointer to the byte-encoded representation of the SV.
May cause the SV to be downgraded from UTF-8 as a side-effect.

Usually accessed via the C<SvPVbyte_nolen> macro.

=cut
*/

char *
Perl_sv_2pvbyte_nolen(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_2PVBYTE_NOLEN;

    return sv_2pvbyte(sv, NULL);
}

/*
=for apidoc sv_2pvutf8_nolen

Return a pointer to the UTF-8-encoded representation of the SV.
May cause the SV to be upgraded to UTF-8 as a side-effect.

Usually accessed via the C<SvPVutf8_nolen> macro.

=cut
*/

char *
Perl_sv_2pvutf8_nolen(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_2PVUTF8_NOLEN;

    return sv_2pvutf8(sv, NULL);
}

/*
=for apidoc sv_force_normal

Undo various types of fakery on an SV: if the PV is a shared string, make
a private copy; if we're a ref, stop refing; if we're a glob, downgrade to
an C<xpvmg>.  See also C<L</sv_force_normal_flags>>.

=cut
*/

void
Perl_sv_force_normal(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_FORCE_NORMAL;

    sv_force_normal_flags(sv, 0);
}

/* sv_setsv() is now a macro using Perl_sv_setsv_flags();
 * this function provided for binary compatibility only
 */

void
Perl_sv_setsv(pTHX_ SV *dstr, SV *sstr)
{
    PERL_ARGS_ASSERT_SV_SETSV;

    sv_setsv_flags(dstr, sstr, SV_GMAGIC);
}

/* sv_catpvn() is now a macro using Perl_sv_catpvn_flags();
 * this function provided for binary compatibility only
 */

void
Perl_sv_catpvn(pTHX_ SV *dsv, const char* sstr, STRLEN slen)
{
    PERL_ARGS_ASSERT_SV_CATPVN;

    sv_catpvn_flags(dsv, sstr, slen, SV_GMAGIC);
}

/*
=for apidoc sv_catpvn_mg

Like C<sv_catpvn>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_catpvn_mg(pTHX_ SV *sv, const char *ptr, STRLEN len)
{
    PERL_ARGS_ASSERT_SV_CATPVN_MG;

    sv_catpvn_flags(sv,ptr,len,SV_GMAGIC|SV_SMAGIC);
}

/* sv_catsv() is now a macro using Perl_sv_catsv_flags();
 * this function provided for binary compatibility only
 */

void
Perl_sv_catsv(pTHX_ SV *dstr, SV *sstr)
{
    PERL_ARGS_ASSERT_SV_CATSV;

    sv_catsv_flags(dstr, sstr, SV_GMAGIC);
}

/*
=for apidoc sv_catsv_mg

Like C<sv_catsv>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_catsv_mg(pTHX_ SV *dsv, SV *ssv)
{
    PERL_ARGS_ASSERT_SV_CATSV_MG;

    sv_catsv_flags(dsv,ssv,SV_GMAGIC|SV_SMAGIC);
}

/*
=for apidoc sv_iv

A private implementation of the C<SvIVx> macro for compilers which can't
cope with complex macro expressions.  Always use the macro instead.

=cut
*/

IV
Perl_sv_iv(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_IV;

    if (SvIOK(sv)) {
	if (SvIsUV(sv))
	    return (IV)SvUVX(sv);
	return SvIVX(sv);
    }
    return sv_2iv(sv);
}

/*
=for apidoc sv_uv

A private implementation of the C<SvUVx> macro for compilers which can't
cope with complex macro expressions.  Always use the macro instead.

=cut
*/

UV
Perl_sv_uv(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_UV;

    if (SvIOK(sv)) {
	if (SvIsUV(sv))
	    return SvUVX(sv);
	return (UV)SvIVX(sv);
    }
    return sv_2uv(sv);
}

/*
=for apidoc sv_nv

A private implementation of the C<SvNVx> macro for compilers which can't
cope with complex macro expressions.  Always use the macro instead.

=cut
*/

NV
Perl_sv_nv(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_NV;

    if (SvNOK(sv))
	return SvNVX(sv);
    return sv_2nv(sv);
}

/*
=for apidoc sv_pv

Use the C<SvPV_nolen> macro instead

=for apidoc sv_pvn

A private implementation of the C<SvPV> macro for compilers which can't
cope with complex macro expressions.  Always use the macro instead.

=cut
*/

char *
Perl_sv_pvn(pTHX_ SV *sv, STRLEN *lp)
{
    PERL_ARGS_ASSERT_SV_PVN;

    if (SvPOK(sv)) {
	*lp = SvCUR(sv);
	return SvPVX(sv);
    }
    return sv_2pv(sv, lp);
}


char *
Perl_sv_pvn_nomg(pTHX_ SV *sv, STRLEN *lp)
{
    PERL_ARGS_ASSERT_SV_PVN_NOMG;

    if (SvPOK(sv)) {
	*lp = SvCUR(sv);
	return SvPVX(sv);
    }
    return sv_2pv_flags(sv, lp, 0);
}

/* sv_pv() is now a macro using SvPV_nolen();
 * this function provided for binary compatibility only
 */

char *
Perl_sv_pv(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_PV;

    if (SvPOK(sv))
        return SvPVX(sv);

    return sv_2pv(sv, NULL);
}

/* sv_pvn_force() is now a macro using Perl_sv_pvn_force_flags();
 * this function provided for binary compatibility only
 */

char *
Perl_sv_pvn_force(pTHX_ SV *sv, STRLEN *lp)
{
    PERL_ARGS_ASSERT_SV_PVN_FORCE;

    return sv_pvn_force_flags(sv, lp, SV_GMAGIC);
}

/* sv_pvbyte () is now a macro using Perl_sv_2pv_flags();
 * this function provided for binary compatibility only
 */

char *
Perl_sv_pvbyte(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_PVBYTE;

    sv_utf8_downgrade(sv, FALSE);
    return sv_pv(sv);
}

/*
=for apidoc sv_pvbyte

Use C<SvPVbyte_nolen> instead.

=for apidoc sv_pvbyten

A private implementation of the C<SvPVbyte> macro for compilers
which can't cope with complex macro expressions.  Always use the macro
instead.

=cut
*/

char *
Perl_sv_pvbyten(pTHX_ SV *sv, STRLEN *lp)
{
    PERL_ARGS_ASSERT_SV_PVBYTEN;

    sv_utf8_downgrade(sv, FALSE);
    return sv_pvn(sv,lp);
}

/* sv_pvutf8 () is now a macro using Perl_sv_2pv_flags();
 * this function provided for binary compatibility only
 */

char *
Perl_sv_pvutf8(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_PVUTF8;

    sv_utf8_upgrade(sv);
    return sv_pv(sv);
}

/*
=for apidoc sv_pvutf8

Use the C<SvPVutf8_nolen> macro instead

=for apidoc sv_pvutf8n

A private implementation of the C<SvPVutf8> macro for compilers
which can't cope with complex macro expressions.  Always use the macro
instead.

=cut
*/

char *
Perl_sv_pvutf8n(pTHX_ SV *sv, STRLEN *lp)
{
    PERL_ARGS_ASSERT_SV_PVUTF8N;

    sv_utf8_upgrade(sv);
    return sv_pvn(sv,lp);
}

/* sv_utf8_upgrade() is now a macro using sv_utf8_upgrade_flags();
 * this function provided for binary compatibility only
 */

STRLEN
Perl_sv_utf8_upgrade(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_UTF8_UPGRADE;

    return sv_utf8_upgrade_flags(sv, SV_GMAGIC);
}

int
Perl_fprintf_nocontext(PerlIO *stream, const char *format, ...)
{
    int ret = 0;
    va_list arglist;

    /* Easier to special case this here than in embed.pl. (Look at what it
       generates for proto.h) */
#ifdef PERL_IMPLICIT_CONTEXT
    PERL_ARGS_ASSERT_FPRINTF_NOCONTEXT;
#endif

    va_start(arglist, format);
    ret = PerlIO_vprintf(stream, format, arglist);
    va_end(arglist);
    return ret;
}

int
Perl_printf_nocontext(const char *format, ...)
{
    dTHX;
    va_list arglist;
    int ret = 0;

#ifdef PERL_IMPLICIT_CONTEXT
    PERL_ARGS_ASSERT_PRINTF_NOCONTEXT;
#endif

    va_start(arglist, format);
    ret = PerlIO_vprintf(PerlIO_stdout(), format, arglist);
    va_end(arglist);
    return ret;
}

#if defined(HUGE_VAL) || (defined(USE_LONG_DOUBLE) && defined(HUGE_VALL))
/*
 * This hack is to force load of "huge" support from libm.a
 * So it is in perl for (say) POSIX to use.
 * Needed for SunOS with Sun's 'acc' for example.
 */
NV
Perl_huge(void)
{
#  if defined(USE_LONG_DOUBLE) && defined(HUGE_VALL)
    return HUGE_VALL;
#  else
    return HUGE_VAL;
#  endif
}
#endif

/* compatibility with versions <= 5.003. */
void
Perl_gv_fullname(pTHX_ SV *sv, const GV *gv)
{
    PERL_ARGS_ASSERT_GV_FULLNAME;

    gv_fullname3(sv, gv, sv == (const SV*)gv ? "*" : "");
}

/* compatibility with versions <= 5.003. */
void
Perl_gv_efullname(pTHX_ SV *sv, const GV *gv)
{
    PERL_ARGS_ASSERT_GV_EFULLNAME;

    gv_efullname3(sv, gv, sv == (const SV*)gv ? "*" : "");
}

void
Perl_gv_fullname3(pTHX_ SV *sv, const GV *gv, const char *prefix)
{
    PERL_ARGS_ASSERT_GV_FULLNAME3;

    gv_fullname4(sv, gv, prefix, TRUE);
}

void
Perl_gv_efullname3(pTHX_ SV *sv, const GV *gv, const char *prefix)
{
    PERL_ARGS_ASSERT_GV_EFULLNAME3;

    gv_efullname4(sv, gv, prefix, TRUE);
}

/*
=for apidoc gv_fetchmethod

See L</gv_fetchmethod_autoload>.

=cut
*/

GV *
Perl_gv_fetchmethod(pTHX_ HV *stash, const char *name)
{
    PERL_ARGS_ASSERT_GV_FETCHMETHOD;

    return gv_fetchmethod_autoload(stash, name, TRUE);
}

HE *
Perl_hv_iternext(pTHX_ HV *hv)
{
    PERL_ARGS_ASSERT_HV_ITERNEXT;

    return hv_iternext_flags(hv, 0);
}

void
Perl_hv_magic(pTHX_ HV *hv, GV *gv, int how)
{
    PERL_ARGS_ASSERT_HV_MAGIC;

    sv_magic(MUTABLE_SV(hv), MUTABLE_SV(gv), how, NULL, 0);
}

bool
Perl_do_open(pTHX_ GV *gv, const char *name, I32 len, int as_raw,
	     int rawmode, int rawperm, PerlIO *supplied_fp)
{
    PERL_ARGS_ASSERT_DO_OPEN;

    return do_openn(gv, name, len, as_raw, rawmode, rawperm,
		    supplied_fp, (SV **) NULL, 0);
}

bool
Perl_do_open9(pTHX_ GV *gv, const char *name, I32 len, int
as_raw,
              int rawmode, int rawperm, PerlIO *supplied_fp, SV *svs,
              I32 num_svs)
{
    PERL_ARGS_ASSERT_DO_OPEN9;

    PERL_UNUSED_ARG(num_svs);
    return do_openn(gv, name, len, as_raw, rawmode, rawperm,
                    supplied_fp, &svs, 1);
}

int
Perl_do_binmode(pTHX_ PerlIO *fp, int iotype, int mode)
{
 /* The old body of this is now in non-LAYER part of perlio.c
  * This is a stub for any XS code which might have been calling it.
  */
 const char *name = ":raw";

 PERL_ARGS_ASSERT_DO_BINMODE;

#ifdef PERLIO_USING_CRLF
 if (!(mode & O_BINARY))
     name = ":crlf";
#endif
 return PerlIO_binmode(aTHX_ fp, iotype, mode, name);
}

#ifndef OS2
bool
Perl_do_aexec(pTHX_ SV *really, SV **mark, SV **sp)
{
    PERL_ARGS_ASSERT_DO_AEXEC;

    return do_aexec5(really, mark, sp, 0, 0);
}
#endif

/* Backwards compatibility. */
int
Perl_init_i18nl14n(pTHX_ int printwarn)
{
    return init_i18nl10n(printwarn);
}

bool
Perl_is_utf8_string_loc(const U8 *s, const STRLEN len, const U8 **ep)
{
    PERL_ARGS_ASSERT_IS_UTF8_STRING_LOC;

    return is_utf8_string_loclen(s, len, ep, 0);
}

/*
=for apidoc sv_nolocking

Dummy routine which "locks" an SV when there is no locking module present.
Exists to avoid test for a C<NULL> function pointer and because it could
potentially warn under some level of strict-ness.

"Superseded" by C<sv_nosharing()>.

=cut
*/

void
Perl_sv_nolocking(pTHX_ SV *sv)
{
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(sv);
}


/*
=for apidoc sv_nounlocking

Dummy routine which "unlocks" an SV when there is no locking module present.
Exists to avoid test for a C<NULL> function pointer and because it could
potentially warn under some level of strict-ness.

"Superseded" by C<sv_nosharing()>.

=cut
*/

void
Perl_sv_nounlocking(pTHX_ SV *sv)
{
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(sv);
}

void
Perl_save_long(pTHX_ long int *longp)
{
    PERL_ARGS_ASSERT_SAVE_LONG;

    SSCHECK(3);
    SSPUSHLONG(*longp);
    SSPUSHPTR(longp);
    SSPUSHUV(SAVEt_LONG);
}

void
Perl_save_nogv(pTHX_ GV *gv)
{
    PERL_ARGS_ASSERT_SAVE_NOGV;

    SSCHECK(2);
    SSPUSHPTR(gv);
    SSPUSHUV(SAVEt_NSTAB);
}

void
Perl_save_list(pTHX_ SV **sarg, I32 maxsarg)
{
    I32 i;

    PERL_ARGS_ASSERT_SAVE_LIST;

    for (i = 1; i <= maxsarg; i++) {
	SV *sv;
	SvGETMAGIC(sarg[i]);
	sv = newSV(0);
	sv_setsv_nomg(sv,sarg[i]);
	SSCHECK(3);
	SSPUSHPTR(sarg[i]);		/* remember the pointer */
	SSPUSHPTR(sv);			/* remember the value */
	SSPUSHUV(SAVEt_ITEM);
    }
}

/*
=for apidoc sv_usepvn_mg

Like C<sv_usepvn>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_usepvn_mg(pTHX_ SV *sv, char *ptr, STRLEN len)
{
    PERL_ARGS_ASSERT_SV_USEPVN_MG;

    sv_usepvn_flags(sv,ptr,len, SV_SMAGIC);
}

/*
=for apidoc sv_usepvn

Tells an SV to use C<ptr> to find its string value.  Implemented by
calling C<sv_usepvn_flags> with C<flags> of 0, hence does not handle 'set'
magic.  See C<L</sv_usepvn_flags>>.

=cut
*/

void
Perl_sv_usepvn(pTHX_ SV *sv, char *ptr, STRLEN len)
{
    PERL_ARGS_ASSERT_SV_USEPVN;

    sv_usepvn_flags(sv,ptr,len, 0);
}

/*
=for apidoc unpack_str

The engine implementing C<unpack()> Perl function.  Note: parameters C<strbeg>,
C<new_s> and C<ocnt> are not used.  This call should not be used, use
C<unpackstring> instead.

=cut */

SSize_t
Perl_unpack_str(pTHX_ const char *pat, const char *patend, const char *s,
		const char *strbeg, const char *strend, char **new_s, I32 ocnt,
		U32 flags)
{
    PERL_ARGS_ASSERT_UNPACK_STR;

    PERL_UNUSED_ARG(strbeg);
    PERL_UNUSED_ARG(new_s);
    PERL_UNUSED_ARG(ocnt);

    return unpackstring(pat, patend, s, strend, flags);
}

/*
=for apidoc pack_cat

The engine implementing C<pack()> Perl function.  Note: parameters
C<next_in_list> and C<flags> are not used.  This call should not be used; use
C<packlist> instead.

=cut
*/

void
Perl_pack_cat(pTHX_ SV *cat, const char *pat, const char *patend, SV **beglist, SV **endlist, SV ***next_in_list, U32 flags)
{
    PERL_ARGS_ASSERT_PACK_CAT;

    PERL_UNUSED_ARG(next_in_list);
    PERL_UNUSED_ARG(flags);

    packlist(cat, pat, patend, beglist, endlist);
}

HE *
Perl_hv_store_ent(pTHX_ HV *hv, SV *keysv, SV *val, U32 hash)
{
  return (HE *)hv_common(hv, keysv, NULL, 0, 0, HV_FETCH_ISSTORE, val, hash);
}

bool
Perl_hv_exists_ent(pTHX_ HV *hv, SV *keysv, U32 hash)
{
    PERL_ARGS_ASSERT_HV_EXISTS_ENT;

    return cBOOL(hv_common(hv, keysv, NULL, 0, 0, HV_FETCH_ISEXISTS, 0, hash));
}

HE *
Perl_hv_fetch_ent(pTHX_ HV *hv, SV *keysv, I32 lval, U32 hash)
{
    PERL_ARGS_ASSERT_HV_FETCH_ENT;

    return (HE *)hv_common(hv, keysv, NULL, 0, 0, 
		     (lval ? HV_FETCH_LVALUE : 0), NULL, hash);
}

SV *
Perl_hv_delete_ent(pTHX_ HV *hv, SV *keysv, I32 flags, U32 hash)
{
    PERL_ARGS_ASSERT_HV_DELETE_ENT;

    return MUTABLE_SV(hv_common(hv, keysv, NULL, 0, 0, flags | HV_DELETE, NULL,
				hash));
}

SV**
Perl_hv_store_flags(pTHX_ HV *hv, const char *key, I32 klen, SV *val, U32 hash,
		    int flags)
{
    return (SV**) hv_common(hv, NULL, key, klen, flags,
			    (HV_FETCH_ISSTORE|HV_FETCH_JUST_SV), val, hash);
}

SV**
Perl_hv_store(pTHX_ HV *hv, const char *key, I32 klen_i32, SV *val, U32 hash)
{
    STRLEN klen;
    int flags;

    if (klen_i32 < 0) {
	klen = -klen_i32;
	flags = HVhek_UTF8;
    } else {
	klen = klen_i32;
	flags = 0;
    }
    return (SV **) hv_common(hv, NULL, key, klen, flags,
			     (HV_FETCH_ISSTORE|HV_FETCH_JUST_SV), val, hash);
}

bool
Perl_hv_exists(pTHX_ HV *hv, const char *key, I32 klen_i32)
{
    STRLEN klen;
    int flags;

    PERL_ARGS_ASSERT_HV_EXISTS;

    if (klen_i32 < 0) {
	klen = -klen_i32;
	flags = HVhek_UTF8;
    } else {
	klen = klen_i32;
	flags = 0;
    }
    return cBOOL(hv_common(hv, NULL, key, klen, flags, HV_FETCH_ISEXISTS, 0, 0));
}

SV**
Perl_hv_fetch(pTHX_ HV *hv, const char *key, I32 klen_i32, I32 lval)
{
    STRLEN klen;
    int flags;

    PERL_ARGS_ASSERT_HV_FETCH;

    if (klen_i32 < 0) {
	klen = -klen_i32;
	flags = HVhek_UTF8;
    } else {
	klen = klen_i32;
	flags = 0;
    }
    return (SV **) hv_common(hv, NULL, key, klen, flags,
			     lval ? (HV_FETCH_JUST_SV | HV_FETCH_LVALUE)
			     : HV_FETCH_JUST_SV, NULL, 0);
}

SV *
Perl_hv_delete(pTHX_ HV *hv, const char *key, I32 klen_i32, I32 flags)
{
    STRLEN klen;
    int k_flags;

    PERL_ARGS_ASSERT_HV_DELETE;

    if (klen_i32 < 0) {
	klen = -klen_i32;
	k_flags = HVhek_UTF8;
    } else {
	klen = klen_i32;
	k_flags = 0;
    }
    return MUTABLE_SV(hv_common(hv, NULL, key, klen, k_flags, flags | HV_DELETE,
				NULL, 0));
}

AV *
Perl_newAV(pTHX)
{
    return MUTABLE_AV(newSV_type(SVt_PVAV));
    /* sv_upgrade does AvREAL_only():
    AvALLOC(av) = 0;
    AvARRAY(av) = NULL;
    AvMAX(av) = AvFILLp(av) = -1; */
}

HV *
Perl_newHV(pTHX)
{
    HV * const hv = MUTABLE_HV(newSV_type(SVt_PVHV));
    assert(!SvOK(hv));

    return hv;
}

void
Perl_sv_insert(pTHX_ SV *const bigstr, const STRLEN offset, const STRLEN len, 
              const char *const little, const STRLEN littlelen)
{
    PERL_ARGS_ASSERT_SV_INSERT;
    sv_insert_flags(bigstr, offset, len, little, littlelen, SV_GMAGIC);
}

void
Perl_save_freesv(pTHX_ SV *sv)
{
    save_freesv(sv);
}

void
Perl_save_mortalizesv(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SAVE_MORTALIZESV;

    save_mortalizesv(sv);
}

void
Perl_save_freeop(pTHX_ OP *o)
{
    save_freeop(o);
}

void
Perl_save_freepv(pTHX_ char *pv)
{
    save_freepv(pv);
}

void
Perl_save_op(pTHX)
{
    save_op();
}

#ifdef PERL_DONT_CREATE_GVSV
GV *
Perl_gv_SVadd(pTHX_ GV *gv)
{
    return gv_SVadd(gv);
}
#endif

GV *
Perl_gv_AVadd(pTHX_ GV *gv)
{
    return gv_AVadd(gv);
}

GV *
Perl_gv_HVadd(pTHX_ GV *gv)
{
    return gv_HVadd(gv);
}

GV *
Perl_gv_IOadd(pTHX_ GV *gv)
{
    return gv_IOadd(gv);
}

IO *
Perl_newIO(pTHX)
{
    return MUTABLE_IO(newSV_type(SVt_PVIO));
}

I32
Perl_my_stat(pTHX)
{
    return my_stat_flags(SV_GMAGIC);
}

I32
Perl_my_lstat(pTHX)
{
    return my_lstat_flags(SV_GMAGIC);
}

I32
Perl_sv_eq(pTHX_ SV *sv1, SV *sv2)
{
    return sv_eq_flags(sv1, sv2, SV_GMAGIC);
}

#ifdef USE_LOCALE_COLLATE
char *
Perl_sv_collxfrm(pTHX_ SV *const sv, STRLEN *const nxp)
{
    PERL_ARGS_ASSERT_SV_COLLXFRM;
    return sv_collxfrm_flags(sv, nxp, SV_GMAGIC);
}

char *
Perl_mem_collxfrm(pTHX_ const char *input_string, STRLEN len, STRLEN *xlen)
{
    /* This function is retained for compatibility in case someone outside core
     * is using this (but it is undocumented) */

    PERL_ARGS_ASSERT_MEM_COLLXFRM;

    return _mem_collxfrm(input_string, len, xlen, FALSE);
}

#endif

bool
Perl_sv_2bool(pTHX_ SV *const sv)
{
    PERL_ARGS_ASSERT_SV_2BOOL;
    return sv_2bool_flags(sv, SV_GMAGIC);
}


/*
=for apidoc custom_op_name
Return the name for a given custom op.  This was once used by the C<OP_NAME>
macro, but is no longer: it has only been kept for compatibility, and
should not be used.

=for apidoc custom_op_desc
Return the description of a given custom op.  This was once used by the
C<OP_DESC> macro, but is no longer: it has only been kept for
compatibility, and should not be used.

=cut
*/

const char*
Perl_custom_op_name(pTHX_ const OP* o)
{
    PERL_ARGS_ASSERT_CUSTOM_OP_NAME;
    return XopENTRYCUSTOM(o, xop_name);
}

const char*
Perl_custom_op_desc(pTHX_ const OP* o)
{
    PERL_ARGS_ASSERT_CUSTOM_OP_DESC;
    return XopENTRYCUSTOM(o, xop_desc);
}

CV *
Perl_newSUB(pTHX_ I32 floor, OP *o, OP *proto, OP *block)
{
    return newATTRSUB(floor, o, proto, NULL, block);
}

UV
Perl_to_utf8_fold(pTHX_ const U8 *p, U8* ustrp, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UTF8_FOLD;

    return toFOLD_utf8(p, ustrp, lenp);
}

UV
Perl_to_utf8_lower(pTHX_ const U8 *p, U8* ustrp, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UTF8_LOWER;

    return toLOWER_utf8(p, ustrp, lenp);
}

UV
Perl_to_utf8_title(pTHX_ const U8 *p, U8* ustrp, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UTF8_TITLE;

    return toTITLE_utf8(p, ustrp, lenp);
}

UV
Perl_to_utf8_upper(pTHX_ const U8 *p, U8* ustrp, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UTF8_UPPER;

    return toUPPER_utf8(p, ustrp, lenp);
}

SV *
Perl_sv_mortalcopy(pTHX_ SV *const oldstr)
{
    return Perl_sv_mortalcopy_flags(aTHX_ oldstr, SV_GMAGIC);
}

void
Perl_sv_copypv(pTHX_ SV *const dsv, SV *const ssv)
{
    PERL_ARGS_ASSERT_SV_COPYPV;

    sv_copypv_flags(dsv, ssv, SV_GMAGIC);
}

UV      /* Made into a function, so can be deprecated */
NATIVE_TO_NEED(const UV enc, const UV ch)
{
    PERL_UNUSED_ARG(enc);
    return ch;
}

UV      /* Made into a function, so can be deprecated */
ASCII_TO_NEED(const UV enc, const UV ch)
{
    PERL_UNUSED_ARG(enc);
    return ch;
}

bool      /* Made into a function, so can be deprecated */
Perl_isIDFIRST_lazy(pTHX_ const char* p)
{
    PERL_ARGS_ASSERT_ISIDFIRST_LAZY;

    return isIDFIRST_lazy_if(p,1);
}

bool      /* Made into a function, so can be deprecated */
Perl_isALNUM_lazy(pTHX_ const char* p)
{
    PERL_ARGS_ASSERT_ISALNUM_LAZY;

    return isALNUM_lazy_if(p,1);
}

bool
Perl_is_uni_alnum(pTHX_ UV c)
{
    return isWORDCHAR_uni(c);
}

bool
Perl_is_uni_alnumc(pTHX_ UV c)
{
    return isALNUM_uni(c);
}

bool
Perl_is_uni_alpha(pTHX_ UV c)
{
    return isALPHA_uni(c);
}

bool
Perl_is_uni_ascii(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isASCII_uni(c);
}

bool
Perl_is_uni_blank(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isBLANK_uni(c);
}

bool
Perl_is_uni_space(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isSPACE_uni(c);
}

bool
Perl_is_uni_digit(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isDIGIT_uni(c);
}

bool
Perl_is_uni_upper(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isUPPER_uni(c);
}

bool
Perl_is_uni_lower(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isLOWER_uni(c);
}

bool
Perl_is_uni_cntrl(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isCNTRL_L1(c);
}

bool
Perl_is_uni_graph(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isGRAPH_uni(c);
}

bool
Perl_is_uni_print(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isPRINT_uni(c);
}

bool
Perl_is_uni_punct(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isPUNCT_uni(c);
}

bool
Perl_is_uni_xdigit(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isXDIGIT_uni(c);
}

bool
Perl_is_uni_alnum_lc(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isWORDCHAR_LC_uvchr(c);
}

bool
Perl_is_uni_alnumc_lc(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isALPHANUMERIC_LC_uvchr(c);
}

bool
Perl_is_uni_idfirst_lc(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    /* XXX Should probably be something that resolves to the old IDFIRST, but
     * this function is deprecated, so not bothering */
    return isIDFIRST_LC_uvchr(c);
}

bool
Perl_is_uni_alpha_lc(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isALPHA_LC_uvchr(c);
}

bool
Perl_is_uni_ascii_lc(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isASCII_LC_uvchr(c);
}

bool
Perl_is_uni_blank_lc(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isBLANK_LC_uvchr(c);
}

bool
Perl_is_uni_space_lc(pTHX_ UV c)
{
    PERL_UNUSED_CONTEXT;
    return isSPACE_LC_uvchr(c);
}

bool
Perl_is_uni_digit_lc(pTHX_ UV c)
{
    return isDIGIT_LC_uvchr(c);
}

bool
Perl_is_uni_idfirst(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return _is_utf8_idstart(tmpbuf);
}

bool
Perl_is_utf8_idfirst(pTHX_ const U8 *p) /* The naming is historical. */
{
    PERL_ARGS_ASSERT_IS_UTF8_IDFIRST;

    return _is_utf8_idstart(p);
}

bool
Perl_is_utf8_xidfirst(pTHX_ const U8 *p) /* The naming is historical. */
{
    PERL_ARGS_ASSERT_IS_UTF8_XIDFIRST;

    return _is_utf8_xidstart(p);
}

bool
Perl_is_utf8_idcont(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_IDCONT;

    return _is_utf8_idcont(p);
}

bool
Perl_is_utf8_xidcont(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_XIDCONT;

    return _is_utf8_xidcont(p);
}

bool
Perl_is_uni_upper_lc(pTHX_ UV c)
{
    return isUPPER_LC_uvchr(c);
}

bool
Perl_is_uni_lower_lc(pTHX_ UV c)
{
    return isLOWER_LC_uvchr(c);
}

bool
Perl_is_uni_cntrl_lc(pTHX_ UV c)
{
    return isCNTRL_LC_uvchr(c);
}

bool
Perl_is_uni_graph_lc(pTHX_ UV c)
{
    return isGRAPH_LC_uvchr(c);
}

bool
Perl_is_uni_print_lc(pTHX_ UV c)
{
    return isPRINT_LC_uvchr(c);
}

bool
Perl_is_uni_punct_lc(pTHX_ UV c)
{
    return isPUNCT_LC_uvchr(c);
}

bool
Perl_is_uni_xdigit_lc(pTHX_ UV c)
{
    return isXDIGIT_LC_uvchr(c);
}

U32
Perl_to_uni_upper_lc(pTHX_ U32 c)
{
    /* XXX returns only the first character -- do not use XXX */
    /* XXX no locale support yet */
    STRLEN len;
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    return (U32)to_uni_upper(c, tmpbuf, &len);
}

U32
Perl_to_uni_title_lc(pTHX_ U32 c)
{
    /* XXX returns only the first character XXX -- do not use XXX */
    /* XXX no locale support yet */
    STRLEN len;
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    return (U32)to_uni_title(c, tmpbuf, &len);
}

U32
Perl_to_uni_lower_lc(pTHX_ U32 c)
{
    /* XXX returns only the first character -- do not use XXX */
    /* XXX no locale support yet */
    STRLEN len;
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    return (U32)to_uni_lower(c, tmpbuf, &len);
}

bool
Perl_is_utf8_alnum(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_ALNUM;

    /* NOTE: "IsWord", not "IsAlnum", since Alnum is a true
     * descendant of isalnum(3), in other words, it doesn't
     * contain the '_'. --jhi */
    return isWORDCHAR_utf8(p);
}

bool
Perl_is_utf8_alnumc(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_ALNUMC;

    return isALPHANUMERIC_utf8(p);
}

bool
Perl_is_utf8_alpha(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_ALPHA;

    return isALPHA_utf8(p);
}

bool
Perl_is_utf8_ascii(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_ASCII;
    PERL_UNUSED_CONTEXT;

    return isASCII_utf8(p);
}

bool
Perl_is_utf8_blank(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_BLANK;
    PERL_UNUSED_CONTEXT;

    return isBLANK_utf8(p);
}

bool
Perl_is_utf8_space(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_SPACE;
    PERL_UNUSED_CONTEXT;

    return isSPACE_utf8(p);
}

bool
Perl_is_utf8_perl_space(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_PERL_SPACE;
    PERL_UNUSED_CONTEXT;

    /* Only true if is an ASCII space-like character, and ASCII is invariant
     * under utf8, so can just use the macro */
    return isSPACE_A(*p);
}

bool
Perl_is_utf8_perl_word(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_PERL_WORD;
    PERL_UNUSED_CONTEXT;

    /* Only true if is an ASCII word character, and ASCII is invariant
     * under utf8, so can just use the macro */
    return isWORDCHAR_A(*p);
}

bool
Perl_is_utf8_digit(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_DIGIT;

    return isDIGIT_utf8(p);
}

bool
Perl_is_utf8_posix_digit(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_POSIX_DIGIT;
    PERL_UNUSED_CONTEXT;

    /* Only true if is an ASCII digit character, and ASCII is invariant
     * under utf8, so can just use the macro */
    return isDIGIT_A(*p);
}

bool
Perl_is_utf8_upper(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_UPPER;

    return isUPPER_utf8(p);
}

bool
Perl_is_utf8_lower(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_LOWER;

    return isLOWER_utf8(p);
}

bool
Perl_is_utf8_cntrl(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_CNTRL;
    PERL_UNUSED_CONTEXT;

    return isCNTRL_utf8(p);
}

bool
Perl_is_utf8_graph(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_GRAPH;

    return isGRAPH_utf8(p);
}

bool
Perl_is_utf8_print(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_PRINT;

    return isPRINT_utf8(p);
}

bool
Perl_is_utf8_punct(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_PUNCT;

    return isPUNCT_utf8(p);
}

bool
Perl_is_utf8_xdigit(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_XDIGIT;
    PERL_UNUSED_CONTEXT;

    return isXDIGIT_utf8(p);
}

bool
Perl_is_utf8_mark(pTHX_ const U8 *p)
{
    PERL_ARGS_ASSERT_IS_UTF8_MARK;

    return _is_utf8_mark(p);
}

/*
=for apidoc is_utf8_char

Tests if some arbitrary number of bytes begins in a valid UTF-8
character.  Note that an INVARIANT (i.e. ASCII on non-EBCDIC machines)
character is a valid UTF-8 character.  The actual number of bytes in the UTF-8
character will be returned if it is valid, otherwise 0.

This function is deprecated due to the possibility that malformed input could
cause reading beyond the end of the input buffer.  Use L</isUTF8_CHAR>
instead.

=cut */

STRLEN
Perl_is_utf8_char(const U8 *s)
{
    PERL_ARGS_ASSERT_IS_UTF8_CHAR;

    /* Assumes we have enough space, which is why this is deprecated.  But the
     * strnlen() makes it safe for the common case of NUL-terminated strings */
    return isUTF8_CHAR(s, s + my_strnlen((char *) s, UTF8SKIP(s)));
}

/*
=for apidoc is_utf8_char_buf

This is identical to the macro L</isUTF8_CHAR>.

=cut */

STRLEN
Perl_is_utf8_char_buf(const U8 *buf, const U8* buf_end)
{

    PERL_ARGS_ASSERT_IS_UTF8_CHAR_BUF;

    return isUTF8_CHAR(buf, buf_end);
}

/* DEPRECATED!
 * Like L</utf8_to_uvuni_buf>(), but should only be called when it is known that
 * there are no malformations in the input UTF-8 string C<s>.  Surrogates,
 * non-character code points, and non-Unicode code points are allowed */

UV
Perl_valid_utf8_to_uvuni(pTHX_ const U8 *s, STRLEN *retlen)
{
    PERL_UNUSED_CONTEXT;
    PERL_ARGS_ASSERT_VALID_UTF8_TO_UVUNI;

    return NATIVE_TO_UNI(valid_utf8_to_uvchr(s, retlen));
}

/*
=for apidoc utf8_to_uvuni

Returns the Unicode code point of the first character in the string C<s>
which is assumed to be in UTF-8 encoding; C<retlen> will be set to the
length, in bytes, of that character.

Some, but not all, UTF-8 malformations are detected, and in fact, some
malformed input could cause reading beyond the end of the input buffer, which
is one reason why this function is deprecated.  The other is that only in
extremely limited circumstances should the Unicode versus native code point be
of any interest to you.  See L</utf8_to_uvuni_buf> for alternatives.

If C<s> points to one of the detected malformations, and UTF8 warnings are
enabled, zero is returned and C<*retlen> is set (if C<retlen> doesn't point to
NULL) to -1.  If those warnings are off, the computed value if well-defined (or
the Unicode REPLACEMENT CHARACTER, if not) is silently returned, and C<*retlen>
is set (if C<retlen> isn't NULL) so that (S<C<s> + C<*retlen>>) is the
next possible position in C<s> that could begin a non-malformed character.
See L</utf8n_to_uvchr> for details on when the REPLACEMENT CHARACTER is returned.

=cut
*/

UV
Perl_utf8_to_uvuni(pTHX_ const U8 *s, STRLEN *retlen)
{
    PERL_UNUSED_CONTEXT;
    PERL_ARGS_ASSERT_UTF8_TO_UVUNI;

    return NATIVE_TO_UNI(valid_utf8_to_uvchr(s, retlen));
}

/*
=for apidoc Am|HV *|pad_compname_type|PADOFFSET po

Looks up the type of the lexical variable at position C<po> in the
currently-compiling pad.  If the variable is typed, the stash of the
class to which it is typed is returned.  If not, C<NULL> is returned.

=cut
*/

HV *
Perl_pad_compname_type(pTHX_ const PADOFFSET po)
{
    return PAD_COMPNAME_TYPE(po);
}

/* return ptr to little string in big string, NULL if not found */
/* The original version of this routine was donated by Corey Satten. */

char *
Perl_instr(const char *big, const char *little)
{
    PERL_ARGS_ASSERT_INSTR;

    return instr((char *) big, (char *) little);
}

SV *
Perl_newSVsv(pTHX_ SV *const old)
{
    return newSVsv(old);
}

#endif /* NO_MATHOMS */

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
