/*    scope.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * For the fashion of Minas Tirith was such that it was built on seven
 * levels...
 *
 *     [p.751 of _The Lord of the Rings_, V/i: "Minas Tirith"]
 */

/* This file contains functions to manipulate several of Perl's stacks;
 * in particular it contains code to push various types of things onto
 * the savestack, then to pop them off and perform the correct restorative
 * action for each one. This corresponds to the cleanup Perl does at
 * each scope exit.
 */

#include "EXTERN.h"
#define PERL_IN_SCOPE_C
#include "perl.h"

SV**
Perl_stack_grow(pTHX_ SV **sp, SV **p, int n)
{
    dVAR;

    PERL_ARGS_ASSERT_STACK_GROW;

    PL_stack_sp = sp;
#ifndef STRESS_REALLOC
    av_extend(PL_curstack, (p - PL_stack_base) + (n) + 128);
#else
    av_extend(PL_curstack, (p - PL_stack_base) + (n) + 1);
#endif
    return PL_stack_sp;
}

#ifndef STRESS_REALLOC
#define GROW(old) ((old) * 3 / 2)
#else
#define GROW(old) ((old) + 1)
#endif

PERL_SI *
Perl_new_stackinfo(pTHX_ I32 stitems, I32 cxitems)
{
    dVAR;
    PERL_SI *si;
    Newx(si, 1, PERL_SI);
    si->si_stack = newAV();
    AvREAL_off(si->si_stack);
    av_extend(si->si_stack, stitems > 0 ? stitems-1 : 0);
    AvALLOC(si->si_stack)[0] = &PL_sv_undef;
    AvFILLp(si->si_stack) = 0;
    si->si_prev = 0;
    si->si_next = 0;
    si->si_cxmax = cxitems - 1;
    si->si_cxix = -1;
    si->si_type = PERLSI_UNDEF;
    Newx(si->si_cxstack, cxitems, PERL_CONTEXT);
    /* Without any kind of initialising PUSHSUBST()
     * in pp_subst() will read uninitialised heap. */
    PoisonNew(si->si_cxstack, cxitems, PERL_CONTEXT);
    return si;
}

I32
Perl_cxinc(pTHX)
{
    dVAR;
    const IV old_max = cxstack_max;
    cxstack_max = GROW(cxstack_max);
    Renew(cxstack, cxstack_max + 1, PERL_CONTEXT);	/* XXX should fix CXINC macro */
    /* Without any kind of initialising deep enough recursion
     * will end up reading uninitialised PERL_CONTEXTs. */
    PoisonNew(cxstack + old_max + 1, cxstack_max - old_max, PERL_CONTEXT);
    return cxstack_ix + 1;
}

void
Perl_push_scope(pTHX)
{
    dVAR;
    if (PL_scopestack_ix == PL_scopestack_max) {
	PL_scopestack_max = GROW(PL_scopestack_max);
	Renew(PL_scopestack, PL_scopestack_max, I32);
    }
    PL_scopestack[PL_scopestack_ix++] = PL_savestack_ix;

}

void
Perl_pop_scope(pTHX)
{
    dVAR;
    const I32 oldsave = PL_scopestack[--PL_scopestack_ix];
    LEAVE_SCOPE(oldsave);
}

void
Perl_markstack_grow(pTHX)
{
    dVAR;
    const I32 oldmax = PL_markstack_max - PL_markstack;
    const I32 newmax = GROW(oldmax);

    Renew(PL_markstack, newmax, I32);
    PL_markstack_ptr = PL_markstack + oldmax;
    PL_markstack_max = PL_markstack + newmax;
}

void
Perl_savestack_grow(pTHX)
{
    dVAR;
    PL_savestack_max = GROW(PL_savestack_max) + 4;
    Renew(PL_savestack, PL_savestack_max, ANY);
}

void
Perl_savestack_grow_cnt(pTHX_ I32 need)
{
    dVAR;
    PL_savestack_max = PL_savestack_ix + need;
    Renew(PL_savestack, PL_savestack_max, ANY);
}

#undef GROW

void
Perl_tmps_grow(pTHX_ I32 n)
{
    dVAR;
#ifndef STRESS_REALLOC
    if (n < 128)
	n = (PL_tmps_max < 512) ? 128 : 512;
#endif
    PL_tmps_max = PL_tmps_ix + n + 1;
    Renew(PL_tmps_stack, PL_tmps_max, SV*);
}


void
Perl_free_tmps(pTHX)
{
    dVAR;
    /* XXX should tmps_floor live in cxstack? */
    const I32 myfloor = PL_tmps_floor;
    while (PL_tmps_ix > myfloor) {      /* clean up after last statement */
	SV* const sv = PL_tmps_stack[PL_tmps_ix];
	PL_tmps_stack[PL_tmps_ix--] = NULL;
	if (sv && sv != &PL_sv_undef) {
	    SvTEMP_off(sv);
	    SvREFCNT_dec(sv);		/* note, can modify tmps_ix!!! */
	}
    }
}

STATIC SV *
S_save_scalar_at(pTHX_ SV **sptr)
{
    dVAR;
    SV * const osv = *sptr;
    register SV * const sv = *sptr = newSV(0);

    PERL_ARGS_ASSERT_SAVE_SCALAR_AT;

    if (SvTYPE(osv) >= SVt_PVMG && SvMAGIC(osv) && SvTYPE(osv) != SVt_PVGV) {
	if (SvGMAGICAL(osv)) {
	    const bool oldtainted = PL_tainted;
	    SvFLAGS(osv) |= (SvFLAGS(osv) &
	       (SVp_IOK|SVp_NOK|SVp_POK)) >> PRIVSHIFT;
	    PL_tainted = oldtainted;
	}
	mg_localize(osv, sv);
    }
    return sv;
}

void
Perl_save_pushptrptr(pTHX_ void *const ptr1, void *const ptr2, const int type)
{
    dVAR;
    SSCHECK(3);
    SSPUSHPTR(ptr1);
    SSPUSHPTR(ptr2);
    SSPUSHINT(type);
}

SV *
Perl_save_scalar(pTHX_ GV *gv)
{
    dVAR;
    SV ** const sptr = &GvSVn(gv);

    PERL_ARGS_ASSERT_SAVE_SCALAR;

    PL_localizing = 1;
    SvGETMAGIC(*sptr);
    PL_localizing = 0;
    save_pushptrptr(SvREFCNT_inc_simple(gv), SvREFCNT_inc(*sptr), SAVEt_SV);
    return save_scalar_at(sptr);
}

/* Like save_sptr(), but also SvREFCNT_dec()s the new value.  Can be used to
 * restore a global SV to its prior contents, freeing new value. */
void
Perl_save_generic_svref(pTHX_ SV **sptr)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_GENERIC_SVREF;

    save_pushptrptr(sptr, SvREFCNT_inc(*sptr), SAVEt_GENERIC_SVREF);
}

/* Like save_pptr(), but also Safefree()s the new value if it is different
 * from the old one.  Can be used to restore a global char* to its prior
 * contents, freeing new value. */
void
Perl_save_generic_pvref(pTHX_ char **str)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_GENERIC_PVREF;

    save_pushptrptr(*str, str, SAVEt_GENERIC_PVREF);
}

/* Like save_generic_pvref(), but uses PerlMemShared_free() rather than Safefree().
 * Can be used to restore a shared global char* to its prior
 * contents, freeing new value. */
void
Perl_save_shared_pvref(pTHX_ char **str)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_SHARED_PVREF;

    save_pushptrptr(str, *str, SAVEt_SHARED_PVREF);
}

/* set the SvFLAGS specified by mask to the values in val */

void
Perl_save_set_svflags(pTHX_ SV* sv, U32 mask, U32 val)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_SET_SVFLAGS;

    SSCHECK(4);
    SSPUSHPTR(sv);
    SSPUSHINT(mask);
    SSPUSHINT(val);
    SSPUSHINT(SAVEt_SET_SVFLAGS);
}

void
Perl_save_gp(pTHX_ GV *gv, I32 empty)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_GP;

    save_pushptrptr(SvREFCNT_inc(gv), GvGP(gv), SAVEt_GP);

    if (empty) {
	GP *gp = Perl_newGP(aTHX_ gv);

	if (GvCVu(gv))
            mro_method_changed_in(GvSTASH(gv)); /* taking a method out of circulation ("local")*/
	if (GvIOp(gv) && (IoFLAGS(GvIOp(gv)) & IOf_ARGV)) {
	    gp->gp_io = newIO();
	    IoFLAGS(gp->gp_io) |= IOf_ARGV|IOf_START;
	}
#ifdef PERL_DONT_CREATE_GVSV
	if (gv == PL_errgv) {
	    /* We could scatter this logic everywhere by changing the
	       definition of ERRSV from GvSV() to GvSVn(), but it seems more
	       efficient to do this check once here.  */
	    gp->gp_sv = newSV(0);
	}
#endif
	GvGP(gv) = gp;
    }
    else {
	gp_ref(GvGP(gv));
	GvINTRO_on(gv);
    }
}

AV *
Perl_save_ary(pTHX_ GV *gv)
{
    dVAR;
    AV * const oav = GvAVn(gv);
    AV *av;

    PERL_ARGS_ASSERT_SAVE_ARY;

    if (!AvREAL(oav) && AvREIFY(oav))
	av_reify(oav);
    save_pushptrptr(gv, oav, SAVEt_AV);

    GvAV(gv) = NULL;
    av = GvAVn(gv);
    if (SvMAGIC(oav))
	mg_localize(MUTABLE_SV(oav), MUTABLE_SV(av));
    return av;
}

HV *
Perl_save_hash(pTHX_ GV *gv)
{
    dVAR;
    HV *ohv, *hv;

    PERL_ARGS_ASSERT_SAVE_HASH;

    save_pushptrptr(gv, (ohv = GvHVn(gv)), SAVEt_HV);

    GvHV(gv) = NULL;
    hv = GvHVn(gv);
    if (SvMAGIC(ohv))
	mg_localize(MUTABLE_SV(ohv), MUTABLE_SV(hv));
    return hv;
}

void
Perl_save_item(pTHX_ register SV *item)
{
    dVAR;
    register SV * const sv = newSVsv(item);

    PERL_ARGS_ASSERT_SAVE_ITEM;

    save_pushptrptr(item, /* remember the pointer */
		    sv,   /* remember the value */
		    SAVEt_ITEM);
}

void
Perl_save_bool(pTHX_ bool *boolp)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_BOOL;

    SSCHECK(3);
    SSPUSHBOOL(*boolp);
    SSPUSHPTR(boolp);
    SSPUSHINT(SAVEt_BOOL);
}

void
Perl_save_pushi32ptr(pTHX_ const I32 i, void *const ptr, const int type)
{
    dVAR;
    SSCHECK(3);
    SSPUSHINT(i);
    SSPUSHPTR(ptr);
    SSPUSHINT(type);
}

void
Perl_save_int(pTHX_ int *intp)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_INT;

    save_pushi32ptr(*intp, intp, SAVEt_INT);
}

void
Perl_save_I8(pTHX_ I8 *bytep)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_I8;

    save_pushi32ptr(*bytep, bytep, SAVEt_I8);
}

void
Perl_save_I16(pTHX_ I16 *intp)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_I16;

    save_pushi32ptr(*intp, intp, SAVEt_I16);
}

void
Perl_save_I32(pTHX_ I32 *intp)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_I32;

    save_pushi32ptr(*intp, intp, SAVEt_I32);
}

/* Cannot use save_sptr() to store a char* since the SV** cast will
 * force word-alignment and we'll miss the pointer.
 */
void
Perl_save_pptr(pTHX_ char **pptr)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_PPTR;

    save_pushptrptr(*pptr, pptr, SAVEt_PPTR);
}

void
Perl_save_vptr(pTHX_ void *ptr)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_VPTR;

    save_pushptrptr(*(char**)ptr, ptr, SAVEt_VPTR);
}

void
Perl_save_sptr(pTHX_ SV **sptr)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_SPTR;

    save_pushptrptr(*sptr, sptr, SAVEt_SPTR);
}

void
Perl_save_padsv_and_mortalize(pTHX_ PADOFFSET off)
{
    dVAR;
    SSCHECK(4);
    ASSERT_CURPAD_ACTIVE("save_padsv");
    SSPUSHPTR(SvREFCNT_inc_simple_NN(PL_curpad[off]));
    SSPUSHPTR(PL_comppad);
    SSPUSHLONG((long)off);
    SSPUSHINT(SAVEt_PADSV_AND_MORTALIZE);
}

void
Perl_save_hptr(pTHX_ HV **hptr)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_HPTR;

    save_pushptrptr(*hptr, hptr, SAVEt_HPTR);
}

void
Perl_save_aptr(pTHX_ AV **aptr)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_APTR;

    save_pushptrptr(*aptr, aptr, SAVEt_APTR);
}

void
Perl_save_pushptr(pTHX_ void *const ptr, const int type)
{
    dVAR;
    SSCHECK(2);
    SSPUSHPTR(ptr);
    SSPUSHINT(type);
}

void
Perl_save_clearsv(pTHX_ SV **svp)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_CLEARSV;

    ASSERT_CURPAD_ACTIVE("save_clearsv");
    SSCHECK(2);
    SSPUSHLONG((long)(svp-PL_curpad));
    SSPUSHINT(SAVEt_CLEARSV);
    SvPADSTALE_off(*svp); /* mark lexical as active */
}

void
Perl_save_delete(pTHX_ HV *hv, char *key, I32 klen)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_DELETE;

    save_pushptri32ptr(key, klen, SvREFCNT_inc_simple(hv), SAVEt_DELETE);
}

void
Perl_save_destructor(pTHX_ DESTRUCTORFUNC_NOCONTEXT_t f, void* p)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_DESTRUCTOR;

    SSCHECK(3);
    SSPUSHDPTR(f);
    SSPUSHPTR(p);
    SSPUSHINT(SAVEt_DESTRUCTOR);
}

void
Perl_save_destructor_x(pTHX_ DESTRUCTORFUNC_t f, void* p)
{
    dVAR;
    SSCHECK(3);
    SSPUSHDXPTR(f);
    SSPUSHPTR(p);
    SSPUSHINT(SAVEt_DESTRUCTOR_X);
}

void
Perl_save_hints(pTHX)
{
    dVAR;
    SSCHECK(4);
    if (PL_hints & HINT_LOCALIZE_HH) {
	SSPUSHPTR(GvHV(PL_hintgv));
	GvHV(PL_hintgv) = Perl_hv_copy_hints_hv(aTHX_ GvHV(PL_hintgv));
    }
    if (PL_compiling.cop_hints_hash) {
	HINTS_REFCNT_LOCK;
	PL_compiling.cop_hints_hash->refcounted_he_refcnt++;
	HINTS_REFCNT_UNLOCK;
    }
    SSPUSHPTR(PL_compiling.cop_hints_hash);
    SSPUSHINT(PL_hints);
    SSPUSHINT(SAVEt_HINTS);
}

static void
S_save_pushptri32ptr(pTHX_ void *const ptr1, const I32 i, void *const ptr2,
			const int type)
{
    SSCHECK(4);
    SSPUSHPTR(ptr1);
    SSPUSHINT(i);
    SSPUSHPTR(ptr2);
    SSPUSHINT(type);
}

void
Perl_save_aelem(pTHX_ AV *av, I32 idx, SV **sptr)
{
    dVAR;
    SV *sv;

    PERL_ARGS_ASSERT_SAVE_AELEM;

    SvGETMAGIC(*sptr);
    save_pushptri32ptr(SvREFCNT_inc_simple(av), idx, SvREFCNT_inc(*sptr),
		       SAVEt_AELEM);
    /* if it gets reified later, the restore will have the wrong refcnt */
    if (!AvREAL(av) && AvREIFY(av))
	SvREFCNT_inc_void(*sptr);
    save_scalar_at(sptr);
    sv = *sptr;
    /* If we're localizing a tied array element, this new sv
     * won't actually be stored in the array - so it won't get
     * reaped when the localize ends. Ensure it gets reaped by
     * mortifying it instead. DAPM */
    if (SvTIED_mg(sv, PERL_MAGIC_tiedelem))
	sv_2mortal(sv);
}

void
Perl_save_helem(pTHX_ HV *hv, SV *key, SV **sptr)
{
    dVAR;
    SV *sv;

    PERL_ARGS_ASSERT_SAVE_HELEM;

    SvGETMAGIC(*sptr);
    SSCHECK(4);
    SSPUSHPTR(SvREFCNT_inc_simple(hv));
    SSPUSHPTR(newSVsv(key));
    SSPUSHPTR(SvREFCNT_inc(*sptr));
    SSPUSHINT(SAVEt_HELEM);
    save_scalar_at(sptr);
    sv = *sptr;
    /* If we're localizing a tied hash element, this new sv
     * won't actually be stored in the hash - so it won't get
     * reaped when the localize ends. Ensure it gets reaped by
     * mortifying it instead. DAPM */
    if (SvTIED_mg(sv, PERL_MAGIC_tiedelem))
	sv_2mortal(sv);
}

SV*
Perl_save_svref(pTHX_ SV **sptr)
{
    dVAR;

    PERL_ARGS_ASSERT_SAVE_SVREF;

    SvGETMAGIC(*sptr);
    save_pushptrptr(sptr, SvREFCNT_inc(*sptr), SAVEt_SVREF);
    return save_scalar_at(sptr);
}

I32
Perl_save_alloc(pTHX_ I32 size, I32 pad)
{
    dVAR;
    register const I32 start = pad + ((char*)&PL_savestack[PL_savestack_ix]
				- (char*)PL_savestack);
    register const I32 elems = 1 + ((size + pad - 1) / sizeof(*PL_savestack));

    SSGROW(elems + 2);

    PL_savestack_ix += elems;
    SSPUSHINT(elems);
    SSPUSHINT(SAVEt_ALLOC);
    return start;
}

void
Perl_leave_scope(pTHX_ I32 base)
{
    dVAR;
    register SV *sv;
    register SV *value;
    register GV *gv;
    register AV *av;
    register HV *hv;
    void* ptr;
    register char* str;
    I32 i;
    /* Localise the effects of the TAINT_NOT inside the loop.  */
    const bool was = PL_tainted;

    if (base < -1)
	Perl_croak(aTHX_ "panic: corrupt saved stack index");
    while (PL_savestack_ix > base) {
	TAINT_NOT;

	switch (SSPOPINT) {
	case SAVEt_ITEM:			/* normal string */
	    value = MUTABLE_SV(SSPOPPTR);
	    sv = MUTABLE_SV(SSPOPPTR);
	    sv_replace(sv,value);
	    PL_localizing = 2;
	    SvSETMAGIC(sv);
	    PL_localizing = 0;
	    break;
	case SAVEt_SV:				/* scalar reference */
	    value = MUTABLE_SV(SSPOPPTR);
	    gv = MUTABLE_GV(SSPOPPTR);
	    ptr = &GvSV(gv);
	    av = MUTABLE_AV(gv); /* what to refcnt_dec */
	restore_sv:
	    sv = *(SV**)ptr;
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "restore svref: %p %p:%s -> %p:%s\n",
				  (void*)ptr, (void*)sv, SvPEEK(sv),
				  (void*)value, SvPEEK(value)));
	    *(SV**)ptr = value;
	    SvREFCNT_dec(sv);
	    PL_localizing = 2;
	    SvSETMAGIC(value);
	    PL_localizing = 0;
	    SvREFCNT_dec(value);
	    if (av) /* actually an av, hv or gv */
		SvREFCNT_dec(av);
	    break;
	case SAVEt_GENERIC_PVREF:		/* generic pv */
	    ptr = SSPOPPTR;
	    str = (char*)SSPOPPTR;
	    if (*(char**)ptr != str) {
		Safefree(*(char**)ptr);
		*(char**)ptr = str;
	    }
	    break;
	case SAVEt_SHARED_PVREF:		/* shared pv */
	    str = (char*)SSPOPPTR;
	    ptr = SSPOPPTR;
	    if (*(char**)ptr != str) {
#ifdef NETWARE
		PerlMem_free(*(char**)ptr);
#else
		PerlMemShared_free(*(char**)ptr);
#endif
		*(char**)ptr = str;
	    }
	    break;
	case SAVEt_GENERIC_SVREF:		/* generic sv */
	    value = MUTABLE_SV(SSPOPPTR);
	    ptr = SSPOPPTR;
	    sv = *(SV**)ptr;
	    *(SV**)ptr = value;
	    SvREFCNT_dec(sv);
	    SvREFCNT_dec(value);
	    break;
	case SAVEt_AV:				/* array reference */
	    av = MUTABLE_AV(SSPOPPTR);
	    gv = MUTABLE_GV(SSPOPPTR);
	    if (GvAV(gv)) {
		SvREFCNT_dec(GvAV(gv));
	    }
	    GvAV(gv) = av;
	    if (SvMAGICAL(av)) {
		PL_localizing = 2;
		SvSETMAGIC(MUTABLE_SV(av));
		PL_localizing = 0;
	    }
	    break;
	case SAVEt_HV:				/* hash reference */
	    hv = MUTABLE_HV(SSPOPPTR);
	    gv = MUTABLE_GV(SSPOPPTR);
	    if (GvHV(gv)) {
		SvREFCNT_dec(GvHV(gv));
	    }
	    GvHV(gv) = hv;
	    if (SvMAGICAL(hv)) {
		PL_localizing = 2;
		SvSETMAGIC(MUTABLE_SV(hv));
		PL_localizing = 0;
	    }
	    break;
	case SAVEt_INT:				/* int reference */
	    ptr = SSPOPPTR;
	    *(int*)ptr = (int)SSPOPINT;
	    break;
	case SAVEt_BOOL:			/* bool reference */
	    ptr = SSPOPPTR;
	    *(bool*)ptr = (bool)SSPOPBOOL;
	    break;
	case SAVEt_I32:				/* I32 reference */
	    ptr = SSPOPPTR;
#ifdef PERL_DEBUG_READONLY_OPS
	    {
		const I32 val = SSPOPINT;
		if (*(I32*)ptr != val)
		    *(I32*)ptr = val;
	    }
#else
	    *(I32*)ptr = (I32)SSPOPINT;
#endif
	    break;
	case SAVEt_SPTR:			/* SV* reference */
	    ptr = SSPOPPTR;
	    *(SV**)ptr = MUTABLE_SV(SSPOPPTR);
	    break;
	case SAVEt_VPTR:			/* random* reference */
	case SAVEt_PPTR:			/* char* reference */
	    ptr = SSPOPPTR;
	    *(char**)ptr = (char*)SSPOPPTR;
	    break;
	case SAVEt_HPTR:			/* HV* reference */
	    ptr = SSPOPPTR;
	    *(HV**)ptr = MUTABLE_HV(SSPOPPTR);
	    break;
	case SAVEt_APTR:			/* AV* reference */
	    ptr = SSPOPPTR;
	    *(AV**)ptr = MUTABLE_AV(SSPOPPTR);
	    break;
	case SAVEt_GP:				/* scalar reference */
	    ptr = SSPOPPTR;
	    gv = MUTABLE_GV(SSPOPPTR);
	    gp_free(gv);
	    GvGP(gv) = (GP*)ptr;
            /* putting a method back into circulation ("local")*/
	    if (GvCVu(gv) && (hv=GvSTASH(gv)) && HvNAME_get(hv))
                mro_method_changed_in(hv);
	    SvREFCNT_dec(gv);
	    break;
	case SAVEt_FREESV:
	    ptr = SSPOPPTR;
	    SvREFCNT_dec(MUTABLE_SV(ptr));
	    break;
	case SAVEt_MORTALIZESV:
	    ptr = SSPOPPTR;
	    sv_2mortal(MUTABLE_SV(ptr));
	    break;
	case SAVEt_FREEOP:
	    ptr = SSPOPPTR;
	    ASSERT_CURPAD_LEGAL("SAVEt_FREEOP"); /* XXX DAPM tmp */
	    op_free((OP*)ptr);
	    break;
	case SAVEt_FREEPV:
	    ptr = SSPOPPTR;
	    Safefree(ptr);
	    break;
	case SAVEt_CLEARSV:
	    ptr = (void*)&PL_curpad[SSPOPLONG];
	    sv = *(SV**)ptr;

	    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
	     "Pad 0x%"UVxf"[0x%"UVxf"] clearsv: %ld sv=0x%"UVxf"<%"IVdf"> %s\n",
		PTR2UV(PL_comppad), PTR2UV(PL_curpad),
		(long)((SV **)ptr-PL_curpad), PTR2UV(sv), (IV)SvREFCNT(sv),
		(SvREFCNT(sv) <= 1 && !SvOBJECT(sv)) ? "clear" : "abandon"
	    ));

	    /* Can clear pad variable in place? */
	    if (SvREFCNT(sv) <= 1 && !SvOBJECT(sv)) {
		/*
		 * if a my variable that was made readonly is going out of
		 * scope, we want to remove the readonlyness so that it can
		 * go out of scope quietly
		 */
		if (SvPADMY(sv) && !SvFAKE(sv))
		    SvREADONLY_off(sv);

		if (SvTHINKFIRST(sv))
		    sv_force_normal_flags(sv, SV_IMMEDIATE_UNREF);
		if (SvMAGICAL(sv))
		    mg_free(sv);

		switch (SvTYPE(sv)) {
		case SVt_NULL:
		    break;
		case SVt_PVAV:
		    av_clear(MUTABLE_AV(sv));
		    break;
		case SVt_PVHV:
		    hv_clear(MUTABLE_HV(sv));
		    break;
		case SVt_PVCV:
		    Perl_croak(aTHX_ "panic: leave_scope pad code");
		default:
		    SvOK_off(sv);
		    break;
		}
		SvPADSTALE_on(sv); /* mark as no longer live */
	    }
	    else {	/* Someone has a claim on this, so abandon it. */
		const U32 padflags = SvFLAGS(sv) & (SVs_PADMY|SVs_PADTMP);
		switch (SvTYPE(sv)) {	/* Console ourselves with a new value */
		case SVt_PVAV:	*(SV**)ptr = MUTABLE_SV(newAV());	break;
		case SVt_PVHV:	*(SV**)ptr = MUTABLE_SV(newHV());	break;
		default:	*(SV**)ptr = newSV(0);		break;
		}
		SvREFCNT_dec(sv);	/* Cast current value to the winds. */
		/* preserve pad nature, but also mark as not live
		 * for any closure capturing */
		SvFLAGS(*(SV**)ptr) |= padflags | SVs_PADSTALE;
	    }
	    break;
	case SAVEt_DELETE:
	    ptr = SSPOPPTR;
	    hv = MUTABLE_HV(ptr);
	    i = SSPOPINT;
	    ptr = SSPOPPTR;
	    (void)hv_delete(hv, (char*)ptr, i, G_DISCARD);
	    SvREFCNT_dec(hv);
	    Safefree(ptr);
	    break;
	case SAVEt_DESTRUCTOR_X:
	    ptr = SSPOPPTR;
	    (*SSPOPDXPTR)(aTHX_ ptr);
	    break;
	case SAVEt_REGCONTEXT:
	case SAVEt_ALLOC:
	    i = SSPOPINT;
	    PL_savestack_ix -= i;  	/* regexp must have croaked */
	    break;
	case SAVEt_STACK_POS:		/* Position on Perl stack */
	    i = SSPOPINT;
	    PL_stack_sp = PL_stack_base + i;
	    break;
	case SAVEt_STACK_CXPOS:         /* blk_oldsp on context stack */
	    i = SSPOPINT;
	    cxstack[i].blk_oldsp = SSPOPINT;
	    break;
	case SAVEt_AELEM:		/* array element */
	    value = MUTABLE_SV(SSPOPPTR);
	    i = SSPOPINT;
	    av = MUTABLE_AV(SSPOPPTR);
	    ptr = av_fetch(av,i,1);
	    if (!AvREAL(av) && AvREIFY(av)) /* undo reify guard */
		SvREFCNT_dec(value);
	    if (ptr) {
		sv = *(SV**)ptr;
		if (sv && sv != &PL_sv_undef) {
		    if (SvTIED_mg((const SV *)av, PERL_MAGIC_tied))
			SvREFCNT_inc_void_NN(sv);
		    goto restore_sv;
		}
	    }
	    SvREFCNT_dec(av);
	    SvREFCNT_dec(value);
	    break;
	case SAVEt_HELEM:		/* hash element */
	    value = MUTABLE_SV(SSPOPPTR);
	    sv = MUTABLE_SV(SSPOPPTR);
	    hv = MUTABLE_HV(SSPOPPTR);
	    ptr = hv_fetch_ent(hv, sv, 1, 0);
	    SvREFCNT_dec(sv);
	    if (ptr) {
		const SV * const oval = HeVAL((HE*)ptr);
		if (oval && oval != &PL_sv_undef) {
		    ptr = &HeVAL((HE*)ptr);
		    if (SvTIED_mg((const SV *)hv, PERL_MAGIC_tied))
			SvREFCNT_inc_void(*(SV**)ptr);
		    av = MUTABLE_AV(hv); /* what to refcnt_dec */
		    goto restore_sv;
		}
	    }
	    SvREFCNT_dec(hv);
	    SvREFCNT_dec(value);
	    break;
	case SAVEt_OP:
	    PL_op = (OP*)SSPOPPTR;
	    break;
	case SAVEt_HINTS:
	    if ((PL_hints & HINT_LOCALIZE_HH) && GvHV(PL_hintgv)) {
		SvREFCNT_dec(MUTABLE_SV(GvHV(PL_hintgv)));
		GvHV(PL_hintgv) = NULL;
	    }
	    *(I32*)&PL_hints = (I32)SSPOPINT;
	    Perl_refcounted_he_free(aTHX_ PL_compiling.cop_hints_hash);
	    PL_compiling.cop_hints_hash = (struct refcounted_he *) SSPOPPTR;
	    if (PL_hints & HINT_LOCALIZE_HH) {
		SvREFCNT_dec(MUTABLE_SV(GvHV(PL_hintgv)));
		GvHV(PL_hintgv) = MUTABLE_HV(SSPOPPTR);
		assert(GvHV(PL_hintgv));
	    } else if (!GvHV(PL_hintgv)) {
		/* Need to add a new one manually, else gv_fetchpv() can
		   add one in this code:
		   
		   if (SvTYPE(gv) == SVt_PVGV) {
		       if (add) {
		       GvMULTI_on(gv);
		       gv_init_sv(gv, sv_type);
		       if (*name=='!' && sv_type == SVt_PVHV && len==1)
			   require_errno(gv);
		       }
		       return gv;
		   }

		   and it won't have the magic set.  */

		HV *const hv = newHV();
		hv_magic(hv, NULL, PERL_MAGIC_hints);
		GvHV(PL_hintgv) = hv;
	    }
	    assert(GvHV(PL_hintgv));
	    break;
	case SAVEt_COMPPAD:
	    PL_comppad = (PAD*)SSPOPPTR;
	    if (PL_comppad)
		PL_curpad = AvARRAY(PL_comppad);
	    else
		PL_curpad = NULL;
	    break;
	case SAVEt_PADSV_AND_MORTALIZE:
	    {
		const PADOFFSET off = (PADOFFSET)SSPOPLONG;
		SV **svp;
		ptr = SSPOPPTR;
		assert (ptr);
		svp = AvARRAY((PAD*)ptr) + off;
		/* This mortalizing used to be done by POPLOOP() via itersave.
		   But as we have all the information here, we can do it here,
		   save even having to have itersave in the struct.  */
		sv_2mortal(*svp);
		*svp = MUTABLE_SV(SSPOPPTR);
	    }
	    break;
	case SAVEt_SAVESWITCHSTACK:
	    {
		dSP;
		AV *const t = MUTABLE_AV(SSPOPPTR);
		AV *const f = MUTABLE_AV(SSPOPPTR);
		SWITCHSTACK(t,f);
		PL_curstackinfo->si_stack = f;
	    }
	    break;
	case SAVEt_SET_SVFLAGS:
	    {
		const U32 val  = (U32)SSPOPINT;
		const U32 mask = (U32)SSPOPINT;
		sv = MUTABLE_SV(SSPOPPTR);
		SvFLAGS(sv) &= ~mask;
		SvFLAGS(sv) |= val;
	    }
	    break;

	    /* This would be a mathom, but Perl_save_svref() calls a static
	       function, S_save_scalar_at(), so has to stay in this file.  */
	case SAVEt_SVREF:			/* scalar reference */
	    value = MUTABLE_SV(SSPOPPTR);
	    ptr = SSPOPPTR;
	    av = NULL; /* what to refcnt_dec */
	    goto restore_sv;

	    /* These are only saved in mathoms.c */
	case SAVEt_NSTAB:
	    gv = MUTABLE_GV(SSPOPPTR);
	    (void)sv_clear(MUTABLE_SV(gv));
	    break;
	case SAVEt_LONG:			/* long reference */
	    ptr = SSPOPPTR;
	    *(long*)ptr = (long)SSPOPLONG;
	    break;
	case SAVEt_IV:				/* IV reference */
	    ptr = SSPOPPTR;
	    *(IV*)ptr = (IV)SSPOPIV;
	    break;

	    /* This case is rendered redundant by the integration of change
	       33078. See the comment near Perl_save_padsv().  */
	case SAVEt_PADSV:
	    {
		const PADOFFSET off = (PADOFFSET)SSPOPLONG;
		ptr = SSPOPPTR;
		if (ptr)
		    AvARRAY((PAD*)ptr)[off] = (SV*)SSPOPPTR;
		else {
		  /* Can we ever get here?
		     POPs must balance PUSHes.  */
		    (void) SSPOPPTR;
		}
	    }
	    break;
	case SAVEt_I16:				/* I16 reference */
	    ptr = SSPOPPTR;
	    *(I16*)ptr = (I16)SSPOPINT;
	    break;
	case SAVEt_I8:				/* I8 reference */
	    ptr = SSPOPPTR;
	    *(I8*)ptr = (I8)SSPOPINT;
	    break;
	case SAVEt_DESTRUCTOR:
	    ptr = SSPOPPTR;
	    (*SSPOPDPTR)(ptr);
	    break;
	case SAVEt_COP_ARYBASE:
	    ptr = SSPOPPTR;
	    i = SSPOPINT;
	    CopARYBASE_set((COP *)ptr, i);
	    break;
	case SAVEt_COMPILE_WARNINGS:
	    ptr = SSPOPPTR;

	    if (!specialWARN(PL_compiling.cop_warnings))
		PerlMemShared_free(PL_compiling.cop_warnings);

	    PL_compiling.cop_warnings = (STRLEN*)ptr;
	    break;
	case SAVEt_RE_STATE:
	    {
		const struct re_save_state *const state
		    = (struct re_save_state *)
		    (PL_savestack + PL_savestack_ix
		     - SAVESTACK_ALLOC_FOR_RE_SAVE_STATE);
		PL_savestack_ix -= SAVESTACK_ALLOC_FOR_RE_SAVE_STATE;

		if (PL_reg_start_tmp != state->re_state_reg_start_tmp) {
		    Safefree(PL_reg_start_tmp);
		}
		if (PL_reg_poscache != state->re_state_reg_poscache) {
		    Safefree(PL_reg_poscache);
		}
		Copy(state, &PL_reg_state, 1, struct re_save_state);
	    }
	    break;
	case SAVEt_PARSER:
	    ptr = SSPOPPTR;
	    parser_free((yy_parser *) ptr);
	    break;
	default:
	    Perl_croak(aTHX_ "panic: leave_scope inconsistency");
	}
    }

    PL_tainted = was;
}

void
Perl_cx_dump(pTHX_ PERL_CONTEXT *cx)
{
    dVAR;

    PERL_ARGS_ASSERT_CX_DUMP;

#ifdef DEBUGGING
    PerlIO_printf(Perl_debug_log, "CX %ld = %s\n", (long)(cx - cxstack), PL_block_type[CxTYPE(cx)]);
    if (CxTYPE(cx) != CXt_SUBST) {
	PerlIO_printf(Perl_debug_log, "BLK_OLDSP = %ld\n", (long)cx->blk_oldsp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDCOP = 0x%"UVxf"\n",
		      PTR2UV(cx->blk_oldcop));
	PerlIO_printf(Perl_debug_log, "BLK_OLDMARKSP = %ld\n", (long)cx->blk_oldmarksp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDSCOPESP = %ld\n", (long)cx->blk_oldscopesp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDPM = 0x%"UVxf"\n",
		      PTR2UV(cx->blk_oldpm));
	PerlIO_printf(Perl_debug_log, "BLK_GIMME = %s\n", cx->blk_gimme ? "LIST" : "SCALAR");
    }
    switch (CxTYPE(cx)) {
    case CXt_NULL:
    case CXt_BLOCK:
	break;
    case CXt_FORMAT:
	PerlIO_printf(Perl_debug_log, "BLK_SUB.CV = 0x%"UVxf"\n",
		PTR2UV(cx->blk_sub.cv));
	PerlIO_printf(Perl_debug_log, "BLK_SUB.GV = 0x%"UVxf"\n",
		PTR2UV(cx->blk_sub.gv));
	PerlIO_printf(Perl_debug_log, "BLK_SUB.DFOUTGV = 0x%"UVxf"\n",
		PTR2UV(cx->blk_sub.dfoutgv));
	PerlIO_printf(Perl_debug_log, "BLK_SUB.HASARGS = %d\n",
		      (int)CxHASARGS(cx));
	PerlIO_printf(Perl_debug_log, "BLK_SUB.RETOP = 0x%"UVxf"\n",
		PTR2UV(cx->blk_sub.retop));
	break;
    case CXt_SUB:
	PerlIO_printf(Perl_debug_log, "BLK_SUB.CV = 0x%"UVxf"\n",
		PTR2UV(cx->blk_sub.cv));
	PerlIO_printf(Perl_debug_log, "BLK_SUB.OLDDEPTH = %ld\n",
		(long)cx->blk_sub.olddepth);
	PerlIO_printf(Perl_debug_log, "BLK_SUB.HASARGS = %d\n",
		(int)CxHASARGS(cx));
	PerlIO_printf(Perl_debug_log, "BLK_SUB.LVAL = %d\n", (int)CxLVAL(cx));
	PerlIO_printf(Perl_debug_log, "BLK_SUB.RETOP = 0x%"UVxf"\n",
		PTR2UV(cx->blk_sub.retop));
	break;
    case CXt_EVAL:
	PerlIO_printf(Perl_debug_log, "BLK_EVAL.OLD_IN_EVAL = %ld\n",
		(long)CxOLD_IN_EVAL(cx));
	PerlIO_printf(Perl_debug_log, "BLK_EVAL.OLD_OP_TYPE = %s (%s)\n",
		PL_op_name[CxOLD_OP_TYPE(cx)],
		PL_op_desc[CxOLD_OP_TYPE(cx)]);
	if (cx->blk_eval.old_namesv)
	    PerlIO_printf(Perl_debug_log, "BLK_EVAL.OLD_NAME = %s\n",
			  SvPVX_const(cx->blk_eval.old_namesv));
	PerlIO_printf(Perl_debug_log, "BLK_EVAL.OLD_EVAL_ROOT = 0x%"UVxf"\n",
		PTR2UV(cx->blk_eval.old_eval_root));
	PerlIO_printf(Perl_debug_log, "BLK_EVAL.RETOP = 0x%"UVxf"\n",
		PTR2UV(cx->blk_eval.retop));
	break;

    case CXt_LOOP:
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.LABEL = %s\n", CxLABEL(cx));
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.RESETSP = %ld\n",
		(long)cx->blk_loop.resetsp);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.MY_OP = 0x%"UVxf"\n",
		PTR2UV(cx->blk_loop.my_op));
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.NEXT_OP = 0x%"UVxf"\n",
		PTR2UV(CX_LOOP_NEXTOP_GET(cx)));
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERIX = %ld\n",
		(long)cx->blk_loop.iterix);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERARY = 0x%"UVxf"\n",
		PTR2UV(cx->blk_loop.iterary));
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERVAR = 0x%"UVxf"\n",
		PTR2UV(CxITERVAR(cx)));
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERLVAL = 0x%"UVxf"\n",
		PTR2UV(cx->blk_loop.iterlval));
	break;

    case CXt_SUBST:
	PerlIO_printf(Perl_debug_log, "SB_ITERS = %ld\n",
		(long)cx->sb_iters);
	PerlIO_printf(Perl_debug_log, "SB_MAXITERS = %ld\n",
		(long)cx->sb_maxiters);
	PerlIO_printf(Perl_debug_log, "SB_RFLAGS = %ld\n",
		(long)cx->sb_rflags);
	PerlIO_printf(Perl_debug_log, "SB_ONCE = %ld\n",
		(long)CxONCE(cx));
	PerlIO_printf(Perl_debug_log, "SB_ORIG = %s\n",
		cx->sb_orig);
	PerlIO_printf(Perl_debug_log, "SB_DSTR = 0x%"UVxf"\n",
		PTR2UV(cx->sb_dstr));
	PerlIO_printf(Perl_debug_log, "SB_TARG = 0x%"UVxf"\n",
		PTR2UV(cx->sb_targ));
	PerlIO_printf(Perl_debug_log, "SB_S = 0x%"UVxf"\n",
		PTR2UV(cx->sb_s));
	PerlIO_printf(Perl_debug_log, "SB_M = 0x%"UVxf"\n",
		PTR2UV(cx->sb_m));
	PerlIO_printf(Perl_debug_log, "SB_STREND = 0x%"UVxf"\n",
		PTR2UV(cx->sb_strend));
	PerlIO_printf(Perl_debug_log, "SB_RXRES = 0x%"UVxf"\n",
		PTR2UV(cx->sb_rxres));
	break;
    }
#else
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(cx);
#endif	/* DEBUGGING */
}

/* This is rendered a mathom by the integration of change 33078. However, until
   we have versioned mathom logic in mathoms.c, we can't move it there for
   5.10.1, as other code in production may have linked to it.  */

void
Perl_save_padsv(pTHX_ PADOFFSET off)
{
    dVAR;
    SSCHECK(4);
    ASSERT_CURPAD_ACTIVE("save_padsv");
    SSPUSHPTR(PL_curpad[off]);
    SSPUSHPTR(PL_comppad);
    SSPUSHLONG((long)off);
    SSPUSHINT(SAVEt_PADSV);
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
