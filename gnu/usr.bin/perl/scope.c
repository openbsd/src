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
Perl_stack_grow(pTHX_ SV **sp, SV **p, SSize_t n)
{
    SSize_t extra;
    SSize_t current = (p - PL_stack_base);

    PERL_ARGS_ASSERT_STACK_GROW;

    if (UNLIKELY(n < 0))
        Perl_croak(aTHX_
            "panic: stack_grow() negative count (%"IVdf")", (IV)n);

    PL_stack_sp = sp;
    extra =
#ifdef STRESS_REALLOC
        1;
#else
        128;
#endif
    /* If the total might wrap, panic instead. This is really testing
     * that (current + n + extra < SSize_t_MAX), but done in a way that
     * can't wrap */
    if (UNLIKELY(   current         > SSize_t_MAX - extra
                 || current + extra > SSize_t_MAX - n
    ))
        /* diag_listed_as: Out of memory during %s extend */
        Perl_croak(aTHX_ "Out of memory during stack extend");

    av_extend(PL_curstack, current + n + extra);
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
    /* Without any kind of initialising CX_PUSHSUBST()
     * in pp_subst() will read uninitialised heap. */
    PoisonNew(si->si_cxstack, cxitems, PERL_CONTEXT);
    return si;
}

I32
Perl_cxinc(pTHX)
{
    const IV old_max = cxstack_max;
    cxstack_max = GROW(cxstack_max);
    Renew(cxstack, cxstack_max + 1, PERL_CONTEXT);
    /* Without any kind of initialising deep enough recursion
     * will end up reading uninitialised PERL_CONTEXTs. */
    PoisonNew(cxstack + old_max + 1, cxstack_max - old_max, PERL_CONTEXT);
    return cxstack_ix + 1;
}

void
Perl_push_scope(pTHX)
{
    if (UNLIKELY(PL_scopestack_ix == PL_scopestack_max)) {
	PL_scopestack_max = GROW(PL_scopestack_max);
	Renew(PL_scopestack, PL_scopestack_max, I32);
#ifdef DEBUGGING
	Renew(PL_scopestack_name, PL_scopestack_max, const char*);
#endif
    }
#ifdef DEBUGGING
    PL_scopestack_name[PL_scopestack_ix] = "unknown";
#endif
    PL_scopestack[PL_scopestack_ix++] = PL_savestack_ix;

}

void
Perl_pop_scope(pTHX)
{
    const I32 oldsave = PL_scopestack[--PL_scopestack_ix];
    LEAVE_SCOPE(oldsave);
}

I32 *
Perl_markstack_grow(pTHX)
{
    const I32 oldmax = PL_markstack_max - PL_markstack;
    const I32 newmax = GROW(oldmax);

    Renew(PL_markstack, newmax, I32);
    PL_markstack_max = PL_markstack + newmax;
    PL_markstack_ptr = PL_markstack + oldmax;
    DEBUG_s(DEBUG_v(PerlIO_printf(Perl_debug_log,
            "MARK grow %p %"IVdf" by %"IVdf"\n",
            PL_markstack_ptr, (IV)*PL_markstack_ptr, (IV)oldmax)));
    return PL_markstack_ptr;
}

void
Perl_savestack_grow(pTHX)
{
    PL_savestack_max = GROW(PL_savestack_max);
    /* Note that we allocate SS_MAXPUSH slots higher than ss_max
     * so that SS_ADD_END(), SSGROW() etc can do a simper check */
    Renew(PL_savestack, PL_savestack_max + SS_MAXPUSH, ANY);
}

void
Perl_savestack_grow_cnt(pTHX_ I32 need)
{
    PL_savestack_max = PL_savestack_ix + need;
    /* Note that we allocate SS_MAXPUSH slots higher than ss_max
     * so that SS_ADD_END(), SSGROW() etc can do a simper check */
    Renew(PL_savestack, PL_savestack_max + SS_MAXPUSH, ANY);
}

#undef GROW

/*  The original function was called Perl_tmps_grow and was removed from public
    API, Perl_tmps_grow_p is the replacement and it used in public macros but
    isn't public itself.

    Perl_tmps_grow_p takes a proposed ix. A proposed ix is PL_tmps_ix + extend_by,
    where the result of (PL_tmps_ix + extend_by) is >= PL_tmps_max
    Upon return, PL_tmps_stack[ix] will be a valid address. For machine code
    optimization and register usage reasons, the proposed ix passed into
    tmps_grow is returned to the caller which the caller can then use to write
    an SV * to PL_tmps_stack[ix]. If the caller was using tmps_grow in
    pre-extend mode (EXTEND_MORTAL macro), then it ignores the return value of
    tmps_grow. Note, tmps_grow DOES NOT write ix to PL_tmps_ix, the caller
    must assign ix or ret val of tmps_grow to PL_temps_ix themselves if that is
    appropriate. The assignment to PL_temps_ix can happen before or after
    tmps_grow call since tmps_grow doesn't look at PL_tmps_ix.
 */

SSize_t
Perl_tmps_grow_p(pTHX_ SSize_t ix)
{
    SSize_t extend_to = ix;
#ifndef STRESS_REALLOC
    if (ix - PL_tmps_max < 128)
	extend_to += (PL_tmps_max < 512) ? 128 : 512;
#endif
    PL_tmps_max = extend_to + 1;
    Renew(PL_tmps_stack, PL_tmps_max, SV*);
    return ix;
}


void
Perl_free_tmps(pTHX)
{
    /* XXX should tmps_floor live in cxstack? */
    const SSize_t myfloor = PL_tmps_floor;
    while (PL_tmps_ix > myfloor) {      /* clean up after last statement */
	SV* const sv = PL_tmps_stack[PL_tmps_ix--];
#ifdef PERL_POISON
	PoisonWith(PL_tmps_stack + PL_tmps_ix + 1, 1, SV *, 0xAB);
#endif
	if (LIKELY(sv)) {
	    SvTEMP_off(sv);
	    SvREFCNT_dec_NN(sv);		/* note, can modify tmps_ix!!! */
	}
    }
}

STATIC SV *
S_save_scalar_at(pTHX_ SV **sptr, const U32 flags)
{
    SV * osv;
    SV *sv;

    PERL_ARGS_ASSERT_SAVE_SCALAR_AT;

    osv = *sptr;
    if (flags & SAVEf_KEEPOLDELEM)
        sv = osv;
    else {
        sv  = (*sptr = newSV(0));
        if (SvTYPE(osv) >= SVt_PVMG && SvMAGIC(osv))
            mg_localize(osv, sv, cBOOL(flags & SAVEf_SETMAGIC));
    }

    return sv;
}

void
Perl_save_pushptrptr(pTHX_ void *const ptr1, void *const ptr2, const int type)
{
    dSS_ADD;
    SS_ADD_PTR(ptr1);
    SS_ADD_PTR(ptr2);
    SS_ADD_UV(type);
    SS_ADD_END(3);
}

SV *
Perl_save_scalar(pTHX_ GV *gv)
{
    SV ** const sptr = &GvSVn(gv);

    PERL_ARGS_ASSERT_SAVE_SCALAR;

    if (UNLIKELY(SvGMAGICAL(*sptr))) {
        PL_localizing = 1;
        (void)mg_get(*sptr);
        PL_localizing = 0;
    }
    save_pushptrptr(SvREFCNT_inc_simple(gv), SvREFCNT_inc(*sptr), SAVEt_SV);
    return save_scalar_at(sptr, SAVEf_SETMAGIC); /* XXX - FIXME - see #60360 */
}

/* Like save_sptr(), but also SvREFCNT_dec()s the new value.  Can be used to
 * restore a global SV to its prior contents, freeing new value. */
void
Perl_save_generic_svref(pTHX_ SV **sptr)
{
    PERL_ARGS_ASSERT_SAVE_GENERIC_SVREF;

    save_pushptrptr(sptr, SvREFCNT_inc(*sptr), SAVEt_GENERIC_SVREF);
}

/* Like save_pptr(), but also Safefree()s the new value if it is different
 * from the old one.  Can be used to restore a global char* to its prior
 * contents, freeing new value. */
void
Perl_save_generic_pvref(pTHX_ char **str)
{
    PERL_ARGS_ASSERT_SAVE_GENERIC_PVREF;

    save_pushptrptr(*str, str, SAVEt_GENERIC_PVREF);
}

/* Like save_generic_pvref(), but uses PerlMemShared_free() rather than Safefree().
 * Can be used to restore a shared global char* to its prior
 * contents, freeing new value. */
void
Perl_save_shared_pvref(pTHX_ char **str)
{
    PERL_ARGS_ASSERT_SAVE_SHARED_PVREF;

    save_pushptrptr(str, *str, SAVEt_SHARED_PVREF);
}

/* set the SvFLAGS specified by mask to the values in val */

void
Perl_save_set_svflags(pTHX_ SV* sv, U32 mask, U32 val)
{
    dSS_ADD;

    PERL_ARGS_ASSERT_SAVE_SET_SVFLAGS;

    SS_ADD_PTR(sv);
    SS_ADD_INT(mask);
    SS_ADD_INT(val);
    SS_ADD_UV(SAVEt_SET_SVFLAGS);
    SS_ADD_END(4);
}

/*
=for apidoc save_gp

Saves the current GP of gv on the save stack to be restored on scope exit.

If empty is true, replace the GP with a new GP.

If empty is false, mark gv with GVf_INTRO so the next reference
assigned is localized, which is how C< local *foo = $someref; > works.

=cut
*/

void
Perl_save_gp(pTHX_ GV *gv, I32 empty)
{
    PERL_ARGS_ASSERT_SAVE_GP;

    save_pushptrptr(SvREFCNT_inc(gv), GvGP(gv), SAVEt_GP);

    if (empty) {
	GP *gp = Perl_newGP(aTHX_ gv);
	HV * const stash = GvSTASH(gv);
	bool isa_changed = 0;

	if (stash && HvENAME(stash)) {
	    if (GvNAMELEN(gv) == 3 && strnEQ(GvNAME(gv), "ISA", 3))
		isa_changed = TRUE;
	    else if (GvCVu(gv))
		/* taking a method out of circulation ("local")*/
                mro_method_changed_in(stash);
	}
	if (GvIOp(gv) && (IoFLAGS(GvIOp(gv)) & IOf_ARGV)) {
	    gp->gp_io = newIO();
	    IoFLAGS(gp->gp_io) |= IOf_ARGV|IOf_START;
	}
	GvGP_set(gv,gp);
	if (isa_changed) mro_isa_changed_in(stash);
    }
    else {
	gp_ref(GvGP(gv));
	GvINTRO_on(gv);
    }
}

AV *
Perl_save_ary(pTHX_ GV *gv)
{
    AV * const oav = GvAVn(gv);
    AV *av;

    PERL_ARGS_ASSERT_SAVE_ARY;

    if (UNLIKELY(!AvREAL(oav) && AvREIFY(oav)))
	av_reify(oav);
    save_pushptrptr(SvREFCNT_inc_simple_NN(gv), oav, SAVEt_AV);

    GvAV(gv) = NULL;
    av = GvAVn(gv);
    if (UNLIKELY(SvMAGIC(oav)))
	mg_localize(MUTABLE_SV(oav), MUTABLE_SV(av), TRUE);
    return av;
}

HV *
Perl_save_hash(pTHX_ GV *gv)
{
    HV *ohv, *hv;

    PERL_ARGS_ASSERT_SAVE_HASH;

    save_pushptrptr(
	SvREFCNT_inc_simple_NN(gv), (ohv = GvHVn(gv)), SAVEt_HV
    );

    GvHV(gv) = NULL;
    hv = GvHVn(gv);
    if (UNLIKELY(SvMAGIC(ohv)))
	mg_localize(MUTABLE_SV(ohv), MUTABLE_SV(hv), TRUE);
    return hv;
}

void
Perl_save_item(pTHX_ SV *item)
{
    SV * const sv = newSVsv(item);

    PERL_ARGS_ASSERT_SAVE_ITEM;

    save_pushptrptr(item, /* remember the pointer */
		    sv,   /* remember the value */
		    SAVEt_ITEM);
}

void
Perl_save_bool(pTHX_ bool *boolp)
{
    dSS_ADD;

    PERL_ARGS_ASSERT_SAVE_BOOL;

    SS_ADD_PTR(boolp);
    SS_ADD_UV(SAVEt_BOOL | (*boolp << 8));
    SS_ADD_END(2);
}

void
Perl_save_pushi32ptr(pTHX_ const I32 i, void *const ptr, const int type)
{
    dSS_ADD;

    SS_ADD_INT(i);
    SS_ADD_PTR(ptr);
    SS_ADD_UV(type);
    SS_ADD_END(3);
}

void
Perl_save_int(pTHX_ int *intp)
{
    const int i = *intp;
    UV type = ((UV)((UV)i << SAVE_TIGHT_SHIFT) | SAVEt_INT_SMALL);
    int size = 2;
    dSS_ADD;

    PERL_ARGS_ASSERT_SAVE_INT;

    if (UNLIKELY((int)(type >> SAVE_TIGHT_SHIFT) != i)) {
        SS_ADD_INT(i);
        type = SAVEt_INT;
        size++;
    }
    SS_ADD_PTR(intp);
    SS_ADD_UV(type);
    SS_ADD_END(size);
}

void
Perl_save_I8(pTHX_ I8 *bytep)
{
    dSS_ADD;

    PERL_ARGS_ASSERT_SAVE_I8;

    SS_ADD_PTR(bytep);
    SS_ADD_UV(SAVEt_I8 | ((UV)*bytep << 8));
    SS_ADD_END(2);
}

void
Perl_save_I16(pTHX_ I16 *intp)
{
    dSS_ADD;

    PERL_ARGS_ASSERT_SAVE_I16;

    SS_ADD_PTR(intp);
    SS_ADD_UV(SAVEt_I16 | ((UV)*intp << 8));
    SS_ADD_END(2);
}

void
Perl_save_I32(pTHX_ I32 *intp)
{
    const I32 i = *intp;
    UV type = ((I32)((U32)i << SAVE_TIGHT_SHIFT) | SAVEt_I32_SMALL);
    int size = 2;
    dSS_ADD;

    PERL_ARGS_ASSERT_SAVE_I32;

    if (UNLIKELY((I32)(type >> SAVE_TIGHT_SHIFT) != i)) {
        SS_ADD_INT(i);
        type = SAVEt_I32;
        size++;
    }
    SS_ADD_PTR(intp);
    SS_ADD_UV(type);
    SS_ADD_END(size);
}

void
Perl_save_strlen(pTHX_ STRLEN *ptr)
{
    dSS_ADD;

    PERL_ARGS_ASSERT_SAVE_STRLEN;

    SS_ADD_IV(*ptr);
    SS_ADD_PTR(ptr);
    SS_ADD_UV(SAVEt_STRLEN);
    SS_ADD_END(3);
}

/* Cannot use save_sptr() to store a char* since the SV** cast will
 * force word-alignment and we'll miss the pointer.
 */
void
Perl_save_pptr(pTHX_ char **pptr)
{
    PERL_ARGS_ASSERT_SAVE_PPTR;

    save_pushptrptr(*pptr, pptr, SAVEt_PPTR);
}

void
Perl_save_vptr(pTHX_ void *ptr)
{
    PERL_ARGS_ASSERT_SAVE_VPTR;

    save_pushptrptr(*(char**)ptr, ptr, SAVEt_VPTR);
}

void
Perl_save_sptr(pTHX_ SV **sptr)
{
    PERL_ARGS_ASSERT_SAVE_SPTR;

    save_pushptrptr(*sptr, sptr, SAVEt_SPTR);
}

void
Perl_save_padsv_and_mortalize(pTHX_ PADOFFSET off)
{
    dSS_ADD;

    ASSERT_CURPAD_ACTIVE("save_padsv");
    SS_ADD_PTR(SvREFCNT_inc_simple_NN(PL_curpad[off]));
    SS_ADD_PTR(PL_comppad);
    SS_ADD_UV((UV)off);
    SS_ADD_UV(SAVEt_PADSV_AND_MORTALIZE);
    SS_ADD_END(4);
}

void
Perl_save_hptr(pTHX_ HV **hptr)
{
    PERL_ARGS_ASSERT_SAVE_HPTR;

    save_pushptrptr(*hptr, hptr, SAVEt_HPTR);
}

void
Perl_save_aptr(pTHX_ AV **aptr)
{
    PERL_ARGS_ASSERT_SAVE_APTR;

    save_pushptrptr(*aptr, aptr, SAVEt_APTR);
}

void
Perl_save_pushptr(pTHX_ void *const ptr, const int type)
{
    dSS_ADD;
    SS_ADD_PTR(ptr);
    SS_ADD_UV(type);
    SS_ADD_END(2);
}

void
Perl_save_clearsv(pTHX_ SV **svp)
{
    const UV offset = svp - PL_curpad;
    const UV offset_shifted = offset << SAVE_TIGHT_SHIFT;

    PERL_ARGS_ASSERT_SAVE_CLEARSV;

    ASSERT_CURPAD_ACTIVE("save_clearsv");
    SvPADSTALE_off(*svp); /* mark lexical as active */
    if (UNLIKELY((offset_shifted >> SAVE_TIGHT_SHIFT) != offset)) {
	Perl_croak(aTHX_ "panic: pad offset %"UVuf" out of range (%p-%p)",
		   offset, svp, PL_curpad);
    }

    {
        dSS_ADD;
        SS_ADD_UV(offset_shifted | SAVEt_CLEARSV);
        SS_ADD_END(1);
    }
}

void
Perl_save_delete(pTHX_ HV *hv, char *key, I32 klen)
{
    PERL_ARGS_ASSERT_SAVE_DELETE;

    save_pushptri32ptr(key, klen, SvREFCNT_inc_simple(hv), SAVEt_DELETE);
}

void
Perl_save_hdelete(pTHX_ HV *hv, SV *keysv)
{
    STRLEN len;
    I32 klen;
    const char *key;

    PERL_ARGS_ASSERT_SAVE_HDELETE;

    key  = SvPV_const(keysv, len);
    klen = SvUTF8(keysv) ? -(I32)len : (I32)len;
    SvREFCNT_inc_simple_void_NN(hv);
    save_pushptri32ptr(savepvn(key, len), klen, hv, SAVEt_DELETE);
}

void
Perl_save_adelete(pTHX_ AV *av, SSize_t key)
{
    dSS_ADD;

    PERL_ARGS_ASSERT_SAVE_ADELETE;

    SvREFCNT_inc_void(av);
    SS_ADD_UV(key);
    SS_ADD_PTR(av);
    SS_ADD_IV(SAVEt_ADELETE);
    SS_ADD_END(3);
}

void
Perl_save_destructor(pTHX_ DESTRUCTORFUNC_NOCONTEXT_t f, void* p)
{
    dSS_ADD;
    PERL_ARGS_ASSERT_SAVE_DESTRUCTOR;

    SS_ADD_DPTR(f);
    SS_ADD_PTR(p);
    SS_ADD_UV(SAVEt_DESTRUCTOR);
    SS_ADD_END(3);
}

void
Perl_save_destructor_x(pTHX_ DESTRUCTORFUNC_t f, void* p)
{
    dSS_ADD;

    SS_ADD_DXPTR(f);
    SS_ADD_PTR(p);
    SS_ADD_UV(SAVEt_DESTRUCTOR_X);
    SS_ADD_END(3);
}

void
Perl_save_hints(pTHX)
{
    COPHH *save_cophh = cophh_copy(CopHINTHASH_get(&PL_compiling));
    if (PL_hints & HINT_LOCALIZE_HH) {
	HV *oldhh = GvHV(PL_hintgv);
	save_pushptri32ptr(oldhh, PL_hints, save_cophh, SAVEt_HINTS);
	GvHV(PL_hintgv) = NULL; /* in case copying dies */
	GvHV(PL_hintgv) = hv_copy_hints_hv(oldhh);
    } else {
	save_pushi32ptr(PL_hints, save_cophh, SAVEt_HINTS);
    }
}

static void
S_save_pushptri32ptr(pTHX_ void *const ptr1, const I32 i, void *const ptr2,
			const int type)
{
    dSS_ADD;
    SS_ADD_PTR(ptr1);
    SS_ADD_INT(i);
    SS_ADD_PTR(ptr2);
    SS_ADD_UV(type);
    SS_ADD_END(4);
}

void
Perl_save_aelem_flags(pTHX_ AV *av, SSize_t idx, SV **sptr,
			    const U32 flags)
{
    dSS_ADD;
    SV *sv;

    PERL_ARGS_ASSERT_SAVE_AELEM_FLAGS;

    SvGETMAGIC(*sptr);
    SS_ADD_PTR(SvREFCNT_inc_simple(av));
    SS_ADD_IV(idx);
    SS_ADD_PTR(SvREFCNT_inc(*sptr));
    SS_ADD_UV(SAVEt_AELEM);
    SS_ADD_END(4);
    /* The array needs to hold a reference count on its new element, so it
       must be AvREAL. */
    if (UNLIKELY(!AvREAL(av) && AvREIFY(av)))
	av_reify(av);
    save_scalar_at(sptr, flags); /* XXX - FIXME - see #60360 */
    if (flags & SAVEf_KEEPOLDELEM)
	return;
    sv = *sptr;
    /* If we're localizing a tied array element, this new sv
     * won't actually be stored in the array - so it won't get
     * reaped when the localize ends. Ensure it gets reaped by
     * mortifying it instead. DAPM */
    if (UNLIKELY(SvTIED_mg((const SV *)av, PERL_MAGIC_tied)))
	sv_2mortal(sv);
}

void
Perl_save_helem_flags(pTHX_ HV *hv, SV *key, SV **sptr, const U32 flags)
{
    SV *sv;

    PERL_ARGS_ASSERT_SAVE_HELEM_FLAGS;

    SvGETMAGIC(*sptr);
    {
        dSS_ADD;
        SS_ADD_PTR(SvREFCNT_inc_simple(hv));
        SS_ADD_PTR(newSVsv(key));
        SS_ADD_PTR(SvREFCNT_inc(*sptr));
        SS_ADD_UV(SAVEt_HELEM);
        SS_ADD_END(4);
    }
    save_scalar_at(sptr, flags);
    if (flags & SAVEf_KEEPOLDELEM)
	return;
    sv = *sptr;
    /* If we're localizing a tied hash element, this new sv
     * won't actually be stored in the hash - so it won't get
     * reaped when the localize ends. Ensure it gets reaped by
     * mortifying it instead. DAPM */
    if (UNLIKELY(SvTIED_mg((const SV *)hv, PERL_MAGIC_tied)))
	sv_2mortal(sv);
}

SV*
Perl_save_svref(pTHX_ SV **sptr)
{
    PERL_ARGS_ASSERT_SAVE_SVREF;

    SvGETMAGIC(*sptr);
    save_pushptrptr(sptr, SvREFCNT_inc(*sptr), SAVEt_SVREF);
    return save_scalar_at(sptr, SAVEf_SETMAGIC); /* XXX - FIXME - see #60360 */
}


void
Perl_savetmps(pTHX)
{
    dSS_ADD;
    SS_ADD_IV(PL_tmps_floor);
    PL_tmps_floor = PL_tmps_ix;
    SS_ADD_UV(SAVEt_TMPSFLOOR);
    SS_ADD_END(2);
}


I32
Perl_save_alloc(pTHX_ I32 size, I32 pad)
{
    const I32 start = pad + ((char*)&PL_savestack[PL_savestack_ix]
                          - (char*)PL_savestack);
    const UV elems = 1 + ((size + pad - 1) / sizeof(*PL_savestack));
    const UV elems_shifted = elems << SAVE_TIGHT_SHIFT;

    if (UNLIKELY((elems_shifted >> SAVE_TIGHT_SHIFT) != elems))
	Perl_croak(aTHX_
            "panic: save_alloc elems %"UVuf" out of range (%"IVdf"-%"IVdf")",
		   elems, (IV)size, (IV)pad);

    SSGROW(elems + 1);

    PL_savestack_ix += elems;
    SSPUSHUV(SAVEt_ALLOC | elems_shifted);
    return start;
}



#define ARG0_SV  MUTABLE_SV(arg0.any_ptr)
#define ARG0_AV  MUTABLE_AV(arg0.any_ptr)
#define ARG0_HV  MUTABLE_HV(arg0.any_ptr)
#define ARG0_PTR arg0.any_ptr
#define ARG0_PV  (char*)(arg0.any_ptr)
#define ARG0_PVP (char**)(arg0.any_ptr)
#define ARG0_I32 (arg0.any_i32)

#define ARG1_SV  MUTABLE_SV(arg1.any_ptr)
#define ARG1_AV  MUTABLE_AV(arg1.any_ptr)
#define ARG1_GV  MUTABLE_GV(arg1.any_ptr)
#define ARG1_SVP (SV**)(arg1.any_ptr)
#define ARG1_PVP (char**)(arg1.any_ptr)
#define ARG1_PTR arg1.any_ptr
#define ARG1_PV  (char*)(arg1.any_ptr)
#define ARG1_I32 (arg1.any_i32)

#define ARG2_SV  MUTABLE_SV(arg2.any_ptr)
#define ARG2_AV  MUTABLE_AV(arg2.any_ptr)
#define ARG2_HV  MUTABLE_HV(arg2.any_ptr)
#define ARG2_GV  MUTABLE_GV(arg2.any_ptr)
#define ARG2_PV  (char*)(arg2.any_ptr)

void
Perl_leave_scope(pTHX_ I32 base)
{
    /* Localise the effects of the TAINT_NOT inside the loop.  */
    bool was = TAINT_get;

    I32 i;
    SV *sv;

    ANY arg0, arg1, arg2;

    /* these initialisations are logically unnecessary, but they shut up
     * spurious 'may be used uninitialized' compiler warnings */
    arg0.any_ptr = NULL;
    arg1.any_ptr = NULL;
    arg2.any_ptr = NULL;

    if (UNLIKELY(base < -1))
	Perl_croak(aTHX_ "panic: corrupt saved stack index %ld", (long) base);
    DEBUG_l(Perl_deb(aTHX_ "savestack: releasing items %ld -> %ld\n",
			(long)PL_savestack_ix, (long)base));
    while (PL_savestack_ix > base) {
	UV uv;
	U8 type;

        SV *refsv;
        SV **svp;

	TAINT_NOT;

        {
            I32 ix = PL_savestack_ix - 1;
            ANY *p = &PL_savestack[ix];
            uv = p->any_uv;
            type = (U8)uv & SAVE_MASK;
            if (type > SAVEt_ARG0_MAX) {
                ANY *p0 = p;
                arg0 = *--p;
                if (type > SAVEt_ARG1_MAX) {
                    arg1 = *--p;
                    if (type > SAVEt_ARG2_MAX) {
                        arg2 = *--p;
                    }
                }
                ix -= (p0 - p);
            }
            PL_savestack_ix = ix;
        }

	switch (type) {
	case SAVEt_ITEM:			/* normal string */
	    sv_replace(ARG1_SV, ARG0_SV);
            if (UNLIKELY(SvSMAGICAL(ARG1_SV))) {
                PL_localizing = 2;
                mg_set(ARG1_SV);
                PL_localizing = 0;
            }
	    break;

	    /* This would be a mathom, but Perl_save_svref() calls a static
	       function, S_save_scalar_at(), so has to stay in this file.  */
	case SAVEt_SVREF:			/* scalar reference */
	    svp = ARG1_SVP;
	    refsv = NULL; /* what to refcnt_dec */
	    goto restore_sv;

	case SAVEt_SV:				/* scalar reference */
	    svp = &GvSV(ARG1_GV);
	    refsv = ARG1_SV; /* what to refcnt_dec */
	restore_sv:
        {
	    SV * const sv = *svp;
	    *svp = ARG0_SV;
	    SvREFCNT_dec(sv);
            if (UNLIKELY(SvSMAGICAL(ARG0_SV))) {
                /* mg_set could die, skipping the freeing of ARG0_SV and
                 * refsv; Ensure that they're always freed in that case */
                dSS_ADD;
                SS_ADD_PTR(ARG0_SV);
                SS_ADD_UV(SAVEt_FREESV);
                SS_ADD_PTR(refsv);
                SS_ADD_UV(SAVEt_FREESV);
                SS_ADD_END(4);
                PL_localizing = 2;
                mg_set(ARG0_SV);
                PL_localizing = 0;
                break;
            }
	    SvREFCNT_dec_NN(ARG0_SV);
	    SvREFCNT_dec(refsv);
	    break;
        }
	case SAVEt_GENERIC_PVREF:		/* generic pv */
	    if (*ARG0_PVP != ARG1_PV) {
		Safefree(*ARG0_PVP);
		*ARG0_PVP = ARG1_PV;
	    }
	    break;
	case SAVEt_SHARED_PVREF:		/* shared pv */
	    if (*ARG1_PVP != ARG0_PV) {
#ifdef NETWARE
		PerlMem_free(*ARG1_PVP);
#else
		PerlMemShared_free(*ARG1_PVP);
#endif
		*ARG1_PVP = ARG0_PV;
	    }
	    break;
	case SAVEt_GVSV:			/* scalar slot in GV */
	    svp = &GvSV(ARG1_GV);
	    goto restore_svp;
	case SAVEt_GENERIC_SVREF:		/* generic sv */
            svp = ARG1_SVP;
	restore_svp:
        {
	    SV * const sv = *svp;
	    *svp = ARG0_SV;
	    SvREFCNT_dec(sv);
	    SvREFCNT_dec(ARG0_SV);
	    break;
        }
	case SAVEt_GVSLOT:			/* any slot in GV */
        {
            HV *const hv = GvSTASH(ARG2_GV);
	    svp = ARG1_SVP;
	    if (hv && HvENAME(hv) && (
		    (ARG0_SV && SvTYPE(ARG0_SV) == SVt_PVCV)
		 || (*svp && SvTYPE(*svp) == SVt_PVCV)
	       ))
	    {
		if ((char *)svp < (char *)GvGP(ARG2_GV)
		 || (char *)svp > (char *)GvGP(ARG2_GV) + sizeof(struct gp)
		 || GvREFCNT(ARG2_GV) > 2) /* "> 2" to ignore savestack's ref */
		    PL_sub_generation++;
		else mro_method_changed_in(hv);
	    }
	    goto restore_svp;
        }
	case SAVEt_AV:				/* array reference */
	    SvREFCNT_dec(GvAV(ARG1_GV));
	    GvAV(ARG1_GV) = ARG0_AV;
          avhv_common:
            if (UNLIKELY(SvSMAGICAL(ARG0_SV))) {
                /* mg_set might die, so make sure ARG1 isn't leaked */
                dSS_ADD;
                SS_ADD_PTR(ARG1_SV);
                SS_ADD_UV(SAVEt_FREESV);
                SS_ADD_END(2);
                PL_localizing = 2;
                mg_set(ARG0_SV);
                PL_localizing = 0;
                break;
            }
	    SvREFCNT_dec_NN(ARG1_GV);
	    break;
	case SAVEt_HV:				/* hash reference */
	    SvREFCNT_dec(GvHV(ARG1_GV));
	    GvHV(ARG1_GV) = ARG0_HV;
            goto avhv_common;

	case SAVEt_INT_SMALL:
	    *(int*)ARG0_PTR = (int)(uv >> SAVE_TIGHT_SHIFT);
	    break;
	case SAVEt_INT:				/* int reference */
	    *(int*)ARG0_PTR = (int)ARG1_I32;
	    break;
	case SAVEt_STRLEN:			/* STRLEN/size_t ref */
	    *(STRLEN*)ARG0_PTR = (STRLEN)arg1.any_iv;
	    break;
	case SAVEt_TMPSFLOOR:			/* restore PL_tmps_floor */
	    PL_tmps_floor = (SSize_t)arg0.any_iv;
	    break;
	case SAVEt_BOOL:			/* bool reference */
	    *(bool*)ARG0_PTR = cBOOL(uv >> 8);
#ifdef NO_TAINT_SUPPORT
            PERL_UNUSED_VAR(was);
#else
	    if (UNLIKELY(ARG0_PTR == &(TAINT_get))) {
		/* If we don't update <was>, to reflect what was saved on the
		 * stack for PL_tainted, then we will overwrite this attempt to
		 * restore it when we exit this routine.  Note that this won't
		 * work if this value was saved in a wider-than necessary type,
		 * such as I32 */
		was = *(bool*)ARG0_PTR;
	    }
#endif
	    break;
	case SAVEt_I32_SMALL:
	    *(I32*)ARG0_PTR = (I32)(uv >> SAVE_TIGHT_SHIFT);
	    break;
	case SAVEt_I32:				/* I32 reference */
#ifdef PERL_DEBUG_READONLY_OPS
            if (*(I32*)ARG0_PTR != ARG1_I32)
#endif
                *(I32*)ARG0_PTR = ARG1_I32;
	    break;
	case SAVEt_SPTR:			/* SV* reference */
	    *(SV**)(ARG0_PTR)= ARG1_SV;
	    break;
	case SAVEt_VPTR:			/* random* reference */
	case SAVEt_PPTR:			/* char* reference */
	    *ARG0_PVP = ARG1_PV;
	    break;
	case SAVEt_HPTR:			/* HV* reference */
	    *(HV**)ARG0_PTR = MUTABLE_HV(ARG1_PTR);
	    break;
	case SAVEt_APTR:			/* AV* reference */
	    *(AV**)ARG0_PTR = ARG1_AV;
	    break;
	case SAVEt_GP:				/* scalar reference */
        {
            HV *hv;
            /* possibly taking a method out of circulation */	
	    const bool had_method = !!GvCVu(ARG1_GV);
	    gp_free(ARG1_GV);
	    GvGP_set(ARG1_GV, (GP*)ARG0_PTR);
	    if ((hv=GvSTASH(ARG1_GV)) && HvENAME_get(hv)) {
	        if (   GvNAMELEN(ARG1_GV) == 3
                    && strnEQ(GvNAME(ARG1_GV), "ISA", 3)
                )
	            mro_isa_changed_in(hv);
                else if (had_method || GvCVu(ARG1_GV))
                    /* putting a method back into circulation ("local")*/	
                    gv_method_changed(ARG1_GV);
	    }
	    SvREFCNT_dec_NN(ARG1_GV);
	    break;
        }
	case SAVEt_FREESV:
	    SvREFCNT_dec(ARG0_SV);
	    break;
	case SAVEt_FREEPADNAME:
	    PadnameREFCNT_dec((PADNAME *)ARG0_PTR);
	    break;
	case SAVEt_FREECOPHH:
	    cophh_free((COPHH *)ARG0_PTR);
	    break;
	case SAVEt_MORTALIZESV:
	    sv_2mortal(ARG0_SV);
	    break;
	case SAVEt_FREEOP:
	    ASSERT_CURPAD_LEGAL("SAVEt_FREEOP");
	    op_free((OP*)ARG0_PTR);
	    break;
	case SAVEt_FREEPV:
	    Safefree(ARG0_PTR);
	    break;

        case SAVEt_CLEARPADRANGE:
            i = (I32)((uv >> SAVE_TIGHT_SHIFT) & OPpPADRANGE_COUNTMASK);
	    svp = &PL_curpad[uv >>
                    (OPpPADRANGE_COUNTSHIFT + SAVE_TIGHT_SHIFT)] + i - 1;
            goto clearsv;
	case SAVEt_CLEARSV:
	    svp = &PL_curpad[uv >> SAVE_TIGHT_SHIFT];
            i = 1;
          clearsv:
            for (; i; i--, svp--) {
                sv = *svp;

                DEBUG_Xv(PerlIO_printf(Perl_debug_log,
             "Pad 0x%"UVxf"[0x%"UVxf"] clearsv: %ld sv=0x%"UVxf"<%"IVdf"> %s\n",
                    PTR2UV(PL_comppad), PTR2UV(PL_curpad),
                    (long)(svp-PL_curpad), PTR2UV(sv), (IV)SvREFCNT(sv),
                    (SvREFCNT(sv) <= 1 && !SvOBJECT(sv)) ? "clear" : "abandon"
                ));

                /* Can clear pad variable in place? */
                if (SvREFCNT(sv) == 1 && !SvOBJECT(sv)) {

                    /* these flags are the union of all the relevant flags
                     * in the individual conditions within */
                    if (UNLIKELY(SvFLAGS(sv) & (
                            SVf_READONLY|SVf_PROTECT /*for SvREADONLY_off*/
                          | (SVs_GMG|SVs_SMG|SVs_RMG) /* SvMAGICAL() */
                          | SVf_OOK
                          | SVf_THINKFIRST)))
                    {
                        /* if a my variable that was made readonly is
                         * going out of scope, we want to remove the
                         * readonlyness so that it can go out of scope
                         * quietly
                         */
                        if (SvREADONLY(sv))
                            SvREADONLY_off(sv);

                        if (SvOOK(sv)) { /* OOK or HvAUX */
                            if (SvTYPE(sv) == SVt_PVHV)
                                Perl_hv_kill_backrefs(aTHX_ MUTABLE_HV(sv));
                            else
                                sv_backoff(sv);
                        }

                        if (SvMAGICAL(sv)) {
                            /* note that backrefs (either in HvAUX or magic)
                             * must be removed before other magic */
                            sv_unmagic(sv, PERL_MAGIC_backref);
                            if (SvTYPE(sv) != SVt_PVCV)
                                mg_free(sv);
                        }
                        if (SvTHINKFIRST(sv))
                            sv_force_normal_flags(sv, SV_IMMEDIATE_UNREF
                                                     |SV_COW_DROP_PV);

                    }
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
                    {
                        HEK *hek =
			      CvNAMED(sv)
				? CvNAME_HEK((CV *)sv)
				: GvNAME_HEK(CvGV(sv));
                        assert(hek);
                        (void)share_hek_hek(hek);
                        cv_undef((CV *)sv);
                        CvNAME_HEK_set(sv, hek);
                        CvLEXICAL_on(sv);
                        break;
                    }
                    default:
                        /* This looks odd, but these two macros are for use in
                           expressions and finish with a trailing comma, so
                           adding a ; after them would be wrong. */
                        assert_not_ROK(sv)
                        assert_not_glob(sv)
                        SvFLAGS(sv) &=~ (SVf_OK|SVf_IVisUV|SVf_UTF8);
                        break;
                    }
                    SvPADTMP_off(sv);
                    SvPADSTALE_on(sv); /* mark as no longer live */
                }
                else {	/* Someone has a claim on this, so abandon it. */
                    switch (SvTYPE(sv)) {	/* Console ourselves with a new value */
                    case SVt_PVAV:	*svp = MUTABLE_SV(newAV());	break;
                    case SVt_PVHV:	*svp = MUTABLE_SV(newHV());	break;
                    case SVt_PVCV:
                    {
                        HEK * const hek = CvNAMED(sv)
                                             ? CvNAME_HEK((CV *)sv)
                                             : GvNAME_HEK(CvGV(sv));

                        /* Create a stub */
                        *svp = newSV_type(SVt_PVCV);

                        /* Share name */
                        CvNAME_HEK_set(*svp,
                                       share_hek_hek(hek));
                        CvLEXICAL_on(*svp);
                        break;
                    }
                    default:	*svp = newSV(0);		break;
                    }
                    SvREFCNT_dec_NN(sv); /* Cast current value to the winds. */
                    /* preserve pad nature, but also mark as not live
                     * for any closure capturing */
                    SvFLAGS(*svp) |= SVs_PADSTALE;
                }
            }
	    break;
	case SAVEt_DELETE:
	    (void)hv_delete(ARG0_HV, ARG2_PV, ARG1_I32, G_DISCARD);
	    SvREFCNT_dec(ARG0_HV);
	    Safefree(arg2.any_ptr);
	    break;
	case SAVEt_ADELETE:
	    (void)av_delete(ARG0_AV, arg1.any_iv, G_DISCARD);
	    SvREFCNT_dec(ARG0_AV);
	    break;
	case SAVEt_DESTRUCTOR_X:
	    (*arg1.any_dxptr)(aTHX_ ARG0_PTR);
	    break;
	case SAVEt_REGCONTEXT:
	    /* regexp must have croaked */
	case SAVEt_ALLOC:
	    PL_savestack_ix -= uv >> SAVE_TIGHT_SHIFT;
	    break;
	case SAVEt_STACK_POS:		/* Position on Perl stack */
	    PL_stack_sp = PL_stack_base + arg0.any_i32;
	    break;
	case SAVEt_AELEM:		/* array element */
	    svp = av_fetch(ARG2_AV, arg1.any_iv, 1);
	    if (UNLIKELY(!AvREAL(ARG2_AV) && AvREIFY(ARG2_AV))) /* undo reify guard */
		SvREFCNT_dec(ARG0_SV);
	    if (LIKELY(svp)) {
		SV * const sv = *svp;
		if (LIKELY(sv && sv != &PL_sv_undef)) {
		    if (UNLIKELY(SvTIED_mg((const SV *)ARG2_AV, PERL_MAGIC_tied)))
			SvREFCNT_inc_void_NN(sv);
                    refsv = ARG2_SV;
		    goto restore_sv;
		}
	    }
	    SvREFCNT_dec(ARG2_AV);
	    SvREFCNT_dec(ARG0_SV);
	    break;
	case SAVEt_HELEM:		/* hash element */
        {
	    HE * const he = hv_fetch_ent(ARG2_HV, ARG1_SV, 1, 0);
	    SvREFCNT_dec(ARG1_SV);
	    if (LIKELY(he)) {
		const SV * const oval = HeVAL(he);
		if (LIKELY(oval && oval != &PL_sv_undef)) {
		    svp = &HeVAL(he);
		    if (UNLIKELY(SvTIED_mg((const SV *)ARG2_HV, PERL_MAGIC_tied)))
			SvREFCNT_inc_void(*svp);
		    refsv = ARG2_SV; /* what to refcnt_dec */
		    goto restore_sv;
		}
	    }
	    SvREFCNT_dec(ARG2_HV);
	    SvREFCNT_dec(ARG0_SV);
	    break;
        }
	case SAVEt_OP:
	    PL_op = (OP*)ARG0_PTR;
	    break;
	case SAVEt_HINTS:
	    if ((PL_hints & HINT_LOCALIZE_HH)) {
	      while (GvHV(PL_hintgv)) {
		HV *hv = GvHV(PL_hintgv);
		GvHV(PL_hintgv) = NULL;
		SvREFCNT_dec(MUTABLE_SV(hv));
	      }
	    }
	    cophh_free(CopHINTHASH_get(&PL_compiling));
	    CopHINTHASH_set(&PL_compiling, (COPHH*)ARG0_PTR);
	    *(I32*)&PL_hints = ARG1_I32;
	    if (PL_hints & HINT_LOCALIZE_HH) {
		SvREFCNT_dec(MUTABLE_SV(GvHV(PL_hintgv)));
		GvHV(PL_hintgv) = MUTABLE_HV(SSPOPPTR);
	    }
	    if (!GvHV(PL_hintgv)) {
		/* Need to add a new one manually, else rv2hv can
		   add one via GvHVn and it won't have the magic set.  */
		HV *const hv = newHV();
		hv_magic(hv, NULL, PERL_MAGIC_hints);
		GvHV(PL_hintgv) = hv;
	    }
	    assert(GvHV(PL_hintgv));
	    break;
	case SAVEt_COMPPAD:
	    PL_comppad = (PAD*)ARG0_PTR;
	    if (LIKELY(PL_comppad))
		PL_curpad = AvARRAY(PL_comppad);
	    else
		PL_curpad = NULL;
	    break;
	case SAVEt_PADSV_AND_MORTALIZE:
	    {
		SV **svp;
		assert (ARG1_PTR);
		svp = AvARRAY((PAD*)ARG1_PTR) + (PADOFFSET)arg0.any_uv;
                /* This mortalizing used to be done by CX_POOPLOOP() via
                   itersave.  But as we have all the information here, we
                   can do it here, save even having to have itersave in
                   the struct.
                   */
		sv_2mortal(*svp);
		*svp = ARG2_SV;
	    }
	    break;
	case SAVEt_SAVESWITCHSTACK:
	    {
		dSP;
		SWITCHSTACK(ARG0_AV, ARG1_AV);
		PL_curstackinfo->si_stack = ARG1_AV;
	    }
	    break;
	case SAVEt_SET_SVFLAGS:
            SvFLAGS(ARG2_SV) &= ~((U32)ARG1_I32);
            SvFLAGS(ARG2_SV) |= (U32)ARG0_I32;
	    break;

	    /* These are only saved in mathoms.c */
	case SAVEt_NSTAB:
	    (void)sv_clear(ARG0_SV);
	    break;
	case SAVEt_LONG:			/* long reference */
	    *(long*)ARG0_PTR = arg1.any_long;
	    break;
	case SAVEt_IV:				/* IV reference */
	    *(IV*)ARG0_PTR = arg1.any_iv;
	    break;

	case SAVEt_I16:				/* I16 reference */
	    *(I16*)ARG0_PTR = (I16)(uv >> 8);
	    break;
	case SAVEt_I8:				/* I8 reference */
	    *(I8*)ARG0_PTR = (I8)(uv >> 8);
	    break;
	case SAVEt_DESTRUCTOR:
	    (*arg1.any_dptr)(ARG0_PTR);
	    break;
	case SAVEt_COMPILE_WARNINGS:
	    if (!specialWARN(PL_compiling.cop_warnings))
		PerlMemShared_free(PL_compiling.cop_warnings);

	    PL_compiling.cop_warnings = (STRLEN*)ARG0_PTR;
	    break;
	case SAVEt_PARSER:
	    parser_free((yy_parser *) ARG0_PTR);
	    break;
	case SAVEt_READONLY_OFF:
	    SvREADONLY_off(ARG0_SV);
	    break;
	default:
	    Perl_croak(aTHX_ "panic: leave_scope inconsistency %u", type);
	}
    }

    TAINT_set(was);
}

void
Perl_cx_dump(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_DUMP;

#ifdef DEBUGGING
    PerlIO_printf(Perl_debug_log, "CX %ld = %s\n", (long)(cx - cxstack), PL_block_type[CxTYPE(cx)]);
    if (CxTYPE(cx) != CXt_SUBST) {
	const char *gimme_text;
	PerlIO_printf(Perl_debug_log, "BLK_OLDSP = %ld\n", (long)cx->blk_oldsp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDCOP = 0x%"UVxf"\n",
		      PTR2UV(cx->blk_oldcop));
	PerlIO_printf(Perl_debug_log, "BLK_OLDMARKSP = %ld\n", (long)cx->blk_oldmarksp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDSCOPESP = %ld\n", (long)cx->blk_oldscopesp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDSAVEIX = %ld\n", (long)cx->blk_oldsaveix);
	PerlIO_printf(Perl_debug_log, "BLK_OLDPM = 0x%"UVxf"\n",
		      PTR2UV(cx->blk_oldpm));
	switch (cx->blk_gimme) {
	    case G_VOID:
		gimme_text = "VOID";
		break;
	    case G_SCALAR:
		gimme_text = "SCALAR";
		break;
	    case G_ARRAY:
		gimme_text = "LIST";
		break;
	    default:
		gimme_text = "UNKNOWN";
		break;
	}
	PerlIO_printf(Perl_debug_log, "BLK_GIMME = %s\n", gimme_text);
    }
    switch (CxTYPE(cx)) {
    case CXt_NULL:
    case CXt_BLOCK:
	break;
    case CXt_FORMAT:
	PerlIO_printf(Perl_debug_log, "BLK_FORMAT.CV = 0x%"UVxf"\n",
		PTR2UV(cx->blk_format.cv));
	PerlIO_printf(Perl_debug_log, "BLK_FORMAT.GV = 0x%"UVxf"\n",
		PTR2UV(cx->blk_format.gv));
	PerlIO_printf(Perl_debug_log, "BLK_FORMAT.DFOUTGV = 0x%"UVxf"\n",
		PTR2UV(cx->blk_format.dfoutgv));
	PerlIO_printf(Perl_debug_log, "BLK_FORMAT.HASARGS = %d\n",
		      (int)CxHASARGS(cx));
	PerlIO_printf(Perl_debug_log, "BLK_FORMAT.RETOP = 0x%"UVxf"\n",
		PTR2UV(cx->blk_format.retop));
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

    case CXt_LOOP_PLAIN:
    case CXt_LOOP_LAZYIV:
    case CXt_LOOP_LAZYSV:
    case CXt_LOOP_LIST:
    case CXt_LOOP_ARY:
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.LABEL = %s\n", CxLABEL(cx));
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.MY_OP = 0x%"UVxf"\n",
		PTR2UV(cx->blk_loop.my_op));
        if (CxTYPE(cx) != CXt_LOOP_PLAIN) {
            PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERVAR = 0x%"UVxf"\n",
                    PTR2UV(CxITERVAR(cx)));
            PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERSAVE = 0x%"UVxf"\n",
                    PTR2UV(cx->blk_loop.itersave));
            /* XXX: not accurate for LAZYSV/IV/LIST */
            PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERARY = 0x%"UVxf"\n",
                    PTR2UV(cx->blk_loop.state_u.ary.ary));
            PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERIX = %ld\n",
                    (long)cx->blk_loop.state_u.ary.ix);
        }
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

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
