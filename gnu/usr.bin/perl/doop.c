/*    doop.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "'So that was the job I felt I had to do when I started,' thought Sam."
 */

#include "EXTERN.h"
#include "perl.h"

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

#ifdef BUGGY_MSC
 #pragma function(memcmp)
#endif /* BUGGY_MSC */

#ifdef BUGGY_MSC
 #pragma intrinsic(memcmp)
#endif /* BUGGY_MSC */

I32
do_trans(sv,arg)
SV *sv;
OP *arg;
{
    register short *tbl;
    register U8 *s;
    register U8 *send;
    register U8 *d;
    register I32 ch;
    register I32 matches = 0;
    register I32 squash = op->op_private & OPpTRANS_SQUASH;
    STRLEN len;

    if (SvREADONLY(sv))
	croak(no_modify);
    tbl = (short*)cPVOP->op_pv;
    s = (U8*)SvPV(sv, len);
    if (!len)
	return 0;
    if (!SvPOKp(sv))
	s = (U8*)SvPV_force(sv, len);
    (void)SvPOK_only(sv);
    send = s + len;
    if (!tbl || !s)
	croak("panic: do_trans");
    DEBUG_t( deb("2.TBL\n"));
    if (!op->op_private) {
	while (s < send) {
	    if ((ch = tbl[*s]) >= 0) {
		matches++;
		*s = ch;
	    }
	    s++;
	}
    }
    else {
	d = s;
	while (s < send) {
	    if ((ch = tbl[*s]) >= 0) {
		*d = ch;
		if (matches++ && squash) {
		    if (d[-1] == *d)
			matches--;
		    else
			d++;
		}
		else
		    d++;
	    }
	    else if (ch == -1)		/* -1 is unmapped character */
		*d++ = *s;		/* -2 is delete character */
	    s++;
	}
	matches += send - d;	/* account for disappeared chars */
	*d = '\0';
	SvCUR_set(sv, d - (U8*)SvPVX(sv));
    }
    SvSETMAGIC(sv);
    return matches;
}

void
do_join(sv,del,mark,sp)
register SV *sv;
SV *del;
register SV **mark;
register SV **sp;
{
    SV **oldmark = mark;
    register I32 items = sp - mark;
    register STRLEN len;
    STRLEN delimlen;
    register char *delim = SvPV(del, delimlen);
    STRLEN tmplen;

    mark++;
    len = (items > 0 ? (delimlen * (items - 1) ) : 0);
    if (SvTYPE(sv) < SVt_PV)
	sv_upgrade(sv, SVt_PV);
    if (SvLEN(sv) < len + items) {	/* current length is way too short */
	while (items-- > 0) {
	    if (*mark) {
		SvPV(*mark, tmplen);
		len += tmplen;
	    }
	    mark++;
	}
	SvGROW(sv, len + 1);		/* so try to pre-extend */

	mark = oldmark;
	items = sp - mark;;
	++mark;
    }

    if (items-- > 0) {
	char *s;

	if (*mark) {
	    s = SvPV(*mark, tmplen);
	    sv_setpvn(sv, s, tmplen);
	}
	else
	    sv_setpv(sv, "");
	mark++;
    }
    else
	sv_setpv(sv,"");
    len = delimlen;
    if (len) {
	for (; items > 0; items--,mark++) {
	    sv_catpvn(sv,delim,len);
	    sv_catsv(sv,*mark);
	}
    }
    else {
	for (; items > 0; items--,mark++)
	    sv_catsv(sv,*mark);
    }
    SvSETMAGIC(sv);
}

void
do_sprintf(sv,len,sarg)
register SV *sv;
register I32 len;
register SV **sarg;
{
    register char *s;
    register char *t;
    register char *f;
    bool dolong;
#ifdef HAS_QUAD
    bool doquad;
#endif /* HAS_QUAD */
    char ch;
    register char *send;
    register SV *arg;
    char *xs;
    I32 xlen;
    I32 pre;
    I32 post;
    double value;
    STRLEN arglen;

    sv_setpv(sv,"");
    len--;			/* don't count pattern string */
    t = s = SvPV(*sarg, arglen);	/* XXX Don't know t is writeable */
    send = s + arglen;
    sarg++;
    for ( ; ; len--) {

	/*SUPPRESS 560*/
	if (len <= 0 || !(arg = *sarg++))
	    arg = &sv_no;

	/*SUPPRESS 530*/
	for ( ; t < send && *t != '%'; t++) ;
	if (t >= send)
	    break;		/* end of run_format string, ignore extra args */
	f = t;
	*buf = '\0';
	xs = buf;
#ifdef HAS_QUAD
	doquad =
#endif /* HAS_QUAD */
	dolong = FALSE;
	pre = post = 0;
	for (t++; t < send; t++) {
	    switch (*t) {
	    default:
		ch = *(++t);
		*t = '\0';
		(void)sprintf(xs,f);
		len++, sarg--;
		xlen = strlen(xs);
		break;
	    case 'n': case '*':
		croak("Use of %c in printf format not supported", *t);

	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9': 
	    case '.': case '#': case '-': case '+': case ' ':
		continue;
	    case 'l':
#ifdef HAS_QUAD
		if (dolong) {
		    dolong = FALSE;
		    doquad = TRUE;
		} else
#endif
		dolong = TRUE;
		continue;
	    case 'c':
		ch = *(++t);
		*t = '\0';
		xlen = SvIV(arg);
		if (strEQ(f,"%c")) { /* some printfs fail on null chars */
		    *xs = xlen;
		    xs[1] = '\0';
		    xlen = 1;
		}
		else {
		    (void)sprintf(xs,f,xlen);
		    xlen = strlen(xs);
		}
		break;
	    case 'D':
		dolong = TRUE;
		/* FALL THROUGH */
	    case 'd':
		ch = *(++t);
		*t = '\0';
#ifdef HAS_QUAD
		if (doquad)
		    (void)sprintf(buf,s,(Quad_t)SvNV(arg));
		else
#endif
		if (dolong)
		    (void)sprintf(xs,f,(long)SvNV(arg));
		else
		    (void)sprintf(xs,f,SvIV(arg));
		xlen = strlen(xs);
		break;
	    case 'X': case 'O':
		dolong = TRUE;
		/* FALL THROUGH */
	    case 'x': case 'o': case 'u':
		ch = *(++t);
		*t = '\0';
		value = SvNV(arg);
#ifdef HAS_QUAD
		if (doquad)
		    (void)sprintf(buf,s,(unsigned Quad_t)value);
		else
#endif
		if (dolong)
		    (void)sprintf(xs,f,U_L(value));
		else
		    (void)sprintf(xs,f,U_I(value));
		xlen = strlen(xs);
		break;
	    case 'E': case 'e': case 'f': case 'G': case 'g':
		ch = *(++t);
		*t = '\0';
		(void)sprintf(xs,f,SvNV(arg));
		xlen = strlen(xs);
		break;
	    case 's':
		ch = *(++t);
		*t = '\0';
		xs = SvPV(arg, arglen);
		xlen = (I32)arglen;
		if (strEQ(f,"%s")) {	/* some printfs fail on >128 chars */
		    break;		/* so handle simple cases */
		}
		else if (f[1] == '-') {
		    char *mp = strchr(f, '.');
		    I32 min = atoi(f+2);

		    if (mp) {
			I32 max = atoi(mp+1);

			if (xlen > max)
			    xlen = max;
		    }
		    if (xlen < min)
			post = min - xlen;
		    break;
		}
		else if (isDIGIT(f[1])) {
		    char *mp = strchr(f, '.');
		    I32 min = atoi(f+1);

		    if (mp) {
			I32 max = atoi(mp+1);

			if (xlen > max)
			    xlen = max;
		    }
		    if (xlen < min)
			pre = min - xlen;
		    break;
		}
		strcpy(tokenbuf+64,f);	/* sprintf($s,...$s...) */
		*t = ch;
		(void)sprintf(buf,tokenbuf+64,xs);
		xs = buf;
		xlen = strlen(xs);
		break;
	    }
	    /* end of switch, copy results */
	    *t = ch;
	    if (xs == buf && xlen >= sizeof(buf)) {	/* Ooops! */
		fputs("panic: sprintf overflow - memory corrupted!\n",stderr);
		my_exit(1);
	    }
	    SvGROW(sv, SvCUR(sv) + (f - s) + xlen + 1 + pre + post);
	    sv_catpvn(sv, s, f - s);
	    if (pre) {
		repeatcpy(SvPVX(sv) + SvCUR(sv), " ", 1, pre);
		SvCUR(sv) += pre;
	    }
	    sv_catpvn(sv, xs, xlen);
	    if (post) {
		repeatcpy(SvPVX(sv) + SvCUR(sv), " ", 1, post);
		SvCUR(sv) += post;
	    }
	    s = t;
	    break;		/* break from for loop */
	}
    }
    sv_catpvn(sv, s, t - s);
    SvSETMAGIC(sv);
}

void
do_vecset(sv)
SV *sv;
{
    SV *targ = LvTARG(sv);
    register I32 offset;
    register I32 size;
    register unsigned char *s;
    register unsigned long lval;
    I32 mask;
    STRLEN targlen;
    STRLEN len;

    if (!targ)
	return;
    s = (unsigned char*)SvPV_force(targ, targlen);
    lval = U_L(SvNV(sv));
    offset = LvTARGOFF(sv);
    size = LvTARGLEN(sv);
    
    len = (offset + size + 7) / 8;
    if (len > targlen) {
	s = (unsigned char*)SvGROW(targ, len + 1);
	(void)memzero(s + targlen, len - targlen + 1);
	SvCUR_set(targ, len);
    }
    
    if (size < 8) {
	mask = (1 << size) - 1;
	size = offset & 7;
	lval &= mask;
	offset >>= 3;
	s[offset] &= ~(mask << size);
	s[offset] |= lval << size;
    }
    else {
	offset >>= 3;
	if (size == 8)
	    s[offset] = lval & 255;
	else if (size == 16) {
	    s[offset] = (lval >> 8) & 255;
	    s[offset+1] = lval & 255;
	}
	else if (size == 32) {
	    s[offset] = (lval >> 24) & 255;
	    s[offset+1] = (lval >> 16) & 255;
	    s[offset+2] = (lval >> 8) & 255;
	    s[offset+3] = lval & 255;
	}
    }
}

void
do_chop(astr,sv)
register SV *astr;
register SV *sv;
{
    STRLEN len;
    char *s;
    
    if (SvTYPE(sv) == SVt_PVAV) {
	register I32 i;
        I32 max;
	AV* av = (AV*)sv;
        max = AvFILL(av);
        for (i = 0; i <= max; i++) {
	    sv = (SV*)av_fetch(av, i, FALSE);
	    if (sv && ((sv = *(SV**)sv), sv != &sv_undef))
		do_chop(astr, sv);
	}
        return;
    }
    if (SvTYPE(sv) == SVt_PVHV) {
        HV* hv = (HV*)sv;
	HE* entry;
        (void)hv_iterinit(hv);
        /*SUPPRESS 560*/
        while (entry = hv_iternext(hv))
            do_chop(astr,hv_iterval(hv,entry));
        return;
    }
    s = SvPV(sv, len);
    if (len && !SvPOK(sv))
	s = SvPV_force(sv, len);
    if (s && len) {
	s += --len;
	sv_setpvn(astr, s, 1);
	*s = '\0';
	SvCUR_set(sv, len);
	SvNIOK_off(sv);
    }
    else
	sv_setpvn(astr, "", 0);
    SvSETMAGIC(sv);
} 

I32
do_chomp(sv)
register SV *sv;
{
    register I32 count;
    STRLEN len;
    char *s;

    if (RsSNARF(rs))
	return 0;
    count = 0;
    if (SvTYPE(sv) == SVt_PVAV) {
	register I32 i;
        I32 max;
	AV* av = (AV*)sv;
        max = AvFILL(av);
        for (i = 0; i <= max; i++) {
	    sv = (SV*)av_fetch(av, i, FALSE);
	    if (sv && ((sv = *(SV**)sv), sv != &sv_undef))
		count += do_chomp(sv);
	}
        return count;
    }
    if (SvTYPE(sv) == SVt_PVHV) {
        HV* hv = (HV*)sv;
	HE* entry;
        (void)hv_iterinit(hv);
        /*SUPPRESS 560*/
        while (entry = hv_iternext(hv))
            count += do_chomp(hv_iterval(hv,entry));
        return count;
    }
    s = SvPV(sv, len);
    if (len && !SvPOKp(sv))
	s = SvPV_force(sv, len);
    if (s && len) {
	s += --len;
	if (RsPARA(rs)) {
	    if (*s != '\n')
		goto nope;
	    ++count;
	    while (len && s[-1] == '\n') {
		--len;
		--s;
		++count;
	    }
	}
	else {
	    STRLEN rslen;
	    char *rsptr = SvPV(rs, rslen);
	    if (rslen == 1) {
		if (*s != *rsptr)
		    goto nope;
		++count;
	    }
	    else {
		if (len < rslen)
		    goto nope;
		len -= rslen - 1;
		s -= rslen - 1;
		if (bcmp(s, rsptr, rslen))
		    goto nope;
		count += rslen;
	    }
	}
	*s = '\0';
	SvCUR_set(sv, len);
	SvNIOK_off(sv);
    }
  nope:
    SvSETMAGIC(sv);
    return count;
} 

void
do_vop(optype,sv,left,right)
I32 optype;
SV *sv;
SV *left;
SV *right;
{
#ifdef LIBERAL
    register long *dl;
    register long *ll;
    register long *rl;
#endif
    register char *dc;
    STRLEN leftlen;
    STRLEN rightlen;
    register char *lc = SvPV(left, leftlen);
    register char *rc = SvPV(right, rightlen);
    register I32 len;
    I32 lensave;

    dc = SvPV_force(sv,na);
    len = leftlen < rightlen ? leftlen : rightlen;
    lensave = len;
    if (SvCUR(sv) < len) {
	dc = SvGROW(sv,len + 1);
	(void)memzero(dc + SvCUR(sv), len - SvCUR(sv) + 1);
    }
    SvCUR_set(sv, len);
    (void)SvPOK_only(sv);
#ifdef LIBERAL
    if (len >= sizeof(long)*4 &&
	!((long)dc % sizeof(long)) &&
	!((long)lc % sizeof(long)) &&
	!((long)rc % sizeof(long)))	/* It's almost always aligned... */
    {
	I32 remainder = len % (sizeof(long)*4);
	len /= (sizeof(long)*4);

	dl = (long*)dc;
	ll = (long*)lc;
	rl = (long*)rc;

	switch (optype) {
	case OP_BIT_AND:
	    while (len--) {
		*dl++ = *ll++ & *rl++;
		*dl++ = *ll++ & *rl++;
		*dl++ = *ll++ & *rl++;
		*dl++ = *ll++ & *rl++;
	    }
	    break;
	case OP_BIT_XOR:
	    while (len--) {
		*dl++ = *ll++ ^ *rl++;
		*dl++ = *ll++ ^ *rl++;
		*dl++ = *ll++ ^ *rl++;
		*dl++ = *ll++ ^ *rl++;
	    }
	    break;
	case OP_BIT_OR:
	    while (len--) {
		*dl++ = *ll++ | *rl++;
		*dl++ = *ll++ | *rl++;
		*dl++ = *ll++ | *rl++;
		*dl++ = *ll++ | *rl++;
	    }
	}

	dc = (char*)dl;
	lc = (char*)ll;
	rc = (char*)rl;

	len = remainder;
    }
#endif
    {
	char *lsave = lc;
	char *rsave = rc;
	
	switch (optype) {
	case OP_BIT_AND:
	    while (len--)
		*dc++ = *lc++ & *rc++;
	    break;
	case OP_BIT_XOR:
	    while (len--)
		*dc++ = *lc++ ^ *rc++;
	    goto mop_up;
	case OP_BIT_OR:
	    while (len--)
		*dc++ = *lc++ | *rc++;
	  mop_up:
	    len = lensave;
	    if (rightlen > len)
		sv_catpvn(sv, rsave + len, rightlen - len);
	    else if (leftlen > len)
		sv_catpvn(sv, lsave + len, leftlen - len);
	    else
		*SvEND(sv) = '\0';
	    break;
	}
    }
}

OP *
do_kv(ARGS)
dARGS
{
    dSP;
    HV *hv = (HV*)POPs;
    I32 i;
    register HE *entry;
    char *tmps;
    SV *tmpstr;
    I32 dokeys =   (op->op_type == OP_KEYS);
    I32 dovalues = (op->op_type == OP_VALUES);

    if (op->op_type == OP_RV2HV || op->op_type == OP_PADHV) 
	dokeys = dovalues = TRUE;

    if (!hv)
	RETURN;

    (void)hv_iterinit(hv);	/* always reset iterator regardless */

    if (GIMME != G_ARRAY) {
	dTARGET;

	if (!SvRMAGICAL(hv) || !mg_find((SV*)hv,'P'))
	    i = HvKEYS(hv);
	else {
	    i = 0;
	    /*SUPPRESS 560*/
	    while (entry = hv_iternext(hv)) {
		i++;
	    }
	}
	PUSHi( i );
	RETURN;
    }

    /* Guess how much room we need.  hv_max may be a few too many.  Oh well. */
    EXTEND(sp, HvMAX(hv) * (dokeys + dovalues));

    PUTBACK;	/* hv_iternext and hv_iterval might clobber stack_sp */
    while (entry = hv_iternext(hv)) {
	SPAGAIN;
	if (dokeys) {
	    tmps = hv_iterkey(entry,&i);	/* won't clobber stack_sp */
	    if (!i)
		tmps = "";
	    XPUSHs(sv_2mortal(newSVpv(tmps,i)));
	}
	if (dovalues) {
	    tmpstr = NEWSV(45,0);
	    PUTBACK;
	    sv_setsv(tmpstr,hv_iterval(hv,entry));
	    SPAGAIN;
	    DEBUG_H( {
		sprintf(buf,"%d%%%d=%d\n",entry->hent_hash,
		    HvMAX(hv)+1,entry->hent_hash & HvMAX(hv));
		sv_setpv(tmpstr,buf);
	    } )
	    XPUSHs(sv_2mortal(tmpstr));
	}
	PUTBACK;
    }
    return NORMAL;
}

