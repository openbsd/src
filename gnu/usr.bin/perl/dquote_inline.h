/*    dquote_inline.h
 *
 *    Copyright (C) 2015 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#ifndef DQUOTE_INLINE_H /* Guard against nested #inclusion */
#define DQUOTE_INLINE_H

/*
 - regcurly - a little FSA that accepts {\d+,?\d*}
    Pulled from reg.c.
 */
PERL_STATIC_INLINE I32
S_regcurly(const char *s)
{
    PERL_ARGS_ASSERT_REGCURLY;

    if (*s++ != '{')
	return FALSE;
    if (!isDIGIT(*s))
	return FALSE;
    while (isDIGIT(*s))
	s++;
    if (*s == ',') {
	s++;
	while (isDIGIT(*s))
	    s++;
    }

    return *s == '}';
}

/* This is inline not for speed, but because it is so tiny */

PERL_STATIC_INLINE char*
S_form_short_octal_warning(pTHX_
                           const char * const s, /* Points to first non-octal */
                           const STRLEN len      /* Length of octals string, so
                                                    (s-len) points to first
                                                    octal */
)
{
    /* Return a character string consisting of a warning message for when a
     * string constant in octal is weird, like "\078".  */

    const char * sans_leading_zeros = s - len;

    PERL_ARGS_ASSERT_FORM_SHORT_OCTAL_WARNING;

    assert(*s == '8' || *s == '9');

    /* Remove the leading zeros, retaining one zero so won't be zero length */
    while (*sans_leading_zeros == '0') sans_leading_zeros++;
    if (sans_leading_zeros == s) {
        sans_leading_zeros--;
    }

    return Perl_form(aTHX_
                     "'%.*s' resolved to '\\o{%.*s}%c'",
                     (int) (len + 2), s - len - 1,
                     (int) (s - sans_leading_zeros), sans_leading_zeros,
                     *s);
}
#endif  /* DQUOTE_INLINE_H */
