/*    mathoms.c
 *
 *    Copyright (C) 2005, 2006, 2007, 2008 by Larry Wall and others
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
 * versions of Perl.  For binary or source compatibility reasons, though,
 * we cannot completely remove them from the core code.  
 *
 * SMP - Oct. 24, 2005
 *
 */

#include "EXTERN.h"
#define PERL_IN_MATHOMS_C
#include "perl.h"

#ifdef NO_MATHOMS
/* ..." warning: ISO C forbids an empty source file"
   So make sure we have something in here by processing the headers anyway.
 */
#else

PERL_CALLCONV OP * Perl_ref(pTHX_ OP *o, I32 type);
PERL_CALLCONV void Perl_sv_unref(pTHX_ SV *sv);
PERL_CALLCONV void Perl_sv_taint(pTHX_ SV *sv);
PERL_CALLCONV IV Perl_sv_2iv(pTHX_ register SV *sv);
PERL_CALLCONV UV Perl_sv_2uv(pTHX_ register SV *sv);
PERL_CALLCONV char * Perl_sv_2pv(pTHX_ register SV *sv, STRLEN *lp);
PERL_CALLCONV char * Perl_sv_2pv_nolen(pTHX_ register SV *sv);
PERL_CALLCONV char * Perl_sv_2pvbyte_nolen(pTHX_ register SV *sv);
PERL_CALLCONV char * Perl_sv_2pvutf8_nolen(pTHX_ register SV *sv);
PERL_CALLCONV void Perl_sv_force_normal(pTHX_ register SV *sv);
PERL_CALLCONV void Perl_sv_setsv(pTHX_ SV *dstr, register SV *sstr);
PERL_CALLCONV void Perl_sv_catpvn(pTHX_ SV *dsv, const char* sstr, STRLEN slen);
PERL_CALLCONV void Perl_sv_catpvn_mg(pTHX_ register SV *sv, register const char *ptr, register STRLEN len);
PERL_CALLCONV void Perl_sv_catsv(pTHX_ SV *dstr, register SV *sstr);
PERL_CALLCONV void Perl_sv_catsv_mg(pTHX_ SV *dsv, register SV *ssv);
PERL_CALLCONV char * Perl_sv_pv(pTHX_ SV *sv);
PERL_CALLCONV char * Perl_sv_pvn_force(pTHX_ SV *sv, STRLEN *lp);
PERL_CALLCONV char * Perl_sv_pvbyte(pTHX_ SV *sv);
PERL_CALLCONV char * Perl_sv_pvutf8(pTHX_ SV *sv);
PERL_CALLCONV STRLEN Perl_sv_utf8_upgrade(pTHX_ register SV *sv);
PERL_CALLCONV NV Perl_huge(void);
PERL_CALLCONV void Perl_gv_fullname3(pTHX_ SV *sv, const GV *gv, const char *prefix);
PERL_CALLCONV void Perl_gv_efullname3(pTHX_ SV *sv, const GV *gv, const char *prefix);
PERL_CALLCONV GV * Perl_gv_fetchmethod(pTHX_ HV *stash, const char *name);
PERL_CALLCONV HE * Perl_hv_iternext(pTHX_ HV *hv);
PERL_CALLCONV void Perl_hv_magic(pTHX_ HV *hv, GV *gv, int how);
PERL_CALLCONV bool Perl_do_open(pTHX_ GV *gv, register const char *name, I32 len, int as_raw, int rawmode, int rawperm, PerlIO *supplied_fp);
PERL_CALLCONV bool Perl_do_aexec(pTHX_ SV *really, register SV **mark, register SV **sp);
PERL_CALLCONV bool Perl_do_exec(pTHX_ const char *cmd);
PERL_CALLCONV U8 * Perl_uvuni_to_utf8(pTHX_ U8 *d, UV uv);
PERL_CALLCONV bool Perl_is_utf8_string_loc(pTHX_ const U8 *s, STRLEN len, const U8 **ep);
PERL_CALLCONV void Perl_sv_nolocking(pTHX_ SV *sv);
PERL_CALLCONV void Perl_sv_usepvn_mg(pTHX_ SV *sv, char *ptr, STRLEN len);
PERL_CALLCONV void Perl_sv_usepvn(pTHX_ SV *sv, char *ptr, STRLEN len);
PERL_CALLCONV int Perl_fprintf_nocontext(PerlIO *stream, const char *format, ...);
PERL_CALLCONV int Perl_printf_nocontext(const char *format, ...);
PERL_CALLCONV int Perl_magic_setglob(pTHX_ SV* sv, MAGIC* mg);
PERL_CALLCONV AV * Perl_newAV(pTHX);
PERL_CALLCONV HV * Perl_newHV(pTHX);

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
being zero.  See C<SvROK_off>.

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

Taint an SV. Use C<SvTAINTED_on> instead.
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
Perl_sv_2iv(pTHX_ register SV *sv)
{
    return sv_2iv_flags(sv, SV_GMAGIC);
}

/* sv_2uv() is now a macro using Perl_sv_2uv_flags();
 * this function provided for binary compatibility only
 */

UV
Perl_sv_2uv(pTHX_ register SV *sv)
{
    return sv_2uv_flags(sv, SV_GMAGIC);
}

/* sv_2pv() is now a macro using Perl_sv_2pv_flags();
 * this function provided for binary compatibility only
 */

char *
Perl_sv_2pv(pTHX_ register SV *sv, STRLEN *lp)
{
    return sv_2pv_flags(sv, lp, SV_GMAGIC);
}

/*
=for apidoc sv_2pv_nolen

Like C<sv_2pv()>, but doesn't return the length too. You should usually
use the macro wrapper C<SvPV_nolen(sv)> instead.
=cut
*/

char *
Perl_sv_2pv_nolen(pTHX_ register SV *sv)
{
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
Perl_sv_2pvbyte_nolen(pTHX_ register SV *sv)
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
Perl_sv_2pvutf8_nolen(pTHX_ register SV *sv)
{
    PERL_ARGS_ASSERT_SV_2PVUTF8_NOLEN;

    return sv_2pvutf8(sv, NULL);
}

/*
=for apidoc sv_force_normal

Undo various types of fakery on an SV: if the PV is a shared string, make
a private copy; if we're a ref, stop refing; if we're a glob, downgrade to
an xpvmg. See also C<sv_force_normal_flags>.

=cut
*/

void
Perl_sv_force_normal(pTHX_ register SV *sv)
{
    PERL_ARGS_ASSERT_SV_FORCE_NORMAL;

    sv_force_normal_flags(sv, 0);
}

/* sv_setsv() is now a macro using Perl_sv_setsv_flags();
 * this function provided for binary compatibility only
 */

void
Perl_sv_setsv(pTHX_ SV *dstr, register SV *sstr)
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
Perl_sv_catpvn_mg(pTHX_ register SV *sv, register const char *ptr, register STRLEN len)
{
    PERL_ARGS_ASSERT_SV_CATPVN_MG;

    sv_catpvn_flags(sv,ptr,len,SV_GMAGIC|SV_SMAGIC);
}

/* sv_catsv() is now a macro using Perl_sv_catsv_flags();
 * this function provided for binary compatibility only
 */

void
Perl_sv_catsv(pTHX_ SV *dstr, register SV *sstr)
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
Perl_sv_catsv_mg(pTHX_ SV *dsv, register SV *ssv)
{
    PERL_ARGS_ASSERT_SV_CATSV_MG;

    sv_catsv_flags(dsv,ssv,SV_GMAGIC|SV_SMAGIC);
}

/*
=for apidoc sv_iv

A private implementation of the C<SvIVx> macro for compilers which can't
cope with complex macro expressions. Always use the macro instead.

=cut
*/

IV
Perl_sv_iv(pTHX_ register SV *sv)
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
cope with complex macro expressions. Always use the macro instead.

=cut
*/

UV
Perl_sv_uv(pTHX_ register SV *sv)
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
cope with complex macro expressions. Always use the macro instead.

=cut
*/

NV
Perl_sv_nv(pTHX_ register SV *sv)
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
cope with complex macro expressions. Always use the macro instead.

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
Perl_sv_pvn_nomg(pTHX_ register SV *sv, STRLEN *lp)
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
which can't cope with complex macro expressions. Always use the macro
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
which can't cope with complex macro expressions. Always use the macro
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
Perl_sv_utf8_upgrade(pTHX_ register SV *sv)
{
    PERL_ARGS_ASSERT_SV_UTF8_UPGRADE;

    return sv_utf8_upgrade_flags(sv, SV_GMAGIC);
}

int
Perl_fprintf_nocontext(PerlIO *stream, const char *format, ...)
{
    dTHXs;
    va_list(arglist);

    /* Easier to special case this here than in embed.pl. (Look at what it
       generates for proto.h) */
#ifdef PERL_IMPLICIT_CONTEXT
    PERL_ARGS_ASSERT_FPRINTF_NOCONTEXT;
#endif

    va_start(arglist, format);
    return PerlIO_vprintf(stream, format, arglist);
}

int
Perl_printf_nocontext(const char *format, ...)
{
    dTHX;
    va_list(arglist);

#ifdef PERL_IMPLICIT_CONTEXT
    PERL_ARGS_ASSERT_PRINTF_NOCONTEXT;
#endif

    va_start(arglist, format);
    return PerlIO_vprintf(PerlIO_stdout(), format, arglist);
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

See L<gv_fetchmethod_autoload>.

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

AV *
Perl_av_fake(pTHX_ register I32 size, register SV **strp)
{
    register SV** ary;
    register AV * const av = MUTABLE_AV(newSV_type(SVt_PVAV));

    PERL_ARGS_ASSERT_AV_FAKE;

    Newx(ary,size+1,SV*);
    AvALLOC(av) = ary;
    Copy(strp,ary,size,SV*);
    AvREIFY_only(av);
    AvARRAY(av) = ary;
    AvFILLp(av) = size - 1;
    AvMAX(av) = size - 1;
    while (size--) {
        assert (*strp);
        SvTEMP_off(*strp);
        strp++;
    }
    return av;
}

bool
Perl_do_open(pTHX_ GV *gv, register const char *name, I32 len, int as_raw,
	     int rawmode, int rawperm, PerlIO *supplied_fp)
{
    PERL_ARGS_ASSERT_DO_OPEN;

    return do_openn(gv, name, len, as_raw, rawmode, rawperm,
		    supplied_fp, (SV **) NULL, 0);
}

bool
Perl_do_open9(pTHX_ GV *gv, register const char *name, I32 len, int 
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
Perl_do_aexec(pTHX_ SV *really, register SV **mark, register SV **sp)
{
    PERL_ARGS_ASSERT_DO_AEXEC;

    return do_aexec5(really, mark, sp, 0, 0);
}
#endif

#ifdef PERL_DEFAULT_DO_EXEC3_IMPLEMENTATION
bool
Perl_do_exec(pTHX_ const char *cmd)
{
    PERL_ARGS_ASSERT_DO_EXEC;

    return do_exec3(cmd,0,0);
}
#endif

/* Backwards compatibility. */
int
Perl_init_i18nl14n(pTHX_ int printwarn)
{
    return init_i18nl10n(printwarn);
}

OP *
Perl_oopsCV(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_OOPSCV;

    Perl_croak(aTHX_ "NOT IMPL LINE %d",__LINE__);
    /* STUB */
    PERL_UNUSED_ARG(o);
    NORETURN_FUNCTION_END;
}

PP(pp_padany)
{
    DIE(aTHX_ "NOT IMPL LINE %d",__LINE__);
}

PP(pp_mapstart)
{
    DIE(aTHX_ "panic: mapstart");	/* uses grepstart */
}

/* These ops all have the same body as pp_null.  */
PP(pp_scalar)
{
    dVAR;
    return NORMAL;
}

PP(pp_regcmaybe)
{
    dVAR;
    return NORMAL;
}

PP(pp_lineseq)
{
    dVAR;
    return NORMAL;
}

PP(pp_scope)
{
    dVAR;
    return NORMAL;
}

/* Ops that are calls to do_kv.  */
PP(pp_values)
{
    return do_kv();
}

PP(pp_keys)
{
    return do_kv();
}

/* Ops that are simply calls to other ops.  */
PP(pp_dump)
{
    return pp_goto();
    /*NOTREACHED*/
}

PP(pp_dofile)
{
    return pp_require();
}

PP(pp_dbmclose)
{
    return pp_untie();
}

PP(pp_read)
{
    return pp_sysread();
}

PP(pp_recv)
{
    return pp_sysread();
}

PP(pp_seek)
{
    return pp_sysseek();
}

PP(pp_fcntl)
{
    return pp_ioctl();
}

PP(pp_gsockopt)
{
    return pp_ssockopt();
}

PP(pp_getsockname)
{
    return pp_getpeername();
}

PP(pp_lstat)
{
    return pp_stat();
}

PP(pp_fteowned)
{
    return pp_ftrowned();
}

PP(pp_ftbinary)
{
    return pp_fttext();
}

PP(pp_localtime)
{
    return pp_gmtime();
}

PP(pp_shmget)
{
    return pp_semget();
}

PP(pp_shmctl)
{
    return pp_semctl();
}

PP(pp_shmread)
{
    return pp_shmwrite();
}

PP(pp_msgget)
{
    return pp_semget();
}

PP(pp_msgctl)
{
    return pp_semctl();
}

PP(pp_ghbyname)
{
    return pp_ghostent();
}

PP(pp_ghbyaddr)
{
    return pp_ghostent();
}

PP(pp_gnbyname)
{
    return pp_gnetent();
}

PP(pp_gnbyaddr)
{
    return pp_gnetent();
}

PP(pp_gpbyname)
{
    return pp_gprotoent();
}

PP(pp_gpbynumber)
{
    return pp_gprotoent();
}

PP(pp_gsbyname)
{
    return pp_gservent();
}

PP(pp_gsbyport)
{
    return pp_gservent();
}

PP(pp_gpwnam)
{
    return pp_gpwent();
}

PP(pp_gpwuid)
{
    return pp_gpwent();
}

PP(pp_ggrnam)
{
    return pp_ggrent();
}

PP(pp_ggrgid)
{
    return pp_ggrent();
}

PP(pp_ftsize)
{
    return pp_ftis();
}

PP(pp_ftmtime)
{
    return pp_ftis();
}

PP(pp_ftatime)
{
    return pp_ftis();
}

PP(pp_ftctime)
{
    return pp_ftis();
}

PP(pp_ftzero)
{
    return pp_ftrowned();
}

PP(pp_ftsock)
{
    return pp_ftrowned();
}

PP(pp_ftchr)
{
    return pp_ftrowned();
}

PP(pp_ftblk)
{
    return pp_ftrowned();
}

PP(pp_ftfile)
{
    return pp_ftrowned();
}

PP(pp_ftdir)
{
    return pp_ftrowned();
}

PP(pp_ftpipe)
{
    return pp_ftrowned();
}

PP(pp_ftsuid)
{
    return pp_ftrowned();
}

PP(pp_ftsgid)
{
    return pp_ftrowned();
}

PP(pp_ftsvtx)
{
    return pp_ftrowned();
}

PP(pp_unlink)
{
    return pp_chown();
}

PP(pp_chmod)
{
    return pp_chown();
}

PP(pp_utime)
{
    return pp_chown();
}

PP(pp_kill)
{
    return pp_chown();
}

PP(pp_symlink)
{
    return pp_link();
}

PP(pp_ftrwrite)
{
    return pp_ftrread();
}

PP(pp_ftrexec)
{
    return pp_ftrread();
}

PP(pp_fteread)
{
    return pp_ftrread();
}

PP(pp_ftewrite)
{
    return pp_ftrread();
}

PP(pp_fteexec)
{
    return pp_ftrread();
}

PP(pp_msgsnd)
{
    return pp_shmwrite();
}

PP(pp_msgrcv)
{
    return pp_shmwrite();
}

PP(pp_syswrite)
{
    return pp_send();
}

PP(pp_semop)
{
    return pp_shmwrite();
}

PP(pp_dor)
{
    return pp_defined();
}

PP(pp_andassign)
{
    return pp_and();
}

PP(pp_orassign)
{
    return pp_or();
}

PP(pp_dorassign)
{
    return pp_defined();
} 

PP(pp_lcfirst)
{
    return pp_ucfirst();
}

PP(pp_slt)
{
    return pp_sle();
}

PP(pp_sgt)
{
    return pp_sle();
}

PP(pp_sge)
{
    return pp_sle();
}

PP(pp_rindex)
{
    return pp_index();
}

PP(pp_hex)
{
    return pp_oct();
}

PP(pp_pop)
{
    return pp_shift();
}

PP(pp_cos)
{
    return pp_sin();
}

PP(pp_exp)
{
    return pp_sin();
}

PP(pp_log)
{
    return pp_sin();
}

PP(pp_sqrt)
{
    return pp_sin();
}

PP(pp_bit_xor)
{
    return pp_bit_or();
}

PP(pp_rv2hv)
{
    return Perl_pp_rv2av(aTHX);
}

U8 *
Perl_uvuni_to_utf8(pTHX_ U8 *d, UV uv)
{
    PERL_ARGS_ASSERT_UVUNI_TO_UTF8;

    return Perl_uvuni_to_utf8_flags(aTHX_ d, uv, 0);
}

bool
Perl_is_utf8_string_loc(pTHX_ const U8 *s, STRLEN len, const U8 **ep)
{
    PERL_ARGS_ASSERT_IS_UTF8_STRING_LOC;

    return is_utf8_string_loclen(s, len, ep, 0);
}

/*
=for apidoc sv_nolocking

Dummy routine which "locks" an SV when there is no locking module present.
Exists to avoid test for a NULL function pointer and because it could
potentially warn under some level of strict-ness.

"Superseded" by sv_nosharing().

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
Exists to avoid test for a NULL function pointer and because it could
potentially warn under some level of strict-ness.

"Superseded" by sv_nosharing().

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
    dVAR;

    PERL_ARGS_ASSERT_SAVE_LONG;

    SSCHECK(3);
    SSPUSHLONG(*longp);
    SSPUSHPTR(longp);
    SSPUSHINT(SAVEt_LONG);
}

void
Perl_save_iv(pTHX_ IV *ivp)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_IV;

    SSCHECK(3);
    SSPUSHIV(*ivp);
    SSPUSHPTR(ivp);
    SSPUSHINT(SAVEt_IV);
}

void
Perl_save_nogv(pTHX_ GV *gv)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_NOGV;

    SSCHECK(2);
    SSPUSHPTR(gv);
    SSPUSHINT(SAVEt_NSTAB);
}

void
Perl_save_list(pTHX_ register SV **sarg, I32 maxsarg)
{
    dVAR;
    register I32 i;

    PERL_ARGS_ASSERT_SAVE_LIST;

    for (i = 1; i <= maxsarg; i++) {
	register SV * const sv = newSV(0);
	sv_setsv(sv,sarg[i]);
	SSCHECK(3);
	SSPUSHPTR(sarg[i]);		/* remember the pointer */
	SSPUSHPTR(sv);			/* remember the value */
	SSPUSHINT(SAVEt_ITEM);
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

Tells an SV to use C<ptr> to find its string value. Implemented by
calling C<sv_usepvn_flags> with C<flags> of 0, hence does not handle 'set'
magic. See C<sv_usepvn_flags>.

=cut
*/

void
Perl_sv_usepvn(pTHX_ SV *sv, char *ptr, STRLEN len)
{
    PERL_ARGS_ASSERT_SV_USEPVN;

    sv_usepvn_flags(sv,ptr,len, 0);
}

void
Perl_cv_ckproto(pTHX_ const CV *cv, const GV *gv, const char *p)
{
    PERL_ARGS_ASSERT_CV_CKPROTO;

    cv_ckproto_len(cv, gv, p, p ? strlen(p) : 0);
}

/*
=for apidoc unpack_str

The engine implementing unpack() Perl function. Note: parameters strbeg, new_s
and ocnt are not used. This call should not be used, use unpackstring instead.

=cut */

I32
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

The engine implementing pack() Perl function. Note: parameters next_in_list and
flags are not used. This call should not be used; use packlist instead.

=cut
*/

void
Perl_pack_cat(pTHX_ SV *cat, const char *pat, const char *patend, register SV **beglist, SV **endlist, SV ***next_in_list, U32 flags)
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

    return hv_common(hv, keysv, NULL, 0, 0, HV_FETCH_ISEXISTS, 0, hash)
	? TRUE : FALSE;
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
    return hv_common(hv, NULL, key, klen, flags, HV_FETCH_ISEXISTS, 0, 0)
	? TRUE : FALSE;
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

/* Functions after here were made mathoms post 5.10.0 but pre 5.8.9 */
int
Perl_magic_setglob(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_UNUSED_ARG(mg);
    PERL_UNUSED_ARG(sv);

    Perl_croak(aTHX_ "Perl_magic_setglob is dead code?");

    return 0;
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

int
Perl_magic_setbm(pTHX_ SV *sv, MAGIC *mg)
{
    return Perl_magic_setregexp(aTHX_ sv, mg);
}

int
Perl_magic_setfm(pTHX_ SV *sv, MAGIC *mg)
{
    return Perl_magic_setregexp(aTHX_ sv, mg);
}

void
Perl_sv_insert(pTHX_ SV *bigstr, STRLEN offset, STRLEN len, const char *little, STRLEN littlelen)
{
    PERL_ARGS_ASSERT_SV_INSERT;
    sv_insert_flags(bigstr, offset, len, little, littlelen, SV_GMAGIC);
}

void
Perl_save_freesv(pTHX_ SV *sv)
{
    dVAR;
    save_freesv(sv);
}

void
Perl_save_mortalizesv(pTHX_ SV *sv)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_MORTALIZESV;

    save_mortalizesv(sv);
}

void
Perl_save_freeop(pTHX_ OP *o)
{
    dVAR;
    save_freeop(o);
}

void
Perl_save_freepv(pTHX_ char *pv)
{
    dVAR;
    save_freepv(pv);
}

void
Perl_save_op(pTHX)
{
    dVAR;
    save_op();
}

#endif /* NO_MATHOMS */

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
