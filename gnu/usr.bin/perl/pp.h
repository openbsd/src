/*    pp.h
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#define ARGS
#define ARGSproto void
#define dARGS
#define PP(s) OP* s(ARGS) dARGS

#define SP sp
#define MARK mark
#define TARG targ

#define PUSHMARK(p) if (++markstack_ptr == markstack_max)	\
			markstack_grow();			\
		    *markstack_ptr = (p) - stack_base

#define TOPMARK		(*markstack_ptr)
#define POPMARK		(*markstack_ptr--)

#define dSP		register SV **sp = stack_sp
#define dMARK		register SV **mark = stack_base + POPMARK
#define dORIGMARK	I32 origmark = mark - stack_base
#define SETORIGMARK	origmark = mark - stack_base
#define ORIGMARK	(stack_base + origmark)

#define SPAGAIN		sp = stack_sp
#define MSPAGAIN	sp = stack_sp; mark = ORIGMARK

#define GETTARGETSTACKED targ = (op->op_flags & OPf_STACKED ? POPs : PAD_SV(op->op_targ))
#define dTARGETSTACKED SV * GETTARGETSTACKED

#define GETTARGET targ = PAD_SV(op->op_targ)
#define dTARGET SV * GETTARGET

#define GETATARGET targ = (op->op_flags & OPf_STACKED ? sp[-1] : PAD_SV(op->op_targ))
#define dATARGET SV * GETATARGET

#define dTARG SV *targ

#define NORMAL op->op_next
#define DIE return die

#define PUTBACK		stack_sp = sp
#define RETURN		return PUTBACK, NORMAL
#define RETURNOP(o)	return PUTBACK, o
#define RETURNX(x)	return x, PUTBACK, NORMAL

#define POPs		(*sp--)
#define POPp		(SvPVx(POPs, na))
#define POPn		(SvNVx(POPs))
#define POPi		((IV)SvIVx(POPs))
#define POPl		((long)SvIVx(POPs))

#define TOPs		(*sp)
#define TOPp		(SvPV(TOPs, na))
#define TOPn		(SvNV(TOPs))
#define TOPi		((IV)SvIV(TOPs))
#define TOPl		((long)SvIV(TOPs))

/* Go to some pains in the rare event that we must extend the stack. */
#define EXTEND(p,n)	STMT_START { if (stack_max - p < (n)) {		  	    \
			    sp = stack_grow(sp,p, (int) (n));		    \
			} } STMT_END

/* Same thing, but update mark register too. */
#define MEXTEND(p,n)	STMT_START {if (stack_max - p < (n)) {			    \
			    int markoff = mark - stack_base;		    \
			    sp = stack_grow(sp,p,(int) (n));		    \
			    mark = stack_base + markoff;		    \
			} } STMT_END

#define PUSHs(s)	(*++sp = (s))
#define PUSHTARG	STMT_START { SvSETMAGIC(TARG); PUSHs(TARG); } STMT_END
#define PUSHp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); PUSHTARG; } STMT_END
#define PUSHn(n)	STMT_START { sv_setnv(TARG, (double)(n)); PUSHTARG; } STMT_END
#define PUSHi(i)	STMT_START { sv_setiv(TARG, (IV)(i)); PUSHTARG; } STMT_END

#define XPUSHs(s)	STMT_START { EXTEND(sp,1); (*++sp = (s)); } STMT_END
#define XPUSHTARG	STMT_START { SvSETMAGIC(TARG); XPUSHs(TARG); } STMT_END
#define XPUSHp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); XPUSHTARG; } STMT_END
#define XPUSHn(n)	STMT_START { sv_setnv(TARG, (double)(n)); XPUSHTARG; } STMT_END
#define XPUSHi(i)	STMT_START { sv_setiv(TARG, (IV)(i)); XPUSHTARG; } STMT_END

#define SETs(s)		(*sp = s)
#define SETTARG		STMT_START { SvSETMAGIC(TARG); SETs(TARG); } STMT_END
#define SETp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); SETTARG; } STMT_END
#define SETn(n)		STMT_START { sv_setnv(TARG, (double)(n)); SETTARG; } STMT_END
#define SETi(i)		STMT_START { sv_setiv(TARG, (IV)(i)); SETTARG; } STMT_END

#ifdef OVERLOAD
#define SETsv(sv)	STMT_START { sv_setsv(TARG, (sv)); SETTARG; } STMT_END
#endif /* OVERLOAD */

#define dTOPss		SV *sv = TOPs
#define dPOPss		SV *sv = POPs
#define dTOPnv		double value = TOPn
#define dPOPnv		double value = POPn
#define dTOPiv		IV value = TOPi
#define dPOPiv		IV value = POPi

#define dPOPPOPssrl	SV *right = POPs; SV *left = POPs
#define dPOPPOPnnrl	double right = POPn; double left = POPn
#define dPOPPOPiirl	IV right = POPi; IV left = POPi

#define dPOPTOPssrl	SV *right = POPs; SV *left = TOPs
#define dPOPTOPnnrl	double right = POPn; double left = TOPn
#define dPOPTOPiirl	IV right = POPi; IV left = TOPi

#define RETPUSHYES	RETURNX(PUSHs(&sv_yes))
#define RETPUSHNO	RETURNX(PUSHs(&sv_no))
#define RETPUSHUNDEF	RETURNX(PUSHs(&sv_undef))

#define RETSETYES	RETURNX(SETs(&sv_yes))
#define RETSETNO	RETURNX(SETs(&sv_no))
#define RETSETUNDEF	RETURNX(SETs(&sv_undef))

#define ARGTARG		op->op_targ
#define MAXARG		op->op_private

#define SWITCHSTACK(f,t)	AvFILL(f) = sp - stack_base;		\
				stack_base = AvARRAY(t);		\
				stack_max = stack_base + AvMAX(t);	\
				sp = stack_sp = stack_base + AvFILL(t);	\
				stack = t;

#ifdef OVERLOAD

#define AMGf_noright	1
#define AMGf_noleft	2
#define AMGf_assign	4
#define AMGf_unary	8

#define tryAMAGICbinW(meth,assign,set) STMT_START { \
          if (amagic_generation) { \
	    SV* tmpsv; \
	    SV* right= *(sp); SV* left= *(sp-1);\
	    if ((SvAMAGIC(left)||SvAMAGIC(right))&&\
		(tmpsv=amagic_call(left, \
				   right, \
				   CAT2(meth,_amg), \
				   (assign)? AMGf_assign: 0))) {\
	       SPAGAIN;	\
	       (void)POPs; set(tmpsv); RETURN; } \
	  } \
	} STMT_END

#define tryAMAGICbin(meth,assign) tryAMAGICbinW(meth,assign,SETsv)
#define tryAMAGICbinSET(meth,assign) tryAMAGICbinW(meth,assign,SETs)

#define AMG_CALLun(sv,meth) amagic_call(sv,&sv_undef,  \
					CAT2(meth,_amg),AMGf_noright | AMGf_unary)
#define AMG_CALLbinL(left,right,meth) \
            amagic_call(left,right,CAT2(meth,_amg),AMGf_noright)

#define tryAMAGICunW(meth,set) STMT_START { \
          if (amagic_generation) { \
	    SV* tmpsv; \
	    SV* arg= *(sp); \
	    if ((SvAMAGIC(arg))&&\
		(tmpsv=AMG_CALLun(arg,meth))) {\
	       SPAGAIN;	\
	       set(tmpsv); RETURN; } \
	  } \
	} STMT_END

#define tryAMAGICun(meth) tryAMAGICunW(meth,SETsv)
#define tryAMAGICunSET(meth) tryAMAGICunW(meth,SETs)

#define opASSIGN (op->op_flags & OPf_STACKED)

/* newSVsv does not behave as advertised, so we copy missing
 * information by hand */


#define RvDEEPCP(rv) STMT_START { SV* ref=SvRV(rv);      \
  if (SvREFCNT(ref)>1) {                 \
    SvREFCNT_dec(ref);                   \
    SvRV(rv)=AMG_CALLun(rv,copy);        \
  } } STMT_END
#else

#define tryAMAGICbin(a,b)
#define tryAMAGICbinSET(a,b)
#define tryAMAGICun(a)
#define tryAMAGICunSET(a)

#endif /* OVERLOAD */
