#define PERL_NO_GET_CONTEXT     /* we want efficiency */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* These are tightly coupled to the RXapif_* flags defined in regexp.h  */
#define UNDEF_FATAL  0x80000
#define DISCARD      0x40000
#define EXPECT_SHIFT 24
#define ACTION_MASK  0x000FF

#define FETCH_ALIAS  (RXapif_FETCH  | (2 << EXPECT_SHIFT))
#define STORE_ALIAS  (RXapif_STORE  | (3 << EXPECT_SHIFT) | UNDEF_FATAL | DISCARD)
#define DELETE_ALIAS (RXapif_DELETE | (2 << EXPECT_SHIFT) | UNDEF_FATAL)
#define CLEAR_ALIAS  (RXapif_CLEAR  | (1 << EXPECT_SHIFT) | UNDEF_FATAL | DISCARD)
#define EXISTS_ALIAS (RXapif_EXISTS | (2 << EXPECT_SHIFT))
#define SCALAR_ALIAS (RXapif_SCALAR | (1 << EXPECT_SHIFT))

static void
tie_it(pTHX_ const char name, UV flag, HV *const stash)
{
    GV *const gv = gv_fetchpvn(&name, 1, GV_ADDMULTI|GV_NOTQUAL, SVt_PVHV);
    HV *const hv = GvHV(gv);
    SV *rv = newSV_type(SVt_RV);

    SvRV_set(rv, newSVuv(flag));
    SvROK_on(rv);
    sv_bless(rv, stash);

    sv_unmagic((SV *)hv, PERL_MAGIC_tied);
    sv_magic((SV *)hv, rv, PERL_MAGIC_tied, NULL, 0);
    SvREFCNT_dec(rv); /* As sv_magic increased it by one.  */
}

MODULE = Tie::Hash::NamedCapture	PACKAGE = Tie::Hash::NamedCapture
PROTOTYPES: DISABLE

BOOT:
	{
	    HV *const stash = GvSTASH(CvGV(cv));
	    tie_it(aTHX_ '-', RXapif_ALL, stash);
	    tie_it(aTHX_ '+', RXapif_ONE, stash);
	}

SV *
TIEHASH(package, ...)
	const char *package;
    PREINIT:
	UV flag = RXapif_ONE;
    CODE:
	mark += 2;
	while(mark < sp) {
	    STRLEN len;
	    const char *p = SvPV_const(*mark, len);
	    if(memEQs(p, len, "all"))
		flag = SvTRUE(mark[1]) ? RXapif_ALL : RXapif_ONE;
	    mark += 2;
	}
	RETVAL = newSV_type(SVt_RV);
	sv_setuv(newSVrv(RETVAL, package), flag);
    OUTPUT:
	RETVAL

void
FETCH(...)
    ALIAS:
	Tie::Hash::NamedCapture::FETCH  = FETCH_ALIAS
	Tie::Hash::NamedCapture::STORE  = STORE_ALIAS
	Tie::Hash::NamedCapture::DELETE = DELETE_ALIAS
	Tie::Hash::NamedCapture::CLEAR  = CLEAR_ALIAS
	Tie::Hash::NamedCapture::EXISTS = EXISTS_ALIAS
	Tie::Hash::NamedCapture::SCALAR = SCALAR_ALIAS
    PREINIT:
	REGEXP *const rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;
	U32 flags;
	SV *ret;
	const U32 action = ix & ACTION_MASK;
	const int expect = ix >> EXPECT_SHIFT;
    PPCODE:
	if (items != expect)
	    croak_xs_usage(cv, expect == 2 ? "$key"
				           : (expect == 3 ? "$key, $value"
							  : ""));

	if (!rx || !SvROK(ST(0))) {
	    if (ix & UNDEF_FATAL)
		Perl_croak_no_modify();
	    else
		XSRETURN_UNDEF;
	}

	flags = (U32)SvUV(SvRV(MUTABLE_SV(ST(0))));

	PUTBACK;
	ret = RX_ENGINE(rx)->named_buff(aTHX_ (rx), expect >= 2 ? ST(1) : NULL,
				    expect >= 3 ? ST(2) : NULL, flags | action);
	SPAGAIN;

	if (ix & DISCARD) {
	    /* Called with G_DISCARD, so our return stack state is thrown away.
	       Hence if we were returned anything, free it immediately.  */
	    SvREFCNT_dec(ret);
	} else {
	    PUSHs(ret ? sv_2mortal(ret) : &PL_sv_undef);
	}

void
FIRSTKEY(...)
    ALIAS:
	Tie::Hash::NamedCapture::NEXTKEY = 1
    PREINIT:
	REGEXP *const rx = PL_curpm ? PM_GETRE(PL_curpm) : NULL;
	U32 flags;
	SV *ret;
	const int expect = ix ? 2 : 1;
	const U32 action = ix ? RXapif_NEXTKEY : RXapif_FIRSTKEY;
    PPCODE:
	if (items != expect)
	    croak_xs_usage(cv, expect == 2 ? "$lastkey" : "");

	if (!rx || !SvROK(ST(0)))
	    XSRETURN_UNDEF;

	flags = (U32)SvUV(SvRV(MUTABLE_SV(ST(0))));

	PUTBACK;
	ret = RX_ENGINE(rx)->named_buff_iter(aTHX_ (rx),
					     expect >= 2 ? ST(1) : NULL,
					     flags | action);
	SPAGAIN;

	PUSHs(ret ? sv_2mortal(ret) : &PL_sv_undef);

void
flags(...)
    PPCODE:
	EXTEND(SP, 2);
	mPUSHu(RXapif_ONE);
	mPUSHu(RXapif_ALL);
