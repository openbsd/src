/*
 $Id: Encode.xs,v 2.43 2018/02/21 12:14:33 dankogai Exp dankogai $
 */

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "encode.h"
#include "def_t.h"

# define PERLIO_MODNAME  "PerlIO::encoding"
# define PERLIO_FILENAME "PerlIO/encoding.pm"

/* set 1 or more to profile.  t/encoding.t dumps core because of
   Perl_warner and PerlIO don't work well */
#define ENCODE_XS_PROFILE 0

/* set 0 to disable floating point to calculate buffer size for
   encode_method().  1 is recommended. 2 restores NI-S original */
#define ENCODE_XS_USEFP   1

#define UNIMPLEMENTED(x,y) static y x (SV *sv, char *encoding) {	\
			Perl_croak_nocontext("panic_unimplemented");	\
                        PERL_UNUSED_VAR(sv); \
                        PERL_UNUSED_VAR(encoding); \
             return (y)0; /* fool picky compilers */ \
                         }
/**/

UNIMPLEMENTED(_encoded_utf8_to_bytes, I32)
UNIMPLEMENTED(_encoded_bytes_to_utf8, I32)

#ifndef SvIV_nomg
#define SvIV_nomg SvIV
#endif

#ifndef UTF8_DISALLOW_ILLEGAL_INTERCHANGE
#  define UTF8_DISALLOW_ILLEGAL_INTERCHANGE 0
#  define UTF8_ALLOW_NON_STRICT (UTF8_ALLOW_FE_FF|UTF8_ALLOW_SURROGATE|UTF8_ALLOW_FFFF)
#else
#  define UTF8_ALLOW_NON_STRICT 0
#endif

static void
Encode_XSEncoding(pTHX_ encode_t * enc)
{
    dSP;
    HV *stash = gv_stashpv("Encode::XS", TRUE);
    SV *iv    = newSViv(PTR2IV(enc));
    SV *sv    = sv_bless(newRV_noinc(iv),stash);
    int i = 0;
    /* with the SvLEN() == 0 hack, PVX won't be freed. We cast away name's
    constness, in the hope that perl won't mess with it. */
    assert(SvTYPE(iv) >= SVt_PV); assert(SvLEN(iv) == 0);
    SvFLAGS(iv) |= SVp_POK;
    SvPVX(iv) = (char*) enc->name[0];
    PUSHMARK(sp);
    XPUSHs(sv);
    while (enc->name[i]) {
    const char *name = enc->name[i++];
    XPUSHs(sv_2mortal(newSVpvn(name, strlen(name))));
    }
    PUTBACK;
    call_pv("Encode::define_encoding", G_DISCARD);
    SvREFCNT_dec(sv);
}

static void
call_failure(SV * routine, U8 * done, U8 * dest, U8 * orig)
{
    /* Exists for breakpointing */
    PERL_UNUSED_VAR(routine);
    PERL_UNUSED_VAR(done);
    PERL_UNUSED_VAR(dest);
    PERL_UNUSED_VAR(orig);
}

static void
utf8_safe_downgrade(pTHX_ SV ** src, U8 ** s, STRLEN * slen, bool modify)
{
    if (!modify) {
        SV *tmp = sv_2mortal(newSVpvn((char *)*s, *slen));
        SvUTF8_on(tmp);
        if (SvTAINTED(*src))
            SvTAINTED_on(tmp);
        *src = tmp;
        *s = (U8 *)SvPVX(*src);
    }
    if (*slen) {
        if (!utf8_to_bytes(*s, slen))
            croak("Wide character");
        SvCUR_set(*src, *slen);
    }
    SvUTF8_off(*src);
}

static void
utf8_safe_upgrade(pTHX_ SV ** src, U8 ** s, STRLEN * slen, bool modify)
{
    if (!modify) {
        SV *tmp = sv_2mortal(newSVpvn((char *)*s, *slen));
        if (SvTAINTED(*src))
            SvTAINTED_on(tmp);
        *src = tmp;
    }
    sv_utf8_upgrade_nomg(*src);
    *s = (U8 *)SvPV_nomg(*src, *slen);
}

#define ERR_ENCODE_NOMAP "\"\\x{%04" UVxf "}\" does not map to %s"
#define ERR_DECODE_NOMAP "%s \"\\x%02" UVXf "\" does not map to Unicode"
#define ERR_DECODE_STR_NOMAP "%s \"%s\" does not map to Unicode"

static SV *
do_fallback_cb(pTHX_ UV ch, SV *fallback_cb)
{
    dSP;
    int argc;
    SV *retval;
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSVuv(ch)));
    PUTBACK;
    argc = call_sv(fallback_cb, G_SCALAR);
    SPAGAIN;
    if (argc != 1){
	croak("fallback sub must return scalar!");
    }
    retval = POPs;
    SvREFCNT_inc(retval);
    PUTBACK;
    FREETMPS;
    LEAVE;
    return retval;
}

static SV *
do_bytes_fallback_cb(pTHX_ U8 *s, STRLEN slen, SV *fallback_cb)
{
    dSP;
    int argc;
    STRLEN i;
    SV *retval;
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    for (i=0; i<slen; ++i)
        XPUSHs(sv_2mortal(newSVuv(s[i])));
    PUTBACK;
    argc = call_sv(fallback_cb, G_SCALAR);
    SPAGAIN;
    if (argc != 1){
        croak("fallback sub must return scalar!");
    }
    retval = POPs;
    SvREFCNT_inc(retval);
    PUTBACK;
    FREETMPS;
    LEAVE;
    return retval;
}

static SV *
encode_method(pTHX_ const encode_t * enc, const encpage_t * dir, SV * src, U8 * s, STRLEN slen,
	      int check, STRLEN * offset, SV * term, int * retcode, 
	      SV *fallback_cb)
{
    STRLEN tlen  = slen;
    STRLEN ddone = 0;
    STRLEN sdone = 0;
    /* We allocate slen+1.
       PerlIO dumps core if this value is smaller than this. */
    SV *dst = newSV(slen+1);
    U8 *d = (U8 *)SvPVX(dst);
    STRLEN dlen = SvLEN(dst)-1;
    int code = 0;
    STRLEN trmlen = 0;
    U8 *trm = term ? (U8*) SvPV(term, trmlen) : NULL;

    if (SvTAINTED(src)) SvTAINTED_on(dst); /* propagate taintedness */

    if (offset) {
      s += *offset;
      if (slen > *offset){ /* safeguard against slen overflow */
          slen -= *offset;
      }else{
          slen = 0;
      }
      tlen = slen;
    }

    if (slen == 0){
        SvCUR_set(dst, 0);
        SvPOK_only(dst);
        goto ENCODE_END;
    }

    while( (code = do_encode(dir, s, &slen, d, dlen, &dlen, !check,
                 trm, trmlen)) )
    {
        SvCUR_set(dst, dlen+ddone);
        SvPOK_only(dst);

        if (code == ENCODE_FALLBACK || code == ENCODE_PARTIAL ||
            code == ENCODE_FOUND_TERM) {
            break;
        }
        switch (code) {
        case ENCODE_NOSPACE:
        {
            STRLEN more = 0; /* make sure you initialize! */
            STRLEN sleft;
            sdone += slen;
            ddone += dlen;
            sleft = tlen - sdone;
#if ENCODE_XS_PROFILE >= 2
            Perl_warn(aTHX_
                  "more=%d, sdone=%d, sleft=%d, SvLEN(dst)=%d\n",
                  more, sdone, sleft, SvLEN(dst));
#endif
            if (sdone != 0) { /* has src ever been processed ? */
#if   ENCODE_XS_USEFP == 2
                more = (1.0*tlen*SvLEN(dst)+sdone-1)/sdone
                    - SvLEN(dst);
#elif ENCODE_XS_USEFP
                more = (STRLEN)((1.0*SvLEN(dst)+1)/sdone * sleft);
#else
            /* safe until SvLEN(dst) == MAX_INT/16 */
                more = (16*SvLEN(dst)+1)/sdone/16 * sleft;
#endif
            }
            more += UTF8_MAXLEN; /* insurance policy */
            d = (U8 *) SvGROW(dst, SvLEN(dst) + more);
            /* dst need to grow need MORE bytes! */
            if (ddone >= SvLEN(dst)) {
                Perl_croak(aTHX_ "Destination couldn't be grown.");
            }
            dlen = SvLEN(dst)-ddone-1;
            d   += ddone;
            s   += slen;
            slen = tlen-sdone;
            continue;
        }

    case ENCODE_NOREP:
        /* encoding */	
        if (dir == enc->f_utf8) {
        STRLEN clen;
        UV ch =
            utf8n_to_uvuni(s+slen, (tlen-sdone-slen),
                   &clen, UTF8_ALLOW_ANY|UTF8_CHECK_ONLY);
        /* if non-representable multibyte prefix at end of current buffer - break*/
        if (clen > tlen - sdone - slen) break;
        if (check & ENCODE_DIE_ON_ERR) {
            Perl_croak(aTHX_ ERR_ENCODE_NOMAP,
                   (UV)ch, enc->name[0]);
            return &PL_sv_undef; /* never reaches but be safe */
        }
        if (check & ENCODE_WARN_ON_ERR){
            Perl_warner(aTHX_ packWARN(WARN_UTF8),
                ERR_ENCODE_NOMAP, (UV)ch, enc->name[0]);
        }
        if (check & ENCODE_RETURN_ON_ERR){
            goto ENCODE_SET_SRC;
        }
        if (check & (ENCODE_PERLQQ|ENCODE_HTMLCREF|ENCODE_XMLCREF)){
            STRLEN sublen;
            char *substr;
            SV* subchar = 
            (fallback_cb != &PL_sv_undef)
		? do_fallback_cb(aTHX_ ch, fallback_cb)
		: newSVpvf(check & ENCODE_PERLQQ ? "\\x{%04" UVxf "}" :
                 check & ENCODE_HTMLCREF ? "&#%" UVuf ";" :
                 "&#x%" UVxf ";", (UV)ch);
            substr = SvPV(subchar, sublen);
            if (SvUTF8(subchar) && sublen && !utf8_to_bytes((U8 *)substr, &sublen)) { /* make sure no decoded string gets in */
                SvREFCNT_dec(subchar);
                croak("Wide character");
            }
            sdone += slen + clen;
            ddone += dlen + sublen;
            sv_catpvn(dst, substr, sublen);
            SvREFCNT_dec(subchar);
        } else {
            /* fallback char */
            sdone += slen + clen;
            ddone += dlen + enc->replen;
            sv_catpvn(dst, (char*)enc->rep, enc->replen);
        }
        }
        /* decoding */
        else {
        if (check & ENCODE_DIE_ON_ERR){
            Perl_croak(aTHX_ ERR_DECODE_NOMAP,
                              enc->name[0], (UV)s[slen]);
            return &PL_sv_undef; /* never reaches but be safe */
        }
        if (check & ENCODE_WARN_ON_ERR){
            Perl_warner(
            aTHX_ packWARN(WARN_UTF8),
            ERR_DECODE_NOMAP,
               	        enc->name[0], (UV)s[slen]);
        }
        if (check & ENCODE_RETURN_ON_ERR){
            goto ENCODE_SET_SRC;
        }
        if (check &
            (ENCODE_PERLQQ|ENCODE_HTMLCREF|ENCODE_XMLCREF)){
            STRLEN sublen;
            char *substr;
            SV* subchar = 
            (fallback_cb != &PL_sv_undef)
		? do_fallback_cb(aTHX_ (UV)s[slen], fallback_cb) 
		: newSVpvf("\\x%02" UVXf, (UV)s[slen]);
            substr = SvPVutf8(subchar, sublen);
            sdone += slen + 1;
            ddone += dlen + sublen;
            sv_catpvn(dst, substr, sublen);
            SvREFCNT_dec(subchar);
        } else {
            sdone += slen + 1;
            ddone += dlen + strlen(FBCHAR_UTF8);
            sv_catpvn(dst, FBCHAR_UTF8, strlen(FBCHAR_UTF8));
        }
        }
        /* settle variables when fallback */
        d    = (U8 *)SvEND(dst);
        dlen = SvLEN(dst) - ddone - 1;
        s    = (U8*)SvPVX(src) + sdone;
        slen = tlen - sdone;
        break;

        default:
            Perl_croak(aTHX_ "Unexpected code %d converting %s %s",
                   code, (dir == enc->f_utf8) ? "to" : "from",
                   enc->name[0]);
            return &PL_sv_undef;
        }
    }   /* End of looping through the string */
 ENCODE_SET_SRC:
    if (check && !(check & ENCODE_LEAVE_SRC)){
    sdone = SvCUR(src) - (slen+sdone);
    if (sdone) {
        sv_setpvn(src, (char*)s+slen, sdone);
    }
    SvCUR_set(src, sdone);
    SvSETMAGIC(src);
    }
    /* warn("check = 0x%X, code = 0x%d\n", check, code); */

    SvCUR_set(dst, dlen+ddone);
    SvPOK_only(dst);

#if ENCODE_XS_PROFILE
    if (SvCUR(dst) > SvCUR(src)){
    Perl_warn(aTHX_
          "SvLEN(dst)=%d, SvCUR(dst)=%d. %d bytes unused(%f %%)\n",
          SvLEN(dst), SvCUR(dst), SvLEN(dst) - SvCUR(dst),
          (SvLEN(dst) - SvCUR(dst))*1.0/SvLEN(dst)*100.0);
    }
#endif

    if (offset)
      *offset += sdone + slen;

 ENCODE_END:
    *SvEND(dst) = '\0';
    if (retcode) *retcode = code;
    return dst;
}

static bool
strict_utf8(pTHX_ SV* sv)
{
    HV* hv;
    SV** svp;
    sv = SvRV(sv);
    if (!sv || SvTYPE(sv) != SVt_PVHV)
        return 0;
    hv = (HV*)sv;
    svp = hv_fetch(hv, "strict_utf8", 11, 0);
    if (!svp)
        return 0;
    return SvTRUE(*svp);
}

/* Modern perls have the capability to do this more efficiently and portably */
#ifdef utf8n_to_uvchr_msgs
# define CAN_USE_BASE_PERL
#endif

#ifndef CAN_USE_BASE_PERL

/*
 * https://github.com/dankogai/p5-encode/pull/56#issuecomment-231959126
 */
#ifndef UNICODE_IS_NONCHAR
#define UNICODE_IS_NONCHAR(c) ((c >= 0xFDD0 && c <= 0xFDEF) || (c & 0xFFFE) == 0xFFFE)
#endif

#ifndef UNICODE_IS_SUPER
#define UNICODE_IS_SUPER(c) (c > PERL_UNICODE_MAX)
#endif

#define UNICODE_IS_STRICT(c) (!UNICODE_IS_SURROGATE(c) && !UNICODE_IS_NONCHAR(c) && !UNICODE_IS_SUPER(c))

#ifndef UTF_ACCUMULATION_OVERFLOW_MASK
#ifndef CHARBITS
#define CHARBITS CHAR_BIT
#endif
#define UTF_ACCUMULATION_OVERFLOW_MASK (((UV) UTF_CONTINUATION_MASK) << ((sizeof(UV) * CHARBITS) - UTF_ACCUMULATION_SHIFT))
#endif

/*
 * Convert non strict utf8 sequence of len >= 2 to unicode codepoint
 */
static UV
convert_utf8_multi_seq(U8* s, STRLEN len, STRLEN *rlen)
{
    UV uv;
    U8 *ptr = s;
    bool overflowed = 0;

    uv = NATIVE_TO_UTF(*s) & UTF_START_MASK(UTF8SKIP(s));

    len--;
    s++;

    while (len--) {
        if (!UTF8_IS_CONTINUATION(*s)) {
            *rlen = s-ptr;
            return 0;
        }
        if (uv & UTF_ACCUMULATION_OVERFLOW_MASK)
            overflowed = 1;
        uv = UTF8_ACCUMULATE(uv, *s);
        s++;
    }

    *rlen = s-ptr;

    if (overflowed || *rlen > (STRLEN)UNISKIP(uv)) {
        return 0;
    }

    return uv;
}

#endif  /* CAN_USE_BASE_PERL */

static U8*
process_utf8(pTHX_ SV* dst, U8* s, U8* e, SV *check_sv,
             bool encode, bool strict, bool stop_at_partial)
{
    /* Copies the purportedly UTF-8 encoded string starting at 's' and ending
     * at 'e' - 1 to 'dst', checking as it goes along that the string actually
     * is valid UTF-8.  There are two levels of strictness checking.  If
     * 'strict' is FALSE, the string is checked for being well-formed UTF-8, as
     * extended by Perl.  Additionally, if 'strict' is TRUE, above-Unicode code
     * points, surrogates, and non-character code points are checked for.  When
     * invalid input is encountered, some action is taken, exactly what depends
     * on the flags in 'check_sv'.  'encode' gives if this is from an encode
     * operation (if TRUE), or a decode one.  This function returns the
     * position in 's' of the start of the next character beyond where it got
     * to.  If there were no problems, that will be 'e'.  If 'stop_at_partial'
     * is TRUE, if the final character before 'e' is incomplete, but valid as
     * far as is available, no action will be taken on that partial character,
     * and the return value will point to its first byte */

    UV uv;
    STRLEN ulen;
    SV *fallback_cb;
    int check;
    U8 *d;
    STRLEN dlen;
    char esc[UTF8_MAXLEN * 6 + 1];
    STRLEN i;
    const U32 flags = (strict)
                    ? UTF8_DISALLOW_ILLEGAL_INTERCHANGE
                    : UTF8_ALLOW_NON_STRICT;

    if (SvROK(check_sv)) {
	/* croak("UTF-8 decoder doesn't support callback CHECK"); */
	fallback_cb = check_sv;
	check = ENCODE_PERLQQ|ENCODE_LEAVE_SRC; /* same as perlqq */
    }
    else {
	fallback_cb = &PL_sv_undef;
	check = SvIV_nomg(check_sv);
    }

    SvPOK_only(dst);
    SvCUR_set(dst,0);

    dlen = (s && e && s < e) ? e-s+1 : 1;
    d = (U8 *) SvGROW(dst, dlen);

    stop_at_partial = stop_at_partial || (check & ENCODE_STOP_AT_PARTIAL);

    while (s < e) {

#ifdef CAN_USE_BASE_PERL    /* Use the much faster, portable implementation if
                               available */

        /* If there were no errors, this will be 'e'; otherwise it will point
         * to the first byte of the erroneous input */
        const U8* e_or_where_failed;
        bool valid = is_utf8_string_loc_flags(s, e - s, &e_or_where_failed, flags);
        STRLEN len = e_or_where_failed - s;

        /* Copy as far as was successful */
        Move(s, d, len, U8);
        d += len;
        s = (U8 *) e_or_where_failed;

        /* Are done if it was valid, or we are accepting partial characters and
         * the only error is that the final bytes form a partial character */
        if (    LIKELY(valid)
            || (   stop_at_partial
                && is_utf8_valid_partial_char_flags(s, e, flags)))
        {
            break;
        }

        /* Here, was not valid.  If is 'strict', and is legal extended UTF-8,
         * we know it is a code point whose value we can calculate, just not
         * one accepted under strict.  Otherwise, it is malformed in some way.
         * In either case, the system function can calculate either the code
         * point, or the best substitution for it */
        uv = utf8n_to_uvchr(s, e - s, &ulen, UTF8_ALLOW_ANY);

#else   /* Use code for earlier perls */

        ((void)sizeof(flags));  /* Avoid compiler warning */

        if (UTF8_IS_INVARIANT(*s)) {
            *d++ = *s++;
            continue;
        }

        uv = 0;
        ulen = 1;
        if (! UTF8_IS_CONTINUATION(*s)) {
            /* Not an invariant nor a continuation; must be a start byte.  (We
             * can't test for UTF8_IS_START as that excludes things like \xC0
             * which are start bytes, but always lead to overlongs */

            U8 skip = UTF8SKIP(s);
            if ((s + skip) > e) {
                /* just calculate ulen, in pathological cases can be smaller then e-s */
                if (e-s >= 2)
                    convert_utf8_multi_seq(s, e-s, &ulen);
                else
                    ulen = 1;

                if (stop_at_partial && ulen == (STRLEN)(e-s))
                    break;

                goto malformed_byte;
            }

            uv = convert_utf8_multi_seq(s, skip, &ulen);
            if (uv == 0)
                goto malformed_byte;
            else if (strict && !UNICODE_IS_STRICT(uv))
                goto malformed;


             /* Whole char is good */
             memcpy(d, s, skip);
             d += skip;
             s += skip;
             continue;
        }

        /* If we get here there is something wrong with alleged UTF-8 */
        /* uv is used only when encoding */
    malformed_byte:
        if (uv == 0)
            uv = (UV)*s;
        if (encode || ulen == 0)
            ulen = 1;

    malformed:

#endif  /* The two versions for processing come back together here, for the
         * error handling code.
         *
         * Here, we are looping through the input and found an error.
         * 'uv' is the code point in error if calculable, or the REPLACEMENT
         *      CHARACTER if not.
         * 'ulen' is how many bytes of input this iteration of the loop
         *        consumes */

        if (!encode && (check & (ENCODE_DIE_ON_ERR|ENCODE_WARN_ON_ERR|ENCODE_PERLQQ)))
            for (i=0; i<ulen; ++i) sprintf(esc+4*i, "\\x%02X", s[i]);
        if (check & ENCODE_DIE_ON_ERR){
            if (encode)
                Perl_croak(aTHX_ ERR_ENCODE_NOMAP, uv, (strict ? "UTF-8" : "utf8"));
            else
                Perl_croak(aTHX_ ERR_DECODE_STR_NOMAP, (strict ? "UTF-8" : "utf8"), esc);
        }
        if (check & ENCODE_WARN_ON_ERR){
            if (encode)
                Perl_warner(aTHX_ packWARN(WARN_UTF8),
                            ERR_ENCODE_NOMAP, uv, (strict ? "UTF-8" : "utf8"));
            else
                Perl_warner(aTHX_ packWARN(WARN_UTF8),
                            ERR_DECODE_STR_NOMAP, (strict ? "UTF-8" : "utf8"), esc);
        }
        if (check & ENCODE_RETURN_ON_ERR) {
                break;
        }
        if (check & (ENCODE_PERLQQ|ENCODE_HTMLCREF|ENCODE_XMLCREF)){
            STRLEN sublen;
            char *substr;
            SV* subchar;
            if (encode) {
                subchar =
                    (fallback_cb != &PL_sv_undef)
                    ? do_fallback_cb(aTHX_ uv, fallback_cb)
                    : newSVpvf(check & ENCODE_PERLQQ
                        ? (ulen == 1 ? "\\x%02" UVXf : "\\x{%04" UVXf "}")
                        :  check & ENCODE_HTMLCREF ? "&#%" UVuf ";"
                        : "&#x%" UVxf ";", uv);
                substr = SvPV(subchar, sublen);
                if (SvUTF8(subchar) && sublen && !utf8_to_bytes((U8 *)substr, &sublen)) { /* make sure no decoded string gets in */
                    SvREFCNT_dec(subchar);
                    croak("Wide character");
                }
            } else {
                if (fallback_cb != &PL_sv_undef) {
                    /* in decode mode we have sequence of wrong bytes */
                    subchar = do_bytes_fallback_cb(aTHX_ s, ulen, fallback_cb);
                } else {
                    char *ptr = esc;
                    /* ENCODE_PERLQQ is already stored in esc */
                    if (check & (ENCODE_HTMLCREF|ENCODE_XMLCREF))
                        for (i=0; i<ulen; ++i) ptr += sprintf(ptr, ((check & ENCODE_HTMLCREF) ? "&#%u;" : "&#x%02X;"), s[i]);
                    subchar = newSVpvn(esc, strlen(esc));
                }
                substr = SvPVutf8(subchar, sublen);
            }
            dlen += sublen - ulen;
            SvCUR_set(dst, d-(U8 *)SvPVX(dst));
            *SvEND(dst) = '\0';
            sv_catpvn(dst, substr, sublen);
            SvREFCNT_dec(subchar);
            d = (U8 *) SvGROW(dst, dlen) + SvCUR(dst);
        } else {
            STRLEN fbcharlen = strlen(FBCHAR_UTF8);
            dlen += fbcharlen - ulen;
            if (SvLEN(dst) < dlen) {
                SvCUR_set(dst, d-(U8 *)SvPVX(dst));
                d = (U8 *) sv_grow(dst, dlen) + SvCUR(dst);
            }
            memcpy(d, FBCHAR_UTF8, fbcharlen);
            d += fbcharlen;
        }
        s += ulen;
    }
    SvCUR_set(dst, d-(U8 *)SvPVX(dst));
    *SvEND(dst) = '\0';

    return s;
}


MODULE = Encode		PACKAGE = Encode::utf8	PREFIX = Method_

PROTOTYPES: DISABLE

void
Method_decode(obj,src,check_sv = &PL_sv_no)
SV *	obj
SV *	src
SV *	check_sv
PREINIT:
    STRLEN slen;
    U8 *s;
    U8 *e;
    SV *dst;
    bool renewed = 0;
    int check;
    bool modify;
    dSP;
INIT:
    SvGETMAGIC(src);
    SvGETMAGIC(check_sv);
    check = SvROK(check_sv) ? ENCODE_PERLQQ|ENCODE_LEAVE_SRC : SvIV_nomg(check_sv);
    modify = (check && !(check & ENCODE_LEAVE_SRC));
PPCODE:
    if (!SvOK(src))
        XSRETURN_UNDEF;
    s = modify ? (U8 *)SvPV_force_nomg(src, slen) : (U8 *)SvPV_nomg(src, slen);
    if (SvUTF8(src))
        utf8_safe_downgrade(aTHX_ &src, &s, &slen, modify);
    e = s+slen;

    /*
     * PerlIO check -- we assume the object is of PerlIO if renewed
     */
    ENTER; SAVETMPS;
    PUSHMARK(sp);
    XPUSHs(obj);
    PUTBACK;
    if (call_method("renewed",G_SCALAR) == 1) {
    SPAGAIN;
    renewed = (bool)POPi;
    PUTBACK;
#if 0
    fprintf(stderr, "renewed == %d\n", renewed);
#endif
    }
    FREETMPS; LEAVE;
    /* end PerlIO check */

    dst = sv_2mortal(newSV(slen>0?slen:1)); /* newSV() abhors 0 -- inaba */
    s = process_utf8(aTHX_ dst, s, e, check_sv, 0, strict_utf8(aTHX_ obj), renewed);

    /* Clear out translated part of source unless asked not to */
    if (modify) {
    slen = e-s;
    if (slen) {
        sv_setpvn(src, (char*)s, slen);
    }
    SvCUR_set(src, slen);
    SvSETMAGIC(src);
    }
    SvUTF8_on(dst);
    if (SvTAINTED(src)) SvTAINTED_on(dst); /* propagate taintedness */
    ST(0) = dst;
    XSRETURN(1);

void
Method_encode(obj,src,check_sv = &PL_sv_no)
SV *	obj
SV *	src
SV *	check_sv
PREINIT:
    STRLEN slen;
    U8 *s;
    U8 *e;
    SV *dst;
    int check;
    bool modify;
INIT:
    SvGETMAGIC(src);
    SvGETMAGIC(check_sv);
    check = SvROK(check_sv) ? ENCODE_PERLQQ|ENCODE_LEAVE_SRC : SvIV_nomg(check_sv);
    modify = (check && !(check & ENCODE_LEAVE_SRC));
PPCODE:
    if (!SvOK(src))
        XSRETURN_UNDEF;
    s = modify ? (U8 *)SvPV_force_nomg(src, slen) : (U8 *)SvPV_nomg(src, slen);
    e = s+slen;
    dst = sv_2mortal(newSV(slen>0?slen:1)); /* newSV() abhors 0 -- inaba */
    if (SvUTF8(src)) {
    /* Already encoded */
    if (strict_utf8(aTHX_ obj)) {
        s = process_utf8(aTHX_ dst, s, e, check_sv, 1, 1, 0);
    }
        else {
            /* trust it and just copy the octets */
    	    sv_setpvn(dst,(char *)s,(e-s));
        s = e;
        }
    }
    else {
        /* Native bytes - can always encode */
        U8 *d = (U8 *) SvGROW(dst, 2*slen+1); /* +1 or assertion will botch */
        while (s < e) {
#ifdef append_utf8_from_native_byte
            append_utf8_from_native_byte(*s, &d);
            s++;
#else
            UV uv = NATIVE_TO_UNI((UV) *s);
            s++; /* Above expansion of NATIVE_TO_UNI() is safer this way. */
            if (UNI_IS_INVARIANT(uv))
                *d++ = (U8)UTF_TO_NATIVE(uv);
            else {
                *d++ = (U8)UTF8_EIGHT_BIT_HI(uv);
                *d++ = (U8)UTF8_EIGHT_BIT_LO(uv);
            }
#endif
        }
        SvCUR_set(dst, d- (U8 *)SvPVX(dst));
        *SvEND(dst) = '\0';
    }

    /* Clear out translated part of source unless asked not to */
    if (modify) {
    slen = e-s;
    if (slen) {
        sv_setpvn(src, (char*)s, slen);
    }
    SvCUR_set(src, slen);
    SvSETMAGIC(src);
    }
    SvPOK_only(dst);
    SvUTF8_off(dst);
    if (SvTAINTED(src)) SvTAINTED_on(dst); /* propagate taintedness */
    ST(0) = dst;
    XSRETURN(1);

MODULE = Encode		PACKAGE = Encode::XS	PREFIX = Method_

PROTOTYPES: DISABLE

SV *
Method_renew(obj)
SV *	obj
CODE:
    PERL_UNUSED_VAR(obj);
    RETVAL = newSVsv(obj);
OUTPUT:
    RETVAL

int
Method_renewed(obj)
SV *    obj
CODE:
    RETVAL = 0;
    PERL_UNUSED_VAR(obj);
OUTPUT:
    RETVAL

SV *
Method_name(obj)
SV *	obj
PREINIT:
    encode_t *enc;
INIT:
    enc = INT2PTR(encode_t *, SvIV(SvRV(obj)));
CODE:
    RETVAL = newSVpvn(enc->name[0], strlen(enc->name[0]));
OUTPUT:
    RETVAL

bool
Method_cat_decode(obj, dst, src, off, term, check_sv = &PL_sv_no)
SV *	obj
SV *	dst
SV *	src
SV *	off
SV *	term
SV *    check_sv
PREINIT:
    int check;
    SV *fallback_cb;
    bool modify;
    encode_t *enc;
    STRLEN offset;
    int code = 0;
    U8 *s;
    STRLEN slen;
    SV *tmp;
INIT:
    SvGETMAGIC(src);
    SvGETMAGIC(check_sv);
    check = SvROK(check_sv) ? ENCODE_PERLQQ|ENCODE_LEAVE_SRC : SvIV_nomg(check_sv);
    fallback_cb = SvROK(check_sv) ? check_sv : &PL_sv_undef;
    modify = (check && !(check & ENCODE_LEAVE_SRC));
    enc = INT2PTR(encode_t *, SvIV(SvRV(obj)));
    offset = (STRLEN)SvIV(off);
CODE:
    if (!SvOK(src))
        XSRETURN_NO;
    s = modify ? (U8 *)SvPV_force_nomg(src, slen) : (U8 *)SvPV_nomg(src, slen);
    if (SvUTF8(src))
        utf8_safe_downgrade(aTHX_ &src, &s, &slen, modify);
    tmp = encode_method(aTHX_ enc, enc->t_utf8, src, s, slen, check,
                &offset, term, &code, fallback_cb);
    sv_catsv(dst, tmp);
    SvREFCNT_dec(tmp);
    SvIV_set(off, (IV)offset);
    RETVAL = (code == ENCODE_FOUND_TERM);
OUTPUT:
    RETVAL

SV *
Method_decode(obj,src,check_sv = &PL_sv_no)
SV *	obj
SV *	src
SV *	check_sv
PREINIT:
    int check;
    SV *fallback_cb;
    bool modify;
    encode_t *enc;
    U8 *s;
    STRLEN slen;
INIT:
    SvGETMAGIC(src);
    SvGETMAGIC(check_sv);
    check = SvROK(check_sv) ? ENCODE_PERLQQ|ENCODE_LEAVE_SRC : SvIV_nomg(check_sv);
    fallback_cb = SvROK(check_sv) ? check_sv : &PL_sv_undef;
    modify = (check && !(check & ENCODE_LEAVE_SRC));
    enc = INT2PTR(encode_t *, SvIV(SvRV(obj)));
CODE:
    if (!SvOK(src))
        XSRETURN_UNDEF;
    s = modify ? (U8 *)SvPV_force_nomg(src, slen) : (U8 *)SvPV_nomg(src, slen);
    if (SvUTF8(src))
        utf8_safe_downgrade(aTHX_ &src, &s, &slen, modify);
    RETVAL = encode_method(aTHX_ enc, enc->t_utf8, src, s, slen, check,
              NULL, Nullsv, NULL, fallback_cb);
    SvUTF8_on(RETVAL);
OUTPUT:
    RETVAL

SV *
Method_encode(obj,src,check_sv = &PL_sv_no)
SV *	obj
SV *	src
SV *	check_sv
PREINIT:
    int check;
    SV *fallback_cb;
    bool modify;
    encode_t *enc;
    U8 *s;
    STRLEN slen;
INIT:
    SvGETMAGIC(src);
    SvGETMAGIC(check_sv);
    check = SvROK(check_sv) ? ENCODE_PERLQQ|ENCODE_LEAVE_SRC : SvIV_nomg(check_sv);
    fallback_cb = SvROK(check_sv) ? check_sv : &PL_sv_undef;
    modify = (check && !(check & ENCODE_LEAVE_SRC));
    enc = INT2PTR(encode_t *, SvIV(SvRV(obj)));
CODE:
    if (!SvOK(src))
        XSRETURN_UNDEF;
    s = modify ? (U8 *)SvPV_force_nomg(src, slen) : (U8 *)SvPV_nomg(src, slen);
    if (!SvUTF8(src))
        utf8_safe_upgrade(aTHX_ &src, &s, &slen, modify);
    RETVAL = encode_method(aTHX_ enc, enc->f_utf8, src, s, slen, check,
              NULL, Nullsv, NULL, fallback_cb);
OUTPUT:
    RETVAL

bool
Method_needs_lines(obj)
SV *	obj
CODE:
    PERL_UNUSED_VAR(obj);
    RETVAL = FALSE;
OUTPUT:
    RETVAL

bool
Method_perlio_ok(obj)
SV *	obj
PREINIT:
    SV *sv;
CODE:
    PERL_UNUSED_VAR(obj);
    sv = eval_pv("require PerlIO::encoding", 0);
    RETVAL = SvTRUE(sv);
OUTPUT:
    RETVAL

SV *
Method_mime_name(obj)
SV *	obj
PREINIT:
    encode_t *enc;
INIT:
    enc = INT2PTR(encode_t *, SvIV(SvRV(obj)));
CODE:
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSVpvn(enc->name[0], strlen(enc->name[0]))));
    PUTBACK;
    call_pv("Encode::MIME::Name::get_mime_name", G_SCALAR);
    SPAGAIN;
    RETVAL = newSVsv(POPs);
    PUTBACK;
    FREETMPS;
    LEAVE;
OUTPUT:
    RETVAL

MODULE = Encode         PACKAGE = Encode

PROTOTYPES: ENABLE

I32
_bytes_to_utf8(sv, ...)
SV *    sv
PREINIT:
    SV * encoding;
INIT:
    encoding = items == 2 ? ST(1) : Nullsv;
CODE:
    if (encoding)
    RETVAL = _encoded_bytes_to_utf8(sv, SvPV_nolen(encoding));
    else {
    STRLEN len;
    U8*    s = (U8*)SvPV(sv, len);
    U8*    converted;

    converted = bytes_to_utf8(s, &len); /* This allocs */
    sv_setpvn(sv, (char *)converted, len);
    SvUTF8_on(sv); /* XXX Should we? */
    Safefree(converted);                /* ... so free it */
    RETVAL = len;
    }
OUTPUT:
    RETVAL

I32
_utf8_to_bytes(sv, ...)
SV *    sv
PREINIT:
    SV * to;
    SV * check;
INIT:
    to    = items > 1 ? ST(1) : Nullsv;
    check = items > 2 ? ST(2) : Nullsv;
CODE:
    if (to) {
    RETVAL = _encoded_utf8_to_bytes(sv, SvPV_nolen(to));
    } else {
    STRLEN len;
    U8 *s = (U8*)SvPV(sv, len);

    RETVAL = 0;
    if (SvTRUE(check)) {
        /* Must do things the slow way */
        U8 *dest;
            /* We need a copy to pass to check() */
        U8 *src  = s;
        U8 *send = s + len;
        U8 *d0;

        New(83, dest, len, U8); /* I think */
        d0 = dest;

        while (s < send) {
                if (*s < 0x80){
            *dest++ = *s++;
                } else {
            STRLEN ulen;
            UV uv = *s++;

            /* Have to do it all ourselves because of error routine,
               aargh. */
            if (!(uv & 0x40)){ goto failure; }
            if      (!(uv & 0x20)) { ulen = 2;  uv &= 0x1f; }
            else if (!(uv & 0x10)) { ulen = 3;  uv &= 0x0f; }
            else if (!(uv & 0x08)) { ulen = 4;  uv &= 0x07; }
            else if (!(uv & 0x04)) { ulen = 5;  uv &= 0x03; }
            else if (!(uv & 0x02)) { ulen = 6;  uv &= 0x01; }
            else if (!(uv & 0x01)) { ulen = 7;  uv = 0; }
            else                   { ulen = 13; uv = 0; }
        
            /* Note change to utf8.c variable naming, for variety */
            while (ulen--) {
            if ((*s & 0xc0) != 0x80){
                goto failure;
            } else {
                uv = (uv << 6) | (*s++ & 0x3f);
            }
          }
          if (uv > 256) {
          failure:
              call_failure(check, s, dest, src);
              /* Now what happens? */
          }
          *dest++ = (U8)uv;
        }
        }
        RETVAL = dest - d0;
        sv_usepvn(sv, (char *)dest, RETVAL);
        SvUTF8_off(sv);
    } else {
        RETVAL = (utf8_to_bytes(s, &len) ? len : 0);
    }
    }
OUTPUT:
    RETVAL

bool
is_utf8(sv, check = 0)
SV *	sv
int	check
PREINIT:
    char *str;
    STRLEN len;
CODE:
    SvGETMAGIC(sv); /* SvGETMAGIC() can modify SvOK flag */
    str = SvOK(sv) ? SvPV_nomg(sv, len) : NULL; /* SvPV() can modify SvUTF8 flag */
    RETVAL = SvUTF8(sv) ? TRUE : FALSE;
    if (RETVAL && check && (!str || !is_utf8_string((U8 *)str, len)))
        RETVAL = FALSE;
OUTPUT:
    RETVAL

SV *
_utf8_on(sv)
SV *	sv
CODE:
    SvGETMAGIC(sv);
    if (!SvTAINTED(sv) && SvPOKp(sv)) {
        if (SvTHINKFIRST(sv)) sv_force_normal(sv);
        RETVAL = boolSV(SvUTF8(sv));
        SvUTF8_on(sv);
        SvSETMAGIC(sv);
    } else {
        RETVAL = &PL_sv_undef;
    }
OUTPUT:
    RETVAL

SV *
_utf8_off(sv)
SV *	sv
CODE:
    SvGETMAGIC(sv);
    if (!SvTAINTED(sv) && SvPOKp(sv)) {
        if (SvTHINKFIRST(sv)) sv_force_normal(sv);
        RETVAL = boolSV(SvUTF8(sv));
        SvUTF8_off(sv);
        SvSETMAGIC(sv);
    } else {
        RETVAL = &PL_sv_undef;
    }
OUTPUT:
    RETVAL

void
onBOOT()
CODE:
{
#include "def_t.exh"
}

BOOT:
{
    HV *stash = gv_stashpvn("Encode", strlen("Encode"), GV_ADD);
    newCONSTSUB(stash, "DIE_ON_ERR", newSViv(ENCODE_DIE_ON_ERR));
    newCONSTSUB(stash, "WARN_ON_ERR", newSViv(ENCODE_WARN_ON_ERR));
    newCONSTSUB(stash, "RETURN_ON_ERR", newSViv(ENCODE_RETURN_ON_ERR));
    newCONSTSUB(stash, "LEAVE_SRC", newSViv(ENCODE_LEAVE_SRC));
    newCONSTSUB(stash, "PERLQQ", newSViv(ENCODE_PERLQQ));
    newCONSTSUB(stash, "HTMLCREF", newSViv(ENCODE_HTMLCREF));
    newCONSTSUB(stash, "XMLCREF", newSViv(ENCODE_XMLCREF));
    newCONSTSUB(stash, "STOP_AT_PARTIAL", newSViv(ENCODE_STOP_AT_PARTIAL));
    newCONSTSUB(stash, "FB_DEFAULT", newSViv(ENCODE_FB_DEFAULT));
    newCONSTSUB(stash, "FB_CROAK", newSViv(ENCODE_FB_CROAK));
    newCONSTSUB(stash, "FB_QUIET", newSViv(ENCODE_FB_QUIET));
    newCONSTSUB(stash, "FB_WARN", newSViv(ENCODE_FB_WARN));
    newCONSTSUB(stash, "FB_PERLQQ", newSViv(ENCODE_FB_PERLQQ));
    newCONSTSUB(stash, "FB_HTMLCREF", newSViv(ENCODE_FB_HTMLCREF));
    newCONSTSUB(stash, "FB_XMLCREF", newSViv(ENCODE_FB_XMLCREF));
}
