/*    pad.c
 *
 *    Copyright (C) 2002, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 *  "Anyway: there was this Mr Frodo left an orphan and stranded, as you
 *  might say, among those queer Bucklanders, being brought up anyhow in
 *  Brandy Hall. A regular warren, by all accounts. Old Master Gorbadoc
 *  never had fewer than a couple of hundred relations in the place. Mr
 *  Bilbo never did a kinder deed than when he brought the lad back to
 *  live among decent folk." --the Gaffer
 */

/* XXX DAPM
 * As of Sept 2002, this file is new and may be in a state of flux for
 * a while. I've marked things I intent to come back and look at further
 * with an 'XXX DAPM' comment.
 */

/*
=head1 Pad Data Structures

=for apidoc m|AV *|CvPADLIST|CV *cv
CV's can have CvPADLIST(cv) set to point to an AV.

For these purposes "forms" are a kind-of CV, eval""s are too (except they're
not callable at will and are always thrown away after the eval"" is done
executing).

XSUBs don't have CvPADLIST set - dXSTARG fetches values from PL_curpad,
but that is really the callers pad (a slot of which is allocated by
every entersub).

The CvPADLIST AV has does not have AvREAL set, so REFCNT of component items
is managed "manual" (mostly in pad.c) rather than normal av.c rules.
The items in the AV are not SVs as for a normal AV, but other AVs:

0'th Entry of the CvPADLIST is an AV which represents the "names" or rather
the "static type information" for lexicals.

The CvDEPTH'th entry of CvPADLIST AV is an AV which is the stack frame at that
depth of recursion into the CV.
The 0'th slot of a frame AV is an AV which is @_.
other entries are storage for variables and op targets.

During compilation:
C<PL_comppad_name> is set to the names AV.
C<PL_comppad> is set to the frame AV for the frame CvDEPTH == 1.
C<PL_curpad> is set to the body of the frame AV (i.e. AvARRAY(PL_comppad)).

During execution, C<PL_comppad> and C<PL_curpad> refer to the live
frame of the currently executing sub.

Iterating over the names AV iterates over all possible pad
items. Pad slots that are SVs_PADTMP (targets/GVs/constants) end up having
&PL_sv_undef "names" (see pad_alloc()).

Only my/our variable (SVs_PADMY/SVs_PADOUR) slots get valid names.
The rest are op targets/GVs/constants which are statically allocated
or resolved at compile time.  These don't have names by which they
can be looked up from Perl code at run time through eval"" like
my/our variables can be.  Since they can't be looked up by "name"
but only by their index allocated at compile time (which is usually
in PL_op->op_targ), wasting a name SV for them doesn't make sense.

The SVs in the names AV have their PV being the name of the variable.
NV+1..IV inclusive is a range of cop_seq numbers for which the name is
valid.  For typed lexicals name SV is SVt_PVMG and SvSTASH points at the
type.  For C<our> lexicals, the type is SVt_PVGV, and GvSTASH points at the
stash of the associated global (so that duplicate C<our> delarations in the
same package can be detected).  SvCUR is sometimes hijacked to
store the generation number during compilation.

If SvFAKE is set on the name SV then slot in the frame AVs are
a REFCNT'ed references to a lexical from "outside". In this case,
the name SV does not have a cop_seq range, since it is in scope
throughout.

If the 'name' is '&' the corresponding entry in frame AV
is a CV representing a possible closure.
(SvFAKE and name of '&' is not a meaningful combination currently but could
become so if C<my sub foo {}> is implemented.)

=cut
*/


#include "EXTERN.h"
#define PERL_IN_PAD_C
#include "perl.h"


#define PAD_MAX 999999999



/*
=for apidoc pad_new

Create a new compiling padlist, saving and updating the various global
vars at the same time as creating the pad itself. The following flags
can be OR'ed together:

    padnew_CLONE	this pad is for a cloned CV
    padnew_SAVE		save old globals
    padnew_SAVESUB	also save extra stuff for start of sub

=cut
*/

PADLIST *
Perl_pad_new(pTHX_ int flags)
{
    AV *padlist, *padname, *pad, *a0;

    ASSERT_CURPAD_LEGAL("pad_new");

    /* XXX DAPM really need a new SAVEt_PAD which restores all or most
     * vars (based on flags) rather than storing vals + addresses for
     * each individually. Also see pad_block_start.
     * XXX DAPM Try to see whether all these conditionals are required
     */

    /* save existing state, ... */

    if (flags & padnew_SAVE) {
	SAVECOMPPAD();
	SAVESPTR(PL_comppad_name);
	if (! (flags & padnew_CLONE)) {
	    SAVEI32(PL_padix);
	    SAVEI32(PL_comppad_name_fill);
	    SAVEI32(PL_min_intro_pending);
	    SAVEI32(PL_max_intro_pending);
	    if (flags & padnew_SAVESUB) {
		SAVEI32(PL_pad_reset_pending);
	    }
	}
    }
    /* XXX DAPM interestingly, PL_comppad_name_floor never seems to be
     * saved - check at some pt that this is okay */

    /* ... create new pad ... */

    padlist	= newAV();
    padname	= newAV();
    pad		= newAV();

    if (flags & padnew_CLONE) {
	/* XXX DAPM  I dont know why cv_clone needs it
	 * doing differently yet - perhaps this separate branch can be
	 * dispensed with eventually ???
	 */

	a0 = newAV();			/* will be @_ */
	av_extend(a0, 0);
	av_store(pad, 0, (SV*)a0);
	AvFLAGS(a0) = AVf_REIFY;
    }
    else {
#ifdef USE_5005THREADS
	av_store(padname, 0, newSVpvn("@_", 2));
	a0 = newAV();
	SvPADMY_on((SV*)a0);		/* XXX Needed? */
	av_store(pad, 0, (SV*)a0);
#else
	av_store(pad, 0, Nullsv);
#endif /* USE_THREADS */
    }

    AvREAL_off(padlist);
    av_store(padlist, 0, (SV*)padname);
    av_store(padlist, 1, (SV*)pad);

    /* ... then update state variables */

    PL_comppad_name	= (AV*)(*av_fetch(padlist, 0, FALSE));
    PL_comppad		= (AV*)(*av_fetch(padlist, 1, FALSE));
    PL_curpad		= AvARRAY(PL_comppad);

    if (! (flags & padnew_CLONE)) {
	PL_comppad_name_fill = 0;
	PL_min_intro_pending = 0;
	PL_padix	     = 0;
    }

    DEBUG_X(PerlIO_printf(Perl_debug_log,
	  "Pad 0x%"UVxf"[0x%"UVxf"] new:       padlist=0x%"UVxf
	      " name=0x%"UVxf" flags=0x%"UVxf"\n",
	  PTR2UV(PL_comppad), PTR2UV(PL_curpad), PTR2UV(padlist),
	      PTR2UV(padname), (UV)flags
	)
    );

    return (PADLIST*)padlist;
}

/*
=for apidoc pad_undef

Free the padlist associated with a CV.
If parts of it happen to be current, we null the relevant
PL_*pad* global vars so that we don't have any dangling references left.
We also repoint the CvOUTSIDE of any about-to-be-orphaned
inner subs to the outer of this cv.

(This function should really be called pad_free, but the name was already
taken)

=cut
*/

void
Perl_pad_undef(pTHX_ CV* cv)
{
    I32 ix;
    PADLIST *padlist = CvPADLIST(cv);

    if (!padlist)
	return;
    if (!SvREFCNT(CvPADLIST(cv))) /* may be during global destruction */
	return;

    DEBUG_X(PerlIO_printf(Perl_debug_log,
	  "Pad undef: padlist=0x%"UVxf"\n" , PTR2UV(padlist))
    );

    /* detach any '&' anon children in the pad; if afterwards they
     * are still live, fix up their CvOUTSIDEs to point to our outside,
     * bypassing us. */
    /* XXX DAPM for efficiency, we should only do this if we know we have
     * children, or integrate this loop with general cleanup */

    if (!PL_dirty) { /* don't bother during global destruction */
	CV *outercv = CvOUTSIDE(cv);
	U32 seq = CvOUTSIDE_SEQ(cv);
	AV *comppad_name = (AV*)AvARRAY(padlist)[0];
	SV **namepad = AvARRAY(comppad_name);
	AV *comppad = (AV*)AvARRAY(padlist)[1];
	SV **curpad = AvARRAY(comppad);
	for (ix = AvFILLp(comppad_name); ix > 0; ix--) {
	    SV *namesv = namepad[ix];
	    if (namesv && namesv != &PL_sv_undef
		&& *SvPVX(namesv) == '&')
	    {
		CV *innercv = (CV*)curpad[ix];
		namepad[ix] = Nullsv;
		SvREFCNT_dec(namesv);
		curpad[ix] = Nullsv;
		SvREFCNT_dec(innercv);
		if (SvREFCNT(innercv) /* in use, not just a prototype */
		    && CvOUTSIDE(innercv) == cv)
		{
		    assert(CvWEAKOUTSIDE(innercv));
		    /* don't relink to grandfather if he's being freed */
		    if (outercv && SvREFCNT(outercv)) {
			CvWEAKOUTSIDE_off(innercv);
			CvOUTSIDE(innercv) = outercv;
			CvOUTSIDE_SEQ(innercv) = seq;
			SvREFCNT_inc(outercv);
		    }
		    else {
			CvOUTSIDE(innercv) = Nullcv;
		    }

		}

	    }
	}
    }

    ix = AvFILLp(padlist);
    while (ix >= 0) {
	SV* sv = AvARRAY(padlist)[ix--];
	if (!sv)
	    continue;
	if (sv == (SV*)PL_comppad_name)
	    PL_comppad_name = Nullav;
	else if (sv == (SV*)PL_comppad) {
	    PL_comppad = Null(PAD*);
	    PL_curpad = Null(SV**);
	}
	SvREFCNT_dec(sv);
    }
    SvREFCNT_dec((SV*)CvPADLIST(cv));
    CvPADLIST(cv) = Null(PADLIST*);
}




/*
=for apidoc pad_add_name

Create a new name in the current pad at the specified offset.
If C<typestash> is valid, the name is for a typed lexical; set the
name's stash to that value.
If C<ourstash> is valid, it's an our lexical, set the name's
GvSTASH to that value

Also, if the name is @.. or %.., create a new array or hash for that slot

If fake, it means we're cloning an existing entry

=cut
*/

/*
 * XXX DAPM this doesn't seem the right place to create a new array/hash.
 * Whatever we do, we should be consistent - create scalars too, and
 * create even if fake. Really need to integrate better the whole entry
 * creation business - when + where does the name and value get created?
 */

PADOFFSET
Perl_pad_add_name(pTHX_ char *name, HV* typestash, HV* ourstash, bool fake)
{
    PADOFFSET offset = pad_alloc(OP_PADSV, SVs_PADMY);
    SV* namesv = NEWSV(1102, 0);

    ASSERT_CURPAD_ACTIVE("pad_add_name");


    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
	  "Pad addname: %ld \"%s\"%s\n",
	   (long)offset, name, (fake ? " FAKE" : "")
	  )
    );

    sv_upgrade(namesv, ourstash ? SVt_PVGV : typestash ? SVt_PVMG : SVt_PVNV);
    sv_setpv(namesv, name);

    if (typestash) {
	SvFLAGS(namesv) |= SVpad_TYPED;
	SvSTASH(namesv) = (HV*)SvREFCNT_inc((SV*) typestash);
    }
    if (ourstash) {
	SvFLAGS(namesv) |= SVpad_OUR;
	GvSTASH(namesv) = (HV*)SvREFCNT_inc((SV*) ourstash);
    }

    av_store(PL_comppad_name, offset, namesv);
    if (fake)
	SvFAKE_on(namesv);
    else {
	/* not yet introduced */
	SvNVX(namesv) = (NV)PAD_MAX;	/* min */
	SvIVX(namesv) = 0;		/* max */

	if (!PL_min_intro_pending)
	    PL_min_intro_pending = offset;
	PL_max_intro_pending = offset;
	/* XXX DAPM since slot has been allocated, replace
	 * av_store with PL_curpad[offset] ? */
	if (*name == '@')
	    av_store(PL_comppad, offset, (SV*)newAV());
	else if (*name == '%')
	    av_store(PL_comppad, offset, (SV*)newHV());
	SvPADMY_on(PL_curpad[offset]);
    }

    return offset;
}




/*
=for apidoc pad_alloc

Allocate a new my or tmp pad entry. For a my, simply push a null SV onto
the end of PL_comppad, but for a tmp, scan the pad from PL_padix upwards
for a slot which has no name and and no active value.

=cut
*/

/* XXX DAPM integrate alloc(), add_name() and add_anon(),
 * or at least rationalise ??? */


PADOFFSET
Perl_pad_alloc(pTHX_ I32 optype, U32 tmptype)
{
    SV *sv;
    I32 retval;

    ASSERT_CURPAD_ACTIVE("pad_alloc");

    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_alloc");
    if (PL_pad_reset_pending)
	pad_reset();
    if (tmptype & SVs_PADMY) {
	do {
	    sv = *av_fetch(PL_comppad, AvFILLp(PL_comppad) + 1, TRUE);
	} while (SvPADBUSY(sv));		/* need a fresh one */
	retval = AvFILLp(PL_comppad);
    }
    else {
	SV **names = AvARRAY(PL_comppad_name);
	SSize_t names_fill = AvFILLp(PL_comppad_name);
	for (;;) {
	    /*
	     * "foreach" index vars temporarily become aliases to non-"my"
	     * values.  Thus we must skip, not just pad values that are
	     * marked as current pad values, but also those with names.
	     */
	    /* HVDS why copy to sv here? we don't seem to use it */
	    if (++PL_padix <= names_fill &&
		   (sv = names[PL_padix]) && sv != &PL_sv_undef)
		continue;
	    sv = *av_fetch(PL_comppad, PL_padix, TRUE);
	    if (!(SvFLAGS(sv) & (SVs_PADTMP | SVs_PADMY)) &&
		!IS_PADGV(sv) && !IS_PADCONST(sv))
		break;
	}
	retval = PL_padix;
    }
    SvFLAGS(sv) |= tmptype;
    PL_curpad = AvARRAY(PL_comppad);

    DEBUG_X(PerlIO_printf(Perl_debug_log,
	  "Pad 0x%"UVxf"[0x%"UVxf"] alloc:   %ld for %s\n",
	  PTR2UV(PL_comppad), PTR2UV(PL_curpad), (long) retval,
	  PL_op_name[optype]));
    return (PADOFFSET)retval;
}

/*
=for apidoc pad_add_anon

Add an anon code entry to the current compiling pad

=cut
*/

PADOFFSET
Perl_pad_add_anon(pTHX_ SV* sv, OPCODE op_type)
{
    PADOFFSET ix;
    SV* name;

    name = NEWSV(1106, 0);
    sv_upgrade(name, SVt_PVNV);
    sv_setpvn(name, "&", 1);
    SvIVX(name) = -1;
    SvNVX(name) = 1;
    ix = pad_alloc(op_type, SVs_PADMY);
    av_store(PL_comppad_name, ix, name);
    /* XXX DAPM use PL_curpad[] ? */
    av_store(PL_comppad, ix, sv);
    SvPADMY_on(sv);

    /* to avoid ref loops, we never have parent + child referencing each
     * other simultaneously */
    if (CvOUTSIDE((CV*)sv)) {
	assert(!CvWEAKOUTSIDE((CV*)sv));
	CvWEAKOUTSIDE_on((CV*)sv);
	SvREFCNT_dec(CvOUTSIDE((CV*)sv));
    }
    return ix;
}



/*
=for apidoc pad_check_dup

Check for duplicate declarations: report any of:
     * a my in the current scope with the same name;
     * an our (anywhere in the pad) with the same name and the same stash
       as C<ourstash>
C<is_our> indicates that the name to check is an 'our' declaration

=cut
*/

/* XXX DAPM integrate this into pad_add_name ??? */

void
Perl_pad_check_dup(pTHX_ char *name, bool is_our, HV *ourstash)
{
    SV		**svp, *sv;
    PADOFFSET	top, off;

    ASSERT_CURPAD_ACTIVE("pad_check_dup");
    if (!ckWARN(WARN_MISC) || AvFILLp(PL_comppad_name) < 0)
	return; /* nothing to check */

    svp = AvARRAY(PL_comppad_name);
    top = AvFILLp(PL_comppad_name);
    /* check the current scope */
    /* XXX DAPM - why the (I32) cast - shouldn't we ensure they're the same
     * type ? */
    for (off = top; (I32)off > PL_comppad_name_floor; off--) {
	if ((sv = svp[off])
	    && sv != &PL_sv_undef
	    && !SvFAKE(sv)
	    && (SvIVX(sv) == PAD_MAX || SvIVX(sv) == 0)
	    && (!is_our
		|| ((SvFLAGS(sv) & SVpad_OUR) && GvSTASH(sv) == ourstash))
	    && strEQ(name, SvPVX(sv)))
	{
	    Perl_warner(aTHX_ packWARN(WARN_MISC),
		"\"%s\" variable %s masks earlier declaration in same %s",
		(is_our ? "our" : "my"),
		name,
		(SvIVX(sv) == PAD_MAX ? "scope" : "statement"));
	    --off;
	    break;
	}
    }
    /* check the rest of the pad */
    if (is_our) {
	do {
	    if ((sv = svp[off])
		&& sv != &PL_sv_undef
		&& !SvFAKE(sv)
		&& (SvIVX(sv) == PAD_MAX || SvIVX(sv) == 0)
		&& ((SvFLAGS(sv) & SVpad_OUR) && GvSTASH(sv) == ourstash)
		&& strEQ(name, SvPVX(sv)))
	    {
		Perl_warner(aTHX_ packWARN(WARN_MISC),
		    "\"our\" variable %s redeclared", name);
		Perl_warner(aTHX_ packWARN(WARN_MISC),
		    "\t(Did you mean \"local\" instead of \"our\"?)\n");
		break;
	    }
	} while ( off-- > 0 );
    }
}



/*
=for apidoc pad_findmy

Given a lexical name, try to find its offset, first in the current pad,
or failing that, in the pads of any lexically enclosing subs (including
the complications introduced by eval). If the name is found in an outer pad,
then a fake entry is added to the current pad.
Returns the offset in the current pad, or NOT_IN_PAD on failure.

=cut
*/

PADOFFSET
Perl_pad_findmy(pTHX_ char *name)
{
    I32 off;
    I32 fake_off = 0;
    I32 our_off = 0;
    SV *sv;
    SV **svp = AvARRAY(PL_comppad_name);
    U32 seq = PL_cop_seqmax;

    ASSERT_CURPAD_ACTIVE("pad_findmy");
    DEBUG_Xv(PerlIO_printf(Perl_debug_log, "Pad findmy:  \"%s\"\n", name));

#ifdef USE_5005THREADS
    /*
     * Special case to get lexical (and hence per-thread) @_.
     * XXX I need to find out how to tell at parse-time whether use
     * of @_ should refer to a lexical (from a sub) or defgv (global
     * scope and maybe weird sub-ish things like formats). See
     * startsub in perly.y.  It's possible that @_ could be lexical
     * (at least from subs) even in non-threaded perl.
     */
    if (strEQ(name, "@_"))
	return 0;		/* success. (NOT_IN_PAD indicates failure) */
#endif /* USE_5005THREADS */

    /* The one we're looking for is probably just before comppad_name_fill. */
    for (off = AvFILLp(PL_comppad_name); off > 0; off--) {
	sv = svp[off];
	if (!sv || sv == &PL_sv_undef || !strEQ(SvPVX(sv), name))
	    continue;
	if (SvFAKE(sv)) {
	    /* we'll use this later if we don't find a real entry */
	    fake_off = off;
	    continue;
	}
	else {
	    if (   seq >  U_32(SvNVX(sv))	/* min */
		&& seq <= (U32)SvIVX(sv))	/* max */
		return off;
	    else if ((SvFLAGS(sv) & SVpad_OUR)
		    && U_32(SvNVX(sv)) == PAD_MAX) /* min */
	    {
		/* look for an our that's being introduced; this allows
		 *    our $foo = 0 unless defined $foo;
		 * to not give a warning. (Yes, this is a hack) */
		our_off = off;
	    }
	}
    }
    if (fake_off)
	return fake_off;

    /* See if it's in a nested scope */
    off = pad_findlex(name, 0, PL_compcv);
    if (off)			/* pad_findlex returns 0 for failure...*/
	return off;
    if (our_off)
	return our_off;
    return NOT_IN_PAD;		/* ...but we return NOT_IN_PAD for failure */

}



/*
=for apidoc pad_findlex

Find a named lexical anywhere in a chain of nested pads. Add fake entries
in the inner pads if it's found in an outer one. innercv is the CV *inside*
the chain of outer CVs to be searched. If newoff is non-null, this is a
run-time cloning: don't add fake entries, just find the lexical and add a
ref to it at newoff in the current pad.

=cut
*/

STATIC PADOFFSET
S_pad_findlex(pTHX_ char *name, PADOFFSET newoff, CV* innercv)
{
    CV *cv;
    I32 off = 0;
    SV *sv;
    CV* startcv;
    U32 seq;
    I32 depth;
    AV *oldpad;
    SV *oldsv;
    AV *curlist;

    ASSERT_CURPAD_ACTIVE("pad_findlex");
    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
	"Pad findlex: \"%s\" off=%ld startcv=0x%"UVxf"\n",
	    name, (long)newoff, PTR2UV(innercv))
    );

    seq = CvOUTSIDE_SEQ(innercv);
    startcv = CvOUTSIDE(innercv);

    for (cv = startcv; cv; seq = CvOUTSIDE_SEQ(cv), cv = CvOUTSIDE(cv)) {
	SV **svp;
	AV *curname;
	I32 fake_off = 0;

	DEBUG_Xv(PerlIO_printf(Perl_debug_log,
	    "             searching: cv=0x%"UVxf" seq=%d\n",
	    PTR2UV(cv), (int) seq )
	);

	curlist = CvPADLIST(cv);
	if (!curlist)
	    continue; /* an undef CV */
	svp = av_fetch(curlist, 0, FALSE);
	if (!svp || *svp == &PL_sv_undef)
	    continue;
	curname = (AV*)*svp;
	svp = AvARRAY(curname);

	depth = CvDEPTH(cv);
	for (off = AvFILLp(curname); off > 0; off--) {
	    sv = svp[off];
	    if (!sv || sv == &PL_sv_undef || !strEQ(SvPVX(sv), name))
		continue;
	    if (SvFAKE(sv)) {
		/* we'll use this later if we don't find a real entry */
		fake_off = off;
		continue;
	    }
	    else {
		if (   seq >  U_32(SvNVX(sv))	/* min */
		    && seq <= (U32)SvIVX(sv)	/* max */
		    && !(newoff && !depth) /* ignore inactive when cloning */
		)
		    goto found;
	    }
	}

	/* no real entry - but did we find a fake one? */
	if (fake_off) {
	    if (newoff && !depth)
		return 0; /* don't clone from inactive stack frame */
	    off = fake_off;
	    sv = svp[off];
	    goto found;
	}
    }
    return 0;

found:

    if (!depth) 
	depth = 1;

    oldpad = (AV*)AvARRAY(curlist)[depth];
    oldsv = *av_fetch(oldpad, off, TRUE);

#ifdef DEBUGGING
    if (SvFAKE(sv))
	DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		"             matched:   offset %ld"
		    " FAKE, sv=0x%"UVxf"\n",
		(long)off,
		PTR2UV(oldsv)
	    )
	);
    else
	DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		"             matched:   offset %ld"
		    " (%lu,%lu), sv=0x%"UVxf"\n",
		(long)off,
		(unsigned long)U_32(SvNVX(sv)),
		(unsigned long)SvIVX(sv),
		PTR2UV(oldsv)
	    )
	);
#endif

    if (!newoff) {		/* Not a mere clone operation. */
	newoff = pad_add_name(
	    SvPVX(sv),
	    (SvFLAGS(sv) & SVpad_TYPED) ? SvSTASH(sv) : Nullhv,
	    (SvFLAGS(sv) & SVpad_OUR)   ? GvSTASH(sv) : Nullhv,
	    1  /* fake */
	);

	if (CvANON(PL_compcv) || SvTYPE(PL_compcv) == SVt_PVFM) {
	    /* "It's closures all the way down." */
	    CvCLONE_on(PL_compcv);
	    if (cv == startcv) {
		if (CvANON(PL_compcv))
		    oldsv = Nullsv; /* no need to keep ref */
	    }
	    else {
		CV *bcv;
		for (bcv = startcv;
		     bcv && bcv != cv && !CvCLONE(bcv);
		     bcv = CvOUTSIDE(bcv))
		{
		    if (CvANON(bcv)) {
			/* install the missing pad entry in intervening
			 * nested subs and mark them cloneable. */
			AV *ocomppad_name = PL_comppad_name;
			PAD *ocomppad = PL_comppad;
			AV *padlist = CvPADLIST(bcv);
			PL_comppad_name = (AV*)AvARRAY(padlist)[0];
			PL_comppad = (AV*)AvARRAY(padlist)[1];
			PL_curpad = AvARRAY(PL_comppad);
			pad_add_name(
			    SvPVX(sv),
			    (SvFLAGS(sv) & SVpad_TYPED)
				? SvSTASH(sv) : Nullhv,
			    (SvFLAGS(sv) & SVpad_OUR)
				? GvSTASH(sv) : Nullhv,
			    1  /* fake */
			);

			PL_comppad_name = ocomppad_name;
			PL_comppad = ocomppad;
			PL_curpad = ocomppad ?
				AvARRAY(ocomppad) : Null(SV **);
			CvCLONE_on(bcv);
		    }
		    else {
			if (ckWARN(WARN_CLOSURE)
			    && !CvUNIQUE(bcv) && !CvUNIQUE(cv))
			{
			    Perl_warner(aTHX_ packWARN(WARN_CLOSURE),
			      "Variable \"%s\" may be unavailable",
				 name);
			}
			break;
		    }
		}
	    }
	}
	else if (!CvUNIQUE(PL_compcv)) {
	    if (ckWARN(WARN_CLOSURE) && !SvFAKE(sv) && !CvUNIQUE(cv)
		&& !(SvFLAGS(sv) & SVpad_OUR))
	    {
		Perl_warner(aTHX_ packWARN(WARN_CLOSURE),
		    "Variable \"%s\" will not stay shared", name);
	    }
	}
    }
    av_store(PL_comppad, newoff, SvREFCNT_inc(oldsv));
    ASSERT_CURPAD_ACTIVE("pad_findlex 2");
    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		"Pad findlex: set offset %ld to sv 0x%"UVxf"\n",
		(long)newoff, PTR2UV(oldsv)
	    )
    );
    return newoff;
}


/*
=for apidoc pad_sv

Get the value at offset po in the current pad.
Use macro PAD_SV instead of calling this function directly.

=cut
*/


SV *
Perl_pad_sv(pTHX_ PADOFFSET po)
{
    ASSERT_CURPAD_ACTIVE("pad_sv");

#ifndef USE_5005THREADS
    if (!po)
	Perl_croak(aTHX_ "panic: pad_sv po");
#endif
    DEBUG_X(PerlIO_printf(Perl_debug_log,
	"Pad 0x%"UVxf"[0x%"UVxf"] sv:      %ld sv=0x%"UVxf"\n",
	PTR2UV(PL_comppad), PTR2UV(PL_curpad), (long)po, PTR2UV(PL_curpad[po]))
    );
    return PL_curpad[po];
}


/*
=for apidoc pad_setsv

Set the entry at offset po in the current pad to sv.
Use the macro PAD_SETSV() rather than calling this function directly.

=cut
*/

#ifdef DEBUGGING
void
Perl_pad_setsv(pTHX_ PADOFFSET po, SV* sv)
{
    ASSERT_CURPAD_ACTIVE("pad_setsv");

    DEBUG_X(PerlIO_printf(Perl_debug_log,
	"Pad 0x%"UVxf"[0x%"UVxf"] setsv:   %ld sv=0x%"UVxf"\n",
	PTR2UV(PL_comppad), PTR2UV(PL_curpad), (long)po, PTR2UV(sv))
    );
    PL_curpad[po] = sv;
}
#endif



/*
=for apidoc pad_block_start

Update the pad compilation state variables on entry to a new block

=cut
*/

/* XXX DAPM perhaps:
 * 	- integrate this in general state-saving routine ???
 * 	- combine with the state-saving going on in pad_new ???
 * 	- introduce a new SAVE type that does all this in one go ?
 */

void
Perl_pad_block_start(pTHX_ int full)
{
    ASSERT_CURPAD_ACTIVE("pad_block_start");
    SAVEI32(PL_comppad_name_floor);
    PL_comppad_name_floor = AvFILLp(PL_comppad_name);
    if (full)
	PL_comppad_name_fill = PL_comppad_name_floor;
    if (PL_comppad_name_floor < 0)
	PL_comppad_name_floor = 0;
    SAVEI32(PL_min_intro_pending);
    SAVEI32(PL_max_intro_pending);
    PL_min_intro_pending = 0;
    SAVEI32(PL_comppad_name_fill);
    SAVEI32(PL_padix_floor);
    PL_padix_floor = PL_padix;
    PL_pad_reset_pending = FALSE;
}


/*
=for apidoc intro_my

"Introduce" my variables to visible status.

=cut
*/

U32
Perl_intro_my(pTHX)
{
    SV **svp;
    SV *sv;
    I32 i;

    ASSERT_CURPAD_ACTIVE("intro_my");
    if (! PL_min_intro_pending)
	return PL_cop_seqmax;

    svp = AvARRAY(PL_comppad_name);
    for (i = PL_min_intro_pending; i <= PL_max_intro_pending; i++) {
	if ((sv = svp[i]) && sv != &PL_sv_undef
		&& !SvFAKE(sv) && !SvIVX(sv))
	{
	    SvIVX(sv) = PAD_MAX;	/* Don't know scope end yet. */
	    SvNVX(sv) = (NV)PL_cop_seqmax;
	    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		"Pad intromy: %ld \"%s\", (%lu,%lu)\n",
		(long)i, SvPVX(sv),
		(unsigned long)U_32(SvNVX(sv)), (unsigned long)SvIVX(sv))
	    );
	}
    }
    PL_min_intro_pending = 0;
    PL_comppad_name_fill = PL_max_intro_pending; /* Needn't search higher */
    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		"Pad intromy: seq -> %ld\n", (long)(PL_cop_seqmax+1)));

    return PL_cop_seqmax++;
}

/*
=for apidoc pad_leavemy

Cleanup at end of scope during compilation: set the max seq number for
lexicals in this scope and warn of any lexicals that never got introduced.

=cut
*/

void
Perl_pad_leavemy(pTHX)
{
    I32 off;
    SV **svp = AvARRAY(PL_comppad_name);
    SV *sv;

    PL_pad_reset_pending = FALSE;

    ASSERT_CURPAD_ACTIVE("pad_leavemy");
    if (PL_min_intro_pending && PL_comppad_name_fill < PL_min_intro_pending) {
	for (off = PL_max_intro_pending; off >= PL_min_intro_pending; off--) {
	    if ((sv = svp[off]) && sv != &PL_sv_undef
		    && !SvFAKE(sv) && ckWARN_d(WARN_INTERNAL))
		Perl_warner(aTHX_ packWARN(WARN_INTERNAL),
					"%"SVf" never introduced", sv);
	}
    }
    /* "Deintroduce" my variables that are leaving with this scope. */
    for (off = AvFILLp(PL_comppad_name); off > PL_comppad_name_fill; off--) {
	if ((sv = svp[off]) && sv != &PL_sv_undef
		&& !SvFAKE(sv) && SvIVX(sv) == PAD_MAX)
	{
	    SvIVX(sv) = PL_cop_seqmax;
	    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		"Pad leavemy: %ld \"%s\", (%lu,%lu)\n",
		(long)off, SvPVX(sv),
		(unsigned long)U_32(SvNVX(sv)), (unsigned long)SvIVX(sv))
	    );
	}
    }
    PL_cop_seqmax++;
    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
	    "Pad leavemy: seq = %ld\n", (long)PL_cop_seqmax));
}


/*
=for apidoc pad_swipe

Abandon the tmp in the current pad at offset po and replace with a
new one.

=cut
*/

void
Perl_pad_swipe(pTHX_ PADOFFSET po, bool refadjust)
{
    ASSERT_CURPAD_LEGAL("pad_swipe");
    if (!PL_curpad)
	return;
    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_swipe curpad");
    if (!po)
	Perl_croak(aTHX_ "panic: pad_swipe po");

    DEBUG_X(PerlIO_printf(Perl_debug_log,
		"Pad 0x%"UVxf"[0x%"UVxf"] swipe:   %ld\n",
		PTR2UV(PL_comppad), PTR2UV(PL_curpad), (long)po));

    if (PL_curpad[po])
	SvPADTMP_off(PL_curpad[po]);
    if (refadjust)
	SvREFCNT_dec(PL_curpad[po]);

    PL_curpad[po] = NEWSV(1107,0);
    SvPADTMP_on(PL_curpad[po]);
    if ((I32)po < PL_padix)
	PL_padix = po - 1;
}


/*
=for apidoc pad_reset

Mark all the current temporaries for reuse

=cut
*/

/* XXX pad_reset() is currently disabled because it results in serious bugs.
 * It causes pad temp TARGs to be shared between OPs. Since TARGs are pushed
 * on the stack by OPs that use them, there are several ways to get an alias
 * to  a shared TARG.  Such an alias will change randomly and unpredictably.
 * We avoid doing this until we can think of a Better Way.
 * GSAR 97-10-29 */
void
Perl_pad_reset(pTHX)
{
#ifdef USE_BROKEN_PAD_RESET
    register I32 po;

    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_reset curpad");

    DEBUG_X(PerlIO_printf(Perl_debug_log,
	    "Pad 0x%"UVxf"[0x%"UVxf"] reset:     padix %ld -> %ld",
	    PTR2UV(PL_comppad), PTR2UV(PL_curpad),
		(long)PL_padix, (long)PL_padix_floor
	    )
    );

    if (!PL_tainting) {	/* Can't mix tainted and non-tainted temporaries. */
	for (po = AvMAX(PL_comppad); po > PL_padix_floor; po--) {
	    if (PL_curpad[po] && !SvIMMORTAL(PL_curpad[po]))
		SvPADTMP_off(PL_curpad[po]);
	}
	PL_padix = PL_padix_floor;
    }
#endif
    PL_pad_reset_pending = FALSE;
}


/*
=for apidoc pad_tidy

Tidy up a pad after we've finished compiling it:
    * remove most stuff from the pads of anonsub prototypes;
    * give it a @_;
    * mark tmps as such.

=cut
*/

/* XXX DAPM surely most of this stuff should be done properly
 * at the right time beforehand, rather than going around afterwards
 * cleaning up our mistakes ???
 */

void
Perl_pad_tidy(pTHX_ padtidy_type type)
{
    PADOFFSET ix;

    ASSERT_CURPAD_ACTIVE("pad_tidy");
    /* extend curpad to match namepad */
    if (AvFILLp(PL_comppad_name) < AvFILLp(PL_comppad))
	av_store(PL_comppad_name, AvFILLp(PL_comppad), Nullsv);

    if (type == padtidy_SUBCLONE) {
	SV **namep = AvARRAY(PL_comppad_name);
	for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	    SV *namesv;

	    if (SvIMMORTAL(PL_curpad[ix]) || IS_PADGV(PL_curpad[ix]) || IS_PADCONST(PL_curpad[ix]))
		continue;
	    /*
	     * The only things that a clonable function needs in its
	     * pad are references to outer lexicals and anonymous subs.
	     * The rest are created anew during cloning.
	     */
	    if (!((namesv = namep[ix]) != Nullsv &&
		  namesv != &PL_sv_undef &&
		  (SvFAKE(namesv) ||
		   *SvPVX(namesv) == '&')))
	    {
		SvREFCNT_dec(PL_curpad[ix]);
		PL_curpad[ix] = Nullsv;
	    }
	}
    }
    else if (type == padtidy_SUB) {
	/* XXX DAPM this same bit of code keeps appearing !!! Rationalise? */
	AV *av = newAV();			/* Will be @_ */
	av_extend(av, 0);
	av_store(PL_comppad, 0, (SV*)av);
	AvFLAGS(av) = AVf_REIFY;
    }

    /* XXX DAPM rationalise these two similar branches */

    if (type == padtidy_SUB) {
	for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	    if (SvIMMORTAL(PL_curpad[ix]) || IS_PADGV(PL_curpad[ix]) || IS_PADCONST(PL_curpad[ix]))
		continue;
	    if (!SvPADMY(PL_curpad[ix]))
		SvPADTMP_on(PL_curpad[ix]);
	}
    }
    else if (type == padtidy_FORMAT) {
	for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	    if (!SvPADMY(PL_curpad[ix]) && !SvIMMORTAL(PL_curpad[ix]))
		SvPADTMP_on(PL_curpad[ix]);
	}
    }
    PL_curpad = AvARRAY(PL_comppad);
}


/*
=for apidoc pad_free

Free the SV at offet po in the current pad.

=cut
*/

/* XXX DAPM integrate with pad_swipe ???? */
void
Perl_pad_free(pTHX_ PADOFFSET po)
{
    ASSERT_CURPAD_LEGAL("pad_free");
    if (!PL_curpad)
	return;
    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_free curpad");
    if (!po)
	Perl_croak(aTHX_ "panic: pad_free po");

    DEBUG_X(PerlIO_printf(Perl_debug_log,
	    "Pad 0x%"UVxf"[0x%"UVxf"] free:    %ld\n",
	    PTR2UV(PL_comppad), PTR2UV(PL_curpad), (long)po)
    );

    if (PL_curpad[po] && PL_curpad[po] != &PL_sv_undef) {
	SvPADTMP_off(PL_curpad[po]);
#ifdef USE_ITHREADS
	/* SV could be a shared hash key (eg bugid #19022) */
	if (!SvFAKE(PL_curpad[po]))
	    SvREADONLY_off(PL_curpad[po]);	/* could be a freed constant */
#endif

    }
    if ((I32)po < PL_padix)
	PL_padix = po - 1;
}



/*
=for apidoc do_dump_pad

Dump the contents of a padlist

=cut
*/

void
Perl_do_dump_pad(pTHX_ I32 level, PerlIO *file, PADLIST *padlist, int full)
{
    AV *pad_name;
    AV *pad;
    SV **pname;
    SV **ppad;
    SV *namesv;
    I32 ix;

    if (!padlist) {
	return;
    }
    pad_name = (AV*)*av_fetch((AV*)padlist, 0, FALSE);
    pad = (AV*)*av_fetch((AV*)padlist, 1, FALSE);
    pname = AvARRAY(pad_name);
    ppad = AvARRAY(pad);
    Perl_dump_indent(aTHX_ level, file,
	    "PADNAME = 0x%"UVxf"(0x%"UVxf") PAD = 0x%"UVxf"(0x%"UVxf")\n",
	    PTR2UV(pad_name), PTR2UV(pname), PTR2UV(pad), PTR2UV(ppad)
    );

    for (ix = 1; ix <= AvFILLp(pad_name); ix++) {
	namesv = pname[ix];
	if (namesv && namesv == &PL_sv_undef) {
	    namesv = Nullsv;
	}
	if (namesv) {
	    if (SvFAKE(namesv))
		Perl_dump_indent(aTHX_ level+1, file,
		    "%2d. 0x%"UVxf"<%lu> FAKE \"%s\"\n",
		    (int) ix,
		    PTR2UV(ppad[ix]),
		    (unsigned long) (ppad[ix] ? SvREFCNT(ppad[ix]) : 0),
		    SvPVX(namesv)
		);
	    else
		Perl_dump_indent(aTHX_ level+1, file,
		    "%2d. 0x%"UVxf"<%lu> (%lu,%lu) \"%s\"\n",
		    (int) ix,
		    PTR2UV(ppad[ix]),
		    (unsigned long) (ppad[ix] ? SvREFCNT(ppad[ix]) : 0),
		    (unsigned long)U_32(SvNVX(namesv)),
		    (unsigned long)SvIVX(namesv),
		    SvPVX(namesv)
		);
	}
	else if (full) {
	    Perl_dump_indent(aTHX_ level+1, file,
		"%2d. 0x%"UVxf"<%lu>\n",
		(int) ix,
		PTR2UV(ppad[ix]),
		(unsigned long) (ppad[ix] ? SvREFCNT(ppad[ix]) : 0)
	    );
	}
    }
}



/*
=for apidoc cv_dump

dump the contents of a CV

=cut
*/

#ifdef DEBUGGING
STATIC void
S_cv_dump(pTHX_ CV *cv, char *title)
{
    CV *outside = CvOUTSIDE(cv);
    AV* padlist = CvPADLIST(cv);

    PerlIO_printf(Perl_debug_log,
		  "  %s: CV=0x%"UVxf" (%s), OUTSIDE=0x%"UVxf" (%s)\n",
		  title,
		  PTR2UV(cv),
		  (CvANON(cv) ? "ANON"
		   : (cv == PL_main_cv) ? "MAIN"
		   : CvUNIQUE(cv) ? "UNIQUE"
		   : CvGV(cv) ? GvNAME(CvGV(cv)) : "UNDEFINED"),
		  PTR2UV(outside),
		  (!outside ? "null"
		   : CvANON(outside) ? "ANON"
		   : (outside == PL_main_cv) ? "MAIN"
		   : CvUNIQUE(outside) ? "UNIQUE"
		   : CvGV(outside) ? GvNAME(CvGV(outside)) : "UNDEFINED"));

    PerlIO_printf(Perl_debug_log,
		    "    PADLIST = 0x%"UVxf"\n", PTR2UV(padlist));
    do_dump_pad(1, Perl_debug_log, padlist, 1);
}
#endif /* DEBUGGING */





/*
=for apidoc cv_clone

Clone a CV: make a new CV which points to the same code etc, but which
has a newly-created pad built by copying the prototype pad and capturing
any outer lexicals.

=cut
*/

CV *
Perl_cv_clone(pTHX_ CV *proto)
{
    CV *cv;

    LOCK_CRED_MUTEX;			/* XXX create separate mutex */
    cv = cv_clone2(proto, CvOUTSIDE(proto));
    UNLOCK_CRED_MUTEX;			/* XXX create separate mutex */
    return cv;
}


/* XXX DAPM separate out cv and paddish bits ???
 * ideally the CV-related stuff shouldn't be in pad.c - how about
 * a cv.c? */

STATIC CV *
S_cv_clone2(pTHX_ CV *proto, CV *outside)
{
    I32 ix;
    AV* protopadlist = CvPADLIST(proto);
    AV* protopad_name = (AV*)*av_fetch(protopadlist, 0, FALSE);
    AV* protopad = (AV*)*av_fetch(protopadlist, 1, FALSE);
    SV** pname = AvARRAY(protopad_name);
    SV** ppad = AvARRAY(protopad);
    I32 fname = AvFILLp(protopad_name);
    I32 fpad = AvFILLp(protopad);
    AV* comppadlist;
    CV* cv;

    assert(!CvUNIQUE(proto));

    ENTER;
    SAVESPTR(PL_compcv);

    cv = PL_compcv = (CV*)NEWSV(1104, 0);
    sv_upgrade((SV *)cv, SvTYPE(proto));
    CvFLAGS(cv) = CvFLAGS(proto) & ~(CVf_CLONE|CVf_WEAKOUTSIDE);
    CvCLONED_on(cv);

#ifdef USE_5005THREADS
    New(666, CvMUTEXP(cv), 1, perl_mutex);
    MUTEX_INIT(CvMUTEXP(cv));
    CvOWNER(cv)		= 0;
#endif /* USE_5005THREADS */
#ifdef USE_ITHREADS
    CvFILE(cv)		= CvXSUB(proto) ? CvFILE(proto)
					: savepv(CvFILE(proto));
#else
    CvFILE(cv)		= CvFILE(proto);
#endif
    CvGV(cv)		= CvGV(proto);
    CvSTASH(cv)		= CvSTASH(proto);
    CvROOT(cv)		= OpREFCNT_inc(CvROOT(proto));
    CvSTART(cv)		= CvSTART(proto);
    if (outside) {
	CvOUTSIDE(cv)	= (CV*)SvREFCNT_inc(outside);
	CvOUTSIDE_SEQ(cv) = CvOUTSIDE_SEQ(proto);
    }

    if (SvPOK(proto))
	sv_setpvn((SV*)cv, SvPVX(proto), SvCUR(proto));

    CvPADLIST(cv) = comppadlist = pad_new(padnew_CLONE|padnew_SAVE);

    for (ix = fname; ix >= 0; ix--)
	av_store(PL_comppad_name, ix, SvREFCNT_inc(pname[ix]));

    av_fill(PL_comppad, fpad);
    PL_curpad = AvARRAY(PL_comppad);

    for (ix = fpad; ix > 0; ix--) {
	SV* namesv = (ix <= fname) ? pname[ix] : Nullsv;
	if (namesv && namesv != &PL_sv_undef) {
	    char *name = SvPVX(namesv);    /* XXX */
	    if (SvFLAGS(namesv) & SVf_FAKE) {   /* lexical from outside? */
		I32 off = pad_findlex(name, ix, cv);
		if (!off)
		    PL_curpad[ix] = SvREFCNT_inc(ppad[ix]);
		else if (off != ix)
		    Perl_croak(aTHX_ "panic: cv_clone: %s", name);
	    }
	    else {				/* our own lexical */
		SV* sv;
		if (*name == '&') {
		    /* anon code -- we'll come back for it */
		    sv = SvREFCNT_inc(ppad[ix]);
		}
		else if (*name == '@')
		    sv = (SV*)newAV();
		else if (*name == '%')
		    sv = (SV*)newHV();
		else
		    sv = NEWSV(0, 0);
		if (!SvPADBUSY(sv))
		    SvPADMY_on(sv);
		PL_curpad[ix] = sv;
	    }
	}
	else if (IS_PADGV(ppad[ix]) || IS_PADCONST(ppad[ix])) {
	    PL_curpad[ix] = SvREFCNT_inc(ppad[ix]);
	}
	else {
	    SV* sv = NEWSV(0, 0);
	    SvPADTMP_on(sv);
	    PL_curpad[ix] = sv;
	}
    }

    /* Now that vars are all in place, clone nested closures. */

    for (ix = fpad; ix > 0; ix--) {
	SV* namesv = (ix <= fname) ? pname[ix] : Nullsv;
	if (namesv
	    && namesv != &PL_sv_undef
	    && !(SvFLAGS(namesv) & SVf_FAKE)
	    && *SvPVX(namesv) == '&'
	    && CvCLONE(ppad[ix]))
	{
	    CV *kid = cv_clone2((CV*)ppad[ix], cv);
	    SvREFCNT_dec(ppad[ix]);
	    CvCLONE_on(kid);
	    SvPADMY_on(kid);
	    PL_curpad[ix] = (SV*)kid;
	    /* '&' entry points to child, so child mustn't refcnt parent */
	    CvWEAKOUTSIDE_on(kid);
	    SvREFCNT_dec(cv);
	}
    }

    DEBUG_Xv(
	PerlIO_printf(Perl_debug_log, "\nPad CV clone\n");
	cv_dump(outside, "Outside");
	cv_dump(proto,	 "Proto");
	cv_dump(cv,	 "To");
    );

    LEAVE;

    if (CvCONST(cv)) {
	SV* const_sv = op_const_sv(CvSTART(cv), cv);
	assert(const_sv);
	/* constant sub () { $x } closing over $x - see lib/constant.pm */
	SvREFCNT_dec(cv);
	cv = newCONSTSUB(CvSTASH(proto), 0, const_sv);
    }

    return cv;
}


/*
=for apidoc pad_fixup_inner_anons

For any anon CVs in the pad, change CvOUTSIDE of that CV from
old_cv to new_cv if necessary. Needed when a newly-compiled CV has to be
moved to a pre-existing CV struct.

=cut
*/

void
Perl_pad_fixup_inner_anons(pTHX_ PADLIST *padlist, CV *old_cv, CV *new_cv)
{
    I32 ix;
    AV *comppad_name = (AV*)AvARRAY(padlist)[0];
    AV *comppad = (AV*)AvARRAY(padlist)[1];
    SV **namepad = AvARRAY(comppad_name);
    SV **curpad = AvARRAY(comppad);
    for (ix = AvFILLp(comppad_name); ix > 0; ix--) {
	SV *namesv = namepad[ix];
	if (namesv && namesv != &PL_sv_undef
	    && *SvPVX(namesv) == '&')
	{
	    CV *innercv = (CV*)curpad[ix];
	    assert(CvWEAKOUTSIDE(innercv));
	    assert(CvOUTSIDE(innercv) == old_cv);
	    CvOUTSIDE(innercv) = new_cv;
	}
    }
}


/*
=for apidoc pad_push

Push a new pad frame onto the padlist, unless there's already a pad at
this depth, in which case don't bother creating a new one.
If has_args is true, give the new pad an @_ in slot zero.

=cut
*/

void
Perl_pad_push(pTHX_ PADLIST *padlist, int depth, int has_args)
{
    if (depth <= AvFILLp(padlist))
	return;

    {
	SV** svp = AvARRAY(padlist);
	AV *newpad = newAV();
	SV **oldpad = AvARRAY(svp[depth-1]);
	I32 ix = AvFILLp((AV*)svp[1]);
	I32 names_fill = AvFILLp((AV*)svp[0]);
	SV** names = AvARRAY(svp[0]);
	SV* sv;
	for ( ;ix > 0; ix--) {
	    if (names_fill >= ix && names[ix] != &PL_sv_undef) {
		char *name = SvPVX(names[ix]);
		if ((SvFLAGS(names[ix]) & SVf_FAKE) || *name == '&') {
		    /* outer lexical or anon code */
		    av_store(newpad, ix, SvREFCNT_inc(oldpad[ix]));
		}
		else {		/* our own lexical */
		    if (*name == '@')
			av_store(newpad, ix, sv = (SV*)newAV());
		    else if (*name == '%')
			av_store(newpad, ix, sv = (SV*)newHV());
		    else
			av_store(newpad, ix, sv = NEWSV(0, 0));
		    SvPADMY_on(sv);
		}
	    }
	    else if (IS_PADGV(oldpad[ix]) || IS_PADCONST(oldpad[ix])) {
		av_store(newpad, ix, sv = SvREFCNT_inc(oldpad[ix]));
	    }
	    else {
		/* save temporaries on recursion? */
		av_store(newpad, ix, sv = NEWSV(0, 0));
		SvPADTMP_on(sv);
	    }
	}
	if (has_args) {
	    AV* av = newAV();
	    av_extend(av, 0);
	    av_store(newpad, 0, (SV*)av);
	    AvFLAGS(av) = AVf_REIFY;
	}
	av_store(padlist, depth, (SV*)newpad);
	AvFILLp(padlist) = depth;
    }
}
