/*    invlist_inline.h
 *
 *    Copyright (C) 2012 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#if defined(PERL_IN_UTF8_C) || defined(PERL_IN_REGCOMP_C) || defined(PERL_IN_REGEXEC_C)

/* An element is in an inversion list iff its index is even numbered: 0, 2, 4,
 * etc */
#define ELEMENT_RANGE_MATCHES_INVLIST(i) (! ((i) & 1))
#define PREV_RANGE_MATCHES_INVLIST(i) (! ELEMENT_RANGE_MATCHES_INVLIST(i))

/* This converts to/from our UVs to what the SV code is expecting: bytes. */
#define TO_INTERNAL_SIZE(x) ((x) * sizeof(UV))
#define FROM_INTERNAL_SIZE(x) ((x)/ sizeof(UV))

PERL_STATIC_INLINE bool*
S_get_invlist_offset_addr(SV* invlist)
{
    /* Return the address of the field that says whether the inversion list is
     * offset (it contains 1) or not (contains 0) */
    PERL_ARGS_ASSERT_GET_INVLIST_OFFSET_ADDR;

    assert(SvTYPE(invlist) == SVt_INVLIST);

    return &(((XINVLIST*) SvANY(invlist))->is_offset);
}

PERL_STATIC_INLINE UV
S__invlist_len(SV* const invlist)
{
    /* Returns the current number of elements stored in the inversion list's
     * array */

    PERL_ARGS_ASSERT__INVLIST_LEN;

    assert(SvTYPE(invlist) == SVt_INVLIST);

    return (SvCUR(invlist) == 0)
           ? 0
           : FROM_INTERNAL_SIZE(SvCUR(invlist)) - *get_invlist_offset_addr(invlist);
}

PERL_STATIC_INLINE bool
S__invlist_contains_cp(SV* const invlist, const UV cp)
{
    /* Does <invlist> contain code point <cp> as part of the set? */

    IV index = _invlist_search(invlist, cp);

    PERL_ARGS_ASSERT__INVLIST_CONTAINS_CP;

    return index >= 0 && ELEMENT_RANGE_MATCHES_INVLIST(index);
}

PERL_STATIC_INLINE UV*
S_invlist_array(SV* const invlist)
{
    /* Returns the pointer to the inversion list's array.  Every time the
     * length changes, this needs to be called in case malloc or realloc moved
     * it */

    PERL_ARGS_ASSERT_INVLIST_ARRAY;

    /* Must not be empty.  If these fail, you probably didn't check for <len>
     * being non-zero before trying to get the array */
    assert(_invlist_len(invlist));

    /* The very first element always contains zero, The array begins either
     * there, or if the inversion list is offset, at the element after it.
     * The offset header field determines which; it contains 0 or 1 to indicate
     * how much additionally to add */
    assert(0 == *(SvPVX(invlist)));
    return ((UV *) SvPVX(invlist) + *get_invlist_offset_addr(invlist));
}

#   if defined(PERL_IN_UTF8_C) || defined(PERL_IN_REGEXEC_C)

/* These symbols are only needed later in regcomp.c */
#       undef TO_INTERNAL_SIZE
#       undef FROM_INTERNAL_SIZE
#   endif

#endif
