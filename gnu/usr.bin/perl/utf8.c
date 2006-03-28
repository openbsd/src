/*    utf8.c
 *
 *    Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006,
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'What a fix!' said Sam. 'That's the one place in all the lands we've ever
 * heard of that we don't want to see any closer; and that's the one place
 * we're trying to get to!  And that's just where we can't get, nohow.'
 *
 * 'Well do I understand your speech,' he answered in the same language;
 * 'yet few strangers do so.  Why then do you not speak in the Common Tongue,
 * as is the custom in the West, if you wish to be answered?'
 *
 * ...the travellers perceived that the floor was paved with stones of many
 * hues; branching runes and strange devices intertwined beneath their feet.
 */

#include "EXTERN.h"
#define PERL_IN_UTF8_C
#include "perl.h"

static const char unees[] =
    "Malformed UTF-8 character (unexpected end of string)";

/* 
=head1 Unicode Support

This file contains various utility functions for manipulating UTF8-encoded
strings. For the uninitiated, this is a method of representing arbitrary
Unicode characters as a variable number of bytes, in such a way that
characters in the ASCII range are unmodified, and a zero byte never appears
within non-zero characters.

=for apidoc A|U8 *|uvuni_to_utf8_flags|U8 *d|UV uv|UV flags

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
			 "Unicode character 0x%04"UVxf" is illegal", uv);
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
 
U8 *
Perl_uvuni_to_utf8(pTHX_ U8 *d, UV uv)
{
    return Perl_uvuni_to_utf8_flags(aTHX_ d, uv, 0);
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
S_is_utf8_char_slow(pTHX_ const U8 *s, const STRLEN len)
{
    U8 u = *s;
    STRLEN slen;
    UV uv, ouv;

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
=for apidoc A|STRLEN|is_utf8_char|U8 *s

Tests if some arbitrary number of bytes begins in a valid UTF-8
character.  Note that an INVARIANT (i.e. ASCII) character is a valid
UTF-8 character.  The actual number of bytes in the UTF-8 character
will be returned if it is valid, otherwise 0.

=cut */
STRLEN
Perl_is_utf8_char(pTHX_ U8 *s)
{
    const STRLEN len = UTF8SKIP(s);
#ifdef IS_UTF8_CHAR
    if (IS_UTF8_CHAR_FAST(len))
        return IS_UTF8_CHAR(s, len) ? len : 0;
#endif /* #ifdef IS_UTF8_CHAR */
    return is_utf8_char_slow(s, len);
}

/*
=for apidoc A|bool|is_utf8_string|U8 *s|STRLEN len

Returns true if first C<len> bytes of the given string form a valid
UTF-8 string, false otherwise.  Note that 'a valid UTF-8 string' does
not mean 'a string that contains code points above 0x7F encoded in UTF-8'
because a valid ASCII string is a valid UTF-8 string.

See also is_utf8_string_loclen() and is_utf8_string_loc().

=cut
*/

bool
Perl_is_utf8_string(pTHX_ U8 *s, STRLEN len)
{
    const U8* x = s;
    const U8* send;

    if (!len)
	len = strlen((const char *)s);
    send = s + len;

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
		     goto out;
	     } else if (!is_utf8_char_slow(x, c))
	         goto out;
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
=for apidoc A|bool|is_utf8_string_loclen|const U8 *s|STRLEN len|const U8 **ep|const STRLEN *el

Like is_utf8_string() but stores the location of the failure (in the
case of "utf8ness failure") or the location s+len (in the case of
"utf8ness success") in the C<ep>, and the number of UTF-8
encoded characters in the C<el>.

See also is_utf8_string_loc() and is_utf8_string().

=cut
*/

bool
Perl_is_utf8_string_loclen(pTHX_ const U8 *s, STRLEN len, const U8 **ep, STRLEN *el)
{
    const U8* x = s;
    const U8* send;
    STRLEN c;

    if (!len)
        len = strlen((const char *)s);
    send = s + len;
    if (el)
        *el = 0;

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
	 if (el)
	     (*el)++;
    }

 out:
    if (ep)
        *ep = x;
    if (x != send)
	return FALSE;

    return TRUE;
}

/*
=for apidoc A|bool|is_utf8_string_loc|const U8 *s|STRLEN len|const U8 **ep|const STRLEN *el

Like is_utf8_string() but stores the location of the failure (in the
case of "utf8ness failure") or the location s+len (in the case of
"utf8ness success") in the C<ep>.

See also is_utf8_string_loclen() and is_utf8_string().

=cut
*/

bool
Perl_is_utf8_string_loc(pTHX_ U8 *s, STRLEN len, U8 **ep)
{
    return is_utf8_string_loclen(s, len, (const U8 **)ep, 0);
}

/*
=for apidoc A|UV|utf8n_to_uvuni|U8 *s|STRLEN curlen|STRLEN *retlen|U32 flags

Bottom level UTF-8 decode routine.
Returns the unicode code point value of the first character in the string C<s>
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
Perl_utf8n_to_uvuni(pTHX_ U8 *s, STRLEN curlen, STRLEN *retlen, U32 flags)
{
    const U8 *s0 = s;
    UV uv = *s, ouv = 0;
    STRLEN len = 1;
    const bool dowarn = ckWARN_d(WARN_UTF8);
    const UV startbyte = *s;
    STRLEN expectlen = 0;
    U32 warning = 0;

/* This list is a superset of the UTF8_ALLOW_XXX. */

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
	    *retlen = -1;
	return 0;
    }

    if (dowarn) {
	SV* const sv = sv_2mortal(newSVpv("Malformed UTF-8 character ", 0));

	switch (warning) {
	case 0: /* Intentionally empty. */ break;
	case UTF8_WARN_EMPTY:
	    Perl_sv_catpv(aTHX_ sv, "(empty string)");
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
	case UTF8_WARN_FFFF:
	    Perl_sv_catpvf(aTHX_ sv, "(character 0x%04"UVxf")", uv);
	    break;
	default:
	    Perl_sv_catpv(aTHX_ sv, "(unknown reason)");
	    break;
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
=for apidoc A|UV|utf8_to_uvchr|U8 *s|STRLEN *retlen

Returns the native character value of the first character in the string C<s>
which is assumed to be in UTF-8 encoding; C<retlen> will be set to the
length, in bytes, of that character.

If C<s> does not point to a well-formed UTF-8 character, zero is
returned and retlen is set, if possible, to -1.

=cut
*/

UV
Perl_utf8_to_uvchr(pTHX_ U8 *s, STRLEN *retlen)
{
    return Perl_utf8n_to_uvchr(aTHX_ s, UTF8_MAXBYTES, retlen,
			       ckWARN(WARN_UTF8) ? 0 : UTF8_ALLOW_ANY);
}

/*
=for apidoc A|UV|utf8_to_uvuni|U8 *s|STRLEN *retlen

Returns the Unicode code point of the first character in the string C<s>
which is assumed to be in UTF-8 encoding; C<retlen> will be set to the
length, in bytes, of that character.

This function should only be used when returned UV is considered
an index into the Unicode semantic tables (e.g. swashes).

If C<s> does not point to a well-formed UTF-8 character, zero is
returned and retlen is set, if possible, to -1.

=cut
*/

UV
Perl_utf8_to_uvuni(pTHX_ U8 *s, STRLEN *retlen)
{
    /* Call the low level routine asking for checks */
    return Perl_utf8n_to_uvuni(aTHX_ s, UTF8_MAXBYTES, retlen,
			       ckWARN(WARN_UTF8) ? 0 : UTF8_ALLOW_ANY);
}

/*
=for apidoc A|STRLEN|utf8_length|U8 *s|U8 *e

Return the length of the UTF-8 char encoded string C<s> in characters.
Stops at C<e> (inclusive).  If C<e E<lt> s> or if the scan would end
up past C<e>, croaks.

=cut
*/

STRLEN
Perl_utf8_length(pTHX_ U8 *s, U8 *e)
{
    STRLEN len = 0;

    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g.
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    if (e < s)
	goto warn_and_return;
    while (s < e) {
	const U8 t = UTF8SKIP(s);
	if (e - s < t) {
	    warn_and_return:
	    if (ckWARN_d(WARN_UTF8)) {
	        if (PL_op)
		    Perl_warner(aTHX_ packWARN(WARN_UTF8),
			    "%s in %s", unees, OP_DESC(PL_op));
		else
		    Perl_warner(aTHX_ packWARN(WARN_UTF8), unees);
	    }
	    return len;
	}
	s += t;
	len++;
    }

    return len;
}

/*
=for apidoc A|IV|utf8_distance|U8 *a|U8 *b

Returns the number of UTF-8 characters between the UTF-8 pointers C<a>
and C<b>.

WARNING: use only if you *know* that the pointers point inside the
same UTF-8 buffer.

=cut
*/

IV
Perl_utf8_distance(pTHX_ U8 *a, U8 *b)
{
    IV off = 0;

    /* Note: cannot use UTF8_IS_...() too eagerly here since  e.g.
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    if (a < b) {
	while (a < b) {
	    const U8 c = UTF8SKIP(a);
	    if (b - a < c)
		goto warn_and_return;
	    a += c;
	    off--;
	}
    }
    else {
	while (b < a) {
	    const U8 c = UTF8SKIP(b);

	    if (a - b < c) {
		warn_and_return:
	        if (ckWARN_d(WARN_UTF8)) {
		    if (PL_op)
		        Perl_warner(aTHX_ packWARN(WARN_UTF8),
				    "%s in %s", unees, OP_DESC(PL_op));
		    else
		        Perl_warner(aTHX_ packWARN(WARN_UTF8), unees);
		}
		return off;
	    }
	    b += c;
	    off++;
	}
    }

    return off;
}

/*
=for apidoc A|U8 *|utf8_hop|U8 *s|I32 off

Return the UTF-8 pointer C<s> displaced by C<off> characters, either
forward or backward.

WARNING: do not use the following unless you *know* C<off> is within
the UTF-8 data pointed to by C<s> *and* that on entry C<s> is aligned
on the first byte of character or just after the last byte of a character.

=cut
*/

U8 *
Perl_utf8_hop(pTHX_ U8 *s, I32 off)
{
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
=for apidoc A|U8 *|utf8_to_bytes|U8 *s|STRLEN *len

Converts a string C<s> of length C<len> from UTF-8 into byte encoding.
Unlike C<bytes_to_utf8>, this over-writes the original string, and
updates len to contain the new length.
Returns zero on failure, setting C<len> to -1.

=cut
*/

U8 *
Perl_utf8_to_bytes(pTHX_ U8 *s, STRLEN *len)
{
    U8 *send;
    U8 *d;
    U8 *save = s;

    /* ensure valid UTF-8 and chars < 256 before updating string */
    for (send = s + *len; s < send; ) {
        U8 c = *s++;

        if (!UTF8_IS_INVARIANT(c) &&
            (!UTF8_IS_DOWNGRADEABLE_START(c) || (s >= send)
	     || !(c = *s++) || !UTF8_IS_CONTINUATION(c))) {
            *len = -1;
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
=for apidoc A|U8 *|bytes_from_utf8|const U8 *s|STRLEN *len|bool *is_utf8

Converts a string C<s> of length C<len> from UTF-8 into byte encoding.
Unlike C<utf8_to_bytes> but like C<bytes_to_utf8>, returns a pointer to
the newly-created string, and updates C<len> to contain the new
length.  Returns the original string if no conversion occurs, C<len>
is unchanged. Do nothing if C<is_utf8> points to 0. Sets C<is_utf8> to
0 if C<s> is converted or contains all 7bit characters.

=cut
*/

U8 *
Perl_bytes_from_utf8(pTHX_ U8 *s, STRLEN *len, bool *is_utf8)
{
    U8 *d;
    const U8 *start = s;
    const U8 *send;
    I32 count = 0;
    const U8 *s2;

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

    *is_utf8 = 0;		

    Newxz(d, (*len) - count + 1, U8);
    s2 = start; start = d;
    while (s2 < send) {
	U8 c = *s2++;
	if (!UTF8_IS_INVARIANT(c)) {
	    /* Then it is two-byte encoded */
	    c = UTF8_ACCUMULATE(NATIVE_TO_UTF(c), *s2++);
	    c = ASCII_TO_NATIVE(c);
	}
	*d++ = c;
    }
    *d = '\0';
    *len = d - start;
    return (U8 *)start;
}

/*
=for apidoc A|U8 *|bytes_to_utf8|U8 *s|STRLEN *len

Converts a string C<s> of length C<len> from ASCII into UTF-8 encoding.
Returns a pointer to the newly-created string, and sets C<len> to
reflect the new length.

If you want to convert to UTF-8 from other encodings than ASCII,
see sv_recode_to_utf8().

=cut
*/

U8*
Perl_bytes_to_utf8(pTHX_ U8 *s, STRLEN *len)
{
    const U8 * const send = s + (*len);
    U8 *d;
    U8 *dst;

    Newxz(d, (*len) * 2 + 1, U8);
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

    if (bytelen == 1 && p[0] == 0) { /* Be understanding. */
	 d[0] = 0;
	 *newlen = 1;
	 return d;
    }

    if (bytelen & 1)
	Perl_croak(aTHX_ "panic: utf16_to_utf8: odd bytelen %"UVf, (UV)bytelen);

    pend = p + bytelen;

    while (p < pend) {
	UV uv = (p[0] << 8) + p[1]; /* UTF-16BE */
	p += 2;
	if (uv < 0x80) {
	    *d++ = (U8)uv;
	    continue;
	}
	if (uv < 0x800) {
	    *d++ = (U8)(( uv >>  6)         | 0xc0);
	    *d++ = (U8)(( uv        & 0x3f) | 0x80);
	    continue;
	}
	if (uv >= 0xd800 && uv < 0xdbff) {	/* surrogates */
	    UV low = (p[0] << 8) + p[1];
	    p += 2;
	    if (low < 0xdc00 || low >= 0xdfff)
		Perl_croak(aTHX_ "Malformed UTF-16 surrogate");
	    uv = ((uv - 0xd800) << 10) + (low - 0xdc00) + 0x10000;
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
    U8* send = s + bytelen;
    while (s < send) {
	U8 tmp = s[0];
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
Perl_is_uni_alnumc(pTHX_ UV c)
{
    U8 tmpbuf[UTF8_MAXBYTES+1];
    uvchr_to_utf8(tmpbuf, c);
    return is_utf8_alnumc(tmpbuf);
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
    uvchr_to_utf8(p, c);
    return to_utf8_upper(p, p, lenp);
}

UV
Perl_to_uni_title(pTHX_ UV c, U8* p, STRLEN *lenp)
{
    uvchr_to_utf8(p, c);
    return to_utf8_title(p, p, lenp);
}

UV
Perl_to_uni_lower(pTHX_ UV c, U8* p, STRLEN *lenp)
{
    uvchr_to_utf8(p, c);
    return to_utf8_lower(p, p, lenp);
}

UV
Perl_to_uni_fold(pTHX_ UV c, U8* p, STRLEN *lenp)
{
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
Perl_is_uni_alnumc_lc(pTHX_ UV c)
{
    return is_uni_alnumc(c);	/* XXX no locale support yet */
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

bool
Perl_is_utf8_alnum(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_alnum)
	/* NOTE: "IsWord", not "IsAlnum", since Alnum is a true
	 * descendant of isalnum(3), in other words, it doesn't
	 * contain the '_'. --jhi */
	PL_utf8_alnum = swash_init("utf8", "IsWord", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_alnum, p, TRUE) != 0;
/*    return *p == '_' || is_utf8_alpha(p) || is_utf8_digit(p); */
#ifdef SURPRISINGLY_SLOWER  /* probably because alpha is usually true */
    if (!PL_utf8_alnum)
	PL_utf8_alnum = swash_init("utf8", "",
	    sv_2mortal(newSVpv("+utf8::IsAlpha\n+utf8::IsDigit\n005F\n",0)), 0, 0);
    return swash_fetch(PL_utf8_alnum, p, TRUE) != 0;
#endif
}

bool
Perl_is_utf8_alnumc(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_alnumc)
	PL_utf8_alnumc = swash_init("utf8", "IsAlnumC", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_alnumc, p, TRUE) != 0;
/*    return is_utf8_alpha(p) || is_utf8_digit(p); */
#ifdef SURPRISINGLY_SLOWER  /* probably because alpha is usually true */
    if (!PL_utf8_alnum)
	PL_utf8_alnum = swash_init("utf8", "",
	    sv_2mortal(newSVpv("+utf8::IsAlpha\n+utf8::IsDigit\n005F\n",0)), 0, 0);
    return swash_fetch(PL_utf8_alnum, p, TRUE) != 0;
#endif
}

bool
Perl_is_utf8_idfirst(pTHX_ U8 *p) /* The naming is historical. */
{
    if (*p == '_')
	return TRUE;
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_idstart) /* is_utf8_idstart would be more logical. */
	PL_utf8_idstart = swash_init("utf8", "IdStart", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_idstart, p, TRUE) != 0;
}

bool
Perl_is_utf8_idcont(pTHX_ U8 *p)
{
    if (*p == '_')
	return TRUE;
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_idcont)
	PL_utf8_idcont = swash_init("utf8", "IdContinue", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_idcont, p, TRUE) != 0;
}

bool
Perl_is_utf8_alpha(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_alpha)
	PL_utf8_alpha = swash_init("utf8", "IsAlpha", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_alpha, p, TRUE) != 0;
}

bool
Perl_is_utf8_ascii(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_ascii)
	PL_utf8_ascii = swash_init("utf8", "IsAscii", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_ascii, p, TRUE) != 0;
}

bool
Perl_is_utf8_space(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_space)
	PL_utf8_space = swash_init("utf8", "IsSpacePerl", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_space, p, TRUE) != 0;
}

bool
Perl_is_utf8_digit(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_digit)
	PL_utf8_digit = swash_init("utf8", "IsDigit", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_digit, p, TRUE) != 0;
}

bool
Perl_is_utf8_upper(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_upper)
	PL_utf8_upper = swash_init("utf8", "IsUppercase", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_upper, p, TRUE) != 0;
}

bool
Perl_is_utf8_lower(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_lower)
	PL_utf8_lower = swash_init("utf8", "IsLowercase", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_lower, p, TRUE) != 0;
}

bool
Perl_is_utf8_cntrl(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_cntrl)
	PL_utf8_cntrl = swash_init("utf8", "IsCntrl", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_cntrl, p, TRUE) != 0;
}

bool
Perl_is_utf8_graph(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_graph)
	PL_utf8_graph = swash_init("utf8", "IsGraph", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_graph, p, TRUE) != 0;
}

bool
Perl_is_utf8_print(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_print)
	PL_utf8_print = swash_init("utf8", "IsPrint", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_print, p, TRUE) != 0;
}

bool
Perl_is_utf8_punct(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_punct)
	PL_utf8_punct = swash_init("utf8", "IsPunct", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_punct, p, TRUE) != 0;
}

bool
Perl_is_utf8_xdigit(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_xdigit)
	PL_utf8_xdigit = swash_init("utf8", "IsXDigit", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_xdigit, p, TRUE) != 0;
}

bool
Perl_is_utf8_mark(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_mark)
	PL_utf8_mark = swash_init("utf8", "IsM", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_mark, p, TRUE) != 0;
}

/*
=for apidoc A|UV|to_utf8_case|U8 *p|U8* ustrp|STRLEN *lenp|SV **swash|char *normal|char *special

The "p" contains the pointer to the UTF-8 string encoding
the character that is being converted.

The "ustrp" is a pointer to the character buffer to put the
conversion result to.  The "lenp" is a pointer to the length
of the result.

The "swashp" is a pointer to the swash to use.

Both the special and normal mappings are stored lib/unicore/To/Foo.pl,
and loaded by SWASHGET, using lib/utf8_heavy.pl.  The special (usually,
but not always, a multicharacter mapping), is tried first.

The "special" is a string like "utf8::ToSpecLower", which means the
hash %utf8::ToSpecLower.  The access to the hash is through
Perl_to_utf8_case().

The "normal" is a string like "ToLower" which means the swash
%utf8::ToLower.

=cut */

UV
Perl_to_utf8_case(pTHX_ U8 *p, U8* ustrp, STRLEN *lenp, SV **swashp, char *normal, char *special)
{
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    STRLEN len = 0;

    const UV uv0 = utf8_to_uvchr(p, NULL);
    /* The NATIVE_TO_UNI() and UNI_TO_NATIVE() mappings
     * are necessary in EBCDIC, they are redundant no-ops
     * in ASCII-ish platforms, and hopefully optimized away. */
    const UV uv1 = NATIVE_TO_UNI(uv0);
    uvuni_to_utf8(tmpbuf, uv1);

    if (!*swashp) /* load on-demand */
         *swashp = swash_init("utf8", normal, &PL_sv_undef, 4, 0);

    /* The 0xDF is the only special casing Unicode code point below 0x100. */
    if (special && (uv1 == 0xDF || uv1 > 0xFF)) {
         /* It might be "special" (sometimes, but not always,
	  * a multicharacter mapping) */
	 HV *hv;
	 SV **svp;

	 if ((hv  = get_hv(special, FALSE)) &&
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
			     UV c = utf8_to_uvchr(t, &tlen);
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
	 UV uv2 = swash_fetch(*swashp, tmpbuf, TRUE);
	 
	 if (uv2) {
	      /* It was "normal" (a single character mapping). */
	      UV uv3 = UNI_TO_NATIVE(uv2);
	      
	      len = uvchr_to_utf8(ustrp, uv3) - ustrp;
	 }
    }

    if (!len) /* Neither: just copy. */
	 len = uvchr_to_utf8(ustrp, uv0) - ustrp;

    if (lenp)
	 *lenp = len;

    return len ? utf8_to_uvchr(ustrp, 0) : 0;
}

/*
=for apidoc A|UV|to_utf8_upper|U8 *p|U8 *ustrp|STRLEN *lenp

Convert the UTF-8 encoded character at p to its uppercase version and
store that in UTF-8 in ustrp and its length in bytes in lenp.  Note
that the ustrp needs to be at least UTF8_MAXBYTES_CASE+1 bytes since
the uppercase version may be longer than the original character.

The first character of the uppercased version is returned
(but note, as explained above, that there may be more.)

=cut */

UV
Perl_to_utf8_upper(pTHX_ U8 *p, U8* ustrp, STRLEN *lenp)
{
    return Perl_to_utf8_case(aTHX_ p, ustrp, lenp,
                             &PL_utf8_toupper, "ToUpper", "utf8::ToSpecUpper");
}

/*
=for apidoc A|UV|to_utf8_title|U8 *p|U8 *ustrp|STRLEN *lenp

Convert the UTF-8 encoded character at p to its titlecase version and
store that in UTF-8 in ustrp and its length in bytes in lenp.  Note
that the ustrp needs to be at least UTF8_MAXBYTES_CASE+1 bytes since the
titlecase version may be longer than the original character.

The first character of the titlecased version is returned
(but note, as explained above, that there may be more.)

=cut */

UV
Perl_to_utf8_title(pTHX_ U8 *p, U8* ustrp, STRLEN *lenp)
{
    return Perl_to_utf8_case(aTHX_ p, ustrp, lenp,
                             &PL_utf8_totitle, "ToTitle", "utf8::ToSpecTitle");
}

/*
=for apidoc A|UV|to_utf8_lower|U8 *p|U8 *ustrp|STRLEN *lenp

Convert the UTF-8 encoded character at p to its lowercase version and
store that in UTF-8 in ustrp and its length in bytes in lenp.  Note
that the ustrp needs to be at least UTF8_MAXBYTES_CASE+1 bytes since the
lowercase version may be longer than the original character.

The first character of the lowercased version is returned
(but note, as explained above, that there may be more.)

=cut */

UV
Perl_to_utf8_lower(pTHX_ U8 *p, U8* ustrp, STRLEN *lenp)
{
    return Perl_to_utf8_case(aTHX_ p, ustrp, lenp,
                             &PL_utf8_tolower, "ToLower", "utf8::ToSpecLower");
}

/*
=for apidoc A|UV|to_utf8_fold|U8 *p|U8 *ustrp|STRLEN *lenp

Convert the UTF-8 encoded character at p to its foldcase version and
store that in UTF-8 in ustrp and its length in bytes in lenp.  Note
that the ustrp needs to be at least UTF8_MAXBYTES_CASE+1 bytes since the
foldcase version may be longer than the original character (up to
three characters).

The first character of the foldcased version is returned
(but note, as explained above, that there may be more.)

=cut */

UV
Perl_to_utf8_fold(pTHX_ U8 *p, U8* ustrp, STRLEN *lenp)
{
    return Perl_to_utf8_case(aTHX_ p, ustrp, lenp,
                             &PL_utf8_tofold, "ToFold", "utf8::ToSpecFold");
}

/* a "swash" is a swatch hash */

SV*
Perl_swash_init(pTHX_ char* pkg, char* name, SV *listsv, I32 minbits, I32 none)
{
    SV* retval;
    SV* const tokenbufsv = sv_newmortal();
    dSP;
    const size_t pkg_len = strlen(pkg);
    const size_t name_len = strlen(name);
    HV * const stash = gv_stashpvn(pkg, pkg_len, FALSE);
    SV* errsv_save;

    PUSHSTACKi(PERLSI_MAGIC);
    ENTER;
    SAVEI32(PL_hints);
    PL_hints = 0;
    save_re_context();
    if (!gv_fetchmeth(stash, "SWASHNEW", 8, -1)) {	/* demand load utf8 */
	ENTER;
	errsv_save = newSVsv(ERRSV);
	Perl_load_module(aTHX_ PERL_LOADMOD_NOIMPORT, newSVpvn(pkg,pkg_len),
			 Nullsv);
	if (!SvTRUE(ERRSV))
	    sv_setsv(ERRSV, errsv_save);
	SvREFCNT_dec(errsv_save);
	LEAVE;
    }
    SPAGAIN;
    PUSHMARK(SP);
    EXTEND(SP,5);
    PUSHs(sv_2mortal(newSVpvn(pkg, pkg_len)));
    PUSHs(sv_2mortal(newSVpvn(name, name_len)));
    PUSHs(listsv);
    PUSHs(sv_2mortal(newSViv(minbits)));
    PUSHs(sv_2mortal(newSViv(none)));
    PUTBACK;
    if (IN_PERL_COMPILETIME) {
	/* XXX ought to be handled by lex_start */
	SAVEI32(PL_in_my);
	PL_in_my = 0;
	sv_setpv(tokenbufsv, PL_tokenbuf);
    }
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
	STRLEN len;
	const char* const pv = SvPV_const(tokenbufsv, len);

	Copy(pv, PL_tokenbuf, len+1, char);
	PL_curcop->op_private = (U8)(PL_hints & HINT_PRIVATE_MASK);
    }
    if (!SvROK(retval) || SvTYPE(SvRV(retval)) != SVt_PVHV) {
        if (SvPOK(retval))
	    Perl_croak(aTHX_ "Can't find Unicode property definition \"%"SVf"\"",
		       retval);
	Perl_croak(aTHX_ "SWASHNEW didn't return an HV ref");
    }
    return retval;
}


/* This API is wrong for special case conversions since we may need to
 * return several Unicode characters for a single Unicode character
 * (see lib/unicore/SpecCase.txt) The SWASHGET in lib/utf8_heavy.pl is
 * the lower-level routine, and it is similarly broken for returning
 * multiple values.  --jhi */
UV
Perl_swash_fetch(pTHX_ SV *sv, U8 *ptr, bool do_utf8)
{
    HV* const hv = (HV*)SvRV(sv);
    U32 klen;
    U32 off;
    STRLEN slen;
    STRLEN needents;
    const U8 *tmps = NULL;
    U32 bit;
    SV *retval;
    U8 tmputf8[2];
    UV c = NATIVE_TO_ASCII(*ptr);

    if (!do_utf8 && !UNI_IS_INVARIANT(c)) {
        tmputf8[0] = (U8)UTF8_EIGHT_BIT_HI(c);
        tmputf8[1] = (U8)UTF8_EIGHT_BIT_LO(c);
        ptr = tmputf8;
    }
    /* Given a UTF-X encoded char 0xAA..0xYY,0xZZ
     * then the "swatch" is a vec() for al the chars which start
     * with 0xAA..0xYY
     * So the key in the hash (klen) is length of encoded char -1
     */
    klen = UTF8SKIP(ptr) - 1;
    off  = ptr[klen];

    if (klen == 0)
     {
      /* If char in invariant then swatch is for all the invariant chars
       * In both UTF-8 and UTF-8-MOD that happens to be UTF_CONTINUATION_MARK
       */
      needents = UTF_CONTINUATION_MARK;
      off      = NATIVE_TO_UTF(ptr[klen]);
     }
    else
     {
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

	/* If not cached, generate it via utf8::SWASHGET */
	if (!svp || !SvPOK(*svp) || !(tmps = (const U8*)SvPV_const(*svp, slen))) {
	    dSP;
	    /* We use utf8n_to_uvuni() as we want an index into
	       Unicode tables, not a native character number.
	     */
	    const UV code_point = utf8n_to_uvuni(ptr, UTF8_MAXBYTES, 0,
					   ckWARN(WARN_UTF8) ?
					   0 : UTF8_ALLOW_ANY);
	    SV *errsv_save;
	    ENTER;
	    SAVETMPS;
	    save_re_context();
	    PUSHSTACKi(PERLSI_MAGIC);
	    PUSHMARK(SP);
	    EXTEND(SP,3);
	    PUSHs((SV*)sv);
	    /* On EBCDIC & ~(0xA0-1) isn't a useful thing to do */
	    PUSHs(sv_2mortal(newSViv((klen) ?
				     (code_point & ~(needents - 1)) : 0)));
	    PUSHs(sv_2mortal(newSViv(needents)));
	    PUTBACK;
	    errsv_save = newSVsv(ERRSV);
	    if (call_method("SWASHGET", G_SCALAR))
		retval = newSVsv(*PL_stack_sp--);
	    else
		retval = &PL_sv_undef;
	    if (!SvTRUE(ERRSV))
		sv_setsv(ERRSV, errsv_save);
	    SvREFCNT_dec(errsv_save);
	    POPSTACK;
	    FREETMPS;
	    LEAVE;
	    if (IN_PERL_COMPILETIME)
		PL_curcop->op_private = (U8)(PL_hints & HINT_PRIVATE_MASK);

	    svp = hv_store(hv, (const char *)ptr, klen, retval, 0);

	    if (!svp || !(tmps = (U8*)SvPV(*svp, slen)) || (slen << 3) < needents)
		Perl_croak(aTHX_ "SWASHGET didn't return result of proper length");
	}

	PL_last_swash_hv = hv;
	PL_last_swash_klen = klen;
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
    Perl_croak(aTHX_ "panic: swash_fetch");
    return 0;
}


/*
=for apidoc A|U8 *|uvchr_to_utf8|U8 *d|UV uv

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
#undef Perl_uvchr_to_utf8
U8 *
Perl_uvchr_to_utf8(pTHX_ U8 *d, UV uv)
{
    return Perl_uvuni_to_utf8_flags(aTHX_ d, NATIVE_TO_UNI(uv), 0);
}

U8 *
Perl_uvchr_to_utf8_flags(pTHX_ U8 *d, UV uv, UV flags)
{
    return Perl_uvuni_to_utf8_flags(aTHX_ d, NATIVE_TO_UNI(uv), flags);
}

/*
=for apidoc A|UV|utf8n_to_uvchr|U8 *s|STRLEN curlen|STRLEN *retlen|U32 flags

Returns the native character value of the first character in the string C<s>
which is assumed to be in UTF-8 encoding; C<retlen> will be set to the
length, in bytes, of that character.

Allows length and flags to be passed to low level routine.

=cut
*/
/* On ASCII machines this is normally a macro but we want
   a real function in case XS code wants it
*/
#undef Perl_utf8n_to_uvchr
UV
Perl_utf8n_to_uvchr(pTHX_ U8 *s, STRLEN curlen, STRLEN *retlen, U32 flags)
{
    const UV uv = Perl_utf8n_to_uvuni(aTHX_ s, curlen, retlen, flags);
    return UNI_TO_NATIVE(uv);
}

/*
=for apidoc A|char *|pv_uni_display|SV *dsv|U8 *spv|STRLEN len|STRLEN pvlim|UV flags

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
Perl_pv_uni_display(pTHX_ SV *dsv, U8 *spv, STRLEN len, STRLEN pvlim, UV flags)
{
    int truncated = 0;
    const char *s, *e;

    sv_setpvn(dsv, "", 0);
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
	     if (!ok && (flags & UNI_DISPLAY_BACKSLASH)) {
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
		     Perl_sv_catpvf(aTHX_ dsv, "\\%c", ok);
		 }
	     }
	     /* isPRINT() is the locale-blind version. */
	     if (!ok && (flags & UNI_DISPLAY_ISPRINT) && isPRINT(c)) {
	         Perl_sv_catpvf(aTHX_ dsv, "%c", c);
		 ok = 1;
	     }
	 }
	 if (!ok)
	     Perl_sv_catpvf(aTHX_ dsv, "\\x{%"UVxf"}", u);
    }
    if (truncated)
	 sv_catpvn(dsv, "...", 3);
    
    return SvPVX(dsv);
}

/*
=for apidoc A|char *|sv_uni_display|SV *dsv|SV *ssv|STRLEN pvlim|UV flags

Build to the scalar dsv a displayable version of the scalar sv,
the displayable version being at most pvlim bytes long
(if longer, the rest is truncated and "..." will be appended).

The flags argument is as in pv_uni_display().

The pointer to the PV of the dsv is returned.

=cut */
char *
Perl_sv_uni_display(pTHX_ SV *dsv, SV *ssv, STRLEN pvlim, UV flags)
{
     return Perl_pv_uni_display(aTHX_ dsv, (U8*)SvPVX_const(ssv),
				SvCUR(ssv), pvlim, flags);
}

/*
=for apidoc A|I32|ibcmp_utf8|const char *s1|char **pe1|register UV l1|bool u1|const char *s2|char **pe2|register UV l2|bool u2

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
     register const U8 *p1  = (const U8*)s1;
     register const U8 *p2  = (const U8*)s2;
     register const U8 *f1 = 0, *f2 = 0;
     register U8 *e1 = 0, *q1 = 0;
     register U8 *e2 = 0, *q2 = 0;
     STRLEN n1 = 0, n2 = 0;
     U8 foldbuf1[UTF8_MAXBYTES_CASE+1];
     U8 foldbuf2[UTF8_MAXBYTES_CASE+1];
     U8 natbuf[1+1];
     STRLEN foldlen1, foldlen2;
     bool match;
     
     if (pe1)
	  e1 = *(U8**)pe1;
     if (e1 == 0 || (l1 && l1 < (UV)(e1 - (const U8*)s1)))
	  f1 = (const U8*)s1 + l1;
     if (pe2)
	  e2 = *(U8**)pe2;
     if (e2 == 0 || (l2 && l2 < (UV)(e2 - (const U8*)s2)))
	  f2 = (const U8*)s2 + l2;

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
		    to_utf8_fold((U8 *)p1, foldbuf1, &foldlen1);
	       else {
		    uvuni_to_utf8(natbuf, (UV) NATIVE_TO_UNI(((UV)*p1)));
		    to_utf8_fold(natbuf, foldbuf1, &foldlen1);
	       }
	       q1 = foldbuf1;
	       n1 = foldlen1;
	  }
	  if (n2 == 0) {
	       if (u2)
		    to_utf8_fold((U8 *)p2, foldbuf2, &foldlen2);
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
     match = (f1 == 0 || p1 == f1) && (f2 == 0 || p2 == f2);

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
