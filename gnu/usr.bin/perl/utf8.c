/*    utf8.c
 *
 *    Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'What a fix!' said Sam.  'That's the one place in all the lands we've ever
 *  heard of that we don't want to see any closer; and that's the one place
 *  we're trying to get to!  And that's just where we can't get, nohow.'
 *
 *     [p.603 of _The Lord of the Rings_, IV/I: "The Taming of Sméagol"]
 *
 * 'Well do I understand your speech,' he answered in the same language;
 * 'yet few strangers do so.  Why then do you not speak in the Common Tongue,
 *  as is the custom in the West, if you wish to be answered?'
 *                           --Gandalf, addressing Théoden's door wardens
 *
 *     [p.508 of _The Lord of the Rings_, III/vi: "The King of the Golden Hall"]
 *
 * ...the travellers perceived that the floor was paved with stones of many
 * hues; branching runes and strange devices intertwined beneath their feet.
 *
 *     [p.512 of _The Lord of the Rings_, III/vi: "The King of the Golden Hall"]
 */

#include "EXTERN.h"
#define PERL_IN_UTF8_C
#include "perl.h"

#ifndef EBCDIC
/* Separate prototypes needed because in ASCII systems these
 * usually macros but they still are compiled as code, too. */
PERL_CALLCONV UV	Perl_utf8n_to_uvchr(pTHX_ const U8 *s, STRLEN curlen, STRLEN *retlen, U32 flags);
PERL_CALLCONV U8*	Perl_uvchr_to_utf8(pTHX_ U8 *d, UV uv);
#endif

static const char unees[] =
    "Malformed UTF-8 character (unexpected end of string)";

/* 
=head1 Unicode Support

This file contains various utility functions for manipulating UTF8-encoded
strings. For the uninitiated, this is a method of representing arbitrary
Unicode characters as a variable number of bytes, in such a way that
characters in the ASCII range are unmodified, and a zero byte never appears
within non-zero characters.

=cut
*/

/*
=for apidoc is_ascii_string

Returns true if first C<len> bytes of the given string are ASCII (i.e. none
of them even raise the question of UTF-8-ness).

See also is_utf8_string(), is_utf8_string_loclen(), and is_utf8_string_loc().

=cut
*/

bool
Perl_is_ascii_string(const U8 *s, STRLEN len)
{
    const U8* const send = s + (len ? len : strlen((const char *)s));
    const U8* x = s;

    PERL_ARGS_ASSERT_IS_ASCII_STRING;

    for (; x < send; ++x) {
	if (!UTF8_IS_INVARIANT(*x))
	    break;
    }

    return x == send;
}

/*
=for apidoc uvuni_to_utf8_flags

Adds the UTF-8 representation of the Unicode codepoint C<uv> to the end
of the string C<d>; C<d> should be have at least C<UTF8_MAXBYTES+1> free
bytes available. The return value is the pointer to the byte after the
end of the new character. In other words,

    d = uvuni_to_utf8_flags(d, uv, flags);

or, in most cases,

    d = uvuni_to_utf8(d, uv);

(which is equivalent to)

    d = uvuni_to_utf8_flags(d, uv, 0);

is the recommended Unicode-aware way of saying

    *(d++) = uv;

=cut
*/

U8 *
Perl_uvuni_to_utf8_flags(pTHX_ U8 *d, UV uv, UV flags)
{
    PERL_ARGS_ASSERT_UVUNI_TO_UTF8_FLAGS;

    if (ckWARN(WARN_UTF8)) {
	 if (UNICODE_IS_SURROGATE(uv) &&
	     !(flags & UNICODE_ALLOW_SURROGATE))
	      Perl_warner(aTHX_ packWARN(WARN_UTF8), "UTF-16 surrogate 0x%04"UVxf, uv);
	 else if (
		  ((uv >= 0xFDD0 && uv <= 0xFDEF &&
		    !(flags & UNICODE_ALLOW_FDD0))
		   ||
		   ((uv & 0xFFFE) == 0xFFFE && /* Either FFFE or FFFF. */
		    !(flags & UNICODE_ALLOW_FFFF))) &&
		  /* UNICODE_ALLOW_SUPER includes
		   * FFFEs and FFFFs beyond 0x10FFFF. */
		  ((uv <= PERL_UNICODE_MAX) ||
		   !(flags & UNICODE_ALLOW_SUPER))
		  )
	      Perl_warner(aTHX_ packWARN(WARN_UTF8),
		      "Unicode non-character 0x%04"UVxf" is illegal for interchange", uv);
    }
    if (UNI_IS_INVARIANT(uv)) {
	*d++ = (U8)UTF_TO_NATIVE(uv);
	return d;
    }
#if defined(EBCDIC)
    else {
	STRLEN len  = UNISKIP(uv);
	U8 *p = d+len-1;
	while (p > d) {
	    *p-- = (U8)UTF_TO_NATIVE((uv & UTF_CONTINUATION_MASK) | UTF_CONTINUATION_MARK);
	    uv >>= UTF_ACCUMULATION_SHIFT;
	}
	*p = (U8)UTF_TO_NATIVE((uv & UTF_START_MASK(len)) | UTF_START_MARK(len));
	return d+len;
    }
#else /* Non loop style */
    if (uv < 0x800) {
	*d++ = (U8)(( uv >>  6)         | 0xc0);
	*d++ = (U8)(( uv        & 0x3f) | 0x80);
	return d;
    }
    if (uv < 0x10000) {
	*d++ = (U8)(( uv >> 12)         | 0xe0);
	*d++ = (U8)(((uv >>  6) & 0x3f) | 0x80);
	*d++ = (U8)(( uv        & 0x3f) | 0x80);
	return d;
    }
    if (uv < 0x200000) {
	*d++ = (U8)(( uv >> 18)         | 0xf0);
	*d++ = (U8)(((uv >> 12) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >>  6) & 0x3f) | 0x80);
	*d++ = (U8)(( uv        & 0x3f) | 0x80);
	return d;
    }
    if (uv < 0x4000000) {
	*d++ = (U8)(( uv >> 24)         | 0xf8);
	*d++ = (U8)(((uv >> 18) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 12) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >>  6) & 0x3f) | 0x80);
	*d++ = (U8)(( uv        & 0x3f) | 0x80);
	return d;
    }
    if (uv < 0x80000000) {
	*d++ = (U8)(( uv >> 30)         | 0xfc);
	*d++ = (U8)(((uv >> 24) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 18) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 12) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >>  6) & 0x3f) | 0x80);
	*d++ = (U8)(( uv        & 0x3f) | 0x80);
	return d;
    }
#ifdef HAS_QUAD
    if (uv < UTF8_QUAD_MAX)
#endif
    {
	*d++ =                            0xfe;	/* Can't match U+FEFF! */
	*d++ = (U8)(((uv >> 30) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 24) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 18) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 12) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >>  6) & 0x3f) | 0x80);
	*d++ = (U8)(( uv        & 0x3f) | 0x80);
	return d;
    }
#ifdef HAS_QUAD
    {
	*d++ =                            0xff;		/* Can't match U+FFFE! */
	*d++ =                            0x80;		/* 6 Reserved bits */
	*d++ = (U8)(((uv >> 60) & 0x0f) | 0x80);	/* 2 Reserved bits */
	*d++ = (U8)(((uv >> 54) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 48) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 42) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 36) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 30) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 24) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 18) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >> 12) & 0x3f) | 0x80);
	*d++ = (U8)(((uv >>  6) & 0x3f) | 0x80);
	*d++ = (U8)(( uv        & 0x3f) | 0x80);
	return d;
    }
#endif
#endif /* Loop style */
}

/*

Tests if some arbitrary number of bytes begins in a valid UTF-8
character.  Note that an INVARIANT (i.e. ASCII) character is a valid
UTF-8 character.  The actual number of bytes in the UTF-8 character
will be returned if it is valid, otherwise 0.

This is the "slow" version as opposed to the "fast" version which is
the "unrolled" IS_UTF8_CHAR().  E.g. for t/uni/class.t the speed
difference is a factor of 2 to 3.  For lengths (UTF8SKIP(s)) of four
or less you should use the IS_UTF8_CHAR(), for lengths of five or more
you should use the _slow().  In practice this means that the _slow()
will be used very rarely, since the maximum Unicode code point (as of
Unicode 4.1) is U+10FFFF, which encodes in UTF-8 to four bytes.  Only
the "Perl extended UTF-8" (the infamous 'v-strings') will encode into
five bytes or more.

=cut */
STATIC STRLEN
S_is_utf8_char_slow(const U8 *s, const STRLEN len)
{
    U8 u = *s;
    STRLEN slen;
    UV uv, ouv;

    PERL_ARGS_ASSERT_IS_UTF8_CHAR_SLOW;

    if (UTF8_IS_INVARIANT(u))
	return 1;

    if (!UTF8_IS_START(u))
	return 0;

    if (len < 2 || !UTF8_IS_CONTINUATION(s[1]))
	return 0;

    slen = len - 1;
    s++;
#ifdef EBCDIC
    u = NATIVE_TO_UTF(u);
#endif
    u &= UTF_START_MASK(len);
    uv  = u;
    ouv = uv;
    while (slen--) {
	if (!UTF8_IS_CONTINUATION(*s))
	    return 0;
	uv = UTF8_ACCUMULATE(uv, *s);
	if (uv < ouv) 
	    return 0;
	ouv = uv;
	s++;
    }

    if ((STRLEN)UNISKIP(uv) < len)
	return 0;

    return len;
}

/*
=for apidoc is_utf8_char

Tests if some arbitrary number of bytes begins in a valid UTF-8
character.  Note that an INVARIANT (i.e. ASCII on non-EBCDIC machines)
character is a valid UTF-8 character.  The actual number of bytes in the UTF-8
character will be returned if it is valid, otherwise 0.

=cut */
STRLEN
Perl_is_utf8_char(const U8 *s)
{
    const STRLEN len = UTF8SKIP(s);

    PERL_ARGS_ASSERT_IS_UTF8_CHAR;
#ifdef IS_UTF8_CHAR
    if (IS_UTF8_CHAR_FAST(len))
        return IS_UTF8_CHAR(s, len) ? len : 0;
#endif /* #ifdef IS_UTF8_CHAR */
    return is_utf8_char_slow(s, len);
}


/*
=for apidoc is_utf8_string

Returns true if first C<len> bytes of the given string form a valid
UTF-8 string, false otherwise.  Note that 'a valid UTF-8 string' does
not mean 'a string that contains code points above 0x7F encoded in UTF-8'
because a valid ASCII string is a valid UTF-8 string.

See also is_ascii_string(), is_utf8_string_loclen(), and is_utf8_string_loc().

=cut
*/

bool
Perl_is_utf8_string(const U8 *s, STRLEN len)
{
    const U8* const send = s + (len ? len : strlen((const char *)s));
    const U8* x = s;

    PERL_ARGS_ASSERT_IS_UTF8_STRING;

    while (x < send) {
	STRLEN c;
	 /* Inline the easy bits of is_utf8_char() here for speed... */
	 if (UTF8_IS_INVARIANT(*x))
	      c = 1;
	 else if (!UTF8_IS_START(*x))
	     goto out;
	 else {
	      /* ... and call is_utf8_char() only if really needed. */
#ifdef IS_UTF8_CHAR
	     c = UTF8SKIP(x);
	     if (IS_UTF8_CHAR_FAST(c)) {
	         if (!IS_UTF8_CHAR(x, c))
		     c = 0;
	     }
	     else
		c = is_utf8_char_slow(x, c);
#else
	     c = is_utf8_char(x);
#endif /* #ifdef IS_UTF8_CHAR */
	      if (!c)
		  goto out;
	 }
        x += c;
    }

 out:
    if (x != send)
	return FALSE;

    return TRUE;
}

/*
Implemented as a macro in utf8.h

=for apidoc is_utf8_string_loc

Like is_utf8_string() but stores the location of the failure (in the
case of "utf8ness failure") or the location s+len (in the case of
"utf8ness success") in the C<ep>.

See also is_utf8_string_loclen() and is_utf8_string().

=for apidoc is_utf8_string_loclen

Like is_utf8_string() but stores the location of the failure (in the
case of "utf8ness failure") or the location s+len (in the case of
"utf8ness success") in the C<ep>, and the number of UTF-8
encoded characters in the C<el>.

See also is_utf8_string_loc() and is_utf8_string().

=cut
*/

bool
Perl_is_utf8_string_loclen(const U8 *s, STRLEN len, const U8 **ep, STRLEN *el)
{
    const U8* const send = s + (len ? len : strlen((const char *)s));
    const U8* x = s;
    STRLEN c;
    STRLEN outlen = 0;

    PERL_ARGS_ASSERT_IS_UTF8_STRING_LOCLEN;

    while (x < send) {
	 /* Inline the easy bits of is_utf8_char() here for speed... */
	 if (UTF8_IS_INVARIANT(*x))
	     c = 1;
	 else if (!UTF8_IS_START(*x))
	     goto out;
	 else {
	     /* ... and call is_utf8_char() only if really needed. */
#ifdef IS_UTF8_CHAR
	     c = UTF8SKIP(x);
	     if (IS_UTF8_CHAR_FAST(c)) {
	         if (!IS_UTF8_CHAR(x, c))
		     c = 0;
	     } else
	         c = is_utf8_char_slow(x, c);
#else
	     c = is_utf8_char(x);
#endif /* #ifdef IS_UTF8_CHAR */
	     if (!c)
	         goto out;
	 }
         x += c;
	 outlen++;
    }

 out:
    if (el)
        *el = outlen;

    if (ep)
        *ep = x;
    return (x == send);
}

/*

=for apidoc utf8n_to_uvuni

Bottom level UTF-8 decode routine.
Returns the Unicode code point value of the first character in the string C<s>
which is assumed to be in UTF-8 encoding and no longer than C<curlen>;
C<retlen> will be set to the length, in bytes, of that character.

If C<s> does not point to a well-formed UTF-8 character, the behaviour
is dependent on the value of C<flags>: if it contains UTF8_CHECK_ONLY,
it is assumed that the caller will raise a warning, and this function
will silently just set C<retlen> to C<-1> and return zero.  If the
C<flags> does not contain UTF8_CHECK_ONLY, warnings about
malformations will be given, C<retlen> will be set to the expected
length of the UTF-8 character in bytes, and zero will be returned.

The C<flags> can also contain various flags to allow deviations from
the strict UTF-8 encoding (see F<utf8.h>).

Most code should use utf8_to_uvchr() rather than call this directly.

=cut
*/

UV
Perl_utf8n_to_uvuni(pTHX_ const U8 *s, STRLEN curlen, STRLEN *retlen, U32 flags)
{
    dVAR;
    const U8 * const s0 = s;
    UV uv = *s, ouv = 0;
    STRLEN len = 1;
    const bool dowarn = ckWARN_d(WARN_UTF8);
    const UV startbyte = *s;
    STRLEN expectlen = 0;
    U32 warning = 0;
    SV* sv;

    PERL_ARGS_ASSERT_UTF8N_TO_UVUNI;

/* This list is a superset of the UTF8_ALLOW_XXX.  BUT it isn't, eg SUPER missing XXX */

#define UTF8_WARN_EMPTY				 1
#define UTF8_WARN_CONTINUATION			 2
#define UTF8_WARN_NON_CONTINUATION	 	 3
#define UTF8_WARN_FE_FF				 4
#define UTF8_WARN_SHORT				 5
#define UTF8_WARN_OVERFLOW			 6
#define UTF8_WARN_SURROGATE			 7
#define UTF8_WARN_LONG				 8
#define UTF8_WARN_FFFF				 9 /* Also FFFE. */

    if (curlen == 0 &&
	!(flags & UTF8_ALLOW_EMPTY)) {
	warning = UTF8_WARN_EMPTY;
	goto malformed;
    }

    if (UTF8_IS_INVARIANT(uv)) {
	if (retlen)
	    *retlen = 1;
	return (UV) (NATIVE_TO_UTF(*s));
    }

    if (UTF8_IS_CONTINUATION(uv) &&
	!(flags & UTF8_ALLOW_CONTINUATION)) {
	warning = UTF8_WARN_CONTINUATION;
	goto malformed;
    }

    if (UTF8_IS_START(uv) && curlen > 1 && !UTF8_IS_CONTINUATION(s[1]) &&
	!(flags & UTF8_ALLOW_NON_CONTINUATION)) {
	warning = UTF8_WARN_NON_CONTINUATION;
	goto malformed;
    }

#ifdef EBCDIC
    uv = NATIVE_TO_UTF(uv);
#else
    if ((uv == 0xfe || uv == 0xff) &&
	!(flags & UTF8_ALLOW_FE_FF)) {
	warning = UTF8_WARN_FE_FF;
	goto malformed;
    }
#endif

    if      (!(uv & 0x20))	{ len =  2; uv &= 0x1f; }
    else if (!(uv & 0x10))	{ len =  3; uv &= 0x0f; }
    else if (!(uv & 0x08))	{ len =  4; uv &= 0x07; }
    else if (!(uv & 0x04))	{ len =  5; uv &= 0x03; }
#ifdef EBCDIC
    else if (!(uv & 0x02))	{ len =  6; uv &= 0x01; }
    else			{ len =  7; uv &= 0x01; }
#else
    else if (!(uv & 0x02))	{ len =  6; uv &= 0x01; }
    else if (!(uv & 0x01))	{ len =  7; uv = 0; }
    else			{ len = 13; uv = 0; } /* whoa! */
#endif

    if (retlen)
	*retlen = len;

    expectlen = len;

    if ((curlen < expectlen) &&
	!(flags & UTF8_ALLOW_SHORT)) {
	warning = UTF8_WARN_SHORT;
	goto malformed;
    }

    len--;
    s++;
    ouv = uv;

    while (len--) {
	if (!UTF8_IS_CONTINUATION(*s) &&
	    !(flags & UTF8_ALLOW_NON_CONTINUATION)) {
	    s--;
	    warning = UTF8_WARN_NON_CONTINUATION;
	    goto malformed;
	}
	else
	    uv = UTF8_ACCUMULATE(uv, *s);
	if (!(uv > ouv)) {
	    /* These cannot be allowed. */
	    if (uv == ouv) {
		if (expectlen != 13 && !(flags & UTF8_ALLOW_LONG)) {
		    warning = UTF8_WARN_LONG;
		    goto malformed;
		}
	    }
	    else { /* uv < ouv */
		/* This cannot be allowed. */
		warning = UTF8_WARN_OVERFLOW;
		goto malformed;
	    }
	}
	s++;
	ouv = uv;
    }

    if (UNICODE_IS_SURROGATE(uv) &&
	!(flags & UTF8_ALLOW_SURROGATE)) {
	warning = UTF8_WARN_SURROGATE;
	goto malformed;
    } else if ((expectlen > (STRLEN)UNISKIP(uv)) &&
	       !(flags & UTF8_ALLOW_LONG)) {
	warning = UTF8_WARN_LONG;
	goto malformed;
    } else if (UNICODE_IS_ILLEGAL(uv) &&
	       !(flags & UTF8_ALLOW_FFFF)) {
	warning = UTF8_WARN_FFFF;
	goto malformed;
    }

    return uv;

malformed:

    if (flags & UTF8_CHECK_ONLY) {
	if (retlen)
	    *retlen = ((STRLEN) -1);
	return 0;
    }

    if (dowarn) {
	if (warning == UTF8_WARN_FFFF) {
	    sv = newSVpvs_flags("Unicode non-character ", SVs_TEMP);
	    Perl_sv_catpvf(aTHX_ sv, "0x%04"UVxf" is illegal for interchange", uv);
	}
	else {
	    sv = newSVpvs_flags("Malformed UTF-8 character ", SVs_TEMP);

	    switch (warning) {
		case 0: /* Intentionally empty. */ break;
		case UTF8_WARN_EMPTY:
		    sv_catpvs(sv, "(empty string)");
		    break;
		case UTF8_WARN_CONTINUATION:
		    Perl_sv_catpvf(aTHX_ sv, "(unexpected continuation byte 0x%02"UVxf", with no preceding start byte)", uv);
		    break;
		case UTF8_WARN_NON_CONTINUATION:
		    if (s == s0)
			Perl_sv_catpvf(aTHX_ sv, "(unexpected non-continuation byte 0x%02"UVxf", immediately after start byte 0x%02"UVxf")",
				   (UV)s[1], startbyte);
		    else {
			const int len = (int)(s-s0);
			Perl_sv_catpvf(aTHX_ sv, "(unexpected non-continuation byte 0x%02"UVxf", %d byte%s after start byte 0x%02"UVxf", expected %d bytes)",
				   (UV)s[1], len, len > 1 ? "s" : "", startbyte, (int)expectlen);
		    }

		    break;
		case UTF8_WARN_FE_FF:
		    Perl_sv_catpvf(aTHX_ sv, "(byte 0x%02"UVxf")", uv);
		    break;
		case UTF8_WARN_SHORT:
		    Perl_sv_catpvf(aTHX_ sv, "(%d byte%s, need %d, after start byte 0x%02"UVxf")",
				   (int)curlen, curlen == 1 ? "" : "s", (int)expectlen, startbyte);
		    expectlen = curlen;		/* distance for caller to skip */
		    break;
		case UTF8_WARN_OVERFLOW:
		    Perl_sv_catpvf(aTHX_ sv, "(overflow at 0x%"UVxf", byte 0x%02x, after start byte 0x%02"UVxf")",
				   ouv, *s, startbyte);
		    break;
		case UTF8_WARN_SURROGATE:
		    Perl_sv_catpvf(aTHX_ sv, "(UTF-16 surrogate 0x%04"UVxf")", uv);
		    break;
		case UTF8_WARN_LONG:
		    Perl_sv_catpvf(aTHX_ sv, "(%d byte%s, need %d, after start byte 0x%02"UVxf")",
				   (int)expectlen, expectlen == 1 ? "": "s", UNISKIP(uv), startbyte);
		    break;
		default:
		    sv_catpvs(sv, "(unknown reason)");
		    break;
	    }
	}
	
	if (warning) {
	    const char * const s = SvPVX_const(sv);

	    if (PL_op)
		Perl_warner(aTHX_ packWARN(WARN_UTF8),
			    "%s in %s", s,  OP_DESC(PL_op));
	    else
		Perl_warner(aTHX_ packWARN(WARN_UTF8), "%s", s);
	}
    }

    if (retlen)
	*retlen = expectlen ? expectlen : len;

    return 0;
}

/*
=for apidoc utf8_to_uvchr

Returns the native character value of the first character in the string C<s>
which is assumed to be in UTF-8 encoding; C<retlen> will be set to the
length, in bytes, of that character.

If C<s> does not point to a well-formed UTF-8 character, zero is
returned and retlen is set, if possible, to -1.

=cut
*/

UV
Perl_utf8_to_uvchr(pTHX_ const U8 *s, STRLEN *retlen)
{
    PERL_ARGS_ASSERT_UTF8_TO_UVCHR;

    return utf8n_to_uvchr(s, UTF8_MAXBYTES, retlen,
			  ckWARN(WARN_UTF8) ? 0 : UTF8_ALLOW_ANY);
}

/*
=for apidoc utf8_to_uvuni

Returns the Unicode code point of the first character in the string C<s>
which is assumed to be in UTF-8 encoding; C<retlen> will be set to the
length, in bytes, of that character.

This function should only be used when the returned UV is considered
an index into the Unicode semantic tables (e.g. swashes).

If C<s> does not point to a well-formed UTF-8 character, zero is
returned and retlen is set, if possible, to -1.

=cut
*/

UV
Perl_utf8_to_uvuni(pTHX_ const U8 *s, STRLEN *retlen)
{
    PERL_ARGS_ASSERT_UTF8_TO_UVUNI;

    /* Call the low level routine asking for checks */
    return Perl_utf8n_to_uvuni(aTHX_ s, UTF8_MAXBYTES, retlen,
			       ckWARN(WARN_UTF8) ? 0 : UTF8_ALLOW_ANY);
}

/*
=for apidoc utf8_length

Return the length of the UTF-8 char encoded string C<s> in characters.
Stops at C<e> (inclusive).  If C<e E<lt> s> or if the scan would end
up past C<e>, croaks.

=cut
*/

STRLEN
Perl_utf8_length(pTHX_ const U8 *s, const U8 *e)
{
    dVAR;
    STRLEN len = 0;

    PERL_ARGS_ASSERT_UTF8_LENGTH;

    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g.
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    if (e < s)
	goto warn_and_return;
    while (s < e) {
	if (!UTF8_IS_INVARIANT(*s))
	    s += UTF8SKIP(s);
	else
	    s++;
	len++;
    }

    if (e != s) {
	len--;
        warn_and_return:
	if (PL_op)
	    Perl_ck_warner_d(aTHX_ packWARN(WARN_UTF8),
			     "%s in %s", unees, OP_DESC(PL_op));
	else
	    Perl_ck_warner_d(aTHX_ packWARN(WARN_UTF8), unees);
    }

    return len;
}

/*
=for apidoc utf8_distance

Returns the number of UTF-8 characters between the UTF-8 pointers C<a>
and C<b>.

WARNING: use only if you *know* that the pointers point inside the
same UTF-8 buffer.

=cut
*/

IV
Perl_utf8_distance(pTHX_ const U8 *a, const U8 *b)
{
    PERL_ARGS_ASSERT_UTF8_DISTANCE;

    return (a < b) ? -1 * (IV) utf8_length(a, b) : (IV) utf8_length(b, a);
}

/*
=for apidoc utf8_hop

Return the UTF-8 pointer C<s> displaced by C<off> characters, either
forward or backward.

WARNING: do not use the following unless you *know* C<off> is within
the UTF-8 data pointed to by C<s> *and* that on entry C<s> is aligned
on the first byte of character or just after the last byte of a character.

=cut
*/

U8 *
Perl_utf8_hop(pTHX_ const U8 *s, I32 off)
{
    PERL_ARGS_ASSERT_UTF8_HOP;

    PERL_UNUSED_CONTEXT;
    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    if (off >= 0) {
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
    return (U8 *)s;
}

/*
=for apidoc utf8_to_bytes

Converts a string C<s> of length C<len> from UTF-8 into native byte encoding.
Unlike C<bytes_to_utf8>, this over-writes the original string, and
updates len to contain the new length.
Returns zero on failure, setting C<len> to -1.

If you need a copy of the string, see C<bytes_from_utf8>.

=cut
*/

U8 *
Perl_utf8_to_bytes(pTHX_ U8 *s, STRLEN *len)
{
    U8 * const save = s;
    U8 * const send = s + *len;
    U8 *d;

    PERL_ARGS_ASSERT_UTF8_TO_BYTES;

    /* ensure valid UTF-8 and chars < 256 before updating string */
    while (s < send) {
        U8 c = *s++;

        if (!UTF8_IS_INVARIANT(c) &&
            (!UTF8_IS_DOWNGRADEABLE_START(c) || (s >= send)
	     || !(c = *s++) || !UTF8_IS_CONTINUATION(c))) {
            *len = ((STRLEN) -1);
            return 0;
        }
    }

    d = s = save;
    while (s < send) {
        STRLEN ulen;
        *d++ = (U8)utf8_to_uvchr(s, &ulen);
        s += ulen;
    }
    *d = '\0';
    *len = d - save;
    return save;
}

/*
=for apidoc bytes_from_utf8

Converts a string C<s> of length C<len> from UTF-8 into native byte encoding.
Unlike C<utf8_to_bytes> but like C<bytes_to_utf8>, returns a pointer to
the newly-created string, and updates C<len> to contain the new
length.  Returns the original string if no conversion occurs, C<len>
is unchanged. Do nothing if C<is_utf8> points to 0. Sets C<is_utf8> to
0 if C<s> is converted or consisted entirely of characters that are invariant
in utf8 (i.e., US-ASCII on non-EBCDIC machines).

=cut
*/

U8 *
Perl_bytes_from_utf8(pTHX_ const U8 *s, STRLEN *len, bool *is_utf8)
{
    U8 *d;
    const U8 *start = s;
    const U8 *send;
    I32 count = 0;

    PERL_ARGS_ASSERT_BYTES_FROM_UTF8;

    PERL_UNUSED_CONTEXT;
    if (!*is_utf8)
        return (U8 *)start;

    /* ensure valid UTF-8 and chars < 256 before converting string */
    for (send = s + *len; s < send;) {
        U8 c = *s++;
	if (!UTF8_IS_INVARIANT(c)) {
	    if (UTF8_IS_DOWNGRADEABLE_START(c) && s < send &&
                (c = *s++) && UTF8_IS_CONTINUATION(c))
		count++;
	    else
                return (U8 *)start;
	}
    }

    *is_utf8 = FALSE;

    Newx(d, (*len) - count + 1, U8);
    s = start; start = d;
    while (s < send) {
	U8 c = *s++;
	if (!UTF8_IS_INVARIANT(c)) {
	    /* Then it is two-byte encoded */
	    c = UTF8_ACCUMULATE(NATIVE_TO_UTF(c), *s++);
	    c = ASCII_TO_NATIVE(c);
	}
	*d++ = c;
    }
    *d = '\0';
    *len = d - start;
    return (U8 *)start;
}

/*
=for apidoc bytes_to_utf8

Converts a string C<s> of length C<len> from the native encoding into UTF-8.
Returns a pointer to the newly-created string, and sets C<len> to
reflect the new length.

A NUL character will be written after the end of the string.

If you want to convert to UTF-8 from encodings other than
the native (Latin1 or EBCDIC),
see sv_recode_to_utf8().

=cut
*/

U8*
Perl_bytes_to_utf8(pTHX_ const U8 *s, STRLEN *len)
{
    const U8 * const send = s + (*len);
    U8 *d;
    U8 *dst;

    PERL_ARGS_ASSERT_BYTES_TO_UTF8;
    PERL_UNUSED_CONTEXT;

    Newx(d, (*len) * 2 + 1, U8);
    dst = d;

    while (s < send) {
        const UV uv = NATIVE_TO_ASCII(*s++);
        if (UNI_IS_INVARIANT(uv))
            *d++ = (U8)UTF_TO_NATIVE(uv);
        else {
            *d++ = (U8)UTF8_EIGHT_BIT_HI(uv);
            *d++ = (U8)UTF8_EIGHT_BIT_LO(uv);
        }
    }
    *d = '\0';
    *len = d-dst;
    return dst;
}

/*
 * Convert native (big-endian) or reversed (little-endian) UTF-16 to UTF-8.
 *
 * Destination must be pre-extended to 3/2 source.  Do not use in-place.
 * We optimize for native, for obvious reasons. */

U8*
Perl_utf16_to_utf8(pTHX_ U8* p, U8* d, I32 bytelen, I32 *newlen)
{
    U8* pend;
    U8* dstart = d;

    PERL_ARGS_ASSERT_UTF16_TO_UTF8;

    if (bytelen & 1)
	Perl_croak(aTHX_ "panic: utf16_to_utf8: odd bytelen %"UVuf, (UV)bytelen);

    pend = p + bytelen;

    while (p < pend) {
	UV uv = (p[0] << 8) + p[1]; /* UTF-16BE */
	p += 2;
	if (uv < 0x80) {
#ifdef EBCDIC
	    *d++ = UNI_TO_NATIVE(uv);
#else
	    *d++ = (U8)uv;
#endif
	    continue;
	}
	if (uv < 0x800) {
	    *d++ = (U8)(( uv >>  6)         | 0xc0);
	    *d++ = (U8)(( uv        & 0x3f) | 0x80);
	    continue;
	}
	if (uv >= 0xd800 && uv <= 0xdbff) {	/* surrogates */
	    if (p >= pend) {
		Perl_croak(aTHX_ "Malformed UTF-16 surrogate");
	    } else {
		UV low = (p[0] << 8) + p[1];
		p += 2;
		if (low < 0xdc00 || low > 0xdfff)
		    Perl_croak(aTHX_ "Malformed UTF-16 surrogate");
		uv = ((uv - 0xd800) << 10) + (low - 0xdc00) + 0x10000;
	    }
	} else if (uv >= 0xdc00 && uv <= 0xdfff) {
	    Perl_croak(aTHX_ "Malformed UTF-16 surrogate");
	}
	if (uv < 0x10000) {
	    *d++ = (U8)(( uv >> 12)         | 0xe0);
	    *d++ = (U8)(((uv >>  6) & 0x3f) | 0x80);
	    *d++ = (U8)(( uv        & 0x3f) | 0x80);
	    continue;
	}
	else {
	    *d++ = (U8)(( uv >> 18)         | 0xf0);
	    *d++ = (U8)(((uv >> 12) & 0x3f) | 0x80);
	    *d++ = (U8)(((uv >>  6) & 0x3f) | 0x80);
	    *d++ = (U8)(( uv        & 0x3f) | 0x80);
	    continue;
	}
    }
    *newlen = d - dstart;
    return d;
}

/* Note: this one is slightly destructive of the source. */

U8*
Perl_utf16_to_utf8_reversed(pTHX_ U8* p, U8* d, I32 bytelen, I32 *newlen)
{
    U8* s = (U8*)p;
    U8* const send = s + bytelen;

    PERL_ARGS_ASSERT_UTF16_TO_UTF8_REVERSED;

    if (bytelen & 1)
	Perl_croak(aTHX_ "panic: utf16_to_utf8_reversed: odd bytelen %"UVuf,
		   (UV)bytelen);

    while (s < send) {
	const U8 tmp = s[0];
	s[0] = s[1];
	s[1] = tmp;
	s += 2;
    }
    return utf16_to_utf8(p, d, bytelen, newlen);
}

/* for now these are all defined (inefficiently) in terms of the utf8 versions */

bool
Perl_is_uni_alnum(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_alnum(tmpbuf);
}

bool
Perl_is_uni_idfirst(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_idfirst(tmpbuf);
}

bool
Perl_is_uni_alpha(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_alpha(tmpbuf);
}

bool
Perl_is_uni_ascii(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_ascii(tmpbuf);
}

bool
Perl_is_uni_space(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_space(tmpbuf);
}

bool
Perl_is_uni_digit(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_digit(tmpbuf);
}

bool
Perl_is_uni_upper(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_upper(tmpbuf);
}

bool
Perl_is_uni_lower(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_lower(tmpbuf);
}

bool
Perl_is_uni_cntrl(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_cntrl(tmpbuf);
}

bool
Perl_is_uni_graph(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_graph(tmpbuf);
}

bool
Perl_is_uni_print(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_print(tmpbuf);
}

bool
Perl_is_uni_punct(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_punct(tmpbuf);
}

bool
Perl_is_uni_xdigit(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_xdigit(tmpbuf);
}

UV
Perl_to_uni_upper(pTHX_ UV c, U8* p, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UNI_UPPER;

    uvchr_to_utf8(p, c);
    return to_utf8_upper(p, p, lenp);
}

UV
Perl_to_uni_title(pTHX_ UV c, U8* p, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UNI_TITLE;

    uvchr_to_utf8(p, c);
    return to_utf8_title(p, p, lenp);
}

UV
Perl_to_uni_lower(pTHX_ UV c, U8* p, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UNI_LOWER;

    uvchr_to_utf8(p, c);
    return to_utf8_lower(p, p, lenp);
}

UV
Perl_to_uni_fold(pTHX_ UV c, U8* p, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UNI_FOLD;

    uvchr_to_utf8(p, c);
    return to_utf8_fold(p, p, lenp);
}

/* for now these all assume no locale info available for Unicode > 255 */

bool
Perl_is_uni_alnum_lc(pTHX_ UV c)
{
    return is_uni_alnum(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_idfirst_lc(pTHX_ UV c)
{
    return is_uni_idfirst(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_alpha_lc(pTHX_ UV c)
{
    return is_uni_alpha(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_ascii_lc(pTHX_ UV c)
{
    return is_uni_ascii(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_space_lc(pTHX_ UV c)
{
    return is_uni_space(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_digit_lc(pTHX_ UV c)
{
    return is_uni_digit(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_upper_lc(pTHX_ UV c)
{
    return is_uni_upper(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_lower_lc(pTHX_ UV c)
{
    return is_uni_lower(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_cntrl_lc(pTHX_ UV c)
{
    return is_uni_cntrl(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_graph_lc(pTHX_ UV c)
{
    return is_uni_graph(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_print_lc(pTHX_ UV c)
{
    return is_uni_print(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_punct_lc(pTHX_ UV c)
{
    return is_uni_punct(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_xdigit_lc(pTHX_ UV c)
{
    return is_uni_xdigit(c);	/* XXX no locale support yet */
}

U32
Perl_to_uni_upper_lc(pTHX_ U32 c)
{
    /* XXX returns only the first character -- do not use XXX */
    /* XXX no locale support yet */
    STRLEN len;
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    return (U32)to_uni_upper(c, tmpbuf, &len);
}

U32
Perl_to_uni_title_lc(pTHX_ U32 c)
{
    /* XXX returns only the first character XXX -- do not use XXX */
    /* XXX no locale support yet */
    STRLEN len;
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    return (U32)to_uni_title(c, tmpbuf, &len);
}

U32
Perl_to_uni_lower_lc(pTHX_ U32 c)
{
    /* XXX returns only the first character -- do not use XXX */
    /* XXX no locale support yet */
    STRLEN len;
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    return (U32)to_uni_lower(c, tmpbuf, &len);
}

static bool
S_is_utf8_common(pTHX_ const U8 *const p, SV **swash,
		 const char *const swashname)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_COMMON;

    if (!is_utf8_char(p))
	return FALSE;
    if (!*swash)
	*swash = swash_init("utf8", swashname, &PL_sv_undef, 1, 0);
    return swash_fetch(*swash, p, TRUE) != 0;
}

bool
Perl_is_utf8_alnum(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_ALNUM;

    /* NOTE: "IsWord", not "IsAlnum", since Alnum is a true
     * descendant of isalnum(3), in other words, it doesn't
     * contain the '_'. --jhi */
    return is_utf8_common(p, &PL_utf8_alnum, "IsWord");
}

bool
Perl_is_utf8_idfirst(pTHX_ const U8 *p) /* The naming is historical. */
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_IDFIRST;

    if (*p == '_')
	return TRUE;
    /* is_utf8_idstart would be more logical. */
    return is_utf8_common(p, &PL_utf8_idstart, "IdStart");
}

bool
Perl_is_utf8_idcont(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_IDCONT;

    if (*p == '_')
	return TRUE;
    return is_utf8_common(p, &PL_utf8_idcont, "IdContinue");
}

bool
Perl_is_utf8_alpha(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_ALPHA;

    return is_utf8_common(p, &PL_utf8_alpha, "IsAlpha");
}

bool
Perl_is_utf8_ascii(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_ASCII;

    return is_utf8_common(p, &PL_utf8_ascii, "IsAscii");
}

bool
Perl_is_utf8_space(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_SPACE;

    return is_utf8_common(p, &PL_utf8_space, "IsSpacePerl");
}

bool
Perl_is_utf8_perl_space(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_PERL_SPACE;

    return is_utf8_common(p, &PL_utf8_perl_space, "IsPerlSpace");
}

bool
Perl_is_utf8_perl_word(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_PERL_WORD;

    return is_utf8_common(p, &PL_utf8_perl_word, "IsPerlWord");
}

bool
Perl_is_utf8_digit(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_DIGIT;

    return is_utf8_common(p, &PL_utf8_digit, "IsDigit");
}

bool
Perl_is_utf8_posix_digit(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_POSIX_DIGIT;

    return is_utf8_common(p, &PL_utf8_posix_digit, "IsPosixDigit");
}

bool
Perl_is_utf8_upper(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_UPPER;

    return is_utf8_common(p, &PL_utf8_upper, "IsUppercase");
}

bool
Perl_is_utf8_lower(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_LOWER;

    return is_utf8_common(p, &PL_utf8_lower, "IsLowercase");
}

bool
Perl_is_utf8_cntrl(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_CNTRL;

    return is_utf8_common(p, &PL_utf8_cntrl, "IsCntrl");
}

bool
Perl_is_utf8_graph(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_GRAPH;

    return is_utf8_common(p, &PL_utf8_graph, "IsGraph");
}

bool
Perl_is_utf8_print(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_PRINT;

    return is_utf8_common(p, &PL_utf8_print, "IsPrint");
}

bool
Perl_is_utf8_punct(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_PUNCT;

    return is_utf8_common(p, &PL_utf8_punct, "IsPunct");
}

bool
Perl_is_utf8_xdigit(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_XDIGIT;

    return is_utf8_common(p, &PL_utf8_xdigit, "IsXDigit");
}

bool
Perl_is_utf8_mark(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_MARK;

    return is_utf8_common(p, &PL_utf8_mark, "IsM");
}

bool
Perl_is_utf8_X_begin(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_BEGIN;

    return is_utf8_common(p, &PL_utf8_X_begin, "_X_Begin");
}

bool
Perl_is_utf8_X_extend(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_EXTEND;

    return is_utf8_common(p, &PL_utf8_X_extend, "_X_Extend");
}

bool
Perl_is_utf8_X_prepend(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_PREPEND;

    return is_utf8_common(p, &PL_utf8_X_prepend, "GCB=Prepend");
}

bool
Perl_is_utf8_X_non_hangul(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_NON_HANGUL;

    return is_utf8_common(p, &PL_utf8_X_non_hangul, "HST=Not_Applicable");
}

bool
Perl_is_utf8_X_L(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_L;

    return is_utf8_common(p, &PL_utf8_X_L, "GCB=L");
}

bool
Perl_is_utf8_X_LV(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_LV;

    return is_utf8_common(p, &PL_utf8_X_LV, "GCB=LV");
}

bool
Perl_is_utf8_X_LVT(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_LVT;

    return is_utf8_common(p, &PL_utf8_X_LVT, "GCB=LVT");
}

bool
Perl_is_utf8_X_T(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_T;

    return is_utf8_common(p, &PL_utf8_X_T, "GCB=T");
}

bool
Perl_is_utf8_X_V(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_V;

    return is_utf8_common(p, &PL_utf8_X_V, "GCB=V");
}

bool
Perl_is_utf8_X_LV_LVT_V(pTHX_ const U8 *p)
{
    dVAR;

    PERL_ARGS_ASSERT_IS_UTF8_X_LV_LVT_V;

    return is_utf8_common(p, &PL_utf8_X_LV_LVT_V, "_X_LV_LVT_V");
}

/*
=for apidoc to_utf8_case

The "p" contains the pointer to the UTF-8 string encoding
the character that is being converted.

The "ustrp" is a pointer to the character buffer to put the
conversion result to.  The "lenp" is a pointer to the length
of the result.

The "swashp" is a pointer to the swash to use.

Both the special and normal mappings are stored lib/unicore/To/Foo.pl,
and loaded by SWASHNEW, using lib/utf8_heavy.pl.  The special (usually,
but not always, a multicharacter mapping), is tried first.

The "special" is a string like "utf8::ToSpecLower", which means the
hash %utf8::ToSpecLower.  The access to the hash is through
Perl_to_utf8_case().

The "normal" is a string like "ToLower" which means the swash
%utf8::ToLower.

=cut */

UV
Perl_to_utf8_case(pTHX_ const U8 *p, U8* ustrp, STRLEN *lenp,
			SV **swashp, const char *normal, const char *special)
{
    dVAR;
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    STRLEN len = 0;
    const UV uv0 = utf8_to_uvchr(p, NULL);
    /* The NATIVE_TO_UNI() and UNI_TO_NATIVE() mappings
     * are necessary in EBCDIC, they are redundant no-ops
     * in ASCII-ish platforms, and hopefully optimized away. */
    const UV uv1 = NATIVE_TO_UNI(uv0);

    PERL_ARGS_ASSERT_TO_UTF8_CASE;

    uvuni_to_utf8(tmpbuf, uv1);

    if (!*swashp) /* load on-demand */
         *swashp = swash_init("utf8", normal, &PL_sv_undef, 4, 0);
    /* This is the beginnings of a skeleton of code to read the info section
     * that is in all the swashes in case we ever want to do that, so one can
     * read things whose maps aren't code points, and whose default if missing
     * is not to the code point itself.  This was just to see if it actually
     * worked.  Details on what the possibilities are are in perluniprops.pod
	HV * const hv = get_hv("utf8::SwashInfo", 0);
	if (hv) {
	 SV **svp;
	 svp = hv_fetch(hv, (const char*)normal, strlen(normal), FALSE);
	     const char *s;

	      HV * const this_hash = SvRV(*svp);
		svp = hv_fetch(this_hash, "type", strlen("type"), FALSE);
	      s = SvPV_const(*svp, len);
	}
    }*/

    /* The 0xDF is the only special casing Unicode code point below 0x100. */
    if (special && (uv1 == 0xDF || uv1 > 0xFF)) {
         /* It might be "special" (sometimes, but not always,
	  * a multicharacter mapping) */
	 HV * const hv = get_hv(special, 0);
	 SV **svp;

	 if (hv &&
	     (svp = hv_fetch(hv, (const char*)tmpbuf, UNISKIP(uv1), FALSE)) &&
	     (*svp)) {
	     const char *s;

	      s = SvPV_const(*svp, len);
	      if (len == 1)
		   len = uvuni_to_utf8(ustrp, NATIVE_TO_UNI(*(U8*)s)) - ustrp;
	      else {
#ifdef EBCDIC
		   /* If we have EBCDIC we need to remap the characters
		    * since any characters in the low 256 are Unicode
		    * code points, not EBCDIC. */
		   U8 *t = (U8*)s, *tend = t + len, *d;
		
		   d = tmpbuf;
		   if (SvUTF8(*svp)) {
			STRLEN tlen = 0;
			
			while (t < tend) {
			     const UV c = utf8_to_uvchr(t, &tlen);
			     if (tlen > 0) {
				  d = uvchr_to_utf8(d, UNI_TO_NATIVE(c));
				  t += tlen;
			     }
			     else
				  break;
			}
		   }
		   else {
			while (t < tend) {
			     d = uvchr_to_utf8(d, UNI_TO_NATIVE(*t));
			     t++;
			}
		   }
		   len = d - tmpbuf;
		   Copy(tmpbuf, ustrp, len, U8);
#else
		   Copy(s, ustrp, len, U8);
#endif
	      }
	 }
    }

    if (!len && *swashp) {
	const UV uv2 = swash_fetch(*swashp, tmpbuf, TRUE);

	 if (uv2) {
	      /* It was "normal" (a single character mapping). */
	      const UV uv3 = UNI_TO_NATIVE(uv2);
	      len = uvchr_to_utf8(ustrp, uv3) - ustrp;
	 }
    }

    if (!len) /* Neither: just copy.  In other words, there was no mapping
		 defined, which means that the code point maps to itself */
	 len = uvchr_to_utf8(ustrp, uv0) - ustrp;

    if (lenp)
	 *lenp = len;

    return len ? utf8_to_uvchr(ustrp, 0) : 0;
}

/*
=for apidoc to_utf8_upper

Convert the UTF-8 encoded character at p to its uppercase version and
store that in UTF-8 in ustrp and its length in bytes in lenp.  Note
that the ustrp needs to be at least UTF8_MAXBYTES_CASE+1 bytes since
the uppercase version may be longer than the original character.

The first character of the uppercased version is returned
(but note, as explained above, that there may be more.)

=cut */

UV
Perl_to_utf8_upper(pTHX_ const U8 *p, U8* ustrp, STRLEN *lenp)
{
    dVAR;

    PERL_ARGS_ASSERT_TO_UTF8_UPPER;

    return Perl_to_utf8_case(aTHX_ p, ustrp, lenp,
                             &PL_utf8_toupper, "ToUpper", "utf8::ToSpecUpper");
}

/*
=for apidoc to_utf8_title

Convert the UTF-8 encoded character at p to its titlecase version and
store that in UTF-8 in ustrp and its length in bytes in lenp.  Note
that the ustrp needs to be at least UTF8_MAXBYTES_CASE+1 bytes since the
titlecase version may be longer than the original character.

The first character of the titlecased version is returned
(but note, as explained above, that there may be more.)

=cut */

UV
Perl_to_utf8_title(pTHX_ const U8 *p, U8* ustrp, STRLEN *lenp)
{
    dVAR;

    PERL_ARGS_ASSERT_TO_UTF8_TITLE;

    return Perl_to_utf8_case(aTHX_ p, ustrp, lenp,
                             &PL_utf8_totitle, "ToTitle", "utf8::ToSpecTitle");
}

/*
=for apidoc to_utf8_lower

Convert the UTF-8 encoded character at p to its lowercase version and
store that in UTF-8 in ustrp and its length in bytes in lenp.  Note
that the ustrp needs to be at least UTF8_MAXBYTES_CASE+1 bytes since the
lowercase version may be longer than the original character.

The first character of the lowercased version is returned
(but note, as explained above, that there may be more.)

=cut */

UV
Perl_to_utf8_lower(pTHX_ const U8 *p, U8* ustrp, STRLEN *lenp)
{
    dVAR;

    PERL_ARGS_ASSERT_TO_UTF8_LOWER;

    return Perl_to_utf8_case(aTHX_ p, ustrp, lenp,
                             &PL_utf8_tolower, "ToLower", "utf8::ToSpecLower");
}

/*
=for apidoc to_utf8_fold

Convert the UTF-8 encoded character at p to its foldcase version and
store that in UTF-8 in ustrp and its length in bytes in lenp.  Note
that the ustrp needs to be at least UTF8_MAXBYTES_CASE+1 bytes since the
foldcase version may be longer than the original character (up to
three characters).

The first character of the foldcased version is returned
(but note, as explained above, that there may be more.)

=cut */

UV
Perl_to_utf8_fold(pTHX_ const U8 *p, U8* ustrp, STRLEN *lenp)
{
    dVAR;

    PERL_ARGS_ASSERT_TO_UTF8_FOLD;

    return Perl_to_utf8_case(aTHX_ p, ustrp, lenp,
                             &PL_utf8_tofold, "ToFold", "utf8::ToSpecFold");
}

/* Note:
 * A "swash" is a swatch hash.
 * A "swatch" is a bit vector generated by utf8.c:S_swash_get().
 * C<pkg> is a pointer to a package name for SWASHNEW, should be "utf8".
 * For other parameters, see utf8::SWASHNEW in lib/utf8_heavy.pl.
 */
SV*
Perl_swash_init(pTHX_ const char* pkg, const char* name, SV *listsv, I32 minbits, I32 none)
{
    dVAR;
    SV* retval;
    dSP;
    const size_t pkg_len = strlen(pkg);
    const size_t name_len = strlen(name);
    HV * const stash = gv_stashpvn(pkg, pkg_len, 0);
    SV* errsv_save;

    PERL_ARGS_ASSERT_SWASH_INIT;

    PUSHSTACKi(PERLSI_MAGIC);
    ENTER;
    SAVEHINTS();
    save_re_context();
    if (!gv_fetchmeth(stash, "SWASHNEW", 8, -1)) {	/* demand load utf8 */
	ENTER;
	errsv_save = newSVsv(ERRSV);
	/* It is assumed that callers of this routine are not passing in any
	   user derived data.  */
	/* Need to do this after save_re_context() as it will set PL_tainted to
	   1 while saving $1 etc (see the code after getrx: in Perl_magic_get).
	   Even line to create errsv_save can turn on PL_tainted.  */
	SAVEBOOL(PL_tainted);
	PL_tainted = 0;
	Perl_load_module(aTHX_ PERL_LOADMOD_NOIMPORT, newSVpvn(pkg,pkg_len),
			 NULL);
	if (!SvTRUE(ERRSV))
	    sv_setsv(ERRSV, errsv_save);
	SvREFCNT_dec(errsv_save);
	LEAVE;
    }
    SPAGAIN;
    PUSHMARK(SP);
    EXTEND(SP,5);
    mPUSHp(pkg, pkg_len);
    mPUSHp(name, name_len);
    PUSHs(listsv);
    mPUSHi(minbits);
    mPUSHi(none);
    PUTBACK;
    errsv_save = newSVsv(ERRSV);
    if (call_method("SWASHNEW", G_SCALAR))
	retval = newSVsv(*PL_stack_sp--);
    else
	retval = &PL_sv_undef;
    if (!SvTRUE(ERRSV))
	sv_setsv(ERRSV, errsv_save);
    SvREFCNT_dec(errsv_save);
    LEAVE;
    POPSTACK;
    if (IN_PERL_COMPILETIME) {
	CopHINTS_set(PL_curcop, PL_hints);
    }
    if (!SvROK(retval) || SvTYPE(SvRV(retval)) != SVt_PVHV) {
        if (SvPOK(retval))
	    Perl_croak(aTHX_ "Can't find Unicode property definition \"%"SVf"\"",
		       SVfARG(retval));
	Perl_croak(aTHX_ "SWASHNEW didn't return an HV ref");
    }
    return retval;
}


/* This API is wrong for special case conversions since we may need to
 * return several Unicode characters for a single Unicode character
 * (see lib/unicore/SpecCase.txt) The SWASHGET in lib/utf8_heavy.pl is
 * the lower-level routine, and it is similarly broken for returning
 * multiple values.  --jhi */
/* Now SWASHGET is recasted into S_swash_get in this file. */

/* Note:
 * Returns the value of property/mapping C<swash> for the first character
 * of the string C<ptr>. If C<do_utf8> is true, the string C<ptr> is
 * assumed to be in utf8. If C<do_utf8> is false, the string C<ptr> is
 * assumed to be in native 8-bit encoding. Caches the swatch in C<swash>.
 */
UV
Perl_swash_fetch(pTHX_ SV *swash, const U8 *ptr, bool do_utf8)
{
    dVAR;
    HV *const hv = MUTABLE_HV(SvRV(swash));
    U32 klen;
    U32 off;
    STRLEN slen;
    STRLEN needents;
    const U8 *tmps = NULL;
    U32 bit;
    SV *swatch;
    U8 tmputf8[2];
    const UV c = NATIVE_TO_ASCII(*ptr);

    PERL_ARGS_ASSERT_SWASH_FETCH;

    if (!do_utf8 && !UNI_IS_INVARIANT(c)) {
	tmputf8[0] = (U8)UTF8_EIGHT_BIT_HI(c);
	tmputf8[1] = (U8)UTF8_EIGHT_BIT_LO(c);
	ptr = tmputf8;
    }
    /* Given a UTF-X encoded char 0xAA..0xYY,0xZZ
     * then the "swatch" is a vec() for all the chars which start
     * with 0xAA..0xYY
     * So the key in the hash (klen) is length of encoded char -1
     */
    klen = UTF8SKIP(ptr) - 1;
    off  = ptr[klen];

    if (klen == 0) {
      /* If char is invariant then swatch is for all the invariant chars
       * In both UTF-8 and UTF-8-MOD that happens to be UTF_CONTINUATION_MARK
       */
	needents = UTF_CONTINUATION_MARK;
	off      = NATIVE_TO_UTF(ptr[klen]);
    }
    else {
      /* If char is encoded then swatch is for the prefix */
	needents = (1 << UTF_ACCUMULATION_SHIFT);
	off      = NATIVE_TO_UTF(ptr[klen]) & UTF_CONTINUATION_MASK;
    }

    /*
     * This single-entry cache saves about 1/3 of the utf8 overhead in test
     * suite.  (That is, only 7-8% overall over just a hash cache.  Still,
     * it's nothing to sniff at.)  Pity we usually come through at least
     * two function calls to get here...
     *
     * NB: this code assumes that swatches are never modified, once generated!
     */

    if (hv   == PL_last_swash_hv &&
	klen == PL_last_swash_klen &&
	(!klen || memEQ((char *)ptr, (char *)PL_last_swash_key, klen)) )
    {
	tmps = PL_last_swash_tmps;
	slen = PL_last_swash_slen;
    }
    else {
	/* Try our second-level swatch cache, kept in a hash. */
	SV** svp = hv_fetch(hv, (const char*)ptr, klen, FALSE);

	/* If not cached, generate it via swash_get */
	if (!svp || !SvPOK(*svp)
		 || !(tmps = (const U8*)SvPV_const(*svp, slen))) {
	    /* We use utf8n_to_uvuni() as we want an index into
	       Unicode tables, not a native character number.
	     */
	    const UV code_point = utf8n_to_uvuni(ptr, UTF8_MAXBYTES, 0,
					   ckWARN(WARN_UTF8) ?
					   0 : UTF8_ALLOW_ANY);
	    swatch = swash_get(swash,
		    /* On EBCDIC & ~(0xA0-1) isn't a useful thing to do */
				(klen) ? (code_point & ~(needents - 1)) : 0,
				needents);

	    if (IN_PERL_COMPILETIME)
		CopHINTS_set(PL_curcop, PL_hints);

	    svp = hv_store(hv, (const char *)ptr, klen, swatch, 0);

	    if (!svp || !(tmps = (U8*)SvPV(*svp, slen))
		     || (slen << 3) < needents)
		Perl_croak(aTHX_ "panic: swash_fetch got improper swatch");
	}

	PL_last_swash_hv = hv;
	assert(klen <= sizeof(PL_last_swash_key));
	PL_last_swash_klen = (U8)klen;
	/* FIXME change interpvar.h?  */
	PL_last_swash_tmps = (U8 *) tmps;
	PL_last_swash_slen = slen;
	if (klen)
	    Copy(ptr, PL_last_swash_key, klen, U8);
    }

    switch ((int)((slen << 3) / needents)) {
    case 1:
	bit = 1 << (off & 7);
	off >>= 3;
	return (tmps[off] & bit) != 0;
    case 8:
	return tmps[off];
    case 16:
	off <<= 1;
	return (tmps[off] << 8) + tmps[off + 1] ;
    case 32:
	off <<= 2;
	return (tmps[off] << 24) + (tmps[off+1] << 16) + (tmps[off+2] << 8) + tmps[off + 3] ;
    }
    Perl_croak(aTHX_ "panic: swash_fetch got swatch of unexpected bit width");
    NORETURN_FUNCTION_END;
}

/* Note:
 * Returns a swatch (a bit vector string) for a code point sequence
 * that starts from the value C<start> and comprises the number C<span>.
 * A C<swash> must be an object created by SWASHNEW (see lib/utf8_heavy.pl).
 * Should be used via swash_fetch, which will cache the swatch in C<swash>.
 */
STATIC SV*
S_swash_get(pTHX_ SV* swash, UV start, UV span)
{
    SV *swatch;
    U8 *l, *lend, *x, *xend, *s;
    STRLEN lcur, xcur, scur;
    HV *const hv = MUTABLE_HV(SvRV(swash));
    SV** const listsvp = hv_fetchs(hv, "LIST", FALSE);
    SV** const typesvp = hv_fetchs(hv, "TYPE", FALSE);
    SV** const bitssvp = hv_fetchs(hv, "BITS", FALSE);
    SV** const nonesvp = hv_fetchs(hv, "NONE", FALSE);
    SV** const extssvp = hv_fetchs(hv, "EXTRAS", FALSE);
    const U8* const typestr = (U8*)SvPV_nolen(*typesvp);
    const int  typeto  = typestr[0] == 'T' && typestr[1] == 'o';
    const STRLEN bits  = SvUV(*bitssvp);
    const STRLEN octets = bits >> 3; /* if bits == 1, then octets == 0 */
    const UV     none  = SvUV(*nonesvp);
    const UV     end   = start + span;

    PERL_ARGS_ASSERT_SWASH_GET;

    if (bits != 1 && bits != 8 && bits != 16 && bits != 32) {
	Perl_croak(aTHX_ "panic: swash_get doesn't expect bits %"UVuf,
						 (UV)bits);
    }

    /* create and initialize $swatch */
    scur   = octets ? (span * octets) : (span + 7) / 8;
    swatch = newSV(scur);
    SvPOK_on(swatch);
    s = (U8*)SvPVX(swatch);
    if (octets && none) {
	const U8* const e = s + scur;
	while (s < e) {
	    if (bits == 8)
		*s++ = (U8)(none & 0xff);
	    else if (bits == 16) {
		*s++ = (U8)((none >>  8) & 0xff);
		*s++ = (U8)( none        & 0xff);
	    }
	    else if (bits == 32) {
		*s++ = (U8)((none >> 24) & 0xff);
		*s++ = (U8)((none >> 16) & 0xff);
		*s++ = (U8)((none >>  8) & 0xff);
		*s++ = (U8)( none        & 0xff);
	    }
	}
	*s = '\0';
    }
    else {
	(void)memzero((U8*)s, scur + 1);
    }
    SvCUR_set(swatch, scur);
    s = (U8*)SvPVX(swatch);

    /* read $swash->{LIST} */
    l = (U8*)SvPV(*listsvp, lcur);
    lend = l + lcur;
    while (l < lend) {
	UV min, max, val;
	STRLEN numlen;
	I32 flags = PERL_SCAN_SILENT_ILLDIGIT | PERL_SCAN_DISALLOW_PREFIX;

	U8* const nl = (U8*)memchr(l, '\n', lend - l);

	numlen = lend - l;
	min = grok_hex((char *)l, &numlen, &flags, NULL);
	if (numlen)
	    l += numlen;
	else if (nl) {
	    l = nl + 1; /* 1 is length of "\n" */
	    continue;
	}
	else {
	    l = lend; /* to LIST's end at which \n is not found */
	    break;
	}

	if (isBLANK(*l)) {
	    ++l;
	    flags = PERL_SCAN_SILENT_ILLDIGIT | PERL_SCAN_DISALLOW_PREFIX;
	    numlen = lend - l;
	    max = grok_hex((char *)l, &numlen, &flags, NULL);
	    if (numlen)
		l += numlen;
	    else
		max = min;

	    if (octets) {
		if (isBLANK(*l)) {
		    ++l;
		    flags = PERL_SCAN_SILENT_ILLDIGIT |
			    PERL_SCAN_DISALLOW_PREFIX;
		    numlen = lend - l;
		    val = grok_hex((char *)l, &numlen, &flags, NULL);
		    if (numlen)
			l += numlen;
		    else
			val = 0;
		}
		else {
		    val = 0;
		    if (typeto) {
			Perl_croak(aTHX_ "%s: illegal mapping '%s'",
					 typestr, l);
		    }
		}
	    }
	    else
		val = 0; /* bits == 1, then val should be ignored */
	}
	else {
	    max = min;
	    if (octets) {
		val = 0;
		if (typeto) {
		    Perl_croak(aTHX_ "%s: illegal mapping '%s'", typestr, l);
		}
	    }
	    else
		val = 0; /* bits == 1, then val should be ignored */
	}

	if (nl)
	    l = nl + 1;
	else
	    l = lend;

	if (max < start)
	    continue;

	if (octets) {
	    UV key;
	    if (min < start) {
		if (!none || val < none) {
		    val += start - min;
		}
		min = start;
	    }
	    for (key = min; key <= max; key++) {
		STRLEN offset;
		if (key >= end)
		    goto go_out_list;
		/* offset must be non-negative (start <= min <= key < end) */
		offset = octets * (key - start);
		if (bits == 8)
		    s[offset] = (U8)(val & 0xff);
		else if (bits == 16) {
		    s[offset    ] = (U8)((val >>  8) & 0xff);
		    s[offset + 1] = (U8)( val        & 0xff);
		}
		else if (bits == 32) {
		    s[offset    ] = (U8)((val >> 24) & 0xff);
		    s[offset + 1] = (U8)((val >> 16) & 0xff);
		    s[offset + 2] = (U8)((val >>  8) & 0xff);
		    s[offset + 3] = (U8)( val        & 0xff);
		}

		if (!none || val < none)
		    ++val;
	    }
	}
	else { /* bits == 1, then val should be ignored */
	    UV key;
	    if (min < start)
		min = start;
	    for (key = min; key <= max; key++) {
		const STRLEN offset = (STRLEN)(key - start);
		if (key >= end)
		    goto go_out_list;
		s[offset >> 3] |= 1 << (offset & 7);
	    }
	}
    } /* while */
  go_out_list:

    /* read $swash->{EXTRAS} */
    x = (U8*)SvPV(*extssvp, xcur);
    xend = x + xcur;
    while (x < xend) {
	STRLEN namelen;
	U8 *namestr;
	SV** othersvp;
	HV* otherhv;
	STRLEN otherbits;
	SV **otherbitssvp, *other;
	U8 *s, *o, *nl;
	STRLEN slen, olen;

	const U8 opc = *x++;
	if (opc == '\n')
	    continue;

	nl = (U8*)memchr(x, '\n', xend - x);

	if (opc != '-' && opc != '+' && opc != '!' && opc != '&') {
	    if (nl) {
		x = nl + 1; /* 1 is length of "\n" */
		continue;
	    }
	    else {
		x = xend; /* to EXTRAS' end at which \n is not found */
		break;
	    }
	}

	namestr = x;
	if (nl) {
	    namelen = nl - namestr;
	    x = nl + 1;
	}
	else {
	    namelen = xend - namestr;
	    x = xend;
	}

	othersvp = hv_fetch(hv, (char *)namestr, namelen, FALSE);
	otherhv = MUTABLE_HV(SvRV(*othersvp));
	otherbitssvp = hv_fetchs(otherhv, "BITS", FALSE);
	otherbits = (STRLEN)SvUV(*otherbitssvp);
	if (bits < otherbits)
	    Perl_croak(aTHX_ "panic: swash_get found swatch size mismatch");

	/* The "other" swatch must be destroyed after. */
	other = swash_get(*othersvp, start, span);
	o = (U8*)SvPV(other, olen);

	if (!olen)
	    Perl_croak(aTHX_ "panic: swash_get got improper swatch");

	s = (U8*)SvPV(swatch, slen);
	if (bits == 1 && otherbits == 1) {
	    if (slen != olen)
		Perl_croak(aTHX_ "panic: swash_get found swatch length mismatch");

	    switch (opc) {
	    case '+':
		while (slen--)
		    *s++ |= *o++;
		break;
	    case '!':
		while (slen--)
		    *s++ |= ~*o++;
		break;
	    case '-':
		while (slen--)
		    *s++ &= ~*o++;
		break;
	    case '&':
		while (slen--)
		    *s++ &= *o++;
		break;
	    default:
		break;
	    }
	}
	else {
	    STRLEN otheroctets = otherbits >> 3;
	    STRLEN offset = 0;
	    U8* const send = s + slen;

	    while (s < send) {
		UV otherval = 0;

		if (otherbits == 1) {
		    otherval = (o[offset >> 3] >> (offset & 7)) & 1;
		    ++offset;
		}
		else {
		    STRLEN vlen = otheroctets;
		    otherval = *o++;
		    while (--vlen) {
			otherval <<= 8;
			otherval |= *o++;
		    }
		}

		if (opc == '+' && otherval)
		    NOOP;   /* replace with otherval */
		else if (opc == '!' && !otherval)
		    otherval = 1;
		else if (opc == '-' && otherval)
		    otherval = 0;
		else if (opc == '&' && !otherval)
		    otherval = 0;
		else {
		    s += octets; /* no replacement */
		    continue;
		}

		if (bits == 8)
		    *s++ = (U8)( otherval & 0xff);
		else if (bits == 16) {
		    *s++ = (U8)((otherval >>  8) & 0xff);
		    *s++ = (U8)( otherval        & 0xff);
		}
		else if (bits == 32) {
		    *s++ = (U8)((otherval >> 24) & 0xff);
		    *s++ = (U8)((otherval >> 16) & 0xff);
		    *s++ = (U8)((otherval >>  8) & 0xff);
		    *s++ = (U8)( otherval        & 0xff);
		}
	    }
	}
	sv_free(other); /* through with it! */
    } /* while */
    return swatch;
}

/*
=for apidoc uvchr_to_utf8

Adds the UTF-8 representation of the Native codepoint C<uv> to the end
of the string C<d>; C<d> should be have at least C<UTF8_MAXBYTES+1> free
bytes available. The return value is the pointer to the byte after the
end of the new character. In other words,

    d = uvchr_to_utf8(d, uv);

is the recommended wide native character-aware way of saying

    *(d++) = uv;

=cut
*/

/* On ASCII machines this is normally a macro but we want a
   real function in case XS code wants it
*/
U8 *
Perl_uvchr_to_utf8(pTHX_ U8 *d, UV uv)
{
    PERL_ARGS_ASSERT_UVCHR_TO_UTF8;

    return Perl_uvuni_to_utf8_flags(aTHX_ d, NATIVE_TO_UNI(uv), 0);
}

U8 *
Perl_uvchr_to_utf8_flags(pTHX_ U8 *d, UV uv, UV flags)
{
    PERL_ARGS_ASSERT_UVCHR_TO_UTF8_FLAGS;

    return Perl_uvuni_to_utf8_flags(aTHX_ d, NATIVE_TO_UNI(uv), flags);
}

/*
=for apidoc utf8n_to_uvchr
flags

Returns the native character value of the first character in the string 
C<s>
which is assumed to be in UTF-8 encoding; C<retlen> will be set to the
length, in bytes, of that character.

Allows length and flags to be passed to low level routine.

=cut
*/
/* On ASCII machines this is normally a macro but we want
   a real function in case XS code wants it
*/
UV
Perl_utf8n_to_uvchr(pTHX_ const U8 *s, STRLEN curlen, STRLEN *retlen, 
U32 flags)
{
    const UV uv = Perl_utf8n_to_uvuni(aTHX_ s, curlen, retlen, flags);

    PERL_ARGS_ASSERT_UTF8N_TO_UVCHR;

    return UNI_TO_NATIVE(uv);
}

/*
=for apidoc pv_uni_display

Build to the scalar dsv a displayable version of the string spv,
length len, the displayable version being at most pvlim bytes long
(if longer, the rest is truncated and "..." will be appended).

The flags argument can have UNI_DISPLAY_ISPRINT set to display
isPRINT()able characters as themselves, UNI_DISPLAY_BACKSLASH
to display the \\[nrfta\\] as the backslashed versions (like '\n')
(UNI_DISPLAY_BACKSLASH is preferred over UNI_DISPLAY_ISPRINT for \\).
UNI_DISPLAY_QQ (and its alias UNI_DISPLAY_REGEX) have both
UNI_DISPLAY_BACKSLASH and UNI_DISPLAY_ISPRINT turned on.

The pointer to the PV of the dsv is returned.

=cut */
char *
Perl_pv_uni_display(pTHX_ SV *dsv, const U8 *spv, STRLEN len, STRLEN pvlim, UV flags)
{
    int truncated = 0;
    const char *s, *e;

    PERL_ARGS_ASSERT_PV_UNI_DISPLAY;

    sv_setpvs(dsv, "");
    SvUTF8_off(dsv);
    for (s = (const char *)spv, e = s + len; s < e; s += UTF8SKIP(s)) {
	 UV u;
	  /* This serves double duty as a flag and a character to print after
	     a \ when flags & UNI_DISPLAY_BACKSLASH is true.
	  */
	 char ok = 0;

	 if (pvlim && SvCUR(dsv) >= pvlim) {
	      truncated++;
	      break;
	 }
	 u = utf8_to_uvchr((U8*)s, 0);
	 if (u < 256) {
	     const unsigned char c = (unsigned char)u & 0xFF;
	     if (flags & UNI_DISPLAY_BACKSLASH) {
	         switch (c) {
		 case '\n':
		     ok = 'n'; break;
		 case '\r':
		     ok = 'r'; break;
		 case '\t':
		     ok = 't'; break;
		 case '\f':
		     ok = 'f'; break;
		 case '\a':
		     ok = 'a'; break;
		 case '\\':
		     ok = '\\'; break;
		 default: break;
		 }
		 if (ok) {
		     const char string = ok;
		     sv_catpvs(dsv, "\\");
		     sv_catpvn(dsv, &string, 1);
		 }
	     }
	     /* isPRINT() is the locale-blind version. */
	     if (!ok && (flags & UNI_DISPLAY_ISPRINT) && isPRINT(c)) {
		 const char string = c;
		 sv_catpvn(dsv, &string, 1);
		 ok = 1;
	     }
	 }
	 if (!ok)
	     Perl_sv_catpvf(aTHX_ dsv, "\\x{%"UVxf"}", u);
    }
    if (truncated)
	 sv_catpvs(dsv, "...");
    
    return SvPVX(dsv);
}

/*
=for apidoc sv_uni_display

Build to the scalar dsv a displayable version of the scalar sv,
the displayable version being at most pvlim bytes long
(if longer, the rest is truncated and "..." will be appended).

The flags argument is as in pv_uni_display().

The pointer to the PV of the dsv is returned.

=cut
*/
char *
Perl_sv_uni_display(pTHX_ SV *dsv, SV *ssv, STRLEN pvlim, UV flags)
{
    PERL_ARGS_ASSERT_SV_UNI_DISPLAY;

     return Perl_pv_uni_display(aTHX_ dsv, (const U8*)SvPVX_const(ssv),
				SvCUR(ssv), pvlim, flags);
}

/*
=for apidoc ibcmp_utf8

Return true if the strings s1 and s2 differ case-insensitively, false
if not (if they are equal case-insensitively).  If u1 is true, the
string s1 is assumed to be in UTF-8-encoded Unicode.  If u2 is true,
the string s2 is assumed to be in UTF-8-encoded Unicode.  If u1 or u2
are false, the respective string is assumed to be in native 8-bit
encoding.

If the pe1 and pe2 are non-NULL, the scanning pointers will be copied
in there (they will point at the beginning of the I<next> character).
If the pointers behind pe1 or pe2 are non-NULL, they are the end
pointers beyond which scanning will not continue under any
circumstances.  If the byte lengths l1 and l2 are non-zero, s1+l1 and
s2+l2 will be used as goal end pointers that will also stop the scan,
and which qualify towards defining a successful match: all the scans
that define an explicit length must reach their goal pointers for
a match to succeed).

For case-insensitiveness, the "casefolding" of Unicode is used
instead of upper/lowercasing both the characters, see
http://www.unicode.org/unicode/reports/tr21/ (Case Mappings).

=cut */
I32
Perl_ibcmp_utf8(pTHX_ const char *s1, char **pe1, register UV l1, bool u1, const char *s2, char **pe2, register UV l2, bool u2)
{
     dVAR;
     register const U8 *p1  = (const U8*)s1;
     register const U8 *p2  = (const U8*)s2;
     register const U8 *f1 = NULL;
     register const U8 *f2 = NULL;
     register U8 *e1 = NULL;
     register U8 *q1 = NULL;
     register U8 *e2 = NULL;
     register U8 *q2 = NULL;
     STRLEN n1 = 0, n2 = 0;
     U8 foldbuf1[UTF8_MAXBYTES_CASE+1];
     U8 foldbuf2[UTF8_MAXBYTES_CASE+1];
     U8 natbuf[1+1];
     STRLEN foldlen1, foldlen2;
     bool match;

     PERL_ARGS_ASSERT_IBCMP_UTF8;
     
     if (pe1)
	  e1 = *(U8**)pe1;
     /* assert(e1 || l1); */
     if (e1 == 0 || (l1 && l1 < (UV)(e1 - (const U8*)s1)))
	  f1 = (const U8*)s1 + l1;
     if (pe2)
	  e2 = *(U8**)pe2;
     /* assert(e2 || l2); */
     if (e2 == 0 || (l2 && l2 < (UV)(e2 - (const U8*)s2)))
	  f2 = (const U8*)s2 + l2;

     /* This shouldn't happen. However, putting an assert() there makes some
      * tests fail. */
     /* assert((e1 == 0 && f1 == 0) || (e2 == 0 && f2 == 0) || (f1 == 0 && f2 == 0)); */
     if ((e1 == 0 && f1 == 0) || (e2 == 0 && f2 == 0) || (f1 == 0 && f2 == 0))
	  return 1; /* mismatch; possible infinite loop or false positive */

     if (!u1 || !u2)
	  natbuf[1] = 0; /* Need to terminate the buffer. */

     while ((e1 == 0 || p1 < e1) &&
	    (f1 == 0 || p1 < f1) &&
	    (e2 == 0 || p2 < e2) &&
	    (f2 == 0 || p2 < f2)) {
	  if (n1 == 0) {
	       if (u1)
		    to_utf8_fold(p1, foldbuf1, &foldlen1);
	       else {
		    uvuni_to_utf8(natbuf, (UV) NATIVE_TO_UNI(((UV)*p1)));
		    to_utf8_fold(natbuf, foldbuf1, &foldlen1);
	       }
	       q1 = foldbuf1;
	       n1 = foldlen1;
	  }
	  if (n2 == 0) {
	       if (u2)
		    to_utf8_fold(p2, foldbuf2, &foldlen2);
	       else {
		    uvuni_to_utf8(natbuf, (UV) NATIVE_TO_UNI(((UV)*p2)));
		    to_utf8_fold(natbuf, foldbuf2, &foldlen2);
	       }
	       q2 = foldbuf2;
	       n2 = foldlen2;
	  }
	  while (n1 && n2) {
	       if ( UTF8SKIP(q1) != UTF8SKIP(q2) ||
		   (UTF8SKIP(q1) == 1 && *q1 != *q2) ||
		    memNE((char*)q1, (char*)q2, UTF8SKIP(q1)) )
		   return 1; /* mismatch */
	       n1 -= UTF8SKIP(q1);
	       q1 += UTF8SKIP(q1);
	       n2 -= UTF8SKIP(q2);
	       q2 += UTF8SKIP(q2);
	  }
	  if (n1 == 0)
	       p1 += u1 ? UTF8SKIP(p1) : 1;
	  if (n2 == 0)
	       p2 += u2 ? UTF8SKIP(p2) : 1;

     }

     /* A match is defined by all the scans that specified
      * an explicit length reaching their final goals. */
     match = (n1 == 0 && n2 == 0    /* Must not match partial char; Bug #72998 */
	     && (f1 == 0 || p1 == f1) && (f2 == 0 || p2 == f2));

     if (match) {
	  if (pe1)
	       *pe1 = (char*)p1;
	  if (pe2)
	       *pe2 = (char*)p2;
     }

     return match ? 0 : 1; /* 0 match, 1 mismatch */
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
