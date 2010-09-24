/*    mro.c
 *
 *    Copyright (c) 2007 Brandon L Black
 *    Copyright (c) 2007, 2008 Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'Which order shall we go in?' said Frodo.  'Eldest first, or quickest first?
 *  You'll be last either way, Master Peregrin.'
 *
 *     [p.101 of _The Lord of the Rings_, I/iii: "A Conspiracy Unmasked"]
 */

/*
=head1 MRO Functions

These functions are related to the method resolution order of perl classes

=cut
*/

#include "EXTERN.h"
#define PERL_IN_MRO_C
#include "perl.h"

static const struct mro_alg dfs_alg =
    {S_mro_get_linear_isa_dfs, "dfs", 3, 0, 0};

SV *
Perl_mro_get_private_data(pTHX_ struct mro_meta *const smeta,
			  const struct mro_alg *const which)
{
    SV **data;
    PERL_ARGS_ASSERT_MRO_GET_PRIVATE_DATA;

    data = (SV **)Perl_hv_common(aTHX_ smeta->mro_linear_all, NULL,
				 which->name, which->length, which->kflags,
				 HV_FETCH_JUST_SV, NULL, which->hash);
    if (!data)
	return NULL;

    /* If we've been asked to look up the private data for the current MRO, then
       cache it.  */
    if (smeta->mro_which == which)
	smeta->mro_linear_current = *data;

    return *data;
}

SV *
Perl_mro_set_private_data(pTHX_ struct mro_meta *const smeta,
			  const struct mro_alg *const which, SV *const data)
{
    PERL_ARGS_ASSERT_MRO_SET_PRIVATE_DATA;

    if (!smeta->mro_linear_all) {
	if (smeta->mro_which == which) {
	    /* If all we need to store is the current MRO's data, then don't use
	       memory on a hash with 1 element - store it direct, and signal
	       this by leaving the would-be-hash NULL.  */
	    smeta->mro_linear_current = data;
	    return data;
	} else {
	    HV *const hv = newHV();
	    /* Start with 2 buckets. It's unlikely we'll need more. */
	    HvMAX(hv) = 1;	
	    smeta->mro_linear_all = hv;

	    if (smeta->mro_linear_current) {
		/* If we were storing something directly, put it in the hash
		   before we lose it. */
		Perl_mro_set_private_data(aTHX_ smeta, smeta->mro_which, 
					  smeta->mro_linear_current);
	    }
	}
    }

    /* We get here if we're storing more than one linearisation for this stash,
       or the linearisation we are storing is not that if its current MRO.  */

    if (smeta->mro_which == which) {
	/* If we've been asked to store the private data for the current MRO,
	   then cache it.  */
	smeta->mro_linear_current = data;
    }

    if (!Perl_hv_common(aTHX_ smeta->mro_linear_all, NULL,
			which->name, which->length, which->kflags,
			HV_FETCH_ISSTORE, data, which->hash)) {
	Perl_croak(aTHX_ "panic: hv_store() failed in set_mro_private_data() "
		   "for '%.*s' %d", (int) which->length, which->name,
		   which->kflags);
    }

    return data;
}

const struct mro_alg *
Perl_mro_get_from_name(pTHX_ SV *name) {
    SV **data;

    PERL_ARGS_ASSERT_MRO_GET_FROM_NAME;

    data = (SV **)Perl_hv_common(aTHX_ PL_registered_mros, name, NULL, 0, 0,
				 HV_FETCH_JUST_SV, NULL, 0);
    if (!data)
	return NULL;
    assert(SvTYPE(*data) == SVt_IV);
    assert(SvIOK(*data));
    return INT2PTR(const struct mro_alg *, SvUVX(*data));
}

void
Perl_mro_register(pTHX_ const struct mro_alg *mro) {
    SV *wrapper = newSVuv(PTR2UV(mro));

    PERL_ARGS_ASSERT_MRO_REGISTER;

    
    if (!Perl_hv_common(aTHX_ PL_registered_mros, NULL,
			mro->name, mro->length, mro->kflags,
			HV_FETCH_ISSTORE, wrapper, mro->hash)) {
	SvREFCNT_dec(wrapper);
	Perl_croak(aTHX_ "panic: hv_store() failed in mro_register() "
		   "for '%.*s' %d", (int) mro->length, mro->name, mro->kflags);
    }
}

struct mro_meta*
Perl_mro_meta_init(pTHX_ HV* stash)
{
    struct mro_meta* newmeta;

    PERL_ARGS_ASSERT_MRO_META_INIT;
    assert(HvAUX(stash));
    assert(!(HvAUX(stash)->xhv_mro_meta));
    Newxz(newmeta, 1, struct mro_meta);
    HvAUX(stash)->xhv_mro_meta = newmeta;
    newmeta->cache_gen = 1;
    newmeta->pkg_gen = 1;
    newmeta->mro_which = &dfs_alg;

    return newmeta;
}

#if defined(USE_ITHREADS)

/* for sv_dup on new threads */
struct mro_meta*
Perl_mro_meta_dup(pTHX_ struct mro_meta* smeta, CLONE_PARAMS* param)
{
    struct mro_meta* newmeta;

    PERL_ARGS_ASSERT_MRO_META_DUP;

    Newx(newmeta, 1, struct mro_meta);
    Copy(smeta, newmeta, 1, struct mro_meta);

    if (newmeta->mro_linear_all) {
	newmeta->mro_linear_all
	    = MUTABLE_HV(SvREFCNT_inc(sv_dup((const SV *)newmeta->mro_linear_all, param)));
	/* This is just acting as a shortcut pointer, and will be automatically
	   updated on the first get.  */
	newmeta->mro_linear_current = NULL;
    } else if (newmeta->mro_linear_current) {
	/* Only the current MRO is stored, so this owns the data.  */
	newmeta->mro_linear_current
	    = SvREFCNT_inc(sv_dup((const SV *)newmeta->mro_linear_current,
				  param));
    }

    if (newmeta->mro_nextmethod)
	newmeta->mro_nextmethod
	    = MUTABLE_HV(SvREFCNT_inc(sv_dup((const SV *)newmeta->mro_nextmethod, param)));
    if (newmeta->isa)
	newmeta->isa
	    = MUTABLE_HV(SvREFCNT_inc(sv_dup((const SV *)newmeta->isa, param)));

    return newmeta;
}

#endif /* USE_ITHREADS */

/*
=for apidoc mro_get_linear_isa_dfs

Returns the Depth-First Search linearization of @ISA
the given stash.  The return value is a read-only AV*.
C<level> should be 0 (it is used internally in this
function's recursion).

You are responsible for C<SvREFCNT_inc()> on the
return value if you plan to store it anywhere
semi-permanently (otherwise it might be deleted
out from under you the next time the cache is
invalidated).

=cut
*/
static AV*
S_mro_get_linear_isa_dfs(pTHX_ HV *stash, U32 level)
{
    AV* retval;
    GV** gvp;
    GV* gv;
    AV* av;
    const HEK* stashhek;
    struct mro_meta* meta;
    SV *our_name;
    HV *stored = NULL;

    PERL_ARGS_ASSERT_MRO_GET_LINEAR_ISA_DFS;
    assert(HvAUX(stash));

    stashhek = HvNAME_HEK(stash);
    if (!stashhek)
      Perl_croak(aTHX_ "Can't linearize anonymous symbol table");

    if (level > 100)
        Perl_croak(aTHX_ "Recursive inheritance detected in package '%s'",
		   HEK_KEY(stashhek));

    meta = HvMROMETA(stash);

    /* return cache if valid */
    if((retval = MUTABLE_AV(MRO_GET_PRIVATE_DATA(meta, &dfs_alg)))) {
        return retval;
    }

    /* not in cache, make a new one */

    retval = MUTABLE_AV(sv_2mortal(MUTABLE_SV(newAV())));
    /* We use this later in this function, but don't need a reference to it
       beyond the end of this function, so reference count is fine.  */
    our_name = newSVhek(stashhek);
    av_push(retval, our_name); /* add ourselves at the top */

    /* fetch our @ISA */
    gvp = (GV**)hv_fetchs(stash, "ISA", FALSE);
    av = (gvp && (gv = *gvp) && isGV_with_GP(gv)) ? GvAV(gv) : NULL;

    /* "stored" is used to keep track of all of the classnames we have added to
       the MRO so far, so we can do a quick exists check and avoid adding
       duplicate classnames to the MRO as we go.
       It's then retained to be re-used as a fast lookup for ->isa(), by adding
       our own name and "UNIVERSAL" to it.  */

    if(av && AvFILLp(av) >= 0) {

        SV **svp = AvARRAY(av);
        I32 items = AvFILLp(av) + 1;

        /* foreach(@ISA) */
        while (items--) {
            SV* const sv = *svp++;
            HV* const basestash = gv_stashsv(sv, 0);
	    SV *const *subrv_p;
	    I32 subrv_items;

            if (!basestash) {
                /* if no stash exists for this @ISA member,
                   simply add it to the MRO and move on */
		subrv_p = &sv;
		subrv_items = 1;
            }
            else {
                /* otherwise, recurse into ourselves for the MRO
                   of this @ISA member, and append their MRO to ours.
		   The recursive call could throw an exception, which
		   has memory management implications here, hence the use of
		   the mortal.  */
		const AV *const subrv
		    = mro_get_linear_isa_dfs(basestash, level + 1);

		subrv_p = AvARRAY(subrv);
		subrv_items = AvFILLp(subrv) + 1;
	    }
	    if (stored) {
		while(subrv_items--) {
		    SV *const subsv = *subrv_p++;
		    /* LVALUE fetch will create a new undefined SV if necessary
		     */
		    HE *const he = hv_fetch_ent(stored, subsv, 1, 0);
		    assert(he);
		    if(HeVAL(he) != &PL_sv_undef) {
			/* It was newly created.  Steal it for our new SV, and
			   replace it in the hash with the "real" thing.  */
			SV *const val = HeVAL(he);
			HEK *const key = HeKEY_hek(he);

			HeVAL(he) = &PL_sv_undef;
			/* Save copying by making a shared hash key scalar. We
			   inline this here rather than calling
			   Perl_newSVpvn_share because we already have the
			   scalar, and we already have the hash key.  */
			assert(SvTYPE(val) == SVt_NULL);
			sv_upgrade(val, SVt_PV);
			SvPV_set(val, HEK_KEY(share_hek_hek(key)));
			SvCUR_set(val, HEK_LEN(key));
			SvREADONLY_on(val);
			SvFAKE_on(val);
			SvPOK_on(val);
			if (HEK_UTF8(key))
			    SvUTF8_on(val);

			av_push(retval, val);
		    }
		}
            } else {
		/* We are the first (or only) parent. We can short cut the
		   complexity above, because our @ISA is simply us prepended
		   to our parent's @ISA, and our ->isa cache is simply our
		   parent's, with our name added.  */
		/* newSVsv() is slow. This code is only faster if we can avoid
		   it by ensuring that SVs in the arrays are shared hash key
		   scalar SVs, because we can "copy" them very efficiently.
		   Although to be fair, we can't *ensure* this, as a reference
		   to the internal array is returned by mro::get_linear_isa(),
		   so we'll have to be defensive just in case someone faffed
		   with it.  */
		if (basestash) {
		    SV **svp;
		    stored = MUTABLE_HV(sv_2mortal((SV*)newHVhv(HvMROMETA(basestash)->isa)));
		    av_extend(retval, subrv_items);
		    AvFILLp(retval) = subrv_items;
		    svp = AvARRAY(retval);
		    while(subrv_items--) {
			SV *const val = *subrv_p++;
			*++svp = SvIsCOW_shared_hash(val)
			    ? newSVhek(SvSHARED_HEK_FROM_PV(SvPVX(val)))
			    : newSVsv(val);
		    }
		} else {
		    /* They have no stash.  So create ourselves an ->isa cache
		       as if we'd copied it from what theirs should be.  */
		    stored = MUTABLE_HV(sv_2mortal(MUTABLE_SV(newHV())));
		    (void) hv_store(stored, "UNIVERSAL", 9, &PL_sv_undef, 0);
		    av_push(retval,
			    newSVhek(HeKEY_hek(hv_store_ent(stored, sv,
							    &PL_sv_undef, 0))));
		}
	    }
        }
    } else {
	/* We have no parents.  */
	stored = MUTABLE_HV(sv_2mortal(MUTABLE_SV(newHV())));
	(void) hv_store(stored, "UNIVERSAL", 9, &PL_sv_undef, 0);
    }

    (void) hv_store_ent(stored, our_name, &PL_sv_undef, 0);

    SvREFCNT_inc_simple_void_NN(stored);
    SvTEMP_off(stored);
    SvREADONLY_on(stored);

    meta->isa = stored;

    /* now that we're past the exception dangers, grab our own reference to
       the AV we're about to use for the result. The reference owned by the
       mortals' stack will be released soon, so everything will balance.  */
    SvREFCNT_inc_simple_void_NN(retval);
    SvTEMP_off(retval);

    /* we don't want anyone modifying the cache entry but us,
       and we do so by replacing it completely */
    SvREADONLY_on(retval);

    return MUTABLE_AV(Perl_mro_set_private_data(aTHX_ meta, &dfs_alg,
						MUTABLE_SV(retval)));
}

/*
=for apidoc mro_get_linear_isa

Returns either C<mro_get_linear_isa_c3> or
C<mro_get_linear_isa_dfs> for the given stash,
dependant upon which MRO is in effect
for that stash.  The return value is a
read-only AV*.

You are responsible for C<SvREFCNT_inc()> on the
return value if you plan to store it anywhere
semi-permanently (otherwise it might be deleted
out from under you the next time the cache is
invalidated).

=cut
*/
AV*
Perl_mro_get_linear_isa(pTHX_ HV *stash)
{
    struct mro_meta* meta;

    PERL_ARGS_ASSERT_MRO_GET_LINEAR_ISA;
    if(!SvOOK(stash))
        Perl_croak(aTHX_ "Can't linearize anonymous symbol table");

    meta = HvMROMETA(stash);
    if (!meta->mro_which)
        Perl_croak(aTHX_ "panic: invalid MRO!");
    return meta->mro_which->resolve(aTHX_ stash, 0);
}

/*
=for apidoc mro_isa_changed_in

Takes the necessary steps (cache invalidations, mostly)
when the @ISA of the given package has changed.  Invoked
by the C<setisa> magic, should not need to invoke directly.

=cut
*/
void
Perl_mro_isa_changed_in(pTHX_ HV* stash)
{
    dVAR;
    HV* isarev;
    AV* linear_mro;
    HE* iter;
    SV** svp;
    I32 items;
    bool is_universal;
    struct mro_meta * meta;

    const char * const stashname = HvNAME_get(stash);
    const STRLEN stashname_len = HvNAMELEN_get(stash);

    PERL_ARGS_ASSERT_MRO_ISA_CHANGED_IN;

    if(!stashname)
        Perl_croak(aTHX_ "Can't call mro_isa_changed_in() on anonymous symbol table");

    /* wipe out the cached linearizations for this stash */
    meta = HvMROMETA(stash);
    if (meta->mro_linear_all) {
	SvREFCNT_dec(MUTABLE_SV(meta->mro_linear_all));
	meta->mro_linear_all = NULL;
	/* This is just acting as a shortcut pointer.  */
	meta->mro_linear_current = NULL;
    } else if (meta->mro_linear_current) {
	/* Only the current MRO is stored, so this owns the data.  */
	SvREFCNT_dec(meta->mro_linear_current);
	meta->mro_linear_current = NULL;
    }
    if (meta->isa) {
	SvREFCNT_dec(meta->isa);
	meta->isa = NULL;
    }

    /* Inc the package generation, since our @ISA changed */
    meta->pkg_gen++;

    /* Wipe the global method cache if this package
       is UNIVERSAL or one of its parents */

    svp = hv_fetch(PL_isarev, stashname, stashname_len, 0);
    isarev = svp ? MUTABLE_HV(*svp) : NULL;

    if((stashname_len == 9 && strEQ(stashname, "UNIVERSAL"))
        || (isarev && hv_exists(isarev, "UNIVERSAL", 9))) {
        PL_sub_generation++;
        is_universal = TRUE;
    }
    else { /* Wipe the local method cache otherwise */
        meta->cache_gen++;
	is_universal = FALSE;
    }

    /* wipe next::method cache too */
    if(meta->mro_nextmethod) hv_clear(meta->mro_nextmethod);

    /* Iterate the isarev (classes that are our children),
       wiping out their linearization, method and isa caches */
    if(isarev) {
        hv_iterinit(isarev);
        while((iter = hv_iternext(isarev))) {
	    I32 len;
            const char* const revkey = hv_iterkey(iter, &len);
            HV* revstash = gv_stashpvn(revkey, len, 0);
            struct mro_meta* revmeta;

            if(!revstash) continue;
            revmeta = HvMROMETA(revstash);
	    if (revmeta->mro_linear_all) {
		SvREFCNT_dec(MUTABLE_SV(revmeta->mro_linear_all));
		revmeta->mro_linear_all = NULL;
		/* This is just acting as a shortcut pointer.  */
		revmeta->mro_linear_current = NULL;
	    } else if (revmeta->mro_linear_current) {
		/* Only the current MRO is stored, so this owns the data.  */
		SvREFCNT_dec(revmeta->mro_linear_current);
		revmeta->mro_linear_current = NULL;
	    }
            if(!is_universal)
                revmeta->cache_gen++;
            if(revmeta->mro_nextmethod)
                hv_clear(revmeta->mro_nextmethod);
	    if (revmeta->isa) {
		SvREFCNT_dec(revmeta->isa);
		revmeta->isa = NULL;
	    }
        }
    }

    /* Now iterate our MRO (parents), and do a few things:
         1) instantiate with the "fake" flag if they don't exist
         2) flag them as universal if we are universal
         3) Add everything from our isarev to their isarev
    */

    /* We're starting at the 2nd element, skipping ourselves here */
    linear_mro = mro_get_linear_isa(stash);
    svp = AvARRAY(linear_mro) + 1;
    items = AvFILLp(linear_mro);

    while (items--) {
        SV* const sv = *svp++;
        HV* mroisarev;

        HE *he = hv_fetch_ent(PL_isarev, sv, TRUE, 0);

	/* That fetch should not fail.  But if it had to create a new SV for
	   us, then will need to upgrade it to an HV (which sv_upgrade() can
	   now do for us. */

        mroisarev = MUTABLE_HV(HeVAL(he));

	SvUPGRADE(MUTABLE_SV(mroisarev), SVt_PVHV);

	/* This hash only ever contains PL_sv_yes. Storing it over itself is
	   almost as cheap as calling hv_exists, so on aggregate we expect to
	   save time by not making two calls to the common HV code for the
	   case where it doesn't exist.  */
	   
	(void)hv_store(mroisarev, stashname, stashname_len, &PL_sv_yes, 0);

        if(isarev) {
            hv_iterinit(isarev);
            while((iter = hv_iternext(isarev))) {
                I32 revkeylen;
                char* const revkey = hv_iterkey(iter, &revkeylen);
		(void)hv_store(mroisarev, revkey, revkeylen, &PL_sv_yes, 0);
            }
        }
    }
}

/*
=for apidoc mro_method_changed_in

Invalidates method caching on any child classes
of the given stash, so that they might notice
the changes in this one.

Ideally, all instances of C<PL_sub_generation++> in
perl source outside of C<mro.c> should be
replaced by calls to this.

Perl automatically handles most of the common
ways a method might be redefined.  However, there
are a few ways you could change a method in a stash
without the cache code noticing, in which case you
need to call this method afterwards:

1) Directly manipulating the stash HV entries from
XS code.

2) Assigning a reference to a readonly scalar
constant into a stash entry in order to create
a constant subroutine (like constant.pm
does).

This same method is available from pure perl
via, C<mro::method_changed_in(classname)>.

=cut
*/
void
Perl_mro_method_changed_in(pTHX_ HV *stash)
{
    const char * const stashname = HvNAME_get(stash);
    const STRLEN stashname_len = HvNAMELEN_get(stash);

    SV ** const svp = hv_fetch(PL_isarev, stashname, stashname_len, 0);
    HV * const isarev = svp ? MUTABLE_HV(*svp) : NULL;

    PERL_ARGS_ASSERT_MRO_METHOD_CHANGED_IN;

    if(!stashname)
        Perl_croak(aTHX_ "Can't call mro_method_changed_in() on anonymous symbol table");

    /* Inc the package generation, since a local method changed */
    HvMROMETA(stash)->pkg_gen++;

    /* If stash is UNIVERSAL, or one of UNIVERSAL's parents,
       invalidate all method caches globally */
    if((stashname_len == 9 && strEQ(stashname, "UNIVERSAL"))
        || (isarev && hv_exists(isarev, "UNIVERSAL", 9))) {
        PL_sub_generation++;
        return;
    }

    /* else, invalidate the method caches of all child classes,
       but not itself */
    if(isarev) {
	HE* iter;

        hv_iterinit(isarev);
        while((iter = hv_iternext(isarev))) {
	    I32 len;
            const char* const revkey = hv_iterkey(iter, &len);
            HV* const revstash = gv_stashpvn(revkey, len, 0);
            struct mro_meta* mrometa;

            if(!revstash) continue;
            mrometa = HvMROMETA(revstash);
            mrometa->cache_gen++;
            if(mrometa->mro_nextmethod)
                hv_clear(mrometa->mro_nextmethod);
        }
    }
}

void
Perl_mro_set_mro(pTHX_ struct mro_meta *const meta, SV *const name)
{
    const struct mro_alg *const which = Perl_mro_get_from_name(aTHX_ name);
 
    PERL_ARGS_ASSERT_MRO_SET_MRO;

    if (!which)
        Perl_croak(aTHX_ "Invalid mro name: '%"SVf"'", name);

    if(meta->mro_which != which) {
	if (meta->mro_linear_current && !meta->mro_linear_all) {
	    /* If we were storing something directly, put it in the hash before
	       we lose it. */
	    Perl_mro_set_private_data(aTHX_ meta, meta->mro_which, 
				      MUTABLE_SV(meta->mro_linear_current));
	}
	meta->mro_which = which;
	/* Scrub our cached pointer to the private data.  */
	meta->mro_linear_current = NULL;
        /* Only affects local method cache, not
           even child classes */
        meta->cache_gen++;
        if(meta->mro_nextmethod)
            hv_clear(meta->mro_nextmethod);
    }
}

#include "XSUB.h"

XS(XS_mro_method_changed_in);

void
Perl_boot_core_mro(pTHX)
{
    dVAR;
    static const char file[] = __FILE__;

    Perl_mro_register(aTHX_ &dfs_alg);

    newXSproto("mro::method_changed_in", XS_mro_method_changed_in, file, "$");
}

XS(XS_mro_method_changed_in)
{
    dVAR;
    dXSARGS;
    SV* classname;
    HV* class_stash;

    if(items != 1)
	croak_xs_usage(cv, "classname");
    
    classname = ST(0);

    class_stash = gv_stashsv(classname, 0);
    if(!class_stash) Perl_croak(aTHX_ "No such class: '%"SVf"'!", SVfARG(classname));

    mro_method_changed_in(class_stash);

    XSRETURN_EMPTY;
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
