/*    pp_pack.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2003, 2004, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * He still hopefully carried some of his gear in his pack: a small tinder-box,
 * two small shallow pans, the smaller fitting into the larger; inside them a
 * wooden spoon, a short two-pronged fork and some skewers were stowed; and
 * hidden at the bottom of the pack in a flat wooden box a dwindling treasure,
 * some salt.
 */

#include "EXTERN.h"
#define PERL_IN_PP_PACK_C
#include "perl.h"

/*
 * Offset for integer pack/unpack.
 *
 * On architectures where I16 and I32 aren't really 16 and 32 bits,
 * which for now are all Crays, pack and unpack have to play games.
 */

/*
 * These values are required for portability of pack() output.
 * If they're not right on your machine, then pack() and unpack()
 * wouldn't work right anyway; you'll need to apply the Cray hack.
 * (I'd like to check them with #if, but you can't use sizeof() in
 * the preprocessor.)  --???
 */
/*
    The appropriate SHORTSIZE, INTSIZE, LONGSIZE, and LONGLONGSIZE
    defines are now in config.h.  --Andy Dougherty  April 1998
 */
#define SIZE16 2
#define SIZE32 4

/* CROSSCOMPILE and MULTIARCH are going to affect pp_pack() and pp_unpack().
   --jhi Feb 1999 */

#if SHORTSIZE != SIZE16 || LONGSIZE != SIZE32
#   define PERL_NATINT_PACK
#endif

#if LONGSIZE > 4 && defined(_CRAY)
#  if BYTEORDER == 0x12345678
#    define OFF16(p)	(char*)(p)
#    define OFF32(p)	(char*)(p)
#  else
#    if BYTEORDER == 0x87654321
#      define OFF16(p)	((char*)(p) + (sizeof(U16) - SIZE16))
#      define OFF32(p)	((char*)(p) + (sizeof(U32) - SIZE32))
#    else
       }}}} bad cray byte order
#    endif
#  endif
#  define COPY16(s,p)  (*(p) = 0, Copy(s, OFF16(p), SIZE16, char))
#  define COPY32(s,p)  (*(p) = 0, Copy(s, OFF32(p), SIZE32, char))
#  define COPYNN(s,p,n) (*(p) = 0, Copy(s, (char *)(p), n, char))
#  define CAT16(sv,p)  sv_catpvn(sv, OFF16(p), SIZE16)
#  define CAT32(sv,p)  sv_catpvn(sv, OFF32(p), SIZE32)
#else
#  define COPY16(s,p)  Copy(s, p, SIZE16, char)
#  define COPY32(s,p)  Copy(s, p, SIZE32, char)
#  define COPYNN(s,p,n) Copy(s, (char *)(p), n, char)
#  define CAT16(sv,p)  sv_catpvn(sv, (char*)(p), SIZE16)
#  define CAT32(sv,p)  sv_catpvn(sv, (char*)(p), SIZE32)
#endif

/* Avoid stack overflow due to pathological templates. 100 should be plenty. */
#define MAX_SUB_TEMPLATE_LEVEL 100

/* flags */
#define FLAG_UNPACK_ONLY_ONE  0x10
#define FLAG_UNPACK_DO_UTF8   0x08
#define FLAG_SLASH            0x04
#define FLAG_COMMA            0x02
#define FLAG_PACK             0x01

STATIC SV *
S_mul128(pTHX_ SV *sv, U8 m)
{
  STRLEN          len;
  char           *s = SvPV(sv, len);
  char           *t;
  U32             i = 0;

  if (!strnEQ(s, "0000", 4)) {  /* need to grow sv */
    SV             *tmpNew = newSVpvn("0000000000", 10);

    sv_catsv(tmpNew, sv);
    SvREFCNT_dec(sv);		/* free old sv */
    sv = tmpNew;
    s = SvPV(sv, len);
  }
  t = s + len - 1;
  while (!*t)                   /* trailing '\0'? */
    t--;
  while (t > s) {
    i = ((*t - '0') << 7) + m;
    *(t--) = '0' + (char)(i % 10);
    m = (char)(i / 10);
  }
  return (sv);
}

/* Explosives and implosives. */

#if 'I' == 73 && 'J' == 74
/* On an ASCII/ISO kind of system */
#define ISUUCHAR(ch)    ((ch) >= ' ' && (ch) < 'a')
#else
/*
  Some other sort of character set - use memchr() so we don't match
  the null byte.
 */
#define ISUUCHAR(ch)    (memchr(PL_uuemap, (ch), sizeof(PL_uuemap)-1) || (ch) == ' ')
#endif

#define TYPE_IS_SHRIEKING	0x100

/* Returns the sizeof() struct described by pat */
STATIC I32
S_measure_struct(pTHX_ register tempsym_t* symptr)
{
    register I32 len = 0;
    register I32 total = 0;
    int star;

    register int size;

    while (next_symbol(symptr)) {

        switch( symptr->howlen ){
        case e_no_len:
	case e_number:
	    len = symptr->length;
	    break;
        case e_star:
   	    Perl_croak(aTHX_ "Within []-length '*' not allowed in %s",
                       symptr->flags & FLAG_PACK ? "pack" : "unpack" );
            break;
        }

	switch(symptr->code) {
	default:
    Perl_croak(aTHX_ "Invalid type '%c' in %s",
                       (int)symptr->code,
                       symptr->flags & FLAG_PACK ? "pack" : "unpack" );
	case '@':
	case '/':
	case 'U':			/* XXXX Is it correct? */
	case 'w':
	case 'u':
	    Perl_croak(aTHX_ "Within []-length '%c' not allowed in %s",
                       (int)symptr->code,
                       symptr->flags & FLAG_PACK ? "pack" : "unpack" );
	case '%':
	    size = 0;
	    break;
	case '(':
	{
            tempsym_t savsym = *symptr;
  	    symptr->patptr = savsym.grpbeg;
            symptr->patend = savsym.grpend;
 	    /* XXXX Theoretically, we need to measure many times at different
 	       positions, since the subexpression may contain
 	       alignment commands, but be not of aligned length.
 	       Need to detect this and croak().  */
	    size = measure_struct(symptr);
            *symptr = savsym;
	    break;
	}
 	case 'X' | TYPE_IS_SHRIEKING:
 	    /* XXXX Is this useful?  Then need to treat MEASURE_BACKWARDS. */
 	    if (!len)			/* Avoid division by 0 */
 		len = 1;
 	    len = total % len;		/* Assumed: the start is aligned. */
 	    /* FALL THROUGH */
	case 'X':
	    size = -1;
	    if (total < len)
		Perl_croak(aTHX_ "'X' outside of string in %s",
                          symptr->flags & FLAG_PACK ? "pack" : "unpack" );
	    break;
 	case 'x' | TYPE_IS_SHRIEKING:
 	    if (!len)			/* Avoid division by 0 */
 		len = 1;
 	    star = total % len;		/* Assumed: the start is aligned. */
 	    if (star)			/* Other portable ways? */
 		len = len - star;
 	    else
 		len = 0;
 	    /* FALL THROUGH */
	case 'x':
	case 'A':
	case 'Z':
	case 'a':
	case 'c':
	case 'C':
	    size = 1;
	    break;
	case 'B':
	case 'b':
	    len = (len + 7)/8;
	    size = 1;
	    break;
	case 'H':
	case 'h':
	    len = (len + 1)/2;
	    size = 1;
	    break;
	case 's' | TYPE_IS_SHRIEKING:
#if SHORTSIZE != SIZE16
	    size = sizeof(short);
	    break;
#else
            /* FALL THROUGH */
#endif
	case 's':
	    size = SIZE16;
	    break;
	case 'S' | TYPE_IS_SHRIEKING:
#if SHORTSIZE != SIZE16
	    size = sizeof(unsigned short);
	    break;
#else
            /* FALL THROUGH */
#endif
	case 'v':
	case 'n':
	case 'S':
	    size = SIZE16;
	    break;
	case 'i' | TYPE_IS_SHRIEKING:
	case 'i':
	    size = sizeof(int);
	    break;
	case 'I' | TYPE_IS_SHRIEKING:
	case 'I':
	    size = sizeof(unsigned int);
	    break;
	case 'j':
	    size = IVSIZE;
	    break;
	case 'J':
	    size = UVSIZE;
	    break;
	case 'l' | TYPE_IS_SHRIEKING:
#if LONGSIZE != SIZE32
	    size = sizeof(long);
            break;
#else
            /* FALL THROUGH */
#endif
	case 'l':
	    size = SIZE32;
	    break;
	case 'L' | TYPE_IS_SHRIEKING:
#if LONGSIZE != SIZE32
	    size = sizeof(unsigned long);
	    break;
#else
            /* FALL THROUGH */
#endif
	case 'V':
	case 'N':
	case 'L':
	    size = SIZE32;
	    break;
	case 'P':
	    len = 1;
	    /* FALL THROUGH */
	case 'p':
	    size = sizeof(char*);
	    break;
#ifdef HAS_QUAD
	case 'q':
	    size = sizeof(Quad_t);
	    break;
	case 'Q':
	    size = sizeof(Uquad_t);
	    break;
#endif
	case 'f':
	    size = sizeof(float);
	    break;
	case 'd':
	    size = sizeof(double);
	    break;
	case 'F':
	    size = NVSIZE;
	    break;
#if defined(HAS_LONG_DOUBLE) && defined(USE_LONG_DOUBLE)
	case 'D':
	    size = LONG_DOUBLESIZE;
	    break;
#endif
	}
	total += len * size;
    }
    return total;
}


/* locate matching closing parenthesis or bracket
 * returns char pointer to char after match, or NULL
 */
STATIC char *
S_group_end(pTHX_ register char *patptr, register char *patend, char ender)
{
    while (patptr < patend) {
	char c = *patptr++;

	if (isSPACE(c))
	    continue;
	else if (c == ender)
	    return patptr-1;
	else if (c == '#') {
	    while (patptr < patend && *patptr != '\n')
		patptr++;
	    continue;
	} else if (c == '(')
	    patptr = group_end(patptr, patend, ')') + 1;
	else if (c == '[')
	    patptr = group_end(patptr, patend, ']') + 1;
    }
    Perl_croak(aTHX_ "No group ending character '%c' found in template",
               ender);
    return 0;
}


/* Convert unsigned decimal number to binary.
 * Expects a pointer to the first digit and address of length variable
 * Advances char pointer to 1st non-digit char and returns number
 */ 
STATIC char *
S_get_num(pTHX_ register char *patptr, I32 *lenptr )
{
  I32 len = *patptr++ - '0';
  while (isDIGIT(*patptr)) {
    if (len >= 0x7FFFFFFF/10)
      Perl_croak(aTHX_ "pack/unpack repeat count overflow");
    len = (len * 10) + (*patptr++ - '0');
  }
  *lenptr = len;
  return patptr;
}

/* The marvellous template parsing routine: Using state stored in *symptr,
 * locates next template code and count
 */
STATIC bool
S_next_symbol(pTHX_ register tempsym_t* symptr )
{
  register char* patptr = symptr->patptr; 
  register char* patend = symptr->patend; 

  symptr->flags &= ~FLAG_SLASH;

  while (patptr < patend) {
    if (isSPACE(*patptr))
      patptr++;
    else if (*patptr == '#') {
      patptr++;
      while (patptr < patend && *patptr != '\n')
	patptr++;
      if (patptr < patend)
	patptr++;
    } else {
      /* We should have found a template code */ 
      I32 code = *patptr++ & 0xFF;

      if (code == ','){ /* grandfather in commas but with a warning */
	if (((symptr->flags & FLAG_COMMA) == 0) && ckWARN(WARN_UNPACK)){
          symptr->flags |= FLAG_COMMA;
	  Perl_warner(aTHX_ packWARN(WARN_UNPACK),
	 	      "Invalid type ',' in %s",
                      symptr->flags & FLAG_PACK ? "pack" : "unpack" );
        }
	continue;
      }
      
      /* for '(', skip to ')' */
      if (code == '(') {  
        if( isDIGIT(*patptr) || *patptr == '*' || *patptr == '[' )
          Perl_croak(aTHX_ "()-group starts with a count in %s",
                     symptr->flags & FLAG_PACK ? "pack" : "unpack" );
        symptr->grpbeg = patptr;
        patptr = 1 + ( symptr->grpend = group_end(patptr, patend, ')') );
        if( symptr->level >= MAX_SUB_TEMPLATE_LEVEL )
	  Perl_croak(aTHX_ "Too deeply nested ()-groups in %s",
                     symptr->flags & FLAG_PACK ? "pack" : "unpack" );
      }

      /* test for '!' modifier */
      if (patptr < patend && *patptr == '!') {
	static const char natstr[] = "sSiIlLxX";
        patptr++;	        
        if (strchr(natstr, code))
 	  code |= TYPE_IS_SHRIEKING;
        else
   	  Perl_croak(aTHX_ "'!' allowed only after types %s in %s",
                     natstr, symptr->flags & FLAG_PACK ? "pack" : "unpack" );
      }

      /* look for count and/or / */ 
      if (patptr < patend) {
	if (isDIGIT(*patptr)) {
 	  patptr = get_num( patptr, &symptr->length );
          symptr->howlen = e_number;

        } else if (*patptr == '*') {
          patptr++;
          symptr->howlen = e_star;

        } else if (*patptr == '[') {
          char* lenptr = ++patptr;            
          symptr->howlen = e_number;
          patptr = group_end( patptr, patend, ']' ) + 1;
          /* what kind of [] is it? */
          if (isDIGIT(*lenptr)) {
            lenptr = get_num( lenptr, &symptr->length );
            if( *lenptr != ']' )
              Perl_croak(aTHX_ "Malformed integer in [] in %s",
                         symptr->flags & FLAG_PACK ? "pack" : "unpack");
          } else {
            tempsym_t savsym = *symptr;
            symptr->patend = patptr-1;
            symptr->patptr = lenptr;
            savsym.length = measure_struct(symptr);
            *symptr = savsym;
          }
        } else {
          symptr->howlen = e_no_len;
          symptr->length = 1;
        }

        /* try to find / */
        while (patptr < patend) {
          if (isSPACE(*patptr))
            patptr++;
          else if (*patptr == '#') {
            patptr++;
            while (patptr < patend && *patptr != '\n')
	      patptr++;
            if (patptr < patend)
	      patptr++;
          } else {
            if( *patptr == '/' ){ 
              symptr->flags |= FLAG_SLASH;
              patptr++;
              if( patptr < patend &&
                  (isDIGIT(*patptr) || *patptr == '*' || *patptr == '[') )
                Perl_croak(aTHX_ "'/' does not take a repeat count in %s",
                           symptr->flags & FLAG_PACK ? "pack" : "unpack" );
            }
            break;
	  }
	}
      } else {
        /* at end - no count, no / */
        symptr->howlen = e_no_len;
        symptr->length = 1;
      }

      symptr->code = code;
      symptr->patptr = patptr; 
      return TRUE;
    }
  }
  symptr->patptr = patptr; 
  return FALSE;
}

/*
=for apidoc unpack_str

The engine implementing unpack() Perl function. Note: parameters strbeg, new_s
and ocnt are not used. This call should not be used, use unpackstring instead.

=cut */

I32
Perl_unpack_str(pTHX_ char *pat, register char *patend, register char *s, char *strbeg, char *strend, char **new_s, I32 ocnt, U32 flags)
{
    tempsym_t sym = { 0 };
    sym.patptr = pat;
    sym.patend = patend;
    sym.flags  = flags;

    return unpack_rec(&sym, s, s, strend, NULL );
}

/*
=for apidoc unpackstring

The engine implementing unpack() Perl function. C<unpackstring> puts the
extracted list items on the stack and returns the number of elements.
Issue C<PUTBACK> before and C<SPAGAIN> after the call to this function.

=cut */

I32
Perl_unpackstring(pTHX_ char *pat, register char *patend, register char *s, char *strend, U32 flags)
{
    tempsym_t sym = { 0 };
    sym.patptr = pat;
    sym.patend = patend;
    sym.flags  = flags;

    return unpack_rec(&sym, s, s, strend, NULL );
}

STATIC
I32
S_unpack_rec(pTHX_ register tempsym_t* symptr, register char *s, char *strbeg, char *strend, char **new_s )
{
    dSP;
    I32 datumtype;
    register I32 len = 0;
    register I32 bits = 0;
    register char *str;
    SV *sv;
    I32 start_sp_offset = SP - PL_stack_base;
    howlen_t howlen;

    /* These must not be in registers: */
    short ashort;
    int aint;
    long along;
#ifdef HAS_QUAD
    Quad_t aquad;
#endif
    U16 aushort;
    unsigned int auint;
    U32 aulong;
#ifdef HAS_QUAD
    Uquad_t auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;
    I32 checksum = 0;
    UV cuv = 0;
    NV cdouble = 0.0;
    const int bits_in_uv = 8 * sizeof(cuv);
    char* strrelbeg = s;
    bool beyond = FALSE;
    bool unpack_only_one = (symptr->flags & FLAG_UNPACK_ONLY_ONE) != 0;

    IV aiv;
    UV auv;
    NV anv;
#if defined(HAS_LONG_DOUBLE) && defined(USE_LONG_DOUBLE)
    long double aldouble;
#endif

    while (next_symbol(symptr)) {
        datumtype = symptr->code;
	/* do first one only unless in list context
	   / is implemented by unpacking the count, then poping it from the
	   stack, so must check that we're not in the middle of a /  */
        if ( unpack_only_one
	     && (SP - PL_stack_base == start_sp_offset + 1)
	     && (datumtype != '/') )   /* XXX can this be omitted */
            break;

        switch( howlen = symptr->howlen ){
        case e_no_len:
	case e_number:
	    len = symptr->length;
	    break;
        case e_star:
	    len = strend - strbeg;	/* long enough */          
	    break;
        }

      redo_switch:
        beyond = s >= strend;
	switch(datumtype) {
	default:
	    Perl_croak(aTHX_ "Invalid type '%c' in unpack", (int)datumtype );

	case '%':
	    if (howlen == e_no_len)
		len = 16;		/* len is not specified */
	    checksum = len;
	    cuv = 0;
	    cdouble = 0;
	    continue;
	    break;
	case '(':
	{
	    char *ss = s;		/* Move from register */
            tempsym_t savsym = *symptr;
            symptr->patend = savsym.grpend;
            symptr->level++;
	    PUTBACK;
	    while (len--) {
  	        symptr->patptr = savsym.grpbeg;
 	        unpack_rec(symptr, ss, strbeg, strend, &ss );
                if (ss == strend && savsym.howlen == e_star)
		    break; /* No way to continue */
	    }
	    SPAGAIN;
	    s = ss;
            savsym.flags = symptr->flags;
            *symptr = savsym;
	    break;
	}
	case '@':
	    if (len > strend - strrelbeg)
		Perl_croak(aTHX_ "'@' outside of string in unpack");
	    s = strrelbeg + len;
	    break;
 	case 'X' | TYPE_IS_SHRIEKING:
 	    if (!len)			/* Avoid division by 0 */
 		len = 1;
 	    len = (s - strbeg) % len;
 	    /* FALL THROUGH */
	case 'X':
	    if (len > s - strbeg)
		Perl_croak(aTHX_ "'X' outside of string in unpack" );
	    s -= len;
	    break;
 	case 'x' | TYPE_IS_SHRIEKING:
 	    if (!len)			/* Avoid division by 0 */
 		len = 1;
 	    aint = (s - strbeg) % len;
 	    if (aint)			/* Other portable ways? */
 		len = len - aint;
 	    else
 		len = 0;
 	    /* FALL THROUGH */
	case 'x':
	    if (len > strend - s)
		Perl_croak(aTHX_ "'x' outside of string in unpack");
	    s += len;
	    break;
	case '/':
	    Perl_croak(aTHX_ "'/' must follow a numeric type in unpack");
            break;
	case 'A':
	case 'Z':
	case 'a':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum)
		goto uchar_checksum;
	    sv = NEWSV(35, len);
	    sv_setpvn(sv, s, len);
	    if (len > 0 && (datumtype == 'A' || datumtype == 'Z')) {
		aptr = s;	/* borrow register */
		if (datumtype == 'Z') {	/* 'Z' strips stuff after first null */
		    s = SvPVX(sv);
		    while (*s)
			s++;
		    if (howlen == e_star) /* exact for 'Z*' */
		        len = s - SvPVX(sv) + 1;
		}
		else {		/* 'A' strips both nulls and spaces */
		    s = SvPVX(sv) + len - 1;
		    while (s >= SvPVX(sv) && (!*s || isSPACE(*s)))
			s--;
		    *++s = '\0';
		}
		SvCUR_set(sv, s - SvPVX(sv));
		s = aptr;	/* unborrow register */
	    }
	    s += len;
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'B':
	case 'b':
	    if (howlen == e_star || len > (strend - s) * 8)
		len = (strend - s) * 8;
	    if (checksum) {
		if (!PL_bitcount) {
		    Newz(601, PL_bitcount, 256, char);
		    for (bits = 1; bits < 256; bits++) {
			if (bits & 1)	PL_bitcount[bits]++;
			if (bits & 2)	PL_bitcount[bits]++;
			if (bits & 4)	PL_bitcount[bits]++;
			if (bits & 8)	PL_bitcount[bits]++;
			if (bits & 16)	PL_bitcount[bits]++;
			if (bits & 32)	PL_bitcount[bits]++;
			if (bits & 64)	PL_bitcount[bits]++;
			if (bits & 128)	PL_bitcount[bits]++;
		    }
		}
		while (len >= 8) {
		    cuv += PL_bitcount[*(unsigned char*)s++];
		    len -= 8;
		}
		if (len) {
		    bits = *s;
		    if (datumtype == 'b') {
			while (len-- > 0) {
			    if (bits & 1) cuv++;
			    bits >>= 1;
			}
		    }
		    else {
			while (len-- > 0) {
			    if (bits & 128) cuv++;
			    bits <<= 1;
			}
		    }
		}
		break;
	    }
	    sv = NEWSV(35, len + 1);
	    SvCUR_set(sv, len);
	    SvPOK_on(sv);
	    str = SvPVX(sv);
	    if (datumtype == 'b') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)		/*SUPPRESS 595*/
			bits >>= 1;
		    else
			bits = *s++;
		    *str++ = '0' + (bits & 1);
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)
			bits <<= 1;
		    else
			bits = *s++;
		    *str++ = '0' + ((bits & 128) != 0);
		}
	    }
	    *str = '\0';
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'H':
	case 'h':
	    if (howlen == e_star || len > (strend - s) * 2)
		len = (strend - s) * 2;
	    sv = NEWSV(35, len + 1);
	    SvCUR_set(sv, len);
	    SvPOK_on(sv);
	    str = SvPVX(sv);
	    if (datumtype == 'h') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits >>= 4;
		    else
			bits = *s++;
		    *str++ = PL_hexdigit[bits & 15];
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits <<= 4;
		    else
			bits = *s++;
		    *str++ = PL_hexdigit[(bits >> 4) & 15];
		}
	    }
	    *str = '\0';
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'c':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
		while (len-- > 0) {
		    aint = *s++;
		    if (aint >= 128)	/* fake up signed chars */
			aint -= 256;
		    if (checksum > bits_in_uv)
			cdouble += (NV)aint;
		    else
			cuv += aint;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    aint = *s++;
		    if (aint >= 128)	/* fake up signed chars */
			aint -= 256;
		    sv = NEWSV(36, 0);
		    sv_setiv(sv, (IV)aint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'C':
	unpack_C: /* unpack U will jump here if not UTF-8 */
            if (len == 0) {
                symptr->flags &= ~FLAG_UNPACK_DO_UTF8;
		break;
	    }
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
	      uchar_checksum:
		while (len-- > 0) {
		    auint = *s++ & 255;
		    cuv += auint;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    auint = *s++ & 255;
		    sv = NEWSV(37, 0);
		    sv_setiv(sv, (IV)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'U':
	    if (len == 0) {
                symptr->flags |= FLAG_UNPACK_DO_UTF8;
		break;
	    }
	    if ((symptr->flags & FLAG_UNPACK_DO_UTF8) == 0)
		 goto unpack_C;
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
		while (len-- > 0 && s < strend) {
		    STRLEN alen;
		    auint = NATIVE_TO_UNI(utf8n_to_uvchr((U8*)s, strend - s, &alen, ckWARN(WARN_UTF8) ? 0 : UTF8_ALLOW_ANYUV));
		    along = alen;
		    s += along;
		    if (checksum > bits_in_uv)
			cdouble += (NV)auint;
		    else
			cuv += auint;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0 && s < strend) {
		    STRLEN alen;
		    auint = NATIVE_TO_UNI(utf8n_to_uvchr((U8*)s, strend - s, &alen, ckWARN(WARN_UTF8) ? 0 : UTF8_ALLOW_ANYUV));
		    along = alen;
		    s += along;
		    sv = NEWSV(37, 0);
		    sv_setuv(sv, (UV)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 's' | TYPE_IS_SHRIEKING:
#if SHORTSIZE != SIZE16
	    along = (strend - s) / sizeof(short);
	    if (len > along)
		len = along;
	    if (checksum) {
		short ashort;
		while (len-- > 0) {
		     COPYNN(s, &ashort, sizeof(short));
		      s += sizeof(short);
		      if (checksum > bits_in_uv)
			  cdouble += (NV)ashort;
		      else
			  cuv += ashort;

		}
	    }
	    else {
		short ashort;
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    COPYNN(s, &ashort, sizeof(short));
		    s += sizeof(short);
		    sv = NEWSV(38, 0);
		    sv_setiv(sv, (IV)ashort);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
#else
	    /* Fallthrough! */
#endif
	case 's':
	    along = (strend - s) / SIZE16;
	    if (len > along)
		len = along;
	    if (checksum) {
      		while (len-- > 0) {
		    COPY16(s, &ashort);
#if SHORTSIZE > SIZE16
		    if (ashort > 32767)
			ashort -= 65536;
#endif
		    s += SIZE16;
		    if (checksum > bits_in_uv)
			cdouble += (NV)ashort;
		    else
			cuv += ashort;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);

		while (len-- > 0) {
		    COPY16(s, &ashort);
#if SHORTSIZE > SIZE16
		    if (ashort > 32767)
			ashort -= 65536;
#endif
		    s += SIZE16;
		    sv = NEWSV(38, 0);
		    sv_setiv(sv, (IV)ashort);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'S' | TYPE_IS_SHRIEKING:
#if SHORTSIZE != SIZE16
	    along = (strend - s) / sizeof(unsigned short);
	    if (len > along)
		len = along;
	    if (checksum) {
		unsigned short aushort;
		while (len-- > 0) {
		    COPYNN(s, &aushort, sizeof(unsigned short));
		    s += sizeof(unsigned short);
		    if (checksum > bits_in_uv)
			cdouble += (NV)aushort;
		    else
			cuv += aushort;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
  		    unsigned short aushort;
		    COPYNN(s, &aushort, sizeof(unsigned short));
		    s += sizeof(unsigned short);
		    sv = NEWSV(39, 0);
		    sv_setiv(sv, (UV)aushort);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
#else
            /* Fallhrough! */
#endif
	case 'v':
	case 'n':
	case 'S':
	    along = (strend - s) / SIZE16;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    COPY16(s, &aushort);
		    s += SIZE16;
#ifdef HAS_NTOHS
		    if (datumtype == 'n')
		        aushort = PerlSock_ntohs(aushort);
#endif
#ifdef HAS_VTOHS
		    if (datumtype == 'v')
			aushort = vtohs(aushort);
#endif
		    if (checksum > bits_in_uv)
			cdouble += (NV)aushort;
		    else
		        cuv += aushort;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    COPY16(s, &aushort);
		    s += SIZE16;
		    sv = NEWSV(39, 0);
#ifdef HAS_NTOHS
		    if (datumtype == 'n')
			aushort = PerlSock_ntohs(aushort);
#endif
#ifdef HAS_VTOHS
		    if (datumtype == 'v')
			aushort = vtohs(aushort);
#endif
		    sv_setiv(sv, (UV)aushort);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'i':
	case 'i' | TYPE_IS_SHRIEKING:
	    along = (strend - s) / sizeof(int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &aint, 1, int);
		    s += sizeof(int);
		    if (checksum > bits_in_uv)
			cdouble += (NV)aint;
		    else
			cuv += aint;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &aint, 1, int);
		    s += sizeof(int);
		    sv = NEWSV(40, 0);
#ifdef __osf__
                    /* Without the dummy below unpack("i", pack("i",-1))
                     * return 0xFFffFFff instead of -1 for Digital Unix V4.0
                     * cc with optimization turned on.
		     *
		     * The bug was detected in
		     * DEC C V5.8-009 on Digital UNIX V4.0 (Rev. 1091) (V4.0E)
		     * with optimization (-O4) turned on.
		     * DEC C V5.2-040 on Digital UNIX V4.0 (Rev. 564) (V4.0B)
		     * does not have this problem even with -O4.
		     *
		     * This bug was reported as DECC_BUGS 1431
		     * and tracked internally as GEM_BUGS 7775.
		     *
		     * The bug is fixed in
		     * Tru64 UNIX V5.0:      Compaq C V6.1-006 or later
		     * UNIX V4.0F support:   DEC C V5.9-006 or later
		     * UNIX V4.0E support:   DEC C V5.8-011 or later
		     * and also in DTK.
		     *
		     * See also few lines later for the same bug.
		     */
                    (aint) ?
		    	sv_setiv(sv, (IV)aint) :
#endif
		    sv_setiv(sv, (IV)aint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'I':
	case 'I' | TYPE_IS_SHRIEKING:
	    along = (strend - s) / sizeof(unsigned int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &auint, 1, unsigned int);
		    s += sizeof(unsigned int);
		    if (checksum > bits_in_uv)
			cdouble += (NV)auint;
		    else
			cuv += auint;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &auint, 1, unsigned int);
		    s += sizeof(unsigned int);
		    sv = NEWSV(41, 0);
#ifdef __osf__
                    /* Without the dummy below unpack("I", pack("I",0xFFFFFFFF))
                     * returns 1.84467440737096e+19 instead of 0xFFFFFFFF.
		     * See details few lines earlier. */
                    (auint) ?
		        sv_setuv(sv, (UV)auint) :
#endif
		    sv_setuv(sv, (UV)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'j':
	    along = (strend - s) / IVSIZE;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &aiv, 1, IV);
		    s += IVSIZE;
		    if (checksum > bits_in_uv)
			cdouble += (NV)aiv;
		    else
			cuv += aiv;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &aiv, 1, IV);
		    s += IVSIZE;
		    sv = NEWSV(40, 0);
		    sv_setiv(sv, aiv);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'J':
	    along = (strend - s) / UVSIZE;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &auv, 1, UV);
		    s += UVSIZE;
		    if (checksum > bits_in_uv)
			cdouble += (NV)auv;
		    else
			cuv += auv;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &auv, 1, UV);
		    s += UVSIZE;
		    sv = NEWSV(41, 0);
		    sv_setuv(sv, auv);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'l' | TYPE_IS_SHRIEKING:
#if LONGSIZE != SIZE32
	    along = (strend - s) / sizeof(long);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    COPYNN(s, &along, sizeof(long));
		    s += sizeof(long);
		    if (checksum > bits_in_uv)
			cdouble += (NV)along;
		    else
			cuv += along;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    COPYNN(s, &along, sizeof(long));
		    s += sizeof(long);
		    sv = NEWSV(42, 0);
		    sv_setiv(sv, (IV)along);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
#else
	    /* Fallthrough! */
#endif
	case 'l':
	    along = (strend - s) / SIZE32;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
#if LONGSIZE > SIZE32 && INTSIZE == SIZE32
		    I32 along;
#endif
		    COPY32(s, &along);
#if LONGSIZE > SIZE32
		    if (along > 2147483647)
		        along -= 4294967296;
#endif
		    s += SIZE32;
		    if (checksum > bits_in_uv)
			cdouble += (NV)along;
		    else
			cuv += along;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
#if LONGSIZE > SIZE32 && INTSIZE == SIZE32
		    I32 along;
#endif
		    COPY32(s, &along);
#if LONGSIZE > SIZE32
		    if (along > 2147483647)
		        along -= 4294967296;
#endif
		    s += SIZE32;
		    sv = NEWSV(42, 0);
		    sv_setiv(sv, (IV)along);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'L' | TYPE_IS_SHRIEKING:
#if LONGSIZE != SIZE32
	    along = (strend - s) / sizeof(unsigned long);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    unsigned long aulong;
		    COPYNN(s, &aulong, sizeof(unsigned long));
		    s += sizeof(unsigned long);
		    if (checksum > bits_in_uv)
			cdouble += (NV)aulong;
		    else
			cuv += aulong;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    unsigned long aulong;
		    COPYNN(s, &aulong, sizeof(unsigned long));
		    s += sizeof(unsigned long);
		    sv = NEWSV(43, 0);
		    sv_setuv(sv, (UV)aulong);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
#else
            /* Fall through! */
#endif
	case 'V':
	case 'N':
	case 'L':
	    along = (strend - s) / SIZE32;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    COPY32(s, &aulong);
		    s += SIZE32;
#ifdef HAS_NTOHL
		    if (datumtype == 'N')
			aulong = PerlSock_ntohl(aulong);
#endif
#ifdef HAS_VTOHL
		    if (datumtype == 'V')
			aulong = vtohl(aulong);
#endif
		    if (checksum > bits_in_uv)
			cdouble += (NV)aulong;
		    else
			cuv += aulong;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    COPY32(s, &aulong);
		    s += SIZE32;
#ifdef HAS_NTOHL
		    if (datumtype == 'N')
			aulong = PerlSock_ntohl(aulong);
#endif
#ifdef HAS_VTOHL
		    if (datumtype == 'V')
			aulong = vtohl(aulong);
#endif
		    sv = NEWSV(43, 0);
		    sv_setuv(sv, (UV)aulong);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'p':
	    along = (strend - s) / sizeof(char*);
	    if (len > along)
		len = along;
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    while (len-- > 0) {
		if (sizeof(char*) > strend - s)
		    break;
		else {
		    Copy(s, &aptr, 1, char*);
		    s += sizeof(char*);
		}
		sv = NEWSV(44, 0);
		if (aptr)
		    sv_setpv(sv, aptr);
		PUSHs(sv_2mortal(sv));
	    }
	    break;
	case 'w':
            if (len && unpack_only_one)
                len = 1;
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    {
		UV auv = 0;
		U32 bytes = 0;
		
		while ((len > 0) && (s < strend)) {
		    auv = (auv << 7) | (*s & 0x7f);
		    /* UTF8_IS_XXXXX not right here - using constant 0x80 */
		    if ((U8)(*s++) < 0x80) {
			bytes = 0;
			sv = NEWSV(40, 0);
			sv_setuv(sv, auv);
			PUSHs(sv_2mortal(sv));
			len--;
			auv = 0;
		    }
		    else if (++bytes >= sizeof(UV)) {	/* promote to string */
			char *t;
			STRLEN n_a;

			sv = Perl_newSVpvf(aTHX_ "%.*"UVf, (int)TYPE_DIGITS(UV), auv);
			while (s < strend) {
			    sv = mul128(sv, (U8)(*s & 0x7f));
			    if (!(*s++ & 0x80)) {
				bytes = 0;
				break;
			    }
			}
			t = SvPV(sv, n_a);
			while (*t == '0')
			    t++;
			sv_chop(sv, t);
			PUSHs(sv_2mortal(sv));
			len--;
			auv = 0;
		    }
		}
		if ((s >= strend) && bytes)
		    Perl_croak(aTHX_ "Unterminated compressed integer in unpack");
	    }
	    break;
	case 'P':
	    if (symptr->howlen == e_star)
	        Perl_croak(aTHX_ "'P' must have an explicit size in unpack");
	    EXTEND(SP, 1);
	    if (sizeof(char*) > strend - s)
		break;
	    else {
		Copy(s, &aptr, 1, char*);
		s += sizeof(char*);
	    }
	    sv = NEWSV(44, 0);
	    if (aptr)
		sv_setpvn(sv, aptr, len);
	    PUSHs(sv_2mortal(sv));
	    break;
#ifdef HAS_QUAD
	case 'q':
	    along = (strend - s) / sizeof(Quad_t);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &aquad, 1, Quad_t);
		    s += sizeof(Quad_t);
		    if (checksum > bits_in_uv)
			cdouble += (NV)aquad;
		    else
			cuv += aquad;
		}
	    }
            else {
                if (len && unpack_only_one)
                    len = 1;
                EXTEND(SP, len);
                EXTEND_MORTAL(len);
                while (len-- > 0) {
                    if (s + sizeof(Quad_t) > strend)
                        aquad = 0;
                    else {
		        Copy(s, &aquad, 1, Quad_t);
		        s += sizeof(Quad_t);
                    }
                    sv = NEWSV(42, 0);
                    if (aquad >= IV_MIN && aquad <= IV_MAX)
		        sv_setiv(sv, (IV)aquad);
                    else
                        sv_setnv(sv, (NV)aquad);
                    PUSHs(sv_2mortal(sv));
                }
            }
	    break;
	case 'Q':
	    along = (strend - s) / sizeof(Uquad_t);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &auquad, 1, Uquad_t);
		    s += sizeof(Uquad_t);
		    if (checksum > bits_in_uv)
			cdouble += (NV)auquad;
		    else
			cuv += auquad;
		}
	    }
            else {
                if (len && unpack_only_one)
                    len = 1;
                EXTEND(SP, len);
                EXTEND_MORTAL(len);
                while (len-- > 0) {
                    if (s + sizeof(Uquad_t) > strend)
                        auquad = 0;
                    else {
                        Copy(s, &auquad, 1, Uquad_t);
                        s += sizeof(Uquad_t);
                    }
                    sv = NEWSV(43, 0);
                    if (auquad <= UV_MAX)
                        sv_setuv(sv, (UV)auquad);
                    else
		    sv_setnv(sv, (NV)auquad);
                    PUSHs(sv_2mortal(sv));
                }
            }
	    break;
#endif
	/* float and double added gnb@melba.bby.oz.au 22/11/89 */
	case 'f':
	    along = (strend - s) / sizeof(float);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &afloat, 1, float);
		    s += sizeof(float);
		    cdouble += afloat;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &afloat, 1, float);
		    s += sizeof(float);
		    sv = NEWSV(47, 0);
		    sv_setnv(sv, (NV)afloat);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'd':
	    along = (strend - s) / sizeof(double);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &adouble, 1, double);
		    s += sizeof(double);
		    cdouble += adouble;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &adouble, 1, double);
		    s += sizeof(double);
		    sv = NEWSV(48, 0);
		    sv_setnv(sv, (NV)adouble);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'F':
	    along = (strend - s) / NVSIZE;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &anv, 1, NV);
		    s += NVSIZE;
		    cdouble += anv;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &anv, 1, NV);
		    s += NVSIZE;
		    sv = NEWSV(48, 0);
		    sv_setnv(sv, anv);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
#if defined(HAS_LONG_DOUBLE) && defined(USE_LONG_DOUBLE)
	case 'D':
	    along = (strend - s) / LONG_DOUBLESIZE;
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &aldouble, 1, long double);
		    s += LONG_DOUBLESIZE;
		    cdouble += aldouble;
		}
	    }
	    else {
                if (len && unpack_only_one)
                    len = 1;
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &aldouble, 1, long double);
		    s += LONG_DOUBLESIZE;
		    sv = NEWSV(48, 0);
		    sv_setnv(sv, (NV)aldouble);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
#endif
	case 'u':
	    /* MKS:
	     * Initialise the decode mapping.  By using a table driven
             * algorithm, the code will be character-set independent
             * (and just as fast as doing character arithmetic)
             */
            if (PL_uudmap['M'] == 0) {
                int i;

                for (i = 0; i < sizeof(PL_uuemap); i += 1)
                    PL_uudmap[(U8)PL_uuemap[i]] = i;
                /*
                 * Because ' ' and '`' map to the same value,
                 * we need to decode them both the same.
                 */
                PL_uudmap[' '] = 0;
            }

	    along = (strend - s) * 3 / 4;
	    sv = NEWSV(42, along);
	    if (along)
		SvPOK_on(sv);
	    while (s < strend && *s > ' ' && ISUUCHAR(*s)) {
		I32 a, b, c, d;
		char hunk[4];

		hunk[3] = '\0';
		len = PL_uudmap[*(U8*)s++] & 077;
		while (len > 0) {
		    if (s < strend && ISUUCHAR(*s))
			a = PL_uudmap[*(U8*)s++] & 077;
 		    else
 			a = 0;
		    if (s < strend && ISUUCHAR(*s))
			b = PL_uudmap[*(U8*)s++] & 077;
 		    else
 			b = 0;
		    if (s < strend && ISUUCHAR(*s))
			c = PL_uudmap[*(U8*)s++] & 077;
 		    else
 			c = 0;
		    if (s < strend && ISUUCHAR(*s))
			d = PL_uudmap[*(U8*)s++] & 077;
		    else
			d = 0;
		    hunk[0] = (char)((a << 2) | (b >> 4));
		    hunk[1] = (char)((b << 4) | (c >> 2));
		    hunk[2] = (char)((c << 6) | d);
		    sv_catpvn(sv, hunk, (len > 3) ? 3 : len);
		    len -= 3;
		}
		if (*s == '\n')
		    s++;
		else	/* possible checksum byte */
		    if (s + 1 < strend && s[1] == '\n')
		        s += 2;
	    }
	    XPUSHs(sv_2mortal(sv));
	    break;
	}

	if (checksum) {
	    sv = NEWSV(42, 0);
	    if (strchr("fFdD", datumtype) ||
	      (checksum > bits_in_uv &&
	       strchr("csSiIlLnNUvVqQjJ", datumtype&0xFF)) ) {
		NV trouble;

                adouble = (NV) (1 << (checksum & 15));
		while (checksum >= 16) {
		    checksum -= 16;
		    adouble *= 65536.0;
		}
		while (cdouble < 0.0)
		    cdouble += adouble;
		cdouble = Perl_modf(cdouble / adouble, &trouble) * adouble;
		sv_setnv(sv, cdouble);
	    }
	    else {
		if (checksum < bits_in_uv) {
		    UV mask = ((UV)1 << checksum) - 1;
		    cuv &= mask;
		}
		sv_setuv(sv, cuv);
	    }
	    XPUSHs(sv_2mortal(sv));
	    checksum = 0;
	}
    
        if (symptr->flags & FLAG_SLASH){
            if (SP - PL_stack_base - start_sp_offset <= 0)
                Perl_croak(aTHX_ "'/' must follow a numeric type in unpack");
            if( next_symbol(symptr) ){
              if( symptr->howlen == e_number )
		Perl_croak(aTHX_ "Count after length/code in unpack" );
              if( beyond ){
         	/* ...end of char buffer then no decent length available */
		Perl_croak(aTHX_ "length/code after end of string in unpack" );
              } else {
         	/* take top of stack (hope it's numeric) */
                len = POPi;
                if( len < 0 )
                    Perl_croak(aTHX_ "Negative '/' count in unpack" );
              }
            } else {
		Perl_croak(aTHX_ "Code missing after '/' in unpack" );
            }
            datumtype = symptr->code;
	    goto redo_switch;
        }
    }

    if (new_s)
	*new_s = s;
    PUTBACK;
    return SP - PL_stack_base - start_sp_offset;
}

PP(pp_unpack)
{
    dSP;
    dPOPPOPssrl;
    I32 gimme = GIMME_V;
    STRLEN llen;
    STRLEN rlen;
    register char *pat = SvPV(left, llen);
#ifdef PACKED_IS_OCTETS
    /* Packed side is assumed to be octets - so force downgrade if it
       has been UTF-8 encoded by accident
     */
    register char *s = SvPVbyte(right, rlen);
#else
    register char *s = SvPV(right, rlen);
#endif
    char *strend = s + rlen;
    register char *patend = pat + llen;
    register I32 cnt;

    PUTBACK;
    cnt = unpackstring(pat, patend, s, strend,
		     ((gimme == G_SCALAR) ? FLAG_UNPACK_ONLY_ONE : 0)
		     | (DO_UTF8(right) ? FLAG_UNPACK_DO_UTF8 : 0));

    SPAGAIN;
    if ( !cnt && gimme == G_SCALAR )
       PUSHs(&PL_sv_undef);
    RETURN;
}

STATIC void
S_doencodes(pTHX_ register SV *sv, register char *s, register I32 len)
{
    char hunk[5];

    *hunk = PL_uuemap[len];
    sv_catpvn(sv, hunk, 1);
    hunk[4] = '\0';
    while (len > 2) {
	hunk[0] = PL_uuemap[(077 & (*s >> 2))];
	hunk[1] = PL_uuemap[(077 & (((*s << 4) & 060) | ((s[1] >> 4) & 017)))];
	hunk[2] = PL_uuemap[(077 & (((s[1] << 2) & 074) | ((s[2] >> 6) & 03)))];
	hunk[3] = PL_uuemap[(077 & (s[2] & 077))];
	sv_catpvn(sv, hunk, 4);
	s += 3;
	len -= 3;
    }
    if (len > 0) {
	char r = (len > 1 ? s[1] : '\0');
	hunk[0] = PL_uuemap[(077 & (*s >> 2))];
	hunk[1] = PL_uuemap[(077 & (((*s << 4) & 060) | ((r >> 4) & 017)))];
	hunk[2] = PL_uuemap[(077 & ((r << 2) & 074))];
	hunk[3] = PL_uuemap[0];
	sv_catpvn(sv, hunk, 4);
    }
    sv_catpvn(sv, "\n", 1);
}

STATIC SV *
S_is_an_int(pTHX_ char *s, STRLEN l)
{
  STRLEN	 n_a;
  SV             *result = newSVpvn(s, l);
  char           *result_c = SvPV(result, n_a);	/* convenience */
  char           *out = result_c;
  bool            skip = 1;
  bool            ignore = 0;

  while (*s) {
    switch (*s) {
    case ' ':
      break;
    case '+':
      if (!skip) {
	SvREFCNT_dec(result);
	return (NULL);
      }
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      skip = 0;
      if (!ignore) {
	*(out++) = *s;
      }
      break;
    case '.':
      ignore = 1;
      break;
    default:
      SvREFCNT_dec(result);
      return (NULL);
    }
    s++;
  }
  *(out++) = '\0';
  SvCUR_set(result, out - result_c);
  return (result);
}

/* pnum must be '\0' terminated */
STATIC int
S_div128(pTHX_ SV *pnum, bool *done)
{
  STRLEN          len;
  char           *s = SvPV(pnum, len);
  int             m = 0;
  int             r = 0;
  char           *t = s;

  *done = 1;
  while (*t) {
    int             i;

    i = m * 10 + (*t - '0');
    m = i & 0x7F;
    r = (i >> 7);		/* r < 10 */
    if (r) {
      *done = 0;
    }
    *(t++) = '0' + r;
  }
  *(t++) = '\0';
  SvCUR_set(pnum, (STRLEN) (t - s));
  return (m);
}



/*
=for apidoc pack_cat

The engine implementing pack() Perl function. Note: parameters next_in_list and
flags are not used. This call should not be used; use packlist instead.

=cut */


void
Perl_pack_cat(pTHX_ SV *cat, char *pat, register char *patend, register SV **beglist, SV **endlist, SV ***next_in_list, U32 flags)
{
    tempsym_t sym = { 0 };
    sym.patptr = pat;
    sym.patend = patend;
    sym.flags  = FLAG_PACK;

    (void)pack_rec( cat, &sym, beglist, endlist );
}


/*
=for apidoc packlist

The engine implementing pack() Perl function.

=cut */


void
Perl_packlist(pTHX_ SV *cat, char *pat, register char *patend, register SV **beglist, SV **endlist )
{
    tempsym_t sym = { 0 };
    sym.patptr = pat;
    sym.patend = patend;
    sym.flags  = FLAG_PACK;

    (void)pack_rec( cat, &sym, beglist, endlist );
}


STATIC
SV **
S_pack_rec(pTHX_ SV *cat, register tempsym_t* symptr, register SV **beglist, SV **endlist )
{
    register I32 items;
    STRLEN fromlen;
    register I32 len = 0;
    SV *fromstr;
    /*SUPPRESS 442*/
    static char null10[] = {0,0,0,0,0,0,0,0,0,0};
    static char *space10 = "          ";
    bool found;

    /* These must not be in registers: */
    char achar;
    I16 ashort;
    int aint;
    unsigned int auint;
    I32 along;
    U32 aulong;
    IV aiv;
    UV auv;
    NV anv;
#if defined(HAS_LONG_DOUBLE) && defined(USE_LONG_DOUBLE)
    long double aldouble;
#endif
#ifdef HAS_QUAD
    Quad_t aquad;
    Uquad_t auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;
    int strrelbeg = SvCUR(cat);
    tempsym_t lookahead;

    items = endlist - beglist;
    found = next_symbol( symptr );

#ifndef PACKED_IS_OCTETS
    if (symptr->level == 0 && found && symptr->code == 'U' ){
	SvUTF8_on(cat);
    }
#endif

    while (found) {
	SV *lengthcode = Nullsv;
#define NEXTFROM ( lengthcode ? lengthcode : items-- > 0 ? *beglist++ : &PL_sv_no)

        I32 datumtype = symptr->code;
        howlen_t howlen;

        switch( howlen = symptr->howlen ){
        case e_no_len:
	case e_number:
	    len = symptr->length;
	    break;
        case e_star:
	    len = strchr("@Xxu", datumtype) ? 0 : items; 
	    break;
        }

        /* Look ahead for next symbol. Do we have code/code? */
        lookahead = *symptr;
        found = next_symbol(&lookahead);
	if ( symptr->flags & FLAG_SLASH ) {
	    if (found){
 	        if ( 0 == strchr( "aAZ", lookahead.code ) ||
                     e_star != lookahead.howlen )
 		    Perl_croak(aTHX_ "'/' must be followed by 'a*', 'A*' or 'Z*' in pack");
	        lengthcode = sv_2mortal(newSViv(sv_len(items > 0
						   ? *beglist : &PL_sv_no)
                                           + (lookahead.code == 'Z' ? 1 : 0)));
	    } else {
 		Perl_croak(aTHX_ "Code missing after '/' in pack");
            }
	}

	switch(datumtype) {
	default:
	    Perl_croak(aTHX_ "Invalid type '%c' in pack", (int)datumtype);
	case '%':
	    Perl_croak(aTHX_ "'%%' may not be used in pack");
	case '@':
	    len += strrelbeg - SvCUR(cat);
	    if (len > 0)
		goto grow;
	    len = -len;
	    if (len > 0)
		goto shrink;
	    break;
	case '(':
	{
            tempsym_t savsym = *symptr;
            symptr->patend = savsym.grpend;
            symptr->level++;
	    while (len--) {
  	        symptr->patptr = savsym.grpbeg;
		beglist = pack_rec(cat, symptr, beglist, endlist );
		if (savsym.howlen == e_star && beglist == endlist)
		    break;		/* No way to continue */
	    }
            lookahead.flags = symptr->flags;
            *symptr = savsym;
	    break;
	}
	case 'X' | TYPE_IS_SHRIEKING:
	    if (!len)			/* Avoid division by 0 */
		len = 1;
	    len = (SvCUR(cat)) % len;
	    /* FALL THROUGH */
	case 'X':
	  shrink:
	    if ((I32)SvCUR(cat) < len)
		Perl_croak(aTHX_ "'X' outside of string in pack");
	    SvCUR(cat) -= len;
	    *SvEND(cat) = '\0';
	    break;
	case 'x' | TYPE_IS_SHRIEKING:
	    if (!len)			/* Avoid division by 0 */
		len = 1;
	    aint = (SvCUR(cat)) % len;
	    if (aint)			/* Other portable ways? */
		len = len - aint;
	    else
		len = 0;
	    /* FALL THROUGH */

	case 'x':
	  grow:
	    while (len >= 10) {
		sv_catpvn(cat, null10, 10);
		len -= 10;
	    }
	    sv_catpvn(cat, null10, len);
	    break;
	case 'A':
	case 'Z':
	case 'a':
	    fromstr = NEXTFROM;
	    aptr = SvPV(fromstr, fromlen);
	    if (howlen == e_star) {   
		len = fromlen;
		if (datumtype == 'Z')
		    ++len;
	    }
	    if ((I32)fromlen >= len) {
		sv_catpvn(cat, aptr, len);
		if (datumtype == 'Z')
		    *(SvEND(cat)-1) = '\0';
	    }
	    else {
		sv_catpvn(cat, aptr, fromlen);
		len -= fromlen;
		if (datumtype == 'A') {
		    while (len >= 10) {
			sv_catpvn(cat, space10, 10);
			len -= 10;
		    }
		    sv_catpvn(cat, space10, len);
		}
		else {
		    while (len >= 10) {
			sv_catpvn(cat, null10, 10);
			len -= 10;
		    }
		    sv_catpvn(cat, null10, len);
		}
	    }
	    break;
	case 'B':
	case 'b':
	    {
		register char *str;
		I32 saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		str = SvPV(fromstr, fromlen);
		if (howlen == e_star)
		    len = fromlen;
		aint = SvCUR(cat);
		SvCUR(cat) += (len+7)/8;
		SvGROW(cat, SvCUR(cat) + 1);
		aptr = SvPVX(cat) + aint;
		if (len > (I32)fromlen)
		    len = fromlen;
		aint = len;
		items = 0;
		if (datumtype == 'B') {
		    for (len = 0; len++ < aint;) {
			items |= *str++ & 1;
			if (len & 7)
			    items <<= 1;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		else {
		    for (len = 0; len++ < aint;) {
			if (*str++ & 1)
			    items |= 128;
			if (len & 7)
			    items >>= 1;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		if (aint & 7) {
		    if (datumtype == 'B')
			items <<= 7 - (aint & 7);
		    else
			items >>= 7 - (aint & 7);
		    *aptr++ = items & 0xff;
		}
		str = SvPVX(cat) + SvCUR(cat);
		while (aptr <= str)
		    *aptr++ = '\0';

		items = saveitems;
	    }
	    break;
	case 'H':
	case 'h':
	    {
		register char *str;
		I32 saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		str = SvPV(fromstr, fromlen);
		if (howlen == e_star)
		    len = fromlen;
		aint = SvCUR(cat);
		SvCUR(cat) += (len+1)/2;
		SvGROW(cat, SvCUR(cat) + 1);
		aptr = SvPVX(cat) + aint;
		if (len > (I32)fromlen)
		    len = fromlen;
		aint = len;
		items = 0;
		if (datumtype == 'H') {
		    for (len = 0; len++ < aint;) {
			if (isALPHA(*str))
			    items |= ((*str++ & 15) + 9) & 15;
			else
			    items |= *str++ & 15;
			if (len & 1)
			    items <<= 4;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		else {
		    for (len = 0; len++ < aint;) {
			if (isALPHA(*str))
			    items |= (((*str++ & 15) + 9) & 15) << 4;
			else
			    items |= (*str++ & 15) << 4;
			if (len & 1)
			    items >>= 4;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		if (aint & 1)
		    *aptr++ = items & 0xff;
		str = SvPVX(cat) + SvCUR(cat);
		while (aptr <= str)
		    *aptr++ = '\0';

		items = saveitems;
	    }
	    break;
	case 'C':
	case 'c':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		switch (datumtype) {
		case 'C':
		    aint = SvIV(fromstr);
		    if ((aint < 0 || aint > 255) &&
			ckWARN(WARN_PACK))
		        Perl_warner(aTHX_ packWARN(WARN_PACK),
				    "Character in 'C' format wrapped in pack");
		    achar = aint & 255;
		    sv_catpvn(cat, &achar, sizeof(char));
		    break;
		case 'c':
		    aint = SvIV(fromstr);
		    if ((aint < -128 || aint > 127) &&
			ckWARN(WARN_PACK))
		        Perl_warner(aTHX_ packWARN(WARN_PACK),
				    "Character in 'c' format wrapped in pack" );
		    achar = aint & 255;
		    sv_catpvn(cat, &achar, sizeof(char));
		    break;
		}
	    }
	    break;
	case 'U':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auint = UNI_TO_NATIVE(SvUV(fromstr));
		SvGROW(cat, SvCUR(cat) + UTF8_MAXLEN + 1);
		SvCUR_set(cat,
			  (char*)uvchr_to_utf8_flags((U8*)SvEND(cat),
						     auint,
						     ckWARN(WARN_UTF8) ?
						     0 : UNICODE_ALLOW_ANY)
			  - SvPVX(cat));
	    }
	    *SvEND(cat) = '\0';
	    break;
	/* Float and double added by gnb@melba.bby.oz.au  22/11/89 */
	case 'f':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
#ifdef __VOS__
/* VOS does not automatically map a floating-point overflow
   during conversion from double to float into infinity, so we
   do it by hand.  This code should either be generalized for
   any OS that needs it, or removed if and when VOS implements
   posix-976 (suggestion to support mapping to infinity).
   Paul.Green@stratus.com 02-04-02.  */
		if (SvNV(fromstr) > FLT_MAX)
		     afloat = _float_constants[0];   /* single prec. inf. */
		else if (SvNV(fromstr) < -FLT_MAX)
		     afloat = _float_constants[0];   /* single prec. inf. */
		else afloat = (float)SvNV(fromstr);
#else
# if defined(VMS) && !defined(__IEEE_FP)
/* IEEE fp overflow shenanigans are unavailable on VAX and optional
 * on Alpha; fake it if we don't have them.
 */
		if (SvNV(fromstr) > FLT_MAX)
		     afloat = FLT_MAX;
		else if (SvNV(fromstr) < -FLT_MAX)
		     afloat = -FLT_MAX;
		else afloat = (float)SvNV(fromstr);
# else
		afloat = (float)SvNV(fromstr);
# endif
#endif
		sv_catpvn(cat, (char *)&afloat, sizeof (float));
	    }
	    break;
	case 'd':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
#ifdef __VOS__
/* VOS does not automatically map a floating-point overflow
   during conversion from long double to double into infinity,
   so we do it by hand.  This code should either be generalized
   for any OS that needs it, or removed if and when VOS
   implements posix-976 (suggestion to support mapping to
   infinity).  Paul.Green@stratus.com 02-04-02.  */
		if (SvNV(fromstr) > DBL_MAX)
		     adouble = _double_constants[0];   /* double prec. inf. */
		else if (SvNV(fromstr) < -DBL_MAX)
		     adouble = _double_constants[0];   /* double prec. inf. */
		else adouble = (double)SvNV(fromstr);
#else
# if defined(VMS) && !defined(__IEEE_FP)
/* IEEE fp overflow shenanigans are unavailable on VAX and optional
 * on Alpha; fake it if we don't have them.
 */
		if (SvNV(fromstr) > DBL_MAX)
		     adouble = DBL_MAX;
		else if (SvNV(fromstr) < -DBL_MAX)
		     adouble = -DBL_MAX;
		else adouble = (double)SvNV(fromstr);
# else
		adouble = (double)SvNV(fromstr);
# endif
#endif
		sv_catpvn(cat, (char *)&adouble, sizeof (double));
	    }
	    break;
	case 'F':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		anv = SvNV(fromstr);
		sv_catpvn(cat, (char *)&anv, NVSIZE);
	    }
	    break;
#if defined(HAS_LONG_DOUBLE) && defined(USE_LONG_DOUBLE)
	case 'D':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aldouble = (long double)SvNV(fromstr);
		sv_catpvn(cat, (char *)&aldouble, LONG_DOUBLESIZE);
	    }
	    break;
#endif
	case 'n':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
#ifdef HAS_HTONS
		ashort = PerlSock_htons(ashort);
#endif
		CAT16(cat, &ashort);
	    }
	    break;
	case 'v':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
#ifdef HAS_HTOVS
		ashort = htovs(ashort);
#endif
		CAT16(cat, &ashort);
	    }
	    break;
        case 'S' | TYPE_IS_SHRIEKING:
#if SHORTSIZE != SIZE16
	    {
		unsigned short aushort;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    aushort = SvUV(fromstr);
		    sv_catpvn(cat, (char *)&aushort, sizeof(unsigned short));
		}
            }
            break;
#else
            /* Fall through! */
#endif
	case 'S':
            {
		U16 aushort;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    aushort = (U16)SvUV(fromstr);
		    CAT16(cat, &aushort);
		}

	    }
	    break;
	case 's' | TYPE_IS_SHRIEKING:
#if SHORTSIZE != SIZE16
	    {
		short ashort;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    ashort = SvIV(fromstr);
		    sv_catpvn(cat, (char *)&ashort, sizeof(short));
		}
	    }
            break;
#else
            /* Fall through! */
#endif
	case 's':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
		CAT16(cat, &ashort);
	    }
	    break;
	case 'I':
	case 'I' | TYPE_IS_SHRIEKING:
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auint = SvUV(fromstr);
		sv_catpvn(cat, (char*)&auint, sizeof(unsigned int));
	    }
	    break;
	case 'j':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aiv = SvIV(fromstr);
		sv_catpvn(cat, (char*)&aiv, IVSIZE);
	    }
	    break;
	case 'J':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auv = SvUV(fromstr);
		sv_catpvn(cat, (char*)&auv, UVSIZE);
	    }
	    break;
	case 'w':
            while (len-- > 0) {
		fromstr = NEXTFROM;
		anv = SvNV(fromstr);

		if (anv < 0)
		    Perl_croak(aTHX_ "Cannot compress negative numbers in pack");

                /* 0xFFFFFFFFFFFFFFFF may cast to 18446744073709551616.0,
                   which is == UV_MAX_P1. IOK is fine (instead of UV_only), as
                   any negative IVs will have already been got by the croak()
                   above. IOK is untrue for fractions, so we test them
                   against UV_MAX_P1.  */
		if (SvIOK(fromstr) || anv < UV_MAX_P1)
		{
		    char   buf[(sizeof(UV)*8)/7+1];
		    char  *in = buf + sizeof(buf);
		    UV     auv = SvUV(fromstr);

		    do {
			*--in = (char)((auv & 0x7f) | 0x80);
			auv >>= 7;
		    } while (auv);
		    buf[sizeof(buf) - 1] &= 0x7f; /* clear continue bit */
		    sv_catpvn(cat, in, (buf + sizeof(buf)) - in);
		}
		else if (SvPOKp(fromstr)) {  /* decimal string arithmetics */
		    char           *from, *result, *in;
		    SV             *norm;
		    STRLEN          len;
		    bool            done;

		    /* Copy string and check for compliance */
		    from = SvPV(fromstr, len);
		    if ((norm = is_an_int(from, len)) == NULL)
			Perl_croak(aTHX_ "Can only compress unsigned integers in pack");

		    New('w', result, len, char);
		    in = result + len;
		    done = FALSE;
		    while (!done)
			*--in = div128(norm, &done) | 0x80;
		    result[len - 1] &= 0x7F; /* clear continue bit */
		    sv_catpvn(cat, in, (result + len) - in);
		    Safefree(result);
		    SvREFCNT_dec(norm);	/* free norm */
                }
		else if (SvNOKp(fromstr)) {
		    /* 10**NV_MAX_10_EXP is the largest power of 10
		       so 10**(NV_MAX_10_EXP+1) is definately unrepresentable
		       given 10**(NV_MAX_10_EXP+1) == 128 ** x solve for x:
		       x = (NV_MAX_10_EXP+1) * log (10) / log (128)
		       And with that many bytes only Inf can overflow.
		       Some C compilers are strict about integral constant
		       expressions so we conservatively divide by a slightly
		       smaller integer instead of multiplying by the exact
		       floating-point value.
		    */
#ifdef NV_MAX_10_EXP
/*		    char   buf[1 + (int)((NV_MAX_10_EXP + 1) * 0.47456)]; -- invalid C */
		    char   buf[1 + (int)((NV_MAX_10_EXP + 1) / 2)]; /* valid C */
#else
/*		    char   buf[1 + (int)((308 + 1) * 0.47456)]; -- invalid C */
		    char   buf[1 + (int)((308 + 1) / 2)]; /* valid C */
#endif
		    char  *in = buf + sizeof(buf);

                    anv = Perl_floor(anv);
		    do {
			NV next = Perl_floor(anv / 128);
			if (in <= buf)  /* this cannot happen ;-) */
			    Perl_croak(aTHX_ "Cannot compress integer in pack");
			*--in = (unsigned char)(anv - (next * 128)) | 0x80;
			anv = next;
		    } while (anv > 0);
		    buf[sizeof(buf) - 1] &= 0x7f; /* clear continue bit */
		    sv_catpvn(cat, in, (buf + sizeof(buf)) - in);
		}
		else {
		    char           *from, *result, *in;
		    SV             *norm;
		    STRLEN          len;
		    bool            done;

		    /* Copy string and check for compliance */
		    from = SvPV(fromstr, len);
		    if ((norm = is_an_int(from, len)) == NULL)
			Perl_croak(aTHX_ "Can only compress unsigned integers in pack");

		    New('w', result, len, char);
		    in = result + len;
		    done = FALSE;
		    while (!done)
			*--in = div128(norm, &done) | 0x80;
		    result[len - 1] &= 0x7F; /* clear continue bit */
		    sv_catpvn(cat, in, (result + len) - in);
		    Safefree(result);
		    SvREFCNT_dec(norm);	/* free norm */
               }
	    }
            break;
	case 'i':
	case 'i' | TYPE_IS_SHRIEKING:
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aint = SvIV(fromstr);
		sv_catpvn(cat, (char*)&aint, sizeof(int));
	    }
	    break;
	case 'N':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = SvUV(fromstr);
#ifdef HAS_HTONL
		aulong = PerlSock_htonl(aulong);
#endif
		CAT32(cat, &aulong);
	    }
	    break;
	case 'V':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = SvUV(fromstr);
#ifdef HAS_HTOVL
		aulong = htovl(aulong);
#endif
		CAT32(cat, &aulong);
	    }
	    break;
	case 'L' | TYPE_IS_SHRIEKING:
#if LONGSIZE != SIZE32
	    {
		unsigned long aulong;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    aulong = SvUV(fromstr);
		    sv_catpvn(cat, (char *)&aulong, sizeof(unsigned long));
		}
	    }
	    break;
#else
            /* Fall though! */
#endif
	case 'L':
            {
		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    aulong = SvUV(fromstr);
		    CAT32(cat, &aulong);
		}
	    }
	    break;
	case 'l' | TYPE_IS_SHRIEKING:
#if LONGSIZE != SIZE32
	    {
		long along;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    along = SvIV(fromstr);
		    sv_catpvn(cat, (char *)&along, sizeof(long));
		}
	    }
	    break;
#else
            /* Fall though! */
#endif
	case 'l':
            while (len-- > 0) {
		fromstr = NEXTFROM;
		along = SvIV(fromstr);
		CAT32(cat, &along);
	    }
	    break;
#ifdef HAS_QUAD
	case 'Q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auquad = (Uquad_t)SvUV(fromstr);
		sv_catpvn(cat, (char*)&auquad, sizeof(Uquad_t));
	    }
	    break;
	case 'q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aquad = (Quad_t)SvIV(fromstr);
		sv_catpvn(cat, (char*)&aquad, sizeof(Quad_t));
	    }
	    break;
#endif
	case 'P':
	    len = 1;		/* assume SV is correct length */
	    /* Fall through! */
	case 'p':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		if (fromstr == &PL_sv_undef)
		    aptr = NULL;
		else {
		    STRLEN n_a;
		    /* XXX better yet, could spirit away the string to
		     * a safe spot and hang on to it until the result
		     * of pack() (and all copies of the result) are
		     * gone.
		     */
		    if (ckWARN(WARN_PACK) && (SvTEMP(fromstr)
						|| (SvPADTMP(fromstr)
						    && !SvREADONLY(fromstr))))
		    {
			Perl_warner(aTHX_ packWARN(WARN_PACK),
				"Attempt to pack pointer to temporary value");
		    }
		    if (SvPOK(fromstr) || SvNIOK(fromstr))
			aptr = SvPV(fromstr,n_a);
		    else
			aptr = SvPV_force(fromstr,n_a);
		}
		sv_catpvn(cat, (char*)&aptr, sizeof(char*));
	    }
	    break;
	case 'u':
	    fromstr = NEXTFROM;
	    aptr = SvPV(fromstr, fromlen);
	    SvGROW(cat, fromlen * 4 / 3);
	    if (len <= 2)
		len = 45;
	    else
		len = len / 3 * 3;
	    while (fromlen > 0) {
		I32 todo;

		if ((I32)fromlen > len)
		    todo = len;
		else
		    todo = fromlen;
		doencodes(cat, aptr, todo);
		fromlen -= todo;
		aptr += todo;
	    }
	    break;
	}
	*symptr = lookahead;
    }
    return beglist;
}
#undef NEXTFROM


PP(pp_pack)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register SV *cat = TARG;
    STRLEN fromlen;
    register char *pat = SvPVx(*++MARK, fromlen);
    register char *patend = pat + fromlen;

    MARK++;
    sv_setpvn(cat, "", 0);

    packlist(cat, pat, patend, MARK, SP + 1);

    SvSETMAGIC(cat);
    SP = ORIGMARK;
    PUSHs(cat);
    RETURN;
}

