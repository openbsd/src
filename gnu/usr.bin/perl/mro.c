/*    mro.c
 *
 *    Copyright (c) 2007 Brandon L Black
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Which order shall we go in?" said Frodo. "Eldest first, or quickest first?
 *  You'll be last either way, Master Peregrin."
 */

/*
=head1 MRO Functions

These functions are related to the method resolution order of perl classes

=cut
*/

#include "EXTERN.h"
#define PERL_IN_MRO_C
#include "perl.h"

struct mro_alg {
    const char *name;
    AV *(*resolve)(pTHX_ HV* stash, I32 level);
};

/* First one is the default */
static struct mro_alg mros[] = {
    {"dfs", S_mro_get_linear_isa_dfs},
    {"c3", S_mro_get_linear_isa_c3}
};

#define NUMBER_OF_MROS (sizeof(mros)/sizeof(struct mro_alg))

static const struct mro_alg *
S_get_mro_from_name(pTHX_ const char *const name) {
    const struct mro_alg *algo = mros;
    const struct mro_alg *const end = mros + NUMBER_OF_MROS;
    while (algo < end) {
	if(strEQ(name, algo->name))
	    return algo;
	++algo;
    }
    return NULL;
}

struct mro_meta*
Perl_mro_meta_init(pTHX_ HV* stash)
{
    struct mro_meta* newmeta;

    assert(stash);
    assert(HvAUX(stash));
    assert(!(HvAUX(stash)->xhv_mro_meta));
    Newxz(newmeta, 1, struct mro_meta);
    HvAUX(stash)->xhv_mro_meta = newmeta;
    newmeta->cache_gen = 1;
    newmeta->pkg_gen = 1;
    newmeta->mro_which = mros;

    return newmeta;
}

#if defined(USE_ITHREADS)

/* for sv_dup on new threads */
struct mro_meta*
Perl_mro_meta_dup(pTHX_ struct mro_meta* smeta, CLONE_PARAMS* param)
{
    struct mro_meta* newmeta;

    assert(smeta);

    Newx(newmeta, 1, struct mro_meta);
    Copy(smeta, newmeta, 1, struct mro_meta);

    if (newmeta->mro_linear_dfs)
	newmeta->mro_linear_dfs
	    = (AV*) SvREFCNT_inc(sv_dup((SV*)newmeta->mro_linear_dfs, param));
    if (newmeta->mro_linear_c3)
	newmeta->mro_linear_c3
	    = (AV*) SvREFCNT_inc(sv_dup((SV*)newmeta->mro_linear_c3, param));
    if (newmeta->mro_nextmethod)
	newmeta->mro_nextmethod
	    = (HV*) SvREFCNT_inc(sv_dup((SV*)newmeta->mro_nextmethod, param));

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
S_mro_get_linear_isa_dfs(pTHX_ HV *stash, I32 level)
{
    AV* retval;
    GV** gvp;
    GV* gv;
    AV* av;
    const HEK* stashhek;
    struct mro_meta* meta;

    assert(stash);
    assert(HvAUX(stash));

    stashhek = HvNAME_HEK(stash);
    if (!stashhek)
      Perl_croak(aTHX_ "Can't linearize anonymous symbol table");

    if (level > 100)
        Perl_croak(aTHX_ "Recursive inheritance detected in package '%s'",
		   HEK_KEY(stashhek));

    meta = HvMROMETA(stash);

    /* return cache if valid */
    if((retval = meta->mro_linear_dfs)) {
        return retval;
    }

    /* not in cache, make a new one */

    retval = (AV*)sv_2mortal((SV *)newAV());
    av_push(retval, newSVhek(stashhek)); /* add ourselves at the top */

    /* fetch our @ISA */
    gvp = (GV**)hv_fetchs(stash, "ISA", FALSE);
    av = (gvp && (gv = *gvp) && isGV_with_GP(gv)) ? GvAV(gv) : NULL;

    if(av && AvFILLp(av) >= 0) {

        /* "stored" is used to keep track of all of the classnames
           we have added to the MRO so far, so we can do a quick
           exists check and avoid adding duplicate classnames to
           the MRO as we go. */

        HV* const stored = (HV*)sv_2mortal((SV*)newHV());
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
	    while(subrv_items--) {
		SV *const subsv = *subrv_p++;
		if(!hv_exists_ent(stored, subsv, 0)) {
		    (void)hv_store_ent(stored, subsv, &PL_sv_undef, 0);
		    av_push(retval, newSVsv(subsv));
		}
            }
        }
    }

    /* now that we're past the exception dangers, grab our own reference to
       the AV we're about to use for the result. The reference owned by the
       mortals' stack will be released soon, so everything will balance.  */
    SvREFCNT_inc_simple_void_NN(retval);
    SvTEMP_off(retval);

    /* we don't want anyone modifying the cache entry but us,
       and we do so by replacing it completely */
    SvREADONLY_on(retval);

    meta->mro_linear_dfs = retval;
    return retval;
}

/*
=for apidoc mro_get_linear_isa_c3

Returns the C3 linearization of @ISA
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
S_mro_get_linear_isa_c3(pTHX_ HV* stash, I32 level)
{
    AV* retval;
    GV** gvp;
    GV* gv;
    AV* isa;
    const HEK* stashhek;
    struct mro_meta* meta;

    assert(stash);
    assert(HvAUX(stash));

    stashhek = HvNAME_HEK(stash);
    if (!stashhek)
      Perl_croak(aTHX_ "Can't linearize anonymous symbol table");

    if (level > 100)
        Perl_croak(aTHX_ "Recursive inheritance detected in package '%s'",
		   HEK_KEY(stashhek));

    meta = HvMROMETA(stash);

    /* return cache if valid */
    if((retval = meta->mro_linear_c3)) {
        return retval;
    }

    /* not in cache, make a new one */

    gvp = (GV**)hv_fetchs(stash, "ISA", FALSE);
    isa = (gvp && (gv = *gvp) && isGV_with_GP(gv)) ? GvAV(gv) : NULL;

    /* For a better idea how the rest of this works, see the much clearer
       pure perl version in Algorithm::C3 0.01:
       http://search.cpan.org/src/STEVAN/Algorithm-C3-0.01/lib/Algorithm/C3.pm
       (later versions go about it differently than this code for speed reasons)
    */

    if(isa && AvFILLp(isa) >= 0) {
        SV** seqs_ptr;
        I32 seqs_items;
        HV* const tails = (HV*)sv_2mortal((SV*)newHV());
        AV* const seqs = (AV*)sv_2mortal((SV*)newAV());
        I32* heads;

        /* This builds @seqs, which is an array of arrays.
           The members of @seqs are the MROs of
           the members of @ISA, followed by @ISA itself.
        */
        I32 items = AvFILLp(isa) + 1;
        SV** isa_ptr = AvARRAY(isa);
        while(items--) {
            SV* const isa_item = *isa_ptr++;
            HV* const isa_item_stash = gv_stashsv(isa_item, 0);
            if(!isa_item_stash) {
                /* if no stash, make a temporary fake MRO
                   containing just itself */
                AV* const isa_lin = newAV();
                av_push(isa_lin, newSVsv(isa_item));
                av_push(seqs, (SV*)isa_lin);
            }
            else {
                /* recursion */
                AV* const isa_lin = mro_get_linear_isa_c3(isa_item_stash, level + 1);
                av_push(seqs, SvREFCNT_inc_simple_NN((SV*)isa_lin));
            }
        }
        av_push(seqs, SvREFCNT_inc_simple_NN((SV*)isa));

        /* This builds "heads", which as an array of integer array
           indices, one per seq, which point at the virtual "head"
           of the seq (initially zero) */
        Newxz(heads, AvFILLp(seqs)+1, I32);

        /* This builds %tails, which has one key for every class
           mentioned in the tail of any sequence in @seqs (tail meaning
           everything after the first class, the "head").  The value
           is how many times this key appears in the tails of @seqs.
        */
        seqs_ptr = AvARRAY(seqs);
        seqs_items = AvFILLp(seqs) + 1;
        while(seqs_items--) {
            AV* const seq = (AV*)*seqs_ptr++;
            I32 seq_items = AvFILLp(seq);
            if(seq_items > 0) {
                SV** seq_ptr = AvARRAY(seq) + 1;
                while(seq_items--) {
                    SV* const seqitem = *seq_ptr++;
		    /* LVALUE fetch will create a new undefined SV if necessary
		     */
                    HE* const he = hv_fetch_ent(tails, seqitem, 1, 0);
                    if(he) {
                        SV* const val = HeVAL(he);
			/* This will increment undef to 1, which is what we
			   want for a newly created entry.  */
                        sv_inc(val);
                    }
                }
            }
        }

        /* Initialize retval to build the return value in */
        retval = newAV();
        av_push(retval, newSVhek(stashhek)); /* us first */

        /* This loop won't terminate until we either finish building
           the MRO, or get an exception. */
        while(1) {
            SV* cand = NULL;
            SV* winner = NULL;
            int s;

            /* "foreach $seq (@seqs)" */
            SV** const avptr = AvARRAY(seqs);
            for(s = 0; s <= AvFILLp(seqs); s++) {
                SV** svp;
                AV * const seq = (AV*)(avptr[s]);
		SV* seqhead;
                if(!seq) continue; /* skip empty seqs */
                svp = av_fetch(seq, heads[s], 0);
                seqhead = *svp; /* seqhead = head of this seq */
                if(!winner) {
		    HE* tail_entry;
		    SV* val;
                    /* if we haven't found a winner for this round yet,
                       and this seqhead is not in tails (or the count
                       for it in tails has dropped to zero), then this
                       seqhead is our new winner, and is added to the
                       final MRO immediately */
                    cand = seqhead;
                    if((tail_entry = hv_fetch_ent(tails, cand, 0, 0))
                       && (val = HeVAL(tail_entry))
                       && (SvIVX(val) > 0))
                           continue;
                    winner = newSVsv(cand);
                    av_push(retval, winner);
                    /* note however that even when we find a winner,
                       we continue looping over @seqs to do housekeeping */
                }
                if(!sv_cmp(seqhead, winner)) {
                    /* Once we have a winner (including the iteration
                       where we first found him), inc the head ptr
                       for any seq which had the winner as a head,
                       NULL out any seq which is now empty,
                       and adjust tails for consistency */

                    const int new_head = ++heads[s];
                    if(new_head > AvFILLp(seq)) {
                        SvREFCNT_dec(avptr[s]);
                        avptr[s] = NULL;
                    }
                    else {
			HE* tail_entry;
			SV* val;
                        /* Because we know this new seqhead used to be
                           a tail, we can assume it is in tails and has
                           a positive value, which we need to dec */
                        svp = av_fetch(seq, new_head, 0);
                        seqhead = *svp;
                        tail_entry = hv_fetch_ent(tails, seqhead, 0, 0);
                        val = HeVAL(tail_entry);
                        sv_dec(val);
                    }
                }
            }

            /* if we found no candidates, we are done building the MRO.
               !cand means no seqs have any entries left to check */
            if(!cand) {
                Safefree(heads);
                break;
            }

            /* If we had candidates, but nobody won, then the @ISA
               hierarchy is not C3-incompatible */
            if(!winner) {
                /* we have to do some cleanup before we croak */

                SvREFCNT_dec(retval);
                Safefree(heads);

                Perl_croak(aTHX_ "Inconsistent hierarchy during C3 merge of class '%s': "
                    "merging failed on parent '%"SVf"'", HEK_KEY(stashhek), SVfARG(cand));
            }
        }
    }
    else { /* @ISA was undefined or empty */
        /* build a retval containing only ourselves */
        retval = newAV();
        av_push(retval, newSVhek(stashhek));
    }

    /* we don't want anyone modifying the cache entry but us,
       and we do so by replacing it completely */
    SvREADONLY_on(retval);

    meta->mro_linear_c3 = retval;
    return retval;
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

    assert(stash);
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

    if(!stashname)
        Perl_croak(aTHX_ "Can't call mro_isa_changed_in() on anonymous symbol table");

    /* wipe out the cached linearizations for this stash */
    meta = HvMROMETA(stash);
    SvREFCNT_dec((SV*)meta->mro_linear_dfs);
    SvREFCNT_dec((SV*)meta->mro_linear_c3);
    meta->mro_linear_dfs = NULL;
    meta->mro_linear_c3 = NULL;

    /* Inc the package generation, since our @ISA changed */
    meta->pkg_gen++;

    /* Wipe the global method cache if this package
       is UNIVERSAL or one of its parents */

    svp = hv_fetch(PL_isarev, stashname, stashname_len, 0);
    isarev = svp ? (HV*)*svp : NULL;

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
       wiping out their linearization and method caches */
    if(isarev) {
        hv_iterinit(isarev);
        while((iter = hv_iternext(isarev))) {
	    I32 len;
            const char* const revkey = hv_iterkey(iter, &len);
            HV* revstash = gv_stashpvn(revkey, len, 0);
            struct mro_meta* revmeta;

            if(!revstash) continue;
            revmeta = HvMROMETA(revstash);
            SvREFCNT_dec((SV*)revmeta->mro_linear_dfs);
            SvREFCNT_dec((SV*)revmeta->mro_linear_c3);
            revmeta->mro_linear_dfs = NULL;
            revmeta->mro_linear_c3 = NULL;
            if(!is_universal)
                revmeta->cache_gen++;
            if(revmeta->mro_nextmethod)
                hv_clear(revmeta->mro_nextmethod);
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
	   us, then we can detect it, because it will not be the correct type.
	   Probably faster and cleaner for us to free that scalar [very little
	   code actually executed to free it] and create a new HV than to
	   copy&paste [SIN!] the code from newHV() to allow us to upgrade the
	   new SV from SVt_NULL.  */

        mroisarev = (HV*)HeVAL(he);

	if(SvTYPE(mroisarev) != SVt_PVHV) {
	    SvREFCNT_dec(mroisarev);
	    mroisarev = newHV();
	    HeVAL(he) = (SV *)mroisarev;
        }

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
    HV * const isarev = svp ? (HV*)*svp : NULL;

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

/* These two are static helpers for next::method and friends,
   and re-implement a bunch of the code from pp_caller() in
   a more efficient manner for this particular usage.
*/

STATIC I32
__dopoptosub_at(const PERL_CONTEXT *cxstk, I32 startingblock) {
    I32 i;
    for (i = startingblock; i >= 0; i--) {
        if(CxTYPE((PERL_CONTEXT*)(&cxstk[i])) == CXt_SUB) return i;
    }
    return i;
}

#include "XSUB.h"

XS(XS_mro_get_linear_isa);
XS(XS_mro_set_mro);
XS(XS_mro_get_mro);
XS(XS_mro_get_isarev);
XS(XS_mro_is_universal);
XS(XS_mro_invalidate_method_caches);
XS(XS_mro_method_changed_in);
XS(XS_mro_get_pkg_gen);
XS(XS_mro_nextcan);

void
Perl_boot_core_mro(pTHX)
{
    dVAR;
    static const char file[] = __FILE__;

    newXSproto("mro::get_linear_isa", XS_mro_get_linear_isa, file, "$;$");
    newXSproto("mro::set_mro", XS_mro_set_mro, file, "$$");
    newXSproto("mro::get_mro", XS_mro_get_mro, file, "$");
    newXSproto("mro::get_isarev", XS_mro_get_isarev, file, "$");
    newXSproto("mro::is_universal", XS_mro_is_universal, file, "$");
    newXSproto("mro::invalidate_all_method_caches", XS_mro_invalidate_method_caches, file, "");
    newXSproto("mro::method_changed_in", XS_mro_method_changed_in, file, "$");
    newXSproto("mro::get_pkg_gen", XS_mro_get_pkg_gen, file, "$");
    newXS("mro::_nextcan", XS_mro_nextcan, file);
}

XS(XS_mro_get_linear_isa) {
    dVAR;
    dXSARGS;
    AV* RETVAL;
    HV* class_stash;
    SV* classname;

    PERL_UNUSED_ARG(cv);

    if(items < 1 || items > 2)
       Perl_croak(aTHX_ "Usage: mro::get_linear_isa(classname [, type ])");

    classname = ST(0);
    class_stash = gv_stashsv(classname, 0);

    if(!class_stash) {
        /* No stash exists yet, give them just the classname */
        AV* isalin = newAV();
        av_push(isalin, newSVsv(classname));
        ST(0) = sv_2mortal(newRV_noinc((SV*)isalin));
        XSRETURN(1);
    }
    else if(items > 1) {
        const char* const which = SvPV_nolen(ST(1));
	const struct mro_alg *const algo = S_get_mro_from_name(aTHX_ which);
	if (!algo)
	    Perl_croak(aTHX_ "Invalid mro name: '%s'", which);
	RETVAL = algo->resolve(aTHX_ class_stash, 0);
    }
    else {
        RETVAL = mro_get_linear_isa(class_stash);
    }

    ST(0) = newRV_inc((SV*)RETVAL);
    sv_2mortal(ST(0));
    XSRETURN(1);
}

XS(XS_mro_set_mro)
{
    dVAR;
    dXSARGS;
    SV* classname;
    const char* whichstr;
    const struct mro_alg *which;
    HV* class_stash;
    struct mro_meta* meta;

    PERL_UNUSED_ARG(cv);

    if (items != 2)
       Perl_croak(aTHX_ "Usage: mro::set_mro(classname, type)");

    classname = ST(0);
    whichstr = SvPV_nolen(ST(1));
    class_stash = gv_stashsv(classname, GV_ADD);
    if(!class_stash) Perl_croak(aTHX_ "Cannot create class: '%"SVf"'!", SVfARG(classname));
    meta = HvMROMETA(class_stash);

    which = S_get_mro_from_name(aTHX_ whichstr);
    if (!which)
        Perl_croak(aTHX_ "Invalid mro name: '%s'", whichstr);

    if(meta->mro_which != which) {
        meta->mro_which = which;
        /* Only affects local method cache, not
           even child classes */
        meta->cache_gen++;
        if(meta->mro_nextmethod)
            hv_clear(meta->mro_nextmethod);
    }

    XSRETURN_EMPTY;
}


XS(XS_mro_get_mro)
{
    dVAR;
    dXSARGS;
    SV* classname;
    HV* class_stash;

    PERL_UNUSED_ARG(cv);

    if (items != 1)
       Perl_croak(aTHX_ "Usage: mro::get_mro(classname)");

    classname = ST(0);
    class_stash = gv_stashsv(classname, 0);

    ST(0) = sv_2mortal(newSVpv(class_stash
			       ? HvMROMETA(class_stash)->mro_which->name
			       : "dfs", 0));
    XSRETURN(1);
}

XS(XS_mro_get_isarev)
{
    dVAR;
    dXSARGS;
    SV* classname;
    HE* he;
    HV* isarev;
    AV* ret_array;

    PERL_UNUSED_ARG(cv);

    if (items != 1)
       Perl_croak(aTHX_ "Usage: mro::get_isarev(classname)");

    classname = ST(0);

    SP -= items;

    
    he = hv_fetch_ent(PL_isarev, classname, 0, 0);
    isarev = he ? (HV*)HeVAL(he) : NULL;

    ret_array = newAV();
    if(isarev) {
        HE* iter;
        hv_iterinit(isarev);
        while((iter = hv_iternext(isarev)))
            av_push(ret_array, newSVsv(hv_iterkeysv(iter)));
    }
    XPUSHs(sv_2mortal(newRV_noinc((SV*)ret_array)));

    PUTBACK;
    return;
}

XS(XS_mro_is_universal)
{
    dVAR;
    dXSARGS;
    SV* classname;
    HV* isarev;
    char* classname_pv;
    STRLEN classname_len;
    HE* he;

    PERL_UNUSED_ARG(cv);

    if (items != 1)
       Perl_croak(aTHX_ "Usage: mro::is_universal(classname)");

    classname = ST(0);

    classname_pv = SvPV(classname,classname_len);

    he = hv_fetch_ent(PL_isarev, classname, 0, 0);
    isarev = he ? (HV*)HeVAL(he) : NULL;

    if((classname_len == 9 && strEQ(classname_pv, "UNIVERSAL"))
        || (isarev && hv_exists(isarev, "UNIVERSAL", 9)))
        XSRETURN_YES;
    else
        XSRETURN_NO;
}

XS(XS_mro_invalidate_method_caches)
{
    dVAR;
    dXSARGS;

    PERL_UNUSED_ARG(cv);

    if (items != 0)
        Perl_croak(aTHX_ "Usage: mro::invalidate_all_method_caches()");

    PL_sub_generation++;

    XSRETURN_EMPTY;
}

XS(XS_mro_method_changed_in)
{
    dVAR;
    dXSARGS;
    SV* classname;
    HV* class_stash;

    PERL_UNUSED_ARG(cv);

    if(items != 1)
        Perl_croak(aTHX_ "Usage: mro::method_changed_in(classname)");
    
    classname = ST(0);

    class_stash = gv_stashsv(classname, 0);
    if(!class_stash) Perl_croak(aTHX_ "No such class: '%"SVf"'!", SVfARG(classname));

    mro_method_changed_in(class_stash);

    XSRETURN_EMPTY;
}

XS(XS_mro_get_pkg_gen)
{
    dVAR;
    dXSARGS;
    SV* classname;
    HV* class_stash;

    PERL_UNUSED_ARG(cv);

    if(items != 1)
        Perl_croak(aTHX_ "Usage: mro::get_pkg_gen(classname)");
    
    classname = ST(0);

    class_stash = gv_stashsv(classname, 0);

    SP -= items;

    XPUSHs(sv_2mortal(newSViv(
        class_stash ? HvMROMETA(class_stash)->pkg_gen : 0
    )));
    
    PUTBACK;
    return;
}

XS(XS_mro_nextcan)
{
    dVAR;
    dXSARGS;
    SV* self = ST(0);
    const I32 throw_nomethod = SvIVX(ST(1));
    register I32 cxix = cxstack_ix;
    register const PERL_CONTEXT *ccstack = cxstack;
    const PERL_SI *top_si = PL_curstackinfo;
    HV* selfstash;
    SV *stashname;
    const char *fq_subname;
    const char *subname;
    STRLEN stashname_len;
    STRLEN subname_len;
    SV* sv;
    GV** gvp;
    AV* linear_av;
    SV** linear_svp;
    const char *hvname;
    I32 entries;
    struct mro_meta* selfmeta;
    HV* nmcache;
    I32 i;

    PERL_UNUSED_ARG(cv);

    SP -= items;

    if(sv_isobject(self))
        selfstash = SvSTASH(SvRV(self));
    else
        selfstash = gv_stashsv(self, 0);

    assert(selfstash);

    hvname = HvNAME_get(selfstash);
    if (!hvname)
        Perl_croak(aTHX_ "Can't use anonymous symbol table for method lookup");

    /* This block finds the contextually-enclosing fully-qualified subname,
       much like looking at (caller($i))[3] until you find a real sub that
       isn't ANON, etc (also skips over pureperl next::method, etc) */
    for(i = 0; i < 2; i++) {
        cxix = __dopoptosub_at(ccstack, cxix);
        for (;;) {
	    GV* cvgv;
	    STRLEN fq_subname_len;

            /* we may be in a higher stacklevel, so dig down deeper */
            while (cxix < 0) {
                if(top_si->si_type == PERLSI_MAIN)
                    Perl_croak(aTHX_ "next::method/next::can/maybe::next::method must be used in method context");
                top_si = top_si->si_prev;
                ccstack = top_si->si_cxstack;
                cxix = __dopoptosub_at(ccstack, top_si->si_cxix);
            }

            if(CxTYPE((PERL_CONTEXT*)(&ccstack[cxix])) != CXt_SUB
              || (PL_DBsub && GvCV(PL_DBsub) && ccstack[cxix].blk_sub.cv == GvCV(PL_DBsub))) {
                cxix = __dopoptosub_at(ccstack, cxix - 1);
                continue;
            }

            {
                const I32 dbcxix = __dopoptosub_at(ccstack, cxix - 1);
                if (PL_DBsub && GvCV(PL_DBsub) && dbcxix >= 0 && ccstack[dbcxix].blk_sub.cv == GvCV(PL_DBsub)) {
                    if(CxTYPE((PERL_CONTEXT*)(&ccstack[dbcxix])) != CXt_SUB) {
                        cxix = dbcxix;
                        continue;
                    }
                }
            }

            cvgv = CvGV(ccstack[cxix].blk_sub.cv);

            if(!isGV(cvgv)) {
                cxix = __dopoptosub_at(ccstack, cxix - 1);
                continue;
            }

            /* we found a real sub here */
            sv = sv_2mortal(newSV(0));

            gv_efullname3(sv, cvgv, NULL);

            fq_subname = SvPVX(sv);
            fq_subname_len = SvCUR(sv);

            subname = strrchr(fq_subname, ':');
            if(!subname)
                Perl_croak(aTHX_ "next::method/next::can/maybe::next::method cannot find enclosing method");

            subname++;
            subname_len = fq_subname_len - (subname - fq_subname);
            if(subname_len == 8 && strEQ(subname, "__ANON__")) {
                cxix = __dopoptosub_at(ccstack, cxix - 1);
                continue;
            }
            break;
        }
        cxix--;
    }

    /* If we made it to here, we found our context */

    /* Initialize the next::method cache for this stash
       if necessary */
    selfmeta = HvMROMETA(selfstash);
    if(!(nmcache = selfmeta->mro_nextmethod)) {
        nmcache = selfmeta->mro_nextmethod = newHV();
    }
    else { /* Use the cached coderef if it exists */
	HE* cache_entry = hv_fetch_ent(nmcache, sv, 0, 0);
	if (cache_entry) {
	    SV* const val = HeVAL(cache_entry);
	    if(val == &PL_sv_undef) {
		if(throw_nomethod)
		    Perl_croak(aTHX_ "No next::method '%s' found for %s", subname, hvname);
                XSRETURN_EMPTY;
	    }
	    XPUSHs(sv_2mortal(newRV_inc(val)));
            XSRETURN(1);
	}
    }

    /* beyond here is just for cache misses, so perf isn't as critical */

    stashname_len = subname - fq_subname - 2;
    stashname = sv_2mortal(newSVpvn(fq_subname, stashname_len));

    linear_av = mro_get_linear_isa_c3(selfstash, 0); /* has ourselves at the top of the list */

    linear_svp = AvARRAY(linear_av);
    entries = AvFILLp(linear_av) + 1;

    /* Walk down our MRO, skipping everything up
       to the contextually enclosing class */
    while (entries--) {
        SV * const linear_sv = *linear_svp++;
        assert(linear_sv);
        if(sv_eq(linear_sv, stashname))
            break;
    }

    /* Now search the remainder of the MRO for the
       same method name as the contextually enclosing
       method */
    if(entries > 0) {
        while (entries--) {
            SV * const linear_sv = *linear_svp++;
	    HV* curstash;
	    GV* candidate;
	    CV* cand_cv;

            assert(linear_sv);
            curstash = gv_stashsv(linear_sv, FALSE);

            if (!curstash) {
                if (ckWARN(WARN_SYNTAX))
                    Perl_warner(aTHX_ packWARN(WARN_SYNTAX), "Can't locate package %"SVf" for @%s::ISA",
                        (void*)linear_sv, hvname);
                continue;
            }

            assert(curstash);

            gvp = (GV**)hv_fetch(curstash, subname, subname_len, 0);
            if (!gvp) continue;

            candidate = *gvp;
            assert(candidate);

            if (SvTYPE(candidate) != SVt_PVGV)
                gv_init(candidate, curstash, subname, subname_len, TRUE);

            /* Notably, we only look for real entries, not method cache
               entries, because in C3 the method cache of a parent is not
               valid for the child */
            if (SvTYPE(candidate) == SVt_PVGV && (cand_cv = GvCV(candidate)) && !GvCVGEN(candidate)) {
                SvREFCNT_inc_simple_void_NN((SV*)cand_cv);
                (void)hv_store_ent(nmcache, newSVsv(sv), (SV*)cand_cv, 0);
                XPUSHs(sv_2mortal(newRV_inc((SV*)cand_cv)));
                XSRETURN(1);
            }
        }
    }

    (void)hv_store_ent(nmcache, newSVsv(sv), &PL_sv_undef, 0);
    if(throw_nomethod)
        Perl_croak(aTHX_ "No next::method '%s' found for %s", subname, hvname);
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
