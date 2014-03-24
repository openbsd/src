/*    dquote_static.c
 *
 * This file contains static functions that are related to
 * parsing double-quotish expressions, but are used in more than
 * one file.
 *
 * It is currently #included by regcomp.c and toke.c.
*/

#define PERL_IN_DQUOTE_STATIC_C
#include "embed.h"

/*
 - regcurly - a little FSA that accepts {\d+,?\d*}
    Pulled from regcomp.c.
 */
PERL_STATIC_INLINE I32
S_regcurly(pTHX_ const char *s,
           const bool rbrace_must_be_escaped /* Should the terminating '} be
                                                preceded by a backslash?  This
                                                is an abnormal case */
    )
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

    return (rbrace_must_be_escaped)
           ? *s == '\\' && *(s+1) == '}'
           : *s == '}';
}

/* XXX Add documentation after final interface and behavior is decided */
/* May want to show context for error, so would pass Perl_bslash_c(pTHX_ const char* current, const char* start, const bool output_warning)
    U8 source = *current;
*/

STATIC char
S_grok_bslash_c(pTHX_ const char source, const bool utf8, const bool output_warning)
{

    U8 result;

    if (utf8) {
	/* Trying to deprecate non-ASCII usages.  This construct has never
	 * worked for a utf8 variant.  So, even though are accepting non-ASCII
	 * Latin1 in 5.14, no need to make them work under utf8 */
	if (! isASCII(source)) {
	    Perl_croak(aTHX_ "Character following \"\\c\" must be ASCII");
	}
    }

    result = toCTRL(source);
    if (! isASCII(source)) {
	    Perl_ck_warner_d(aTHX_ packWARN2(WARN_DEPRECATED, WARN_SYNTAX),
			    "Character following \"\\c\" must be ASCII");
    }
    else if (! isCNTRL(result) && output_warning) {
	if (source == '{') {
	    Perl_ck_warner_d(aTHX_ packWARN2(WARN_DEPRECATED, WARN_SYNTAX),
			    "\"\\c{\" is deprecated and is more clearly written as \";\"");
	}
	else {
	    U8 clearer[3];
	    U8 i = 0;
	    if (! isWORDCHAR(result)) {
		clearer[i++] = '\\';
	    }
	    clearer[i++] = result;
	    clearer[i++] = '\0';

	    Perl_ck_warner(aTHX_ packWARN(WARN_SYNTAX),
			    "\"\\c%c\" is more clearly written simply as \"%s\"",
			    source,
			    clearer);
	}
    }

    return result;
}

STATIC bool
S_grok_bslash_o(pTHX_ char **s, UV *uv, const char** error_msg,
                      const bool output_warning, const bool strict,
                      const bool silence_non_portable,
                      const bool UTF)
{

/*  Documentation to be supplied when interface nailed down finally
 *  This returns FALSE if there is an error which the caller need not recover
 *  from; , otherwise TRUE.  In either case the caller should look at *len
 *  On input:
 *	s   is the address of a pointer to a NULL terminated string that begins
 *	    with 'o', and the previous character was a backslash.  At exit, *s
 *	    will be advanced to the byte just after those absorbed by this
 *	    function.  Hence the caller can continue parsing from there.  In
 *	    the case of an error, this routine has generally positioned *s to
 *	    point just to the right of the first bad spot, so that a message
 *	    that has a "<--" to mark the spot will be correctly positioned.
 *	uv  points to a UV that will hold the output value, valid only if the
 *	    return from the function is TRUE
 *      error_msg is a pointer that will be set to an internal buffer giving an
 *	    error message upon failure (the return is FALSE).  Untouched if
 *	    function succeeds
 *	output_warning says whether to output any warning messages, or suppress
 *	    them
 *	strict is true if this should fail instead of warn if there are
 *	    non-octal digits within the braces
 *      silence_non_portable is true if to suppress warnings about the code
 *          point returned being too large to fit on all platforms.
 *	UTF is true iff the string *s is encoded in UTF-8.
 */
    char* e;
    STRLEN numbers_len;
    I32 flags = PERL_SCAN_ALLOW_UNDERSCORES
		| PERL_SCAN_DISALLOW_PREFIX
		/* XXX Until the message is improved in grok_oct, handle errors
		 * ourselves */
	        | PERL_SCAN_SILENT_ILLDIGIT;

    PERL_ARGS_ASSERT_GROK_BSLASH_O;


    assert(**s == 'o');
    (*s)++;

    if (**s != '{') {
	*error_msg = "Missing braces on \\o{}";
	return FALSE;
    }

    e = strchr(*s, '}');
    if (!e) {
        (*s)++;  /* Move past the '{' */
        while (isOCTAL(**s)) { /* Position beyond the legal digits */
            (*s)++;
        }
        *error_msg = "Missing right brace on \\o{";
	return FALSE;
    }

    (*s)++;    /* Point to expected first digit (could be first byte of utf8
                  sequence if not a digit) */
    numbers_len = e - *s;
    if (numbers_len == 0) {
        (*s)++;    /* Move past the } */
	*error_msg = "Number with no digits";
	return FALSE;
    }

    if (silence_non_portable) {
        flags |= PERL_SCAN_SILENT_NON_PORTABLE;
    }

    *uv = grok_oct(*s, &numbers_len, &flags, NULL);
    /* Note that if has non-octal, will ignore everything starting with that up
     * to the '}' */

    if (numbers_len != (STRLEN) (e - *s)) {
        if (strict) {
            *s += numbers_len;
            *s += (UTF) ? UTF8SKIP(*s) : (STRLEN) 1;
            *error_msg = "Non-octal character";
            return FALSE;
        }
        else if (output_warning) {
            Perl_ck_warner(aTHX_ packWARN(WARN_DIGIT),
            /* diag_listed_as: Non-octal character '%c'.  Resolved as "%s" */
                        "Non-octal character '%c'.  Resolved as \"\\o{%.*s}\"",
                        *(*s + numbers_len),
                        (int) numbers_len,
                        *s);
        }
    }

    /* Return past the '}' */
    *s = e + 1;

    return TRUE;
}

PERL_STATIC_INLINE bool
S_grok_bslash_x(pTHX_ char **s, UV *uv, const char** error_msg,
                      const bool output_warning, const bool strict,
                      const bool silence_non_portable,
                      const bool UTF)
{

/*  Documentation to be supplied when interface nailed down finally
 *  This returns FALSE if there is an error which the caller need not recover
 *  from; , otherwise TRUE.  In either case the caller should look at *len
 *  On input:
 *	s   is the address of a pointer to a NULL terminated string that begins
 *	    with 'x', and the previous character was a backslash.  At exit, *s
 *	    will be advanced to the byte just after those absorbed by this
 *	    function.  Hence the caller can continue parsing from there.  In
 *	    the case of an error, this routine has generally positioned *s to
 *	    point just to the right of the first bad spot, so that a message
 *	    that has a "<--" to mark the spot will be correctly positioned.
 *	uv  points to a UV that will hold the output value, valid only if the
 *	    return from the function is TRUE
 *      error_msg is a pointer that will be set to an internal buffer giving an
 *	    error message upon failure (the return is FALSE).  Untouched if
 *	    function succeeds
 *	output_warning says whether to output any warning messages, or suppress
 *	    them
 *	strict is true if anything out of the ordinary should cause this to
 *	    fail instead of warn or be silent.  For example, it requires
 *	    exactly 2 digits following the \x (when there are no braces).
 *	    3 digits could be a mistake, so is forbidden in this mode.
 *      silence_non_portable is true if to suppress warnings about the code
 *          point returned being too large to fit on all platforms.
 *	UTF is true iff the string *s is encoded in UTF-8.
 */
    char* e;
    STRLEN numbers_len;
    I32 flags = PERL_SCAN_DISALLOW_PREFIX;

    PERL_ARGS_ASSERT_GROK_BSLASH_X;

    PERL_UNUSED_ARG(output_warning);

    assert(**s == 'x');
    (*s)++;

    if (strict) {
        flags |= PERL_SCAN_SILENT_ILLDIGIT;
    }

    if (**s != '{') {
        STRLEN len = (strict) ? 3 : 2;

	*uv = grok_hex(*s, &len, &flags, NULL);
	*s += len;
        if (strict && len != 2) {
            if (len < 2) {
                *s += (UTF) ? UTF8SKIP(*s) : 1;
                *error_msg = "Non-hex character";
            }
            else {
                *error_msg = "Use \\x{...} for more than two hex characters";
            }
            return FALSE;
        }
	return TRUE;
    }

    e = strchr(*s, '}');
    if (!e) {
        (*s)++;  /* Move past the '{' */
        while (isXDIGIT(**s)) { /* Position beyond the legal digits */
            (*s)++;
        }
        /* XXX The corresponding message above for \o is just '\\o{'; other
         * messages for other constructs include the '}', so are inconsistent.
         */
	*error_msg = "Missing right brace on \\x{}";
	return FALSE;
    }

    (*s)++;    /* Point to expected first digit (could be first byte of utf8
                  sequence if not a digit) */
    numbers_len = e - *s;
    if (numbers_len == 0) {
        if (strict) {
            (*s)++;    /* Move past the } */
            *error_msg = "Number with no digits";
            return FALSE;
        }
        return TRUE;
    }

    flags |= PERL_SCAN_ALLOW_UNDERSCORES;
    if (silence_non_portable) {
        flags |= PERL_SCAN_SILENT_NON_PORTABLE;
    }

    *uv = grok_hex(*s, &numbers_len, &flags, NULL);
    /* Note that if has non-hex, will ignore everything starting with that up
     * to the '}' */

    if (strict && numbers_len != (STRLEN) (e - *s)) {
        *s += numbers_len;
        *s += (UTF) ? UTF8SKIP(*s) : 1;
        *error_msg = "Non-hex character";
        return FALSE;
    }

    /* Return past the '}' */
    *s = e + 1;

    return TRUE;
}

STATIC char*
S_form_short_octal_warning(pTHX_
                           const char * const s, /* Points to first non-octal */
                           const STRLEN len      /* Length of octals string, so
                                                    (s-len) points to first
                                                    octal */
) {
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

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
