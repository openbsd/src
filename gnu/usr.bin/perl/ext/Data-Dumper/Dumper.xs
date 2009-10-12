#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef USE_PPPORT_H
#  define NEED_my_snprintf
#  define NEED_sv_2pv_flags
#  include "ppport.h"
#endif

#if PERL_VERSION < 8
#  define DD_USE_OLD_ID_FORMAT
#endif

static I32 num_q (const char *s, STRLEN slen);
static I32 esc_q (char *dest, const char *src, STRLEN slen);
static I32 esc_q_utf8 (pTHX_ SV *sv, const char *src, STRLEN slen);
static I32 needs_quote(register const char *s);
static SV *sv_x (pTHX_ SV *sv, const char *str, STRLEN len, I32 n);
static I32 DD_dump (pTHX_ SV *val, const char *name, STRLEN namelen, SV *retval,
		    HV *seenhv, AV *postav, I32 *levelp, I32 indent,
		    SV *pad, SV *xpad, SV *apad, SV *sep, SV *pair,
		    SV *freezer, SV *toaster,
		    I32 purity, I32 deepcopy, I32 quotekeys, SV *bless,
		    I32 maxdepth, SV *sortkeys);

#ifndef HvNAME_get
#define HvNAME_get HvNAME
#endif

#if PERL_VERSION <= 6 /* Perl 5.6 and earlier */

# ifdef EBCDIC
#  define UNI_TO_NATIVE(ch) (((ch) > 255) ? (ch) : ASCII_TO_NATIVE(ch))
# else
#  define UNI_TO_NATIVE(ch) (ch)
# endif

UV
Perl_utf8_to_uvchr(pTHX_ U8 *s, STRLEN *retlen)
{
    const UV uv = utf8_to_uv(s, UTF8_MAXLEN, retlen,
                    ckWARN(WARN_UTF8) ? 0 : UTF8_ALLOW_ANY);
    return UNI_TO_NATIVE(uv);
}

# if !defined(PERL_IMPLICIT_CONTEXT)
#  define utf8_to_uvchr	     Perl_utf8_to_uvchr
# else
#  define utf8_to_uvchr(a,b) Perl_utf8_to_uvchr(aTHX_ a,b)
# endif

#endif /* PERL_VERSION <= 6 */

/* Changes in 5.7 series mean that now IOK is only set if scalar is
   precisely integer but in 5.6 and earlier we need to do a more
   complex test  */
#if PERL_VERSION <= 6
#define DD_is_integer(sv) (SvIOK(sv) && (SvIsUV(val) ? SvUV(sv) == SvNV(sv) : SvIV(sv) == SvNV(sv)))
#else
#define DD_is_integer(sv) SvIOK(sv)
#endif

/* does a string need to be protected? */
static I32
needs_quote(register const char *s)
{
TOP:
    if (s[0] == ':') {
	if (*++s) {
	    if (*s++ != ':')
		return 1;
	}
	else
	    return 1;
    }
    if (isIDFIRST(*s)) {
	while (*++s)
	    if (!isALNUM(*s)) {
		if (*s == ':')
		    goto TOP;
		else
		    return 1;
	    }
    }
    else
	return 1;
    return 0;
}

/* count the number of "'"s and "\"s in string */
static I32
num_q(register const char *s, register STRLEN slen)
{
    register I32 ret = 0;

    while (slen > 0) {
	if (*s == '\'' || *s == '\\')
	    ++ret;
	++s;
	--slen;
    }
    return ret;
}


/* returns number of chars added to escape "'"s and "\"s in s */
/* slen number of characters in s will be escaped */
/* destination must be long enough for additional chars */
static I32
esc_q(register char *d, register const char *s, register STRLEN slen)
{
    register I32 ret = 0;

    while (slen > 0) {
	switch (*s) {
	case '\'':
	case '\\':
	    *d = '\\';
	    ++d; ++ret;
	default:
	    *d = *s;
	    ++d; ++s; --slen;
	    break;
	}
    }
    return ret;
}

static I32
esc_q_utf8(pTHX_ SV* sv, register const char *src, register STRLEN slen)
{
    char *r, *rstart;
    const char *s = src;
    const char * const send = src + slen;
    STRLEN j, cur = SvCUR(sv);
    /* Could count 128-255 and 256+ in two variables, if we want to
       be like &qquote and make a distinction.  */
    STRLEN grow = 0;	/* bytes needed to represent chars 128+ */
    /* STRLEN topbit_grow = 0;	bytes needed to represent chars 128-255 */
    STRLEN backslashes = 0;
    STRLEN single_quotes = 0;
    STRLEN qq_escapables = 0;	/* " $ @ will need a \ in "" strings.  */
    STRLEN normal = 0;

    /* this will need EBCDICification */
    for (s = src; s < send; s += UTF8SKIP(s)) {
        const UV k = utf8_to_uvchr((U8*)s, NULL);

#ifdef EBCDIC
	if (!isprint(k) || k > 256) {
#else
	if (k > 127) {
#endif
            /* 4: \x{} then count the number of hex digits.  */
            grow += 4 + (k <= 0xFF ? 2 : k <= 0xFFF ? 3 : k <= 0xFFFF ? 4 :
#if UVSIZE == 4
                8 /* We may allocate a bit more than the minimum here.  */
#else
                k <= 0xFFFFFFFF ? 8 : UVSIZE * 4
#endif
                );
        } else if (k == '\\') {
            backslashes++;
        } else if (k == '\'') {
            single_quotes++;
        } else if (k == '"' || k == '$' || k == '@') {
            qq_escapables++;
        } else {
            normal++;
        }
    }
    if (grow) {
        /* We have something needing hex. 3 is ""\0 */
        sv_grow(sv, cur + 3 + grow + 2*backslashes + single_quotes
		+ 2*qq_escapables + normal);
        rstart = r = SvPVX(sv) + cur;

        *r++ = '"';

        for (s = src; s < send; s += UTF8SKIP(s)) {
            const UV k = utf8_to_uvchr((U8*)s, NULL);

            if (k == '"' || k == '\\' || k == '$' || k == '@') {
                *r++ = '\\';
                *r++ = (char)k;
            }
            else
#ifdef EBCDIC
	      if (isprint(k) && k < 256)
#else
	      if (k < 0x80)
#endif
                *r++ = (char)k;
            else {
#if PERL_VERSION < 10
                sprintf(r, "\\x{%"UVxf"}", k);
                r += strlen(r);
                /* my_sprintf is not supported by ppport.h */
#else
                r = r + my_sprintf(r, "\\x{%"UVxf"}", k);
#endif
            }
        }
        *r++ = '"';
    } else {
        /* Single quotes.  */
        sv_grow(sv, cur + 3 + 2*backslashes + 2*single_quotes
		+ qq_escapables + normal);
        rstart = r = SvPVX(sv) + cur;
        *r++ = '\'';
        for (s = src; s < send; s ++) {
            const char k = *s;
            if (k == '\'' || k == '\\')
                *r++ = '\\';
            *r++ = k;
        }
        *r++ = '\'';
    }
    *r = '\0';
    j = r - rstart;
    SvCUR_set(sv, cur + j);

    return j;
}

/* append a repeated string to an SV */
static SV *
sv_x(pTHX_ SV *sv, const char *str, STRLEN len, I32 n)
{
    if (!sv)
	sv = newSVpvn("", 0);
#ifdef DEBUGGING
    else
	assert(SvTYPE(sv) >= SVt_PV);
#endif

    if (n > 0) {
	SvGROW(sv, len*n + SvCUR(sv) + 1);
	if (len == 1) {
	    char * const start = SvPVX(sv) + SvCUR(sv);
	    SvCUR_set(sv, SvCUR(sv) + n);
	    start[n] = '\0';
	    while (n > 0)
		start[--n] = str[0];
	}
	else
	    while (n > 0) {
		sv_catpvn(sv, str, len);
		--n;
	    }
    }
    return sv;
}

/*
 * This ought to be split into smaller functions. (it is one long function since
 * it exactly parallels the perl version, which was one long thing for
 * efficiency raisins.)  Ugggh!
 */
static I32
DD_dump(pTHX_ SV *val, const char *name, STRLEN namelen, SV *retval, HV *seenhv,
	AV *postav, I32 *levelp, I32 indent, SV *pad, SV *xpad,
	SV *apad, SV *sep, SV *pair, SV *freezer, SV *toaster, I32 purity,
	I32 deepcopy, I32 quotekeys, SV *bless, I32 maxdepth, SV *sortkeys)
{
    char tmpbuf[128];
    U32 i;
    char *c, *r, *realpack;
#ifdef DD_USE_OLD_ID_FORMAT
    char id[128];
#else
    UV id_buffer;
    char *const id = (char *)&id_buffer;
#endif
    SV **svp;
    SV *sv, *ipad, *ival;
    SV *blesspad = Nullsv;
    AV *seenentry = NULL;
    char *iname;
    STRLEN inamelen, idlen = 0;
    U32 realtype;
    bool no_bless = 0; /* when a qr// is blessed into Regexp we dont want to bless it.
                          in later perls we should actually check the classname of the 
                          engine. this gets tricky as it involves lexical issues that arent so
                          easy to resolve */
    bool is_regex = 0; /* we are dumping a regex, we need to know this before we bless */

    if (!val)
	return 0;

    /* If the ouput buffer has less than some arbitary amount of space
       remaining, then enlarge it. For the test case (25M of output),
       *1.1 was slower, *2.0 was the same, so the first guess of 1.5 is
	deemed to be good enough.  */
    if (SvTYPE(retval) >= SVt_PV && (SvLEN(retval) - SvCUR(retval)) < 42) {
	sv_grow(retval, SvCUR(retval) * 3 / 2);
    }

    realtype = SvTYPE(val);

    if (SvGMAGICAL(val))
        mg_get(val);
    if (SvROK(val)) {

        /* If a freeze method is provided and the object has it, call
           it.  Warn on errors. */
	if (SvOBJECT(SvRV(val)) && freezer &&
	    SvPOK(freezer) && SvCUR(freezer) &&
            gv_fetchmeth(SvSTASH(SvRV(val)), SvPVX_const(freezer), 
                         SvCUR(freezer), -1) != NULL)
	{
	    dSP; ENTER; SAVETMPS; PUSHMARK(sp);
	    XPUSHs(val); PUTBACK;
	    i = perl_call_method(SvPVX_const(freezer), G_EVAL|G_VOID);
	    SPAGAIN;
	    if (SvTRUE(ERRSV))
		warn("WARNING(Freezer method call failed): %"SVf"", ERRSV);
	    PUTBACK; FREETMPS; LEAVE;
	}
	
	ival = SvRV(val);
	realtype = SvTYPE(ival);
#ifdef DD_USE_OLD_ID_FORMAT
        idlen = my_snprintf(id, sizeof(id), "0x%"UVxf, PTR2UV(ival));
#else
	id_buffer = PTR2UV(ival);
	idlen = sizeof(id_buffer);
#endif
	if (SvOBJECT(ival))
	    realpack = HvNAME_get(SvSTASH(ival));
	else
	    realpack = NULL;

	/* if it has a name, we need to either look it up, or keep a tab
	 * on it so we know when we hit it later
	 */
	if (namelen) {
	    if ((svp = hv_fetch(seenhv, id, idlen, FALSE))
		&& (sv = *svp) && SvROK(sv) && (seenentry = (AV*)SvRV(sv)))
	    {
		SV *othername;
		if ((svp = av_fetch(seenentry, 0, FALSE))
		    && (othername = *svp))
		{
		    if (purity && *levelp > 0) {
			SV *postentry;
			
			if (realtype == SVt_PVHV)
			    sv_catpvn(retval, "{}", 2);
			else if (realtype == SVt_PVAV)
			    sv_catpvn(retval, "[]", 2);
			else
			    sv_catpvn(retval, "do{my $o}", 9);
			postentry = newSVpvn(name, namelen);
			sv_catpvn(postentry, " = ", 3);
			sv_catsv(postentry, othername);
			av_push(postav, postentry);
		    }
		    else {
			if (name[0] == '@' || name[0] == '%') {
			    if ((SvPVX_const(othername))[0] == '\\' &&
				(SvPVX_const(othername))[1] == name[0]) {
				sv_catpvn(retval, SvPVX_const(othername)+1,
					  SvCUR(othername)-1);
			    }
			    else {
				sv_catpvn(retval, name, 1);
				sv_catpvn(retval, "{", 1);
				sv_catsv(retval, othername);
				sv_catpvn(retval, "}", 1);
			    }
			}
			else
			    sv_catsv(retval, othername);
		    }
		    return 1;
		}
		else {
#ifdef DD_USE_OLD_ID_FORMAT
		    warn("ref name not found for %s", id);
#else
		    warn("ref name not found for 0x%"UVxf, PTR2UV(ival));
#endif
		    return 0;
		}
	    }
	    else {   /* store our name and continue */
		SV *namesv;
		if (name[0] == '@' || name[0] == '%') {
		    namesv = newSVpvn("\\", 1);
		    sv_catpvn(namesv, name, namelen);
		}
		else if (realtype == SVt_PVCV && name[0] == '*') {
		    namesv = newSVpvn("\\", 2);
		    sv_catpvn(namesv, name, namelen);
		    (SvPVX(namesv))[1] = '&';
		}
		else
		    namesv = newSVpvn(name, namelen);
		seenentry = newAV();
		av_push(seenentry, namesv);
		(void)SvREFCNT_inc(val);
		av_push(seenentry, val);
		(void)hv_store(seenhv, id, idlen,
			       newRV_inc((SV*)seenentry), 0);
		SvREFCNT_dec(seenentry);
	    }
	}
        /* regexps dont have to be blessed into package "Regexp"
         * they can be blessed into any package. 
         */
#if PERL_VERSION < 8
	if (realpack && *realpack == 'R' && strEQ(realpack, "Regexp")) 
#elif PERL_VERSION < 11
        if (realpack && realtype == SVt_PVMG && mg_find(ival, PERL_MAGIC_qr))
#else        
        if (realpack && realtype == SVt_REGEXP) 
#endif
        {
            is_regex = 1;
            if (strEQ(realpack, "Regexp")) 
                no_bless = 1;
            else
                no_bless = 0;
        }

	/* If purity is not set and maxdepth is set, then check depth:
	 * if we have reached maximum depth, return the string
	 * representation of the thing we are currently examining
	 * at this depth (i.e., 'Foo=ARRAY(0xdeadbeef)').
	 */
	if (!purity && maxdepth > 0 && *levelp >= maxdepth) {
	    STRLEN vallen;
	    const char * const valstr = SvPV(val,vallen);
	    sv_catpvn(retval, "'", 1);
	    sv_catpvn(retval, valstr, vallen);
	    sv_catpvn(retval, "'", 1);
	    return 1;
	}

	if (realpack && !no_bless) {				/* we have a blessed ref */
	    STRLEN blesslen;
	    const char * const blessstr = SvPV(bless, blesslen);
	    sv_catpvn(retval, blessstr, blesslen);
	    sv_catpvn(retval, "( ", 2);
	    if (indent >= 2) {
		blesspad = apad;
		apad = newSVsv(apad);
		sv_x(aTHX_ apad, " ", 1, blesslen+2);
	    }
	}

	(*levelp)++;
	ipad = sv_x(aTHX_ Nullsv, SvPVX_const(xpad), SvCUR(xpad), *levelp);

        if (is_regex) 
        {
            STRLEN rlen;
	    const char *rval = SvPV(val, rlen);
	    const char *slash = strchr(rval, '/');
	    sv_catpvn(retval, "qr/", 3);
	    while (slash) {
		sv_catpvn(retval, rval, slash-rval);
		sv_catpvn(retval, "\\/", 2);
		rlen -= slash-rval+1;
		rval = slash+1;
		slash = strchr(rval, '/');
	    }
	    sv_catpvn(retval, rval, rlen);
	    sv_catpvn(retval, "/", 1);
	} 
        else if (
#if PERL_VERSION < 9
		realtype <= SVt_PVBM
#else
		realtype <= SVt_PVMG
#endif
	) {			     /* scalar ref */
	    SV * const namesv = newSVpvn("${", 2);
	    sv_catpvn(namesv, name, namelen);
	    sv_catpvn(namesv, "}", 1);
	    if (realpack) {				     /* blessed */
		sv_catpvn(retval, "do{\\(my $o = ", 13);
		DD_dump(aTHX_ ival, SvPVX_const(namesv), SvCUR(namesv), retval, seenhv,
			postav, levelp,	indent, pad, xpad, apad, sep, pair,
			freezer, toaster, purity, deepcopy, quotekeys, bless,
			maxdepth, sortkeys);
		sv_catpvn(retval, ")}", 2);
	    }						     /* plain */
	    else {
		sv_catpvn(retval, "\\", 1);
		DD_dump(aTHX_ ival, SvPVX_const(namesv), SvCUR(namesv), retval, seenhv,
			postav, levelp,	indent, pad, xpad, apad, sep, pair,
			freezer, toaster, purity, deepcopy, quotekeys, bless,
			maxdepth, sortkeys);
	    }
	    SvREFCNT_dec(namesv);
	}
	else if (realtype == SVt_PVGV) {		     /* glob ref */
	    SV * const namesv = newSVpvn("*{", 2);
	    sv_catpvn(namesv, name, namelen);
	    sv_catpvn(namesv, "}", 1);
	    sv_catpvn(retval, "\\", 1);
	    DD_dump(aTHX_ ival, SvPVX_const(namesv), SvCUR(namesv), retval, seenhv,
		    postav, levelp,	indent, pad, xpad, apad, sep, pair,
		    freezer, toaster, purity, deepcopy, quotekeys, bless,
		    maxdepth, sortkeys);
	    SvREFCNT_dec(namesv);
	}
	else if (realtype == SVt_PVAV) {
	    SV *totpad;
	    I32 ix = 0;
	    const I32 ixmax = av_len((AV *)ival);
	
	    SV * const ixsv = newSViv(0);
	    /* allowing for a 24 char wide array index */
	    New(0, iname, namelen+28, char);
	    (void)strcpy(iname, name);
	    inamelen = namelen;
	    if (name[0] == '@') {
		sv_catpvn(retval, "(", 1);
		iname[0] = '$';
	    }
	    else {
		sv_catpvn(retval, "[", 1);
		/* omit "->" in $foo{bar}->[0], but not in ${$foo}->[0] */
		/*if (namelen > 0
		    && name[namelen-1] != ']' && name[namelen-1] != '}'
		    && (namelen < 4 || (name[1] != '{' && name[2] != '{')))*/
		if ((namelen > 0
		     && name[namelen-1] != ']' && name[namelen-1] != '}')
		    || (namelen > 4
		        && (name[1] == '{'
			    || (name[0] == '\\' && name[2] == '{'))))
		{
		    iname[inamelen++] = '-'; iname[inamelen++] = '>';
		    iname[inamelen] = '\0';
		}
	    }
	    if (iname[0] == '*' && iname[inamelen-1] == '}' && inamelen >= 8 &&
		(instr(iname+inamelen-8, "{SCALAR}") ||
		 instr(iname+inamelen-7, "{ARRAY}") ||
		 instr(iname+inamelen-6, "{HASH}"))) {
		iname[inamelen++] = '-'; iname[inamelen++] = '>';
	    }
	    iname[inamelen++] = '['; iname[inamelen] = '\0';
	    totpad = newSVsv(sep);
	    sv_catsv(totpad, pad);
	    sv_catsv(totpad, apad);

	    for (ix = 0; ix <= ixmax; ++ix) {
		STRLEN ilen;
		SV *elem;
		svp = av_fetch((AV*)ival, ix, FALSE);
		if (svp)
		    elem = *svp;
		else
		    elem = &PL_sv_undef;
		
		ilen = inamelen;
		sv_setiv(ixsv, ix);
#if PERL_VERSION < 10
                (void) sprintf(iname+ilen, "%"IVdf, (IV)ix);
		ilen = strlen(iname);
#else
                ilen = ilen + my_sprintf(iname+ilen, "%"IVdf, (IV)ix);
#endif
		iname[ilen++] = ']'; iname[ilen] = '\0';
		if (indent >= 3) {
		    sv_catsv(retval, totpad);
		    sv_catsv(retval, ipad);
		    sv_catpvn(retval, "#", 1);
		    sv_catsv(retval, ixsv);
		}
		sv_catsv(retval, totpad);
		sv_catsv(retval, ipad);
		DD_dump(aTHX_ elem, iname, ilen, retval, seenhv, postav,
			levelp,	indent, pad, xpad, apad, sep, pair,
			freezer, toaster, purity, deepcopy, quotekeys, bless,
			maxdepth, sortkeys);
		if (ix < ixmax)
		    sv_catpvn(retval, ",", 1);
	    }
	    if (ixmax >= 0) {
		SV * const opad = sv_x(aTHX_ Nullsv, SvPVX_const(xpad), SvCUR(xpad), (*levelp)-1);
		sv_catsv(retval, totpad);
		sv_catsv(retval, opad);
		SvREFCNT_dec(opad);
	    }
	    if (name[0] == '@')
		sv_catpvn(retval, ")", 1);
	    else
		sv_catpvn(retval, "]", 1);
	    SvREFCNT_dec(ixsv);
	    SvREFCNT_dec(totpad);
	    Safefree(iname);
	}
	else if (realtype == SVt_PVHV) {
	    SV *totpad, *newapad;
	    SV *sname;
	    HE *entry;
	    char *key;
	    I32 klen;
	    SV *hval;
	    AV *keys = NULL;
	
	    SV * const iname = newSVpvn(name, namelen);
	    if (name[0] == '%') {
		sv_catpvn(retval, "(", 1);
		(SvPVX(iname))[0] = '$';
	    }
	    else {
		sv_catpvn(retval, "{", 1);
		/* omit "->" in $foo[0]->{bar}, but not in ${$foo}->{bar} */
		if ((namelen > 0
		     && name[namelen-1] != ']' && name[namelen-1] != '}')
		    || (namelen > 4
		        && (name[1] == '{'
			    || (name[0] == '\\' && name[2] == '{'))))
		{
		    sv_catpvn(iname, "->", 2);
		}
	    }
	    if (name[0] == '*' && name[namelen-1] == '}' && namelen >= 8 &&
		(instr(name+namelen-8, "{SCALAR}") ||
		 instr(name+namelen-7, "{ARRAY}") ||
		 instr(name+namelen-6, "{HASH}"))) {
		sv_catpvn(iname, "->", 2);
	    }
	    sv_catpvn(iname, "{", 1);
	    totpad = newSVsv(sep);
	    sv_catsv(totpad, pad);
	    sv_catsv(totpad, apad);
	
	    /* If requested, get a sorted/filtered array of hash keys */
	    if (sortkeys) {
		if (sortkeys == &PL_sv_yes) {
#if PERL_VERSION < 8
                    sortkeys = sv_2mortal(newSVpvn("Data::Dumper::_sortkeys", 23));
#else
		    keys = newAV();
		    (void)hv_iterinit((HV*)ival);
		    while ((entry = hv_iternext((HV*)ival))) {
			sv = hv_iterkeysv(entry);
			SvREFCNT_inc(sv);
			av_push(keys, sv);
		    }
# ifdef USE_LOCALE_NUMERIC
		    sortsv(AvARRAY(keys), 
			   av_len(keys)+1, 
			   IN_LOCALE ? Perl_sv_cmp_locale : Perl_sv_cmp);
# else
		    sortsv(AvARRAY(keys), 
			   av_len(keys)+1, 
			   Perl_sv_cmp);
# endif
#endif
		}
		if (sortkeys != &PL_sv_yes) {
		    dSP; ENTER; SAVETMPS; PUSHMARK(sp);
		    XPUSHs(sv_2mortal(newRV_inc(ival))); PUTBACK;
		    i = perl_call_sv(sortkeys, G_SCALAR | G_EVAL);
		    SPAGAIN;
		    if (i) {
			sv = POPs;
			if (SvROK(sv) && (SvTYPE(SvRV(sv)) == SVt_PVAV))
			    keys = (AV*)SvREFCNT_inc(SvRV(sv));
		    }
		    if (! keys)
			warn("Sortkeys subroutine did not return ARRAYREF\n");
		    PUTBACK; FREETMPS; LEAVE;
		}
		if (keys)
		    sv_2mortal((SV*)keys);
	    }
	    else
		(void)hv_iterinit((HV*)ival);

            /* foreach (keys %hash) */
            for (i = 0; 1; i++) {
		char *nkey;
                char *nkey_buffer = NULL;
		I32 nticks = 0;
		SV* keysv;
		STRLEN keylen;
                I32 nlen;
		bool do_utf8 = FALSE;

               if (sortkeys) {
                   if (!(keys && (I32)i <= av_len(keys))) break;
               } else {
                   if (!(entry = hv_iternext((HV *)ival))) break;
               }

		if (i)
		    sv_catpvn(retval, ",", 1);

		if (sortkeys) {
		    char *key;
		    svp = av_fetch(keys, i, FALSE);
		    keysv = svp ? *svp : sv_mortalcopy(&PL_sv_undef);
		    key = SvPV(keysv, keylen);
		    svp = hv_fetch((HV*)ival, key,
                                   SvUTF8(keysv) ? -(I32)keylen : keylen, 0);
		    hval = svp ? *svp : sv_mortalcopy(&PL_sv_undef);
		}
		else {
		    keysv = hv_iterkeysv(entry);
		    hval = hv_iterval((HV*)ival, entry);
		}

		key = SvPV(keysv, keylen);
		do_utf8 = DO_UTF8(keysv);
		klen = keylen;

                sv_catsv(retval, totpad);
                sv_catsv(retval, ipad);
                /* old logic was first to check utf8 flag, and if utf8 always
                   call esc_q_utf8.  This caused test to break under -Mutf8,
                   because there even strings like 'c' have utf8 flag on.
                   Hence with quotekeys == 0 the XS code would still '' quote
                   them based on flags, whereas the perl code would not,
                   based on regexps.
                   The perl code is correct.
                   needs_quote() decides that anything that isn't a valid
                   perl identifier needs to be quoted, hence only correctly
                   formed strings with no characters outside [A-Za-z0-9_:]
                   won't need quoting.  None of those characters are used in
                   the byte encoding of utf8, so anything with utf8
                   encoded characters in will need quoting. Hence strings
                   with utf8 encoded characters in will end up inside do_utf8
                   just like before, but now strings with utf8 flag set but
                   only ascii characters will end up in the unquoted section.

                   There should also be less tests for the (probably currently)
                   more common doesn't need quoting case.
                   The code is also smaller (22044 vs 22260) because I've been
                   able to pull the common logic out to both sides.  */
                if (quotekeys || needs_quote(key)) {
                    if (do_utf8) {
                        STRLEN ocur = SvCUR(retval);
                        nlen = esc_q_utf8(aTHX_ retval, key, klen);
                        nkey = SvPVX(retval) + ocur;
                    }
                    else {
		        nticks = num_q(key, klen);
			New(0, nkey_buffer, klen+nticks+3, char);
                        nkey = nkey_buffer;
			nkey[0] = '\'';
			if (nticks)
			    klen += esc_q(nkey+1, key, klen);
			else
			    (void)Copy(key, nkey+1, klen, char);
			nkey[++klen] = '\'';
			nkey[++klen] = '\0';
                        nlen = klen;
                        sv_catpvn(retval, nkey, klen);
		    }
                }
                else {
                    nkey = key;
                    nlen = klen;
                    sv_catpvn(retval, nkey, klen);
		}
                sname = newSVsv(iname);
                sv_catpvn(sname, nkey, nlen);
                sv_catpvn(sname, "}", 1);

		sv_catsv(retval, pair);
		if (indent >= 2) {
		    char *extra;
		    I32 elen = 0;
		    newapad = newSVsv(apad);
		    New(0, extra, klen+4+1, char);
		    while (elen < (klen+4))
			extra[elen++] = ' ';
		    extra[elen] = '\0';
		    sv_catpvn(newapad, extra, elen);
		    Safefree(extra);
		}
		else
		    newapad = apad;

		DD_dump(aTHX_ hval, SvPVX_const(sname), SvCUR(sname), retval, seenhv,
			postav, levelp,	indent, pad, xpad, newapad, sep, pair,
			freezer, toaster, purity, deepcopy, quotekeys, bless,
			maxdepth, sortkeys);
		SvREFCNT_dec(sname);
		Safefree(nkey_buffer);
		if (indent >= 2)
		    SvREFCNT_dec(newapad);
	    }
	    if (i) {
		SV *opad = sv_x(aTHX_ Nullsv, SvPVX_const(xpad), SvCUR(xpad), *levelp-1);
		sv_catsv(retval, totpad);
		sv_catsv(retval, opad);
		SvREFCNT_dec(opad);
	    }
	    if (name[0] == '%')
		sv_catpvn(retval, ")", 1);
	    else
		sv_catpvn(retval, "}", 1);
	    SvREFCNT_dec(iname);
	    SvREFCNT_dec(totpad);
	}
	else if (realtype == SVt_PVCV) {
	    sv_catpvn(retval, "sub { \"DUMMY\" }", 15);
	    if (purity)
		warn("Encountered CODE ref, using dummy placeholder");
	}
	else {
	    warn("cannot handle ref type %ld", realtype);
	}

	if (realpack && !no_bless) {  /* free blessed allocs */
	    I32 plen;
	    I32 pticks;

	    if (indent >= 2) {
		SvREFCNT_dec(apad);
		apad = blesspad;
	    }
	    sv_catpvn(retval, ", '", 3);

	    plen = strlen(realpack);
	    pticks = num_q(realpack, plen);
	    if (pticks) { /* needs escaping */
	        char *npack;
	        char *npack_buffer = NULL;

	        New(0, npack_buffer, plen+pticks+1, char);
	        npack = npack_buffer;
	        plen += esc_q(npack, realpack, plen);
	        npack[plen] = '\0';

	        sv_catpvn(retval, npack, plen);
	        Safefree(npack_buffer);
	    }
	    else {
	        sv_catpvn(retval, realpack, strlen(realpack));
	    }
	    sv_catpvn(retval, "' )", 3);
	    if (toaster && SvPOK(toaster) && SvCUR(toaster)) {
		sv_catpvn(retval, "->", 2);
		sv_catsv(retval, toaster);
		sv_catpvn(retval, "()", 2);
	    }
	}
	SvREFCNT_dec(ipad);
	(*levelp)--;
    }
    else {
	STRLEN i;
	
	if (namelen) {
#ifdef DD_USE_OLD_ID_FORMAT
	    idlen = my_snprintf(id, sizeof(id), "0x%"UVxf, PTR2UV(val));
#else
	    id_buffer = PTR2UV(val);
	    idlen = sizeof(id_buffer);
#endif
	    if ((svp = hv_fetch(seenhv, id, idlen, FALSE)) &&
		(sv = *svp) && SvROK(sv) &&
		(seenentry = (AV*)SvRV(sv)))
	    {
		SV *othername;
		if ((svp = av_fetch(seenentry, 0, FALSE)) && (othername = *svp)
		    && (svp = av_fetch(seenentry, 2, FALSE)) && *svp && SvIV(*svp) > 0)
		{
		    sv_catpvn(retval, "${", 2);
		    sv_catsv(retval, othername);
		    sv_catpvn(retval, "}", 1);
		    return 1;
		}
	    }
	    else if (val != &PL_sv_undef) {
		SV * const namesv = newSVpvn("\\", 1);
		sv_catpvn(namesv, name, namelen);
		seenentry = newAV();
		av_push(seenentry, namesv);
		av_push(seenentry, newRV_inc(val));
		(void)hv_store(seenhv, id, idlen, newRV_inc((SV*)seenentry), 0);
		SvREFCNT_dec(seenentry);
	    }
	}

        if (DD_is_integer(val)) {
            STRLEN len;
	    if (SvIsUV(val))
	      len = my_snprintf(tmpbuf, sizeof(tmpbuf), "%"UVuf, SvUV(val));
	    else
	      len = my_snprintf(tmpbuf, sizeof(tmpbuf), "%"IVdf, SvIV(val));
            if (SvPOK(val)) {
              /* Need to check to see if this is a string such as " 0".
                 I'm assuming from sprintf isn't going to clash with utf8.
                 Is this valid on EBCDIC?  */
              STRLEN pvlen;
              const char * const pv = SvPV(val, pvlen);
              if (pvlen != len || memNE(pv, tmpbuf, len))
                goto integer_came_from_string;
            }
            if (len > 10) {
              /* Looks like we're on a 64 bit system.  Make it a string so that
                 if a 32 bit system reads the number it will cope better.  */
              sv_catpvf(retval, "'%s'", tmpbuf);
            } else
              sv_catpvn(retval, tmpbuf, len);
	}
	else if (realtype == SVt_PVGV) {/* GLOBs can end up with scribbly names */
	    c = SvPV(val, i);
	    ++c; --i;			/* just get the name */
	    if (i >= 6 && strncmp(c, "main::", 6) == 0) {
		c += 4;
		i -= 4;
	    }
	    if (needs_quote(c)) {
		sv_grow(retval, SvCUR(retval)+6+2*i);
		r = SvPVX(retval)+SvCUR(retval);
		r[0] = '*'; r[1] = '{';	r[2] = '\'';
		i += esc_q(r+3, c, i);
		i += 3;
		r[i++] = '\''; r[i++] = '}';
		r[i] = '\0';
	    }
	    else {
		sv_grow(retval, SvCUR(retval)+i+2);
		r = SvPVX(retval)+SvCUR(retval);
		r[0] = '*'; strcpy(r+1, c);
		i++;
	    }
	    SvCUR_set(retval, SvCUR(retval)+i);

	    if (purity) {
		static const char* const entries[] = { "{SCALAR}", "{ARRAY}", "{HASH}" };
		static const STRLEN sizes[] = { 8, 7, 6 };
		SV *e;
		SV * const nname = newSVpvn("", 0);
		SV * const newapad = newSVpvn("", 0);
		GV * const gv = (GV*)val;
		I32 j;
		
		for (j=0; j<3; j++) {
		    e = ((j == 0) ? GvSV(gv) : (j == 1) ? (SV*)GvAV(gv) : (SV*)GvHV(gv));
		    if (!e)
			continue;
		    if (j == 0 && !SvOK(e))
			continue;

		    {
			I32 nlevel = 0;
			SV *postentry = newSVpvn(r,i);
			
			sv_setsv(nname, postentry);
			sv_catpvn(nname, entries[j], sizes[j]);
			sv_catpvn(postentry, " = ", 3);
			av_push(postav, postentry);
			e = newRV_inc(e);
			
			SvCUR_set(newapad, 0);
			if (indent >= 2)
			    (void)sv_x(aTHX_ newapad, " ", 1, SvCUR(postentry));
			
			DD_dump(aTHX_ e, SvPVX_const(nname), SvCUR(nname), postentry,
				seenhv, postav, &nlevel, indent, pad, xpad,
				newapad, sep, pair, freezer, toaster, purity,
				deepcopy, quotekeys, bless, maxdepth, 
				sortkeys);
			SvREFCNT_dec(e);
		    }
		}
		
		SvREFCNT_dec(newapad);
		SvREFCNT_dec(nname);
	    }
	}
	else if (val == &PL_sv_undef || !SvOK(val)) {
	    sv_catpvn(retval, "undef", 5);
	}
	else {
        integer_came_from_string:
	    c = SvPV(val, i);
	    if (DO_UTF8(val))
	        i += esc_q_utf8(aTHX_ retval, c, i);
	    else {
		sv_grow(retval, SvCUR(retval)+3+2*i); /* 3: ""\0 */
		r = SvPVX(retval) + SvCUR(retval);
		r[0] = '\'';
		i += esc_q(r+1, c, i);
		++i;
		r[i++] = '\'';
		r[i] = '\0';
		SvCUR_set(retval, SvCUR(retval)+i);
	    }
	}
    }

    if (idlen) {
	if (deepcopy)
	    (void)hv_delete(seenhv, id, idlen, G_DISCARD);
	else if (namelen && seenentry) {
	    SV *mark = *av_fetch(seenentry, 2, TRUE);
	    sv_setiv(mark,1);
	}
    }
    return 1;
}


MODULE = Data::Dumper		PACKAGE = Data::Dumper         PREFIX = Data_Dumper_

#
# This is the exact equivalent of Dump.  Well, almost. The things that are
# different as of now (due to Laziness):
#   * doesnt do double-quotes yet.
#

void
Data_Dumper_Dumpxs(href, ...)
	SV	*href;
	PROTOTYPE: $;$$
	PPCODE:
	{
	    HV *hv;
	    SV *retval, *valstr;
	    HV *seenhv = NULL;
	    AV *postav, *todumpav, *namesav;
	    I32 level = 0;
	    I32 indent, terse, i, imax, postlen;
	    SV **svp;
	    SV *val, *name, *pad, *xpad, *apad, *sep, *pair, *varname;
	    SV *freezer, *toaster, *bless, *sortkeys;
	    I32 purity, deepcopy, quotekeys, maxdepth = 0;
	    char tmpbuf[1024];
	    I32 gimme = GIMME;

	    if (!SvROK(href)) {		/* call new to get an object first */
		if (items < 2)
		    croak("Usage: Data::Dumper::Dumpxs(PACKAGE, VAL_ARY_REF, [NAME_ARY_REF])");
		
		ENTER;
		SAVETMPS;
		
		PUSHMARK(sp);
		XPUSHs(href);
		XPUSHs(sv_2mortal(newSVsv(ST(1))));
		if (items >= 3)
		    XPUSHs(sv_2mortal(newSVsv(ST(2))));
		PUTBACK;
		i = perl_call_method("new", G_SCALAR);
		SPAGAIN;
		if (i)
		    href = newSVsv(POPs);

		PUTBACK;
		FREETMPS;
		LEAVE;
		if (i)
		    (void)sv_2mortal(href);
	    }

	    todumpav = namesav = NULL;
	    seenhv = NULL;
	    val = pad = xpad = apad = sep = pair = varname
		= freezer = toaster = bless = sortkeys = &PL_sv_undef;
	    name = sv_newmortal();
	    indent = 2;
	    terse = purity = deepcopy = 0;
	    quotekeys = 1;
	
	    retval = newSVpvn("", 0);
	    if (SvROK(href)
		&& (hv = (HV*)SvRV((SV*)href))
		&& SvTYPE(hv) == SVt_PVHV)		{

		if ((svp = hv_fetch(hv, "seen", 4, FALSE)) && SvROK(*svp))
		    seenhv = (HV*)SvRV(*svp);
		if ((svp = hv_fetch(hv, "todump", 6, FALSE)) && SvROK(*svp))
		    todumpav = (AV*)SvRV(*svp);
		if ((svp = hv_fetch(hv, "names", 5, FALSE)) && SvROK(*svp))
		    namesav = (AV*)SvRV(*svp);
		if ((svp = hv_fetch(hv, "indent", 6, FALSE)))
		    indent = SvIV(*svp);
		if ((svp = hv_fetch(hv, "purity", 6, FALSE)))
		    purity = SvIV(*svp);
		if ((svp = hv_fetch(hv, "terse", 5, FALSE)))
		    terse = SvTRUE(*svp);
#if 0 /* useqq currently unused */
		if ((svp = hv_fetch(hv, "useqq", 5, FALSE)))
		    useqq = SvTRUE(*svp);
#endif
		if ((svp = hv_fetch(hv, "pad", 3, FALSE)))
		    pad = *svp;
		if ((svp = hv_fetch(hv, "xpad", 4, FALSE)))
		    xpad = *svp;
		if ((svp = hv_fetch(hv, "apad", 4, FALSE)))
		    apad = *svp;
		if ((svp = hv_fetch(hv, "sep", 3, FALSE)))
		    sep = *svp;
		if ((svp = hv_fetch(hv, "pair", 4, FALSE)))
		    pair = *svp;
		if ((svp = hv_fetch(hv, "varname", 7, FALSE)))
		    varname = *svp;
		if ((svp = hv_fetch(hv, "freezer", 7, FALSE)))
		    freezer = *svp;
		if ((svp = hv_fetch(hv, "toaster", 7, FALSE)))
		    toaster = *svp;
		if ((svp = hv_fetch(hv, "deepcopy", 8, FALSE)))
		    deepcopy = SvTRUE(*svp);
		if ((svp = hv_fetch(hv, "quotekeys", 9, FALSE)))
		    quotekeys = SvTRUE(*svp);
		if ((svp = hv_fetch(hv, "bless", 5, FALSE)))
		    bless = *svp;
		if ((svp = hv_fetch(hv, "maxdepth", 8, FALSE)))
		    maxdepth = SvIV(*svp);
		if ((svp = hv_fetch(hv, "sortkeys", 8, FALSE))) {
		    sortkeys = *svp;
		    if (! SvTRUE(sortkeys))
			sortkeys = NULL;
		    else if (! (SvROK(sortkeys) &&
				SvTYPE(SvRV(sortkeys)) == SVt_PVCV) )
		    {
			/* flag to use qsortsv() for sorting hash keys */	
			sortkeys = &PL_sv_yes; 
		    }
		}
		postav = newAV();

		if (todumpav)
		    imax = av_len(todumpav);
		else
		    imax = -1;
		valstr = newSVpvn("",0);
		for (i = 0; i <= imax; ++i) {
		    SV *newapad;
		
		    av_clear(postav);
		    if ((svp = av_fetch(todumpav, i, FALSE)))
			val = *svp;
		    else
			val = &PL_sv_undef;
		    if ((svp = av_fetch(namesav, i, TRUE))) {
			sv_setsv(name, *svp);
			if (SvOK(*svp) && !SvPOK(*svp))
			    (void)SvPV_nolen_const(name);
		    }
		    else
			(void)SvOK_off(name);
		
		    if (SvPOK(name)) {
			if ((SvPVX_const(name))[0] == '*') {
			    if (SvROK(val)) {
				switch (SvTYPE(SvRV(val))) {
				case SVt_PVAV:
				    (SvPVX(name))[0] = '@';
				    break;
				case SVt_PVHV:
				    (SvPVX(name))[0] = '%';
				    break;
				case SVt_PVCV:
				    (SvPVX(name))[0] = '*';
				    break;
				default:
				    (SvPVX(name))[0] = '$';
				    break;
				}
			    }
			    else
				(SvPVX(name))[0] = '$';
			}
			else if ((SvPVX_const(name))[0] != '$')
			    sv_insert(name, 0, 0, "$", 1);
		    }
		    else {
			STRLEN nchars;
			sv_setpvn(name, "$", 1);
			sv_catsv(name, varname);
			nchars = my_snprintf(tmpbuf, sizeof(tmpbuf), "%"IVdf, (IV)(i+1));
			sv_catpvn(name, tmpbuf, nchars);
		    }
		
		    if (indent >= 2) {
			SV * const tmpsv = sv_x(aTHX_ NULL, " ", 1, SvCUR(name)+3);
			newapad = newSVsv(apad);
			sv_catsv(newapad, tmpsv);
			SvREFCNT_dec(tmpsv);
		    }
		    else
			newapad = apad;
		
		    DD_dump(aTHX_ val, SvPVX_const(name), SvCUR(name), valstr, seenhv,
			    postav, &level, indent, pad, xpad, newapad, sep, pair,
			    freezer, toaster, purity, deepcopy, quotekeys,
			    bless, maxdepth, sortkeys);
		
		    if (indent >= 2)
			SvREFCNT_dec(newapad);

		    postlen = av_len(postav);
		    if (postlen >= 0 || !terse) {
			sv_insert(valstr, 0, 0, " = ", 3);
			sv_insert(valstr, 0, 0, SvPVX_const(name), SvCUR(name));
			sv_catpvn(valstr, ";", 1);
		    }
		    sv_catsv(retval, pad);
		    sv_catsv(retval, valstr);
		    sv_catsv(retval, sep);
		    if (postlen >= 0) {
			I32 i;
			sv_catsv(retval, pad);
			for (i = 0; i <= postlen; ++i) {
			    SV *elem;
			    svp = av_fetch(postav, i, FALSE);
			    if (svp && (elem = *svp)) {
				sv_catsv(retval, elem);
				if (i < postlen) {
				    sv_catpvn(retval, ";", 1);
				    sv_catsv(retval, sep);
				    sv_catsv(retval, pad);
				}
			    }
			}
			sv_catpvn(retval, ";", 1);
			    sv_catsv(retval, sep);
		    }
		    sv_setpvn(valstr, "", 0);
		    if (gimme == G_ARRAY) {
			XPUSHs(sv_2mortal(retval));
			if (i < imax)	/* not the last time thro ? */
			    retval = newSVpvn("",0);
		    }
		}
		SvREFCNT_dec(postav);
		SvREFCNT_dec(valstr);
	    }
	    else
		croak("Call to new() method failed to return HASH ref");
	    if (gimme == G_SCALAR)
		XPUSHs(sv_2mortal(retval));
	}
