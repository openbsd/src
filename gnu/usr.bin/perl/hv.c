/*    hv.c
 *
 *    Copyright (c) 1991-2002, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "I sit beside the fire and think of all that I have seen."  --Bilbo
 */

/* 
=head1 Hash Manipulation Functions
*/

#include "EXTERN.h"
#define PERL_IN_HV_C
#include "perl.h"

STATIC HE*
S_new_he(pTHX)
{
    HE* he;
    LOCK_SV_MUTEX;
    if (!PL_he_root)
	more_he();
    he = PL_he_root;
    PL_he_root = HeNEXT(he);
    UNLOCK_SV_MUTEX;
    return he;
}

STATIC void
S_del_he(pTHX_ HE *p)
{
    LOCK_SV_MUTEX;
    HeNEXT(p) = (HE*)PL_he_root;
    PL_he_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_he(pTHX)
{
    register HE* he;
    register HE* heend;
    XPV *ptr;
    New(54, ptr, 1008/sizeof(XPV), XPV);
    ptr->xpv_pv = (char*)PL_he_arenaroot;
    PL_he_arenaroot = ptr;

    he = (HE*)ptr;
    heend = &he[1008 / sizeof(HE) - 1];
    PL_he_root = ++he;
    while (he < heend) {
	HeNEXT(he) = (HE*)(he + 1);
	he++;
    }
    HeNEXT(he) = 0;
}

#ifdef PURIFY

#define new_HE() (HE*)safemalloc(sizeof(HE))
#define del_HE(p) safefree((char*)p)

#else

#define new_HE() new_he()
#define del_HE(p) del_he(p)

#endif

STATIC HEK *
S_save_hek_flags(pTHX_ const char *str, I32 len, U32 hash, int flags)
{
    char *k;
    register HEK *hek;

    New(54, k, HEK_BASESIZE + len + 2, char);
    hek = (HEK*)k;
    Copy(str, HEK_KEY(hek), len, char);
    HEK_KEY(hek)[len] = 0;
    HEK_LEN(hek) = len;
    HEK_HASH(hek) = hash;
    HEK_FLAGS(hek) = (unsigned char)flags;
    return hek;
}

#if defined(USE_ITHREADS)
HE *
Perl_he_dup(pTHX_ HE *e, bool shared, CLONE_PARAMS* param)
{
    HE *ret;

    if (!e)
	return Nullhe;
    /* look for it in the table first */
    ret = (HE*)ptr_table_fetch(PL_ptr_table, e);
    if (ret)
	return ret;

    /* create anew and remember what it is */
    ret = new_HE();
    ptr_table_store(PL_ptr_table, e, ret);

    HeNEXT(ret) = he_dup(HeNEXT(e),shared, param);
    if (HeKLEN(e) == HEf_SVKEY)
	HeKEY_sv(ret) = SvREFCNT_inc(sv_dup(HeKEY_sv(e), param));
    else if (shared)
	HeKEY_hek(ret) = share_hek_flags(HeKEY(e), HeKLEN(e), HeHASH(e),
                                         HeKFLAGS(e));
    else
	HeKEY_hek(ret) = save_hek_flags(HeKEY(e), HeKLEN(e), HeHASH(e),
                                        HeKFLAGS(e));
    HeVAL(ret) = SvREFCNT_inc(sv_dup(HeVAL(e), param));
    return ret;
}
#endif	/* USE_ITHREADS */

static void
S_hv_notallowed(pTHX_ int flags, const char *key, I32 klen,
		const char *msg)
{
    SV *sv = sv_newmortal(), *esv = sv_newmortal();
    if (!(flags & HVhek_FREEKEY)) {
	sv_setpvn(sv, key, klen);
    }
    else {
	/* Need to free saved eventually assign to mortal SV */
	SV *sv = sv_newmortal();
	sv_usepvn(sv, (char *) key, klen);
    }
    if (flags & HVhek_UTF8) {
	SvUTF8_on(sv);
    }
    Perl_sv_setpvf(aTHX_ esv, "Attempt to %s a restricted hash", msg);
    Perl_croak(aTHX_ SvPVX(esv), sv);
}

/* (klen == HEf_SVKEY) is special for MAGICAL hv entries, meaning key slot
 * contains an SV* */

/*
=for apidoc hv_fetch

Returns the SV which corresponds to the specified key in the hash.  The
C<klen> is the length of the key.  If C<lval> is set then the fetch will be
part of a store.  Check that the return value is non-null before
dereferencing it to an C<SV*>.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for more
information on how to use this function on tied hashes.

=cut
*/


SV**
Perl_hv_fetch(pTHX_ HV *hv, const char *key, I32 klen, I32 lval)
{
    bool is_utf8 = FALSE;
    const char *keysave = key;
    int flags = 0;

    if (klen < 0) {
      klen = -klen;
      is_utf8 = TRUE;
    }

    if (is_utf8) {
	STRLEN tmplen = klen;
	/* Just casting the &klen to (STRLEN) won't work well
	 * if STRLEN and I32 are of different widths. --jhi */
	key = (char*)bytes_from_utf8((U8*)key, &tmplen, &is_utf8);
	klen = tmplen;
        /* If we were able to downgrade here, then than means that we were
           passed in a key which only had chars 0-255, but was utf8 encoded.  */
        if (is_utf8)
            flags = HVhek_UTF8;
        /* If we found we were able to downgrade the string to bytes, then
           we should flag that it needs upgrading on keys or each.  */
        if (key != keysave)
            flags |= HVhek_WASUTF8 | HVhek_FREEKEY;
    }

    return hv_fetch_flags (hv, key, klen, lval, flags);
}

STATIC SV**
S_hv_fetch_flags(pTHX_ HV *hv, const char *key, I32 klen, I32 lval, int flags)
{
    register XPVHV* xhv;
    register U32 hash;
    register HE *entry;
    SV *sv;

    if (!hv)
	return 0;

    if (SvRMAGICAL(hv)) {
        /* All this clause seems to be utf8 unaware.
           By moving the utf8 stuff out to hv_fetch_flags I need to ensure
           key doesn't leak. I've not tried solving the utf8-ness.
           NWC.
        */
	if (mg_find((SV*)hv, PERL_MAGIC_tied) || SvGMAGICAL((SV*)hv)) {
	    sv = sv_newmortal();
	    mg_copy((SV*)hv, sv, key, klen);
            if (flags & HVhek_FREEKEY)
                Safefree(key);
	    PL_hv_fetch_sv = sv;
	    return &PL_hv_fetch_sv;
	}
#ifdef ENV_IS_CASELESS
	else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
	    I32 i;
	    for (i = 0; i < klen; ++i)
		if (isLOWER(key[i])) {
		    char *nkey = strupr(SvPVX(sv_2mortal(newSVpvn(key,klen))));
		    SV **ret = hv_fetch(hv, nkey, klen, 0);
		    if (!ret && lval) {
			ret = hv_store_flags(hv, key, klen, NEWSV(61,0), 0,
                                             flags);
                    } else if (flags & HVhek_FREEKEY)
                        Safefree(key);
		    return ret;
		}
	}
#endif
    }

    /* We use xhv->xhv_foo fields directly instead of HvFOO(hv) to
       avoid unnecessary pointer dereferencing. */
    xhv = (XPVHV*)SvANY(hv);
    if (!xhv->xhv_array /* !HvARRAY(hv) */) {
	if (lval
#ifdef DYNAMIC_ENV_FETCH  /* if it's an %ENV lookup, we may get it on the fly */
		 || (SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env))
#endif
								  )
	    Newz(503, xhv->xhv_array /* HvARRAY(hv) */,
		 PERL_HV_ARRAY_ALLOC_BYTES(xhv->xhv_max+1 /* HvMAX(hv)+1 */),
		 char);
	else {
            if (flags & HVhek_FREEKEY)
                Safefree(key);
	    return 0;
        }
    }

    PERL_HASH(hash, key, klen);

    /* entry = (HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    entry = ((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    for (; entry; entry = HeNEXT(entry)) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
        /* flags is 0 if not utf8. need HeKFLAGS(entry) also 0.
           flags is 1 if utf8. need HeKFLAGS(entry) also 1.
           xor is true if bits differ, in which case this isn't a match.  */
	if ((HeKFLAGS(entry) ^ flags) & HVhek_UTF8)
	    continue;
        if (lval && HeKFLAGS(entry) != flags) {
            /* We match if HVhek_UTF8 bit in our flags and hash key's match.
               But if entry was set previously with HVhek_WASUTF8 and key now
               doesn't (or vice versa) then we should change the key's flag,
               as this is assignment.  */
            if (HvSHAREKEYS(hv)) {
                /* Need to swap the key we have for a key with the flags we
                   need. As keys are shared we can't just write to the flag,
                   so we share the new one, unshare the old one.  */
                int flags_nofree = flags & ~HVhek_FREEKEY;
                HEK *new_hek = share_hek_flags(key, klen, hash, flags_nofree);
                unshare_hek (HeKEY_hek(entry));
                HeKEY_hek(entry) = new_hek;
            }
            else
                HeKFLAGS(entry) = flags;
        }
        if (flags & HVhek_FREEKEY)
            Safefree(key);
	/* if we find a placeholder, we pretend we haven't found anything */
	if (HeVAL(entry) == &PL_sv_undef)
	    break;
	return &HeVAL(entry);

    }
#ifdef DYNAMIC_ENV_FETCH  /* %ENV lookup?  If so, try to fetch the value now */
    if (SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env)) {
	unsigned long len;
	char *env = PerlEnv_ENVgetenv_len(key,&len);
	if (env) {
	    sv = newSVpvn(env,len);
	    SvTAINTED_on(sv);
	    if (flags & HVhek_FREEKEY)
		Safefree(key);
	    return hv_store(hv,key,klen,sv,hash);
	}
    }
#endif
    if (!entry && SvREADONLY(hv)) {
	S_hv_notallowed(aTHX_ flags, key, klen,
			"access disallowed key '%"SVf"' in"
			);
    }
    if (lval) {		/* gonna assign to this, so it better be there */
	sv = NEWSV(61,0);
        return hv_store_flags(hv,key,klen,sv,hash,flags);
    }
    if (flags & HVhek_FREEKEY)
        Safefree(key);
    return 0;
}

/* returns an HE * structure with the all fields set */
/* note that hent_val will be a mortal sv for MAGICAL hashes */
/*
=for apidoc hv_fetch_ent

Returns the hash entry which corresponds to the specified key in the hash.
C<hash> must be a valid precomputed hash number for the given C<key>, or 0
if you want the function to compute it.  IF C<lval> is set then the fetch
will be part of a store.  Make sure the return value is non-null before
accessing it.  The return value when C<tb> is a tied hash is a pointer to a
static location, so be sure to make a copy of the structure if you need to
store it somewhere.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for more
information on how to use this function on tied hashes.

=cut
*/

HE *
Perl_hv_fetch_ent(pTHX_ HV *hv, SV *keysv, I32 lval, register U32 hash)
{
    register XPVHV* xhv;
    register char *key;
    STRLEN klen;
    register HE *entry;
    SV *sv;
    bool is_utf8;
    int flags = 0;
    char *keysave;

    if (!hv)
	return 0;

    if (SvRMAGICAL(hv)) {
	if (mg_find((SV*)hv, PERL_MAGIC_tied) || SvGMAGICAL((SV*)hv)) {
	    sv = sv_newmortal();
	    keysv = sv_2mortal(newSVsv(keysv));
	    mg_copy((SV*)hv, sv, (char*)keysv, HEf_SVKEY);
	    if (!HeKEY_hek(&PL_hv_fetch_ent_mh)) {
		char *k;
		New(54, k, HEK_BASESIZE + sizeof(SV*), char);
		HeKEY_hek(&PL_hv_fetch_ent_mh) = (HEK*)k;
	    }
	    HeSVKEY_set(&PL_hv_fetch_ent_mh, keysv);
	    HeVAL(&PL_hv_fetch_ent_mh) = sv;
	    return &PL_hv_fetch_ent_mh;
	}
#ifdef ENV_IS_CASELESS
	else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
	    U32 i;
	    key = SvPV(keysv, klen);
	    for (i = 0; i < klen; ++i)
		if (isLOWER(key[i])) {
		    SV *nkeysv = sv_2mortal(newSVpvn(key,klen));
		    (void)strupr(SvPVX(nkeysv));
		    entry = hv_fetch_ent(hv, nkeysv, 0, 0);
		    if (!entry && lval)
			entry = hv_store_ent(hv, keysv, NEWSV(61,0), hash);
		    return entry;
		}
	}
#endif
    }

    xhv = (XPVHV*)SvANY(hv);
    if (!xhv->xhv_array /* !HvARRAY(hv) */) {
	if (lval
#ifdef DYNAMIC_ENV_FETCH  /* if it's an %ENV lookup, we may get it on the fly */
		 || (SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env))
#endif
								  )
	    Newz(503, xhv->xhv_array /* HvARRAY(hv) */,
		 PERL_HV_ARRAY_ALLOC_BYTES(xhv->xhv_max+1 /* HvMAX(hv)+1 */),
		 char);
	else
	    return 0;
    }

    keysave = key = SvPV(keysv, klen);
    is_utf8 = (SvUTF8(keysv)!=0);

    if (is_utf8) {
	key = (char*)bytes_from_utf8((U8*)key, &klen, &is_utf8);
        if (is_utf8)
            flags = HVhek_UTF8;
        if (key != keysave)
            flags |= HVhek_WASUTF8 | HVhek_FREEKEY;
    }

    if (!hash)
	PERL_HASH(hash, key, klen);

    /* entry = (HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    entry = ((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    for (; entry; entry = HeNEXT(entry)) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ flags) & HVhek_UTF8)
	    continue;
        if (lval && HeKFLAGS(entry) != flags) {
            /* We match if HVhek_UTF8 bit in our flags and hash key's match.
               But if entry was set previously with HVhek_WASUTF8 and key now
               doesn't (or vice versa) then we should change the key's flag,
               as this is assignment.  */
            if (HvSHAREKEYS(hv)) {
                /* Need to swap the key we have for a key with the flags we
                   need. As keys are shared we can't just write to the flag,
                   so we share the new one, unshare the old one.  */
                int flags_nofree = flags & ~HVhek_FREEKEY;
                HEK *new_hek = share_hek_flags(key, klen, hash, flags_nofree);
                unshare_hek (HeKEY_hek(entry));
                HeKEY_hek(entry) = new_hek;
            }
            else
                HeKFLAGS(entry) = flags;
        }
	if (key != keysave)
	    Safefree(key);
	/* if we find a placeholder, we pretend we haven't found anything */
	if (HeVAL(entry) == &PL_sv_undef)
	    break;
	return entry;
    }
#ifdef DYNAMIC_ENV_FETCH  /* %ENV lookup?  If so, try to fetch the value now */
    if (SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env)) {
	unsigned long len;
	char *env = PerlEnv_ENVgetenv_len(key,&len);
	if (env) {
	    sv = newSVpvn(env,len);
	    SvTAINTED_on(sv);
	    return hv_store_ent(hv,keysv,sv,hash);
	}
    }
#endif
    if (!entry && SvREADONLY(hv)) {
	S_hv_notallowed(aTHX_ flags, key, klen,
			"access disallowed key '%"SVf"' in"
			);
    }
    if (flags & HVhek_FREEKEY)
	Safefree(key);
    if (lval) {		/* gonna assign to this, so it better be there */
	sv = NEWSV(61,0);
	return hv_store_ent(hv,keysv,sv,hash);
    }
    return 0;
}

STATIC void
S_hv_magic_check(pTHX_ HV *hv, bool *needs_copy, bool *needs_store)
{
    MAGIC *mg = SvMAGIC(hv);
    *needs_copy = FALSE;
    *needs_store = TRUE;
    while (mg) {
	if (isUPPER(mg->mg_type)) {
	    *needs_copy = TRUE;
	    switch (mg->mg_type) {
	    case PERL_MAGIC_tied:
	    case PERL_MAGIC_sig:
		*needs_store = FALSE;
	    }
	}
	mg = mg->mg_moremagic;
    }
}

/*
=for apidoc hv_store

Stores an SV in a hash.  The hash key is specified as C<key> and C<klen> is
the length of the key.  The C<hash> parameter is the precomputed hash
value; if it is zero then Perl will compute it.  The return value will be
NULL if the operation failed or if the value did not need to be actually
stored within the hash (as in the case of tied hashes).  Otherwise it can
be dereferenced to get the original C<SV*>.  Note that the caller is
responsible for suitably incrementing the reference count of C<val> before
the call, and decrementing it if the function returned NULL.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for more
information on how to use this function on tied hashes.

=cut
*/

SV**
Perl_hv_store(pTHX_ HV *hv, const char *key, I32 klen, SV *val, U32 hash)
{
    bool is_utf8 = FALSE;
    const char *keysave = key;
    int flags = 0;

    if (klen < 0) {
      klen = -klen;
      is_utf8 = TRUE;
    }

    if (is_utf8) {
	STRLEN tmplen = klen;
	/* Just casting the &klen to (STRLEN) won't work well
	 * if STRLEN and I32 are of different widths. --jhi */
	key = (char*)bytes_from_utf8((U8*)key, &tmplen, &is_utf8);
	klen = tmplen;
        /* If we were able to downgrade here, then than means that we were
           passed in a key which only had chars 0-255, but was utf8 encoded.  */
        if (is_utf8)
            flags = HVhek_UTF8;
        /* If we found we were able to downgrade the string to bytes, then
           we should flag that it needs upgrading on keys or each.  */
        if (key != keysave)
            flags |= HVhek_WASUTF8 | HVhek_FREEKEY;
    }

    return hv_store_flags (hv, key, klen, val, hash, flags);
}

SV**
Perl_hv_store_flags(pTHX_ HV *hv, const char *key, I32 klen, SV *val,
                 register U32 hash, int flags)
{
    register XPVHV* xhv;
    register I32 i;
    register HE *entry;
    register HE **oentry;

    if (!hv)
	return 0;

    xhv = (XPVHV*)SvANY(hv);
    if (SvMAGICAL(hv)) {
	bool needs_copy;
	bool needs_store;
	hv_magic_check (hv, &needs_copy, &needs_store);
	if (needs_copy) {
	    mg_copy((SV*)hv, val, key, klen);
	    if (!xhv->xhv_array /* !HvARRAY */ && !needs_store) {
                if (flags & HVhek_FREEKEY)
                    Safefree(key);
		return 0;
            }
#ifdef ENV_IS_CASELESS
	    else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
		key = savepvn(key,klen);
		key = (const char*)strupr((char*)key);
		hash = 0;
	    }
#endif
	}
    }

    if (flags)
        HvHASKFLAGS_on((SV*)hv);

    if (!hash)
	PERL_HASH(hash, key, klen);

    if (!xhv->xhv_array /* !HvARRAY(hv) */)
	Newz(505, xhv->xhv_array /* HvARRAY(hv) */,
	     PERL_HV_ARRAY_ALLOC_BYTES(xhv->xhv_max+1 /* HvMAX(hv)+1 */),
	     char);

    /* oentry = &(HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    i = 1;

    for (entry = *oentry; entry; i=0, entry = HeNEXT(entry)) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ flags) & HVhek_UTF8)
	    continue;
	if (HeVAL(entry) == &PL_sv_undef)
	    xhv->xhv_placeholders--; /* yes, can store into placeholder slot */
	else
	    SvREFCNT_dec(HeVAL(entry));
        if (flags & HVhek_PLACEHOLD) {
            /* We have been requested to insert a placeholder. Currently
               only Storable is allowed to do this.  */
            xhv->xhv_placeholders++;
            HeVAL(entry) = &PL_sv_undef;
        } else
            HeVAL(entry) = val;

        if (HeKFLAGS(entry) != flags) {
            /* We match if HVhek_UTF8 bit in our flags and hash key's match.
               But if entry was set previously with HVhek_WASUTF8 and key now
               doesn't (or vice versa) then we should change the key's flag,
               as this is assignment.  */
            if (HvSHAREKEYS(hv)) {
                /* Need to swap the key we have for a key with the flags we
                   need. As keys are shared we can't just write to the flag,
                   so we share the new one, unshare the old one.  */
                int flags_nofree = flags & ~HVhek_FREEKEY;
                HEK *new_hek = share_hek_flags(key, klen, hash, flags_nofree);
                unshare_hek (HeKEY_hek(entry));
                HeKEY_hek(entry) = new_hek;
            }
            else
                HeKFLAGS(entry) = flags;
        }
        if (flags & HVhek_FREEKEY)
            Safefree(key);
	return &HeVAL(entry);
    }

    if (SvREADONLY(hv)) {
	S_hv_notallowed(aTHX_ flags, key, klen,
			"access disallowed key '%"SVf"' to"
			);
    }

    entry = new_HE();
    /* share_hek_flags will do the free for us.  This might be considered
       bad API design.  */
    if (HvSHAREKEYS(hv))
	HeKEY_hek(entry) = share_hek_flags(key, klen, hash, flags);
    else                                       /* gotta do the real thing */
	HeKEY_hek(entry) = save_hek_flags(key, klen, hash, flags);
    if (flags & HVhek_PLACEHOLD) {
        /* We have been requested to insert a placeholder. Currently
           only Storable is allowed to do this.  */
        xhv->xhv_placeholders++;
        HeVAL(entry) = &PL_sv_undef;
    } else
        HeVAL(entry) = val;
    HeNEXT(entry) = *oentry;
    *oentry = entry;

    xhv->xhv_keys++; /* HvKEYS(hv)++ */
    if (i) {				/* initial entry? */
	xhv->xhv_fill++; /* HvFILL(hv)++ */
	if (xhv->xhv_keys > (IV)xhv->xhv_max /* HvKEYS(hv) > HvMAX(hv) */)
	    hsplit(hv);
    }

    return &HeVAL(entry);
}

/*
=for apidoc hv_store_ent

Stores C<val> in a hash.  The hash key is specified as C<key>.  The C<hash>
parameter is the precomputed hash value; if it is zero then Perl will
compute it.  The return value is the new hash entry so created.  It will be
NULL if the operation failed or if the value did not need to be actually
stored within the hash (as in the case of tied hashes).  Otherwise the
contents of the return value can be accessed using the C<He?> macros
described here.  Note that the caller is responsible for suitably
incrementing the reference count of C<val> before the call, and
decrementing it if the function returned NULL.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for more
information on how to use this function on tied hashes.

=cut
*/

HE *
Perl_hv_store_ent(pTHX_ HV *hv, SV *keysv, SV *val, U32 hash)
{
    XPVHV* xhv;
    char *key;
    STRLEN klen;
    I32 i;
    HE *entry;
    HE **oentry;
    bool is_utf8;
    int flags = 0;
    char *keysave;

    if (!hv)
	return 0;

    xhv = (XPVHV*)SvANY(hv);
    if (SvMAGICAL(hv)) {
	bool needs_copy;
	bool needs_store;
	hv_magic_check (hv, &needs_copy, &needs_store);
	if (needs_copy) {
	    bool save_taint = PL_tainted;
	    if (PL_tainting)
		PL_tainted = SvTAINTED(keysv);
	    keysv = sv_2mortal(newSVsv(keysv));
	    mg_copy((SV*)hv, val, (char*)keysv, HEf_SVKEY);
	    TAINT_IF(save_taint);
	    if (!xhv->xhv_array /* !HvARRAY(hv) */ && !needs_store)
		return Nullhe;
#ifdef ENV_IS_CASELESS
	    else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
		key = SvPV(keysv, klen);
		keysv = sv_2mortal(newSVpvn(key,klen));
		(void)strupr(SvPVX(keysv));
		hash = 0;
	    }
#endif
	}
    }

    keysave = key = SvPV(keysv, klen);
    is_utf8 = (SvUTF8(keysv) != 0);

    if (is_utf8) {
	key = (char*)bytes_from_utf8((U8*)key, &klen, &is_utf8);
        if (is_utf8)
            flags = HVhek_UTF8;
        if (key != keysave)
            flags |= HVhek_WASUTF8 | HVhek_FREEKEY;
        HvHASKFLAGS_on((SV*)hv);
    }

    if (!hash)
	PERL_HASH(hash, key, klen);

    if (!xhv->xhv_array /* !HvARRAY(hv) */)
	Newz(505, xhv->xhv_array /* HvARRAY(hv) */,
	     PERL_HV_ARRAY_ALLOC_BYTES(xhv->xhv_max+1 /* HvMAX(hv)+1 */),
	     char);

    /* oentry = &(HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    i = 1;
    entry = *oentry;
    for (; entry; i=0, entry = HeNEXT(entry)) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ flags) & HVhek_UTF8)
	    continue;
	if (HeVAL(entry) == &PL_sv_undef)
	    xhv->xhv_placeholders--; /* yes, can store into placeholder slot */
	else
	    SvREFCNT_dec(HeVAL(entry));
	HeVAL(entry) = val;
        if (HeKFLAGS(entry) != flags) {
            /* We match if HVhek_UTF8 bit in our flags and hash key's match.
               But if entry was set previously with HVhek_WASUTF8 and key now
               doesn't (or vice versa) then we should change the key's flag,
               as this is assignment.  */
            if (HvSHAREKEYS(hv)) {
                /* Need to swap the key we have for a key with the flags we
                   need. As keys are shared we can't just write to the flag,
                   so we share the new one, unshare the old one.  */
                int flags_nofree = flags & ~HVhek_FREEKEY;
                HEK *new_hek = share_hek_flags(key, klen, hash, flags_nofree);
                unshare_hek (HeKEY_hek(entry));
                HeKEY_hek(entry) = new_hek;
            }
            else
                HeKFLAGS(entry) = flags;
        }
        if (flags & HVhek_FREEKEY)
	    Safefree(key);
	return entry;
    }

    if (SvREADONLY(hv)) {
	S_hv_notallowed(aTHX_ flags, key, klen,
			"access disallowed key '%"SVf"' to"
			);
    }

    entry = new_HE();
    /* share_hek_flags will do the free for us.  This might be considered
       bad API design.  */
    if (HvSHAREKEYS(hv))
	HeKEY_hek(entry) = share_hek_flags(key, klen, hash, flags);
    else                                       /* gotta do the real thing */
	HeKEY_hek(entry) = save_hek_flags(key, klen, hash, flags);
    HeVAL(entry) = val;
    HeNEXT(entry) = *oentry;
    *oentry = entry;

    xhv->xhv_keys++; /* HvKEYS(hv)++ */
    if (i) {				/* initial entry? */
	xhv->xhv_fill++; /* HvFILL(hv)++ */
	if (xhv->xhv_keys > (IV)xhv->xhv_max /* HvKEYS(hv) > HvMAX(hv) */)
	    hsplit(hv);
    }

    return entry;
}

/*
=for apidoc hv_delete

Deletes a key/value pair in the hash.  The value SV is removed from the
hash and returned to the caller.  The C<klen> is the length of the key.
The C<flags> value will normally be zero; if set to G_DISCARD then NULL
will be returned.

=cut
*/

SV *
Perl_hv_delete(pTHX_ HV *hv, const char *key, I32 klen, I32 flags)
{
    register XPVHV* xhv;
    register I32 i;
    register U32 hash;
    register HE *entry;
    register HE **oentry;
    SV **svp;
    SV *sv;
    bool is_utf8 = FALSE;
    int k_flags = 0;
    const char *keysave = key;

    if (!hv)
	return Nullsv;
    if (klen < 0) {
      klen = -klen;
      is_utf8 = TRUE;
    }
    if (SvRMAGICAL(hv)) {
	bool needs_copy;
	bool needs_store;
	hv_magic_check (hv, &needs_copy, &needs_store);

	if (needs_copy && (svp = hv_fetch(hv, key, klen, TRUE))) {
	    sv = *svp;
	    mg_clear(sv);
	    if (!needs_store) {
		if (mg_find(sv, PERL_MAGIC_tiedelem)) {
		    /* No longer an element */
		    sv_unmagic(sv, PERL_MAGIC_tiedelem);
		    return sv;
		}
		return Nullsv;          /* element cannot be deleted */
	    }
#ifdef ENV_IS_CASELESS
	    else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
		sv = sv_2mortal(newSVpvn(key,klen));
		key = strupr(SvPVX(sv));
	    }
#endif
	}
    }
    xhv = (XPVHV*)SvANY(hv);
    if (!xhv->xhv_array /* !HvARRAY(hv) */)
	return Nullsv;

    if (is_utf8) {
	STRLEN tmplen = klen;
	/* See the note in hv_fetch(). --jhi */
	key = (char*)bytes_from_utf8((U8*)key, &tmplen, &is_utf8);
	klen = tmplen;
        if (is_utf8)
            k_flags = HVhek_UTF8;
        if (key != keysave)
            k_flags |= HVhek_FREEKEY;
    }

    PERL_HASH(hash, key, klen);

    /* oentry = &(HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    entry = *oentry;
    i = 1;
    for (; entry; i=0, oentry = &HeNEXT(entry), entry = *oentry) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ k_flags) & HVhek_UTF8)
	    continue;
	if (k_flags & HVhek_FREEKEY)
	    Safefree(key);
	/* if placeholder is here, it's already been deleted.... */
	if (HeVAL(entry) == &PL_sv_undef)
	{
	    if (SvREADONLY(hv))
		return Nullsv;  /* if still SvREADONLY, leave it deleted. */
	    else {
		/* okay, really delete the placeholder... */
		*oentry = HeNEXT(entry);
		if (i && !*oentry)
		    xhv->xhv_fill--; /* HvFILL(hv)-- */
		if (entry == xhv->xhv_eiter /* HvEITER(hv) */)
		    HvLAZYDEL_on(hv);
		else
		    hv_free_ent(hv, entry);
		xhv->xhv_keys--; /* HvKEYS(hv)-- */
		if (xhv->xhv_keys == 0)
		    HvHASKFLAGS_off(hv);
		xhv->xhv_placeholders--;
		return Nullsv;
	    }
	}
	else if (SvREADONLY(hv) && HeVAL(entry) && SvREADONLY(HeVAL(entry))) {
	    S_hv_notallowed(aTHX_ k_flags, key, klen,
			    "delete readonly key '%"SVf"' from"
			    );
	}

	if (flags & G_DISCARD)
	    sv = Nullsv;
	else {
	    sv = sv_2mortal(HeVAL(entry));
	    HeVAL(entry) = &PL_sv_undef;
	}

	/*
	 * If a restricted hash, rather than really deleting the entry, put
	 * a placeholder there. This marks the key as being "approved", so
	 * we can still access via not-really-existing key without raising
	 * an error.
	 */
	if (SvREADONLY(hv)) {
	    HeVAL(entry) = &PL_sv_undef;
	    /* We'll be saving this slot, so the number of allocated keys
	     * doesn't go down, but the number placeholders goes up */
	    xhv->xhv_placeholders++; /* HvPLACEHOLDERS(hv)++ */
	} else {
	    *oentry = HeNEXT(entry);
	    if (i && !*oentry)
		xhv->xhv_fill--; /* HvFILL(hv)-- */
	    if (entry == xhv->xhv_eiter /* HvEITER(hv) */)
		HvLAZYDEL_on(hv);
	    else
		hv_free_ent(hv, entry);
	    xhv->xhv_keys--; /* HvKEYS(hv)-- */
	    if (xhv->xhv_keys == 0)
	        HvHASKFLAGS_off(hv);
	}
	return sv;
    }
    if (SvREADONLY(hv)) {
	S_hv_notallowed(aTHX_ k_flags, key, klen,
			"access disallowed key '%"SVf"' from"
			);
    }

    if (k_flags & HVhek_FREEKEY)
	Safefree(key);
    return Nullsv;
}

/*
=for apidoc hv_delete_ent

Deletes a key/value pair in the hash.  The value SV is removed from the
hash and returned to the caller.  The C<flags> value will normally be zero;
if set to G_DISCARD then NULL will be returned.  C<hash> can be a valid
precomputed hash value, or 0 to ask for it to be computed.

=cut
*/

SV *
Perl_hv_delete_ent(pTHX_ HV *hv, SV *keysv, I32 flags, U32 hash)
{
    register XPVHV* xhv;
    register I32 i;
    register char *key;
    STRLEN klen;
    register HE *entry;
    register HE **oentry;
    SV *sv;
    bool is_utf8;
    int k_flags = 0;
    char *keysave;

    if (!hv)
	return Nullsv;
    if (SvRMAGICAL(hv)) {
	bool needs_copy;
	bool needs_store;
	hv_magic_check (hv, &needs_copy, &needs_store);

	if (needs_copy && (entry = hv_fetch_ent(hv, keysv, TRUE, hash))) {
	    sv = HeVAL(entry);
	    mg_clear(sv);
	    if (!needs_store) {
		if (mg_find(sv, PERL_MAGIC_tiedelem)) {
		    /* No longer an element */
		    sv_unmagic(sv, PERL_MAGIC_tiedelem);
		    return sv;
		}		
		return Nullsv;		/* element cannot be deleted */
	    }
#ifdef ENV_IS_CASELESS
	    else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
		key = SvPV(keysv, klen);
		keysv = sv_2mortal(newSVpvn(key,klen));
		(void)strupr(SvPVX(keysv));
		hash = 0;
	    }
#endif
	}
    }
    xhv = (XPVHV*)SvANY(hv);
    if (!xhv->xhv_array /* !HvARRAY(hv) */)
	return Nullsv;

    keysave = key = SvPV(keysv, klen);
    is_utf8 = (SvUTF8(keysv) != 0);

    if (is_utf8) {
	key = (char*)bytes_from_utf8((U8*)key, &klen, &is_utf8);
        if (is_utf8)
            k_flags = HVhek_UTF8;
        if (key != keysave)
            k_flags |= HVhek_FREEKEY;
    }

    if (!hash)
	PERL_HASH(hash, key, klen);

    /* oentry = &(HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    entry = *oentry;
    i = 1;
    for (; entry; i=0, oentry = &HeNEXT(entry), entry = *oentry) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ k_flags) & HVhek_UTF8)
	    continue;
        if (k_flags & HVhek_FREEKEY)
            Safefree(key);

	/* if placeholder is here, it's already been deleted.... */
	if (HeVAL(entry) == &PL_sv_undef)
	{
	    if (SvREADONLY(hv))
		return Nullsv; /* if still SvREADONLY, leave it deleted. */

           /* okay, really delete the placeholder. */
           *oentry = HeNEXT(entry);
           if (i && !*oentry)
               xhv->xhv_fill--; /* HvFILL(hv)-- */
           if (entry == xhv->xhv_eiter /* HvEITER(hv) */)
               HvLAZYDEL_on(hv);
           else
               hv_free_ent(hv, entry);
           xhv->xhv_keys--; /* HvKEYS(hv)-- */
	   if (xhv->xhv_keys == 0)
               HvHASKFLAGS_off(hv);
           xhv->xhv_placeholders--;
           return Nullsv;
	}
	else if (SvREADONLY(hv) && HeVAL(entry) && SvREADONLY(HeVAL(entry))) {
	    S_hv_notallowed(aTHX_ k_flags, key, klen,
			    "delete readonly key '%"SVf"' from"
			    );
	}

	if (flags & G_DISCARD)
	    sv = Nullsv;
	else {
	    sv = sv_2mortal(HeVAL(entry));
	    HeVAL(entry) = &PL_sv_undef;
	}

	/*
	 * If a restricted hash, rather than really deleting the entry, put
	 * a placeholder there. This marks the key as being "approved", so
	 * we can still access via not-really-existing key without raising
	 * an error.
	 */
	if (SvREADONLY(hv)) {
	    HeVAL(entry) = &PL_sv_undef;
	    /* We'll be saving this slot, so the number of allocated keys
	     * doesn't go down, but the number placeholders goes up */
	    xhv->xhv_placeholders++; /* HvPLACEHOLDERS(hv)++ */
	} else {
	    *oentry = HeNEXT(entry);
	    if (i && !*oentry)
		xhv->xhv_fill--; /* HvFILL(hv)-- */
	    if (entry == xhv->xhv_eiter /* HvEITER(hv) */)
		HvLAZYDEL_on(hv);
	    else
		hv_free_ent(hv, entry);
	    xhv->xhv_keys--; /* HvKEYS(hv)-- */
	    if (xhv->xhv_keys == 0)
	        HvHASKFLAGS_off(hv);
	}
	return sv;
    }
    if (SvREADONLY(hv)) {
        S_hv_notallowed(aTHX_ k_flags, key, klen,
			"delete disallowed key '%"SVf"' from"
			);
    }

    if (k_flags & HVhek_FREEKEY)
	Safefree(key);
    return Nullsv;
}

/*
=for apidoc hv_exists

Returns a boolean indicating whether the specified hash key exists.  The
C<klen> is the length of the key.

=cut
*/

bool
Perl_hv_exists(pTHX_ HV *hv, const char *key, I32 klen)
{
    register XPVHV* xhv;
    register U32 hash;
    register HE *entry;
    SV *sv;
    bool is_utf8 = FALSE;
    const char *keysave = key;
    int k_flags = 0;

    if (!hv)
	return 0;

    if (klen < 0) {
      klen = -klen;
      is_utf8 = TRUE;
    }

    if (SvRMAGICAL(hv)) {
	if (mg_find((SV*)hv, PERL_MAGIC_tied) || SvGMAGICAL((SV*)hv)) {
	    sv = sv_newmortal();
	    mg_copy((SV*)hv, sv, key, klen);
	    magic_existspack(sv, mg_find(sv, PERL_MAGIC_tiedelem));
	    return (bool)SvTRUE(sv);
	}
#ifdef ENV_IS_CASELESS
	else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
	    sv = sv_2mortal(newSVpvn(key,klen));
	    key = strupr(SvPVX(sv));
	}
#endif
    }

    xhv = (XPVHV*)SvANY(hv);
#ifndef DYNAMIC_ENV_FETCH
    if (!xhv->xhv_array /* !HvARRAY(hv) */)
	return 0;
#endif

    if (is_utf8) {
	STRLEN tmplen = klen;
	/* See the note in hv_fetch(). --jhi */
	key = (char*)bytes_from_utf8((U8*)key, &tmplen, &is_utf8);
	klen = tmplen;
        if (is_utf8)
            k_flags = HVhek_UTF8;
        if (key != keysave)
            k_flags |= HVhek_FREEKEY;
    }

    PERL_HASH(hash, key, klen);

#ifdef DYNAMIC_ENV_FETCH
    if (!xhv->xhv_array /* !HvARRAY(hv) */) entry = Null(HE*);
    else
#endif
    /* entry = (HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    entry = ((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    for (; entry; entry = HeNEXT(entry)) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ k_flags) & HVhek_UTF8)
	    continue;
	if (k_flags & HVhek_FREEKEY)
	    Safefree(key);
	/* If we find the key, but the value is a placeholder, return false. */
	if (HeVAL(entry) == &PL_sv_undef)
	    return FALSE;

	return TRUE;
    }
#ifdef DYNAMIC_ENV_FETCH  /* is it out there? */
    if (SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env)) {
	unsigned long len;
	char *env = PerlEnv_ENVgetenv_len(key,&len);
	if (env) {
	    sv = newSVpvn(env,len);
	    SvTAINTED_on(sv);
	    (void)hv_store(hv,key,klen,sv,hash);
            if (k_flags & HVhek_FREEKEY)
                Safefree(key);
	    return TRUE;
	}
    }
#endif
    if (k_flags & HVhek_FREEKEY)
        Safefree(key);
    return FALSE;
}


/*
=for apidoc hv_exists_ent

Returns a boolean indicating whether the specified hash key exists. C<hash>
can be a valid precomputed hash value, or 0 to ask for it to be
computed.

=cut
*/

bool
Perl_hv_exists_ent(pTHX_ HV *hv, SV *keysv, U32 hash)
{
    register XPVHV* xhv;
    register char *key;
    STRLEN klen;
    register HE *entry;
    SV *sv;
    bool is_utf8;
    char *keysave;
    int k_flags = 0;

    if (!hv)
	return 0;

    if (SvRMAGICAL(hv)) {
	if (mg_find((SV*)hv, PERL_MAGIC_tied) || SvGMAGICAL((SV*)hv)) {
	   SV* svret = sv_newmortal();
	    sv = sv_newmortal();
	    keysv = sv_2mortal(newSVsv(keysv));
	    mg_copy((SV*)hv, sv, (char*)keysv, HEf_SVKEY);
	   magic_existspack(svret, mg_find(sv, PERL_MAGIC_tiedelem));
	   return (bool)SvTRUE(svret);
	}
#ifdef ENV_IS_CASELESS
	else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
	    key = SvPV(keysv, klen);
	    keysv = sv_2mortal(newSVpvn(key,klen));
	    (void)strupr(SvPVX(keysv));
	    hash = 0;
	}
#endif
    }

    xhv = (XPVHV*)SvANY(hv);
#ifndef DYNAMIC_ENV_FETCH
    if (!xhv->xhv_array /* !HvARRAY(hv) */)
	return 0;
#endif

    keysave = key = SvPV(keysv, klen);
    is_utf8 = (SvUTF8(keysv) != 0);
    if (is_utf8) {
	key = (char*)bytes_from_utf8((U8*)key, &klen, &is_utf8);
        if (is_utf8)
            k_flags = HVhek_UTF8;
        if (key != keysave)
            k_flags |= HVhek_FREEKEY;
    }
    if (!hash)
	PERL_HASH(hash, key, klen);

#ifdef DYNAMIC_ENV_FETCH
    if (!xhv->xhv_array /* !HvARRAY(hv) */) entry = Null(HE*);
    else
#endif
    /* entry = (HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    entry = ((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    for (; entry; entry = HeNEXT(entry)) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ k_flags) & HVhek_UTF8)
	    continue;
	if (k_flags & HVhek_FREEKEY)
	    Safefree(key);
	/* If we find the key, but the value is a placeholder, return false. */
	if (HeVAL(entry) == &PL_sv_undef)
	    return FALSE;
	return TRUE;
    }
#ifdef DYNAMIC_ENV_FETCH  /* is it out there? */
    if (SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env)) {
	unsigned long len;
	char *env = PerlEnv_ENVgetenv_len(key,&len);
	if (env) {
	    sv = newSVpvn(env,len);
	    SvTAINTED_on(sv);
	    (void)hv_store_ent(hv,keysv,sv,hash);
            if (k_flags & HVhek_FREEKEY)
                Safefree(key);
	    return TRUE;
	}
    }
#endif
    if (k_flags & HVhek_FREEKEY)
        Safefree(key);
    return FALSE;
}

STATIC void
S_hsplit(pTHX_ HV *hv)
{
    register XPVHV* xhv = (XPVHV*)SvANY(hv);
    I32 oldsize = (I32) xhv->xhv_max+1; /* HvMAX(hv)+1 (sick) */
    register I32 newsize = oldsize * 2;
    register I32 i;
    register char *a = xhv->xhv_array; /* HvARRAY(hv) */
    register HE **aep;
    register HE **bep;
    register HE *entry;
    register HE **oentry;

    PL_nomemok = TRUE;
#if defined(STRANGE_MALLOC) || defined(MYMALLOC)
    Renew(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
    if (!a) {
      PL_nomemok = FALSE;
      return;
    }
#else
    New(2, a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
    if (!a) {
      PL_nomemok = FALSE;
      return;
    }
    Copy(xhv->xhv_array /* HvARRAY(hv) */, a, oldsize * sizeof(HE*), char);
    if (oldsize >= 64) {
	offer_nice_chunk(xhv->xhv_array /* HvARRAY(hv) */,
			PERL_HV_ARRAY_ALLOC_BYTES(oldsize));
    }
    else
	Safefree(xhv->xhv_array /* HvARRAY(hv) */);
#endif

    PL_nomemok = FALSE;
    Zero(&a[oldsize * sizeof(HE*)], (newsize-oldsize) * sizeof(HE*), char);	/* zero 2nd half*/
    xhv->xhv_max = --newsize;	/* HvMAX(hv) = --newsize */
    xhv->xhv_array = a;		/* HvARRAY(hv) = a */
    aep = (HE**)a;

    for (i=0; i<oldsize; i++,aep++) {
	if (!*aep)				/* non-existent */
	    continue;
	bep = aep+oldsize;
	for (oentry = aep, entry = *aep; entry; entry = *oentry) {
	    if ((HeHASH(entry) & newsize) != (U32)i) {
		*oentry = HeNEXT(entry);
		HeNEXT(entry) = *bep;
		if (!*bep)
		    xhv->xhv_fill++; /* HvFILL(hv)++ */
		*bep = entry;
		continue;
	    }
	    else
		oentry = &HeNEXT(entry);
	}
	if (!*aep)				/* everything moved */
	    xhv->xhv_fill--; /* HvFILL(hv)-- */
    }
}

void
Perl_hv_ksplit(pTHX_ HV *hv, IV newmax)
{
    register XPVHV* xhv = (XPVHV*)SvANY(hv);
    I32 oldsize = (I32) xhv->xhv_max+1; /* HvMAX(hv)+1 (sick) */
    register I32 newsize;
    register I32 i;
    register I32 j;
    register char *a;
    register HE **aep;
    register HE *entry;
    register HE **oentry;

    newsize = (I32) newmax;			/* possible truncation here */
    if (newsize != newmax || newmax <= oldsize)
	return;
    while ((newsize & (1 + ~newsize)) != newsize) {
	newsize &= ~(newsize & (1 + ~newsize));	/* get proper power of 2 */
    }
    if (newsize < newmax)
	newsize *= 2;
    if (newsize < newmax)
	return;					/* overflow detection */

    a = xhv->xhv_array; /* HvARRAY(hv) */
    if (a) {
	PL_nomemok = TRUE;
#if defined(STRANGE_MALLOC) || defined(MYMALLOC)
	Renew(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
	if (!a) {
	  PL_nomemok = FALSE;
	  return;
	}
#else
	New(2, a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
	if (!a) {
	  PL_nomemok = FALSE;
	  return;
	}
	Copy(xhv->xhv_array /* HvARRAY(hv) */, a, oldsize * sizeof(HE*), char);
	if (oldsize >= 64) {
	    offer_nice_chunk(xhv->xhv_array /* HvARRAY(hv) */,
			    PERL_HV_ARRAY_ALLOC_BYTES(oldsize));
	}
	else
	    Safefree(xhv->xhv_array /* HvARRAY(hv) */);
#endif
	PL_nomemok = FALSE;
	Zero(&a[oldsize * sizeof(HE*)], (newsize-oldsize) * sizeof(HE*), char); /* zero 2nd half*/
    }
    else {
	Newz(0, a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
    }
    xhv->xhv_max = --newsize; 	/* HvMAX(hv) = --newsize */
    xhv->xhv_array = a; 	/* HvARRAY(hv) = a */
    if (!xhv->xhv_fill /* !HvFILL(hv) */)	/* skip rest if no entries */
	return;

    aep = (HE**)a;
    for (i=0; i<oldsize; i++,aep++) {
	if (!*aep)				/* non-existent */
	    continue;
	for (oentry = aep, entry = *aep; entry; entry = *oentry) {
	    if ((j = (HeHASH(entry) & newsize)) != i) {
		j -= i;
		*oentry = HeNEXT(entry);
		if (!(HeNEXT(entry) = aep[j]))
		    xhv->xhv_fill++; /* HvFILL(hv)++ */
		aep[j] = entry;
		continue;
	    }
	    else
		oentry = &HeNEXT(entry);
	}
	if (!*aep)				/* everything moved */
	    xhv->xhv_fill--; /* HvFILL(hv)-- */
    }
}

/*
=for apidoc newHV

Creates a new HV.  The reference count is set to 1.

=cut
*/

HV *
Perl_newHV(pTHX)
{
    register HV *hv;
    register XPVHV* xhv;

    hv = (HV*)NEWSV(502,0);
    sv_upgrade((SV *)hv, SVt_PVHV);
    xhv = (XPVHV*)SvANY(hv);
    SvPOK_off(hv);
    SvNOK_off(hv);
#ifndef NODEFAULT_SHAREKEYS
    HvSHAREKEYS_on(hv);         /* key-sharing on by default */
#endif
    xhv->xhv_max    = 7;	/* HvMAX(hv) = 7 (start with 8 buckets) */
    xhv->xhv_fill   = 0;	/* HvFILL(hv) = 0 */
    xhv->xhv_pmroot = 0;	/* HvPMROOT(hv) = 0 */
    (void)hv_iterinit(hv);	/* so each() will start off right */
    return hv;
}

HV *
Perl_newHVhv(pTHX_ HV *ohv)
{
    HV *hv = newHV();
    STRLEN hv_max, hv_fill;

    if (!ohv || (hv_fill = HvFILL(ohv)) == 0)
	return hv;
    hv_max = HvMAX(ohv);

    if (!SvMAGICAL((SV *)ohv)) {
	/* It's an ordinary hash, so copy it fast. AMS 20010804 */
	STRLEN i;
	bool shared = !!HvSHAREKEYS(ohv);
	HE **ents, **oents = (HE **)HvARRAY(ohv);
	char *a;
	New(0, a, PERL_HV_ARRAY_ALLOC_BYTES(hv_max+1), char);
	ents = (HE**)a;

	/* In each bucket... */
	for (i = 0; i <= hv_max; i++) {
	    HE *prev = NULL, *ent = NULL, *oent = oents[i];

	    if (!oent) {
		ents[i] = NULL;
		continue;
	    }

	    /* Copy the linked list of entries. */
	    for (oent = oents[i]; oent; oent = HeNEXT(oent)) {
		U32 hash   = HeHASH(oent);
		char *key  = HeKEY(oent);
		STRLEN len = HeKLEN(oent);
                int flags  = HeKFLAGS(oent);

		ent = new_HE();
		HeVAL(ent)     = newSVsv(HeVAL(oent));
		HeKEY_hek(ent)
                    = shared ? share_hek_flags(key, len, hash, flags)
                             :  save_hek_flags(key, len, hash, flags);
		if (prev)
		    HeNEXT(prev) = ent;
		else
		    ents[i] = ent;
		prev = ent;
		HeNEXT(ent) = NULL;
	    }
	}

	HvMAX(hv)   = hv_max;
	HvFILL(hv)  = hv_fill;
	HvTOTALKEYS(hv)  = HvTOTALKEYS(ohv);
	HvARRAY(hv) = ents;
    }
    else {
	/* Iterate over ohv, copying keys and values one at a time. */
	HE *entry;
	I32 riter = HvRITER(ohv);
	HE *eiter = HvEITER(ohv);

	/* Can we use fewer buckets? (hv_max is always 2^n-1) */
	while (hv_max && hv_max + 1 >= hv_fill * 2)
	    hv_max = hv_max / 2;
	HvMAX(hv) = hv_max;

	hv_iterinit(ohv);
	while ((entry = hv_iternext_flags(ohv, 0))) {
	    hv_store_flags(hv, HeKEY(entry), HeKLEN(entry),
                           newSVsv(HeVAL(entry)), HeHASH(entry),
                           HeKFLAGS(entry));
	}
	HvRITER(ohv) = riter;
	HvEITER(ohv) = eiter;
    }

    return hv;
}

void
Perl_hv_free_ent(pTHX_ HV *hv, register HE *entry)
{
    SV *val;

    if (!entry)
	return;
    val = HeVAL(entry);
    if (val && isGV(val) && GvCVu(val) && HvNAME(hv))
	PL_sub_generation++;	/* may be deletion of method from stash */
    SvREFCNT_dec(val);
    if (HeKLEN(entry) == HEf_SVKEY) {
	SvREFCNT_dec(HeKEY_sv(entry));
	Safefree(HeKEY_hek(entry));
    }
    else if (HvSHAREKEYS(hv))
	unshare_hek(HeKEY_hek(entry));
    else
	Safefree(HeKEY_hek(entry));
    del_HE(entry);
}

void
Perl_hv_delayfree_ent(pTHX_ HV *hv, register HE *entry)
{
    if (!entry)
	return;
    if (isGV(HeVAL(entry)) && GvCVu(HeVAL(entry)) && HvNAME(hv))
	PL_sub_generation++;	/* may be deletion of method from stash */
    sv_2mortal(HeVAL(entry));	/* free between statements */
    if (HeKLEN(entry) == HEf_SVKEY) {
	sv_2mortal(HeKEY_sv(entry));
	Safefree(HeKEY_hek(entry));
    }
    else if (HvSHAREKEYS(hv))
	unshare_hek(HeKEY_hek(entry));
    else
	Safefree(HeKEY_hek(entry));
    del_HE(entry);
}

/*
=for apidoc hv_clear

Clears a hash, making it empty.

=cut
*/

void
Perl_hv_clear(pTHX_ HV *hv)
{
    register XPVHV* xhv;
    if (!hv)
	return;

    if(SvREADONLY(hv)) {
        Perl_croak(aTHX_ "Attempt to clear a restricted hash");
    }

    xhv = (XPVHV*)SvANY(hv);
    hfreeentries(hv);
    xhv->xhv_fill = 0; /* HvFILL(hv) = 0 */
    xhv->xhv_keys = 0; /* HvKEYS(hv) = 0 */
    xhv->xhv_placeholders = 0; /* HvPLACEHOLDERS(hv) = 0 */
    if (xhv->xhv_array /* HvARRAY(hv) */)
	(void)memzero(xhv->xhv_array /* HvARRAY(hv) */,
		      (xhv->xhv_max+1 /* HvMAX(hv)+1 */) * sizeof(HE*));

    if (SvRMAGICAL(hv))
	mg_clear((SV*)hv);

    HvHASKFLAGS_off(hv);
}

STATIC void
S_hfreeentries(pTHX_ HV *hv)
{
    register HE **array;
    register HE *entry;
    register HE *oentry = Null(HE*);
    I32 riter;
    I32 max;

    if (!hv)
	return;
    if (!HvARRAY(hv))
	return;

    riter = 0;
    max = HvMAX(hv);
    array = HvARRAY(hv);
    entry = array[0];
    for (;;) {
	if (entry) {
	    oentry = entry;
	    entry = HeNEXT(entry);
	    hv_free_ent(hv, oentry);
	}
	if (!entry) {
	    if (++riter > max)
		break;
	    entry = array[riter];
	}
    }
    (void)hv_iterinit(hv);
}

/*
=for apidoc hv_undef

Undefines the hash.

=cut
*/

void
Perl_hv_undef(pTHX_ HV *hv)
{
    register XPVHV* xhv;
    if (!hv)
	return;
    xhv = (XPVHV*)SvANY(hv);
    hfreeentries(hv);
    Safefree(xhv->xhv_array /* HvARRAY(hv) */);
    if (HvNAME(hv)) {
	Safefree(HvNAME(hv));
	HvNAME(hv) = 0;
    }
    xhv->xhv_max   = 7;	/* HvMAX(hv) = 7 (it's a normal hash) */
    xhv->xhv_array = 0;	/* HvARRAY(hv) = 0 */
    xhv->xhv_fill  = 0;	/* HvFILL(hv) = 0 */
    xhv->xhv_keys  = 0;	/* HvKEYS(hv) = 0 */
    xhv->xhv_placeholders = 0; /* HvPLACEHOLDERS(hv) = 0 */

    if (SvRMAGICAL(hv))
	mg_clear((SV*)hv);
}

/*
=for apidoc hv_iterinit

Prepares a starting point to traverse a hash table.  Returns the number of
keys in the hash (i.e. the same as C<HvKEYS(tb)>).  The return value is
currently only meaningful for hashes without tie magic.

NOTE: Before version 5.004_65, C<hv_iterinit> used to return the number of
hash buckets that happen to be in use.  If you still need that esoteric
value, you can get it through the macro C<HvFILL(tb)>.


=cut
*/

I32
Perl_hv_iterinit(pTHX_ HV *hv)
{
    register XPVHV* xhv;
    HE *entry;

    if (!hv)
	Perl_croak(aTHX_ "Bad hash");
    xhv = (XPVHV*)SvANY(hv);
    entry = xhv->xhv_eiter; /* HvEITER(hv) */
    if (entry && HvLAZYDEL(hv)) {	/* was deleted earlier? */
	HvLAZYDEL_off(hv);
	hv_free_ent(hv, entry);
    }
    xhv->xhv_riter = -1; 	/* HvRITER(hv) = -1 */
    xhv->xhv_eiter = Null(HE*); /* HvEITER(hv) = Null(HE*) */
    /* used to be xhv->xhv_fill before 5.004_65 */
    return XHvTOTALKEYS(xhv);
}
/*
=for apidoc hv_iternext

Returns entries from a hash iterator.  See C<hv_iterinit>.

You may call C<hv_delete> or C<hv_delete_ent> on the hash entry that the
iterator currently points to, without losing your place or invalidating your
iterator.  Note that in this case the current entry is deleted from the hash
with your iterator holding the last reference to it.  Your iterator is flagged
to free the entry on the next call to C<hv_iternext>, so you must not discard
your iterator immediately else the entry will leak - call C<hv_iternext> to
trigger the resource deallocation.

=cut
*/

HE *
Perl_hv_iternext(pTHX_ HV *hv)
{
    return hv_iternext_flags(hv, 0);
}

/*
=for apidoc hv_iternext_flags

Returns entries from a hash iterator.  See C<hv_iterinit> and C<hv_iternext>.
The C<flags> value will normally be zero; if HV_ITERNEXT_WANTPLACEHOLDERS is
set the placeholders keys (for restricted hashes) will be returned in addition
to normal keys. By default placeholders are automatically skipped over.
Currently a placeholder is implemented with a value that is literally
<&Perl_sv_undef> (a regular C<undef> value is a normal read-write SV for which
C<!SvOK> is false). Note that the implementation of placeholders and
restricted hashes may change, and the implementation currently is
insufficiently abstracted for any change to be tidy.

=cut
*/

HE *
Perl_hv_iternext_flags(pTHX_ HV *hv, I32 flags)
{
    register XPVHV* xhv;
    register HE *entry;
    HE *oldentry;
    MAGIC* mg;

    if (!hv)
	Perl_croak(aTHX_ "Bad hash");
    xhv = (XPVHV*)SvANY(hv);
    oldentry = entry = xhv->xhv_eiter; /* HvEITER(hv) */

    if ((mg = SvTIED_mg((SV*)hv, PERL_MAGIC_tied))) {
	SV *key = sv_newmortal();
	if (entry) {
	    sv_setsv(key, HeSVKEY_force(entry));
	    SvREFCNT_dec(HeSVKEY(entry));	/* get rid of previous key */
	}
	else {
	    char *k;
	    HEK *hek;

	    /* one HE per MAGICAL hash */
	    xhv->xhv_eiter = entry = new_HE(); /* HvEITER(hv) = new_HE() */
	    Zero(entry, 1, HE);
	    Newz(54, k, HEK_BASESIZE + sizeof(SV*), char);
	    hek = (HEK*)k;
	    HeKEY_hek(entry) = hek;
	    HeKLEN(entry) = HEf_SVKEY;
	}
	magic_nextpack((SV*) hv,mg,key);
	if (SvOK(key)) {
	    /* force key to stay around until next time */
	    HeSVKEY_set(entry, SvREFCNT_inc(key));
	    return entry;		/* beware, hent_val is not set */
	}
	if (HeVAL(entry))
	    SvREFCNT_dec(HeVAL(entry));
	Safefree(HeKEY_hek(entry));
	del_HE(entry);
	xhv->xhv_eiter = Null(HE*); /* HvEITER(hv) = Null(HE*) */
	return Null(HE*);
    }
#ifdef DYNAMIC_ENV_FETCH  /* set up %ENV for iteration */
    if (!entry && SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env))
	prime_env_iter();
#endif

    if (!xhv->xhv_array /* !HvARRAY(hv) */)
	Newz(506, xhv->xhv_array /* HvARRAY(hv) */,
	     PERL_HV_ARRAY_ALLOC_BYTES(xhv->xhv_max+1 /* HvMAX(hv)+1 */),
	     char);
    if (entry)
    {
	entry = HeNEXT(entry);
        if (!(flags & HV_ITERNEXT_WANTPLACEHOLDERS)) {
            /*
             * Skip past any placeholders -- don't want to include them in
             * any iteration.
             */
            while (entry && HeVAL(entry) == &PL_sv_undef) {
                entry = HeNEXT(entry);
            }
	}
    }
    while (!entry) {
	xhv->xhv_riter++; /* HvRITER(hv)++ */
	if (xhv->xhv_riter > (I32)xhv->xhv_max /* HvRITER(hv) > HvMAX(hv) */) {
	    xhv->xhv_riter = -1; /* HvRITER(hv) = -1 */
	    break;
	}
	/* entry = (HvARRAY(hv))[HvRITER(hv)]; */
	entry = ((HE**)xhv->xhv_array)[xhv->xhv_riter];

        if (!(flags & HV_ITERNEXT_WANTPLACEHOLDERS)) {
            /* if we have an entry, but it's a placeholder, don't count it */
            if (entry && HeVAL(entry) == &PL_sv_undef)
                entry = 0;
        }
    }

    if (oldentry && HvLAZYDEL(hv)) {		/* was deleted earlier? */
	HvLAZYDEL_off(hv);
	hv_free_ent(hv, oldentry);
    }

    xhv->xhv_eiter = entry; /* HvEITER(hv) = entry */
    return entry;
}

/*
=for apidoc hv_iterkey

Returns the key from the current position of the hash iterator.  See
C<hv_iterinit>.

=cut
*/

char *
Perl_hv_iterkey(pTHX_ register HE *entry, I32 *retlen)
{
    if (HeKLEN(entry) == HEf_SVKEY) {
	STRLEN len;
	char *p = SvPV(HeKEY_sv(entry), len);
	*retlen = len;
	return p;
    }
    else {
	*retlen = HeKLEN(entry);
	return HeKEY(entry);
    }
}

/* unlike hv_iterval(), this always returns a mortal copy of the key */
/*
=for apidoc hv_iterkeysv

Returns the key as an C<SV*> from the current position of the hash
iterator.  The return value will always be a mortal copy of the key.  Also
see C<hv_iterinit>.

=cut
*/

SV *
Perl_hv_iterkeysv(pTHX_ register HE *entry)
{
    if (HeKLEN(entry) != HEf_SVKEY) {
        HEK *hek = HeKEY_hek(entry);
        int flags = HEK_FLAGS(hek);
        SV *sv;

        if (flags & HVhek_WASUTF8) {
            /* Trouble :-)
               Andreas would like keys he put in as utf8 to come back as utf8
            */
            STRLEN utf8_len = HEK_LEN(hek);
            U8 *as_utf8 = bytes_to_utf8 ((U8*)HEK_KEY(hek), &utf8_len);

            sv = newSVpvn ((char*)as_utf8, utf8_len);
            SvUTF8_on (sv);
	    Safefree (as_utf8); /* bytes_to_utf8() allocates a new string */
        } else {
            sv = newSVpvn_share(HEK_KEY(hek),
                                (HEK_UTF8(hek) ? -HEK_LEN(hek) : HEK_LEN(hek)),
                                HEK_HASH(hek));
        }
        return sv_2mortal(sv);
    }
    return sv_mortalcopy(HeKEY_sv(entry));
}

/*
=for apidoc hv_iterval

Returns the value from the current position of the hash iterator.  See
C<hv_iterkey>.

=cut
*/

SV *
Perl_hv_iterval(pTHX_ HV *hv, register HE *entry)
{
    if (SvRMAGICAL(hv)) {
	if (mg_find((SV*)hv, PERL_MAGIC_tied)) {
	    SV* sv = sv_newmortal();
	    if (HeKLEN(entry) == HEf_SVKEY)
		mg_copy((SV*)hv, sv, (char*)HeKEY_sv(entry), HEf_SVKEY);
	    else mg_copy((SV*)hv, sv, HeKEY(entry), HeKLEN(entry));
	    return sv;
	}
    }
    return HeVAL(entry);
}

/*
=for apidoc hv_iternextsv

Performs an C<hv_iternext>, C<hv_iterkey>, and C<hv_iterval> in one
operation.

=cut
*/

SV *
Perl_hv_iternextsv(pTHX_ HV *hv, char **key, I32 *retlen)
{
    HE *he;
    if ( (he = hv_iternext_flags(hv, 0)) == NULL)
	return NULL;
    *key = hv_iterkey(he, retlen);
    return hv_iterval(hv, he);
}

/*
=for apidoc hv_magic

Adds magic to a hash.  See C<sv_magic>.

=cut
*/

void
Perl_hv_magic(pTHX_ HV *hv, GV *gv, int how)
{
    sv_magic((SV*)hv, (SV*)gv, how, Nullch, 0);
}

#if 0 /* use the macro from hv.h instead */

char*	
Perl_sharepvn(pTHX_ const char *sv, I32 len, U32 hash)
{
    return HEK_KEY(share_hek(sv, len, hash));
}

#endif

/* possibly free a shared string if no one has access to it
 * len and hash must both be valid for str.
 */
void
Perl_unsharepvn(pTHX_ const char *str, I32 len, U32 hash)
{
    unshare_hek_or_pvn (NULL, str, len, hash);
}


void
Perl_unshare_hek(pTHX_ HEK *hek)
{
    unshare_hek_or_pvn(hek, NULL, 0, 0);
}

/* possibly free a shared string if no one has access to it
   hek if non-NULL takes priority over the other 3, else str, len and hash
   are used.  If so, len and hash must both be valid for str.
 */
STATIC void
S_unshare_hek_or_pvn(pTHX_ HEK *hek, const char *str, I32 len, U32 hash)
{
    register XPVHV* xhv;
    register HE *entry;
    register HE **oentry;
    register I32 i = 1;
    I32 found = 0;
    bool is_utf8 = FALSE;
    int k_flags = 0;
    const char *save = str;

    if (hek) {
        hash = HEK_HASH(hek);
    } else if (len < 0) {
        STRLEN tmplen = -len;
        is_utf8 = TRUE;
        /* See the note in hv_fetch(). --jhi */
        str = (char*)bytes_from_utf8((U8*)str, &tmplen, &is_utf8);
        len = tmplen;
        if (is_utf8)
            k_flags = HVhek_UTF8;
        if (str != save)
            k_flags |= HVhek_WASUTF8 | HVhek_FREEKEY;
    }

    /* what follows is the moral equivalent of:
    if ((Svp = hv_fetch(PL_strtab, tmpsv, FALSE, hash))) {
	if (--*Svp == Nullsv)
	    hv_delete(PL_strtab, str, len, G_DISCARD, hash);
    } */
    xhv = (XPVHV*)SvANY(PL_strtab);
    /* assert(xhv_array != 0) */
    LOCK_STRTAB_MUTEX;
    /* oentry = &(HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    if (hek) {
        for (entry = *oentry; entry; i=0, oentry = &HeNEXT(entry), entry = *oentry) {
            if (HeKEY_hek(entry) != hek)
                continue;
            found = 1;
            break;
        }
    } else {
        int flags_masked = k_flags & HVhek_MASK;
        for (entry = *oentry; entry; i=0, oentry = &HeNEXT(entry), entry = *oentry) {
            if (HeHASH(entry) != hash)		/* strings can't be equal */
                continue;
            if (HeKLEN(entry) != len)
                continue;
            if (HeKEY(entry) != str && memNE(HeKEY(entry),str,len))	/* is this it? */
                continue;
            if (HeKFLAGS(entry) != flags_masked)
                continue;
            found = 1;
            break;
        }
    }

    if (found) {
        if (--HeVAL(entry) == Nullsv) {
            *oentry = HeNEXT(entry);
            if (i && !*oentry)
                xhv->xhv_fill--; /* HvFILL(hv)-- */
            Safefree(HeKEY_hek(entry));
            del_HE(entry);
            xhv->xhv_keys--; /* HvKEYS(hv)-- */
        }
    }

    UNLOCK_STRTAB_MUTEX;
    if (!found && ckWARN_d(WARN_INTERNAL))
	Perl_warner(aTHX_ packWARN(WARN_INTERNAL),
                    "Attempt to free non-existent shared string '%s'%s",
                    hek ? HEK_KEY(hek) : str,
                    (k_flags & HVhek_UTF8) ? " (utf8)" : "");
    if (k_flags & HVhek_FREEKEY)
	Safefree(str);
}

/* get a (constant) string ptr from the global string table
 * string will get added if it is not already there.
 * len and hash must both be valid for str.
 */
HEK *
Perl_share_hek(pTHX_ const char *str, I32 len, register U32 hash)
{
    bool is_utf8 = FALSE;
    int flags = 0;
    const char *save = str;

    if (len < 0) {
      STRLEN tmplen = -len;
      is_utf8 = TRUE;
      /* See the note in hv_fetch(). --jhi */
      str = (char*)bytes_from_utf8((U8*)str, &tmplen, &is_utf8);
      len = tmplen;
      /* If we were able to downgrade here, then than means that we were passed
         in a key which only had chars 0-255, but was utf8 encoded.  */
      if (is_utf8)
          flags = HVhek_UTF8;
      /* If we found we were able to downgrade the string to bytes, then
         we should flag that it needs upgrading on keys or each.  Also flag
         that we need share_hek_flags to free the string.  */
      if (str != save)
          flags |= HVhek_WASUTF8 | HVhek_FREEKEY;
    }

    return share_hek_flags (str, len, hash, flags);
}

STATIC HEK *
S_share_hek_flags(pTHX_ const char *str, I32 len, register U32 hash, int flags)
{
    register XPVHV* xhv;
    register HE *entry;
    register HE **oentry;
    register I32 i = 1;
    I32 found = 0;
    int flags_masked = flags & HVhek_MASK;

    /* what follows is the moral equivalent of:

    if (!(Svp = hv_fetch(PL_strtab, str, len, FALSE)))
	hv_store(PL_strtab, str, len, Nullsv, hash);
    */
    xhv = (XPVHV*)SvANY(PL_strtab);
    /* assert(xhv_array != 0) */
    LOCK_STRTAB_MUTEX;
    /* oentry = &(HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    for (entry = *oentry; entry; i=0, entry = HeNEXT(entry)) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != len)
	    continue;
	if (HeKEY(entry) != str && memNE(HeKEY(entry),str,len))	/* is this it? */
	    continue;
	if (HeKFLAGS(entry) != flags_masked)
	    continue;
	found = 1;
	break;
    }
    if (!found) {
	entry = new_HE();
	HeKEY_hek(entry) = save_hek_flags(str, len, hash, flags);
	HeVAL(entry) = Nullsv;
	HeNEXT(entry) = *oentry;
	*oentry = entry;
	xhv->xhv_keys++; /* HvKEYS(hv)++ */
	if (i) {				/* initial entry? */
	    xhv->xhv_fill++; /* HvFILL(hv)++ */
	    if (xhv->xhv_keys > (IV)xhv->xhv_max /* HvKEYS(hv) > HvMAX(hv) */)
		hsplit(PL_strtab);
	}
    }

    ++HeVAL(entry);				/* use value slot as REFCNT */
    UNLOCK_STRTAB_MUTEX;

    if (flags & HVhek_FREEKEY)
	Safefree(str);

    return HeKEY_hek(entry);
}
