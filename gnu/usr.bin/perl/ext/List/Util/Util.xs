/* Copyright (c) 1997-2000 Graham Barr <gbarr@pobox.com>. All rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the same terms as Perl itself.
 */

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#ifndef PERL_VERSION
#    include <patchlevel.h>
#    if !(defined(PERL_VERSION) || (SUBVERSION > 0 && defined(PATCHLEVEL)))
#        include <could_not_find_Perl_patchlevel.h>
#    endif
#    define PERL_REVISION	5
#    define PERL_VERSION	PATCHLEVEL
#    define PERL_SUBVERSION	SUBVERSION
#endif

#ifndef aTHX
#  define aTHX
#  define pTHX
#endif

/* Some platforms have strict exports. And before 5.7.3 cxinc (or Perl_cxinc)
   was not exported. Therefore platforms like win32, VMS etc have problems
   so we redefine it here -- GMB
*/
#if PERL_VERSION < 7
/* Not in 5.6.1. */
#  define SvUOK(sv)           SvIOK_UV(sv)
#  ifdef cxinc
#    undef cxinc
#  endif
#  define cxinc() my_cxinc(aTHX)
static I32
my_cxinc(pTHX)
{
    cxstack_max = cxstack_max * 3 / 2;
    Renew(cxstack, cxstack_max + 1, struct context);      /* XXX should fix CXINC macro */
    return cxstack_ix + 1;
}
#endif

#if PERL_VERSION < 6
#    define NV double
#endif

#ifdef SVf_IVisUV
#  define slu_sv_value(sv) (SvIOK(sv)) ? (SvIOK_UV(sv)) ? (NV)(SvUVX(sv)) : (NV)(SvIVX(sv)) : (SvNV(sv))
#else
#  define slu_sv_value(sv) (SvIOK(sv)) ? (NV)(SvIVX(sv)) : (SvNV(sv))
#endif

#ifndef Drand01
#    define Drand01()		((rand() & 0x7FFF) / (double) ((unsigned long)1 << 15))
#endif

#if PERL_VERSION < 5
#  ifndef gv_stashpvn
#    define gv_stashpvn(n,l,c) gv_stashpv(n,c)
#  endif
#  ifndef SvTAINTED

static bool
sv_tainted(SV *sv)
{
    if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv)) {
	MAGIC *mg = mg_find(sv, 't');
	if (mg && ((mg->mg_len & 1) || (mg->mg_len & 2) && mg->mg_obj == sv))
	    return TRUE;
    }
    return FALSE;
}

#    define SvTAINTED_on(sv) sv_magic((sv), Nullsv, 't', Nullch, 0)
#    define SvTAINTED(sv) (SvMAGICAL(sv) && sv_tainted(sv))
#  endif
#  define PL_defgv defgv
#  define PL_op op
#  define PL_curpad curpad
#  define CALLRUNOPS runops
#  define PL_curpm curpm
#  define PL_sv_undef sv_undef
#  define PERL_CONTEXT struct context
#endif
#if (PERL_VERSION < 5) || (PERL_VERSION == 5 && PERL_SUBVERSION <50)
#  ifndef PL_tainting
#    define PL_tainting tainting
#  endif
#  ifndef PL_stack_base
#    define PL_stack_base stack_base
#  endif
#  ifndef PL_stack_sp
#    define PL_stack_sp stack_sp
#  endif
#  ifndef PL_ppaddr
#    define PL_ppaddr ppaddr
#  endif
#endif

#ifndef PTR2UV
#  define PTR2UV(ptr) (UV)(ptr)
#endif

MODULE=List::Util	PACKAGE=List::Util

void
min(...)
PROTOTYPE: @
ALIAS:
    min = 0
    max = 1
CODE:
{
    int index;
    NV retval;
    SV *retsv;
    if(!items) {
	XSRETURN_UNDEF;
    }
    retsv = ST(0);
    retval = slu_sv_value(retsv);
    for(index = 1 ; index < items ; index++) {
	SV *stacksv = ST(index);
	NV val = slu_sv_value(stacksv);
	if(val < retval ? !ix : ix) {
	    retsv = stacksv;
	    retval = val;
	}
    }
    ST(0) = retsv;
    XSRETURN(1);
}



NV
sum(...)
PROTOTYPE: @
CODE:
{
    SV *sv;
    int index;
    if(!items) {
	XSRETURN_UNDEF;
    }
    sv = ST(0);
    RETVAL = slu_sv_value(sv);
    for(index = 1 ; index < items ; index++) {
	sv = ST(index);
	RETVAL += slu_sv_value(sv);
    }
}
OUTPUT:
    RETVAL


void
minstr(...)
PROTOTYPE: @
ALIAS:
    minstr = 2
    maxstr = 0
CODE:
{
    SV *left;
    int index;
    if(!items) {
	XSRETURN_UNDEF;
    }
    /*
      sv_cmp & sv_cmp_locale return 1,0,-1 for gt,eq,lt
      so we set ix to the value we are looking for
      xsubpp does not allow -ve values, so we start with 0,2 and subtract 1
    */
    ix -= 1;
    left = ST(0);
#ifdef OPpLOCALE
    if(MAXARG & OPpLOCALE) {
	for(index = 1 ; index < items ; index++) {
	    SV *right = ST(index);
	    if(sv_cmp_locale(left, right) == ix)
		left = right;
	}
    }
    else {
#endif
	for(index = 1 ; index < items ; index++) {
	    SV *right = ST(index);
	    if(sv_cmp(left, right) == ix)
		left = right;
	}
#ifdef OPpLOCALE
    }
#endif
    ST(0) = left;
    XSRETURN(1);
}



void
reduce(block,...)
    SV * block
PROTOTYPE: &@
CODE:
{
    SV *ret = sv_newmortal();
    int index;
    GV *agv,*bgv,*gv;
    HV *stash;
    CV *cv;
    OP *reducecop;
    PERL_CONTEXT *cx;
    SV** newsp;
    I32 gimme = G_SCALAR;
    U8 hasargs = 0;
    bool oldcatch = CATCH_GET;

    if(items <= 1) {
	XSRETURN_UNDEF;
    }
    agv = gv_fetchpv("a", TRUE, SVt_PV);
    bgv = gv_fetchpv("b", TRUE, SVt_PV);
    SAVESPTR(GvSV(agv));
    SAVESPTR(GvSV(bgv));
    GvSV(agv) = ret;
    cv = sv_2cv(block, &stash, &gv, 0);
    reducecop = CvSTART(cv);
    SAVESPTR(CvROOT(cv)->op_ppaddr);
    CvROOT(cv)->op_ppaddr = PL_ppaddr[OP_NULL];
#ifdef PAD_SET_CUR
    PAD_SET_CUR(CvPADLIST(cv),1);
#else
    SAVESPTR(PL_curpad);
    PL_curpad = AvARRAY((AV*)AvARRAY(CvPADLIST(cv))[1]);
#endif
    SAVETMPS;
    SAVESPTR(PL_op);
    SvSetSV(ret, ST(1));
    CATCH_SET(TRUE);
    PUSHBLOCK(cx, CXt_SUB, SP);
    PUSHSUB(cx);
    for(index = 2 ; index < items ; index++) {
	GvSV(bgv) = ST(index);
	PL_op = reducecop;
	CALLRUNOPS(aTHX);
	SvSetSV(ret, *PL_stack_sp);
    }
    ST(0) = ret;
    POPBLOCK(cx,PL_curpm);
    CATCH_SET(oldcatch);
    XSRETURN(1);
}

void
first(block,...)
    SV * block
PROTOTYPE: &@
CODE:
{
    int index;
    GV *gv;
    HV *stash;
    CV *cv;
    OP *reducecop;
    PERL_CONTEXT *cx;
    SV** newsp;
    I32 gimme = G_SCALAR;
    U8 hasargs = 0;
    bool oldcatch = CATCH_GET;

    if(items <= 1) {
	XSRETURN_UNDEF;
    }
    SAVESPTR(GvSV(PL_defgv));
    cv = sv_2cv(block, &stash, &gv, 0);
    reducecop = CvSTART(cv);
    SAVESPTR(CvROOT(cv)->op_ppaddr);
    CvROOT(cv)->op_ppaddr = PL_ppaddr[OP_NULL];
#ifdef PAD_SET_CUR
    PAD_SET_CUR(CvPADLIST(cv),1);
#else
    SAVESPTR(PL_curpad);
    PL_curpad = AvARRAY((AV*)AvARRAY(CvPADLIST(cv))[1]);
#endif
    SAVETMPS;
    SAVESPTR(PL_op);
    CATCH_SET(TRUE);
    PUSHBLOCK(cx, CXt_SUB, SP);
    PUSHSUB(cx);
    if (!CvDEPTH(cv))
        (void)SvREFCNT_inc(cv);

    for(index = 1 ; index < items ; index++) {
	GvSV(PL_defgv) = ST(index);
	PL_op = reducecop;
	CALLRUNOPS(aTHX);
	if (SvTRUE(*PL_stack_sp)) {
	  ST(0) = ST(index);
	  POPBLOCK(cx,PL_curpm);
	  CATCH_SET(oldcatch);
	  XSRETURN(1);
	}
    }
    POPBLOCK(cx,PL_curpm);
    CATCH_SET(oldcatch);
    XSRETURN_UNDEF;
}

void
shuffle(...)
PROTOTYPE: @
CODE:
{
    int index;
    struct op dmy_op;
    struct op *old_op = PL_op;

    /* We call pp_rand here so that Drand01 get initialized if rand()
       or srand() has not already been called
    */
    memzero((char*)(&dmy_op), sizeof(struct op));
    /* we let pp_rand() borrow the TARG allocated for this XS sub */
    dmy_op.op_targ = PL_op->op_targ;
    PL_op = &dmy_op;
    (void)*(PL_ppaddr[OP_RAND])(aTHX);
    PL_op = old_op;
    for (index = items ; index > 1 ; ) {
	int swap = (int)(Drand01() * (double)(index--));
	SV *tmp = ST(swap);
	ST(swap) = ST(index);
	ST(index) = tmp;
    }
    XSRETURN(items);
}


MODULE=List::Util	PACKAGE=Scalar::Util

void
dualvar(num,str)
    SV *	num
    SV *	str
PROTOTYPE: $$
CODE:
{
    STRLEN len;
    char *ptr = SvPV(str,len);
    ST(0) = sv_newmortal();
    (void)SvUPGRADE(ST(0),SVt_PVNV);
    sv_setpvn(ST(0),ptr,len);
    if(SvNOK(num) || SvPOK(num) || SvMAGICAL(num)) {
	SvNVX(ST(0)) = SvNV(num);
	SvNOK_on(ST(0));
    }
#ifdef SVf_IVisUV
    else if (SvUOK(num)) {
	SvUVX(ST(0)) = SvUV(num);
	SvIOK_on(ST(0));
	SvIsUV_on(ST(0));
    }
#endif
    else {
	SvIVX(ST(0)) = SvIV(num);
	SvIOK_on(ST(0));
    }
    if(PL_tainting && (SvTAINTED(num) || SvTAINTED(str)))
	SvTAINTED_on(ST(0));
    XSRETURN(1);
}

char *
blessed(sv)
    SV * sv
PROTOTYPE: $
CODE:
{
    if (SvMAGICAL(sv))
	mg_get(sv);
    if(!sv_isobject(sv)) {
	XSRETURN_UNDEF;
    }
    RETVAL = sv_reftype(SvRV(sv),TRUE);
}
OUTPUT:
    RETVAL

char *
reftype(sv)
    SV * sv
PROTOTYPE: $
CODE:
{
    if (SvMAGICAL(sv))
	mg_get(sv);
    if(!SvROK(sv)) {
	XSRETURN_UNDEF;
    }
    RETVAL = sv_reftype(SvRV(sv),FALSE);
}
OUTPUT:
    RETVAL

UV
refaddr(sv)
    SV * sv
PROTOTYPE: $
CODE:
{
    if (SvMAGICAL(sv))
	mg_get(sv);
    if(!SvROK(sv)) {
	XSRETURN_UNDEF;
    }
    RETVAL = PTR2UV(SvRV(sv));
}
OUTPUT:
    RETVAL

void
weaken(sv)
	SV *sv
PROTOTYPE: $
CODE:
#ifdef SvWEAKREF
	sv_rvweaken(sv);
#else
	croak("weak references are not implemented in this release of perl");
#endif

void
isweak(sv)
	SV *sv
PROTOTYPE: $
CODE:
#ifdef SvWEAKREF
	ST(0) = boolSV(SvROK(sv) && SvWEAKREF(sv));
	XSRETURN(1);
#else
	croak("weak references are not implemented in this release of perl");
#endif

int
readonly(sv)
	SV *sv
PROTOTYPE: $
CODE:
  RETVAL = SvREADONLY(sv);
OUTPUT:
  RETVAL

int
tainted(sv)
	SV *sv
PROTOTYPE: $
CODE:
  RETVAL = SvTAINTED(sv);
OUTPUT:
  RETVAL

void
isvstring(sv)
       SV *sv
PROTOTYPE: $
CODE:
#ifdef SvVOK
  ST(0) = boolSV(SvVOK(sv));
  XSRETURN(1);
#else
	croak("vstrings are not implemented in this release of perl");
#endif

int
looks_like_number(sv)
	SV *sv
PROTOTYPE: $
CODE:
  RETVAL = looks_like_number(sv);
OUTPUT:
  RETVAL

void
set_prototype(subref, proto)
    SV *subref
    SV *proto
PROTOTYPE: &$
CODE:
{
    if (SvROK(subref)) {
	SV *sv = SvRV(subref);
	if (SvTYPE(sv) != SVt_PVCV) {
	    /* not a subroutine reference */
	    croak("set_prototype: not a subroutine reference");
	}
	if (SvPOK(proto)) {
	    /* set the prototype */
	    STRLEN len;
	    char *ptr = SvPV(proto, len);
	    sv_setpvn(sv, ptr, len);
	}
	else {
	    /* delete the prototype */
	    SvPOK_off(sv);
	}
    }
    else {
	croak("set_prototype: not a reference");
    }
    XSRETURN(1);
}

BOOT:
{
#if !defined(SvWEAKREF) || !defined(SvVOK)
    HV *stash = gv_stashpvn("Scalar::Util", 12, TRUE);
    GV *vargv = *(GV**)hv_fetch(stash, "EXPORT_FAIL", 11, TRUE);
    AV *varav;
    if (SvTYPE(vargv) != SVt_PVGV)
	gv_init(vargv, stash, "Scalar::Util", 12, TRUE);
    varav = GvAVn(vargv);
#endif
#ifndef SvWEAKREF
    av_push(varav, newSVpv("weaken",6));
    av_push(varav, newSVpv("isweak",6));
#endif
#ifndef SvVOK
    av_push(varav, newSVpv("isvstring",9));
#endif
}
