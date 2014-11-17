/* Copyright (c) 1997-2000 Graham Barr <gbarr@pobox.com>. All rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the same terms as Perl itself.
 */
#define PERL_NO_GET_CONTEXT /* we want efficiency */
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#define NEED_sv_2pv_flags 1
#include "ppport.h"

#if PERL_BCDVERSION >= 0x5006000
#  include "multicall.h"
#endif

#ifndef CvISXSUB
#  define CvISXSUB(cv) CvXSUB(cv)
#endif

/* Some platforms have strict exports. And before 5.7.3 cxinc (or Perl_cxinc)
   was not exported. Therefore platforms like win32, VMS etc have problems
   so we redefine it here -- GMB
*/
#if PERL_BCDVERSION < 0x5007000
/* Not in 5.6.1. */
#  ifdef cxinc
#    undef cxinc
#  endif
#  define cxinc() my_cxinc(aTHX)
static I32
my_cxinc(pTHX)
{
    cxstack_max = cxstack_max * 3 / 2;
    Renew(cxstack, cxstack_max + 1, struct context); /* fencepost bug in older CXINC macros requires +1 here */
    return cxstack_ix + 1;
}
#endif

#ifndef sv_copypv
#define sv_copypv(a, b) my_sv_copypv(aTHX_ a, b)
static void
my_sv_copypv(pTHX_ SV *const dsv, SV *const ssv)
{
    STRLEN len;
    const char * const s = SvPV_const(ssv,len);
    sv_setpvn(dsv,s,len);
    if(SvUTF8(ssv))
        SvUTF8_on(dsv);
    else
        SvUTF8_off(dsv);
}
#endif

#ifdef SVf_IVisUV
#  define slu_sv_value(sv) (SvIOK(sv)) ? (SvIOK_UV(sv)) ? (NV)(SvUVX(sv)) : (NV)(SvIVX(sv)) : (SvNV(sv))
#else
#  define slu_sv_value(sv) (SvIOK(sv)) ? (NV)(SvIVX(sv)) : (SvNV(sv))
#endif

#if PERL_VERSION < 13 || (PERL_VERSION == 13 && PERL_SUBVERSION < 9)
#  define PERL_HAS_BAD_MULTICALL_REFCOUNT
#endif

#if PERL_VERSION < 14
#  define croak_no_modify() croak("%s", PL_no_modify)
#endif

MODULE=List::Util       PACKAGE=List::Util

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
    int magic;

    if(!items)
        XSRETURN_UNDEF;

    retsv = ST(0);
    magic = SvAMAGIC(retsv);
    if(!magic)
      retval = slu_sv_value(retsv);

    for(index = 1 ; index < items ; index++) {
        SV *stacksv = ST(index);
        SV *tmpsv;
        if((magic || SvAMAGIC(stacksv)) && (tmpsv = amagic_call(retsv, stacksv, gt_amg, 0))) {
             if(SvTRUE(tmpsv) ? !ix : ix) {
                  retsv = stacksv;
                  magic = SvAMAGIC(retsv);
                  if(!magic) {
                      retval = slu_sv_value(retsv);
                  }
             }
        }
        else {
            NV val = slu_sv_value(stacksv);
            if(magic) {
                retval = slu_sv_value(retsv);
                magic = 0;
            }
            if(val < retval ? !ix : ix) {
                retsv = stacksv;
                retval = val;
            }
        }
    }
    ST(0) = retsv;
    XSRETURN(1);
}


void
sum(...)
PROTOTYPE: @
ALIAS:
    sum     = 0
    sum0    = 1
    product = 2
CODE:
{
    dXSTARG;
    SV *sv;
    SV *retsv = NULL;
    int index;
    NV retval = 0;
    int magic;
    int is_product = (ix == 2);

    if(!items)
        switch(ix) {
            case 0: XSRETURN_UNDEF;
            case 1: ST(0) = newSViv(0); XSRETURN(1);
            case 2: ST(0) = newSViv(1); XSRETURN(1);
        }

    sv    = ST(0);
    magic = SvAMAGIC(sv);
    if(magic) {
        retsv = TARG;
        sv_setsv(retsv, sv);
    }
    else {
        retval = slu_sv_value(sv);
    }

    for(index = 1 ; index < items ; index++) {
        sv = ST(index);
        if(!magic && SvAMAGIC(sv)){
            magic = TRUE;
            if(!retsv)
                retsv = TARG;
            sv_setnv(retsv,retval);
        }
        if(magic) {
            SV *const tmpsv = amagic_call(retsv, sv, 
                is_product ? mult_amg : add_amg,
                SvAMAGIC(retsv) ? AMGf_assign : 0);
            if(tmpsv) {
                magic = SvAMAGIC(tmpsv);
                if(!magic) {
                    retval = slu_sv_value(tmpsv);
                }
                else {
                    retsv = tmpsv;
                }
            }
            else {
                /* fall back to default */
                magic = FALSE;
                is_product ? (retval = SvNV(retsv) * SvNV(sv))
                           : (retval = SvNV(retsv) + SvNV(sv));
            }
        }
        else {
            is_product ? (retval *= slu_sv_value(sv))
                       : (retval += slu_sv_value(sv));
        }
    }
    if(!magic) {
        if(!retsv)
            retsv = TARG;
        sv_setnv(retsv,retval);
    }

    ST(0) = retsv;
    XSRETURN(1);
}

#define SLU_CMP_LARGER   1
#define SLU_CMP_SMALLER -1

void
minstr(...)
PROTOTYPE: @
ALIAS:
    minstr = SLU_CMP_LARGER
    maxstr = SLU_CMP_SMALLER
CODE:
{
    SV *left;
    int index;

    if(!items)
        XSRETURN_UNDEF;

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
    SV *block
PROTOTYPE: &@
CODE:
{
    SV *ret = sv_newmortal();
    int index;
    GV *agv,*bgv,*gv;
    HV *stash;
    SV **args = &PL_stack_base[ax];
    CV *cv    = sv_2cv(block, &stash, &gv, 0);

    if(cv == Nullcv)
        croak("Not a subroutine reference");

    if(items <= 1)
        XSRETURN_UNDEF;

    agv = gv_fetchpv("a", GV_ADD, SVt_PV);
    bgv = gv_fetchpv("b", GV_ADD, SVt_PV);
    SAVESPTR(GvSV(agv));
    SAVESPTR(GvSV(bgv));
    GvSV(agv) = ret;
    SvSetSV(ret, args[1]);
#ifdef dMULTICALL
    if(!CvISXSUB(cv)) {
        dMULTICALL;
        I32 gimme = G_SCALAR;

        PUSH_MULTICALL(cv);
        for(index = 2 ; index < items ; index++) {
            GvSV(bgv) = args[index];
            MULTICALL;
            SvSetSV(ret, *PL_stack_sp);
        }
#  ifdef PERL_HAS_BAD_MULTICALL_REFCOUNT
        if(CvDEPTH(multicall_cv) > 1)
            SvREFCNT_inc_simple_void_NN(multicall_cv);
#  endif
        POP_MULTICALL;
    }
    else
#endif
    {
        for(index = 2 ; index < items ; index++) {
            dSP;
            GvSV(bgv) = args[index];

            PUSHMARK(SP);
            call_sv((SV*)cv, G_SCALAR);

            SvSetSV(ret, *PL_stack_sp);
        }
    }

    ST(0) = ret;
    XSRETURN(1);
}

void
first(block,...)
    SV *block
PROTOTYPE: &@
CODE:
{
    int index;
    GV *gv;
    HV *stash;
    SV **args = &PL_stack_base[ax];
    CV *cv    = sv_2cv(block, &stash, &gv, 0);

    if(cv == Nullcv)
        croak("Not a subroutine reference");

    if(items <= 1)
        XSRETURN_UNDEF;

    SAVESPTR(GvSV(PL_defgv));
#ifdef dMULTICALL
    if(!CvISXSUB(cv)) {
        dMULTICALL;
        I32 gimme = G_SCALAR;
        PUSH_MULTICALL(cv);

        for(index = 1 ; index < items ; index++) {
            GvSV(PL_defgv) = args[index];
            MULTICALL;
            if(SvTRUEx(*PL_stack_sp)) {
#  ifdef PERL_HAS_BAD_MULTICALL_REFCOUNT
                if(CvDEPTH(multicall_cv) > 1)
                    SvREFCNT_inc_simple_void_NN(multicall_cv);
#  endif
                POP_MULTICALL;
                ST(0) = ST(index);
                XSRETURN(1);
            }
        }
#  ifdef PERL_HAS_BAD_MULTICALL_REFCOUNT
        if(CvDEPTH(multicall_cv) > 1)
            SvREFCNT_inc_simple_void_NN(multicall_cv);
#  endif
        POP_MULTICALL;
    }
    else
#endif
    {
        for(index = 1 ; index < items ; index++) {
            dSP;
            GvSV(PL_defgv) = args[index];

            PUSHMARK(SP);
            call_sv((SV*)cv, G_SCALAR);
            if(SvTRUEx(*PL_stack_sp)) {
                ST(0) = ST(index);
                XSRETURN(1);
            }
        }
    }
    XSRETURN_UNDEF;
}


void
any(block,...)
    SV *block
ALIAS:
    none   = 0
    all    = 1
    any    = 2
    notall = 3
PROTOTYPE: &@
PPCODE:
{
    int ret_true = !(ix & 2); /* return true at end of loop for none/all; false for any/notall */
    int invert   =  (ix & 1); /* invert block test for all/notall */
    GV *gv;
    HV *stash;
    SV **args = &PL_stack_base[ax];
    CV *cv    = sv_2cv(block, &stash, &gv, 0);

    if(cv == Nullcv)
        croak("Not a subroutine reference");

    SAVESPTR(GvSV(PL_defgv));
#ifdef dMULTICALL
    if(!CvISXSUB(cv)) {
        dMULTICALL;
        I32 gimme = G_SCALAR;
        int index;

        PUSH_MULTICALL(cv);
        for(index = 1; index < items; index++) {
            GvSV(PL_defgv) = args[index];

            MULTICALL;
            if(SvTRUEx(*PL_stack_sp) ^ invert) {
                POP_MULTICALL;
                ST(0) = ret_true ? &PL_sv_no : &PL_sv_yes;
                XSRETURN(1);
            }
        }
        POP_MULTICALL;
    }
    else
#endif
    {
        int index;
        for(index = 1; index < items; index++) {
            dSP;
            GvSV(PL_defgv) = args[index];

            PUSHMARK(SP);
            call_sv((SV*)cv, G_SCALAR);
            if(SvTRUEx(*PL_stack_sp) ^ invert) {
                ST(0) = ret_true ? &PL_sv_no : &PL_sv_yes;
                XSRETURN(1);
            }
        }
    }

    ST(0) = ret_true ? &PL_sv_yes : &PL_sv_no;
    XSRETURN(1);
}

void
pairfirst(block,...)
    SV *block
PROTOTYPE: &@
PPCODE:
{
    GV *agv,*bgv,*gv;
    HV *stash;
    CV *cv    = sv_2cv(block, &stash, &gv, 0);
    I32 ret_gimme = GIMME_V;
    int argi = 1; /* "shift" the block */

    if(!(items % 2) && ckWARN(WARN_MISC))
        warn("Odd number of elements in pairfirst");

    agv = gv_fetchpv("a", GV_ADD, SVt_PV);
    bgv = gv_fetchpv("b", GV_ADD, SVt_PV);
    SAVESPTR(GvSV(agv));
    SAVESPTR(GvSV(bgv));
#ifdef dMULTICALL
    if(!CvISXSUB(cv)) {
        /* Since MULTICALL is about to move it */
        SV **stack = PL_stack_base + ax;

        dMULTICALL;
        I32 gimme = G_SCALAR;

        PUSH_MULTICALL(cv);
        for(; argi < items; argi += 2) {
            SV *a = GvSV(agv) = stack[argi];
            SV *b = GvSV(bgv) = argi < items-1 ? stack[argi+1] : &PL_sv_undef;

            MULTICALL;

            if(!SvTRUEx(*PL_stack_sp))
                continue;

            POP_MULTICALL;
            if(ret_gimme == G_ARRAY) {
                ST(0) = sv_mortalcopy(a);
                ST(1) = sv_mortalcopy(b);
                XSRETURN(2);
            }
            else
                XSRETURN_YES;
        }
        POP_MULTICALL;
        XSRETURN(0);
    }
    else
#endif
    {
        for(; argi < items; argi += 2) {
            dSP;
            SV *a = GvSV(agv) = ST(argi);
            SV *b = GvSV(bgv) = argi < items-1 ? ST(argi+1) : &PL_sv_undef;

            PUSHMARK(SP);
            call_sv((SV*)cv, G_SCALAR);

            SPAGAIN;

            if(!SvTRUEx(*PL_stack_sp))
                continue;

            if(ret_gimme == G_ARRAY) {
                ST(0) = sv_mortalcopy(a);
                ST(1) = sv_mortalcopy(b);
                XSRETURN(2);
            }
            else
                XSRETURN_YES;
        }
    }

    XSRETURN(0);
}

void
pairgrep(block,...)
    SV *block
PROTOTYPE: &@
PPCODE:
{
    GV *agv,*bgv,*gv;
    HV *stash;
    CV *cv    = sv_2cv(block, &stash, &gv, 0);
    I32 ret_gimme = GIMME_V;

    /* This function never returns more than it consumed in arguments. So we
     * can build the results "live", behind the arguments
     */
    int argi = 1; /* "shift" the block */
    int reti = 0;

    if(!(items % 2) && ckWARN(WARN_MISC))
        warn("Odd number of elements in pairgrep");

    agv = gv_fetchpv("a", GV_ADD, SVt_PV);
    bgv = gv_fetchpv("b", GV_ADD, SVt_PV);
    SAVESPTR(GvSV(agv));
    SAVESPTR(GvSV(bgv));
#ifdef dMULTICALL
    if(!CvISXSUB(cv)) {
        /* Since MULTICALL is about to move it */
        SV **stack = PL_stack_base + ax;
        int i;

        dMULTICALL;
        I32 gimme = G_SCALAR;

        PUSH_MULTICALL(cv);
        for(; argi < items; argi += 2) {
            SV *a = GvSV(agv) = stack[argi];
            SV *b = GvSV(bgv) = argi < items-1 ? stack[argi+1] : &PL_sv_undef;

            MULTICALL;

            if(SvTRUEx(*PL_stack_sp)) {
                if(ret_gimme == G_ARRAY) {
                    /* We can't mortalise yet or they'd be mortal too early */
                    stack[reti++] = newSVsv(a);
                    stack[reti++] = newSVsv(b);
                }
                else if(ret_gimme == G_SCALAR)
                    reti++;
            }
        }
        POP_MULTICALL;

        if(ret_gimme == G_ARRAY)
            for(i = 0; i < reti; i++)
                sv_2mortal(stack[i]);
    }
    else
#endif
    {
        for(; argi < items; argi += 2) {
            dSP;
            SV *a = GvSV(agv) = ST(argi);
            SV *b = GvSV(bgv) = argi < items-1 ? ST(argi+1) : &PL_sv_undef;

            PUSHMARK(SP);
            call_sv((SV*)cv, G_SCALAR);

            SPAGAIN;

            if(SvTRUEx(*PL_stack_sp)) {
                if(ret_gimme == G_ARRAY) {
                    ST(reti++) = sv_mortalcopy(a);
                    ST(reti++) = sv_mortalcopy(b);
                }
                else if(ret_gimme == G_SCALAR)
                    reti++;
            }
        }
    }

    if(ret_gimme == G_ARRAY)
        XSRETURN(reti);
    else if(ret_gimme == G_SCALAR) {
        ST(0) = newSViv(reti);
        XSRETURN(1);
    }
}

void
pairmap(block,...)
    SV *block
PROTOTYPE: &@
PPCODE:
{
    GV *agv,*bgv,*gv;
    HV *stash;
    CV *cv    = sv_2cv(block, &stash, &gv, 0);
    SV **args_copy = NULL;
    I32 ret_gimme = GIMME_V;

    int argi = 1; /* "shift" the block */
    int reti = 0;

    if(!(items % 2) && ckWARN(WARN_MISC))
        warn("Odd number of elements in pairmap");

    agv = gv_fetchpv("a", GV_ADD, SVt_PV);
    bgv = gv_fetchpv("b", GV_ADD, SVt_PV);
    SAVESPTR(GvSV(agv));
    SAVESPTR(GvSV(bgv));
/* This MULTICALL-based code appears to fail on perl 5.10.0 and 5.8.9
 * Skip it on those versions (RT#87857)
 */
#if defined(dMULTICALL) && (PERL_BCDVERSION > 0x5010000 || PERL_BCDVERSION < 0x5008009)
    if(!CvISXSUB(cv)) {
        /* Since MULTICALL is about to move it */
        SV **stack = PL_stack_base + ax;
        I32 ret_gimme = GIMME_V;
        int i;

        dMULTICALL;
        I32 gimme = G_ARRAY;

        PUSH_MULTICALL(cv);
        for(; argi < items; argi += 2) {
            SV *a = GvSV(agv) = args_copy ? args_copy[argi] : stack[argi];
            SV *b = GvSV(bgv) = argi < items-1 ? 
                (args_copy ? args_copy[argi+1] : stack[argi+1]) :
                &PL_sv_undef;
            int count;

            MULTICALL;
            count = PL_stack_sp - PL_stack_base;

            if(count > 2 && !args_copy) {
                /* We can't return more than 2 results for a given input pair
                 * without trashing the remaining argmuents on the stack still
                 * to be processed. So, we'll copy them out to a temporary
                 * buffer and work from there instead.
                 * We didn't do this initially because in the common case, most
                 * code blocks will return only 1 or 2 items so it won't be
                 * necessary
                 */
                int n_args = items - argi;
                Newx(args_copy, n_args, SV *);
                SAVEFREEPV(args_copy);

                Copy(stack + argi, args_copy, n_args, SV *);

                argi = 0;
                items = n_args;
            }

            for(i = 0; i < count; i++)
                stack[reti++] = newSVsv(PL_stack_sp[i - count + 1]);
        }
        POP_MULTICALL;

        if(ret_gimme == G_ARRAY)
            for(i = 0; i < reti; i++)
                sv_2mortal(stack[i]);
    }
    else
#endif
    {
        for(; argi < items; argi += 2) {
            dSP;
            SV *a = GvSV(agv) = args_copy ? args_copy[argi] : ST(argi);
            SV *b = GvSV(bgv) = argi < items-1 ? 
                (args_copy ? args_copy[argi+1] : ST(argi+1)) :
                &PL_sv_undef;
            int count;
            int i;

            PUSHMARK(SP);
            count = call_sv((SV*)cv, G_ARRAY);

            SPAGAIN;

            if(count > 2 && !args_copy && ret_gimme == G_ARRAY) {
                int n_args = items - argi;
                Newx(args_copy, n_args, SV *);
                SAVEFREEPV(args_copy);

                Copy(&ST(argi), args_copy, n_args, SV *);

                argi = 0;
                items = n_args;
            }

            if(ret_gimme == G_ARRAY)
                for(i = 0; i < count; i++)
                    ST(reti++) = sv_mortalcopy(SP[i - count + 1]);
            else
                reti += count;

            PUTBACK;
        }
    }

    if(ret_gimme == G_ARRAY)
        XSRETURN(reti);

    ST(0) = sv_2mortal(newSViv(reti));
    XSRETURN(1);
}

void
pairs(...)
PROTOTYPE: @
PPCODE:
{
    int argi = 0;
    int reti = 0;

    if(items % 2 && ckWARN(WARN_MISC))
        warn("Odd number of elements in pairs");

    {
        for(; argi < items; argi += 2) {
            SV *a = ST(argi);
            SV *b = argi < items-1 ? ST(argi+1) : &PL_sv_undef;

            AV *av = newAV();
            av_push(av, newSVsv(a));
            av_push(av, newSVsv(b));

            ST(reti++) = sv_2mortal(newRV_noinc((SV *)av));
        }
    }

    XSRETURN(reti);
}

void
pairkeys(...)
PROTOTYPE: @
PPCODE:
{
    int argi = 0;
    int reti = 0;

    if(items % 2 && ckWARN(WARN_MISC))
        warn("Odd number of elements in pairkeys");

    {
        for(; argi < items; argi += 2) {
            SV *a = ST(argi);

            ST(reti++) = sv_2mortal(newSVsv(a));
        }
    }

    XSRETURN(reti);
}

void
pairvalues(...)
PROTOTYPE: @
PPCODE:
{
    int argi = 0;
    int reti = 0;

    if(items % 2 && ckWARN(WARN_MISC))
        warn("Odd number of elements in pairvalues");

    {
        for(; argi < items; argi += 2) {
            SV *b = argi < items-1 ? ST(argi+1) : &PL_sv_undef;

            ST(reti++) = sv_2mortal(newSVsv(b));
        }
    }

    XSRETURN(reti);
}

void
shuffle(...)
PROTOTYPE: @
CODE:
{
    int index;
#if (PERL_VERSION < 9)
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
#else
    /* Initialize Drand01 if rand() or srand() has
       not already been called
    */
    if(!PL_srand_called) {
        (void)seedDrand01((Rand_seed_t)Perl_seed(aTHX));
        PL_srand_called = TRUE;
    }
#endif

    for (index = items ; index > 1 ; ) {
        int swap = (int)(Drand01() * (double)(index--));
        SV *tmp = ST(swap);
        ST(swap) = ST(index);
        ST(index) = tmp;
    }

    XSRETURN(items);
}


MODULE=List::Util       PACKAGE=Scalar::Util

void
dualvar(num,str)
    SV *num
    SV *str
PROTOTYPE: $$
CODE:
{
    dXSTARG;

    (void)SvUPGRADE(TARG, SVt_PVNV);

    sv_copypv(TARG,str);

    if(SvNOK(num) || SvPOK(num) || SvMAGICAL(num)) {
        SvNV_set(TARG, SvNV(num));
        SvNOK_on(TARG);
    }
#ifdef SVf_IVisUV
    else if(SvUOK(num)) {
        SvUV_set(TARG, SvUV(num));
        SvIOK_on(TARG);
        SvIsUV_on(TARG);
    }
#endif
    else {
        SvIV_set(TARG, SvIV(num));
        SvIOK_on(TARG);
    }

    if(PL_tainting && (SvTAINTED(num) || SvTAINTED(str)))
        SvTAINTED_on(TARG);

    ST(0) = TARG;
    XSRETURN(1);
}

void
isdual(sv)
    SV *sv
PROTOTYPE: $
CODE:
    if(SvMAGICAL(sv))
        mg_get(sv);

    ST(0) = boolSV((SvPOK(sv) || SvPOKp(sv)) && (SvNIOK(sv) || SvNIOKp(sv)));
    XSRETURN(1);

char *
blessed(sv)
    SV *sv
PROTOTYPE: $
CODE:
{
    SvGETMAGIC(sv);

    if(!(SvROK(sv) && SvOBJECT(SvRV(sv))))
        XSRETURN_UNDEF;

    RETVAL = (char*)sv_reftype(SvRV(sv),TRUE);
}
OUTPUT:
    RETVAL

char *
reftype(sv)
    SV *sv
PROTOTYPE: $
CODE:
{
    SvGETMAGIC(sv);
    if(!SvROK(sv))
        XSRETURN_UNDEF;

    RETVAL = (char*)sv_reftype(SvRV(sv),FALSE);
}
OUTPUT:
    RETVAL

UV
refaddr(sv)
    SV *sv
PROTOTYPE: $
CODE:
{
    SvGETMAGIC(sv);
    if(!SvROK(sv))
        XSRETURN_UNDEF;

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
unweaken(sv)
    SV *sv
PROTOTYPE: $
INIT:
    SV *tsv;
CODE:
#ifdef SvWEAKREF
    /* This code stolen from core's sv_rvweaken() and modified */
    if (!SvOK(sv))
        return;
    if (!SvROK(sv))
        croak("Can't unweaken a nonreference");
    else if (!SvWEAKREF(sv)) {
        if(ckWARN(WARN_MISC))
            warn("Reference is not weak");
        return;
    }
    else if (SvREADONLY(sv)) croak_no_modify();

    tsv = SvRV(sv);
#if PERL_VERSION >= 14
    SvWEAKREF_off(sv); SvROK_on(sv);
    SvREFCNT_inc_NN(tsv);
    Perl_sv_del_backref(aTHX_ tsv, sv);
#else
    /* Lacking sv_del_backref() the best we can do is clear the old (weak) ref
     * then set a new strong one
     */
    sv_setsv(sv, &PL_sv_undef);
    SvRV_set(sv, SvREFCNT_inc_NN(tsv));
    SvROK_on(sv);
#endif
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
    SvGETMAGIC(sv);
    RETVAL = SvREADONLY(sv);
OUTPUT:
    RETVAL

int
tainted(sv)
    SV *sv
PROTOTYPE: $
CODE:
    SvGETMAGIC(sv);
    RETVAL = SvTAINTED(sv);
OUTPUT:
    RETVAL

void
isvstring(sv)
    SV *sv
PROTOTYPE: $
CODE:
#ifdef SvVOK
    SvGETMAGIC(sv);
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
    SV *tempsv;
    SvGETMAGIC(sv);
    if(SvAMAGIC(sv) && (tempsv = AMG_CALLun(sv, numer))) {
        sv = tempsv;
    }
#if PERL_BCDVERSION < 0x5008005
    if(SvPOK(sv) || SvPOKp(sv)) {
        RETVAL = looks_like_number(sv);
    }
    else {
        RETVAL = SvFLAGS(sv) & (SVf_NOK|SVp_NOK|SVf_IOK|SVp_IOK);
    }
#else
    RETVAL = looks_like_number(sv);
#endif
OUTPUT:
    RETVAL

void
set_prototype(subref, proto)
    SV *subref
    SV *proto
PROTOTYPE: &$
CODE:
{
    if(SvROK(subref)) {
        SV *sv = SvRV(subref);
        if(SvTYPE(sv) != SVt_PVCV) {
            /* not a subroutine reference */
            croak("set_prototype: not a subroutine reference");
        }
        if(SvPOK(proto)) {
            /* set the prototype */
            sv_copypv(sv, proto);
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

void
openhandle(SV *sv)
PROTOTYPE: $
CODE:
{
    IO *io = NULL;
    SvGETMAGIC(sv);
    if(SvROK(sv)){
        /* deref first */
        sv = SvRV(sv);
    }

    /* must be GLOB or IO */
    if(isGV(sv)){
        io = GvIO((GV*)sv);
    }
    else if(SvTYPE(sv) == SVt_PVIO){
        io = (IO*)sv;
    }

    if(io){
        /* real or tied filehandle? */
        if(IoIFP(io) || SvTIED_mg((SV*)io, PERL_MAGIC_tiedscalar)){
            XSRETURN(1);
        }
    }
    XSRETURN_UNDEF;
}

BOOT:
{
    HV *lu_stash = gv_stashpvn("List::Util", 10, TRUE);
    GV *rmcgv = *(GV**)hv_fetch(lu_stash, "REAL_MULTICALL", 14, TRUE);
    SV *rmcsv;
#if !defined(SvWEAKREF) || !defined(SvVOK)
    HV *su_stash = gv_stashpvn("Scalar::Util", 12, TRUE);
    GV *vargv = *(GV**)hv_fetch(su_stash, "EXPORT_FAIL", 11, TRUE);
    AV *varav;
    if(SvTYPE(vargv) != SVt_PVGV)
        gv_init(vargv, su_stash, "Scalar::Util", 12, TRUE);
    varav = GvAVn(vargv);
#endif
    if(SvTYPE(rmcgv) != SVt_PVGV)
        gv_init(rmcgv, lu_stash, "List::Util", 10, TRUE);
    rmcsv = GvSVn(rmcgv);
#ifndef SvWEAKREF
    av_push(varav, newSVpv("weaken",6));
    av_push(varav, newSVpv("isweak",6));
#endif
#ifndef SvVOK
    av_push(varav, newSVpv("isvstring",9));
#endif
#ifdef REAL_MULTICALL
    sv_setsv(rmcsv, &PL_sv_yes);
#else
    sv_setsv(rmcsv, &PL_sv_no);
#endif
}
