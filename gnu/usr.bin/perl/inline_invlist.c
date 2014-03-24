/*    inline_invlist.c
 *
 *    Copyright (C) 2012 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#if defined(PERL_IN_UTF8_C) || defined(PERL_IN_REGCOMP_C) || defined(PERL_IN_REGEXEC_C)

#define INVLIST_LEN_OFFSET 0	/* Number of elements in the inversion list */
#define INVLIST_ITER_OFFSET 1	/* Current iteration position */
#define INVLIST_PREVIOUS_INDEX_OFFSET 2  /* Place to cache index of previous
                                            result */

/* This is a combination of a version and data structure type, so that one
 * being passed in can be validated to be an inversion list of the correct
 * vintage.  When the structure of the header is changed, a new random number
 * in the range 2**31-1 should be generated and the new() method changed to
 * insert that at this location.  Then, if an auxiliary program doesn't change
 * correspondingly, it will be discovered immediately */
#define INVLIST_VERSION_ID_OFFSET 3
#define INVLIST_VERSION_ID 290655244

/* For safety, when adding new elements, remember to #undef them at the end of
 * the inversion list code section */

#define INVLIST_ZERO_OFFSET 4	/* 0 or 1; must be last element in header */
/* The UV at position ZERO contains either 0 or 1.  If 0, the inversion list
 * contains the code point U+00000, and begins here.  If 1, the inversion list
 * doesn't contain U+0000, and it begins at the next UV in the array.
 * Inverting an inversion list consists of adding or removing the 0 at the
 * beginning of it.  By reserving a space for that 0, inversion can be made
 * very fast */

#define HEADER_LENGTH (INVLIST_ZERO_OFFSET + 1)

/* An element is in an inversion list iff its index is even numbered: 0, 2, 4,
 * etc */
#define ELEMENT_RANGE_MATCHES_INVLIST(i) (! ((i) & 1))
#define PREV_RANGE_MATCHES_INVLIST(i) (! ELEMENT_RANGE_MATCHES_INVLIST(i))

PERL_STATIC_INLINE UV*
S__get_invlist_len_addr(pTHX_ SV* invlist)
{
    /* Return the address of the UV that contains the current number
     * of used elements in the inversion list */

    PERL_ARGS_ASSERT__GET_INVLIST_LEN_ADDR;

    return (UV *) (SvPVX(invlist) + (INVLIST_LEN_OFFSET * sizeof (UV)));
}

PERL_STATIC_INLINE UV
S__invlist_len(pTHX_ SV* const invlist)
{
    /* Returns the current number of elements stored in the inversion list's
     * array */

    PERL_ARGS_ASSERT__INVLIST_LEN;

    return *_get_invlist_len_addr(invlist);
}

PERL_STATIC_INLINE bool
S__invlist_contains_cp(pTHX_ SV* const invlist, const UV cp)
{
    /* Does <invlist> contain code point <cp> as part of the set? */

    IV index = _invlist_search(invlist, cp);

    PERL_ARGS_ASSERT__INVLIST_CONTAINS_CP;

    return index >= 0 && ELEMENT_RANGE_MATCHES_INVLIST(index);
}

#endif
