/*    scope.c
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "For the fashion of Minas Tirith was such that it was built on seven
 * levels..."
 */

#include "EXTERN.h"
#include "perl.h"

SV**
stack_grow(sp, p, n)
SV** sp;
SV** p;
int n;
{
    stack_sp = sp;
    av_extend(curstack, (p - stack_base) + (n) + 128);
    return stack_sp;
}

I32
cxinc()
{
    cxstack_max = cxstack_max * 3 / 2;
    Renew(cxstack, cxstack_max + 1, CONTEXT);	/* XXX should fix CXINC macro */
    return cxstack_ix + 1;
}

void
push_return(retop)
OP *retop;
{
    if (retstack_ix == retstack_max) {
	retstack_max = retstack_max * 3 / 2;
	Renew(retstack, retstack_max, OP*);
    }
    retstack[retstack_ix++] = retop;
}

OP *
pop_return()
{
    if (retstack_ix > 0)
	return retstack[--retstack_ix];
    else
	return Nullop;
}

void
push_scope()
{
    if (scopestack_ix == scopestack_max) {
	scopestack_max = scopestack_max * 3 / 2;
	Renew(scopestack, scopestack_max, I32);
    }
    scopestack[scopestack_ix++] = savestack_ix;

}

void
pop_scope()
{
    I32 oldsave = scopestack[--scopestack_ix];
    LEAVE_SCOPE(oldsave);
}

void
markstack_grow()
{
    I32 oldmax = markstack_max - markstack;
    I32 newmax = oldmax * 3 / 2;

    Renew(markstack, newmax, I32);
    markstack_ptr = markstack + oldmax;
    markstack_max = markstack + newmax;
}

void
savestack_grow()
{
    savestack_max = savestack_max * 3 / 2;
    Renew(savestack, savestack_max, ANY);
}

void
free_tmps()
{
    /* XXX should tmps_floor live in cxstack? */
    I32 myfloor = tmps_floor;
    while (tmps_ix > myfloor) {      /* clean up after last statement */
	SV* sv = tmps_stack[tmps_ix];
	tmps_stack[tmps_ix--] = Nullsv;
	if (sv) {
#ifdef DEBUGGING
	    SvTEMP_off(sv);
#endif
	    SvREFCNT_dec(sv);		/* note, can modify tmps_ix!!! */
	}
    }
}

static SV *
save_scalar_at(sptr)
SV **sptr;
{
    register SV *sv;
    SV *osv = *sptr;

    sv = *sptr = NEWSV(0,0);
    if (SvTYPE(osv) >= SVt_PVMG && SvMAGIC(osv) && SvTYPE(osv) != SVt_PVGV) {
	sv_upgrade(sv, SvTYPE(osv));
	if (SvGMAGICAL(osv)) {
	    MAGIC* mg;
	    bool oldtainted = tainted;
	    mg_get(osv);
	    if (tainting && tainted && (mg = mg_find(osv, 't'))) {
		SAVESPTR(mg->mg_obj);
		mg->mg_obj = osv;
	    }
	    SvFLAGS(osv) |= (SvFLAGS(osv) &
		(SVp_IOK|SVp_NOK|SVp_POK)) >> PRIVSHIFT;
	    tainted = oldtainted;
	}
	SvMAGIC(sv) = SvMAGIC(osv);
	SvFLAGS(sv) |= SvMAGICAL(osv);
	localizing = 1;
	SvSETMAGIC(sv);
	localizing = 0;
    }
    return sv;
}

SV *
save_scalar(gv)
GV *gv;
{
    SSCHECK(3);
    SSPUSHPTR(gv);
    SSPUSHPTR(GvSV(gv));
    SSPUSHINT(SAVEt_SV);
    return save_scalar_at(&GvSV(gv));
}

SV*
save_svref(sptr)
SV **sptr;
{
    SSCHECK(3);
    SSPUSHPTR(sptr);
    SSPUSHPTR(*sptr);
    SSPUSHINT(SAVEt_SVREF);
    return save_scalar_at(sptr);
}

void
save_gp(gv, empty)
GV *gv;
I32 empty;
{
    SSCHECK(6);
    SSPUSHIV((IV)SvLEN(gv));
    SvLEN(gv) = 0; /* forget that anything was allocated here */
    SSPUSHIV((IV)SvCUR(gv));
    SSPUSHPTR(SvPVX(gv));
    SvPOK_off(gv);
    SSPUSHPTR(SvREFCNT_inc(gv));
    SSPUSHPTR(GvGP(gv));
    SSPUSHINT(SAVEt_GP);

    if (empty) {
	register GP *gp;
	Newz(602, gp, 1, GP);
	GvGP(gv) = gp_ref(gp);
	GvSV(gv) = NEWSV(72,0);
	GvLINE(gv) = curcop->cop_line;
	GvEGV(gv) = gv;
    }
    else {
	gp_ref(GvGP(gv));
	GvINTRO_on(gv);
    }
}

AV *
save_ary(gv)
GV *gv;
{
    AV *oav, *av;

    SSCHECK(3);
    SSPUSHPTR(gv);
    SSPUSHPTR(oav = GvAVn(gv));
    SSPUSHINT(SAVEt_AV);

    GvAV(gv) = Null(AV*);
    av = GvAVn(gv);
    if (SvMAGIC(oav)) {
	SvMAGIC(av) = SvMAGIC(oav);
	SvFLAGS(av) |= SvMAGICAL(oav);
	SvMAGICAL_off(oav);
	SvMAGIC(oav) = 0;
	localizing = 1;
	SvSETMAGIC((SV*)av);
	localizing = 0;
    }
    return av;
}

HV *
save_hash(gv)
GV *gv;
{
    HV *ohv, *hv;

    SSCHECK(3);
    SSPUSHPTR(gv);
    SSPUSHPTR(ohv = GvHVn(gv));
    SSPUSHINT(SAVEt_HV);

    GvHV(gv) = Null(HV*);
    hv = GvHVn(gv);
    if (SvMAGIC(ohv)) {
	SvMAGIC(hv) = SvMAGIC(ohv);
	SvFLAGS(hv) |= SvMAGICAL(ohv);
	SvMAGICAL_off(ohv);
	SvMAGIC(ohv) = 0;
	localizing = 1;
	SvSETMAGIC((SV*)hv);
	localizing = 0;
    }
    return hv;
}

void
save_item(item)
register SV *item;
{
    register SV *sv;

    SSCHECK(3);
    SSPUSHPTR(item);		/* remember the pointer */
    sv = NEWSV(0,0);
    sv_setsv(sv,item);
    SSPUSHPTR(sv);		/* remember the value */
    SSPUSHINT(SAVEt_ITEM);
}

void
save_int(intp)
int *intp;
{
    SSCHECK(3);
    SSPUSHINT(*intp);
    SSPUSHPTR(intp);
    SSPUSHINT(SAVEt_INT);
}

void
save_long(longp)
long *longp;
{
    SSCHECK(3);
    SSPUSHLONG(*longp);
    SSPUSHPTR(longp);
    SSPUSHINT(SAVEt_LONG);
}

void
save_I32(intp)
I32 *intp;
{
    SSCHECK(3);
    SSPUSHINT(*intp);
    SSPUSHPTR(intp);
    SSPUSHINT(SAVEt_I32);
}

void
save_I16(intp)
I16 *intp;
{
    SSCHECK(3);
    SSPUSHINT(*intp);
    SSPUSHPTR(intp);
    SSPUSHINT(SAVEt_I16);
}

void
save_iv(ivp)
IV *ivp;
{
    SSCHECK(3);
    SSPUSHIV(*ivp);
    SSPUSHPTR(ivp);
    SSPUSHINT(SAVEt_IV);
}

/* Cannot use save_sptr() to store a char* since the SV** cast will
 * force word-alignment and we'll miss the pointer.
 */
void
save_pptr(pptr)
char **pptr;
{
    SSCHECK(3);
    SSPUSHPTR(*pptr);
    SSPUSHPTR(pptr);
    SSPUSHINT(SAVEt_PPTR);
}

void
save_sptr(sptr)
SV **sptr;
{
    SSCHECK(3);
    SSPUSHPTR(*sptr);
    SSPUSHPTR(sptr);
    SSPUSHINT(SAVEt_SPTR);
}

void
save_nogv(gv)
GV *gv;
{
    SSCHECK(2);
    SSPUSHPTR(gv);
    SSPUSHINT(SAVEt_NSTAB);
}

void
save_hptr(hptr)
HV **hptr;
{
    SSCHECK(3);
    SSPUSHPTR(*hptr);
    SSPUSHPTR(hptr);
    SSPUSHINT(SAVEt_HPTR);
}

void
save_aptr(aptr)
AV **aptr;
{
    SSCHECK(3);
    SSPUSHPTR(*aptr);
    SSPUSHPTR(aptr);
    SSPUSHINT(SAVEt_APTR);
}

void
save_freesv(sv)
SV *sv;
{
    SSCHECK(2);
    SSPUSHPTR(sv);
    SSPUSHINT(SAVEt_FREESV);
}

void
save_freeop(op)
OP *op;
{
    SSCHECK(2);
    SSPUSHPTR(op);
    SSPUSHINT(SAVEt_FREEOP);
}

void
save_freepv(pv)
char *pv;
{
    SSCHECK(2);
    SSPUSHPTR(pv);
    SSPUSHINT(SAVEt_FREEPV);
}

void
save_clearsv(svp)
SV** svp;
{
    SSCHECK(2);
    SSPUSHLONG((long)(svp-curpad));
    SSPUSHINT(SAVEt_CLEARSV);
}

void
save_delete(hv,key,klen)
HV *hv;
char *key;
I32 klen;
{
    SSCHECK(4);
    SSPUSHINT(klen);
    SSPUSHPTR(key);
    SSPUSHPTR(hv);
    SSPUSHINT(SAVEt_DELETE);
}

void
save_list(sarg,maxsarg)
register SV **sarg;
I32 maxsarg;
{
    register SV *sv;
    register I32 i;

    SSCHECK(3 * maxsarg);
    for (i = 1; i <= maxsarg; i++) {
	SSPUSHPTR(sarg[i]);		/* remember the pointer */
	sv = NEWSV(0,0);
	sv_setsv(sv,sarg[i]);
	SSPUSHPTR(sv);			/* remember the value */
	SSPUSHINT(SAVEt_ITEM);
    }
}

void
save_destructor(f,p)
void (*f) _((void*));
void* p;
{
    SSCHECK(3);
    SSPUSHDPTR(f);
    SSPUSHPTR(p);
    SSPUSHINT(SAVEt_DESTRUCTOR);
}

void
leave_scope(base)
I32 base;
{
    register SV *sv;
    register SV *value;
    register GV *gv;
    register AV *av;
    register HV *hv;
    register void* ptr;

    if (base < -1)
	croak("panic: corrupt saved stack index");
    while (savestack_ix > base) {
	switch (SSPOPINT) {
	case SAVEt_ITEM:			/* normal string */
	    value = (SV*)SSPOPPTR;
	    sv = (SV*)SSPOPPTR;
	    sv_replace(sv,value);
	    localizing = 2;
	    SvSETMAGIC(sv);
	    localizing = 0;
	    break;
        case SAVEt_SV:				/* scalar reference */
	    value = (SV*)SSPOPPTR;
	    gv = (GV*)SSPOPPTR;
	    ptr = &GvSV(gv);
	    goto restore_sv;
        case SAVEt_SVREF:			/* scalar reference */
	    value = (SV*)SSPOPPTR;
	    ptr = SSPOPPTR;
	restore_sv:
	    sv = *(SV**)ptr;
	    if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv) &&
		SvTYPE(sv) != SVt_PVGV)
	    {
		(void)SvUPGRADE(value, SvTYPE(sv));
		SvMAGIC(value) = SvMAGIC(sv);
		SvFLAGS(value) |= SvMAGICAL(sv);
		SvMAGICAL_off(sv);
		SvMAGIC(sv) = 0;
	    }
	    else if (SvTYPE(value) >= SVt_PVMG && SvMAGIC(value) &&
		     SvTYPE(value) != SVt_PVGV)
	    {
		SvFLAGS(value) |= (SvFLAGS(value) &
				   (SVp_IOK|SVp_NOK|SVp_POK)) >> PRIVSHIFT;
		SvMAGICAL_off(value);
		SvMAGIC(value) = 0;
	    }
            SvREFCNT_dec(sv);
	    *(SV**)ptr = value;
	    localizing = 2;
	    SvSETMAGIC(value);
	    localizing = 0;
            break;
        case SAVEt_AV:				/* array reference */
	    av = (AV*)SSPOPPTR;
	    gv = (GV*)SSPOPPTR;
	    if (GvAV(gv)) {
		AV *goner = GvAV(gv);
		SvMAGIC(av) = SvMAGIC(goner);
		SvFLAGS(av) |= SvMAGICAL(goner);
		SvMAGICAL_off(goner);
		SvMAGIC(goner) = 0;
		SvREFCNT_dec(goner);
	    }
            GvAV(gv) = av;
	    if (SvMAGICAL(av)) {
		localizing = 2;
		SvSETMAGIC((SV*)av);
		localizing = 0;
	    }
            break;
        case SAVEt_HV:				/* hash reference */
	    hv = (HV*)SSPOPPTR;
	    gv = (GV*)SSPOPPTR;
	    if (GvHV(gv)) {
		HV *goner = GvHV(gv);
		SvMAGIC(hv) = SvMAGIC(goner);
		SvFLAGS(hv) |= SvMAGICAL(goner);
		SvMAGICAL_off(goner);
		SvMAGIC(goner) = 0;
		SvREFCNT_dec(goner);
	    }
            GvHV(gv) = hv;
	    if (SvMAGICAL(hv)) {
		localizing = 2;
		SvSETMAGIC((SV*)hv);
		localizing = 0;
	    }
            break;
	case SAVEt_INT:				/* int reference */
	    ptr = SSPOPPTR;
	    *(int*)ptr = (int)SSPOPINT;
	    break;
	case SAVEt_LONG:			/* long reference */
	    ptr = SSPOPPTR;
	    *(long*)ptr = (long)SSPOPLONG;
	    break;
	case SAVEt_I32:				/* I32 reference */
	    ptr = SSPOPPTR;
	    *(I32*)ptr = (I32)SSPOPINT;
	    break;
	case SAVEt_I16:				/* I16 reference */
	    ptr = SSPOPPTR;
	    *(I16*)ptr = (I16)SSPOPINT;
	    break;
	case SAVEt_IV:				/* IV reference */
	    ptr = SSPOPPTR;
	    *(IV*)ptr = (IV)SSPOPIV;
	    break;
	case SAVEt_SPTR:			/* SV* reference */
	    ptr = SSPOPPTR;
	    *(SV**)ptr = (SV*)SSPOPPTR;
	    break;
	case SAVEt_PPTR:			/* char* reference */
	    ptr = SSPOPPTR;
	    *(char**)ptr = (char*)SSPOPPTR;
	    break;
	case SAVEt_HPTR:			/* HV* reference */
	    ptr = SSPOPPTR;
	    *(HV**)ptr = (HV*)SSPOPPTR;
	    break;
	case SAVEt_APTR:			/* AV* reference */
	    ptr = SSPOPPTR;
	    *(AV**)ptr = (AV*)SSPOPPTR;
	    break;
	case SAVEt_NSTAB:
	    gv = (GV*)SSPOPPTR;
	    (void)sv_clear((SV*)gv);
	    break;
	case SAVEt_GP:				/* scalar reference */
	    ptr = SSPOPPTR;
	    gv = (GV*)SSPOPPTR;
            gp_free(gv);
            GvGP(gv) = (GP*)ptr;
            if (SvPOK(gv) && SvLEN(gv) > 0) {
                Safefree(SvPVX(gv));
            }
            SvPVX(gv) = (char *)SSPOPPTR;
            SvCUR(gv) = (STRLEN)SSPOPIV;
            SvLEN(gv) = (STRLEN)SSPOPIV;
	    SvREFCNT_dec(gv);
            break;
	case SAVEt_FREESV:
	    ptr = SSPOPPTR;
	    SvREFCNT_dec((SV*)ptr);
	    break;
	case SAVEt_FREEOP:
	    ptr = SSPOPPTR;
	    if (comppad)
		curpad = AvARRAY(comppad);
	    op_free((OP*)ptr);
	    break;
	case SAVEt_FREEPV:
	    ptr = SSPOPPTR;
	    Safefree((char*)ptr);
	    break;
	case SAVEt_CLEARSV:
	    ptr = (void*)&curpad[SSPOPLONG];
	    sv = *(SV**)ptr;
	    /* Can clear pad variable in place? */
	    if (SvREFCNT(sv) <= 1 && !SvOBJECT(sv)) {
		if (SvTHINKFIRST(sv)) {
		    if (SvREADONLY(sv))
			croak("panic: leave_scope clearsv");
		    if (SvROK(sv))
			sv_unref(sv);
		}
		if (SvMAGICAL(sv))
		    mg_free(sv);

		switch (SvTYPE(sv)) {
		case SVt_NULL:
		    break;
		case SVt_PVAV:
		    av_clear((AV*)sv);
		    break;
		case SVt_PVHV:
		    hv_clear((HV*)sv);
		    break;
		case SVt_PVCV:
		    croak("panic: leave_scope pad code");
		case SVt_RV:
		case SVt_IV:
		case SVt_NV:
		    (void)SvOK_off(sv);
		    break;
		default:
		    (void)SvOK_off(sv);
		    (void)SvOOK_off(sv);
		    break;
		}
	    }
	    else {	/* Someone has a claim on this, so abandon it. */
		U32 padflags = SvFLAGS(sv) & (SVs_PADBUSY|SVs_PADMY|SVs_PADTMP);
		SvREFCNT_dec(sv);	/* Cast current value to the winds. */
		switch (SvTYPE(sv)) {	/* Console ourselves with a new value */
		case SVt_PVAV:	*(SV**)ptr = (SV*)newAV();	break;
		case SVt_PVHV:	*(SV**)ptr = (SV*)newHV();	break;
		default:	*(SV**)ptr = NEWSV(0,0);	break;
		}
		SvFLAGS(*(SV**)ptr) |= padflags; /* preserve pad nature */
	    }
	    break;
	case SAVEt_DELETE:
	    ptr = SSPOPPTR;
	    hv = (HV*)ptr;
	    ptr = SSPOPPTR;
	    (void)hv_delete(hv, (char*)ptr, (U32)SSPOPINT, G_DISCARD);
	    Safefree(ptr);
	    break;
	case SAVEt_DESTRUCTOR:
	    ptr = SSPOPPTR;
	    (*SSPOPDPTR)(ptr);
	    break;
	case SAVEt_REGCONTEXT:
	    {
		I32 delta = SSPOPINT;
		savestack_ix -= delta;	/* regexp must have croaked */
	    }
	    break;
	case SAVEt_STACK_POS:		/* Position on Perl stack */
	    {
		I32 delta = SSPOPINT;
		stack_sp = stack_base + delta;
	    }
	    break;
	default:
	    croak("panic: leave_scope inconsistency");
	}
    }
}

#ifdef DEBUGGING

void
cx_dump(cx)
CONTEXT* cx;
{
    PerlIO_printf(Perl_debug_log, "CX %ld = %s\n", (long)(cx - cxstack), block_type[cx->cx_type]);
    if (cx->cx_type != CXt_SUBST) {
	PerlIO_printf(Perl_debug_log, "BLK_OLDSP = %ld\n", (long)cx->blk_oldsp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDCOP = 0x%lx\n", (long)cx->blk_oldcop);
	PerlIO_printf(Perl_debug_log, "BLK_OLDMARKSP = %ld\n", (long)cx->blk_oldmarksp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDSCOPESP = %ld\n", (long)cx->blk_oldscopesp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDRETSP = %ld\n", (long)cx->blk_oldretsp);
	PerlIO_printf(Perl_debug_log, "BLK_OLDPM = 0x%lx\n", (long)cx->blk_oldpm);
	PerlIO_printf(Perl_debug_log, "BLK_GIMME = %s\n", cx->blk_gimme ? "LIST" : "SCALAR");
    }
    switch (cx->cx_type) {
    case CXt_NULL:
    case CXt_BLOCK:
	break;
    case CXt_SUB:
	PerlIO_printf(Perl_debug_log, "BLK_SUB.CV = 0x%lx\n",
		(long)cx->blk_sub.cv);
	PerlIO_printf(Perl_debug_log, "BLK_SUB.GV = 0x%lx\n",
		(long)cx->blk_sub.gv);
	PerlIO_printf(Perl_debug_log, "BLK_SUB.DFOUTGV = 0x%lx\n",
		(long)cx->blk_sub.dfoutgv);
	PerlIO_printf(Perl_debug_log, "BLK_SUB.OLDDEPTH = %ld\n",
		(long)cx->blk_sub.olddepth);
	PerlIO_printf(Perl_debug_log, "BLK_SUB.HASARGS = %d\n",
		(int)cx->blk_sub.hasargs);
	break;
    case CXt_EVAL:
	PerlIO_printf(Perl_debug_log, "BLK_EVAL.OLD_IN_EVAL = %ld\n",
		(long)cx->blk_eval.old_in_eval);
	PerlIO_printf(Perl_debug_log, "BLK_EVAL.OLD_OP_TYPE = %s (%s)\n",
		op_name[cx->blk_eval.old_op_type],
		op_desc[cx->blk_eval.old_op_type]);
	PerlIO_printf(Perl_debug_log, "BLK_EVAL.OLD_NAME = %s\n",
		cx->blk_eval.old_name);
	PerlIO_printf(Perl_debug_log, "BLK_EVAL.OLD_EVAL_ROOT = 0x%lx\n",
		(long)cx->blk_eval.old_eval_root);
	break;

    case CXt_LOOP:
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.LABEL = %s\n",
		cx->blk_loop.label);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.RESETSP = %ld\n",
		(long)cx->blk_loop.resetsp);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.REDO_OP = 0x%lx\n",
		(long)cx->blk_loop.redo_op);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.NEXT_OP = 0x%lx\n",
		(long)cx->blk_loop.next_op);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.LAST_OP = 0x%lx\n",
		(long)cx->blk_loop.last_op);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERIX = %ld\n",
		(long)cx->blk_loop.iterix);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERARY = 0x%lx\n",
		(long)cx->blk_loop.iterary);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERVAR = 0x%lx\n",
		(long)cx->blk_loop.itervar);
	if (cx->blk_loop.itervar)
	    PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERSAVE = 0x%lx\n",
		(long)cx->blk_loop.itersave);
	PerlIO_printf(Perl_debug_log, "BLK_LOOP.ITERLVAL = 0x%lx\n",
		(long)cx->blk_loop.iterlval);
	break;

    case CXt_SUBST:
	PerlIO_printf(Perl_debug_log, "SB_ITERS = %ld\n",
		(long)cx->sb_iters);
	PerlIO_printf(Perl_debug_log, "SB_MAXITERS = %ld\n",
		(long)cx->sb_maxiters);
	PerlIO_printf(Perl_debug_log, "SB_SAFEBASE = %ld\n",
		(long)cx->sb_safebase);
	PerlIO_printf(Perl_debug_log, "SB_ONCE = %ld\n",
		(long)cx->sb_once);
	PerlIO_printf(Perl_debug_log, "SB_ORIG = %s\n",
		cx->sb_orig);
	PerlIO_printf(Perl_debug_log, "SB_DSTR = 0x%lx\n",
		(long)cx->sb_dstr);
	PerlIO_printf(Perl_debug_log, "SB_TARG = 0x%lx\n",
		(long)cx->sb_targ);
	PerlIO_printf(Perl_debug_log, "SB_S = 0x%lx\n",
		(long)cx->sb_s);
	PerlIO_printf(Perl_debug_log, "SB_M = 0x%lx\n",
		(long)cx->sb_m);
	PerlIO_printf(Perl_debug_log, "SB_STREND = 0x%lx\n",
		(long)cx->sb_strend);
	PerlIO_printf(Perl_debug_log, "SB_RXRES = 0x%lx\n",
		(long)cx->sb_rxres);
	break;
    }
}
#endif
