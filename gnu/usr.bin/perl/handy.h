/*    handy.h
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1999, 2000,
 *    2001, 2002, 2004, 2005, 2006, 2007, 2008, 2012 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* IMPORTANT NOTE: Everything whose name begins with an underscore is for
 * internal core Perl use only. */

#ifndef HANDY_H /* Guard against nested #inclusion */
#define HANDY_H

#if !defined(__STDC__)
#ifdef NULL
#undef NULL
#endif
#  define NULL 0
#endif

#ifndef PERL_CORE
#  define Null(type) ((type)NULL)

/*
=head1 Handy Values

=for apidoc AmU||Nullch
Null character pointer.  (No longer available when C<PERL_CORE> is
defined.)

=for apidoc AmU||Nullsv
Null SV pointer.  (No longer available when C<PERL_CORE> is defined.)

=cut
*/

#  define Nullch Null(char*)
#  define Nullfp Null(PerlIO*)
#  define Nullsv Null(SV*)
#endif

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE (1)
#define FALSE (0)

/* The MUTABLE_*() macros cast pointers to the types shown, in such a way
 * (compiler permitting) that casting away const-ness will give a warning;
 * e.g.:
 *
 * const SV *sv = ...;
 * AV *av1 = (AV*)sv;        <== BAD:  the const has been silently cast away
 * AV *av2 = MUTABLE_AV(sv); <== GOOD: it may warn
 */

#if defined(__GNUC__) && !defined(PERL_GCC_BRACE_GROUPS_FORBIDDEN)
#  define MUTABLE_PTR(p) ({ void *_p = (p); _p; })
#else
#  define MUTABLE_PTR(p) ((void *) (p))
#endif

#define MUTABLE_AV(p)	((AV *)MUTABLE_PTR(p))
#define MUTABLE_CV(p)	((CV *)MUTABLE_PTR(p))
#define MUTABLE_GV(p)	((GV *)MUTABLE_PTR(p))
#define MUTABLE_HV(p)	((HV *)MUTABLE_PTR(p))
#define MUTABLE_IO(p)	((IO *)MUTABLE_PTR(p))
#define MUTABLE_SV(p)	((SV *)MUTABLE_PTR(p))

#if defined(I_STDBOOL) && !defined(PERL_BOOL_AS_CHAR)
#  include <stdbool.h>
#  ifndef HAS_BOOL
#    define HAS_BOOL 1
#  endif
#endif

/* bool is built-in for g++-2.6.3 and later, which might be used
   for extensions.  <_G_config.h> defines _G_HAVE_BOOL, but we can't
   be sure _G_config.h will be included before this file.  _G_config.h
   also defines _G_HAVE_BOOL for both gcc and g++, but only g++
   actually has bool.  Hence, _G_HAVE_BOOL is pretty useless for us.
   g++ can be identified by __GNUG__.
   Andy Dougherty	February 2000
*/
#ifdef __GNUG__		/* GNU g++ has bool built-in */
# ifndef PERL_BOOL_AS_CHAR
#  ifndef HAS_BOOL
#    define HAS_BOOL 1
#  endif
# endif
#endif

/* The NeXT dynamic loader headers will not build with the bool macro
   So declare them now to clear confusion.
*/
#if defined(NeXT) || defined(__NeXT__)
# undef FALSE
# undef TRUE
  typedef enum bool { FALSE = 0, TRUE = 1 } bool;
# define ENUM_BOOL 1
# ifndef HAS_BOOL
#  define HAS_BOOL 1
# endif /* !HAS_BOOL */
#endif /* NeXT || __NeXT__ */

#ifndef HAS_BOOL
# ifdef bool
#  undef bool
# endif
# define bool char
# define HAS_BOOL 1
#endif

/* cast-to-bool.  A simple (bool) cast may not do the right thing: if bool is
 * defined as char for example, then the cast from int is
 * implementation-defined (bool)!!(cbool) in a ternary triggers a bug in xlc on
 * AIX */
#define cBOOL(cbool) ((cbool) ? (bool)1 : (bool)0)

/* Try to figure out __func__ or __FUNCTION__ equivalent, if any.
 * XXX Should really be a Configure probe, with HAS__FUNCTION__
 *     and FUNCTION__ as results.
 * XXX Similarly, a Configure probe for __FILE__ and __LINE__ is needed. */
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || (defined(__SUNPRO_C)) /* C99 or close enough. */
#  define FUNCTION__ __func__
#else
#  if (defined(USING_MSVC6)) || /* MSVC6 has neither __func__ nor __FUNCTION and no good workarounds, either. */ \
      (defined(__DECC_VER)) /* Tru64 or VMS, and strict C89 being used, but not modern enough cc (in Tur64, -c99 not known, only -std1). */
#    define FUNCTION__ ""
#  else
#    define FUNCTION__ __FUNCTION__ /* Common extension. */
#  endif
#endif

/* XXX A note on the perl source internal type system.  The
   original intent was that I32 be *exactly* 32 bits.

   Currently, we only guarantee that I32 is *at least* 32 bits.
   Specifically, if int is 64 bits, then so is I32.  (This is the case
   for the Cray.)  This has the advantage of meshing nicely with
   standard library calls (where we pass an I32 and the library is
   expecting an int), but the disadvantage that an I32 is not 32 bits.
   Andy Dougherty	August 1996

   There is no guarantee that there is *any* integral type with
   exactly 32 bits.  It is perfectly legal for a system to have
   sizeof(short) == sizeof(int) == sizeof(long) == 8.

   Similarly, there is no guarantee that I16 and U16 have exactly 16
   bits.

   For dealing with issues that may arise from various 32/64-bit
   systems, we will ask Configure to check out

	SHORTSIZE == sizeof(short)
	INTSIZE == sizeof(int)
	LONGSIZE == sizeof(long)
	LONGLONGSIZE == sizeof(long long) (if HAS_LONG_LONG)
	PTRSIZE == sizeof(void *)
	DOUBLESIZE == sizeof(double)
	LONG_DOUBLESIZE == sizeof(long double) (if HAS_LONG_DOUBLE).

*/

#ifdef I_INTTYPES /* e.g. Linux has int64_t without <inttypes.h> */
#   include <inttypes.h>
#   ifdef INT32_MIN_BROKEN
#       undef  INT32_MIN
#       define INT32_MIN (-2147483647-1)
#   endif
#   ifdef INT64_MIN_BROKEN
#       undef  INT64_MIN
#       define INT64_MIN (-9223372036854775807LL-1)
#   endif
#endif

typedef I8TYPE I8;
typedef U8TYPE U8;
typedef I16TYPE I16;
typedef U16TYPE U16;
typedef I32TYPE I32;
typedef U32TYPE U32;
#ifdef PERL_CORE
#   ifdef HAS_QUAD
typedef I64TYPE I64;
typedef U64TYPE U64;
#   endif
#endif /* PERL_CORE */

#if defined(HAS_QUAD) && defined(USE_64_BIT_INT)
#   if defined(HAS_LONG_LONG) && QUADKIND == QUAD_IS_LONG_LONG
#       define PeRl_INT64_C(c)	CAT2(c,LL)
#       define PeRl_UINT64_C(c)	CAT2(c,ULL)
#   else
#       if QUADKIND == QUAD_IS___INT64
#           define PeRl_INT64_C(c)	CAT2(c,I64)
#           define PeRl_UINT64_C(c)	CAT2(c,UI64)
#       else
#           if LONGSIZE == 8 && QUADKIND == QUAD_IS_LONG
#               define PeRl_INT64_C(c)	CAT2(c,L)
#               define PeRl_UINT64_C(c)	CAT2(c,UL)
#           else
#               define PeRl_INT64_C(c)	((I64TYPE)(c))
#               define PeRl_UINT64_C(c)	((U64TYPE)(c))
#           endif
#       endif
#   endif
#   ifndef UINT64_C
#   define UINT64_C(c) PeRl_UINT64_C(c)
#   endif
#   ifndef INT64_C
#   define INT64_C(c) PeRl_INT64_C(c)
#   endif
#endif

#if defined(UINT8_MAX) && defined(INT16_MAX) && defined(INT32_MAX)

/* I8_MAX and I8_MIN constants are not defined, as I8 is an ambiguous type.
   Please search CHAR_MAX in perl.h for further details. */
#define U8_MAX UINT8_MAX
#define U8_MIN UINT8_MIN

#define I16_MAX INT16_MAX
#define I16_MIN INT16_MIN
#define U16_MAX UINT16_MAX
#define U16_MIN UINT16_MIN

#define I32_MAX INT32_MAX
#define I32_MIN INT32_MIN
#ifndef UINT32_MAX_BROKEN /* e.g. HP-UX with gcc messes this up */
#  define U32_MAX UINT32_MAX
#else
#  define U32_MAX 4294967295U
#endif
#define U32_MIN UINT32_MIN

#else

/* I8_MAX and I8_MIN constants are not defined, as I8 is an ambiguous type.
   Please search CHAR_MAX in perl.h for further details. */
#define U8_MAX PERL_UCHAR_MAX
#define U8_MIN PERL_UCHAR_MIN

#define I16_MAX PERL_SHORT_MAX
#define I16_MIN PERL_SHORT_MIN
#define U16_MAX PERL_USHORT_MAX
#define U16_MIN PERL_USHORT_MIN

#if LONGSIZE > 4
# define I32_MAX PERL_INT_MAX
# define I32_MIN PERL_INT_MIN
# define U32_MAX PERL_UINT_MAX
# define U32_MIN PERL_UINT_MIN
#else
# define I32_MAX PERL_LONG_MAX
# define I32_MIN PERL_LONG_MIN
# define U32_MAX PERL_ULONG_MAX
# define U32_MIN PERL_ULONG_MIN
#endif

#endif

/* log(2) is pretty close to  0.30103, just in case anyone is grepping for it */
#define BIT_DIGITS(N)   (((N)*146)/485 + 1)  /* log2(10) =~ 146/485 */
#define TYPE_DIGITS(T)  BIT_DIGITS(sizeof(T) * 8)
#define TYPE_CHARS(T)   (TYPE_DIGITS(T) + 2) /* sign, NUL */

#define Ctl(ch) ((ch) & 037)

/* This is a helper macro to avoid preprocessor issues, replaced by nothing
 * unless under DEBUGGING, where it expands to an assert of its argument,
 * followed by a comma (hence the comma operator).  If we just used a straight
 * assert(), we would get a comma with nothing before it when not DEBUGGING */
#ifdef DEBUGGING
#   define __ASSERT_(statement)  assert(statement),
#else
#   define __ASSERT_(statement)
#endif

/*
=head1 SV-Body Allocation

=for apidoc Ama|SV*|newSVpvs|const char* s
Like C<newSVpvn>, but takes a literal C<NUL>-terminated string instead of a
string/length pair.

=for apidoc Ama|SV*|newSVpvs_flags|const char* s|U32 flags
Like C<newSVpvn_flags>, but takes a literal C<NUL>-terminated string instead of
a string/length pair.

=for apidoc Ama|SV*|newSVpvs_share|const char* s
Like C<newSVpvn_share>, but takes a literal C<NUL>-terminated string instead of
a string/length pair and omits the hash parameter.

=for apidoc Am|void|sv_catpvs_flags|SV* sv|const char* s|I32 flags
Like C<sv_catpvn_flags>, but takes a literal C<NUL>-terminated string instead
of a string/length pair.

=for apidoc Am|void|sv_catpvs_nomg|SV* sv|const char* s
Like C<sv_catpvn_nomg>, but takes a literal string instead of a
string/length pair.

=for apidoc Am|void|sv_catpvs|SV* sv|const char* s
Like C<sv_catpvn>, but takes a literal string instead of a string/length pair.

=for apidoc Am|void|sv_catpvs_mg|SV* sv|const char* s
Like C<sv_catpvn_mg>, but takes a literal string instead of a
string/length pair.

=for apidoc Am|void|sv_setpvs|SV* sv|const char* s
Like C<sv_setpvn>, but takes a literal string instead of a string/length pair.

=for apidoc Am|void|sv_setpvs_mg|SV* sv|const char* s
Like C<sv_setpvn_mg>, but takes a literal string instead of a
string/length pair.

=for apidoc Am|SV *|sv_setref_pvs|const char* s
Like C<sv_setref_pvn>, but takes a literal string instead of a
string/length pair.

=head1 Memory Management

=for apidoc Ama|char*|savepvs|const char* s
Like C<savepvn>, but takes a literal C<NUL>-terminated string instead of a
string/length pair.

=for apidoc Ama|char*|savesharedpvs|const char* s
A version of C<savepvs()> which allocates the duplicate string in memory
which is shared between threads.

=head1 GV Functions

=for apidoc Am|HV*|gv_stashpvs|const char* name|I32 create
Like C<gv_stashpvn>, but takes a literal string instead of a string/length pair.

=head1 Hash Manipulation Functions

=for apidoc Am|SV**|hv_fetchs|HV* tb|const char* key|I32 lval
Like C<hv_fetch>, but takes a literal string instead of a string/length pair.

=for apidoc Am|SV**|hv_stores|HV* tb|const char* key|NULLOK SV* val
Like C<hv_store>, but takes a literal string instead of a string/length pair
and omits the hash parameter.

=head1 Lexer interface

=for apidoc Amx|void|lex_stuff_pvs|const char *pv|U32 flags

Like L</lex_stuff_pvn>, but takes a literal string instead of a
string/length pair.

=cut
*/

/* concatenating with "" ensures that only literal strings are accepted as
 * argument */
#define STR_WITH_LEN(s)  ("" s ""), (sizeof(s)-1)

/* note that STR_WITH_LEN() can't be used as argument to macros or functions
 * that under some configurations might be macros, which means that it requires
 * the full Perl_xxx(aTHX_ ...) form for any API calls where it's used.
 */

/* STR_WITH_LEN() shortcuts */
#define newSVpvs(str) Perl_newSVpvn(aTHX_ STR_WITH_LEN(str))
#define newSVpvs_flags(str,flags)	\
    Perl_newSVpvn_flags(aTHX_ STR_WITH_LEN(str), flags)
#define newSVpvs_share(str) Perl_newSVpvn_share(aTHX_ STR_WITH_LEN(str), 0)
#define sv_catpvs_flags(sv, str, flags) \
    Perl_sv_catpvn_flags(aTHX_ sv, STR_WITH_LEN(str), flags)
#define sv_catpvs_nomg(sv, str) \
    Perl_sv_catpvn_flags(aTHX_ sv, STR_WITH_LEN(str), 0)
#define sv_catpvs(sv, str) \
    Perl_sv_catpvn_flags(aTHX_ sv, STR_WITH_LEN(str), SV_GMAGIC)
#define sv_catpvs_mg(sv, str) \
    Perl_sv_catpvn_flags(aTHX_ sv, STR_WITH_LEN(str), SV_GMAGIC|SV_SMAGIC)
#define sv_setpvs(sv, str) Perl_sv_setpvn(aTHX_ sv, STR_WITH_LEN(str))
#define sv_setpvs_mg(sv, str) Perl_sv_setpvn_mg(aTHX_ sv, STR_WITH_LEN(str))
#define sv_setref_pvs(rv, classname, str) \
    Perl_sv_setref_pvn(aTHX_ rv, classname, STR_WITH_LEN(str))
#define savepvs(str) Perl_savepvn(aTHX_ STR_WITH_LEN(str))
#define savesharedpvs(str) Perl_savesharedpvn(aTHX_ STR_WITH_LEN(str))
#define gv_stashpvs(str, create) \
    Perl_gv_stashpvn(aTHX_ STR_WITH_LEN(str), create)
#define gv_fetchpvs(namebeg, add, sv_type) \
    Perl_gv_fetchpvn_flags(aTHX_ STR_WITH_LEN(namebeg), add, sv_type)
#define gv_fetchpvn(namebeg, len, add, sv_type) \
    Perl_gv_fetchpvn_flags(aTHX_ namebeg, len, add, sv_type)
#define sv_catxmlpvs(dsv, str, utf8) \
    Perl_sv_catxmlpvn(aTHX_ dsv, STR_WITH_LEN(str), utf8)
#define hv_fetchs(hv,key,lval)						\
  ((SV **)Perl_hv_common(aTHX_ (hv), NULL, STR_WITH_LEN(key), 0,	\
			 (lval) ? (HV_FETCH_JUST_SV | HV_FETCH_LVALUE)	\
			 : HV_FETCH_JUST_SV, NULL, 0))

#define hv_stores(hv,key,val)						\
  ((SV **)Perl_hv_common(aTHX_ (hv), NULL, STR_WITH_LEN(key), 0,	\
			 (HV_FETCH_ISSTORE|HV_FETCH_JUST_SV), (val), 0))

#define lex_stuff_pvs(pv,flags) Perl_lex_stuff_pvn(aTHX_ STR_WITH_LEN(pv), flags)

#define get_cvs(str, flags)					\
	Perl_get_cvn_flags(aTHX_ STR_WITH_LEN(str), (flags))

/*
=head1 Miscellaneous Functions

=for apidoc Am|bool|strNE|char* s1|char* s2
Test two strings to see if they are different.  Returns true or
false.

=for apidoc Am|bool|strEQ|char* s1|char* s2
Test two strings to see if they are equal.  Returns true or false.

=for apidoc Am|bool|strLT|char* s1|char* s2
Test two strings to see if the first, C<s1>, is less than the second,
C<s2>.  Returns true or false.

=for apidoc Am|bool|strLE|char* s1|char* s2
Test two strings to see if the first, C<s1>, is less than or equal to the
second, C<s2>.  Returns true or false.

=for apidoc Am|bool|strGT|char* s1|char* s2
Test two strings to see if the first, C<s1>, is greater than the second,
C<s2>.  Returns true or false.

=for apidoc Am|bool|strGE|char* s1|char* s2
Test two strings to see if the first, C<s1>, is greater than or equal to
the second, C<s2>.  Returns true or false.

=for apidoc Am|bool|strnNE|char* s1|char* s2|STRLEN len
Test two strings to see if they are different.  The C<len> parameter
indicates the number of bytes to compare.  Returns true or false.  (A
wrapper for C<strncmp>).

=for apidoc Am|bool|strnEQ|char* s1|char* s2|STRLEN len
Test two strings to see if they are equal.  The C<len> parameter indicates
the number of bytes to compare.  Returns true or false.  (A wrapper for
C<strncmp>).

=cut
*/

#define strNE(s1,s2) (strcmp(s1,s2))
#define strEQ(s1,s2) (!strcmp(s1,s2))
#define strLT(s1,s2) (strcmp(s1,s2) < 0)
#define strLE(s1,s2) (strcmp(s1,s2) <= 0)
#define strGT(s1,s2) (strcmp(s1,s2) > 0)
#define strGE(s1,s2) (strcmp(s1,s2) >= 0)
#define strnNE(s1,s2,l) (strncmp(s1,s2,l))
#define strnEQ(s1,s2,l) (!strncmp(s1,s2,l))

#ifdef HAS_MEMCMP
#  define memNE(s1,s2,l) (memcmp(s1,s2,l))
#  define memEQ(s1,s2,l) (!memcmp(s1,s2,l))
#else
#  define memNE(s1,s2,l) (bcmp(s1,s2,l))
#  define memEQ(s1,s2,l) (!bcmp(s1,s2,l))
#endif

#define memEQs(s1, l, s2) \
	(sizeof(s2)-1 == l && memEQ(s1, ("" s2 ""), (sizeof(s2)-1)))
#define memNEs(s1, l, s2) !memEQs(s1, l, s2)

/*
 * Character classes.
 *
 * Unfortunately, the introduction of locales means that we
 * can't trust isupper(), etc. to tell the truth.  And when
 * it comes to /\w+/ with tainting enabled, we *must* be able
 * to trust our character classes.
 *
 * Therefore, the default tests in the text of Perl will be
 * independent of locale.  Any code that wants to depend on
 * the current locale will use the tests that begin with "lc".
 */

#ifdef HAS_SETLOCALE  /* XXX Is there a better test for this? */
#  ifndef CTYPE256
#    define CTYPE256
#  endif
#endif

/*

=head1 Character classes
This section is about functions (really macros) that classify characters
into types, such as punctuation versus alphabetic, etc.  Most of these are
analogous to regular expression character classes.  (See
L<perlrecharclass/POSIX Character Classes>.)  There are several variants for
each class.  (Not all macros have all variants; each item below lists the
ones valid for it.)  None are affected by C<use bytes>, and only the ones
with C<LC> in the name are affected by the current locale.

The base function, e.g., C<isALPHA()>, takes an octet (either a C<char> or a
C<U8>) as input and returns a boolean as to whether or not the character
represented by that octet is (or on non-ASCII platforms, corresponds to) an
ASCII character in the named class based on platform, Unicode, and Perl rules.
If the input is a number that doesn't fit in an octet, FALSE is returned.

Variant C<isFOO_A> (e.g., C<isALPHA_A()>) is identical to the base function
with no suffix C<"_A">.

Variant C<isFOO_L1> imposes the Latin-1 (or EBCDIC equivlalent) character set
onto the platform.  That is, the code points that are ASCII are unaffected,
since ASCII is a subset of Latin-1.  But the non-ASCII code points are treated
as if they are Latin-1 characters.  For example, C<isWORDCHAR_L1()> will return
true when called with the code point 0xDF, which is a word character in both
ASCII and EBCDIC (though it represents different characters in each).

Variant C<isFOO_uni> is like the C<isFOO_L1> variant, but accepts any UV code
point as input.  If the code point is larger than 255, Unicode rules are used
to determine if it is in the character class.  For example,
C<isWORDCHAR_uni(0x100)> returns TRUE, since 0x100 is LATIN CAPITAL LETTER A
WITH MACRON in Unicode, and is a word character.

Variant C<isFOO_utf8> is like C<isFOO_uni>, but the input is a pointer to a
(known to be well-formed) UTF-8 encoded string (C<U8*> or C<char*>).  The
classification of just the first (possibly multi-byte) character in the string
is tested.

Variant C<isFOO_LC> is like the C<isFOO_A> and C<isFOO_L1> variants, but the
result is based on the current locale, which is what C<LC> in the name stands
for.  If Perl can determine that the current locale is a UTF-8 locale, it uses
the published Unicode rules; otherwise, it uses the C library function that
gives the named classification.  For example, C<isDIGIT_LC()> when not in a
UTF-8 locale returns the result of calling C<isdigit()>.  FALSE is always
returned if the input won't fit into an octet.

Variant C<isFOO_LC_uvchr> is like C<isFOO_LC>, but is defined on any UV.  It
returns the same as C<isFOO_LC> for input code points less than 256, and
returns the hard-coded, not-affected-by-locale, Unicode results for larger ones.

Variant C<isFOO_LC_utf8> is like C<isFOO_LC_uvchr>, but the input is a pointer to a
(known to be well-formed) UTF-8 encoded string (C<U8*> or C<char*>).  The
classification of just the first (possibly multi-byte) character in the string
is tested.

=for apidoc Am|bool|isALPHA|char ch
Returns a boolean indicating whether the specified character is an
alphabetic character, analogous to C<m/[[:alpha:]]/>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isALPHA_A>, C<isALPHA_L1>, C<isALPHA_uni>, C<isALPHA_utf8>, C<isALPHA_LC>,
C<isALPHA_LC_uvchr>, and C<isALPHA_LC_utf8>.

=for apidoc Am|bool|isALPHANUMERIC|char ch
Returns a boolean indicating whether the specified character is a either an
alphabetic character or decimal digit, analogous to C<m/[[:alnum:]]/>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isALPHANUMERIC_A>, C<isALPHANUMERIC_L1>, C<isALPHANUMERIC_uni>,
C<isALPHANUMERIC_utf8>, C<isALPHANUMERIC_LC>, C<isALPHANUMERIC_LC_uvchr>, and
C<isALPHANUMERIC_LC_utf8>.

=for apidoc Am|bool|isASCII|char ch
Returns a boolean indicating whether the specified character is one of the 128
characters in the ASCII character set, analogous to C<m/[[:ascii:]]/>.
On non-ASCII platforms, it returns TRUE iff this
character corresponds to an ASCII character.  Variants C<isASCII_A()> and
C<isASCII_L1()> are identical to C<isASCII()>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isASCII_uni>, C<isASCII_utf8>, C<isASCII_LC>, C<isASCII_LC_uvchr>, and
C<isASCII_LC_utf8>.  Note, however, that some platforms do not have the C
library routine C<isascii()>.  In these cases, the variants whose names contain
C<LC> are the same as the corresponding ones without.

Also note, that because all ASCII characters are UTF-8 invariant (meaning they
have the exact same representation (always a single byte) whether encoded in
UTF-8 or not), C<isASCII> will give the correct results when called with any
byte in any string encoded or not in UTF-8.  And similarly C<isASCII_utf8> will
work properly on any string encoded or not in UTF-8.

=for apidoc Am|bool|isBLANK|char ch
Returns a boolean indicating whether the specified character is a
character considered to be a blank, analogous to C<m/[[:blank:]]/>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isBLANK_A>, C<isBLANK_L1>, C<isBLANK_uni>, C<isBLANK_utf8>, C<isBLANK_LC>,
C<isBLANK_LC_uvchr>, and C<isBLANK_LC_utf8>.  Note, however, that some
platforms do not have the C library routine C<isblank()>.  In these cases, the
variants whose names contain C<LC> are the same as the corresponding ones
without.

=for apidoc Am|bool|isCNTRL|char ch
Returns a boolean indicating whether the specified character is a
control character, analogous to C<m/[[:cntrl:]]/>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isCNTRL_A>, C<isCNTRL_L1>, C<isCNTRL_uni>, C<isCNTRL_utf8>, C<isCNTRL_LC>,
C<isCNTRL_LC_uvchr>, and C<isCNTRL_LC_utf8>
On EBCDIC platforms, you almost always want to use the C<isCNTRL_L1> variant.

=for apidoc Am|bool|isDIGIT|char ch
Returns a boolean indicating whether the specified character is a
digit, analogous to C<m/[[:digit:]]/>.
Variants C<isDIGIT_A> and C<isDIGIT_L1> are identical to C<isDIGIT>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isDIGIT_uni>, C<isDIGIT_utf8>, C<isDIGIT_LC>, C<isDIGIT_LC_uvchr>, and
C<isDIGIT_LC_utf8>.

=for apidoc Am|bool|isGRAPH|char ch
Returns a boolean indicating whether the specified character is a
graphic character, analogous to C<m/[[:graph:]]/>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isGRAPH_A>, C<isGRAPH_L1>, C<isGRAPH_uni>, C<isGRAPH_utf8>, C<isGRAPH_LC>,
C<isGRAPH_LC_uvchr>, and C<isGRAPH_LC_utf8>.

=for apidoc Am|bool|isLOWER|char ch
Returns a boolean indicating whether the specified character is a
lowercase character, analogous to C<m/[[:lower:]]/>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isLOWER_A>, C<isLOWER_L1>, C<isLOWER_uni>, C<isLOWER_utf8>, C<isLOWER_LC>,
C<isLOWER_LC_uvchr>, and C<isLOWER_LC_utf8>.

=for apidoc Am|bool|isOCTAL|char ch
Returns a boolean indicating whether the specified character is an
octal digit, [0-7].
The only two variants are C<isOCTAL_A> and C<isOCTAL_L1>; each is identical to
C<isOCTAL>.

=for apidoc Am|bool|isPUNCT|char ch
Returns a boolean indicating whether the specified character is a
punctuation character, analogous to C<m/[[:punct:]]/>.
Note that the definition of what is punctuation isn't as
straightforward as one might desire.  See L<perlrecharclass/POSIX Character
Classes> for details.
See the L<top of this section|/Character classes> for an explanation of variants
C<isPUNCT_A>, C<isPUNCT_L1>, C<isPUNCT_uni>, C<isPUNCT_utf8>, C<isPUNCT_LC>,
C<isPUNCT_LC_uvchr>, and C<isPUNCT_LC_utf8>.

=for apidoc Am|bool|isSPACE|char ch
Returns a boolean indicating whether the specified character is a
whitespace character.  This is analogous
to what C<m/\s/> matches in a regular expression.  Starting in Perl 5.18
(experimentally), this also matches what C<m/[[:space:]]/> does.
("Experimentally" means that this change may be backed out in 5.22 if
field experience indicates that it was unwise.)  Prior to 5.18, only the
locale forms of this macro (the ones with C<LC> in their names) matched
precisely what C<m/[[:space:]]/> does.  In those releases, the only difference,
in the non-locale variants, was that C<isSPACE()> did not match a vertical tab.
(See L</isPSXSPC> for a macro that matches a vertical tab in all releases.)
See the L<top of this section|/Character classes> for an explanation of variants
C<isSPACE_A>, C<isSPACE_L1>, C<isSPACE_uni>, C<isSPACE_utf8>, C<isSPACE_LC>,
C<isSPACE_LC_uvchr>, and C<isSPACE_LC_utf8>.

=for apidoc Am|bool|isPSXSPC|char ch
(short for Posix Space)
Starting in 5.18, this is identical (experimentally) in all its forms to the
corresponding C<isSPACE()> macros.  ("Experimentally" means that this change
may be backed out in 5.22 if field experience indicates that it
was unwise.)
The locale forms of this macro are identical to their corresponding
C<isSPACE()> forms in all Perl releases.  In releases prior to 5.18, the
non-locale forms differ from their C<isSPACE()> forms only in that the
C<isSPACE()> forms don't match a Vertical Tab, and the C<isPSXSPC()> forms do.
Otherwise they are identical.  Thus this macro is analogous to what
C<m/[[:space:]]/> matches in a regular expression.
See the L<top of this section|/Character classes> for an explanation of variants
C<isPSXSPC_A>, C<isPSXSPC_L1>, C<isPSXSPC_uni>, C<isPSXSPC_utf8>, C<isPSXSPC_LC>,
C<isPSXSPC_LC_uvchr>, and C<isPSXSPC_LC_utf8>.

=for apidoc Am|bool|isUPPER|char ch
Returns a boolean indicating whether the specified character is an
uppercase character, analogous to C<m/[[:upper:]]/>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isUPPER_A>, C<isUPPER_L1>, C<isUPPER_uni>, C<isUPPER_utf8>, C<isUPPER_LC>,
C<isUPPER_LC_uvchr>, and C<isUPPER_LC_utf8>.

=for apidoc Am|bool|isPRINT|char ch
Returns a boolean indicating whether the specified character is a
printable character, analogous to C<m/[[:print:]]/>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isPRINT_A>, C<isPRINT_L1>, C<isPRINT_uni>, C<isPRINT_utf8>, C<isPRINT_LC>,
C<isPRINT_LC_uvchr>, and C<isPRINT_LC_utf8>.

=for apidoc Am|bool|isWORDCHAR|char ch
Returns a boolean indicating whether the specified character is a character
that is a word character, analogous to what C<m/\w/> and C<m/[[:word:]]/> match
in a regular expression.  A word character is an alphabetic character, a
decimal digit, a connecting punctuation character (such as an underscore), or
a "mark" character that attaches to one of those (like some sort of accent).
C<isALNUM()> is a synonym provided for backward compatibility, even though a
word character includes more than the standard C language meaning of
alphanumeric.
See the L<top of this section|/Character classes> for an explanation of variants
C<isWORDCHAR_A>, C<isWORDCHAR_L1>, C<isWORDCHAR_uni>, C<isWORDCHAR_utf8>,
C<isWORDCHAR_LC>, C<isWORDCHAR_LC_uvchr>, and C<isWORDCHAR_LC_utf8>.

=for apidoc Am|bool|isXDIGIT|char ch
Returns a boolean indicating whether the specified character is a hexadecimal
digit.  In the ASCII range these are C<[0-9A-Fa-f]>.  Variants C<isXDIGIT_A()>
and C<isXDIGIT_L1()> are identical to C<isXDIGIT()>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isXDIGIT_uni>, C<isXDIGIT_utf8>, C<isXDIGIT_LC>, C<isXDIGIT_LC_uvchr>, and
C<isXDIGIT_LC_utf8>.

=for apidoc Am|bool|isIDFIRST|char ch
Returns a boolean indicating whether the specified character can be the first
character of an identifier.  This is very close to, but not quite the same as
the official Unicode property C<XID_Start>.  The difference is that this
returns true only if the input character also matches L</isWORDCHAR>.
See the L<top of this section|/Character classes> for an explanation of variants
C<isIDFIRST_A>, C<isIDFIRST_L1>, C<isIDFIRST_uni>, C<isIDFIRST_utf8>,
C<isIDFIRST_LC>, C<isIDFIRST_LC_uvchr>, and C<isIDFIRST_LC_utf8>.

=for apidoc Am|bool|isIDCONT|char ch
Returns a boolean indicating whether the specified character can be the
second or succeeding character of an identifier.  This is very close to, but
not quite the same as the official Unicode property C<XID_Continue>.  The
difference is that this returns true only if the input character also matches
L</isWORDCHAR>.  See the L<top of this section|/Character classes> for an
explanation of variants C<isIDCONT_A>, C<isIDCONT_L1>, C<isIDCONT_uni>,
C<isIDCONT_utf8>, C<isIDCONT_LC>, C<isIDCONT_LC_uvchr>, and
C<isIDCONT_LC_utf8>.

=head1 Miscellaneous Functions

=for apidoc Am|U8|READ_XDIGIT|char str*
Returns the value of an ASCII-range hex digit and advances the string pointer.
Behaviour is only well defined when isXDIGIT(*str) is true.

=head1 Character case changing

=for apidoc Am|U8|toUPPER|U8 ch
Converts the specified character to uppercase.  If the input is anything but an
ASCII lowercase character, that input character itself is returned.  Variant
C<toUPPER_A> is equivalent.

=for apidoc Am|UV|toUPPER_uni|UV cp|U8* s|STRLEN* lenp
Converts the Unicode code point C<cp> to its uppercase version, and
stores that in UTF-8 in C<s>, and its length in bytes in C<lenp>.  Note
that the buffer pointed to by C<s> needs to be at least C<UTF8_MAXBYTES_CASE+1>
bytes since the uppercase version may be longer than the original character.

The first code point of the uppercased version is returned
(but note, as explained just above, that there may be more.)

=for apidoc Am|UV|toUPPER_utf8|U8* p|U8* s|STRLEN* lenp
Converts the UTF-8 encoded character at C<p> to its uppercase version, and
stores that in UTF-8 in C<s>, and its length in bytes in C<lenp>.  Note
that the buffer pointed to by C<s> needs to be at least C<UTF8_MAXBYTES_CASE+1>
bytes since the uppercase version may be longer than the original character.

The first code point of the uppercased version is returned
(but note, as explained just above, that there may be more.)

The input character at C<p> is assumed to be well-formed.

=for apidoc Am|U8|toFOLD|U8 ch
Converts the specified character to foldcase.  If the input is anything but an
ASCII uppercase character, that input character itself is returned.  Variant
C<toFOLD_A> is equivalent.  (There is no equivalent C<to_FOLD_L1> for the full
Latin1 range, as the full generality of L</toFOLD_uni> is needed there.)

=for apidoc Am|UV|toFOLD_uni|UV cp|U8* s|STRLEN* lenp
Converts the Unicode code point C<cp> to its foldcase version, and
stores that in UTF-8 in C<s>, and its length in bytes in C<lenp>.  Note
that the buffer pointed to by C<s> needs to be at least C<UTF8_MAXBYTES_CASE+1>
bytes since the foldcase version may be longer than the original character.

The first code point of the foldcased version is returned
(but note, as explained just above, that there may be more.)

=for apidoc Am|UV|toFOLD_utf8|U8* p|U8* s|STRLEN* lenp
Converts the UTF-8 encoded character at C<p> to its foldcase version, and
stores that in UTF-8 in C<s>, and its length in bytes in C<lenp>.  Note
that the buffer pointed to by C<s> needs to be at least C<UTF8_MAXBYTES_CASE+1>
bytes since the foldcase version may be longer than the original character.

The first code point of the foldcased version is returned
(but note, as explained just above, that there may be more.)

The input character at C<p> is assumed to be well-formed.

=for apidoc Am|U8|toLOWER|U8 ch
Converts the specified character to lowercase.  If the input is anything but an
ASCII uppercase character, that input character itself is returned.  Variant
C<toLOWER_A> is equivalent.

=for apidoc Am|U8|toLOWER_L1|U8 ch
Converts the specified Latin1 character to lowercase.  The results are undefined if
the input doesn't fit in a byte.

=for apidoc Am|U8|toLOWER_LC|U8 ch
Converts the specified character to lowercase using the current locale's rules,
if possible; otherwise returns the input character itself.

=for apidoc Am|UV|toLOWER_uni|UV cp|U8* s|STRLEN* lenp
Converts the Unicode code point C<cp> to its lowercase version, and
stores that in UTF-8 in C<s>, and its length in bytes in C<lenp>.  Note
that the buffer pointed to by C<s> needs to be at least C<UTF8_MAXBYTES_CASE+1>
bytes since the lowercase version may be longer than the original character.

The first code point of the lowercased version is returned
(but note, as explained just above, that there may be more.)

=for apidoc Am|UV|toLOWER_utf8|U8* p|U8* s|STRLEN* lenp
Converts the UTF-8 encoded character at C<p> to its lowercase version, and
stores that in UTF-8 in C<s>, and its length in bytes in C<lenp>.  Note
that the buffer pointed to by C<s> needs to be at least C<UTF8_MAXBYTES_CASE+1>
bytes since the lowercase version may be longer than the original character.

The first code point of the lowercased version is returned
(but note, as explained just above, that there may be more.)

The input character at C<p> is assumed to be well-formed.

=for apidoc Am|U8|toLOWER_LC|U8 ch
Converts the specified character to lowercase using the current locale's rules,
if possible; otherwise returns the input character itself.

=for apidoc Am|U8|toTITLE|U8 ch
Converts the specified character to titlecase.  If the input is anything but an
ASCII lowercase character, that input character itself is returned.  Variant
C<toTITLE_A> is equivalent.  (There is no C<toTITLE_L1> for the full Latin1 range,
as the full generality of L</toTITLE_uni> is needed there.  Titlecase is not a
concept used in locale handling, so there is no functionality for that.)

=for apidoc Am|UV|toTITLE_uni|UV cp|U8* s|STRLEN* lenp
Converts the Unicode code point C<cp> to its titlecase version, and
stores that in UTF-8 in C<s>, and its length in bytes in C<lenp>.  Note
that the buffer pointed to by C<s> needs to be at least C<UTF8_MAXBYTES_CASE+1>
bytes since the titlecase version may be longer than the original character.

The first code point of the titlecased version is returned
(but note, as explained just above, that there may be more.)

=for apidoc Am|UV|toTITLE_utf8|U8* p|U8* s|STRLEN* lenp
Converts the UTF-8 encoded character at C<p> to its titlecase version, and
stores that in UTF-8 in C<s>, and its length in bytes in C<lenp>.  Note
that the buffer pointed to by C<s> needs to be at least C<UTF8_MAXBYTES_CASE+1>
bytes since the titlecase version may be longer than the original character.

The first code point of the titlecased version is returned
(but note, as explained just above, that there may be more.)

The input character at C<p> is assumed to be well-formed.

=cut

XXX Still undocumented isVERTWS_uni and _utf8; it's unclear what their names
really should be.  Also toUPPER_LC and toFOLD_LC, which are subject to change.

Note that these macros are repeated in Devel::PPPort, so should also be
patched there.  The file as of this writing is cpan/Devel-PPPort/parts/inc/misc

*/

/* Specify the widest unsigned type on the platform.  Use U64TYPE because U64
 * is known only in the perl core, and this macro can be called from outside
 * that */
#ifdef HAS_QUAD
#   define WIDEST_UTYPE U64TYPE
#else
#   define WIDEST_UTYPE U32
#endif

/* FITS_IN_8_BITS(c) returns true if c doesn't have  a bit set other than in
 * the lower 8.  It is designed to be hopefully bomb-proof, making sure that no
 * bits of information are lost even on a 64-bit machine, but to get the
 * compiler to optimize it out if possible.  This is because Configure makes
 * sure that the machine has an 8-bit byte, so if c is stored in a byte, the
 * sizeof() guarantees that this evaluates to a constant true at compile time.
 */
#define FITS_IN_8_BITS(c) ((sizeof(c) == 1) || !(((WIDEST_UTYPE)(c)) & ~0xFF))

#ifdef EBCDIC
#   ifndef _ALL_SOURCE
        /* This returns the wrong results on at least z/OS unless this is
         * defined. */
#       error   _ALL_SOURCE should probably be defined
#   endif

    /* We could be called without perl.h, in which case NATIVE_TO_ASCII() is
     * likely not defined, and so we use the native function */
#   define isASCII(c)    cBOOL(isascii(c))
#else
#   define isASCII(c)    ((WIDEST_UTYPE)(c) < 128)
#endif

#define isASCII_A(c)  isASCII(c)
#define isASCII_L1(c)  isASCII(c)

/* The lower 3 bits in both the ASCII and EBCDIC representations of '0' are 0,
 * and the 8 possible permutations of those bits exactly comprise the 8 octal
 * digits */
#define isOCTAL_A(c)  cBOOL(FITS_IN_8_BITS(c) && (0xF8 & (c)) == '0')

/* ASCII range only */
#ifdef H_PERL       /* If have access to perl.h, lookup in its table */

/* Character class numbers.  For internal core Perl use only.  The ones less
 * than 32 are used in PL_charclass[] and the ones up through the one that
 * corresponds to <_HIGHEST_REGCOMP_DOT_H_SYNC> are used by regcomp.h and
 * related files.  PL_charclass ones use names used in l1_char_class_tab.h but
 * their actual definitions are here.  If that file has a name not used here,
 * it won't compile.
 *
 * The first group of these is ordered in what I (khw) estimate to be the
 * frequency of their use.  This gives a slight edge to exiting a loop earlier
 * (in reginclass() in regexec.c) */
#  define _CC_WORDCHAR           0      /* \w and [:word:] */
#  define _CC_DIGIT              1      /* \d and [:digit:] */
#  define _CC_ALPHA              2      /* [:alpha:] */
#  define _CC_LOWER              3      /* [:lower:] */
#  define _CC_UPPER              4      /* [:upper:] */
#  define _CC_PUNCT              5      /* [:punct:] */
#  define _CC_PRINT              6      /* [:print:] */
#  define _CC_ALPHANUMERIC       7      /* [:alnum:] */
#  define _CC_GRAPH              8      /* [:graph:] */
#  define _CC_CASED              9      /* [:lower:] and [:upper:] under /i */

#define _FIRST_NON_SWASH_CC     10
/* The character classes above are implemented with swashes.  The second group
 * (just below) contains the ones implemented without.  These are also sorted
 * in rough order of the frequency of their use, except that \v should be last,
 * as it isn't a real Posix character class, and some (small) inefficiencies in
 * regular expression handling would be introduced by putting it in the middle
 * of those that are.  Also, cntrl and ascii come after the others as it may be
 * useful to group these which have no members that match above Latin1, (or
 * above ASCII in the latter case) */

#  define _CC_SPACE             10      /* \s */
#  define _CC_BLANK             11      /* [:blank:] */
#  define _CC_XDIGIT            12      /* [:xdigit:] */
#  define _CC_PSXSPC            13      /* [:space:] */
#  define _CC_CNTRL             14      /* [:cntrl:] */
#  define _CC_ASCII             15      /* [:ascii:] */
#  define _CC_VERTSPACE         16      /* \v */

#  define _HIGHEST_REGCOMP_DOT_H_SYNC _CC_VERTSPACE

/* The members of the third group below do not need to be coordinated with data
 * structures in regcomp.[ch] and regexec.c.  But they should be added to
 * bootstrap_ctype() */
#  define _CC_IDFIRST           17
#  define _CC_CHARNAME_CONT     18
#  define _CC_NONLATIN1_FOLD    19
#  define _CC_QUOTEMETA         20
#  define _CC_NON_FINAL_FOLD    21
#  define _CC_IS_IN_SOME_FOLD   22
#  define _CC_BACKSLASH_FOO_LBRACE_IS_META 31 /* temp, see mk_PL_charclass.pl */
/* Unused: 23-30
 * If more bits are needed, one could add a second word for non-64bit
 * QUAD_IS_INT systems, using some #ifdefs to distinguish between having a 2nd
 * word or not.  The IS_IN_SOME_FOLD bit is the most easily expendable, as it
 * is used only for optimization (as of this writing), and differs in the
 * Latin1 range from the ALPHA bit only in two relatively unimportant
 * characters: the masculine and feminine ordinal indicators, so removing it
 * would just cause /i regexes which match them to run less efficiently */

#if defined(PERL_CORE) || defined(PERL_EXT)
/* An enum version of the character class numbers, to help compilers
 * optimize */
typedef enum {
    _CC_ENUM_ALPHA          = _CC_ALPHA,
    _CC_ENUM_ALPHANUMERIC   = _CC_ALPHANUMERIC,
    _CC_ENUM_ASCII          = _CC_ASCII,
    _CC_ENUM_BLANK          = _CC_BLANK,
    _CC_ENUM_CASED          = _CC_CASED,
    _CC_ENUM_CNTRL          = _CC_CNTRL,
    _CC_ENUM_DIGIT          = _CC_DIGIT,
    _CC_ENUM_GRAPH          = _CC_GRAPH,
    _CC_ENUM_LOWER          = _CC_LOWER,
    _CC_ENUM_PRINT          = _CC_PRINT,
    _CC_ENUM_PSXSPC         = _CC_PSXSPC,
    _CC_ENUM_PUNCT          = _CC_PUNCT,
    _CC_ENUM_SPACE          = _CC_SPACE,
    _CC_ENUM_UPPER          = _CC_UPPER,
    _CC_ENUM_VERTSPACE      = _CC_VERTSPACE,
    _CC_ENUM_WORDCHAR       = _CC_WORDCHAR,
    _CC_ENUM_XDIGIT         = _CC_XDIGIT
} _char_class_number;
#endif

#define POSIX_SWASH_COUNT _FIRST_NON_SWASH_CC
#define POSIX_CC_COUNT    (_HIGHEST_REGCOMP_DOT_H_SYNC + 1)

#if defined(PERL_IN_UTF8_C) || defined(PERL_IN_REGCOMP_C) || defined(PERL_IN_REGEXEC_C)
#   if _CC_WORDCHAR != 0 || _CC_DIGIT != 1 || _CC_ALPHA != 2 || _CC_LOWER != 3 \
       || _CC_UPPER != 4 || _CC_PUNCT != 5 || _CC_PRINT != 6                   \
       || _CC_ALPHANUMERIC != 7 || _CC_GRAPH != 8 || _CC_CASED != 9
      #error Need to adjust order of swash_property_names[]
#   endif

/* This is declared static in each of the few files that this is #defined for
 * to keep them from being publicly accessible.  Hence there is a small amount
 * of wasted space */

static const char* const swash_property_names[] = {
    "XPosixWord",
    "XPosixDigit",
    "XPosixAlpha",
    "XPosixLower",
    "XPosixUpper",
    "XPosixPunct",
    "XPosixPrint",
    "XPosixAlnum",
    "XPosixGraph",
    "Cased"
};
#endif

#  ifdef DOINIT
EXTCONST  U32 PL_charclass[] = {
#    include "l1_char_class_tab.h"
};

#  else /* ! DOINIT */
EXTCONST U32 PL_charclass[];
#  endif
#endif  /* Has perl.h */

#if defined(H_PERL) && ! defined(BOOTSTRAP_CHARSET)

    /* The 1U keeps Solaris from griping when shifting sets the uppermost bit */
#   define _CC_mask(classnum) (1U << (classnum))

    /* For internal core Perl use only: the base macro for defining macros like
     * isALPHA */
#   define _generic_isCC(c, classnum) cBOOL(FITS_IN_8_BITS(c) \
                && (PL_charclass[(U8) (c)] & _CC_mask(classnum)))

    /* The mask for the _A versions of the macros; it just adds in the bit for
     * ASCII. */
#   define _CC_mask_A(classnum) (_CC_mask(classnum) | _CC_mask(_CC_ASCII))

    /* For internal core Perl use only: the base macro for defining macros like
     * isALPHA_A.  The foo_A version makes sure that both the desired bit and
     * the ASCII bit are present */
#   define _generic_isCC_A(c, classnum) (FITS_IN_8_BITS(c)  \
        && ((PL_charclass[(U8) (c)] & _CC_mask_A(classnum)) \
                                == _CC_mask_A(classnum)))

#   define isALPHA_A(c)  _generic_isCC_A(c, _CC_ALPHA)
#   define isALPHANUMERIC_A(c) _generic_isCC_A(c, _CC_ALPHANUMERIC)
#   define isBLANK_A(c)  _generic_isCC_A(c, _CC_BLANK)
#   define isCNTRL_A(c)  _generic_isCC_A(c, _CC_CNTRL)
#   define isDIGIT_A(c)  _generic_isCC(c, _CC_DIGIT)
#   define isGRAPH_A(c)  _generic_isCC_A(c, _CC_GRAPH)
#   define isLOWER_A(c)  _generic_isCC_A(c, _CC_LOWER)
#   define isPRINT_A(c)  _generic_isCC_A(c, _CC_PRINT)
#   define isPSXSPC_A(c) _generic_isCC_A(c, _CC_PSXSPC)
#   define isPUNCT_A(c)  _generic_isCC_A(c, _CC_PUNCT)
#   define isSPACE_A(c)  _generic_isCC_A(c, _CC_SPACE)
#   define isUPPER_A(c)  _generic_isCC_A(c, _CC_UPPER)
#   define isWORDCHAR_A(c) _generic_isCC_A(c, _CC_WORDCHAR)
#   define isXDIGIT_A(c)  _generic_isCC(c, _CC_XDIGIT)
#   define isIDFIRST_A(c) _generic_isCC_A(c, _CC_IDFIRST)
#   define isALPHA_L1(c)  _generic_isCC(c, _CC_ALPHA)
#   define isALPHANUMERIC_L1(c) _generic_isCC(c, _CC_ALPHANUMERIC)
#   define isBLANK_L1(c)  _generic_isCC(c, _CC_BLANK)

    /* continuation character for legal NAME in \N{NAME} */
#   define isCHARNAME_CONT(c) _generic_isCC(c, _CC_CHARNAME_CONT)

#   define isCNTRL_L1(c)  _generic_isCC(c, _CC_CNTRL)
#   define isGRAPH_L1(c)  _generic_isCC(c, _CC_GRAPH)
#   define isLOWER_L1(c)  _generic_isCC(c, _CC_LOWER)
#   define isPRINT_L1(c)  _generic_isCC(c, _CC_PRINT)
#   define isPSXSPC_L1(c) _generic_isCC(c, _CC_PSXSPC)
#   define isPUNCT_L1(c)  _generic_isCC(c, _CC_PUNCT)
#   define isSPACE_L1(c)  _generic_isCC(c, _CC_SPACE)
#   define isUPPER_L1(c)  _generic_isCC(c, _CC_UPPER)
#   define isWORDCHAR_L1(c) _generic_isCC(c, _CC_WORDCHAR)
#   define isIDFIRST_L1(c) _generic_isCC(c, _CC_IDFIRST)

    /* Either participates in a fold with a character above 255, or is a
     * multi-char fold */
#   define _HAS_NONLATIN1_FOLD_CLOSURE_ONLY_FOR_USE_BY_REGCOMP_DOT_C_AND_REGEXEC_DOT_C(c) ((! cBOOL(FITS_IN_8_BITS(c))) || (PL_charclass[(U8) (c)] & _CC_mask(_CC_NONLATIN1_FOLD)))

#   define _isQUOTEMETA(c) _generic_isCC(c, _CC_QUOTEMETA)
#   define _IS_NON_FINAL_FOLD_ONLY_FOR_USE_BY_REGCOMP_DOT_C(c) \
                                            _generic_isCC(c, _CC_NON_FINAL_FOLD)
#   define _IS_IN_SOME_FOLD_ONLY_FOR_USE_BY_REGCOMP_DOT_C(c) \
                                            _generic_isCC(c, _CC_IS_IN_SOME_FOLD)
#else   /* Either don't have perl.h or don't want to use char_class_tab.h */

    /* If we don't have perl.h, we are compiling a utility program.  Below we
     * hard-code various macro definitions that wouldn't otherwise be available
     * to it.  We can also get here if we are configured to bootstrap up Perl
     * on a non-ASCII platform that doesn't have a working Perl (currently only
     * EBCDIC).  For these we currently use the native definitions to get
     * things going.  (It should also be possible to use the translation
     * function NATIVE_TO_LATIN1(), but that is an extra layer of dependence on
     * Perl, so it is currently avoided for the macros where it's possible to
     * do so.) */
#   ifdef EBCDIC
        /* Use the native functions.  They likely will return false for all
         * non-ASCII values, but this makes sure */
#       define isALPHA_A(c)    (isASCII(c) && isalpha(c))
#       define isALPHANUMERIC_A(c) (isASCII(c) && isalnum(c))
#       define isCNTRL_A(c)    (isASCII(c) && iscntrl(c))
#       define isDIGIT_A(c)    (isASCII(c) && isdigit(c))
#       define isGRAPH_A(c)    (isASCII(c) && isgraph(c))
#       define isLOWER_A(c)    (isASCII(c) && islower(c))
#       define isPRINT_A(c)    (isASCII(c) && isprint(c))
#       define isPUNCT_A(c)    (isASCII(c) && ispunct(c))
#       define isSPACE_A(c)    (isASCII(c) && isspace(c))
#       define isUPPER_A(c)    (isASCII(c) && isupper(c))
#       define isXDIGIT_A(c)   (isASCII(c) && isxdigit(c))
#   else   /* ASCII platform.  These are coded based on first principals */
#       define isALPHA_A(c)  (isUPPER_A(c) || isLOWER_A(c))
#       define isALPHANUMERIC_A(c) (isALPHA_A(c) || isDIGIT_A(c))
#       define isCNTRL_A(c)  (isASCII(c) && (! isPRINT_A(c)))
#       define isDIGIT_A(c)  ((c) <= '9' && (c) >= '0')
#       define isGRAPH_A(c)  (isPRINT_A(c) && (c) != ' ')
#       define isLOWER_A(c)  ((c) >= 'a' && (c) <= 'z')
#       define isPRINT_A(c)  (((c) >= 32 && (c) < 127))
#       define isPUNCT_A(c)  (isGRAPH_A(c) && (! isALPHANUMERIC_A(c)))
#       define isSPACE_A(c)  ((c) == ' '                                     \
                              || (c) == '\t'                                 \
                              || (c) == '\n'                                 \
                              || (c) == '\r'                                 \
                              || (c) =='\v'                                  \
                              || (c) == '\f')
#       define isUPPER_A(c)  ((c) <= 'Z' && (c) >= 'A')
#       define isXDIGIT_A(c) (isDIGIT_A(c)                                   \
                              || ((c) >= 'a' && (c) <= 'f')                  \
                              || ((c) <= 'F' && (c) >= 'A'))
#   endif   /* Below are common definitions for ASCII and non-ASCII */
#   define isBLANK_A(c)      ((c) == ' ' || (c) == '\t')
#   define isIDFIRST_A(c)    (isALPHA_A(c) || (c) == '_')
#   define isWORDCHAR_A(c)   (isALPHANUMERIC_A(c) || (c) == '_')

    /* The _L1 macros may be unnecessary for both the utilities and for
     * bootstrapping; I (khw) added them during debugging of bootstrapping, and
     * it seems best to keep them. */
#   define isPSXSPC_A(c)     isSPACE_A(c) /* XXX Assumes SPACE matches '\v' */
#   define isALPHA_L1(c)     (isUPPER_L1(c) || isLOWER_L1(c))
#   define isALPHANUMERIC_L1(c) (isALPHA_L1(c) || isDIGIT_A(c))
#   define isBLANK_L1(c)     (isBLANK_A(c)                                   \
                              || (FITS_IN_8_BITS(c)                          \
                                  && NATIVE_TO_LATIN1((U8) c) == 0xA0))
#   define isCNTRL_L1(c)     (FITS_IN_8_BITS(c) && (! isPRINT_L1(c)))
#   define isGRAPH_L1(c)     (isPRINT_L1(c) && (! isBLANK_L1(c)))
#   define isLOWER_L1(c)     (isLOWER_A(c)                                   \
                              || (FITS_IN_8_BITS(c)                          \
                                  && ((NATIVE_TO_LATIN1((U8) c) >= 0xDF      \
                                       && NATIVE_TO_LATIN1((U8) c) != 0xF7)  \
                                       || NATIVE_TO_LATIN1((U8) c) == 0xAA   \
                                       || NATIVE_TO_LATIN1((U8) c) == 0xBA   \
                                       || NATIVE_TO_LATIN1((U8) c) == 0xB5)))
#   define isPRINT_L1(c)     (isPRINT_A(c)                                   \
                              || (FITS_IN_8_BITS(c)                          \
                                  && NATIVE_TO_LATIN1((U8) c) >= 0xA0))
#   define isPSXSPC_L1(c)    isSPACE_L1(c)
#   define isPUNCT_L1(c)     (isPUNCT_A(c)                                   \
                              || (FITS_IN_8_BITS(c)                          \
                                  && (NATIVE_TO_LATIN1((U8) c) == 0xA1       \
                                      || NATIVE_TO_LATIN1((U8) c) == 0xA7    \
                                      || NATIVE_TO_LATIN1((U8) c) == 0xAB    \
                                      || NATIVE_TO_LATIN1((U8) c) == 0xB6    \
                                      || NATIVE_TO_LATIN1((U8) c) == 0xB7    \
                                      || NATIVE_TO_LATIN1((U8) c) == 0xBB    \
                                      || NATIVE_TO_LATIN1((U8) c) == 0xBF)))
#   define isSPACE_L1(c)     (isSPACE_A(c)                                   \
                              || (FITS_IN_8_BITS(c)                          \
                                  && (NATIVE_TO_LATIN1((U8) c) == 0x85       \
                                      || NATIVE_TO_LATIN1((U8) c) == 0xA0)))
#   define isUPPER_L1(c)     (isUPPER_A(c)                                   \
                              || (FITS_IN_8_BITS(c)                          \
                                  && (NATIVE_TO_LATIN1((U8) c) >= 0xC0       \
                                      && NATIVE_TO_LATIN1((U8) c) <= 0xDE    \
                                      && NATIVE_TO_LATIN1((U8) c) != 0xD7)))
#   define isWORDCHAR_L1(c)  (isIDFIRST_L1(c) || isDIGIT_A(c))
#   define isIDFIRST_L1(c)   (isALPHA_L1(c) || NATIVE_TO_LATIN1(c) == '_')
#   define isCHARNAME_CONT(c) (isWORDCHAR_L1(c)                              \
                               || isBLANK_L1(c)                              \
                               || (c) == '-'                                 \
                               || (c) == '('                                 \
                               || (c) == ')')
    /* The following are not fully accurate in the above-ASCII range.  I (khw)
     * don't think it's necessary to be so for the purposes where this gets
     * compiled */
#   define _isQUOTEMETA(c)      (FITS_IN_8_BITS(c) && ! isWORDCHAR_L1(c))
#   define _IS_IN_SOME_FOLD_ONLY_FOR_USE_BY_REGCOMP_DOT_C(c) isALPHA_L1(c)

    /*  And these aren't accurate at all.  They are useful only for above
     *  Latin1, which utilities and bootstrapping don't deal with */
#   define _IS_NON_FINAL_FOLD_ONLY_FOR_USE_BY_REGCOMP_DOT_C(c) 0
#   define _HAS_NONLATIN1_FOLD_CLOSURE_ONLY_FOR_USE_BY_REGCOMP_DOT_C_AND_REGEXEC_DOT_C(c) 0

    /* Many of the macros later in this file are defined in terms of these.  By
     * implementing them with a function, which converts the class number into
     * a call to the desired macro, all of the later ones work.  However, that
     * function won't be actually defined when building a utility program (no
     * perl.h), and so a compiler error will be generated if one is attempted
     * to be used.  And the above-Latin1 code points require Unicode tables to
     * be present, something unlikely to be the case when bootstrapping */
#   define _generic_isCC(c, classnum)                                        \
         (FITS_IN_8_BITS(c) && S_bootstrap_ctype((U8) (c), (classnum), TRUE))
#   define _generic_isCC_A(c, classnum)                                      \
         (FITS_IN_8_BITS(c) && S_bootstrap_ctype((U8) (c), (classnum), FALSE))
#endif  /* End of no perl.h or have BOOTSTRAP_CHARSET */

#define isALPHANUMERIC(c)  isALPHANUMERIC_A(c)
#define isALPHA(c)   isALPHA_A(c)
#define isBLANK(c)   isBLANK_A(c)
#define isCNTRL(c)   isCNTRL_A(c)
#define isDIGIT(c)   isDIGIT_A(c)
#define isGRAPH(c)   isGRAPH_A(c)
#define isIDFIRST(c) isIDFIRST_A(c)
#define isLOWER(c)   isLOWER_A(c)
#define isPRINT(c)   isPRINT_A(c)
#define isPSXSPC(c)  isPSXSPC_A(c)
#define isPUNCT(c)   isPUNCT_A(c)
#define isSPACE(c)   isSPACE_A(c)
#define isUPPER(c)   isUPPER_A(c)
#define isWORDCHAR(c) isWORDCHAR_A(c)
#define isXDIGIT(c)  isXDIGIT_A(c)

/* ASCII casing.  These could also be written as
    #define toLOWER(c) (isASCII(c) ? toLOWER_LATIN1(c) : (c))
    #define toUPPER(c) (isASCII(c) ? toUPPER_LATIN1_MOD(c) : (c))
   which uses table lookup and mask instead of subtraction.  (This would
   work because the _MOD does not apply in the ASCII range) */
#define toLOWER(c)  (isUPPER(c) ? (U8)((c) + ('a' - 'A')) : (c))
#define toUPPER(c)  (isLOWER(c) ? (U8)((c) - ('a' - 'A')) : (c))

/* In the ASCII range, these are equivalent to what they're here defined to be.
 * But by creating these definitions, other code doesn't have to be aware of
 * this detail */
#define toFOLD(c)    toLOWER(c)
#define toTITLE(c)   toUPPER(c)

#define toLOWER_A(c) toLOWER(c)
#define toUPPER_A(c) toUPPER(c)
#define toFOLD_A(c)  toFOLD(c)
#define toTITLE_A(c) toTITLE(c)

/* Use table lookup for speed; returns the input itself if is out-of-range */
#define toLOWER_LATIN1(c)    ((! FITS_IN_8_BITS(c))                        \
                             ? (c)                                         \
                             : PL_latin1_lc[ (U8) (c) ])
#define toLOWER_L1(c)    toLOWER_LATIN1(c)  /* Synonym for consistency */

/* Modified uc.  Is correct uc except for three non-ascii chars which are
 * all mapped to one of them, and these need special handling; returns the
 * input itself if is out-of-range */
#define toUPPER_LATIN1_MOD(c) ((! FITS_IN_8_BITS(c))                       \
                               ? (c)                                       \
                               : PL_mod_latin1_uc[ (U8) (c) ])
#define IN_UTF8_CTYPE_LOCALE PL_in_utf8_CTYPE_locale

/* Use foo_LC_uvchr() instead  of these for beyond the Latin1 range */

/* For internal core Perl use only: the base macro for defining macros like
 * isALPHA_LC, which uses the current LC_CTYPE locale.  'c' is the code point
 * (0-255) to check.  In a UTF-8 locale, the result is the same as calling
 * isFOO_L1(); the 'utf8_locale_classnum' parameter is something like
 * _CC_UPPER, which gives the class number for doing this.  For non-UTF-8
 * locales, the code to actually do the test this is passed in 'non_utf8'.  If
 * 'c' is above 255, 0 is returned.  For accessing the full range of possible
 * code points under locale rules, use the macros based on _generic_LC_uvchr
 * instead of this. */
#define _generic_LC_base(c, utf8_locale_classnum, non_utf8)                    \
           (! FITS_IN_8_BITS(c)                                                \
           ? 0                                                                 \
           : IN_UTF8_CTYPE_LOCALE                                              \
             ? cBOOL(PL_charclass[(U8) (c)] & _CC_mask(utf8_locale_classnum))  \
             : cBOOL(non_utf8))

/* For internal core Perl use only: a helper macro for defining macros like
 * isALPHA_LC.  'c' is the code point (0-255) to check.  The function name to
 * actually do this test is passed in 'non_utf8_func', which is called on 'c',
 * casting 'c' to the macro _LC_CAST, which should not be parenthesized.  See
 * _generic_LC_base for more info */
#define _generic_LC(c, utf8_locale_classnum, non_utf8_func)                    \
                        _generic_LC_base(c,utf8_locale_classnum,               \
                                         non_utf8_func( (_LC_CAST) (c)))

/* For internal core Perl use only: like _generic_LC, but also returns TRUE if
 * 'c' is the platform's native underscore character */
#define _generic_LC_underscore(c,utf8_locale_classnum,non_utf8_func)           \
                        _generic_LC_base(c, utf8_locale_classnum,              \
                                         (non_utf8_func( (_LC_CAST) (c))       \
                                          || (char)(c) == '_'))

/* These next three are also for internal core Perl use only: case-change
 * helper macros */
#define _generic_toLOWER_LC(c, function, cast)  (! FITS_IN_8_BITS(c)           \
                                                ? (c)                          \
                                                : (IN_UTF8_CTYPE_LOCALE)       \
                                                  ? PL_latin1_lc[ (U8) (c) ]   \
                                                : function((cast)(c)))

/* Note that the result can be larger than a byte in a UTF-8 locale.  It
 * returns a single value, so can't adequately return the upper case of LATIN
 * SMALL LETTER SHARP S in a UTF-8 locale (which should be a string of two
 * values "SS");  instead it asserts against that under DEBUGGING, and
 * otherwise returns its input */
#define _generic_toUPPER_LC(c, function, cast)                                 \
                    (! FITS_IN_8_BITS(c)                                       \
                    ? (c)                                                      \
                    : ((! IN_UTF8_CTYPE_LOCALE)                                \
                      ? function((cast)(c))                                    \
                      : ((((U8)(c)) == MICRO_SIGN)                             \
                        ? GREEK_CAPITAL_LETTER_MU                              \
                        : ((((U8)(c)) == LATIN_SMALL_LETTER_Y_WITH_DIAERESIS)  \
                          ? LATIN_CAPITAL_LETTER_Y_WITH_DIAERESIS              \
                          : ((((U8)(c)) == LATIN_SMALL_LETTER_SHARP_S)         \
                            ? (__ASSERT_(0) (c))                               \
                            : PL_mod_latin1_uc[ (U8) (c) ])))))

/* Note that the result can be larger than a byte in a UTF-8 locale.  It
 * returns a single value, so can't adequately return the fold case of LATIN
 * SMALL LETTER SHARP S in a UTF-8 locale (which should be a string of two
 * values "ss"); instead it asserts against that under DEBUGGING, and
 * otherwise returns its input */
#define _generic_toFOLD_LC(c, function, cast)                                  \
                    ((UNLIKELY((c) == MICRO_SIGN) && IN_UTF8_CTYPE_LOCALE)     \
                      ? GREEK_SMALL_LETTER_MU                                  \
                      : (__ASSERT_(! IN_UTF8_CTYPE_LOCALE                      \
                                   || (c) != LATIN_SMALL_LETTER_SHARP_S)       \
                         _generic_toLOWER_LC(c, function, cast)))

/* Use the libc versions for these if available. */
#if defined(HAS_ISASCII) && ! defined(USE_NEXT_CTYPE)
#   define isASCII_LC(c) (FITS_IN_8_BITS(c) && isascii( (U8) (c)))
#else
#   define isASCII_LC(c) isASCII(c)
#endif

#if defined(HAS_ISBLANK) && ! defined(USE_NEXT_CTYPE)
#   define isBLANK_LC(c) _generic_LC(c, _CC_BLANK, isblank)
#else /* Unlike isASCII, varies if in a UTF-8 locale */
#   define isBLANK_LC(c) (IN_UTF8_CTYPE_LOCALE) ? isBLANK_L1(c) : isBLANK(c)
#endif

#ifdef USE_NEXT_CTYPE   /* NeXT computers */

#    define _LC_CAST unsigned int   /* Needed by _generic_LC.  NeXT functions
                                       use this as their input type */

#    define isALPHA_LC(c)   _generic_LC(c, _CC_ALPHA, NXIsAlpha)
#    define isALPHANUMERIC_LC(c)  _generic_LC(c, _CC_ALPHANUMERIC, NXIsAlNum)
#    define isCNTRL_LC(c)    _generic_LC(c, _CC_CNTRL, NXIsCntrl)
#    define isDIGIT_LC(c)    _generic_LC(c, _CC_DIGIT, NXIsDigit)
#    define isGRAPH_LC(c)    _generic_LC(c, _CC_GRAPH, NXIsGraph)
#    define isIDFIRST_LC(c)  _generic_LC_underscore(c, _CC_IDFIRST, NXIsAlpha)
#    define isLOWER_LC(c)    _generic_LC(c, _CC_LOWER, NXIsLower)
#    define isPRINT_LC(c)    _generic_LC(c, _CC_PRINT, NXIsPrint)
#    define isPUNCT_LC(c)    _generic_LC(c, _CC_PUNCT, NXIsPunct)
#    define isSPACE_LC(c)    _generic_LC(c, _CC_SPACE, NXIsSpace)
#    define isUPPER_LC(c)    _generic_LC(c, _CC_UPPER, NXIsUpper)
#    define isWORDCHAR_LC(c) _generic_LC_underscore(c, _CC_WORDCHAR, NXIsAlNum)
#    define isXDIGIT_LC(c)   _generic_LC(c, _CC_XDIGIT, NXIsXdigit)

#    define toLOWER_LC(c) _generic_toLOWER_LC((c), NXToLower, unsigned int)
#    define toUPPER_LC(c) _generic_toUPPER_LC((c), NXToUpper, unsigned int)
#    define toFOLD_LC(c)  _generic_toFOLD_LC((c), NXToLower, unsigned int)

#else /* !USE_NEXT_CTYPE */

#  define _LC_CAST U8

#  if defined(CTYPE256) || (!defined(isascii) && !defined(HAS_ISASCII))
    /* For most other platforms */

#    define isALPHA_LC(c)   _generic_LC(c, _CC_ALPHA, isalpha)
#    define isALPHANUMERIC_LC(c)  _generic_LC(c, _CC_ALPHANUMERIC, isalnum)
#    define isCNTRL_LC(c)    _generic_LC(c, _CC_CNTRL, iscntrl)
#    define isDIGIT_LC(c)    _generic_LC(c, _CC_DIGIT, isdigit)
#    define isGRAPH_LC(c)    _generic_LC(c, _CC_GRAPH, isgraph)
#    define isIDFIRST_LC(c)  _generic_LC_underscore(c, _CC_IDFIRST, isalpha)
#    define isLOWER_LC(c)    _generic_LC(c, _CC_LOWER, islower)
#    define isPRINT_LC(c)    _generic_LC(c, _CC_PRINT, isprint)
#    define isPUNCT_LC(c)    _generic_LC(c, _CC_PUNCT, ispunct)
#    define isSPACE_LC(c)    _generic_LC(c, _CC_SPACE, isspace)
#    define isUPPER_LC(c)    _generic_LC(c, _CC_UPPER, isupper)
#    define isWORDCHAR_LC(c) _generic_LC_underscore(c, _CC_WORDCHAR, isalnum)
#    define isXDIGIT_LC(c)   _generic_LC(c, _CC_XDIGIT, isxdigit)


#    define toLOWER_LC(c) _generic_toLOWER_LC((c), tolower, U8)
#    define toUPPER_LC(c) _generic_toUPPER_LC((c), toupper, U8)
#    define toFOLD_LC(c)  _generic_toFOLD_LC((c), tolower, U8)

#  else  /* The final fallback position */

#    define isALPHA_LC(c)	(isascii(c) && isalpha(c))
#    define isALPHANUMERIC_LC(c) (isascii(c) && isalnum(c))
#    define isCNTRL_LC(c)	(isascii(c) && iscntrl(c))
#    define isDIGIT_LC(c)	(isascii(c) && isdigit(c))
#    define isGRAPH_LC(c)	(isascii(c) && isgraph(c))
#    define isIDFIRST_LC(c)	(isascii(c) && (isalpha(c) || (c) == '_'))
#    define isLOWER_LC(c)	(isascii(c) && islower(c))
#    define isPRINT_LC(c)	(isascii(c) && isprint(c))
#    define isPUNCT_LC(c)	(isascii(c) && ispunct(c))
#    define isSPACE_LC(c)	(isascii(c) && isspace(c))
#    define isUPPER_LC(c)	(isascii(c) && isupper(c))
#    define isWORDCHAR_LC(c)	(isascii(c) && (isalnum(c) || (c) == '_'))
#    define isXDIGIT_LC(c)      (isascii(c) && isxdigit(c))

#    define toLOWER_LC(c)	(isascii(c) ? tolower(c) : (c))
#    define toUPPER_LC(c)	(isascii(c) ? toupper(c) : (c))
#    define toFOLD_LC(c)	(isascii(c) ? tolower(c) : (c))

#  endif
#endif /* USE_NEXT_CTYPE */

#define isIDCONT(c)             isWORDCHAR(c)
#define isIDCONT_A(c)           isWORDCHAR_A(c)
#define isIDCONT_L1(c)	        isWORDCHAR_L1(c)
#define isIDCONT_LC(c)	        isWORDCHAR_LC(c)
#define isPSXSPC_LC(c)		isSPACE_LC(c)

/* For internal core Perl use only: the base macros for defining macros like
 * isALPHA_uni.  'c' is the code point to check.  'classnum' is the POSIX class
 * number defined earlier in this file.  _generic_uni() is used for POSIX
 * classes where there is a macro or function 'above_latin1' that takes the
 * single argument 'c' and returns the desired value.  These exist for those
 * classes which have simple definitions, avoiding the overhead of a hash
 * lookup or inversion list binary search.  _generic_swash_uni() can be used
 * for classes where that overhead is faster than a direct lookup.
 * _generic_uni() won't compile if 'c' isn't unsigned, as it won't match the
 * 'above_latin1' prototype. _generic_isCC() macro does bounds checking, so
 * have duplicate checks here, so could create versions of the macros that
 * don't, but experiments show that gcc optimizes them out anyway. */

/* Note that all ignore 'use bytes' */
#define _generic_uni(classnum, above_latin1, c) ((c) < 256                    \
                                             ? _generic_isCC(c, classnum)     \
                                             : above_latin1(c))
#define _generic_swash_uni(classnum, c) ((c) < 256                            \
                                             ? _generic_isCC(c, classnum)     \
                                             : _is_uni_FOO(classnum, c))
#define isALPHA_uni(c)      _generic_swash_uni(_CC_ALPHA, c)
#define isALPHANUMERIC_uni(c) _generic_swash_uni(_CC_ALPHANUMERIC, c)
#define isASCII_uni(c)      isASCII(c)
#define isBLANK_uni(c)      _generic_uni(_CC_BLANK, is_HORIZWS_cp_high, c)
#define isCNTRL_uni(c)      isCNTRL_L1(c) /* All controls are in Latin1 */
#define isDIGIT_uni(c)      _generic_swash_uni(_CC_DIGIT, c)
#define isGRAPH_uni(c)      _generic_swash_uni(_CC_GRAPH, c)
#define isIDCONT_uni(c)     _generic_uni(_CC_WORDCHAR, _is_uni_perl_idcont, c)
#define isIDFIRST_uni(c)    _generic_uni(_CC_IDFIRST, _is_uni_perl_idstart, c)
#define isLOWER_uni(c)      _generic_swash_uni(_CC_LOWER, c)
#define isPRINT_uni(c)      _generic_swash_uni(_CC_PRINT, c)

/* Posix and regular space are identical above Latin1 */
#define isPSXSPC_uni(c)     _generic_uni(_CC_PSXSPC, is_XPERLSPACE_cp_high, c)

#define isPUNCT_uni(c)      _generic_swash_uni(_CC_PUNCT, c)
#define isSPACE_uni(c)      _generic_uni(_CC_SPACE, is_XPERLSPACE_cp_high, c)
#define isUPPER_uni(c)      _generic_swash_uni(_CC_UPPER, c)
#define isVERTWS_uni(c)     _generic_uni(_CC_VERTSPACE, is_VERTWS_cp_high, c)
#define isWORDCHAR_uni(c)   _generic_swash_uni(_CC_WORDCHAR, c)
#define isXDIGIT_uni(c)     _generic_uni(_CC_XDIGIT, is_XDIGIT_cp_high, c)

#define toFOLD_uni(c,s,l)	to_uni_fold(c,s,l)
#define toLOWER_uni(c,s,l)	to_uni_lower(c,s,l)
#define toTITLE_uni(c,s,l)	to_uni_title(c,s,l)
#define toUPPER_uni(c,s,l)	to_uni_upper(c,s,l)

/* For internal core Perl use only: the base macros for defining macros like
 * isALPHA_LC_uvchr.  These are like isALPHA_LC, but the input can be any code
 * point, not just 0-255.  Like _generic_uni, there are two versions, one for
 * simple class definitions; the other for more complex.  These are like
 * _generic_uni, so see it for more info. */
#define _generic_LC_uvchr(latin1, above_latin1, c)                            \
                                    (c < 256 ? latin1(c) : above_latin1(c))
#define _generic_LC_swash_uvchr(latin1, classnum, c)                          \
                            (c < 256 ? latin1(c) : _is_uni_FOO(classnum, c))

#define isALPHA_LC_uvchr(c)  _generic_LC_swash_uvchr(isALPHA_LC, _CC_ALPHA, c)
#define isALPHANUMERIC_LC_uvchr(c)  _generic_LC_swash_uvchr(isALPHANUMERIC_LC, \
                                                         _CC_ALPHANUMERIC, c)
#define isASCII_LC_uvchr(c)  isASCII_LC(c)
#define isBLANK_LC_uvchr(c)  _generic_LC_uvchr(isBLANK_LC, is_HORIZWS_cp_high, c)
#define isCNTRL_LC_uvchr(c)  (c < 256 ? isCNTRL_LC(c) : 0)
#define isDIGIT_LC_uvchr(c)  _generic_LC_swash_uvchr(isDIGIT_LC, _CC_DIGIT, c)
#define isGRAPH_LC_uvchr(c)  _generic_LC_swash_uvchr(isGRAPH_LC, _CC_GRAPH, c)
#define isIDCONT_LC_uvchr(c)  _generic_LC_uvchr(isIDCONT_LC,                  \
                                                  _is_uni_perl_idcont, c)
#define isIDFIRST_LC_uvchr(c)  _generic_LC_uvchr(isIDFIRST_LC,                 \
                                                  _is_uni_perl_idstart, c)
#define isLOWER_LC_uvchr(c)  _generic_LC_swash_uvchr(isLOWER_LC, _CC_LOWER, c)
#define isPRINT_LC_uvchr(c)  _generic_LC_swash_uvchr(isPRINT_LC, _CC_PRINT, c)
#define isPSXSPC_LC_uvchr(c) isSPACE_LC_uvchr(c) /* space is identical to posix
                                                    space under locale */
#define isPUNCT_LC_uvchr(c)  _generic_LC_swash_uvchr(isPUNCT_LC, _CC_PUNCT, c)
#define isSPACE_LC_uvchr(c)  _generic_LC_uvchr(isSPACE_LC,                     \
                                                    is_XPERLSPACE_cp_high, c)
#define isUPPER_LC_uvchr(c)  _generic_LC_swash_uvchr(isUPPER_LC, _CC_UPPER, c)
#define isWORDCHAR_LC_uvchr(c)  _generic_LC_swash_uvchr(isWORDCHAR_LC,              \
                                                           _CC_WORDCHAR, c)
#define isXDIGIT_LC_uvchr(c) _generic_LC_uvchr(isXDIGIT_LC, is_XDIGIT_cp_high, c)

#define isBLANK_LC_uni(c)	isBLANK_LC_uvchr(UNI_TO_NATIVE(c))

/* For internal core Perl use only: the base macros for defining macros like
 * isALPHA_utf8.  These are like the earlier defined macros, but take an input
 * UTF-8 encoded string 'p'. If the input is in the Latin1 range, use
 * the Latin1 macro 'classnum' on 'p'.  Otherwise use the value given by the
 * 'utf8' parameter.  This relies on the fact that ASCII characters have the
 * same representation whether utf8 or not.  Note that it assumes that the utf8
 * has been validated, and ignores 'use bytes' */
#define _generic_utf8(classnum, p, utf8) (UTF8_IS_INVARIANT(*(p))              \
                                         ? _generic_isCC(*(p), classnum)       \
                                         : (UTF8_IS_DOWNGRADEABLE_START(*(p))) \
                                           ? _generic_isCC(                    \
                                                TWO_BYTE_UTF8_TO_NATIVE(*(p),  \
                                                                   *((p)+1 )), \
                                                classnum)                      \
                                           : utf8)
/* Like the above, but calls 'above_latin1(p)' to get the utf8 value.  'above_latin1'
 * can be a macro */
#define _generic_func_utf8(classnum, above_latin1, p)  \
                                    _generic_utf8(classnum, p, above_latin1(p))
/* Like the above, but passes classnum to _isFOO_utf8(), instead of having an
 * 'above_latin1' parameter */
#define _generic_swash_utf8(classnum, p)  \
                      _generic_utf8(classnum, p, _is_utf8_FOO(classnum, p))

/* Like the above, but should be used only when it is known that there are no
 * characters in the range 128-255 which the class is TRUE for.  Hence it can
 * skip the tests for this range.  'above_latin1' should include its arguments */
#define _generic_utf8_no_upper_latin1(classnum, p, above_latin1)               \
                                         (UTF8_IS_INVARIANT(*(p))              \
                                         ? _generic_isCC(*(p), classnum)       \
                                         : (UTF8_IS_ABOVE_LATIN1(*(p)))        \
                                           ? above_latin1                      \
                                           : 0)

/* NOTE that some of these macros have very similar ones in regcharclass.h.
 * For example, there is (at the time of this writing) an 'is_SPACE_utf8()'
 * there, differing in name only by an underscore from the one here
 * 'isSPACE_utf8().  The difference is that the ones here are probably more
 * efficient and smaller, using an O(1) array lookup for Latin1-range code
 * points; the regcharclass.h ones are implemented as a series of
 * "if-else-if-else ..." */

#define isALPHA_utf8(p)         _generic_swash_utf8(_CC_ALPHA, p)
#define isALPHANUMERIC_utf8(p)  _generic_swash_utf8(_CC_ALPHANUMERIC, p)
#define isASCII_utf8(p)         isASCII(*p) /* Because ASCII is invariant under
                                               utf8, the non-utf8 macro works
                                             */
#define isBLANK_utf8(p)         _generic_func_utf8(_CC_BLANK, is_HORIZWS_high, p)
#define isCNTRL_utf8(p)         _generic_utf8(_CC_CNTRL, p, 0)
#define isDIGIT_utf8(p)         _generic_utf8_no_upper_latin1(_CC_DIGIT, p,   \
                                                  _is_utf8_FOO(_CC_DIGIT, p))
#define isGRAPH_utf8(p)         _generic_swash_utf8(_CC_GRAPH, p)
#define isIDCONT_utf8(p)        _generic_func_utf8(_CC_WORDCHAR,              \
                                                  _is_utf8_perl_idcont, p)

/* To prevent S_scan_word in toke.c from hanging, we have to make sure that
 * IDFIRST is an alnum.  See
 * http://rt.perl.org/rt3/Ticket/Display.html?id=74022 for more detail than you
 * ever wanted to know about.  (In the ASCII range, there isn't a difference.)
 * This used to be not the XID version, but we decided to go with the more
 * modern Unicode definition */
#define isIDFIRST_utf8(p)       _generic_func_utf8(_CC_IDFIRST,               \
                                                _is_utf8_perl_idstart, p)

#define isLOWER_utf8(p)         _generic_swash_utf8(_CC_LOWER, p)
#define isPRINT_utf8(p)         _generic_swash_utf8(_CC_PRINT, p)

/* Posix and regular space are identical above Latin1 */
#define isPSXSPC_utf8(p)        _generic_func_utf8(_CC_PSXSPC, is_XPERLSPACE_high, p)

#define isPUNCT_utf8(p)         _generic_swash_utf8(_CC_PUNCT, p)
#define isSPACE_utf8(p)         _generic_func_utf8(_CC_SPACE, is_XPERLSPACE_high, p)
#define isUPPER_utf8(p)         _generic_swash_utf8(_CC_UPPER, p)
#define isVERTWS_utf8(p)        _generic_func_utf8(_CC_VERTSPACE, is_VERTWS_high, p)
#define isWORDCHAR_utf8(p)      _generic_swash_utf8(_CC_WORDCHAR, p)
#define isXDIGIT_utf8(p)        _generic_utf8_no_upper_latin1(_CC_XDIGIT, p,   \
                                                          is_XDIGIT_high(p))

#define toFOLD_utf8(p,s,l)	to_utf8_fold(p,s,l)
#define toLOWER_utf8(p,s,l)	to_utf8_lower(p,s,l)
#define toTITLE_utf8(p,s,l)	to_utf8_title(p,s,l)
#define toUPPER_utf8(p,s,l)	to_utf8_upper(p,s,l)

/* For internal core Perl use only: the base macros for defining macros like
 * isALPHA_LC_utf8.  These are like _generic_utf8, but if the first code point
 * in 'p' is within the 0-255 range, it uses locale rules from the passed-in
 * 'macro' parameter */
#define _generic_LC_utf8(macro, p, utf8)                                    \
                         (UTF8_IS_INVARIANT(*(p))                           \
                         ? macro(*(p))                                      \
                         : (UTF8_IS_DOWNGRADEABLE_START(*(p)))              \
                           ? macro(TWO_BYTE_UTF8_TO_NATIVE(*(p), *((p)+1))) \
                           : utf8)

#define _generic_LC_swash_utf8(macro, classnum, p)                         \
                    _generic_LC_utf8(macro, p, _is_utf8_FOO(classnum, p))
#define _generic_LC_func_utf8(macro, above_latin1, p)                         \
                              _generic_LC_utf8(macro, p, above_latin1(p))

#define isALPHANUMERIC_LC_utf8(p)  _generic_LC_swash_utf8(isALPHANUMERIC_LC,  \
                                                      _CC_ALPHANUMERIC, p)
#define isALPHA_LC_utf8(p)   _generic_LC_swash_utf8(isALPHA_LC, _CC_ALPHA, p)
#define isASCII_LC_utf8(p)   isASCII_LC(*p)
#define isBLANK_LC_utf8(p)   _generic_LC_func_utf8(isBLANK_LC, is_HORIZWS_high, p)
#define isCNTRL_LC_utf8(p)   _generic_LC_utf8(isCNTRL_LC, p, 0)
#define isDIGIT_LC_utf8(p)   _generic_LC_swash_utf8(isDIGIT_LC, _CC_DIGIT, p)
#define isGRAPH_LC_utf8(p)   _generic_LC_swash_utf8(isGRAPH_LC, _CC_GRAPH, p)
#define isIDCONT_LC_utf8(p) _generic_LC_func_utf8(isIDCONT_LC, _is_utf8_perl_idcont, p)
#define isIDFIRST_LC_utf8(p) _generic_LC_func_utf8(isIDFIRST_LC, _is_utf8_perl_idstart, p)
#define isLOWER_LC_utf8(p)   _generic_LC_swash_utf8(isLOWER_LC, _CC_LOWER, p)
#define isPRINT_LC_utf8(p)   _generic_LC_swash_utf8(isPRINT_LC, _CC_PRINT, p)
#define isPSXSPC_LC_utf8(p)  isSPACE_LC_utf8(p) /* space is identical to posix
                                                   space under locale */
#define isPUNCT_LC_utf8(p)   _generic_LC_swash_utf8(isPUNCT_LC, _CC_PUNCT, p)
#define isSPACE_LC_utf8(p)   _generic_LC_func_utf8(isSPACE_LC, is_XPERLSPACE_high, p)
#define isUPPER_LC_utf8(p)   _generic_LC_swash_utf8(isUPPER_LC, _CC_UPPER, p)
#define isWORDCHAR_LC_utf8(p) _generic_LC_swash_utf8(isWORDCHAR_LC,           \
                                                            _CC_WORDCHAR, p)
#define isXDIGIT_LC_utf8(p)  _generic_LC_func_utf8(isXDIGIT_LC, is_XDIGIT_high, p)

/* Macros for backwards compatibility and for completeness when the ASCII and
 * Latin1 values are identical */
#define isALPHAU(c)     isALPHA_L1(c)
#define isDIGIT_L1(c)   isDIGIT_A(c)
#define isOCTAL(c)      isOCTAL_A(c)
#define isOCTAL_L1(c)   isOCTAL_A(c)
#define isXDIGIT_L1(c)  isXDIGIT_A(c)
#define isALNUM(c)      isWORDCHAR(c)
#define isALNUMU(c)     isWORDCHAR_L1(c)
#define isALNUM_LC(c)   isWORDCHAR_LC(c)
#define isALNUM_uni(c)  isWORDCHAR_uni(c)
#define isALNUM_LC_uvchr(c) isWORDCHAR_LC_uvchr(c)
#define isALNUM_utf8(p) isWORDCHAR_utf8(p)
#define isALNUM_LC_utf8(p) isWORDCHAR_LC_utf8(p)
#define isALNUMC_A(c)   isALPHANUMERIC_A(c)      /* Mnemonic: "C's alnum" */
#define isALNUMC_L1(c)  isALPHANUMERIC_L1(c)
#define isALNUMC(c)	isALPHANUMERIC(c)
#define isALNUMC_LC(c)	isALPHANUMERIC_LC(c)
#define isALNUMC_uni(c) isALPHANUMERIC_uni(c)
#define isALNUMC_LC_uvchr(c) isALPHANUMERIC_LC_uvchr(c)
#define isALNUMC_utf8(p) isALPHANUMERIC_utf8(p)
#define isALNUMC_LC_utf8(p) isALPHANUMERIC_LC_utf8(p)

/* On EBCDIC platforms, CTRL-@ is 0, CTRL-A is 1, etc, just like on ASCII,
 * except that they don't necessarily mean the same characters, e.g. CTRL-D is
 * 4 on both systems, but that is EOT on ASCII;  ST on EBCDIC.
 * '?' is special-cased on EBCDIC to APC, which is the control there that is
 * the outlier from the block that contains the other controls, just like
 * toCTRL('?') on ASCII yields DEL, the control that is the outlier from the C0
 * block.  If it weren't special cased, it would yield a non-control.
 * The conversion works both ways, so CTRL('D') is 4, and CTRL(4) is D, etc. */
#ifndef EBCDIC
#  define toCTRL(c)    (toUPPER(c) ^ 64)
#else
#  define toCTRL(c)    ((c) == '?'                               \
                        ? LATIN1_TO_NATIVE(0x9F)                 \
                        : (c) == LATIN1_TO_NATIVE(0x9F)          \
                          ? '?'                                  \
                          : (NATIVE_TO_LATIN1(toUPPER(c)) ^ 64))
#endif

/* Line numbers are unsigned, 32 bits. */
typedef U32 line_t;
#define NOLINE ((line_t) 4294967295UL)  /* = FFFFFFFF */

/* Helpful alias for version prescan */
#define is_LAX_VERSION(a,b) \
	(a != Perl_prescan_version(aTHX_ a, FALSE, b, NULL, NULL, NULL, NULL))

#define is_STRICT_VERSION(a,b) \
	(a != Perl_prescan_version(aTHX_ a, TRUE, b, NULL, NULL, NULL, NULL))

#define BADVERSION(a,b,c) \
	if (b) { \
	    *b = c; \
	} \
	return a;

/* Converts a character known to represent a hexadecimal digit (0-9, A-F, or
 * a-f) to its numeric value.  READ_XDIGIT's argument is a string pointer,
 * which is advanced.  The input is validated only by an assert() in DEBUGGING
 * builds.  In both ASCII and EBCDIC the last 4 bits of the digits are 0-9; and
 * the last 4 bits of A-F and a-f are 1-6, so adding 9 yields 10-15 */
#define XDIGIT_VALUE(c) (__ASSERT_(isXDIGIT(c)) (0xf & (isDIGIT(c)        \
                                                        ? (c)             \
                                                        : ((c) + 9))))
#define READ_XDIGIT(s)  (__ASSERT_(isXDIGIT(*s)) (0xf & (isDIGIT(*(s))     \
                                                        ? (*(s)++)         \
                                                        : (*(s)++ + 9))))

/* Converts a character known to represent an octal digit (0-7) to its numeric
 * value.  The input is validated only by an assert() in DEBUGGING builds.  In
 * both ASCII and EBCDIC the last 3 bits of the octal digits range from 0-7. */
#define OCTAL_VALUE(c) (__ASSERT_(isOCTAL(c)) (7 & (c)))

/*
=head1 Memory Management

=for apidoc Am|void|Newx|void* ptr|int nitems|type
The XSUB-writer's interface to the C C<malloc> function.

Memory obtained by this should B<ONLY> be freed with L<"Safefree">.

In 5.9.3, Newx() and friends replace the older New() API, and drops
the first parameter, I<x>, a debug aid which allowed callers to identify
themselves.  This aid has been superseded by a new build option,
PERL_MEM_LOG (see L<perlhacktips/PERL_MEM_LOG>).  The older API is still
there for use in XS modules supporting older perls.

=for apidoc Am|void|Newxc|void* ptr|int nitems|type|cast
The XSUB-writer's interface to the C C<malloc> function, with
cast.  See also C<Newx>.

Memory obtained by this should B<ONLY> be freed with L<"Safefree">.

=for apidoc Am|void|Newxz|void* ptr|int nitems|type
The XSUB-writer's interface to the C C<malloc> function.  The allocated
memory is zeroed with C<memzero>.  See also C<Newx>.

Memory obtained by this should B<ONLY> be freed with L<"Safefree">.

=for apidoc Am|void|Renew|void* ptr|int nitems|type
The XSUB-writer's interface to the C C<realloc> function.

Memory obtained by this should B<ONLY> be freed with L<"Safefree">.

=for apidoc Am|void|Renewc|void* ptr|int nitems|type|cast
The XSUB-writer's interface to the C C<realloc> function, with
cast.

Memory obtained by this should B<ONLY> be freed with L<"Safefree">.

=for apidoc Am|void|Safefree|void* ptr
The XSUB-writer's interface to the C C<free> function.

This should B<ONLY> be used on memory obtained using L<"Newx"> and friends.

=for apidoc Am|void|Move|void* src|void* dest|int nitems|type
The XSUB-writer's interface to the C C<memmove> function.  The C<src> is the
source, C<dest> is the destination, C<nitems> is the number of items, and
C<type> is the type.  Can do overlapping moves.  See also C<Copy>.

=for apidoc Am|void *|MoveD|void* src|void* dest|int nitems|type
Like C<Move> but returns dest.  Useful
for encouraging compilers to tail-call
optimise.

=for apidoc Am|void|Copy|void* src|void* dest|int nitems|type
The XSUB-writer's interface to the C C<memcpy> function.  The C<src> is the
source, C<dest> is the destination, C<nitems> is the number of items, and
C<type> is the type.  May fail on overlapping copies.  See also C<Move>.

=for apidoc Am|void *|CopyD|void* src|void* dest|int nitems|type

Like C<Copy> but returns dest.  Useful
for encouraging compilers to tail-call
optimise.

=for apidoc Am|void|Zero|void* dest|int nitems|type

The XSUB-writer's interface to the C C<memzero> function.  The C<dest> is the
destination, C<nitems> is the number of items, and C<type> is the type.

=for apidoc Am|void *|ZeroD|void* dest|int nitems|type

Like C<Zero> but returns dest.  Useful
for encouraging compilers to tail-call
optimise.

=for apidoc Am|void|StructCopy|type *src|type *dest|type
This is an architecture-independent macro to copy one structure to another.

=for apidoc Am|void|PoisonWith|void* dest|int nitems|type|U8 byte

Fill up memory with a byte pattern (a byte repeated over and over
again) that hopefully catches attempts to access uninitialized memory.

=for apidoc Am|void|PoisonNew|void* dest|int nitems|type

PoisonWith(0xAB) for catching access to allocated but uninitialized memory.

=for apidoc Am|void|PoisonFree|void* dest|int nitems|type

PoisonWith(0xEF) for catching access to freed memory.

=for apidoc Am|void|Poison|void* dest|int nitems|type

PoisonWith(0xEF) for catching access to freed memory.

=cut */

/* Maintained for backwards-compatibility only. Use newSV() instead. */
#ifndef PERL_CORE
#define NEWSV(x,len)	newSV(len)
#endif

#define MEM_SIZE_MAX ((MEM_SIZE)~0)

/* The +0.0 in MEM_WRAP_CHECK_ is an attempt to foil
 * overly eager compilers that will bleat about e.g.
 * (U16)n > (size_t)~0/sizeof(U16) always being false. */
#ifdef PERL_MALLOC_WRAP
#define MEM_WRAP_CHECK(n,t) \
	(void)(UNLIKELY(sizeof(t) > 1 && ((MEM_SIZE)(n)+0.0) > MEM_SIZE_MAX/sizeof(t)) && (croak_memory_wrap(),0))
#define MEM_WRAP_CHECK_1(n,t,a) \
	(void)(UNLIKELY(sizeof(t) > 1 && ((MEM_SIZE)(n)+0.0) > MEM_SIZE_MAX/sizeof(t)) && (Perl_croak_nocontext("%s",(a)),0))
#define MEM_WRAP_CHECK_(n,t) MEM_WRAP_CHECK(n,t),

#define PERL_STRLEN_ROUNDUP(n) ((void)(((n) > MEM_SIZE_MAX - 2 * PERL_STRLEN_ROUNDUP_QUANTUM) ? (croak_memory_wrap(),0):0),((n-1+PERL_STRLEN_ROUNDUP_QUANTUM)&~((MEM_SIZE)PERL_STRLEN_ROUNDUP_QUANTUM-1)))
#else

#define MEM_WRAP_CHECK(n,t)
#define MEM_WRAP_CHECK_1(n,t,a)
#define MEM_WRAP_CHECK_2(n,t,a,b)
#define MEM_WRAP_CHECK_(n,t)

#define PERL_STRLEN_ROUNDUP(n) (((n-1+PERL_STRLEN_ROUNDUP_QUANTUM)&~((MEM_SIZE)PERL_STRLEN_ROUNDUP_QUANTUM-1)))

#endif

#ifdef PERL_MEM_LOG
/*
 * If PERL_MEM_LOG is defined, all Newx()s, Renew()s, and Safefree()s
 * go through functions, which are handy for debugging breakpoints, but
 * which more importantly get the immediate calling environment (file and
 * line number, and C function name if available) passed in.  This info can
 * then be used for logging the calls, for which one gets a sample
 * implementation unless -DPERL_MEM_LOG_NOIMPL is also defined.
 *
 * Known problems:
 * - not all memory allocs get logged, only those
 *   that go through Newx() and derivatives (while all
 *   Safefrees do get logged)
 * - __FILE__ and __LINE__ do not work everywhere
 * - __func__ or __FUNCTION__ even less so
 * - I think more goes on after the perlio frees but
 *   the thing is that STDERR gets closed (as do all
 *   the file descriptors)
 * - no deeper calling stack than the caller of the Newx()
 *   or the kind, but do I look like a C reflection/introspection
 *   utility to you?
 * - the function prototypes for the logging functions
 *   probably should maybe be somewhere else than handy.h
 * - one could consider inlining (macrofying) the logging
 *   for speed, but I am too lazy
 * - one could imagine recording the allocations in a hash,
 *   (keyed by the allocation address?), and maintain that
 *   through reallocs and frees, but how to do that without
 *   any News() happening...?
 * - lots of -Ddefines to get useful/controllable output
 * - lots of ENV reads
 */

PERL_EXPORT_C Malloc_t Perl_mem_log_alloc(const UV n, const UV typesize, const char *type_name, Malloc_t newalloc, const char *filename, const int linenumber, const char *funcname);

PERL_EXPORT_C Malloc_t Perl_mem_log_realloc(const UV n, const UV typesize, const char *type_name, Malloc_t oldalloc, Malloc_t newalloc, const char *filename, const int linenumber, const char *funcname);

PERL_EXPORT_C Malloc_t Perl_mem_log_free(Malloc_t oldalloc, const char *filename, const int linenumber, const char *funcname);

# ifdef PERL_CORE
#  ifndef PERL_MEM_LOG_NOIMPL
enum mem_log_type {
  MLT_ALLOC,
  MLT_REALLOC,
  MLT_FREE,
  MLT_NEW_SV,
  MLT_DEL_SV
};
#  endif
#  if defined(PERL_IN_SV_C)  /* those are only used in sv.c */
void Perl_mem_log_new_sv(const SV *sv, const char *filename, const int linenumber, const char *funcname);
void Perl_mem_log_del_sv(const SV *sv, const char *filename, const int linenumber, const char *funcname);
#  endif
# endif

#endif

#ifdef PERL_MEM_LOG
#define MEM_LOG_ALLOC(n,t,a)     Perl_mem_log_alloc(n,sizeof(t),STRINGIFY(t),a,__FILE__,__LINE__,FUNCTION__)
#define MEM_LOG_REALLOC(n,t,v,a) Perl_mem_log_realloc(n,sizeof(t),STRINGIFY(t),v,a,__FILE__,__LINE__,FUNCTION__)
#define MEM_LOG_FREE(a)          Perl_mem_log_free(a,__FILE__,__LINE__,FUNCTION__)
#endif

#ifndef MEM_LOG_ALLOC
#define MEM_LOG_ALLOC(n,t,a)     (a)
#endif
#ifndef MEM_LOG_REALLOC
#define MEM_LOG_REALLOC(n,t,v,a) (a)
#endif
#ifndef MEM_LOG_FREE
#define MEM_LOG_FREE(a)          (a)
#endif

#define Newx(v,n,t)	(v = (MEM_WRAP_CHECK_(n,t) (t*)MEM_LOG_ALLOC(n,t,safemalloc((MEM_SIZE)((n)*sizeof(t))))))
#define Newxc(v,n,t,c)	(v = (MEM_WRAP_CHECK_(n,t) (c*)MEM_LOG_ALLOC(n,t,safemalloc((MEM_SIZE)((n)*sizeof(t))))))
#define Newxz(v,n,t)	(v = (MEM_WRAP_CHECK_(n,t) (t*)MEM_LOG_ALLOC(n,t,safecalloc((n),sizeof(t)))))

#ifndef PERL_CORE
/* pre 5.9.x compatibility */
#define New(x,v,n,t)	Newx(v,n,t)
#define Newc(x,v,n,t,c)	Newxc(v,n,t,c)
#define Newz(x,v,n,t)	Newxz(v,n,t)
#endif

#define Renew(v,n,t) \
	  (v = (MEM_WRAP_CHECK_(n,t) (t*)MEM_LOG_REALLOC(n,t,v,saferealloc((Malloc_t)(v),(MEM_SIZE)((n)*sizeof(t))))))
#define Renewc(v,n,t,c) \
	  (v = (MEM_WRAP_CHECK_(n,t) (c*)MEM_LOG_REALLOC(n,t,v,saferealloc((Malloc_t)(v),(MEM_SIZE)((n)*sizeof(t))))))

#ifdef PERL_POISON
#define Safefree(d) \
  ((d) ? (void)(safefree(MEM_LOG_FREE((Malloc_t)(d))), Poison(&(d), 1, Malloc_t)) : (void) 0)
#else
#define Safefree(d)	safefree(MEM_LOG_FREE((Malloc_t)(d)))
#endif

#define Move(s,d,n,t)	(MEM_WRAP_CHECK_(n,t) (void)memmove((char*)(d),(const char*)(s), (n) * sizeof(t)))
#define Copy(s,d,n,t)	(MEM_WRAP_CHECK_(n,t) (void)memcpy((char*)(d),(const char*)(s), (n) * sizeof(t)))
#define Zero(d,n,t)	(MEM_WRAP_CHECK_(n,t) (void)memzero((char*)(d), (n) * sizeof(t)))

#define MoveD(s,d,n,t)	(MEM_WRAP_CHECK_(n,t) memmove((char*)(d),(const char*)(s), (n) * sizeof(t)))
#define CopyD(s,d,n,t)	(MEM_WRAP_CHECK_(n,t) memcpy((char*)(d),(const char*)(s), (n) * sizeof(t)))
#ifdef HAS_MEMSET
#define ZeroD(d,n,t)	(MEM_WRAP_CHECK_(n,t) memzero((char*)(d), (n) * sizeof(t)))
#else
/* Using bzero(), which returns void.  */
#define ZeroD(d,n,t)	(MEM_WRAP_CHECK_(n,t) memzero((char*)(d), (n) * sizeof(t)),d)
#endif

#define PoisonWith(d,n,t,b)	(MEM_WRAP_CHECK_(n,t) (void)memset((char*)(d), (U8)(b), (n) * sizeof(t)))
#define PoisonNew(d,n,t)	PoisonWith(d,n,t,0xAB)
#define PoisonFree(d,n,t)	PoisonWith(d,n,t,0xEF)
#define Poison(d,n,t)		PoisonFree(d,n,t)

#ifdef PERL_POISON
#  define PERL_POISON_EXPR(x) x
#else
#  define PERL_POISON_EXPR(x)
#endif

#ifdef USE_STRUCT_COPY
#define StructCopy(s,d,t) (*((t*)(d)) = *((t*)(s)))
#else
#define StructCopy(s,d,t) Copy(s,d,1,t)
#endif

/* C_ARRAY_LENGTH is the number of elements in the C array (so you
 * want your zero-based indices to be less than but not equal to).
 *
 * C_ARRAY_END is one past the last: half-open/half-closed range,
 * not last-inclusive range. */
#define C_ARRAY_LENGTH(a)	(sizeof(a)/sizeof((a)[0]))
#define C_ARRAY_END(a)		((a) + C_ARRAY_LENGTH(a))

#ifdef NEED_VA_COPY
# ifdef va_copy
#  define Perl_va_copy(s, d) va_copy(d, s)
# else
#  if defined(__va_copy)
#   define Perl_va_copy(s, d) __va_copy(d, s)
#  else
#   define Perl_va_copy(s, d) Copy(s, d, 1, va_list)
#  endif
# endif
#endif

/* convenience debug macros */
#ifdef USE_ITHREADS
#define pTHX_FORMAT  "Perl interpreter: 0x%p"
#define pTHX__FORMAT ", Perl interpreter: 0x%p"
#define pTHX_VALUE_   (void *)my_perl,
#define pTHX_VALUE    (void *)my_perl
#define pTHX__VALUE_ ,(void *)my_perl,
#define pTHX__VALUE  ,(void *)my_perl
#else
#define pTHX_FORMAT
#define pTHX__FORMAT
#define pTHX_VALUE_
#define pTHX_VALUE
#define pTHX__VALUE_
#define pTHX__VALUE
#endif /* USE_ITHREADS */

/* Perl_deprecate was not part of the public API, and did not have a deprecate()
   shortcut macro defined without -DPERL_CORE. Neither codesearch.google.com nor
   CPAN::Unpack show any users outside the core.  */
#ifdef PERL_CORE
#  define deprecate(s) Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED), "Use of " s " is deprecated")
#endif

/* Internal macros to deal with gids and uids */
#ifdef PERL_CORE

#  if Uid_t_size > IVSIZE
#    define sv_setuid(sv, uid)       sv_setnv((sv), (NV)(uid))
#    define SvUID(sv)                SvNV(sv)
#  else
#    if Uid_t_sign <= 0
#      define sv_setuid(sv, uid)       sv_setiv((sv), (IV)(uid))
#      define SvUID(sv)                SvIV(sv)
#    else
#      define sv_setuid(sv, uid)       sv_setuv((sv), (UV)(uid))
#      define SvUID(sv)                SvUV(sv)
#    endif
#  endif /* Uid_t_size */

#  if Gid_t_size > IVSIZE
#    define sv_setgid(sv, gid)       sv_setnv((sv), (NV)(gid))
#    define SvGID(sv)                SvNV(sv)
#  else
#    if Gid_t_sign <= 0
#      define sv_setgid(sv, gid)       sv_setiv((sv), (IV)(gid))
#      define SvGID(sv)                SvIV(sv)
#    else
#      define sv_setgid(sv, gid)       sv_setuv((sv), (UV)(gid))
#      define SvGID(sv)                SvUV(sv)
#    endif
#  endif /* Gid_t_size */

#endif

#endif  /* HANDY_H */

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
