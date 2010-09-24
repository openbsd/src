#define PERL_IN_XS_APITEST
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"


/* for my_cxt tests */

#define MY_CXT_KEY "XS::APItest::_guts" XS_VERSION

typedef struct {
    int i;
    SV *sv;
} my_cxt_t;

START_MY_CXT

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

typedef void (freeent_function)(pTHX_ HV *, register HE *);

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
    hv_store(test_hash, "", 0, &PL_sv_yes, 0);
    hv_delete(test_hash, "", 0, 0);

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
    } while (++i < sizeof(results)/sizeof(results[0]));

    /* Goodbye to our extra reference.  */
    SvREFCNT_dec(test_scalar);
}


static I32
bitflip_key(pTHX_ IV action, SV *field) {
    MAGIC *mg = mg_find(field, PERL_MAGIC_uvar);
    SV *keysv;
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
		    UV chr = utf8_to_uvuni((U8 *)p, &len);
		    new_p = (char *)uvuni_to_utf8((U8 *)new_p, chr ^ 32);
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
    return 0;
}

STATIC MGVTBL rmagical_b = { 0 };

#include "const-c.inc"

MODULE = XS::APItest:Hash		PACKAGE = XS::APItest::Hash

INCLUDE: const-xs.inc

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
	RETVAL = (Perl_refcounted_he_fetch(aTHX_ PL_curcop->cop_hints_hash,
					   key, NULL, 0, 0, 0)
		  != &PL_sv_placeholder);
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
	RETVAL = Perl_refcounted_he_fetch(aTHX_ PL_curcop->cop_hints_hash, key,
					  NULL, 0, 0, 0);
	SvREFCNT_inc(RETVAL);
	OUTPUT:
	RETVAL
	
#endif
	
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

MODULE = XS::APItest		PACKAGE = XS::APItest

PROTOTYPES: DISABLE

BOOT:
{
    MY_CXT_INIT;
    MY_CXT.i  = 99;
    MY_CXT.sv = newSVpv("initial",0);
}                              

void
CLONE(...)
    CODE:
    MY_CXT_CLONE;
    MY_CXT.sv = newSVpv("initial_clone",0);

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

U32
pmflag (flag, before = 0)
	int flag
	U32 before
   CODE:
	pmflag(&before, flag);
	RETVAL = before;
    OUTPUT:
	RETVAL

void
my_exit(int exitcode)
        PPCODE:
        my_exit(exitcode);
