/*    hv.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *      I sit beside the fire and think
 *          of all that I have seen.
 *                         --Bilbo
 *
 *     [p.278 of _The Lord of the Rings_, II/iii: "The Ring Goes South"]
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

static const char S_strtab_error[]
    = "Cannot modify shared string table in hv_%s";

STATIC void
S_more_he(pTHX)
{
    dVAR;
    /* We could generate this at compile time via (another) auxiliary C
       program?  */
    const size_t arena_size = Perl_malloc_good_size(PERL_ARENA_SIZE);
    HE* he = (HE*) Perl_get_arena(aTHX_ arena_size, HE_SVSLOT);
    HE * const heend = &he[arena_size / sizeof(HE) - 1];

    PL_body_roots[HE_SVSLOT] = he;
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

STATIC HE*
S_new_he(pTHX)
{
    dVAR;
    HE* he;
    void ** const root = &PL_body_roots[HE_SVSLOT];

    if (!*root)
	S_more_he(aTHX);
    he = (HE*) *root;
    assert(he);
    *root = HeNEXT(he);
    return he;
}

#define new_HE() new_he()
#define del_HE(p) \
    STMT_START { \
	HeNEXT(p) = (HE*)(PL_body_roots[HE_SVSLOT]);	\
	PL_body_roots[HE_SVSLOT] = p; \
    } STMT_END



#endif

STATIC HEK *
S_save_hek_flags(const char *str, I32 len, U32 hash, int flags)
{
    const int flags_masked = flags & HVhek_MASK;
    char *k;
    register HEK *hek;

    PERL_ARGS_ASSERT_SAVE_HEK_FLAGS;

    Newx(k, HEK_BASESIZE + len + 2, char);
    hek = (HEK*)k;
    Copy(str, HEK_KEY(hek), len, char);
    HEK_KEY(hek)[len] = 0;
    HEK_LEN(hek) = len;
    HEK_HASH(hek) = hash;
    HEK_FLAGS(hek) = (unsigned char)flags_masked | HVhek_UNSHARED;

    if (flags & HVhek_FREEKEY)
	Safefree(str);
    return hek;
}

/* free the pool of temporary HE/HEK pairs returned by hv_fetch_ent
 * for tied hashes */

void
Perl_free_tied_hv_pool(pTHX)
{
    dVAR;
    HE *he = PL_hv_fetch_ent_mh;
    while (he) {
	HE * const ohe = he;
	Safefree(HeKEY_hek(he));
	he = HeNEXT(he);
	del_HE(ohe);
    }
    PL_hv_fetch_ent_mh = NULL;
}

#if defined(USE_ITHREADS)
HEK *
Perl_hek_dup(pTHX_ HEK *source, CLONE_PARAMS* param)
{
    HEK *shared;

    PERL_ARGS_ASSERT_HEK_DUP;
    PERL_UNUSED_ARG(param);

    if (!source)
	return NULL;

    shared = (HEK*)ptr_table_fetch(PL_ptr_table, source);
    if (shared) {
	/* We already shared this hash key.  */
	(void)share_hek_hek(shared);
    }
    else {
	shared
	    = share_hek_flags(HEK_KEY(source), HEK_LEN(source),
			      HEK_HASH(source), HEK_FLAGS(source));
	ptr_table_store(PL_ptr_table, source, shared);
    }
    return shared;
}

HE *
Perl_he_dup(pTHX_ const HE *e, bool shared, CLONE_PARAMS* param)
{
    HE *ret;

    PERL_ARGS_ASSERT_HE_DUP;

    if (!e)
	return NULL;
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
	Newx(k, HEK_BASESIZE + sizeof(const SV *), char);
	HeKEY_hek(ret) = (HEK*)k;
	HeKEY_sv(ret) = SvREFCNT_inc(sv_dup(HeKEY_sv(e), param));
    }
    else if (shared) {
	/* This is hek_dup inlined, which seems to be important for speed
	   reasons.  */
	HEK * const source = HeKEY_hek(e);
	HEK *shared = (HEK*)ptr_table_fetch(PL_ptr_table, source);

	if (shared) {
	    /* We already shared this hash key.  */
	    (void)share_hek_hek(shared);
	}
	else {
	    shared
		= share_hek_flags(HEK_KEY(source), HEK_LEN(source),
				  HEK_HASH(source), HEK_FLAGS(source));
	    ptr_table_store(PL_ptr_table, source, shared);
	}
	HeKEY_hek(ret) = shared;
    }
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
    SV * const sv = sv_newmortal();

    PERL_ARGS_ASSERT_HV_NOTALLOWED;

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
    Perl_croak(aTHX_ msg, SVfARG(sv));
}

/* (klen == HEf_SVKEY) is special for MAGICAL hv entries, meaning key slot
 * contains an SV* */

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

=for apidoc hv_exists

Returns a boolean indicating whether the specified hash key exists.  The
C<klen> is the length of the key.

=for apidoc hv_fetch

Returns the SV which corresponds to the specified key in the hash.  The
C<klen> is the length of the key.  If C<lval> is set then the fetch will be
part of a store.  Check that the return value is non-null before
dereferencing it to an C<SV*>.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for more
information on how to use this function on tied hashes.

=for apidoc hv_exists_ent

Returns a boolean indicating whether the specified hash key exists. C<hash>
can be a valid precomputed hash value, or 0 to ask for it to be
computed.

=cut
*/

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

/* Common code for hv_delete()/hv_exists()/hv_fetch()/hv_store()  */
void *
Perl_hv_common_key_len(pTHX_ HV *hv, const char *key, I32 klen_i32,
		       const int action, SV *val, const U32 hash)
{
    STRLEN klen;
    int flags;

    PERL_ARGS_ASSERT_HV_COMMON_KEY_LEN;

    if (klen_i32 < 0) {
	klen = -klen_i32;
	flags = HVhek_UTF8;
    } else {
	klen = klen_i32;
	flags = 0;
    }
    return hv_common(hv, NULL, key, klen, flags, action, val, hash);
}

void *
Perl_hv_common(pTHX_ HV *hv, SV *keysv, const char *key, STRLEN klen,
	       int flags, int action, SV *val, register U32 hash)
{
    dVAR;
    XPVHV* xhv;
    HE *entry;
    HE **oentry;
    SV *sv;
    bool is_utf8;
    int masked_flags;
    const int return_svp = action & HV_FETCH_JUST_SV;

    if (!hv)
	return NULL;
    if (SvTYPE(hv) == SVTYPEMASK)
	return NULL;

    assert(SvTYPE(hv) == SVt_PVHV);

    if (SvSMAGICAL(hv) && SvGMAGICAL(hv) && !(action & HV_DISABLE_UVAR_XKEY)) {
	MAGIC* mg;
	if ((mg = mg_find((const SV *)hv, PERL_MAGIC_uvar))) {
	    struct ufuncs * const uf = (struct ufuncs *)mg->mg_ptr;
	    if (uf->uf_set == NULL) {
		SV* obj = mg->mg_obj;

		if (!keysv) {
		    keysv = newSVpvn_flags(key, klen, SVs_TEMP |
					   ((flags & HVhek_UTF8)
					    ? SVf_UTF8 : 0));
		}
		
		mg->mg_obj = keysv;         /* pass key */
		uf->uf_index = action;      /* pass action */
		magic_getuvar(MUTABLE_SV(hv), mg);
		keysv = mg->mg_obj;         /* may have changed */
		mg->mg_obj = obj;

		/* If the key may have changed, then we need to invalidate
		   any passed-in computed hash value.  */
		hash = 0;
	    }
	}
    }
    if (keysv) {
	if (flags & HVhek_FREEKEY)
	    Safefree(key);
	key = SvPV_const(keysv, klen);
	is_utf8 = (SvUTF8(keysv) != 0);
	if (SvIsCOW_shared_hash(keysv)) {
	    flags = HVhek_KEYCANONICAL | (is_utf8 ? HVhek_UTF8 : 0);
	} else {
	    flags = 0;
	}
    } else {
	is_utf8 = ((flags & HVhek_UTF8) ? TRUE : FALSE);
    }

    if (action & HV_DELETE) {
	return (void *) hv_delete_common(hv, keysv, key, klen,
					 flags | (is_utf8 ? HVhek_UTF8 : 0),
					 action, hash);
    }

    xhv = (XPVHV*)SvANY(hv);
    if (SvMAGICAL(hv)) {
	if (SvRMAGICAL(hv) && !(action & (HV_FETCH_ISSTORE|HV_FETCH_ISEXISTS))) {
	    if (mg_find((const SV *)hv, PERL_MAGIC_tied)
		|| SvGMAGICAL((const SV *)hv))
	    {
		/* FIXME should be able to skimp on the HE/HEK here when
		   HV_FETCH_JUST_SV is true.  */
		if (!keysv) {
		    keysv = newSVpvn_utf8(key, klen, is_utf8);
  		} else {
		    keysv = newSVsv(keysv);
		}
                sv = sv_newmortal();
                mg_copy(MUTABLE_SV(hv), sv, (char *)keysv, HEf_SVKEY);

		/* grab a fake HE/HEK pair from the pool or make a new one */
		entry = PL_hv_fetch_ent_mh;
		if (entry)
		    PL_hv_fetch_ent_mh = HeNEXT(entry);
		else {
		    char *k;
		    entry = new_HE();
		    Newx(k, HEK_BASESIZE + sizeof(const SV *), char);
		    HeKEY_hek(entry) = (HEK*)k;
		}
		HeNEXT(entry) = NULL;
		HeSVKEY_set(entry, keysv);
		HeVAL(entry) = sv;
		sv_upgrade(sv, SVt_PVLV);
		LvTYPE(sv) = 'T';
		 /* so we can free entry when freeing sv */
		LvTARG(sv) = MUTABLE_SV(entry);

		/* XXX remove at some point? */
		if (flags & HVhek_FREEKEY)
		    Safefree(key);

		if (return_svp) {
		    return entry ? (void *) &HeVAL(entry) : NULL;
		}
		return (void *) entry;
	    }
#ifdef ENV_IS_CASELESS
	    else if (mg_find((const SV *)hv, PERL_MAGIC_env)) {
		U32 i;
		for (i = 0; i < klen; ++i)
		    if (isLOWER(key[i])) {
			/* Would be nice if we had a routine to do the
			   copy and upercase in a single pass through.  */
			const char * const nkey = strupr(savepvn(key,klen));
			/* Note that this fetch is for nkey (the uppercased
			   key) whereas the store is for key (the original)  */
			void *result = hv_common(hv, NULL, nkey, klen,
						 HVhek_FREEKEY, /* free nkey */
						 0 /* non-LVAL fetch */
						 | HV_DISABLE_UVAR_XKEY
						 | return_svp,
						 NULL /* no value */,
						 0 /* compute hash */);
			if (!result && (action & HV_FETCH_LVALUE)) {
			    /* This call will free key if necessary.
			       Do it this way to encourage compiler to tail
			       call optimise.  */
			    result = hv_common(hv, keysv, key, klen, flags,
					       HV_FETCH_ISSTORE
					       | HV_DISABLE_UVAR_XKEY
					       | return_svp,
					       newSV(0), hash);
			} else {
			    if (flags & HVhek_FREEKEY)
				Safefree(key);
			}
			return result;
		    }
	    }
#endif
	} /* ISFETCH */
	else if (SvRMAGICAL(hv) && (action & HV_FETCH_ISEXISTS)) {
	    if (mg_find((const SV *)hv, PERL_MAGIC_tied)
		|| SvGMAGICAL((const SV *)hv)) {
		/* I don't understand why hv_exists_ent has svret and sv,
		   whereas hv_exists only had one.  */
		SV * const svret = sv_newmortal();
		sv = sv_newmortal();

		if (keysv || is_utf8) {
		    if (!keysv) {
			keysv = newSVpvn_utf8(key, klen, TRUE);
		    } else {
			keysv = newSVsv(keysv);
		    }
		    mg_copy(MUTABLE_SV(hv), sv, (char *)sv_2mortal(keysv), HEf_SVKEY);
		} else {
		    mg_copy(MUTABLE_SV(hv), sv, key, klen);
		}
		if (flags & HVhek_FREEKEY)
		    Safefree(key);
		magic_existspack(svret, mg_find(sv, PERL_MAGIC_tiedelem));
		/* This cast somewhat evil, but I'm merely using NULL/
		   not NULL to return the boolean exists.
		   And I know hv is not NULL.  */
		return SvTRUE(svret) ? (void *)hv : NULL;
		}
#ifdef ENV_IS_CASELESS
	    else if (mg_find((const SV *)hv, PERL_MAGIC_env)) {
		/* XXX This code isn't UTF8 clean.  */
		char * const keysave = (char * const)key;
		/* Will need to free this, so set FREEKEY flag.  */
		key = savepvn(key,klen);
		key = (const char*)strupr((char*)key);
		is_utf8 = FALSE;
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
			keysv = newSVpvn_utf8(key, klen, TRUE);
		    }
		    if (PL_tainting)
			PL_tainted = SvTAINTED(keysv);
		    keysv = sv_2mortal(newSVsv(keysv));
		    mg_copy(MUTABLE_SV(hv), val, (char*)keysv, HEf_SVKEY);
		} else {
		    mg_copy(MUTABLE_SV(hv), val, key, klen);
		}

		TAINT_IF(save_taint);
		if (!needs_store) {
		    if (flags & HVhek_FREEKEY)
			Safefree(key);
		    return NULL;
		}
#ifdef ENV_IS_CASELESS
		else if (mg_find((const SV *)hv, PERL_MAGIC_env)) {
		    /* XXX This code isn't UTF8 clean.  */
		    const char *keysave = key;
		    /* Will need to free this, so set FREEKEY flag.  */
		    key = savepvn(key,klen);
		    key = (const char*)strupr((char*)key);
		    is_utf8 = FALSE;
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

    if (!HvARRAY(hv)) {
	if ((action & (HV_FETCH_LVALUE | HV_FETCH_ISSTORE))
#ifdef DYNAMIC_ENV_FETCH  /* if it's an %ENV lookup, we may get it on the fly */
		 || (SvRMAGICAL((const SV *)hv)
		     && mg_find((const SV *)hv, PERL_MAGIC_env))
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

	    return NULL;
	}
    }

    if (is_utf8 & !(flags & HVhek_KEYCANONICAL)) {
	char * const keysave = (char *)key;
	key = (char*)bytes_from_utf8((U8*)key, &klen, &is_utf8);
        if (is_utf8)
	    flags |= HVhek_UTF8;
	else
	    flags &= ~HVhek_UTF8;
        if (key != keysave) {
	    if (flags & HVhek_FREEKEY)
		Safefree(keysave);
            flags |= HVhek_WASUTF8 | HVhek_FREEKEY;
	    /* If the caller calculated a hash, it was on the sequence of
	       octets that are the UTF-8 form. We've now changed the sequence
	       of octets stored to that of the equivalent byte representation,
	       so the hash we need is different.  */
	    hash = 0;
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
        if (keysv && (SvIsCOW_shared_hash(keysv))) {
            hash = SvSHARED_HASH(keysv);
        } else {
            PERL_HASH(hash, key, klen);
        }
    }

    masked_flags = (flags & HVhek_MASK);

#ifdef DYNAMIC_ENV_FETCH
    if (!HvARRAY(hv)) entry = NULL;
    else
#endif
    {
	entry = (HvARRAY(hv))[hash & (I32) HvMAX(hv)];
    }
    for (; entry; entry = HeNEXT(entry)) {
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
		    HEK * const new_hek = share_hek_flags(key, klen, hash,
						   masked_flags);
		    unshare_hek (HeKEY_hek(entry));
		    HeKEY_hek(entry) = new_hek;
		}
		else if (hv == PL_strtab) {
		    /* PL_strtab is usually the only hash without HvSHAREKEYS,
		       so putting this test here is cheap  */
		    if (flags & HVhek_FREEKEY)
			Safefree(key);
		    Perl_croak(aTHX_ S_strtab_error,
			       action & HV_FETCH_LVALUE ? "fetch" : "store");
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
		    val = newSV(0);
		    HvPLACEHOLDERS(hv)--;
		} else {
		    /* store */
		    if (val != &PL_sv_placeholder)
			HvPLACEHOLDERS(hv)--;
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
	if (return_svp) {
	    return entry ? (void *) &HeVAL(entry) : NULL;
	}
	return entry;
    }
#ifdef DYNAMIC_ENV_FETCH  /* %ENV lookup?  If so, try to fetch the value now */
    if (!(action & HV_FETCH_ISSTORE) 
	&& SvRMAGICAL((const SV *)hv)
	&& mg_find((const SV *)hv, PERL_MAGIC_env)) {
	unsigned long len;
	const char * const env = PerlEnv_ENVgetenv_len(key,&len);
	if (env) {
	    sv = newSVpvn(env,len);
	    SvTAINTED_on(sv);
	    return hv_common(hv, keysv, key, klen, flags,
			     HV_FETCH_ISSTORE|HV_DISABLE_UVAR_XKEY|return_svp,
			     sv, hash);
	}
    }
#endif

    if (!entry && SvREADONLY(hv) && !(action & HV_FETCH_ISEXISTS)) {
	hv_notallowed(flags, key, klen,
			"Attempt to access disallowed key '%"SVf"' in"
			" a restricted hash");
    }
    if (!(action & (HV_FETCH_LVALUE|HV_FETCH_ISSTORE))) {
	/* Not doing some form of store, so return failure.  */
	if (flags & HVhek_FREEKEY)
	    Safefree(key);
	return NULL;
    }
    if (action & HV_FETCH_LVALUE) {
	val = newSV(0);
	if (SvMAGICAL(hv)) {
	    /* At this point the old hv_fetch code would call to hv_store,
	       which in turn might do some tied magic. So we need to make that
	       magic check happen.  */
	    /* gonna assign to this, so it better be there */
	    /* If a fetch-as-store fails on the fetch, then the action is to
	       recurse once into "hv_store". If we didn't do this, then that
	       recursive call would call the key conversion routine again.
	       However, as we replace the original key with the converted
	       key, this would result in a double conversion, which would show
	       up as a bug if the conversion routine is not idempotent.  */
	    return hv_common(hv, keysv, key, klen, flags,
			     HV_FETCH_ISSTORE|HV_DISABLE_UVAR_XKEY|return_svp,
			     val, hash);
	    /* XXX Surely that could leak if the fetch-was-store fails?
	       Just like the hv_fetch.  */
	}
    }

    /* Welcome to hv_store...  */

    if (!HvARRAY(hv)) {
	/* Not sure if we can get here.  I think the only case of oentry being
	   NULL is for %ENV with dynamic env fetch.  But that should disappear
	   with magic in the previous code.  */
	char *array;
	Newxz(array,
	     PERL_HV_ARRAY_ALLOC_BYTES(xhv->xhv_max+1 /* HvMAX(hv)+1 */),
	     char);
	HvARRAY(hv) = (HE**)array;
    }

    oentry = &(HvARRAY(hv))[hash & (I32) xhv->xhv_max];

    entry = new_HE();
    /* share_hek_flags will do the free for us.  This might be considered
       bad API design.  */
    if (HvSHAREKEYS(hv))
	HeKEY_hek(entry) = share_hek_flags(key, klen, hash, flags);
    else if (hv == PL_strtab) {
	/* PL_strtab is usually the only hash without HvSHAREKEYS, so putting
	   this test here is cheap  */
	if (flags & HVhek_FREEKEY)
	    Safefree(key);
	Perl_croak(aTHX_ S_strtab_error,
		   action & HV_FETCH_LVALUE ? "fetch" : "store");
    }
    else                                       /* gotta do the real thing */
	HeKEY_hek(entry) = save_hek_flags(key, klen, hash, flags);
    HeVAL(entry) = val;
    HeNEXT(entry) = *oentry;
    *oentry = entry;

    if (val == &PL_sv_placeholder)
	HvPLACEHOLDERS(hv)++;
    if (masked_flags & HVhek_ENABLEHVKFLAGS)
	HvHASKFLAGS_on(hv);

    {
	const HE *counter = HeNEXT(entry);

	xhv->xhv_keys++; /* HvTOTALKEYS(hv)++ */
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

    if (return_svp) {
	return entry ? (void *) &HeVAL(entry) : NULL;
    }
    return (void *) entry;
}

STATIC void
S_hv_magic_check(HV *hv, bool *needs_copy, bool *needs_store)
{
    const MAGIC *mg = SvMAGIC(hv);

    PERL_ARGS_ASSERT_HV_MAGIC_CHECK;

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

    PERL_ARGS_ASSERT_HV_SCALAR;

    if (SvRMAGICAL(hv)) {
	MAGIC * const mg = mg_find((const SV *)hv, PERL_MAGIC_tied);
	if (mg)
	    return magic_scalarpack(hv, mg);
    }

    sv = sv_newmortal();
    if (HvFILL((const HV *)hv)) 
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

=for apidoc hv_delete_ent

Deletes a key/value pair in the hash.  The value SV is removed from the
hash and returned to the caller.  The C<flags> value will normally be zero;
if set to G_DISCARD then NULL will be returned.  C<hash> can be a valid
precomputed hash value, or 0 to ask for it to be computed.

=cut
*/

STATIC SV *
S_hv_delete_common(pTHX_ HV *hv, SV *keysv, const char *key, STRLEN klen,
		   int k_flags, I32 d_flags, U32 hash)
{
    dVAR;
    register XPVHV* xhv;
    register HE *entry;
    register HE **oentry;
    HE *const *first_entry;
    bool is_utf8 = (k_flags & HVhek_UTF8) ? TRUE : FALSE;
    int masked_flags;

    if (SvRMAGICAL(hv)) {
	bool needs_copy;
	bool needs_store;
	hv_magic_check (hv, &needs_copy, &needs_store);

	if (needs_copy) {
	    SV *sv;
	    entry = (HE *) hv_common(hv, keysv, key, klen,
				     k_flags & ~HVhek_FREEKEY,
				     HV_FETCH_LVALUE|HV_DISABLE_UVAR_XKEY,
				     NULL, hash);
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
		    return NULL;		/* element cannot be deleted */
		}
#ifdef ENV_IS_CASELESS
		else if (mg_find((const SV *)hv, PERL_MAGIC_env)) {
		    /* XXX This code isn't UTF8 clean.  */
		    keysv = newSVpvn_flags(key, klen, SVs_TEMP);
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
    if (!HvARRAY(hv))
	return NULL;

    if (is_utf8) {
	const char * const keysave = key;
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
        HvHASKFLAGS_on(MUTABLE_SV(hv));
    }

    if (HvREHASH(hv)) {
	PERL_HASH_INTERNAL(hash, key, klen);
    } else if (!hash) {
        if (keysv && (SvIsCOW_shared_hash(keysv))) {
            hash = SvSHARED_HASH(keysv);
        } else {
            PERL_HASH(hash, key, klen);
        }
    }

    masked_flags = (k_flags & HVhek_MASK);

    first_entry = oentry = &(HvARRAY(hv))[hash & (I32) HvMAX(hv)];
    entry = *oentry;
    for (; entry; oentry = &HeNEXT(entry), entry = *oentry) {
	SV *sv;
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != (I32)klen)
	    continue;
	if (HeKEY(entry) != key && memNE(HeKEY(entry),key,klen))	/* is this it? */
	    continue;
	if ((HeKFLAGS(entry) ^ masked_flags) & HVhek_UTF8)
	    continue;

	if (hv == PL_strtab) {
	    if (k_flags & HVhek_FREEKEY)
		Safefree(key);
	    Perl_croak(aTHX_ S_strtab_error, "delete");
	}

	/* if placeholder is here, it's already been deleted.... */
	if (HeVAL(entry) == &PL_sv_placeholder) {
	    if (k_flags & HVhek_FREEKEY)
		Safefree(key);
	    return NULL;
	}
	if (SvREADONLY(hv) && HeVAL(entry) && SvREADONLY(HeVAL(entry))) {
	    hv_notallowed(k_flags, key, klen,
			    "Attempt to delete readonly key '%"SVf"' from"
			    " a restricted hash");
	}
        if (k_flags & HVhek_FREEKEY)
            Safefree(key);

	if (d_flags & G_DISCARD)
	    sv = NULL;
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
	    HvPLACEHOLDERS(hv)++;
	} else {
	    *oentry = HeNEXT(entry);
	    if(!*first_entry) {
		xhv->xhv_fill--; /* HvFILL(hv)-- */
	    }
	    if (SvOOK(hv) && entry == HvAUX(hv)->xhv_eiter /* HvEITER(hv) */)
		HvLAZYDEL_on(hv);
	    else
		hv_free_ent(hv, entry);
	    xhv->xhv_keys--; /* HvTOTALKEYS(hv)-- */
	    if (xhv->xhv_keys == 0)
	        HvHASKFLAGS_off(hv);
	}
	return sv;
    }
    if (SvREADONLY(hv)) {
	hv_notallowed(k_flags, key, klen,
			"Attempt to delete disallowed key '%"SVf"' from"
			" a restricted hash");
    }

    if (k_flags & HVhek_FREEKEY)
	Safefree(key);
    return NULL;
}

STATIC void
S_hsplit(pTHX_ HV *hv)
{
    dVAR;
    register XPVHV* const xhv = (XPVHV*)SvANY(hv);
    const I32 oldsize = (I32) xhv->xhv_max+1; /* HvMAX(hv)+1 (sick) */
    register I32 newsize = oldsize * 2;
    register I32 i;
    char *a = (char*) HvARRAY(hv);
    register HE **aep;
    register HE **oentry;
    int longest_chain = 0;
    int was_shared;

    PERL_ARGS_ASSERT_HSPLIT;

    /*PerlIO_printf(PerlIO_stderr(), "hsplit called for %p which had %d\n",
      (void*)hv, (int) oldsize);*/

    if (HvPLACEHOLDERS_get(hv) && !SvREADONLY(hv)) {
      /* Can make this clear any placeholders first for non-restricted hashes,
	 even though Storable rebuilds restricted hashes by putting in all the
	 placeholders (first) before turning on the readonly flag, because
	 Storable always pre-splits the hash.  */
      hv_clear_placeholders(hv);
    }
	       
    PL_nomemok = TRUE;
#if defined(STRANGE_MALLOC) || defined(MYMALLOC)
    Renew(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize)
	  + (SvOOK(hv) ? sizeof(struct xpvhv_aux) : 0), char);
    if (!a) {
      PL_nomemok = FALSE;
      return;
    }
    if (SvOOK(hv)) {
	Move(&a[oldsize * sizeof(HE*)], &a[newsize * sizeof(HE*)], 1, struct xpvhv_aux);
    }
#else
    Newx(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize)
	+ (SvOOK(hv) ? sizeof(struct xpvhv_aux) : 0), char);
    if (!a) {
      PL_nomemok = FALSE;
      return;
    }
    Copy(HvARRAY(hv), a, oldsize * sizeof(HE*), char);
    if (SvOOK(hv)) {
	Copy(HvAUX(hv), &a[newsize * sizeof(HE*)], 1, struct xpvhv_aux);
    }
    if (oldsize >= 64) {
	offer_nice_chunk(HvARRAY(hv),
			 PERL_HV_ARRAY_ALLOC_BYTES(oldsize)
			 + (SvOOK(hv) ? sizeof(struct xpvhv_aux) : 0));
    }
    else
	Safefree(HvARRAY(hv));
#endif

    PL_nomemok = FALSE;
    Zero(&a[oldsize * sizeof(HE*)], (newsize-oldsize) * sizeof(HE*), char);	/* zero 2nd half*/
    xhv->xhv_max = --newsize;	/* HvMAX(hv) = --newsize */
    HvARRAY(hv) = (HE**) a;
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
    /*PerlIO_printf(PerlIO_stderr(), "%p %d of %d with %d/%d buckets\n", (void*)hv,
      longest_chain, HvTOTALKEYS(hv), HvFILL(hv),  1+HvMAX(hv));*/

    ++newsize;
    Newxz(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize)
	 + (SvOOK(hv) ? sizeof(struct xpvhv_aux) : 0), char);
    if (SvOOK(hv)) {
	Copy(HvAUX(hv), &a[newsize * sizeof(HE*)], 1, struct xpvhv_aux);
    }

    was_shared = HvSHAREKEYS(hv);

    xhv->xhv_fill = 0;
    HvSHAREKEYS_off(hv);
    HvREHASH_on(hv);

    aep = HvARRAY(hv);

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
    Safefree (HvARRAY(hv));
    HvARRAY(hv) = (HE **)a;
}

void
Perl_hv_ksplit(pTHX_ HV *hv, IV newmax)
{
    dVAR;
    register XPVHV* xhv = (XPVHV*)SvANY(hv);
    const I32 oldsize = (I32) xhv->xhv_max+1; /* HvMAX(hv)+1 (sick) */
    register I32 newsize;
    register I32 i;
    register char *a;
    register HE **aep;
    register HE *entry;
    register HE **oentry;

    PERL_ARGS_ASSERT_HV_KSPLIT;

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

    a = (char *) HvARRAY(hv);
    if (a) {
	PL_nomemok = TRUE;
#if defined(STRANGE_MALLOC) || defined(MYMALLOC)
	Renew(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize)
	      + (SvOOK(hv) ? sizeof(struct xpvhv_aux) : 0), char);
	if (!a) {
	  PL_nomemok = FALSE;
	  return;
	}
	if (SvOOK(hv)) {
	    Copy(&a[oldsize * sizeof(HE*)], &a[newsize * sizeof(HE*)], 1, struct xpvhv_aux);
	}
#else
	Newx(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize)
	    + (SvOOK(hv) ? sizeof(struct xpvhv_aux) : 0), char);
	if (!a) {
	  PL_nomemok = FALSE;
	  return;
	}
	Copy(HvARRAY(hv), a, oldsize * sizeof(HE*), char);
	if (SvOOK(hv)) {
	    Copy(HvAUX(hv), &a[newsize * sizeof(HE*)], 1, struct xpvhv_aux);
	}
	if (oldsize >= 64) {
	    offer_nice_chunk(HvARRAY(hv),
			     PERL_HV_ARRAY_ALLOC_BYTES(oldsize)
			     + (SvOOK(hv) ? sizeof(struct xpvhv_aux) : 0));
	}
	else
	    Safefree(HvARRAY(hv));
#endif
	PL_nomemok = FALSE;
	Zero(&a[oldsize * sizeof(HE*)], (newsize-oldsize) * sizeof(HE*), char); /* zero 2nd half*/
    }
    else {
	Newxz(a, PERL_HV_ARRAY_ALLOC_BYTES(newsize), char);
    }
    xhv->xhv_max = --newsize; 	/* HvMAX(hv) = --newsize */
    HvARRAY(hv) = (HE **) a;
    if (!xhv->xhv_fill /* !HvFILL(hv) */)	/* skip rest if no entries */
	return;

    aep = (HE**)a;
    for (i=0; i<oldsize; i++,aep++) {
	if (!*aep)				/* non-existent */
	    continue;
	for (oentry = aep, entry = *aep; entry; entry = *oentry) {
	    register I32 j = (HeHASH(entry) & newsize);

	    if (j != i) {
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

HV *
Perl_newHVhv(pTHX_ HV *ohv)
{
    dVAR;
    HV * const hv = newHV();
    STRLEN hv_max, hv_fill;

    if (!ohv || (hv_fill = HvFILL(ohv)) == 0)
	return hv;
    hv_max = HvMAX(ohv);

    if (!SvMAGICAL((const SV *)ohv)) {
	/* It's an ordinary hash, so copy it fast. AMS 20010804 */
	STRLEN i;
	const bool shared = !!HvSHAREKEYS(ohv);
	HE **ents, ** const oents = (HE **)HvARRAY(ohv);
	char *a;
	Newx(a, PERL_HV_ARRAY_ALLOC_BYTES(hv_max+1), char);
	ents = (HE**)a;

	/* In each bucket... */
	for (i = 0; i <= hv_max; i++) {
	    HE *prev = NULL;
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
		HE * const ent   = new_HE();
		SV *const val    = HeVAL(oent);

		HeVAL(ent) = SvIMMORTAL(val) ? val : newSVsv(val);
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
	    SV *const val = HeVAL(entry);
	    (void)hv_store_flags(hv, HeKEY(entry), HeKLEN(entry),
			         SvIMMORTAL(val) ? val : newSVsv(val),
				 HeHASH(entry), HeKFLAGS(entry));
	}
	HvRITER_set(ohv, riter);
	HvEITER_set(ohv, eiter);
    }

    return hv;
}

/* A rather specialised version of newHVhv for copying %^H, ensuring all the
   magic stays on it.  */
HV *
Perl_hv_copy_hints_hv(pTHX_ HV *const ohv)
{
    HV * const hv = newHV();
    STRLEN hv_fill;

    if (ohv && (hv_fill = HvFILL(ohv))) {
	STRLEN hv_max = HvMAX(ohv);
	HE *entry;
	const I32 riter = HvRITER_get(ohv);
	HE * const eiter = HvEITER_get(ohv);

	while (hv_max && hv_max + 1 >= hv_fill * 2)
	    hv_max = hv_max / 2;
	HvMAX(hv) = hv_max;

	hv_iterinit(ohv);
	while ((entry = hv_iternext_flags(ohv, 0))) {
	    SV *const sv = newSVsv(HeVAL(entry));
	    SV *heksv = newSVhek(HeKEY_hek(entry));
	    sv_magic(sv, NULL, PERL_MAGIC_hintselem,
		     (char *)heksv, HEf_SVKEY);
	    SvREFCNT_dec(heksv);
	    (void)hv_store_flags(hv, HeKEY(entry), HeKLEN(entry),
				 sv, HeHASH(entry), HeKFLAGS(entry));
	}
	HvRITER_set(ohv, riter);
	HvEITER_set(ohv, eiter);
    }
    hv_magic(hv, NULL, PERL_MAGIC_hints);
    return hv;
}

void
Perl_hv_free_ent(pTHX_ HV *hv, register HE *entry)
{
    dVAR;
    SV *val;

    PERL_ARGS_ASSERT_HV_FREE_ENT;

    if (!entry)
	return;
    val = HeVAL(entry);
    if (HvNAME(hv) && anonymise_cv(HvNAME_HEK(hv), val) && GvCVu(val))
	mro_method_changed_in(hv);
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

static I32
S_anonymise_cv(pTHX_ HEK *stash, SV *val)
{
    CV *cv;

    PERL_ARGS_ASSERT_ANONYMISE_CV;

    if (val && isGV(val) && isGV_with_GP(val) && (cv = GvCV(val))) {
	if ((SV *)CvGV(cv) == val) {
	    GV *anongv;

	    if (stash) {
		SV *gvname = newSVhek(stash);
		sv_catpvs(gvname, "::__ANON__");
		anongv = gv_fetchsv(gvname, GV_ADDMULTI, SVt_PVCV);
		SvREFCNT_dec(gvname);
	    } else {
		anongv = gv_fetchpvs("__ANON__::__ANON__", GV_ADDMULTI,
				     SVt_PVCV);
	    }
	    CvGV(cv) = anongv;
	    CvANON_on(cv);
	    return 1;
	}
    }
    return 0;
}

void
Perl_hv_delayfree_ent(pTHX_ HV *hv, register HE *entry)
{
    dVAR;

    PERL_ARGS_ASSERT_HV_DELAYFREE_ENT;

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
    dVAR;
    register XPVHV* xhv;
    if (!hv)
	return;

    DEBUG_A(Perl_hv_assert(aTHX_ hv));

    xhv = (XPVHV*)SvANY(hv);

    if (SvREADONLY(hv) && HvARRAY(hv) != NULL) {
	/* restricted hash: convert all keys to placeholders */
	STRLEN i;
	for (i = 0; i <= xhv->xhv_max; i++) {
	    HE *entry = (HvARRAY(hv))[i];
	    for (; entry; entry = HeNEXT(entry)) {
		/* not already placeholder */
		if (HeVAL(entry) != &PL_sv_placeholder) {
		    if (HeVAL(entry) && SvREADONLY(HeVAL(entry))) {
			SV* const keysv = hv_iterkeysv(entry);
			Perl_croak(aTHX_
				   "Attempt to delete readonly key '%"SVf"' from a restricted hash",
				   (void*)keysv);
		    }
		    SvREFCNT_dec(HeVAL(entry));
		    HeVAL(entry) = &PL_sv_placeholder;
		    HvPLACEHOLDERS(hv)++;
		}
	    }
	}
	goto reset;
    }

    hfreeentries(hv);
    HvPLACEHOLDERS_set(hv, 0);
    if (HvARRAY(hv))
	Zero(HvARRAY(hv), xhv->xhv_max+1 /* HvMAX(hv)+1 */, HE*);

    if (SvRMAGICAL(hv))
	mg_clear(MUTABLE_SV(hv));

    HvHASKFLAGS_off(hv);
    HvREHASH_off(hv);
    reset:
    if (SvOOK(hv)) {
        if(HvNAME_get(hv))
            mro_isa_changed_in(hv);
	HvEITER_set(hv, NULL);
    }
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
    dVAR;
    const U32 items = (U32)HvPLACEHOLDERS_get(hv);

    PERL_ARGS_ASSERT_HV_CLEAR_PLACEHOLDERS;

    if (items)
	clear_placeholders(hv, items);
}

static void
S_clear_placeholders(pTHX_ HV *hv, U32 items)
{
    dVAR;
    I32 i;

    PERL_ARGS_ASSERT_CLEAR_PLACEHOLDERS;

    if (items == 0)
	return;

    i = HvMAX(hv);
    do {
	/* Loop down the linked list heads  */
	bool first = TRUE;
	HE **oentry = &(HvARRAY(hv))[i];
	HE *entry;

	while ((entry = *oentry)) {
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
		first = FALSE;
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
    /* This is the array that we're going to restore  */
    HE **const orig_array = HvARRAY(hv);
    HEK *name;
    int attempts = 100;

    PERL_ARGS_ASSERT_HFREEENTRIES;

    if (!orig_array)
	return;

    if (HvNAME(hv) && orig_array != NULL) {
	/* symbol table: make all the contained subs ANON */
	STRLEN i;
	XPVHV *xhv = (XPVHV*)SvANY(hv);

	for (i = 0; i <= xhv->xhv_max; i++) {
	    HE *entry = (HvARRAY(hv))[i];
	    for (; entry; entry = HeNEXT(entry)) {
		SV *val = HeVAL(entry);
		/* we need to put the subs in the __ANON__ symtable, as
		 * this one is being cleared. */
		anonymise_cv(NULL, val);
	    }
	}
    }

    if (SvOOK(hv)) {
	/* If the hash is actually a symbol table with a name, look after the
	   name.  */
	struct xpvhv_aux *iter = HvAUX(hv);

	name = iter->xhv_name;
	iter->xhv_name = NULL;
    } else {
	name = NULL;
    }

    /* orig_array remains unchanged throughout the loop. If after freeing all
       the entries it turns out that one of the little blighters has triggered
       an action that has caused HvARRAY to be re-allocated, then we set
       array to the new HvARRAY, and try again.  */

    while (1) {
	/* This is the one we're going to try to empty.  First time round
	   it's the original array.  (Hopefully there will only be 1 time
	   round) */
	HE ** const array = HvARRAY(hv);
	I32 i = HvMAX(hv);

	/* Because we have taken xhv_name out, the only allocated pointer
	   in the aux structure that might exist is the backreference array.
	*/

	if (SvOOK(hv)) {
	    HE *entry;
            struct mro_meta *meta;
	    struct xpvhv_aux *iter = HvAUX(hv);
	    /* If there are weak references to this HV, we need to avoid
	       freeing them up here.  In particular we need to keep the AV
	       visible as what we're deleting might well have weak references
	       back to this HV, so the for loop below may well trigger
	       the removal of backreferences from this array.  */

	    if (iter->xhv_backreferences) {
		/* So donate them to regular backref magic to keep them safe.
		   The sv_magic will increase the reference count of the AV,
		   so we need to drop it first. */
		SvREFCNT_dec(iter->xhv_backreferences);
		if (AvFILLp(iter->xhv_backreferences) == -1) {
		    /* Turns out that the array is empty. Just free it.  */
		    SvREFCNT_dec(iter->xhv_backreferences);

		} else {
		    sv_magic(MUTABLE_SV(hv),
			     MUTABLE_SV(iter->xhv_backreferences),
			     PERL_MAGIC_backref, NULL, 0);
		}
		iter->xhv_backreferences = NULL;
	    }

	    entry = iter->xhv_eiter; /* HvEITER(hv) */
	    if (entry && HvLAZYDEL(hv)) {	/* was deleted earlier? */
		HvLAZYDEL_off(hv);
		hv_free_ent(hv, entry);
	    }
	    iter->xhv_riter = -1; 	/* HvRITER(hv) = -1 */
	    iter->xhv_eiter = NULL;	/* HvEITER(hv) = NULL */

            if((meta = iter->xhv_mro_meta)) {
		if (meta->mro_linear_all) {
		    SvREFCNT_dec(MUTABLE_SV(meta->mro_linear_all));
		    meta->mro_linear_all = NULL;
		    /* This is just acting as a shortcut pointer.  */
		    meta->mro_linear_current = NULL;
		} else if (meta->mro_linear_current) {
		    /* Only the current MRO is stored, so this owns the data.
		     */
		    SvREFCNT_dec(meta->mro_linear_current);
		    meta->mro_linear_current = NULL;
		}
                if(meta->mro_nextmethod) SvREFCNT_dec(meta->mro_nextmethod);
                SvREFCNT_dec(meta->isa);
                Safefree(meta);
                iter->xhv_mro_meta = NULL;
            }

	    /* There are now no allocated pointers in the aux structure.  */

	    SvFLAGS(hv) &= ~SVf_OOK; /* Goodbye, aux structure.  */
	    /* What aux structure?  */
	}

	/* make everyone else think the array is empty, so that the destructors
	 * called for freed entries can't recusively mess with us */
	HvARRAY(hv) = NULL;
	HvFILL(hv) = 0;
	((XPVHV*) SvANY(hv))->xhv_keys = 0;


	do {
	    /* Loop down the linked list heads  */
	    HE *entry = array[i];

	    while (entry) {
		register HE * const oentry = entry;
		entry = HeNEXT(entry);
		hv_free_ent(hv, oentry);
	    }
	} while (--i >= 0);

	/* As there are no allocated pointers in the aux structure, it's now
	   safe to free the array we just cleaned up, if it's not the one we're
	   going to put back.  */
	if (array != orig_array) {
	    Safefree(array);
	}

	if (!HvARRAY(hv)) {
	    /* Good. No-one added anything this time round.  */
	    break;
	}

	if (SvOOK(hv)) {
	    /* Someone attempted to iterate or set the hash name while we had
	       the array set to 0.  We'll catch backferences on the next time
	       round the while loop.  */
	    assert(HvARRAY(hv));

	    if (HvAUX(hv)->xhv_name) {
		unshare_hek_or_pvn(HvAUX(hv)->xhv_name, 0, 0, 0);
	    }
	}

	if (--attempts == 0) {
	    Perl_die(aTHX_ "panic: hfreeentries failed to free hash - something is repeatedly re-creating entries");
	}
    }
	
    HvARRAY(hv) = orig_array;

    /* If the hash was actually a symbol table, put the name back.  */
    if (name) {
	/* We have restored the original array.  If name is non-NULL, then
	   the original array had an aux structure at the end. So this is
	   valid:  */
	SvFLAGS(hv) |= SVf_OOK;
	HvAUX(hv)->xhv_name = name;
    }
}

/*
=for apidoc hv_undef

Undefines the hash.

=cut
*/

void
Perl_hv_undef(pTHX_ HV *hv)
{
    dVAR;
    register XPVHV* xhv;
    const char *name;

    if (!hv)
	return;
    DEBUG_A(Perl_hv_assert(aTHX_ hv));
    xhv = (XPVHV*)SvANY(hv);

    if ((name = HvNAME_get(hv)) && !PL_dirty)
        mro_isa_changed_in(hv);

    hfreeentries(hv);
    if (name) {
        if (PL_stashcache)
	    (void)hv_delete(PL_stashcache, name, HvNAMELEN_get(hv), G_DISCARD);
	hv_name_set(hv, NULL, 0, 0);
    }
    SvFLAGS(hv) &= ~SVf_OOK;
    Safefree(HvARRAY(hv));
    xhv->xhv_max   = 7;	/* HvMAX(hv) = 7 (it's a normal hash) */
    HvARRAY(hv) = 0;
    HvPLACEHOLDERS_set(hv, 0);

    if (SvRMAGICAL(hv))
	mg_clear(MUTABLE_SV(hv));
}

static struct xpvhv_aux*
S_hv_auxinit(HV *hv) {
    struct xpvhv_aux *iter;
    char *array;

    PERL_ARGS_ASSERT_HV_AUXINIT;

    if (!HvARRAY(hv)) {
	Newxz(array, PERL_HV_ARRAY_ALLOC_BYTES(HvMAX(hv) + 1)
	    + sizeof(struct xpvhv_aux), char);
    } else {
	array = (char *) HvARRAY(hv);
	Renew(array, PERL_HV_ARRAY_ALLOC_BYTES(HvMAX(hv) + 1)
	      + sizeof(struct xpvhv_aux), char);
    }
    HvARRAY(hv) = (HE**) array;
    /* SvOOK_on(hv) attacks the IV flags.  */
    SvFLAGS(hv) |= SVf_OOK;
    iter = HvAUX(hv);

    iter->xhv_riter = -1; 	/* HvRITER(hv) = -1 */
    iter->xhv_eiter = NULL;	/* HvEITER(hv) = NULL */
    iter->xhv_name = 0;
    iter->xhv_backreferences = 0;
    iter->xhv_mro_meta = NULL;
    return iter;
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
    PERL_ARGS_ASSERT_HV_ITERINIT;

    /* FIXME: Are we not NULL, or do we croak? Place bets now! */

    if (!hv)
	Perl_croak(aTHX_ "Bad hash");

    if (SvOOK(hv)) {
	struct xpvhv_aux * const iter = HvAUX(hv);
	HE * const entry = iter->xhv_eiter; /* HvEITER(hv) */
	if (entry && HvLAZYDEL(hv)) {	/* was deleted earlier? */
	    HvLAZYDEL_off(hv);
	    hv_free_ent(hv, entry);
	}
	iter->xhv_riter = -1; 	/* HvRITER(hv) = -1 */
	iter->xhv_eiter = NULL; /* HvEITER(hv) = NULL */
    } else {
	hv_auxinit(hv);
    }

    /* used to be xhv->xhv_fill before 5.004_65 */
    return HvTOTALKEYS(hv);
}

I32 *
Perl_hv_riter_p(pTHX_ HV *hv) {
    struct xpvhv_aux *iter;

    PERL_ARGS_ASSERT_HV_RITER_P;

    if (!hv)
	Perl_croak(aTHX_ "Bad hash");

    iter = SvOOK(hv) ? HvAUX(hv) : hv_auxinit(hv);
    return &(iter->xhv_riter);
}

HE **
Perl_hv_eiter_p(pTHX_ HV *hv) {
    struct xpvhv_aux *iter;

    PERL_ARGS_ASSERT_HV_EITER_P;

    if (!hv)
	Perl_croak(aTHX_ "Bad hash");

    iter = SvOOK(hv) ? HvAUX(hv) : hv_auxinit(hv);
    return &(iter->xhv_eiter);
}

void
Perl_hv_riter_set(pTHX_ HV *hv, I32 riter) {
    struct xpvhv_aux *iter;

    PERL_ARGS_ASSERT_HV_RITER_SET;

    if (!hv)
	Perl_croak(aTHX_ "Bad hash");

    if (SvOOK(hv)) {
	iter = HvAUX(hv);
    } else {
	if (riter == -1)
	    return;

	iter = hv_auxinit(hv);
    }
    iter->xhv_riter = riter;
}

void
Perl_hv_eiter_set(pTHX_ HV *hv, HE *eiter) {
    struct xpvhv_aux *iter;

    PERL_ARGS_ASSERT_HV_EITER_SET;

    if (!hv)
	Perl_croak(aTHX_ "Bad hash");

    if (SvOOK(hv)) {
	iter = HvAUX(hv);
    } else {
	/* 0 is the default so don't go malloc()ing a new structure just to
	   hold 0.  */
	if (!eiter)
	    return;

	iter = hv_auxinit(hv);
    }
    iter->xhv_eiter = eiter;
}

void
Perl_hv_name_set(pTHX_ HV *hv, const char *name, U32 len, U32 flags)
{
    dVAR;
    struct xpvhv_aux *iter;
    U32 hash;

    PERL_ARGS_ASSERT_HV_NAME_SET;
    PERL_UNUSED_ARG(flags);

    if (len > I32_MAX)
	Perl_croak(aTHX_ "panic: hv name too long (%"UVuf")", (UV) len);

    if (SvOOK(hv)) {
	iter = HvAUX(hv);
	if (iter->xhv_name) {
	    unshare_hek_or_pvn(iter->xhv_name, 0, 0, 0);
	}
    } else {
	if (name == 0)
	    return;

	iter = hv_auxinit(hv);
    }
    PERL_HASH(hash, name, len);
    iter->xhv_name = name ? share_hek(name, len, hash) : NULL;
}

AV **
Perl_hv_backreferences_p(pTHX_ HV *hv) {
    struct xpvhv_aux * const iter = SvOOK(hv) ? HvAUX(hv) : hv_auxinit(hv);

    PERL_ARGS_ASSERT_HV_BACKREFERENCES_P;
    PERL_UNUSED_CONTEXT;

    return &(iter->xhv_backreferences);
}

void
Perl_hv_kill_backrefs(pTHX_ HV *hv) {
    AV *av;

    PERL_ARGS_ASSERT_HV_KILL_BACKREFS;

    if (!SvOOK(hv))
	return;

    av = HvAUX(hv)->xhv_backreferences;

    if (av) {
	HvAUX(hv)->xhv_backreferences = 0;
	Perl_sv_kill_backrefs(aTHX_ MUTABLE_SV(hv), av);
	SvREFCNT_dec(av);
    }
}

/*
hv_iternext is implemented as a macro in hv.h

=for apidoc hv_iternext

Returns entries from a hash iterator.  See C<hv_iterinit>.

You may call C<hv_delete> or C<hv_delete_ent> on the hash entry that the
iterator currently points to, without losing your place or invalidating your
iterator.  Note that in this case the current entry is deleted from the hash
with your iterator holding the last reference to it.  Your iterator is flagged
to free the entry on the next call to C<hv_iternext>, so you must not discard
your iterator immediately else the entry will leak - call C<hv_iternext> to
trigger the resource deallocation.

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
    dVAR;
    register XPVHV* xhv;
    register HE *entry;
    HE *oldentry;
    MAGIC* mg;
    struct xpvhv_aux *iter;

    PERL_ARGS_ASSERT_HV_ITERNEXT_FLAGS;

    if (!hv)
	Perl_croak(aTHX_ "Bad hash");

    xhv = (XPVHV*)SvANY(hv);

    if (!SvOOK(hv)) {
	/* Too many things (well, pp_each at least) merrily assume that you can
	   call iv_iternext without calling hv_iterinit, so we'll have to deal
	   with it.  */
	hv_iterinit(hv);
    }
    iter = HvAUX(hv);

    oldentry = entry = iter->xhv_eiter; /* HvEITER(hv) */
    if (SvMAGICAL(hv) && SvRMAGICAL(hv)) {
	if ( ( mg = mg_find((const SV *)hv, PERL_MAGIC_tied) ) ) {
            SV * const key = sv_newmortal();
            if (entry) {
                sv_setsv(key, HeSVKEY_force(entry));
                SvREFCNT_dec(HeSVKEY(entry));       /* get rid of previous key */
            }
            else {
                char *k;
                HEK *hek;

                /* one HE per MAGICAL hash */
                iter->xhv_eiter = entry = new_HE(); /* HvEITER(hv) = new_HE() */
                Zero(entry, 1, HE);
                Newxz(k, HEK_BASESIZE + sizeof(const SV *), char);
                hek = (HEK*)k;
                HeKEY_hek(entry) = hek;
                HeKLEN(entry) = HEf_SVKEY;
            }
            magic_nextpack(MUTABLE_SV(hv),mg,key);
            if (SvOK(key)) {
                /* force key to stay around until next time */
                HeSVKEY_set(entry, SvREFCNT_inc_simple_NN(key));
                return entry;               /* beware, hent_val is not set */
            }
            SvREFCNT_dec(HeVAL(entry));
            Safefree(HeKEY_hek(entry));
            del_HE(entry);
            iter->xhv_eiter = NULL; /* HvEITER(hv) = NULL */
            return NULL;
        }
    }
#if defined(DYNAMIC_ENV_FETCH) && !defined(__riscos__)  /* set up %ENV for iteration */
    if (!entry && SvRMAGICAL((const SV *)hv)
	&& mg_find((const SV *)hv, PERL_MAGIC_env)) {
	prime_env_iter();
#ifdef VMS
	/* The prime_env_iter() on VMS just loaded up new hash values
	 * so the iteration count needs to be reset back to the beginning
	 */
	hv_iterinit(hv);
	iter = HvAUX(hv);
	oldentry = entry = iter->xhv_eiter; /* HvEITER(hv) */
#endif
    }
#endif

    /* hv_iterint now ensures this.  */
    assert (HvARRAY(hv));

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

    /* Skip the entire loop if the hash is empty.   */
    if ((flags & HV_ITERNEXT_WANTPLACEHOLDERS)
	? HvTOTALKEYS(hv) : HvUSEDKEYS(hv)) {
	while (!entry) {
	    /* OK. Come to the end of the current list.  Grab the next one.  */

	    iter->xhv_riter++; /* HvRITER(hv)++ */
	    if (iter->xhv_riter > (I32)xhv->xhv_max /* HvRITER(hv) > HvMAX(hv) */) {
		/* There is no next one.  End of the hash.  */
		iter->xhv_riter = -1; /* HvRITER(hv) = -1 */
		break;
	    }
	    entry = (HvARRAY(hv))[iter->xhv_riter];

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
    }

    if (oldentry && HvLAZYDEL(hv)) {		/* was deleted earlier? */
	HvLAZYDEL_off(hv);
	hv_free_ent(hv, oldentry);
    }

    /*if (HvREHASH(hv) && entry && !HeKREHASH(entry))
      PerlIO_printf(PerlIO_stderr(), "Awooga %p %p\n", (void*)hv, (void*)entry);*/

    iter->xhv_eiter = entry; /* HvEITER(hv) = entry */
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
    PERL_ARGS_ASSERT_HV_ITERKEY;

    if (HeKLEN(entry) == HEf_SVKEY) {
	STRLEN len;
	char * const p = SvPV(HeKEY_sv(entry), len);
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
    PERL_ARGS_ASSERT_HV_ITERKEYSV;

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
    PERL_ARGS_ASSERT_HV_ITERVAL;

    if (SvRMAGICAL(hv)) {
	if (mg_find((const SV *)hv, PERL_MAGIC_tied)) {
	    SV* const sv = sv_newmortal();
	    if (HeKLEN(entry) == HEf_SVKEY)
		mg_copy(MUTABLE_SV(hv), sv, (char*)HeKEY_sv(entry), HEf_SVKEY);
	    else
		mg_copy(MUTABLE_SV(hv), sv, HeKEY(entry), HeKLEN(entry));
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
    HE * const he = hv_iternext_flags(hv, 0);

    PERL_ARGS_ASSERT_HV_ITERNEXTSV;

    if (!he)
	return NULL;
    *key = hv_iterkey(he, retlen);
    return hv_iterval(hv, he);
}

/*

Now a macro in hv.h

=for apidoc hv_magic

Adds magic to a hash.  See C<sv_magic>.

=cut
*/

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
    assert(hek);
    unshare_hek_or_pvn(hek, NULL, 0, 0);
}

/* possibly free a shared string if no one has access to it
   hek if non-NULL takes priority over the other 3, else str, len and hash
   are used.  If so, len and hash must both be valid for str.
 */
STATIC void
S_unshare_hek_or_pvn(pTHX_ const HEK *hek, const char *str, I32 len, U32 hash)
{
    dVAR;
    register XPVHV* xhv;
    HE *entry;
    register HE **oentry;
    HE **first;
    bool is_utf8 = FALSE;
    int k_flags = 0;
    const char * const save = str;
    struct shared_he *he = NULL;

    if (hek) {
	/* Find the shared he which is just before us in memory.  */
	he = (struct shared_he *)(((char *)hek)
				  - STRUCT_OFFSET(struct shared_he,
						  shared_he_hek));

	/* Assert that the caller passed us a genuine (or at least consistent)
	   shared hek  */
	assert (he->shared_he_he.hent_hek == hek);

	if (he->shared_he_he.he_valu.hent_refcount - 1) {
	    --he->shared_he_he.he_valu.hent_refcount;
	    return;
	}

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

    /* what follows was the moral equivalent of:
    if ((Svp = hv_fetch(PL_strtab, tmpsv, FALSE, hash))) {
	if (--*Svp == NULL)
	    hv_delete(PL_strtab, str, len, G_DISCARD, hash);
    } */
    xhv = (XPVHV*)SvANY(PL_strtab);
    /* assert(xhv_array != 0) */
    first = oentry = &(HvARRAY(PL_strtab))[hash & (I32) HvMAX(PL_strtab)];
    if (he) {
	const HE *const he_he = &(he->shared_he_he);
        for (entry = *oentry; entry; oentry = &HeNEXT(entry), entry = *oentry) {
            if (entry == he_he)
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
            break;
        }
    }

    if (entry) {
        if (--entry->he_valu.hent_refcount == 0) {
            *oentry = HeNEXT(entry);
            if (!*first) {
		/* There are now no entries in our slot.  */
                xhv->xhv_fill--; /* HvFILL(hv)-- */
	    }
            Safefree(entry);
            xhv->xhv_keys--; /* HvTOTALKEYS(hv)-- */
        }
    }

    if (!entry)
	Perl_ck_warner_d(aTHX_ packWARN(WARN_INTERNAL),
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

    PERL_ARGS_ASSERT_SHARE_HEK;

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
    dVAR;
    register HE *entry;
    const int flags_masked = flags & HVhek_MASK;
    const U32 hindex = hash & (I32) HvMAX(PL_strtab);
    register XPVHV * const xhv = (XPVHV*)SvANY(PL_strtab);

    PERL_ARGS_ASSERT_SHARE_HEK_FLAGS;

    /* what follows is the moral equivalent of:

    if (!(Svp = hv_fetch(PL_strtab, str, len, FALSE)))
	hv_store(PL_strtab, str, len, NULL, hash);

	Can't rehash the shared string table, so not sure if it's worth
	counting the number of entries in the linked list
    */

    /* assert(xhv_array != 0) */
    entry = (HvARRAY(PL_strtab))[hindex];
    for (;entry; entry = HeNEXT(entry)) {
	if (HeHASH(entry) != hash)		/* strings can't be equal */
	    continue;
	if (HeKLEN(entry) != len)
	    continue;
	if (HeKEY(entry) != str && memNE(HeKEY(entry),str,len))	/* is this it? */
	    continue;
	if (HeKFLAGS(entry) != flags_masked)
	    continue;
	break;
    }

    if (!entry) {
	/* What used to be head of the list.
	   If this is NULL, then we're the first entry for this slot, which
	   means we need to increate fill.  */
	struct shared_he *new_entry;
	HEK *hek;
	char *k;
	HE **const head = &HvARRAY(PL_strtab)[hindex];
	HE *const next = *head;

	/* We don't actually store a HE from the arena and a regular HEK.
	   Instead we allocate one chunk of memory big enough for both,
	   and put the HEK straight after the HE. This way we can find the
	   HEK directly from the HE.
	*/

	Newx(k, STRUCT_OFFSET(struct shared_he,
				shared_he_hek.hek_key[0]) + len + 2, char);
	new_entry = (struct shared_he *)k;
	entry = &(new_entry->shared_he_he);
	hek = &(new_entry->shared_he_hek);

	Copy(str, HEK_KEY(hek), len, char);
	HEK_KEY(hek)[len] = 0;
	HEK_LEN(hek) = len;
	HEK_HASH(hek) = hash;
	HEK_FLAGS(hek) = (unsigned char)flags_masked;

	/* Still "point" to the HEK, so that other code need not know what
	   we're up to.  */
	HeKEY_hek(entry) = hek;
	entry->he_valu.hent_refcount = 0;
	HeNEXT(entry) = next;
	*head = entry;

	xhv->xhv_keys++; /* HvTOTALKEYS(hv)++ */
	if (!next) {			/* initial entry? */
	    xhv->xhv_fill++; /* HvFILL(hv)++ */
	} else if (xhv->xhv_keys > (IV)xhv->xhv_max /* HvKEYS(hv) > HvMAX(hv) */) {
		hsplit(PL_strtab);
	}
    }

    ++entry->he_valu.hent_refcount;

    if (flags & HVhek_FREEKEY)
	Safefree(str);

    return HeKEY_hek(entry);
}

I32 *
Perl_hv_placeholders_p(pTHX_ HV *hv)
{
    dVAR;
    MAGIC *mg = mg_find((const SV *)hv, PERL_MAGIC_rhash);

    PERL_ARGS_ASSERT_HV_PLACEHOLDERS_P;

    if (!mg) {
	mg = sv_magicext(MUTABLE_SV(hv), 0, PERL_MAGIC_rhash, 0, 0, 0);

	if (!mg) {
	    Perl_die(aTHX_ "panic: hv_placeholders_p");
	}
    }
    return &(mg->mg_len);
}


I32
Perl_hv_placeholders_get(pTHX_ const HV *hv)
{
    dVAR;
    MAGIC * const mg = mg_find((const SV *)hv, PERL_MAGIC_rhash);

    PERL_ARGS_ASSERT_HV_PLACEHOLDERS_GET;

    return mg ? mg->mg_len : 0;
}

void
Perl_hv_placeholders_set(pTHX_ HV *hv, I32 ph)
{
    dVAR;
    MAGIC * const mg = mg_find((const SV *)hv, PERL_MAGIC_rhash);

    PERL_ARGS_ASSERT_HV_PLACEHOLDERS_SET;

    if (mg) {
	mg->mg_len = ph;
    } else if (ph) {
	if (!sv_magicext(MUTABLE_SV(hv), 0, PERL_MAGIC_rhash, 0, 0, ph))
	    Perl_die(aTHX_ "panic: hv_placeholders_set");
    }
    /* else we don't need to add magic to record 0 placeholders.  */
}

STATIC SV *
S_refcounted_he_value(pTHX_ const struct refcounted_he *he)
{
    dVAR;
    SV *value;

    PERL_ARGS_ASSERT_REFCOUNTED_HE_VALUE;

    switch(he->refcounted_he_data[0] & HVrhek_typemask) {
    case HVrhek_undef:
	value = newSV(0);
	break;
    case HVrhek_delete:
	value = &PL_sv_placeholder;
	break;
    case HVrhek_IV:
	value = newSViv(he->refcounted_he_val.refcounted_he_u_iv);
	break;
    case HVrhek_UV:
	value = newSVuv(he->refcounted_he_val.refcounted_he_u_uv);
	break;
    case HVrhek_PV:
    case HVrhek_PV_UTF8:
	/* Create a string SV that directly points to the bytes in our
	   structure.  */
	value = newSV_type(SVt_PV);
	SvPV_set(value, (char *) he->refcounted_he_data + 1);
	SvCUR_set(value, he->refcounted_he_val.refcounted_he_u_len);
	/* This stops anything trying to free it  */
	SvLEN_set(value, 0);
	SvPOK_on(value);
	SvREADONLY_on(value);
	if ((he->refcounted_he_data[0] & HVrhek_typemask) == HVrhek_PV_UTF8)
	    SvUTF8_on(value);
	break;
    default:
	Perl_croak(aTHX_ "panic: refcounted_he_value bad flags %x",
		   he->refcounted_he_data[0]);
    }
    return value;
}

/*
=for apidoc refcounted_he_chain_2hv

Generates and returns a C<HV *> by walking up the tree starting at the passed
in C<struct refcounted_he *>.

=cut
*/
HV *
Perl_refcounted_he_chain_2hv(pTHX_ const struct refcounted_he *chain)
{
    dVAR;
    HV *hv = newHV();
    U32 placeholders = 0;
    /* We could chase the chain once to get an idea of the number of keys,
       and call ksplit.  But for now we'll make a potentially inefficient
       hash with only 8 entries in its array.  */
    const U32 max = HvMAX(hv);

    if (!HvARRAY(hv)) {
	char *array;
	Newxz(array, PERL_HV_ARRAY_ALLOC_BYTES(max + 1), char);
	HvARRAY(hv) = (HE**)array;
    }

    while (chain) {
#ifdef USE_ITHREADS
	U32 hash = chain->refcounted_he_hash;
#else
	U32 hash = HEK_HASH(chain->refcounted_he_hek);
#endif
	HE **oentry = &((HvARRAY(hv))[hash & max]);
	HE *entry = *oentry;
	SV *value;

	for (; entry; entry = HeNEXT(entry)) {
	    if (HeHASH(entry) == hash) {
		/* We might have a duplicate key here.  If so, entry is older
		   than the key we've already put in the hash, so if they are
		   the same, skip adding entry.  */
#ifdef USE_ITHREADS
		const STRLEN klen = HeKLEN(entry);
		const char *const key = HeKEY(entry);
		if (klen == chain->refcounted_he_keylen
		    && (!!HeKUTF8(entry)
			== !!(chain->refcounted_he_data[0] & HVhek_UTF8))
		    && memEQ(key, REF_HE_KEY(chain), klen))
		    goto next_please;
#else
		if (HeKEY_hek(entry) == chain->refcounted_he_hek)
		    goto next_please;
		if (HeKLEN(entry) == HEK_LEN(chain->refcounted_he_hek)
		    && HeKUTF8(entry) == HEK_UTF8(chain->refcounted_he_hek)
		    && memEQ(HeKEY(entry), HEK_KEY(chain->refcounted_he_hek),
			     HeKLEN(entry)))
		    goto next_please;
#endif
	    }
	}
	assert (!entry);
	entry = new_HE();

#ifdef USE_ITHREADS
	HeKEY_hek(entry)
	    = share_hek_flags(REF_HE_KEY(chain),
			      chain->refcounted_he_keylen,
			      chain->refcounted_he_hash,
			      (chain->refcounted_he_data[0]
			       & (HVhek_UTF8|HVhek_WASUTF8)));
#else
	HeKEY_hek(entry) = share_hek_hek(chain->refcounted_he_hek);
#endif
	value = refcounted_he_value(chain);
	if (value == &PL_sv_placeholder)
	    placeholders++;
	HeVAL(entry) = value;

	/* Link it into the chain.  */
	HeNEXT(entry) = *oentry;
	if (!HeNEXT(entry)) {
	    /* initial entry.   */
	    HvFILL(hv)++;
	}
	*oentry = entry;

	HvTOTALKEYS(hv)++;

    next_please:
	chain = chain->refcounted_he_next;
    }

    if (placeholders) {
	clear_placeholders(hv, placeholders);
	HvTOTALKEYS(hv) -= placeholders;
    }

    /* We could check in the loop to see if we encounter any keys with key
       flags, but it's probably not worth it, as this per-hash flag is only
       really meant as an optimisation for things like Storable.  */
    HvHASKFLAGS_on(hv);
    DEBUG_A(Perl_hv_assert(aTHX_ hv));

    return hv;
}

SV *
Perl_refcounted_he_fetch(pTHX_ const struct refcounted_he *chain, SV *keysv,
			 const char *key, STRLEN klen, int flags, U32 hash)
{
    dVAR;
    /* Just to be awkward, if you're using this interface the UTF-8-or-not-ness
       of your key has to exactly match that which is stored.  */
    SV *value = &PL_sv_placeholder;

    if (chain) {
	/* No point in doing any of this if there's nothing to find.  */
	bool is_utf8;

	if (keysv) {
	    if (flags & HVhek_FREEKEY)
		Safefree(key);
	    key = SvPV_const(keysv, klen);
	    flags = 0;
	    is_utf8 = (SvUTF8(keysv) != 0);
	} else {
	    is_utf8 = ((flags & HVhek_UTF8) ? TRUE : FALSE);
	}

	if (!hash) {
	    if (keysv && (SvIsCOW_shared_hash(keysv))) {
		hash = SvSHARED_HASH(keysv);
	    } else {
		PERL_HASH(hash, key, klen);
	    }
	}

	for (; chain; chain = chain->refcounted_he_next) {
#ifdef USE_ITHREADS
	    if (hash != chain->refcounted_he_hash)
		continue;
	    if (klen != chain->refcounted_he_keylen)
		continue;
	    if (memNE(REF_HE_KEY(chain),key,klen))
		continue;
	    if (!!is_utf8 != !!(chain->refcounted_he_data[0] & HVhek_UTF8))
		continue;
#else
	    if (hash != HEK_HASH(chain->refcounted_he_hek))
		continue;
	    if (klen != (STRLEN)HEK_LEN(chain->refcounted_he_hek))
		continue;
	    if (memNE(HEK_KEY(chain->refcounted_he_hek),key,klen))
		continue;
	    if (!!is_utf8 != !!HEK_UTF8(chain->refcounted_he_hek))
		continue;
#endif

	    value = sv_2mortal(refcounted_he_value(chain));
	    break;
	}
    }

    if (flags & HVhek_FREEKEY)
	Safefree(key);

    return value;
}

/*
=for apidoc refcounted_he_new

Creates a new C<struct refcounted_he>. As S<key> is copied, and value is
stored in a compact form, all references remain the property of the caller.
The C<struct refcounted_he> is returned with a reference count of 1.

=cut
*/

struct refcounted_he *
Perl_refcounted_he_new(pTHX_ struct refcounted_he *const parent,
		       SV *const key, SV *const value) {
    dVAR;
    STRLEN key_len;
    const char *key_p = SvPV_const(key, key_len);
    STRLEN value_len = 0;
    const char *value_p = NULL;
    char value_type;
    char flags;
    bool is_utf8 = SvUTF8(key) ? TRUE : FALSE;

    if (SvPOK(value)) {
	value_type = HVrhek_PV;
    } else if (SvIOK(value)) {
	value_type = SvUOK((const SV *)value) ? HVrhek_UV : HVrhek_IV;
    } else if (value == &PL_sv_placeholder) {
	value_type = HVrhek_delete;
    } else if (!SvOK(value)) {
	value_type = HVrhek_undef;
    } else {
	value_type = HVrhek_PV;
    }

    if (value_type == HVrhek_PV) {
	/* Do it this way so that the SvUTF8() test is after the SvPV, in case
	   the value is overloaded, and doesn't yet have the UTF-8flag set.  */
	value_p = SvPV_const(value, value_len);
	if (SvUTF8(value))
	    value_type = HVrhek_PV_UTF8;
    }
    flags = value_type;

    if (is_utf8) {
	/* Hash keys are always stored normalised to (yes) ISO-8859-1.
	   As we're going to be building hash keys from this value in future,
	   normalise it now.  */
	key_p = (char*)bytes_from_utf8((const U8*)key_p, &key_len, &is_utf8);
	flags |= is_utf8 ? HVhek_UTF8 : HVhek_WASUTF8;
    }

    return refcounted_he_new_common(parent, key_p, key_len, flags, value_type,
				    ((value_type == HVrhek_PV
				      || value_type == HVrhek_PV_UTF8) ?
				     (void *)value_p : (void *)value),
				    value_len);
}

static struct refcounted_he *
S_refcounted_he_new_common(pTHX_ struct refcounted_he *const parent,
			   const char *const key_p, const STRLEN key_len,
			   const char flags, char value_type,
			   const void *value, const STRLEN value_len) {
    dVAR;
    struct refcounted_he *he;
    U32 hash;
    const bool is_pv = value_type == HVrhek_PV || value_type == HVrhek_PV_UTF8;
    STRLEN key_offset = is_pv ? value_len + 2 : 1;

    PERL_ARGS_ASSERT_REFCOUNTED_HE_NEW_COMMON;

#ifdef USE_ITHREADS
    he = (struct refcounted_he*)
	PerlMemShared_malloc(sizeof(struct refcounted_he) - 1
			     + key_len
			     + key_offset);
#else
    he = (struct refcounted_he*)
	PerlMemShared_malloc(sizeof(struct refcounted_he) - 1
			     + key_offset);
#endif

    he->refcounted_he_next = parent;

    if (is_pv) {
	Copy((char *)value, he->refcounted_he_data + 1, value_len + 1, char);
	he->refcounted_he_val.refcounted_he_u_len = value_len;
    } else if (value_type == HVrhek_IV) {
	he->refcounted_he_val.refcounted_he_u_iv = SvIVX((const SV *)value);
    } else if (value_type == HVrhek_UV) {
	he->refcounted_he_val.refcounted_he_u_uv = SvUVX((const SV *)value);
    }

    PERL_HASH(hash, key_p, key_len);

#ifdef USE_ITHREADS
    he->refcounted_he_hash = hash;
    he->refcounted_he_keylen = key_len;
    Copy(key_p, he->refcounted_he_data + key_offset, key_len, char);
#else
    he->refcounted_he_hek = share_hek_flags(key_p, key_len, hash, flags);
#endif

    if (flags & HVhek_WASUTF8) {
	/* If it was downgraded from UTF-8, then the pointer returned from
	   bytes_from_utf8 is an allocated pointer that we must free.  */
	Safefree(key_p);
    }

    he->refcounted_he_data[0] = flags;
    he->refcounted_he_refcnt = 1;

    return he;
}

/*
=for apidoc refcounted_he_free

Decrements the reference count of the passed in C<struct refcounted_he *>
by one. If the reference count reaches zero the structure's memory is freed,
and C<refcounted_he_free> iterates onto the parent node.

=cut
*/

void
Perl_refcounted_he_free(pTHX_ struct refcounted_he *he) {
    dVAR;
    PERL_UNUSED_CONTEXT;

    while (he) {
	struct refcounted_he *copy;
	U32 new_count;

	HINTS_REFCNT_LOCK;
	new_count = --he->refcounted_he_refcnt;
	HINTS_REFCNT_UNLOCK;
	
	if (new_count) {
	    return;
	}

#ifndef USE_ITHREADS
	unshare_hek_or_pvn (he->refcounted_he_hek, 0, 0, 0);
#endif
	copy = he;
	he = he->refcounted_he_next;
	PerlMemShared_free(copy);
    }
}

/* pp_entereval is aware that labels are stored with a key ':' at the top of
   the linked list.  */
const char *
Perl_fetch_cop_label(pTHX_ struct refcounted_he *const chain, STRLEN *len,
		     U32 *flags) {
    if (!chain)
	return NULL;
#ifdef USE_ITHREADS
    if (chain->refcounted_he_keylen != 1)
	return NULL;
    if (*REF_HE_KEY(chain) != ':')
	return NULL;
#else
    if ((STRLEN)HEK_LEN(chain->refcounted_he_hek) != 1)
	return NULL;
    if (*HEK_KEY(chain->refcounted_he_hek) != ':')
	return NULL;
#endif
    /* Stop anyone trying to really mess us up by adding their own value for
       ':' into %^H  */
    if ((chain->refcounted_he_data[0] & HVrhek_typemask) != HVrhek_PV
	&& (chain->refcounted_he_data[0] & HVrhek_typemask) != HVrhek_PV_UTF8)
	return NULL;

    if (len)
	*len = chain->refcounted_he_val.refcounted_he_u_len;
    if (flags) {
	*flags = ((chain->refcounted_he_data[0] & HVrhek_typemask)
		  == HVrhek_PV_UTF8) ? SVf_UTF8 : 0;
    }
    return chain->refcounted_he_data + 1;
}

/* As newSTATEOP currently gets passed plain char* labels, we will only provide
   that interface. Once it works out how to pass in length and UTF-8 ness, this
   function will need superseding.  */
struct refcounted_he *
Perl_store_cop_label(pTHX_ struct refcounted_he *const chain, const char *label)
{
    PERL_ARGS_ASSERT_STORE_COP_LABEL;

    return refcounted_he_new_common(chain, ":", 1, HVrhek_PV, HVrhek_PV,
				    label, strlen(label));
}

/*
=for apidoc hv_assert

Check that a hash is in an internally consistent state.

=cut
*/

#ifdef DEBUGGING

void
Perl_hv_assert(pTHX_ HV *hv)
{
    dVAR;
    HE* entry;
    int withflags = 0;
    int placeholders = 0;
    int real = 0;
    int bad = 0;
    const I32 riter = HvRITER_get(hv);
    HE *eiter = HvEITER_get(hv);

    PERL_ARGS_ASSERT_HV_ASSERT;

    (void)hv_iterinit(hv);

    while ((entry = hv_iternext_flags(hv, HV_ITERNEXT_WANTPLACEHOLDERS))) {
	/* sanity check the values */
	if (HeVAL(entry) == &PL_sv_placeholder)
	    placeholders++;
	else
	    real++;
	/* sanity check the keys */
	if (HeSVKEY(entry)) {
	    NOOP;   /* Don't know what to check on SV keys.  */
	} else if (HeKUTF8(entry)) {
	    withflags++;
	    if (HeKWASUTF8(entry)) {
		PerlIO_printf(Perl_debug_log,
			    "hash key has both WASUTF8 and UTF8: '%.*s'\n",
			    (int) HeKLEN(entry),  HeKEY(entry));
		bad = 1;
	    }
	} else if (HeKWASUTF8(entry))
	    withflags++;
    }
    if (!SvTIED_mg((const SV *)hv, PERL_MAGIC_tied)) {
	static const char bad_count[] = "Count %d %s(s), but hash reports %d\n";
	const int nhashkeys = HvUSEDKEYS(hv);
	const int nhashplaceholders = HvPLACEHOLDERS_get(hv);

	if (nhashkeys != real) {
	    PerlIO_printf(Perl_debug_log, bad_count, real, "keys", nhashkeys );
	    bad = 1;
	}
	if (nhashplaceholders != placeholders) {
	    PerlIO_printf(Perl_debug_log, bad_count, placeholders, "placeholder", nhashplaceholders );
	    bad = 1;
	}
    }
    if (withflags && ! HvHASKFLAGS(hv)) {
	PerlIO_printf(Perl_debug_log,
		    "Hash has HASKFLAGS off but I count %d key(s) with flags\n",
		    withflags);
	bad = 1;
    }
    if (bad) {
	sv_dump(MUTABLE_SV(hv));
    }
    HvRITER_set(hv, riter);		/* Restore hash iterator state */
    HvEITER_set(hv, eiter);
}

#endif

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
