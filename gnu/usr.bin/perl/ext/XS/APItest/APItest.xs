#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"


MODULE = XS::APItest:Hash		PACKAGE = XS::APItest::Hash

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

SV *
delete(hash, key_sv)
	PREINIT:
	STRLEN len;
	const char *key;
	INPUT:
	HV *hash
	SV *key_sv
	CODE:
	key = SvPV(key_sv, len);
	/* It's already mortal, so need to increase reference count.  */
	RETVAL = SvREFCNT_inc(hv_delete(hash, key, UTF8KLEN(key_sv, len), 0));
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
	mPUSHn(-1);
	mPUSHn(2);
	mPUSHn(-3);
	XSRETURN(3);

void
mpushu()
	PPCODE:
	EXTEND(SP, 3);
	mPUSHn(1);
	mPUSHn(2);
	mPUSHn(3);
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
	mXPUSHn(-1);
	mXPUSHn(2);
	mXPUSHn(-3);
	XSRETURN(3);

void
mxpushu()
	PPCODE:
	mXPUSHn(1);
	mXPUSHn(2);
	mXPUSHn(3);
	XSRETURN(3);
