/*    pp_hot.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * Then he heard Merry change the note, and up went the Horn-cry of Buckland,
 * shaking the air.
 *
 *            Awake!  Awake!  Fear, Fire, Foes!  Awake!
 *                     Fire, Foes!  Awake!
 */

#include "EXTERN.h"
#include "perl.h"

/* Hot code. */

PP(pp_const)
{
    dSP;
    XPUSHs(cSVOP->op_sv);
    RETURN;
}

PP(pp_nextstate)
{
    curcop = (COP*)op;
    TAINT_NOT;		/* Each statement is presumed innocent */
    stack_sp = stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREETMPS;
    return NORMAL;
}

PP(pp_gvsv)
{
    dSP;
    EXTEND(sp,1);
    if (op->op_private & OPpLVAL_INTRO)
	PUSHs(save_scalar(cGVOP->op_gv));
    else
	PUSHs(GvSV(cGVOP->op_gv));
    RETURN;
}

PP(pp_null)
{
    return NORMAL;
}

PP(pp_pushmark)
{
    PUSHMARK(stack_sp);
    return NORMAL;
}

PP(pp_stringify)
{
    dSP; dTARGET;
    STRLEN len;
    char *s;
    s = SvPV(TOPs,len);
    sv_setpvn(TARG,s,len);
    SETTARG;
    RETURN;
}

PP(pp_gv)
{
    dSP;
    XPUSHs((SV*)cGVOP->op_gv);
    RETURN;
}

PP(pp_gelem)
{
    GV *gv;
    SV *sv;
    SV *ref;
    char *elem;
    dSP;

    sv = POPs;
    elem = SvPV(sv, na);
    gv = (GV*)POPs;
    ref = Nullsv;
    sv = Nullsv;
    switch (elem ? *elem : '\0')
    {
    case 'A':
	if (strEQ(elem, "ARRAY"))
	    ref = (SV*)GvAV(gv);
	break;
    case 'C':
	if (strEQ(elem, "CODE"))
	    ref = (SV*)GvCV(gv);
	break;
    case 'F':
	if (strEQ(elem, "FILEHANDLE"))
	    ref = (SV*)GvIOp(gv);
	break;
    case 'G':
	if (strEQ(elem, "GLOB"))
	    ref = (SV*)gv;
	break;
    case 'H':
	if (strEQ(elem, "HASH"))
	    ref = (SV*)GvHV(gv);
	break;
    case 'N':
	if (strEQ(elem, "NAME"))
	    sv = newSVpv(GvNAME(gv), GvNAMELEN(gv));
	break;
    case 'P':
	if (strEQ(elem, "PACKAGE"))
	    sv = newSVpv(HvNAME(GvSTASH(gv)), 0);
	break;
    case 'S':
	if (strEQ(elem, "SCALAR"))
	    ref = GvSV(gv);
	break;
    }
    if (ref)
	sv = newRV(ref);
    if (sv)
	sv_2mortal(sv);
    else
	sv = &sv_undef;
    XPUSHs(sv);
    RETURN;
}

PP(pp_and)
{
    dSP;
    if (!SvTRUE(TOPs))
	RETURN;
    else {
	--SP;
	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_sassign)
{
    dSP; dPOPTOPssrl;
    MAGIC *mg;

    if (op->op_private & OPpASSIGN_BACKWARDS) {
	SV *temp;
	temp = left; left = right; right = temp;
    }
    if (tainting && tainted && (!SvGMAGICAL(left) || !SvSMAGICAL(left) ||
				!((mg = mg_find(left, 't')) && mg->mg_len & 1)))
    {
	TAINT_NOT;
    }
    SvSetSV(right, left);
    SvSETMAGIC(right);
    SETs(right);
    RETURN;
}

PP(pp_cond_expr)
{
    dSP;
    if (SvTRUEx(POPs))
	RETURNOP(cCONDOP->op_true);
    else
	RETURNOP(cCONDOP->op_false);
}

PP(pp_unstack)
{
    I32 oldsave;
    TAINT_NOT;		/* Each statement is presumed innocent */
    stack_sp = stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREETMPS;
    oldsave = scopestack[scopestack_ix - 1];
    LEAVE_SCOPE(oldsave);
    return NORMAL;
}

PP(pp_seq)
{
    dSP; tryAMAGICbinSET(seq,0); 
    {
      dPOPTOPssrl;
      SETs( sv_eq(left, right) ? &sv_yes : &sv_no );
      RETURN;
    }
}

PP(pp_concat)
{
  dSP; dATARGET; tryAMAGICbin(concat,opASSIGN);
  {
    dPOPTOPssrl;
    STRLEN len;
    char *s;
    if (TARG != left) {
	s = SvPV(left,len);
	sv_setpvn(TARG,s,len);
    }
    else if (SvGMAGICAL(TARG))
	mg_get(TARG);
    else if (!SvOK(TARG)) {
	s = SvPV_force(TARG, len);
	sv_setpv(TARG, "");	/* Suppress warning. */
    }
    s = SvPV(right,len);
    sv_catpvn(TARG,s,len);
    SETTARG;
    RETURN;
  }
}

PP(pp_padsv)
{
    dSP; dTARGET;
    XPUSHs(TARG);
    if (op->op_flags & OPf_MOD) {
	if (op->op_private & OPpLVAL_INTRO)
	    SAVECLEARSV(curpad[op->op_targ]);
        else if (op->op_private & (OPpDEREF_HV|OPpDEREF_AV))
	    provide_ref(op, curpad[op->op_targ]);
    }
    RETURN;
}

PP(pp_readline)
{
    last_in_gv = (GV*)(*stack_sp--);
    return do_readline();
}

PP(pp_eq)
{
    dSP; tryAMAGICbinSET(eq,0); 
    {
      dPOPnv;
      SETs((TOPn == value) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_preinc)
{
    dSP;
    if (SvIOK(TOPs)) {
	++SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVf_NOK|SVf_POK|SVp_NOK|SVp_POK);
    }
    else
	sv_inc(TOPs);
    SvSETMAGIC(TOPs);
    return NORMAL;
}

PP(pp_or)
{
    dSP;
    if (SvTRUE(TOPs))
	RETURN;
    else {
	--SP;
	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_add)
{
    dSP; dATARGET; tryAMAGICbin(add,opASSIGN); 
    {
      dPOPTOPnnrl;
      SETn( left + right );
      RETURN;
    }
}

PP(pp_aelemfast)
{
    dSP;
    AV *av = GvAV((GV*)cSVOP->op_sv);
    SV** svp = av_fetch(av, op->op_private, op->op_flags & OPf_MOD);
    PUSHs(svp ? *svp : &sv_undef);
    RETURN;
}

PP(pp_join)
{
    dSP; dMARK; dTARGET;
    MARK++;
    do_join(TARG, *MARK, MARK, SP);
    SP = MARK;
    SETs(TARG);
    RETURN;
}

PP(pp_pushre)
{
    dSP;
    XPUSHs((SV*)op);
    RETURN;
}

/* Oversized hot code. */

PP(pp_print)
{
    dSP; dMARK; dORIGMARK;
    GV *gv;
    IO *io;
    register FILE *fp;

    if (op->op_flags & OPf_STACKED)
	gv = (GV*)*++MARK;
    else
	gv = defoutgv;
    if (!(io = GvIO(gv))) {
	if (dowarn) {
	    SV* sv = sv_newmortal();
            gv_fullname(sv,gv);
            warn("Filehandle %s never opened", SvPV(sv,na));
        }

	SETERRNO(EBADF,RMS$_IFI);
	goto just_say_no;
    }
    else if (!(fp = IoOFP(io))) {
	if (dowarn)  {
	    SV* sv = sv_newmortal();
            gv_fullname(sv,gv);
	    if (IoIFP(io))
		warn("Filehandle %s opened only for input", SvPV(sv,na));
	    else
		warn("print on closed filehandle %s", SvPV(sv,na));
	}
	SETERRNO(EBADF,IoIFP(io)?RMS$_FAC:RMS$_IFI);
	goto just_say_no;
    }
    else {
	MARK++;
	if (ofslen) {
	    while (MARK <= SP) {
		if (!do_print(*MARK, fp))
		    break;
		MARK++;
		if (MARK <= SP) {
		    if (fwrite1(ofs, 1, ofslen, fp) == 0 || ferror(fp)) {
			MARK--;
			break;
		    }
		}
	    }
	}
	else {
	    while (MARK <= SP) {
		if (!do_print(*MARK, fp))
		    break;
		MARK++;
	    }
	}
	if (MARK <= SP)
	    goto just_say_no;
	else {
	    if (orslen)
		if (fwrite1(ors, 1, orslen, fp) == 0 || ferror(fp))
		    goto just_say_no;

	    if (IoFLAGS(io) & IOf_FLUSH)
		if (Fflush(fp) == EOF)
		    goto just_say_no;
	}
    }
    SP = ORIGMARK;
    PUSHs(&sv_yes);
    RETURN;

  just_say_no:
    SP = ORIGMARK;
    PUSHs(&sv_undef);
    RETURN;
}

PP(pp_rv2av)
{
    dSP; dPOPss;

    AV *av;

    if (SvROK(sv)) {
      wasref:
	av = (AV*)SvRV(sv);
	if (SvTYPE(av) != SVt_PVAV)
	    DIE("Not an ARRAY reference");
	if (op->op_private & OPpLVAL_INTRO)
	    av = (AV*)save_svref((SV**)sv);
	if (op->op_flags & OPf_REF) {
	    PUSHs((SV*)av);
	    RETURN;
	}
    }
    else {
	if (SvTYPE(sv) == SVt_PVAV) {
	    av = (AV*)sv;
	    if (op->op_flags & OPf_REF) {
		PUSHs((SV*)av);
		RETURN;
	    }
	}
	else {
	    if (SvTYPE(sv) != SVt_PVGV) {
		char *sym;

		if (SvGMAGICAL(sv)) {
		    mg_get(sv);
		    if (SvROK(sv))
			goto wasref;
		}
		if (!SvOK(sv)) {
		    if (op->op_flags & OPf_REF ||
		      op->op_private & HINT_STRICT_REFS)
			DIE(no_usym, "an ARRAY");
		    if (GIMME == G_ARRAY)
			RETURN;
		    RETPUSHUNDEF;
		}
		sym = SvPV(sv,na);
		if (op->op_private & HINT_STRICT_REFS)
		    DIE(no_symref, sym, "an ARRAY");
		sv = (SV*)gv_fetchpv(sym, TRUE, SVt_PVAV);
	    }
	    av = GvAVn(sv);
	    if (op->op_private & OPpLVAL_INTRO)
		av = save_ary(sv);
	    if (op->op_flags & OPf_REF) {
		PUSHs((SV*)av);
		RETURN;
	    }
	}
    }

    if (GIMME == G_ARRAY) {
	I32 maxarg = AvFILL(av) + 1;
	EXTEND(SP, maxarg);
	Copy(AvARRAY(av), SP+1, maxarg, SV*);
	SP += maxarg;
    }
    else {
	dTARGET;
	I32 maxarg = AvFILL(av) + 1;
	PUSHi(maxarg);
    }
    RETURN;
}

PP(pp_rv2hv)
{

    dSP; dTOPss;

    HV *hv;

    if (SvROK(sv)) {
      wasref:
	hv = (HV*)SvRV(sv);
	if (SvTYPE(hv) != SVt_PVHV)
	    DIE("Not a HASH reference");
	if (op->op_private & OPpLVAL_INTRO)
	    hv = (HV*)save_svref((SV**)sv);
	if (op->op_flags & OPf_REF) {
	    SETs((SV*)hv);
	    RETURN;
	}
    }
    else {
	if (SvTYPE(sv) == SVt_PVHV) {
	    hv = (HV*)sv;
	    if (op->op_flags & OPf_REF) {
		SETs((SV*)hv);
		RETURN;
	    }
	}
	else {
	    if (SvTYPE(sv) != SVt_PVGV) {
		char *sym;

		if (SvGMAGICAL(sv)) {
		    mg_get(sv);
		    if (SvROK(sv))
			goto wasref;
		}
		if (!SvOK(sv)) {
		    if (op->op_flags & OPf_REF ||
		      op->op_private & HINT_STRICT_REFS)
			DIE(no_usym, "a HASH");
		    if (GIMME == G_ARRAY) {
			SP--;
			RETURN;
		    }
		    RETSETUNDEF;
		}
		sym = SvPV(sv,na);
		if (op->op_private & HINT_STRICT_REFS)
		    DIE(no_symref, sym, "a HASH");
		sv = (SV*)gv_fetchpv(sym, TRUE, SVt_PVHV);
	    }
	    hv = GvHVn(sv);
	    if (op->op_private & OPpLVAL_INTRO)
		hv = save_hash(sv);
	    if (op->op_flags & OPf_REF) {
		SETs((SV*)hv);
		RETURN;
	    }
	}
    }

    if (GIMME == G_ARRAY) { /* array wanted */
	*stack_sp = (SV*)hv;
	return do_kv(ARGS);
    }
    else {
	dTARGET;
	if (HvFILL(hv)) {
	    sprintf(buf, "%d/%d", HvFILL(hv), HvMAX(hv)+1);
	    sv_setpv(TARG, buf);
	}
	else
	    sv_setiv(TARG, 0);
	SETTARG;
	RETURN;
    }
}

PP(pp_aassign)
{
    dSP;
    SV **lastlelem = stack_sp;
    SV **lastrelem = stack_base + POPMARK;
    SV **firstrelem = stack_base + POPMARK + 1;
    SV **firstlelem = lastrelem + 1;

    register SV **relem;
    register SV **lelem;

    register SV *sv;
    register AV *ary;

    HV *hash;
    I32 i;
    int magic;

    delaymagic = DM_DELAY;		/* catch simultaneous items */

    /* If there's a common identifier on both sides we have to take
     * special care that assigning the identifier on the left doesn't
     * clobber a value on the right that's used later in the list.
     */
    if (op->op_private & OPpASSIGN_COMMON) {
        for (relem = firstrelem; relem <= lastrelem; relem++) {
            /*SUPPRESS 560*/
            if (sv = *relem)
                *relem = sv_mortalcopy(sv);
        }
    }

    relem = firstrelem;
    lelem = firstlelem;
    ary = Null(AV*);
    hash = Null(HV*);
    while (lelem <= lastlelem) {
	tainted = 0;		/* Each item stands on its own, taintwise. */
	sv = *lelem++;
	switch (SvTYPE(sv)) {
	case SVt_PVAV:
	    ary = (AV*)sv;
	    magic = SvMAGICAL(ary) != 0;
	    
	    av_clear(ary);
	    i = 0;
	    while (relem <= lastrelem) {	/* gobble up all the rest */
		sv = NEWSV(28,0);
		assert(*relem);
		sv_setsv(sv,*relem);
		*(relem++) = sv;
		(void)av_store(ary,i++,sv);
		if (magic)
		    mg_set(sv);
		tainted = 0;
	    }
	    break;
	case SVt_PVHV: {
		char *tmps;
		SV *tmpstr;

		hash = (HV*)sv;
		magic = SvMAGICAL(hash) != 0;
		hv_clear(hash);

		while (relem < lastrelem) {	/* gobble up all the rest */
		    STRLEN len;
		    if (*relem)
			sv = *(relem++);
		    else
			sv = &sv_no, relem++;
		    tmps = SvPV(sv, len);
		    tmpstr = NEWSV(29,0);
		    if (*relem)
			sv_setsv(tmpstr,*relem);	/* value */
		    *(relem++) = tmpstr;
		    (void)hv_store(hash,tmps,len,tmpstr,0);
		    if (magic)
			mg_set(tmpstr);
		    tainted = 0;
		}
	    }
	    break;
	default:
	    if (SvTHINKFIRST(sv)) {
		if (SvREADONLY(sv) && curcop != &compiling) {
		    if (sv != &sv_undef && sv != &sv_yes && sv != &sv_no)
			DIE(no_modify);
		    if (relem <= lastrelem)
			relem++;
		    break;
		}
		if (SvROK(sv))
		    sv_unref(sv);
	    }
	    if (relem <= lastrelem) {
		sv_setsv(sv, *relem);
		*(relem++) = sv;
	    }
	    else
		sv_setsv(sv, &sv_undef);
	    SvSETMAGIC(sv);
	    break;
	}
    }
    if (delaymagic & ~DM_DELAY) {
	if (delaymagic & DM_UID) {
#ifdef HAS_SETRESUID
	    (void)setresuid(uid,euid,(Uid_t)-1);
#else
#  ifdef HAS_SETREUID
	    (void)setreuid(uid,euid);
#  else
#    ifdef HAS_SETRUID
	    if ((delaymagic & DM_UID) == DM_RUID) {
		(void)setruid(uid);
		delaymagic &= ~DM_RUID;
	    }
#    endif /* HAS_SETRUID */
#    ifdef HAS_SETEUID
	    if ((delaymagic & DM_UID) == DM_EUID) {
		(void)seteuid(uid);
		delaymagic &= ~DM_EUID;
	    }
#    endif /* HAS_SETEUID */
	    if (delaymagic & DM_UID) {
		if (uid != euid)
		    DIE("No setreuid available");
		(void)setuid(uid);
	    }
#  endif /* HAS_SETREUID */
#endif /* HAS_SETRESUID */
	    uid = (int)getuid();
	    euid = (int)geteuid();
	}
	if (delaymagic & DM_GID) {
#ifdef HAS_SETRESGID
	    (void)setresgid(gid,egid,(Gid_t)-1);
#else
#  ifdef HAS_SETREGID
	    (void)setregid(gid,egid);
#  else
#    ifdef HAS_SETRGID
	    if ((delaymagic & DM_GID) == DM_RGID) {
		(void)setrgid(gid);
		delaymagic &= ~DM_RGID;
	    }
#    endif /* HAS_SETRGID */
#    ifdef HAS_SETEGID
	    if ((delaymagic & DM_GID) == DM_EGID) {
		(void)setegid(gid);
		delaymagic &= ~DM_EGID;
	    }
#    endif /* HAS_SETEGID */
	    if (delaymagic & DM_GID) {
		if (gid != egid)
		    DIE("No setregid available");
		(void)setgid(gid);
	    }
#  endif /* HAS_SETREGID */
#endif /* HAS_SETRESGID */
	    gid = (int)getgid();
	    egid = (int)getegid();
	}
	tainting |= (uid && (euid != uid || egid != gid));
    }
    delaymagic = 0;
    if (GIMME == G_ARRAY) {
	if (ary || hash)
	    SP = lastrelem;
	else
	    SP = firstrelem + (lastlelem - firstlelem);
	RETURN;
    }
    else {
	dTARGET;
	SP = firstrelem;
		
	SETi(lastrelem - firstrelem + 1);
	RETURN;
    }
}

PP(pp_match)
{
    dSP; dTARG;
    register PMOP *pm = cPMOP;
    register char *t;
    register char *s;
    char *strend;
    I32 global;
    I32 safebase;
    char *truebase;
    register REGEXP *rx = pm->op_pmregexp;
    I32 gimme = GIMME;
    STRLEN len;
    I32 minmatch = 0;
    I32 oldsave = savestack_ix;

    if (op->op_flags & OPf_STACKED)
	TARG = POPs;
    else {
	TARG = GvSV(defgv);
	EXTEND(SP,1);
    }
    s = SvPV(TARG, len);
    strend = s + len;
    if (!s)
	DIE("panic: do_match");

    if (pm->op_pmflags & PMf_USED) {
	if (gimme == G_ARRAY)
	    RETURN;
	RETPUSHNO;
    }

    if (!rx->prelen && curpm) {
	pm = curpm;
	rx = pm->op_pmregexp;
    }
    truebase = t = s;
    if (global = pm->op_pmflags & PMf_GLOBAL) {
	rx->startp[0] = 0;
	if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG)) {
	    MAGIC* mg = mg_find(TARG, 'g');
	    if (mg && mg->mg_len >= 0) {
		rx->endp[0] = rx->startp[0] = s + mg->mg_len; 
		minmatch = (mg->mg_flags & MGf_MINMATCH);
	    }
	}
    }
    if (!rx->nparens && !global)
	gimme = G_SCALAR;			/* accidental array context? */
    safebase = (gimme == G_ARRAY) || global;
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(multiline);
	multiline = pm->op_pmflags & PMf_MULTILINE;
    }

play_it_again:
    if (global && rx->startp[0]) {
	t = s = rx->endp[0];
	if (s > strend)
	    goto nope;
	minmatch = (s == rx->startp[0]);
    }
    if (pm->op_pmshort) {
	if (pm->op_pmflags & PMf_SCANFIRST) {
	    if (SvSCREAM(TARG)) {
		if (screamfirst[BmRARE(pm->op_pmshort)] < 0)
		    goto nope;
		else if (!(s = screaminstr(TARG, pm->op_pmshort)))
		    goto nope;
		else if (pm->op_pmflags & PMf_ALL)
		    goto yup;
	    }
	    else if (!(s = fbm_instr((unsigned char*)s,
	      (unsigned char*)strend, pm->op_pmshort)))
		goto nope;
	    else if (pm->op_pmflags & PMf_ALL)
		goto yup;
	    if (s && rx->regback >= 0) {
		++BmUSEFUL(pm->op_pmshort);
		s -= rx->regback;
		if (s < t)
		    s = t;
	    }
	    else
		s = t;
	}
	else if (!multiline) {
	    if (*SvPVX(pm->op_pmshort) != *s ||
	      bcmp(SvPVX(pm->op_pmshort), s, pm->op_pmslen) ) {
		if (pm->op_pmflags & PMf_FOLD) {
		    if (ibcmp((U8*)SvPVX(pm->op_pmshort), (U8*)s, pm->op_pmslen) )
			goto nope;
		}
		else
		    goto nope;
	    }
	}
	if (!rx->naughty && --BmUSEFUL(pm->op_pmshort) < 0) {
	    SvREFCNT_dec(pm->op_pmshort);
	    pm->op_pmshort = Nullsv;	/* opt is being useless */
	}
    }
    if (pregexec(rx, s, strend, truebase, minmatch,
      SvSCREAM(TARG) ? TARG : Nullsv,
      safebase)) {
	curpm = pm;
	if (pm->op_pmflags & PMf_ONCE)
	    pm->op_pmflags |= PMf_USED;
	goto gotcha;
    }
    else
	goto ret_no;
    /*NOTREACHED*/

  gotcha:
    if (gimme == G_ARRAY) {
	I32 iters, i, len;

	iters = rx->nparens;
	if (global && !iters)
	    i = 1;
	else
	    i = 0;
	EXTEND(SP, iters + i);
	for (i = !i; i <= iters; i++) {
	    PUSHs(sv_newmortal());
	    /*SUPPRESS 560*/
	    if ((s = rx->startp[i]) && rx->endp[i] ) {
		len = rx->endp[i] - s;
		sv_setpvn(*SP, s, len);
	    }
	}
	if (global) {
	    truebase = rx->subbeg;
	    if (rx->startp[0] && rx->startp[0] == rx->endp[0])
		++rx->endp[0];
	    goto play_it_again;
	}
	LEAVE_SCOPE(oldsave);
	RETURN;
    }
    else {
	if (global) {
	    MAGIC* mg = 0;
	    if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG))
		mg = mg_find(TARG, 'g');
	    if (!mg) {
		sv_magic(TARG, (SV*)0, 'g', Nullch, 0);
		mg = mg_find(TARG, 'g');
	    }
	    if (rx->startp[0]) {
		mg->mg_len = rx->endp[0] - truebase;
		if (rx->startp[0] == rx->endp[0])
		    mg->mg_flags |= MGf_MINMATCH;
		else
		    mg->mg_flags &= ~MGf_MINMATCH;
	    }
	    else
		mg->mg_len = -1;
	}
	LEAVE_SCOPE(oldsave);
	RETPUSHYES;
    }

yup:
    ++BmUSEFUL(pm->op_pmshort);
    curpm = pm;
    if (pm->op_pmflags & PMf_ONCE)
	pm->op_pmflags |= PMf_USED;
    if (global) {
	rx->subbeg = truebase;
	rx->subend = strend;
	rx->startp[0] = s;
	rx->endp[0] = s + SvCUR(pm->op_pmshort);
	goto gotcha;
    }
    if (sawampersand) {
	char *tmps;

	if (rx->subbase)
	    Safefree(rx->subbase);
	tmps = rx->subbase = savepvn(t, strend-t);
	rx->subbeg = tmps;
	rx->subend = tmps + (strend-t);
	tmps = rx->startp[0] = tmps + (s - t);
	rx->endp[0] = tmps + SvCUR(pm->op_pmshort);
    }
    LEAVE_SCOPE(oldsave);
    RETPUSHYES;

nope:
    if (pm->op_pmshort)
	++BmUSEFUL(pm->op_pmshort);

ret_no:
    if (global) {
	if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG)) {
	    MAGIC* mg = mg_find(TARG, 'g');
	    if (mg)
		mg->mg_len = -1;
	}
    }
    LEAVE_SCOPE(oldsave);
    if (gimme == G_ARRAY)
	RETURN;
    RETPUSHNO;
}

OP *
do_readline()
{
    dSP; dTARGETSTACKED;
    register SV *sv;
    STRLEN tmplen = 0;
    STRLEN offset;
    FILE *fp;
    register IO *io = GvIO(last_in_gv);
    register I32 type = op->op_type;

    fp = Nullfp;
    if (io) {
	fp = IoIFP(io);
	if (!fp) {
	    if (IoFLAGS(io) & IOf_ARGV) {
		if (IoFLAGS(io) & IOf_START) {
		    IoFLAGS(io) &= ~IOf_START;
		    IoLINES(io) = 0;
		    if (av_len(GvAVn(last_in_gv)) < 0) {
			SV *tmpstr = newSVpv("-", 1); /* assume stdin */
			av_push(GvAVn(last_in_gv), tmpstr);
		    }
		}
		fp = nextargv(last_in_gv);
		if (!fp) { /* Note: fp != IoIFP(io) */
		    (void)do_close(last_in_gv, FALSE); /* now it does*/
		    IoFLAGS(io) |= IOf_START;
		}
	    }
	    else if (type == OP_GLOB) {
		SV *tmpcmd = NEWSV(55, 0);
		SV *tmpglob = POPs;
		ENTER;
		SAVEFREESV(tmpcmd);
#ifdef VMS /* expand the wildcards right here, rather than opening a pipe, */
           /* since spawning off a process is a real performance hit */
		{
#include <descrip.h>
#include <lib$routines.h>
#include <nam.h>
#include <rmsdef.h>
		    char rslt[NAM$C_MAXRSS+1+sizeof(unsigned short int)] = {'\0','\0'};
		    char vmsspec[NAM$C_MAXRSS+1];
		    char *rstr = rslt + sizeof(unsigned short int), *begin, *end, *cp;
		    char tmpfnam[L_tmpnam] = "SYS$SCRATCH:";
		    $DESCRIPTOR(dfltdsc,"SYS$DISK:[]*.*;");
		    FILE *tmpfp;
		    STRLEN i;
		    struct dsc$descriptor_s wilddsc
		       = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
		    struct dsc$descriptor_vs rsdsc
		       = {sizeof rslt, DSC$K_DTYPE_VT, DSC$K_CLASS_VS, rslt};
		    unsigned long int cxt = 0, sts = 0, ok = 1, hasdir = 0, hasver = 0, isunix = 0;

		    /* We could find out if there's an explicit dev/dir or version
		       by peeking into lib$find_file's internal context at
		       ((struct NAM *)((struct FAB *)cxt)->fab$l_nam)->nam$l_fnb
		       but that's unsupported, so I don't want to do it now and
		       have it bite someone in the future. */
		    strcat(tmpfnam,tmpnam(NULL));
		    cp = SvPV(tmpglob,i);
		    for (; i; i--) {
		       if (cp[i] == ';') hasver = 1;
		       if (cp[i] == '.') {
		           if (sts) hasver = 1;
		           else sts = 1;
		       }
		       if (cp[i] == '/') {
		          hasdir = isunix = 1;
		          break;
		       }
		       if (cp[i] == ']' || cp[i] == '>' || cp[i] == ':') {
		           hasdir = 1;
		           break;
		       }
		    }
		    if ((tmpfp = fopen(tmpfnam,"w+","fop=dlt")) != NULL) {
		        ok = ((wilddsc.dsc$a_pointer = tovmsspec(SvPVX(tmpglob),vmsspec)) != NULL);
		        if (ok) wilddsc.dsc$w_length = (unsigned short int) strlen(wilddsc.dsc$a_pointer);
		        while (ok && ((sts = lib$find_file(&wilddsc,&rsdsc,&cxt,
		                                    &dfltdsc,NULL,NULL,NULL))&1)) {
		            end = rstr + (unsigned long int) *rslt;
		            if (!hasver) while (*end != ';') end--;
		            *(end++) = '\n';  *end = '\0';
		            for (cp = rstr; *cp; cp++) *cp = _tolower(*cp);
		            if (hasdir) {
		              if (isunix) trim_unixpath(rstr,SvPVX(tmpglob));
		              begin = rstr;
		            }
		            else {
		                begin = end;
		                while (*(--begin) != ']' && *begin != '>') ;
		                ++begin;
		            }
		            ok = (fputs(begin,tmpfp) != EOF);
		        }
		        if (cxt) (void)lib$find_file_end(&cxt);
		        if (ok && sts != RMS$_NMF &&
		            sts != RMS$_DNF && sts != RMS$_FNF) ok = 0;
		        if (!ok) {
		            if (!(sts & 1)) {
		              SETERRNO((sts == RMS$_SYN ? EINVAL : EVMSERR),sts);
		            }
		            fclose(tmpfp);
		            fp = NULL;
		        }
		        else {
		           rewind(tmpfp);
		           IoTYPE(io) = '<';
		           IoIFP(io) = fp = tmpfp;
		        }
		    }
		}
#else /* !VMS */
#ifdef DOSISH
		sv_setpv(tmpcmd, "perlglob ");
		sv_catsv(tmpcmd, tmpglob);
		sv_catpv(tmpcmd, " |");
#else
#ifdef CSH
		sv_setpvn(tmpcmd, cshname, cshlen);
		sv_catpv(tmpcmd, " -cf 'set nonomatch; glob ");
		sv_catsv(tmpcmd, tmpglob);
		sv_catpv(tmpcmd, "' 2>/dev/null |");
#else
		sv_setpv(tmpcmd, "echo ");
		sv_catsv(tmpcmd, tmpglob);
#if 'z' - 'a' == 25
		sv_catpv(tmpcmd, "|tr -s ' \t\f\r' '\\012\\012\\012\\012'|");
#else
		sv_catpv(tmpcmd, "|tr -s ' \t\f\r' '\\n\\n\\n\\n'|");
#endif
#endif /* !CSH */
#endif /* !MSDOS */
		(void)do_open(last_in_gv, SvPVX(tmpcmd), SvCUR(tmpcmd),
			      FALSE, 0, 0, Nullfp);
		fp = IoIFP(io);
#endif /* !VMS */
		LEAVE;
	    }
	}
	else if (type == OP_GLOB)
	    SP--;
    }
    if (!fp) {
	if (dowarn && io && !(IoFLAGS(io) & IOf_START))
	    warn("Read on closed filehandle <%s>", GvENAME(last_in_gv));
	if (GIMME == G_SCALAR) {
	    (void)SvOK_off(TARG);
	    PUSHTARG;
	}
	RETURN;
    }
    if (GIMME == G_ARRAY) {
	sv = sv_2mortal(NEWSV(57, 80));
	offset = 0;
    }
    else {
	sv = TARG;
	(void)SvUPGRADE(sv, SVt_PV);
	tmplen = SvLEN(sv);	/* remember if already alloced */
	if (!tmplen)
	    Sv_Grow(sv, 80);	/* try short-buffering it */
	if (type == OP_RCATLINE)
	    offset = SvCUR(sv);
	else
	    offset = 0;
    }
    for (;;) {
	if (!sv_gets(sv, fp, offset)) {
	    clearerr(fp);
	    if (IoFLAGS(io) & IOf_ARGV) {
		fp = nextargv(last_in_gv);
		if (fp)
		    continue;
		(void)do_close(last_in_gv, FALSE);
		IoFLAGS(io) |= IOf_START;
	    }
	    else if (type == OP_GLOB) {
		(void)do_close(last_in_gv, FALSE);
	    }
	    if (GIMME == G_SCALAR) {
		(void)SvOK_off(TARG);
		PUSHTARG;
	    }
	    RETURN;
	}
	IoLINES(io)++;
	XPUSHs(sv);
	if (tainting) {
	    tainted = TRUE;
	    SvTAINT(sv); /* Anything from the outside world...*/
	}
	if (type == OP_GLOB) {
	    char *tmps;

	    if (SvCUR(sv) > 0 && SvCUR(rs) > 0) {
		tmps = SvEND(sv) - 1;
		if (*tmps == *SvPVX(rs)) {
		    *tmps = '\0';
		    SvCUR(sv)--;
		}
	    }
	    for (tmps = SvPVX(sv); *tmps; tmps++)
		if (!isALPHA(*tmps) && !isDIGIT(*tmps) &&
		    strchr("$&*(){}[]'\";\\|?<>~`", *tmps))
			break;
	    if (*tmps && Stat(SvPVX(sv), &statbuf) < 0) {
		(void)POPs;		/* Unmatched wildcard?  Chuck it... */
		continue;
	    }
	}
	if (GIMME == G_ARRAY) {
	    if (SvLEN(sv) - SvCUR(sv) > 20) {
		SvLEN_set(sv, SvCUR(sv)+1);
		Renew(SvPVX(sv), SvLEN(sv), char);
	    }
	    sv = sv_2mortal(NEWSV(58, 80));
	    continue;
	}
	else if (!tmplen && SvLEN(sv) - SvCUR(sv) > 80) {
	    /* try to reclaim a bit of scalar space (only on 1st alloc) */
	    if (SvCUR(sv) < 60)
		SvLEN_set(sv, 80);
	    else
		SvLEN_set(sv, SvCUR(sv)+40);	/* allow some slop */
	    Renew(SvPVX(sv), SvLEN(sv), char);
	}
	RETURN;
    }
}

PP(pp_enter)
{
    dSP;
    register CONTEXT *cx;
    I32 gimme;

    /*
     * We don't just use the GIMME macro here because it assumes there's
     * already a context, which ain't necessarily so at initial startup.
     */

    if (op->op_flags & OPf_KNOW)
	gimme = op->op_flags & OPf_LIST;
    else if (cxstack_ix >= 0)
	gimme = cxstack[cxstack_ix].blk_gimme;
    else
	gimme = G_SCALAR;

    ENTER;

    SAVETMPS;
    PUSHBLOCK(cx, CXt_BLOCK, sp);

    RETURN;
}

PP(pp_helem)
{
    dSP;
    SV** svp;
    SV *keysv = POPs;
    STRLEN keylen;
    char *key = SvPV(keysv, keylen);
    HV *hv = (HV*)POPs;
    I32 lval = op->op_flags & OPf_MOD;

    if (SvTYPE(hv) != SVt_PVHV)
	RETPUSHUNDEF;
    svp = hv_fetch(hv, key, keylen, lval);
    if (lval) {
	if (!svp || *svp == &sv_undef)
	    DIE(no_helem, key);
	if (op->op_private & OPpLVAL_INTRO)
	    save_svref(svp);
	else if (op->op_private & (OPpDEREF_HV|OPpDEREF_AV))
	    provide_ref(op, *svp);
    }
    PUSHs(svp ? *svp : &sv_undef);
    RETURN;
}

PP(pp_leave)
{
    dSP;
    register CONTEXT *cx;
    register SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;

    if (op->op_flags & OPf_SPECIAL) {
	cx = &cxstack[cxstack_ix];
	cx->blk_oldpm = curpm;	/* fake block should preserve $1 et al */
    }

    POPBLOCK(cx,newpm);

    if (op->op_flags & OPf_KNOW)
	gimme = op->op_flags & OPf_LIST;
    else if (cxstack_ix >= 0)
	gimme = cxstack[cxstack_ix].blk_gimme;
    else
	gimme = G_SCALAR;

    if (gimme == G_SCALAR) {
	if (op->op_private & OPpLEAVE_VOID)
	    SP = newsp;
	else {
	    MARK = newsp + 1;
	    if (MARK <= SP)
		if (SvFLAGS(TOPs) & (SVs_PADTMP|SVs_TEMP))
		    *MARK = TOPs;
		else
		    *MARK = sv_mortalcopy(TOPs);
	    else {
		MEXTEND(mark,0);
		*MARK = &sv_undef;
	    }
	    SP = MARK;
	}
    }
    else {
	for (mark = newsp + 1; mark <= SP; mark++)
	    if (!(SvFLAGS(*mark) & (SVs_PADTMP|SVs_TEMP)))
		*mark = sv_mortalcopy(*mark);
		/* in case LEAVE wipes old return values */
    }
    curpm = newpm;	/* Don't pop $1 et al till now */

    LEAVE;

    RETURN;
}

PP(pp_iter)
{
    dSP;
    register CONTEXT *cx;
    SV *sv;
    AV* av;

    EXTEND(sp, 1);
    cx = &cxstack[cxstack_ix];
    if (cx->cx_type != CXt_LOOP)
	DIE("panic: pp_iter");
    av = cx->blk_loop.iterary;
    if (av == stack && cx->blk_loop.iterix >= cx->blk_oldsp)
	RETPUSHNO;

    if (cx->blk_loop.iterix >= AvFILL(av))
	RETPUSHNO;

    if (sv = AvARRAY(av)[++cx->blk_loop.iterix]) {
	SvTEMP_off(sv);
	*cx->blk_loop.itervar = sv;
    }
    else
	*cx->blk_loop.itervar = &sv_undef;

    RETPUSHYES;
}

PP(pp_subst)
{
    dSP; dTARG;
    register PMOP *pm = cPMOP;
    PMOP *rpm = pm;
    register SV *dstr;
    register char *s;
    char *strend;
    register char *m;
    char *c;
    register char *d;
    STRLEN clen;
    I32 iters = 0;
    I32 maxiters;
    register I32 i;
    bool once;
    char *orig;
    I32 safebase;
    register REGEXP *rx = pm->op_pmregexp;
    STRLEN len;
    int force_on_match = 0;
    I32 oldsave = savestack_ix;

    if (pm->op_pmflags & PMf_CONST)	/* known replacement string? */
	dstr = POPs;
    if (op->op_flags & OPf_STACKED)
	TARG = POPs;
    else {
	TARG = GvSV(defgv);
	EXTEND(SP,1);
    }
    s = SvPV(TARG, len);
    if (!SvPOKp(TARG) || SvREADONLY(TARG) || (SvTYPE(TARG) == SVt_PVGV))
	force_on_match = 1;

  force_it:
    if (!pm || !s)
	DIE("panic: do_subst");

    strend = s + len;
    maxiters = (strend - s) + 10;

    if (!rx->prelen && curpm) {
	pm = curpm;
	rx = pm->op_pmregexp;
    }
    safebase = ((!rx || !rx->nparens) && !sawampersand);
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(multiline);
	multiline = pm->op_pmflags & PMf_MULTILINE;
    }
    orig = m = s;
    if (pm->op_pmshort) {
	if (pm->op_pmflags & PMf_SCANFIRST) {
	    if (SvSCREAM(TARG)) {
		if (screamfirst[BmRARE(pm->op_pmshort)] < 0)
		    goto nope;
		else if (!(s = screaminstr(TARG, pm->op_pmshort)))
		    goto nope;
	    }
	    else if (!(s = fbm_instr((unsigned char*)s, (unsigned char*)strend,
	      pm->op_pmshort)))
		goto nope;
	    if (s && rx->regback >= 0) {
		++BmUSEFUL(pm->op_pmshort);
		s -= rx->regback;
		if (s < m)
		    s = m;
	    }
	    else
		s = m;
	}
	else if (!multiline) {
	    if (*SvPVX(pm->op_pmshort) != *s ||
	      bcmp(SvPVX(pm->op_pmshort), s, pm->op_pmslen) ) {
		if (pm->op_pmflags & PMf_FOLD) {
		    if (ibcmp((U8*)SvPVX(pm->op_pmshort), (U8*)s, pm->op_pmslen) )
			goto nope;
		}
		else
		    goto nope;
	    }
	}
	if (!rx->naughty && --BmUSEFUL(pm->op_pmshort) < 0) {
	    SvREFCNT_dec(pm->op_pmshort);
	    pm->op_pmshort = Nullsv;	/* opt is being useless */
	}
    }
    once = !(rpm->op_pmflags & PMf_GLOBAL);
    if (rpm->op_pmflags & PMf_CONST) {	/* known replacement string? */
	c = SvPV(dstr, clen);
	if (clen <= rx->minlen) {
					/* can do inplace substitution */
	    if (pregexec(rx, s, strend, orig, 0,
	      SvSCREAM(TARG) ? TARG : Nullsv, safebase)) {
		if (force_on_match) {
		    force_on_match = 0;
		    s = SvPV_force(TARG, len);
		    goto force_it;
		}
		if (rx->subbase) 	/* oops, no we can't */
		    goto long_way;
		d = s;
		curpm = pm;
		SvSCREAM_off(TARG);	/* disable possible screamer */
		if (once) {
		    m = rx->startp[0];
		    d = rx->endp[0];
		    s = orig;
		    if (m - s > strend - d) {	/* faster to shorten from end */
			if (clen) {
			    Copy(c, m, clen, char);
			    m += clen;
			}
			i = strend - d;
			if (i > 0) {
			    Move(d, m, i, char);
			    m += i;
			}
			*m = '\0';
			SvCUR_set(TARG, m - s);
			(void)SvPOK_only(TARG);
			SvSETMAGIC(TARG);
			PUSHs(&sv_yes);
			LEAVE_SCOPE(oldsave);
			RETURN;
		    }
		    /*SUPPRESS 560*/
		    else if (i = m - s) {	/* faster from front */
			d -= clen;
			m = d;
			sv_chop(TARG, d-i);
			s += i;
			while (i--)
			    *--d = *--s;
			if (clen)
			    Copy(c, m, clen, char);
			(void)SvPOK_only(TARG);
			SvSETMAGIC(TARG);
			PUSHs(&sv_yes);
			LEAVE_SCOPE(oldsave);
			RETURN;
		    }
		    else if (clen) {
			d -= clen;
			sv_chop(TARG, d);
			Copy(c, d, clen, char);
			(void)SvPOK_only(TARG);
			SvSETMAGIC(TARG);
			PUSHs(&sv_yes);
			LEAVE_SCOPE(oldsave);
			RETURN;
		    }
		    else {
			sv_chop(TARG, d);
			(void)SvPOK_only(TARG);
			SvSETMAGIC(TARG);
			PUSHs(&sv_yes);
			LEAVE_SCOPE(oldsave);
			RETURN;
		    }
		    /* NOTREACHED */
		}
		do {
		    if (iters++ > maxiters)
			DIE("Substitution loop");
		    m = rx->startp[0];
		    /*SUPPRESS 560*/
		    if (i = m - s) {
			if (s != d)
			    Move(s, d, i, char);
			d += i;
		    }
		    if (clen) {
			Copy(c, d, clen, char);
			d += clen;
		    }
		    s = rx->endp[0];
		} while (pregexec(rx, s, strend, orig, s == m,
		    Nullsv, TRUE));	/* (don't match same null twice) */
		if (s != d) {
		    i = strend - s;
		    SvCUR_set(TARG, d - SvPVX(TARG) + i);
		    Move(s, d, i+1, char);		/* include the Null */
		}
		(void)SvPOK_only(TARG);
		SvSETMAGIC(TARG);
		PUSHs(sv_2mortal(newSViv((I32)iters)));
		LEAVE_SCOPE(oldsave);
		RETURN;
	    }
	    PUSHs(&sv_no);
	    LEAVE_SCOPE(oldsave);
	    RETURN;
	}
    }
    else
	c = Nullch;
    if (pregexec(rx, s, strend, orig, 0,
      SvSCREAM(TARG) ? TARG : Nullsv, safebase)) {
    long_way:
	if (force_on_match) {
	    force_on_match = 0;
	    s = SvPV_force(TARG, len);
	    goto force_it;
	}
	dstr = NEWSV(25, sv_len(TARG));
	sv_setpvn(dstr, m, s-m);
	curpm = pm;
	if (!c) {
	    register CONTEXT *cx;
	    PUSHSUBST(cx);
	    RETURNOP(cPMOP->op_pmreplroot);
	}
	do {
	    if (iters++ > maxiters)
		DIE("Substitution loop");
	    if (rx->subbase && rx->subbase != orig) {
		m = s;
		s = orig;
		orig = rx->subbase;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = rx->startp[0];
	    sv_catpvn(dstr, s, m-s);
	    s = rx->endp[0];
	    if (clen)
		sv_catpvn(dstr, c, clen);
	    if (once)
		break;
	} while (pregexec(rx, s, strend, orig, s == m, Nullsv,
	    safebase));
	sv_catpvn(dstr, s, strend - s);

	(void)SvOOK_off(TARG);
	Safefree(SvPVX(TARG));
	SvPVX(TARG) = SvPVX(dstr);
	SvCUR_set(TARG, SvCUR(dstr));
	SvLEN_set(TARG, SvLEN(dstr));
	SvPVX(dstr) = 0;
	sv_free(dstr);

	(void)SvPOK_only(TARG);
	SvSETMAGIC(TARG);
	PUSHs(sv_2mortal(newSViv((I32)iters)));
	LEAVE_SCOPE(oldsave);
	RETURN;
    }
    PUSHs(&sv_no);
    LEAVE_SCOPE(oldsave);
    RETURN;

nope:
    ++BmUSEFUL(pm->op_pmshort);
    PUSHs(&sv_no);
    LEAVE_SCOPE(oldsave);
    RETURN;
}

PP(pp_grepwhile)
{
    dSP;

    if (SvTRUEx(POPs))
	stack_base[markstack_ptr[-1]++] = stack_base[*markstack_ptr];
    ++*markstack_ptr;
    LEAVE;					/* exit inner scope */

    /* All done yet? */
    if (stack_base + *markstack_ptr > sp) {
	I32 items;

	LEAVE;					/* exit outer scope */
	(void)POPMARK;				/* pop src */
	items = --*markstack_ptr - markstack_ptr[-1];
	(void)POPMARK;				/* pop dst */
	SP = stack_base + POPMARK;		/* pop original mark */
	if (GIMME != G_ARRAY) {
	    dTARGET;
	    XPUSHi(items);
	    RETURN;
	}
	SP += items;
	RETURN;
    }
    else {
	SV *src;

	ENTER;					/* enter inner scope */
	SAVESPTR(curpm);

	src = stack_base[*markstack_ptr];
	SvTEMP_off(src);
	GvSV(defgv) = src;

	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_leavesub)
{
    dSP;
    SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register CONTEXT *cx;

    POPBLOCK(cx,newpm);
    POPSUB(cx);

    if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP)
	    if (SvFLAGS(TOPs) & SVs_TEMP)
		*MARK = TOPs;
	    else
		*MARK = sv_mortalcopy(TOPs);
	else {
	    MEXTEND(mark,0);
	    *MARK = &sv_undef;
	}
	SP = MARK;
    }
    else {
	for (mark = newsp + 1; mark <= SP; mark++)
	    if (!(SvFLAGS(*mark) & SVs_TEMP))
		*mark = sv_mortalcopy(*mark);
		/* in case LEAVE wipes old return values */
    }

    if (cx->blk_sub.hasargs) {		/* You don't exist; go away. */
	AV* av = cx->blk_sub.argarray;

	av_clear(av);
	AvREAL_off(av);
    }
    curpm = newpm;	/* Don't pop $1 et al till now */

    LEAVE;
    PUTBACK;
    return pop_return();
}

PP(pp_entersub)
{
    dSP; dPOPss;
    GV *gv;
    HV *stash;
    register CV *cv;
    register CONTEXT *cx;
    I32 gimme;

    if (!sv)
	DIE("Not a CODE reference");
    switch (SvTYPE(sv)) {
    default:
	if (!SvROK(sv)) {
	    char *sym;

	    if (sv == &sv_yes)		/* unfound import, ignore */
		RETURN;
	    if (!SvOK(sv))
		DIE(no_usym, "a subroutine");
	    sym = SvPV(sv,na);
	    if (op->op_private & HINT_STRICT_REFS)
		DIE(no_symref, sym, "a subroutine");
	    cv = perl_get_cv(sym, TRUE);
	    break;
	}
	cv = (CV*)SvRV(sv);
	if (SvTYPE(cv) == SVt_PVCV)
	    break;
	/* FALL THROUGH */
    case SVt_PVHV:
    case SVt_PVAV:
	DIE("Not a CODE reference");
    case SVt_PVCV:
	cv = (CV*)sv;
	break;
    case SVt_PVGV:
	if (!(cv = GvCV((GV*)sv)))
	    cv = sv_2cv(sv, &stash, &gv, TRUE);
	break;
    }

    ENTER;
    SAVETMPS;

  retry:
    if (!cv)
	DIE("Not a CODE reference");

    if (!CvROOT(cv) && !CvXSUB(cv)) {
	if (gv = CvGV(cv)) {
	    SV *tmpstr;
	    GV *ngv;
	    if (SvFAKE(cv) && GvCV(gv) != cv) {	/* autoloaded stub? */
		cv = GvCV(gv);
		if (SvTYPE(sv) == SVt_PVGV) {
		    SvREFCNT_dec(GvCV((GV*)sv));
		    GvCV((GV*)sv) = (CV*)SvREFCNT_inc((SV*)cv);
		}
		goto retry;
	    }
	    tmpstr = sv_newmortal();
	    gv_efullname(tmpstr, gv);
	    ngv = gv_fetchmethod(GvESTASH(gv), "AUTOLOAD");
	    if (ngv && ngv != gv && (cv = GvCV(ngv))) {	/* One more chance... */
		gv = ngv;
		sv_setsv(GvSV(CvGV(cv)), tmpstr);	/* Set CV's $AUTOLOAD */
		if (tainting)
		    sv_unmagic(GvSV(CvGV(cv)), 't');
		goto retry;
	    }
	    else
		DIE("Undefined subroutine &%s called",SvPVX(tmpstr));
	}
	DIE("Undefined subroutine called");
    }

    gimme = GIMME;
    if ((op->op_private & OPpENTERSUB_DB) && !CvXSUB(cv)) {
	sv = GvSV(DBsub);
	save_item(sv);
	if (CvFLAGS(cv) & (CVf_ANON | CVf_CLONED)) {
	    /* GV is potentially non-unique */
	    sv_setsv(sv, newRV((SV*)cv));
	}
	else {
	    gv = CvGV(cv);
	    gv_efullname(sv,gv);
	}
	cv = GvCV(DBsub);
	if (!cv)
	    DIE("No DBsub routine");
    }

    if (CvXSUB(cv)) {
	if (CvOLDSTYLE(cv)) {
	    I32 (*fp3)_((int,int,int));
	    dMARK;
	    register I32 items = SP - MARK;
	    while (sp > mark) {
		sp[1] = sp[0];
		sp--;
	    }
	    stack_sp = mark + 1;
	    fp3 = (I32(*)_((int,int,int)))CvXSUB(cv);
	    items = (*fp3)(CvXSUBANY(cv).any_i32, 
			   MARK - stack_base + 1,
			   items);
	    stack_sp = stack_base + items;
	}
	else {
	    I32 markix = TOPMARK;

	    PUTBACK;
	    (void)(*CvXSUB(cv))(cv);

	    /* Enforce some sanity in scalar context. */
	    if (gimme == G_SCALAR && ++markix != stack_sp - stack_base ) {
		if (markix > stack_sp - stack_base)
		    *(stack_base + markix) = &sv_undef;
		else
		    *(stack_base + markix) = *stack_sp;
		stack_sp = stack_base + markix;
	    }
	}
	LEAVE;
	return NORMAL;
    }
    else {
	dMARK;
	register I32 items = SP - MARK;
	I32 hasargs = (op->op_flags & OPf_STACKED) != 0;
	AV* padlist = CvPADLIST(cv);
	SV** svp = AvARRAY(padlist);
	push_return(op->op_next);
	PUSHBLOCK(cx, CXt_SUB, MARK);
	PUSHSUB(cx);
	CvDEPTH(cv)++;
	if (CvDEPTH(cv) < 2)
	    (void)SvREFCNT_inc(cv);
	else {	/* save temporaries on recursion? */
	    if (CvDEPTH(cv) == 100 && dowarn)
		warn("Deep recursion on subroutine \"%s\"",GvENAME(CvGV(cv)));
	    if (CvDEPTH(cv) > AvFILL(padlist)) {
		AV *av;
		AV *newpad = newAV();
		SV **oldpad = AvARRAY(svp[CvDEPTH(cv)-1]);
		I32 ix = AvFILL((AV*)svp[1]);
		svp = AvARRAY(svp[0]);
		for ( ;ix > 0; ix--) {
		    if (svp[ix] != &sv_undef) {
			char *name = SvPVX(svp[ix]);
			if (SvFLAGS(svp[ix]) & SVf_FAKE) { /* outer lexical? */
			    av_store(newpad, ix,
				SvREFCNT_inc(oldpad[ix]) );
			}
			else {				/* our own lexical */
			    if (*name == '@')
				av_store(newpad, ix, sv = (SV*)newAV());
			    else if (*name == '%')
				av_store(newpad, ix, sv = (SV*)newHV());
			    else
				av_store(newpad, ix, sv = NEWSV(0,0));
			    SvPADMY_on(sv);
			}
		    }
		    else {
			av_store(newpad, ix, sv = NEWSV(0,0));
			SvPADTMP_on(sv);
		    }
		}
		av = newAV();		/* will be @_ */
		av_extend(av, 0);
		av_store(newpad, 0, (SV*)av);
		AvFLAGS(av) = AVf_REIFY;
		av_store(padlist, CvDEPTH(cv), (SV*)newpad);
		AvFILL(padlist) = CvDEPTH(cv);
		svp = AvARRAY(padlist);
	    }
	}
	SAVESPTR(curpad);
	curpad = AvARRAY((AV*)svp[CvDEPTH(cv)]);
	if (hasargs) {
	    AV* av = (AV*)curpad[0];
	    SV** ary;

	    if (AvREAL(av)) {
		av_clear(av);
		AvREAL_off(av);
	    }
	    cx->blk_sub.savearray = GvAV(defgv);
	    cx->blk_sub.argarray = av;
	    GvAV(defgv) = cx->blk_sub.argarray;
	    ++MARK;

	    if (items > AvMAX(av) + 1) {
		ary = AvALLOC(av);
		if (AvARRAY(av) != ary) {
		    AvMAX(av) += AvARRAY(av) - AvALLOC(av);
		    SvPVX(av) = (char*)ary;
		}
		if (items > AvMAX(av) + 1) {
		    AvMAX(av) = items - 1;
		    Renew(ary,items,SV*);
		    AvALLOC(av) = ary;
		    SvPVX(av) = (char*)ary;
		}
	    }
	    Copy(MARK,AvARRAY(av),items,SV*);
	    AvFILL(av) = items - 1;
	    
	    while (items--) {
		if (*MARK)
		    SvTEMP_off(*MARK);
		MARK++;
	    }
	}
	RETURNOP(CvSTART(cv));
    }
}

PP(pp_aelem)
{
    dSP;
    SV** svp;
    I32 elem = POPi;
    AV *av = (AV*)POPs;
    I32 lval = op->op_flags & OPf_MOD;

    if (elem > 0)
	elem -= curcop->cop_arybase;
    if (SvTYPE(av) != SVt_PVAV)
	RETPUSHUNDEF;
    svp = av_fetch(av, elem, lval);
    if (lval) {
	if (!svp || *svp == &sv_undef)
	    DIE(no_aelem, elem);
	if (op->op_private & OPpLVAL_INTRO)
	    save_svref(svp);
	else if (op->op_private & (OPpDEREF_HV|OPpDEREF_AV))
	    provide_ref(op, *svp);
    }
    PUSHs(svp ? *svp : &sv_undef);
    RETURN;
}

void
provide_ref(op, sv)
OP* op;
SV* sv;
{
    if (SvGMAGICAL(sv))
	mg_get(sv);
    if (!SvOK(sv)) {
	if (SvREADONLY(sv))
	    croak(no_modify);
	(void)SvUPGRADE(sv, SVt_RV);
	SvRV(sv) = (op->op_private & OPpDEREF_HV ?
		    (SV*)newHV() : (SV*)newAV());
	SvROK_on(sv);
	SvSETMAGIC(sv);
    }
}

PP(pp_method)
{
    dSP;
    SV* sv;
    SV* ob;
    GV* gv;
    SV* nm;

    nm = TOPs;
    sv = *(stack_base + TOPMARK + 1);
    
    gv = 0;
    if (SvGMAGICAL(sv))
        mg_get(sv);
    if (SvROK(sv))
	ob = (SV*)SvRV(sv);
    else {
	GV* iogv;
	char* packname = 0;

	if (!SvOK(sv) ||
	    !(packname = SvPV(sv, na)) ||
	    !(iogv = gv_fetchpv(packname, FALSE, SVt_PVIO)) ||
	    !(ob=(SV*)GvIO(iogv)))
	{
	    char *name = SvPV(nm, na);
	    HV *stash;
	    if (!packname || !isALPHA(*packname))
DIE("Can't call method \"%s\" without a package or object reference", name);
	    if (!(stash = gv_stashpv(packname, FALSE))) {
		if (gv_stashpv("UNIVERSAL", FALSE))
		    stash = gv_stashpv(packname, TRUE);
		else
		    DIE("Can't call method \"%s\" in empty package \"%s\"",
			name, packname);
	    }
	    gv = gv_fetchmethod(stash,name);
	    if (!gv)
		DIE("Can't locate object method \"%s\" via package \"%s\"",
		    name, packname);
	    SETs(gv);
	    RETURN;
	}
	*(stack_base + TOPMARK + 1) = sv_2mortal(newRV(iogv));
    }

    if (!ob || !SvOBJECT(ob)) {
	char *name = SvPV(nm, na);
	DIE("Can't call method \"%s\" on unblessed reference", name);
    }

    if (!gv) {		/* nothing cached */
	char *name = SvPV(nm, na);
	gv = gv_fetchmethod(SvSTASH(ob),name);
	if (!gv)
	    DIE("Can't locate object method \"%s\" via package \"%s\"",
		name, HvNAME(SvSTASH(ob)));
    }

    SETs(gv);
    RETURN;
}

