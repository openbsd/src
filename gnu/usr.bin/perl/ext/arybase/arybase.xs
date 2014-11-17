#define PERL_NO_GET_CONTEXT     /* we want efficiency */
#define PERL_EXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "feature.h"

/* ... op => info map ................................................. */

typedef struct {
 OP *(*old_pp)(pTHX);
 IV base;
} ab_op_info;

#define PTABLE_NAME        ptable_map
#define PTABLE_VAL_FREE(V) PerlMemShared_free(V)
#include "ptable.h"
#define ptable_map_store(T, K, V) ptable_map_store(aPTBLMS_ (T), (K), (V))

STATIC ptable *ab_op_map = NULL;

#ifdef USE_ITHREADS
STATIC perl_mutex ab_op_map_mutex;
#endif

STATIC const ab_op_info *ab_map_fetch(const OP *o, ab_op_info *oi) {
 const ab_op_info *val;

#ifdef USE_ITHREADS
 MUTEX_LOCK(&ab_op_map_mutex);
#endif

 val = (ab_op_info *)ptable_fetch(ab_op_map, o);
 if (val) {
  *oi = *val;
  val = oi;
 }

#ifdef USE_ITHREADS
 MUTEX_UNLOCK(&ab_op_map_mutex);
#endif

 return val;
}

STATIC const ab_op_info *ab_map_store_locked(
 pPTBLMS_ const OP *o, OP *(*old_pp)(pTHX), IV base
) {
#define ab_map_store_locked(O, PP, B) \
  ab_map_store_locked(aPTBLMS_ (O), (PP), (B))
 ab_op_info *oi;

 if (!(oi = (ab_op_info *)ptable_fetch(ab_op_map, o))) {
  oi = (ab_op_info *)PerlMemShared_malloc(sizeof *oi);
  ptable_map_store(ab_op_map, o, oi);
 }

 oi->old_pp = old_pp;
 oi->base   = base;
 return oi;
}

STATIC void ab_map_store(
 pPTBLMS_ const OP *o, OP *(*old_pp)(pTHX), IV base)
{
#define ab_map_store(O, PP, B) ab_map_store(aPTBLMS_ (O),(PP),(B))

#ifdef USE_ITHREADS
 MUTEX_LOCK(&ab_op_map_mutex);
#endif

 ab_map_store_locked(o, old_pp, base);

#ifdef USE_ITHREADS
 MUTEX_UNLOCK(&ab_op_map_mutex);
#endif
}

STATIC void ab_map_delete(pTHX_ const OP *o) {
#define ab_map_delete(O) ab_map_delete(aTHX_ (O))
#ifdef USE_ITHREADS
 MUTEX_LOCK(&ab_op_map_mutex);
#endif

 ptable_map_store(ab_op_map, o, NULL);

#ifdef USE_ITHREADS
 MUTEX_UNLOCK(&ab_op_map_mutex);
#endif
}

/* ... $[ Implementation .............................................. */

#define hintkey     "$["
#define hintkey_len  (sizeof(hintkey)-1)

STATIC SV * ab_hint(pTHX_ const bool create) {
#define ab_hint(c) ab_hint(aTHX_ c)
 dVAR;
 SV **val
  = hv_fetch(GvHV(PL_hintgv), hintkey, hintkey_len, create);
 if (!val)
  return 0;
 return *val;
}

/* current base at compile time */
STATIC IV current_base(pTHX) {
#define current_base() current_base(aTHX)
 SV *hsv = ab_hint(0);
 assert(FEATURE_ARYBASE_IS_ENABLED);
 if (!hsv || !SvOK(hsv)) return 0;
 return SvIV(hsv);
}

STATIC void set_arybase_to(pTHX_ IV base) {
#define set_arybase_to(base) set_arybase_to(aTHX_ (base))
 dVAR;
 SV *hsv = ab_hint(1);
 sv_setiv_mg(hsv, base);
}

#define old_ck(opname) STATIC OP *(*ab_old_ck_##opname)(pTHX_ OP *) = 0
old_ck(sassign);
old_ck(aassign);
old_ck(aelem);
old_ck(aslice);
old_ck(lslice);
old_ck(av2arylen);
old_ck(splice);
old_ck(keys);
old_ck(each);
old_ck(substr);
old_ck(rindex);
old_ck(index);
old_ck(pos);

STATIC bool ab_op_is_dollar_bracket(pTHX_ OP *o) {
#define ab_op_is_dollar_bracket(o) ab_op_is_dollar_bracket(aTHX_ (o))
 OP *c;
 return o->op_type == OP_RV2SV && (o->op_flags & OPf_KIDS)
  && (c = cUNOPx(o)->op_first)
  && c->op_type == OP_GV
  && GvSTASH(cGVOPx_gv(c)) == PL_defstash
  && strEQ(GvNAME(cGVOPx_gv(c)), "[");
}

STATIC void ab_neuter_dollar_bracket(pTHX_ OP *o) {
#define ab_neuter_dollar_bracket(o) ab_neuter_dollar_bracket(aTHX_ (o))
 OP *oldc, *newc;
 /*
  * Must replace the core's $[ with something that can accept assignment
  * of non-zero value and can be local()ised.  Simplest thing is a
  * different global variable.
  */
 oldc = cUNOPx(o)->op_first;
 newc = newGVOP(OP_GV, 0,
   gv_fetchpvs("arybase::leftbrack", GV_ADDMULTI, SVt_PVGV));
 cUNOPx(o)->op_first = newc;
 op_free(oldc);
}

STATIC void ab_process_assignment(pTHX_ OP *left, OP *right) {
#define ab_process_assignment(l, r) \
    ab_process_assignment(aTHX_ (l), (r))
 if (ab_op_is_dollar_bracket(left) && right->op_type == OP_CONST) {
  set_arybase_to(SvIV(cSVOPx_sv(right)));
  ab_neuter_dollar_bracket(left);
  Perl_ck_warner_d(aTHX_
   packWARN(WARN_DEPRECATED), "Use of assignment to $[ is deprecated"
  );
 }
}

STATIC OP *ab_ck_sassign(pTHX_ OP *o) {
 o = (*ab_old_ck_sassign)(aTHX_ o);
 if (o->op_type == OP_SASSIGN && FEATURE_ARYBASE_IS_ENABLED) {
  OP *right = cBINOPx(o)->op_first;
  OP *left = right->op_sibling;
  if (left) ab_process_assignment(left, right);
 }
 return o;
}

STATIC OP *ab_ck_aassign(pTHX_ OP *o) {
 o = (*ab_old_ck_aassign)(aTHX_ o);
 if (o->op_type == OP_AASSIGN && FEATURE_ARYBASE_IS_ENABLED) {
  OP *right = cBINOPx(o)->op_first;
  OP *left = cBINOPx(right->op_sibling)->op_first->op_sibling;
  right = cBINOPx(right)->op_first->op_sibling;
  ab_process_assignment(left, right);
 }
 return o;
}

void
tie(pTHX_ SV * const sv, SV * const obj, HV *const stash)
{
    SV *rv = newSV_type(SVt_RV);

    SvRV_set(rv, obj ? SvREFCNT_inc_simple_NN(obj) : newSV(0));
    SvROK_on(rv);
    sv_bless(rv, stash);

    sv_unmagic((SV *)sv, PERL_MAGIC_tiedscalar);
    sv_magic((SV *)sv, rv, PERL_MAGIC_tiedscalar, NULL, 0);
    SvREFCNT_dec(rv); /* As sv_magic increased it by one.  */
}

/* This function converts from base-based to 0-based an index to be passed
   as an argument. */
static IV
adjust_index(IV index, IV base)
{
 if (index >= base || index > -1) return index-base;
 return index;
}
/* This function converts from 0-based to base-based an index to
   be returned. */
static IV
adjust_index_r(IV index, IV base)
{
 return index + base;
}

#define replace_sv(sv,base) \
 ((sv) = sv_2mortal(newSViv(adjust_index(SvIV(sv),base))))
#define replace_sv_r(sv,base) \
 ((sv) = sv_2mortal(newSViv(adjust_index_r(SvIV(sv),base))))

static OP *ab_pp_basearg(pTHX) {
 dVAR; dSP;
 SV **firstp = NULL;
 SV **svp;
 UV count = 1;
 ab_op_info oi;
 ab_map_fetch(PL_op, &oi);
 
 switch (PL_op->op_type) {
 case OP_AELEM:
  firstp = SP;
  break;
 case OP_ASLICE:
  firstp = PL_stack_base + TOPMARK + 1;
  count = SP-firstp;
  break;
 case OP_LSLICE:
  firstp = PL_stack_base + *(PL_markstack_ptr-1)+1;
  count = TOPMARK - *(PL_markstack_ptr-1);
  if (GIMME != G_ARRAY) {
   firstp += count-1;
   count = 1;
  }
  break;
 case OP_SPLICE:
  if (SP - PL_stack_base - TOPMARK >= 2)
   firstp = PL_stack_base + TOPMARK + 2;
  else count = 0;
  break;
 case OP_SUBSTR:
  firstp = SP-(PL_op->op_private & 7)+2;
  break;
 default:
  DIE(aTHX_
     "panic: invalid op type for arybase.xs:ab_pp_basearg: %d",
      PL_op->op_type);
 }
 svp = firstp;
 while (count--) replace_sv(*svp,oi.base), svp++;
 return (*oi.old_pp)(aTHX);
}

static OP *ab_pp_av2arylen(pTHX) {
 dSP; dVAR;
 SV *sv;
 ab_op_info oi;
 OP *ret;
 ab_map_fetch(PL_op, &oi);
 ret = (*oi.old_pp)(aTHX);
 if (PL_op->op_flags & OPf_MOD || LVRET) {
  sv = newSV(0);
  tie(aTHX_ sv, TOPs, gv_stashpv("arybase::mg",1));
  SETs(sv);
 }
 else {
  SvGETMAGIC(TOPs);
  if (SvOK(TOPs)) replace_sv_r(TOPs, oi.base);
 }
 return ret;
}

static OP *ab_pp_keys(pTHX) {
 dVAR; dSP;
 ab_op_info oi;
 OP *retval;
 const I32 offset = SP - PL_stack_base;
 SV **svp;
 ab_map_fetch(PL_op, &oi);
 retval = (*oi.old_pp)(aTHX);
 if (GIMME_V == G_SCALAR) return retval;
 SPAGAIN;
 svp = PL_stack_base + offset;
 while (svp <= SP) replace_sv_r(*svp,oi.base), ++svp;
 return retval; 
}

static OP *ab_pp_each(pTHX) {
 dVAR; dSP;
 ab_op_info oi;
 OP *retval;
 const I32 offset = SP - PL_stack_base;
 ab_map_fetch(PL_op, &oi);
 retval = (*oi.old_pp)(aTHX);
 SPAGAIN;
 if (GIMME_V == G_SCALAR) {
  if (SvOK(TOPs)) replace_sv_r(TOPs,oi.base);
 }
 else if (offset < SP - PL_stack_base) replace_sv_r(TOPm1s,oi.base);
 return retval; 
}

static OP *ab_pp_index(pTHX) {
 dVAR; dSP;
 ab_op_info oi;
 OP *retval;
 ab_map_fetch(PL_op, &oi);
 if (MAXARG == 3 && TOPs) replace_sv(TOPs,oi.base);
 retval = (*oi.old_pp)(aTHX);
 SPAGAIN;
 replace_sv_r(TOPs,oi.base);
 return retval; 
}

static OP *ab_ck_base(pTHX_ OP *o)
{
 OP * (*old_ck)(pTHX_ OP *o) = 0;
 OP * (*new_pp)(pTHX)        = ab_pp_basearg;
 switch (o->op_type) {
 case OP_AELEM    : old_ck = ab_old_ck_aelem    ; break;
 case OP_ASLICE   : old_ck = ab_old_ck_aslice   ; break;
 case OP_LSLICE   : old_ck = ab_old_ck_lslice   ; break;
 case OP_AV2ARYLEN: old_ck = ab_old_ck_av2arylen; break;
 case OP_SPLICE   : old_ck = ab_old_ck_splice   ; break;
 case OP_KEYS     : old_ck = ab_old_ck_keys     ; break;
 case OP_EACH     : old_ck = ab_old_ck_each     ; break;
 case OP_SUBSTR   : old_ck = ab_old_ck_substr   ; break;
 case OP_RINDEX   : old_ck = ab_old_ck_rindex   ; break;
 case OP_INDEX    : old_ck = ab_old_ck_index    ; break;
 case OP_POS      : old_ck = ab_old_ck_pos      ; break;
 default:
  DIE(aTHX_
     "panic: invalid op type for arybase.xs:ab_ck_base: %d",
      PL_op->op_type);
 }
 o = (*old_ck)(aTHX_ o);
 if (!FEATURE_ARYBASE_IS_ENABLED) return o;
 /* We need two switch blocks, as the type may have changed. */
 switch (o->op_type) {
 case OP_AELEM    :
 case OP_ASLICE   :
 case OP_LSLICE   :
 case OP_SPLICE   :
 case OP_SUBSTR   : break;
 case OP_POS      :
 case OP_AV2ARYLEN: new_pp = ab_pp_av2arylen    ; break;
 case OP_AKEYS    : new_pp = ab_pp_keys         ; break;
 case OP_AEACH    : new_pp = ab_pp_each         ; break;
 case OP_RINDEX   :
 case OP_INDEX    : new_pp = ab_pp_index        ; break;
 default: return o;
 }
 {
  IV const base = current_base();
  if (base) {
   ab_map_store(o, o->op_ppaddr, base);
   o->op_ppaddr = new_pp;
   /* Break the aelemfast optimisation */
   if (o->op_type == OP_AELEM &&
       cBINOPo->op_first->op_sibling->op_type == OP_CONST) {
     cBINOPo->op_first->op_sibling
      = newUNOP(OP_NULL,0,cBINOPo->op_first->op_sibling);
   }
  }
  else ab_map_delete(o);
 }
 return o;
}


STATIC U32 ab_initialized = 0;

/* --- XS ------------------------------------------------------------- */

MODULE = arybase	PACKAGE = arybase
PROTOTYPES: DISABLE

BOOT:
{
    GV *const gv = gv_fetchpvn("[", 1, GV_ADDMULTI|GV_NOTQUAL, SVt_PV);
    sv_unmagic(GvSV(gv), PERL_MAGIC_sv); /* This is *our* scalar now! */
    tie(aTHX_ GvSV(gv), NULL, GvSTASH(CvGV(cv)));

    if (!ab_initialized++) {
	ab_op_map = ptable_new();
#ifdef USE_ITHREADS
	MUTEX_INIT(&ab_op_map_mutex);
#endif
#define check(uc,lc,ck) \
		wrap_op_checker(OP_##uc, ab_ck_##ck, &ab_old_ck_##lc)
	check(SASSIGN,  sassign,  sassign);
	check(AASSIGN,  aassign,  aassign);
	check(AELEM,    aelem,    base);
	check(ASLICE,   aslice,   base);
	check(LSLICE,   lslice,   base);
	check(AV2ARYLEN,av2arylen,base);
	check(SPLICE,   splice,   base);
	check(KEYS,     keys,     base);
	check(EACH,     each,     base);
	check(SUBSTR,   substr,   base);
	check(RINDEX,   rindex,   base);
	check(INDEX,    index,    base);
	check(POS,      pos,      base);
    }
}

void
FETCH(...)
    PREINIT:
	SV *ret = FEATURE_ARYBASE_IS_ENABLED
		   ? cop_hints_fetch_pvs(PL_curcop, "$[", 0)
		   : 0;
    PPCODE:
	if (!ret || !SvOK(ret)) mXPUSHi(0);
	else XPUSHs(ret);

void
STORE(SV *sv, IV newbase)
    CODE:
      PERL_UNUSED_VAR(sv);
      if (FEATURE_ARYBASE_IS_ENABLED) {
	SV *base = cop_hints_fetch_pvs(PL_curcop, "$[", 0);
	if (SvOK(base) ? SvIV(base) == newbase : !newbase) XSRETURN_EMPTY;
	Perl_croak(aTHX_ "That use of $[ is unsupported");
      }
      else if (newbase)
	Perl_croak(aTHX_ "Assigning non-zero to $[ is no longer possible");


MODULE = arybase	PACKAGE = arybase::mg
PROTOTYPES: DISABLE

void
FETCH(SV *sv)
    PPCODE:
	if (!SvROK(sv) || SvTYPE(SvRV(sv)) >= SVt_PVAV)
	    Perl_croak(aTHX_ "Not a SCALAR reference");
	{
	    SV *base = FEATURE_ARYBASE_IS_ENABLED
			 ? cop_hints_fetch_pvs(PL_curcop, "$[", 0)
			 : 0;
	    SvGETMAGIC(SvRV(sv));
	    if (!SvOK(SvRV(sv))) XSRETURN_UNDEF;
	    mXPUSHi(adjust_index_r(
		SvIV_nomg(SvRV(sv)), base&&SvOK(base)?SvIV(base):0
	    ));
	}

void
STORE(SV *sv, SV *newbase)
    CODE:
	if (!SvROK(sv) || SvTYPE(SvRV(sv)) >= SVt_PVAV)
	    Perl_croak(aTHX_ "Not a SCALAR reference");
	{
	    SV *base = FEATURE_ARYBASE_IS_ENABLED
			? cop_hints_fetch_pvs(PL_curcop, "$[", 0)
			: 0;
	    SvGETMAGIC(newbase);
	    if (!SvOK(newbase)) SvSetMagicSV(SvRV(sv),&PL_sv_undef);
	    else 
		sv_setiv_mg(
		   SvRV(sv),
		   adjust_index(
		      SvIV_nomg(newbase), base&&SvOK(base)?SvIV(base):0
		   )
		);
	}
