/*    cv.h
 *
 *    Copyright (c) 1991-2002, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* This structure much match XPVCV in B/C.pm and the beginning of XPVFM
 * in sv.h  */

struct xpvcv {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xp_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xof_off;	/* integer value */
    NV		xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* magic for scalar array */
    HV*		xmg_stash;	/* class package */

    HV *	xcv_stash;
    OP *	xcv_start;
    OP *	xcv_root;
    void	(*xcv_xsub) (pTHX_ CV*);
    ANY		xcv_xsubany;
    GV *	xcv_gv;
    char *	xcv_file;
    long	xcv_depth;	/* >= 2 indicates recursive call */
    AV *	xcv_padlist;
    CV *	xcv_outside;
#ifdef USE_5005THREADS
    perl_mutex *xcv_mutexp;
    struct perl_thread *xcv_owner;	/* current owner thread */
#endif /* USE_5005THREADS */
    cv_flags_t	xcv_flags;
};

/*
=head1 Handy Values

=for apidoc AmU||Nullcv
Null CV pointer.

=head1 CV Manipulation Functions

=for apidoc Am|HV*|CvSTASH|CV* cv
Returns the stash of the CV.

=cut
*/

#define Nullcv Null(CV*)

#define CvSTASH(sv)	((XPVCV*)SvANY(sv))->xcv_stash
#define CvSTART(sv)	((XPVCV*)SvANY(sv))->xcv_start
#define CvROOT(sv)	((XPVCV*)SvANY(sv))->xcv_root
#define CvXSUB(sv)	((XPVCV*)SvANY(sv))->xcv_xsub
#define CvXSUBANY(sv)	((XPVCV*)SvANY(sv))->xcv_xsubany
#define CvGV(sv)	((XPVCV*)SvANY(sv))->xcv_gv
#define CvFILE(sv)	((XPVCV*)SvANY(sv))->xcv_file
#ifdef USE_ITHREADS
#  define CvFILE_set_from_cop(sv, cop)	(CvFILE(sv) = savepv(CopFILE(cop)))
#else
#  define CvFILE_set_from_cop(sv, cop)	(CvFILE(sv) = CopFILE(cop))
#endif
#define CvFILEGV(sv)	(gv_fetchfile(CvFILE(sv)))
#define CvDEPTH(sv)	((XPVCV*)SvANY(sv))->xcv_depth
#define CvPADLIST(sv)	((XPVCV*)SvANY(sv))->xcv_padlist
#define CvOUTSIDE(sv)	((XPVCV*)SvANY(sv))->xcv_outside
#ifdef USE_5005THREADS
#define CvMUTEXP(sv)	((XPVCV*)SvANY(sv))->xcv_mutexp
#define CvOWNER(sv)	((XPVCV*)SvANY(sv))->xcv_owner
#endif /* USE_5005THREADS */
#define CvFLAGS(sv)	((XPVCV*)SvANY(sv))->xcv_flags

#define CVf_CLONE	0x0001	/* anon CV uses external lexicals */
#define CVf_CLONED	0x0002	/* a clone of one of those */
#define CVf_ANON	0x0004	/* CvGV() can't be trusted */
#define CVf_OLDSTYLE	0x0008
#define CVf_UNIQUE	0x0010	/* can't be cloned */
#define CVf_NODEBUG	0x0020	/* no DB::sub indirection for this CV
				   (esp. useful for special XSUBs) */
#define CVf_METHOD	0x0040	/* CV is explicitly marked as a method */
#define CVf_LOCKED	0x0080	/* CV locks itself or first arg on entry */
#define CVf_LVALUE	0x0100  /* CV return value can be used as lvalue */
#define CVf_CONST	0x0200  /* inlinable sub */
/* This symbol for optimised communication between toke.c and op.c: */
#define CVf_BUILTIN_ATTRS	(CVf_METHOD|CVf_LOCKED|CVf_LVALUE)

#define CvCLONE(cv)		(CvFLAGS(cv) & CVf_CLONE)
#define CvCLONE_on(cv)		(CvFLAGS(cv) |= CVf_CLONE)
#define CvCLONE_off(cv)		(CvFLAGS(cv) &= ~CVf_CLONE)

#define CvCLONED(cv)		(CvFLAGS(cv) & CVf_CLONED)
#define CvCLONED_on(cv)		(CvFLAGS(cv) |= CVf_CLONED)
#define CvCLONED_off(cv)	(CvFLAGS(cv) &= ~CVf_CLONED)

#define CvANON(cv)		(CvFLAGS(cv) & CVf_ANON)
#define CvANON_on(cv)		(CvFLAGS(cv) |= CVf_ANON)
#define CvANON_off(cv)		(CvFLAGS(cv) &= ~CVf_ANON)

#ifdef PERL_XSUB_OLDSTYLE
#define CvOLDSTYLE(cv)		(CvFLAGS(cv) & CVf_OLDSTYLE)
#define CvOLDSTYLE_on(cv)	(CvFLAGS(cv) |= CVf_OLDSTYLE)
#define CvOLDSTYLE_off(cv)	(CvFLAGS(cv) &= ~CVf_OLDSTYLE)
#endif

#define CvUNIQUE(cv)		(CvFLAGS(cv) & CVf_UNIQUE)
#define CvUNIQUE_on(cv)		(CvFLAGS(cv) |= CVf_UNIQUE)
#define CvUNIQUE_off(cv)	(CvFLAGS(cv) &= ~CVf_UNIQUE)

#define CvNODEBUG(cv)		(CvFLAGS(cv) & CVf_NODEBUG)
#define CvNODEBUG_on(cv)	(CvFLAGS(cv) |= CVf_NODEBUG)
#define CvNODEBUG_off(cv)	(CvFLAGS(cv) &= ~CVf_NODEBUG)

#define CvMETHOD(cv)		(CvFLAGS(cv) & CVf_METHOD)
#define CvMETHOD_on(cv)		(CvFLAGS(cv) |= CVf_METHOD)
#define CvMETHOD_off(cv)	(CvFLAGS(cv) &= ~CVf_METHOD)

#define CvLOCKED(cv)		(CvFLAGS(cv) & CVf_LOCKED)
#define CvLOCKED_on(cv)		(CvFLAGS(cv) |= CVf_LOCKED)
#define CvLOCKED_off(cv)	(CvFLAGS(cv) &= ~CVf_LOCKED)

#define CvLVALUE(cv)		(CvFLAGS(cv) & CVf_LVALUE)
#define CvLVALUE_on(cv)		(CvFLAGS(cv) |= CVf_LVALUE)
#define CvLVALUE_off(cv)	(CvFLAGS(cv) &= ~CVf_LVALUE)

#define CvEVAL(cv)		(CvUNIQUE(cv) && !SvFAKE(cv))
#define CvEVAL_on(cv)		(CvUNIQUE_on(cv),SvFAKE_off(cv))
#define CvEVAL_off(cv)		CvUNIQUE_off(cv)

/* BEGIN|INIT|END */
#define CvSPECIAL(cv)		(CvUNIQUE(cv) && SvFAKE(cv))
#define CvSPECIAL_on(cv)	(CvUNIQUE_on(cv),SvFAKE_on(cv))
#define CvSPECIAL_off(cv)	(CvUNIQUE_off(cv),SvFAKE_off(cv))

#define CvCONST(cv)		(CvFLAGS(cv) & CVf_CONST)
#define CvCONST_on(cv)		(CvFLAGS(cv) |= CVf_CONST)
#define CvCONST_off(cv)		(CvFLAGS(cv) &= ~CVf_CONST)

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
is managed "manual" (mostly in op.c) rather than normal av.c rules.
The items in the AV are not SVs as for a normal AV, but other AVs:

0'th Entry of the CvPADLIST is an AV which represents the "names" or rather
the "static type information" for lexicals.

The CvDEPTH'th entry of CvPADLIST AV is an AV which is the stack frame at that
depth of recursion into the CV.
The 0'th slot of a frame AV is an AV which is @_.
other entries are storage for variables and op targets.

During compilation:
C<PL_comppad_name> is set the the the names AV.
C<PL_comppad> is set the the frame AV for the frame CvDEPTH == 1.
C<PL_curpad> is set the body of the frame AV (i.e. AvARRAY(PL_comppad)).

Itterating over the names AV itterates over all possible pad
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
NV+1..IV inclusive is a range of cop_seq numbers for which the name is valid.
For typed lexicals name SV is SVt_PVMG and SvSTASH points at the type.

If SvFAKE is set on the name SV then slot in the frame AVs are
a REFCNT'ed references to a lexical from "outside".

If the 'name' is '&' the the corresponding entry in frame AV
is a CV representing a possible closure.
(SvFAKE and name of '&' is not a meaningful combination currently but could
become so if C<my sub foo {}> is implemented.)

=cut
*/

