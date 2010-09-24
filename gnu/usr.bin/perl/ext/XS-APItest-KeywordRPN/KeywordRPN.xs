#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define sv_is_glob(sv) (SvTYPE(sv) == SVt_PVGV)
#define sv_is_regexp(sv) (SvTYPE(sv) == SVt_REGEXP)
#define sv_is_string(sv) \
	(!sv_is_glob(sv) && !sv_is_regexp(sv) && \
	 (SvFLAGS(sv) & (SVf_IOK|SVf_NOK|SVf_POK|SVp_IOK|SVp_NOK|SVp_POK)))

static SV *hintkey_rpn_sv, *hintkey_calcrpn_sv, *hintkey_stufftest_sv;
static int (*next_keyword_plugin)(pTHX_ char *, STRLEN, OP **);

/* low-level parser helpers */

#define PL_bufptr (PL_parser->bufptr)
#define PL_bufend (PL_parser->bufend)

/* RPN parser */

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
	{
		/* because pad_findmy() doesn't really use length yet */
		SV *namesv = sv_2mortal(newSVpvn(start, s-start));
		varpos = pad_findmy(SvPVX(namesv), s-start, 0);
	}
	if(varpos == NOT_IN_PAD || PAD_COMPNAME_FLAGS_isOUR(varpos))
		croak("RPN only supports \"my\" variables");
	padop = newOP(OP_PADSV, 0);
	padop->op_targ = varpos;
	return padop;
}
#define parse_var() THX_parse_var(aTHX)

#define push_rpn_item(o) \
	(tmpop = (o), tmpop->op_sibling = stack, stack = tmpop)
#define pop_rpn_item() \
	(!stack ? (croak("RPN stack underflow"), (OP*)NULL) : \
	 (tmpop = stack, stack = stack->op_sibling, \
	  tmpop->op_sibling = NULL, tmpop))

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
				if(stack)
					croak("RPN expression must return "
						"a single value");
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
				push_rpn_item(newSVOP(OP_CONST, 0,
					newSVuv(val)));
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
#define parse_rpn_expr() THX_parse_rpn_expr(aTHX)

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
#define parse_keyword_rpn() THX_parse_keyword_rpn(aTHX)

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
#define parse_keyword_calcrpn() THX_parse_keyword_calcrpn(aTHX)

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
	if(do_stuff) lex_stuff_pvn(" ", 1, 0);
	return newOP(OP_NULL, 0);
}
#define parse_keyword_stufftest() THX_parse_keyword_stufftest(aTHX)

/* plugin glue */

static int THX_keyword_active(pTHX_ SV *hintkey_sv)
{
	HE *he;
	if(!GvHV(PL_hintgv)) return 0;
	he = hv_fetch_ent(GvHV(PL_hintgv), hintkey_sv, 0,
				SvSHARED_HASH(hintkey_sv));
	return he && SvTRUE(HeVAL(he));
}
#define keyword_active(hintkey_sv) THX_keyword_active(aTHX_ hintkey_sv)

static void THX_keyword_enable(pTHX_ SV *hintkey_sv)
{
	SV *val_sv = newSViv(1);
	HE *he;
	PL_hints |= HINT_LOCALIZE_HH;
	gv_HVadd(PL_hintgv);
	he = hv_store_ent(GvHV(PL_hintgv),
		hintkey_sv, val_sv, SvSHARED_HASH(hintkey_sv));
	if(he) {
		SV *val = HeVAL(he);
		SvSETMAGIC(val);
	} else {
		SvREFCNT_dec(val_sv);
	}
}
#define keyword_enable(hintkey_sv) THX_keyword_enable(aTHX_ hintkey_sv)

static void THX_keyword_disable(pTHX_ SV *hintkey_sv)
{
	if(GvHV(PL_hintgv)) {
		PL_hints |= HINT_LOCALIZE_HH;
		hv_delete_ent(GvHV(PL_hintgv),
			hintkey_sv, G_DISCARD, SvSHARED_HASH(hintkey_sv));
	}
}
#define keyword_disable(hintkey_sv) THX_keyword_disable(aTHX_ hintkey_sv)

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
	} else {
		return next_keyword_plugin(aTHX_
				keyword_ptr, keyword_len, op_ptr);
	}
}

MODULE = XS::APItest::KeywordRPN PACKAGE = XS::APItest::KeywordRPN

BOOT:
	hintkey_rpn_sv = newSVpvs_share("XS::APItest::KeywordRPN/rpn");
	hintkey_calcrpn_sv = newSVpvs_share("XS::APItest::KeywordRPN/calcrpn");
	hintkey_stufftest_sv =
		newSVpvs_share("XS::APItest::KeywordRPN/stufftest");
	next_keyword_plugin = PL_keyword_plugin;
	PL_keyword_plugin = my_keyword_plugin;

void
import(SV *classname, ...)
PREINIT:
	int i;
PPCODE:
	for(i = 1; i != items; i++) {
		SV *item = ST(i);
		if(sv_is_string(item) && strEQ(SvPVX(item), "rpn")) {
			keyword_enable(hintkey_rpn_sv);
		} else if(sv_is_string(item) && strEQ(SvPVX(item), "calcrpn")) {
			keyword_enable(hintkey_calcrpn_sv);
		} else if(sv_is_string(item) &&
				strEQ(SvPVX(item), "stufftest")) {
			keyword_enable(hintkey_stufftest_sv);
		} else {
			croak("\"%s\" is not exported by the %s module",
				SvPV_nolen(item), SvPV_nolen(ST(0)));
		}
	}

void
unimport(SV *classname, ...)
PREINIT:
	int i;
PPCODE:
	for(i = 1; i != items; i++) {
		SV *item = ST(i);
		if(sv_is_string(item) && strEQ(SvPVX(item), "rpn")) {
			keyword_disable(hintkey_rpn_sv);
		} else if(sv_is_string(item) && strEQ(SvPVX(item), "calcrpn")) {
			keyword_disable(hintkey_calcrpn_sv);
		} else if(sv_is_string(item) &&
				strEQ(SvPVX(item), "stufftest")) {
			keyword_disable(hintkey_stufftest_sv);
		} else {
			croak("\"%s\" is not exported by the %s module",
				SvPV_nolen(item), SvPV_nolen(ST(0)));
		}
	}
