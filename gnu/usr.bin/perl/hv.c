/*    hv.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2003, 2004, 2005, 2006, by Larry Wall and others
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

A HV structure represents a Perl hash. It consists mainly of an array
of pointers, each of which points to a linked list of HE structures. The
array is indexed by the hash function of the key, so each linked list
represents all the hash entries with the same hash value. Each HE contains
a pointer to the actual value, plus a pointer to a HEK structure which
holds the key and hash value.

=cut

*/

#include "EXTERN.h"
#define PERL_IN_HV_C
#define PERL_HASH_INTERNAL_ACCESS
#include "perl.h"

#define HV_MAX_LENGTH_BEFORE_SPLIT 14

STATIC void
S_more_he(pTHX)
{
    register HE* he;
    register HE* heend;
    XPV *ptr;
    Newx(ptr, PERL_ARENA_SIZE/sizeof(XPV), XPV);
    ptr->xpv_pv = (char*)PL_he_arenaroot;
    PL_he_arenaroot = ptr;

    he = (HE*)ptr;
    heend = &he[PERL_ARENA_SIZE / sizeof(HE) - 1];
    PL_he_root = ++he;
    while (he < heend) {
	HeNEXT(he) = (HE*)(he + 1);
	he++;
    }
    HeNEXT(he) = 0;
}

STATIC HE*
S_new_he(pTHX)
{
    HE* he;
    LOCK_SV_MUTEX;
    if (!PL_he_root)
	S_more_he(aTHX);
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
    const int flags_masked = flags & HVhek_MASK;
    char *k;
    register HEK *hek;

    Newx(k, HEK_BASESIZE + len + 2, char);
    hek = (HEK*)k;
    Copy(str, HEK_KEY(hek), len, char);
    HEK_KEY(hek)[len] = 0;
    HEK_LEN(hek) = len;
    HEK_HASH(hek) = hash;
    HEK_FLAGS(hek) = (unsigned char)flags_masked;

    if (flags & HVhek_FREEKEY)
	Safefree(str);
    return hek;
}

/* free the pool of temporary HE/HEK pairs returned by hv_fetch_ent
 * for tied hashes */

void
Perl_free_tied_hv_pool(pTHX)
{
    HE *he = PL_hv_fetch_ent_mh;
    while (he) {
	HE * const ohe = he;
	Safefree(HeKEY_hek(he));
	he = HeNEXT(he);
	del_HE(ohe);
    }
    PL_hv_fetch_ent_mh = Nullhe;
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
    if (HeKLEN(e) == HEf_SVKEY) {
	char *k;
	Newx(k, HEK_BASESIZE + sizeof(SV*), char);
	HeKEY_hek(ret) = (HEK*)k;
	HeKEY_sv(ret) = SvREFCNT_inc(sv_dup(HeKEY_sv(e), param));
    }
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
    SV *sv = sv_newmortal();
    if (!(flags & HVhek_FREEKEY)) {
	sv_setpvn(sv, key, klen);
    }
    else {
	/* Need to free saved eventually assign to mortal SV */
	/* XXX is this line an error ???:  SV *sv = sv_newmortal(); */
	sv_usepvn(sv, (char *) key, klen);
    }
    if (flags & HVhek_UTF8) {
	SvUTF8_on(sv);
    }
    Perl_croak(aTHX_ msg, sv);
}

/* (klen == HEf_SVKEY) is special for MAGICAL hv entries, meaning key slot
 * contains an SV* */

#define HV_FETCH_ISSTORE   0x01
#define HV_FETCH_ISEXISTS  0x02
#define HV_FETCH_LVALUE    0x04
#define HV_FETCH_JUST_SV   0x08

/*
=for apidoc hv_store

Stores an SV in a hash.  The hash key is specified as C<key> and C<klen> is
the length of the key.  The C<hash> parameter is the precomputed hash
value; if it is zero then Perl will compute it.  The return value will be
NULL if the operation failed or if the value did not need to be actually
stored within the hash (as in the case of tied hashes).  Otherwise it can
be dereferenced to get the original C<SV*>.  Note that the caller is
responsible for suitably incrementing the reference count of C<val> before
the call, and decrementing it if the function returned NULL.  Effectively
a successful hv_store takes ownership of one reference to C<val>.  This is
usually what you want; a newly created SV has a reference count of one, so
if all your code does is create SVs then store them in a hash, hv_store
will own the only reference to the new SV, and your code doesn't need to do
anything further to tidy up.  hv_store is not implemented as a call to
hv_store_ent, and does not create a temporary SV for the key, so if your
key data is not already in SV form then use hv_store in preference to
hv_store_ent.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for more
information on how to use this function on tied hashes.

=cut
*/

SV**
Perl_hv_store(pTHX_ HV *hv, const char *key, I32 klen_i32, SV *val, U32 hash)
{
    HE *hek;
    STRLEN klen;
    int flags;

    if (klen_i32 < 0) {
	klen = -klen_i32;
	flags = HVhek_UTF8;
    } else {
	klen = klen_i32;
	flags = 0;
    }
    hek = hv_fetch_common (hv, NULL, key, klen, flags,
			   (HV_FETCH_ISSTORE|HV_FETCH_JUST_SV), val, hash);
    return hek ? &HeVAL(hek) : NULL;
}

SV**
Perl_hv_store_flags(pTHX_ HV *hv, const char *key, I32 klen, SV *val,
                 register U32 hash, int flags)
{
    HE * const hek = hv_fetch_common (hv, NULL, key, klen, flags,
			       (HV_FETCH_ISSTORE|HV_FETCH_JUST_SV), val, hash);
    return hek ? &HeVAL(hek) : NULL;
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
decrementing it if the function returned NULL.  Effectively a successful
hv_store_ent takes ownership of one reference to C<val>.  This is
usually what you want; a newly created SV has a reference count of one, so
if all your code does is create SVs then store them in a hash, hv_store
will own the only reference to the new SV, and your code doesn't need to do
anything further to tidy up.  Note that hv_store_ent only reads the C<key>;
unlike C<val> it does not take ownership of it, so maintaining the correct
reference count on C<key> is entirely the caller's responsibility.  hv_store
is not implemented as a call to hv_store_ent, and does not create a temporary
SV for the key, so if your key data is not already in SV form then use
hv_store in preference to hv_store_ent.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for more
information on how to use this function on tied hashes.

=cut
*/

HE *
Perl_hv_store_ent(pTHX_ HV *hv, SV *keysv, SV *val, U32 hash)
{
  return hv_fetch_common(hv, keysv, NULL, 0, 0, HV_FETCH_ISSTORE, val, hash);
}

/*
=for apidoc hv_exists

Returns a boolean indicating whether the specified hash key exists.  The
C<klen> is the length of the key.

=cut
*/

bool
Perl_hv_exists(pTHX_ HV *hv, const char *key, I32 klen_i32)
{
    STRLEN klen;
    int flags;

    if (klen_i32 < 0) {
	klen = -klen_i32;
	flags = HVhek_UTF8;
    } else {
	klen = klen_i32;
	flags = 0;
    }
    return hv_fetch_common(hv, NULL, key, klen, flags, HV_FETCH_ISEXISTS, 0, 0)
	? TRUE : FALSE;
}

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
Perl_hv_fetch(pTHX_ HV *hv, const char *key, I32 klen_i32, I32 lval)
{
    HE *hek;
    STRLEN klen;
    int flags;

    if (klen_i32 < 0) {
	klen = -klen_i32;
	flags = HVhek_UTF8;
    } else {
	klen = klen_i32;
	flags = 0;
    }
    hek = hv_fetch_common (hv, NULL, key, klen, flags,
			   HV_FETCH_JUST_SV | (lval ? HV_FETCH_LVALUE : 0),
			   Nullsv, 0);
    return hek ? &HeVAL(hek) : NULL;
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
    return hv_fetch_common(hv, keysv, NULL, 0, 0, HV_FETCH_ISEXISTS, 0, hash)
	? TRUE : FALSE;
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
    return hv_fetch_common(hv, keysv, NULL, 0, 0, 
			   (lval ? HV_FETCH_LVALUE : 0), Nullsv, hash);
}

STATIC HE *
S_hv_fetch_common(pTHX_ HV *hv, SV *keysv, const char *key, STRLEN klen,
		  int flags, int action, SV *val, register U32 hash)
{
    XPVHV* xhv;
    HE *entry;
    HE **oentry;
    SV *sv;
    bool is_utf8;
    int masked_flags;

    if (!hv)
	return 0;

    if (keysv) {
	if (flags & HVhek_FREEKEY)
	    Safefree(key);
	key = SvPV_const(keysv, klen);
	flags = 0;
	is_utf8 = (SvUTF8(keysv) != 0);
    } else {
	is_utf8 = ((flags & HVhek_UTF8) ? TRUE : FALSE);
    }

    xhv = (XPVHV*)SvANY(hv);
    if (SvMAGICAL(hv)) {
	if (SvRMAGICAL(hv) && !(action & (HV_FETCH_ISSTORE|HV_FETCH_ISEXISTS)))
	  {
	    if (mg_find((SV*)hv, PERL_MAGIC_tied) || SvGMAGICAL((SV*)hv)) {
		sv = sv_newmortal();

		/* XXX should be able to skimp on the HE/HEK here when
		   HV_FETCH_JUST_SV is true.  */

		if (!keysv) {
		    keysv = newSVpvn(key, klen);
		    if (is_utf8) {
			SvUTF8_on(keysv);
		    }
		} else {
		    keysv = newSVsv(keysv);
		}
		mg_copy((SV*)hv, sv, (char *)keysv, HEf_SVKEY);

		/* grab a fake HE/HEK pair from the pool or make a new one */
		entry = PL_hv_fetch_ent_mh;
		if (entry)
		    PL_hv_fetch_ent_mh = HeNEXT(entry);
		else {
		    char *k;
		    entry = new_HE();
		    Newx(k, HEK_BASESIZE + sizeof(SV*), char);
		    HeKEY_hek(entry) = (HEK*)k;
		}
		HeNEXT(entry) = Nullhe;
		HeSVKEY_set(entry, keysv);
		HeVAL(entry) = sv;
		sv_upgrade(sv, SVt_PVLV);
		LvTYPE(sv) = 'T';
		 /* so we can free entry when freeing sv */
		LvTARG(sv) = (SV*)entry;

		/* XXX remove at some point? */
		if (flags & HVhek_FREEKEY)
		    Safefree(key);

		return entry;
	    }
#ifdef ENV_IS_CASELESS
	    else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
		U32 i;
		for (i = 0; i < klen; ++i)
		    if (isLOWER(key[i])) {
			/* Would be nice if we had a routine to do the
			   copy and upercase in a single pass through.  */
			const char *nkey = strupr(savepvn(key,klen));
			/* Note that this fetch is for nkey (the uppercased
			   key) whereas the store is for key (the original)  */
			entry = hv_fetch_common(hv, Nullsv, nkey, klen,
						HVhek_FREEKEY, /* free nkey */
						0 /* non-LVAL fetch */,
						Nullsv /* no value */,
						0 /* compute hash */);
			if (!entry && (action & HV_FETCH_LVALUE)) {
			    /* This call will free key if necessary.
			       Do it this way to encourage compiler to tail
			       call optimise.  */
			    entry = hv_fetch_common(hv, keysv, key, klen,
						    flags, HV_FETCH_ISSTORE,
						    NEWSV(61,0), hash);
			} else {
			    if (flags & HVhek_FREEKEY)
				Safefree(key);
			}
			return entry;
		    }
	    }
#endif
	} /* ISFETCH */
	else if (SvRMAGICAL(hv) && (action & HV_FETCH_ISEXISTS)) {
	    if (mg_find((SV*)hv, PERL_MAGIC_tied) || SvGMAGICAL((SV*)hv)) {
		/* I don't understand why hv_exists_ent has svret and sv,
		   whereas hv_exists only had one.  */
		SV * const svret = sv_newmortal();
		sv = sv_newmortal();

		if (keysv || is_utf8) {
		    if (!keysv) {
			keysv = newSVpvn(key, klen);
			SvUTF8_on(keysv);
		    } else {
			keysv = newSVsv(keysv);
		    }
		    mg_copy((SV*)hv, sv, (char *)sv_2mortal(keysv), HEf_SVKEY);
		} else {
		    mg_copy((SV*)hv, sv, key, klen);
		}
		if (flags & HVhek_FREEKEY)
		    Safefree(key);
		magic_existspack(svret, mg_find(sv, PERL_MAGIC_tiedelem));
		/* This cast somewhat evil, but I'm merely using NULL/
		   not NULL to return the boolean exists.
		   And I know hv is not NULL.  */
		return SvTRUE(svret) ? (HE *)hv : NULL;
		}
#ifdef ENV_IS_CASELESS
	    else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
		/* XXX This code isn't UTF8 clean.  */
		char * const keysave = (char * const)key;
		/* Will need to free this, so set FREEKEY flag.  */
		key = savepvn(key,klen);
		key = (const char*)strupr((char*)key);
		is_utf8 = 0;
		hash = 0;
		keysv = 0;

		if (flags & HVhek_FREEKEY) {
		    Safefree(keysave);
		}
		flags |= HVhek_FREEKEY;
	    }
#endif
	} /* ISEXISTS */
	else if (action & HV_FETCH_ISSTORE) {
	    bool needs_copy;
	    bool needs_store;
	    hv_magic_check (hv, &needs_copy, &needs_store);
	    if (needs_copy) {
		const bool save_taint = PL_tainted;
		if (keysv || is_utf8) {
		    if (!keysv) {
			keysv = newSVpvn(key, klen);
			SvUTF8_on(keysv);
		    }
		    if (PL_tainting)
			PL_tainted = SvTAINTED(keysv);
		    keysv = sv_2mortal(newSVsv(keysv));
		    mg_copy((SV*)hv, val, (char*)keysv, HEf_SVKEY);
		} else {
		    mg_copy((SV*)hv, val, key, klen);
		}

		TAINT_IF(save_taint);
		if (!xhv->xhv_array /* !HvARRAY(hv) */ && !needs_store) {
		    if (flags & HVhek_FREEKEY)
			Safefree(key);
		    return Nullhe;
		}
#ifdef ENV_IS_CASELESS
		else if (mg_find((SV*)hv, PERL_MAGIC_env)) {
		    /* XXX This code isn't UTF8 clean.  */
		    const char *keysave = key;
		    /* Will need to free this, so set FREEKEY flag.  */
		    key = savepvn(key,klen);
		    key = (const char*)strupr((char*)key);
		    is_utf8 = 0;
		    hash = 0;
		    keysv = 0;

		    if (flags & HVhek_FREEKEY) {
			Safefree(keysave);
		    }
		    flags |= HVhek_FREEKEY;
		}
#endif
	    }
	} /* ISSTORE */
    } /* SvMAGICAL */

    if (!xhv->xhv_array /* !HvARRAY(hv) */) {
	if ((action & (HV_FETCH_LVALUE | HV_FETCH_ISSTORE))
#ifdef DYNAMIC_ENV_FETCH  /* if it's an %ENV lookup, we may get it on the fly */
		 || (SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env))
#endif
								  ) {
	    char *array;
	    Newxz(array,
		 PERL_HV_ARRAY_ALLOC_BYTES(xhv->xhv_max+1 /* HvMAX(hv)+1 */),
		 char);
	    HvARRAY(hv) = (HE**)array;
	}
#ifdef DYNAMIC_ENV_FETCH
	else if (action & HV_FETCH_ISEXISTS) {
	    /* for an %ENV exists, if we do an insert it's by a recursive
	       store call, so avoid creating HvARRAY(hv) right now.  */
	}
#endif
	else {
	    /* XXX remove at some point? */
            if (flags & HVhek_FREEKEY)
                Safefree(key);

	    return 0;
	}
    }

    if (is_utf8) {
	char * const keysave = (char * const)key;
	key = (char*)bytes_from_utf8((U8*)key, &klen, &is_utf8);
        if (is_utf8)
	    flags |= HVhek_UTF8;
	else
	    flags &= ~HVhek_UTF8;
        if (key != keysave) {
	    if (flags & HVhek_FREEKEY)
		Safefree(keysave);
            flags |= HVhek_WASUTF8 | HVhek_FREEKEY;
	}
    }

    if (HvREHASH(hv)) {
	PERL_HASH_INTERNAL(hash, key, klen);
	/* We don't have a pointer to the hv, so we have to replicate the
	   flag into every HEK, so that hv_iterkeysv can see it.  */
	/* And yes, you do need this even though you are not "storing" because
	   you can flip the flags below if doing an lval lookup.  (And that
	   was put in to give the semantics Andreas was expecting.)  */
	flags |= HVhek_REHASH;
    } else if (!hash) {
	/* Not enough shared hash key scalars around to make this worthwhile
	   (about 4% slowdown in perlbench with this in)
        if (keysv && (SvIsCOW_shared_hash(keysv))) {
            hash = SvSHARED_HASH(keysv);
        } else
	*/
	{
            PERL_HASH(hash, key, klen);
        }
    }

    masked_flags = (flags & HVhek_MASK);

#ifdef DYNAMIC_ENV_FETCH
    if (!xhv->xhv_array /* !HvARRAY(hv) */) entry = Null(HE*);
    else
#endif
    {
	/* entry = (HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
	entry = ((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    }
    for (; entry; entry = HeNEXT(entry)) {
	if (!HeKEY_hek(entry))
	    continue;
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ masked_flags) & HVhek_UTF8)
	    continue;

        if (action & (HV_FETCH_LVALUE|HV_FETCH_ISSTORE)) {
	    if (HeKFLAGS(entry) != masked_flags) {
		/* We match if HVhek_UTF8 bit in our flags and hash key's
		   match.  But if entry was set previously with HVhek_WASUTF8
		   and key now doesn't (or vice versa) then we should change
		   the key's flag, as this is assignment.  */
		if (HvSHAREKEYS(hv)) {
		    /* Need to swap the key we have for a key with the flags we
		       need. As keys are shared we can't just write to the
		       flag, so we share the new one, unshare the old one.  */
		    HEK *new_hek = share_hek_flags(key, klen, hash,
						   masked_flags);
		    unshare_hek (HeKEY_hek(entry));
		    HeKEY_hek(entry) = new_hek;
		}
		else
		    HeKFLAGS(entry) = masked_flags;
		if (masked_flags & HVhek_ENABLEHVKFLAGS)
		    HvHASKFLAGS_on(hv);
	    }
	    if (HeVAL(entry) == &PL_sv_placeholder) {
		/* yes, can store into placeholder slot */
		if (action & HV_FETCH_LVALUE) {
		    if (SvMAGICAL(hv)) {
			/* This preserves behaviour with the old hv_fetch
			   implementation which at this point would bail out
			   with a break; (at "if we find a placeholder, we
			   pretend we haven't found anything")

			   That break mean that if a placeholder were found, it
			   caused a call into hv_store, which in turn would
			   check magic, and if there is no magic end up pretty
			   much back at this point (in hv_store's code).  */
			break;
		    }
		    /* LVAL fetch which actaully needs a store.  */
		    val = NEWSV(61,0);
		    xhv->xhv_placeholders--;
		} else {
		    /* store */
		    if (val != &PL_sv_placeholder)
			xhv->xhv_placeholders--;
		}
		HeVAL(entry) = val;
	    } else if (action & HV_FETCH_ISSTORE) {
		SvREFCNT_dec(HeVAL(entry));
		HeVAL(entry) = val;
	    }
	} else if (HeVAL(entry) == &PL_sv_placeholder) {
	    /* if we find a placeholder, we pretend we haven't found
	       anything */
	    break;
	}
	if (flags & HVhek_FREEKEY)
	    Safefree(key);
	return entry;
    }
#ifdef DYNAMIC_ENV_FETCH  /* %ENV lookup?  If so, try to fetch the value now */
    if (!(action & HV_FETCH_ISSTORE) 
	&& SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env)) {
	unsigned long len;
	const char * const env = PerlEnv_ENVgetenv_len(key,&len);
	if (env) {
	    sv = newSVpvn(env,len);
	    SvTAINTED_on(sv);
	    return hv_fetch_common(hv,keysv,key,klen,flags,HV_FETCH_ISSTORE,sv,
				   hash);
	}
    }
#endif

    if (!entry && SvREADONLY(hv) && !(action & HV_FETCH_ISEXISTS)) {
	S_hv_notallowed(aTHX_ flags, key, klen,
			"Attempt to access disallowed key '%"SVf"' in"
			" a restricted hash");
    }
    if (!(action & (HV_FETCH_LVALUE|HV_FETCH_ISSTORE))) {
	/* Not doing some form of store, so return failure.  */
	if (flags & HVhek_FREEKEY)
	    Safefree(key);
	return 0;
    }
    if (action & HV_FETCH_LVALUE) {
	val = NEWSV(61,0);
	if (SvMAGICAL(hv)) {
	    /* At this point the old hv_fetch code would call to hv_store,
	       which in turn might do some tied magic. So we need to make that
	       magic check happen.  */
	    /* gonna assign to this, so it better be there */
	    return hv_fetch_common(hv, keysv, key, klen, flags,
				   HV_FETCH_ISSTORE, val, hash);
	    /* XXX Surely that could leak if the fetch-was-store fails?
	       Just like the hv_fetch.  */
	}
    }

    /* Welcome to hv_store...  */

    if (!xhv->xhv_array) {
	/* Not sure if we can get here.  I think the only case of oentry being
	   NULL is for %ENV with dynamic env fetch.  But that should disappear
	   with magic in the previous code.  */
	char *array;
	Newxz(array,
	     PERL_HV_ARRAY_ALLOC_BYTES(xhv->xhv_max+1 /* HvMAX(hv)+1 */),
	     char);
	HvARRAY(hv) = (HE**)array;
    }

    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];

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

    if (val == &PL_sv_placeholder)
	xhv->xhv_placeholders++;
    if (masked_flags & HVhek_ENABLEHVKFLAGS)
	HvHASKFLAGS_on(hv);

    {
	const HE *counter = HeNEXT(entry);

	xhv->xhv_keys++; /* HvKEYS(hv)++ */
	if (!counter) {				/* initial entry? */
	    xhv->xhv_fill++; /* HvFILL(hv)++ */
	} else if (xhv->xhv_keys > (IV)xhv->xhv_max) {
	    hsplit(hv);
	} else if(!HvREHASH(hv)) {
	    U32 n_links = 1;

	    while ((counter = HeNEXT(counter)))
		n_links++;

	    if (n_links > HV_MAX_LENGTH_BEFORE_SPLIT) {
		/* Use only the old HvKEYS(hv) > HvMAX(hv) condition to limit
		   bucket splits on a rehashed hash, as we're not going to
		   split it again, and if someone is lucky (evil) enough to
		   get all the keys in one list they could exhaust our memory
		   as we repeatedly double the number of buckets on every
		   entry. Linear search feels a less worse thing to do.  */
		hsplit(hv);
	    }
	}
    }

    return entry;
}

STATIC void
S_hv_magic_check(pTHX_ HV *hv, bool *needs_copy, bool *needs_store)
{
    const MAGIC *mg = SvMAGIC(hv);
    *needs_copy = FALSE;
    *needs_store = TRUE;
    while (mg) {
	if (isUPPER(mg->mg_type)) {
	    *needs_copy = TRUE;
	    if (mg->mg_type == PERL_MAGIC_tied) {
		*needs_store = FALSE;
		return; /* We've set all there is to set. */
	    }
	}
	mg = mg->mg_moremagic;
    }
}

/*
=for apidoc hv_scalar

Evaluates the hash in scalar context and returns the result. Handles magic when the hash is tied.

=cut
*/

SV *
Perl_hv_scalar(pTHX_ HV *hv)
{
    SV *sv;

    if (SvRMAGICAL(hv)) {
	MAGIC * const mg = mg_find((SV*)hv, PERL_MAGIC_tied);
	if (mg)
	    return magic_scalarpack(hv, mg);
    }

    sv = sv_newmortal();
    if (HvFILL((HV*)hv)) 
        Perl_sv_setpvf(aTHX_ sv, "%ld/%ld",
                (long)HvFILL(hv), (long)HvMAX(hv) + 1);
    else
        sv_setiv(sv, 0);
    
    return sv;
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
Perl_hv_delete(pTHX_ HV *hv, const char *key, I32 klen_i32, I32 flags)
{
    STRLEN klen;
    int k_flags = 0;

    if (klen_i32 < 0) {
	klen = -klen_i32;
	k_flags |= HVhek_UTF8;
    } else {
	klen = klen_i32;
    }
    return hv_delete_common(hv, NULL, key, klen, k_flags, flags, 0);
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
    return hv_delete_common(hv, keysv, NULL, 0, 0, flags, hash);
}

STATIC SV *
S_hv_delete_common(pTHX_ HV *hv, SV *keysv, const char *key, STRLEN klen,
		   int k_flags, I32 d_flags, U32 hash)
{
    register XPVHV* xhv;
    register HE *entry;
    register HE **oentry;
    HE *const *first_entry;
    SV *sv;
    bool is_utf8;
    int masked_flags;

    if (!hv)
	return Nullsv;

    if (keysv) {
	if (k_flags & HVhek_FREEKEY)
	    Safefree(key);
	key = SvPV_const(keysv, klen);
	k_flags = 0;
	is_utf8 = (SvUTF8(keysv) != 0);
    } else {
	is_utf8 = ((k_flags & HVhek_UTF8) ? TRUE : FALSE);
    }

    if (SvRMAGICAL(hv)) {
	bool needs_copy;
	bool needs_store;
	hv_magic_check (hv, &needs_copy, &needs_store);

	if (needs_copy) {
	    entry = hv_fetch_common(hv, keysv, key, klen,
				    k_flags & ~HVhek_FREEKEY, HV_FETCH_LVALUE,
				    Nullsv, hash);
	    sv = entry ? HeVAL(entry) : NULL;
	    if (sv) {
		if (SvMAGICAL(sv)) {
		    mg_clear(sv);
		}
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
		    /* XXX This code isn't UTF8 clean.  */
		    keysv = sv_2mortal(newSVpvn(key,klen));
		    if (k_flags & HVhek_FREEKEY) {
			Safefree(key);
		    }
		    key = strupr(SvPVX(keysv));
		    is_utf8 = 0;
		    k_flags = 0;
		    hash = 0;
		}
#endif
	    }
	}
    }
    xhv = (XPVHV*)SvANY(hv);
    if (!xhv->xhv_array /* !HvARRAY(hv) */)
	return Nullsv;

    if (is_utf8) {
	const char *keysave = key;
	key = (char*)bytes_from_utf8((U8*)key, &klen, &is_utf8);

        if (is_utf8)
            k_flags |= HVhek_UTF8;
	else
            k_flags &= ~HVhek_UTF8;
        if (key != keysave) {
	    if (k_flags & HVhek_FREEKEY) {
		/* This shouldn't happen if our caller does what we expect,
		   but strictly the API allows it.  */
		Safefree(keysave);
	    }
	    k_flags |= HVhek_WASUTF8 | HVhek_FREEKEY;
	}
        HvHASKFLAGS_on((SV*)hv);
    }

    if (HvREHASH(hv)) {
	PERL_HASH_INTERNAL(hash, key, klen);
    } else if (!hash) {
	/* Not enough shared hash key scalars around to make this worthwhile
	   (about 4% slowdown in perlbench with this in)
        if (keysv && (SvIsCOW_shared_hash(keysv))) {
            hash = SvSHARED_HASH(keysv);
        } else
	*/
	{
            PERL_HASH(hash, key, klen);
        }
    }

    masked_flags = (k_flags & HVhek_MASK);

    /* oentry = &(HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    first_entry = oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    entry = *oentry;
    for (; entry; oentry = &HeNEXT(entry), entry = *oentry) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ masked_flags) & HVhek_UTF8)
	    continue;

	/* if placeholder is here, it's already been deleted.... */
	if (HeVAL(entry) == &PL_sv_placeholder)
	{
	  if (k_flags & HVhek_FREEKEY)
            Safefree(key);
	  return Nullsv;
	}
	else if (SvREADONLY(hv) && HeVAL(entry) && SvREADONLY(HeVAL(entry))) {
	    S_hv_notallowed(aTHX_ k_flags, key, klen,
			    "Attempt to delete readonly key '%"SVf"' from"
			    " a restricted hash");
	}
        if (k_flags & HVhek_FREEKEY)
            Safefree(key);

	if (d_flags & G_DISCARD)
	    sv = Nullsv;
	else {
	    sv = sv_2mortal(HeVAL(entry));
	    HeVAL(entry) = &PL_sv_placeholder;
	}

	/*
	 * If a restricted hash, rather than really deleting the entry, put
	 * a placeholder there. This marks the key as being "approved", so
	 * we can still access via not-really-existing key without raising
	 * an error.
	 */
	if (SvREADONLY(hv)) {
	    SvREFCNT_dec(HeVAL(entry));
	    HeVAL(entry) = &PL_sv_placeholder;
	    /* We'll be saving this slot, so the number of allocated keys
	     * doesn't go down, but the number placeholders goes up */
	    xhv->xhv_placeholders++; /* HvPLACEHOLDERS(hv)++ */
	} else {
	    *oentry = HeNEXT(entry);
	    if(!*first_entry) {
		xhv->xhv_fill--; /* HvFILL(hv)-- */
	    }
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
			"Attempt to delete disallowed key '%"SVf"' from"
			" a restricted hash");
    }

    if (k_flags & HVhek_FREEKEY)
	Safefree(key);
    return Nullsv;
}

STATIC void
S_hsplit(pTHX_ HV *hv)
{
    register XPVHV* xhv = (XPVHV*)SvANY(hv);
    const I32 oldsize = (I32) xhv->xhv_max+1; /* HvMAX(hv)+1 (sick) */
    register I32 newsize = oldsize * 2;
    register I32 i;
    register char *a = xhv->xhv_array; /* HvARRAY(hv) */
    register HE **aep;
    register HE **oentry;
    int longest_chain = 0;
    int was_shared;

    /*PerlIO_printf(PerlIO_stderr(), "hsplit called for %p which had %d\n",
      hv, (int) oldsize);*/

    if (HvPLACEHOLDERS_get(hv) && !SvREADONLY(hv)) {
      /* Can make this clear any placeholders first for non-restricted hashes,
	 even though Storable rebuilds restricted hashes by putting in all the
	 placeholders (first) before turning on the readonly flag, because
	 Storable always pre-splits the hash.  */
      hv_clear_placeholders(hv);
    }
	       
    PL_nomemok = TRUE;
#if defined(STRANGE_MALLOC) || defined(MYMALLOC)
    Renew(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
    if (!a) {
      PL_nomemok = FALSE;
      return;
    }
#else
    Newx(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
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
	int left_length = 0;
	int right_length = 0;
	register HE *entry;
	register HE **bep;

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
		right_length++;
		continue;
	    }
	    else {
		oentry = &HeNEXT(entry);
		left_length++;
	    }
	}
	if (!*aep)				/* everything moved */
	    xhv->xhv_fill--; /* HvFILL(hv)-- */
	/* I think we don't actually need to keep track of the longest length,
	   merely flag if anything is too long. But for the moment while
	   developing this code I'll track it.  */
	if (left_length > longest_chain)
	    longest_chain = left_length;
	if (right_length > longest_chain)
	    longest_chain = right_length;
    }


    /* Pick your policy for "hashing isn't working" here:  */
    if (longest_chain <= HV_MAX_LENGTH_BEFORE_SPLIT /* split worked?  */
	|| HvREHASH(hv)) {
	return;
    }

    if (hv == PL_strtab) {
	/* Urg. Someone is doing something nasty to the string table.
	   Can't win.  */
	return;
    }

    /* Awooga. Awooga. Pathological data.  */
    /*PerlIO_printf(PerlIO_stderr(), "%p %d of %d with %d/%d buckets\n", hv,
      longest_chain, HvTOTALKEYS(hv), HvFILL(hv),  1+HvMAX(hv));*/

    ++newsize;
    Newxz(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
    was_shared = HvSHAREKEYS(hv);

    xhv->xhv_fill = 0;
    HvSHAREKEYS_off(hv);
    HvREHASH_on(hv);

    aep = (HE **) xhv->xhv_array;

    for (i=0; i<newsize; i++,aep++) {
	register HE *entry = *aep;
	while (entry) {
	    /* We're going to trash this HE's next pointer when we chain it
	       into the new hash below, so store where we go next.  */
	    HE * const next = HeNEXT(entry);
	    UV hash;
	    HE **bep;

	    /* Rehash it */
	    PERL_HASH_INTERNAL(hash, HeKEY(entry), HeKLEN(entry));

	    if (was_shared) {
		/* Unshare it.  */
		HEK * const new_hek
		    = save_hek_flags(HeKEY(entry), HeKLEN(entry),
				     hash, HeKFLAGS(entry));
		unshare_hek (HeKEY_hek(entry));
		HeKEY_hek(entry) = new_hek;
	    } else {
		/* Not shared, so simply write the new hash in. */
		HeHASH(entry) = hash;
	    }
	    /*PerlIO_printf(PerlIO_stderr(), "%d ", HeKFLAGS(entry));*/
	    HEK_REHASH_on(HeKEY_hek(entry));
	    /*PerlIO_printf(PerlIO_stderr(), "%d\n", HeKFLAGS(entry));*/

	    /* Copy oentry to the correct new chain.  */
	    bep = ((HE**)a) + (hash & (I32) xhv->xhv_max);
	    if (!*bep)
		    xhv->xhv_fill++; /* HvFILL(hv)++ */
	    HeNEXT(entry) = *bep;
	    *bep = entry;

	    entry = next;
	}
    }
    Safefree (xhv->xhv_array);
    xhv->xhv_array = a;		/* HvARRAY(hv) = a */
}

void
Perl_hv_ksplit(pTHX_ HV *hv, IV newmax)
{
    register XPVHV* xhv = (XPVHV*)SvANY(hv);
    const I32 oldsize = (I32) xhv->xhv_max+1; /* HvMAX(hv)+1 (sick) */
    register I32 newsize;
    register I32 i;
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
	Newx(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
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
	Newxz(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
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
	    register I32 j;
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
    register XPVHV* xhv;
    HV * const hv = (HV*)NEWSV(502,0);

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
    HV * const hv = newHV();
    STRLEN hv_max, hv_fill;

    if (!ohv || (hv_fill = HvFILL(ohv)) == 0)
	return hv;
    hv_max = HvMAX(ohv);

    if (!SvMAGICAL((SV *)ohv)) {
	/* It's an ordinary hash, so copy it fast. AMS 20010804 */
	STRLEN i;
	const bool shared = !!HvSHAREKEYS(ohv);
	HE **ents, ** const oents = (HE **)HvARRAY(ohv);
	char *a;
	Newx(a, PERL_HV_ARRAY_ALLOC_BYTES(hv_max+1), char);
	ents = (HE**)a;

	/* In each bucket... */
	for (i = 0; i <= hv_max; i++) {
	    HE *prev = NULL, *ent = NULL;
	    HE *oent = oents[i];

	    if (!oent) {
		ents[i] = NULL;
		continue;
	    }

	    /* Copy the linked list of entries. */
	    for (; oent; oent = HeNEXT(oent)) {
		const U32 hash   = HeHASH(oent);
		const char * const key = HeKEY(oent);
		const STRLEN len = HeKLEN(oent);
		const int flags  = HeKFLAGS(oent);

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
    } /* not magical */
    else {
	/* Iterate over ohv, copying keys and values one at a time. */
	HE *entry;
	const I32 riter = HvRITER_get(ohv);
	HE * const eiter = HvEITER_get(ohv);

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
	HvRITER_set(ohv, riter);
	HvEITER_set(ohv, eiter);
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
    if (val && isGV(val) && GvCVu(val) && HvNAME_get(hv))
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
    /* SvREFCNT_inc to counter the SvREFCNT_dec in hv_free_ent  */
    sv_2mortal(SvREFCNT_inc(HeVAL(entry)));	/* free between statements */
    if (HeKLEN(entry) == HEf_SVKEY) {
	sv_2mortal(SvREFCNT_inc(HeKEY_sv(entry)));
    }
    hv_free_ent(hv, entry);
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

    xhv = (XPVHV*)SvANY(hv);

    if (SvREADONLY(hv) && xhv->xhv_array != NULL) {
	/* restricted hash: convert all keys to placeholders */
	STRLEN i;
	for (i = 0; i <= xhv->xhv_max; i++) {
	    HE *entry = ((HE**)xhv->xhv_array)[i];
	    for (; entry; entry = HeNEXT(entry)) {
		/* not already placeholder */
		if (HeVAL(entry) != &PL_sv_placeholder) {
		    if (HeVAL(entry) && SvREADONLY(HeVAL(entry))) {
			SV* keysv = hv_iterkeysv(entry);
			Perl_croak(aTHX_
	"Attempt to delete readonly key '%"SVf"' from a restricted hash",
				   keysv);
		    }
		    SvREFCNT_dec(HeVAL(entry));
		    HeVAL(entry) = &PL_sv_placeholder;
		    xhv->xhv_placeholders++; /* HvPLACEHOLDERS(hv)++ */
		}
	    }
	}
	goto reset;
    }

    hfreeentries(hv);
    xhv->xhv_placeholders = 0; /* HvPLACEHOLDERS(hv) = 0 */
    if (xhv->xhv_array /* HvARRAY(hv) */)
	(void)memzero(xhv->xhv_array /* HvARRAY(hv) */,
		      (xhv->xhv_max+1 /* HvMAX(hv)+1 */) * sizeof(HE*));

    if (SvRMAGICAL(hv))
	mg_clear((SV*)hv);

    HvHASKFLAGS_off(hv);
    HvREHASH_off(hv);
    reset:
    HvEITER_set(hv, NULL);
}

/*
=for apidoc hv_clear_placeholders

Clears any placeholders from a hash.  If a restricted hash has any of its keys
marked as readonly and the key is subsequently deleted, the key is not actually
deleted but is marked by assigning it a value of &PL_sv_placeholder.  This tags
it so it will be ignored by future operations such as iterating over the hash,
but will still allow the hash to have a value reassigned to the key at some
future point.  This function clears any such placeholder keys from the hash.
See Hash::Util::lock_keys() for an example of its use.

=cut
*/

void
Perl_hv_clear_placeholders(pTHX_ HV *hv)
{
    I32 items = (I32)HvPLACEHOLDERS_get(hv);
    I32 i;

    if (items == 0)
	return;

    i = HvMAX(hv);
    do {
	/* Loop down the linked list heads  */
	bool first = 1;
	HE **oentry = &(HvARRAY(hv))[i];
	HE *entry = *oentry;

	if (!entry)
	    continue;

	for (; entry; entry = *oentry) {
	    if (HeVAL(entry) == &PL_sv_placeholder) {
		*oentry = HeNEXT(entry);
		if (first && !*oentry)
		    HvFILL(hv)--; /* This linked list is now empty.  */
		if (entry == HvEITER_get(hv))
		    HvLAZYDEL_on(hv);
		else
		    hv_free_ent(hv, entry);

		if (--items == 0) {
		    /* Finished.  */
		    HvTOTALKEYS(hv) -= (IV)HvPLACEHOLDERS_get(hv);
		    if (HvKEYS(hv) == 0)
			HvHASKFLAGS_off(hv);
		    HvPLACEHOLDERS_set(hv, 0);
		    return;
		}
	    } else {
		oentry = &HeNEXT(entry);
		first = 0;
	    }
	}
    } while (--i >= 0);
    /* You can't get here, hence assertion should always fail.  */
    assert (items == 0);
    assert (0);
}

STATIC void
S_hfreeentries(pTHX_ HV *hv)
{
    register HE **array;
    register HE *entry;
    I32 riter;
    I32 max;


    if (!HvARRAY(hv))
	return;

    riter = 0;
    max = HvMAX(hv);
    array = HvARRAY(hv);
    /* make everyone else think the array is empty, so that the destructors
     * called for freed entries can't recusively mess with us */
    HvARRAY(hv) = Null(HE**); 
    HvFILL(hv) = 0;
    ((XPVHV*) SvANY(hv))->xhv_keys = 0;

    entry = array[0];
    for (;;) {
	if (entry) {
	    register HE * const oentry = entry;
	    entry = HeNEXT(entry);
	    hv_free_ent(hv, oentry);
	}
	if (!entry) {
	    if (++riter > max)
		break;
	    entry = array[riter];
	}
    }
    HvARRAY(hv) = array;
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
    const char *name;
    if (!hv)
	return;
    xhv = (XPVHV*)SvANY(hv);
    hfreeentries(hv);
    Safefree(xhv->xhv_array /* HvARRAY(hv) */);
    if ((name = HvNAME_get(hv))) {
	/* FIXME - strlen HvNAME  */
        if(PL_stashcache)
	    hv_delete(PL_stashcache, name, strlen(name), G_DISCARD);
	hv_name_set(hv, Nullch, 0, 0);
    }
    xhv->xhv_max   = 7;	/* HvMAX(hv) = 7 (it's a normal hash) */
    xhv->xhv_array = 0;	/* HvARRAY(hv) = 0 */
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
    return HvTOTALKEYS(hv);
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
Currently a placeholder is implemented with a value that is
C<&Perl_sv_placeholder>. Note that the implementation of placeholders and
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
	    Newxz(k, HEK_BASESIZE + sizeof(SV*), char);
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
    if (!entry && SvRMAGICAL((SV*)hv) && mg_find((SV*)hv, PERL_MAGIC_env)) {
	prime_env_iter();
#ifdef VMS
	/* The prime_env_iter() on VMS just loaded up new hash values
	 * so the iteration count needs to be reset back to the beginning
	 */
	hv_iterinit(hv);
	oldentry = entry = xhv->xhv_eiter; /* HvEITER(hv) */
#endif
    }
#endif

    if (!xhv->xhv_array /* !HvARRAY(hv) */)
	Newz(506, xhv->xhv_array /* HvARRAY(hv) */,
	     PERL_HV_ARRAY_ALLOC_BYTES(xhv->xhv_max+1 /* HvMAX(hv)+1 */),
	     char);
    /* At start of hash, entry is NULL.  */
    if (entry)
    {
	entry = HeNEXT(entry);
        if (!(flags & HV_ITERNEXT_WANTPLACEHOLDERS)) {
            /*
             * Skip past any placeholders -- don't want to include them in
             * any iteration.
             */
            while (entry && HeVAL(entry) == &PL_sv_placeholder) {
                entry = HeNEXT(entry);
            }
	}
    }
    while (!entry) {
	/* OK. Come to the end of the current list.  Grab the next one.  */

	xhv->xhv_riter++; /* HvRITER(hv)++ */
	if (xhv->xhv_riter > (I32)xhv->xhv_max /* HvRITER(hv) > HvMAX(hv) */) {
	    /* There is no next one.  End of the hash.  */
	    xhv->xhv_riter = -1; /* HvRITER(hv) = -1 */
	    break;
	}
	/* entry = (HvARRAY(hv))[HvRITER(hv)]; */
	entry = ((HE**)xhv->xhv_array)[xhv->xhv_riter];

        if (!(flags & HV_ITERNEXT_WANTPLACEHOLDERS)) {
            /* If we have an entry, but it's a placeholder, don't count it.
	       Try the next.  */
	    while (entry && HeVAL(entry) == &PL_sv_placeholder)
		entry = HeNEXT(entry);
	}
	/* Will loop again if this linked list starts NULL
	   (for HV_ITERNEXT_WANTPLACEHOLDERS)
	   or if we run through it and find only placeholders.  */
    }

    if (oldentry && HvLAZYDEL(hv)) {		/* was deleted earlier? */
	HvLAZYDEL_off(hv);
	hv_free_ent(hv, oldentry);
    }

    /*if (HvREHASH(hv) && entry && !HeKREHASH(entry))
      PerlIO_printf(PerlIO_stderr(), "Awooga %p %p\n", hv, entry);*/

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
    return sv_2mortal(newSVhek(HeKEY_hek(entry)));
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
	    else
		mg_copy((SV*)hv, sv, HeKEY(entry), HeKLEN(entry));
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
    HE **first;
    bool found = 0;
    bool is_utf8 = FALSE;
    int k_flags = 0;
    const char * const save = str;

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
    first = oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    if (hek) {
        for (entry = *oentry; entry; oentry = &HeNEXT(entry), entry = *oentry) {
            if (HeKEY_hek(entry) != hek)
                continue;
            found = 1;
            break;
        }
    } else {
        const int flags_masked = k_flags & HVhek_MASK;
        for (entry = *oentry; entry; oentry = &HeNEXT(entry), entry = *oentry) {
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
            if (!*first) {
		/* There are now no entries in our slot.  */
                xhv->xhv_fill--; /* HvFILL(hv)-- */
	    }
            Safefree(HeKEY_hek(entry));
            del_HE(entry);
            xhv->xhv_keys--; /* HvKEYS(hv)-- */
        }
    }

    UNLOCK_STRTAB_MUTEX;
    if (!found && ckWARN_d(WARN_INTERNAL))
	Perl_warner(aTHX_ packWARN(WARN_INTERNAL),
                    "Attempt to free non-existent shared string '%s'%s"
                    pTHX__FORMAT,
                    hek ? HEK_KEY(hek) : str,
                    ((k_flags & HVhek_UTF8) ? " (utf8)" : "") pTHX__VALUE);
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
    const char * const save = str;

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
    I32 found = 0;
    const int flags_masked = flags & HVhek_MASK;

    /* what follows is the moral equivalent of:

    if (!(Svp = hv_fetch(PL_strtab, str, len, FALSE)))
	hv_store(PL_strtab, str, len, Nullsv, hash);

	Can't rehash the shared string table, so not sure if it's worth
	counting the number of entries in the linked list
    */
    xhv = (XPVHV*)SvANY(PL_strtab);
    /* assert(xhv_array != 0) */
    LOCK_STRTAB_MUTEX;
    /* oentry = &(HvARRAY(hv))[hash & (I32) HvMAX(hv)]; */
    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    for (entry = *oentry; entry; entry = HeNEXT(entry)) {
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
	/* What used to be head of the list.
	   If this is NULL, then we're the first entry for this slot, which
	   means we need to increate fill.  */
	const HE *old_first = *oentry;
	entry = new_HE();
	HeKEY_hek(entry) = save_hek_flags(str, len, hash, flags_masked);
	HeVAL(entry) = Nullsv;
	HeNEXT(entry) = *oentry;
	*oentry = entry;
	xhv->xhv_keys++; /* HvKEYS(hv)++ */
	if (!old_first) {			/* initial entry? */
	    xhv->xhv_fill++; /* HvFILL(hv)++ */
	} else if (xhv->xhv_keys > (IV)xhv->xhv_max /* HvKEYS(hv) > HvMAX(hv) */) {
		hsplit(PL_strtab);
	}
    }

    ++HeVAL(entry);				/* use value slot as REFCNT */
    UNLOCK_STRTAB_MUTEX;

    if (flags & HVhek_FREEKEY)
	Safefree(str);

    return HeKEY_hek(entry);
}

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
