#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"


MODULE = Hash::Util		PACKAGE = Hash::Util


SV*
all_keys(hash,keys,placeholder)
	SV* hash
	SV* keys
	SV* placeholder
    PROTOTYPE: \%\@\@
    PREINIT:
	AV* av_k;
        AV* av_p;
        HV* hv;
        SV *key;
        HE *he;
    CODE:
	if (!SvROK(hash) || SvTYPE(SvRV(hash)) != SVt_PVHV)
	   croak("First argument to all_keys() must be an HASH reference");
	if (!SvROK(keys) || SvTYPE(SvRV(keys)) != SVt_PVAV)
	   croak("Second argument to all_keys() must be an ARRAY reference");
        if (!SvROK(placeholder) || SvTYPE(SvRV(placeholder)) != SVt_PVAV)
	   croak("Third argument to all_keys() must be an ARRAY reference");

	hv = (HV*)SvRV(hash);
	av_k = (AV*)SvRV(keys);
	av_p = (AV*)SvRV(placeholder);

        av_clear(av_k);
        av_clear(av_p);

        (void)hv_iterinit(hv);
	while((he = hv_iternext_flags(hv, HV_ITERNEXT_WANTPLACEHOLDERS))!= NULL) {
	    key=hv_iterkeysv(he);
            if (HeVAL(he) == &PL_sv_placeholder) {
                SvREFCNT_inc(key);
	        av_push(av_p, key);
            } else {
                SvREFCNT_inc(key);
	        av_push(av_k, key);
            }
        }
        RETVAL=hash;


void
hidden_ref_keys(hash)
	SV* hash
    PREINIT:
        HV* hv;
        SV *key;
        HE *he;
    PPCODE:
	if (!SvROK(hash) || SvTYPE(SvRV(hash)) != SVt_PVHV)
	   croak("First argument to hidden_keys() must be an HASH reference");

	hv = (HV*)SvRV(hash);

        (void)hv_iterinit(hv);
	while((he = hv_iternext_flags(hv, HV_ITERNEXT_WANTPLACEHOLDERS))!= NULL) {
	    key=hv_iterkeysv(he);
            if (HeVAL(he) == &PL_sv_placeholder) {
                XPUSHs( key );
            }
        }

void
legal_ref_keys(hash)
	SV* hash
    PREINIT:
        HV* hv;
        SV *key;
        HE *he;
    PPCODE:
	if (!SvROK(hash) || SvTYPE(SvRV(hash)) != SVt_PVHV)
	   croak("First argument to legal_keys() must be an HASH reference");

	hv = (HV*)SvRV(hash);

        (void)hv_iterinit(hv);
	while((he = hv_iternext_flags(hv, HV_ITERNEXT_WANTPLACEHOLDERS))!= NULL) {
	    key=hv_iterkeysv(he);
            XPUSHs( key );
        }

void
hv_store(hvref, key, val)
	SV* hvref
	SV* key
	SV* val
    PROTOTYPE: \%$$
    PREINIT:
	HV* hv;
    CODE:
    {
	if (!SvROK(hvref) || SvTYPE(SvRV(hvref)) != SVt_PVHV)
	   croak("First argument to hv_store() must be a hash reference");
	hv = (HV*)SvRV(hvref);
        SvREFCNT_inc(val);
	if (!hv_store_ent(hv, key, val, 0)) {
	    SvREFCNT_dec(val);
	    XSRETURN_NO;
	} else {
	    XSRETURN_YES;
	}
    }

