/*>    inline.h
 *
 *    Copyright (C) 2012 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 *    This file contains tables and code adapted from
 *    https://bjoern.hoehrmann.de/utf-8/decoder/dfa/, which requires this
 *    copyright notice:

Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 *
 * This file is a home for static inline functions that cannot go in other
 * header files, because they depend on proto.h (included after most other
 * headers) or struct definitions.
 *
 * Note also perlstatic.h for functions that can't or shouldn't be inlined, but
 * whose details should be exposed to the compiler, for such things as tail
 * call optimization.
 *
 * Each section names the header file that the functions "belong" to.
 */

/* ------------------------------- av.h ------------------------------- */

/*
=for apidoc_section $AV
=for apidoc av_count
Returns the number of elements in the array C<av>.  This is the true length of
the array, including any undefined elements.  It is always the same as
S<C<av_top_index(av) + 1>>.

=cut
*/
PERL_STATIC_INLINE Size_t
Perl_av_count(pTHX_ AV *av)
{
    PERL_ARGS_ASSERT_AV_COUNT;
    assert(SvTYPE(av) == SVt_PVAV);

    return AvFILL(av) + 1;
}

/* ------------------------------- av.c ------------------------------- */

/*
=for apidoc av_store_simple

This is a cut-down version of av_store that assumes that the array is
very straightforward - no magic, not readonly, and AvREAL - and that
C<key> is not negative. This function MUST NOT be used in situations
where any of those assumptions may not hold.

Stores an SV in an array.  The array index is specified as C<key>. It
can be dereferenced to get the C<SV*> that was stored there (= C<val>)).

Note that the caller is responsible for suitably incrementing the reference
count of C<val> before the call.

Approximate Perl equivalent: C<splice(@myarray, $key, 1, $val)>.

=cut
*/

PERL_STATIC_INLINE SV**
Perl_av_store_simple(pTHX_ AV *av, SSize_t key, SV *val)
{
    SV** ary;

    PERL_ARGS_ASSERT_AV_STORE_SIMPLE;
    assert(SvTYPE(av) == SVt_PVAV);
    assert(!SvMAGICAL(av));
    assert(!SvREADONLY(av));
    assert(AvREAL(av));
    assert(key > -1);

    ary = AvARRAY(av);

    if (AvFILLp(av) < key) {
        if (key > AvMAX(av)) {
            av_extend(av,key);
            ary = AvARRAY(av);
        }
        AvFILLp(av) = key;
    } else
        SvREFCNT_dec(ary[key]);

    ary[key] = val;
    return &ary[key];
}

/*
=for apidoc av_fetch_simple

This is a cut-down version of av_fetch that assumes that the array is
very straightforward - no magic, not readonly, and AvREAL - and that
C<key> is not negative. This function MUST NOT be used in situations
where any of those assumptions may not hold.

Returns the SV at the specified index in the array.  The C<key> is the
index.  If lval is true, you are guaranteed to get a real SV back (in case
it wasn't real before), which you can then modify.  Check that the return
value is non-null before dereferencing it to a C<SV*>.

The rough perl equivalent is C<$myarray[$key]>.

=cut
*/

PERL_STATIC_INLINE SV**
Perl_av_fetch_simple(pTHX_ AV *av, SSize_t key, I32 lval)
{
    PERL_ARGS_ASSERT_AV_FETCH_SIMPLE;
    assert(SvTYPE(av) == SVt_PVAV);
    assert(!SvMAGICAL(av));
    assert(!SvREADONLY(av));
    assert(AvREAL(av));
    assert(key > -1);

    if ( (key > AvFILLp(av)) || !AvARRAY(av)[key]) {
        return lval ? av_store_simple(av,key,newSV_type(SVt_NULL)) : NULL;
    } else {
        return &AvARRAY(av)[key];
    }
}

PERL_STATIC_INLINE void
Perl_av_push_simple(pTHX_ AV *av, SV *val)
{
    PERL_ARGS_ASSERT_AV_PUSH_SIMPLE;
    assert(SvTYPE(av) == SVt_PVAV);
    assert(!SvMAGICAL(av));
    assert(!SvREADONLY(av));
    assert(AvREAL(av));
    assert(AvFILLp(av) > -2);

    (void)av_store_simple(av,AvFILLp(av)+1,val);
}

/*
=for apidoc av_new_alloc

This implements L<perlapi/C<newAV_alloc_x>>
and L<perlapi/C<newAV_alloc_xz>>, which are the public API for this
functionality.

Creates a new AV and allocates its SV* array.

This is similar to, but more efficient than doing:

    AV *av = newAV();
    av_extend(av, key);

The size parameter is used to pre-allocate a SV* array large enough to
hold at least elements C<0..(size-1)>.  C<size> must be at least 1.

The C<zeroflag> parameter controls whether or not the array is NULL
initialized.

=cut
*/

PERL_STATIC_INLINE AV *
Perl_av_new_alloc(pTHX_ SSize_t size, bool zeroflag)
{
    AV * const av = newAV();
    SV** ary;
    PERL_ARGS_ASSERT_AV_NEW_ALLOC;
    assert(size > 0);

    Newx(ary, size, SV*); /* Newx performs the memwrap check */
    AvALLOC(av) = ary;
    AvARRAY(av) = ary;
    AvMAX(av) = size - 1;

    if (zeroflag)
        Zero(ary, size, SV*);

    return av;
}


/* remove (AvARRAY(av) - AvALLOC(av)) offset from empty array */

PERL_STATIC_INLINE void
Perl_av_remove_offset(pTHX_ AV *av)
{
    PERL_ARGS_ASSERT_AV_REMOVE_OFFSET;
    assert(AvFILLp(av) == -1);
    SSize_t i = AvARRAY(av) - AvALLOC(av);
    if (i) {
        AvARRAY(av) = AvALLOC(av);
        AvMAX(av)   += i;
#ifdef PERL_RC_STACK
        Zero(AvALLOC(av), i, SV*);
#endif
    }
}


/* ------------------------------- cv.h ------------------------------- */

/*
=for apidoc_section $CV
=for apidoc CvGV
Returns the GV associated with the CV C<sv>, reifying it if necessary.

=cut
*/
PERL_STATIC_INLINE GV *
Perl_CvGV(pTHX_ CV *sv)
{
    PERL_ARGS_ASSERT_CVGV;

    return CvNAMED(sv)
        ? Perl_cvgv_from_hek(aTHX_ sv)
        : ((XPVCV*)MUTABLE_PTR(SvANY(sv)))->xcv_gv_u.xcv_gv;
}

/*
=for apidoc CvDEPTH
Returns the recursion level of the CV C<sv>.  Hence >= 2 indicates we are in a
recursive call.

=cut
*/
PERL_STATIC_INLINE I32 *
Perl_CvDEPTH(const CV * const sv)
{
    PERL_ARGS_ASSERT_CVDEPTH;
    assert(SvTYPE(sv) == SVt_PVCV || SvTYPE(sv) == SVt_PVFM);

    return &((XPVCV*)SvANY(sv))->xcv_depth;
}

/*
 CvPROTO returns the prototype as stored, which is not necessarily what
 the interpreter should be using. Specifically, the interpreter assumes
 that spaces have been stripped, which has been the case if the prototype
 was added by toke.c, but is generally not the case if it was added elsewhere.
 Since we can't enforce the spacelessness at assignment time, this routine
 provides a temporary copy at parse time with spaces removed.
 I<orig> is the start of the original buffer, I<len> is the length of the
 prototype and will be updated when this returns.
 */

#ifdef PERL_CORE
PERL_STATIC_INLINE char *
S_strip_spaces(pTHX_ const char * orig, STRLEN * const len)
{
    SV * tmpsv;
    char * tmps;
    tmpsv = newSVpvn_flags(orig, *len, SVs_TEMP);
    tmps = SvPVX(tmpsv);
    while ((*len)--) {
        if (!isSPACE(*orig))
            *tmps++ = *orig;
        orig++;
    }
    *tmps = '\0';
    *len = tmps - SvPVX(tmpsv);
                return SvPVX(tmpsv);
}
#endif

/* ------------------------------- iperlsys.h ------------------------------- */
#if ! defined(PERL_IMPLICIT_SYS) && defined(USE_ITHREADS)

/* Otherwise this function is implemented as macros in iperlsys.h */

PERL_STATIC_INLINE bool
S_PerlEnv_putenv(pTHX_ char * str)
{
    PERL_ARGS_ASSERT_PERLENV_PUTENV;

    ENV_LOCK;
    bool retval = putenv(str);
    ENV_UNLOCK;

    return retval;
}

#endif

/* ------------------------------- mg.h ------------------------------- */

#if defined(PERL_CORE) || defined(PERL_EXT)
/* assumes get-magic and stringification have already occurred */
PERL_STATIC_INLINE STRLEN
S_MgBYTEPOS(pTHX_ MAGIC *mg, SV *sv, const char *s, STRLEN len)
{
    assert(mg->mg_type == PERL_MAGIC_regex_global);
    assert(mg->mg_len != -1);
    if (mg->mg_flags & MGf_BYTES || !DO_UTF8(sv))
        return (STRLEN)mg->mg_len;
    else {
        const STRLEN pos = (STRLEN)mg->mg_len;
        /* Without this check, we may read past the end of the buffer: */
        if (pos > sv_or_pv_len_utf8(sv, s, len)) return len+1;
        return sv_or_pv_pos_u2b(sv, s, pos, NULL);
    }
}
#endif

/* ------------------------------- pad.h ------------------------------ */

#if defined(PERL_IN_PAD_C) || defined(PERL_IN_OP_C)
PERL_STATIC_INLINE bool
S_PadnameIN_SCOPE(const PADNAME * const pn, const U32 seq)
{
    PERL_ARGS_ASSERT_PADNAMEIN_SCOPE;

    /* is seq within the range _LOW to _HIGH ?
     * This is complicated by the fact that PL_cop_seqmax
     * may have wrapped around at some point */
    if (COP_SEQ_RANGE_LOW(pn) == PERL_PADSEQ_INTRO)
        return FALSE; /* not yet introduced */

    if (COP_SEQ_RANGE_HIGH(pn) == PERL_PADSEQ_INTRO) {
    /* in compiling scope */
        if (
            (seq >  COP_SEQ_RANGE_LOW(pn))
            ? (seq - COP_SEQ_RANGE_LOW(pn) < (U32_MAX >> 1))
            : (COP_SEQ_RANGE_LOW(pn) - seq > (U32_MAX >> 1))
        )
            return TRUE;
    }
    else if (
        (COP_SEQ_RANGE_LOW(pn) > COP_SEQ_RANGE_HIGH(pn))
        ?
            (  seq >  COP_SEQ_RANGE_LOW(pn)
            || seq <= COP_SEQ_RANGE_HIGH(pn))

        :    (  seq >  COP_SEQ_RANGE_LOW(pn)
             && seq <= COP_SEQ_RANGE_HIGH(pn))
    )
        return TRUE;
    return FALSE;
}
#endif

/* ------------------------------- pp.h ------------------------------- */

PERL_STATIC_INLINE Stack_off_t
Perl_TOPMARK(pTHX)
{
    DEBUG_s(DEBUG_v(PerlIO_printf(Perl_debug_log,
                                 "MARK top  %p %" IVdf "\n",
                                  PL_markstack_ptr,
                                  (IV)*PL_markstack_ptr)));
    return *PL_markstack_ptr;
}

PERL_STATIC_INLINE Stack_off_t
Perl_POPMARK(pTHX)
{
    DEBUG_s(DEBUG_v(PerlIO_printf(Perl_debug_log,
                                 "MARK pop  %p %" IVdf "\n",
                                  (PL_markstack_ptr-1),
                                  (IV)*(PL_markstack_ptr-1))));
    assert((PL_markstack_ptr > PL_markstack) || !"MARK underflow");
    return *PL_markstack_ptr--;
}

/*
=for apidoc_section $rpp

=for apidoc rpp_extend
Ensures that there is space on the stack to push C<n> items, extending it
if necessary.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_extend(pTHX_ SSize_t n)
{
    PERL_ARGS_ASSERT_RPP_EXTEND;

    EXTEND_HWM_SET(PL_stack_sp, n);
#ifndef STRESS_REALLOC
    if (UNLIKELY(_EXTEND_NEEDS_GROW(PL_stack_sp, n)))
#endif
    {
        (void)stack_grow(PL_stack_sp, PL_stack_sp, n);
    }
}


/*
=for apidoc rpp_popfree_to

Pop and free all items on the argument stack above C<sp>. On return,
C<PL_stack_sp> will be equal to C<sp>.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_popfree_to(pTHX_ SV **sp)
{
    PERL_ARGS_ASSERT_RPP_POPFREE_TO;

    assert(sp <= PL_stack_sp);
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    while (PL_stack_sp > sp) {
        SV *sv = *PL_stack_sp--;
        SvREFCNT_dec(sv);
    }
#else
    PL_stack_sp = sp;
#endif
}


/*
=for apidoc rpp_popfree_to_NN

A variant of rpp_popfree_to() which assumes that all the pointers being
popped off the stack are non-NULL.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_popfree_to_NN(pTHX_ SV **sp)
{
    PERL_ARGS_ASSERT_RPP_POPFREE_TO_NN;

    assert(sp <= PL_stack_sp);
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    while (PL_stack_sp > sp) {
        SV *sv = *PL_stack_sp--;
        assert(sv);
        SvREFCNT_dec_NN(sv);
    }
#else
    PL_stack_sp = sp;
#endif
}


/*
=for apidoc rpp_popfree_1

Pop and free the top item on the argument stack and update C<PL_stack_sp>.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_popfree_1(pTHX)
{
    PERL_ARGS_ASSERT_RPP_POPFREE_1;

#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SV *sv = *PL_stack_sp--;
    SvREFCNT_dec(sv);
#else
    PL_stack_sp--;
#endif
}


/*
=for apidoc rpp_popfree_1_NN

A variant of rpp_popfree_1() which assumes that the pointer being popped
off the stack is non-NULL.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_popfree_1_NN(pTHX)
{
    PERL_ARGS_ASSERT_RPP_POPFREE_1_NN;

    assert(*PL_stack_sp);
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SV *sv = *PL_stack_sp--;
    SvREFCNT_dec_NN(sv);
#else
    PL_stack_sp--;
#endif
}


/*
=for apidoc rpp_popfree_2

Pop and free the top two items on the argument stack and update
C<PL_stack_sp>.

=cut
*/


PERL_STATIC_INLINE void
Perl_rpp_popfree_2(pTHX)
{
    PERL_ARGS_ASSERT_RPP_POPFREE_2;

#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    for (int i = 0; i < 2; i++) {
        SV *sv = *PL_stack_sp--;
        SvREFCNT_dec(sv);
    }
#else
    PL_stack_sp -= 2;
#endif
}


/*
=for apidoc rpp_popfree_2_NN

A variant of rpp_popfree_2() which assumes that the two pointers being
popped off the stack are non-NULL.

=cut
*/


PERL_STATIC_INLINE void
Perl_rpp_popfree_2_NN(pTHX)
{
    PERL_ARGS_ASSERT_RPP_POPFREE_2_NN;
#ifdef PERL_RC_STACK
    SV *sv2 = *PL_stack_sp--;
    assert(sv2);
    SV *sv1 = *PL_stack_sp;
    assert(sv1);

    assert(rpp_stack_is_rc());
    U32 rc1 = SvREFCNT(sv1);
    U32 rc2 = SvREFCNT(sv2);
    /* This expression is intended to be true if either of rc1 or rc2 has
     * the value 0 or 1, but using only a single branch test, rather
     * than the two branches that a compiler would plant for a boolean
     * expression. We are working on the assumption that, most of the
     * time, neither of the args to a binary function will need to be
     * freed - they're likely to lex vars, or PADTMPs or whatever.
     * So give the CPU a single branch that is rarely taken. */
    if (UNLIKELY( !(rc1>>1) + !(rc2>>1) ))
        /* at least one of the old SVs needs freeing. Do it the long way */
        Perl_rpp_free_2_(aTHX_ sv1, sv2, rc1, rc2);
    else {
        SvREFCNT(sv1) = rc1 - 1;
        SvREFCNT(sv2) = rc2 - 1;
    }
    PL_stack_sp--;
#else
    PL_stack_sp -= 2;
#endif
}


/*
=for apidoc rpp_pop_1_norc

Pop and return the top item off the argument stack and update
C<PL_stack_sp>. It's similar to rpp_popfree_1(), except that it actually
returns a value, and it I<doesn't> decrement the SV's reference count.
On non-C<PERL_RC_STACK> builds it actually increments the SV's reference
count.

This is useful in cases where the popped value is immediately embedded
somewhere e.g. via av_store(), allowing you skip decrementing and then
immediately incrementing the reference count again (and risk prematurely
freeing the SV if it had a RC of 1). On non-RC builds, the reference count
bookkeeping still works too, which is why it should be used rather than
a simple C<*PL_stack_sp-->.

=cut
*/

PERL_STATIC_INLINE SV*
Perl_rpp_pop_1_norc(pTHX)
{
    PERL_ARGS_ASSERT_RPP_POP_1_NORC

    SV *sv = *PL_stack_sp--;

#ifndef PERL_RC_STACK
    SvREFCNT_inc(sv);
#else
    assert(rpp_stack_is_rc());
#endif
    return sv;
}



/*
=for apidoc      rpp_push_1
=for apidoc_item rpp_push_IMM
=for apidoc_item rpp_push_2
=for apidoc_item rpp_xpush_1
=for apidoc_item rpp_xpush_IMM
=for apidoc_item rpp_xpush_2

Push one or two SVs onto the stack, incrementing their reference counts
and updating C<PL_stack_sp>. With the C<x> variants, it extends the stack
first. The C<IMM> variants assume that the single argument is an immortal
such as <&PL_sv_undef> and, for efficiency, will skip incrementing its
reference count.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_push_1(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_PUSH_1;

    *++PL_stack_sp = sv;
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SvREFCNT_inc_simple_void_NN(sv);
#endif
}

PERL_STATIC_INLINE void
Perl_rpp_push_IMM(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_PUSH_IMM;

    assert(SvIMMORTAL(sv));
    *++PL_stack_sp = sv;
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
#endif
}

PERL_STATIC_INLINE void
Perl_rpp_push_2(pTHX_ SV *sv1, SV *sv2)
{
    PERL_ARGS_ASSERT_RPP_PUSH_2;

    *++PL_stack_sp = sv1;
    *++PL_stack_sp = sv2;
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SvREFCNT_inc_simple_void_NN(sv1);
    SvREFCNT_inc_simple_void_NN(sv2);
#endif
}

PERL_STATIC_INLINE void
Perl_rpp_xpush_1(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_XPUSH_1;

    rpp_extend(1);
    rpp_push_1(sv);
}

PERL_STATIC_INLINE void
Perl_rpp_xpush_IMM(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_XPUSH_IMM;

    rpp_extend(1);
    rpp_push_IMM(sv);
}

PERL_STATIC_INLINE void
Perl_rpp_xpush_2(pTHX_ SV *sv1, SV *sv2)
{
    PERL_ARGS_ASSERT_RPP_XPUSH_2;

    rpp_extend(2);
    rpp_push_2(sv1, sv2);
}


/*
=for apidoc rpp_push_1_norc

Push C<sv> onto the stack without incrementing its reference count, and
update C<PL_stack_sp>. On non-PERL_RC_STACK builds, mortalise too.

This is most useful where an SV has just been created and already has a
reference count of 1, but has not yet been anchored anywhere.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_push_1_norc(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_PUSH_1;

    *++PL_stack_sp = sv;
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
#else
    sv_2mortal(sv);
#endif
}


/*
=for apidoc      rpp_replace_1_1
=for apidoc_item rpp_replace_1_1_NN
=for apidoc_item rpp_replace_1_IMM_NN

Replace the current top stack item with C<sv>, while suitably adjusting
reference counts. Equivalent to rpp_popfree_1(); rpp_push_1(sv), but
is more efficient and handles both SVs being the same.

The C<_NN> variant assumes that the pointer on the stack to the SV being
freed is non-NULL.

The C<IMM_NN> variant is like the C<_NN> variant, but in addition, assumes
that the single argument is an immortal such as <&PL_sv_undef> and, for
efficiency, will skip incrementing its reference count.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_replace_1_1(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_1_1;

    assert(sv);
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SV *oldsv = *PL_stack_sp;
    *PL_stack_sp = sv;
    SvREFCNT_inc_simple_void_NN(sv);
    SvREFCNT_dec(oldsv);
#else
    *PL_stack_sp = sv;
#endif
}


PERL_STATIC_INLINE void
Perl_rpp_replace_1_1_NN(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_1_1_NN;

    assert(sv);
    assert(*PL_stack_sp);
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SV *oldsv = *PL_stack_sp;
    *PL_stack_sp = sv;
    SvREFCNT_inc_simple_void_NN(sv);
    SvREFCNT_dec_NN(oldsv);
#else
    *PL_stack_sp = sv;
#endif
}


PERL_STATIC_INLINE void
Perl_rpp_replace_1_IMM_NN(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_1_IMM_NN;

    assert(sv);
    assert(SvIMMORTAL(sv));
    assert(*PL_stack_sp);
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SV *oldsv = *PL_stack_sp;
    *PL_stack_sp = sv;
    SvREFCNT_dec_NN(oldsv);
#else
    *PL_stack_sp = sv;
#endif
}


/*
=for apidoc      rpp_replace_2_1
=for apidoc_item rpp_replace_2_1_NN
=for apidoc_item rpp_replace_2_IMM_NN

Replace the current top to stacks item with C<sv>, while suitably
adjusting reference counts. Equivalent to rpp_popfree_2(); rpp_push_1(sv),
but is more efficient and handles SVs being the same.

The C<_NN> variant assumes that the pointers on the stack to the SVs being
freed are non-NULL.

The C<IMM_NN> variant is like the C<_NN> variant, but in addition, assumes
that the single argument is an immortal such as <&PL_sv_undef> and, for
efficiency, will skip incrementing its reference count.
=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_replace_2_1(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_2_1;

#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    /* replace PL_stack_sp[-1] first; leave PL_stack_sp[0] in place while
     * we free [-1], so if an exception occurs, [0] will still be freed.
     */
    SV *oldsv = PL_stack_sp[-1];
    PL_stack_sp[-1] = sv;
    SvREFCNT_inc_simple_void_NN(sv);
    SvREFCNT_dec(oldsv);
    oldsv = *PL_stack_sp--;
    SvREFCNT_dec(oldsv);
#else
    *--PL_stack_sp = sv;
#endif
}


/* Private helper function for _NN and _IMM_NN variants.
 * Assumes sv has already had its ref count incremented,
 * ready for being put on the stack.
 * Intended to be small and fast, since it's inlined into many hot parts of
 * code.
 */

PERL_STATIC_INLINE void
Perl_rpp_replace_2_1_COMMON(pTHX_ SV *sv)
{

    assert(sv);
#ifdef PERL_RC_STACK
    SV *sv2 = *PL_stack_sp--;
    assert(sv2);
    SV *sv1 = *PL_stack_sp;
    assert(sv1);

    *PL_stack_sp = sv;
    assert(rpp_stack_is_rc());
    U32 rc1 = SvREFCNT(sv1);
    U32 rc2 = SvREFCNT(sv2);
    /* This expression is intended to be true if either of rc1 or rc2 has
     * the value 0 or 1, but using only a single branch test, rather
     * than the two branches that a compiler would plant for a boolean
     * expression. We are working on the assumption that, most of the
     * time, neither of the args to a binary function will need to be
     * freed - they're likely to lex vars, or PADTMPs or whatever.
     * So give the CPU a single branch that is rarely taken. */
    if (UNLIKELY( !(rc1>>1) + !(rc2>>1) ))
        /* at least one of the old SVs needs freeing. Do it the long way */
        Perl_rpp_free_2_(aTHX_ sv1, sv2, rc1, rc2);
    else {
        SvREFCNT(sv1) = rc1 - 1;
        SvREFCNT(sv2) = rc2 - 1;
    }
#else
    *--PL_stack_sp = sv;
#endif
}


PERL_STATIC_INLINE void
Perl_rpp_replace_2_1_NN(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_2_1_NN;

    assert(sv);
#ifdef PERL_RC_STACK
    SvREFCNT_inc_simple_void_NN(sv);
#endif
    rpp_replace_2_1_COMMON(sv);
}


PERL_STATIC_INLINE void
Perl_rpp_replace_2_IMM_NN(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_2_IMM_NN;

    assert(sv);
    assert(SvIMMORTAL(sv));
    rpp_replace_2_1_COMMON(sv);
}


/*
=for apidoc rpp_replace_at

Replace the SV at address sp within the stack with C<sv>, while suitably
adjusting reference counts. Equivalent to C<*sp = sv>, except with proper
reference count handling.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_replace_at(pTHX_ SV **sp, SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_AT;

#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SV *oldsv = *sp;
    *sp = sv;
    SvREFCNT_inc_simple_void_NN(sv);
    SvREFCNT_dec(oldsv);
#else
    *sp = sv;
#endif
}


/*
=for apidoc rpp_replace_at_NN

A variant of rpp_replace_at() which assumes that the SV pointer on the
stack is non-NULL.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_replace_at_NN(pTHX_ SV **sp, SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_AT_NN;

    assert(sv);
    assert(*sp);
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SV *oldsv = *sp;
    *sp = sv;
    SvREFCNT_inc_simple_void_NN(sv);
    SvREFCNT_dec_NN(oldsv);
#else
    *sp = sv;
#endif
}


/*
=for apidoc rpp_replace_at_norc

Replace the SV at address sp within the stack with C<sv>, while suitably
adjusting the reference count of the old SV. Equivalent to C<*sp = sv>,
except with proper reference count handling.

C<sv>'s reference count doesn't get incremented. On non-C<PERL_RC_STACK>
builds, it gets mortalised too.

This is most useful where an SV has just been created and already has a
reference count of 1, but has not yet been anchored anywhere.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_replace_at_norc(pTHX_ SV **sp, SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_AT_NORC;

#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SV *oldsv = *sp;
    *sp = sv;
    SvREFCNT_dec(oldsv);
#else
    *sp = sv;
    sv_2mortal(sv);
#endif
}


/*
=for apidoc rpp_replace_at_norc_NN

A variant of rpp_replace_at_norc() which assumes that the SV pointer on the
stack is non-NULL.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_replace_at_norc_NN(pTHX_ SV **sp, SV *sv)
{
    PERL_ARGS_ASSERT_RPP_REPLACE_AT_NORC_NN;

    assert(*sp);
#ifdef PERL_RC_STACK
    assert(rpp_stack_is_rc());
    SV *oldsv = *sp;
    *sp = sv;
    SvREFCNT_dec_NN(oldsv);
#else
    *sp = sv;
    sv_2mortal(sv);
#endif
}


/*
=for apidoc rpp_context

Impose void, scalar or list context on the stack.
First, pop C<extra> items off the stack, then when C<gimme> is:
C<G_LIST>:   return as-is.
C<G_VOID>:   pop everything back to C<mark>
C<G_SCALAR>: move the top stack item (or C<&PL_sv_undef> if none) to
C<mark+1> and free everything above it.

=cut
*/

PERL_STATIC_INLINE void
Perl_rpp_context(pTHX_ SV **mark, U8 gimme, SSize_t extra)
{
    PERL_ARGS_ASSERT_RPP_CONTEXT;
    assert(extra >= 0);
    assert(mark <= PL_stack_sp - extra);

    if (gimme == G_LIST)
        mark = PL_stack_sp - extra;
    else if (gimme == G_SCALAR) {
        SV **svp = PL_stack_sp - extra;
        mark++;
        if (mark > svp) {
            /* empty list (plus extra) */
            rpp_popfree_to(svp);
            rpp_extend(1);
            *++PL_stack_sp = &PL_sv_undef;
            return;
        }
        /* swap top and bottom list items */
        SV *top = *svp;
        *svp = *mark;
        *mark = top;
     }
    rpp_popfree_to(mark);
}




/*
=for apidoc      rpp_try_AMAGIC_1
=for apidoc_item rpp_try_AMAGIC_2

Check whether either of the one or two SVs at the top of the stack is
magical or a ref, and in either case handle it specially: invoke get
magic, call an overload method, or replace a ref with a temporary numeric
value, as appropriate. If this function returns true, it indicates that
the correct return value is already on the stack. Intended to be used at
the beginning of the PP function for unary or binary ops.

=cut
*/

PERL_STATIC_INLINE bool
Perl_rpp_try_AMAGIC_1(pTHX_ int method, int flags)
{
    return    UNLIKELY((SvFLAGS(*PL_stack_sp) & (SVf_ROK|SVs_GMG)))
           && Perl_try_amagic_un(aTHX_ method, flags);
}

PERL_STATIC_INLINE bool
Perl_rpp_try_AMAGIC_2(pTHX_ int method, int flags)
{
    return    UNLIKELY(((SvFLAGS(PL_stack_sp[-1])|SvFLAGS(PL_stack_sp[0]))
                     & (SVf_ROK|SVs_GMG)))
           && Perl_try_amagic_bin(aTHX_ method, flags);
}


/*
=for apidoc rpp_stack_is_rc

Returns a boolean value indicating whether the stack is currently
reference-counted. Note that if the stack is split (bottom half RC, top
half non-RC), this function returns false, even if the top half currently
contains zero items.

=cut
*/

PERL_STATIC_INLINE bool
Perl_rpp_stack_is_rc(pTHX)
{
#ifdef PERL_RC_STACK
    return AvREAL(PL_curstack) && !PL_curstackinfo->si_stack_nonrc_base;
#else
    return 0;
#endif

}


/*
=for apidoc rpp_is_lone

Indicates whether the stacked SV C<sv> (assumed to be not yet popped off
the stack) is only kept alive due to a single reference from the argument
stack and/or and the temps stack.

This can used for example to decide whether the copying of return values
in rvalue context can be skipped, or whether it shouldn't be assigned to
in lvalue context.

=cut
*/

PERL_STATIC_INLINE bool
Perl_rpp_is_lone(pTHX_ SV *sv)
{
#ifdef PERL_RC_STACK
    /* note that rpp_is_lone() can be used in wrapped pp functions,
     * where technically the stack is no longer ref-counted; but because
     * the args are non-RC copies of RC args further down the stack, we
     * can't be in a *completely* non-ref stack.
     */
    assert(AvREAL(PL_curstack));
#endif

    return SvREFCNT(sv) <= (U32)cBOOL(SvTEMP(sv))
#ifdef PERL_RC_STACK
                         + 1u
            && !SvIMMORTAL(sv) /* PL_sv_undef etc are never stealable */
#endif
    ;
}


/*
=for apidoc rpp_invoke_xs

Call the XS function associated with C<cv>. Wraps the call if necessary to
handle XS functions which are not aware of reference-counted stacks.

=cut
*/


PERL_STATIC_INLINE void
Perl_rpp_invoke_xs(pTHX_ CV *cv)
{
    PERL_ARGS_ASSERT_RPP_INVOKE_XS;

#ifdef PERL_RC_STACK
    if (!CvXS_RCSTACK(cv))
        Perl_xs_wrap(aTHX_ CvXSUB(cv), cv);
    else
#endif
        CvXSUB(cv)(aTHX_ cv);
}


/* for SvCANEXISTDELETE() macro in pp.h */
PERL_STATIC_INLINE bool
Perl_sv_can_existdelete(pTHX_ SV *sv)
{
    /* Anything without tie magic is fine */
    MAGIC *mg;
    if(!SvRMAGICAL(sv) || !(mg = mg_find(sv, PERL_MAGIC_tied)))
        return true;

    HV *stash = SvSTASH(SvRV(SvTIED_obj(sv, mg)));
    return stash &&
        gv_fetchmethod_autoload(stash, "EXISTS", TRUE) &&
        gv_fetchmethod_autoload(stash, "DELETE", TRUE);
}


/* ----------------------------- regexp.h ----------------------------- */

/* PVLVs need to act as a superset of all scalar types - they are basically
 * PVMGs with a few extra fields.
 * REGEXPs are first class scalars, but have many fields that can't be copied
 * into a PVLV body.
 *
 * Hence we take a different approach - instead of a copy, PVLVs store a pointer
 * back to the original body. To avoid increasing the size of PVLVs just for the
 * rare case of REGEXP assignment, this pointer is stored in the memory usually
 * used for SvLEN(). Hence the check for SVt_PVLV below, and the ? : ternary to
 * read the pointer from the two possible locations. The macro SvLEN() wraps the
 * access to the union's member xpvlenu_len, but there is no equivalent macro
 * for wrapping the union's member xpvlenu_rx, hence the direct reference here.
 *
 * See commit df6b4bd56551f2d3 for more details. */

PERL_STATIC_INLINE struct regexp *
Perl_ReANY(const REGEXP * const re)
{
    XPV* const p = (XPV*)SvANY(re);

    PERL_ARGS_ASSERT_REANY;
    assert(isREGEXP(re));

    return SvTYPE(re) == SVt_PVLV ? p->xpv_len_u.xpvlenu_rx
                                   : (struct regexp *)p;
}

/* ------------------------------- utf8.h ------------------------------- */

/*
=for apidoc_section $unicode
*/

PERL_STATIC_INLINE void
Perl_append_utf8_from_native_byte(const U8 byte, U8** dest)
{
    /* Takes an input 'byte' (Latin1 or EBCDIC) and appends it to the UTF-8
     * encoded string at '*dest', updating '*dest' to include it */

    PERL_ARGS_ASSERT_APPEND_UTF8_FROM_NATIVE_BYTE;

    if (NATIVE_BYTE_IS_INVARIANT(byte))
        *((*dest)++) = byte;
    else {
        *((*dest)++) = UTF8_EIGHT_BIT_HI(byte);
        *((*dest)++) = UTF8_EIGHT_BIT_LO(byte);
    }
}

PERL_STATIC_INLINE U8 *
Perl_bytes_to_utf8(pTHX_ const U8 *s, STRLEN *lenp)
{
    return bytes_to_utf8_free_me(s, lenp, NULL);
}

PERL_STATIC_INLINE U8 *
Perl_bytes_to_utf8_temp_pv(pTHX_ const U8 *s, STRLEN *lenp)
{
    void * free_me = NULL;
    U8 * converted = bytes_to_utf8_free_me(s, lenp, &free_me);

    if (free_me) {
        SAVEFREEPV(free_me);
    }

    return converted;
}

PERL_STATIC_INLINE bool
Perl_utf8_to_bytes_new_pv(pTHX_ U8 const **s_ptr, STRLEN *lenp, void ** free_me)
{
    /* utf8_to_bytes_() is declared to take a non-const s_ptr because it may
     * change it, but NOT when called with PL_utf8_to_bytes_new_memory, so it
     * is ok to cast away const */
    return utf8_to_bytes_((U8 **) s_ptr, lenp, free_me,
                          PL_utf8_to_bytes_new_memory);
}

PERL_STATIC_INLINE bool
Perl_utf8_to_bytes_temp_pv(pTHX_ U8 const **s_ptr, STRLEN *lenp)
{
    /* utf8_to_bytes_() requires a non-NULL pointer, but doesn't use it when
     * called with PL_utf8_to_bytes_use_temporary */
    void* dummy = NULL;

    /* utf8_to_bytes_() is declared to take a non-const s_ptr because it may
     * change it, but NOT when called with PL_utf8_to_bytes_use_temporary, so
     * it is ok to cast away const */
    return utf8_to_bytes_((U8 **) s_ptr, lenp, &dummy,
                          PL_utf8_to_bytes_use_temporary);
}

PERL_STATIC_INLINE bool
Perl_utf8_to_bytes_overwrite(pTHX_ U8 **s_ptr, STRLEN *lenp)
{
    /* utf8_to_bytes_() requires a non-NULL pointer, but doesn't use it when
     * called with PL_utf8_to_bytes_overwrite */
    void* dummy = NULL;

    return utf8_to_bytes_(s_ptr, lenp, &dummy, PL_utf8_to_bytes_overwrite);
}

/*
=for apidoc valid_utf8_to_uvchr
Like C<L<perlapi/utf8_to_uv>>, but should only be called when it is
known that the next character in the input UTF-8 string C<s> is well-formed
(I<e.g.>, it passes C<L<perlapi/isUTF8_CHAR>>.  Surrogates, non-character code
points, and non-Unicode code points are allowed.

=cut

 */

PERL_STATIC_INLINE UV
Perl_valid_utf8_to_uvchr(const U8 *s, STRLEN *retlen)
{
    const UV expectlen = UTF8SKIP(s);
    const U8* send = s + expectlen;
    UV uv = *s;

    PERL_ARGS_ASSERT_VALID_UTF8_TO_UVCHR;

    if (retlen) {
        *retlen = expectlen;
    }

    /* An invariant is trivially returned */
    if (expectlen == 1) {
        return uv;
    }

    /* Remove the leading bits that indicate the number of bytes, leaving just
     * the bits that are part of the value */
    uv = NATIVE_UTF8_TO_I8(uv) & UTF_START_MASK(expectlen);

    /* Now, loop through the remaining bytes, accumulating each into the
     * working total as we go.  (I khw tried unrolling the loop for up to 4
     * bytes, but there was no performance improvement) */
    for (++s; s < send; s++) {
        uv = UTF8_ACCUMULATE(uv, *s);
    }

    return UNI_TO_NATIVE(uv);

}

/* This looks like 0x010101... */
#  define PERL_COUNT_MULTIPLIER   (~ (UINTMAX_C(0)) / 0xFF)

/* This looks like 0x808080... */
#  define PERL_VARIANTS_WORD_MASK (PERL_COUNT_MULTIPLIER * 0x80)
#  define PERL_WORDSIZE            sizeof(PERL_UINTMAX_T)
#  define PERL_WORD_BOUNDARY_MASK (PERL_WORDSIZE - 1)

/* Evaluates to 0 if 'x' is at a word boundary; otherwise evaluates to 1, by
 * or'ing together the lowest bits of 'x'.  Hopefully the final term gets
 * optimized out completely on a 32-bit system, and its mask gets optimized out
 * on a 64-bit system */
#  define PERL_IS_SUBWORD_ADDR(x) (1 & (       PTR2nat(x)                     \
                                      |   (  PTR2nat(x) >> 1)                 \
                                      | ( ( (PTR2nat(x)                       \
                                           & PERL_WORD_BOUNDARY_MASK) >> 2))))

/*
=for apidoc      is_utf8_invariant_string
=for apidoc_item is_utf8_invariant_string_loc
=for apidoc_item is_ascii_string
=for apidoc_item is_invariant_string

These each return TRUE if the first C<len> bytes of the string C<s> are the
same regardless of the UTF-8 encoding of the string (or UTF-EBCDIC encoding on
EBCDIC machines); otherwise they returns FALSE.  That is, they return TRUE if
they are UTF-8 invariant.  On ASCII-ish machines, all the ASCII characters and
only the ASCII characters fit this definition.  On EBCDIC machines, the
ASCII-range characters are invariant, but so also are the C1 controls.

If C<len> is 0, it will be calculated using C<strlen(s)>, (which means if you
use this option, that C<s> can't have embedded C<NUL> characters and has to
have a terminating C<NUL> byte).

All forms except C<is_utf8_invariant_string_loc> have identical behavior.  The
only difference with it is that it has an extra pointer parameter, C<ep>, into
which, if it isn't NULL, the location of the first UTF-8 variant character in
the C<ep> pointer will be stored upon failure.  If all characters are UTF-8
invariant, this function does not change the contents of C<*ep>.

C<is_invariant_string> is somewhat misleadingly named.
C<is_utf8_invariant_string> is preferred, as it indicates under what conditions
the string is invariant.

C<is_ascii_string> is misleadingly-named.  On ASCII-ish platforms, the name
isn't misleading: the ASCII-range characters are exactly the UTF-8 invariants.
But EBCDIC machines have more UTF-8 invariants than just the ASCII characters,
so the name C<is_utf8_invariant_string> is preferred.

See also
C<L</is_utf8_string>> and C<L</is_utf8_fixed_width_buf_flags>>.

=for apidoc_defn ARTm|bool|is_utf8_invariant_string|NN const U8 * const s|STRLEN len

=cut

*/

#define is_utf8_invariant_string(s, len)                                    \
                                is_utf8_invariant_string_loc(s, len, NULL)

PERL_STATIC_INLINE bool
Perl_is_utf8_invariant_string_loc(const U8* const s, STRLEN len, const U8 ** ep)
{
    const U8* send;
    const U8* x = s;

    PERL_ARGS_ASSERT_IS_UTF8_INVARIANT_STRING_LOC;

    if (len == 0) {
        len = strlen((const char *)s);
    }

    send = s + len;

#ifndef EBCDIC

    /* Do the word-at-a-time iff there is at least one usable full word.  That
     * means that after advancing to a word boundary, there still is at least a
     * full word left.  The number of bytes needed to advance is 'wordsize -
     * offset' unless offset is 0. */
    if ((STRLEN) (send - x) >= PERL_WORDSIZE

                            /* This term is wordsize if subword; 0 if not */
                          + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(x)

                            /* 'offset' */
                          - (PTR2nat(x) & PERL_WORD_BOUNDARY_MASK))
    {

        /* Process per-byte until reach word boundary.  XXX This loop could be
         * eliminated if we knew that this platform had fast unaligned reads */
        while (PTR2nat(x) & PERL_WORD_BOUNDARY_MASK) {
            if (! UTF8_IS_INVARIANT(*x)) {
                if (ep) {
                    *ep = x;
                }

                return FALSE;
            }
            x++;
        }

        /* Here, we know we have at least one full word to process.  Process
         * per-word as long as we have at least a full word left */
        do {
            if ((* (const PERL_UINTMAX_T *) x) & PERL_VARIANTS_WORD_MASK)  {

                /* Found a variant.  Just return if caller doesn't want its
                 * exact position */
                if (! ep) {
                    return FALSE;
                }

#  if   BYTEORDER == 0x1234 || BYTEORDER == 0x12345678    \
     || BYTEORDER == 0x4321 || BYTEORDER == 0x87654321

                *ep = x + variant_byte_number(* (const PERL_UINTMAX_T *) x);
                assert(*ep >= s && *ep < send);

                return FALSE;

#  else   /* If weird byte order, drop into next loop to do byte-at-a-time
           checks. */

                break;
#  endif
            }

            x += PERL_WORDSIZE;

        } while (x + PERL_WORDSIZE <= send);
    }

#endif      /* End of ! EBCDIC */

    /* Process per-byte.  (Can't use libc functions like strpbrk() because
     * input isn't necessarily a C string) */
    while (x < send) {
        if (! UTF8_IS_INVARIANT(*x)) {
            if (ep) {
                *ep = x;
            }

            return FALSE;
        }

        x++;
    }

    return TRUE;
}

/* See if the platform has builtins for finding the most/least significant bit,
 * and which one is right for using on 32 and 64 bit operands */
#if (__has_builtin(__builtin_clz) || PERL_GCC_VERSION_GE(3,4,0))
#  if U32SIZE == INTSIZE
#    define PERL_CLZ_32 __builtin_clz
#  endif
#  if defined(U64TYPE) && U64SIZE == INTSIZE
#    define PERL_CLZ_64 __builtin_clz
#  endif
#endif
#if (__has_builtin(__builtin_ctz) || PERL_GCC_VERSION_GE(3,4,0))
#  if U32SIZE == INTSIZE
#    define PERL_CTZ_32 __builtin_ctz
#  endif
#  if defined(U64TYPE) && U64SIZE == INTSIZE
#    define PERL_CTZ_64 __builtin_ctz
#  endif
#endif

#if (__has_builtin(__builtin_clzl) || PERL_GCC_VERSION_GE(3,4,0))
#  if U32SIZE == LONGSIZE && ! defined(PERL_CLZ_32)
#    define PERL_CLZ_32 __builtin_clzl
#  endif
#  if defined(U64TYPE) && U64SIZE == LONGSIZE && ! defined(PERL_CLZ_64)
#    define PERL_CLZ_64 __builtin_clzl
#  endif
#endif
#if (__has_builtin(__builtin_ctzl) || PERL_GCC_VERSION_GE(3,4,0))
#  if U32SIZE == LONGSIZE && ! defined(PERL_CTZ_32)
#    define PERL_CTZ_32 __builtin_ctzl
#  endif
#  if defined(U64TYPE) && U64SIZE == LONGSIZE && ! defined(PERL_CTZ_64)
#    define PERL_CTZ_64 __builtin_ctzl
#  endif
#endif

#if (__has_builtin(__builtin_clzll) || PERL_GCC_VERSION_GE(3,4,0))
#  if U32SIZE == LONGLONGSIZE && ! defined(PERL_CLZ_32)
#    define PERL_CLZ_32 __builtin_clzll
#  endif
#  if defined(U64TYPE) && U64SIZE == LONGLONGSIZE && ! defined(PERL_CLZ_64)
#    define PERL_CLZ_64 __builtin_clzll
#  endif
#endif
#if (__has_builtin(__builtin_ctzll) || PERL_GCC_VERSION_GE(3,4,0))
#  if U32SIZE == LONGLONGSIZE && ! defined(PERL_CTZ_32)
#    define PERL_CTZ_32 __builtin_ctzll
#  endif
#  if defined(U64TYPE) && U64SIZE == LONGLONGSIZE && ! defined(PERL_CTZ_64)
#    define PERL_CTZ_64 __builtin_ctzll
#  endif
#endif

#if defined(WIN32)
#  include <intrin.h>
   /* MinGW warns that it ignores "pragma intrinsic". */
#  if defined(_MSC_VER)
#    pragma intrinsic(_BitScanForward)
#    pragma intrinsic(_BitScanReverse)
#    if defined(_WIN64)
#      pragma intrinsic(_BitScanForward64)
#      pragma intrinsic(_BitScanReverse64)
#    endif
#  endif
#endif

/* The reason there are not checks to see if ffs() and ffsl() are available for
 * determining the lsb, is because these don't improve on the deBruijn method
 * fallback, which is just a branchless integer multiply, array element
 * retrieval, and shift.  The others, even if the function call overhead is
 * optimized out, have to cope with the possibility of the input being all
 * zeroes, and almost certainly will have conditionals for this eventuality.
 * khw, at the time of this commit, looked at the source for both gcc and clang
 * to verify this.  (gcc used a method inferior to deBruijn.) */

/* Below are functions to find the first, last, or only set bit in a word.  On
 * platforms with 64-bit capability, there is a pair for each operation; the
 * first taking a 64 bit operand, and the second a 32 bit one.  The logic is
 * the same in each pair, so the second is stripped of most comments. */

#ifdef U64TYPE  /* HAS_QUAD not usable outside the core */

PERL_STATIC_INLINE unsigned
Perl_lsbit_pos64(U64 word)
{
    /* Find the position (0..63) of the least significant set bit in the input
     * word */

    ASSUME(word != 0);

    /* If we can determine that the platform has a usable fast method to get
     * this info, use that */

#  if defined(PERL_CTZ_64)
#    define PERL_HAS_FAST_GET_LSB_POS64

    return (unsigned) PERL_CTZ_64(word);

#  elif U64SIZE == 8 && defined(_WIN64)
#    define PERL_HAS_FAST_GET_LSB_POS64

    {
        unsigned long index;
        _BitScanForward64(&index, word);
        return (unsigned)index;
    }

#  else

    /* Here, we didn't find a fast method for finding the lsb.  Fall back to
     * making the lsb the only set bit in the word, and use our function that
     * works on words with a single bit set.
     *
     * Isolate the lsb;
     * https://stackoverflow.com/questions/757059/position-of-least-significant-bit-that-is-set
     *
     * The word will look like this, with a rightmost set bit in position 's':
     * ('x's are don't cares, and 'y's are their complements)
     *      s
     *  x..x100..00
     *  y..y011..11      Complement
     *  y..y100..00      Add 1
     *  0..0100..00      And with the original
     *
     *  (Yes, complementing and adding 1 is just taking the negative on 2's
     *  complement machines, but not on 1's complement ones, and some compilers
     *  complain about negating an unsigned.)
     */
    return single_1bit_pos64(word & (~word + 1));

#  endif

}

#  define lsbit_pos_uintmax_(word) lsbit_pos64(word)
#else   /* ! QUAD */
#  define lsbit_pos_uintmax_(word) lsbit_pos32(word)
#endif

PERL_STATIC_INLINE unsigned     /* Like above for 32 bit word */
Perl_lsbit_pos32(U32 word)
{
    /* Find the position (0..31) of the least significant set bit in the input
     * word */

    ASSUME(word != 0);

#if defined(PERL_CTZ_32)
#  define PERL_HAS_FAST_GET_LSB_POS32

    return (unsigned) PERL_CTZ_32(word);

#elif U32SIZE == 4 && defined(WIN32)
#  define PERL_HAS_FAST_GET_LSB_POS32

    {
        unsigned long index;
        _BitScanForward(&index, word);
        return (unsigned)index;
    }

#elif defined(PERL_HAS_FAST_GET_LSB_POS64)
#  define PERL_HAS_FAST_GET_LSB_POS32

    /* Unlikely, but possible for the platform to have a wider fast operation
     * but not a narrower one.  But easy enough to handle the case by widening
     * the parameter size. */
    return lsbit_pos64(word);

#else

    return single_1bit_pos32(word & (~word + 1));

#endif

}


/* Convert the leading zeros count to the bit position of the first set bit.
 * This just subtracts from the highest position, 31 or 63.  But some compilers
 * don't optimize this optimally, and so a bit of bit twiddling encourages them
 * to do the right thing.  It turns out that subtracting a smaller non-negative
 * number 'x' from 2**n-1 for any n is the same as taking the exclusive-or of
 * the two numbers.  To see why, first note that the sum of any number, x, and
 * its complement, x', is all ones.  So all ones minus x is x'.  Then note that
 * the xor of x and all ones is x'. */
#define LZC_TO_MSBIT_POS_(size, lzc)  ((size##SIZE * CHARBITS - 1) ^ (lzc))

#ifdef U64TYPE  /* HAS_QUAD not usable outside the core */

PERL_STATIC_INLINE unsigned
Perl_msbit_pos64(U64 word)
{
    /* Find the position (0..63) of the most significant set bit in the input
     * word */

    ASSUME(word != 0);

    /* If we can determine that the platform has a usable fast method to get
     * this, use that */

#  if defined(PERL_CLZ_64)
#    define PERL_HAS_FAST_GET_MSB_POS64

    return (unsigned) LZC_TO_MSBIT_POS_(U64, PERL_CLZ_64(word));

#  elif U64SIZE == 8 && defined(_WIN64)
#    define PERL_HAS_FAST_GET_MSB_POS64

    {
        unsigned long index;
        _BitScanReverse64(&index, word);
        return (unsigned)index;
    }

#  else

    /* Here, we didn't find a fast method for finding the msb.  Fall back to
     * making the msb the only set bit in the word, and use our function that
     * works on words with a single bit set.
     *
     * Isolate the msb; http://codeforces.com/blog/entry/10330
     *
     * Only the most significant set bit matters.  Or'ing word with its right
     * shift of 1 makes that bit and the next one to its right both 1.
     * Repeating that with the right shift of 2 makes for 4 1-bits in a row.
     * ...  We end with the msb and all to the right being 1. */
    word |= (word >>  1);
    word |= (word >>  2);
    word |= (word >>  4);
    word |= (word >>  8);
    word |= (word >> 16);
    word |= (word >> 32);

    /* Then subtracting the right shift by 1 clears all but the left-most of
     * the 1 bits, which is our desired result */
    word -= (word >> 1);

    /* Now we have a single bit set */
    return single_1bit_pos64(word);

#  endif

}

#  define msbit_pos_uintmax_(word) msbit_pos64(word)
#else   /* ! QUAD */
#  define msbit_pos_uintmax_(word) msbit_pos32(word)
#endif

PERL_STATIC_INLINE unsigned
Perl_msbit_pos32(U32 word)
{
    /* Find the position (0..31) of the most significant set bit in the input
     * word */

    ASSUME(word != 0);

#if defined(PERL_CLZ_32)
#  define PERL_HAS_FAST_GET_MSB_POS32

    return (unsigned) LZC_TO_MSBIT_POS_(U32, PERL_CLZ_32(word));
#elif U32SIZE == 4 && defined(WIN32)
#  define PERL_HAS_FAST_GET_MSB_POS32

    {
        unsigned long index;
        _BitScanReverse(&index, word);
        return (unsigned)index;
    }

#elif defined(PERL_HAS_FAST_GET_MSB_POS64)
#  define PERL_HAS_FAST_GET_MSB_POS32

    return msbit_pos64(word);   /* Let compiler widen parameter */

#else

    word |= (word >>  1);
    word |= (word >>  2);
    word |= (word >>  4);
    word |= (word >>  8);
    word |= (word >> 16);
    word -= (word >> 1);
    return single_1bit_pos32(word);

#endif

}

/* Note that if you are working through all the 1 bits in a word, and don't
 * care which order you process them in, it is better to use lsbit_pos.  This
 * is because some platforms have a fast way to find the msb but not the lsb,
 * and others vice versa.  The code above falls back to use the single
 * available fast method when the desired one is missing, and it is cheaper to
 * fall back from lsb to msb than the other way around */

#if UVSIZE == U64SIZE
#  define msbit_pos(word)  msbit_pos64(word)
#  define lsbit_pos(word)  lsbit_pos64(word)
#elif UVSIZE == U32SIZE
#  define msbit_pos(word)  msbit_pos32(word)
#  define lsbit_pos(word)  lsbit_pos32(word)
#endif

#ifdef U64TYPE  /* HAS_QUAD not usable outside the core */

PERL_STATIC_INLINE unsigned
Perl_single_1bit_pos64(U64 word)
{
    /* Given a 64-bit word known to contain all zero bits except one 1 bit,
     * find and return the 1's position: 0..63 */

#  ifdef PERL_CORE    /* macro not exported */
    ASSUME(isPOWER_OF_2(word));
#  else
    ASSUME(word && (word & (word-1)) == 0);
#  endif

    /* The only set bit is both the most and least significant bit.  If we have
     * a fast way of finding either one, use that.
     *
     * It may appear at first glance that those functions call this one, but
     * they don't if the corresponding #define is set */

#  ifdef PERL_HAS_FAST_GET_MSB_POS64

    return msbit_pos64(word);

#  elif defined(PERL_HAS_FAST_GET_LSB_POS64)

    return lsbit_pos64(word);

#  else

    /* The position of the only set bit in a word can be quickly calculated
     * using deBruijn sequences.  See for example
     * https://en.wikipedia.org/wiki/De_Bruijn_sequence */
    return PL_deBruijn_bitpos_tab64[(word * PERL_deBruijnMagic64_)
                                                    >> PERL_deBruijnShift64_];
#  endif

}

#endif

PERL_STATIC_INLINE unsigned
Perl_single_1bit_pos32(U32 word)
{
    /* Given a 32-bit word known to contain all zero bits except one 1 bit,
     * find and return the 1's position: 0..31 */

#ifdef PERL_CORE    /* macro not exported */
    ASSUME(isPOWER_OF_2(word));
#else
    ASSUME(word && (word & (word-1)) == 0);
#endif
#ifdef PERL_HAS_FAST_GET_MSB_POS32

    return msbit_pos32(word);

#elif defined(PERL_HAS_FAST_GET_LSB_POS32)

    return lsbit_pos32(word);

#else

    return PL_deBruijn_bitpos_tab32[(word * PERL_deBruijnMagic32_)
                                                    >> PERL_deBruijnShift32_];
#endif

}

#ifndef EBCDIC

PERL_STATIC_INLINE unsigned int
Perl_variant_byte_number(PERL_UINTMAX_T word)
{
    /* This returns the position in a word (0..7) of the first variant byte in
     * it.  This is a helper function.  Note that there are no branches */

    /* Get just the msb bits of each byte */
    word &= PERL_VARIANTS_WORD_MASK;

    /* This should only be called if we know there is a variant byte in the
     * word */
    assert(word);

#  if BYTEORDER == 0x1234 || BYTEORDER == 0x12345678

    /* Bytes are stored like
     *  Byte8 ... Byte2 Byte1
     *  63..56...15...8 7...0
     * so getting the lsb of the whole modified word is getting the msb of the
     * first byte that has its msb set */
    word = lsbit_pos_uintmax_(word);

    /* Here, word contains the position 7,15,23,...55,63 of that bit.  Convert
     * to 0..7 */
    return (unsigned int) ((word + 1) >> 3) - 1;

#  elif BYTEORDER == 0x4321 || BYTEORDER == 0x87654321

    /* Bytes are stored like
     *  Byte1 Byte2  ... Byte8
     * 63..56 55..47 ... 7...0
     * so getting the msb of the whole modified word is getting the msb of the
     * first byte that has its msb set */
    word = msbit_pos_uintmax_(word);

    /* Here, word contains the position 63,55,...,23,15,7 of that bit.  Convert
     * to 0..7 */
    word = ((word + 1) >> 3) - 1;

    /* And invert the result because of the reversed byte order on this
     * platform */
    word = CHARBITS - word - 1;

    return (unsigned int) word;

#  else
#    error Unexpected byte order
#  endif

}

#endif
#if defined(PERL_CORE) || defined(PERL_EXT)

/*
=for apidoc variant_under_utf8_count

This function looks at the sequence of bytes between C<s> and C<e>, which are
assumed to be encoded in ASCII/Latin1, and returns how many of them would
change should the string be translated into UTF-8.  Due to the nature of UTF-8,
each of these would occupy two bytes instead of the single one in the input
string.  Thus, this function returns the precise number of bytes the string
would expand by when translated to UTF-8.

Unlike most of the other functions that have C<utf8> in their name, the input
to this function is NOT a UTF-8-encoded string.  The function name is slightly
I<odd> to emphasize this.

This function is internal to Perl because khw thinks that any XS code that
would want this is probably operating too close to the internals.  Presenting a
valid use case could change that.

See also
C<L<perlapi/is_utf8_invariant_string>>
and
C<L<perlapi/is_utf8_invariant_string_loc>>,

=cut

*/

PERL_STATIC_INLINE Size_t
S_variant_under_utf8_count(const U8* const s, const U8* const e)
{
    const U8* x = s;
    Size_t count = 0;

    PERL_ARGS_ASSERT_VARIANT_UNDER_UTF8_COUNT;

#  ifndef EBCDIC

    /* Test if the string is long enough to use word-at-a-time.  (Logic is the
     * same as for is_utf8_invariant_string()) */
    if ((STRLEN) (e - x) >= PERL_WORDSIZE
                          + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(x)
                          - (PTR2nat(x) & PERL_WORD_BOUNDARY_MASK))
    {

        /* Process per-byte until reach word boundary.  XXX This loop could be
         * eliminated if we knew that this platform had fast unaligned reads */
        while (PTR2nat(x) & PERL_WORD_BOUNDARY_MASK) {
            count += ! UTF8_IS_INVARIANT(*x++);
        }

        /* Process per-word as long as we have at least a full word left */
        do {    /* Commit 03c1e4ab1d6ee9062fb3f94b0ba31db6698724b1 contains an
                   explanation of how this works */
            PERL_UINTMAX_T increment
                = ((((* (PERL_UINTMAX_T *) x) & PERL_VARIANTS_WORD_MASK) >> 7)
                      * PERL_COUNT_MULTIPLIER)
                    >> ((PERL_WORDSIZE - 1) * CHARBITS);
            count += (Size_t) increment;
            x += PERL_WORDSIZE;
        } while (x + PERL_WORDSIZE <= e);
    }

#  endif

    /* Process per-byte */
    while (x < e) {
        if (! UTF8_IS_INVARIANT(*x)) {
            count++;
        }

        x++;
    }

    return count;
}

#endif

   /* Keep  these around for these files */
#if ! defined(PERL_IN_REGEXEC_C) && ! defined(PERL_IN_UTF8_C)
#  undef PERL_WORDSIZE
#  undef PERL_COUNT_MULTIPLIER
#  undef PERL_WORD_BOUNDARY_MASK
#  undef PERL_VARIANTS_WORD_MASK
#endif

#define is_utf8_string(s, len)  is_utf8_string_loclen(s, len, NULL, NULL)

#if defined(PERL_CORE) || defined (PERL_EXT)

/*
=for apidoc is_utf8_non_invariant_string

Returns TRUE if L<perlapi/is_utf8_invariant_string> returns FALSE for the first
C<len> bytes of the string C<s>, but they are, nonetheless, legal Perl-extended
UTF-8; otherwise returns FALSE.

A TRUE return means that at least one code point represented by the sequence
either is a wide character not representable as a single byte, or the
representation differs depending on whether the sequence is encoded in UTF-8 or
not.

See also C<L<perlapi/is_utf8_invariant_string>>.

=cut

This is commonly used to determine if a SV's UTF-8 flag should be turned on.
It generally needn't be if its string is entirely UTF-8 invariant, and it
shouldn't be if it otherwise contains invalid UTF-8.

It is an internal function because khw thinks that XS code shouldn't be working
at this low a level.  A valid use case could change that.

*/

PERL_STATIC_INLINE bool
Perl_is_utf8_non_invariant_string(const U8* const s, STRLEN len)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_UTF8_NON_INVARIANT_STRING;

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        return FALSE;
    }

    return is_utf8_string(first_variant, len - (first_variant - s));
}

#endif

/*
=for apidoc      is_utf8_string
=for apidoc_item is_utf8_string_loc
=for apidoc_item is_utf8_string_loclen
=for apidoc_item is_strict_utf8_string
=for apidoc_item is_strict_utf8_string_loc
=for apidoc_item is_strict_utf8_string_loclen
=for apidoc_item is_c9strict_utf8_string
=for apidoc_item is_c9strict_utf8_string_loc
=for apidoc_item is_c9strict_utf8_string_loclen
=for apidoc_item is_utf8_string_flags
=for apidoc_item is_utf8_string_loc_flags
=for apidoc_item is_utf8_string_loclen_flags

These each return TRUE if the first C<len> bytes of string C<s> form a valid
UTF-8 string for varying degrees of strictness, FALSE otherwise.  If C<len> is
0, it will be calculated using C<strlen(s)> (which means if you use this
option, that C<s> can't have embedded C<NUL> characters and has to have a
terminating C<NUL> byte).  Note that all characters being ASCII constitute 'a
valid UTF-8 string'.

Some of the functions also return information about the string.  Those that
have the suffix C<_loc> in their names have an extra parameter, C<ep>.  If that
is not NULL, the function stores into it the location of how far it got in
parsing C<s>.  If the function is returning TRUE, this will be a pointer to the
byte immediately after the end of C<s>.  If FALSE, it will be the location of
the first byte that fails the criteria.

The functions that instead have the suffix C<_loclen> have a second extra
parameter, C<el>.  They act as the plain C<_loc> functions do with their C<ep>
parameter, but if C<el> is not null, the functions store into it the number of
UTF-8 encoded characters found at the point where parsing stopped.  If the
function is returning TRUE, this will be the full count of the UTF-8 characters
in C<s>; if FALSE, it will be the count before the first invalid one.

C<is_utf8_string> (and C<is_utf8_string_loc> and C<is_utf8_string_loclen>)
consider Perl's extended UTF-8 to be valid.  That means that
code points above Unicode, surrogates, and non-character code points are
all considered valid by this function.  Problems may arise in interchange with
non-Perl applications, or (unlikely) between machines with different word
sizes.

C<is_strict_utf8_string> (and C<is_strict_utf8_string_loc> and
C<is_strict_utf8_string_loclen>) consider only Unicode-range (0 to 0x10FFFF)
code points to be valid, with the surrogates and non-character code points
invalid.  This level of strictness is what is safe to accept from outside
sources that use Unicode rules.

The forms whose names contain C<c9strict> conform to the level of strictness
given in
L<Unicode Corrigendum #9|http://www.unicode.org/versions/corrigendum9.html>.
This means Unicode-range code points including non-character ones are
considered valid, but not the surrogates.  This level of strictness is
considered safe for cooperating components that know how the other components
handle non-character code points.

The forms whose names contain C<_flags> allow you to customize the acceptable
level of strictness.  They have an extra parameter, C<flags> to indicate the
types of code points that are acceptable.  If C<flags> is 0, they give the
same results as C<L</is_utf8_string>> (and kin); if C<flags> is
C<UTF8_DISALLOW_ILLEGAL_INTERCHANGE>, they give the same results as
C<L</is_strict_utf8_string>> (and kin); and if C<flags> is
C<UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE>, they give the same results as
C<L</is_c9strict_utf8_string>> (and kin).  Otherwise C<flags> may be any
combination of the C<UTF8_DISALLOW_I<foo>> flags understood by
C<L</utf8_to_uv>>, with the same meanings.

It's better to use one of the non-C<_flags> functions if they give you the
desired strictness, as those have a better chance of being inlined by the C
compiler.

See also
C<L</is_utf8_invariant_string>>,
C<L</is_utf8_fixed_width_buf_flags>>,

=cut
*/

#define is_strict_utf8_string(s, len)  is_strict_utf8_string_loclen(s, len, NULL, NULL)

#define is_c9strict_utf8_string(s, len)  is_c9strict_utf8_string_loclen(s, len, NULL, 0)

PERL_STATIC_INLINE bool
Perl_is_utf8_string_flags(const U8 *s, STRLEN len, const U32 flags)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_UTF8_STRING_FLAGS;
    assert(0 == (flags & ~UTF8_DISALLOW_ILLEGAL_INTERCHANGE));

    if (len == 0) {
        len = strlen((const char *)s);
    }

    if (flags == 0) {
        return is_utf8_string(s, len);
    }

    if ((flags & UTF8_DISALLOW_ILLEGAL_INTERCHANGE)
                                        == UTF8_DISALLOW_ILLEGAL_INTERCHANGE)
    {
        return is_strict_utf8_string(s, len);
    }

    if ((flags & UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE)
                                       == UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE)
    {
        return is_c9strict_utf8_string(s, len);
    }

    if (! is_utf8_invariant_string_loc(s, len, &first_variant)) {
        const U8* const send = s + len;
        const U8* x = first_variant;

        while (x < send) {
            STRLEN cur_len = isUTF8_CHAR_flags(x, send, flags);
            if (UNLIKELY(! cur_len)) {
                return FALSE;
            }
            x += cur_len;
        }
    }

    return TRUE;
}

#define Perl_is_utf8_string_loc(s, len, ep)          \
        Perl_is_utf8_string_loclen(s, len, ep, 0)

PERL_STATIC_INLINE bool
Perl_is_utf8_string_loclen(const U8 *s, STRLEN len, const U8 **ep, STRLEN *el)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_UTF8_STRING_LOCLEN;

    if (len == 0) {
        len = strlen((const char *) s);
    }

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        if (el)
            *el = len;

        if (ep) {
            *ep = s + len;
        }

        return TRUE;
    }

    {
        const U8* const send = s + len;
        const U8* x = first_variant;
        STRLEN outlen = first_variant - s;

        while (x < send) {
            const STRLEN cur_len = isUTF8_CHAR(x, send);
            if (UNLIKELY(! cur_len)) {
                break;
            }
            x += cur_len;
            outlen++;
        }

        if (el)
            *el = outlen;

        if (ep) {
            *ep = x;
        }

        return (x == send);
    }
}

/* The perl core arranges to never call the DFA below without there being at
 * least one byte available to look at.  This allows the DFA to use a do {}
 * while loop which means that calling it with a UTF-8 invariant has a single
 * conditional, same as the calling code checking for invariance ahead of time.
 * And having the calling code remove that conditional speeds up by that
 * conditional, the case where it wasn't invariant.  So there's no reason to
 * check before calling this.
 *
 * But we don't know this for non-core calls, so have to retain the check for
 * them. */
#ifdef PERL_CORE
#  define PERL_NON_CORE_CHECK_EMPTY(s,e)  assert((e) > (s))
#else
#  define PERL_NON_CORE_CHECK_EMPTY(s,e)  if ((e) <= (s)) return FALSE
#endif

/*
 * DFA for checking input is valid UTF-8 syntax.
 *
 * This uses adaptations of the table and algorithm given in
 * https://bjoern.hoehrmann.de/utf-8/decoder/dfa/, which provides comprehensive
 * documentation of the original version.  A copyright notice for the original
 * version is given at the beginning of this file.  The Perl adaptations are
 * documented at the definition of PL_extended_utf8_dfa_tab[].
 *
 * This dfa is fast.  There are three exit conditions:
 *  1) a well-formed code point, acceptable to the table
 *  2) the beginning bytes of an incomplete character, whose completion might
 *     or might not be acceptable
 *  3) unacceptable to the table.  Some of the adaptations have certain,
 *     hopefully less likely to occur, legal inputs be unacceptable to the
 *     table, so these must be sorted out afterwards.
 *
 * This macro is a complete implementation of the code executing the DFA.  It
 * is passed the input sequence bounds and the table to use, and what to do
 * for each of the exit conditions.  There are three canned actions, likely to
 * be the ones you want:
 *      DFA_RETURN_SUCCESS_
 *      DFA_RETURN_FAILURE_
 *      DFA_GOTO_TEASE_APART_FF_
 *
 * You pass a parameter giving the action to take for each of the three
 * possible exit conditions:
 *
 * 'accept_action'  This is executed when the DFA accepts the input.
 *                  DFA_RETURN_SUCCESS_ is the most likely candidate.
 * 'reject_action'  This is executed when the DFA rejects the input.
 *                  DFA_RETURN_FAILURE_ is a candidate, or 'goto label' where
 *                  you have written code to distinguish the rejecting state
 *                  results.  Because it happens in several places, and
 *                  involves #ifdefs, the special action
 *                  DFA_GOTO_TEASE_APART_FF_ is what you want with
 *                  PL_extended_utf8_dfa_tab.  On platforms without
 *                  EXTRA_LONG_UTF8, there is no need to tease anything apart,
 *                  so this evaluates to DFA_RETURN_FAILURE_; otherwise you
 *                  need to have a label 'tease_apart_FF' that it will transfer
 *                  to.
 * 'incomplete_char_action'  This is executed when the DFA ran off the end
 *                  before accepting or rejecting the input.
 *                  DFA_RETURN_FAILURE_ is the likely action, but you could
 *                  have a 'goto', or NOOP.  In the latter case the DFA drops
 *                  off the end, and you place your code to handle this case
 *                  immediately after it.
 */

#define DFA_RETURN_SUCCESS_      return (s8dfa_ - s0)
#define DFA_RETURN_FAILURE_      return 0
#ifdef HAS_EXTRA_LONG_UTF8
#  define DFA_TEASE_APART_FF_  goto tease_apart_FF
#else
#  define DFA_TEASE_APART_FF_  DFA_RETURN_FAILURE_
#endif

#define PERL_IS_UTF8_CHAR_DFA(s0, e, dfa_tab,                               \
                              accept_action,                                \
                              reject_action,                                \
                              incomplete_char_action)                       \
    STMT_START {                                                            \
        const U8 * s8dfa_ = s0;                                             \
        const U8 * const e8dfa_ = e;                                        \
        PERL_UINT_FAST16_T state = 0;                                        \
                                                                            \
        PERL_NON_CORE_CHECK_EMPTY(s8dfa_, e8dfa_);                          \
                                                                            \
        do {                                                                \
            state = dfa_tab[256 + state + dfa_tab[*s8dfa_]];                \
        } while (++s8dfa_ < e8dfa_ && state > 1);                           \
                                                                            \
        if (LIKELY(state == 0)) {   /* Accepting state */                   \
            accept_action;                                                  \
        }                                                                   \
                                                                            \
        if (state == 1) { /* Rejecting state */                             \
            reject_action;                                                  \
        }                                                                   \
                                                                            \
        /* Here, dropped out of loop before end-of-char */                  \
        incomplete_char_action;                                             \
    } STMT_END


/*

=for apidoc      isUTF8_CHAR
=for apidoc_item isSTRICT_UTF8_CHAR
=for apidoc_item isC9_STRICT_UTF8_CHAR
=for apidoc_item isUTF8_CHAR_flags
=for apidoc_item is_utf8_char_buf

These each evaluate to non-zero if the first few bytes of the string starting
at C<s> and looking no further than S<C<e - 1>> are well-formed UTF-8 that
represents some code point, for varying degrees of strictness.  Otherwise they
evaluate to 0.  If non-zero, the value gives how many bytes starting at C<s>
comprise the code point's representation.  Any bytes remaining before C<e>, but
beyond the ones needed to form the first code point in C<s>, are not examined.

These are used to efficiently decide if the next few bytes in C<s> are
legal UTF-8 for a single character.

With C<isUTF8_CHAR>, the code point can be any that will fit in an IV on this
machine, using Perl's extension to official UTF-8 to represent those higher
than the Unicode maximum of 0x10FFFF.  That means that this will consider byte
sequences to be valid that are unrecognized or considered illegal by non-Perl
applications.

With C<L</isSTRICT_UTF8_CHAR>>, acceptable code points are restricted to those
defined by Unicode to be fully interchangeable across applications.
This means code points above the Unicode range (max legal is 0x10FFFF),
surrogates, and non-character code points are rejected.

With C<L</isC9_STRICT_UTF8_CHAR>>, acceptable code points are restricted to
those defined by Unicode to be fully interchangeable within an application.
This means code points above the Unicode range and surrogates are rejected, but
non-character code points are accepted.  See L<Unicode Corrigendum
#9|http://www.unicode.org/versions/corrigendum9.html>.

Use C<L</isUTF8_CHAR_flags>> to customize what code points are acceptable.
If C<flags> is 0, this gives the same results as C<L</isUTF8_CHAR>>;
if C<flags> is C<UTF8_DISALLOW_ILLEGAL_INTERCHANGE>, this gives the same results
as C<L</isSTRICT_UTF8_CHAR>>;
and if C<flags> is C<UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE>, this gives
the same results as C<L</isC9_STRICT_UTF8_CHAR>>.
Otherwise C<flags> may be any combination of the C<UTF8_DISALLOW_I<foo>> flags
understood by C<L</utf8_to_uv>>, with the same meanings.

The three alternative macros are for the most commonly needed validations; they
are likely to run somewhat faster than this more general one, as they can be
inlined into your code.

Use one of the C<L</is_utf8_string>> forms to check entire strings.

Note also that a UTF-8 "invariant" character (i.e. ASCII on non-EBCDIC
machines) is a valid UTF-8 character.

C<is_utf8_char_buf> is the old name for C<isUTF8_CHAR>.  Do not use it in new
code.

=cut

All the functions except isUTF8_CHAR_flags) use adaptations of the table and
algorithm given in https://bjoern.hoehrmann.de/utf-8/decoder/dfa/, which
provides comprehensive documentation of the original version.  A copyright
notice for the original version is given at the beginning of this file.

The Perl adaptation for isUTF8_CHAR is documented at the definition of
PL_extended_utf8_dfa_tab[].

The Perl adaptation for isSTRICT_UTF8_CHAR is documented at the definition of
PL_strict_utf8_dfa_tab[];

The Perl adaptation for isC9_STRICT_UTF8_CHAR is documented at the definition
of PL_c9_utf8_dfa_tab[].

*/

PERL_STATIC_INLINE Size_t
Perl_isSTRICT_UTF8_CHAR(const U8 * const s0, const U8 * const e)
{
    PERL_ARGS_ASSERT_ISSTRICT_UTF8_CHAR;

    PERL_IS_UTF8_CHAR_DFA(s0, e, PL_strict_utf8_dfa_tab,
                          DFA_RETURN_SUCCESS_,
                          goto check_hanguls,
                          DFA_RETURN_FAILURE_);
  check_hanguls:

    /* Here, we didn't return success, but dropped out of the loop.  In the
     * case of PL_strict_utf8_dfa_tab, this means the input is either
     * malformed, or was for certain Hanguls; handle them specially */

    /* The dfa above drops out for incomplete or illegal inputs, and certain
     * legal Hanguls; check and return accordingly */
    return is_HANGUL_ED_utf8_safe(s0, e);
}

PERL_STATIC_INLINE Size_t
Perl_isUTF8_CHAR(const U8 * const s0, const U8 * const e)
{
    PERL_ARGS_ASSERT_ISUTF8_CHAR;

    PERL_IS_UTF8_CHAR_DFA(s0, e, PL_extended_utf8_dfa_tab,
                          DFA_RETURN_SUCCESS_,
                          DFA_TEASE_APART_FF_,
                          DFA_RETURN_FAILURE_);

    /* Here, we didn't return success, but dropped out of the loop.  In the
     * case of PL_extended_utf8_dfa_tab, this means the input is either
     * malformed, or the start byte was FF on a platform that the dfa doesn't
     * handle FF's.  Call a helper function. */

#ifdef HAS_EXTRA_LONG_UTF8

  tease_apart_FF:

    /* In the case of PL_extended_utf8_dfa_tab, getting here means the input is
     * either malformed, or was for the largest possible start byte, which we
     * now check, not inline */
    if (*s0 != I8_TO_NATIVE_UTF8(0xFF)) {
        return 0;
    }

    return is_utf8_FF_helper_(s0, e,
                              FALSE /* require full, not partial char */
                             );
#endif

}

PERL_STATIC_INLINE Size_t
Perl_isC9_STRICT_UTF8_CHAR(const U8 * const s0, const U8 * const e)
{
    PERL_ARGS_ASSERT_ISC9_STRICT_UTF8_CHAR;

    PERL_IS_UTF8_CHAR_DFA(s0, e, PL_c9_utf8_dfa_tab,
                          DFA_RETURN_SUCCESS_,
                          DFA_RETURN_FAILURE_,
                          DFA_RETURN_FAILURE_);
}

#define is_strict_utf8_string_loc(s, len, ep)                               \
                                is_strict_utf8_string_loclen(s, len, ep, 0)

PERL_STATIC_INLINE bool
Perl_is_strict_utf8_string_loclen(const U8 *s, STRLEN len, const U8 **ep, STRLEN *el)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_STRICT_UTF8_STRING_LOCLEN;

    if (len == 0) {
        len = strlen((const char *) s);
    }

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        if (el)
            *el = len;

        if (ep) {
            *ep = s + len;
        }

        return TRUE;
    }

    {
        const U8* const send = s + len;
        const U8* x = first_variant;
        STRLEN outlen = first_variant - s;

        while (x < send) {
            const STRLEN cur_len = isSTRICT_UTF8_CHAR(x, send);
            if (UNLIKELY(! cur_len)) {
                break;
            }
            x += cur_len;
            outlen++;
        }

        if (el)
            *el = outlen;

        if (ep) {
            *ep = x;
        }

        return (x == send);
    }
}

#define is_c9strict_utf8_string_loc(s, len, ep)	                            \
                            is_c9strict_utf8_string_loclen(s, len, ep, 0)

PERL_STATIC_INLINE bool
Perl_is_c9strict_utf8_string_loclen(const U8 *s, STRLEN len, const U8 **ep, STRLEN *el)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_C9STRICT_UTF8_STRING_LOCLEN;

    if (len == 0) {
        len = strlen((const char *) s);
    }

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        if (el)
            *el = len;

        if (ep) {
            *ep = s + len;
        }

        return TRUE;
    }

    {
        const U8* const send = s + len;
        const U8* x = first_variant;
        STRLEN outlen = first_variant - s;

        while (x < send) {
            const STRLEN cur_len = isC9_STRICT_UTF8_CHAR(x, send);
            if (UNLIKELY(! cur_len)) {
                break;
            }
            x += cur_len;
            outlen++;
        }

        if (el)
            *el = outlen;

        if (ep) {
            *ep = x;
        }

        return (x == send);
    }
}

#define is_utf8_string_loc_flags(s, len, ep, flags)                         \
                        is_utf8_string_loclen_flags(s, len, ep, 0, flags)


/* The above 3 actual functions could have been moved into the more general one
 * just below, and made #defines that call it with the right 'flags'.  They are
 * currently kept separate to increase their chances of getting inlined */

PERL_STATIC_INLINE bool
Perl_is_utf8_string_loclen_flags(const U8 *s, STRLEN len, const U8 **ep, STRLEN *el, const U32 flags)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_UTF8_STRING_LOCLEN_FLAGS;
    assert(0 == (flags & ~UTF8_DISALLOW_ILLEGAL_INTERCHANGE));

    if (flags == 0) {
        return is_utf8_string_loclen(s, len, ep, el);
    }

    if ((flags & UTF8_DISALLOW_ILLEGAL_INTERCHANGE)
                                        == UTF8_DISALLOW_ILLEGAL_INTERCHANGE)
    {
        return is_strict_utf8_string_loclen(s, len, ep, el);
    }

    if ((flags & UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE)
                                    == UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE)
    {
        return is_c9strict_utf8_string_loclen(s, len, ep, el);
    }

    if (len == 0) {
        len = strlen((const char *) s);
    }

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        if (el)
            *el = len;

        if (ep) {
            *ep = s + len;
        }

        return TRUE;
    }

    {
        const U8* send = s + len;
        const U8* x = first_variant;
        STRLEN outlen = first_variant - s;

        while (x < send) {
            const STRLEN cur_len = isUTF8_CHAR_flags(x, send, flags);
            if (UNLIKELY(! cur_len)) {
                break;
            }
            x += cur_len;
            outlen++;
        }

        if (el)
            *el = outlen;

        if (ep) {
            *ep = x;
        }

        return (x == send);
    }
}

/*
=for apidoc utf8_distance

Returns the number of UTF-8 characters between the UTF-8 pointers C<a>
and C<b>.

WARNING: use only if you *know* that the pointers point inside the
same UTF-8 buffer.

=cut
*/

PERL_STATIC_INLINE IV
Perl_utf8_distance(pTHX_ const U8 *a, const U8 *b)
{
    PERL_ARGS_ASSERT_UTF8_DISTANCE;

    return (a < b) ? -1 * (IV) utf8_length(a, b) : (IV) utf8_length(b, a);
}

/*
=for apidoc utf8_hop

Return the UTF-8 pointer C<s> displaced by C<off> characters, either
forward (if C<off> is positive) or backward (if negative).  C<s> does not need
to be pointing to the starting byte of a character.  If it isn't, one count of
C<off> will be used up to get to the start of the next character for forward
hops, and to the start of the current character for negative ones.

WARNING: Prefer L</utf8_hop_safe> to this one.

Do NOT use this function unless you B<know> C<off> is within
the UTF-8 data pointed to by C<s> B<and> that on entry C<s> is aligned
on the first byte of a character or just after the last byte of a character.

=cut
*/

PERL_STATIC_INLINE U8 *
Perl_utf8_hop(const U8 *s, SSize_t off)
{
    PERL_ARGS_ASSERT_UTF8_HOP;

    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g
     * the XXX bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    if (off > 0) {

        /* Get to next non-continuation byte */
        if (UNLIKELY(UTF8_IS_CONTINUATION(*s))) {
            do {
                s++;
            }
            while (UTF8_IS_CONTINUATION(*s));
            off--;
        }

        while (off--)
            s += UTF8SKIP(s);
    }
    else {
        while (off++) {
            s--;
            while (UTF8_IS_CONTINUATION(*s))
                s--;
        }
    }

    GCC_DIAG_IGNORE(-Wcast-qual)
    return (U8 *)s;
    GCC_DIAG_RESTORE
}

/*
=for apidoc      utf8_hop_forward
=for apidoc_item utf8_hop_forward_overshoot

These each take as input a position, C<s0>, into a string encoded as UTF-8
which ends at the byte before C<end>, and return the position within it that is
C<s0> displaced by up to C<off> characters forwards.

If there are fewer than C<off> characters between C<s0> and C<end>, the
functions return C<end>.

The functions differ in two ways

=over 4

=item *

C<utf8_hop_forward_overshoot> can return how many characters beyond the edge
the request was for.  When its parameter, C<&remaining>, is not NULL, the
function stores into it the count of the excess; zero if the request was
completely fulfilled.  The actual number of characters that were displaced can
then be calculated as S<C<off - remaining>>.

=item *

C<utf8_hop_forward> will panic if called with C<s0> already positioned at or
beyond the edge of the string ending at C<end> and the request is to go even
further over the edge.  C<utf8_hop_forward_overshoot> presumes the caller will
handle any errors, and just stores C<off> into C<remaining> without doing
anything else.

=back

(The above contains a slight lie.  When C<remaining> is NULL, the two functions
act identically.)

C<s0> does not need to be pointing to the starting byte of a character.  If it
isn't, one count of C<off> will be used up to get to that start.

C<off> must be non-negative, and if zero, no action is taken; C<s0> is returned
unchanged.

=cut
*/
# define Perl_utf8_hop_forward(          s, off, end)           \
         Perl_utf8_hop_forward_overshoot(s, off, end, NULL)

PERL_STATIC_INLINE U8 *
Perl_utf8_hop_forward_overshoot(const U8 * s, SSize_t off,
                                const U8 * const end, SSize_t *remaining)
{
    PERL_ARGS_ASSERT_UTF8_HOP_FORWARD_OVERSHOOT;
    assert(off >= 0);

    if (off != 0) {
        if (UNLIKELY(s >= end && ! remaining)) {
            Perl_croak_nocontext("panic: Start of forward hop (0x%p) is %zd"
                                 " bytes beyond legal end position (0x%p)",
                                 s, 1 + s - end, end);
        }

        if (UNLIKELY(UTF8_IS_CONTINUATION(*s))) {
            do {    /* Get to next non-continuation byte */
                if (! UTF8_IS_CONTINUATION(*s)) {
                    off--;
                    break;
                }
                s++;
            } while (s < end);
        }

        while (off > 0 && s < end) {
            STRLEN skip = UTF8SKIP(s);

            /* Quit without counting this character if it overshoots the edge.
             * */
            if ((STRLEN)(end - s) < skip) {
                s = end;
                break;
            }

            s += skip;
            off--;
        }
    }

    if (remaining) {
        *remaining = off;
    }

    GCC_DIAG_IGNORE(-Wcast-qual)
    return (U8 *)s;
    GCC_DIAG_RESTORE
}

/*
=for apidoc      utf8_hop_back
=for apidoc_item utf8_hop_back_overshoot

These each take as input a string encoded as UTF-8 which starts at C<start>,
and a position into it given by C<s>, and return the position within it that is
C<s> displaced by up to C<off> characters backwards.

If there are fewer than C<off> characters between C<start> and C<s>, the
functions return C<start>.

The functions differ in that C<utf8_hop_back_overshoot> can return how many
characters C<off> beyond the edge the request was for.  When its parameter,
C<&remaining>, is not NULL, the function stores into it the count of the
excess; zero if the request was completely fulfilled.  The actual number of
characters that were displaced can then be calculated as S<C<off - remaining>>.
This function acts identically to plain C<utf8_hop_back> when this parameter is
NULL.

C<s> does not need to be pointing to the starting byte of a character.  If it
isn't, one count of C<off> will be used up to get to that start.

C<off> must be non-positive, and if zero, no action is taken; C<s> is returned
unchanged.  That it otherwise must be negative means that the earlier
description is a lie, to avoid burdening you with this detail too soon.  An
C<off> of C<-2> means to displace two characters backwards, so the displacement
is actually the absolute value of C<off>.  C<remaining> will also be
non-positive.  If there was only one character between C<start> and C<s>, and a
displacement of C<-2> was requested, C<remaining> would be set to C<-1>.  The
subtraction formula works, yielding the result that only C<-1> character was
displaced.

=cut
*/

# define Perl_utf8_hop_back(          s, off, start)            \
         Perl_utf8_hop_back_overshoot(s, off, start, NULL)

PERL_STATIC_INLINE U8 *
Perl_utf8_hop_back_overshoot(const U8 *s, SSize_t off,
                             const U8 * const start, SSize_t *remaining)
{
    PERL_ARGS_ASSERT_UTF8_HOP_BACK_OVERSHOOT;
    assert(start <= s);
    assert(off <= 0);

    /* Note: if we know that the input is well-formed, we can do per-word
     * hop-back.  Commit d6ad3b72778369a84a215b498d8d60d5b03aa1af implemented
     * that.  But it was reverted because doing per-word has some
     * start-up/tear-down overhead, so only makes sense if the distance to be
     * moved is large, and core perl doesn't currently move more than a few
     * characters at a time.  You can reinstate it if it does become
     * advantageous. */
    while (off < 0 && s > start) {
        do {    /* Find the beginning of this character */
            s--;
            if (! UTF8_IS_CONTINUATION(*s)) {
                off++;
                break;
            }
        } while (s > start);
    }

    if (remaining) {
        *remaining = off;
    }

    GCC_DIAG_IGNORE(-Wcast-qual)
    return (U8 *)s;
    GCC_DIAG_RESTORE
}

/*
=for apidoc      utf8_hop_safe
=for apidoc_item utf8_hop_overshoot

These each take as input a string encoded as UTF-8 which starts at C<start>,
ending at C<end>, and a position into it given by C<s>, and return the
position within it that is C<s> displaced by up to C<off> characters, either
forwards if C<off> is positive, or backwards if C<off> is negative.  (Nothing
is done if C<off> is 0.)

If there are fewer than C<off> characters between C<s> and the respective edge,
the functions return that edge.

The functions differ in that C<utf8_hop_overshoot> can return how many
characters beyond the edge the request was for.  When its parameter,
C<&remaining>, is not NULL, the function stores into it the count of the
excess; zero if the request was completely fulfilled.  The actual number of
characters that were displaced can then be calculated as S<C<off - remaining>>.
This function acts identically to plain C<utf8_hop_safe> when this parameter is
NULL.

C<s> does not need to be pointing to the starting byte of a character.  If it
isn't, one count of C<off> will be used up to get to that start.

To be more precise, the displacement is by the absolute value of C<off>, and
the excess count is the absolute value of C<remaining>.

=cut
*/

#define Perl_utf8_hop_safe(s, o, b, e)  Perl_utf8_hop_overshoot(s, o, b, e, 0)

PERL_STATIC_INLINE U8 *
Perl_utf8_hop_overshoot(const U8 *s, SSize_t off,
                   const U8 * const start, const U8 * const end,
                   SSize_t * remaining)
{
    PERL_ARGS_ASSERT_UTF8_HOP_OVERSHOOT;

    assert(start <= s && s <= end);

    if (off >= 0) {
        return utf8_hop_forward_overshoot(s, off, end, remaining);
    }
    else {
        return utf8_hop_back_overshoot(s, off, start, remaining);
    }
}

PERL_STATIC_INLINE STRLEN
Perl_isUTF8_CHAR_flags(const U8 * const s0, const U8 * const e, const U32 flags)
{
    PERL_ARGS_ASSERT_ISUTF8_CHAR_FLAGS;
    assert(0 == (flags & ~UTF8_DISALLOW_ILLEGAL_INTERCHANGE));

    PERL_IS_UTF8_CHAR_DFA(s0, e, PL_extended_utf8_dfa_tab,
                          goto check_success,
                          DFA_TEASE_APART_FF_,
                          DFA_RETURN_FAILURE_);

  check_success:

    return is_utf8_char_helper_(s0, e, flags);

#ifdef HAS_EXTRA_LONG_UTF8

  tease_apart_FF:

    /* In the case of PL_extended_utf8_dfa_tab, getting here means the input is
     * either malformed, or was for the largest possible start byte, which
     * indicates perl extended UTF-8, well above the Unicode maximum */
    if (   *s0 != I8_TO_NATIVE_UTF8(0xFF)
        || (flags & (UTF8_DISALLOW_SUPER|UTF8_DISALLOW_PERL_EXTENDED)))
    {
        return 0;
    }

    /* Otherwise examine the sequence not inline */
    return is_utf8_FF_helper_(s0, e,
                              FALSE /* require full, not partial char */
                             );
#endif

}

/*

=for apidoc      is_utf8_valid_partial_char
=for apidoc_item is_utf8_valid_partial_char_flags

These each return FALSE if the sequence of bytes starting at C<s> and looking no
further than S<C<e - 1>> is the UTF-8 encoding for one or more code points.
That is, FALSE is returned if C<s> points to at least one entire UTF-8 encoded
character.

Otherwise, they return TRUE if there exists at least one non-empty sequence of
bytes that when appended to sequence C<s>, starting at position C<e> causes the
entire sequence to be the well-formed UTF-8 of some code point

In other words they return TRUE if C<s> points to an incomplete UTF-8-encoded
code point; FALSE otherwise.

This is useful when a fixed-length buffer is being tested for being well-formed
UTF-8, but the final few bytes in it don't comprise a full character; that is,
it is split somewhere in the middle of the final code point's UTF-8
representation.  (Presumably when the buffer is refreshed with the next chunk
of data, the new first bytes will complete the partial code point.)   This
function is used to verify that the final bytes in the current buffer are in
fact the legal beginning of some code point, so that if they aren't, the
failure can be signalled without having to wait for the next read.

C<is_utf8_valid_partial_char> behaves identically to
C<is_utf8_valid_partial_char_flags> when the latter is called with a zero
C<flags> parameter.  This parameter is used to restrict the classes of code
points that are considered to be valid.  When zero, Perl's extended UTF-8 is
used.  Otherwise C<flags> can be any combination of the C<UTF8_DISALLOW_I<foo>>
flags accepted by C<L</utf8_to_uv>>.  If there is any sequence of bytes
that can complete the input partial character in such a way that a
non-prohibited character is formed, the function returns TRUE; otherwise FALSE.
Non-character code points cannot be determined based on partial character
input, so TRUE is always returned if C<s> looks like it could be the beginning
on one of those.  But many  of the other possible excluded types can be
determined from just the first one or two bytes.

=cut
*/
#define is_utf8_valid_partial_char(s, e)                                    \
                                is_utf8_valid_partial_char_flags(s, e, 0)

PERL_STATIC_INLINE bool
Perl_is_utf8_valid_partial_char_flags(const U8 * const s0, const U8 * const e, const U32 flags)
{
    PERL_ARGS_ASSERT_IS_UTF8_VALID_PARTIAL_CHAR_FLAGS;
    assert(0 == (flags & ~UTF8_DISALLOW_ILLEGAL_INTERCHANGE));

    PERL_IS_UTF8_CHAR_DFA(s0, e, PL_extended_utf8_dfa_tab,
                          DFA_RETURN_FAILURE_,
                          DFA_TEASE_APART_FF_,
                          NOOP);

    /* The NOOP above causes the DFA to drop down here iff the input was a
     * partial character.  flags=0 => can return TRUE immediately; otherwise we
     * need to check (not inline) if the partial character is the beginning of
     * a disallowed one */
    if (flags == 0) {
        return TRUE;
    }

    return cBOOL(is_utf8_char_helper_(s0, e, flags));

#ifdef HAS_EXTRA_LONG_UTF8

  tease_apart_FF:

    /* Getting here means the input is either malformed, or, in the case of
     * PL_extended_utf8_dfa_tab, was for the largest possible start byte.  The
     * latter case has to be extended UTF-8, so can fail immediately if that is
     * forbidden */

    if (   *s0 != I8_TO_NATIVE_UTF8(0xFF)
        || (flags & (UTF8_DISALLOW_SUPER|UTF8_DISALLOW_PERL_EXTENDED)))
    {
        return FALSE;
    }

    return is_utf8_FF_helper_(s0, e,
                              TRUE /* Require to be a partial character */
                             );
#endif

}

/*

=for apidoc      is_utf8_fixed_width_buf_flags
=for apidoc_item is_utf8_fixed_width_buf_loc_flags
=for apidoc_item is_utf8_fixed_width_buf_loclen_flags

These each return TRUE if the fixed-width buffer starting at C<s> with length
C<len> is entirely valid UTF-8, subject to the restrictions given by C<flags>;
otherwise they return FALSE.

If C<flags> is 0, any well-formed UTF-8, as extended by Perl, is accepted
without restriction.  If the final few bytes of the buffer do not form a
complete code point, this will return TRUE anyway, provided that
C<L</is_utf8_valid_partial_char_flags>> returns TRUE for them.

C<flags> can be zero or any combination of the C<UTF8_DISALLOW_I<foo>> flags
accepted by C<L</utf8_to_uv>>, and with the same meanings.

The functions differ from C<L</is_utf8_string_flags>> only in that the latter
returns FALSE if the final few bytes of the string don't form a complete code
point.

C<is_utf8_fixed_width_buf_loc_flags>> does all the preceding, but takes an
extra parameter, C<ep> into which it stores the location of the failure, if
C<ep> is not NULL.  If instead the function returns TRUE, C<*ep> will point to
the beginning of any partial character at the end of the buffer; if there is no
partial character C<*ep> will contain C<s>+C<len>.

C<is_utf8_fixed_width_buf_loclen_flags>> does all the preceding, but takes
another extra parameter, C<el> into which it stores the number of complete,
valid characters found, if C<el> is not NULL.

=cut
 */
#define is_utf8_fixed_width_buf_flags(s, len, flags)                        \
                is_utf8_fixed_width_buf_loclen_flags(s, len, 0, 0, flags)

#define is_utf8_fixed_width_buf_loc_flags(s, len, loc, flags)               \
                is_utf8_fixed_width_buf_loclen_flags(s, len, loc, 0, flags)

PERL_STATIC_INLINE bool
Perl_is_utf8_fixed_width_buf_loclen_flags(const U8 * const s,
                                       STRLEN len,
                                       const U8 **ep,
                                       STRLEN *el,
                                       const U32 flags)
{
    const U8 * maybe_partial;

    PERL_ARGS_ASSERT_IS_UTF8_FIXED_WIDTH_BUF_LOCLEN_FLAGS;

    if (! ep) {
        ep  = &maybe_partial;
    }

    /* If it's entirely valid, return that; otherwise see if the only error is
     * that the final few bytes are for a partial character */
    return    is_utf8_string_loclen_flags(s, len, ep, el, flags)
           || is_utf8_valid_partial_char_flags(*ep, s + len, flags);
}

PERL_STATIC_INLINE bool
Perl_utf8_to_uv_msgs(const U8 * const s0,
                     const U8 * const e,
                     UV * cp_p,
                     Size_t *advance_p,
                     U32 flags,
                     U32 * errors,
                     AV ** msgs)
{
    PERL_ARGS_ASSERT_UTF8_TO_UV_MSGS;

    /* This is the inlined portion of utf8_to_uv_msgs.  It handles the simple
     * cases, and, if necessary calls a helper function to deal with the more
     * complex ones.  Almost all well-formed non-problematic code points are
     * considered simple, so that it's unlikely that the helper function will
     * need to be called. */

    /* Assume that isn't malformed; the vast majority of calls won't be */
    if (errors) {
        *errors = 0;
    }
    if (msgs) {
        *msgs = NULL;
    }


    /* No calls from core pass in an empty string; non-core need a check */
#ifdef PERL_CORE
    assert(e > s0);
#else
    if (LIKELY(e > s0))
#endif

    {
        /* UTF-8 invariants are returned unchanged.  The code below is quite
         * capable of handling this, but this shortcuts this very common case
         * */
        if (UTF8_IS_INVARIANT(*s0)) {
            if (advance_p) {
                *advance_p = 1;
            }

            *cp_p = *s0;
            return true;
        }

        const U8 * s = s0;

        /* This dfa is fast.  If it accepts the input, it was for a
         * well-formed, non-problematic code point, which can be returned
         * immediately.  Otherwise we call a helper function to figure out the
         * more complicated cases.
         *
         * It is an adaptation of the tables and algorithm given in
         * https://bjoern.hoehrmann.de/utf-8/decoder/dfa/, which provides
         * comprehensive documentation of the original version.  A copyright
         * notice for the original version is given at the beginning of this
         * file.  The Perl adaptation is documented at the definition of
         * PL_strict_utf8_dfa_tab[].
         *
         * The terminology of the dfa refers to a 'class'.  The variable 'type'
         * would have been named 'class' except that is a reserved word in C++
         * 
         * The table can be a U16 on EBCDIC platforms, so 'state' is declared
         * as U16; 'type' is likely to never occupy more than 5 bits.  */
        PERL_UINT_FAST8_T type = PL_strict_utf8_dfa_tab[*s];
        PERL_UINT_FAST16_T state = PL_strict_utf8_dfa_tab[256 + type];
        UV uv = (0xff >> type) & NATIVE_UTF8_TO_I8(*s);

        while (state > 1 && ++s < e) {
            type  = PL_strict_utf8_dfa_tab[*s];
            state = PL_strict_utf8_dfa_tab[256 + state + type];

            uv = UTF8_ACCUMULATE(uv, *s);
        }

        if (LIKELY(state == 0)) {
            if (advance_p) {
                *advance_p = s - s0 + 1;
            }

            *cp_p = UNI_TO_NATIVE(uv);
            return true;
        }
    }

    /* Here is potentially problematic.  Use the full mechanism */
    return utf8_to_uv_msgs_helper_(s0, e, cp_p, advance_p, flags, errors, msgs);
}

PERL_STATIC_INLINE UV
Perl_utf8_to_uv_or_die(const U8 *s, const U8 *e, STRLEN *advance_p)
{
    PERL_ARGS_ASSERT_UTF8_TO_UV_OR_DIE;

    UV cp;
    (void) utf8_to_uv_flags(s, e, &cp, advance_p, UTF8_DIE_IF_MALFORMED);
    return cp;
}

PERL_STATIC_INLINE UV
Perl_utf8n_to_uvchr_msgs(const U8 * const s0,
                         STRLEN curlen,
                         STRLEN *retlen,
                         U32 flags,
                         U32 * errors,
                         AV ** msgs)
{
    PERL_ARGS_ASSERT_UTF8N_TO_UVCHR_MSGS;

    UV cp;
    if (LIKELY(utf8_to_uv_msgs(s0, s0 + curlen, &cp, retlen, flags, errors,
                                                                        msgs)))
    {
        return cp;
    }

    if ((flags & UTF8_CHECK_ONLY) && retlen) {
        *retlen = ((STRLEN) -1);
    }

    return 0;
}


PERL_STATIC_INLINE UV
Perl_utf8_to_uvchr_buf(pTHX_ const U8 *s, const U8 *send, STRLEN *retlen)
{
    PERL_ARGS_ASSERT_UTF8_TO_UVCHR_BUF;

    UV cp;

    /* When everything is legal, just return that; but when not:
     *  1) if warnings are enabled return 0 and retlen to -1
     *  2) if warnings are disabled, set 'flags' to accept any malformation,
     *     but that will just cause the REPLACEMENT CHARACTER to be returned,
     *     as the documentation indicates.  EMPTY is not really allowed, and
     *     asserts on debugging builds.  But on non-debugging we have to deal
     *     with it.
     * This API means 0 can mean a legal NUL, or the input is malformed; and
     * the caller has to know if warnings are disabled to know if it can rely on
     * 'retlen'.  Best to use utf8_to_uv() instead */
    U32 flags = (ckWARN_d(WARN_UTF8)) ? 0 : (UTF8_ALLOW_ANY | UTF8_ALLOW_EMPTY);

    if (   LIKELY(utf8_to_uv_flags(s, send, &cp, retlen, flags))
        || flags)
    {
        return cp;
    }

    if (retlen) {
        *retlen = (STRLEN) -1;
    }

    return 0;
}

PERL_STATIC_INLINE U8 *
Perl_uv_to_utf8(pTHX_ U8 *d, UV uv)
{
    return uv_to_utf8_msgs(d, uv, 0, 0);
}

PERL_STATIC_INLINE U8 *
Perl_uv_to_utf8_flags(pTHX_ U8 *d, UV uv, UV flags)
{
    return uv_to_utf8_msgs(d, uv, flags, 0);
}

PERL_STATIC_INLINE U8 *
Perl_uv_to_utf8_msgs(pTHX_ U8 *d, UV uv, UV flags , HV **msgs)
{
    return uvoffuni_to_utf8_flags_msgs(d, NATIVE_TO_UNI(uv), flags, msgs);
}

/* ------------------------------- perl.h ----------------------------- */

/*
=for apidoc_section $utility

=for apidoc is_safe_syscall

Test that the given C<pv> (with length C<len>) doesn't contain any internal
C<NUL> characters.
If it does, set C<errno> to C<ENOENT>, optionally warn using the C<syscalls>
category, and return FALSE.

Return TRUE if the name is safe.

C<what> and C<op_name> are used in any warning.

Used by the C<IS_SAFE_SYSCALL()> macro.

=cut
*/

PERL_STATIC_INLINE bool
Perl_is_safe_syscall(pTHX_ const char *pv, STRLEN len, const char *what, const char *op_name)
{
    /* While the Windows CE API provides only UCS-16 (or UTF-16) APIs
     * perl itself uses xce*() functions which accept 8-bit strings.
     */

    PERL_ARGS_ASSERT_IS_SAFE_SYSCALL;

    if (len > 1) {
        char *null_at;
        if (UNLIKELY((null_at = (char *)memchr(pv, 0, len-1)) != NULL)) {
                SETERRNO(ENOENT, LIB_INVARG);
                Perl_ck_warner(aTHX_ packWARN(WARN_SYSCALLS),
                                   "Invalid \\0 character in %s for %s: %s\\0%s",
                                   what, op_name, pv, null_at+1);
                return FALSE;
        }
    }

    return TRUE;
}

/*

Return true if the supplied filename has a newline character
immediately before the first (hopefully only) NUL.

My original look at this incorrectly used the len from SvPV(), but
that's incorrect, since we allow for a NUL in pv[len-1].

So instead, strlen() and work from there.

This allow for the user reading a filename, forgetting to chomp it,
then calling:

  open my $foo, "$file\0";

*/

#ifdef PERL_CORE

PERL_STATIC_INLINE bool
S_should_warn_nl(const char *pv)
{
    STRLEN len;

    PERL_ARGS_ASSERT_SHOULD_WARN_NL;

    len = strlen(pv);

    return len > 0 && pv[len-1] == '\n';
}

#endif

#if defined(PERL_IN_PP_C) || defined(PERL_IN_PP_HOT_C)

PERL_STATIC_INLINE bool
S_lossless_NV_to_IV(const NV nv, IV *ivp)
{
    /* This function determines if the input NV 'nv' may be converted without
     * loss of data to an IV.  If not, it returns FALSE taking no other action.
     * But if it is possible, it does the conversion, returning TRUE, and
     * storing the converted result in '*ivp' */

    PERL_ARGS_ASSERT_LOSSLESS_NV_TO_IV;

#  if defined(NAN_COMPARE_BROKEN) && defined(Perl_isnan)
    /* Normally any comparison with a NaN returns false; if we can't rely
     * on that behaviour, check explicitly */
    if (UNLIKELY(Perl_isnan(nv))) {
        return FALSE;
    }
#  endif

#  ifndef NV_PRESERVES_UV
    STATIC_ASSERT_STMT(((UV)1 << NV_PRESERVES_UV_BITS) - 1 <= (UV)IV_MAX);
#  endif

    /* Written this way so that with an always-false NaN comparison we
     * return false */
    if (
#  ifdef NV_PRESERVES_UV
        LIKELY(nv >= (NV) IV_MIN) && LIKELY(nv < IV_MAX_P1) &&
#  else
        /* If the condition below is not satisfied, lower bits of nv's
         * integral part is already lost and accurate conversion to integer
         * is impossible.
         * Note this should be consistent with S_sv_2iuv_common in sv.c. */
        Perl_fabs(nv) < (NV) ((UV)1 << NV_PRESERVES_UV_BITS) &&
#  endif
        (IV) nv == nv) {
        *ivp = (IV) nv;
        return TRUE;
    }
    return FALSE;
}

#endif

/* ------------------ pp.c, regcomp.c, toke.c, universal.c ------------ */

#if defined(PERL_IN_PP_C) || defined(PERL_IN_REGCOMP_ANY) || defined(PERL_IN_TOKE_C) || defined(PERL_IN_UNIVERSAL_C)

#define MAX_CHARSET_NAME_LENGTH 2

PERL_STATIC_INLINE const char *
S_get_regex_charset_name(const U32 flags, STRLEN* const lenp)
{
    PERL_ARGS_ASSERT_GET_REGEX_CHARSET_NAME;

    /* Returns a string that corresponds to the name of the regex character set
     * given by 'flags', and *lenp is set the length of that string, which
     * cannot exceed MAX_CHARSET_NAME_LENGTH characters */

    *lenp = 1;
    switch (get_regex_charset(flags)) {
        case REGEX_DEPENDS_CHARSET: return DEPENDS_PAT_MODS;
        case REGEX_LOCALE_CHARSET:  return LOCALE_PAT_MODS;
        case REGEX_UNICODE_CHARSET: return UNICODE_PAT_MODS;
        case REGEX_ASCII_RESTRICTED_CHARSET: return ASCII_RESTRICT_PAT_MODS;
        case REGEX_ASCII_MORE_RESTRICTED_CHARSET:
            *lenp = 2;
            return ASCII_MORE_RESTRICT_PAT_MODS;
    }
    /* The NOT_REACHED; hides an assert() which has a rather complex
     * definition in perl.h. */
    NOT_REACHED; /* NOTREACHED */
    return "?";	    /* Unknown */
}

#endif

/*

Return false if any get magic is on the SV other than taint magic.

*/

PERL_STATIC_INLINE bool
Perl_sv_only_taint_gmagic(SV *sv)
{
    MAGIC *mg = SvMAGIC(sv);

    PERL_ARGS_ASSERT_SV_ONLY_TAINT_GMAGIC;

    while (mg) {
        if (mg->mg_type != PERL_MAGIC_taint
            && !(mg->mg_flags & MGf_GSKIP)
            && mg->mg_virtual->svt_get) {
            return FALSE;
        }
        mg = mg->mg_moremagic;
    }

    return TRUE;
}

/* ------------------ cop.h ------------------------------------------- */

/* implement GIMME_V() macro */

PERL_STATIC_INLINE U8
Perl_gimme_V(pTHX)
{
    I32 cxix;
    U8  gimme = (PL_op->op_flags & OPf_WANT);

    if (gimme)
        return gimme;
    cxix = PL_curstackinfo->si_cxsubix;
    if (cxix < 0)
        return PL_curstackinfo->si_type == PERLSI_SORT ? G_SCALAR: G_VOID;
    assert(cxstack[cxix].blk_gimme & G_WANT);
    return (cxstack[cxix].blk_gimme & G_WANT);
}


/* Enter a block. Push a new base context and return its address. */

PERL_STATIC_INLINE PERL_CONTEXT *
Perl_cx_pushblock(pTHX_ U8 type, U8 gimme, SV** sp, I32 saveix)
{
    PERL_CONTEXT * cx;

    PERL_ARGS_ASSERT_CX_PUSHBLOCK;

    CXINC;
    cx = CX_CUR();
    cx->cx_type        = type;
    cx->blk_gimme      = gimme;
    cx->blk_oldsaveix  = saveix;
    cx->blk_oldsp      = (Stack_off_t)(sp - PL_stack_base);
    assert(cxstack_ix <= 0
            || CxTYPE(cx-1) == CXt_SUBST
            || cx->blk_oldsp >= (cx-1)->blk_oldsp);
    cx->blk_oldcop     = PL_curcop;
    cx->blk_oldmarksp  = (I32)(PL_markstack_ptr - PL_markstack);
    cx->blk_oldscopesp = PL_scopestack_ix;
    cx->blk_oldpm      = PL_curpm;
    cx->blk_old_tmpsfloor = PL_tmps_floor;

    PL_tmps_floor        = PL_tmps_ix;
    CX_DEBUG(cx, "PUSH");
    return cx;
}


/* Exit a block (RETURN and LAST). */

PERL_STATIC_INLINE void
Perl_cx_popblock(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_POPBLOCK;

    CX_DEBUG(cx, "POP");
    /* these 3 are common to cx_popblock and cx_topblock */
    PL_markstack_ptr = PL_markstack + cx->blk_oldmarksp;
    PL_scopestack_ix = cx->blk_oldscopesp;
    PL_curpm         = cx->blk_oldpm;

    /* LEAVE_SCOPE() should have made this true. /(?{})/ cheats
     * and leaves a CX entry lying around for repeated use, so
     * skip for multicall */                  \
    assert(   (CxTYPE(cx) == CXt_SUB && CxMULTICALL(cx))
            || PL_savestack_ix == cx->blk_oldsaveix);
    PL_curcop     = cx->blk_oldcop;
    PL_tmps_floor = cx->blk_old_tmpsfloor;
}

/* Continue a block elsewhere (e.g. NEXT, REDO, GOTO).
 * Whereas cx_popblock() restores the state to the point just before
 * cx_pushblock() was called,  cx_topblock() restores it to the point just
 * *after* cx_pushblock() was called. */

PERL_STATIC_INLINE void
Perl_cx_topblock(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_TOPBLOCK;

    CX_DEBUG(cx, "TOP");
    /* these 3 are common to cx_popblock and cx_topblock */
    PL_markstack_ptr = PL_markstack + cx->blk_oldmarksp;
    PL_scopestack_ix = cx->blk_oldscopesp;
    PL_curpm         = cx->blk_oldpm;
    Perl_rpp_popfree_to(aTHX_ PL_stack_base + cx->blk_oldsp);
}


PERL_STATIC_INLINE void
Perl_cx_pushsub(pTHX_ PERL_CONTEXT *cx, CV *cv, OP *retop, bool hasargs)
{
    U8 phlags = CX_PUSHSUB_GET_LVALUE_MASK(Perl_was_lvalue_sub);

    PERL_ARGS_ASSERT_CX_PUSHSUB;

    PERL_DTRACE_PROBE_ENTRY(cv);
    cx->blk_sub.old_cxsubix     = PL_curstackinfo->si_cxsubix;
    PL_curstackinfo->si_cxsubix = (I32)(cx - PL_curstackinfo->si_cxstack);
    cx->blk_sub.cv = cv;
    cx->blk_sub.olddepth = CvDEPTH(cv);
    cx->blk_sub.prevcomppad = PL_comppad;
    cx->cx_type |= (hasargs) ? CXp_HASARGS : 0;
    cx->blk_sub.retop = retop;
    SvREFCNT_inc_simple_void_NN(cv);
    cx->blk_u16 = PL_op->op_private & (phlags|OPpDEREF);
}


/* subsets of cx_popsub() */

PERL_STATIC_INLINE void
Perl_cx_popsub_common(pTHX_ PERL_CONTEXT *cx)
{
    CV *cv;

    PERL_ARGS_ASSERT_CX_POPSUB_COMMON;
    assert(CxTYPE(cx) == CXt_SUB);

    PL_comppad = cx->blk_sub.prevcomppad;
    PL_curpad = LIKELY(PL_comppad != NULL) ? AvARRAY(PL_comppad) : NULL;
    cv = cx->blk_sub.cv;
    CvDEPTH(cv) = cx->blk_sub.olddepth;
    cx->blk_sub.cv = NULL;
    SvREFCNT_dec(cv);
    PL_curstackinfo->si_cxsubix = cx->blk_sub.old_cxsubix;
}


/* handle the @_ part of leaving a sub */

PERL_STATIC_INLINE void
Perl_cx_popsub_args(pTHX_ PERL_CONTEXT *cx)
{
    AV *av;

    PERL_ARGS_ASSERT_CX_POPSUB_ARGS;
    assert(CxTYPE(cx) == CXt_SUB);
    assert(AvARRAY(MUTABLE_AV(
        PadlistARRAY(CvPADLIST(cx->blk_sub.cv))[
                CvDEPTH(cx->blk_sub.cv)])) == PL_curpad);

    CX_POP_SAVEARRAY(cx);
    av = MUTABLE_AV(PAD_SVl(0));
    if (!SvMAGICAL(av) && SvREFCNT(av) == 1
#ifndef PERL_RC_STACK
        && !AvREAL(av)
#endif
    )
        clear_defarray_simple(av);
    else
        /* abandon @_ if it got reified */
        clear_defarray(av, 0);
}


PERL_STATIC_INLINE void
Perl_cx_popsub(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_POPSUB;
    assert(CxTYPE(cx) == CXt_SUB);

    PERL_DTRACE_PROBE_RETURN(cx->blk_sub.cv);

    if (CxHASARGS(cx))
        cx_popsub_args(cx);
    cx_popsub_common(cx);
}


PERL_STATIC_INLINE void
Perl_cx_pushformat(pTHX_ PERL_CONTEXT *cx, CV *cv, OP *retop, GV *gv)
{
    PERL_ARGS_ASSERT_CX_PUSHFORMAT;

    cx->blk_format.old_cxsubix = PL_curstackinfo->si_cxsubix;
    PL_curstackinfo->si_cxsubix= (I32)(cx - PL_curstackinfo->si_cxstack);
    cx->blk_format.cv          = cv;
    cx->blk_format.retop       = retop;
    cx->blk_format.gv          = gv;
    cx->blk_format.dfoutgv     = PL_defoutgv;
    cx->blk_format.prevcomppad = PL_comppad;
    cx->blk_u16                = 0;

    SvREFCNT_inc_simple_void_NN(cv);
    CvDEPTH(cv)++;
    SvREFCNT_inc_void(cx->blk_format.dfoutgv);
}


PERL_STATIC_INLINE void
Perl_cx_popformat(pTHX_ PERL_CONTEXT *cx)
{
    CV *cv;
    GV *dfout;

    PERL_ARGS_ASSERT_CX_POPFORMAT;
    assert(CxTYPE(cx) == CXt_FORMAT);

    dfout = cx->blk_format.dfoutgv;
    setdefout(dfout);
    cx->blk_format.dfoutgv = NULL;
    SvREFCNT_dec_NN(dfout);

    PL_comppad = cx->blk_format.prevcomppad;
    PL_curpad = LIKELY(PL_comppad != NULL) ? AvARRAY(PL_comppad) : NULL;
    cv = cx->blk_format.cv;
    cx->blk_format.cv = NULL;
    --CvDEPTH(cv);
    SvREFCNT_dec_NN(cv);
    PL_curstackinfo->si_cxsubix = cx->blk_format.old_cxsubix;
}


PERL_STATIC_INLINE void
Perl_push_evalortry_common(pTHX_ PERL_CONTEXT *cx, OP *retop, SV *namesv)
{
    cx->blk_eval.retop         = retop;
    cx->blk_eval.old_namesv    = namesv;
    cx->blk_eval.old_eval_root = PL_eval_root;
    cx->blk_eval.cur_text      = PL_parser ? PL_parser->linestr : NULL;
    cx->blk_eval.cv            = NULL; /* later set by doeval_compile() */
    cx->blk_eval.cur_top_env   = PL_top_env;

    assert(!(PL_in_eval     & ~ 0x3F));
    assert(!(PL_op->op_type & ~0x1FF));
    cx->blk_u16 = (PL_in_eval & 0x3F) | ((U16)PL_op->op_type << 7);
}

PERL_STATIC_INLINE void
Perl_cx_pusheval(pTHX_ PERL_CONTEXT *cx, OP *retop, SV *namesv)
{
    PERL_ARGS_ASSERT_CX_PUSHEVAL;

    Perl_push_evalortry_common(aTHX_ cx, retop, namesv);

    cx->blk_eval.old_cxsubix    = PL_curstackinfo->si_cxsubix;
    PL_curstackinfo->si_cxsubix = (I32)(cx - PL_curstackinfo->si_cxstack);
}

PERL_STATIC_INLINE void
Perl_cx_pushtry(pTHX_ PERL_CONTEXT *cx, OP *retop)
{
    PERL_ARGS_ASSERT_CX_PUSHTRY;

    Perl_push_evalortry_common(aTHX_ cx, retop, NULL);

    /* Don't actually change it, just store the current value so it's restored
     * by the common popeval */
    cx->blk_eval.old_cxsubix = PL_curstackinfo->si_cxsubix;
}


PERL_STATIC_INLINE void
Perl_cx_popeval(pTHX_ PERL_CONTEXT *cx)
{
    SV *sv;

    PERL_ARGS_ASSERT_CX_POPEVAL;
    assert(CxTYPE(cx) == CXt_EVAL);

    PL_in_eval = CxOLD_IN_EVAL(cx);
    assert(!(PL_in_eval & 0xc0));
    PL_eval_root = cx->blk_eval.old_eval_root;
    sv = cx->blk_eval.cur_text;
    if (sv && CxEVAL_TXT_REFCNTED(cx)) {
        cx->blk_eval.cur_text = NULL;
        SvREFCNT_dec_NN(sv);
    }

    sv = cx->blk_eval.old_namesv;
    if (sv) {
        cx->blk_eval.old_namesv = NULL;
        SvREFCNT_dec_NN(sv);
    }
    PL_curstackinfo->si_cxsubix = cx->blk_eval.old_cxsubix;
}


/* push a plain loop, i.e.
 *     { block }
 *     while (cond) { block }
 *     for (init;cond;continue) { block }
 * This loop can be last/redo'ed etc.
 */

PERL_STATIC_INLINE void
Perl_cx_pushloop_plain(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_PUSHLOOP_PLAIN;
    cx->blk_loop.my_op = cLOOP;
}


/* push a true for loop, i.e.
 *     for var (list) { block }
 */

PERL_STATIC_INLINE void
Perl_cx_pushloop_for(pTHX_ PERL_CONTEXT *cx, void *itervarp, SV* itersave)
{
    PERL_ARGS_ASSERT_CX_PUSHLOOP_FOR;

    /* this one line is common with cx_pushloop_plain */
    cx->blk_loop.my_op = cLOOP;

    cx->blk_loop.itervar_u.svp = (SV**)itervarp;
    cx->blk_loop.itersave      = itersave;
#ifdef USE_ITHREADS
    cx->blk_loop.oldcomppad = PL_comppad;
#endif
}


/* pop all loop types, including plain */

PERL_STATIC_INLINE void
Perl_cx_poploop(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_POPLOOP;

    assert(CxTYPE_is_LOOP(cx));
    if (  CxTYPE(cx) == CXt_LOOP_ARY
       || CxTYPE(cx) == CXt_LOOP_LAZYSV)
    {
        /* Free ary or cur. This assumes that state_u.ary.ary
         * aligns with state_u.lazysv.cur. See cx_dup() */
        SV *sv = cx->blk_loop.state_u.lazysv.cur;
        cx->blk_loop.state_u.lazysv.cur = NULL;
        SvREFCNT_dec_NN(sv);
        if (CxTYPE(cx) == CXt_LOOP_LAZYSV) {
            sv = cx->blk_loop.state_u.lazysv.end;
            cx->blk_loop.state_u.lazysv.end = NULL;
            SvREFCNT_dec_NN(sv);
        }
    }
    if (cx->cx_type & (CXp_FOR_PAD|CXp_FOR_GV)) {
        SV *cursv;
        SV **svp = (cx)->blk_loop.itervar_u.svp;
        if ((cx->cx_type & CXp_FOR_GV))
            svp = &GvSV((GV*)svp);
        cursv = *svp;
        *svp = cx->blk_loop.itersave;
        cx->blk_loop.itersave = NULL;
        SvREFCNT_dec(cursv);
    }
    if (cx->cx_type & (CXp_FOR_GV|CXp_FOR_LVREF))
        SvREFCNT_dec(cx->blk_loop.itervar_u.svp);
}


PERL_STATIC_INLINE void
Perl_cx_pushwhen(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_PUSHWHEN;

    cx->blk_givwhen.leave_op = cLOGOP->op_other;
}


PERL_STATIC_INLINE void
Perl_cx_popwhen(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_POPWHEN;
    assert(CxTYPE(cx) == CXt_WHEN);

    PERL_UNUSED_ARG(cx);
    PERL_UNUSED_CONTEXT;
    /* currently NOOP */
}


PERL_STATIC_INLINE void
Perl_cx_pushgiven(pTHX_ PERL_CONTEXT *cx, SV *orig_defsv)
{
    PERL_ARGS_ASSERT_CX_PUSHGIVEN;

    cx->blk_givwhen.leave_op = cLOGOP->op_other;
    cx->blk_givwhen.defsv_save = orig_defsv;
}


PERL_STATIC_INLINE void
Perl_cx_popgiven(pTHX_ PERL_CONTEXT *cx)
{
    SV *sv;

    PERL_ARGS_ASSERT_CX_POPGIVEN;
    assert(CxTYPE(cx) == CXt_GIVEN);

    sv = GvSV(PL_defgv);
    GvSV(PL_defgv) = cx->blk_givwhen.defsv_save;
    cx->blk_givwhen.defsv_save = NULL;
    SvREFCNT_dec(sv);
}


/* Make @_ empty in-place in simple cases: a cheap av_clear().
 * See Perl_clear_defarray() for non-simple cases */


PERL_STATIC_INLINE void
Perl_clear_defarray_simple(pTHX_ AV *av)
{
    PERL_ARGS_ASSERT_CLEAR_DEFARRAY_SIMPLE;

    assert(SvTYPE(av) == SVt_PVAV);
    assert(!SvREADONLY(av));
    assert(!SvMAGICAL(av));
    assert(SvREFCNT(av) == 1);

#ifdef PERL_RC_STACK
    assert(AvREAL(av));
    /* this code assumes that destructors called here can't free av
     * itself, because pad[0] and/or CX pointers will keep it alive */
    SSize_t i = AvFILLp(av);
    while (i >= 0) {
        SV *sv = AvARRAY(av)[i];
        AvARRAY(av)[i--] = NULL;
        SvREFCNT_dec(sv);
    }
#else
    assert(!AvREAL(av));
#endif
    AvFILLp(av) = -1;
    Perl_av_remove_offset(aTHX_ av);
}

/* Switch to a different argument stack.
 *
 * Note that it doesn't update PL_curstackinfo->si_stack_nonrc_base,
 * so this should only be used as part of a general switching between
 * stackinfos.
 */

PERL_STATIC_INLINE void
Perl_switch_argstack(pTHX_ AV *to)
{
    PERL_ARGS_ASSERT_SWITCH_ARGSTACK;

    AvFILLp(PL_curstack) = PL_stack_sp - PL_stack_base;
    PL_stack_base = AvARRAY(to);
    PL_stack_max  = PL_stack_base + AvMAX(to);
    PL_stack_sp   = PL_stack_base + AvFILLp(to);
    PL_curstack   = to;
}


/* Push, and switch to a new stackinfo, allocating one if none are spare,
 * to get a fresh set of stacks.
 * Update all the interpreter variables like PL_curstackinfo,
 * PL_stack_sp, etc.
 * current flag meanings:
 *   1 make the new arg stack AvREAL
 */


PERL_STATIC_INLINE void
Perl_push_stackinfo(pTHX_ I32 type, UV flags)
{
    PERL_ARGS_ASSERT_PUSH_STACKINFO;

    PERL_SI *next = PL_curstackinfo->si_next;
    DEBUG_l({
        int i = 0; PERL_SI *p = PL_curstackinfo;
        while (p) { i++; p = p->si_prev; }
        Perl_deb(aTHX_ "push STACKINFO %d in %s at %s:%d\n",
                     i, SAFE_FUNCTION__, __FILE__, __LINE__);
    })

    if (!next) {
        next = new_stackinfo_flags(32, 2048/sizeof(PERL_CONTEXT) - 1, flags);
        next->si_prev = PL_curstackinfo;
        PL_curstackinfo->si_next = next;
    }
    next->si_type = type;
    next->si_cxix = -1;
    next->si_cxsubix = -1;
    PUSHSTACK_INIT_HWM(next);
#ifdef PERL_RC_STACK
    next->si_stack_nonrc_base = 0;
#endif
    if (flags & 1)
        AvREAL_on(next->si_stack);
    else
        AvREAL_off(next->si_stack);
    AvFILLp(next->si_stack) = 0;
    switch_argstack(next->si_stack);
    PL_curstackinfo = next;
    SET_MARK_OFFSET;
}


/* Pop, then switch to the previous stackinfo and set of stacks.
 * Update all the interpreter variables like PL_curstackinfo,
 * PL_stack_sp, etc. */

PERL_STATIC_INLINE void
Perl_pop_stackinfo(pTHX)
{
    PERL_ARGS_ASSERT_POP_STACKINFO;

    PERL_SI * const prev = PL_curstackinfo->si_prev;
    DEBUG_l({
        int i = -1; PERL_SI *p = PL_curstackinfo;
        while (p) { i++; p = p->si_prev; }
        Perl_deb(aTHX_ "pop  STACKINFO %d in %s at %s:%d\n",
                     i, SAFE_FUNCTION__, __FILE__, __LINE__);})
    if (!prev) {
        Perl_croak_popstack();
    }

    switch_argstack(prev->si_stack);
    /* don't free prev here, free them all at the END{} */
    PL_curstackinfo = prev;
}



/*
=for apidoc newPADxVOP

Constructs, checks and returns an op containing a pad offset.  C<type> is
the opcode, which should be one of C<OP_PADSV>, C<OP_PADAV>, C<OP_PADHV>
or C<OP_PADCV>.  The returned op will have the C<op_targ> field set by
the C<padix> argument.

This is convenient when constructing a large optree in nested function
calls, as it avoids needing to store the pad op directly to set the
C<op_targ> field as a side-effect. For example

    o = op_append_elem(OP_LINESEQ, o,
        newPADxVOP(OP_PADSV, 0, padix));

=cut
*/

PERL_STATIC_INLINE OP *
Perl_newPADxVOP(pTHX_ I32 type, I32 flags, PADOFFSET padix)
{
    PERL_ARGS_ASSERT_NEWPADXVOP;

    assert(type == OP_PADSV || type == OP_PADAV || type == OP_PADHV
            || type == OP_PADCV);
    OP *o = newOP(type, flags);
    o->op_targ = padix;
    return o;
}

/* ------------------ util.h ------------------------------------------- */

/*
=for apidoc_section $string

=for apidoc      foldEQ
=for apidoc_item foldEQ_locale

These each return true if the leading C<len> bytes of the strings C<s1> and
C<s2> are the same case-insensitively; false otherwise.

In C<foldEQ>, uppercase and lowercase ASCII range bytes match themselves and
their opposite case counterparts.  Non-cased and non-ASCII range bytes match
only themselves.

In C<foldEQ_locale>, the comparison is based on the current locale.
If that locale is UTF-8, the results are the same as C<foldEQ>, leading to
incorrect values for non-ASCII range code points.  Use C<L</foldEQ_utf8>>
instead.

=cut
*/

PERL_STATIC_INLINE I32
Perl_foldEQ(pTHX_ const char *s1, const char *s2, I32 len)
{
    PERL_UNUSED_CONTEXT;

    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;

    PERL_ARGS_ASSERT_FOLDEQ;

    assert(len >= 0);

    while (len--) {
        if (*a != *b && *a != PL_fold[*b])
            return 0;
        a++,b++;
    }
    return 1;
}

PERL_STATIC_INLINE I32
Perl_foldEQ_latin1(pTHX_ const char *s1, const char *s2, I32 len)
{
    /* Compare non-UTF-8 using Unicode (Latin1) semantics.  Works on all folds
     * representable without UTF-8, except for LATIN_SMALL_LETTER_SHARP_S, and
     * does not check for this.  Nor does it check that the strings each have
     * at least 'len' characters. */

    PERL_UNUSED_CONTEXT;

    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;

    PERL_ARGS_ASSERT_FOLDEQ_LATIN1;

    assert(len >= 0);

    while (len--) {
        if (*a != *b && *a != PL_fold_latin1[*b]) {
            return 0;
        }
        a++, b++;
    }
    return 1;
}

PERL_STATIC_INLINE I32
Perl_foldEQ_locale(pTHX_ const char *s1, const char *s2, I32 len)
{
    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;

    PERL_ARGS_ASSERT_FOLDEQ_LOCALE;

    assert(len >= 0);

    while (len--) {
        if (*a != *b && *a != PL_fold_locale[*b]) {
            DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                     "%s:%d: Our records indicate %02x is not a fold of %02x"
                     " or its mate %02x\n",
                     __FILE__, __LINE__, *a, *b, PL_fold_locale[*b]));

            return 0;
        }
        a++,b++;
    }
    return 1;
}

/*
=for apidoc_section $string
=for apidoc my_strnlen

The C library C<strnlen> if available, or a Perl implementation of it.

C<my_strnlen()> computes the length of the string, up to C<maxlen>
bytes.  It will never attempt to address more than C<maxlen>
bytes, making it suitable for use with strings that are not
guaranteed to be NUL-terminated.

=cut

Description stolen from http://man.openbsd.org/strnlen.3,
implementation stolen from PostgreSQL.
*/
#ifndef HAS_STRNLEN

PERL_STATIC_INLINE Size_t
Perl_my_strnlen(const char *str, Size_t maxlen)
{
    PERL_ARGS_ASSERT_MY_STRNLEN;

    const char *end = (const char *) memchr(str, '\0', maxlen);

    if (end == NULL) return maxlen;
    return end - str;
}

#endif

#if ! defined (HAS_MEMRCHR) && (defined(PERL_CORE) || defined(PERL_EXT))

PERL_STATIC_INLINE void *
S_my_memrchr(const char * s, const char c, const STRLEN len)
{
    /* memrchr(), since many platforms lack it */

    const char * t = s + len - 1;

    PERL_ARGS_ASSERT_MY_MEMRCHR;

    while (t >= s) {
        if (*t == c) {
            return (void *) t;
        }
        t--;
    }

    return NULL;
}

#endif

PERL_STATIC_INLINE char *
Perl_mortal_getenv(const char * str)
{
    /* This implements a (mostly) thread-safe, sequential-call-safe getenv().
     *
     * It's (mostly) thread-safe because it uses a mutex to prevent other
     * threads (that look at this mutex) from destroying the result before this
     * routine has a chance to copy the result to a place that won't be
     * destroyed before the caller gets a chance to handle it.  That place is a
     * mortal SV.  khw chose this over SAVEFREEPV because he is under the
     * impression that the SV will hang around longer under more circumstances
     *
     * The reason it isn't completely thread-safe is that other code could
     * simply not pay attention to the mutex.  All of the Perl core uses the
     * mutex, but it is possible for code from, say XS, to not use this mutex,
     * defeating the safety.
     *
     * getenv() returns, in some implementations, a pointer to a spot in the
     * **environ array, which could be invalidated at any time by this or
     * another thread changing the environment.  Other implementations copy the
     * **environ value to a static buffer, returning a pointer to that.  That
     * buffer might or might not be invalidated by a getenv() call in another
     * thread.  If it does get zapped, we need an exclusive lock.  Otherwise,
     * many getenv() calls can safely be running simultaneously, so a
     * many-reader (but no simultaneous writers) lock is ok.  There is a
     * Configure probe to see if another thread destroys the buffer, and the
     * mutex is defined accordingly.
     *
     * But in all cases, using the mutex prevents these problems, as long as
     * all code uses the same mutex.
     *
     * A complication is that this can be called during phases where the
     * mortalization process isn't available.  These are in interpreter
     * destruction or early in construction.  khw believes that at these times
     * there shouldn't be anything else going on, so plain getenv is safe AS
     * LONG AS the caller acts on the return before calling it again. */

    char * ret;
    dTHX;

    PERL_ARGS_ASSERT_MORTAL_GETENV;

    /* Can't mortalize without stacks.  khw believes that no other threads
     * should be running, so no need to lock things, and this may be during a
     * phase when locking isn't even available */
    if (UNLIKELY(PL_scopestack_ix == 0)) {
        return getenv(str);
    }

#ifdef PERL_MEM_LOG

    /* A major complication arises under PERL_MEM_LOG.  When that is active,
     * every memory allocation may result in logging, depending on the value of
     * ENV{PERL_MEM_LOG} at the moment.  That means, as we create the SV for
     * saving ENV{foo}'s value (but before saving it), the logging code will
     * call us recursively to find out what ENV{PERL_MEM_LOG} is.  Without some
     * care that could lead to: 1) infinite recursion; or 2) deadlock (trying to
     * lock a boolean mutex recursively); 3) destroying the getenv() static
     * buffer; or 4) destroying the temporary created by this for the copy
     * causes a log entry to be made which could cause a new temporary to be
     * created, which will need to be destroyed at some point, leading to an
     * infinite loop.
     *
     * The solution adopted here (after some gnashing of teeth) is to detect
     * the recursive calls and calls from the logger, and treat them specially.
     * Let's say we want to do getenv("foo").  We first find
     * getenv(PERL_MEM_LOG) and save it to a fixed-length per-interpreter
     * variable, so no temporary is required.  Then we do getenv(foo), and in
     * the process of creating a temporary to save it, this function will be
     * called recursively to do a getenv(PERL_MEM_LOG).  On the recursed call,
     * we detect that it is such a call and return our saved value instead of
     * locking and doing a new getenv().  This solves all of problems 1), 2),
     * and 3).  Because all the getenv()s are done while the mutex is locked,
     * the state cannot have changed.  To solve 4), we don't create a temporary
     * when this is called from the logging code.  That code disposes of the
     * return value while the mutex is still locked.
     *
     * The value of getenv(PERL_MEM_LOG) can be anything, but only initial
     * digits and 3 particular letters are significant; the rest are ignored by
     * the memory logging code.  Thus the per-interpreter variable only needs
     * to be large enough to save the significant information, the size of
     * which is known at compile time.  The first byte is extra, reserved for
     * flags for our use.  To protect against overflowing, only the reserved
     * byte, as many digits as don't overflow, and the three letters are
     * stored.
     *
     * The reserved byte has two bits:
     *      0x1 if set indicates that if we get here, it is a recursive call of
     *          getenv()
     *      0x2 if set indicates that the call is from the logging code.
     *
     * If the flag indicates this is a recursive call, just return the stored
     * value of PL_mem_log;  An empty value gets turned into NULL. */
    if (strEQ(str, "PERL_MEM_LOG") && PL_mem_log[0] & 0x1) {
        if (PL_mem_log[1] == '\0') {
            return NULL;
        } else {
            return PL_mem_log + 1;
        }
    }

#endif

    GETENV_LOCK;

#ifdef PERL_MEM_LOG

    /* Here we are in a critical section.  As explained above, we do our own
     * getenv(PERL_MEM_LOG), saving the result safely. */
    ret = getenv("PERL_MEM_LOG");
    if (ret == NULL) {  /* No logging active */

        /* Return that immediately if called from the logging code */
        if (PL_mem_log[0] & 0x2) {
            GETENV_UNLOCK;
            return NULL;
        }

        PL_mem_log[1] = '\0';
    }
    else {
        char *mem_log_meat = PL_mem_log + 1;    /* first byte reserved */

        /* There is nothing to prevent the value of PERL_MEM_LOG from being an
         * extremely long string.  But we want only a few characters from it.
         * PL_mem_log has been made large enough to hold just the ones we need.
         * First the file descriptor. */
        if (isDIGIT(*ret)) {
            const char * s = ret;
            if (UNLIKELY(*s == '0')) {

                /* Reduce multiple leading zeros to a single one.  This is to
                 * allow the caller to change what to do with leading zeros. */
                *mem_log_meat++ = '0';
                s++;
                while (*s == '0') {
                    s++;
                }
            }

            /* If the input overflows, copy just enough for the result to also
             * overflow, plus 1 to make sure */
            while (isDIGIT(*s) && s < ret + TYPE_DIGITS(UV) + 1) {
                *mem_log_meat++ = *s++;
            }
        }

        /* Then each of the four significant characters */
        if (strchr(ret, 'm')) {
            *mem_log_meat++ = 'm';
        }
        if (strchr(ret, 's')) {
            *mem_log_meat++ = 's';
        }
        if (strchr(ret, 't')) {
            *mem_log_meat++ = 't';
        }
        if (strchr(ret, 'c')) {
            *mem_log_meat++ = 'c';
        }
        *mem_log_meat = '\0';

        assert(mem_log_meat < PL_mem_log + sizeof(PL_mem_log));
    }

    /* If we are being called from the logger, it only needs the significant
     * portion of PERL_MEM_LOG, and doesn't need a safe copy */
    if (PL_mem_log[0] & 0x2) {
        assert(strEQ(str, "PERL_MEM_LOG"));
        GETENV_UNLOCK;
        return PL_mem_log + 1;
    }

    /* Here is a generic getenv().  This could be a getenv("PERL_MEM_LOG") that
     * is coming from other than the logging code, so it should be treated the
     * same as any other getenv(), returning the full value, not just the
     * significant part, and having its value saved.  Set the flag that
     * indicates any call to this routine will be a recursion from here */
    PL_mem_log[0] = 0x1;

#endif

    /* Now get the value of the real desired variable, and save a copy */
    ret = getenv(str);

    if (ret != NULL) {
        ret = SvPVX( newSVpvn_flags(ret, strlen(ret) ,SVs_TEMP) );
    }

    GETENV_UNLOCK;

#ifdef PERL_MEM_LOG

    /* Clear the buffer */
    Zero(PL_mem_log, sizeof(PL_mem_log), char);

#endif

    return ret;
}

PERL_STATIC_INLINE bool
Perl_sv_isbool(pTHX_ const SV *sv)
{
    PERL_UNUSED_CONTEXT;
    return SvBoolFlagsOK(sv) && BOOL_INTERNALS_sv_isbool(sv);
}

#ifdef USE_ITHREADS

PERL_STATIC_INLINE AV *
Perl_cop_file_avn(pTHX_ const COP *cop) {

    PERL_ARGS_ASSERT_COP_FILE_AVN;

    const char *file = CopFILE(cop);
    if (file) {
        GV *gv = gv_fetchfile_flags(file, strlen(file), GVF_NOADD);
        if (gv) {
            return GvAVn(gv);
        }
        else
            return NULL;
     }
     else
         return NULL;
}

#endif

PERL_STATIC_INLINE PADNAME *
Perl_padname_refcnt_inc(PADNAME *pn)
{
    PadnameREFCNT(pn)++;
    return pn;
}

PERL_STATIC_INLINE PADNAMELIST *
Perl_padnamelist_refcnt_inc(PADNAMELIST *pnl)
{
    PadnamelistREFCNT(pnl)++;
    return pnl;
}

/* copy a string to a safe spot */

/*
=for apidoc_section $string
=for apidoc      savepv
=for apidoc_item savepvn
=for apidoc_item savepvs
=for apidoc_item savesvpv
=for apidoc_item savesharedpv
=for apidoc_item savesharedpvn
=for apidoc_item savesharedpvs
=for apidoc_item savesharedsvpv

Perl's version of C<strdup()> (or C<strndup()> would be if it existed).

These each return a pointer to a newly allocated string which is a duplicate of
the input string.

The forms differ in how the string to be copied is specified, and where the new
memory is allocated from.

To prevent memory leaks, the memory allocated for the new string needs to be
freed when no longer needed.  This can be done with the C<L</Safefree>>
function, or L<C<SAVEFREEPV>|perlguts/SAVEFREEPV(p)>.

The forms whose names contain C<shared> differ from the corresponding form
without that in its name, only in that the memory in the former comes from
memory shared between threads.  This is needed, because on some platforms,
Windows for example, all allocated memory owned by a thread is deallocated when
that thread ends.  So if you need that not to happen, you need to use the
shared memory forms.

The string to copy in C<savepvs> is a C language string literal surrounded by
double quotes.

The string to copy in the forms whose name contains C<svpv> comes from the PV
in the SV argument C<sv>, using C<SvPV()>

The string to copy in the remaining forms comes from the C<pv> argument.

In the case of C<savepv>, the size of the string is determined by C<strlen()>,
which means it may not contain embedded C<NUL> characters, and must have a
trailing C<NUL>.

In the case of C<savepvn>, C<len> gives the length of C<pv>, hence it may
contain embedded C<NUL> characters.  The copy will be guaranteed to have a
trailing NUL added if not already present.

=cut
*/

PERL_STATIC_INLINE char *
Perl_savepv(pTHX_ const char *pv)
{
    PERL_UNUSED_CONTEXT;
    if (!pv)
        return NULL;
    else {
        char *newaddr;
        const STRLEN pvlen = strlen(pv)+1;
        Newx(newaddr, pvlen, char);
        return (char*)memcpy(newaddr, pv, pvlen);
    }
}

/* same thing but with a known length */

PERL_STATIC_INLINE char *
Perl_savepvn(pTHX_ const char *pv, Size_t len)
{
    char *newaddr;
    PERL_UNUSED_CONTEXT;

    Newx(newaddr,len+1,char);
    /* Give a meaning to NULL pointer mainly for the use in sv_magic() */
    if (pv) {
        /* might not be null terminated */
        newaddr[len] = '\0';
        return (char *) CopyD(pv,newaddr,len,char);
    }
    else {
        return (char *) ZeroD(newaddr,len+1,char);
    }
}

PERL_STATIC_INLINE char *
Perl_savesvpv(pTHX_ SV *sv)
{
    STRLEN len;
    const char * const pv = SvPV_const(sv, len);
    char *newaddr;

    PERL_ARGS_ASSERT_SAVESVPV;

    ++len;
    Newx(newaddr,len,char);
    return (char *) CopyD(pv,newaddr,len,char);
}

PERL_STATIC_INLINE char *
Perl_savesharedsvpv(pTHX_ SV *sv)
{
    STRLEN len;
    const char * const pv = SvPV_const(sv, len);

    PERL_ARGS_ASSERT_SAVESHAREDSVPV;

    return savesharedpvn(pv, len);
}

#ifndef PERL_GET_CONTEXT_DEFINED

/*
=for apidoc_section $embedding
=for apidoc get_context

Implements L<perlapi/C<PERL_GET_CONTEXT>>, which you should use instead.

=cut
*/

PERL_STATIC_INLINE void *
Perl_get_context(void)
{
#  if defined(USE_ITHREADS)
#    ifdef OLD_PTHREADS_API
    pthread_addr_t t;
    int error = pthread_getspecific(PL_thr_key, &t);
    if (error)
        Perl_croak_nocontext("panic: pthread_getspecific, error=%d", error);
    return (void*)t;
#    elif defined(I_MACH_CTHREADS)
    return (void*)cthread_data(cthread_self());
#    else
    return (void*)PTHREAD_GETSPECIFIC(PL_thr_key);
#    endif
#  else
    return (void*)NULL;
#  endif
}

#endif

PERL_STATIC_INLINE MGVTBL*
Perl_get_vtbl(pTHX_ int vtbl_id)
{
    PERL_UNUSED_CONTEXT;

    return (vtbl_id < 0 || vtbl_id >= magic_vtable_max)
        ? NULL : (MGVTBL*)PL_magic_vtables + vtbl_id;
}

/*
=for apidoc_section $string
=for apidoc my_strlcat

The C library C<strlcat> if available, or a Perl implementation of it.
This operates on C C<NUL>-terminated strings.

C<my_strlcat()> appends string C<src> to the end of C<dst>.  It will append at
most S<C<size - strlen(dst) - 1>> bytes.  It will then C<NUL>-terminate,
unless C<size> is 0 or the original C<dst> string was longer than C<size> (in
practice this should not happen as it means that either C<size> is incorrect or
that C<dst> is not a proper C<NUL>-terminated string).

Note that C<size> is the full size of the destination buffer and
the result is guaranteed to be C<NUL>-terminated if there is room.  Note that
room for the C<NUL> should be included in C<size>.

The return value is the total length that C<dst> would have if C<size> is
sufficiently large.  Thus it is the initial length of C<dst> plus the length of
C<src>.  If C<size> is smaller than the return, the excess was not appended.

=cut

Description stolen from http://man.openbsd.org/strlcat.3
*/
#ifndef HAS_STRLCAT
PERL_STATIC_INLINE Size_t
Perl_my_strlcat(char *dst, const char *src, Size_t size)
{
    Size_t used, length, copy;

    used = strlen(dst);
    length = strlen(src);
    if (size > 0 && used < size - 1) {
        copy = (length >= size - used) ? size - used - 1 : length;
        memcpy(dst + used, src, copy);
        dst[used + copy] = '\0';
    }
    return used + length;
}
#endif


/*
=for apidoc my_strlcpy

The C library C<strlcpy> if available, or a Perl implementation of it.
This operates on C C<NUL>-terminated strings.

C<my_strlcpy()> copies up to S<C<size - 1>> bytes from the string C<src>
to C<dst>, C<NUL>-terminating the result if C<size> is not 0.

The return value is the total length C<src> would be if the copy completely
succeeded.  If it is larger than C<size>, the excess was not copied.

=cut

Description stolen from http://man.openbsd.org/strlcpy.3
*/
#ifndef HAS_STRLCPY
PERL_STATIC_INLINE Size_t
Perl_my_strlcpy(char *dst, const char *src, Size_t size)
{
    Size_t length, copy;

    length = strlen(src);
    if (size > 0) {
        copy = (length >= size) ? size - 1 : length;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return length;
}
#endif

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
