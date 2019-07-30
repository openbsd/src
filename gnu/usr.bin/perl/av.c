/*    av.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * '...for the Entwives desired order, and plenty, and peace (by which they
 *  meant that things should remain where they had set them).' --Treebeard
 *
 *     [p.476 of _The Lord of the Rings_, III/iv: "Treebeard"]
 */

/*
=head1 Array Manipulation Functions
*/

#include "EXTERN.h"
#define PERL_IN_AV_C
#include "perl.h"

void
Perl_av_reify(pTHX_ AV *av)
{
    SSize_t key;

    PERL_ARGS_ASSERT_AV_REIFY;
    assert(SvTYPE(av) == SVt_PVAV);

    if (AvREAL(av))
	return;
#ifdef DEBUGGING
    if (SvTIED_mg((const SV *)av, PERL_MAGIC_tied))
	Perl_ck_warner_d(aTHX_ packWARN(WARN_DEBUGGING), "av_reify called on tied array");
#endif
    key = AvMAX(av) + 1;
    while (key > AvFILLp(av) + 1)
	AvARRAY(av)[--key] = NULL;
    while (key) {
	SV * const sv = AvARRAY(av)[--key];
	if (sv != &PL_sv_undef)
	    SvREFCNT_inc_simple_void(sv);
    }
    key = AvARRAY(av) - AvALLOC(av);
    while (key)
	AvALLOC(av)[--key] = NULL;
    AvREIFY_off(av);
    AvREAL_on(av);
}

/*
=for apidoc av_extend

Pre-extend an array.  The C<key> is the index to which the array should be
extended.

=cut
*/

void
Perl_av_extend(pTHX_ AV *av, SSize_t key)
{
    MAGIC *mg;

    PERL_ARGS_ASSERT_AV_EXTEND;
    assert(SvTYPE(av) == SVt_PVAV);

    mg = SvTIED_mg((const SV *)av, PERL_MAGIC_tied);
    if (mg) {
	SV *arg1 = sv_newmortal();
	sv_setiv(arg1, (IV)(key + 1));
	Perl_magic_methcall(aTHX_ MUTABLE_SV(av), mg, SV_CONST(EXTEND), G_DISCARD, 1,
			    arg1);
	return;
    }
    av_extend_guts(av,key,&AvMAX(av),&AvALLOC(av),&AvARRAY(av));
}    

/* The guts of av_extend.  *Not* for general use! */
void
Perl_av_extend_guts(pTHX_ AV *av, SSize_t key, SSize_t *maxp, SV ***allocp,
			  SV ***arrayp)
{
    PERL_ARGS_ASSERT_AV_EXTEND_GUTS;

    if (key < -1) /* -1 is legal */
        Perl_croak(aTHX_
            "panic: av_extend_guts() negative count (%"IVdf")", (IV)key);

    if (key > *maxp) {
	SV** ary;
	SSize_t tmp;
	SSize_t newmax;

	if (av && *allocp != *arrayp) {
	    ary = *allocp + AvFILLp(av) + 1;
	    tmp = *arrayp - *allocp;
	    Move(*arrayp, *allocp, AvFILLp(av)+1, SV*);
	    *maxp += tmp;
	    *arrayp = *allocp;
	    if (AvREAL(av)) {
		while (tmp)
		    ary[--tmp] = NULL;
	    }
	    if (key > *maxp - 10) {
		newmax = key + *maxp;
		goto resize;
	    }
	}
	else {
	    if (*allocp) {

#ifdef Perl_safesysmalloc_size
		/* Whilst it would be quite possible to move this logic around
		   (as I did in the SV code), so as to set AvMAX(av) early,
		   based on calling Perl_safesysmalloc_size() immediately after
		   allocation, I'm not convinced that it is a great idea here.
		   In an array we have to loop round setting everything to
		   NULL, which means writing to memory, potentially lots
		   of it, whereas for the SV buffer case we don't touch the
		   "bonus" memory. So there there is no cost in telling the
		   world about it, whereas here we have to do work before we can
		   tell the world about it, and that work involves writing to
		   memory that might never be read. So, I feel, better to keep
		   the current lazy system of only writing to it if our caller
		   has a need for more space. NWC  */
		newmax = Perl_safesysmalloc_size((void*)*allocp) /
		    sizeof(const SV *) - 1;

		if (key <= newmax) 
		    goto resized;
#endif 
                /* overflow-safe version of newmax = key + *maxp/5 */
		newmax = *maxp / 5;
                newmax = (key > SSize_t_MAX - newmax)
                            ? SSize_t_MAX : key + newmax;
	      resize:
		{
#ifdef PERL_MALLOC_WRAP /* Duplicated in pp_hot.c */
		    static const char oom_array_extend[] =
			"Out of memory during array extend";
#endif
                    /* it should really be newmax+1 here, but if newmax
                     * happens to equal SSize_t_MAX, then newmax+1 is
                     * undefined. This means technically we croak one
                     * index lower than we should in theory; in practice
                     * its unlikely the system has SSize_t_MAX/sizeof(SV*)
                     * bytes to spare! */
		    MEM_WRAP_CHECK_1(newmax, SV*, oom_array_extend);
		}
#ifdef STRESS_REALLOC
		{
		    SV ** const old_alloc = *allocp;
		    Newx(*allocp, newmax+1, SV*);
		    Copy(old_alloc, *allocp, *maxp + 1, SV*);
		    Safefree(old_alloc);
		}
#else
		Renew(*allocp,newmax+1, SV*);
#endif
#ifdef Perl_safesysmalloc_size
	      resized:
#endif
		ary = *allocp + *maxp + 1;
		tmp = newmax - *maxp;
		if (av == PL_curstack) {	/* Oops, grew stack (via av_store()?) */
		    PL_stack_sp = *allocp + (PL_stack_sp - PL_stack_base);
		    PL_stack_base = *allocp;
		    PL_stack_max = PL_stack_base + newmax;
		}
	    }
	    else {
		newmax = key < 3 ? 3 : key;
		{
#ifdef PERL_MALLOC_WRAP /* Duplicated in pp_hot.c */
		    static const char oom_array_extend[] =
			"Out of memory during array extend";
#endif
                    /* see comment above about newmax+1*/
		    MEM_WRAP_CHECK_1(newmax, SV*, oom_array_extend);
		}
		Newx(*allocp, newmax+1, SV*);
		ary = *allocp + 1;
		tmp = newmax;
		*allocp[0] = NULL;	/* For the stacks */
	    }
	    if (av && AvREAL(av)) {
		while (tmp)
		    ary[--tmp] = NULL;
	    }
	    
	    *arrayp = *allocp;
	    *maxp = newmax;
	}
    }
}

/*
=for apidoc av_fetch

Returns the SV at the specified index in the array.  The C<key> is the
index.  If lval is true, you are guaranteed to get a real SV back (in case
it wasn't real before), which you can then modify.  Check that the return
value is non-null before dereferencing it to a C<SV*>.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for
more information on how to use this function on tied arrays. 

The rough perl equivalent is C<$myarray[$idx]>.

=cut
*/

static bool
S_adjust_index(pTHX_ AV *av, const MAGIC *mg, SSize_t *keyp)
{
    bool adjust_index = 1;
    if (mg) {
	/* Handle negative array indices 20020222 MJD */
	SV * const ref = SvTIED_obj(MUTABLE_SV(av), mg);
	SvGETMAGIC(ref);
	if (SvROK(ref) && SvOBJECT(SvRV(ref))) {
	    SV * const * const negative_indices_glob =
		hv_fetchs(SvSTASH(SvRV(ref)), NEGATIVE_INDICES_VAR, 0);

	    if (negative_indices_glob && isGV(*negative_indices_glob)
	     && SvTRUE(GvSV(*negative_indices_glob)))
		adjust_index = 0;
	}
    }

    if (adjust_index) {
	*keyp += AvFILL(av) + 1;
	if (*keyp < 0)
	    return FALSE;
    }
    return TRUE;
}

SV**
Perl_av_fetch(pTHX_ AV *av, SSize_t key, I32 lval)
{
    PERL_ARGS_ASSERT_AV_FETCH;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvRMAGICAL(av)) {
        const MAGIC * const tied_magic
	    = mg_find((const SV *)av, PERL_MAGIC_tied);
        if (tied_magic || mg_find((const SV *)av, PERL_MAGIC_regdata)) {
	    SV *sv;
	    if (key < 0) {
		if (!S_adjust_index(aTHX_ av, tied_magic, &key))
			return NULL;
	    }

            sv = sv_newmortal();
	    sv_upgrade(sv, SVt_PVLV);
	    mg_copy(MUTABLE_SV(av), sv, 0, key);
	    if (!tied_magic) /* for regdata, force leavesub to make copies */
		SvTEMP_off(sv);
	    LvTYPE(sv) = 't';
	    LvTARG(sv) = sv; /* fake (SV**) */
	    return &(LvTARG(sv));
        }
    }

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return NULL;
    }

    if (key > AvFILLp(av) || !AvARRAY(av)[key]) {
      emptyness:
	return lval ? av_store(av,key,newSV(0)) : NULL;
    }

    if (AvREIFY(av)
	     && (!AvARRAY(av)[key]	/* eg. @_ could have freed elts */
		 || SvIS_FREED(AvARRAY(av)[key]))) {
	AvARRAY(av)[key] = NULL;	/* 1/2 reify */
	goto emptyness;
    }
    return &AvARRAY(av)[key];
}

/*
=for apidoc av_store

Stores an SV in an array.  The array index is specified as C<key>.  The
return value will be C<NULL> if the operation failed or if the value did not
need to be actually stored within the array (as in the case of tied
arrays).  Otherwise, it can be dereferenced
to get the C<SV*> that was stored
there (= C<val>)).

Note that the caller is responsible for suitably incrementing the reference
count of C<val> before the call, and decrementing it if the function
returned C<NULL>.

Approximate Perl equivalent: C<$myarray[$key] = $val;>.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for
more information on how to use this function on tied arrays.

=cut
*/

SV**
Perl_av_store(pTHX_ AV *av, SSize_t key, SV *val)
{
    SV** ary;

    PERL_ARGS_ASSERT_AV_STORE;
    assert(SvTYPE(av) == SVt_PVAV);

    /* S_regclass relies on being able to pass in a NULL sv
       (unicode_alternate may be NULL).
    */

    if (SvRMAGICAL(av)) {
        const MAGIC * const tied_magic = mg_find((const SV *)av, PERL_MAGIC_tied);
        if (tied_magic) {
            if (key < 0) {
		if (!S_adjust_index(aTHX_ av, tied_magic, &key))
                        return 0;
            }
	    if (val) {
		mg_copy(MUTABLE_SV(av), val, 0, key);
	    }
	    return NULL;
        }
    }


    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return NULL;
    }

    if (SvREADONLY(av) && key >= AvFILL(av))
	Perl_croak_no_modify();

    if (!AvREAL(av) && AvREIFY(av))
	av_reify(av);
    if (key > AvMAX(av))
	av_extend(av,key);
    ary = AvARRAY(av);
    if (AvFILLp(av) < key) {
	if (!AvREAL(av)) {
	    if (av == PL_curstack && key > PL_stack_sp - PL_stack_base)
		PL_stack_sp = PL_stack_base + key;	/* XPUSH in disguise */
	    do {
		ary[++AvFILLp(av)] = NULL;
	    } while (AvFILLp(av) < key);
	}
	AvFILLp(av) = key;
    }
    else if (AvREAL(av))
	SvREFCNT_dec(ary[key]);
    ary[key] = val;
    if (SvSMAGICAL(av)) {
	const MAGIC *mg = SvMAGIC(av);
	bool set = TRUE;
	for (; mg; mg = mg->mg_moremagic) {
	  if (!isUPPER(mg->mg_type)) continue;
	  if (val) {
	    sv_magic(val, MUTABLE_SV(av), toLOWER(mg->mg_type), 0, key);
	  }
	  if (PL_delaymagic && mg->mg_type == PERL_MAGIC_isa) {
	    PL_delaymagic |= DM_ARRAY_ISA;
	    set = FALSE;
	  }
	}
	if (set)
	   mg_set(MUTABLE_SV(av));
    }
    return &ary[key];
}

/*
=for apidoc av_make

Creates a new AV and populates it with a list of SVs.  The SVs are copied
into the array, so they may be freed after the call to C<av_make>.  The new AV
will have a reference count of 1.

Perl equivalent: C<my @new_array = ($scalar1, $scalar2, $scalar3...);>

=cut
*/

AV *
Perl_av_make(pTHX_ SSize_t size, SV **strp)
{
    AV * const av = MUTABLE_AV(newSV_type(SVt_PVAV));
    /* sv_upgrade does AvREAL_only()  */
    PERL_ARGS_ASSERT_AV_MAKE;
    assert(SvTYPE(av) == SVt_PVAV);

    if (size) {		/* "defined" was returning undef for size==0 anyway. */
        SV** ary;
        SSize_t i;
	Newx(ary,size,SV*);
	AvALLOC(av) = ary;
	AvARRAY(av) = ary;
	AvMAX(av) = size - 1;
	AvFILLp(av) = -1;
	ENTER;
	SAVEFREESV(av);
	for (i = 0; i < size; i++) {
	    assert (*strp);

	    /* Don't let sv_setsv swipe, since our source array might
	       have multiple references to the same temp scalar (e.g.
	       from a list slice) */

	    SvGETMAGIC(*strp); /* before newSV, in case it dies */
	    AvFILLp(av)++;
	    ary[i] = newSV(0);
	    sv_setsv_flags(ary[i], *strp,
			   SV_DO_COW_SVSETSV|SV_NOSTEAL);
	    strp++;
	}
	SvREFCNT_inc_simple_void_NN(av);
	LEAVE;
    }
    return av;
}

/*
=for apidoc av_clear

Frees the all the elements of an array, leaving it empty.
The XS equivalent of C<@array = ()>.  See also L</av_undef>.

Note that it is possible that the actions of a destructor called directly
or indirectly by freeing an element of the array could cause the reference
count of the array itself to be reduced (e.g. by deleting an entry in the
symbol table). So it is a possibility that the AV could have been freed
(or even reallocated) on return from the call unless you hold a reference
to it.

=cut
*/

void
Perl_av_clear(pTHX_ AV *av)
{
    SSize_t extra;
    bool real;

    PERL_ARGS_ASSERT_AV_CLEAR;
    assert(SvTYPE(av) == SVt_PVAV);

#ifdef DEBUGGING
    if (SvREFCNT(av) == 0) {
	Perl_ck_warner_d(aTHX_ packWARN(WARN_DEBUGGING), "Attempt to clear deleted array");
    }
#endif

    if (SvREADONLY(av))
	Perl_croak_no_modify();

    /* Give any tie a chance to cleanup first */
    if (SvRMAGICAL(av)) {
	const MAGIC* const mg = SvMAGIC(av);
	if (PL_delaymagic && mg && mg->mg_type == PERL_MAGIC_isa)
	    PL_delaymagic |= DM_ARRAY_ISA;
        else
	    mg_clear(MUTABLE_SV(av)); 
    }

    if (AvMAX(av) < 0)
	return;

    if ((real = !!AvREAL(av))) {
	SV** const ary = AvARRAY(av);
	SSize_t index = AvFILLp(av) + 1;
	ENTER;
	SAVEFREESV(SvREFCNT_inc_simple_NN(av));
	while (index) {
	    SV * const sv = ary[--index];
	    /* undef the slot before freeing the value, because a
	     * destructor might try to modify this array */
	    ary[index] = NULL;
	    SvREFCNT_dec(sv);
	}
    }
    extra = AvARRAY(av) - AvALLOC(av);
    if (extra) {
	AvMAX(av) += extra;
	AvARRAY(av) = AvALLOC(av);
    }
    AvFILLp(av) = -1;
    if (real) LEAVE;
}

/*
=for apidoc av_undef

Undefines the array. The XS equivalent of C<undef(@array)>.

As well as freeing all the elements of the array (like C<av_clear()>), this
also frees the memory used by the av to store its list of scalars.

See L</av_clear> for a note about the array possibly being invalid on
return.

=cut
*/

void
Perl_av_undef(pTHX_ AV *av)
{
    bool real;

    PERL_ARGS_ASSERT_AV_UNDEF;
    assert(SvTYPE(av) == SVt_PVAV);

    /* Give any tie a chance to cleanup first */
    if (SvTIED_mg((const SV *)av, PERL_MAGIC_tied)) 
	av_fill(av, -1);

    if ((real = !!AvREAL(av))) {
	SSize_t key = AvFILLp(av) + 1;
	ENTER;
	SAVEFREESV(SvREFCNT_inc_simple_NN(av));
	while (key)
	    SvREFCNT_dec(AvARRAY(av)[--key]);
    }

    Safefree(AvALLOC(av));
    AvALLOC(av) = NULL;
    AvARRAY(av) = NULL;
    AvMAX(av) = AvFILLp(av) = -1;

    if(SvRMAGICAL(av)) mg_clear(MUTABLE_SV(av));
    if(real) LEAVE;
}

/*

=for apidoc av_create_and_push

Push an SV onto the end of the array, creating the array if necessary.
A small internal helper function to remove a commonly duplicated idiom.

=cut
*/

void
Perl_av_create_and_push(pTHX_ AV **const avp, SV *const val)
{
    PERL_ARGS_ASSERT_AV_CREATE_AND_PUSH;

    if (!*avp)
	*avp = newAV();
    av_push(*avp, val);
}

/*
=for apidoc av_push

Pushes an SV (transferring control of one reference count) onto the end of the
array.  The array will grow automatically to accommodate the addition.

Perl equivalent: C<push @myarray, $elem;>.

=cut
*/

void
Perl_av_push(pTHX_ AV *av, SV *val)
{             
    MAGIC *mg;

    PERL_ARGS_ASSERT_AV_PUSH;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak_no_modify();

    if ((mg = SvTIED_mg((const SV *)av, PERL_MAGIC_tied))) {
	Perl_magic_methcall(aTHX_ MUTABLE_SV(av), mg, SV_CONST(PUSH), G_DISCARD, 1,
			    val);
	return;
    }
    av_store(av,AvFILLp(av)+1,val);
}

/*
=for apidoc av_pop

Removes one SV from the end of the array, reducing its size by one and
returning the SV (transferring control of one reference count) to the
caller.  Returns C<&PL_sv_undef> if the array is empty.

Perl equivalent: C<pop(@myarray);>

=cut
*/

SV *
Perl_av_pop(pTHX_ AV *av)
{
    SV *retval;
    MAGIC* mg;

    PERL_ARGS_ASSERT_AV_POP;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak_no_modify();
    if ((mg = SvTIED_mg((const SV *)av, PERL_MAGIC_tied))) {
	retval = Perl_magic_methcall(aTHX_ MUTABLE_SV(av), mg, SV_CONST(POP), 0, 0);
	if (retval)
	    retval = newSVsv(retval);
	return retval;
    }
    if (AvFILL(av) < 0)
	return &PL_sv_undef;
    retval = AvARRAY(av)[AvFILLp(av)];
    AvARRAY(av)[AvFILLp(av)--] = NULL;
    if (SvSMAGICAL(av))
	mg_set(MUTABLE_SV(av));
    return retval ? retval : &PL_sv_undef;
}

/*

=for apidoc av_create_and_unshift_one

Unshifts an SV onto the beginning of the array, creating the array if
necessary.
A small internal helper function to remove a commonly duplicated idiom.

=cut
*/

SV **
Perl_av_create_and_unshift_one(pTHX_ AV **const avp, SV *const val)
{
    PERL_ARGS_ASSERT_AV_CREATE_AND_UNSHIFT_ONE;

    if (!*avp)
	*avp = newAV();
    av_unshift(*avp, 1);
    return av_store(*avp, 0, val);
}

/*
=for apidoc av_unshift

Unshift the given number of C<undef> values onto the beginning of the
array.  The array will grow automatically to accommodate the addition.  You
must then use C<av_store> to assign values to these new elements.

Perl equivalent: S<C<unshift @myarray, ( (undef) x $n );>>

=cut
*/

void
Perl_av_unshift(pTHX_ AV *av, SSize_t num)
{
    SSize_t i;
    MAGIC* mg;

    PERL_ARGS_ASSERT_AV_UNSHIFT;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak_no_modify();

    if ((mg = SvTIED_mg((const SV *)av, PERL_MAGIC_tied))) {
	Perl_magic_methcall(aTHX_ MUTABLE_SV(av), mg, SV_CONST(UNSHIFT),
			    G_DISCARD | G_UNDEF_FILL, num);
	return;
    }

    if (num <= 0)
      return;
    if (!AvREAL(av) && AvREIFY(av))
	av_reify(av);
    i = AvARRAY(av) - AvALLOC(av);
    if (i) {
	if (i > num)
	    i = num;
	num -= i;
    
	AvMAX(av) += i;
	AvFILLp(av) += i;
	AvARRAY(av) = AvARRAY(av) - i;
    }
    if (num) {
	SV **ary;
	const SSize_t i = AvFILLp(av);
	/* Create extra elements */
	const SSize_t slide = i > 0 ? i : 0;
	num += slide;
	av_extend(av, i + num);
	AvFILLp(av) += num;
	ary = AvARRAY(av);
	Move(ary, ary + num, i + 1, SV*);
	do {
	    ary[--num] = NULL;
	} while (num);
	/* Make extra elements into a buffer */
	AvMAX(av) -= slide;
	AvFILLp(av) -= slide;
	AvARRAY(av) = AvARRAY(av) + slide;
    }
}

/*
=for apidoc av_shift

Removes one SV from the start of the array, reducing its size by one and
returning the SV (transferring control of one reference count) to the
caller.  Returns C<&PL_sv_undef> if the array is empty.

Perl equivalent: C<shift(@myarray);>

=cut
*/

SV *
Perl_av_shift(pTHX_ AV *av)
{
    SV *retval;
    MAGIC* mg;

    PERL_ARGS_ASSERT_AV_SHIFT;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak_no_modify();
    if ((mg = SvTIED_mg((const SV *)av, PERL_MAGIC_tied))) {
	retval = Perl_magic_methcall(aTHX_ MUTABLE_SV(av), mg, SV_CONST(SHIFT), 0, 0);
	if (retval)
	    retval = newSVsv(retval);
	return retval;
    }
    if (AvFILL(av) < 0)
      return &PL_sv_undef;
    retval = *AvARRAY(av);
    if (AvREAL(av))
	*AvARRAY(av) = NULL;
    AvARRAY(av) = AvARRAY(av) + 1;
    AvMAX(av)--;
    AvFILLp(av)--;
    if (SvSMAGICAL(av))
	mg_set(MUTABLE_SV(av));
    return retval ? retval : &PL_sv_undef;
}

/*
=for apidoc av_top_index

Returns the highest index in the array.  The number of elements in the
array is S<C<av_top_index(av) + 1>>.  Returns -1 if the array is empty.

The Perl equivalent for this is C<$#myarray>.

(A slightly shorter form is C<av_tindex>.)

=for apidoc av_len

Same as L</av_top_index>.  Note that, unlike what the name implies, it returns
the highest index in the array, so to get the size of the array you need to use
S<C<av_len(av) + 1>>.  This is unlike L</sv_len>, which returns what you would
expect.

=cut
*/

SSize_t
Perl_av_len(pTHX_ AV *av)
{
    PERL_ARGS_ASSERT_AV_LEN;

    return av_top_index(av);
}

/*
=for apidoc av_fill

Set the highest index in the array to the given number, equivalent to
Perl's S<C<$#array = $fill;>>.

The number of elements in the array will be S<C<fill + 1>> after
C<av_fill()> returns.  If the array was previously shorter, then the
additional elements appended are set to NULL.  If the array
was longer, then the excess elements are freed.  S<C<av_fill(av, -1)>> is
the same as C<av_clear(av)>.

=cut
*/
void
Perl_av_fill(pTHX_ AV *av, SSize_t fill)
{
    MAGIC *mg;

    PERL_ARGS_ASSERT_AV_FILL;
    assert(SvTYPE(av) == SVt_PVAV);

    if (fill < 0)
	fill = -1;
    if ((mg = SvTIED_mg((const SV *)av, PERL_MAGIC_tied))) {
	SV *arg1 = sv_newmortal();
	sv_setiv(arg1, (IV)(fill + 1));
	Perl_magic_methcall(aTHX_ MUTABLE_SV(av), mg, SV_CONST(STORESIZE), G_DISCARD,
			    1, arg1);
	return;
    }
    if (fill <= AvMAX(av)) {
	SSize_t key = AvFILLp(av);
	SV** const ary = AvARRAY(av);

	if (AvREAL(av)) {
	    while (key > fill) {
		SvREFCNT_dec(ary[key]);
		ary[key--] = NULL;
	    }
	}
	else {
	    while (key < fill)
		ary[++key] = NULL;
	}
	    
	AvFILLp(av) = fill;
	if (SvSMAGICAL(av))
	    mg_set(MUTABLE_SV(av));
    }
    else
	(void)av_store(av,fill,NULL);
}

/*
=for apidoc av_delete

Deletes the element indexed by C<key> from the array, makes the element mortal,
and returns it.  If C<flags> equals C<G_DISCARD>, the element is freed and null
is returned.  Perl equivalent: S<C<my $elem = delete($myarray[$idx]);>> for the
non-C<G_DISCARD> version and a void-context S<C<delete($myarray[$idx]);>> for the
C<G_DISCARD> version.

=cut
*/
SV *
Perl_av_delete(pTHX_ AV *av, SSize_t key, I32 flags)
{
    SV *sv;

    PERL_ARGS_ASSERT_AV_DELETE;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak_no_modify();

    if (SvRMAGICAL(av)) {
        const MAGIC * const tied_magic
	    = mg_find((const SV *)av, PERL_MAGIC_tied);
        if ((tied_magic || mg_find((const SV *)av, PERL_MAGIC_regdata))) {
            SV **svp;
            if (key < 0) {
		if (!S_adjust_index(aTHX_ av, tied_magic, &key))
			return NULL;
            }
            svp = av_fetch(av, key, TRUE);
            if (svp) {
                sv = *svp;
                mg_clear(sv);
                if (mg_find(sv, PERL_MAGIC_tiedelem)) {
                    sv_unmagic(sv, PERL_MAGIC_tiedelem); /* No longer an element */
                    return sv;
                }
		return NULL;
            }
        }
    }

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return NULL;
    }

    if (key > AvFILLp(av))
	return NULL;
    else {
	if (!AvREAL(av) && AvREIFY(av))
	    av_reify(av);
	sv = AvARRAY(av)[key];
	AvARRAY(av)[key] = NULL;
	if (key == AvFILLp(av)) {
	    do {
		AvFILLp(av)--;
	    } while (--key >= 0 && !AvARRAY(av)[key]);
	}
	if (SvSMAGICAL(av))
	    mg_set(MUTABLE_SV(av));
    }
    if(sv != NULL) {
	if (flags & G_DISCARD) {
	    SvREFCNT_dec_NN(sv);
	    return NULL;
	}
	else if (AvREAL(av))
	    sv_2mortal(sv);
    }
    return sv;
}

/*
=for apidoc av_exists

Returns true if the element indexed by C<key> has been initialized.

This relies on the fact that uninitialized array elements are set to
C<NULL>.

Perl equivalent: C<exists($myarray[$key])>.

=cut
*/
bool
Perl_av_exists(pTHX_ AV *av, SSize_t key)
{
    PERL_ARGS_ASSERT_AV_EXISTS;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvRMAGICAL(av)) {
        const MAGIC * const tied_magic
	    = mg_find((const SV *)av, PERL_MAGIC_tied);
        const MAGIC * const regdata_magic
            = mg_find((const SV *)av, PERL_MAGIC_regdata);
        if (tied_magic || regdata_magic) {
            MAGIC *mg;
            /* Handle negative array indices 20020222 MJD */
            if (key < 0) {
		if (!S_adjust_index(aTHX_ av, tied_magic, &key))
                        return FALSE;
            }

            if(key >= 0 && regdata_magic) {
                if (key <= AvFILL(av))
                    return TRUE;
                else
                    return FALSE;
            }
	    {
		SV * const sv = sv_newmortal();
		mg_copy(MUTABLE_SV(av), sv, 0, key);
		mg = mg_find(sv, PERL_MAGIC_tiedelem);
		if (mg) {
		    magic_existspack(sv, mg);
		    {
			I32 retbool = SvTRUE_nomg_NN(sv);
			return cBOOL(retbool);
		    }
		}
	    }
        }
    }

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return FALSE;
    }

    if (key <= AvFILLp(av) && AvARRAY(av)[key])
    {
	return TRUE;
    }
    else
	return FALSE;
}

static MAGIC *
S_get_aux_mg(pTHX_ AV *av) {
    MAGIC *mg;

    PERL_ARGS_ASSERT_GET_AUX_MG;
    assert(SvTYPE(av) == SVt_PVAV);

    mg = mg_find((const SV *)av, PERL_MAGIC_arylen_p);

    if (!mg) {
	mg = sv_magicext(MUTABLE_SV(av), 0, PERL_MAGIC_arylen_p,
			 &PL_vtbl_arylen_p, 0, 0);
	assert(mg);
	/* sv_magicext won't set this for us because we pass in a NULL obj  */
	mg->mg_flags |= MGf_REFCOUNTED;
    }
    return mg;
}

SV **
Perl_av_arylen_p(pTHX_ AV *av) {
    MAGIC *const mg = get_aux_mg(av);

    PERL_ARGS_ASSERT_AV_ARYLEN_P;
    assert(SvTYPE(av) == SVt_PVAV);

    return &(mg->mg_obj);
}

IV *
Perl_av_iter_p(pTHX_ AV *av) {
    MAGIC *const mg = get_aux_mg(av);

    PERL_ARGS_ASSERT_AV_ITER_P;
    assert(SvTYPE(av) == SVt_PVAV);

#if IVSIZE == I32SIZE
    return (IV *)&(mg->mg_len);
#else
    if (!mg->mg_ptr) {
	IV *temp;
	mg->mg_len = IVSIZE;
	Newxz(temp, 1, IV);
	mg->mg_ptr = (char *) temp;
    }
    return (IV *)mg->mg_ptr;
#endif
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
