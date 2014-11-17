#define PERL_IN_XS_APITEST
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "fakesdio.h"   /* Causes us to use PerlIO below */

typedef SV *SVREF;
typedef PTR_TBL_t *XS__APItest__PtrTable;

#define croak_fail() croak("fail at " __FILE__ " line %d", __LINE__)
#define croak_fail_ne(h, w) croak("fail %p!=%p at " __FILE__ " line %d", (h), (w), __LINE__)

/* for my_cxt tests */

#define MY_CXT_KEY "XS::APItest::_guts" XS_VERSION

typedef struct {
    int i;
    SV *sv;
    GV *cscgv;
    AV *cscav;
    AV *bhkav;
    bool bhk_record;
    peep_t orig_peep;
    peep_t orig_rpeep;
    int peep_recording;
    AV *peep_recorder;
    AV *rpeep_recorder;
    AV *xop_record;
} my_cxt_t;

START_MY_CXT

MGVTBL vtbl_foo, vtbl_bar;

/* indirect functions to test the [pa]MY_CXT macros */

int
my_cxt_getint_p(pMY_CXT)
{
    return MY_CXT.i;
}

void
my_cxt_setint_p(pMY_CXT_ int i)
{
    MY_CXT.i = i;
}

SV*
my_cxt_getsv_interp_context(void)
{
    dTHX;
    dMY_CXT_INTERP(my_perl);
    return MY_CXT.sv;
}

SV*
my_cxt_getsv_interp(void)
{
    dMY_CXT;
    return MY_CXT.sv;
}

void
my_cxt_setsv_p(SV* sv _pMY_CXT)
{
    MY_CXT.sv = sv;
}


/* from exception.c */
int apitest_exception(int);

/* from core_or_not.inc */
bool sv_setsv_cow_hashkey_core(void);
bool sv_setsv_cow_hashkey_notcore(void);

/* A routine to test hv_delayfree_ent
   (which itself is tested by testing on hv_free_ent  */

typedef void (freeent_function)(pTHX_ HV *, HE *);

void
test_freeent(freeent_function *f) {
    dTHX;
    dSP;
    HV *test_hash = newHV();
    HE *victim;
    SV *test_scalar;
    U32 results[4];
    int i;

#ifdef PURIFY
    victim = (HE*)safemalloc(sizeof(HE));
#else
    /* Storing then deleting something should ensure that a hash entry is
       available.  */
    (void) hv_store(test_hash, "", 0, &PL_sv_yes, 0);
    (void) hv_delete(test_hash, "", 0, 0);

    /* We need to "inline" new_he here as it's static, and the functions we
       test expect to be able to call del_HE on the HE  */
    if (!PL_body_roots[HE_SVSLOT])
	croak("PL_he_root is 0");
    victim = (HE*) PL_body_roots[HE_SVSLOT];
    PL_body_roots[HE_SVSLOT] = HeNEXT(victim);
#endif

    victim->hent_hek = Perl_share_hek(aTHX_ "", 0, 0);

    test_scalar = newSV(0);
    SvREFCNT_inc(test_scalar);
    HeVAL(victim) = test_scalar;

    /* Need this little game else we free the temps on the return stack.  */
    results[0] = SvREFCNT(test_scalar);
    SAVETMPS;
    results[1] = SvREFCNT(test_scalar);
    f(aTHX_ test_hash, victim);
    results[2] = SvREFCNT(test_scalar);
    FREETMPS;
    results[3] = SvREFCNT(test_scalar);

    i = 0;
    do {
	mPUSHu(results[i]);
    } while (++i < (int)(sizeof(results)/sizeof(results[0])));

    /* Goodbye to our extra reference.  */
    SvREFCNT_dec(test_scalar);
}


static I32
bitflip_key(pTHX_ IV action, SV *field) {
    MAGIC *mg = mg_find(field, PERL_MAGIC_uvar);
    SV *keysv;
    PERL_UNUSED_ARG(action);
    if (mg && (keysv = mg->mg_obj)) {
	STRLEN len;
	const char *p = SvPV(keysv, len);

	if (len) {
	    SV *newkey = newSV(len);
	    char *new_p = SvPVX(newkey);

	    if (SvUTF8(keysv)) {
		const char *const end = p + len;
		while (p < end) {
		    STRLEN len;
		    UV chr = utf8_to_uvchr_buf((U8 *)p, (U8 *) end, &len);
		    new_p = (char *)uvchr_to_utf8((U8 *)new_p, chr ^ 32);
		    p += len;
		}
		SvUTF8_on(newkey);
	    } else {
		while (len--)
		    *new_p++ = *p++ ^ 32;
	    }
	    *new_p = '\0';
	    SvCUR_set(newkey, SvCUR(keysv));
	    SvPOK_on(newkey);

	    mg->mg_obj = newkey;
	}
    }
    return 0;
}

static I32
rot13_key(pTHX_ IV action, SV *field) {
    MAGIC *mg = mg_find(field, PERL_MAGIC_uvar);
    SV *keysv;
    PERL_UNUSED_ARG(action);
    if (mg && (keysv = mg->mg_obj)) {
	STRLEN len;
	const char *p = SvPV(keysv, len);

	if (len) {
	    SV *newkey = newSV(len);
	    char *new_p = SvPVX(newkey);

	    /* There's a deliberate fencepost error here to loop len + 1 times
	       to copy the trailing \0  */
	    do {
		char new_c = *p++;
		/* Try doing this cleanly and clearly in EBCDIC another way: */
		switch (new_c) {
		case 'A': new_c = 'N'; break;
		case 'B': new_c = 'O'; break;
		case 'C': new_c = 'P'; break;
		case 'D': new_c = 'Q'; break;
		case 'E': new_c = 'R'; break;
		case 'F': new_c = 'S'; break;
		case 'G': new_c = 'T'; break;
		case 'H': new_c = 'U'; break;
		case 'I': new_c = 'V'; break;
		case 'J': new_c = 'W'; break;
		case 'K': new_c = 'X'; break;
		case 'L': new_c = 'Y'; break;
		case 'M': new_c = 'Z'; break;
		case 'N': new_c = 'A'; break;
		case 'O': new_c = 'B'; break;
		case 'P': new_c = 'C'; break;
		case 'Q': new_c = 'D'; break;
		case 'R': new_c = 'E'; break;
		case 'S': new_c = 'F'; break;
		case 'T': new_c = 'G'; break;
		case 'U': new_c = 'H'; break;
		case 'V': new_c = 'I'; break;
		case 'W': new_c = 'J'; break;
		case 'X': new_c = 'K'; break;
		case 'Y': new_c = 'L'; break;
		case 'Z': new_c = 'M'; break;
		case 'a': new_c = 'n'; break;
		case 'b': new_c = 'o'; break;
		case 'c': new_c = 'p'; break;
		case 'd': new_c = 'q'; break;
		case 'e': new_c = 'r'; break;
		case 'f': new_c = 's'; break;
		case 'g': new_c = 't'; break;
		case 'h': new_c = 'u'; break;
		case 'i': new_c = 'v'; break;
		case 'j': new_c = 'w'; break;
		case 'k': new_c = 'x'; break;
		case 'l': new_c = 'y'; break;
		case 'm': new_c = 'z'; break;
		case 'n': new_c = 'a'; break;
		case 'o': new_c = 'b'; break;
		case 'p': new_c = 'c'; break;
		case 'q': new_c = 'd'; break;
		case 'r': new_c = 'e'; break;
		case 's': new_c = 'f'; break;
		case 't': new_c = 'g'; break;
		case 'u': new_c = 'h'; break;
		case 'v': new_c = 'i'; break;
		case 'w': new_c = 'j'; break;
		case 'x': new_c = 'k'; break;
		case 'y': new_c = 'l'; break;
		case 'z': new_c = 'm'; break;
		}
		*new_p++ = new_c;
	    } while (len--);
	    SvCUR_set(newkey, SvCUR(keysv));
	    SvPOK_on(newkey);
	    if (SvUTF8(keysv))
		SvUTF8_on(newkey);

	    mg->mg_obj = newkey;
	}
    }
    return 0;
}

STATIC I32
rmagical_a_dummy(pTHX_ IV idx, SV *sv) {
    PERL_UNUSED_ARG(idx);
    PERL_UNUSED_ARG(sv);
    return 0;
}

STATIC MGVTBL rmagical_b = { 0 };

STATIC void
blockhook_csc_start(pTHX_ int full)
{
    dMY_CXT;
    AV *const cur = GvAV(MY_CXT.cscgv);

    PERL_UNUSED_ARG(full);
    SAVEGENERICSV(GvAV(MY_CXT.cscgv));

    if (cur) {
        I32 i;
        AV *const new_av = newAV();

        for (i = 0; i <= av_tindex(cur); i++) {
            av_store(new_av, i, newSVsv(*av_fetch(cur, i, 0)));
        }

        GvAV(MY_CXT.cscgv) = new_av;
    }
}

STATIC void
blockhook_csc_pre_end(pTHX_ OP **o)
{
    dMY_CXT;

    PERL_UNUSED_ARG(o);
    /* if we hit the end of a scope we missed the start of, we need to
     * unconditionally clear @CSC */
    if (GvAV(MY_CXT.cscgv) == MY_CXT.cscav && MY_CXT.cscav) {
        av_clear(MY_CXT.cscav);
    }

}

STATIC void
blockhook_test_start(pTHX_ int full)
{
    dMY_CXT;
    AV *av;
    
    if (MY_CXT.bhk_record) {
        av = newAV();
        av_push(av, newSVpvs("start"));
        av_push(av, newSViv(full));
        av_push(MY_CXT.bhkav, newRV_noinc(MUTABLE_SV(av)));
    }
}

STATIC void
blockhook_test_pre_end(pTHX_ OP **o)
{
    dMY_CXT;

    PERL_UNUSED_ARG(o);
    if (MY_CXT.bhk_record)
        av_push(MY_CXT.bhkav, newSVpvs("pre_end"));
}

STATIC void
blockhook_test_post_end(pTHX_ OP **o)
{
    dMY_CXT;

    PERL_UNUSED_ARG(o);
    if (MY_CXT.bhk_record)
        av_push(MY_CXT.bhkav, newSVpvs("post_end"));
}

STATIC void
blockhook_test_eval(pTHX_ OP *const o)
{
    dMY_CXT;
    AV *av;

    if (MY_CXT.bhk_record) {
        av = newAV();
        av_push(av, newSVpvs("eval"));
        av_push(av, newSVpv(OP_NAME(o), 0));
        av_push(MY_CXT.bhkav, newRV_noinc(MUTABLE_SV(av)));
    }
}

STATIC BHK bhk_csc, bhk_test;

STATIC void
my_peep (pTHX_ OP *o)
{
    dMY_CXT;

    if (!o)
	return;

    MY_CXT.orig_peep(aTHX_ o);

    if (!MY_CXT.peep_recording)
	return;

    for (; o; o = o->op_next) {
	if (o->op_type == OP_CONST && cSVOPx_sv(o) && SvPOK(cSVOPx_sv(o))) {
	    av_push(MY_CXT.peep_recorder, newSVsv(cSVOPx_sv(o)));
	}
    }
}

STATIC void
my_rpeep (pTHX_ OP *o)
{
    dMY_CXT;

    if (!o)
	return;

    MY_CXT.orig_rpeep(aTHX_ o);

    if (!MY_CXT.peep_recording)
	return;

    for (; o; o = o->op_next) {
	if (o->op_type == OP_CONST && cSVOPx_sv(o) && SvPOK(cSVOPx_sv(o))) {
	    av_push(MY_CXT.rpeep_recorder, newSVsv(cSVOPx_sv(o)));
	}
    }
}

STATIC OP *
THX_ck_entersub_args_lists(pTHX_ OP *entersubop, GV *namegv, SV *ckobj)
{
    PERL_UNUSED_ARG(namegv);
    PERL_UNUSED_ARG(ckobj);
    return ck_entersub_args_list(entersubop);
}

STATIC OP *
THX_ck_entersub_args_scalars(pTHX_ OP *entersubop, GV *namegv, SV *ckobj)
{
    OP *aop = cUNOPx(entersubop)->op_first;
    PERL_UNUSED_ARG(namegv);
    PERL_UNUSED_ARG(ckobj);
    if (!aop->op_sibling)
	aop = cUNOPx(aop)->op_first;
    for (aop = aop->op_sibling; aop->op_sibling; aop = aop->op_sibling) {
	op_contextualize(aop, G_SCALAR);
    }
    return entersubop;
}

STATIC OP *
THX_ck_entersub_multi_sum(pTHX_ OP *entersubop, GV *namegv, SV *ckobj)
{
    OP *sumop = NULL;
    OP *pushop = cUNOPx(entersubop)->op_first;
    PERL_UNUSED_ARG(namegv);
    PERL_UNUSED_ARG(ckobj);
    if (!pushop->op_sibling)
	pushop = cUNOPx(pushop)->op_first;
    while (1) {
	OP *aop = pushop->op_sibling;
	if (!aop->op_sibling)
	    break;
	pushop->op_sibling = aop->op_sibling;
	aop->op_sibling = NULL;
	op_contextualize(aop, G_SCALAR);
	if (sumop) {
	    sumop = newBINOP(OP_ADD, 0, sumop, aop);
	} else {
	    sumop = aop;
	}
    }
    if (!sumop)
	sumop = newSVOP(OP_CONST, 0, newSViv(0));
    op_free(entersubop);
    return sumop;
}

STATIC void test_op_list_describe_part(SV *res, OP *o);
STATIC void
test_op_list_describe_part(SV *res, OP *o)
{
    sv_catpv(res, PL_op_name[o->op_type]);
    switch (o->op_type) {
	case OP_CONST: {
	    sv_catpvf(res, "(%d)", (int)SvIV(cSVOPx(o)->op_sv));
	} break;
    }
    if (o->op_flags & OPf_KIDS) {
	OP *k;
	sv_catpvs(res, "[");
	for (k = cUNOPx(o)->op_first; k; k = k->op_sibling)
	    test_op_list_describe_part(res, k);
	sv_catpvs(res, "]");
    } else {
	sv_catpvs(res, ".");
    }
}

STATIC char *
test_op_list_describe(OP *o)
{
    SV *res = sv_2mortal(newSVpvs(""));
    if (o)
	test_op_list_describe_part(res, o);
    return SvPVX(res);
}

/* the real new*OP functions have a tendency to call fold_constants, and
 * other such unhelpful things, so we need our own versions for testing */

#define mkUNOP(t, f) THX_mkUNOP(aTHX_ (t), (f))
static OP *
THX_mkUNOP(pTHX_ U32 type, OP *first)
{
    UNOP *unop;
    NewOp(1103, unop, 1, UNOP);
    unop->op_type   = (OPCODE)type;
    unop->op_first  = first;
    unop->op_flags  = OPf_KIDS;
    return (OP *)unop;
}

#define mkBINOP(t, f, l) THX_mkBINOP(aTHX_ (t), (f), (l))
static OP *
THX_mkBINOP(pTHX_ U32 type, OP *first, OP *last)
{
    BINOP *binop;
    NewOp(1103, binop, 1, BINOP);
    binop->op_type      = (OPCODE)type;
    binop->op_first     = first;
    binop->op_flags     = OPf_KIDS;
    binop->op_last      = last;
    first->op_sibling   = last;
    return (OP *)binop;
}

#define mkLISTOP(t, f, s, l) THX_mkLISTOP(aTHX_ (t), (f), (s), (l))
static OP *
THX_mkLISTOP(pTHX_ U32 type, OP *first, OP *sib, OP *last)
{
    LISTOP *listop;
    NewOp(1103, listop, 1, LISTOP);
    listop->op_type     = (OPCODE)type;
    listop->op_flags    = OPf_KIDS;
    listop->op_first    = first;
    first->op_sibling   = sib;
    sib->op_sibling     = last;
    listop->op_last     = last;
    return (OP *)listop;
}

static char *
test_op_linklist_describe(OP *start)
{
    SV *rv = sv_2mortal(newSVpvs(""));
    OP *o;
    o = start = LINKLIST(start);
    do {
        sv_catpvs(rv, ".");
        sv_catpv(rv, OP_NAME(o));
        if (o->op_type == OP_CONST)
            sv_catsv(rv, cSVOPo->op_sv);
        o = o->op_next;
    } while (o && o != start);
    return SvPVX(rv);
}

/** establish_cleanup operator, ripped off from Scope::Cleanup **/

STATIC void
THX_run_cleanup(pTHX_ void *cleanup_code_ref)
{
    dSP;
    PUSHSTACK;
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    call_sv((SV*)cleanup_code_ref, G_VOID|G_DISCARD);
    FREETMPS;
    LEAVE;
    POPSTACK;
}

STATIC OP *
THX_pp_establish_cleanup(pTHX)
{
    dSP;
    SV *cleanup_code_ref;
    cleanup_code_ref = newSVsv(POPs);
    SAVEFREESV(cleanup_code_ref);
    SAVEDESTRUCTOR_X(THX_run_cleanup, cleanup_code_ref);
    if(GIMME_V != G_VOID) PUSHs(&PL_sv_undef);
    RETURN;
}

STATIC OP *
THX_ck_entersub_establish_cleanup(pTHX_ OP *entersubop, GV *namegv, SV *ckobj)
{
    OP *pushop, *argop, *estop;
    ck_entersub_args_proto(entersubop, namegv, ckobj);
    pushop = cUNOPx(entersubop)->op_first;
    if(!pushop->op_sibling) pushop = cUNOPx(pushop)->op_first;
    argop = pushop->op_sibling;
    pushop->op_sibling = argop->op_sibling;
    argop->op_sibling = NULL;
    op_free(entersubop);
    NewOpSz(0, estop, sizeof(UNOP));
    estop->op_type = OP_RAND;
    estop->op_ppaddr = THX_pp_establish_cleanup;
    cUNOPx(estop)->op_flags = OPf_KIDS;
    cUNOPx(estop)->op_first = argop;
    PL_hints |= HINT_BLOCK_SCOPE;
    return estop;
}

STATIC OP *
THX_ck_entersub_postinc(pTHX_ OP *entersubop, GV *namegv, SV *ckobj)
{
    OP *pushop, *argop;
    ck_entersub_args_proto(entersubop, namegv, ckobj);
    pushop = cUNOPx(entersubop)->op_first;
    if(!pushop->op_sibling) pushop = cUNOPx(pushop)->op_first;
    argop = pushop->op_sibling;
    pushop->op_sibling = argop->op_sibling;
    argop->op_sibling = NULL;
    op_free(entersubop);
    return newUNOP(OP_POSTINC, 0,
	op_lvalue(op_contextualize(argop, G_SCALAR), OP_POSTINC));
}

STATIC OP *
THX_ck_entersub_pad_scalar(pTHX_ OP *entersubop, GV *namegv, SV *ckobj)
{
    OP *pushop, *argop;
    PADOFFSET padoff = NOT_IN_PAD;
    SV *a0, *a1;
    ck_entersub_args_proto(entersubop, namegv, ckobj);
    pushop = cUNOPx(entersubop)->op_first;
    if(!pushop->op_sibling) pushop = cUNOPx(pushop)->op_first;
    argop = pushop->op_sibling;
    if(argop->op_type != OP_CONST || argop->op_sibling->op_type != OP_CONST)
	croak("bad argument expression type for pad_scalar()");
    a0 = cSVOPx_sv(argop);
    a1 = cSVOPx_sv(argop->op_sibling);
    switch(SvIV(a0)) {
	case 1: {
	    SV *namesv = sv_2mortal(newSVpvs("$"));
	    sv_catsv(namesv, a1);
	    padoff = pad_findmy_sv(namesv, 0);
	} break;
	case 2: {
	    char *namepv;
	    STRLEN namelen;
	    SV *namesv = sv_2mortal(newSVpvs("$"));
	    sv_catsv(namesv, a1);
	    namepv = SvPV(namesv, namelen);
	    padoff = pad_findmy_pvn(namepv, namelen, SvUTF8(namesv));
	} break;
	case 3: {
	    char *namepv;
	    SV *namesv = sv_2mortal(newSVpvs("$"));
	    sv_catsv(namesv, a1);
	    namepv = SvPV_nolen(namesv);
	    padoff = pad_findmy_pv(namepv, SvUTF8(namesv));
	} break;
	case 4: {
	    padoff = pad_findmy_pvs("$foo", 0);
	} break;
	default: croak("bad type value for pad_scalar()");
    }
    op_free(entersubop);
    if(padoff == NOT_IN_PAD) {
	return newSVOP(OP_CONST, 0, newSVpvs("NOT_IN_PAD"));
    } else if(PAD_COMPNAME_FLAGS_isOUR(padoff)) {
	return newSVOP(OP_CONST, 0, newSVpvs("NOT_MY"));
    } else {
	OP *padop = newOP(OP_PADSV, 0);
	padop->op_targ = padoff;
	return padop;
    }
}

/** RPN keyword parser **/

#define sv_is_glob(sv) (SvTYPE(sv) == SVt_PVGV)
#define sv_is_regexp(sv) (SvTYPE(sv) == SVt_REGEXP)
#define sv_is_string(sv) \
    (!sv_is_glob(sv) && !sv_is_regexp(sv) && \
     (SvFLAGS(sv) & (SVf_IOK|SVf_NOK|SVf_POK|SVp_IOK|SVp_NOK|SVp_POK)))

static SV *hintkey_rpn_sv, *hintkey_calcrpn_sv, *hintkey_stufftest_sv;
static SV *hintkey_swaptwostmts_sv, *hintkey_looprest_sv;
static SV *hintkey_scopelessblock_sv;
static SV *hintkey_stmtasexpr_sv, *hintkey_stmtsasexpr_sv;
static SV *hintkey_loopblock_sv, *hintkey_blockasexpr_sv;
static SV *hintkey_swaplabel_sv, *hintkey_labelconst_sv;
static SV *hintkey_arrayfullexpr_sv, *hintkey_arraylistexpr_sv;
static SV *hintkey_arraytermexpr_sv, *hintkey_arrayarithexpr_sv;
static SV *hintkey_arrayexprflags_sv;
static int (*next_keyword_plugin)(pTHX_ char *, STRLEN, OP **);

/* low-level parser helpers */

#define PL_bufptr (PL_parser->bufptr)
#define PL_bufend (PL_parser->bufend)

/* RPN parser */

#define parse_var() THX_parse_var(aTHX)
static OP *THX_parse_var(pTHX)
{
    char *s = PL_bufptr;
    char *start = s;
    PADOFFSET varpos;
    OP *padop;
    if(*s != '$') croak("RPN syntax error");
    while(1) {
	char c = *++s;
	if(!isALNUM(c)) break;
    }
    if(s-start < 2) croak("RPN syntax error");
    lex_read_to(s);
    varpos = pad_findmy_pvn(start, s-start, 0);
    if(varpos == NOT_IN_PAD || PAD_COMPNAME_FLAGS_isOUR(varpos))
	croak("RPN only supports \"my\" variables");
    padop = newOP(OP_PADSV, 0);
    padop->op_targ = varpos;
    return padop;
}

#define push_rpn_item(o) \
    (tmpop = (o), tmpop->op_sibling = stack, stack = tmpop)
#define pop_rpn_item() \
    (!stack ? (croak("RPN stack underflow"), (OP*)NULL) : \
     (tmpop = stack, stack = stack->op_sibling, \
      tmpop->op_sibling = NULL, tmpop))

#define parse_rpn_expr() THX_parse_rpn_expr(aTHX)
static OP *THX_parse_rpn_expr(pTHX)
{
    OP *stack = NULL, *tmpop;
    while(1) {
	I32 c;
	lex_read_space(0);
	c = lex_peek_unichar(0);
	switch(c) {
	    case /*(*/')': case /*{*/'}': {
		OP *result = pop_rpn_item();
		if(stack) croak("RPN expression must return a single value");
		return result;
	    } break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9': {
		UV val = 0;
		do {
		    lex_read_unichar(0);
		    val = 10*val + (c - '0');
		    c = lex_peek_unichar(0);
		} while(c >= '0' && c <= '9');
		push_rpn_item(newSVOP(OP_CONST, 0, newSVuv(val)));
	    } break;
	    case '$': {
		push_rpn_item(parse_var());
	    } break;
	    case '+': {
		OP *b = pop_rpn_item();
		OP *a = pop_rpn_item();
		lex_read_unichar(0);
		push_rpn_item(newBINOP(OP_I_ADD, 0, a, b));
	    } break;
	    case '-': {
		OP *b = pop_rpn_item();
		OP *a = pop_rpn_item();
		lex_read_unichar(0);
		push_rpn_item(newBINOP(OP_I_SUBTRACT, 0, a, b));
	    } break;
	    case '*': {
		OP *b = pop_rpn_item();
		OP *a = pop_rpn_item();
		lex_read_unichar(0);
		push_rpn_item(newBINOP(OP_I_MULTIPLY, 0, a, b));
	    } break;
	    case '/': {
		OP *b = pop_rpn_item();
		OP *a = pop_rpn_item();
		lex_read_unichar(0);
		push_rpn_item(newBINOP(OP_I_DIVIDE, 0, a, b));
	    } break;
	    case '%': {
		OP *b = pop_rpn_item();
		OP *a = pop_rpn_item();
		lex_read_unichar(0);
		push_rpn_item(newBINOP(OP_I_MODULO, 0, a, b));
	    } break;
	    default: {
		croak("RPN syntax error");
	    } break;
	}
    }
}

#define parse_keyword_rpn() THX_parse_keyword_rpn(aTHX)
static OP *THX_parse_keyword_rpn(pTHX)
{
    OP *op;
    lex_read_space(0);
    if(lex_peek_unichar(0) != '('/*)*/)
	croak("RPN expression must be parenthesised");
    lex_read_unichar(0);
    op = parse_rpn_expr();
    if(lex_peek_unichar(0) != /*(*/')')
	croak("RPN expression must be parenthesised");
    lex_read_unichar(0);
    return op;
}

#define parse_keyword_calcrpn() THX_parse_keyword_calcrpn(aTHX)
static OP *THX_parse_keyword_calcrpn(pTHX)
{
    OP *varop, *exprop;
    lex_read_space(0);
    varop = parse_var();
    lex_read_space(0);
    if(lex_peek_unichar(0) != '{'/*}*/)
	croak("RPN expression must be braced");
    lex_read_unichar(0);
    exprop = parse_rpn_expr();
    if(lex_peek_unichar(0) != /*{*/'}')
	croak("RPN expression must be braced");
    lex_read_unichar(0);
    return newASSIGNOP(OPf_STACKED, varop, 0, exprop);
}

#define parse_keyword_stufftest() THX_parse_keyword_stufftest(aTHX)
static OP *THX_parse_keyword_stufftest(pTHX)
{
    I32 c;
    bool do_stuff;
    lex_read_space(0);
    do_stuff = lex_peek_unichar(0) == '+';
    if(do_stuff) {
	lex_read_unichar(0);
	lex_read_space(0);
    }
    c = lex_peek_unichar(0);
    if(c == ';') {
	lex_read_unichar(0);
    } else if(c != /*{*/'}') {
	croak("syntax error");
    }
    if(do_stuff) lex_stuff_pvs(" ", 0);
    return newOP(OP_NULL, 0);
}

#define parse_keyword_swaptwostmts() THX_parse_keyword_swaptwostmts(aTHX)
static OP *THX_parse_keyword_swaptwostmts(pTHX)
{
    OP *a, *b;
    a = parse_fullstmt(0);
    b = parse_fullstmt(0);
    if(a && b)
	PL_hints |= HINT_BLOCK_SCOPE;
    return op_append_list(OP_LINESEQ, b, a);
}

#define parse_keyword_looprest() THX_parse_keyword_looprest(aTHX)
static OP *THX_parse_keyword_looprest(pTHX)
{
    return newWHILEOP(0, 1, NULL, newSVOP(OP_CONST, 0, &PL_sv_yes),
			parse_stmtseq(0), NULL, 1);
}

#define parse_keyword_scopelessblock() THX_parse_keyword_scopelessblock(aTHX)
static OP *THX_parse_keyword_scopelessblock(pTHX)
{
    I32 c;
    OP *body;
    lex_read_space(0);
    if(lex_peek_unichar(0) != '{'/*}*/) croak("syntax error");
    lex_read_unichar(0);
    body = parse_stmtseq(0);
    c = lex_peek_unichar(0);
    if(c != /*{*/'}' && c != /*[*/']' && c != /*(*/')') croak("syntax error");
    lex_read_unichar(0);
    return body;
}

#define parse_keyword_stmtasexpr() THX_parse_keyword_stmtasexpr(aTHX)
static OP *THX_parse_keyword_stmtasexpr(pTHX)
{
    OP *o = parse_barestmt(0);
    if (!o) o = newOP(OP_STUB, 0);
    if (PL_hints & HINT_BLOCK_SCOPE) o->op_flags |= OPf_PARENS;
    return op_scope(o);
}

#define parse_keyword_stmtsasexpr() THX_parse_keyword_stmtsasexpr(aTHX)
static OP *THX_parse_keyword_stmtsasexpr(pTHX)
{
    OP *o;
    lex_read_space(0);
    if(lex_peek_unichar(0) != '{'/*}*/) croak("syntax error");
    lex_read_unichar(0);
    o = parse_stmtseq(0);
    lex_read_space(0);
    if(lex_peek_unichar(0) != /*{*/'}') croak("syntax error");
    lex_read_unichar(0);
    if (!o) o = newOP(OP_STUB, 0);
    if (PL_hints & HINT_BLOCK_SCOPE) o->op_flags |= OPf_PARENS;
    return op_scope(o);
}

#define parse_keyword_loopblock() THX_parse_keyword_loopblock(aTHX)
static OP *THX_parse_keyword_loopblock(pTHX)
{
    return newWHILEOP(0, 1, NULL, newSVOP(OP_CONST, 0, &PL_sv_yes),
			parse_block(0), NULL, 1);
}

#define parse_keyword_blockasexpr() THX_parse_keyword_blockasexpr(aTHX)
static OP *THX_parse_keyword_blockasexpr(pTHX)
{
    OP *o = parse_block(0);
    if (!o) o = newOP(OP_STUB, 0);
    if (PL_hints & HINT_BLOCK_SCOPE) o->op_flags |= OPf_PARENS;
    return op_scope(o);
}

#define parse_keyword_swaplabel() THX_parse_keyword_swaplabel(aTHX)
static OP *THX_parse_keyword_swaplabel(pTHX)
{
    OP *sop = parse_barestmt(0);
    SV *label = parse_label(PARSE_OPTIONAL);
    if (label) sv_2mortal(label);
    return newSTATEOP(label ? SvUTF8(label) : 0,
                      label ? savepv(SvPVX(label)) : NULL,
                      sop);
}

#define parse_keyword_labelconst() THX_parse_keyword_labelconst(aTHX)
static OP *THX_parse_keyword_labelconst(pTHX)
{
    return newSVOP(OP_CONST, 0, parse_label(0));
}

#define parse_keyword_arrayfullexpr() THX_parse_keyword_arrayfullexpr(aTHX)
static OP *THX_parse_keyword_arrayfullexpr(pTHX)
{
    return newANONLIST(parse_fullexpr(0));
}

#define parse_keyword_arraylistexpr() THX_parse_keyword_arraylistexpr(aTHX)
static OP *THX_parse_keyword_arraylistexpr(pTHX)
{
    return newANONLIST(parse_listexpr(0));
}

#define parse_keyword_arraytermexpr() THX_parse_keyword_arraytermexpr(aTHX)
static OP *THX_parse_keyword_arraytermexpr(pTHX)
{
    return newANONLIST(parse_termexpr(0));
}

#define parse_keyword_arrayarithexpr() THX_parse_keyword_arrayarithexpr(aTHX)
static OP *THX_parse_keyword_arrayarithexpr(pTHX)
{
    return newANONLIST(parse_arithexpr(0));
}

#define parse_keyword_arrayexprflags() THX_parse_keyword_arrayexprflags(aTHX)
static OP *THX_parse_keyword_arrayexprflags(pTHX)
{
    U32 flags = 0;
    I32 c;
    OP *o;
    lex_read_space(0);
    c = lex_peek_unichar(0);
    if (c != '!' && c != '?') croak("syntax error");
    lex_read_unichar(0);
    if (c == '?') flags |= PARSE_OPTIONAL;
    o = parse_listexpr(flags);
    return o ? newANONLIST(o) : newANONHASH(newOP(OP_STUB, 0));
}

/* plugin glue */

#define keyword_active(hintkey_sv) THX_keyword_active(aTHX_ hintkey_sv)
static int THX_keyword_active(pTHX_ SV *hintkey_sv)
{
    HE *he;
    if(!GvHV(PL_hintgv)) return 0;
    he = hv_fetch_ent(GvHV(PL_hintgv), hintkey_sv, 0,
		SvSHARED_HASH(hintkey_sv));
    return he && SvTRUE(HeVAL(he));
}

static int my_keyword_plugin(pTHX_
    char *keyword_ptr, STRLEN keyword_len, OP **op_ptr)
{
    if(keyword_len == 3 && strnEQ(keyword_ptr, "rpn", 3) &&
		    keyword_active(hintkey_rpn_sv)) {
	*op_ptr = parse_keyword_rpn();
	return KEYWORD_PLUGIN_EXPR;
    } else if(keyword_len == 7 && strnEQ(keyword_ptr, "calcrpn", 7) &&
		    keyword_active(hintkey_calcrpn_sv)) {
	*op_ptr = parse_keyword_calcrpn();
	return KEYWORD_PLUGIN_STMT;
    } else if(keyword_len == 9 && strnEQ(keyword_ptr, "stufftest", 9) &&
		    keyword_active(hintkey_stufftest_sv)) {
	*op_ptr = parse_keyword_stufftest();
	return KEYWORD_PLUGIN_STMT;
    } else if(keyword_len == 12 &&
		    strnEQ(keyword_ptr, "swaptwostmts", 12) &&
		    keyword_active(hintkey_swaptwostmts_sv)) {
	*op_ptr = parse_keyword_swaptwostmts();
	return KEYWORD_PLUGIN_STMT;
    } else if(keyword_len == 8 && strnEQ(keyword_ptr, "looprest", 8) &&
		    keyword_active(hintkey_looprest_sv)) {
	*op_ptr = parse_keyword_looprest();
	return KEYWORD_PLUGIN_STMT;
    } else if(keyword_len == 14 && strnEQ(keyword_ptr, "scopelessblock", 14) &&
		    keyword_active(hintkey_scopelessblock_sv)) {
	*op_ptr = parse_keyword_scopelessblock();
	return KEYWORD_PLUGIN_STMT;
    } else if(keyword_len == 10 && strnEQ(keyword_ptr, "stmtasexpr", 10) &&
		    keyword_active(hintkey_stmtasexpr_sv)) {
	*op_ptr = parse_keyword_stmtasexpr();
	return KEYWORD_PLUGIN_EXPR;
    } else if(keyword_len == 11 && strnEQ(keyword_ptr, "stmtsasexpr", 11) &&
		    keyword_active(hintkey_stmtsasexpr_sv)) {
	*op_ptr = parse_keyword_stmtsasexpr();
	return KEYWORD_PLUGIN_EXPR;
    } else if(keyword_len == 9 && strnEQ(keyword_ptr, "loopblock", 9) &&
		    keyword_active(hintkey_loopblock_sv)) {
	*op_ptr = parse_keyword_loopblock();
	return KEYWORD_PLUGIN_STMT;
    } else if(keyword_len == 11 && strnEQ(keyword_ptr, "blockasexpr", 11) &&
		    keyword_active(hintkey_blockasexpr_sv)) {
	*op_ptr = parse_keyword_blockasexpr();
	return KEYWORD_PLUGIN_EXPR;
    } else if(keyword_len == 9 && strnEQ(keyword_ptr, "swaplabel", 9) &&
		    keyword_active(hintkey_swaplabel_sv)) {
	*op_ptr = parse_keyword_swaplabel();
	return KEYWORD_PLUGIN_STMT;
    } else if(keyword_len == 10 && strnEQ(keyword_ptr, "labelconst", 10) &&
		    keyword_active(hintkey_labelconst_sv)) {
	*op_ptr = parse_keyword_labelconst();
	return KEYWORD_PLUGIN_EXPR;
    } else if(keyword_len == 13 && strnEQ(keyword_ptr, "arrayfullexpr", 13) &&
		    keyword_active(hintkey_arrayfullexpr_sv)) {
	*op_ptr = parse_keyword_arrayfullexpr();
	return KEYWORD_PLUGIN_EXPR;
    } else if(keyword_len == 13 && strnEQ(keyword_ptr, "arraylistexpr", 13) &&
		    keyword_active(hintkey_arraylistexpr_sv)) {
	*op_ptr = parse_keyword_arraylistexpr();
	return KEYWORD_PLUGIN_EXPR;
    } else if(keyword_len == 13 && strnEQ(keyword_ptr, "arraytermexpr", 13) &&
		    keyword_active(hintkey_arraytermexpr_sv)) {
	*op_ptr = parse_keyword_arraytermexpr();
	return KEYWORD_PLUGIN_EXPR;
    } else if(keyword_len == 14 && strnEQ(keyword_ptr, "arrayarithexpr", 14) &&
		    keyword_active(hintkey_arrayarithexpr_sv)) {
	*op_ptr = parse_keyword_arrayarithexpr();
	return KEYWORD_PLUGIN_EXPR;
    } else if(keyword_len == 14 && strnEQ(keyword_ptr, "arrayexprflags", 14) &&
		    keyword_active(hintkey_arrayexprflags_sv)) {
	*op_ptr = parse_keyword_arrayexprflags();
	return KEYWORD_PLUGIN_EXPR;
    } else {
	return next_keyword_plugin(aTHX_ keyword_ptr, keyword_len, op_ptr);
    }
}

static XOP my_xop;

static OP *
pp_xop(pTHX)
{
    return PL_op->op_next;
}

static void
peep_xop(pTHX_ OP *o, OP *oldop)
{
    dMY_CXT;
    av_push(MY_CXT.xop_record, newSVpvf("peep:%"UVxf, PTR2UV(o)));
    av_push(MY_CXT.xop_record, newSVpvf("oldop:%"UVxf, PTR2UV(oldop)));
}

static I32
filter_call(pTHX_ int idx, SV *buf_sv, int maxlen)
{
    char *p;
    char *end;
    int n = FILTER_READ(idx + 1, buf_sv, maxlen);

    if (n<=0) return n;

    p = SvPV_force_nolen(buf_sv);
    end = p + SvCUR(buf_sv);
    while (p < end) {
	if (*p == 'o') *p = 'e';
	p++;
    }
    return SvCUR(buf_sv);
}

static AV *
myget_linear_isa(pTHX_ HV *stash, U32 level) {
    GV **gvp = (GV **)hv_fetchs(stash, "ISA", 0);
    PERL_UNUSED_ARG(level);
    return gvp && *gvp && GvAV(*gvp)
	 ? GvAV(*gvp)
	 : (AV *)sv_2mortal((SV *)newAV());
}


XS_EXTERNAL(XS_XS__APItest__XSUB_XS_VERSION_undef);
XS_EXTERNAL(XS_XS__APItest__XSUB_XS_VERSION_empty);
XS_EXTERNAL(XS_XS__APItest__XSUB_XS_APIVERSION_invalid);

static struct mro_alg mymro;

static Perl_check_t addissub_nxck_add;

static OP *
addissub_myck_add(pTHX_ OP *op)
{
    SV **flag_svp = hv_fetchs(GvHV(PL_hintgv), "XS::APItest/addissub", 0);
    OP *aop, *bop;
    U8 flags;
    if (!(flag_svp && SvTRUE(*flag_svp) && (op->op_flags & OPf_KIDS) &&
	    (aop = cBINOPx(op)->op_first) && (bop = aop->op_sibling) &&
	    !bop->op_sibling))
	return addissub_nxck_add(aTHX_ op);
    aop->op_sibling = NULL;
    cBINOPx(op)->op_first = NULL;
    op->op_flags &= ~OPf_KIDS;
    flags = op->op_flags;
    op_free(op);
    return newBINOP(OP_SUBTRACT, flags, aop, bop);
}

static Perl_check_t old_ck_rv2cv;

static OP *
my_ck_rv2cv(pTHX_ OP *o)
{
    SV *ref;
    SV **flag_svp = hv_fetchs(GvHV(PL_hintgv), "XS::APItest/addunder", 0);
    OP *aop;

    if (flag_svp && SvTRUE(*flag_svp) && (o->op_flags & OPf_KIDS)
     && (aop = cUNOPx(o)->op_first) && aop->op_type == OP_CONST
     && aop->op_private & (OPpCONST_ENTERED|OPpCONST_BARE)
     && (ref = cSVOPx(aop)->op_sv) && SvPOK(ref) && SvCUR(ref)
     && *(SvEND(ref)-1) == 'o')
    {
	SvGROW(ref, SvCUR(ref)+2);
	*SvEND(ref) = '_';
	SvCUR(ref)++;
	*SvEND(ref) = '\0';
    }
    return old_ck_rv2cv(aTHX_ o);
}

#include "const-c.inc"

MODULE = XS::APItest		PACKAGE = XS::APItest

INCLUDE: const-xs.inc

INCLUDE: numeric.xs

MODULE = XS::APItest::utf8	PACKAGE = XS::APItest::utf8

int
bytes_cmp_utf8(bytes, utf8)
	SV *bytes
	SV *utf8
    PREINIT:
	const U8 *b;
	STRLEN blen;
	const U8 *u;
	STRLEN ulen;
    CODE:
	b = (const U8 *)SvPVbyte(bytes, blen);
	u = (const U8 *)SvPVbyte(utf8, ulen);
	RETVAL = bytes_cmp_utf8(b, blen, u, ulen);
    OUTPUT:
	RETVAL

AV *
test_utf8n_to_uvchr(s, len, flags)

        SV *s
        SV *len
        SV *flags
    PREINIT:
        STRLEN retlen;
        UV ret;
        STRLEN slen;

    CODE:
        /* Call utf8n_to_uvchr() with the inputs.  It always asks for the
         * actual length to be returned
         *
         * Length to assume <s> is; not checked, so could have buffer overflow
         */
        RETVAL = newAV();
        sv_2mortal((SV*)RETVAL);

        ret
         = utf8n_to_uvchr((U8*) SvPV(s, slen), SvUV(len), &retlen, SvUV(flags));

        /* Returns the return value in [0]; <retlen> in [1] */
        av_push(RETVAL, newSVuv(ret));
        if (retlen == (STRLEN) -1) {
            av_push(RETVAL, newSViv(-1));
        }
        else {
            av_push(RETVAL, newSVuv(retlen));
        }

    OUTPUT:
        RETVAL

MODULE = XS::APItest:Overload	PACKAGE = XS::APItest::Overload

void
amagic_deref_call(sv, what)
	SV *sv
	int what
    PPCODE:
	/* The reference is owned by something else.  */
	PUSHs(amagic_deref_call(sv, what));

# I'd certainly like to discourage the use of this macro, given that we now
# have amagic_deref_call

void
tryAMAGICunDEREF_var(sv, what)
	SV *sv
	int what
    PPCODE:
	{
	    SV **sp = &sv;
	    switch(what) {
	    case to_av_amg:
		tryAMAGICunDEREF(to_av);
		break;
	    case to_cv_amg:
		tryAMAGICunDEREF(to_cv);
		break;
	    case to_gv_amg:
		tryAMAGICunDEREF(to_gv);
		break;
	    case to_hv_amg:
		tryAMAGICunDEREF(to_hv);
		break;
	    case to_sv_amg:
		tryAMAGICunDEREF(to_sv);
		break;
	    default:
		croak("Invalid value %d passed to tryAMAGICunDEREF_var", what);
	    }
	}
	/* The reference is owned by something else.  */
	PUSHs(sv);

MODULE = XS::APItest		PACKAGE = XS::APItest::XSUB

BOOT:
    newXS("XS::APItest::XSUB::XS_VERSION_undef", XS_XS__APItest__XSUB_XS_VERSION_undef, __FILE__);
    newXS("XS::APItest::XSUB::XS_VERSION_empty", XS_XS__APItest__XSUB_XS_VERSION_empty, __FILE__);
    newXS("XS::APItest::XSUB::XS_APIVERSION_invalid", XS_XS__APItest__XSUB_XS_APIVERSION_invalid, __FILE__);

void
XS_VERSION_defined(...)
    PPCODE:
        XS_VERSION_BOOTCHECK;
        XSRETURN_EMPTY;

void
XS_APIVERSION_valid(...)
    PPCODE:
        XS_APIVERSION_BOOTCHECK;
        XSRETURN_EMPTY;

MODULE = XS::APItest:Hash		PACKAGE = XS::APItest::Hash

void
rot13_hash(hash)
	HV *hash
	CODE:
	{
	    struct ufuncs uf;
	    uf.uf_val = rot13_key;
	    uf.uf_set = 0;
	    uf.uf_index = 0;

	    sv_magic((SV*)hash, NULL, PERL_MAGIC_uvar, (char*)&uf, sizeof(uf));
	}

void
bitflip_hash(hash)
	HV *hash
	CODE:
	{
	    struct ufuncs uf;
	    uf.uf_val = bitflip_key;
	    uf.uf_set = 0;
	    uf.uf_index = 0;

	    sv_magic((SV*)hash, NULL, PERL_MAGIC_uvar, (char*)&uf, sizeof(uf));
	}

#define UTF8KLEN(sv, len)   (SvUTF8(sv) ? -(I32)len : (I32)len)

bool
exists(hash, key_sv)
	PREINIT:
	STRLEN len;
	const char *key;
	INPUT:
	HV *hash
	SV *key_sv
	CODE:
	key = SvPV(key_sv, len);
	RETVAL = hv_exists(hash, key, UTF8KLEN(key_sv, len));
        OUTPUT:
        RETVAL

bool
exists_ent(hash, key_sv)
	PREINIT:
	INPUT:
	HV *hash
	SV *key_sv
	CODE:
	RETVAL = hv_exists_ent(hash, key_sv, 0);
        OUTPUT:
        RETVAL

SV *
delete(hash, key_sv, flags = 0)
	PREINIT:
	STRLEN len;
	const char *key;
	INPUT:
	HV *hash
	SV *key_sv
	I32 flags;
	CODE:
	key = SvPV(key_sv, len);
	/* It's already mortal, so need to increase reference count.  */
	RETVAL
	    = SvREFCNT_inc(hv_delete(hash, key, UTF8KLEN(key_sv, len), flags));
        OUTPUT:
        RETVAL

SV *
delete_ent(hash, key_sv, flags = 0)
	INPUT:
	HV *hash
	SV *key_sv
	I32 flags;
	CODE:
	/* It's already mortal, so need to increase reference count.  */
	RETVAL = SvREFCNT_inc(hv_delete_ent(hash, key_sv, flags, 0));
        OUTPUT:
        RETVAL

SV *
store_ent(hash, key, value)
	PREINIT:
	SV *copy;
	HE *result;
	INPUT:
	HV *hash
	SV *key
	SV *value
	CODE:
	copy = newSV(0);
	result = hv_store_ent(hash, key, copy, 0);
	SvSetMagicSV(copy, value);
	if (!result) {
	    SvREFCNT_dec(copy);
	    XSRETURN_EMPTY;
	}
	/* It's about to become mortal, so need to increase reference count.
	 */
	RETVAL = SvREFCNT_inc(HeVAL(result));
        OUTPUT:
        RETVAL

SV *
store(hash, key_sv, value)
	PREINIT:
	STRLEN len;
	const char *key;
	SV *copy;
	SV **result;
	INPUT:
	HV *hash
	SV *key_sv
	SV *value
	CODE:
	key = SvPV(key_sv, len);
	copy = newSV(0);
	result = hv_store(hash, key, UTF8KLEN(key_sv, len), copy, 0);
	SvSetMagicSV(copy, value);
	if (!result) {
	    SvREFCNT_dec(copy);
	    XSRETURN_EMPTY;
	}
	/* It's about to become mortal, so need to increase reference count.
	 */
	RETVAL = SvREFCNT_inc(*result);
        OUTPUT:
        RETVAL

SV *
fetch_ent(hash, key_sv)
	PREINIT:
	HE *result;
	INPUT:
	HV *hash
	SV *key_sv
	CODE:
	result = hv_fetch_ent(hash, key_sv, 0, 0);
	if (!result) {
	    XSRETURN_EMPTY;
	}
	/* Force mg_get  */
	RETVAL = newSVsv(HeVAL(result));
        OUTPUT:
        RETVAL

SV *
fetch(hash, key_sv)
	PREINIT:
	STRLEN len;
	const char *key;
	SV **result;
	INPUT:
	HV *hash
	SV *key_sv
	CODE:
	key = SvPV(key_sv, len);
	result = hv_fetch(hash, key, UTF8KLEN(key_sv, len), 0);
	if (!result) {
	    XSRETURN_EMPTY;
	}
	/* Force mg_get  */
	RETVAL = newSVsv(*result);
        OUTPUT:
        RETVAL

#if defined (hv_common)

SV *
common(params)
	INPUT:
	HV *params
	PREINIT:
	HE *result;
	HV *hv = NULL;
	SV *keysv = NULL;
	const char *key = NULL;
	STRLEN klen = 0;
	int flags = 0;
	int action = 0;
	SV *val = NULL;
	U32 hash = 0;
	SV **svp;
	CODE:
	if ((svp = hv_fetchs(params, "hv", 0))) {
	    SV *const rv = *svp;
	    if (!SvROK(rv))
		croak("common passed a non-reference for parameter hv");
	    hv = (HV *)SvRV(rv);
	}
	if ((svp = hv_fetchs(params, "keysv", 0)))
	    keysv = *svp;
	if ((svp = hv_fetchs(params, "keypv", 0))) {
	    key = SvPV_const(*svp, klen);
	    if (SvUTF8(*svp))
		flags = HVhek_UTF8;
	}
	if ((svp = hv_fetchs(params, "action", 0)))
	    action = SvIV(*svp);
	if ((svp = hv_fetchs(params, "val", 0)))
	    val = newSVsv(*svp);
	if ((svp = hv_fetchs(params, "hash", 0)))
	    hash = SvUV(*svp);

	if ((svp = hv_fetchs(params, "hash_pv", 0))) {
	    PERL_HASH(hash, key, klen);
	}
	if ((svp = hv_fetchs(params, "hash_sv", 0))) {
	    STRLEN len;
	    const char *const p = SvPV(keysv, len);
	    PERL_HASH(hash, p, len);
	}

	result = (HE *)hv_common(hv, keysv, key, klen, flags, action, val, hash);
	if (!result) {
	    XSRETURN_EMPTY;
	}
	/* Force mg_get  */
	RETVAL = newSVsv(HeVAL(result));
        OUTPUT:
        RETVAL

#endif

void
test_hv_free_ent()
	PPCODE:
	test_freeent(&Perl_hv_free_ent);
	XSRETURN(4);

void
test_hv_delayfree_ent()
	PPCODE:
	test_freeent(&Perl_hv_delayfree_ent);
	XSRETURN(4);

SV *
test_share_unshare_pvn(input)
	PREINIT:
	STRLEN len;
	U32 hash;
	char *pvx;
	char *p;
	INPUT:
	SV *input
	CODE:
	pvx = SvPV(input, len);
	PERL_HASH(hash, pvx, len);
	p = sharepvn(pvx, len, hash);
	RETVAL = newSVpvn(p, len);
	unsharepvn(p, len, hash);
	OUTPUT:
	RETVAL

#if PERL_VERSION >= 9

bool
refcounted_he_exists(key, level=0)
	SV *key
	IV level
	CODE:
	if (level) {
	    croak("level must be zero, not %"IVdf, level);
	}
	RETVAL = (cop_hints_fetch_sv(PL_curcop, key, 0, 0) != &PL_sv_placeholder);
	OUTPUT:
	RETVAL

SV *
refcounted_he_fetch(key, level=0)
	SV *key
	IV level
	CODE:
	if (level) {
	    croak("level must be zero, not %"IVdf, level);
	}
	RETVAL = cop_hints_fetch_sv(PL_curcop, key, 0, 0);
	SvREFCNT_inc(RETVAL);
	OUTPUT:
	RETVAL

#endif

void
test_force_keys(HV *hv)
    PREINIT:
        HE *he;
	STRLEN count = 0;
    PPCODE:
        hv_iterinit(hv);
        he = hv_iternext(hv);
        while (he) {
	    SV *sv = HeSVKEY_force(he);
	    ++count;
	    EXTEND(SP, count);
	    PUSHs(sv_mortalcopy(sv));
            he = hv_iternext(hv);
        }

=pod

sub TIEHASH  { bless {}, $_[0] }
sub STORE    { $_[0]->{$_[1]} = $_[2] }
sub FETCH    { $_[0]->{$_[1]} }
sub FIRSTKEY { my $a = scalar keys %{$_[0]}; each %{$_[0]} }
sub NEXTKEY  { each %{$_[0]} }
sub EXISTS   { exists $_[0]->{$_[1]} }
sub DELETE   { delete $_[0]->{$_[1]} }
sub CLEAR    { %{$_[0]} = () }

=cut

MODULE = XS::APItest:TempLv		PACKAGE = XS::APItest::TempLv

void
make_temp_mg_lv(sv)
SV* sv
    PREINIT:
	SV * const lv = newSV_type(SVt_PVLV);
	STRLEN len;
    PPCODE:
        SvPV(sv, len);

	sv_magic(lv, NULL, PERL_MAGIC_substr, NULL, 0);
	LvTYPE(lv) = 'x';
	LvTARG(lv) = SvREFCNT_inc_simple(sv);
	LvTARGOFF(lv) = len == 0 ? 0 : 1;
	LvTARGLEN(lv) = len < 2 ? 0 : len-2;

	EXTEND(SP, 1);
	ST(0) = sv_2mortal(lv);
	XSRETURN(1);


MODULE = XS::APItest::PtrTable	PACKAGE = XS::APItest::PtrTable PREFIX = ptr_table_

void
ptr_table_new(classname)
const char * classname
    PPCODE:
    PUSHs(sv_setref_pv(sv_newmortal(), classname, (void*)ptr_table_new()));

void
DESTROY(table)
XS::APItest::PtrTable table
    CODE:
    ptr_table_free(table);

void
ptr_table_store(table, from, to)
XS::APItest::PtrTable table
SVREF from
SVREF to
   CODE:
   ptr_table_store(table, from, to);

UV
ptr_table_fetch(table, from)
XS::APItest::PtrTable table
SVREF from
   CODE:
   RETVAL = PTR2UV(ptr_table_fetch(table, from));
   OUTPUT:
   RETVAL

void
ptr_table_split(table)
XS::APItest::PtrTable table

void
ptr_table_clear(table)
XS::APItest::PtrTable table

MODULE = XS::APItest::AutoLoader	PACKAGE = XS::APItest::AutoLoader

SV *
AUTOLOAD()
    CODE:
	RETVAL = newSVpvn_flags(SvPVX(cv), SvCUR(cv), SvUTF8(cv));
    OUTPUT:
	RETVAL

SV *
AUTOLOADp(...)
    PROTOTYPE: *$
    CODE:
        PERL_UNUSED_ARG(items);
	RETVAL = newSVpvn_flags(SvPVX(cv), SvCUR(cv), SvUTF8(cv));
    OUTPUT:
	RETVAL


MODULE = XS::APItest		PACKAGE = XS::APItest

PROTOTYPES: DISABLE

BOOT:
    mymro.resolve = myget_linear_isa;
    mymro.name    = "justisa";
    mymro.length  = 7;
    mymro.kflags  = 0;
    mymro.hash    = 0;
    Perl_mro_register(aTHX_ &mymro);

HV *
xop_custom_ops ()
    CODE:
        RETVAL = PL_custom_ops;
    OUTPUT:
        RETVAL

HV *
xop_custom_op_names ()
    CODE:
        PL_custom_op_names = newHV();
        RETVAL = PL_custom_op_names;
    OUTPUT:
        RETVAL

HV *
xop_custom_op_descs ()
    CODE:
        PL_custom_op_descs = newHV();
        RETVAL = PL_custom_op_descs;
    OUTPUT:
        RETVAL

void
xop_register ()
    CODE:
        XopENTRY_set(&my_xop, xop_name, "my_xop");
        XopENTRY_set(&my_xop, xop_desc, "XOP for testing");
        XopENTRY_set(&my_xop, xop_class, OA_UNOP);
        XopENTRY_set(&my_xop, xop_peep, peep_xop);
        Perl_custom_op_register(aTHX_ pp_xop, &my_xop);

void
xop_clear ()
    CODE:
        XopDISABLE(&my_xop, xop_name);
        XopDISABLE(&my_xop, xop_desc);
        XopDISABLE(&my_xop, xop_class);
        XopDISABLE(&my_xop, xop_peep);

IV
xop_my_xop ()
    CODE:
        RETVAL = PTR2IV(&my_xop);
    OUTPUT:
        RETVAL

IV
xop_ppaddr ()
    CODE:
        RETVAL = PTR2IV(pp_xop);
    OUTPUT:
        RETVAL

IV
xop_OA_UNOP ()
    CODE:
        RETVAL = OA_UNOP;
    OUTPUT:
        RETVAL

AV *
xop_build_optree ()
    CODE:
        dMY_CXT;
        UNOP *unop;
        OP *kid;

        MY_CXT.xop_record = newAV();

        kid = newSVOP(OP_CONST, 0, newSViv(42));
        
        NewOp(1102, unop, 1, UNOP);
        unop->op_type       = OP_CUSTOM;
        unop->op_ppaddr     = pp_xop;
        unop->op_flags      = OPf_KIDS;
        unop->op_private    = 0;
        unop->op_first      = kid;
        unop->op_next       = NULL;
        kid->op_next        = (OP*)unop;

        av_push(MY_CXT.xop_record, newSVpvf("unop:%"UVxf, PTR2UV(unop)));
        av_push(MY_CXT.xop_record, newSVpvf("kid:%"UVxf, PTR2UV(kid)));

        av_push(MY_CXT.xop_record, newSVpvf("NAME:%s", OP_NAME((OP*)unop)));
        av_push(MY_CXT.xop_record, newSVpvf("DESC:%s", OP_DESC((OP*)unop)));
        av_push(MY_CXT.xop_record, newSVpvf("CLASS:%d", (int)OP_CLASS((OP*)unop)));

        PL_rpeepp(aTHX_ kid);

        FreeOp(kid);
        FreeOp(unop);

        RETVAL = MY_CXT.xop_record;
        MY_CXT.xop_record = NULL;
    OUTPUT:
        RETVAL

IV
xop_from_custom_op ()
    CODE:
/* author note: this test doesn't imply Perl_custom_op_xop is or isn't public
   API or that Perl_custom_op_xop is known to be used outside the core */
        UNOP *unop;
        XOP *xop;

        NewOp(1102, unop, 1, UNOP);
        unop->op_type       = OP_CUSTOM;
        unop->op_ppaddr     = pp_xop;
        unop->op_flags      = OPf_KIDS;
        unop->op_private    = 0;
        unop->op_first      = NULL;
        unop->op_next       = NULL;

        xop = Perl_custom_op_xop(aTHX_ (OP *)unop);
        FreeOp(unop);
        RETVAL = PTR2IV(xop);
    OUTPUT:
        RETVAL

BOOT:
{
    MY_CXT_INIT;

    MY_CXT.i  = 99;
    MY_CXT.sv = newSVpv("initial",0);

    MY_CXT.bhkav = get_av("XS::APItest::bhkav", GV_ADDMULTI);
    MY_CXT.bhk_record = 0;

    BhkENTRY_set(&bhk_test, bhk_start, blockhook_test_start);
    BhkENTRY_set(&bhk_test, bhk_pre_end, blockhook_test_pre_end);
    BhkENTRY_set(&bhk_test, bhk_post_end, blockhook_test_post_end);
    BhkENTRY_set(&bhk_test, bhk_eval, blockhook_test_eval);
    Perl_blockhook_register(aTHX_ &bhk_test);

    MY_CXT.cscgv = gv_fetchpvs("XS::APItest::COMPILE_SCOPE_CONTAINER",
        GV_ADDMULTI, SVt_PVAV);
    MY_CXT.cscav = GvAV(MY_CXT.cscgv);

    BhkENTRY_set(&bhk_csc, bhk_start, blockhook_csc_start);
    BhkENTRY_set(&bhk_csc, bhk_pre_end, blockhook_csc_pre_end);
    Perl_blockhook_register(aTHX_ &bhk_csc);

    MY_CXT.peep_recorder = newAV();
    MY_CXT.rpeep_recorder = newAV();

    MY_CXT.orig_peep = PL_peepp;
    MY_CXT.orig_rpeep = PL_rpeepp;
    PL_peepp = my_peep;
    PL_rpeepp = my_rpeep;
}

void
CLONE(...)
    CODE:
    MY_CXT_CLONE;
    PERL_UNUSED_VAR(items);
    MY_CXT.sv = newSVpv("initial_clone",0);
    MY_CXT.cscgv = gv_fetchpvs("XS::APItest::COMPILE_SCOPE_CONTAINER",
        GV_ADDMULTI, SVt_PVAV);
    MY_CXT.cscav = NULL;
    MY_CXT.bhkav = get_av("XS::APItest::bhkav", GV_ADDMULTI);
    MY_CXT.bhk_record = 0;
    MY_CXT.peep_recorder = newAV();
    MY_CXT.rpeep_recorder = newAV();

void
print_double(val)
        double val
        CODE:
        printf("%5.3f\n",val);

int
have_long_double()
        CODE:
#ifdef HAS_LONG_DOUBLE
        RETVAL = 1;
#else
        RETVAL = 0;
#endif
        OUTPUT:
        RETVAL

void
print_long_double()
        CODE:
#ifdef HAS_LONG_DOUBLE
#   if defined(PERL_PRIfldbl) && (LONG_DOUBLESIZE > DOUBLESIZE)
        long double val = 7.0;
        printf("%5.3" PERL_PRIfldbl "\n",val);
#   else
        double val = 7.0;
        printf("%5.3f\n",val);
#   endif
#endif

void
print_int(val)
        int val
        CODE:
        printf("%d\n",val);

void
print_long(val)
        long val
        CODE:
        printf("%ld\n",val);

void
print_float(val)
        float val
        CODE:
        printf("%5.3f\n",val);
	
void
print_flush()
    	CODE:
	fflush(stdout);

void
mpushp()
	PPCODE:
	EXTEND(SP, 3);
	mPUSHp("one", 3);
	mPUSHp("two", 3);
	mPUSHp("three", 5);
	XSRETURN(3);

void
mpushn()
	PPCODE:
	EXTEND(SP, 3);
	mPUSHn(0.5);
	mPUSHn(-0.25);
	mPUSHn(0.125);
	XSRETURN(3);

void
mpushi()
	PPCODE:
	EXTEND(SP, 3);
	mPUSHi(-1);
	mPUSHi(2);
	mPUSHi(-3);
	XSRETURN(3);

void
mpushu()
	PPCODE:
	EXTEND(SP, 3);
	mPUSHu(1);
	mPUSHu(2);
	mPUSHu(3);
	XSRETURN(3);

void
mxpushp()
	PPCODE:
	mXPUSHp("one", 3);
	mXPUSHp("two", 3);
	mXPUSHp("three", 5);
	XSRETURN(3);

void
mxpushn()
	PPCODE:
	mXPUSHn(0.5);
	mXPUSHn(-0.25);
	mXPUSHn(0.125);
	XSRETURN(3);

void
mxpushi()
	PPCODE:
	mXPUSHi(-1);
	mXPUSHi(2);
	mXPUSHi(-3);
	XSRETURN(3);

void
mxpushu()
	PPCODE:
	mXPUSHu(1);
	mXPUSHu(2);
	mXPUSHu(3);
	XSRETURN(3);

void
call_sv_C()
PREINIT:
    CV * i_sub;
    GV * i_gv;
    I32 retcnt;
    SV * errsv;
    char * errstr;
    SV * miscsv = sv_newmortal();
    HV * hv = (HV*)sv_2mortal((SV*)newHV());
CODE:
    i_sub = get_cv("i", 0);
    PUSHMARK(SP);
    /* PUTBACK not needed since this sub was called with 0 args, and is calling
      0 args, so global SP doesn't need to be moved before a call_* */
    retcnt = call_sv((SV*)i_sub, 0); /* try a CV* */
    SPAGAIN;
    SP -= retcnt; /* dont care about return count, wipe everything off */
    sv_setpvs(miscsv, "i");
    PUSHMARK(SP);
    retcnt = call_sv(miscsv, 0); /* try a PV */
    SPAGAIN;
    SP -= retcnt;
    /* no add and SVt_NULL are intentional, sub i should be defined already */
    i_gv = gv_fetchpvn_flags("i", sizeof("i")-1, 0, SVt_NULL);
    PUSHMARK(SP);
    retcnt = call_sv((SV*)i_gv, 0); /* try a GV* */
    SPAGAIN;
    SP -= retcnt;
    /* the tests below are not declaring this being public API behavior,
       only current internal behavior, these tests can be changed in the
       future if necessery */
    PUSHMARK(SP);
    retcnt = call_sv(&PL_sv_yes, 0); /* does nothing */
    SPAGAIN;
    SP -= retcnt;
    PUSHMARK(SP);
    retcnt = call_sv(&PL_sv_no, G_EVAL);
    SPAGAIN;
    SP -= retcnt;
    errsv = ERRSV;
    errstr = SvPV_nolen(errsv);
    if(strnEQ(errstr, "Undefined subroutine &main:: called at",
              sizeof("Undefined subroutine &main:: called at") - 1)) {
        PUSHMARK(SP);
        retcnt = call_sv((SV*)i_sub, 0); /* call again to increase counter */
        SPAGAIN;
        SP -= retcnt;
    }
    PUSHMARK(SP);
    retcnt = call_sv(&PL_sv_undef,  G_EVAL);
    SPAGAIN;
    SP -= retcnt;
    errsv = ERRSV;
    errstr = SvPV_nolen(errsv);
    if(strnEQ(errstr, "Can't use an undefined value as a subroutine reference at",
              sizeof("Can't use an undefined value as a subroutine reference at") - 1)) {
        PUSHMARK(SP);
        retcnt = call_sv((SV*)i_sub, 0); /* call again to increase counter */
        SPAGAIN;
        SP -= retcnt;
    }
    PUSHMARK(SP);
    retcnt = call_sv((SV*)hv,  G_EVAL);
    SPAGAIN;
    SP -= retcnt;
    errsv = ERRSV;
    errstr = SvPV_nolen(errsv);
    if(strnEQ(errstr, "Not a CODE reference at",
              sizeof("Not a CODE reference at") - 1)) {
        PUSHMARK(SP);
        retcnt = call_sv((SV*)i_sub, 0); /* call again to increase counter */
        SPAGAIN;
        SP -= retcnt;
    }

void
call_sv(sv, flags, ...)
    SV* sv
    I32 flags
    PREINIT:
	I32 i;
    PPCODE:
	for (i=0; i<items-2; i++)
	    ST(i) = ST(i+2); /* pop first two args */
	PUSHMARK(SP);
	SP += items - 2;
	PUTBACK;
	i = call_sv(sv, flags);
	SPAGAIN;
	EXTEND(SP, 1);
	PUSHs(sv_2mortal(newSViv(i)));

void
call_pv(subname, flags, ...)
    char* subname
    I32 flags
    PREINIT:
	I32 i;
    PPCODE:
	for (i=0; i<items-2; i++)
	    ST(i) = ST(i+2); /* pop first two args */
	PUSHMARK(SP);
	SP += items - 2;
	PUTBACK;
	i = call_pv(subname, flags);
	SPAGAIN;
	EXTEND(SP, 1);
	PUSHs(sv_2mortal(newSViv(i)));

void
call_method(methname, flags, ...)
    char* methname
    I32 flags
    PREINIT:
	I32 i;
    PPCODE:
	for (i=0; i<items-2; i++)
	    ST(i) = ST(i+2); /* pop first two args */
	PUSHMARK(SP);
	SP += items - 2;
	PUTBACK;
	i = call_method(methname, flags);
	SPAGAIN;
	EXTEND(SP, 1);
	PUSHs(sv_2mortal(newSViv(i)));

void
newCONSTSUB(stash, name, flags, sv)
    HV* stash
    SV* name
    I32 flags
    SV* sv
    ALIAS:
	newCONSTSUB_flags = 1
    PREINIT:
	CV* mycv = NULL;
	STRLEN len;
	const char *pv = SvPV(name, len);
    PPCODE:
        switch (ix) {
           case 0:
               mycv = newCONSTSUB(stash, pv, SvOK(sv) ? SvREFCNT_inc(sv) : NULL);
               break;
           case 1:
               mycv = newCONSTSUB_flags(
                 stash, pv, len, flags | SvUTF8(name), SvOK(sv) ? SvREFCNT_inc(sv) : NULL
               );
               break;
        }
        EXTEND(SP, 2);
        PUSHs( CvCONST(mycv) ? &PL_sv_yes : &PL_sv_no );
        PUSHs((SV*)CvGV(mycv));

void
gv_init_type(namesv, multi, flags, type)
    SV* namesv
    int multi
    I32 flags
    int type
    PREINIT:
        STRLEN len;
        const char * const name = SvPV_const(namesv, len);
        GV *gv = *(GV**)hv_fetch(PL_defstash, name, len, TRUE);
    PPCODE:
        if (SvTYPE(gv) == SVt_PVGV)
            Perl_croak(aTHX_ "GV is already a PVGV");
        if (multi) flags |= GV_ADDMULTI;
        switch (type) {
           case 0:
	       gv_init(gv, PL_defstash, name, len, multi);
               break;
           case 1:
               gv_init_sv(gv, PL_defstash, namesv, flags);
               break;
           case 2:
               gv_init_pv(gv, PL_defstash, name, flags | SvUTF8(namesv));
               break;
           case 3:
               gv_init_pvn(gv, PL_defstash, name, len, flags | SvUTF8(namesv));
               break;
        }
	XPUSHs( gv ? (SV*)gv : &PL_sv_undef);

void
gv_fetchmeth_type(stash, methname, type, level, flags)
    HV* stash
    SV* methname
    int type
    I32 level
    I32 flags
    PREINIT:
        STRLEN len;
        const char * const name = SvPV_const(methname, len);
	GV* gv = NULL;
    PPCODE:
        switch (type) {
           case 0:
	       gv = gv_fetchmeth(stash, name, len, level);
               break;
           case 1:
               gv = gv_fetchmeth_sv(stash, methname, level, flags);
               break;
           case 2:
               gv = gv_fetchmeth_pv(stash, name, level, flags | SvUTF8(methname));
               break;
           case 3:
               gv = gv_fetchmeth_pvn(stash, name, len, level, flags | SvUTF8(methname));
               break;
        }
	XPUSHs( gv ? MUTABLE_SV(gv) : &PL_sv_undef );

void
gv_fetchmeth_autoload_type(stash, methname, type, level, flags)
    HV* stash
    SV* methname
    int type
    I32 level
    I32 flags
    PREINIT:
        STRLEN len;
        const char * const name = SvPV_const(methname, len);
	GV* gv = NULL;
    PPCODE:
        switch (type) {
           case 0:
	       gv = gv_fetchmeth_autoload(stash, name, len, level);
               break;
           case 1:
               gv = gv_fetchmeth_sv_autoload(stash, methname, level, flags);
               break;
           case 2:
               gv = gv_fetchmeth_pv_autoload(stash, name, level, flags | SvUTF8(methname));
               break;
           case 3:
               gv = gv_fetchmeth_pvn_autoload(stash, name, len, level, flags | SvUTF8(methname));
               break;
        }
	XPUSHs( gv ? MUTABLE_SV(gv) : &PL_sv_undef );

void
gv_fetchmethod_flags_type(stash, methname, type, flags)
    HV* stash
    SV* methname
    int type
    I32 flags
    PREINIT:
	GV* gv = NULL;
    PPCODE:
        switch (type) {
           case 0:
	       gv = gv_fetchmethod_flags(stash, SvPVX_const(methname), flags);
               break;
           case 1:
               gv = gv_fetchmethod_sv_flags(stash, methname, flags);
               break;
           case 2:
               gv = gv_fetchmethod_pv_flags(stash, SvPV_nolen(methname), flags | SvUTF8(methname));
               break;
           case 3: {
               STRLEN len;
               const char * const name = SvPV_const(methname, len);
               gv = gv_fetchmethod_pvn_flags(stash, name, len, flags | SvUTF8(methname));
               break;
            }
        }
	XPUSHs( gv ? (SV*)gv : &PL_sv_undef);

void
gv_autoload_type(stash, methname, type, method)
    HV* stash
    SV* methname
    int type
    I32 method
    PREINIT:
        STRLEN len;
        const char * const name = SvPV_const(methname, len);
	GV* gv = NULL;
	I32 flags = method ? GV_AUTOLOAD_ISMETHOD : 0;
    PPCODE:
        switch (type) {
           case 0:
	       gv = gv_autoload4(stash, name, len, method);
               break;
           case 1:
               gv = gv_autoload_sv(stash, methname, flags);
               break;
           case 2:
               gv = gv_autoload_pv(stash, name, flags | SvUTF8(methname));
               break;
           case 3:
               gv = gv_autoload_pvn(stash, name, len, flags | SvUTF8(methname));
               break;
        }
	XPUSHs( gv ? (SV*)gv : &PL_sv_undef);

void
whichsig_type(namesv, type)
    SV* namesv
    int type
    PREINIT:
        STRLEN len;
        const char * const name = SvPV_const(namesv, len);
        I32 i = 0;
    PPCODE:
        switch (type) {
           case 0:
              i = whichsig(name);
               break;
           case 1:
               i = whichsig_sv(namesv);
               break;
           case 2:
               i = whichsig_pv(name);
               break;
           case 3:
               i = whichsig_pvn(name, len);
               break;
        }
        XPUSHs(sv_2mortal(newSViv(i)));

void
eval_sv(sv, flags)
    SV* sv
    I32 flags
    PREINIT:
    	I32 i;
    PPCODE:
	PUTBACK;
	i = eval_sv(sv, flags);
	SPAGAIN;
	EXTEND(SP, 1);
	PUSHs(sv_2mortal(newSViv(i)));

void
eval_pv(p, croak_on_error)
    const char* p
    I32 croak_on_error
    PPCODE:
	PUTBACK;
	EXTEND(SP, 1);
	PUSHs(eval_pv(p, croak_on_error));

void
require_pv(pv)
    const char* pv
    PPCODE:
	PUTBACK;
	require_pv(pv);

int
apitest_exception(throw_e)
    int throw_e
    OUTPUT:
        RETVAL

void
mycroak(sv)
    SV* sv
    CODE:
    if (SvOK(sv)) {
        Perl_croak(aTHX_ "%s", SvPV_nolen(sv));
    }
    else {
	Perl_croak(aTHX_ NULL);
    }

SV*
strtab()
   CODE:
   RETVAL = newRV_inc((SV*)PL_strtab);
   OUTPUT:
   RETVAL

int
my_cxt_getint()
    CODE:
	dMY_CXT;
	RETVAL = my_cxt_getint_p(aMY_CXT);
    OUTPUT:
        RETVAL

void
my_cxt_setint(i)
    int i;
    CODE:
	dMY_CXT;
	my_cxt_setint_p(aMY_CXT_ i);

void
my_cxt_getsv(how)
    bool how;
    PPCODE:
	EXTEND(SP, 1);
	ST(0) = how ? my_cxt_getsv_interp_context() : my_cxt_getsv_interp();
	XSRETURN(1);

void
my_cxt_setsv(sv)
    SV *sv;
    CODE:
	dMY_CXT;
	SvREFCNT_dec(MY_CXT.sv);
	my_cxt_setsv_p(sv _aMY_CXT);
	SvREFCNT_inc(sv);

bool
sv_setsv_cow_hashkey_core()

bool
sv_setsv_cow_hashkey_notcore()

void
sv_set_deref(SV *sv, SV *sv2, int which)
    CODE:
    {
	STRLEN len;
	const char *pv = SvPV(sv2,len);
	if (!SvROK(sv)) croak("Not a ref");
	sv = SvRV(sv);
	switch (which) {
	    case 0: sv_setsv(sv,sv2); break;
	    case 1: sv_setpv(sv,pv); break;
	    case 2: sv_setpvn(sv,pv,len); break;
	}
    }

void
rmagical_cast(sv, type)
    SV *sv;
    SV *type;
    PREINIT:
	struct ufuncs uf;
    PPCODE:
	if (!SvOK(sv) || !SvROK(sv) || !SvOK(type)) { XSRETURN_UNDEF; }
	sv = SvRV(sv);
	if (SvTYPE(sv) != SVt_PVHV) { XSRETURN_UNDEF; }
	uf.uf_val = rmagical_a_dummy;
	uf.uf_set = NULL;
	uf.uf_index = 0;
	if (SvTRUE(type)) { /* b */
	    sv_magicext(sv, NULL, PERL_MAGIC_ext, &rmagical_b, NULL, 0);
	} else { /* a */
	    sv_magic(sv, NULL, PERL_MAGIC_uvar, (char *) &uf, sizeof(uf));
	}
	XSRETURN_YES;

void
rmagical_flags(sv)
    SV *sv;
    PPCODE:
	if (!SvOK(sv) || !SvROK(sv)) { XSRETURN_UNDEF; }
	sv = SvRV(sv);
        EXTEND(SP, 3); 
	mXPUSHu(SvFLAGS(sv) & SVs_GMG);
	mXPUSHu(SvFLAGS(sv) & SVs_SMG);
	mXPUSHu(SvFLAGS(sv) & SVs_RMG);
        XSRETURN(3);

void
my_caller(level)
        I32 level
    PREINIT:
        const PERL_CONTEXT *cx, *dbcx;
        const char *pv;
        const GV *gv;
        HV *hv;
    PPCODE:
        cx = caller_cx(level, &dbcx);
        EXTEND(SP, 8);

        pv = CopSTASHPV(cx->blk_oldcop);
        ST(0) = pv ? sv_2mortal(newSVpv(pv, 0)) : &PL_sv_undef;
        gv = CvGV(cx->blk_sub.cv);
        ST(1) = isGV(gv) ? sv_2mortal(newSVpv(GvNAME(gv), 0)) : &PL_sv_undef;

        pv = CopSTASHPV(dbcx->blk_oldcop);
        ST(2) = pv ? sv_2mortal(newSVpv(pv, 0)) : &PL_sv_undef;
        gv = CvGV(dbcx->blk_sub.cv);
        ST(3) = isGV(gv) ? sv_2mortal(newSVpv(GvNAME(gv), 0)) : &PL_sv_undef;

        ST(4) = cop_hints_fetch_pvs(cx->blk_oldcop, "foo", 0);
        ST(5) = cop_hints_fetch_pvn(cx->blk_oldcop, "foo", 3, 0, 0);
        ST(6) = cop_hints_fetch_sv(cx->blk_oldcop, 
                sv_2mortal(newSVpvn("foo", 3)), 0, 0);

        hv = cop_hints_2hv(cx->blk_oldcop, 0);
        ST(7) = hv ? sv_2mortal(newRV_noinc((SV *)hv)) : &PL_sv_undef;

        XSRETURN(8);

void
DPeek (sv)
    SV   *sv

  PPCODE:
    ST (0) = newSVpv (Perl_sv_peek (aTHX_ sv), 0);
    XSRETURN (1);

void
BEGIN()
    CODE:
	sv_inc(get_sv("XS::APItest::BEGIN_called", GV_ADD|GV_ADDMULTI));

void
CHECK()
    CODE:
	sv_inc(get_sv("XS::APItest::CHECK_called", GV_ADD|GV_ADDMULTI));

void
UNITCHECK()
    CODE:
	sv_inc(get_sv("XS::APItest::UNITCHECK_called", GV_ADD|GV_ADDMULTI));

void
INIT()
    CODE:
	sv_inc(get_sv("XS::APItest::INIT_called", GV_ADD|GV_ADDMULTI));

void
END()
    CODE:
	sv_inc(get_sv("XS::APItest::END_called", GV_ADD|GV_ADDMULTI));

void
utf16_to_utf8 (sv, ...)
    SV* sv
	ALIAS:
	    utf16_to_utf8_reversed = 1
    PREINIT:
        STRLEN len;
	U8 *source;
	SV *dest;
	I32 got; /* Gah, badly thought out APIs */
    CODE:
	if (ix) (void)SvPV_force_nolen(sv);
	source = (U8 *)SvPVbyte(sv, len);
	/* Optionally only convert part of the buffer.  */ 	
	if (items > 1) {
	    len = SvUV(ST(1));
 	}
	/* Mortalise this right now, as we'll be testing croak()s  */
	dest = sv_2mortal(newSV(len * 3 / 2 + 1));
	if (ix) {
	    utf16_to_utf8_reversed(source, (U8 *)SvPVX(dest), len, &got);
	} else {
	    utf16_to_utf8(source, (U8 *)SvPVX(dest), len, &got);
	}
	SvCUR_set(dest, got);
	SvPVX(dest)[got] = '\0';
	SvPOK_on(dest);
 	ST(0) = dest;
	XSRETURN(1);

void
my_exit(int exitcode)
        PPCODE:
        my_exit(exitcode);

U8
first_byte(sv)
	SV *sv
   CODE:
    char *s;
    STRLEN len;
	s = SvPVbyte(sv, len);
	RETVAL = s[0];
   OUTPUT:
    RETVAL

I32
sv_count()
        CODE:
	    RETVAL = PL_sv_count;
	OUTPUT:
	    RETVAL

void
bhk_record(bool on)
    CODE:
        dMY_CXT;
        MY_CXT.bhk_record = on;
        if (on)
            av_clear(MY_CXT.bhkav);

void
test_magic_chain()
    PREINIT:
	SV *sv;
	MAGIC *callmg, *uvarmg;
    CODE:
	sv = sv_2mortal(newSV(0));
	if (SvTYPE(sv) >= SVt_PVMG) croak_fail();
	if (SvMAGICAL(sv)) croak_fail();
	sv_magic(sv, &PL_sv_yes, PERL_MAGIC_checkcall, (char*)&callmg, 0);
	if (SvTYPE(sv) < SVt_PVMG) croak_fail();
	if (!SvMAGICAL(sv)) croak_fail();
	if (mg_find(sv, PERL_MAGIC_uvar)) croak_fail();
	callmg = mg_find(sv, PERL_MAGIC_checkcall);
	if (!callmg) croak_fail();
	if (callmg->mg_obj != &PL_sv_yes || callmg->mg_ptr != (char*)&callmg)
	    croak_fail();
	sv_magic(sv, &PL_sv_no, PERL_MAGIC_uvar, (char*)&uvarmg, 0);
	if (SvTYPE(sv) < SVt_PVMG) croak_fail();
	if (!SvMAGICAL(sv)) croak_fail();
	if (mg_find(sv, PERL_MAGIC_checkcall) != callmg) croak_fail();
	uvarmg = mg_find(sv, PERL_MAGIC_uvar);
	if (!uvarmg) croak_fail();
	if (callmg->mg_obj != &PL_sv_yes || callmg->mg_ptr != (char*)&callmg)
	    croak_fail();
	if (uvarmg->mg_obj != &PL_sv_no || uvarmg->mg_ptr != (char*)&uvarmg)
	    croak_fail();
	mg_free_type(sv, PERL_MAGIC_vec);
	if (SvTYPE(sv) < SVt_PVMG) croak_fail();
	if (!SvMAGICAL(sv)) croak_fail();
	if (mg_find(sv, PERL_MAGIC_checkcall) != callmg) croak_fail();
	if (mg_find(sv, PERL_MAGIC_uvar) != uvarmg) croak_fail();
	if (callmg->mg_obj != &PL_sv_yes || callmg->mg_ptr != (char*)&callmg)
	    croak_fail();
	if (uvarmg->mg_obj != &PL_sv_no || uvarmg->mg_ptr != (char*)&uvarmg)
	    croak_fail();
	mg_free_type(sv, PERL_MAGIC_uvar);
	if (SvTYPE(sv) < SVt_PVMG) croak_fail();
	if (!SvMAGICAL(sv)) croak_fail();
	if (mg_find(sv, PERL_MAGIC_checkcall) != callmg) croak_fail();
	if (mg_find(sv, PERL_MAGIC_uvar)) croak_fail();
	if (callmg->mg_obj != &PL_sv_yes || callmg->mg_ptr != (char*)&callmg)
	    croak_fail();
	sv_magic(sv, &PL_sv_no, PERL_MAGIC_uvar, (char*)&uvarmg, 0);
	if (SvTYPE(sv) < SVt_PVMG) croak_fail();
	if (!SvMAGICAL(sv)) croak_fail();
	if (mg_find(sv, PERL_MAGIC_checkcall) != callmg) croak_fail();
	uvarmg = mg_find(sv, PERL_MAGIC_uvar);
	if (!uvarmg) croak_fail();
	if (callmg->mg_obj != &PL_sv_yes || callmg->mg_ptr != (char*)&callmg)
	    croak_fail();
	if (uvarmg->mg_obj != &PL_sv_no || uvarmg->mg_ptr != (char*)&uvarmg)
	    croak_fail();
	mg_free_type(sv, PERL_MAGIC_checkcall);
	if (SvTYPE(sv) < SVt_PVMG) croak_fail();
	if (!SvMAGICAL(sv)) croak_fail();
	if (mg_find(sv, PERL_MAGIC_uvar) != uvarmg) croak_fail();
	if (mg_find(sv, PERL_MAGIC_checkcall)) croak_fail();
	if (uvarmg->mg_obj != &PL_sv_no || uvarmg->mg_ptr != (char*)&uvarmg)
	    croak_fail();
	mg_free_type(sv, PERL_MAGIC_uvar);
	if (SvMAGICAL(sv)) croak_fail();
	if (mg_find(sv, PERL_MAGIC_checkcall)) croak_fail();
	if (mg_find(sv, PERL_MAGIC_uvar)) croak_fail();

void
test_op_contextualize()
    PREINIT:
	OP *o;
    CODE:
	o = newSVOP(OP_CONST, 0, newSViv(0));
	o->op_flags &= ~OPf_WANT;
	o = op_contextualize(o, G_SCALAR);
	if (o->op_type != OP_CONST ||
		(o->op_flags & OPf_WANT) != OPf_WANT_SCALAR)
	    croak_fail();
	op_free(o);
	o = newSVOP(OP_CONST, 0, newSViv(0));
	o->op_flags &= ~OPf_WANT;
	o = op_contextualize(o, G_ARRAY);
	if (o->op_type != OP_CONST ||
		(o->op_flags & OPf_WANT) != OPf_WANT_LIST)
	    croak_fail();
	op_free(o);
	o = newSVOP(OP_CONST, 0, newSViv(0));
	o->op_flags &= ~OPf_WANT;
	o = op_contextualize(o, G_VOID);
	if (o->op_type != OP_NULL) croak_fail();
	op_free(o);

void
test_rv2cv_op_cv()
    PROTOTYPE:
    PREINIT:
	GV *troc_gv;
	CV *troc_cv;
	OP *o;
    CODE:
	troc_gv = gv_fetchpv("XS::APItest::test_rv2cv_op_cv", 0, SVt_PVGV);
	troc_cv = get_cv("XS::APItest::test_rv2cv_op_cv", 0);
	o = newCVREF(0, newGVOP(OP_GV, 0, troc_gv));
	if (rv2cv_op_cv(o, 0) != troc_cv) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_RETURN_NAME_GV) != (CV*)troc_gv)
	    croak_fail();
	o->op_private |= OPpENTERSUB_AMPER;
	if (rv2cv_op_cv(o, 0)) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_RETURN_NAME_GV)) croak_fail();
	o->op_private &= ~OPpENTERSUB_AMPER;
	if (cUNOPx(o)->op_first->op_private & OPpEARLY_CV) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_MARK_EARLY) != troc_cv) croak_fail();
	if (cUNOPx(o)->op_first->op_private & OPpEARLY_CV) croak_fail();
	op_free(o);
	o = newSVOP(OP_CONST, 0, newSVpv("XS::APItest::test_rv2cv_op_cv", 0));
	o->op_private = OPpCONST_BARE;
	o = newCVREF(0, o);
	if (rv2cv_op_cv(o, 0) != troc_cv) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_RETURN_NAME_GV) != (CV*)troc_gv)
	    croak_fail();
	o->op_private |= OPpENTERSUB_AMPER;
	if (rv2cv_op_cv(o, 0)) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_RETURN_NAME_GV)) croak_fail();
	op_free(o);
	o = newCVREF(0, newSVOP(OP_CONST, 0, newRV_inc((SV*)troc_cv)));
	if (rv2cv_op_cv(o, 0) != troc_cv) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_RETURN_NAME_GV) != (CV*)troc_gv)
	    croak_fail();
	o->op_private |= OPpENTERSUB_AMPER;
	if (rv2cv_op_cv(o, 0)) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_RETURN_NAME_GV)) croak_fail();
	o->op_private &= ~OPpENTERSUB_AMPER;
	if (cUNOPx(o)->op_first->op_private & OPpEARLY_CV) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_MARK_EARLY) != troc_cv) croak_fail();
	if (cUNOPx(o)->op_first->op_private & OPpEARLY_CV) croak_fail();
	op_free(o);
	o = newCVREF(0, newUNOP(OP_RAND, 0, newSVOP(OP_CONST, 0, newSViv(0))));
	if (rv2cv_op_cv(o, 0)) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_RETURN_NAME_GV)) croak_fail();
	o->op_private |= OPpENTERSUB_AMPER;
	if (rv2cv_op_cv(o, 0)) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_RETURN_NAME_GV)) croak_fail();
	o->op_private &= ~OPpENTERSUB_AMPER;
	if (cUNOPx(o)->op_first->op_private & OPpEARLY_CV) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_MARK_EARLY)) croak_fail();
	if (cUNOPx(o)->op_first->op_private & OPpEARLY_CV) croak_fail();
	op_free(o);
	o = newUNOP(OP_RAND, 0, newSVOP(OP_CONST, 0, newSViv(0)));
	if (rv2cv_op_cv(o, 0)) croak_fail();
	if (rv2cv_op_cv(o, RV2CVOPCV_RETURN_NAME_GV)) croak_fail();
	op_free(o);

void
test_cv_getset_call_checker()
    PREINIT:
	CV *troc_cv, *tsh_cv;
	Perl_call_checker ckfun;
	SV *ckobj;
    CODE:
#define check_cc(cv, xckfun, xckobj) \
    do { \
	cv_get_call_checker((cv), &ckfun, &ckobj); \
	if (ckfun != (xckfun)) croak_fail_ne(FPTR2DPTR(void *, ckfun), xckfun); \
	if (ckobj != (xckobj)) croak_fail_ne(FPTR2DPTR(void *, ckobj), xckobj); \
    } while(0)
	troc_cv = get_cv("XS::APItest::test_rv2cv_op_cv", 0);
	tsh_cv = get_cv("XS::APItest::test_savehints", 0);
	check_cc(troc_cv, Perl_ck_entersub_args_proto_or_list, (SV*)troc_cv);
	check_cc(tsh_cv, Perl_ck_entersub_args_proto_or_list, (SV*)tsh_cv);
	cv_set_call_checker(tsh_cv, Perl_ck_entersub_args_proto_or_list,
				    &PL_sv_yes);
	check_cc(troc_cv, Perl_ck_entersub_args_proto_or_list, (SV*)troc_cv);
	check_cc(tsh_cv, Perl_ck_entersub_args_proto_or_list, &PL_sv_yes);
	cv_set_call_checker(troc_cv, THX_ck_entersub_args_scalars, &PL_sv_no);
	check_cc(troc_cv, THX_ck_entersub_args_scalars, &PL_sv_no);
	check_cc(tsh_cv, Perl_ck_entersub_args_proto_or_list, &PL_sv_yes);
	cv_set_call_checker(tsh_cv, Perl_ck_entersub_args_proto_or_list,
				    (SV*)tsh_cv);
	check_cc(troc_cv, THX_ck_entersub_args_scalars, &PL_sv_no);
	check_cc(tsh_cv, Perl_ck_entersub_args_proto_or_list, (SV*)tsh_cv);
	cv_set_call_checker(troc_cv, Perl_ck_entersub_args_proto_or_list,
				    (SV*)troc_cv);
	check_cc(troc_cv, Perl_ck_entersub_args_proto_or_list, (SV*)troc_cv);
	check_cc(tsh_cv, Perl_ck_entersub_args_proto_or_list, (SV*)tsh_cv);
	if (SvMAGICAL((SV*)troc_cv) || SvMAGIC((SV*)troc_cv)) croak_fail();
	if (SvMAGICAL((SV*)tsh_cv) || SvMAGIC((SV*)tsh_cv)) croak_fail();
#undef check_cc

void
cv_set_call_checker_lists(CV *cv)
    CODE:
	cv_set_call_checker(cv, THX_ck_entersub_args_lists, &PL_sv_undef);

void
cv_set_call_checker_scalars(CV *cv)
    CODE:
	cv_set_call_checker(cv, THX_ck_entersub_args_scalars, &PL_sv_undef);

void
cv_set_call_checker_proto(CV *cv, SV *proto)
    CODE:
	if (SvROK(proto))
	    proto = SvRV(proto);
	cv_set_call_checker(cv, Perl_ck_entersub_args_proto, proto);

void
cv_set_call_checker_proto_or_list(CV *cv, SV *proto)
    CODE:
	if (SvROK(proto))
	    proto = SvRV(proto);
	cv_set_call_checker(cv, Perl_ck_entersub_args_proto_or_list, proto);

void
cv_set_call_checker_multi_sum(CV *cv)
    CODE:
	cv_set_call_checker(cv, THX_ck_entersub_multi_sum, &PL_sv_undef);

void
test_cophh()
    PREINIT:
	COPHH *a, *b;
    CODE:
#define check_ph(EXPR) \
    	    do { if((EXPR) != &PL_sv_placeholder) croak("fail"); } while(0)
#define check_iv(EXPR, EXPECT) \
    	    do { if(SvIV(EXPR) != (EXPECT)) croak("fail"); } while(0)
#define msvpvs(STR) sv_2mortal(newSVpvs(STR))
#define msviv(VALUE) sv_2mortal(newSViv(VALUE))
	a = cophh_new_empty();
	check_ph(cophh_fetch_pvn(a, "foo_1", 5, 0, 0));
	check_ph(cophh_fetch_pvs(a, "foo_1", 0));
	check_ph(cophh_fetch_pv(a, "foo_1", 0, 0));
	check_ph(cophh_fetch_sv(a, msvpvs("foo_1"), 0, 0));
	a = cophh_store_pvn(a, "foo_1abc", 5, 0, msviv(111), 0);
	a = cophh_store_pvs(a, "foo_2", msviv(222), 0);
	a = cophh_store_pv(a, "foo_3", 0, msviv(333), 0);
	a = cophh_store_sv(a, msvpvs("foo_4"), 0, msviv(444), 0);
	check_iv(cophh_fetch_pvn(a, "foo_1xyz", 5, 0, 0), 111);
	check_iv(cophh_fetch_pvs(a, "foo_1", 0), 111);
	check_iv(cophh_fetch_pv(a, "foo_1", 0, 0), 111);
	check_iv(cophh_fetch_sv(a, msvpvs("foo_1"), 0, 0), 111);
	check_iv(cophh_fetch_pvs(a, "foo_2", 0), 222);
	check_iv(cophh_fetch_pvs(a, "foo_3", 0), 333);
	check_iv(cophh_fetch_pvs(a, "foo_4", 0), 444);
	check_ph(cophh_fetch_pvs(a, "foo_5", 0));
	b = cophh_copy(a);
	b = cophh_store_pvs(b, "foo_1", msviv(1111), 0);
	check_iv(cophh_fetch_pvs(a, "foo_1", 0), 111);
	check_iv(cophh_fetch_pvs(a, "foo_2", 0), 222);
	check_iv(cophh_fetch_pvs(a, "foo_3", 0), 333);
	check_iv(cophh_fetch_pvs(a, "foo_4", 0), 444);
	check_ph(cophh_fetch_pvs(a, "foo_5", 0));
	check_iv(cophh_fetch_pvs(b, "foo_1", 0), 1111);
	check_iv(cophh_fetch_pvs(b, "foo_2", 0), 222);
	check_iv(cophh_fetch_pvs(b, "foo_3", 0), 333);
	check_iv(cophh_fetch_pvs(b, "foo_4", 0), 444);
	check_ph(cophh_fetch_pvs(b, "foo_5", 0));
	a = cophh_delete_pvn(a, "foo_1abc", 5, 0, 0);
	a = cophh_delete_pvs(a, "foo_2", 0);
	b = cophh_delete_pv(b, "foo_3", 0, 0);
	b = cophh_delete_sv(b, msvpvs("foo_4"), 0, 0);
	check_ph(cophh_fetch_pvs(a, "foo_1", 0));
	check_ph(cophh_fetch_pvs(a, "foo_2", 0));
	check_iv(cophh_fetch_pvs(a, "foo_3", 0), 333);
	check_iv(cophh_fetch_pvs(a, "foo_4", 0), 444);
	check_ph(cophh_fetch_pvs(a, "foo_5", 0));
	check_iv(cophh_fetch_pvs(b, "foo_1", 0), 1111);
	check_iv(cophh_fetch_pvs(b, "foo_2", 0), 222);
	check_ph(cophh_fetch_pvs(b, "foo_3", 0));
	check_ph(cophh_fetch_pvs(b, "foo_4", 0));
	check_ph(cophh_fetch_pvs(b, "foo_5", 0));
	b = cophh_delete_pvs(b, "foo_3", 0);
	b = cophh_delete_pvs(b, "foo_5", 0);
	check_iv(cophh_fetch_pvs(b, "foo_1", 0), 1111);
	check_iv(cophh_fetch_pvs(b, "foo_2", 0), 222);
	check_ph(cophh_fetch_pvs(b, "foo_3", 0));
	check_ph(cophh_fetch_pvs(b, "foo_4", 0));
	check_ph(cophh_fetch_pvs(b, "foo_5", 0));
	cophh_free(b);
	check_ph(cophh_fetch_pvs(a, "foo_1", 0));
	check_ph(cophh_fetch_pvs(a, "foo_2", 0));
	check_iv(cophh_fetch_pvs(a, "foo_3", 0), 333);
	check_iv(cophh_fetch_pvs(a, "foo_4", 0), 444);
	check_ph(cophh_fetch_pvs(a, "foo_5", 0));
	a = cophh_store_pvs(a, "foo_1", msviv(11111), COPHH_KEY_UTF8);
	a = cophh_store_pvs(a, "foo_\xaa", msviv(123), 0);
	a = cophh_store_pvs(a, "foo_\xc2\xbb", msviv(456), COPHH_KEY_UTF8);
	a = cophh_store_pvs(a, "foo_\xc3\x8c", msviv(789), COPHH_KEY_UTF8);
	a = cophh_store_pvs(a, "foo_\xd9\xa6", msviv(666), COPHH_KEY_UTF8);
	check_iv(cophh_fetch_pvs(a, "foo_1", 0), 11111);
	check_iv(cophh_fetch_pvs(a, "foo_1", COPHH_KEY_UTF8), 11111);
	check_iv(cophh_fetch_pvs(a, "foo_\xaa", 0), 123);
	check_iv(cophh_fetch_pvs(a, "foo_\xc2\xaa", COPHH_KEY_UTF8), 123);
	check_ph(cophh_fetch_pvs(a, "foo_\xc2\xaa", 0));
	check_iv(cophh_fetch_pvs(a, "foo_\xbb", 0), 456);
	check_iv(cophh_fetch_pvs(a, "foo_\xc2\xbb", COPHH_KEY_UTF8), 456);
	check_ph(cophh_fetch_pvs(a, "foo_\xc2\xbb", 0));
	check_iv(cophh_fetch_pvs(a, "foo_\xcc", 0), 789);
	check_iv(cophh_fetch_pvs(a, "foo_\xc3\x8c", COPHH_KEY_UTF8), 789);
	check_ph(cophh_fetch_pvs(a, "foo_\xc2\x8c", 0));
	check_iv(cophh_fetch_pvs(a, "foo_\xd9\xa6", COPHH_KEY_UTF8), 666);
	check_ph(cophh_fetch_pvs(a, "foo_\xd9\xa6", 0));
	ENTER;
	SAVEFREECOPHH(a);
	LEAVE;
#undef check_ph
#undef check_iv
#undef msvpvs
#undef msviv

void
test_coplabel()
    PREINIT:
        COP *cop;
        const char *label;
        STRLEN len;
        U32 utf8;
    CODE:
        cop = &PL_compiling;
        Perl_cop_store_label(aTHX_ cop, "foo", 3, 0);
        label = Perl_cop_fetch_label(aTHX_ cop, &len, &utf8);
        if (strcmp(label,"foo")) croak("fail # cop_fetch_label label");
        if (len != 3) croak("fail # cop_fetch_label len");
        if (utf8) croak("fail # cop_fetch_label utf8");
        /* SMALL GERMAN UMLAUT A */
        Perl_cop_store_label(aTHX_ cop, "fo\xc3\xa4", 4, SVf_UTF8);
        label = Perl_cop_fetch_label(aTHX_ cop, &len, &utf8);
        if (strcmp(label,"fo\xc3\xa4")) croak("fail # cop_fetch_label label");
        if (len != 4) croak("fail # cop_fetch_label len");
        if (!utf8) croak("fail # cop_fetch_label utf8");


HV *
example_cophh_2hv()
    PREINIT:
	COPHH *a;
    CODE:
#define msviv(VALUE) sv_2mortal(newSViv(VALUE))
	a = cophh_new_empty();
	a = cophh_store_pvs(a, "foo_0", msviv(999), 0);
	a = cophh_store_pvs(a, "foo_1", msviv(111), 0);
	a = cophh_store_pvs(a, "foo_\xaa", msviv(123), 0);
	a = cophh_store_pvs(a, "foo_\xc2\xbb", msviv(456), COPHH_KEY_UTF8);
	a = cophh_store_pvs(a, "foo_\xc3\x8c", msviv(789), COPHH_KEY_UTF8);
	a = cophh_store_pvs(a, "foo_\xd9\xa6", msviv(666), COPHH_KEY_UTF8);
	a = cophh_delete_pvs(a, "foo_0", 0);
	a = cophh_delete_pvs(a, "foo_2", 0);
	RETVAL = cophh_2hv(a, 0);
	cophh_free(a);
#undef msviv
    OUTPUT:
	RETVAL

void
test_savehints()
    PREINIT:
	SV **svp, *sv;
    CODE:
#define store_hint(KEY, VALUE) \
		sv_setiv_mg(*hv_fetchs(GvHV(PL_hintgv), KEY, 1), (VALUE))
#define hint_ok(KEY, EXPECT) \
		((svp = hv_fetchs(GvHV(PL_hintgv), KEY, 0)) && \
		    (sv = *svp) && SvIV(sv) == (EXPECT) && \
		    (sv = cop_hints_fetch_pvs(&PL_compiling, KEY, 0)) && \
		    SvIV(sv) == (EXPECT))
#define check_hint(KEY, EXPECT) \
		do { if (!hint_ok(KEY, EXPECT)) croak_fail(); } while(0)
	PL_hints |= HINT_LOCALIZE_HH;
	ENTER;
	SAVEHINTS();
	PL_hints &= HINT_INTEGER;
	store_hint("t0", 123);
	store_hint("t1", 456);
	if (PL_hints & HINT_INTEGER) croak_fail();
	check_hint("t0", 123); check_hint("t1", 456);
	ENTER;
	SAVEHINTS();
	if (PL_hints & HINT_INTEGER) croak_fail();
	check_hint("t0", 123); check_hint("t1", 456);
	PL_hints |= HINT_INTEGER;
	store_hint("t0", 321);
	if (!(PL_hints & HINT_INTEGER)) croak_fail();
	check_hint("t0", 321); check_hint("t1", 456);
	LEAVE;
	if (PL_hints & HINT_INTEGER) croak_fail();
	check_hint("t0", 123); check_hint("t1", 456);
	ENTER;
	SAVEHINTS();
	if (PL_hints & HINT_INTEGER) croak_fail();
	check_hint("t0", 123); check_hint("t1", 456);
	store_hint("t1", 654);
	if (PL_hints & HINT_INTEGER) croak_fail();
	check_hint("t0", 123); check_hint("t1", 654);
	LEAVE;
	if (PL_hints & HINT_INTEGER) croak_fail();
	check_hint("t0", 123); check_hint("t1", 456);
	LEAVE;
#undef store_hint
#undef hint_ok
#undef check_hint

void
test_copyhints()
    PREINIT:
	HV *a, *b;
    CODE:
	PL_hints |= HINT_LOCALIZE_HH;
	ENTER;
	SAVEHINTS();
	sv_setiv_mg(*hv_fetchs(GvHV(PL_hintgv), "t0", 1), 123);
	if (SvIV(cop_hints_fetch_pvs(&PL_compiling, "t0", 0)) != 123)
	    croak_fail();
	a = newHVhv(GvHV(PL_hintgv));
	sv_2mortal((SV*)a);
	sv_setiv_mg(*hv_fetchs(a, "t0", 1), 456);
	if (SvIV(cop_hints_fetch_pvs(&PL_compiling, "t0", 0)) != 123)
	    croak_fail();
	b = hv_copy_hints_hv(a);
	sv_2mortal((SV*)b);
	sv_setiv_mg(*hv_fetchs(b, "t0", 1), 789);
	if (SvIV(cop_hints_fetch_pvs(&PL_compiling, "t0", 0)) != 789)
	    croak_fail();
	LEAVE;

void
test_op_list()
    PREINIT:
	OP *a;
    CODE:
#define iv_op(iv) newSVOP(OP_CONST, 0, newSViv(iv))
#define check_op(o, expect) \
    do { \
	if (strcmp(test_op_list_describe(o), (expect))) \
	    croak("fail %s %s", test_op_list_describe(o), (expect)); \
    } while(0)
	a = op_append_elem(OP_LIST, NULL, NULL);
	check_op(a, "");
	a = op_append_elem(OP_LIST, iv_op(1), a);
	check_op(a, "const(1).");
	a = op_append_elem(OP_LIST, NULL, a);
	check_op(a, "const(1).");
	a = op_append_elem(OP_LIST, a, iv_op(2));
	check_op(a, "list[pushmark.const(1).const(2).]");
	a = op_append_elem(OP_LIST, a, iv_op(3));
	check_op(a, "list[pushmark.const(1).const(2).const(3).]");
	a = op_append_elem(OP_LIST, a, NULL);
	check_op(a, "list[pushmark.const(1).const(2).const(3).]");
	a = op_append_elem(OP_LIST, NULL, a);
	check_op(a, "list[pushmark.const(1).const(2).const(3).]");
	a = op_append_elem(OP_LIST, iv_op(4), a);
	check_op(a, "list[pushmark.const(4)."
		"list[pushmark.const(1).const(2).const(3).]]");
	a = op_append_elem(OP_LIST, a, iv_op(5));
	check_op(a, "list[pushmark.const(4)."
		"list[pushmark.const(1).const(2).const(3).]const(5).]");
	a = op_append_elem(OP_LIST, a, 
		op_append_elem(OP_LIST, iv_op(7), iv_op(6)));
	check_op(a, "list[pushmark.const(4)."
		"list[pushmark.const(1).const(2).const(3).]const(5)."
		"list[pushmark.const(7).const(6).]]");
	op_free(a);
	a = op_append_elem(OP_LINESEQ, iv_op(1), iv_op(2));
	check_op(a, "lineseq[const(1).const(2).]");
	a = op_append_elem(OP_LINESEQ, a, iv_op(3));
	check_op(a, "lineseq[const(1).const(2).const(3).]");
	op_free(a);
	a = op_append_elem(OP_LINESEQ,
		op_append_elem(OP_LIST, iv_op(1), iv_op(2)),
		iv_op(3));
	check_op(a, "lineseq[list[pushmark.const(1).const(2).]const(3).]");
	op_free(a);
	a = op_prepend_elem(OP_LIST, NULL, NULL);
	check_op(a, "");
	a = op_prepend_elem(OP_LIST, a, iv_op(1));
	check_op(a, "const(1).");
	a = op_prepend_elem(OP_LIST, a, NULL);
	check_op(a, "const(1).");
	a = op_prepend_elem(OP_LIST, iv_op(2), a);
	check_op(a, "list[pushmark.const(2).const(1).]");
	a = op_prepend_elem(OP_LIST, iv_op(3), a);
	check_op(a, "list[pushmark.const(3).const(2).const(1).]");
	a = op_prepend_elem(OP_LIST, NULL, a);
	check_op(a, "list[pushmark.const(3).const(2).const(1).]");
	a = op_prepend_elem(OP_LIST, a, NULL);
	check_op(a, "list[pushmark.const(3).const(2).const(1).]");
	a = op_prepend_elem(OP_LIST, a, iv_op(4));
	check_op(a, "list[pushmark."
		"list[pushmark.const(3).const(2).const(1).]const(4).]");
	a = op_prepend_elem(OP_LIST, iv_op(5), a);
	check_op(a, "list[pushmark.const(5)."
		"list[pushmark.const(3).const(2).const(1).]const(4).]");
	a = op_prepend_elem(OP_LIST,
		op_prepend_elem(OP_LIST, iv_op(6), iv_op(7)), a);
	check_op(a, "list[pushmark.list[pushmark.const(6).const(7).]const(5)."
		"list[pushmark.const(3).const(2).const(1).]const(4).]");
	op_free(a);
	a = op_prepend_elem(OP_LINESEQ, iv_op(2), iv_op(1));
	check_op(a, "lineseq[const(2).const(1).]");
	a = op_prepend_elem(OP_LINESEQ, iv_op(3), a);
	check_op(a, "lineseq[const(3).const(2).const(1).]");
	op_free(a);
	a = op_prepend_elem(OP_LINESEQ, iv_op(3),
		op_prepend_elem(OP_LIST, iv_op(2), iv_op(1)));
	check_op(a, "lineseq[const(3).list[pushmark.const(2).const(1).]]");
	op_free(a);
	a = op_append_list(OP_LINESEQ, NULL, NULL);
	check_op(a, "");
	a = op_append_list(OP_LINESEQ, iv_op(1), a);
	check_op(a, "const(1).");
	a = op_append_list(OP_LINESEQ, NULL, a);
	check_op(a, "const(1).");
	a = op_append_list(OP_LINESEQ, a, iv_op(2));
	check_op(a, "lineseq[const(1).const(2).]");
	a = op_append_list(OP_LINESEQ, a, iv_op(3));
	check_op(a, "lineseq[const(1).const(2).const(3).]");
	a = op_append_list(OP_LINESEQ, iv_op(4), a);
	check_op(a, "lineseq[const(4).const(1).const(2).const(3).]");
	a = op_append_list(OP_LINESEQ, a, NULL);
	check_op(a, "lineseq[const(4).const(1).const(2).const(3).]");
	a = op_append_list(OP_LINESEQ, NULL, a);
	check_op(a, "lineseq[const(4).const(1).const(2).const(3).]");
	a = op_append_list(OP_LINESEQ, a,
		op_append_list(OP_LINESEQ, iv_op(5), iv_op(6)));
	check_op(a, "lineseq[const(4).const(1).const(2).const(3)."
		"const(5).const(6).]");
	op_free(a);
	a = op_append_list(OP_LINESEQ,
		op_append_list(OP_LINESEQ, iv_op(1), iv_op(2)),
		op_append_list(OP_LIST, iv_op(3), iv_op(4)));
	check_op(a, "lineseq[const(1).const(2)."
		"list[pushmark.const(3).const(4).]]");
	op_free(a);
	a = op_append_list(OP_LINESEQ,
		op_append_list(OP_LIST, iv_op(1), iv_op(2)),
		op_append_list(OP_LINESEQ, iv_op(3), iv_op(4)));
	check_op(a, "lineseq[list[pushmark.const(1).const(2).]"
		"const(3).const(4).]");
	op_free(a);
#undef check_op

void
test_op_linklist ()
    PREINIT:
        OP *o;
    CODE:
#define check_ll(o, expect) \
    STMT_START { \
	if (strNE(test_op_linklist_describe(o), (expect))) \
	    croak("fail %s %s", test_op_linklist_describe(o), (expect)); \
    } STMT_END
        o = iv_op(1);
        check_ll(o, ".const1");
        op_free(o);

        o = mkUNOP(OP_NOT, iv_op(1));
        check_ll(o, ".const1.not");
        op_free(o);

        o = mkUNOP(OP_NOT, mkUNOP(OP_NEGATE, iv_op(1)));
        check_ll(o, ".const1.negate.not");
        op_free(o);

        o = mkBINOP(OP_ADD, iv_op(1), iv_op(2));
        check_ll(o, ".const1.const2.add");
        op_free(o);

        o = mkBINOP(OP_ADD, mkUNOP(OP_NOT, iv_op(1)), iv_op(2));
        check_ll(o, ".const1.not.const2.add");
        op_free(o);

        o = mkUNOP(OP_NOT, mkBINOP(OP_ADD, iv_op(1), iv_op(2)));
        check_ll(o, ".const1.const2.add.not");
        op_free(o);

        o = mkLISTOP(OP_LINESEQ, iv_op(1), iv_op(2), iv_op(3));
        check_ll(o, ".const1.const2.const3.lineseq");
        op_free(o);

        o = mkLISTOP(OP_LINESEQ,
                mkBINOP(OP_ADD, iv_op(1), iv_op(2)),
                mkUNOP(OP_NOT, iv_op(3)),
                mkLISTOP(OP_SUBSTR, iv_op(4), iv_op(5), iv_op(6)));
        check_ll(o, ".const1.const2.add.const3.not"
                    ".const4.const5.const6.substr.lineseq");
        op_free(o);

        o = mkBINOP(OP_ADD, iv_op(1), iv_op(2));
        LINKLIST(o);
        o = mkBINOP(OP_SUBTRACT, o, iv_op(3));
        check_ll(o, ".const1.const2.add.const3.subtract");
        op_free(o);
#undef check_ll
#undef iv_op

void
peep_enable ()
    PREINIT:
	dMY_CXT;
    CODE:
	av_clear(MY_CXT.peep_recorder);
	av_clear(MY_CXT.rpeep_recorder);
	MY_CXT.peep_recording = 1;

void
peep_disable ()
    PREINIT:
	dMY_CXT;
    CODE:
	MY_CXT.peep_recording = 0;

SV *
peep_record ()
    PREINIT:
	dMY_CXT;
    CODE:
	RETVAL = newRV_inc((SV *)MY_CXT.peep_recorder);
    OUTPUT:
	RETVAL

SV *
rpeep_record ()
    PREINIT:
	dMY_CXT;
    CODE:
	RETVAL = newRV_inc((SV *)MY_CXT.rpeep_recorder);
    OUTPUT:
	RETVAL

=pod

multicall_each: call a sub for each item in the list. Used to test MULTICALL

=cut

void
multicall_each(block,...)
    SV * block
PROTOTYPE: &@
CODE:
{
    dMULTICALL;
    int index;
    GV *gv;
    HV *stash;
    I32 gimme = G_SCALAR;
    SV **args = &PL_stack_base[ax];
    CV *cv;

    if(items <= 1) {
	XSRETURN_UNDEF;
    }
    cv = sv_2cv(block, &stash, &gv, 0);
    if (cv == Nullcv) {
       croak("multicall_each: not a subroutine reference");
    }
    PUSH_MULTICALL(cv);
    SAVESPTR(GvSV(PL_defgv));

    for(index = 1 ; index < items ; index++) {
	GvSV(PL_defgv) = args[index];
	MULTICALL;
    }
    POP_MULTICALL;
    PERL_UNUSED_VAR(newsp);
    XSRETURN_UNDEF;
}

#ifdef USE_ITHREADS

void
clone_with_stack()
CODE:
{
    PerlInterpreter *interp = aTHX; /* The original interpreter */
    PerlInterpreter *interp_dup;    /* The duplicate interpreter */
    int oldscope = 1; /* We are responsible for all scopes */

    interp_dup = perl_clone(interp, CLONEf_COPY_STACKS | CLONEf_CLONE_HOST );

    /* destroy old perl */
    PERL_SET_CONTEXT(interp);

    POPSTACK_TO(PL_mainstack);
    dounwind(-1);
    LEAVE_SCOPE(0);

    while (interp->Iscopestack_ix > 1)
        LEAVE;
    FREETMPS;

    perl_destruct(interp);
    perl_free(interp);

    /* switch to new perl */
    PERL_SET_CONTEXT(interp_dup);

    /* continue after 'clone_with_stack' */
    if (interp_dup->Iop)
	interp_dup->Iop = interp_dup->Iop->op_next;

    /* run with new perl */
    Perl_runops_standard(interp_dup);

    /* We may have additional unclosed scopes if fork() was called
     * from within a BEGIN block.  See perlfork.pod for more details.
     * We cannot clean up these other scopes because they belong to a
     * different interpreter, but we also cannot leave PL_scopestack_ix
     * dangling because that can trigger an assertion in perl_destruct().
     */
    if (PL_scopestack_ix > oldscope) {
        PL_scopestack[oldscope-1] = PL_scopestack[PL_scopestack_ix-1];
        PL_scopestack_ix = oldscope;
    }

    perl_destruct(interp_dup);
    perl_free(interp_dup);

    /* call the real 'exit' not PerlProc_exit */
#undef exit
    exit(0);
}

#endif /* USE_ITHREDS */

SV*
take_svref(SVREF sv)
CODE:
    RETVAL = newRV_inc(sv);
OUTPUT:
    RETVAL

SV*
take_avref(AV* av)
CODE:
    RETVAL = newRV_inc((SV*)av);
OUTPUT:
    RETVAL

SV*
take_hvref(HV* hv)
CODE:
    RETVAL = newRV_inc((SV*)hv);
OUTPUT:
    RETVAL


SV*
take_cvref(CV* cv)
CODE:
    RETVAL = newRV_inc((SV*)cv);
OUTPUT:
    RETVAL


BOOT:
	{
	HV* stash;
	SV** meth = NULL;
	CV* cv;
	stash = gv_stashpv("XS::APItest::TempLv", 0);
	if (stash)
	    meth = hv_fetchs(stash, "make_temp_mg_lv", 0);
	if (!meth)
	    croak("lost method 'make_temp_mg_lv'");
	cv = GvCV(*meth);
	CvLVALUE_on(cv);
	}

BOOT:
{
    hintkey_rpn_sv = newSVpvs_share("XS::APItest/rpn");
    hintkey_calcrpn_sv = newSVpvs_share("XS::APItest/calcrpn");
    hintkey_stufftest_sv = newSVpvs_share("XS::APItest/stufftest");
    hintkey_swaptwostmts_sv = newSVpvs_share("XS::APItest/swaptwostmts");
    hintkey_looprest_sv = newSVpvs_share("XS::APItest/looprest");
    hintkey_scopelessblock_sv = newSVpvs_share("XS::APItest/scopelessblock");
    hintkey_stmtasexpr_sv = newSVpvs_share("XS::APItest/stmtasexpr");
    hintkey_stmtsasexpr_sv = newSVpvs_share("XS::APItest/stmtsasexpr");
    hintkey_loopblock_sv = newSVpvs_share("XS::APItest/loopblock");
    hintkey_blockasexpr_sv = newSVpvs_share("XS::APItest/blockasexpr");
    hintkey_swaplabel_sv = newSVpvs_share("XS::APItest/swaplabel");
    hintkey_labelconst_sv = newSVpvs_share("XS::APItest/labelconst");
    hintkey_arrayfullexpr_sv = newSVpvs_share("XS::APItest/arrayfullexpr");
    hintkey_arraylistexpr_sv = newSVpvs_share("XS::APItest/arraylistexpr");
    hintkey_arraytermexpr_sv = newSVpvs_share("XS::APItest/arraytermexpr");
    hintkey_arrayarithexpr_sv = newSVpvs_share("XS::APItest/arrayarithexpr");
    hintkey_arrayexprflags_sv = newSVpvs_share("XS::APItest/arrayexprflags");
    next_keyword_plugin = PL_keyword_plugin;
    PL_keyword_plugin = my_keyword_plugin;
}

void
establish_cleanup(...)
PROTOTYPE: $
CODE:
    PERL_UNUSED_VAR(items);
    croak("establish_cleanup called as a function");

BOOT:
{
    CV *estcv = get_cv("XS::APItest::establish_cleanup", 0);
    cv_set_call_checker(estcv, THX_ck_entersub_establish_cleanup, (SV*)estcv);
}

void
postinc(...)
PROTOTYPE: $
CODE:
    PERL_UNUSED_VAR(items);
    croak("postinc called as a function");

void
filter()
CODE:
    filter_add(filter_call, NULL);

BOOT:
{
    CV *asscv = get_cv("XS::APItest::postinc", 0);
    cv_set_call_checker(asscv, THX_ck_entersub_postinc, (SV*)asscv);
}

SV *
lv_temp_object()
CODE:
    RETVAL =
	  sv_bless(
	    newRV_noinc(newSV(0)),
	    gv_stashpvs("XS::APItest::TempObj",GV_ADD)
	  );             /* Package defined in test script */
OUTPUT:
    RETVAL

void
fill_hash_with_nulls(HV *hv)
PREINIT:
    UV i = 0;
CODE:
    for(; i < 1000; ++i) {
	HE *entry = hv_fetch_ent(hv, sv_2mortal(newSVuv(i)), 1, 0);
	SvREFCNT_dec(HeVAL(entry));
	HeVAL(entry) = NULL;
    }

HV *
newHVhv(HV *hv)
CODE:
    RETVAL = newHVhv(hv);
OUTPUT:
    RETVAL

U32
SvIsCOW(SV *sv)
CODE:
    RETVAL = SvIsCOW(sv);
OUTPUT:
    RETVAL

void
pad_scalar(...)
PROTOTYPE: $$
CODE:
    PERL_UNUSED_VAR(items);
    croak("pad_scalar called as a function");

BOOT:
{
    CV *pscv = get_cv("XS::APItest::pad_scalar", 0);
    cv_set_call_checker(pscv, THX_ck_entersub_pad_scalar, (SV*)pscv);
}

SV*
fetch_pad_names( cv )
CV* cv
 PREINIT:
  I32 i;
  PADNAMELIST *pad_namelist;
  AV *retav = newAV();
 CODE:
  pad_namelist = PadlistNAMES(CvPADLIST(cv));

  for ( i = PadnamelistMAX(pad_namelist); i >= 0; i-- ) {
    PADNAME* name = PadnamelistARRAY(pad_namelist)[i];

    if (PadnameLEN(name)) {
        av_push(retav, newSVpadname(name));
    }
  }
  RETVAL = newRV_noinc((SV*)retav);
 OUTPUT:
  RETVAL

STRLEN
underscore_length()
PROTOTYPE:
PREINIT:
    SV *u;
    U8 *pv;
    STRLEN bytelen;
CODE:
    u = find_rundefsv();
    pv = (U8*)SvPV(u, bytelen);
    RETVAL = SvUTF8(u) ? utf8_length(pv, pv+bytelen) : bytelen;
OUTPUT:
    RETVAL

void
stringify(SV *sv)
CODE:
    (void)SvPV_nolen(sv);

SV *
HvENAME(HV *hv)
CODE:
    RETVAL = hv && HvENAME(hv)
	      ? newSVpvn_flags(
		  HvENAME(hv),HvENAMELEN(hv),
		  (HvENAMEUTF8(hv) ? SVf_UTF8 : 0)
		)
	      : NULL;
OUTPUT:
    RETVAL

int
xs_cmp(int a, int b)
CODE:
    /* Odd sorting (odd numbers first), to make sure we are actually
       being called */
    RETVAL = a % 2 != b % 2
	       ? a % 2 ? -1 : 1
	       : a < b ? -1 : a == b ? 0 : 1;
OUTPUT:
    RETVAL

SV *
xs_cmp_undef(SV *a, SV *b)
CODE:
    PERL_UNUSED_ARG(a);
    PERL_UNUSED_ARG(b);
    RETVAL = &PL_sv_undef;
OUTPUT:
    RETVAL

char *
SvPVbyte(SV *sv)
CODE:
    RETVAL = SvPVbyte_nolen(sv);
OUTPUT:
    RETVAL

char *
SvPVutf8(SV *sv)
CODE:
    RETVAL = SvPVutf8_nolen(sv);
OUTPUT:
    RETVAL

void
setup_addissub()
CODE:
    wrap_op_checker(OP_ADD, addissub_myck_add, &addissub_nxck_add);

void
setup_rv2cv_addunderbar()
CODE:
    wrap_op_checker(OP_RV2CV, my_ck_rv2cv, &old_ck_rv2cv);

#ifdef USE_ITHREADS

bool
test_alloccopstash()
CODE:
    RETVAL = PL_stashpad[alloccopstash(PL_defstash)] == PL_defstash;
OUTPUT:
    RETVAL

#endif

bool
test_newFOROP_without_slab()
CODE:
    {
	const I32 floor = start_subparse(0,0);
	/* The slab allocator does not like CvROOT being set. */
	CvROOT(PL_compcv) = (OP *)1;
	op_free(newFOROP(0, 0, newOP(OP_PUSHMARK, 0), 0, 0));
	CvROOT(PL_compcv) = NULL;
	SvREFCNT_dec(PL_compcv);
	LEAVE_SCOPE(floor);
	/* If we have not crashed yet, then the test passes. */
	RETVAL = TRUE;
    }
OUTPUT:
    RETVAL

 # provide access to CALLREGEXEC, except replace pointers within the
 # string with offsets from the start of the string

I32
callregexec(SV *prog, STRLEN stringarg, STRLEN strend, I32 minend, SV *sv, U32 nosave)
CODE:
    {
	STRLEN len;
	char *strbeg;
	if (SvROK(prog))
	    prog = SvRV(prog);
	strbeg = SvPV_force(sv, len);
	RETVAL = CALLREGEXEC((REGEXP *)prog,
			    strbeg + stringarg,
			    strbeg + strend,
			    strbeg,
			    minend,
			    sv,
			    NULL, /* data */
			    nosave);
    }
OUTPUT:
    RETVAL

void
lexical_import(SV *name, CV *cv)
    CODE:
    {
	PADLIST *pl;
	PADOFFSET off;
	if (!PL_compcv)
	    Perl_croak(aTHX_
		      "lexical_import can only be called at compile time");
	pl = CvPADLIST(PL_compcv);
	ENTER;
	SAVESPTR(PL_comppad_name); PL_comppad_name = PadlistNAMES(pl);
	SAVESPTR(PL_comppad);	   PL_comppad	   = PadlistARRAY(pl)[1];
	SAVESPTR(PL_curpad);	   PL_curpad	   = PadARRAY(PL_comppad);
	off = pad_add_name_sv(sv_2mortal(newSVpvf("&%"SVf,name)),
			      padadd_STATE, 0, 0);
	SvREFCNT_dec(PL_curpad[off]);
	PL_curpad[off] = SvREFCNT_inc(cv);
	LEAVE;
    }

SV *
sv_mortalcopy(SV *sv)
    CODE:
	RETVAL = SvREFCNT_inc(sv_mortalcopy(sv));
    OUTPUT:
	RETVAL

SV *
newRV(SV *sv)

void
alias_av(AV *av, IV ix, SV *sv)
    CODE:
	av_store(av, ix, SvREFCNT_inc(sv));

MODULE = XS::APItest PACKAGE = XS::APItest::AUTOLOADtest

int
AUTOLOAD(...)
  INIT:
    SV* comms;
    SV* class_and_method;
  CODE:
    PERL_UNUSED_ARG(items);
    class_and_method = GvSV(CvGV(cv));
    comms = get_sv("main::the_method", 1);
    if (class_and_method == NULL) {
      RETVAL = 1;
    } else if (!SvOK(class_and_method)) {
      RETVAL = 2;
    } else if (!SvPOK(class_and_method)) {
      RETVAL = 3;
    } else {
      sv_setsv(comms, class_and_method);
      RETVAL = 0;
    }
  OUTPUT: RETVAL


MODULE = XS::APItest		PACKAGE = XS::APItest::Magic

PROTOTYPES: DISABLE

void
sv_magic_foo(SV *sv, SV *thingy)
ALIAS:
    sv_magic_bar = 1
CODE:
    sv_magicext(SvRV(sv), NULL, PERL_MAGIC_ext, ix ? &vtbl_bar : &vtbl_foo, (const char *)thingy, 0);

SV *
mg_find_foo(SV *sv)
ALIAS:
    mg_find_bar = 1
CODE:
    MAGIC *mg = mg_findext(SvRV(sv), PERL_MAGIC_ext, ix ? &vtbl_bar : &vtbl_foo);
    RETVAL = mg ? SvREFCNT_inc((SV *)mg->mg_ptr) : &PL_sv_undef;
OUTPUT:
    RETVAL

void
sv_unmagic_foo(SV *sv)
ALIAS:
    sv_unmagic_bar = 1
CODE:
    sv_unmagicext(SvRV(sv), PERL_MAGIC_ext, ix ? &vtbl_bar : &vtbl_foo);

UV
test_get_vtbl()
    PREINIT:
	MGVTBL *have;
	MGVTBL *want;
    CODE:
#define test_get_this_vtable(name) \
	want = (MGVTBL*)CAT2(&PL_vtbl_, name); \
	have = get_vtbl(CAT2(want_vtbl_, name)); \
	if (have != want) \
	    croak("fail %p!=%p for get_vtbl(want_vtbl_" STRINGIFY(name) ") at " __FILE__ " line %d", have, want, __LINE__)

	test_get_this_vtable(sv);
	test_get_this_vtable(env);
	test_get_this_vtable(envelem);
	test_get_this_vtable(sigelem);
	test_get_this_vtable(pack);
	test_get_this_vtable(packelem);
	test_get_this_vtable(dbline);
	test_get_this_vtable(isa);
	test_get_this_vtable(isaelem);
	test_get_this_vtable(arylen);
	test_get_this_vtable(mglob);
	test_get_this_vtable(nkeys);
	test_get_this_vtable(taint);
	test_get_this_vtable(substr);
	test_get_this_vtable(vec);
	test_get_this_vtable(pos);
	test_get_this_vtable(bm);
	test_get_this_vtable(fm);
	test_get_this_vtable(uvar);
	test_get_this_vtable(defelem);
	test_get_this_vtable(regexp);
	test_get_this_vtable(regdata);
	test_get_this_vtable(regdatum);
#ifdef USE_LOCALE_COLLATE
	test_get_this_vtable(collxfrm);
#endif
	test_get_this_vtable(backref);
	test_get_this_vtable(utf8);

	RETVAL = PTR2UV(get_vtbl(-1));
    OUTPUT:
	RETVAL

bool
test_isBLANK_uni(UV ord)
    CODE:
        RETVAL = isBLANK_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isBLANK_LC_uvchr(UV ord)
    CODE:
        RETVAL = isBLANK_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isBLANK_A(UV ord)
    CODE:
        RETVAL = isBLANK_A(ord);
    OUTPUT:
        RETVAL

bool
test_isBLANK_L1(UV ord)
    CODE:
        RETVAL = isBLANK_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isBLANK_LC(UV ord)
    CODE:
        RETVAL = isBLANK_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isBLANK_utf8(unsigned char * p)
    CODE:
        RETVAL = isBLANK_utf8(p);
    OUTPUT:
        RETVAL

bool
test_isBLANK_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isBLANK_LC_utf8(p);
    OUTPUT:
        RETVAL

bool
test_isVERTWS_uni(UV ord)
    CODE:
        RETVAL = isVERTWS_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isVERTWS_utf8(unsigned char * p)
    CODE:
        RETVAL = isVERTWS_utf8(p);
    OUTPUT:
        RETVAL

bool
test_isUPPER_uni(UV ord)
    CODE:
        RETVAL = isUPPER_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isUPPER_LC_uvchr(UV ord)
    CODE:
        RETVAL = isUPPER_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isUPPER_A(UV ord)
    CODE:
        RETVAL = isUPPER_A(ord);
    OUTPUT:
        RETVAL

bool
test_isUPPER_L1(UV ord)
    CODE:
        RETVAL = isUPPER_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isUPPER_LC(UV ord)
    CODE:
        RETVAL = isUPPER_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isUPPER_utf8(unsigned char * p)
    CODE:
        RETVAL = isUPPER_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isUPPER_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isUPPER_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isLOWER_uni(UV ord)
    CODE:
        RETVAL = isLOWER_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isLOWER_LC_uvchr(UV ord)
    CODE:
        RETVAL = isLOWER_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isLOWER_A(UV ord)
    CODE:
        RETVAL = isLOWER_A(ord);
    OUTPUT:
        RETVAL

bool
test_isLOWER_L1(UV ord)
    CODE:
        RETVAL = isLOWER_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isLOWER_LC(UV ord)
    CODE:
        RETVAL = isLOWER_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isLOWER_utf8(unsigned char * p)
    CODE:
        RETVAL = isLOWER_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isLOWER_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isLOWER_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isALPHA_uni(UV ord)
    CODE:
        RETVAL = isALPHA_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHA_LC_uvchr(UV ord)
    CODE:
        RETVAL = isALPHA_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHA_A(UV ord)
    CODE:
        RETVAL = isALPHA_A(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHA_L1(UV ord)
    CODE:
        RETVAL = isALPHA_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHA_LC(UV ord)
    CODE:
        RETVAL = isALPHA_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHA_utf8(unsigned char * p)
    CODE:
        RETVAL = isALPHA_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isALPHA_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isALPHA_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isWORDCHAR_uni(UV ord)
    CODE:
        RETVAL = isWORDCHAR_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isWORDCHAR_LC_uvchr(UV ord)
    CODE:
        RETVAL = isWORDCHAR_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isWORDCHAR_A(UV ord)
    CODE:
        RETVAL = isWORDCHAR_A(ord);
    OUTPUT:
        RETVAL

bool
test_isWORDCHAR_L1(UV ord)
    CODE:
        RETVAL = isWORDCHAR_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isWORDCHAR_LC(UV ord)
    CODE:
        RETVAL = isWORDCHAR_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isWORDCHAR_utf8(unsigned char * p)
    CODE:
        RETVAL = isWORDCHAR_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isWORDCHAR_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isWORDCHAR_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isALPHANUMERIC_uni(UV ord)
    CODE:
        RETVAL = isALPHANUMERIC_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHANUMERIC_LC_uvchr(UV ord)
    CODE:
        RETVAL = isALPHANUMERIC_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHANUMERIC_A(UV ord)
    CODE:
        RETVAL = isALPHANUMERIC_A(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHANUMERIC_L1(UV ord)
    CODE:
        RETVAL = isALPHANUMERIC_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHANUMERIC_LC(UV ord)
    CODE:
        RETVAL = isALPHANUMERIC_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isALPHANUMERIC_utf8(unsigned char * p)
    CODE:
        RETVAL = isALPHANUMERIC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isALPHANUMERIC_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isALPHANUMERIC_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isALNUM_uni(UV ord)
    CODE:
        RETVAL = isALNUM_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isALNUM_LC_uvchr(UV ord)
    CODE:
        RETVAL = isALNUM_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isALNUM_LC(UV ord)
    CODE:
        RETVAL = isALNUM_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isALNUM_utf8(unsigned char * p)
    CODE:
        RETVAL = isALNUM_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isALNUM_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isALNUM_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isDIGIT_uni(UV ord)
    CODE:
        RETVAL = isDIGIT_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isDIGIT_LC_uvchr(UV ord)
    CODE:
        RETVAL = isDIGIT_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isDIGIT_utf8(unsigned char * p)
    CODE:
        RETVAL = isDIGIT_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isDIGIT_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isDIGIT_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isDIGIT_A(UV ord)
    CODE:
        RETVAL = isDIGIT_A(ord);
    OUTPUT:
        RETVAL

bool
test_isDIGIT_L1(UV ord)
    CODE:
        RETVAL = isDIGIT_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isDIGIT_LC(UV ord)
    CODE:
        RETVAL = isDIGIT_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isIDFIRST_uni(UV ord)
    CODE:
        RETVAL = isIDFIRST_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isIDFIRST_LC_uvchr(UV ord)
    CODE:
        RETVAL = isIDFIRST_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isIDFIRST_A(UV ord)
    CODE:
        RETVAL = isIDFIRST_A(ord);
    OUTPUT:
        RETVAL

bool
test_isIDFIRST_L1(UV ord)
    CODE:
        RETVAL = isIDFIRST_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isIDFIRST_LC(UV ord)
    CODE:
        RETVAL = isIDFIRST_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isIDFIRST_utf8(unsigned char * p)
    CODE:
        RETVAL = isIDFIRST_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isIDFIRST_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isIDFIRST_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isIDCONT_uni(UV ord)
    CODE:
        RETVAL = isIDCONT_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isIDCONT_LC_uvchr(UV ord)
    CODE:
        RETVAL = isIDCONT_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isIDCONT_A(UV ord)
    CODE:
        RETVAL = isIDCONT_A(ord);
    OUTPUT:
        RETVAL

bool
test_isIDCONT_L1(UV ord)
    CODE:
        RETVAL = isIDCONT_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isIDCONT_LC(UV ord)
    CODE:
        RETVAL = isIDCONT_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isIDCONT_utf8(unsigned char * p)
    CODE:
        RETVAL = isIDCONT_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isIDCONT_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isIDCONT_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isSPACE_uni(UV ord)
    CODE:
        RETVAL = isSPACE_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isSPACE_LC_uvchr(UV ord)
    CODE:
        RETVAL = isSPACE_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isSPACE_A(UV ord)
    CODE:
        RETVAL = isSPACE_A(ord);
    OUTPUT:
        RETVAL

bool
test_isSPACE_L1(UV ord)
    CODE:
        RETVAL = isSPACE_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isSPACE_LC(UV ord)
    CODE:
        RETVAL = isSPACE_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isSPACE_utf8(unsigned char * p)
    CODE:
        RETVAL = isSPACE_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isSPACE_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isSPACE_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isASCII_uni(UV ord)
    CODE:
        RETVAL = isASCII_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isASCII_LC_uvchr(UV ord)
    CODE:
        RETVAL = isASCII_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isASCII_A(UV ord)
    CODE:
        RETVAL = isASCII_A(ord);
    OUTPUT:
        RETVAL

bool
test_isASCII_L1(UV ord)
    CODE:
        RETVAL = isASCII_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isASCII_LC(UV ord)
    CODE:
        RETVAL = isASCII_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isASCII_utf8(unsigned char * p)
    CODE:
        RETVAL = isASCII_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isASCII_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isASCII_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isCNTRL_uni(UV ord)
    CODE:
        RETVAL = isCNTRL_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isCNTRL_LC_uvchr(UV ord)
    CODE:
        RETVAL = isCNTRL_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isCNTRL_A(UV ord)
    CODE:
        RETVAL = isCNTRL_A(ord);
    OUTPUT:
        RETVAL

bool
test_isCNTRL_L1(UV ord)
    CODE:
        RETVAL = isCNTRL_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isCNTRL_LC(UV ord)
    CODE:
        RETVAL = isCNTRL_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isCNTRL_utf8(unsigned char * p)
    CODE:
        RETVAL = isCNTRL_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isCNTRL_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isCNTRL_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isPRINT_uni(UV ord)
    CODE:
        RETVAL = isPRINT_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isPRINT_LC_uvchr(UV ord)
    CODE:
        RETVAL = isPRINT_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isPRINT_A(UV ord)
    CODE:
        RETVAL = isPRINT_A(ord);
    OUTPUT:
        RETVAL

bool
test_isPRINT_L1(UV ord)
    CODE:
        RETVAL = isPRINT_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isPRINT_LC(UV ord)
    CODE:
        RETVAL = isPRINT_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isPRINT_utf8(unsigned char * p)
    CODE:
        RETVAL = isPRINT_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isPRINT_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isPRINT_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isGRAPH_uni(UV ord)
    CODE:
        RETVAL = isGRAPH_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isGRAPH_LC_uvchr(UV ord)
    CODE:
        RETVAL = isGRAPH_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isGRAPH_A(UV ord)
    CODE:
        RETVAL = isGRAPH_A(ord);
    OUTPUT:
        RETVAL

bool
test_isGRAPH_L1(UV ord)
    CODE:
        RETVAL = isGRAPH_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isGRAPH_LC(UV ord)
    CODE:
        RETVAL = isGRAPH_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isGRAPH_utf8(unsigned char * p)
    CODE:
        RETVAL = isGRAPH_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isGRAPH_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isGRAPH_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isPUNCT_uni(UV ord)
    CODE:
        RETVAL = isPUNCT_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isPUNCT_LC_uvchr(UV ord)
    CODE:
        RETVAL = isPUNCT_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isPUNCT_A(UV ord)
    CODE:
        RETVAL = isPUNCT_A(ord);
    OUTPUT:
        RETVAL

bool
test_isPUNCT_L1(UV ord)
    CODE:
        RETVAL = isPUNCT_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isPUNCT_LC(UV ord)
    CODE:
        RETVAL = isPUNCT_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isPUNCT_utf8(unsigned char * p)
    CODE:
        RETVAL = isPUNCT_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isPUNCT_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isPUNCT_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isXDIGIT_uni(UV ord)
    CODE:
        RETVAL = isXDIGIT_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isXDIGIT_LC_uvchr(UV ord)
    CODE:
        RETVAL = isXDIGIT_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isXDIGIT_A(UV ord)
    CODE:
        RETVAL = isXDIGIT_A(ord);
    OUTPUT:
        RETVAL

bool
test_isXDIGIT_L1(UV ord)
    CODE:
        RETVAL = isXDIGIT_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isXDIGIT_LC(UV ord)
    CODE:
        RETVAL = isXDIGIT_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isXDIGIT_utf8(unsigned char * p)
    CODE:
        RETVAL = isXDIGIT_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isXDIGIT_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isXDIGIT_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isPSXSPC_uni(UV ord)
    CODE:
        RETVAL = isPSXSPC_uni(ord);
    OUTPUT:
        RETVAL

bool
test_isPSXSPC_LC_uvchr(UV ord)
    CODE:
        RETVAL = isPSXSPC_LC_uvchr(ord);
    OUTPUT:
        RETVAL

bool
test_isPSXSPC_A(UV ord)
    CODE:
        RETVAL = isPSXSPC_A(ord);
    OUTPUT:
        RETVAL

bool
test_isPSXSPC_L1(UV ord)
    CODE:
        RETVAL = isPSXSPC_L1(ord);
    OUTPUT:
        RETVAL

bool
test_isPSXSPC_LC(UV ord)
    CODE:
        RETVAL = isPSXSPC_LC(ord);
    OUTPUT:
        RETVAL

bool
test_isPSXSPC_utf8(unsigned char * p)
    CODE:
        RETVAL = isPSXSPC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isPSXSPC_LC_utf8(unsigned char * p)
    CODE:
        RETVAL = isPSXSPC_LC_utf8( p);
    OUTPUT:
        RETVAL

bool
test_isQUOTEMETA(UV ord)
    CODE:
        RETVAL = _isQUOTEMETA(ord);
    OUTPUT:
        RETVAL

UV
test_toLOWER(UV ord)
    CODE:
        RETVAL = toLOWER(ord);
    OUTPUT:
        RETVAL

UV
test_toLOWER_L1(UV ord)
    CODE:
        RETVAL = toLOWER_L1(ord);
    OUTPUT:
        RETVAL

UV
test_toLOWER_LC(UV ord)
    CODE:
        RETVAL = toLOWER_LC(ord);
    OUTPUT:
        RETVAL

AV *
test_toLOWER_uni(UV ord)
    PREINIT:
        U8 s[UTF8_MAXBYTES_CASE + 1];
        STRLEN len;
        AV *av;
        SV *utf8;
    CODE:
        av = newAV();
        av_push(av, newSVuv(toLOWER_uni(ord, s, &len)));

        utf8 = newSVpvn((char *) s, len);
        SvUTF8_on(utf8);
        av_push(av, utf8);

        av_push(av, newSVuv(len));
        RETVAL = av;
    OUTPUT:
        RETVAL

AV *
test_toLOWER_utf8(SV * p)
    PREINIT:
        U8 *input;
        U8 s[UTF8_MAXBYTES_CASE + 1];
        STRLEN len;
        AV *av;
        SV *utf8;
    CODE:
        input = (U8 *) SvPV(p, len);
        av = newAV();
        av_push(av, newSVuv(toLOWER_utf8(input, s, &len)));

        utf8 = newSVpvn((char *) s, len);
        SvUTF8_on(utf8);
        av_push(av, utf8);

        av_push(av, newSVuv(len));
        RETVAL = av;
    OUTPUT:
        RETVAL

UV
test_toFOLD(UV ord)
    CODE:
        RETVAL = toFOLD(ord);
    OUTPUT:
        RETVAL

UV
test_toFOLD_LC(UV ord)
    CODE:
        RETVAL = toFOLD_LC(ord);
    OUTPUT:
        RETVAL

AV *
test_toFOLD_uni(UV ord)
    PREINIT:
        U8 s[UTF8_MAXBYTES_CASE + 1];
        STRLEN len;
        AV *av;
        SV *utf8;
    CODE:
        av = newAV();
        av_push(av, newSVuv(toFOLD_uni(ord, s, &len)));

        utf8 = newSVpvn((char *) s, len);
        SvUTF8_on(utf8);
        av_push(av, utf8);

        av_push(av, newSVuv(len));
        RETVAL = av;
    OUTPUT:
        RETVAL

AV *
test_toFOLD_utf8(SV * p)
    PREINIT:
        U8 *input;
        U8 s[UTF8_MAXBYTES_CASE + 1];
        STRLEN len;
        AV *av;
        SV *utf8;
    CODE:
        input = (U8 *) SvPV(p, len);
        av = newAV();
        av_push(av, newSVuv(toFOLD_utf8(input, s, &len)));

        utf8 = newSVpvn((char *) s, len);
        SvUTF8_on(utf8);
        av_push(av, utf8);

        av_push(av, newSVuv(len));
        RETVAL = av;
    OUTPUT:
        RETVAL

UV
test_toUPPER(UV ord)
    CODE:
        RETVAL = toUPPER(ord);
    OUTPUT:
        RETVAL

UV
test_toUPPER_LC(UV ord)
    CODE:
        RETVAL = toUPPER_LC(ord);
    OUTPUT:
        RETVAL

AV *
test_toUPPER_uni(UV ord)
    PREINIT:
        U8 s[UTF8_MAXBYTES_CASE + 1];
        STRLEN len;
        AV *av;
        SV *utf8;
    CODE:
        av = newAV();
        av_push(av, newSVuv(toUPPER_uni(ord, s, &len)));

        utf8 = newSVpvn((char *) s, len);
        SvUTF8_on(utf8);
        av_push(av, utf8);

        av_push(av, newSVuv(len));
        RETVAL = av;
    OUTPUT:
        RETVAL

AV *
test_toUPPER_utf8(SV * p)
    PREINIT:
        U8 *input;
        U8 s[UTF8_MAXBYTES_CASE + 1];
        STRLEN len;
        AV *av;
        SV *utf8;
    CODE:
        input = (U8 *) SvPV(p, len);
        av = newAV();
        av_push(av, newSVuv(toUPPER_utf8(input, s, &len)));

        utf8 = newSVpvn((char *) s, len);
        SvUTF8_on(utf8);
        av_push(av, utf8);

        av_push(av, newSVuv(len));
        RETVAL = av;
    OUTPUT:
        RETVAL

UV
test_toTITLE(UV ord)
    CODE:
        RETVAL = toTITLE(ord);
    OUTPUT:
        RETVAL

AV *
test_toTITLE_uni(UV ord)
    PREINIT:
        U8 s[UTF8_MAXBYTES_CASE + 1];
        STRLEN len;
        AV *av;
        SV *utf8;
    CODE:
        av = newAV();
        av_push(av, newSVuv(toTITLE_uni(ord, s, &len)));

        utf8 = newSVpvn((char *) s, len);
        SvUTF8_on(utf8);
        av_push(av, utf8);

        av_push(av, newSVuv(len));
        RETVAL = av;
    OUTPUT:
        RETVAL

AV *
test_toTITLE_utf8(SV * p)
    PREINIT:
        U8 *input;
        U8 s[UTF8_MAXBYTES_CASE + 1];
        STRLEN len;
        AV *av;
        SV *utf8;
    CODE:
        input = (U8 *) SvPV(p, len);
        av = newAV();
        av_push(av, newSVuv(toTITLE_utf8(input, s, &len)));

        utf8 = newSVpvn((char *) s, len);
        SvUTF8_on(utf8);
        av_push(av, utf8);

        av_push(av, newSVuv(len));
        RETVAL = av;
    OUTPUT:
        RETVAL
