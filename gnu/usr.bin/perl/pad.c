/*    pad.c
 *
 *    Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

/*
 *  'Anyway: there was this Mr. Frodo left an orphan and stranded, as you
 *   might say, among those queer Bucklanders, being brought up anyhow in
 *   Brandy Hall.  A regular warren, by all accounts.  Old Master Gorbadoc
 *   never had fewer than a couple of hundred relations in the place.
 *   Mr. Bilbo never did a kinder deed than when he brought the lad back
 *   to live among decent folk.'                           --the Gaffer
 *
 *     [p.23 of _The Lord of the Rings_, I/i: "A Long-Expected Party"]
 */

/* XXX DAPM
 * As of Sept 2002, this file is new and may be in a state of flux for
 * a while. I've marked things I intent to come back and look at further
 * with an 'XXX DAPM' comment.
 */

/*
=head1 Pad Data Structures

This file contains the functions that create and manipulate scratchpads,
which are array-of-array data structures attached to a CV (ie a sub)
and which store lexical variables and opcode temporary and per-thread
values.

=for apidoc m|AV *|CvPADLIST|CV *cv
CV's can have CvPADLIST(cv) set to point to an AV.

For these purposes "forms" are a kind-of CV, eval""s are too (except they're
not callable at will and are always thrown away after the eval"" is done
executing). Require'd files are simply evals without any outer lexical
scope.

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
xlow+1..xhigh inclusive in the NV union is a range of cop_seq numbers for
which the name is valid.  For typed lexicals name SV is SVt_PVMG and SvSTASH
points at the type.  For C<our> lexicals, the type is also SVt_PVMG, with the
SvOURSTASH slot pointing at the stash of the associated global (so that
duplicate C<our> declarations in the same package can be detected).  SvUVX is
sometimes hijacked to store the generation number during compilation.

If SvFAKE is set on the name SV, then that slot in the frame AV is
a REFCNT'ed reference to a lexical from "outside". In this case,
the name SV does not use xlow and xhigh to store a cop_seq range, since it is
in scope throughout. Instead xhigh stores some flags containing info about
the real lexical (is it declared in an anon, and is it capable of being
instantiated multiple times?), and for fake ANONs, xlow contains the index
within the parent's pad where the lexical's value is stored, to make
cloning quicker.

If the 'name' is '&' the corresponding entry in frame AV
is a CV representing a possible closure.
(SvFAKE and name of '&' is not a meaningful combination currently but could
become so if C<my sub foo {}> is implemented.)

Note that formats are treated as anon subs, and are cloned each time
write is called (if necessary).

The flag SVs_PADSTALE is cleared on lexicals each time the my() is executed,
and set on scope exit. This allows the 'Variable $x is not available' warning
to be generated in evals, such as 

    { my $x = 1; sub f { eval '$x'} } f();

For state vars, SVs_PADSTALE is overloaded to mean 'not yet initialised'

=cut
*/


#include "EXTERN.h"
#define PERL_IN_PAD_C
#include "perl.h"
#include "keywords.h"

#define COP_SEQ_RANGE_LOW_set(sv,val)		\
  STMT_START { ((XPVNV*)SvANY(sv))->xnv_u.xpad_cop_seq.xlow = (val); } STMT_END
#define COP_SEQ_RANGE_HIGH_set(sv,val)		\
  STMT_START { ((XPVNV*)SvANY(sv))->xnv_u.xpad_cop_seq.xhigh = (val); } STMT_END

#define PARENT_PAD_INDEX_set(sv,val)		\
  STMT_START { ((XPVNV*)SvANY(sv))->xnv_u.xpad_cop_seq.xlow = (val); } STMT_END
#define PARENT_FAKELEX_FLAGS_set(sv,val)	\
  STMT_START { ((XPVNV*)SvANY(sv))->xnv_u.xpad_cop_seq.xhigh = (val); } STMT_END

#define PAD_MAX I32_MAX

#ifdef PERL_MAD
void pad_peg(const char* s) {
    static int pegcnt;

    PERL_ARGS_ASSERT_PAD_PEG;

    pegcnt++;
}
#endif

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
    dVAR;
    AV *padlist, *padname, *pad;

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
	    SAVEBOOL(PL_cv_has_eval);
	    if (flags & padnew_SAVESUB) {
		SAVEBOOL(PL_pad_reset_pending);
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

        AV * const a0 = newAV();			/* will be @_ */
	av_extend(a0, 0);
	av_store(pad, 0, MUTABLE_SV(a0));
	AvREIFY_only(a0);
    }
    else {
	av_store(pad, 0, NULL);
    }

    AvREAL_off(padlist);
    av_store(padlist, 0, MUTABLE_SV(padname));
    av_store(padlist, 1, MUTABLE_SV(pad));

    /* ... then update state variables */

    PL_comppad_name	= MUTABLE_AV((*av_fetch(padlist, 0, FALSE)));
    PL_comppad		= MUTABLE_AV((*av_fetch(padlist, 1, FALSE)));
    PL_curpad		= AvARRAY(PL_comppad);

    if (! (flags & padnew_CLONE)) {
	PL_comppad_name_fill = 0;
	PL_min_intro_pending = 0;
	PL_padix	     = 0;
	PL_cv_has_eval	     = 0;
    }

    DEBUG_X(PerlIO_printf(Perl_debug_log,
	  "Pad 0x%"UVxf"[0x%"UVxf"] new:       compcv=0x%"UVxf
	      " name=0x%"UVxf" flags=0x%"UVxf"\n",
	  PTR2UV(PL_comppad), PTR2UV(PL_curpad), PTR2UV(PL_compcv),
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
    dVAR;
    I32 ix;
    const PADLIST * const padlist = CvPADLIST(cv);

    PERL_ARGS_ASSERT_PAD_UNDEF;

    pad_peg("pad_undef");
    if (!padlist)
	return;
    if (SvIS_FREED(padlist)) /* may be during global destruction */
	return;

    DEBUG_X(PerlIO_printf(Perl_debug_log,
	  "Pad undef: cv=0x%"UVxf" padlist=0x%"UVxf" comppad=0x%"UVxf"\n",
	    PTR2UV(cv), PTR2UV(padlist), PTR2UV(PL_comppad))
    );

    /* detach any '&' anon children in the pad; if afterwards they
     * are still live, fix up their CvOUTSIDEs to point to our outside,
     * bypassing us. */
    /* XXX DAPM for efficiency, we should only do this if we know we have
     * children, or integrate this loop with general cleanup */

    if (!PL_dirty) { /* don't bother during global destruction */
	CV * const outercv = CvOUTSIDE(cv);
        const U32 seq = CvOUTSIDE_SEQ(cv);
	AV *  const comppad_name = MUTABLE_AV(AvARRAY(padlist)[0]);
	SV ** const namepad = AvARRAY(comppad_name);
	AV *  const comppad = MUTABLE_AV(AvARRAY(padlist)[1]);
	SV ** const curpad = AvARRAY(comppad);
	for (ix = AvFILLp(comppad_name); ix > 0; ix--) {
	    SV * const namesv = namepad[ix];
	    if (namesv && namesv != &PL_sv_undef
		&& *SvPVX_const(namesv) == '&')
	    {
		CV * const innercv = MUTABLE_CV(curpad[ix]);
		U32 inner_rc = SvREFCNT(innercv);
		assert(inner_rc);
		namepad[ix] = NULL;
		SvREFCNT_dec(namesv);

		if (SvREFCNT(comppad) < 2) { /* allow for /(?{ sub{} })/  */
		    curpad[ix] = NULL;
		    SvREFCNT_dec(innercv);
		    inner_rc--;
		}

		/* in use, not just a prototype */
		if (inner_rc && (CvOUTSIDE(innercv) == cv)) {
		    assert(CvWEAKOUTSIDE(innercv));
		    /* don't relink to grandfather if he's being freed */
		    if (outercv && SvREFCNT(outercv)) {
			CvWEAKOUTSIDE_off(innercv);
			CvOUTSIDE(innercv) = outercv;
			CvOUTSIDE_SEQ(innercv) = seq;
			SvREFCNT_inc_simple_void_NN(outercv);
		    }
		    else {
			CvOUTSIDE(innercv) = NULL;
		    }
		}
	    }
	}
    }

    ix = AvFILLp(padlist);
    while (ix >= 0) {
	SV* const sv = AvARRAY(padlist)[ix--];
	if (sv) {
	    if (sv == (const SV *)PL_comppad_name)
		PL_comppad_name = NULL;
	    else if (sv == (const SV *)PL_comppad) {
		PL_comppad = NULL;
		PL_curpad = NULL;
	    }
	}
	SvREFCNT_dec(sv);
    }
    SvREFCNT_dec(MUTABLE_SV(CvPADLIST(cv)));
    CvPADLIST(cv) = NULL;
}




static PADOFFSET
S_pad_add_name_sv(pTHX_ SV *namesv, const U32 flags, HV *typestash,
		  HV *ourstash)
{
    dVAR;
    const PADOFFSET offset = pad_alloc(OP_PADSV, SVs_PADMY);

    PERL_ARGS_ASSERT_PAD_ADD_NAME_SV;

    ASSERT_CURPAD_ACTIVE("pad_add_name");

    if (typestash) {
	assert(SvTYPE(namesv) == SVt_PVMG);
	SvPAD_TYPED_on(namesv);
	SvSTASH_set(namesv, MUTABLE_HV(SvREFCNT_inc_simple_NN(MUTABLE_SV(typestash))));
    }
    if (ourstash) {
	SvPAD_OUR_on(namesv);
	SvOURSTASH_set(namesv, ourstash);
	SvREFCNT_inc_simple_void_NN(ourstash);
    }
    else if (flags & padadd_STATE) {
	SvPAD_STATE_on(namesv);
    }

    av_store(PL_comppad_name, offset, namesv);
    return offset;
}

/*
=for apidoc pad_add_name

Create a new name and associated PADMY SV in the current pad; return the
offset.
If C<typestash> is valid, the name is for a typed lexical; set the
name's stash to that value.
If C<ourstash> is valid, it's an our lexical, set the name's
SvOURSTASH to that value

If fake, it means we're cloning an existing entry

=cut
*/

PADOFFSET
Perl_pad_add_name(pTHX_ const char *name, const STRLEN len, const U32 flags,
		  HV *typestash, HV *ourstash)
{
    dVAR;
    PADOFFSET offset;
    SV *namesv;

    PERL_ARGS_ASSERT_PAD_ADD_NAME;

    if (flags & ~(padadd_OUR|padadd_STATE|padadd_NO_DUP_CHECK))
	Perl_croak(aTHX_ "panic: pad_add_name illegal flag bits 0x%" UVxf,
		   (UV)flags);

    namesv = newSV_type((ourstash || typestash) ? SVt_PVMG : SVt_PVNV);

    /* Until we're using the length for real, cross check that we're being told
       the truth.  */
    PERL_UNUSED_ARG(len);
    assert(strlen(name) == len);

    sv_setpv(namesv, name);

    if ((flags & padadd_NO_DUP_CHECK) == 0) {
	/* check for duplicate declaration */
	pad_check_dup(namesv, flags & padadd_OUR, ourstash);
    }

    offset = pad_add_name_sv(namesv, flags, typestash, ourstash);

    /* not yet introduced */
    COP_SEQ_RANGE_LOW_set(namesv, PAD_MAX);	/* min */
    COP_SEQ_RANGE_HIGH_set(namesv, 0);		/* max */

    if (!PL_min_intro_pending)
	PL_min_intro_pending = offset;
    PL_max_intro_pending = offset;
    /* if it's not a simple scalar, replace with an AV or HV */
    /* XXX DAPM since slot has been allocated, replace
     * av_store with PL_curpad[offset] ? */
    if (*name == '@')
	av_store(PL_comppad, offset, MUTABLE_SV(newAV()));
    else if (*name == '%')
	av_store(PL_comppad, offset, MUTABLE_SV(newHV()));
    SvPADMY_on(PL_curpad[offset]);
    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
			   "Pad addname: %ld \"%s\" new lex=0x%"UVxf"\n",
			   (long)offset, name, PTR2UV(PL_curpad[offset])));

    return offset;
}




/*
=for apidoc pad_alloc

Allocate a new my or tmp pad entry. For a my, simply push a null SV onto
the end of PL_comppad, but for a tmp, scan the pad from PL_padix upwards
for a slot which has no name and no active value.

=cut
*/

/* XXX DAPM integrate alloc(), add_name() and add_anon(),
 * or at least rationalise ??? */
/* And flag whether the incoming name is UTF8 or 8 bit?
   Could do this either with the +ve/-ve hack of the HV code, or expanding
   the flag bits. Either way, this makes proper Unicode safe pad support.
   NWC
*/

PADOFFSET
Perl_pad_alloc(pTHX_ I32 optype, U32 tmptype)
{
    dVAR;
    SV *sv;
    I32 retval;

    PERL_UNUSED_ARG(optype);
    ASSERT_CURPAD_ACTIVE("pad_alloc");

    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_alloc");
    if (PL_pad_reset_pending)
	pad_reset();
    if (tmptype & SVs_PADMY) {
	sv = *av_fetch(PL_comppad, AvFILLp(PL_comppad) + 1, TRUE);
	retval = AvFILLp(PL_comppad);
    }
    else {
	SV * const * const names = AvARRAY(PL_comppad_name);
        const SSize_t names_fill = AvFILLp(PL_comppad_name);
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
#ifdef DEBUG_LEAKING_SCALARS
    sv->sv_debug_optype = optype;
    sv->sv_debug_inpad = 1;
#endif
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
    dVAR;
    PADOFFSET ix;
    SV* const name = newSV_type(SVt_PVNV);

    PERL_ARGS_ASSERT_PAD_ADD_ANON;

    pad_peg("add_anon");
    sv_setpvs(name, "&");
    /* Are these two actually ever read? */
    COP_SEQ_RANGE_HIGH_set(name, ~0);
    COP_SEQ_RANGE_LOW_set(name, 1);
    ix = pad_alloc(op_type, SVs_PADMY);
    av_store(PL_comppad_name, ix, name);
    /* XXX DAPM use PL_curpad[] ? */
    av_store(PL_comppad, ix, sv);
    SvPADMY_on(sv);

    /* to avoid ref loops, we never have parent + child referencing each
     * other simultaneously */
    if (CvOUTSIDE((const CV *)sv)) {
	assert(!CvWEAKOUTSIDE((const CV *)sv));
	CvWEAKOUTSIDE_on(MUTABLE_CV(sv));
	SvREFCNT_dec(CvOUTSIDE(MUTABLE_CV(sv)));
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

STATIC void
S_pad_check_dup(pTHX_ SV *name, const U32 flags, const HV *ourstash)
{
    dVAR;
    SV		**svp;
    PADOFFSET	top, off;
    const U32	is_our = flags & padadd_OUR;

    PERL_ARGS_ASSERT_PAD_CHECK_DUP;

    ASSERT_CURPAD_ACTIVE("pad_check_dup");

    assert((flags & ~padadd_OUR) == 0);

    if (AvFILLp(PL_comppad_name) < 0 || !ckWARN(WARN_MISC))
	return; /* nothing to check */

    svp = AvARRAY(PL_comppad_name);
    top = AvFILLp(PL_comppad_name);
    /* check the current scope */
    /* XXX DAPM - why the (I32) cast - shouldn't we ensure they're the same
     * type ? */
    for (off = top; (I32)off > PL_comppad_name_floor; off--) {
	SV * const sv = svp[off];
	if (sv
	    && sv != &PL_sv_undef
	    && !SvFAKE(sv)
	    && (COP_SEQ_RANGE_HIGH(sv) == PAD_MAX || COP_SEQ_RANGE_HIGH(sv) == 0)
	    && sv_eq(name, sv))
	{
	    if (is_our && (SvPAD_OUR(sv)))
		break; /* "our" masking "our" */
	    Perl_warner(aTHX_ packWARN(WARN_MISC),
		"\"%s\" variable %"SVf" masks earlier declaration in same %s",
		(is_our ? "our" : PL_parser->in_my == KEY_my ? "my" : "state"),
		sv,
		(COP_SEQ_RANGE_HIGH(sv) == PAD_MAX ? "scope" : "statement"));
	    --off;
	    break;
	}
    }
    /* check the rest of the pad */
    if (is_our) {
	do {
	    SV * const sv = svp[off];
	    if (sv
		&& sv != &PL_sv_undef
		&& !SvFAKE(sv)
		&& (COP_SEQ_RANGE_HIGH(sv) == PAD_MAX || COP_SEQ_RANGE_HIGH(sv) == 0)
		&& SvOURSTASH(sv) == ourstash
		&& sv_eq(name, sv))
	    {
		Perl_warner(aTHX_ packWARN(WARN_MISC),
		    "\"our\" variable %"SVf" redeclared", sv);
		if ((I32)off <= PL_comppad_name_floor)
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
Perl_pad_findmy(pTHX_ const char *name, STRLEN len, U32 flags)
{
    dVAR;
    SV *out_sv;
    int out_flags;
    I32 offset;
    const AV *nameav;
    SV **name_svp;

    PERL_ARGS_ASSERT_PAD_FINDMY;

    pad_peg("pad_findmy");

    if (flags)
	Perl_croak(aTHX_ "panic: pad_findmy illegal flag bits 0x%" UVxf,
		   (UV)flags);

    /* Yes, it is a bug (read work in progress) that we're not really using this
       length parameter, and instead relying on strlen() later on. But I'm not
       comfortable about changing the pad API piecemeal to use and rely on
       lengths. This only exists to avoid an "unused parameter" warning.  */
    if (len < 2) 
	return NOT_IN_PAD;

    /* But until we're using the length for real, cross check that we're being
       told the truth.  */
    assert(strlen(name) == len);

    offset = pad_findlex(name, PL_compcv, PL_cop_seqmax, 1,
		NULL, &out_sv, &out_flags);
    if ((PADOFFSET)offset != NOT_IN_PAD) 
	return offset;

    /* look for an our that's being introduced; this allows
     *    our $foo = 0 unless defined $foo;
     * to not give a warning. (Yes, this is a hack) */

    nameav = MUTABLE_AV(AvARRAY(CvPADLIST(PL_compcv))[0]);
    name_svp = AvARRAY(nameav);
    for (offset = AvFILLp(nameav); offset > 0; offset--) {
        const SV * const namesv = name_svp[offset];
	if (namesv && namesv != &PL_sv_undef
	    && !SvFAKE(namesv)
	    && (SvPAD_OUR(namesv))
	    && strEQ(SvPVX_const(namesv), name)
	    && COP_SEQ_RANGE_LOW(namesv) == PAD_MAX /* min */
	)
	    return offset;
    }
    return NOT_IN_PAD;
}

/*
 * Returns the offset of a lexical $_, if there is one, at run time.
 * Used by the UNDERBAR XS macro.
 */

PADOFFSET
Perl_find_rundefsvoffset(pTHX)
{
    dVAR;
    SV *out_sv;
    int out_flags;
    return pad_findlex("$_", find_runcv(NULL), PL_curcop->cop_seq, 1,
	    NULL, &out_sv, &out_flags);
}

/*
=for apidoc pad_findlex

Find a named lexical anywhere in a chain of nested pads. Add fake entries
in the inner pads if it's found in an outer one.

Returns the offset in the bottom pad of the lex or the fake lex.
cv is the CV in which to start the search, and seq is the current cop_seq
to match against. If warn is true, print appropriate warnings.  The out_*
vars return values, and so are pointers to where the returned values
should be stored. out_capture, if non-null, requests that the innermost
instance of the lexical is captured; out_name_sv is set to the innermost
matched namesv or fake namesv; out_flags returns the flags normally
associated with the IVX field of a fake namesv.

Note that pad_findlex() is recursive; it recurses up the chain of CVs,
then comes back down, adding fake entries as it goes. It has to be this way
because fake namesvs in anon protoypes have to store in xlow the index into
the parent pad.

=cut
*/

/* the CV has finished being compiled. This is not a sufficient test for
 * all CVs (eg XSUBs), but suffices for the CVs found in a lexical chain */
#define CvCOMPILED(cv)	CvROOT(cv)

/* the CV does late binding of its lexicals */
#define CvLATE(cv) (CvANON(cv) || SvTYPE(cv) == SVt_PVFM)


STATIC PADOFFSET
S_pad_findlex(pTHX_ const char *name, const CV* cv, U32 seq, int warn,
	SV** out_capture, SV** out_name_sv, int *out_flags)
{
    dVAR;
    I32 offset, new_offset;
    SV *new_capture;
    SV **new_capturep;
    const AV * const padlist = CvPADLIST(cv);

    PERL_ARGS_ASSERT_PAD_FINDLEX;

    *out_flags = 0;

    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
	"Pad findlex cv=0x%"UVxf" searching \"%s\" seq=%d%s\n",
	PTR2UV(cv), name, (int)seq, out_capture ? " capturing" : "" ));

    /* first, search this pad */

    if (padlist) { /* not an undef CV */
	I32 fake_offset = 0;
        const AV * const nameav = MUTABLE_AV(AvARRAY(padlist)[0]);
	SV * const * const name_svp = AvARRAY(nameav);

	for (offset = AvFILLp(nameav); offset > 0; offset--) {
            const SV * const namesv = name_svp[offset];
	    if (namesv && namesv != &PL_sv_undef
		    && strEQ(SvPVX_const(namesv), name))
	    {
		if (SvFAKE(namesv))
		    fake_offset = offset; /* in case we don't find a real one */
		else if (  seq >  COP_SEQ_RANGE_LOW(namesv)	/* min */
			&& seq <= COP_SEQ_RANGE_HIGH(namesv))	/* max */
		    break;
	    }
	}

	if (offset > 0 || fake_offset > 0 ) { /* a match! */
	    if (offset > 0) { /* not fake */
		fake_offset = 0;
		*out_name_sv = name_svp[offset]; /* return the namesv */

		/* set PAD_FAKELEX_MULTI if this lex can have multiple
		 * instances. For now, we just test !CvUNIQUE(cv), but
		 * ideally, we should detect my's declared within loops
		 * etc - this would allow a wider range of 'not stayed
		 * shared' warnings. We also treated alreadly-compiled
		 * lexes as not multi as viewed from evals. */

		*out_flags = CvANON(cv) ?
			PAD_FAKELEX_ANON :
			    (!CvUNIQUE(cv) && ! CvCOMPILED(cv))
				? PAD_FAKELEX_MULTI : 0;

		DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		    "Pad findlex cv=0x%"UVxf" matched: offset=%ld (%lu,%lu)\n",
		    PTR2UV(cv), (long)offset,
		    (unsigned long)COP_SEQ_RANGE_LOW(*out_name_sv),
		    (unsigned long)COP_SEQ_RANGE_HIGH(*out_name_sv)));
	    }
	    else { /* fake match */
		offset = fake_offset;
		*out_name_sv = name_svp[offset]; /* return the namesv */
		*out_flags = PARENT_FAKELEX_FLAGS(*out_name_sv);
		DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		    "Pad findlex cv=0x%"UVxf" matched: offset=%ld flags=0x%lx index=%lu\n",
		    PTR2UV(cv), (long)offset, (unsigned long)*out_flags,
		    (unsigned long) PARENT_PAD_INDEX(*out_name_sv) 
		));
	    }

	    /* return the lex? */

	    if (out_capture) {

		/* our ? */
		if (SvPAD_OUR(*out_name_sv)) {
		    *out_capture = NULL;
		    return offset;
		}

		/* trying to capture from an anon prototype? */
		if (CvCOMPILED(cv)
			? CvANON(cv) && CvCLONE(cv) && !CvCLONED(cv)
			: *out_flags & PAD_FAKELEX_ANON)
		{
		    if (warn)
			Perl_ck_warner(aTHX_ packWARN(WARN_CLOSURE),
				       "Variable \"%s\" is not available", name);
		    *out_capture = NULL;
		}

		/* real value */
		else {
		    int newwarn = warn;
		    if (!CvCOMPILED(cv) && (*out_flags & PAD_FAKELEX_MULTI)
			 && !SvPAD_STATE(name_svp[offset])
			 && warn && ckWARN(WARN_CLOSURE)) {
			newwarn = 0;
			Perl_warner(aTHX_ packWARN(WARN_CLOSURE),
			    "Variable \"%s\" will not stay shared", name);
		    }

		    if (fake_offset && CvANON(cv)
			    && CvCLONE(cv) &&!CvCLONED(cv))
		    {
			SV *n;
			/* not yet caught - look further up */
			DEBUG_Xv(PerlIO_printf(Perl_debug_log,
			    "Pad findlex cv=0x%"UVxf" chasing lex in outer pad\n",
			    PTR2UV(cv)));
			n = *out_name_sv;
			(void) pad_findlex(name, CvOUTSIDE(cv),
			    CvOUTSIDE_SEQ(cv),
			    newwarn, out_capture, out_name_sv, out_flags);
			*out_name_sv = n;
			return offset;
		    }

		    *out_capture = AvARRAY(MUTABLE_AV(AvARRAY(padlist)[
				    CvDEPTH(cv) ? CvDEPTH(cv) : 1]))[offset];
		    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
			"Pad findlex cv=0x%"UVxf" found lex=0x%"UVxf"\n",
			PTR2UV(cv), PTR2UV(*out_capture)));

		    if (SvPADSTALE(*out_capture)
			&& !SvPAD_STATE(name_svp[offset]))
		    {
			Perl_ck_warner(aTHX_ packWARN(WARN_CLOSURE),
				       "Variable \"%s\" is not available", name);
			*out_capture = NULL;
		    }
		}
		if (!*out_capture) {
		    if (*name == '@')
			*out_capture = sv_2mortal(MUTABLE_SV(newAV()));
		    else if (*name == '%')
			*out_capture = sv_2mortal(MUTABLE_SV(newHV()));
		    else
			*out_capture = sv_newmortal();
		}
	    }

	    return offset;
	}
    }

    /* it's not in this pad - try above */

    if (!CvOUTSIDE(cv))
	return NOT_IN_PAD;

    /* out_capture non-null means caller wants us to capture lex; in
     * addition we capture ourselves unless it's an ANON/format */
    new_capturep = out_capture ? out_capture :
		CvLATE(cv) ? NULL : &new_capture;

    offset = pad_findlex(name, CvOUTSIDE(cv), CvOUTSIDE_SEQ(cv), 1,
		new_capturep, out_name_sv, out_flags);
    if ((PADOFFSET)offset == NOT_IN_PAD)
	return NOT_IN_PAD;

    /* found in an outer CV. Add appropriate fake entry to this pad */

    /* don't add new fake entries (via eval) to CVs that we have already
     * finished compiling, or to undef CVs */
    if (CvCOMPILED(cv) || !padlist)
	return 0; /* this dummy (and invalid) value isnt used by the caller */

    {
	/* This relies on sv_setsv_flags() upgrading the destination to the same
	   type as the source, independant of the flags set, and on it being
	   "good" and only copying flag bits and pointers that it understands.
	*/
	SV *new_namesv = newSVsv(*out_name_sv);
	AV *  const ocomppad_name = PL_comppad_name;
	PAD * const ocomppad = PL_comppad;
	PL_comppad_name = MUTABLE_AV(AvARRAY(padlist)[0]);
	PL_comppad = MUTABLE_AV(AvARRAY(padlist)[1]);
	PL_curpad = AvARRAY(PL_comppad);

	new_offset
	    = pad_add_name_sv(new_namesv,
			      (SvPAD_STATE(*out_name_sv) ? padadd_STATE : 0),
			      SvPAD_TYPED(*out_name_sv)
			      ? SvSTASH(*out_name_sv) : NULL,
			      SvOURSTASH(*out_name_sv)
			      );

	SvFAKE_on(new_namesv);
	DEBUG_Xv(PerlIO_printf(Perl_debug_log,
			       "Pad addname: %ld \"%.*s\" FAKE\n",
			       (long)new_offset,
			       (int) SvCUR(new_namesv), SvPVX(new_namesv)));
	PARENT_FAKELEX_FLAGS_set(new_namesv, *out_flags);

	PARENT_PAD_INDEX_set(new_namesv, 0);
	if (SvPAD_OUR(new_namesv)) {
	    NOOP;   /* do nothing */
	}
	else if (CvLATE(cv)) {
	    /* delayed creation - just note the offset within parent pad */
	    PARENT_PAD_INDEX_set(new_namesv, offset);
	    CvCLONE_on(cv);
	}
	else {
	    /* immediate creation - capture outer value right now */
	    av_store(PL_comppad, new_offset, SvREFCNT_inc(*new_capturep));
	    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		"Pad findlex cv=0x%"UVxf" saved captured sv 0x%"UVxf" at offset %ld\n",
		PTR2UV(cv), PTR2UV(*new_capturep), (long)new_offset));
	}
	*out_name_sv = new_namesv;
	*out_flags = PARENT_FAKELEX_FLAGS(new_namesv);

	PL_comppad_name = ocomppad_name;
	PL_comppad = ocomppad;
	PL_curpad = ocomppad ? AvARRAY(ocomppad) : NULL;
    }
    return new_offset;
}


#ifdef DEBUGGING
/*
=for apidoc pad_sv

Get the value at offset po in the current pad.
Use macro PAD_SV instead of calling this function directly.

=cut
*/


SV *
Perl_pad_sv(pTHX_ PADOFFSET po)
{
    dVAR;
    ASSERT_CURPAD_ACTIVE("pad_sv");

    if (!po)
	Perl_croak(aTHX_ "panic: pad_sv po");
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

void
Perl_pad_setsv(pTHX_ PADOFFSET po, SV* sv)
{
    dVAR;

    PERL_ARGS_ASSERT_PAD_SETSV;

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
    dVAR;
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
    dVAR;
    SV **svp;
    I32 i;

    ASSERT_CURPAD_ACTIVE("intro_my");
    if (! PL_min_intro_pending)
	return PL_cop_seqmax;

    svp = AvARRAY(PL_comppad_name);
    for (i = PL_min_intro_pending; i <= PL_max_intro_pending; i++) {
	SV * const sv = svp[i];

	if (sv && sv != &PL_sv_undef && !SvFAKE(sv) && !COP_SEQ_RANGE_HIGH(sv)) {
	    COP_SEQ_RANGE_HIGH_set(sv, PAD_MAX);	/* Don't know scope end yet. */
	    COP_SEQ_RANGE_LOW_set(sv, PL_cop_seqmax);
	    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		"Pad intromy: %ld \"%s\", (%lu,%lu)\n",
		(long)i, SvPVX_const(sv),
		(unsigned long)COP_SEQ_RANGE_LOW(sv),
		(unsigned long)COP_SEQ_RANGE_HIGH(sv))
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
    dVAR;
    I32 off;
    SV * const * const svp = AvARRAY(PL_comppad_name);

    PL_pad_reset_pending = FALSE;

    ASSERT_CURPAD_ACTIVE("pad_leavemy");
    if (PL_min_intro_pending && PL_comppad_name_fill < PL_min_intro_pending) {
	for (off = PL_max_intro_pending; off >= PL_min_intro_pending; off--) {
	    const SV * const sv = svp[off];
	    if (sv && sv != &PL_sv_undef && !SvFAKE(sv))
		Perl_ck_warner_d(aTHX_ packWARN(WARN_INTERNAL),
				 "%"SVf" never introduced",
				 SVfARG(sv));
	}
    }
    /* "Deintroduce" my variables that are leaving with this scope. */
    for (off = AvFILLp(PL_comppad_name); off > PL_comppad_name_fill; off--) {
	const SV * const sv = svp[off];
	if (sv && sv != &PL_sv_undef && !SvFAKE(sv) && COP_SEQ_RANGE_HIGH(sv) == PAD_MAX) {
	    COP_SEQ_RANGE_HIGH_set(sv, PL_cop_seqmax);
	    DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		"Pad leavemy: %ld \"%s\", (%lu,%lu)\n",
		(long)off, SvPVX_const(sv),
		(unsigned long)COP_SEQ_RANGE_LOW(sv),
		(unsigned long)COP_SEQ_RANGE_HIGH(sv))
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
    dVAR;
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


    /* if pad tmps aren't shared between ops, then there's no need to
     * create a new tmp when an existing op is freed */
#ifdef USE_BROKEN_PAD_RESET
    PL_curpad[po] = newSV(0);
    SvPADTMP_on(PL_curpad[po]);
#else
    PL_curpad[po] = &PL_sv_undef;
#endif
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
static void
S_pad_reset(pTHX)
{
    dVAR;
#ifdef USE_BROKEN_PAD_RESET
    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_reset curpad");

    DEBUG_X(PerlIO_printf(Perl_debug_log,
	    "Pad 0x%"UVxf"[0x%"UVxf"] reset:     padix %ld -> %ld",
	    PTR2UV(PL_comppad), PTR2UV(PL_curpad),
		(long)PL_padix, (long)PL_padix_floor
	    )
    );

    if (!PL_tainting) {	/* Can't mix tainted and non-tainted temporaries. */
        register I32 po;
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
    dVAR;

    ASSERT_CURPAD_ACTIVE("pad_tidy");

    /* If this CV has had any 'eval-capable' ops planted in it
     * (ie it contains eval '...', //ee, /$var/ or /(?{..})/), Then any
     * anon prototypes in the chain of CVs should be marked as cloneable,
     * so that for example the eval's CV in C<< sub { eval '$x' } >> gets
     * the right CvOUTSIDE.
     * If running with -d, *any* sub may potentially have an eval
     * excuted within it.
     */

    if (PL_cv_has_eval || PL_perldb) {
        const CV *cv;
	for (cv = PL_compcv ;cv; cv = CvOUTSIDE(cv)) {
	    if (cv != PL_compcv && CvCOMPILED(cv))
		break; /* no need to mark already-compiled code */
	    if (CvANON(cv)) {
		DEBUG_Xv(PerlIO_printf(Perl_debug_log,
		    "Pad clone on cv=0x%"UVxf"\n", PTR2UV(cv)));
		CvCLONE_on(cv);
	    }
	}
    }

    /* extend curpad to match namepad */
    if (AvFILLp(PL_comppad_name) < AvFILLp(PL_comppad))
	av_store(PL_comppad_name, AvFILLp(PL_comppad), NULL);

    if (type == padtidy_SUBCLONE) {
	SV * const * const namep = AvARRAY(PL_comppad_name);
	PADOFFSET ix;

	for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	    SV *namesv;

	    if (SvIMMORTAL(PL_curpad[ix]) || IS_PADGV(PL_curpad[ix]) || IS_PADCONST(PL_curpad[ix]))
		continue;
	    /*
	     * The only things that a clonable function needs in its
	     * pad are anonymous subs.
	     * The rest are created anew during cloning.
	     */
	    if (!((namesv = namep[ix]) != NULL &&
		  namesv != &PL_sv_undef &&
		   *SvPVX_const(namesv) == '&'))
	    {
		SvREFCNT_dec(PL_curpad[ix]);
		PL_curpad[ix] = NULL;
	    }
	}
    }
    else if (type == padtidy_SUB) {
	/* XXX DAPM this same bit of code keeps appearing !!! Rationalise? */
	AV * const av = newAV();			/* Will be @_ */
	av_extend(av, 0);
	av_store(PL_comppad, 0, MUTABLE_SV(av));
	AvREIFY_only(av);
    }

    /* XXX DAPM rationalise these two similar branches */

    if (type == padtidy_SUB) {
	PADOFFSET ix;
	for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	    if (SvIMMORTAL(PL_curpad[ix]) || IS_PADGV(PL_curpad[ix]) || IS_PADCONST(PL_curpad[ix]))
		continue;
	    if (!SvPADMY(PL_curpad[ix]))
		SvPADTMP_on(PL_curpad[ix]);
	}
    }
    else if (type == padtidy_FORMAT) {
	PADOFFSET ix;
	for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	    if (!SvPADMY(PL_curpad[ix]) && !SvIMMORTAL(PL_curpad[ix]))
		SvPADTMP_on(PL_curpad[ix]);
	}
    }
    PL_curpad = AvARRAY(PL_comppad);
}


/*
=for apidoc pad_free

Free the SV at offset po in the current pad.

=cut
*/

/* XXX DAPM integrate with pad_swipe ???? */
void
Perl_pad_free(pTHX_ PADOFFSET po)
{
    dVAR;
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
	if (!SvIsCOW(PL_curpad[po]))
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
    dVAR;
    const AV *pad_name;
    const AV *pad;
    SV **pname;
    SV **ppad;
    I32 ix;

    PERL_ARGS_ASSERT_DO_DUMP_PAD;

    if (!padlist) {
	return;
    }
    pad_name = MUTABLE_AV(*av_fetch(MUTABLE_AV(padlist), 0, FALSE));
    pad = MUTABLE_AV(*av_fetch(MUTABLE_AV(padlist), 1, FALSE));
    pname = AvARRAY(pad_name);
    ppad = AvARRAY(pad);
    Perl_dump_indent(aTHX_ level, file,
	    "PADNAME = 0x%"UVxf"(0x%"UVxf") PAD = 0x%"UVxf"(0x%"UVxf")\n",
	    PTR2UV(pad_name), PTR2UV(pname), PTR2UV(pad), PTR2UV(ppad)
    );

    for (ix = 1; ix <= AvFILLp(pad_name); ix++) {
        const SV *namesv = pname[ix];
	if (namesv && namesv == &PL_sv_undef) {
	    namesv = NULL;
	}
	if (namesv) {
	    if (SvFAKE(namesv))
		Perl_dump_indent(aTHX_ level+1, file,
		    "%2d. 0x%"UVxf"<%lu> FAKE \"%s\" flags=0x%lx index=%lu\n",
		    (int) ix,
		    PTR2UV(ppad[ix]),
		    (unsigned long) (ppad[ix] ? SvREFCNT(ppad[ix]) : 0),
		    SvPVX_const(namesv),
		    (unsigned long)PARENT_FAKELEX_FLAGS(namesv),
		    (unsigned long)PARENT_PAD_INDEX(namesv)

		);
	    else
		Perl_dump_indent(aTHX_ level+1, file,
		    "%2d. 0x%"UVxf"<%lu> (%lu,%lu) \"%s\"\n",
		    (int) ix,
		    PTR2UV(ppad[ix]),
		    (unsigned long) (ppad[ix] ? SvREFCNT(ppad[ix]) : 0),
		    (unsigned long)COP_SEQ_RANGE_LOW(namesv),
		    (unsigned long)COP_SEQ_RANGE_HIGH(namesv),
		    SvPVX_const(namesv)
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
S_cv_dump(pTHX_ const CV *cv, const char *title)
{
    dVAR;
    const CV * const outside = CvOUTSIDE(cv);
    AV* const padlist = CvPADLIST(cv);

    PERL_ARGS_ASSERT_CV_DUMP;

    PerlIO_printf(Perl_debug_log,
		  "  %s: CV=0x%"UVxf" (%s), OUTSIDE=0x%"UVxf" (%s)\n",
		  title,
		  PTR2UV(cv),
		  (CvANON(cv) ? "ANON"
		   : (SvTYPE(cv) == SVt_PVFM) ? "FORMAT"
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
    dVAR;
    I32 ix;
    AV* const protopadlist = CvPADLIST(proto);
    const AV *const protopad_name = (const AV *)*av_fetch(protopadlist, 0, FALSE);
    const AV *const protopad = (const AV *)*av_fetch(protopadlist, 1, FALSE);
    SV** const pname = AvARRAY(protopad_name);
    SV** const ppad = AvARRAY(protopad);
    const I32 fname = AvFILLp(protopad_name);
    const I32 fpad = AvFILLp(protopad);
    CV* cv;
    SV** outpad;
    CV* outside;
    long depth;

    PERL_ARGS_ASSERT_CV_CLONE;

    assert(!CvUNIQUE(proto));

    /* Since cloneable anon subs can be nested, CvOUTSIDE may point
     * to a prototype; we instead want the cloned parent who called us.
     * Note that in general for formats, CvOUTSIDE != find_runcv */

    outside = CvOUTSIDE(proto);
    if (outside && CvCLONE(outside) && ! CvCLONED(outside))
	outside = find_runcv(NULL);
    depth = CvDEPTH(outside);
    assert(depth || SvTYPE(proto) == SVt_PVFM);
    if (!depth)
	depth = 1;
    assert(CvPADLIST(outside));

    ENTER;
    SAVESPTR(PL_compcv);

    cv = PL_compcv = MUTABLE_CV(newSV_type(SvTYPE(proto)));
    CvFLAGS(cv) = CvFLAGS(proto) & ~(CVf_CLONE|CVf_WEAKOUTSIDE);
    CvCLONED_on(cv);

#ifdef USE_ITHREADS
    CvFILE(cv)		= CvISXSUB(proto) ? CvFILE(proto)
					  : savepv(CvFILE(proto));
#else
    CvFILE(cv)		= CvFILE(proto);
#endif
    CvGV(cv)		= CvGV(proto);
    CvSTASH(cv)		= CvSTASH(proto);
    OP_REFCNT_LOCK;
    CvROOT(cv)		= OpREFCNT_inc(CvROOT(proto));
    OP_REFCNT_UNLOCK;
    CvSTART(cv)		= CvSTART(proto);
    CvOUTSIDE(cv)	= MUTABLE_CV(SvREFCNT_inc_simple(outside));
    CvOUTSIDE_SEQ(cv) = CvOUTSIDE_SEQ(proto);

    if (SvPOK(proto))
	sv_setpvn(MUTABLE_SV(cv), SvPVX_const(proto), SvCUR(proto));

    CvPADLIST(cv) = pad_new(padnew_CLONE|padnew_SAVE);

    av_fill(PL_comppad, fpad);
    for (ix = fname; ix >= 0; ix--)
	av_store(PL_comppad_name, ix, SvREFCNT_inc(pname[ix]));

    PL_curpad = AvARRAY(PL_comppad);

    outpad = AvARRAY(AvARRAY(CvPADLIST(outside))[depth]);

    for (ix = fpad; ix > 0; ix--) {
	SV* const namesv = (ix <= fname) ? pname[ix] : NULL;
	SV *sv = NULL;
	if (namesv && namesv != &PL_sv_undef) { /* lexical */
	    if (SvFAKE(namesv)) {   /* lexical from outside? */
		sv = outpad[PARENT_PAD_INDEX(namesv)];
		assert(sv);
		/* formats may have an inactive parent,
		   while my $x if $false can leave an active var marked as
		   stale. And state vars are always available */
		if (SvPADSTALE(sv) && !SvPAD_STATE(namesv)) {
		    Perl_ck_warner(aTHX_ packWARN(WARN_CLOSURE),
				   "Variable \"%s\" is not available", SvPVX_const(namesv));
		    sv = NULL;
		}
		else 
		    SvREFCNT_inc_simple_void_NN(sv);
	    }
	    if (!sv) {
                const char sigil = SvPVX_const(namesv)[0];
                if (sigil == '&')
		    sv = SvREFCNT_inc(ppad[ix]);
                else if (sigil == '@')
		    sv = MUTABLE_SV(newAV());
                else if (sigil == '%')
		    sv = MUTABLE_SV(newHV());
		else
		    sv = newSV(0);
		SvPADMY_on(sv);
		/* reset the 'assign only once' flag on each state var */
		if (SvPAD_STATE(namesv))
		    SvPADSTALE_on(sv);
	    }
	}
	else if (IS_PADGV(ppad[ix]) || IS_PADCONST(ppad[ix])) {
	    sv = SvREFCNT_inc_NN(ppad[ix]);
	}
	else {
	    sv = newSV(0);
	    SvPADTMP_on(sv);
	}
	PL_curpad[ix] = sv;
    }

    DEBUG_Xv(
	PerlIO_printf(Perl_debug_log, "\nPad CV clone\n");
	cv_dump(outside, "Outside");
	cv_dump(proto,	 "Proto");
	cv_dump(cv,	 "To");
    );

    LEAVE;

    if (CvCONST(cv)) {
	/* Constant sub () { $x } closing over $x - see lib/constant.pm:
	 * The prototype was marked as a candiate for const-ization,
	 * so try to grab the current const value, and if successful,
	 * turn into a const sub:
	 */
	SV* const const_sv = op_const_sv(CvSTART(cv), cv);
	if (const_sv) {
	    SvREFCNT_dec(cv);
	    cv = newCONSTSUB(CvSTASH(proto), NULL, const_sv);
	}
	else {
	    CvCONST_off(cv);
	}
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
    dVAR;
    I32 ix;
    AV * const comppad_name = MUTABLE_AV(AvARRAY(padlist)[0]);
    AV * const comppad = MUTABLE_AV(AvARRAY(padlist)[1]);
    SV ** const namepad = AvARRAY(comppad_name);
    SV ** const curpad = AvARRAY(comppad);

    PERL_ARGS_ASSERT_PAD_FIXUP_INNER_ANONS;
    PERL_UNUSED_ARG(old_cv);

    for (ix = AvFILLp(comppad_name); ix > 0; ix--) {
        const SV * const namesv = namepad[ix];
	if (namesv && namesv != &PL_sv_undef
	    && *SvPVX_const(namesv) == '&')
	{
	    CV * const innercv = MUTABLE_CV(curpad[ix]);
	    assert(CvWEAKOUTSIDE(innercv));
	    assert(CvOUTSIDE(innercv) == old_cv);
	    CvOUTSIDE(innercv) = new_cv;
	}
    }
}


/*
=for apidoc pad_push

Push a new pad frame onto the padlist, unless there's already a pad at
this depth, in which case don't bother creating a new one.  Then give
the new pad an @_ in slot zero.

=cut
*/

void
Perl_pad_push(pTHX_ PADLIST *padlist, int depth)
{
    dVAR;

    PERL_ARGS_ASSERT_PAD_PUSH;

    if (depth > AvFILLp(padlist)) {
	SV** const svp = AvARRAY(padlist);
	AV* const newpad = newAV();
	SV** const oldpad = AvARRAY(svp[depth-1]);
	I32 ix = AvFILLp((const AV *)svp[1]);
        const I32 names_fill = AvFILLp((const AV *)svp[0]);
	SV** const names = AvARRAY(svp[0]);
	AV *av;

	for ( ;ix > 0; ix--) {
	    if (names_fill >= ix && names[ix] != &PL_sv_undef) {
		const char sigil = SvPVX_const(names[ix])[0];
		if ((SvFLAGS(names[ix]) & SVf_FAKE)
			|| (SvFLAGS(names[ix]) & SVpad_STATE)
			|| sigil == '&')
		{
		    /* outer lexical or anon code */
		    av_store(newpad, ix, SvREFCNT_inc(oldpad[ix]));
		}
		else {		/* our own lexical */
		    SV *sv; 
		    if (sigil == '@')
			sv = MUTABLE_SV(newAV());
		    else if (sigil == '%')
			sv = MUTABLE_SV(newHV());
		    else
			sv = newSV(0);
		    av_store(newpad, ix, sv);
		    SvPADMY_on(sv);
		}
	    }
	    else if (IS_PADGV(oldpad[ix]) || IS_PADCONST(oldpad[ix])) {
		av_store(newpad, ix, SvREFCNT_inc_NN(oldpad[ix]));
	    }
	    else {
		/* save temporaries on recursion? */
		SV * const sv = newSV(0);
		av_store(newpad, ix, sv);
		SvPADTMP_on(sv);
	    }
	}
	av = newAV();
	av_extend(av, 0);
	av_store(newpad, 0, MUTABLE_SV(av));
	AvREIFY_only(av);

	av_store(padlist, depth, MUTABLE_SV(newpad));
	AvFILLp(padlist) = depth;
    }
}


HV *
Perl_pad_compname_type(pTHX_ const PADOFFSET po)
{
    dVAR;
    SV* const * const av = av_fetch(PL_comppad_name, po, FALSE);
    if ( SvPAD_TYPED(*av) ) {
        return SvSTASH(*av);
    }
    return NULL;
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
