#line 2 "op.c"
/*    op.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'You see: Mr. Drogo, he married poor Miss Primula Brandybuck.  She was
 *  our Mr. Bilbo's first cousin on the mother's side (her mother being the
 *  youngest of the Old Took's daughters); and Mr. Drogo was his second
 *  cousin.  So Mr. Frodo is his first *and* second cousin, once removed
 *  either way, as the saying is, if you follow me.'       --the Gaffer
 *
 *     [p.23 of _The Lord of the Rings_, I/i: "A Long-Expected Party"]
 */

/* This file contains the functions that create, manipulate and optimize
 * the OP structures that hold a compiled perl program.
 *
 * A Perl program is compiled into a tree of OPs. Each op contains
 * structural pointers (eg to its siblings and the next op in the
 * execution sequence), a pointer to the function that would execute the
 * op, plus any data specific to that op. For example, an OP_CONST op
 * points to the pp_const() function and to an SV containing the constant
 * value. When pp_const() is executed, its job is to push that SV onto the
 * stack.
 *
 * OPs are mainly created by the newFOO() functions, which are mainly
 * called from the parser (in perly.y) as the code is parsed. For example
 * the Perl code $a + $b * $c would cause the equivalent of the following
 * to be called (oversimplifying a bit):
 *
 *  newBINOP(OP_ADD, flags,
 *	newSVREF($a),
 *	newBINOP(OP_MULTIPLY, flags, newSVREF($b), newSVREF($c))
 *  )
 *
 * Note that during the build of miniperl, a temporary copy of this file
 * is made, called opmini.c.
 */

/*
Perl's compiler is essentially a 3-pass compiler with interleaved phases:

    A bottom-up pass
    A top-down pass
    An execution-order pass

The bottom-up pass is represented by all the "newOP" routines and
the ck_ routines.  The bottom-upness is actually driven by yacc.
So at the point that a ck_ routine fires, we have no idea what the
context is, either upward in the syntax tree, or either forward or
backward in the execution order.  (The bottom-up parser builds that
part of the execution order it knows about, but if you follow the "next"
links around, you'll find it's actually a closed loop through the
top level node.)

Whenever the bottom-up parser gets to a node that supplies context to
its components, it invokes that portion of the top-down pass that applies
to that part of the subtree (and marks the top node as processed, so
if a node further up supplies context, it doesn't have to take the
plunge again).  As a particular subcase of this, as the new node is
built, it takes all the closed execution loops of its subcomponents
and links them into a new closed loop for the higher level node.  But
it's still not the real execution order.

The actual execution order is not known till we get a grammar reduction
to a top-level unit like a subroutine or file that will be called by
"name" rather than via a "next" pointer.  At that point, we can call
into peep() to do that code's portion of the 3rd pass.  It has to be
recursive, but it's recursive on basic blocks, not on tree nodes.
*/

/* To implement user lexical pragmas, there needs to be a way at run time to
   get the compile time state of %^H for that block.  Storing %^H in every
   block (or even COP) would be very expensive, so a different approach is
   taken.  The (running) state of %^H is serialised into a tree of HE-like
   structs.  Stores into %^H are chained onto the current leaf as a struct
   refcounted_he * with the key and the value.  Deletes from %^H are saved
   with a value of PL_sv_placeholder.  The state of %^H at any point can be
   turned back into a regular HV by walking back up the tree from that point's
   leaf, ignoring any key you've already seen (placeholder or not), storing
   the rest into the HV structure, then removing the placeholders. Hence
   memory is only used to store the %^H deltas from the enclosing COP, rather
   than the entire %^H on each COP.

   To cause actions on %^H to write out the serialisation records, it has
   magic type 'H'. This magic (itself) does nothing, but its presence causes
   the values to gain magic type 'h', which has entries for set and clear.
   C<Perl_magic_sethint> updates C<PL_compiling.cop_hints_hash> with a store
   record, with deletes written by C<Perl_magic_clearhint>. C<SAVEHINTS>
   saves the current C<PL_compiling.cop_hints_hash> on the save stack, so that
   it will be correctly restored when any inner compiling scope is exited.
*/

#include "EXTERN.h"
#define PERL_IN_OP_C
#include "perl.h"
#include "keywords.h"
#include "feature.h"
#include "regcomp.h"

#define CALL_PEEP(o) PL_peepp(aTHX_ o)
#define CALL_RPEEP(o) PL_rpeepp(aTHX_ o)
#define CALL_OPFREEHOOK(o) if (PL_opfreehook) PL_opfreehook(aTHX_ o)

/* remove any leading "empty" ops from the op_next chain whose first
 * node's address is stored in op_p. Store the updated address of the
 * first node in op_p.
 */

STATIC void
S_prune_chain_head(pTHX_ OP** op_p)
{
    while (*op_p
        && (   (*op_p)->op_type == OP_NULL
            || (*op_p)->op_type == OP_SCOPE
            || (*op_p)->op_type == OP_SCALAR
            || (*op_p)->op_type == OP_LINESEQ)
    )
        *op_p = (*op_p)->op_next;
}


/* See the explanatory comments above struct opslab in op.h. */

#ifdef PERL_DEBUG_READONLY_OPS
#  define PERL_SLAB_SIZE 128
#  define PERL_MAX_SLAB_SIZE 4096
#  include <sys/mman.h>
#endif

#ifndef PERL_SLAB_SIZE
#  define PERL_SLAB_SIZE 64
#endif
#ifndef PERL_MAX_SLAB_SIZE
#  define PERL_MAX_SLAB_SIZE 2048
#endif

/* rounds up to nearest pointer */
#define SIZE_TO_PSIZE(x)	(((x) + sizeof(I32 *) - 1)/sizeof(I32 *))
#define DIFF(o,p)		((size_t)((I32 **)(p) - (I32**)(o)))

static OPSLAB *
S_new_slab(pTHX_ size_t sz)
{
#ifdef PERL_DEBUG_READONLY_OPS
    OPSLAB *slab = (OPSLAB *) mmap(0, sz * sizeof(I32 *),
				   PROT_READ|PROT_WRITE,
				   MAP_ANON|MAP_PRIVATE, -1, 0);
    DEBUG_m(PerlIO_printf(Perl_debug_log, "mapped %lu at %p\n",
			  (unsigned long) sz, slab));
    if (slab == MAP_FAILED) {
	perror("mmap failed");
	abort();
    }
    slab->opslab_size = (U16)sz;
#else
    OPSLAB *slab = (OPSLAB *)PerlMemShared_calloc(sz, sizeof(I32 *));
#endif
    slab->opslab_first = (OPSLOT *)((I32 **)slab + sz - 1);
    return slab;
}

/* requires double parens and aTHX_ */
#define DEBUG_S_warn(args)					       \
    DEBUG_S( 								\
	PerlIO_printf(Perl_debug_log, "%s", SvPVx_nolen(Perl_mess args)) \
    )

void *
Perl_Slab_Alloc(pTHX_ size_t sz)
{
    dVAR;
    OPSLAB *slab;
    OPSLAB *slab2;
    OPSLOT *slot;
    OP *o;
    size_t opsz, space;

    /* We only allocate ops from the slab during subroutine compilation.
       We find the slab via PL_compcv, hence that must be non-NULL. It could
       also be pointing to a subroutine which is now fully set up (CvROOT()
       pointing to the top of the optree for that sub), or a subroutine
       which isn't using the slab allocator. If our sanity checks aren't met,
       don't use a slab, but allocate the OP directly from the heap.  */
    if (!PL_compcv || CvROOT(PL_compcv)
     || (CvSTART(PL_compcv) && !CvSLABBED(PL_compcv)))
	return PerlMemShared_calloc(1, sz);

    /* While the subroutine is under construction, the slabs are accessed via
       CvSTART(), to avoid needing to expand PVCV by one pointer for something
       unneeded at runtime. Once a subroutine is constructed, the slabs are
       accessed via CvROOT(). So if CvSTART() is NULL, no slab has been
       allocated yet.  See the commit message for 8be227ab5eaa23f2 for more
       details.  */
    if (!CvSTART(PL_compcv)) {
	CvSTART(PL_compcv) =
	    (OP *)(slab = S_new_slab(aTHX_ PERL_SLAB_SIZE));
	CvSLABBED_on(PL_compcv);
	slab->opslab_refcnt = 2; /* one for the CV; one for the new OP */
    }
    else ++(slab = (OPSLAB *)CvSTART(PL_compcv))->opslab_refcnt;

    opsz = SIZE_TO_PSIZE(sz);
    sz = opsz + OPSLOT_HEADER_P;

    /* The slabs maintain a free list of OPs. In particular, constant folding
       will free up OPs, so it makes sense to re-use them where possible. A
       freed up slot is used in preference to a new allocation.  */
    if (slab->opslab_freed) {
	OP **too = &slab->opslab_freed;
	o = *too;
	DEBUG_S_warn((aTHX_ "found free op at %p, slab %p", o, slab));
	while (o && DIFF(OpSLOT(o), OpSLOT(o)->opslot_next) < sz) {
	    DEBUG_S_warn((aTHX_ "Alas! too small"));
	    o = *(too = &o->op_next);
	    if (o) { DEBUG_S_warn((aTHX_ "found another free op at %p", o)); }
	}
	if (o) {
	    *too = o->op_next;
	    Zero(o, opsz, I32 *);
	    o->op_slabbed = 1;
	    return (void *)o;
	}
    }

#define INIT_OPSLOT \
	    slot->opslot_slab = slab;			\
	    slot->opslot_next = slab2->opslab_first;	\
	    slab2->opslab_first = slot;			\
	    o = &slot->opslot_op;			\
	    o->op_slabbed = 1

    /* The partially-filled slab is next in the chain. */
    slab2 = slab->opslab_next ? slab->opslab_next : slab;
    if ((space = DIFF(&slab2->opslab_slots, slab2->opslab_first)) < sz) {
	/* Remaining space is too small. */

	/* If we can fit a BASEOP, add it to the free chain, so as not
	   to waste it. */
	if (space >= SIZE_TO_PSIZE(sizeof(OP)) + OPSLOT_HEADER_P) {
	    slot = &slab2->opslab_slots;
	    INIT_OPSLOT;
	    o->op_type = OP_FREED;
	    o->op_next = slab->opslab_freed;
	    slab->opslab_freed = o;
	}

	/* Create a new slab.  Make this one twice as big. */
	slot = slab2->opslab_first;
	while (slot->opslot_next) slot = slot->opslot_next;
	slab2 = S_new_slab(aTHX_
			    (DIFF(slab2, slot)+1)*2 > PERL_MAX_SLAB_SIZE
					? PERL_MAX_SLAB_SIZE
					: (DIFF(slab2, slot)+1)*2);
	slab2->opslab_next = slab->opslab_next;
	slab->opslab_next = slab2;
    }
    assert(DIFF(&slab2->opslab_slots, slab2->opslab_first) >= sz);

    /* Create a new op slot */
    slot = (OPSLOT *)((I32 **)slab2->opslab_first - sz);
    assert(slot >= &slab2->opslab_slots);
    if (DIFF(&slab2->opslab_slots, slot)
	 < SIZE_TO_PSIZE(sizeof(OP)) + OPSLOT_HEADER_P)
	slot = &slab2->opslab_slots;
    INIT_OPSLOT;
    DEBUG_S_warn((aTHX_ "allocating op at %p, slab %p", o, slab));
    return (void *)o;
}

#undef INIT_OPSLOT

#ifdef PERL_DEBUG_READONLY_OPS
void
Perl_Slab_to_ro(pTHX_ OPSLAB *slab)
{
    PERL_ARGS_ASSERT_SLAB_TO_RO;

    if (slab->opslab_readonly) return;
    slab->opslab_readonly = 1;
    for (; slab; slab = slab->opslab_next) {
	/*DEBUG_U(PerlIO_printf(Perl_debug_log,"mprotect ->ro %lu at %p\n",
			      (unsigned long) slab->opslab_size, slab));*/
	if (mprotect(slab, slab->opslab_size * sizeof(I32 *), PROT_READ))
	    Perl_warn(aTHX_ "mprotect for %p %lu failed with %d", slab,
			     (unsigned long)slab->opslab_size, errno);
    }
}

void
Perl_Slab_to_rw(pTHX_ OPSLAB *const slab)
{
    OPSLAB *slab2;

    PERL_ARGS_ASSERT_SLAB_TO_RW;

    if (!slab->opslab_readonly) return;
    slab2 = slab;
    for (; slab2; slab2 = slab2->opslab_next) {
	/*DEBUG_U(PerlIO_printf(Perl_debug_log,"mprotect ->rw %lu at %p\n",
			      (unsigned long) size, slab2));*/
	if (mprotect((void *)slab2, slab2->opslab_size * sizeof(I32 *),
		     PROT_READ|PROT_WRITE)) {
	    Perl_warn(aTHX_ "mprotect RW for %p %lu failed with %d", slab,
			     (unsigned long)slab2->opslab_size, errno);
	}
    }
    slab->opslab_readonly = 0;
}

#else
#  define Slab_to_rw(op)    NOOP
#endif

/* This cannot possibly be right, but it was copied from the old slab
   allocator, to which it was originally added, without explanation, in
   commit 083fcd5. */
#ifdef NETWARE
#    define PerlMemShared PerlMem
#endif

void
Perl_Slab_Free(pTHX_ void *op)
{
    dVAR;
    OP * const o = (OP *)op;
    OPSLAB *slab;

    PERL_ARGS_ASSERT_SLAB_FREE;

    if (!o->op_slabbed) {
        if (!o->op_static)
	    PerlMemShared_free(op);
	return;
    }

    slab = OpSLAB(o);
    /* If this op is already freed, our refcount will get screwy. */
    assert(o->op_type != OP_FREED);
    o->op_type = OP_FREED;
    o->op_next = slab->opslab_freed;
    slab->opslab_freed = o;
    DEBUG_S_warn((aTHX_ "free op at %p, recorded in slab %p", o, slab));
    OpslabREFCNT_dec_padok(slab);
}

void
Perl_opslab_free_nopad(pTHX_ OPSLAB *slab)
{
    dVAR;
    const bool havepad = !!PL_comppad;
    PERL_ARGS_ASSERT_OPSLAB_FREE_NOPAD;
    if (havepad) {
	ENTER;
	PAD_SAVE_SETNULLPAD();
    }
    opslab_free(slab);
    if (havepad) LEAVE;
}

void
Perl_opslab_free(pTHX_ OPSLAB *slab)
{
    dVAR;
    OPSLAB *slab2;
    PERL_ARGS_ASSERT_OPSLAB_FREE;
    DEBUG_S_warn((aTHX_ "freeing slab %p", slab));
    assert(slab->opslab_refcnt == 1);
    for (; slab; slab = slab2) {
	slab2 = slab->opslab_next;
#ifdef DEBUGGING
	slab->opslab_refcnt = ~(size_t)0;
#endif
#ifdef PERL_DEBUG_READONLY_OPS
	DEBUG_m(PerlIO_printf(Perl_debug_log, "Deallocate slab at %p\n",
					       slab));
	if (munmap(slab, slab->opslab_size * sizeof(I32 *))) {
	    perror("munmap failed");
	    abort();
	}
#else
	PerlMemShared_free(slab);
#endif
    }
}

void
Perl_opslab_force_free(pTHX_ OPSLAB *slab)
{
    OPSLAB *slab2;
    OPSLOT *slot;
#ifdef DEBUGGING
    size_t savestack_count = 0;
#endif
    PERL_ARGS_ASSERT_OPSLAB_FORCE_FREE;
    slab2 = slab;
    do {
	for (slot = slab2->opslab_first;
	     slot->opslot_next;
	     slot = slot->opslot_next) {
	    if (slot->opslot_op.op_type != OP_FREED
	     && !(slot->opslot_op.op_savefree
#ifdef DEBUGGING
		  && ++savestack_count
#endif
		 )
	    ) {
		assert(slot->opslot_op.op_slabbed);
		op_free(&slot->opslot_op);
		if (slab->opslab_refcnt == 1) goto free;
	    }
	}
    } while ((slab2 = slab2->opslab_next));
    /* > 1 because the CV still holds a reference count. */
    if (slab->opslab_refcnt > 1) { /* still referenced by the savestack */
#ifdef DEBUGGING
	assert(savestack_count == slab->opslab_refcnt-1);
#endif
	/* Remove the CV’s reference count. */
	slab->opslab_refcnt--;
	return;
    }
   free:
    opslab_free(slab);
}

#ifdef PERL_DEBUG_READONLY_OPS
OP *
Perl_op_refcnt_inc(pTHX_ OP *o)
{
    if(o) {
        OPSLAB *const slab = o->op_slabbed ? OpSLAB(o) : NULL;
        if (slab && slab->opslab_readonly) {
            Slab_to_rw(slab);
            ++o->op_targ;
            Slab_to_ro(slab);
        } else {
            ++o->op_targ;
        }
    }
    return o;

}

PADOFFSET
Perl_op_refcnt_dec(pTHX_ OP *o)
{
    PADOFFSET result;
    OPSLAB *const slab = o->op_slabbed ? OpSLAB(o) : NULL;

    PERL_ARGS_ASSERT_OP_REFCNT_DEC;

    if (slab && slab->opslab_readonly) {
        Slab_to_rw(slab);
        result = --o->op_targ;
        Slab_to_ro(slab);
    } else {
        result = --o->op_targ;
    }
    return result;
}
#endif
/*
 * In the following definition, the ", (OP*)0" is just to make the compiler
 * think the expression is of the right type: croak actually does a Siglongjmp.
 */
#define CHECKOP(type,o) \
    ((PL_op_mask && PL_op_mask[type])				\
     ? ( op_free((OP*)o),					\
	 Perl_croak(aTHX_ "'%s' trapped by operation mask", PL_op_desc[type]),	\
	 (OP*)0 )						\
     : PL_check[type](aTHX_ (OP*)o))

#define RETURN_UNLIMITED_NUMBER (PERL_INT_MAX / 2)

#define CHANGE_TYPE(o,type) \
    STMT_START {				\
	o->op_type = (OPCODE)type;		\
	o->op_ppaddr = PL_ppaddr[type];		\
    } STMT_END

STATIC SV*
S_gv_ename(pTHX_ GV *gv)
{
    SV* const tmpsv = sv_newmortal();

    PERL_ARGS_ASSERT_GV_ENAME;

    gv_efullname3(tmpsv, gv, NULL);
    return tmpsv;
}

STATIC OP *
S_no_fh_allowed(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_NO_FH_ALLOWED;

    yyerror(Perl_form(aTHX_ "Missing comma after first argument to %s function",
		 OP_DESC(o)));
    return o;
}

STATIC OP *
S_too_few_arguments_sv(pTHX_ OP *o, SV *namesv, U32 flags)
{
    PERL_ARGS_ASSERT_TOO_FEW_ARGUMENTS_SV;
    yyerror_pv(Perl_form(aTHX_ "Not enough arguments for %"SVf, namesv),
                                    SvUTF8(namesv) | flags);
    return o;
}

STATIC OP *
S_too_few_arguments_pv(pTHX_ OP *o, const char* name, U32 flags)
{
    PERL_ARGS_ASSERT_TOO_FEW_ARGUMENTS_PV;
    yyerror_pv(Perl_form(aTHX_ "Not enough arguments for %s", name), flags);
    return o;
}
 
STATIC OP *
S_too_many_arguments_pv(pTHX_ OP *o, const char *name, U32 flags)
{
    PERL_ARGS_ASSERT_TOO_MANY_ARGUMENTS_PV;

    yyerror_pv(Perl_form(aTHX_ "Too many arguments for %s", name), flags);
    return o;
}

STATIC OP *
S_too_many_arguments_sv(pTHX_ OP *o, SV *namesv, U32 flags)
{
    PERL_ARGS_ASSERT_TOO_MANY_ARGUMENTS_SV;

    yyerror_pv(Perl_form(aTHX_ "Too many arguments for %"SVf, SVfARG(namesv)),
                SvUTF8(namesv) | flags);
    return o;
}

STATIC void
S_bad_type_pv(pTHX_ I32 n, const char *t, const char *name, U32 flags, const OP *kid)
{
    PERL_ARGS_ASSERT_BAD_TYPE_PV;

    yyerror_pv(Perl_form(aTHX_ "Type of arg %d to %s must be %s (not %s)",
		 (int)n, name, t, OP_DESC(kid)), flags);
}

STATIC void
S_bad_type_gv(pTHX_ I32 n, const char *t, GV *gv, U32 flags, const OP *kid)
{
    SV * const namesv = gv_ename(gv);
    PERL_ARGS_ASSERT_BAD_TYPE_GV;
 
    yyerror_pv(Perl_form(aTHX_ "Type of arg %d to %"SVf" must be %s (not %s)",
		 (int)n, SVfARG(namesv), t, OP_DESC(kid)), SvUTF8(namesv) | flags);
}

STATIC void
S_no_bareword_allowed(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_NO_BAREWORD_ALLOWED;

    if (PL_madskills)
	return;		/* various ok barewords are hidden in extra OP_NULL */
    qerror(Perl_mess(aTHX_
		     "Bareword \"%"SVf"\" not allowed while \"strict subs\" in use",
		     SVfARG(cSVOPo_sv)));
    o->op_private &= ~OPpCONST_STRICT; /* prevent warning twice about the same OP */
}

/* "register" allocation */

PADOFFSET
Perl_allocmy(pTHX_ const char *const name, const STRLEN len, const U32 flags)
{
    dVAR;
    PADOFFSET off;
    const bool is_our = (PL_parser->in_my == KEY_our);

    PERL_ARGS_ASSERT_ALLOCMY;

    if (flags & ~SVf_UTF8)
	Perl_croak(aTHX_ "panic: allocmy illegal flag bits 0x%" UVxf,
		   (UV)flags);

    /* Until we're using the length for real, cross check that we're being
       told the truth.  */
    assert(strlen(name) == len);

    /* complain about "my $<special_var>" etc etc */
    if (len &&
	!(is_our ||
	  isALPHA(name[1]) ||
	  ((flags & SVf_UTF8) && isIDFIRST_utf8((U8 *)name+1)) ||
	  (name[1] == '_' && (*name == '$' || len > 2))))
    {
	/* name[2] is true if strlen(name) > 2  */
	if (!(flags & SVf_UTF8 && UTF8_IS_START(name[1]))
	 && (!isPRINT(name[1]) || strchr("\t\n\r\f", name[1]))) {
	    yyerror(Perl_form(aTHX_ "Can't use global %c^%c%.*s in \"%s\"",
			      name[0], toCTRL(name[1]), (int)(len - 2), name + 2,
			      PL_parser->in_my == KEY_state ? "state" : "my"));
	} else {
	    yyerror_pv(Perl_form(aTHX_ "Can't use global %.*s in \"%s\"", (int) len, name,
			      PL_parser->in_my == KEY_state ? "state" : "my"), flags & SVf_UTF8);
	}
    }
    else if (len == 2 && name[1] == '_' && !is_our)
	/* diag_listed_as: Use of my $_ is experimental */
	Perl_ck_warner_d(aTHX_ packWARN(WARN_EXPERIMENTAL__LEXICAL_TOPIC),
			      "Use of %s $_ is experimental",
			       PL_parser->in_my == KEY_state
				 ? "state"
				 : "my");

    /* allocate a spare slot and store the name in that slot */

    off = pad_add_name_pvn(name, len,
		       (is_our ? padadd_OUR :
		        PL_parser->in_my == KEY_state ? padadd_STATE : 0)
                            | ( flags & SVf_UTF8 ? SVf_UTF8 : 0 ),
		    PL_parser->in_my_stash,
		    (is_our
		        /* $_ is always in main::, even with our */
			? (PL_curstash && !strEQ(name,"$_") ? PL_curstash : PL_defstash)
			: NULL
		    )
    );
    /* anon sub prototypes contains state vars should always be cloned,
     * otherwise the state var would be shared between anon subs */

    if (PL_parser->in_my == KEY_state && CvANON(PL_compcv))
	CvCLONE_on(PL_compcv);

    return off;
}

/*
=for apidoc alloccopstash

Available only under threaded builds, this function allocates an entry in
C<PL_stashpad> for the stash passed to it.

=cut
*/

#ifdef USE_ITHREADS
PADOFFSET
Perl_alloccopstash(pTHX_ HV *hv)
{
    PADOFFSET off = 0, o = 1;
    bool found_slot = FALSE;

    PERL_ARGS_ASSERT_ALLOCCOPSTASH;

    if (PL_stashpad[PL_stashpadix] == hv) return PL_stashpadix;

    for (; o < PL_stashpadmax; ++o) {
	if (PL_stashpad[o] == hv) return PL_stashpadix = o;
	if (!PL_stashpad[o] || SvTYPE(PL_stashpad[o]) != SVt_PVHV)
	    found_slot = TRUE, off = o;
    }
    if (!found_slot) {
	Renew(PL_stashpad, PL_stashpadmax + 10, HV *);
	Zero(PL_stashpad + PL_stashpadmax, 10, HV *);
	off = PL_stashpadmax;
	PL_stashpadmax += 10;
    }

    PL_stashpad[PL_stashpadix = off] = hv;
    return off;
}
#endif

/* free the body of an op without examining its contents.
 * Always use this rather than FreeOp directly */

static void
S_op_destroy(pTHX_ OP *o)
{
    FreeOp(o);
}

/* Destructor */

/*
=for apidoc Am|void|op_free|OP *o

Free an op.  Only use this when an op is no longer linked to from any
optree.

=cut
*/

void
Perl_op_free(pTHX_ OP *o)
{
    dVAR;
    OPCODE type;

    /* Though ops may be freed twice, freeing the op after its slab is a
       big no-no. */
    assert(!o || !o->op_slabbed || OpSLAB(o)->opslab_refcnt != ~(size_t)0); 
    /* During the forced freeing of ops after compilation failure, kidops
       may be freed before their parents. */
    if (!o || o->op_type == OP_FREED)
	return;

    type = o->op_type;
    if (o->op_private & OPpREFCOUNTED) {
	switch (type) {
	case OP_LEAVESUB:
	case OP_LEAVESUBLV:
	case OP_LEAVEEVAL:
	case OP_LEAVE:
	case OP_SCOPE:
	case OP_LEAVEWRITE:
	    {
	    PADOFFSET refcnt;
	    OP_REFCNT_LOCK;
	    refcnt = OpREFCNT_dec(o);
	    OP_REFCNT_UNLOCK;
	    if (refcnt) {
		/* Need to find and remove any pattern match ops from the list
		   we maintain for reset().  */
		find_and_forget_pmops(o);
		return;
	    }
	    }
	    break;
	default:
	    break;
	}
    }

    /* Call the op_free hook if it has been set. Do it now so that it's called
     * at the right time for refcounted ops, but still before all of the kids
     * are freed. */
    CALL_OPFREEHOOK(o);

    if (o->op_flags & OPf_KIDS) {
        OP *kid, *nextkid;
	for (kid = cUNOPo->op_first; kid; kid = nextkid) {
	    nextkid = kid->op_sibling; /* Get before next freeing kid */
	    op_free(kid);
	}
    }
    if (type == OP_NULL)
	type = (OPCODE)o->op_targ;

    if (o->op_slabbed)
        Slab_to_rw(OpSLAB(o));

    /* COP* is not cleared by op_clear() so that we may track line
     * numbers etc even after null() */
    if (type == OP_NEXTSTATE || type == OP_DBSTATE) {
	cop_free((COP*)o);
    }

    op_clear(o);
    FreeOp(o);
#ifdef DEBUG_LEAKING_SCALARS
    if (PL_op == o)
	PL_op = NULL;
#endif
}

void
Perl_op_clear(pTHX_ OP *o)
{

    dVAR;

    PERL_ARGS_ASSERT_OP_CLEAR;

#ifdef PERL_MAD
    mad_free(o->op_madprop);
    o->op_madprop = 0;
#endif    

 retry:
    switch (o->op_type) {
    case OP_NULL:	/* Was holding old type, if any. */
	if (PL_madskills && o->op_targ != OP_NULL) {
	    o->op_type = (Optype)o->op_targ;
	    o->op_targ = 0;
	    goto retry;
	}
    case OP_ENTERTRY:
    case OP_ENTEREVAL:	/* Was holding hints. */
	o->op_targ = 0;
	break;
    default:
	if (!(o->op_flags & OPf_REF)
	    || (PL_check[o->op_type] != Perl_ck_ftst))
	    break;
	/* FALL THROUGH */
    case OP_GVSV:
    case OP_GV:
    case OP_AELEMFAST:
	{
	    GV *gv = (o->op_type == OP_GV || o->op_type == OP_GVSV)
#ifdef USE_ITHREADS
			&& PL_curpad
#endif
			? cGVOPo_gv : NULL;
	    /* It's possible during global destruction that the GV is freed
	       before the optree. Whilst the SvREFCNT_inc is happy to bump from
	       0 to 1 on a freed SV, the corresponding SvREFCNT_dec from 1 to 0
	       will trigger an assertion failure, because the entry to sv_clear
	       checks that the scalar is not already freed.  A check of for
	       !SvIS_FREED(gv) turns out to be invalid, because during global
	       destruction the reference count can be forced down to zero
	       (with SVf_BREAK set).  In which case raising to 1 and then
	       dropping to 0 triggers cleanup before it should happen.  I
	       *think* that this might actually be a general, systematic,
	       weakness of the whole idea of SVf_BREAK, in that code *is*
	       allowed to raise and lower references during global destruction,
	       so any *valid* code that happens to do this during global
	       destruction might well trigger premature cleanup.  */
	    bool still_valid = gv && SvREFCNT(gv);

	    if (still_valid)
		SvREFCNT_inc_simple_void(gv);
#ifdef USE_ITHREADS
	    if (cPADOPo->op_padix > 0) {
		/* No GvIN_PAD_off(cGVOPo_gv) here, because other references
		 * may still exist on the pad */
		pad_swipe(cPADOPo->op_padix, TRUE);
		cPADOPo->op_padix = 0;
	    }
#else
	    SvREFCNT_dec(cSVOPo->op_sv);
	    cSVOPo->op_sv = NULL;
#endif
	    if (still_valid) {
		int try_downgrade = SvREFCNT(gv) == 2;
		SvREFCNT_dec_NN(gv);
		if (try_downgrade)
		    gv_try_downgrade(gv);
	    }
	}
	break;
    case OP_METHOD_NAMED:
    case OP_CONST:
    case OP_HINTSEVAL:
	SvREFCNT_dec(cSVOPo->op_sv);
	cSVOPo->op_sv = NULL;
#ifdef USE_ITHREADS
	/** Bug #15654
	  Even if op_clear does a pad_free for the target of the op,
	  pad_free doesn't actually remove the sv that exists in the pad;
	  instead it lives on. This results in that it could be reused as 
	  a target later on when the pad was reallocated.
	**/
        if(o->op_targ) {
          pad_swipe(o->op_targ,1);
          o->op_targ = 0;
        }
#endif
	break;
    case OP_DUMP:
    case OP_GOTO:
    case OP_NEXT:
    case OP_LAST:
    case OP_REDO:
	if (o->op_flags & (OPf_SPECIAL|OPf_STACKED|OPf_KIDS))
	    break;
	/* FALL THROUGH */
    case OP_TRANS:
    case OP_TRANSR:
	if (o->op_private & (OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF)) {
	    assert(o->op_type == OP_TRANS || o->op_type == OP_TRANSR);
#ifdef USE_ITHREADS
	    if (cPADOPo->op_padix > 0) {
		pad_swipe(cPADOPo->op_padix, TRUE);
		cPADOPo->op_padix = 0;
	    }
#else
	    SvREFCNT_dec(cSVOPo->op_sv);
	    cSVOPo->op_sv = NULL;
#endif
	}
	else {
	    PerlMemShared_free(cPVOPo->op_pv);
	    cPVOPo->op_pv = NULL;
	}
	break;
    case OP_SUBST:
	op_free(cPMOPo->op_pmreplrootu.op_pmreplroot);
	goto clear_pmop;
    case OP_PUSHRE:
#ifdef USE_ITHREADS
        if (cPMOPo->op_pmreplrootu.op_pmtargetoff) {
	    /* No GvIN_PAD_off here, because other references may still
	     * exist on the pad */
	    pad_swipe(cPMOPo->op_pmreplrootu.op_pmtargetoff, TRUE);
	}
#else
	SvREFCNT_dec(MUTABLE_SV(cPMOPo->op_pmreplrootu.op_pmtargetgv));
#endif
	/* FALL THROUGH */
    case OP_MATCH:
    case OP_QR:
clear_pmop:
	if (!(cPMOPo->op_pmflags & PMf_CODELIST_PRIVATE))
	    op_free(cPMOPo->op_code_list);
	cPMOPo->op_code_list = NULL;
	forget_pmop(cPMOPo);
	cPMOPo->op_pmreplrootu.op_pmreplroot = NULL;
        /* we use the same protection as the "SAFE" version of the PM_ macros
         * here since sv_clean_all might release some PMOPs
         * after PL_regex_padav has been cleared
         * and the clearing of PL_regex_padav needs to
         * happen before sv_clean_all
         */
#ifdef USE_ITHREADS
	if(PL_regex_pad) {        /* We could be in destruction */
	    const IV offset = (cPMOPo)->op_pmoffset;
	    ReREFCNT_dec(PM_GETRE(cPMOPo));
	    PL_regex_pad[offset] = &PL_sv_undef;
            sv_catpvn_nomg(PL_regex_pad[0], (const char *)&offset,
			   sizeof(offset));
        }
#else
	ReREFCNT_dec(PM_GETRE(cPMOPo));
	PM_SETRE(cPMOPo, NULL);
#endif

	break;
    }

    if (o->op_targ > 0) {
	pad_free(o->op_targ);
	o->op_targ = 0;
    }
}

STATIC void
S_cop_free(pTHX_ COP* cop)
{
    PERL_ARGS_ASSERT_COP_FREE;

    CopFILE_free(cop);
    if (! specialWARN(cop->cop_warnings))
	PerlMemShared_free(cop->cop_warnings);
    cophh_free(CopHINTHASH_get(cop));
    if (PL_curcop == cop)
       PL_curcop = NULL;
}

STATIC void
S_forget_pmop(pTHX_ PMOP *const o
	      )
{
    HV * const pmstash = PmopSTASH(o);

    PERL_ARGS_ASSERT_FORGET_PMOP;

    if (pmstash && !SvIS_FREED(pmstash) && SvMAGICAL(pmstash)) {
	MAGIC * const mg = mg_find((const SV *)pmstash, PERL_MAGIC_symtab);
	if (mg) {
	    PMOP **const array = (PMOP**) mg->mg_ptr;
	    U32 count = mg->mg_len / sizeof(PMOP**);
	    U32 i = count;

	    while (i--) {
		if (array[i] == o) {
		    /* Found it. Move the entry at the end to overwrite it.  */
		    array[i] = array[--count];
		    mg->mg_len = count * sizeof(PMOP**);
		    /* Could realloc smaller at this point always, but probably
		       not worth it. Probably worth free()ing if we're the
		       last.  */
		    if(!count) {
			Safefree(mg->mg_ptr);
			mg->mg_ptr = NULL;
		    }
		    break;
		}
	    }
	}
    }
    if (PL_curpm == o) 
	PL_curpm = NULL;
}

STATIC void
S_find_and_forget_pmops(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_FIND_AND_FORGET_PMOPS;

    if (o->op_flags & OPf_KIDS) {
        OP *kid = cUNOPo->op_first;
	while (kid) {
	    switch (kid->op_type) {
	    case OP_SUBST:
	    case OP_PUSHRE:
	    case OP_MATCH:
	    case OP_QR:
		forget_pmop((PMOP*)kid);
	    }
	    find_and_forget_pmops(kid);
	    kid = kid->op_sibling;
	}
    }
}

/*
=for apidoc Am|void|op_null|OP *o

Neutralizes an op when it is no longer needed, but is still linked to from
other ops.

=cut
*/

void
Perl_op_null(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_OP_NULL;

    if (o->op_type == OP_NULL)
	return;
    if (!PL_madskills)
	op_clear(o);
    o->op_targ = o->op_type;
    o->op_type = OP_NULL;
    o->op_ppaddr = PL_ppaddr[OP_NULL];
}

void
Perl_op_refcnt_lock(pTHX)
{
    dVAR;
    PERL_UNUSED_CONTEXT;
    OP_REFCNT_LOCK;
}

void
Perl_op_refcnt_unlock(pTHX)
{
    dVAR;
    PERL_UNUSED_CONTEXT;
    OP_REFCNT_UNLOCK;
}

/* Contextualizers */

/*
=for apidoc Am|OP *|op_contextualize|OP *o|I32 context

Applies a syntactic context to an op tree representing an expression.
I<o> is the op tree, and I<context> must be C<G_SCALAR>, C<G_ARRAY>,
or C<G_VOID> to specify the context to apply.  The modified op tree
is returned.

=cut
*/

OP *
Perl_op_contextualize(pTHX_ OP *o, I32 context)
{
    PERL_ARGS_ASSERT_OP_CONTEXTUALIZE;
    switch (context) {
	case G_SCALAR: return scalar(o);
	case G_ARRAY:  return list(o);
	case G_VOID:   return scalarvoid(o);
	default:
	    Perl_croak(aTHX_ "panic: op_contextualize bad context %ld",
		       (long) context);
	    return o;
    }
}

/*
=head1 Optree Manipulation Functions

=for apidoc Am|OP*|op_linklist|OP *o
This function is the implementation of the L</LINKLIST> macro.  It should
not be called directly.

=cut
*/

OP *
Perl_op_linklist(pTHX_ OP *o)
{
    OP *first;

    PERL_ARGS_ASSERT_OP_LINKLIST;

    if (o->op_next)
	return o->op_next;

    /* establish postfix order */
    first = cUNOPo->op_first;
    if (first) {
        OP *kid;
	o->op_next = LINKLIST(first);
	kid = first;
	for (;;) {
	    if (kid->op_sibling) {
		kid->op_next = LINKLIST(kid->op_sibling);
		kid = kid->op_sibling;
	    } else {
		kid->op_next = o;
		break;
	    }
	}
    }
    else
	o->op_next = o;

    return o->op_next;
}

static OP *
S_scalarkids(pTHX_ OP *o)
{
    if (o && o->op_flags & OPf_KIDS) {
        OP *kid;
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    scalar(kid);
    }
    return o;
}

STATIC OP *
S_scalarboolean(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_SCALARBOOLEAN;

    if (o->op_type == OP_SASSIGN && cBINOPo->op_first->op_type == OP_CONST
     && !(cBINOPo->op_first->op_flags & OPf_SPECIAL)) {
	if (ckWARN(WARN_SYNTAX)) {
	    const line_t oldline = CopLINE(PL_curcop);

	    if (PL_parser && PL_parser->copline != NOLINE) {
		/* This ensures that warnings are reported at the first line
                   of the conditional, not the last.  */
		CopLINE_set(PL_curcop, PL_parser->copline);
            }
	    Perl_warner(aTHX_ packWARN(WARN_SYNTAX), "Found = in conditional, should be ==");
	    CopLINE_set(PL_curcop, oldline);
	}
    }
    return scalar(o);
}

static SV *
S_op_varname(pTHX_ const OP *o)
{
    assert(o);
    assert(o->op_type == OP_PADAV || o->op_type == OP_RV2AV ||
	   o->op_type == OP_PADHV || o->op_type == OP_RV2HV);
    {
	const char funny  = o->op_type == OP_PADAV
			 || o->op_type == OP_RV2AV ? '@' : '%';
	if (o->op_type == OP_RV2AV || o->op_type == OP_RV2HV) {
	    GV *gv;
	    if (cUNOPo->op_first->op_type != OP_GV
	     || !(gv = cGVOPx_gv(cUNOPo->op_first)))
		return NULL;
	    return varname(gv, funny, 0, NULL, 0, 1);
	}
	return
	    varname(MUTABLE_GV(PL_compcv), funny, o->op_targ, NULL, 0, 1);
    }
}

static void
S_op_pretty(pTHX_ const OP *o, SV **retsv, const char **retpv)
{ /* or not so pretty :-) */
    if (o->op_type == OP_CONST) {
	*retsv = cSVOPo_sv;
	if (SvPOK(*retsv)) {
	    SV *sv = *retsv;
	    *retsv = sv_newmortal();
	    pv_pretty(*retsv, SvPVX_const(sv), SvCUR(sv), 32, NULL, NULL,
		      PERL_PV_PRETTY_DUMP |PERL_PV_ESCAPE_UNI_DETECT);
	}
	else if (!SvOK(*retsv))
	    *retpv = "undef";
    }
    else *retpv = "...";
}

static void
S_scalar_slice_warning(pTHX_ const OP *o)
{
    OP *kid;
    const char lbrack =
	o->op_type == OP_HSLICE ? '{' : '[';
    const char rbrack =
	o->op_type == OP_HSLICE ? '}' : ']';
    SV *name;
    SV *keysv = NULL; /* just to silence compiler warnings */
    const char *key = NULL;

    if (!(o->op_private & OPpSLICEWARNING))
	return;
    if (PL_parser && PL_parser->error_count)
	/* This warning can be nonsensical when there is a syntax error. */
	return;

    kid = cLISTOPo->op_first;
    kid = kid->op_sibling; /* get past pushmark */
    /* weed out false positives: any ops that can return lists */
    switch (kid->op_type) {
    case OP_BACKTICK:
    case OP_GLOB:
    case OP_READLINE:
    case OP_MATCH:
    case OP_RV2AV:
    case OP_EACH:
    case OP_VALUES:
    case OP_KEYS:
    case OP_SPLIT:
    case OP_LIST:
    case OP_SORT:
    case OP_REVERSE:
    case OP_ENTERSUB:
    case OP_CALLER:
    case OP_LSTAT:
    case OP_STAT:
    case OP_READDIR:
    case OP_SYSTEM:
    case OP_TMS:
    case OP_LOCALTIME:
    case OP_GMTIME:
    case OP_ENTEREVAL:
    case OP_REACH:
    case OP_RKEYS:
    case OP_RVALUES:
	return;
    }

    /* Don't warn if we have a nulled list either. */
    if (kid->op_type == OP_NULL && kid->op_targ == OP_LIST)
        return;

    assert(kid->op_sibling);
    name = S_op_varname(aTHX_ kid->op_sibling);
    if (!name) /* XS module fiddling with the op tree */
	return;
    S_op_pretty(aTHX_ kid, &keysv, &key);
    assert(SvPOK(name));
    sv_chop(name,SvPVX(name)+1);
    if (key)
       /* diag_listed_as: Scalar value @%s[%s] better written as $%s[%s] */
	Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
		   "Scalar value @%"SVf"%c%s%c better written as $%"SVf
		   "%c%s%c",
		    SVfARG(name), lbrack, key, rbrack, SVfARG(name),
		    lbrack, key, rbrack);
    else
       /* diag_listed_as: Scalar value @%s[%s] better written as $%s[%s] */
	Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
		   "Scalar value @%"SVf"%c%"SVf"%c better written as $%"
		    SVf"%c%"SVf"%c",
		    SVfARG(name), lbrack, keysv, rbrack,
		    SVfARG(name), lbrack, keysv, rbrack);
}

OP *
Perl_scalar(pTHX_ OP *o)
{
    dVAR;
    OP *kid;

    /* assumes no premature commitment */
    if (!o || (PL_parser && PL_parser->error_count)
	 || (o->op_flags & OPf_WANT)
	 || o->op_type == OP_RETURN)
    {
	return o;
    }

    o->op_flags = (o->op_flags & ~OPf_WANT) | OPf_WANT_SCALAR;

    switch (o->op_type) {
    case OP_REPEAT:
	scalar(cBINOPo->op_first);
	break;
    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    scalar(kid);
	break;
	/* FALL THROUGH */
    case OP_SPLIT:
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
    case OP_NULL:
    default:
	if (o->op_flags & OPf_KIDS) {
	    for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling)
		scalar(kid);
	}
	break;
    case OP_LEAVE:
    case OP_LEAVETRY:
	kid = cLISTOPo->op_first;
	scalar(kid);
	kid = kid->op_sibling;
    do_kids:
	while (kid) {
	    OP *sib = kid->op_sibling;
	    if (sib && kid->op_type != OP_LEAVEWHEN)
		scalarvoid(kid);
	    else
		scalar(kid);
	    kid = sib;
	}
	PL_curcop = &PL_compiling;
	break;
    case OP_SCOPE:
    case OP_LINESEQ:
    case OP_LIST:
	kid = cLISTOPo->op_first;
	goto do_kids;
    case OP_SORT:
	Perl_ck_warner(aTHX_ packWARN(WARN_VOID), "Useless use of sort in scalar context");
	break;
    case OP_KVHSLICE:
    case OP_KVASLICE:
    {
	/* Warn about scalar context */
	const char lbrack = o->op_type == OP_KVHSLICE ? '{' : '[';
	const char rbrack = o->op_type == OP_KVHSLICE ? '}' : ']';
	SV *name;
	SV *keysv;
	const char *key = NULL;

	/* This warning can be nonsensical when there is a syntax error. */
	if (PL_parser && PL_parser->error_count)
	    break;

	if (!ckWARN(WARN_SYNTAX)) break;

	kid = cLISTOPo->op_first;
	kid = kid->op_sibling; /* get past pushmark */
	assert(kid->op_sibling);
	name = S_op_varname(aTHX_ kid->op_sibling);
	if (!name) /* XS module fiddling with the op tree */
	    break;
	S_op_pretty(aTHX_ kid, &keysv, &key);
	assert(SvPOK(name));
	sv_chop(name,SvPVX(name)+1);
	if (key)
  /* diag_listed_as: %%s[%s] in scalar context better written as $%s[%s] */
	    Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
		       "%%%"SVf"%c%s%c in scalar context better written "
		       "as $%"SVf"%c%s%c",
			SVfARG(name), lbrack, key, rbrack, SVfARG(name),
			lbrack, key, rbrack);
	else
  /* diag_listed_as: %%s[%s] in scalar context better written as $%s[%s] */
	    Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
		       "%%%"SVf"%c%"SVf"%c in scalar context better "
		       "written as $%"SVf"%c%"SVf"%c",
			SVfARG(name), lbrack, keysv, rbrack,
			SVfARG(name), lbrack, keysv, rbrack);
    }
    }
    return o;
}

OP *
Perl_scalarvoid(pTHX_ OP *o)
{
    dVAR;
    OP *kid;
    SV *useless_sv = NULL;
    const char* useless = NULL;
    SV* sv;
    U8 want;

    PERL_ARGS_ASSERT_SCALARVOID;

    /* trailing mad null ops don't count as "there" for void processing */
    if (PL_madskills &&
    	o->op_type != OP_NULL &&
	o->op_sibling &&
	o->op_sibling->op_type == OP_NULL)
    {
	OP *sib;
	for (sib = o->op_sibling;
		sib && sib->op_type == OP_NULL;
		sib = sib->op_sibling) ;
	
	if (!sib)
	    return o;
    }

    if (o->op_type == OP_NEXTSTATE
	|| o->op_type == OP_DBSTATE
	|| (o->op_type == OP_NULL && (o->op_targ == OP_NEXTSTATE
				      || o->op_targ == OP_DBSTATE)))
	PL_curcop = (COP*)o;		/* for warning below */

    /* assumes no premature commitment */
    want = o->op_flags & OPf_WANT;
    if ((want && want != OPf_WANT_SCALAR)
	 || (PL_parser && PL_parser->error_count)
	 || o->op_type == OP_RETURN || o->op_type == OP_REQUIRE || o->op_type == OP_LEAVEWHEN)
    {
	return o;
    }

    if ((o->op_private & OPpTARGET_MY)
	&& (PL_opargs[o->op_type] & OA_TARGLEX))/* OPp share the meaning */
    {
	return scalar(o);			/* As if inside SASSIGN */
    }

    o->op_flags = (o->op_flags & ~OPf_WANT) | OPf_WANT_VOID;

    switch (o->op_type) {
    default:
	if (!(PL_opargs[o->op_type] & OA_FOLDCONST))
	    break;
	/* FALL THROUGH */
    case OP_REPEAT:
	if (o->op_flags & OPf_STACKED)
	    break;
	goto func_ops;
    case OP_SUBSTR:
	if (o->op_private == 4)
	    break;
	/* FALL THROUGH */
    case OP_GVSV:
    case OP_WANTARRAY:
    case OP_GV:
    case OP_SMARTMATCH:
    case OP_PADSV:
    case OP_PADAV:
    case OP_PADHV:
    case OP_PADANY:
    case OP_AV2ARYLEN:
    case OP_REF:
    case OP_REFGEN:
    case OP_SREFGEN:
    case OP_DEFINED:
    case OP_HEX:
    case OP_OCT:
    case OP_LENGTH:
    case OP_VEC:
    case OP_INDEX:
    case OP_RINDEX:
    case OP_SPRINTF:
    case OP_AELEM:
    case OP_AELEMFAST:
    case OP_AELEMFAST_LEX:
    case OP_ASLICE:
    case OP_KVASLICE:
    case OP_HELEM:
    case OP_HSLICE:
    case OP_KVHSLICE:
    case OP_UNPACK:
    case OP_PACK:
    case OP_JOIN:
    case OP_LSLICE:
    case OP_ANONLIST:
    case OP_ANONHASH:
    case OP_SORT:
    case OP_REVERSE:
    case OP_RANGE:
    case OP_FLIP:
    case OP_FLOP:
    case OP_CALLER:
    case OP_FILENO:
    case OP_EOF:
    case OP_TELL:
    case OP_GETSOCKNAME:
    case OP_GETPEERNAME:
    case OP_READLINK:
    case OP_TELLDIR:
    case OP_GETPPID:
    case OP_GETPGRP:
    case OP_GETPRIORITY:
    case OP_TIME:
    case OP_TMS:
    case OP_LOCALTIME:
    case OP_GMTIME:
    case OP_GHBYNAME:
    case OP_GHBYADDR:
    case OP_GHOSTENT:
    case OP_GNBYNAME:
    case OP_GNBYADDR:
    case OP_GNETENT:
    case OP_GPBYNAME:
    case OP_GPBYNUMBER:
    case OP_GPROTOENT:
    case OP_GSBYNAME:
    case OP_GSBYPORT:
    case OP_GSERVENT:
    case OP_GPWNAM:
    case OP_GPWUID:
    case OP_GGRNAM:
    case OP_GGRGID:
    case OP_GETLOGIN:
    case OP_PROTOTYPE:
    case OP_RUNCV:
      func_ops:
	if (!(o->op_private & (OPpLVAL_INTRO|OPpOUR_INTRO)))
	    /* Otherwise it's "Useless use of grep iterator" */
	    useless = OP_DESC(o);
	break;

    case OP_SPLIT:
	kid = cLISTOPo->op_first;
	if (kid && kid->op_type == OP_PUSHRE
#ifdef USE_ITHREADS
		&& !((PMOP*)kid)->op_pmreplrootu.op_pmtargetoff)
#else
		&& !((PMOP*)kid)->op_pmreplrootu.op_pmtargetgv)
#endif
	    useless = OP_DESC(o);
	break;

    case OP_NOT:
       kid = cUNOPo->op_first;
       if (kid->op_type != OP_MATCH && kid->op_type != OP_SUBST &&
           kid->op_type != OP_TRANS && kid->op_type != OP_TRANSR) {
	        goto func_ops;
       }
       useless = "negative pattern binding (!~)";
       break;

    case OP_SUBST:
	if (cPMOPo->op_pmflags & PMf_NONDESTRUCT)
	    useless = "non-destructive substitution (s///r)";
	break;

    case OP_TRANSR:
	useless = "non-destructive transliteration (tr///r)";
	break;

    case OP_RV2GV:
    case OP_RV2SV:
    case OP_RV2AV:
    case OP_RV2HV:
	if (!(o->op_private & (OPpLVAL_INTRO|OPpOUR_INTRO)) &&
		(!o->op_sibling || o->op_sibling->op_type != OP_READLINE))
	    useless = "a variable";
	break;

    case OP_CONST:
	sv = cSVOPo_sv;
	if (cSVOPo->op_private & OPpCONST_STRICT)
	    no_bareword_allowed(o);
	else {
	    if (ckWARN(WARN_VOID)) {
		/* don't warn on optimised away booleans, eg 
		 * use constant Foo, 5; Foo || print; */
		if (cSVOPo->op_private & OPpCONST_SHORTCIRCUIT)
		    useless = NULL;
		/* the constants 0 and 1 are permitted as they are
		   conventionally used as dummies in constructs like
		        1 while some_condition_with_side_effects;  */
		else if (SvNIOK(sv) && (SvNV(sv) == 0.0 || SvNV(sv) == 1.0))
		    useless = NULL;
		else if (SvPOK(sv)) {
                    SV * const dsv = newSVpvs("");
                    useless_sv
                        = Perl_newSVpvf(aTHX_
                                        "a constant (%s)",
                                        pv_pretty(dsv, SvPVX_const(sv),
                                                  SvCUR(sv), 32, NULL, NULL,
                                                  PERL_PV_PRETTY_DUMP
                                                  | PERL_PV_ESCAPE_NOCLEAR
                                                  | PERL_PV_ESCAPE_UNI_DETECT));
                    SvREFCNT_dec_NN(dsv);
		}
		else if (SvOK(sv)) {
		    useless_sv = Perl_newSVpvf(aTHX_ "a constant (%"SVf")", sv);
		}
		else
		    useless = "a constant (undef)";
	    }
	}
	op_null(o);		/* don't execute or even remember it */
	break;

    case OP_POSTINC:
	o->op_type = OP_PREINC;		/* pre-increment is faster */
	o->op_ppaddr = PL_ppaddr[OP_PREINC];
	break;

    case OP_POSTDEC:
	o->op_type = OP_PREDEC;		/* pre-decrement is faster */
	o->op_ppaddr = PL_ppaddr[OP_PREDEC];
	break;

    case OP_I_POSTINC:
	o->op_type = OP_I_PREINC;	/* pre-increment is faster */
	o->op_ppaddr = PL_ppaddr[OP_I_PREINC];
	break;

    case OP_I_POSTDEC:
	o->op_type = OP_I_PREDEC;	/* pre-decrement is faster */
	o->op_ppaddr = PL_ppaddr[OP_I_PREDEC];
	break;

    case OP_SASSIGN: {
	OP *rv2gv;
	UNOP *refgen, *rv2cv;
	LISTOP *exlist;

	if ((o->op_private & ~OPpASSIGN_BACKWARDS) != 2)
	    break;

	rv2gv = ((BINOP *)o)->op_last;
	if (!rv2gv || rv2gv->op_type != OP_RV2GV)
	    break;

	refgen = (UNOP *)((BINOP *)o)->op_first;

	if (!refgen || refgen->op_type != OP_REFGEN)
	    break;

	exlist = (LISTOP *)refgen->op_first;
	if (!exlist || exlist->op_type != OP_NULL
	    || exlist->op_targ != OP_LIST)
	    break;

	if (exlist->op_first->op_type != OP_PUSHMARK)
	    break;

	rv2cv = (UNOP*)exlist->op_last;

	if (rv2cv->op_type != OP_RV2CV)
	    break;

	assert ((rv2gv->op_private & OPpDONT_INIT_GV) == 0);
	assert ((o->op_private & OPpASSIGN_CV_TO_GV) == 0);
	assert ((rv2cv->op_private & OPpMAY_RETURN_CONSTANT) == 0);

	o->op_private |= OPpASSIGN_CV_TO_GV;
	rv2gv->op_private |= OPpDONT_INIT_GV;
	rv2cv->op_private |= OPpMAY_RETURN_CONSTANT;

	break;
    }

    case OP_AASSIGN: {
	inplace_aassign(o);
	break;
    }

    case OP_OR:
    case OP_AND:
	kid = cLOGOPo->op_first;
	if (kid->op_type == OP_NOT
	    && (kid->op_flags & OPf_KIDS)
	    && !PL_madskills) {
	    if (o->op_type == OP_AND) {
		o->op_type = OP_OR;
		o->op_ppaddr = PL_ppaddr[OP_OR];
	    } else {
		o->op_type = OP_AND;
		o->op_ppaddr = PL_ppaddr[OP_AND];
	    }
	    op_null(kid);
	}

    case OP_DOR:
    case OP_COND_EXPR:
    case OP_ENTERGIVEN:
    case OP_ENTERWHEN:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    scalarvoid(kid);
	break;

    case OP_NULL:
	if (o->op_flags & OPf_STACKED)
	    break;
	/* FALL THROUGH */
    case OP_NEXTSTATE:
    case OP_DBSTATE:
    case OP_ENTERTRY:
    case OP_ENTER:
	if (!(o->op_flags & OPf_KIDS))
	    break;
	/* FALL THROUGH */
    case OP_SCOPE:
    case OP_LEAVE:
    case OP_LEAVETRY:
    case OP_LEAVELOOP:
    case OP_LINESEQ:
    case OP_LIST:
    case OP_LEAVEGIVEN:
    case OP_LEAVEWHEN:
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    scalarvoid(kid);
	break;
    case OP_ENTEREVAL:
	scalarkids(o);
	break;
    case OP_SCALAR:
	return scalar(o);
    }

    if (useless_sv) {
        /* mortalise it, in case warnings are fatal.  */
        Perl_ck_warner(aTHX_ packWARN(WARN_VOID),
                       "Useless use of %"SVf" in void context",
                       sv_2mortal(useless_sv));
    }
    else if (useless) {
       Perl_ck_warner(aTHX_ packWARN(WARN_VOID),
                      "Useless use of %s in void context",
                      useless);
    }
    return o;
}

static OP *
S_listkids(pTHX_ OP *o)
{
    if (o && o->op_flags & OPf_KIDS) {
        OP *kid;
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    list(kid);
    }
    return o;
}

OP *
Perl_list(pTHX_ OP *o)
{
    dVAR;
    OP *kid;

    /* assumes no premature commitment */
    if (!o || (o->op_flags & OPf_WANT)
	 || (PL_parser && PL_parser->error_count)
	 || o->op_type == OP_RETURN)
    {
	return o;
    }

    if ((o->op_private & OPpTARGET_MY)
	&& (PL_opargs[o->op_type] & OA_TARGLEX))/* OPp share the meaning */
    {
	return o;				/* As if inside SASSIGN */
    }

    o->op_flags = (o->op_flags & ~OPf_WANT) | OPf_WANT_LIST;

    switch (o->op_type) {
    case OP_FLOP:
    case OP_REPEAT:
	list(cBINOPo->op_first);
	break;
    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    list(kid);
	break;
    default:
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
    case OP_NULL:
	if (!(o->op_flags & OPf_KIDS))
	    break;
	if (!o->op_next && cUNOPo->op_first->op_type == OP_FLOP) {
	    list(cBINOPo->op_first);
	    return gen_constant_list(o);
	}
    case OP_LIST:
	listkids(o);
	break;
    case OP_LEAVE:
    case OP_LEAVETRY:
	kid = cLISTOPo->op_first;
	list(kid);
	kid = kid->op_sibling;
    do_kids:
	while (kid) {
	    OP *sib = kid->op_sibling;
	    if (sib && kid->op_type != OP_LEAVEWHEN)
		scalarvoid(kid);
	    else
		list(kid);
	    kid = sib;
	}
	PL_curcop = &PL_compiling;
	break;
    case OP_SCOPE:
    case OP_LINESEQ:
	kid = cLISTOPo->op_first;
	goto do_kids;
    }
    return o;
}

static OP *
S_scalarseq(pTHX_ OP *o)
{
    dVAR;
    if (o) {
	const OPCODE type = o->op_type;

	if (type == OP_LINESEQ || type == OP_SCOPE ||
	    type == OP_LEAVE || type == OP_LEAVETRY)
	{
            OP *kid;
	    for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling) {
		if (kid->op_sibling) {
		    scalarvoid(kid);
		}
	    }
	    PL_curcop = &PL_compiling;
	}
	o->op_flags &= ~OPf_PARENS;
	if (PL_hints & HINT_BLOCK_SCOPE)
	    o->op_flags |= OPf_PARENS;
    }
    else
	o = newOP(OP_STUB, 0);
    return o;
}

STATIC OP *
S_modkids(pTHX_ OP *o, I32 type)
{
    if (o && o->op_flags & OPf_KIDS) {
        OP *kid;
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    op_lvalue(kid, type);
    }
    return o;
}

/*
=for apidoc finalize_optree

This function finalizes the optree.  Should be called directly after
the complete optree is built.  It does some additional
checking which can't be done in the normal ck_xxx functions and makes
the tree thread-safe.

=cut
*/
void
Perl_finalize_optree(pTHX_ OP* o)
{
    PERL_ARGS_ASSERT_FINALIZE_OPTREE;

    ENTER;
    SAVEVPTR(PL_curcop);

    finalize_op(o);

    LEAVE;
}

STATIC void
S_finalize_op(pTHX_ OP* o)
{
    PERL_ARGS_ASSERT_FINALIZE_OP;

#if defined(PERL_MAD) && defined(USE_ITHREADS)
    {
	/* Make sure mad ops are also thread-safe */
	MADPROP *mp = o->op_madprop;
	while (mp) {
	    if (mp->mad_type == MAD_OP && mp->mad_vlen) {
		OP *prop_op = (OP *) mp->mad_val;
		/* We only need "Relocate sv to the pad for thread safety.", but this
		   easiest way to make sure it traverses everything */
		if (prop_op->op_type == OP_CONST)
		    cSVOPx(prop_op)->op_private &= ~OPpCONST_STRICT;
		finalize_op(prop_op);
	    }
	    mp = mp->mad_next;
	}
    }
#endif

    switch (o->op_type) {
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	PL_curcop = ((COP*)o);		/* for warnings */
	break;
    case OP_EXEC:
	if ( o->op_sibling
	    && (o->op_sibling->op_type == OP_NEXTSTATE || o->op_sibling->op_type == OP_DBSTATE)
	    && ckWARN(WARN_EXEC))
	    {
		if (o->op_sibling->op_sibling) {
		    const OPCODE type = o->op_sibling->op_sibling->op_type;
		    if (type != OP_EXIT && type != OP_WARN && type != OP_DIE) {
			const line_t oldline = CopLINE(PL_curcop);
			CopLINE_set(PL_curcop, CopLINE((COP*)o->op_sibling));
			Perl_warner(aTHX_ packWARN(WARN_EXEC),
			    "Statement unlikely to be reached");
			Perl_warner(aTHX_ packWARN(WARN_EXEC),
			    "\t(Maybe you meant system() when you said exec()?)\n");
			CopLINE_set(PL_curcop, oldline);
		    }
		}
	    }
	break;

    case OP_GV:
	if ((o->op_private & OPpEARLY_CV) && ckWARN(WARN_PROTOTYPE)) {
	    GV * const gv = cGVOPo_gv;
	    if (SvTYPE(gv) == SVt_PVGV && GvCV(gv) && SvPVX_const(GvCV(gv))) {
		/* XXX could check prototype here instead of just carping */
		SV * const sv = sv_newmortal();
		gv_efullname3(sv, gv, NULL);
		Perl_warner(aTHX_ packWARN(WARN_PROTOTYPE),
		    "%"SVf"() called too early to check prototype",
		    SVfARG(sv));
	    }
	}
	break;

    case OP_CONST:
	if (cSVOPo->op_private & OPpCONST_STRICT)
	    no_bareword_allowed(o);
	/* FALLTHROUGH */
#ifdef USE_ITHREADS
    case OP_HINTSEVAL:
    case OP_METHOD_NAMED:
	/* Relocate sv to the pad for thread safety.
	 * Despite being a "constant", the SV is written to,
	 * for reference counts, sv_upgrade() etc. */
	if (cSVOPo->op_sv) {
	    const PADOFFSET ix = pad_alloc(OP_CONST, SVf_READONLY);
	    SvREFCNT_dec(PAD_SVl(ix));
	    PAD_SETSV(ix, cSVOPo->op_sv);
	    /* XXX I don't know how this isn't readonly already. */
	    if (!SvIsCOW(PAD_SVl(ix))) SvREADONLY_on(PAD_SVl(ix));
	    cSVOPo->op_sv = NULL;
	    o->op_targ = ix;
	}
#endif
	break;

    case OP_HELEM: {
	UNOP *rop;
	SV *lexname;
	GV **fields;
	SVOP *key_op;
	OP *kid;
	bool check_fields;

	if ((key_op = cSVOPx(((BINOP*)o)->op_last))->op_type != OP_CONST)
	    break;

	rop = (UNOP*)((BINOP*)o)->op_first;

	goto check_keys;

    case OP_HSLICE:
	S_scalar_slice_warning(aTHX_ o);

    case OP_KVHSLICE:
        kid = cLISTOPo->op_first->op_sibling;
	if (/* I bet there's always a pushmark... */
	    OP_TYPE_ISNT_AND_WASNT_NN(kid, OP_LIST)
	    && OP_TYPE_ISNT_NN(kid, OP_CONST))
        {
	    break;
        }

	key_op = (SVOP*)(kid->op_type == OP_CONST
				? kid
				: kLISTOP->op_first->op_sibling);

	rop = (UNOP*)((LISTOP*)o)->op_last;

      check_keys:	
	if (o->op_private & OPpLVAL_INTRO || rop->op_type != OP_RV2HV)
	    rop = NULL;
	else if (rop->op_first->op_type == OP_PADSV)
	    /* @$hash{qw(keys here)} */
	    rop = (UNOP*)rop->op_first;
	else {
	    /* @{$hash}{qw(keys here)} */
	    if (rop->op_first->op_type == OP_SCOPE
		&& cLISTOPx(rop->op_first)->op_last->op_type == OP_PADSV)
		{
		    rop = (UNOP*)cLISTOPx(rop->op_first)->op_last;
		}
	    else
		rop = NULL;
	}

        lexname = NULL; /* just to silence compiler warnings */
        fields  = NULL; /* just to silence compiler warnings */

	check_fields =
	    rop
	 && (lexname = *av_fetch(PL_comppad_name, rop->op_targ, TRUE),
	     SvPAD_TYPED(lexname))
	 && (fields = (GV**)hv_fetchs(SvSTASH(lexname), "FIELDS", FALSE))
	 && isGV(*fields) && GvHV(*fields);
	for (; key_op;
	     key_op = (SVOP*)key_op->op_sibling) {
	    SV **svp, *sv;
	    if (key_op->op_type != OP_CONST)
		continue;
	    svp = cSVOPx_svp(key_op);

	    /* Make the CONST have a shared SV */
	    if ((!SvIsCOW_shared_hash(sv = *svp))
	     && SvTYPE(sv) < SVt_PVMG && SvOK(sv) && !SvROK(sv)) {
		SSize_t keylen;
		const char * const key = SvPV_const(sv, *(STRLEN*)&keylen);
		SV *nsv = newSVpvn_share(key,
					 SvUTF8(sv) ? -keylen : keylen,	0);
		SvREFCNT_dec_NN(sv);
		*svp = nsv;
	    }

	    if (check_fields
	     && !hv_fetch_ent(GvHV(*fields), *svp, FALSE, 0)) {
		Perl_croak(aTHX_ "No such class field \"%"SVf"\" " 
			   "in variable %"SVf" of type %"HEKf, 
		      SVfARG(*svp), SVfARG(lexname),
                      HEKfARG(HvNAME_HEK(SvSTASH(lexname))));
	    }
	}
	break;
    }
    case OP_ASLICE:
	S_scalar_slice_warning(aTHX_ o);
	break;

    case OP_SUBST: {
	if (cPMOPo->op_pmreplrootu.op_pmreplroot)
	    finalize_op(cPMOPo->op_pmreplrootu.op_pmreplroot);
	break;
    }
    default:
	break;
    }

    if (o->op_flags & OPf_KIDS) {
	OP *kid;
	for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling)
	    finalize_op(kid);
    }
}

/*
=for apidoc Amx|OP *|op_lvalue|OP *o|I32 type

Propagate lvalue ("modifiable") context to an op and its children.
I<type> represents the context type, roughly based on the type of op that
would do the modifying, although C<local()> is represented by OP_NULL,
because it has no op type of its own (it is signalled by a flag on
the lvalue op).

This function detects things that can't be modified, such as C<$x+1>, and
generates errors for them.  For example, C<$x+1 = 2> would cause it to be
called with an op of type OP_ADD and a C<type> argument of OP_SASSIGN.

It also flags things that need to behave specially in an lvalue context,
such as C<$$x = 5> which might have to vivify a reference in C<$x>.

=cut
*/

static bool
S_vivifies(const OPCODE type)
{
    switch(type) {
    case OP_RV2AV:     case   OP_ASLICE:
    case OP_RV2HV:     case OP_KVASLICE:
    case OP_RV2SV:     case   OP_HSLICE:
    case OP_AELEMFAST: case OP_KVHSLICE:
    case OP_HELEM:
    case OP_AELEM:
	return 1;
    }
    return 0;
}

OP *
Perl_op_lvalue_flags(pTHX_ OP *o, I32 type, U32 flags)
{
    dVAR;
    OP *kid;
    /* -1 = error on localize, 0 = ignore localize, 1 = ok to localize */
    int localize = -1;

    if (!o || (PL_parser && PL_parser->error_count))
	return o;

    if ((o->op_private & OPpTARGET_MY)
	&& (PL_opargs[o->op_type] & OA_TARGLEX))/* OPp share the meaning */
    {
	return o;
    }

    assert( (o->op_flags & OPf_WANT) != OPf_WANT_VOID );

    if (type == OP_PRTF || type == OP_SPRINTF) type = OP_ENTERSUB;

    switch (o->op_type) {
    case OP_UNDEF:
	PL_modcount++;
	return o;
    case OP_STUB:
	if ((o->op_flags & OPf_PARENS) || PL_madskills)
	    break;
	goto nomod;
    case OP_ENTERSUB:
	if ((type == OP_UNDEF || type == OP_REFGEN || type == OP_LOCK) &&
	    !(o->op_flags & OPf_STACKED)) {
	    o->op_type = OP_RV2CV;		/* entersub => rv2cv */
	    /* Both ENTERSUB and RV2CV use this bit, but for different pur-
	       poses, so we need it clear.  */
	    o->op_private &= ~1;
	    o->op_ppaddr = PL_ppaddr[OP_RV2CV];
	    assert(cUNOPo->op_first->op_type == OP_NULL);
	    op_null(((LISTOP*)cUNOPo->op_first)->op_first);/* disable pushmark */
	    break;
	}
	else {				/* lvalue subroutine call */
	    o->op_private |= OPpLVAL_INTRO
	                   |(OPpENTERSUB_INARGS * (type == OP_LEAVESUBLV));
	    PL_modcount = RETURN_UNLIMITED_NUMBER;
	    if (type == OP_GREPSTART || type == OP_ENTERSUB || type == OP_REFGEN) {
		/* Potential lvalue context: */
		o->op_private |= OPpENTERSUB_INARGS;
		break;
	    }
	    else {                      /* Compile-time error message: */
		OP *kid = cUNOPo->op_first;
		CV *cv;

		if (kid->op_type != OP_PUSHMARK) {
		    if (kid->op_type != OP_NULL || kid->op_targ != OP_LIST)
			Perl_croak(aTHX_
				"panic: unexpected lvalue entersub "
				"args: type/targ %ld:%"UVuf,
				(long)kid->op_type, (UV)kid->op_targ);
		    kid = kLISTOP->op_first;
		}
		while (kid->op_sibling)
		    kid = kid->op_sibling;
		if (!(kid->op_type == OP_NULL && kid->op_targ == OP_RV2CV)) {
		    break;	/* Postpone until runtime */
		}

		kid = kUNOP->op_first;
		if (kid->op_type == OP_NULL && kid->op_targ == OP_RV2SV)
		    kid = kUNOP->op_first;
		if (kid->op_type == OP_NULL)
		    Perl_croak(aTHX_
			       "Unexpected constant lvalue entersub "
			       "entry via type/targ %ld:%"UVuf,
			       (long)kid->op_type, (UV)kid->op_targ);
		if (kid->op_type != OP_GV) {
		    break;
		}

		cv = GvCV(kGVOP_gv);
		if (!cv)
		    break;
		if (CvLVALUE(cv))
		    break;
	    }
	}
	/* FALL THROUGH */
    default:
      nomod:
	if (flags & OP_LVALUE_NO_CROAK) return NULL;
	/* grep, foreach, subcalls, refgen */
	if (type == OP_GREPSTART || type == OP_ENTERSUB
	 || type == OP_REFGEN    || type == OP_LEAVESUBLV)
	    break;
	yyerror(Perl_form(aTHX_ "Can't modify %s in %s",
		     (o->op_type == OP_NULL && (o->op_flags & OPf_SPECIAL)
		      ? "do block"
		      : (o->op_type == OP_ENTERSUB
			? "non-lvalue subroutine call"
			: OP_DESC(o))),
		     type ? PL_op_desc[type] : "local"));
	return o;

    case OP_PREINC:
    case OP_PREDEC:
    case OP_POW:
    case OP_MULTIPLY:
    case OP_DIVIDE:
    case OP_MODULO:
    case OP_REPEAT:
    case OP_ADD:
    case OP_SUBTRACT:
    case OP_CONCAT:
    case OP_LEFT_SHIFT:
    case OP_RIGHT_SHIFT:
    case OP_BIT_AND:
    case OP_BIT_XOR:
    case OP_BIT_OR:
    case OP_I_MULTIPLY:
    case OP_I_DIVIDE:
    case OP_I_MODULO:
    case OP_I_ADD:
    case OP_I_SUBTRACT:
	if (!(o->op_flags & OPf_STACKED))
	    goto nomod;
	PL_modcount++;
	break;

    case OP_COND_EXPR:
	localize = 1;
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    op_lvalue(kid, type);
	break;

    case OP_RV2AV:
    case OP_RV2HV:
	if (type == OP_REFGEN && o->op_flags & OPf_PARENS) {
           PL_modcount = RETURN_UNLIMITED_NUMBER;
	    return o;		/* Treat \(@foo) like ordinary list. */
	}
	/* FALL THROUGH */
    case OP_RV2GV:
	if (scalar_mod_type(o, type))
	    goto nomod;
	ref(cUNOPo->op_first, o->op_type);
	/* FALL THROUGH */
    case OP_ASLICE:
    case OP_HSLICE:
	localize = 1;
	/* FALL THROUGH */
    case OP_AASSIGN:
	/* Do not apply the lvsub flag for rv2[ah]v in scalar context.  */
	if (type == OP_LEAVESUBLV && (
		(o->op_type != OP_RV2AV && o->op_type != OP_RV2HV)
	     || (o->op_flags & OPf_WANT) != OPf_WANT_SCALAR
	   ))
	    o->op_private |= OPpMAYBE_LVSUB;
	/* FALL THROUGH */
    case OP_NEXTSTATE:
    case OP_DBSTATE:
       PL_modcount = RETURN_UNLIMITED_NUMBER;
	break;
    case OP_KVHSLICE:
    case OP_KVASLICE:
	if (type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
        goto nomod;
    case OP_AV2ARYLEN:
	PL_hints |= HINT_BLOCK_SCOPE;
	if (type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
	PL_modcount++;
	break;
    case OP_RV2SV:
	ref(cUNOPo->op_first, o->op_type);
	localize = 1;
	/* FALL THROUGH */
    case OP_GV:
	PL_hints |= HINT_BLOCK_SCOPE;
    case OP_SASSIGN:
    case OP_ANDASSIGN:
    case OP_ORASSIGN:
    case OP_DORASSIGN:
	PL_modcount++;
	break;

    case OP_AELEMFAST:
    case OP_AELEMFAST_LEX:
	localize = -1;
	PL_modcount++;
	break;

    case OP_PADAV:
    case OP_PADHV:
       PL_modcount = RETURN_UNLIMITED_NUMBER;
	if (type == OP_REFGEN && o->op_flags & OPf_PARENS)
	    return o;		/* Treat \(@foo) like ordinary list. */
	if (scalar_mod_type(o, type))
	    goto nomod;
	if ((o->op_flags & OPf_WANT) != OPf_WANT_SCALAR
	  && type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
	/* FALL THROUGH */
    case OP_PADSV:
	PL_modcount++;
	if (!type) /* local() */
	    Perl_croak(aTHX_ "Can't localize lexical variable %"SVf,
		 PAD_COMPNAME_SV(o->op_targ));
	break;

    case OP_PUSHMARK:
	localize = 0;
	break;

    case OP_KEYS:
    case OP_RKEYS:
	if (type != OP_SASSIGN && type != OP_LEAVESUBLV)
	    goto nomod;
	goto lvalue_func;
    case OP_SUBSTR:
	if (o->op_private == 4) /* don't allow 4 arg substr as lvalue */
	    goto nomod;
	/* FALL THROUGH */
    case OP_POS:
    case OP_VEC:
      lvalue_func:
	if (type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
	if (o->op_flags & OPf_KIDS)
	    op_lvalue(cBINOPo->op_first->op_sibling, type);
	break;

    case OP_AELEM:
    case OP_HELEM:
	ref(cBINOPo->op_first, o->op_type);
	if (type == OP_ENTERSUB &&
	     !(o->op_private & (OPpLVAL_INTRO | OPpDEREF)))
	    o->op_private |= OPpLVAL_DEFER;
	if (type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
	localize = 1;
	PL_modcount++;
	break;

    case OP_LEAVE:
    case OP_LEAVELOOP:
	o->op_private |= OPpLVALUE;
    case OP_SCOPE:
    case OP_ENTER:
    case OP_LINESEQ:
	localize = 0;
	if (o->op_flags & OPf_KIDS)
	    op_lvalue(cLISTOPo->op_last, type);
	break;

    case OP_NULL:
	localize = 0;
	if (o->op_flags & OPf_SPECIAL)		/* do BLOCK */
	    goto nomod;
	else if (!(o->op_flags & OPf_KIDS))
	    break;
	if (o->op_targ != OP_LIST) {
	    op_lvalue(cBINOPo->op_first, type);
	    break;
	}
	/* FALL THROUGH */
    case OP_LIST:
	localize = 0;
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    /* elements might be in void context because the list is
	       in scalar context or because they are attribute sub calls */
	    if ( (kid->op_flags & OPf_WANT) != OPf_WANT_VOID )
		op_lvalue(kid, type);
	break;

    case OP_RETURN:
	if (type != OP_LEAVESUBLV)
	    goto nomod;
	break; /* op_lvalue()ing was handled by ck_return() */

    case OP_COREARGS:
	return o;

    case OP_AND:
    case OP_OR:
	if (type == OP_LEAVESUBLV
	 || !S_vivifies(cLOGOPo->op_first->op_type))
	    op_lvalue(cLOGOPo->op_first, type);
	if (type == OP_LEAVESUBLV
	 || !S_vivifies(cLOGOPo->op_first->op_sibling->op_type))
	    op_lvalue(cLOGOPo->op_first->op_sibling, type);
	goto nomod;
    }

    /* [20011101.069] File test operators interpret OPf_REF to mean that
       their argument is a filehandle; thus \stat(".") should not set
       it. AMS 20011102 */
    if (type == OP_REFGEN &&
        PL_check[o->op_type] == Perl_ck_ftst)
        return o;

    if (type != OP_LEAVESUBLV)
        o->op_flags |= OPf_MOD;

    if (type == OP_AASSIGN || type == OP_SASSIGN)
	o->op_flags |= OPf_SPECIAL|OPf_REF;
    else if (!type) { /* local() */
	switch (localize) {
	case 1:
	    o->op_private |= OPpLVAL_INTRO;
	    o->op_flags &= ~OPf_SPECIAL;
	    PL_hints |= HINT_BLOCK_SCOPE;
	    break;
	case 0:
	    break;
	case -1:
	    Perl_ck_warner(aTHX_ packWARN(WARN_SYNTAX),
			   "Useless localization of %s", OP_DESC(o));
	}
    }
    else if (type != OP_GREPSTART && type != OP_ENTERSUB
             && type != OP_LEAVESUBLV)
	o->op_flags |= OPf_REF;
    return o;
}

STATIC bool
S_scalar_mod_type(const OP *o, I32 type)
{
    switch (type) {
    case OP_POS:
    case OP_SASSIGN:
	if (o && o->op_type == OP_RV2GV)
	    return FALSE;
	/* FALL THROUGH */
    case OP_PREINC:
    case OP_PREDEC:
    case OP_POSTINC:
    case OP_POSTDEC:
    case OP_I_PREINC:
    case OP_I_PREDEC:
    case OP_I_POSTINC:
    case OP_I_POSTDEC:
    case OP_POW:
    case OP_MULTIPLY:
    case OP_DIVIDE:
    case OP_MODULO:
    case OP_REPEAT:
    case OP_ADD:
    case OP_SUBTRACT:
    case OP_I_MULTIPLY:
    case OP_I_DIVIDE:
    case OP_I_MODULO:
    case OP_I_ADD:
    case OP_I_SUBTRACT:
    case OP_LEFT_SHIFT:
    case OP_RIGHT_SHIFT:
    case OP_BIT_AND:
    case OP_BIT_XOR:
    case OP_BIT_OR:
    case OP_CONCAT:
    case OP_SUBST:
    case OP_TRANS:
    case OP_TRANSR:
    case OP_READ:
    case OP_SYSREAD:
    case OP_RECV:
    case OP_ANDASSIGN:
    case OP_ORASSIGN:
    case OP_DORASSIGN:
	return TRUE;
    default:
	return FALSE;
    }
}

STATIC bool
S_is_handle_constructor(const OP *o, I32 numargs)
{
    PERL_ARGS_ASSERT_IS_HANDLE_CONSTRUCTOR;

    switch (o->op_type) {
    case OP_PIPE_OP:
    case OP_SOCKPAIR:
	if (numargs == 2)
	    return TRUE;
	/* FALL THROUGH */
    case OP_SYSOPEN:
    case OP_OPEN:
    case OP_SELECT:		/* XXX c.f. SelectSaver.pm */
    case OP_SOCKET:
    case OP_OPEN_DIR:
    case OP_ACCEPT:
	if (numargs == 1)
	    return TRUE;
	/* FALLTHROUGH */
    default:
	return FALSE;
    }
}

static OP *
S_refkids(pTHX_ OP *o, I32 type)
{
    if (o && o->op_flags & OPf_KIDS) {
        OP *kid;
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    ref(kid, type);
    }
    return o;
}

OP *
Perl_doref(pTHX_ OP *o, I32 type, bool set_op_ref)
{
    dVAR;
    OP *kid;

    PERL_ARGS_ASSERT_DOREF;

    if (!o || (PL_parser && PL_parser->error_count))
	return o;

    switch (o->op_type) {
    case OP_ENTERSUB:
	if ((type == OP_EXISTS || type == OP_DEFINED) &&
	    !(o->op_flags & OPf_STACKED)) {
	    o->op_type = OP_RV2CV;             /* entersub => rv2cv */
	    o->op_ppaddr = PL_ppaddr[OP_RV2CV];
	    assert(cUNOPo->op_first->op_type == OP_NULL);
	    op_null(((LISTOP*)cUNOPo->op_first)->op_first);	/* disable pushmark */
	    o->op_flags |= OPf_SPECIAL;
	    o->op_private &= ~1;
	}
	else if (type == OP_RV2SV || type == OP_RV2AV || type == OP_RV2HV){
	    o->op_private |= (type == OP_RV2AV ? OPpDEREF_AV
			      : type == OP_RV2HV ? OPpDEREF_HV
			      : OPpDEREF_SV);
	    o->op_flags |= OPf_MOD;
	}

	break;

    case OP_COND_EXPR:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    doref(kid, type, set_op_ref);
	break;
    case OP_RV2SV:
	if (type == OP_DEFINED)
	    o->op_flags |= OPf_SPECIAL;		/* don't create GV */
	doref(cUNOPo->op_first, o->op_type, set_op_ref);
	/* FALL THROUGH */
    case OP_PADSV:
	if (type == OP_RV2SV || type == OP_RV2AV || type == OP_RV2HV) {
	    o->op_private |= (type == OP_RV2AV ? OPpDEREF_AV
			      : type == OP_RV2HV ? OPpDEREF_HV
			      : OPpDEREF_SV);
	    o->op_flags |= OPf_MOD;
	}
	break;

    case OP_RV2AV:
    case OP_RV2HV:
	if (set_op_ref)
	    o->op_flags |= OPf_REF;
	/* FALL THROUGH */
    case OP_RV2GV:
	if (type == OP_DEFINED)
	    o->op_flags |= OPf_SPECIAL;		/* don't create GV */
	doref(cUNOPo->op_first, o->op_type, set_op_ref);
	break;

    case OP_PADAV:
    case OP_PADHV:
	if (set_op_ref)
	    o->op_flags |= OPf_REF;
	break;

    case OP_SCALAR:
    case OP_NULL:
	if (!(o->op_flags & OPf_KIDS) || type == OP_DEFINED)
	    break;
	doref(cBINOPo->op_first, type, set_op_ref);
	break;
    case OP_AELEM:
    case OP_HELEM:
	doref(cBINOPo->op_first, o->op_type, set_op_ref);
	if (type == OP_RV2SV || type == OP_RV2AV || type == OP_RV2HV) {
	    o->op_private |= (type == OP_RV2AV ? OPpDEREF_AV
			      : type == OP_RV2HV ? OPpDEREF_HV
			      : OPpDEREF_SV);
	    o->op_flags |= OPf_MOD;
	}
	break;

    case OP_SCOPE:
    case OP_LEAVE:
	set_op_ref = FALSE;
	/* FALL THROUGH */
    case OP_ENTER:
    case OP_LIST:
	if (!(o->op_flags & OPf_KIDS))
	    break;
	doref(cLISTOPo->op_last, type, set_op_ref);
	break;
    default:
	break;
    }
    return scalar(o);

}

STATIC OP *
S_dup_attrlist(pTHX_ OP *o)
{
    dVAR;
    OP *rop;

    PERL_ARGS_ASSERT_DUP_ATTRLIST;

    /* An attrlist is either a simple OP_CONST or an OP_LIST with kids,
     * where the first kid is OP_PUSHMARK and the remaining ones
     * are OP_CONST.  We need to push the OP_CONST values.
     */
    if (o->op_type == OP_CONST)
	rop = newSVOP(OP_CONST, o->op_flags, SvREFCNT_inc_NN(cSVOPo->op_sv));
#ifdef PERL_MAD
    else if (o->op_type == OP_NULL)
	rop = NULL;
#endif
    else {
	assert((o->op_type == OP_LIST) && (o->op_flags & OPf_KIDS));
	rop = NULL;
	for (o = cLISTOPo->op_first; o; o=o->op_sibling) {
	    if (o->op_type == OP_CONST)
		rop = op_append_elem(OP_LIST, rop,
				  newSVOP(OP_CONST, o->op_flags,
					  SvREFCNT_inc_NN(cSVOPo->op_sv)));
	}
    }
    return rop;
}

STATIC void
S_apply_attrs(pTHX_ HV *stash, SV *target, OP *attrs)
{
    dVAR;
    SV * const stashsv = stash ? newSVhek(HvNAME_HEK(stash)) : &PL_sv_no;

    PERL_ARGS_ASSERT_APPLY_ATTRS;

    /* fake up C<use attributes $pkg,$rv,@attrs> */

#define ATTRSMODULE "attributes"
#define ATTRSMODULE_PM "attributes.pm"

    Perl_load_module(aTHX_ PERL_LOADMOD_IMPORT_OPS,
			 newSVpvs(ATTRSMODULE),
			 NULL,
			 op_prepend_elem(OP_LIST,
				      newSVOP(OP_CONST, 0, stashsv),
				      op_prepend_elem(OP_LIST,
						   newSVOP(OP_CONST, 0,
							   newRV(target)),
						   dup_attrlist(attrs))));
}

STATIC void
S_apply_attrs_my(pTHX_ HV *stash, OP *target, OP *attrs, OP **imopsp)
{
    dVAR;
    OP *pack, *imop, *arg;
    SV *meth, *stashsv, **svp;

    PERL_ARGS_ASSERT_APPLY_ATTRS_MY;

    if (!attrs)
	return;

    assert(target->op_type == OP_PADSV ||
	   target->op_type == OP_PADHV ||
	   target->op_type == OP_PADAV);

    /* Ensure that attributes.pm is loaded. */
    /* Don't force the C<use> if we don't need it. */
    svp = hv_fetchs(GvHVn(PL_incgv), ATTRSMODULE_PM, FALSE);
    if (svp && *svp != &PL_sv_undef)
	NOOP;	/* already in %INC */
    else
	Perl_load_module(aTHX_ PERL_LOADMOD_NOIMPORT,
			       newSVpvs(ATTRSMODULE), NULL);

    /* Need package name for method call. */
    pack = newSVOP(OP_CONST, 0, newSVpvs(ATTRSMODULE));

    /* Build up the real arg-list. */
    stashsv = stash ? newSVhek(HvNAME_HEK(stash)) : &PL_sv_no;

    arg = newOP(OP_PADSV, 0);
    arg->op_targ = target->op_targ;
    arg = op_prepend_elem(OP_LIST,
		       newSVOP(OP_CONST, 0, stashsv),
		       op_prepend_elem(OP_LIST,
				    newUNOP(OP_REFGEN, 0,
					    op_lvalue(arg, OP_REFGEN)),
				    dup_attrlist(attrs)));

    /* Fake up a method call to import */
    meth = newSVpvs_share("import");
    imop = convert(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL|OPf_WANT_VOID,
		   op_append_elem(OP_LIST,
			       op_prepend_elem(OP_LIST, pack, list(arg)),
			       newSVOP(OP_METHOD_NAMED, 0, meth)));

    /* Combine the ops. */
    *imopsp = op_append_elem(OP_LIST, *imopsp, imop);
}

/*
=notfor apidoc apply_attrs_string

Attempts to apply a list of attributes specified by the C<attrstr> and
C<len> arguments to the subroutine identified by the C<cv> argument which
is expected to be associated with the package identified by the C<stashpv>
argument (see L<attributes>).  It gets this wrong, though, in that it
does not correctly identify the boundaries of the individual attribute
specifications within C<attrstr>.  This is not really intended for the
public API, but has to be listed here for systems such as AIX which
need an explicit export list for symbols.  (It's called from XS code
in support of the C<ATTRS:> keyword from F<xsubpp>.)  Patches to fix it
to respect attribute syntax properly would be welcome.

=cut
*/

void
Perl_apply_attrs_string(pTHX_ const char *stashpv, CV *cv,
                        const char *attrstr, STRLEN len)
{
    OP *attrs = NULL;

    PERL_ARGS_ASSERT_APPLY_ATTRS_STRING;

    if (!len) {
        len = strlen(attrstr);
    }

    while (len) {
        for (; isSPACE(*attrstr) && len; --len, ++attrstr) ;
        if (len) {
            const char * const sstr = attrstr;
            for (; !isSPACE(*attrstr) && len; --len, ++attrstr) ;
            attrs = op_append_elem(OP_LIST, attrs,
                                newSVOP(OP_CONST, 0,
                                        newSVpvn(sstr, attrstr-sstr)));
        }
    }

    Perl_load_module(aTHX_ PERL_LOADMOD_IMPORT_OPS,
		     newSVpvs(ATTRSMODULE),
                     NULL, op_prepend_elem(OP_LIST,
				  newSVOP(OP_CONST, 0, newSVpv(stashpv,0)),
				  op_prepend_elem(OP_LIST,
					       newSVOP(OP_CONST, 0,
						       newRV(MUTABLE_SV(cv))),
                                               attrs)));
}

STATIC void
S_move_proto_attr(pTHX_ OP **proto, OP **attrs, const GV * name)
{
    OP *new_proto = NULL;
    STRLEN pvlen;
    char *pv;
    OP *o;

    PERL_ARGS_ASSERT_MOVE_PROTO_ATTR;

    if (!*attrs)
        return;

    o = *attrs;
    if (o->op_type == OP_CONST) {
        pv = SvPV(cSVOPo_sv, pvlen);
        if (pvlen >= 10 && memEQ(pv, "prototype(", 10)) {
            SV * const tmpsv = newSVpvn_flags(pv + 10, pvlen - 11, SvUTF8(cSVOPo_sv));
            SV ** const tmpo = cSVOPx_svp(o);
            SvREFCNT_dec(cSVOPo_sv);
            *tmpo = tmpsv;
            new_proto = o;
            *attrs = NULL;
        }
    } else if (o->op_type == OP_LIST) {
        OP * lasto = NULL;
        assert(o->op_flags & OPf_KIDS);
        assert(cLISTOPo->op_first->op_type == OP_PUSHMARK);
        /* Counting on the first op to hit the lasto = o line */
        for (o = cLISTOPo->op_first; o; o=o->op_sibling) {
            if (o->op_type == OP_CONST) {
                pv = SvPV(cSVOPo_sv, pvlen);
                if (pvlen >= 10 && memEQ(pv, "prototype(", 10)) {
                    SV * const tmpsv = newSVpvn_flags(pv + 10, pvlen - 11, SvUTF8(cSVOPo_sv));
                    SV ** const tmpo = cSVOPx_svp(o);
                    SvREFCNT_dec(cSVOPo_sv);
                    *tmpo = tmpsv;
                    if (new_proto && ckWARN(WARN_MISC)) {
                        STRLEN new_len;
                        const char * newp = SvPV(cSVOPo_sv, new_len);
                        Perl_warner(aTHX_ packWARN(WARN_MISC),
                            "Attribute prototype(%"UTF8f") discards earlier prototype attribute in same sub",
                            UTF8fARG(SvUTF8(cSVOPo_sv), new_len, newp));
                        op_free(new_proto);
                    }
                    else if (new_proto)
                        op_free(new_proto);
                    new_proto = o;
                    lasto->op_sibling = o->op_sibling;
                    continue;
                }
            }
            lasto = o;
        }
        /* If the list is now just the PUSHMARK, scrap the whole thing; otherwise attributes.xs
           would get pulled in with no real need */
        if (!cLISTOPx(*attrs)->op_first->op_sibling) {
            op_free(*attrs);
            *attrs = NULL;
        }
    }

    if (new_proto) {
        SV *svname;
        if (isGV(name)) {
            svname = sv_newmortal();
            gv_efullname3(svname, name, NULL);
        }
        else if (SvPOK(name) && *SvPVX((SV *)name) == '&')
            svname = newSVpvn_flags(SvPVX((SV *)name)+1, SvCUR(name)-1, SvUTF8(name)|SVs_TEMP);
        else
            svname = (SV *)name;
        if (ckWARN(WARN_ILLEGALPROTO))
            (void)validate_proto(svname, cSVOPx_sv(new_proto), TRUE);
        if (*proto && ckWARN(WARN_PROTOTYPE)) {
            STRLEN old_len, new_len;
            const char * oldp = SvPV(cSVOPx_sv(*proto), old_len);
            const char * newp = SvPV(cSVOPx_sv(new_proto), new_len);

            Perl_warner(aTHX_ packWARN(WARN_PROTOTYPE),
                "Prototype '%"UTF8f"' overridden by attribute 'prototype(%"UTF8f")'"
                " in %"SVf,
                UTF8fARG(SvUTF8(cSVOPx_sv(*proto)), old_len, oldp),
                UTF8fARG(SvUTF8(cSVOPx_sv(new_proto)), new_len, newp),
                SVfARG(svname));
        }
        if (*proto)
            op_free(*proto);
        *proto = new_proto;
    }
}

static void
S_cant_declare(pTHX_ OP *o)
{
    if (o->op_type == OP_NULL
     && (o->op_flags & (OPf_SPECIAL|OPf_KIDS)) == OPf_KIDS)
        o = cUNOPo->op_first;
    yyerror(Perl_form(aTHX_ "Can't declare %s in \"%s\"",
                             o->op_type == OP_NULL
                               && o->op_flags & OPf_SPECIAL
                                 ? "do block"
                                 : OP_DESC(o),
                             PL_parser->in_my == KEY_our   ? "our"   :
                             PL_parser->in_my == KEY_state ? "state" :
                                                             "my"));
}

STATIC OP *
S_my_kid(pTHX_ OP *o, OP *attrs, OP **imopsp)
{
    dVAR;
    I32 type;
    const bool stately = PL_parser && PL_parser->in_my == KEY_state;

    PERL_ARGS_ASSERT_MY_KID;

    if (!o || (PL_parser && PL_parser->error_count))
	return o;

    type = o->op_type;
    if (PL_madskills && type == OP_NULL && o->op_flags & OPf_KIDS) {
	(void)my_kid(cUNOPo->op_first, attrs, imopsp);
	return o;
    }

    if (type == OP_LIST) {
        OP *kid;
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    my_kid(kid, attrs, imopsp);
	return o;
    } else if (type == OP_UNDEF || type == OP_STUB) {
	return o;
    } else if (type == OP_RV2SV ||	/* "our" declaration */
	       type == OP_RV2AV ||
	       type == OP_RV2HV) { /* XXX does this let anything illegal in? */
	if (cUNOPo->op_first->op_type != OP_GV) { /* MJD 20011224 */
	    S_cant_declare(aTHX_ o);
	} else if (attrs) {
	    GV * const gv = cGVOPx_gv(cUNOPo->op_first);
	    PL_parser->in_my = FALSE;
	    PL_parser->in_my_stash = NULL;
	    apply_attrs(GvSTASH(gv),
			(type == OP_RV2SV ? GvSV(gv) :
			 type == OP_RV2AV ? MUTABLE_SV(GvAV(gv)) :
			 type == OP_RV2HV ? MUTABLE_SV(GvHV(gv)) : MUTABLE_SV(gv)),
			attrs);
	}
	o->op_private |= OPpOUR_INTRO;
	return o;
    }
    else if (type != OP_PADSV &&
	     type != OP_PADAV &&
	     type != OP_PADHV &&
	     type != OP_PUSHMARK)
    {
	S_cant_declare(aTHX_ o);
	return o;
    }
    else if (attrs && type != OP_PUSHMARK) {
	HV *stash;

	PL_parser->in_my = FALSE;
	PL_parser->in_my_stash = NULL;

	/* check for C<my Dog $spot> when deciding package */
	stash = PAD_COMPNAME_TYPE(o->op_targ);
	if (!stash)
	    stash = PL_curstash;
	apply_attrs_my(stash, o, attrs, imopsp);
    }
    o->op_flags |= OPf_MOD;
    o->op_private |= OPpLVAL_INTRO;
    if (stately)
	o->op_private |= OPpPAD_STATE;
    return o;
}

OP *
Perl_my_attrs(pTHX_ OP *o, OP *attrs)
{
    dVAR;
    OP *rops;
    int maybe_scalar = 0;

    PERL_ARGS_ASSERT_MY_ATTRS;

/* [perl #17376]: this appears to be premature, and results in code such as
   C< our(%x); > executing in list mode rather than void mode */
#if 0
    if (o->op_flags & OPf_PARENS)
	list(o);
    else
	maybe_scalar = 1;
#else
    maybe_scalar = 1;
#endif
    if (attrs)
	SAVEFREEOP(attrs);
    rops = NULL;
    o = my_kid(o, attrs, &rops);
    if (rops) {
	if (maybe_scalar && o->op_type == OP_PADSV) {
	    o = scalar(op_append_list(OP_LIST, rops, o));
	    o->op_private |= OPpLVAL_INTRO;
	}
	else {
	    /* The listop in rops might have a pushmark at the beginning,
	       which will mess up list assignment. */
	    LISTOP * const lrops = (LISTOP *)rops; /* for brevity */
	    if (rops->op_type == OP_LIST && 
	        lrops->op_first && lrops->op_first->op_type == OP_PUSHMARK)
	    {
		OP * const pushmark = lrops->op_first;
		lrops->op_first = pushmark->op_sibling;
		op_free(pushmark);
	    }
	    o = op_append_list(OP_LIST, o, rops);
	}
    }
    PL_parser->in_my = FALSE;
    PL_parser->in_my_stash = NULL;
    return o;
}

OP *
Perl_sawparens(pTHX_ OP *o)
{
    PERL_UNUSED_CONTEXT;
    if (o)
	o->op_flags |= OPf_PARENS;
    return o;
}

OP *
Perl_bind_match(pTHX_ I32 type, OP *left, OP *right)
{
    OP *o;
    bool ismatchop = 0;
    const OPCODE ltype = left->op_type;
    const OPCODE rtype = right->op_type;

    PERL_ARGS_ASSERT_BIND_MATCH;

    if ( (ltype == OP_RV2AV || ltype == OP_RV2HV || ltype == OP_PADAV
	  || ltype == OP_PADHV) && ckWARN(WARN_MISC))
    {
      const char * const desc
	  = PL_op_desc[(
		          rtype == OP_SUBST || rtype == OP_TRANS
		       || rtype == OP_TRANSR
		       )
		       ? (int)rtype : OP_MATCH];
      const bool isary = ltype == OP_RV2AV || ltype == OP_PADAV;
      SV * const name =
	S_op_varname(aTHX_ left);
      if (name)
	Perl_warner(aTHX_ packWARN(WARN_MISC),
             "Applying %s to %"SVf" will act on scalar(%"SVf")",
             desc, name, name);
      else {
	const char * const sample = (isary
	     ? "@array" : "%hash");
	Perl_warner(aTHX_ packWARN(WARN_MISC),
             "Applying %s to %s will act on scalar(%s)",
             desc, sample, sample);
      }
    }

    if (rtype == OP_CONST &&
	cSVOPx(right)->op_private & OPpCONST_BARE &&
	cSVOPx(right)->op_private & OPpCONST_STRICT)
    {
	no_bareword_allowed(right);
    }

    /* !~ doesn't make sense with /r, so error on it for now */
    if (rtype == OP_SUBST && (cPMOPx(right)->op_pmflags & PMf_NONDESTRUCT) &&
	type == OP_NOT)
	/* diag_listed_as: Using !~ with %s doesn't make sense */
	yyerror("Using !~ with s///r doesn't make sense");
    if (rtype == OP_TRANSR && type == OP_NOT)
	/* diag_listed_as: Using !~ with %s doesn't make sense */
	yyerror("Using !~ with tr///r doesn't make sense");

    ismatchop = (rtype == OP_MATCH ||
		 rtype == OP_SUBST ||
		 rtype == OP_TRANS || rtype == OP_TRANSR)
	     && !(right->op_flags & OPf_SPECIAL);
    if (ismatchop && right->op_private & OPpTARGET_MY) {
	right->op_targ = 0;
	right->op_private &= ~OPpTARGET_MY;
    }
    if (!(right->op_flags & OPf_STACKED) && ismatchop) {
	OP *newleft;

	right->op_flags |= OPf_STACKED;
	if (rtype != OP_MATCH && rtype != OP_TRANSR &&
            ! (rtype == OP_TRANS &&
               right->op_private & OPpTRANS_IDENTICAL) &&
	    ! (rtype == OP_SUBST &&
	       (cPMOPx(right)->op_pmflags & PMf_NONDESTRUCT)))
	    newleft = op_lvalue(left, rtype);
	else
	    newleft = left;
	if (right->op_type == OP_TRANS || right->op_type == OP_TRANSR)
	    o = newBINOP(OP_NULL, OPf_STACKED, scalar(newleft), right);
	else
	    o = op_prepend_elem(rtype, scalar(newleft), right);
	if (type == OP_NOT)
	    return newUNOP(OP_NOT, 0, scalar(o));
	return o;
    }
    else
	return bind_match(type, left,
		pmruntime(newPMOP(OP_MATCH, 0), right, 0, 0));
}

OP *
Perl_invert(pTHX_ OP *o)
{
    if (!o)
	return NULL;
    return newUNOP(OP_NOT, OPf_SPECIAL, scalar(o));
}

/*
=for apidoc Amx|OP *|op_scope|OP *o

Wraps up an op tree with some additional ops so that at runtime a dynamic
scope will be created.  The original ops run in the new dynamic scope,
and then, provided that they exit normally, the scope will be unwound.
The additional ops used to create and unwind the dynamic scope will
normally be an C<enter>/C<leave> pair, but a C<scope> op may be used
instead if the ops are simple enough to not need the full dynamic scope
structure.

=cut
*/

OP *
Perl_op_scope(pTHX_ OP *o)
{
    dVAR;
    if (o) {
	if (o->op_flags & OPf_PARENS || PERLDB_NOOPT || TAINTING_get) {
	    o = op_prepend_elem(OP_LINESEQ, newOP(OP_ENTER, 0), o);
	    o->op_type = OP_LEAVE;
	    o->op_ppaddr = PL_ppaddr[OP_LEAVE];
	}
	else if (o->op_type == OP_LINESEQ) {
	    OP *kid;
	    o->op_type = OP_SCOPE;
	    o->op_ppaddr = PL_ppaddr[OP_SCOPE];
	    kid = ((LISTOP*)o)->op_first;
	    if (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE) {
		op_null(kid);

		/* The following deals with things like 'do {1 for 1}' */
		kid = kid->op_sibling;
		if (kid &&
		    (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE))
		    op_null(kid);
	    }
	}
	else
	    o = newLISTOP(OP_SCOPE, 0, o, NULL);
    }
    return o;
}

OP *
Perl_op_unscope(pTHX_ OP *o)
{
    if (o && o->op_type == OP_LINESEQ) {
	OP *kid = cLISTOPo->op_first;
	for(; kid; kid = kid->op_sibling)
	    if (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE)
		op_null(kid);
    }
    return o;
}

int
Perl_block_start(pTHX_ int full)
{
    dVAR;
    const int retval = PL_savestack_ix;

    pad_block_start(full);
    SAVEHINTS();
    PL_hints &= ~HINT_BLOCK_SCOPE;
    SAVECOMPILEWARNINGS();
    PL_compiling.cop_warnings = DUP_WARNINGS(PL_compiling.cop_warnings);

    CALL_BLOCK_HOOKS(bhk_start, full);

    return retval;
}

OP*
Perl_block_end(pTHX_ I32 floor, OP *seq)
{
    dVAR;
    const int needblockscope = PL_hints & HINT_BLOCK_SCOPE;
    OP* retval = scalarseq(seq);
    OP *o;

    CALL_BLOCK_HOOKS(bhk_pre_end, &retval);

    LEAVE_SCOPE(floor);
    if (needblockscope)
	PL_hints |= HINT_BLOCK_SCOPE; /* propagate out */
    o = pad_leavemy();

    if (o) {
	/* pad_leavemy has created a sequence of introcv ops for all my
	   subs declared in the block.  We have to replicate that list with
	   clonecv ops, to deal with this situation:

	       sub {
		   my sub s1;
		   my sub s2;
		   sub s1 { state sub foo { \&s2 } }
	       }->()

	   Originally, I was going to have introcv clone the CV and turn
	   off the stale flag.  Since &s1 is declared before &s2, the
	   introcv op for &s1 is executed (on sub entry) before the one for
	   &s2.  But the &foo sub inside &s1 (which is cloned when &s1 is
	   cloned, since it is a state sub) closes over &s2 and expects
	   to see it in its outer CV’s pad.  If the introcv op clones &s1,
	   then &s2 is still marked stale.  Since &s1 is not active, and
	   &foo closes over &s1’s implicit entry for &s2, we get a ‘Varia-
	   ble will not stay shared’ warning.  Because it is the same stub
	   that will be used when the introcv op for &s2 is executed, clos-
	   ing over it is safe.  Hence, we have to turn off the stale flag
	   on all lexical subs in the block before we clone any of them.
	   Hence, having introcv clone the sub cannot work.  So we create a
	   list of ops like this:

	       lineseq
		  |
		  +-- introcv
		  |
		  +-- introcv
		  |
		  +-- introcv
		  |
		  .
		  .
		  .
		  |
		  +-- clonecv
		  |
		  +-- clonecv
		  |
		  +-- clonecv
		  |
		  .
		  .
		  .
	 */
	OP *kid = o->op_flags & OPf_KIDS ? cLISTOPo->op_first : o;
	OP * const last = o->op_flags & OPf_KIDS ? cLISTOPo->op_last : o;
	for (;; kid = kid->op_sibling) {
	    OP *newkid = newOP(OP_CLONECV, 0);
	    newkid->op_targ = kid->op_targ;
	    o = op_append_elem(OP_LINESEQ, o, newkid);
	    if (kid == last) break;
	}
	retval = op_prepend_elem(OP_LINESEQ, o, retval);
    }

    CALL_BLOCK_HOOKS(bhk_post_end, &retval);

    return retval;
}

/*
=head1 Compile-time scope hooks

=for apidoc Aox||blockhook_register

Register a set of hooks to be called when the Perl lexical scope changes
at compile time.  See L<perlguts/"Compile-time scope hooks">.

=cut
*/

void
Perl_blockhook_register(pTHX_ BHK *hk)
{
    PERL_ARGS_ASSERT_BLOCKHOOK_REGISTER;

    Perl_av_create_and_push(aTHX_ &PL_blockhooks, newSViv(PTR2IV(hk)));
}

STATIC OP *
S_newDEFSVOP(pTHX)
{
    dVAR;
    const PADOFFSET offset = pad_findmy_pvs("$_", 0);
    if (offset == NOT_IN_PAD || PAD_COMPNAME_FLAGS_isOUR(offset)) {
	return newSVREF(newGVOP(OP_GV, 0, PL_defgv));
    }
    else {
	OP * const o = newOP(OP_PADSV, 0);
	o->op_targ = offset;
	return o;
    }
}

void
Perl_newPROG(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_NEWPROG;

    if (PL_in_eval) {
	PERL_CONTEXT *cx;
	I32 i;
	if (PL_eval_root)
		return;
	PL_eval_root = newUNOP(OP_LEAVEEVAL,
			       ((PL_in_eval & EVAL_KEEPERR)
				? OPf_SPECIAL : 0), o);

	cx = &cxstack[cxstack_ix];
	assert(CxTYPE(cx) == CXt_EVAL);

	if ((cx->blk_gimme & G_WANT) == G_VOID)
	    scalarvoid(PL_eval_root);
	else if ((cx->blk_gimme & G_WANT) == G_ARRAY)
	    list(PL_eval_root);
	else
	    scalar(PL_eval_root);

	PL_eval_start = op_linklist(PL_eval_root);
	PL_eval_root->op_private |= OPpREFCOUNTED;
	OpREFCNT_set(PL_eval_root, 1);
	PL_eval_root->op_next = 0;
	i = PL_savestack_ix;
	SAVEFREEOP(o);
	ENTER;
	CALL_PEEP(PL_eval_start);
	finalize_optree(PL_eval_root);
        S_prune_chain_head(aTHX_ &PL_eval_start);
	LEAVE;
	PL_savestack_ix = i;
    }
    else {
	if (o->op_type == OP_STUB) {
            /* This block is entered if nothing is compiled for the main
               program. This will be the case for an genuinely empty main
               program, or one which only has BEGIN blocks etc, so already
               run and freed.

               Historically (5.000) the guard above was !o. However, commit
               f8a08f7b8bd67b28 (Jun 2001), integrated to blead as
               c71fccf11fde0068, changed perly.y so that newPROG() is now
               called with the output of block_end(), which returns a new
               OP_STUB for the case of an empty optree. ByteLoader (and
               maybe other things) also take this path, because they set up
               PL_main_start and PL_main_root directly, without generating an
               optree.

               If the parsing the main program aborts (due to parse errors,
               or due to BEGIN or similar calling exit), then newPROG()
               isn't even called, and hence this code path and its cleanups
               are skipped. This shouldn't make a make a difference:
               * a non-zero return from perl_parse is a failure, and
                 perl_destruct() should be called immediately.
               * however, if exit(0) is called during the parse, then
                 perl_parse() returns 0, and perl_run() is called. As
                 PL_main_start will be NULL, perl_run() will return
                 promptly, and the exit code will remain 0.
            */

	    PL_comppad_name = 0;
	    PL_compcv = 0;
	    S_op_destroy(aTHX_ o);
	    return;
	}
	PL_main_root = op_scope(sawparens(scalarvoid(o)));
	PL_curcop = &PL_compiling;
	PL_main_start = LINKLIST(PL_main_root);
	PL_main_root->op_private |= OPpREFCOUNTED;
	OpREFCNT_set(PL_main_root, 1);
	PL_main_root->op_next = 0;
	CALL_PEEP(PL_main_start);
	finalize_optree(PL_main_root);
        S_prune_chain_head(aTHX_ &PL_main_start);
	cv_forget_slab(PL_compcv);
	PL_compcv = 0;

	/* Register with debugger */
	if (PERLDB_INTER) {
	    CV * const cv = get_cvs("DB::postponed", 0);
	    if (cv) {
		dSP;
		PUSHMARK(SP);
		XPUSHs(MUTABLE_SV(CopFILEGV(&PL_compiling)));
		PUTBACK;
		call_sv(MUTABLE_SV(cv), G_DISCARD);
	    }
	}
    }
}

OP *
Perl_localize(pTHX_ OP *o, I32 lex)
{
    dVAR;

    PERL_ARGS_ASSERT_LOCALIZE;

    if (o->op_flags & OPf_PARENS)
/* [perl #17376]: this appears to be premature, and results in code such as
   C< our(%x); > executing in list mode rather than void mode */
#if 0
	list(o);
#else
	NOOP;
#endif
    else {
	if ( PL_parser->bufptr > PL_parser->oldbufptr
	    && PL_parser->bufptr[-1] == ','
	    && ckWARN(WARN_PARENTHESIS))
	{
	    char *s = PL_parser->bufptr;
	    bool sigil = FALSE;

	    /* some heuristics to detect a potential error */
	    while (*s && (strchr(", \t\n", *s)))
		s++;

	    while (1) {
		if (*s && strchr("@$%*", *s) && *++s
		       && (isWORDCHAR(*s) || UTF8_IS_CONTINUED(*s))) {
		    s++;
		    sigil = TRUE;
		    while (*s && (isWORDCHAR(*s) || UTF8_IS_CONTINUED(*s)))
			s++;
		    while (*s && (strchr(", \t\n", *s)))
			s++;
		}
		else
		    break;
	    }
	    if (sigil && (*s == ';' || *s == '=')) {
		Perl_warner(aTHX_ packWARN(WARN_PARENTHESIS),
				"Parentheses missing around \"%s\" list",
				lex
				    ? (PL_parser->in_my == KEY_our
					? "our"
					: PL_parser->in_my == KEY_state
					    ? "state"
					    : "my")
				    : "local");
	    }
	}
    }
    if (lex)
	o = my(o);
    else
	o = op_lvalue(o, OP_NULL);		/* a bit kludgey */
    PL_parser->in_my = FALSE;
    PL_parser->in_my_stash = NULL;
    return o;
}

OP *
Perl_jmaybe(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_JMAYBE;

    if (o->op_type == OP_LIST) {
	OP * const o2
	    = newSVREF(newGVOP(OP_GV, 0, gv_fetchpvs(";", GV_ADD|GV_NOTQUAL, SVt_PV)));
	o = convert(OP_JOIN, 0, op_prepend_elem(OP_LIST, o2, o));
    }
    return o;
}

PERL_STATIC_INLINE OP *
S_op_std_init(pTHX_ OP *o)
{
    I32 type = o->op_type;

    PERL_ARGS_ASSERT_OP_STD_INIT;

    if (PL_opargs[type] & OA_RETSCALAR)
	scalar(o);
    if (PL_opargs[type] & OA_TARGET && !o->op_targ)
	o->op_targ = pad_alloc(type, SVs_PADTMP);

    return o;
}

PERL_STATIC_INLINE OP *
S_op_integerize(pTHX_ OP *o)
{
    I32 type = o->op_type;

    PERL_ARGS_ASSERT_OP_INTEGERIZE;

    /* integerize op. */
    if ((PL_opargs[type] & OA_OTHERINT) && (PL_hints & HINT_INTEGER))
    {
	dVAR;
	o->op_ppaddr = PL_ppaddr[++(o->op_type)];
    }

    if (type == OP_NEGATE)
	/* XXX might want a ck_negate() for this */
	cUNOPo->op_first->op_private &= ~OPpCONST_STRICT;

    return o;
}

static OP *
S_fold_constants(pTHX_ OP *o)
{
    dVAR;
    OP * VOL curop;
    OP *newop;
    VOL I32 type = o->op_type;
    SV * VOL sv = NULL;
    int ret = 0;
    I32 oldscope;
    OP *old_next;
    SV * const oldwarnhook = PL_warnhook;
    SV * const olddiehook  = PL_diehook;
    COP not_compiling;
    dJMPENV;

    PERL_ARGS_ASSERT_FOLD_CONSTANTS;

    if (!(PL_opargs[type] & OA_FOLDCONST))
	goto nope;

    switch (type) {
    case OP_UCFIRST:
    case OP_LCFIRST:
    case OP_UC:
    case OP_LC:
    case OP_FC:
    case OP_SLT:
    case OP_SGT:
    case OP_SLE:
    case OP_SGE:
    case OP_SCMP:
    case OP_SPRINTF:
	/* XXX what about the numeric ops? */
	if (IN_LOCALE_COMPILETIME)
	    goto nope;
	break;
    case OP_PACK:
	if (!cLISTOPo->op_first->op_sibling
	  || cLISTOPo->op_first->op_sibling->op_type != OP_CONST)
	    goto nope;
	{
	    SV * const sv = cSVOPx_sv(cLISTOPo->op_first->op_sibling);
	    if (!SvPOK(sv) || SvGMAGICAL(sv)) goto nope;
	    {
		const char *s = SvPVX_const(sv);
		while (s < SvEND(sv)) {
		    if (*s == 'p' || *s == 'P') goto nope;
		    s++;
		}
	    }
	}
	break;
    case OP_REPEAT:
	if (o->op_private & OPpREPEAT_DOLIST) goto nope;
	break;
    case OP_SREFGEN:
	if (cUNOPx(cUNOPo->op_first)->op_first->op_type != OP_CONST
	 || SvPADTMP(cSVOPx_sv(cUNOPx(cUNOPo->op_first)->op_first)))
	    goto nope;
    }

    if (PL_parser && PL_parser->error_count)
	goto nope;		/* Don't try to run w/ errors */

    for (curop = LINKLIST(o); curop != o; curop = LINKLIST(curop)) {
	const OPCODE type = curop->op_type;
	if ((type != OP_CONST || (curop->op_private & OPpCONST_BARE)) &&
	    type != OP_LIST &&
	    type != OP_SCALAR &&
	    type != OP_NULL &&
	    type != OP_PUSHMARK)
	{
	    goto nope;
	}
    }

    curop = LINKLIST(o);
    old_next = o->op_next;
    o->op_next = 0;
    PL_op = curop;

    oldscope = PL_scopestack_ix;
    create_eval_scope(G_FAKINGEVAL);

    /* Verify that we don't need to save it:  */
    assert(PL_curcop == &PL_compiling);
    StructCopy(&PL_compiling, &not_compiling, COP);
    PL_curcop = &not_compiling;
    /* The above ensures that we run with all the correct hints of the
       currently compiling COP, but that IN_PERL_RUNTIME is not true. */
    assert(IN_PERL_RUNTIME);
    PL_warnhook = PERL_WARNHOOK_FATAL;
    PL_diehook  = NULL;
    JMPENV_PUSH(ret);

    switch (ret) {
    case 0:
	CALLRUNOPS(aTHX);
	sv = *(PL_stack_sp--);
	if (o->op_targ && sv == PAD_SV(o->op_targ)) {	/* grab pad temp? */
#ifdef PERL_MAD
	    /* Can't simply swipe the SV from the pad, because that relies on
	       the op being freed "real soon now". Under MAD, this doesn't
	       happen (see the #ifdef below).  */
	    sv = newSVsv(sv);
#else
	    pad_swipe(o->op_targ,  FALSE);
#endif
	}
	else if (SvTEMP(sv)) {			/* grab mortal temp? */
	    SvREFCNT_inc_simple_void(sv);
	    SvTEMP_off(sv);
	}
	else { assert(SvIMMORTAL(sv)); }
	break;
    case 3:
	/* Something tried to die.  Abandon constant folding.  */
	/* Pretend the error never happened.  */
	CLEAR_ERRSV();
	o->op_next = old_next;
	break;
    default:
	JMPENV_POP;
	/* Don't expect 1 (setjmp failed) or 2 (something called my_exit)  */
	PL_warnhook = oldwarnhook;
	PL_diehook  = olddiehook;
	/* XXX note that this croak may fail as we've already blown away
	 * the stack - eg any nested evals */
	Perl_croak(aTHX_ "panic: fold_constants JMPENV_PUSH returned %d", ret);
    }
    JMPENV_POP;
    PL_warnhook = oldwarnhook;
    PL_diehook  = olddiehook;
    PL_curcop = &PL_compiling;

    if (PL_scopestack_ix > oldscope)
	delete_eval_scope();

    if (ret)
	goto nope;

#ifndef PERL_MAD
    op_free(o);
#endif
    assert(sv);
    if (type == OP_STRINGIFY) SvPADTMP_off(sv);
    else if (!SvIMMORTAL(sv)) {
	SvPADTMP_on(sv);
	SvREADONLY_on(sv);
    }
    if (type == OP_RV2GV)
	newop = newGVOP(OP_GV, 0, MUTABLE_GV(sv));
    else
    {
	newop = newSVOP(OP_CONST, 0, MUTABLE_SV(sv));
	if (type != OP_STRINGIFY) newop->op_folded = 1;
    }
    op_getmad(o,newop,'f');
    return newop;

 nope:
    return o;
}

static OP *
S_gen_constant_list(pTHX_ OP *o)
{
    dVAR;
    OP *curop;
    const SSize_t oldtmps_floor = PL_tmps_floor;
    SV **svp;
    AV *av;

    list(o);
    if (PL_parser && PL_parser->error_count)
	return o;		/* Don't attempt to run with errors */

    curop = LINKLIST(o);
    o->op_next = 0;
    CALL_PEEP(curop);
    S_prune_chain_head(aTHX_ &curop);
    PL_op = curop;
    Perl_pp_pushmark(aTHX);
    CALLRUNOPS(aTHX);
    PL_op = curop;
    assert (!(curop->op_flags & OPf_SPECIAL));
    assert(curop->op_type == OP_RANGE);
    Perl_pp_anonlist(aTHX);
    PL_tmps_floor = oldtmps_floor;

    o->op_type = OP_RV2AV;
    o->op_ppaddr = PL_ppaddr[OP_RV2AV];
    o->op_flags &= ~OPf_REF;	/* treat \(1..2) like an ordinary list */
    o->op_flags |= OPf_PARENS;	/* and flatten \(1..2,3) */
    o->op_opt = 0;		/* needs to be revisited in rpeep() */
    curop = ((UNOP*)o)->op_first;
    av = (AV *)SvREFCNT_inc_NN(*PL_stack_sp--);
    ((UNOP*)o)->op_first = newSVOP(OP_CONST, 0, (SV *)av);
    if (AvFILLp(av) != -1)
	for (svp = AvARRAY(av) + AvFILLp(av); svp >= AvARRAY(av); --svp)
	{
	    SvPADTMP_on(*svp);
	    SvREADONLY_on(*svp);
	}
#ifdef PERL_MAD
    op_getmad(curop,o,'O');
#else
    op_free(curop);
#endif
    LINKLIST(o);
    return list(o);
}

OP *
Perl_convert(pTHX_ I32 type, I32 flags, OP *o)
{
    dVAR;
    if (type < 0) type = -type, flags |= OPf_SPECIAL;
    if (!o || o->op_type != OP_LIST)
	o = newLISTOP(OP_LIST, 0, o, NULL);
    else
	o->op_flags &= ~OPf_WANT;

    if (!(PL_opargs[type] & OA_MARK))
	op_null(cLISTOPo->op_first);
    else {
	OP * const kid2 = cLISTOPo->op_first->op_sibling;
	if (kid2 && kid2->op_type == OP_COREARGS) {
	    op_null(cLISTOPo->op_first);
	    kid2->op_private |= OPpCOREARGS_PUSHMARK;
	}
    }	

    o->op_type = (OPCODE)type;
    o->op_ppaddr = PL_ppaddr[type];
    o->op_flags |= flags;

    o = CHECKOP(type, o);
    if (o->op_type != (unsigned)type)
	return o;

    return fold_constants(op_integerize(op_std_init(o)));
}

/*
=head1 Optree Manipulation Functions
*/

/* List constructors */

/*
=for apidoc Am|OP *|op_append_elem|I32 optype|OP *first|OP *last

Append an item to the list of ops contained directly within a list-type
op, returning the lengthened list.  I<first> is the list-type op,
and I<last> is the op to append to the list.  I<optype> specifies the
intended opcode for the list.  If I<first> is not already a list of the
right type, it will be upgraded into one.  If either I<first> or I<last>
is null, the other is returned unchanged.

=cut
*/

OP *
Perl_op_append_elem(pTHX_ I32 type, OP *first, OP *last)
{
    if (!first)
	return last;

    if (!last)
	return first;

    if (first->op_type != (unsigned)type
	|| (type == OP_LIST && (first->op_flags & OPf_PARENS)))
    {
	return newLISTOP(type, 0, first, last);
    }

    if (first->op_flags & OPf_KIDS)
	((LISTOP*)first)->op_last->op_sibling = last;
    else {
	first->op_flags |= OPf_KIDS;
	((LISTOP*)first)->op_first = last;
    }
    ((LISTOP*)first)->op_last = last;
    return first;
}

/*
=for apidoc Am|OP *|op_append_list|I32 optype|OP *first|OP *last

Concatenate the lists of ops contained directly within two list-type ops,
returning the combined list.  I<first> and I<last> are the list-type ops
to concatenate.  I<optype> specifies the intended opcode for the list.
If either I<first> or I<last> is not already a list of the right type,
it will be upgraded into one.  If either I<first> or I<last> is null,
the other is returned unchanged.

=cut
*/

OP *
Perl_op_append_list(pTHX_ I32 type, OP *first, OP *last)
{
    if (!first)
	return last;

    if (!last)
	return first;

    if (first->op_type != (unsigned)type)
	return op_prepend_elem(type, first, last);

    if (last->op_type != (unsigned)type)
	return op_append_elem(type, first, last);

    ((LISTOP*)first)->op_last->op_sibling = ((LISTOP*)last)->op_first;
    ((LISTOP*)first)->op_last = ((LISTOP*)last)->op_last;
    first->op_flags |= (last->op_flags & OPf_KIDS);

#ifdef PERL_MAD
    if (((LISTOP*)last)->op_first && first->op_madprop) {
	MADPROP *mp = ((LISTOP*)last)->op_first->op_madprop;
	if (mp) {
	    while (mp->mad_next)
		mp = mp->mad_next;
	    mp->mad_next = first->op_madprop;
	}
	else {
	    ((LISTOP*)last)->op_first->op_madprop = first->op_madprop;
	}
    }
    first->op_madprop = last->op_madprop;
    last->op_madprop = 0;
#endif

    S_op_destroy(aTHX_ last);

    return first;
}

/*
=for apidoc Am|OP *|op_prepend_elem|I32 optype|OP *first|OP *last

Prepend an item to the list of ops contained directly within a list-type
op, returning the lengthened list.  I<first> is the op to prepend to the
list, and I<last> is the list-type op.  I<optype> specifies the intended
opcode for the list.  If I<last> is not already a list of the right type,
it will be upgraded into one.  If either I<first> or I<last> is null,
the other is returned unchanged.

=cut
*/

OP *
Perl_op_prepend_elem(pTHX_ I32 type, OP *first, OP *last)
{
    if (!first)
	return last;

    if (!last)
	return first;

    if (last->op_type == (unsigned)type) {
	if (type == OP_LIST) {	/* already a PUSHMARK there */
	    first->op_sibling = ((LISTOP*)last)->op_first->op_sibling;
	    ((LISTOP*)last)->op_first->op_sibling = first;
            if (!(first->op_flags & OPf_PARENS))
                last->op_flags &= ~OPf_PARENS;
	}
	else {
	    if (!(last->op_flags & OPf_KIDS)) {
		((LISTOP*)last)->op_last = first;
		last->op_flags |= OPf_KIDS;
	    }
	    first->op_sibling = ((LISTOP*)last)->op_first;
	    ((LISTOP*)last)->op_first = first;
	}
	last->op_flags |= OPf_KIDS;
	return last;
    }

    return newLISTOP(type, 0, first, last);
}

/* Constructors */

#ifdef PERL_MAD
 
TOKEN *
Perl_newTOKEN(pTHX_ I32 optype, YYSTYPE lval, MADPROP* madprop)
{
    TOKEN *tk;
    Newxz(tk, 1, TOKEN);
    tk->tk_type = (OPCODE)optype;
    tk->tk_type = 12345;
    tk->tk_lval = lval;
    tk->tk_mad = madprop;
    return tk;
}

void
Perl_token_free(pTHX_ TOKEN* tk)
{
    PERL_ARGS_ASSERT_TOKEN_FREE;

    if (tk->tk_type != 12345)
	return;
    mad_free(tk->tk_mad);
    Safefree(tk);
}

void
Perl_token_getmad(pTHX_ TOKEN* tk, OP* o, char slot)
{
    MADPROP* mp;
    MADPROP* tm;

    PERL_ARGS_ASSERT_TOKEN_GETMAD;

    if (tk->tk_type != 12345) {
	Perl_warner(aTHX_ packWARN(WARN_MISC),
	     "Invalid TOKEN object ignored");
	return;
    }
    tm = tk->tk_mad;
    if (!tm)
	return;

    /* faked up qw list? */
    if (slot == '(' &&
	tm->mad_type == MAD_SV &&
	SvPVX((SV *)tm->mad_val)[0] == 'q')
	    slot = 'x';

    if (o) {
	mp = o->op_madprop;
	if (mp) {
	    for (;;) {
		/* pretend constant fold didn't happen? */
		if (mp->mad_key == 'f' &&
		    (o->op_type == OP_CONST ||
		     o->op_type == OP_GV) )
		{
		    token_getmad(tk,(OP*)mp->mad_val,slot);
		    return;
		}
		if (!mp->mad_next)
		    break;
		mp = mp->mad_next;
	    }
	    mp->mad_next = tm;
	    mp = mp->mad_next;
	}
	else {
	    o->op_madprop = tm;
	    mp = o->op_madprop;
	}
	if (mp->mad_key == 'X')
	    mp->mad_key = slot;	/* just change the first one */

	tk->tk_mad = 0;
    }
    else
	mad_free(tm);
    Safefree(tk);
}

void
Perl_op_getmad_weak(pTHX_ OP* from, OP* o, char slot)
{
    MADPROP* mp;
    if (!from)
	return;
    if (o) {
	mp = o->op_madprop;
	if (mp) {
	    for (;;) {
		/* pretend constant fold didn't happen? */
		if (mp->mad_key == 'f' &&
		    (o->op_type == OP_CONST ||
		     o->op_type == OP_GV) )
		{
		    op_getmad(from,(OP*)mp->mad_val,slot);
		    return;
		}
		if (!mp->mad_next)
		    break;
		mp = mp->mad_next;
	    }
	    mp->mad_next = newMADPROP(slot,MAD_OP,from,0);
	}
	else {
	    o->op_madprop = newMADPROP(slot,MAD_OP,from,0);
	}
    }
}

void
Perl_op_getmad(pTHX_ OP* from, OP* o, char slot)
{
    MADPROP* mp;
    if (!from)
	return;
    if (o) {
	mp = o->op_madprop;
	if (mp) {
	    for (;;) {
		/* pretend constant fold didn't happen? */
		if (mp->mad_key == 'f' &&
		    (o->op_type == OP_CONST ||
		     o->op_type == OP_GV) )
		{
		    op_getmad(from,(OP*)mp->mad_val,slot);
		    return;
		}
		if (!mp->mad_next)
		    break;
		mp = mp->mad_next;
	    }
	    mp->mad_next = newMADPROP(slot,MAD_OP,from,1);
	}
	else {
	    o->op_madprop = newMADPROP(slot,MAD_OP,from,1);
	}
    }
    else {
	PerlIO_printf(PerlIO_stderr(),
		      "DESTROYING op = %0"UVxf"\n", PTR2UV(from));
	op_free(from);
    }
}

void
Perl_prepend_madprops(pTHX_ MADPROP* mp, OP* o, char slot)
{
    MADPROP* tm;
    if (!mp || !o)
	return;
    if (slot)
	mp->mad_key = slot;
    tm = o->op_madprop;
    o->op_madprop = mp;
    for (;;) {
	if (!mp->mad_next)
	    break;
	mp = mp->mad_next;
    }
    mp->mad_next = tm;
}

void
Perl_append_madprops(pTHX_ MADPROP* tm, OP* o, char slot)
{
    if (!o)
	return;
    addmad(tm, &(o->op_madprop), slot);
}

void
Perl_addmad(pTHX_ MADPROP* tm, MADPROP** root, char slot)
{
    MADPROP* mp;
    if (!tm || !root)
	return;
    if (slot)
	tm->mad_key = slot;
    mp = *root;
    if (!mp) {
	*root = tm;
	return;
    }
    for (;;) {
	if (!mp->mad_next)
	    break;
	mp = mp->mad_next;
    }
    mp->mad_next = tm;
}

MADPROP *
Perl_newMADsv(pTHX_ char key, SV* sv)
{
    PERL_ARGS_ASSERT_NEWMADSV;

    return newMADPROP(key, MAD_SV, sv, 0);
}

MADPROP *
Perl_newMADPROP(pTHX_ char key, char type, void* val, I32 vlen)
{
    MADPROP *const mp = (MADPROP *) PerlMemShared_malloc(sizeof(MADPROP));
    mp->mad_next = 0;
    mp->mad_key = key;
    mp->mad_vlen = vlen;
    mp->mad_type = type;
    mp->mad_val = val;
/*    PerlIO_printf(PerlIO_stderr(), "NEW  mp = %0x\n", mp);  */
    return mp;
}

void
Perl_mad_free(pTHX_ MADPROP* mp)
{
/*    PerlIO_printf(PerlIO_stderr(), "FREE mp = %0x\n", mp); */
    if (!mp)
	return;
    if (mp->mad_next)
	mad_free(mp->mad_next);
/*    if (PL_parser && PL_parser->lex_state != LEX_NOTPARSING && mp->mad_vlen)
	PerlIO_printf(PerlIO_stderr(), "DESTROYING '%c'=<%s>\n", mp->mad_key & 255, mp->mad_val); */
    switch (mp->mad_type) {
    case MAD_NULL:
	break;
    case MAD_PV:
	Safefree(mp->mad_val);
	break;
    case MAD_OP:
	if (mp->mad_vlen)	/* vlen holds "strong/weak" boolean */
	    op_free((OP*)mp->mad_val);
	break;
    case MAD_SV:
	sv_free(MUTABLE_SV(mp->mad_val));
	break;
    default:
	PerlIO_printf(PerlIO_stderr(), "Unrecognized mad\n");
	break;
    }
    PerlMemShared_free(mp);
}

#endif

/*
=head1 Optree construction

=for apidoc Am|OP *|newNULLLIST

Constructs, checks, and returns a new C<stub> op, which represents an
empty list expression.

=cut
*/

OP *
Perl_newNULLLIST(pTHX)
{
    return newOP(OP_STUB, 0);
}

static OP *
S_force_list(pTHX_ OP *o)
{
    if (!o || o->op_type != OP_LIST)
	o = newLISTOP(OP_LIST, 0, o, NULL);
    op_null(o);
    return o;
}

/*
=for apidoc Am|OP *|newLISTOP|I32 type|I32 flags|OP *first|OP *last

Constructs, checks, and returns an op of any list type.  I<type> is
the opcode.  I<flags> gives the eight bits of C<op_flags>, except that
C<OPf_KIDS> will be set automatically if required.  I<first> and I<last>
supply up to two ops to be direct children of the list op; they are
consumed by this function and become part of the constructed op tree.

=cut
*/

OP *
Perl_newLISTOP(pTHX_ I32 type, I32 flags, OP *first, OP *last)
{
    dVAR;
    LISTOP *listop;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_LISTOP);

    NewOp(1101, listop, 1, LISTOP);

    listop->op_type = (OPCODE)type;
    listop->op_ppaddr = PL_ppaddr[type];
    if (first || last)
	flags |= OPf_KIDS;
    listop->op_flags = (U8)flags;

    if (!last && first)
	last = first;
    else if (!first && last)
	first = last;
    else if (first)
	first->op_sibling = last;
    listop->op_first = first;
    listop->op_last = last;
    if (type == OP_LIST) {
	OP* const pushop = newOP(OP_PUSHMARK, 0);
	pushop->op_sibling = first;
	listop->op_first = pushop;
	listop->op_flags |= OPf_KIDS;
	if (!last)
	    listop->op_last = pushop;
    }

    return CHECKOP(type, listop);
}

/*
=for apidoc Am|OP *|newOP|I32 type|I32 flags

Constructs, checks, and returns an op of any base type (any type that
has no extra fields).  I<type> is the opcode.  I<flags> gives the
eight bits of C<op_flags>, and, shifted up eight bits, the eight bits
of C<op_private>.

=cut
*/

OP *
Perl_newOP(pTHX_ I32 type, I32 flags)
{
    dVAR;
    OP *o;

    if (type == -OP_ENTEREVAL) {
	type = OP_ENTEREVAL;
	flags |= OPpEVAL_BYTES<<8;
    }

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_BASEOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_BASEOP_OR_UNOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_FILESTATOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_LOOPEXOP);

    NewOp(1101, o, 1, OP);
    o->op_type = (OPCODE)type;
    o->op_ppaddr = PL_ppaddr[type];
    o->op_flags = (U8)flags;

    o->op_next = o;
    o->op_private = (U8)(0 | (flags >> 8));
    if (PL_opargs[type] & OA_RETSCALAR)
	scalar(o);
    if (PL_opargs[type] & OA_TARGET)
	o->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, o);
}

/*
=for apidoc Am|OP *|newUNOP|I32 type|I32 flags|OP *first

Constructs, checks, and returns an op of any unary type.  I<type> is
the opcode.  I<flags> gives the eight bits of C<op_flags>, except that
C<OPf_KIDS> will be set automatically if required, and, shifted up eight
bits, the eight bits of C<op_private>, except that the bit with value 1
is automatically set.  I<first> supplies an optional op to be the direct
child of the unary op; it is consumed by this function and become part
of the constructed op tree.

=cut
*/

OP *
Perl_newUNOP(pTHX_ I32 type, I32 flags, OP *first)
{
    dVAR;
    UNOP *unop;

    if (type == -OP_ENTEREVAL) {
	type = OP_ENTEREVAL;
	flags |= OPpEVAL_BYTES<<8;
    }

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_UNOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_BASEOP_OR_UNOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_FILESTATOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_LOOPEXOP
	|| type == OP_SASSIGN
	|| type == OP_ENTERTRY
	|| type == OP_NULL );

    if (!first)
	first = newOP(OP_STUB, 0);
    if (PL_opargs[type] & OA_MARK)
	first = force_list(first);

    NewOp(1101, unop, 1, UNOP);
    unop->op_type = (OPCODE)type;
    unop->op_ppaddr = PL_ppaddr[type];
    unop->op_first = first;
    unop->op_flags = (U8)(flags | OPf_KIDS);
    unop->op_private = (U8)(1 | (flags >> 8));
    unop = (UNOP*) CHECKOP(type, unop);
    if (unop->op_next)
	return (OP*)unop;

    return fold_constants(op_integerize(op_std_init((OP *) unop)));
}

/*
=for apidoc Am|OP *|newBINOP|I32 type|I32 flags|OP *first|OP *last

Constructs, checks, and returns an op of any binary type.  I<type>
is the opcode.  I<flags> gives the eight bits of C<op_flags>, except
that C<OPf_KIDS> will be set automatically, and, shifted up eight bits,
the eight bits of C<op_private>, except that the bit with value 1 or
2 is automatically set as required.  I<first> and I<last> supply up to
two ops to be the direct children of the binary op; they are consumed
by this function and become part of the constructed op tree.

=cut
*/

OP *
Perl_newBINOP(pTHX_ I32 type, I32 flags, OP *first, OP *last)
{
    dVAR;
    BINOP *binop;

    ASSUME((PL_opargs[type] & OA_CLASS_MASK) == OA_BINOP
	|| type == OP_SASSIGN || type == OP_NULL );

    NewOp(1101, binop, 1, BINOP);

    if (!first)
	first = newOP(OP_NULL, 0);

    binop->op_type = (OPCODE)type;
    binop->op_ppaddr = PL_ppaddr[type];
    binop->op_first = first;
    binop->op_flags = (U8)(flags | OPf_KIDS);
    if (!last) {
	last = first;
	binop->op_private = (U8)(1 | (flags >> 8));
    }
    else {
	binop->op_private = (U8)(2 | (flags >> 8));
	first->op_sibling = last;
    }

    binop = (BINOP*)CHECKOP(type, binop);
    if (binop->op_next || binop->op_type != (OPCODE)type)
	return (OP*)binop;

    binop->op_last = binop->op_first->op_sibling;

    return fold_constants(op_integerize(op_std_init((OP *)binop)));
}

static int uvcompare(const void *a, const void *b)
    __attribute__nonnull__(1)
    __attribute__nonnull__(2)
    __attribute__pure__;
static int uvcompare(const void *a, const void *b)
{
    if (*((const UV *)a) < (*(const UV *)b))
	return -1;
    if (*((const UV *)a) > (*(const UV *)b))
	return 1;
    if (*((const UV *)a+1) < (*(const UV *)b+1))
	return -1;
    if (*((const UV *)a+1) > (*(const UV *)b+1))
	return 1;
    return 0;
}

static OP *
S_pmtrans(pTHX_ OP *o, OP *expr, OP *repl)
{
    dVAR;
    SV * const tstr = ((SVOP*)expr)->op_sv;
    SV * const rstr =
#ifdef PERL_MAD
			(repl->op_type == OP_NULL)
			    ? ((SVOP*)((LISTOP*)repl)->op_first)->op_sv :
#endif
			      ((SVOP*)repl)->op_sv;
    STRLEN tlen;
    STRLEN rlen;
    const U8 *t = (U8*)SvPV_const(tstr, tlen);
    const U8 *r = (U8*)SvPV_const(rstr, rlen);
    I32 i;
    I32 j;
    I32 grows = 0;
    short *tbl;

    const I32 complement = o->op_private & OPpTRANS_COMPLEMENT;
    const I32 squash     = o->op_private & OPpTRANS_SQUASH;
    I32 del              = o->op_private & OPpTRANS_DELETE;
    SV* swash;

    PERL_ARGS_ASSERT_PMTRANS;

    PL_hints |= HINT_BLOCK_SCOPE;

    if (SvUTF8(tstr))
        o->op_private |= OPpTRANS_FROM_UTF;

    if (SvUTF8(rstr))
        o->op_private |= OPpTRANS_TO_UTF;

    if (o->op_private & (OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF)) {
	SV* const listsv = newSVpvs("# comment\n");
	SV* transv = NULL;
	const U8* tend = t + tlen;
	const U8* rend = r + rlen;
	STRLEN ulen;
	UV tfirst = 1;
	UV tlast = 0;
	IV tdiff;
	UV rfirst = 1;
	UV rlast = 0;
	IV rdiff;
	IV diff;
	I32 none = 0;
	U32 max = 0;
	I32 bits;
	I32 havefinal = 0;
	U32 final = 0;
	const I32 from_utf  = o->op_private & OPpTRANS_FROM_UTF;
	const I32 to_utf    = o->op_private & OPpTRANS_TO_UTF;
	U8* tsave = NULL;
	U8* rsave = NULL;
	const U32 flags = UTF8_ALLOW_DEFAULT;

	if (!from_utf) {
	    STRLEN len = tlen;
	    t = tsave = bytes_to_utf8(t, &len);
	    tend = t + len;
	}
	if (!to_utf && rlen) {
	    STRLEN len = rlen;
	    r = rsave = bytes_to_utf8(r, &len);
	    rend = r + len;
	}

/* There is a  snag with this code on EBCDIC: scan_const() in toke.c has
 * encoded chars in native encoding which makes ranges in the EBCDIC 0..255
 * odd.  */

	if (complement) {
	    U8 tmpbuf[UTF8_MAXBYTES+1];
	    UV *cp;
	    UV nextmin = 0;
	    Newx(cp, 2*tlen, UV);
	    i = 0;
	    transv = newSVpvs("");
	    while (t < tend) {
		cp[2*i] = utf8n_to_uvchr(t, tend-t, &ulen, flags);
		t += ulen;
		if (t < tend && *t == ILLEGAL_UTF8_BYTE) {
		    t++;
		    cp[2*i+1] = utf8n_to_uvchr(t, tend-t, &ulen, flags);
		    t += ulen;
		}
		else {
		 cp[2*i+1] = cp[2*i];
		}
		i++;
	    }
	    qsort(cp, i, 2*sizeof(UV), uvcompare);
	    for (j = 0; j < i; j++) {
		UV  val = cp[2*j];
		diff = val - nextmin;
		if (diff > 0) {
		    t = uvchr_to_utf8(tmpbuf,nextmin);
		    sv_catpvn(transv, (char*)tmpbuf, t - tmpbuf);
		    if (diff > 1) {
			U8  range_mark = ILLEGAL_UTF8_BYTE;
			t = uvchr_to_utf8(tmpbuf, val - 1);
			sv_catpvn(transv, (char *)&range_mark, 1);
			sv_catpvn(transv, (char*)tmpbuf, t - tmpbuf);
		    }
	        }
		val = cp[2*j+1];
		if (val >= nextmin)
		    nextmin = val + 1;
	    }
	    t = uvchr_to_utf8(tmpbuf,nextmin);
	    sv_catpvn(transv, (char*)tmpbuf, t - tmpbuf);
	    {
		U8 range_mark = ILLEGAL_UTF8_BYTE;
		sv_catpvn(transv, (char *)&range_mark, 1);
	    }
	    t = uvchr_to_utf8(tmpbuf, 0x7fffffff);
	    sv_catpvn(transv, (char*)tmpbuf, t - tmpbuf);
	    t = (const U8*)SvPVX_const(transv);
	    tlen = SvCUR(transv);
	    tend = t + tlen;
	    Safefree(cp);
	}
	else if (!rlen && !del) {
	    r = t; rlen = tlen; rend = tend;
	}
	if (!squash) {
		if ((!rlen && !del) || t == r ||
		    (tlen == rlen && memEQ((char *)t, (char *)r, tlen)))
		{
		    o->op_private |= OPpTRANS_IDENTICAL;
		}
	}

	while (t < tend || tfirst <= tlast) {
	    /* see if we need more "t" chars */
	    if (tfirst > tlast) {
		tfirst = (I32)utf8n_to_uvchr(t, tend - t, &ulen, flags);
		t += ulen;
		if (t < tend && *t == ILLEGAL_UTF8_BYTE) {	/* illegal utf8 val indicates range */
		    t++;
		    tlast = (I32)utf8n_to_uvchr(t, tend - t, &ulen, flags);
		    t += ulen;
		}
		else
		    tlast = tfirst;
	    }

	    /* now see if we need more "r" chars */
	    if (rfirst > rlast) {
		if (r < rend) {
		    rfirst = (I32)utf8n_to_uvchr(r, rend - r, &ulen, flags);
		    r += ulen;
		    if (r < rend && *r == ILLEGAL_UTF8_BYTE) {	/* illegal utf8 val indicates range */
			r++;
			rlast = (I32)utf8n_to_uvchr(r, rend - r, &ulen, flags);
			r += ulen;
		    }
		    else
			rlast = rfirst;
		}
		else {
		    if (!havefinal++)
			final = rlast;
		    rfirst = rlast = 0xffffffff;
		}
	    }

	    /* now see which range will peter our first, if either. */
	    tdiff = tlast - tfirst;
	    rdiff = rlast - rfirst;

	    if (tdiff <= rdiff)
		diff = tdiff;
	    else
		diff = rdiff;

	    if (rfirst == 0xffffffff) {
		diff = tdiff;	/* oops, pretend rdiff is infinite */
		if (diff > 0)
		    Perl_sv_catpvf(aTHX_ listsv, "%04lx\t%04lx\tXXXX\n",
				   (long)tfirst, (long)tlast);
		else
		    Perl_sv_catpvf(aTHX_ listsv, "%04lx\t\tXXXX\n", (long)tfirst);
	    }
	    else {
		if (diff > 0)
		    Perl_sv_catpvf(aTHX_ listsv, "%04lx\t%04lx\t%04lx\n",
				   (long)tfirst, (long)(tfirst + diff),
				   (long)rfirst);
		else
		    Perl_sv_catpvf(aTHX_ listsv, "%04lx\t\t%04lx\n",
				   (long)tfirst, (long)rfirst);

		if (rfirst + diff > max)
		    max = rfirst + diff;
		if (!grows)
		    grows = (tfirst < rfirst &&
			     UNISKIP(tfirst) < UNISKIP(rfirst + diff));
		rfirst += diff + 1;
	    }
	    tfirst += diff + 1;
	}

	none = ++max;
	if (del)
	    del = ++max;

	if (max > 0xffff)
	    bits = 32;
	else if (max > 0xff)
	    bits = 16;
	else
	    bits = 8;

	swash = MUTABLE_SV(swash_init("utf8", "", listsv, bits, none));
#ifdef USE_ITHREADS
	cPADOPo->op_padix = pad_alloc(OP_TRANS, SVf_READONLY);
	SvREFCNT_dec(PAD_SVl(cPADOPo->op_padix));
	PAD_SETSV(cPADOPo->op_padix, swash);
	SvPADTMP_on(swash);
	SvREADONLY_on(swash);
#else
	cSVOPo->op_sv = swash;
#endif
	SvREFCNT_dec(listsv);
	SvREFCNT_dec(transv);

	if (!del && havefinal && rlen)
	    (void)hv_store(MUTABLE_HV(SvRV(swash)), "FINAL", 5,
			   newSVuv((UV)final), 0);

	if (grows)
	    o->op_private |= OPpTRANS_GROWS;

	Safefree(tsave);
	Safefree(rsave);

#ifdef PERL_MAD
	op_getmad(expr,o,'e');
	op_getmad(repl,o,'r');
#else
	op_free(expr);
	op_free(repl);
#endif
	return o;
    }

    tbl = (short*)PerlMemShared_calloc(
	(o->op_private & OPpTRANS_COMPLEMENT) &&
	    !(o->op_private & OPpTRANS_DELETE) ? 258 : 256,
	sizeof(short));
    cPVOPo->op_pv = (char*)tbl;
    if (complement) {
	for (i = 0; i < (I32)tlen; i++)
	    tbl[t[i]] = -1;
	for (i = 0, j = 0; i < 256; i++) {
	    if (!tbl[i]) {
		if (j >= (I32)rlen) {
		    if (del)
			tbl[i] = -2;
		    else if (rlen)
			tbl[i] = r[j-1];
		    else
			tbl[i] = (short)i;
		}
		else {
		    if (i < 128 && r[j] >= 128)
			grows = 1;
		    tbl[i] = r[j++];
		}
	    }
	}
	if (!del) {
	    if (!rlen) {
		j = rlen;
		if (!squash)
		    o->op_private |= OPpTRANS_IDENTICAL;
	    }
	    else if (j >= (I32)rlen)
		j = rlen - 1;
	    else {
		tbl = 
		    (short *)
		    PerlMemShared_realloc(tbl,
					  (0x101+rlen-j) * sizeof(short));
		cPVOPo->op_pv = (char*)tbl;
	    }
	    tbl[0x100] = (short)(rlen - j);
	    for (i=0; i < (I32)rlen - j; i++)
		tbl[0x101+i] = r[j+i];
	}
    }
    else {
	if (!rlen && !del) {
	    r = t; rlen = tlen;
	    if (!squash)
		o->op_private |= OPpTRANS_IDENTICAL;
	}
	else if (!squash && rlen == tlen && memEQ((char*)t, (char*)r, tlen)) {
	    o->op_private |= OPpTRANS_IDENTICAL;
	}
	for (i = 0; i < 256; i++)
	    tbl[i] = -1;
	for (i = 0, j = 0; i < (I32)tlen; i++,j++) {
	    if (j >= (I32)rlen) {
		if (del) {
		    if (tbl[t[i]] == -1)
			tbl[t[i]] = -2;
		    continue;
		}
		--j;
	    }
	    if (tbl[t[i]] == -1) {
		if (t[i] < 128 && r[j] >= 128)
		    grows = 1;
		tbl[t[i]] = r[j];
	    }
	}
    }

    if(del && rlen == tlen) {
	Perl_ck_warner(aTHX_ packWARN(WARN_MISC), "Useless use of /d modifier in transliteration operator"); 
    } else if(rlen > tlen && !complement) {
	Perl_ck_warner(aTHX_ packWARN(WARN_MISC), "Replacement list is longer than search list");
    }

    if (grows)
	o->op_private |= OPpTRANS_GROWS;
#ifdef PERL_MAD
    op_getmad(expr,o,'e');
    op_getmad(repl,o,'r');
#else
    op_free(expr);
    op_free(repl);
#endif

    return o;
}

/*
=for apidoc Am|OP *|newPMOP|I32 type|I32 flags

Constructs, checks, and returns an op of any pattern matching type.
I<type> is the opcode.  I<flags> gives the eight bits of C<op_flags>
and, shifted up eight bits, the eight bits of C<op_private>.

=cut
*/

OP *
Perl_newPMOP(pTHX_ I32 type, I32 flags)
{
    dVAR;
    PMOP *pmop;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_PMOP);

    NewOp(1101, pmop, 1, PMOP);
    pmop->op_type = (OPCODE)type;
    pmop->op_ppaddr = PL_ppaddr[type];
    pmop->op_flags = (U8)flags;
    pmop->op_private = (U8)(0 | (flags >> 8));

    if (PL_hints & HINT_RE_TAINT)
	pmop->op_pmflags |= PMf_RETAINT;
    if (IN_LOCALE_COMPILETIME) {
	set_regex_charset(&(pmop->op_pmflags), REGEX_LOCALE_CHARSET);
    }
    else if ((! (PL_hints & HINT_BYTES))
                /* Both UNI_8_BIT and locale :not_characters imply Unicode */
	     && (PL_hints & (HINT_UNI_8_BIT|HINT_LOCALE_NOT_CHARS)))
    {
	set_regex_charset(&(pmop->op_pmflags), REGEX_UNICODE_CHARSET);
    }
    if (PL_hints & HINT_RE_FLAGS) {
        SV *reflags = Perl_refcounted_he_fetch_pvn(aTHX_
         PL_compiling.cop_hints_hash, STR_WITH_LEN("reflags"), 0, 0
        );
        if (reflags && SvOK(reflags)) pmop->op_pmflags |= SvIV(reflags);
        reflags = Perl_refcounted_he_fetch_pvn(aTHX_
         PL_compiling.cop_hints_hash, STR_WITH_LEN("reflags_charset"), 0, 0
        );
        if (reflags && SvOK(reflags)) {
            set_regex_charset(&(pmop->op_pmflags), (regex_charset)SvIV(reflags));
        }
    }


#ifdef USE_ITHREADS
    assert(SvPOK(PL_regex_pad[0]));
    if (SvCUR(PL_regex_pad[0])) {
	/* Pop off the "packed" IV from the end.  */
	SV *const repointer_list = PL_regex_pad[0];
	const char *p = SvEND(repointer_list) - sizeof(IV);
	const IV offset = *((IV*)p);

	assert(SvCUR(repointer_list) % sizeof(IV) == 0);

	SvEND_set(repointer_list, p);

	pmop->op_pmoffset = offset;
	/* This slot should be free, so assert this:  */
	assert(PL_regex_pad[offset] == &PL_sv_undef);
    } else {
	SV * const repointer = &PL_sv_undef;
	av_push(PL_regex_padav, repointer);
	pmop->op_pmoffset = av_tindex(PL_regex_padav);
	PL_regex_pad = AvARRAY(PL_regex_padav);
    }
#endif

    return CHECKOP(type, pmop);
}

/* Given some sort of match op o, and an expression expr containing a
 * pattern, either compile expr into a regex and attach it to o (if it's
 * constant), or convert expr into a runtime regcomp op sequence (if it's
 * not)
 *
 * isreg indicates that the pattern is part of a regex construct, eg
 * $x =~ /pattern/ or split /pattern/, as opposed to $x =~ $pattern or
 * split "pattern", which aren't. In the former case, expr will be a list
 * if the pattern contains more than one term (eg /a$b/) or if it contains
 * a replacement, ie s/// or tr///.
 *
 * When the pattern has been compiled within a new anon CV (for
 * qr/(?{...})/ ), then floor indicates the savestack level just before
 * the new sub was created
 */

OP *
Perl_pmruntime(pTHX_ OP *o, OP *expr, bool isreg, I32 floor)
{
    dVAR;
    PMOP *pm;
    LOGOP *rcop;
    I32 repl_has_vars = 0;
    OP* repl = NULL;
    bool is_trans = (o->op_type == OP_TRANS || o->op_type == OP_TRANSR);
    bool is_compiletime;
    bool has_code;

    PERL_ARGS_ASSERT_PMRUNTIME;

    /* for s/// and tr///, last element in list is the replacement; pop it */

    /* If we have a syntax error causing tokens to be popped and the parser
       to see PMFUNC '(' expr ')' with no commas in it; e.g., s/${<>{})//,
       then expr will not be of type OP_LIST, there being no repl.  */
    if ((is_trans || o->op_type == OP_SUBST) && expr->op_type == OP_LIST) {
	OP* kid;
	repl = cLISTOPx(expr)->op_last;
	kid = cLISTOPx(expr)->op_first;
	while (kid->op_sibling != repl)
	    kid = kid->op_sibling;
	kid->op_sibling = NULL;
	cLISTOPx(expr)->op_last = kid;
    }

    /* for TRANS, convert LIST/PUSH/CONST into CONST, and pass to pmtrans() */

    if (is_trans) {
	OP* const oe = expr;
	assert(expr->op_type == OP_LIST);
	assert(cLISTOPx(expr)->op_first->op_type == OP_PUSHMARK);
	assert(cLISTOPx(expr)->op_first->op_sibling == cLISTOPx(expr)->op_last);
	expr = cLISTOPx(oe)->op_last;
	cLISTOPx(oe)->op_first->op_sibling = NULL;
	cLISTOPx(oe)->op_last = NULL;
	op_free(oe);

	return pmtrans(o, expr, repl);
    }

    /* find whether we have any runtime or code elements;
     * at the same time, temporarily set the op_next of each DO block;
     * then when we LINKLIST, this will cause the DO blocks to be excluded
     * from the op_next chain (and from having LINKLIST recursively
     * applied to them). We fix up the DOs specially later */

    is_compiletime = 1;
    has_code = 0;
    if (expr->op_type == OP_LIST) {
	OP *o;
	for (o = cLISTOPx(expr)->op_first; o; o = o->op_sibling) {
	    if (o->op_type == OP_NULL && (o->op_flags & OPf_SPECIAL)) {
		has_code = 1;
		assert(!o->op_next && o->op_sibling);
		o->op_next = o->op_sibling;
	    }
	    else if (o->op_type != OP_CONST && o->op_type != OP_PUSHMARK)
		is_compiletime = 0;
	}
    }
    else if (expr->op_type != OP_CONST)
	is_compiletime = 0;

    LINKLIST(expr);

    /* fix up DO blocks; treat each one as a separate little sub;
     * also, mark any arrays as LIST/REF */

    if (expr->op_type == OP_LIST) {
	OP *o;
	for (o = cLISTOPx(expr)->op_first; o; o = o->op_sibling) {

            if (o->op_type == OP_PADAV || o->op_type == OP_RV2AV) {
                assert( !(o->op_flags  & OPf_WANT));
                /* push the array rather than its contents. The regex
                 * engine will retrieve and join the elements later */
                o->op_flags |= (OPf_WANT_LIST | OPf_REF);
                continue;
            }

	    if (!(o->op_type == OP_NULL && (o->op_flags & OPf_SPECIAL)))
		continue;
	    o->op_next = NULL; /* undo temporary hack from above */
	    scalar(o);
	    LINKLIST(o);
	    if (cLISTOPo->op_first->op_type == OP_LEAVE) {
		LISTOP *leaveop = cLISTOPx(cLISTOPo->op_first);
		/* skip ENTER */
		assert(leaveop->op_first->op_type == OP_ENTER);
		assert(leaveop->op_first->op_sibling);
		o->op_next = leaveop->op_first->op_sibling;
		/* skip leave */
		assert(leaveop->op_flags & OPf_KIDS);
		assert(leaveop->op_last->op_next == (OP*)leaveop);
		leaveop->op_next = NULL; /* stop on last op */
		op_null((OP*)leaveop);
	    }
	    else {
		/* skip SCOPE */
		OP *scope = cLISTOPo->op_first;
		assert(scope->op_type == OP_SCOPE);
		assert(scope->op_flags & OPf_KIDS);
		scope->op_next = NULL; /* stop on last op */
		op_null(scope);
	    }
	    /* have to peep the DOs individually as we've removed it from
	     * the op_next chain */
	    CALL_PEEP(o);
            S_prune_chain_head(aTHX_ &(o->op_next));
	    if (is_compiletime)
		/* runtime finalizes as part of finalizing whole tree */
		finalize_optree(o);
	}
    }
    else if (expr->op_type == OP_PADAV || expr->op_type == OP_RV2AV) {
        assert( !(expr->op_flags  & OPf_WANT));
        /* push the array rather than its contents. The regex
         * engine will retrieve and join the elements later */
        expr->op_flags |= (OPf_WANT_LIST | OPf_REF);
    }

    PL_hints |= HINT_BLOCK_SCOPE;
    pm = (PMOP*)o;
    assert(floor==0 || (pm->op_pmflags & PMf_HAS_CV));

    if (is_compiletime) {
	U32 rx_flags = pm->op_pmflags & RXf_PMf_COMPILETIME;
	regexp_engine const *eng = current_re_engine();

        if (o->op_flags & OPf_SPECIAL)
            rx_flags |= RXf_SPLIT;

	if (!has_code || !eng->op_comp) {
	    /* compile-time simple constant pattern */

	    if ((pm->op_pmflags & PMf_HAS_CV) && !has_code) {
		/* whoops! we guessed that a qr// had a code block, but we
		 * were wrong (e.g. /[(?{}]/ ). Throw away the PL_compcv
		 * that isn't required now. Note that we have to be pretty
		 * confident that nothing used that CV's pad while the
		 * regex was parsed, except maybe op targets for \Q etc.
		 * If there were any op targets, though, they should have
		 * been stolen by constant folding.
		 */
#ifdef DEBUGGING
		PADOFFSET i = 0;
		assert(PadnamelistMAXNAMED(PL_comppad_name) == 0);
		while (++i <= AvFILLp(PL_comppad)) {
		    assert(!PL_curpad[i]);
		}
#endif
		/* But we know that one op is using this CV's slab. */
		cv_forget_slab(PL_compcv);
		LEAVE_SCOPE(floor);
		pm->op_pmflags &= ~PMf_HAS_CV;
	    }

	    PM_SETRE(pm,
		eng->op_comp
		    ? eng->op_comp(aTHX_ NULL, 0, expr, eng, NULL, NULL,
					rx_flags, pm->op_pmflags)
		    : Perl_re_op_compile(aTHX_ NULL, 0, expr, eng, NULL, NULL,
					rx_flags, pm->op_pmflags)
	    );
#ifdef PERL_MAD
	    op_getmad(expr,(OP*)pm,'e');
#else
	    op_free(expr);
#endif
	}
	else {
	    /* compile-time pattern that includes literal code blocks */
	    REGEXP* re = eng->op_comp(aTHX_ NULL, 0, expr, eng, NULL, NULL,
			rx_flags,
			(pm->op_pmflags |
			    ((PL_hints & HINT_RE_EVAL) ? PMf_USE_RE_EVAL : 0))
		    );
	    PM_SETRE(pm, re);
	    if (pm->op_pmflags & PMf_HAS_CV) {
		CV *cv;
		/* this QR op (and the anon sub we embed it in) is never
		 * actually executed. It's just a placeholder where we can
		 * squirrel away expr in op_code_list without the peephole
		 * optimiser etc processing it for a second time */
		OP *qr = newPMOP(OP_QR, 0);
		((PMOP*)qr)->op_code_list = expr;

		/* handle the implicit sub{} wrapped round the qr/(?{..})/ */
		SvREFCNT_inc_simple_void(PL_compcv);
		cv = newATTRSUB(floor, 0, NULL, NULL, qr);
		ReANY(re)->qr_anoncv = cv;

		/* attach the anon CV to the pad so that
		 * pad_fixup_inner_anons() can find it */
		(void)pad_add_anon(cv, o->op_type);
		SvREFCNT_inc_simple_void(cv);
	    }
	    else {
		pm->op_code_list = expr;
	    }
	}
    }
    else {
	/* runtime pattern: build chain of regcomp etc ops */
	bool reglist;
	PADOFFSET cv_targ = 0;

	reglist = isreg && expr->op_type == OP_LIST;
	if (reglist)
	    op_null(expr);

	if (has_code) {
	    pm->op_code_list = expr;
	    /* don't free op_code_list; its ops are embedded elsewhere too */
	    pm->op_pmflags |= PMf_CODELIST_PRIVATE;
	}

        if (o->op_flags & OPf_SPECIAL)
            pm->op_pmflags |= PMf_SPLIT;

	/* the OP_REGCMAYBE is a placeholder in the non-threaded case
	 * to allow its op_next to be pointed past the regcomp and
	 * preceding stacking ops;
	 * OP_REGCRESET is there to reset taint before executing the
	 * stacking ops */
	if (pm->op_pmflags & PMf_KEEP || TAINTING_get)
	    expr = newUNOP((TAINTING_get ? OP_REGCRESET : OP_REGCMAYBE),0,expr);

	if (pm->op_pmflags & PMf_HAS_CV) {
	    /* we have a runtime qr with literal code. This means
	     * that the qr// has been wrapped in a new CV, which
	     * means that runtime consts, vars etc will have been compiled
	     * against a new pad. So... we need to execute those ops
	     * within the environment of the new CV. So wrap them in a call
	     * to a new anon sub. i.e. for
	     *
	     *     qr/a$b(?{...})/,
	     *
	     * we build an anon sub that looks like
	     *
	     *     sub { "a", $b, '(?{...})' }
	     *
	     * and call it, passing the returned list to regcomp.
	     * Or to put it another way, the list of ops that get executed
	     * are:
	     *
	     *     normal              PMf_HAS_CV
	     *     ------              -------------------
	     *                         pushmark (for regcomp)
	     *                         pushmark (for entersub)
	     *                         pushmark (for refgen)
	     *                         anoncode
	     *                         refgen
	     *                         entersub
	     *     regcreset                  regcreset
	     *     pushmark                   pushmark
	     *     const("a")                 const("a")
	     *     gvsv(b)                    gvsv(b)
	     *     const("(?{...})")          const("(?{...})")
	     *                                leavesub
	     *     regcomp             regcomp
	     */

	    SvREFCNT_inc_simple_void(PL_compcv);
	    CvLVALUE_on(PL_compcv);
	    /* these lines are just an unrolled newANONATTRSUB */
	    expr = newSVOP(OP_ANONCODE, 0,
		    MUTABLE_SV(newATTRSUB(floor, 0, NULL, NULL, expr)));
	    cv_targ = expr->op_targ;
	    expr = newUNOP(OP_REFGEN, 0, expr);

	    expr = list(force_list(newUNOP(OP_ENTERSUB, 0, scalar(expr))));
	}

	NewOp(1101, rcop, 1, LOGOP);
	rcop->op_type = OP_REGCOMP;
	rcop->op_ppaddr = PL_ppaddr[OP_REGCOMP];
	rcop->op_first = scalar(expr);
	rcop->op_flags |= OPf_KIDS
			    | ((PL_hints & HINT_RE_EVAL) ? OPf_SPECIAL : 0)
			    | (reglist ? OPf_STACKED : 0);
	rcop->op_private = 0;
	rcop->op_other = o;
	rcop->op_targ = cv_targ;

	/* /$x/ may cause an eval, since $x might be qr/(?{..})/  */
	if (PL_hints & HINT_RE_EVAL) PL_cv_has_eval = 1;

	/* establish postfix order */
	if (expr->op_type == OP_REGCRESET || expr->op_type == OP_REGCMAYBE) {
	    LINKLIST(expr);
	    rcop->op_next = expr;
	    ((UNOP*)expr)->op_first->op_next = (OP*)rcop;
	}
	else {
	    rcop->op_next = LINKLIST(expr);
	    expr->op_next = (OP*)rcop;
	}

	op_prepend_elem(o->op_type, scalar((OP*)rcop), o);
    }

    if (repl) {
	OP *curop = repl;
	bool konst;
	/* If we are looking at s//.../e with a single statement, get past
	   the implicit do{}. */
	if (curop->op_type == OP_NULL && curop->op_flags & OPf_KIDS
	 && cUNOPx(curop)->op_first->op_type == OP_SCOPE
	 && cUNOPx(curop)->op_first->op_flags & OPf_KIDS) {
	    OP *kid = cUNOPx(cUNOPx(curop)->op_first)->op_first;
	    if (kid->op_type == OP_NULL && kid->op_sibling
	     && !kid->op_sibling->op_sibling)
		curop = kid->op_sibling;
	}
	if (curop->op_type == OP_CONST)
	    konst = TRUE;
	else if (( (curop->op_type == OP_RV2SV ||
		    curop->op_type == OP_RV2AV ||
		    curop->op_type == OP_RV2HV ||
		    curop->op_type == OP_RV2GV)
		   && cUNOPx(curop)->op_first
		   && cUNOPx(curop)->op_first->op_type == OP_GV )
		|| curop->op_type == OP_PADSV
		|| curop->op_type == OP_PADAV
		|| curop->op_type == OP_PADHV
		|| curop->op_type == OP_PADANY) {
	    repl_has_vars = 1;
	    konst = TRUE;
	}
	else konst = FALSE;
	if (konst
	    && !(repl_has_vars
		 && (!PM_GETRE(pm)
		     || !RX_PRELEN(PM_GETRE(pm))
		     || RX_EXTFLAGS(PM_GETRE(pm)) & RXf_EVAL_SEEN)))
	{
	    pm->op_pmflags |= PMf_CONST;	/* const for long enough */
	    op_prepend_elem(o->op_type, scalar(repl), o);
	}
	else {
	    NewOp(1101, rcop, 1, LOGOP);
	    rcop->op_type = OP_SUBSTCONT;
	    rcop->op_ppaddr = PL_ppaddr[OP_SUBSTCONT];
	    rcop->op_first = scalar(repl);
	    rcop->op_flags |= OPf_KIDS;
	    rcop->op_private = 1;
	    rcop->op_other = o;

	    /* establish postfix order */
	    rcop->op_next = LINKLIST(repl);
	    repl->op_next = (OP*)rcop;

	    pm->op_pmreplrootu.op_pmreplroot = scalar((OP*)rcop);
	    assert(!(pm->op_pmflags & PMf_ONCE));
	    pm->op_pmstashstartu.op_pmreplstart = LINKLIST(rcop);
	    rcop->op_next = 0;
	}
    }

    return (OP*)pm;
}

/*
=for apidoc Am|OP *|newSVOP|I32 type|I32 flags|SV *sv

Constructs, checks, and returns an op of any type that involves an
embedded SV.  I<type> is the opcode.  I<flags> gives the eight bits
of C<op_flags>.  I<sv> gives the SV to embed in the op; this function
takes ownership of one reference to it.

=cut
*/

OP *
Perl_newSVOP(pTHX_ I32 type, I32 flags, SV *sv)
{
    dVAR;
    SVOP *svop;

    PERL_ARGS_ASSERT_NEWSVOP;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_SVOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_PVOP_OR_SVOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_FILESTATOP);

    NewOp(1101, svop, 1, SVOP);
    svop->op_type = (OPCODE)type;
    svop->op_ppaddr = PL_ppaddr[type];
    svop->op_sv = sv;
    svop->op_next = (OP*)svop;
    svop->op_flags = (U8)flags;
    svop->op_private = (U8)(0 | (flags >> 8));
    if (PL_opargs[type] & OA_RETSCALAR)
	scalar((OP*)svop);
    if (PL_opargs[type] & OA_TARGET)
	svop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, svop);
}

#ifdef USE_ITHREADS

/*
=for apidoc Am|OP *|newPADOP|I32 type|I32 flags|SV *sv

Constructs, checks, and returns an op of any type that involves a
reference to a pad element.  I<type> is the opcode.  I<flags> gives the
eight bits of C<op_flags>.  A pad slot is automatically allocated, and
is populated with I<sv>; this function takes ownership of one reference
to it.

This function only exists if Perl has been compiled to use ithreads.

=cut
*/

OP *
Perl_newPADOP(pTHX_ I32 type, I32 flags, SV *sv)
{
    dVAR;
    PADOP *padop;

    PERL_ARGS_ASSERT_NEWPADOP;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_SVOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_PVOP_OR_SVOP
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_FILESTATOP);

    NewOp(1101, padop, 1, PADOP);
    padop->op_type = (OPCODE)type;
    padop->op_ppaddr = PL_ppaddr[type];
    padop->op_padix = pad_alloc(type, SVs_PADTMP);
    SvREFCNT_dec(PAD_SVl(padop->op_padix));
    PAD_SETSV(padop->op_padix, sv);
    assert(sv);
    padop->op_next = (OP*)padop;
    padop->op_flags = (U8)flags;
    if (PL_opargs[type] & OA_RETSCALAR)
	scalar((OP*)padop);
    if (PL_opargs[type] & OA_TARGET)
	padop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, padop);
}

#endif /* USE_ITHREADS */

/*
=for apidoc Am|OP *|newGVOP|I32 type|I32 flags|GV *gv

Constructs, checks, and returns an op of any type that involves an
embedded reference to a GV.  I<type> is the opcode.  I<flags> gives the
eight bits of C<op_flags>.  I<gv> identifies the GV that the op should
reference; calling this function does not transfer ownership of any
reference to it.

=cut
*/

OP *
Perl_newGVOP(pTHX_ I32 type, I32 flags, GV *gv)
{
    dVAR;

    PERL_ARGS_ASSERT_NEWGVOP;

#ifdef USE_ITHREADS
    GvIN_PAD_on(gv);
    return newPADOP(type, flags, SvREFCNT_inc_simple_NN(gv));
#else
    return newSVOP(type, flags, SvREFCNT_inc_simple_NN(gv));
#endif
}

/*
=for apidoc Am|OP *|newPVOP|I32 type|I32 flags|char *pv

Constructs, checks, and returns an op of any type that involves an
embedded C-level pointer (PV).  I<type> is the opcode.  I<flags> gives
the eight bits of C<op_flags>.  I<pv> supplies the C-level pointer, which
must have been allocated using C<PerlMemShared_malloc>; the memory will
be freed when the op is destroyed.

=cut
*/

OP *
Perl_newPVOP(pTHX_ I32 type, I32 flags, char *pv)
{
    dVAR;
    const bool utf8 = cBOOL(flags & SVf_UTF8);
    PVOP *pvop;

    flags &= ~SVf_UTF8;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_PVOP_OR_SVOP
	|| type == OP_RUNCV
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_LOOPEXOP);

    NewOp(1101, pvop, 1, PVOP);
    pvop->op_type = (OPCODE)type;
    pvop->op_ppaddr = PL_ppaddr[type];
    pvop->op_pv = pv;
    pvop->op_next = (OP*)pvop;
    pvop->op_flags = (U8)flags;
    pvop->op_private = utf8 ? OPpPV_IS_UTF8 : 0;
    if (PL_opargs[type] & OA_RETSCALAR)
	scalar((OP*)pvop);
    if (PL_opargs[type] & OA_TARGET)
	pvop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, pvop);
}

#ifdef PERL_MAD
OP*
#else
void
#endif
Perl_package(pTHX_ OP *o)
{
    dVAR;
    SV *const sv = cSVOPo->op_sv;
#ifdef PERL_MAD
    OP *pegop;
#endif

    PERL_ARGS_ASSERT_PACKAGE;

    SAVEGENERICSV(PL_curstash);
    save_item(PL_curstname);

    PL_curstash = (HV *)SvREFCNT_inc(gv_stashsv(sv, GV_ADD));

    sv_setsv(PL_curstname, sv);

    PL_hints |= HINT_BLOCK_SCOPE;
    PL_parser->copline = NOLINE;
    PL_parser->expect = XSTATE;

#ifndef PERL_MAD
    op_free(o);
#else
    if (!PL_madskills) {
	op_free(o);
	return NULL;
    }

    pegop = newOP(OP_NULL,0);
    op_getmad(o,pegop,'P');
    return pegop;
#endif
}

void
Perl_package_version( pTHX_ OP *v )
{
    dVAR;
    U32 savehints = PL_hints;
    PERL_ARGS_ASSERT_PACKAGE_VERSION;
    PL_hints &= ~HINT_STRICT_VARS;
    sv_setsv( GvSV(gv_fetchpvs("VERSION", GV_ADDMULTI, SVt_PV)), cSVOPx(v)->op_sv );
    PL_hints = savehints;
    op_free(v);
}

#ifdef PERL_MAD
OP*
#else
void
#endif
Perl_utilize(pTHX_ int aver, I32 floor, OP *version, OP *idop, OP *arg)
{
    dVAR;
    OP *pack;
    OP *imop;
    OP *veop;
#ifdef PERL_MAD
    OP *pegop = PL_madskills ? newOP(OP_NULL,0) : NULL;
#endif
    SV *use_version = NULL;

    PERL_ARGS_ASSERT_UTILIZE;

    if (idop->op_type != OP_CONST)
	Perl_croak(aTHX_ "Module name must be constant");

    if (PL_madskills)
	op_getmad(idop,pegop,'U');

    veop = NULL;

    if (version) {
	SV * const vesv = ((SVOP*)version)->op_sv;

	if (PL_madskills)
	    op_getmad(version,pegop,'V');
	if (!arg && !SvNIOKp(vesv)) {
	    arg = version;
	}
	else {
	    OP *pack;
	    SV *meth;

	    if (version->op_type != OP_CONST || !SvNIOKp(vesv))
		Perl_croak(aTHX_ "Version number must be a constant number");

	    /* Make copy of idop so we don't free it twice */
	    pack = newSVOP(OP_CONST, 0, newSVsv(((SVOP*)idop)->op_sv));

	    /* Fake up a method call to VERSION */
	    meth = newSVpvs_share("VERSION");
	    veop = convert(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
			    op_append_elem(OP_LIST,
					op_prepend_elem(OP_LIST, pack, list(version)),
					newSVOP(OP_METHOD_NAMED, 0, meth)));
	}
    }

    /* Fake up an import/unimport */
    if (arg && arg->op_type == OP_STUB) {
	if (PL_madskills)
	    op_getmad(arg,pegop,'S');
	imop = arg;		/* no import on explicit () */
    }
    else if (SvNIOKp(((SVOP*)idop)->op_sv)) {
	imop = NULL;		/* use 5.0; */
	if (aver)
	    use_version = ((SVOP*)idop)->op_sv;
	else
	    idop->op_private |= OPpCONST_NOVER;
    }
    else {
	SV *meth;

	if (PL_madskills)
	    op_getmad(arg,pegop,'A');

	/* Make copy of idop so we don't free it twice */
	pack = newSVOP(OP_CONST, 0, newSVsv(((SVOP*)idop)->op_sv));

	/* Fake up a method call to import/unimport */
	meth = aver
	    ? newSVpvs_share("import") : newSVpvs_share("unimport");
	imop = convert(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
		       op_append_elem(OP_LIST,
				   op_prepend_elem(OP_LIST, pack, list(arg)),
				   newSVOP(OP_METHOD_NAMED, 0, meth)));
    }

    /* Fake up the BEGIN {}, which does its thing immediately. */
    newATTRSUB(floor,
	newSVOP(OP_CONST, 0, newSVpvs_share("BEGIN")),
	NULL,
	NULL,
	op_append_elem(OP_LINESEQ,
	    op_append_elem(OP_LINESEQ,
	        newSTATEOP(0, NULL, newUNOP(OP_REQUIRE, 0, idop)),
	        newSTATEOP(0, NULL, veop)),
	    newSTATEOP(0, NULL, imop) ));

    if (use_version) {
	/* Enable the
	 * feature bundle that corresponds to the required version. */
	use_version = sv_2mortal(new_version(use_version));
	S_enable_feature_bundle(aTHX_ use_version);

	/* If a version >= 5.11.0 is requested, strictures are on by default! */
	if (vcmp(use_version,
		 sv_2mortal(upg_version(newSVnv(5.011000), FALSE))) >= 0) {
	    if (!(PL_hints & HINT_EXPLICIT_STRICT_REFS))
		PL_hints |= HINT_STRICT_REFS;
	    if (!(PL_hints & HINT_EXPLICIT_STRICT_SUBS))
		PL_hints |= HINT_STRICT_SUBS;
	    if (!(PL_hints & HINT_EXPLICIT_STRICT_VARS))
		PL_hints |= HINT_STRICT_VARS;
	}
	/* otherwise they are off */
	else {
	    if (!(PL_hints & HINT_EXPLICIT_STRICT_REFS))
		PL_hints &= ~HINT_STRICT_REFS;
	    if (!(PL_hints & HINT_EXPLICIT_STRICT_SUBS))
		PL_hints &= ~HINT_STRICT_SUBS;
	    if (!(PL_hints & HINT_EXPLICIT_STRICT_VARS))
		PL_hints &= ~HINT_STRICT_VARS;
	}
    }

    /* The "did you use incorrect case?" warning used to be here.
     * The problem is that on case-insensitive filesystems one
     * might get false positives for "use" (and "require"):
     * "use Strict" or "require CARP" will work.  This causes
     * portability problems for the script: in case-strict
     * filesystems the script will stop working.
     *
     * The "incorrect case" warning checked whether "use Foo"
     * imported "Foo" to your namespace, but that is wrong, too:
     * there is no requirement nor promise in the language that
     * a Foo.pm should or would contain anything in package "Foo".
     *
     * There is very little Configure-wise that can be done, either:
     * the case-sensitivity of the build filesystem of Perl does not
     * help in guessing the case-sensitivity of the runtime environment.
     */

    PL_hints |= HINT_BLOCK_SCOPE;
    PL_parser->copline = NOLINE;
    PL_parser->expect = XSTATE;
    PL_cop_seqmax++; /* Purely for B::*'s benefit */
    if (PL_cop_seqmax == PERL_PADSEQ_INTRO) /* not a legal value */
	PL_cop_seqmax++;

#ifdef PERL_MAD
    return pegop;
#endif
}

/*
=head1 Embedding Functions

=for apidoc load_module

Loads the module whose name is pointed to by the string part of name.
Note that the actual module name, not its filename, should be given.
Eg, "Foo::Bar" instead of "Foo/Bar.pm".  flags can be any of
PERL_LOADMOD_DENY, PERL_LOADMOD_NOIMPORT, or PERL_LOADMOD_IMPORT_OPS
(or 0 for no flags).  ver, if specified
and not NULL, provides version semantics
similar to C<use Foo::Bar VERSION>.  The optional trailing SV*
arguments can be used to specify arguments to the module's import()
method, similar to C<use Foo::Bar VERSION LIST>.  They must be
terminated with a final NULL pointer.  Note that this list can only
be omitted when the PERL_LOADMOD_NOIMPORT flag has been used.
Otherwise at least a single NULL pointer to designate the default
import list is required.

The reference count for each specified C<SV*> parameter is decremented.

=cut */

void
Perl_load_module(pTHX_ U32 flags, SV *name, SV *ver, ...)
{
    va_list args;

    PERL_ARGS_ASSERT_LOAD_MODULE;

    va_start(args, ver);
    vload_module(flags, name, ver, &args);
    va_end(args);
}

#ifdef PERL_IMPLICIT_CONTEXT
void
Perl_load_module_nocontext(U32 flags, SV *name, SV *ver, ...)
{
    dTHX;
    va_list args;
    PERL_ARGS_ASSERT_LOAD_MODULE_NOCONTEXT;
    va_start(args, ver);
    vload_module(flags, name, ver, &args);
    va_end(args);
}
#endif

void
Perl_vload_module(pTHX_ U32 flags, SV *name, SV *ver, va_list *args)
{
    dVAR;
    OP *veop, *imop;
    OP * const modname = newSVOP(OP_CONST, 0, name);

    PERL_ARGS_ASSERT_VLOAD_MODULE;

    modname->op_private |= OPpCONST_BARE;
    if (ver) {
	veop = newSVOP(OP_CONST, 0, ver);
    }
    else
	veop = NULL;
    if (flags & PERL_LOADMOD_NOIMPORT) {
	imop = sawparens(newNULLLIST());
    }
    else if (flags & PERL_LOADMOD_IMPORT_OPS) {
	imop = va_arg(*args, OP*);
    }
    else {
	SV *sv;
	imop = NULL;
	sv = va_arg(*args, SV*);
	while (sv) {
	    imop = op_append_elem(OP_LIST, imop, newSVOP(OP_CONST, 0, sv));
	    sv = va_arg(*args, SV*);
	}
    }

    /* utilize() fakes up a BEGIN { require ..; import ... }, so make sure
     * that it has a PL_parser to play with while doing that, and also
     * that it doesn't mess with any existing parser, by creating a tmp
     * new parser with lex_start(). This won't actually be used for much,
     * since pp_require() will create another parser for the real work.
     * The ENTER/LEAVE pair protect callers from any side effects of use.  */

    ENTER;
    SAVEVPTR(PL_curcop);
    lex_start(NULL, NULL, LEX_START_SAME_FILTER);
    utilize(!(flags & PERL_LOADMOD_DENY), start_subparse(FALSE, 0),
	    veop, modname, imop);
    LEAVE;
}

PERL_STATIC_INLINE OP *
S_new_entersubop(pTHX_ GV *gv, OP *arg)
{
    return newUNOP(OP_ENTERSUB, OPf_STACKED,
		   newLISTOP(OP_LIST, 0, arg,
			     newUNOP(OP_RV2CV, 0,
				     newGVOP(OP_GV, 0, gv))));
}

OP *
Perl_dofile(pTHX_ OP *term, I32 force_builtin)
{
    dVAR;
    OP *doop;
    GV *gv;

    PERL_ARGS_ASSERT_DOFILE;

    if (!force_builtin && (gv = gv_override("do", 2))) {
	doop = S_new_entersubop(aTHX_ gv, term);
    }
    else {
	doop = newUNOP(OP_DOFILE, 0, scalar(term));
    }
    return doop;
}

/*
=head1 Optree construction

=for apidoc Am|OP *|newSLICEOP|I32 flags|OP *subscript|OP *listval

Constructs, checks, and returns an C<lslice> (list slice) op.  I<flags>
gives the eight bits of C<op_flags>, except that C<OPf_KIDS> will
be set automatically, and, shifted up eight bits, the eight bits of
C<op_private>, except that the bit with value 1 or 2 is automatically
set as required.  I<listval> and I<subscript> supply the parameters of
the slice; they are consumed by this function and become part of the
constructed op tree.

=cut
*/

OP *
Perl_newSLICEOP(pTHX_ I32 flags, OP *subscript, OP *listval)
{
    return newBINOP(OP_LSLICE, flags,
	    list(force_list(subscript)),
	    list(force_list(listval)) );
}

STATIC I32
S_is_list_assignment(pTHX_ const OP *o)
{
    unsigned type;
    U8 flags;

    if (!o)
	return TRUE;

    if ((o->op_type == OP_NULL) && (o->op_flags & OPf_KIDS))
	o = cUNOPo->op_first;

    flags = o->op_flags;
    type = o->op_type;
    if (type == OP_COND_EXPR) {
        const I32 t = is_list_assignment(cLOGOPo->op_first->op_sibling);
        const I32 f = is_list_assignment(cLOGOPo->op_first->op_sibling->op_sibling);

	if (t && f)
	    return TRUE;
	if (t || f)
	    yyerror("Assignment to both a list and a scalar");
	return FALSE;
    }

    if (type == OP_LIST &&
	(flags & OPf_WANT) == OPf_WANT_SCALAR &&
	o->op_private & OPpLVAL_INTRO)
	return FALSE;

    if (type == OP_LIST || flags & OPf_PARENS ||
	type == OP_RV2AV || type == OP_RV2HV ||
	type == OP_ASLICE || type == OP_HSLICE ||
        type == OP_KVASLICE || type == OP_KVHSLICE)
	return TRUE;

    if (type == OP_PADAV || type == OP_PADHV)
	return TRUE;

    if (type == OP_RV2SV)
	return FALSE;

    return FALSE;
}

/*
  Helper function for newASSIGNOP to detection commonality between the
  lhs and the rhs.  Marks all variables with PL_generation.  If it
  returns TRUE the assignment must be able to handle common variables.
*/
PERL_STATIC_INLINE bool
S_aassign_common_vars(pTHX_ OP* o)
{
    OP *curop;
    for (curop = cUNOPo->op_first; curop; curop=curop->op_sibling) {
	if (PL_opargs[curop->op_type] & OA_DANGEROUS) {
	    if (curop->op_type == OP_GV) {
		GV *gv = cGVOPx_gv(curop);
		if (gv == PL_defgv
		    || (int)GvASSIGN_GENERATION(gv) == PL_generation)
		    return TRUE;
		GvASSIGN_GENERATION_set(gv, PL_generation);
	    }
	    else if (curop->op_type == OP_PADSV ||
		curop->op_type == OP_PADAV ||
		curop->op_type == OP_PADHV ||
		curop->op_type == OP_PADANY)
		{
		    if (PAD_COMPNAME_GEN(curop->op_targ)
			== (STRLEN)PL_generation)
			return TRUE;
		    PAD_COMPNAME_GEN_set(curop->op_targ, PL_generation);

		}
	    else if (curop->op_type == OP_RV2CV)
		return TRUE;
	    else if (curop->op_type == OP_RV2SV ||
		curop->op_type == OP_RV2AV ||
		curop->op_type == OP_RV2HV ||
		curop->op_type == OP_RV2GV) {
		if (cUNOPx(curop)->op_first->op_type != OP_GV)	/* funny deref? */
		    return TRUE;
	    }
	    else if (curop->op_type == OP_PUSHRE) {
		GV *const gv =
#ifdef USE_ITHREADS
		    ((PMOP*)curop)->op_pmreplrootu.op_pmtargetoff
			? MUTABLE_GV(PAD_SVl(((PMOP*)curop)->op_pmreplrootu.op_pmtargetoff))
			: NULL;
#else
		    ((PMOP*)curop)->op_pmreplrootu.op_pmtargetgv;
#endif
		if (gv) {
		    if (gv == PL_defgv
			|| (int)GvASSIGN_GENERATION(gv) == PL_generation)
			return TRUE;
		    GvASSIGN_GENERATION_set(gv, PL_generation);
		}
	    }
	    else
		return TRUE;
	}

	if (curop->op_flags & OPf_KIDS) {
	    if (aassign_common_vars(curop))
		return TRUE;
	}
    }
    return FALSE;
}

/*
=for apidoc Am|OP *|newASSIGNOP|I32 flags|OP *left|I32 optype|OP *right

Constructs, checks, and returns an assignment op.  I<left> and I<right>
supply the parameters of the assignment; they are consumed by this
function and become part of the constructed op tree.

If I<optype> is C<OP_ANDASSIGN>, C<OP_ORASSIGN>, or C<OP_DORASSIGN>, then
a suitable conditional optree is constructed.  If I<optype> is the opcode
of a binary operator, such as C<OP_BIT_OR>, then an op is constructed that
performs the binary operation and assigns the result to the left argument.
Either way, if I<optype> is non-zero then I<flags> has no effect.

If I<optype> is zero, then a plain scalar or list assignment is
constructed.  Which type of assignment it is is automatically determined.
I<flags> gives the eight bits of C<op_flags>, except that C<OPf_KIDS>
will be set automatically, and, shifted up eight bits, the eight bits
of C<op_private>, except that the bit with value 1 or 2 is automatically
set as required.

=cut
*/

OP *
Perl_newASSIGNOP(pTHX_ I32 flags, OP *left, I32 optype, OP *right)
{
    dVAR;
    OP *o;

    if (optype) {
	if (optype == OP_ANDASSIGN || optype == OP_ORASSIGN || optype == OP_DORASSIGN) {
	    return newLOGOP(optype, 0,
		op_lvalue(scalar(left), optype),
		newUNOP(OP_SASSIGN, 0, scalar(right)));
	}
	else {
	    return newBINOP(optype, OPf_STACKED,
		op_lvalue(scalar(left), optype), scalar(right));
	}
    }

    if (is_list_assignment(left)) {
	static const char no_list_state[] = "Initialization of state variables"
	    " in list context currently forbidden";
	OP *curop;
	bool maybe_common_vars = TRUE;

	if (left->op_type == OP_ASLICE || left->op_type == OP_HSLICE)
	    left->op_private &= ~ OPpSLICEWARNING;

	PL_modcount = 0;
	left = op_lvalue(left, OP_AASSIGN);
	curop = list(force_list(left));
	o = newBINOP(OP_AASSIGN, flags, list(force_list(right)), curop);
	o->op_private = (U8)(0 | (flags >> 8));

	if (OP_TYPE_IS_OR_WAS(left, OP_LIST))
	{
	    OP* lop = ((LISTOP*)left)->op_first;
	    maybe_common_vars = FALSE;
	    while (lop) {
		if (lop->op_type == OP_PADSV ||
		    lop->op_type == OP_PADAV ||
		    lop->op_type == OP_PADHV ||
		    lop->op_type == OP_PADANY) {
		    if (!(lop->op_private & OPpLVAL_INTRO))
			maybe_common_vars = TRUE;

		    if (lop->op_private & OPpPAD_STATE) {
			if (left->op_private & OPpLVAL_INTRO) {
			    /* Each variable in state($a, $b, $c) = ... */
			}
			else {
			    /* Each state variable in
			       (state $a, my $b, our $c, $d, undef) = ... */
			}
			yyerror(no_list_state);
		    } else {
			/* Each my variable in
			   (state $a, my $b, our $c, $d, undef) = ... */
		    }
		} else if (lop->op_type == OP_UNDEF ||
                           OP_TYPE_IS_OR_WAS(lop, OP_PUSHMARK)) {
		    /* undef may be interesting in
		       (state $a, undef, state $c) */
		} else {
		    /* Other ops in the list. */
		    maybe_common_vars = TRUE;
		}
		lop = lop->op_sibling;
	    }
	}
	else if ((left->op_private & OPpLVAL_INTRO)
		&& (   left->op_type == OP_PADSV
		    || left->op_type == OP_PADAV
		    || left->op_type == OP_PADHV
		    || left->op_type == OP_PADANY))
	{
	    if (left->op_type == OP_PADSV) maybe_common_vars = FALSE;
	    if (left->op_private & OPpPAD_STATE) {
		/* All single variable list context state assignments, hence
		   state ($a) = ...
		   (state $a) = ...
		   state @a = ...
		   state (@a) = ...
		   (state @a) = ...
		   state %a = ...
		   state (%a) = ...
		   (state %a) = ...
		*/
		yyerror(no_list_state);
	    }
	}

	/* PL_generation sorcery:
	 * an assignment like ($a,$b) = ($c,$d) is easier than
	 * ($a,$b) = ($c,$a), since there is no need for temporary vars.
	 * To detect whether there are common vars, the global var
	 * PL_generation is incremented for each assign op we compile.
	 * Then, while compiling the assign op, we run through all the
	 * variables on both sides of the assignment, setting a spare slot
	 * in each of them to PL_generation. If any of them already have
	 * that value, we know we've got commonality.  We could use a
	 * single bit marker, but then we'd have to make 2 passes, first
	 * to clear the flag, then to test and set it.  To find somewhere
	 * to store these values, evil chicanery is done with SvUVX().
	 */

	if (maybe_common_vars) {
	    PL_generation++;
	    if (aassign_common_vars(o))
		o->op_private |= OPpASSIGN_COMMON;
	    LINKLIST(o);
	}

	if (right && right->op_type == OP_SPLIT && !PL_madskills) {
	    OP* tmpop = ((LISTOP*)right)->op_first;
	    if (tmpop && (tmpop->op_type == OP_PUSHRE)) {
		PMOP * const pm = (PMOP*)tmpop;
		if (left->op_type == OP_RV2AV &&
		    !(left->op_private & OPpLVAL_INTRO) &&
		    !(o->op_private & OPpASSIGN_COMMON) )
		{
		    tmpop = ((UNOP*)left)->op_first;
		    if (tmpop->op_type == OP_GV
#ifdef USE_ITHREADS
			&& !pm->op_pmreplrootu.op_pmtargetoff
#else
			&& !pm->op_pmreplrootu.op_pmtargetgv
#endif
			) {
#ifdef USE_ITHREADS
			pm->op_pmreplrootu.op_pmtargetoff
			    = cPADOPx(tmpop)->op_padix;
			cPADOPx(tmpop)->op_padix = 0;	/* steal it */
#else
			pm->op_pmreplrootu.op_pmtargetgv
			    = MUTABLE_GV(cSVOPx(tmpop)->op_sv);
			cSVOPx(tmpop)->op_sv = NULL;	/* steal it */
#endif
			tmpop = cUNOPo->op_first;	/* to list (nulled) */
			tmpop = ((UNOP*)tmpop)->op_first; /* to pushmark */
			tmpop->op_sibling = NULL;	/* don't free split */
			right->op_next = tmpop->op_next;  /* fix starting loc */
			op_free(o);			/* blow off assign */
			right->op_flags &= ~OPf_WANT;
				/* "I don't know and I don't care." */
			return right;
		    }
		}
		else {
                   if (PL_modcount < RETURN_UNLIMITED_NUMBER &&
		      ((LISTOP*)right)->op_last->op_type == OP_CONST)
		    {
			SV ** const svp =
			    &((SVOP*)((LISTOP*)right)->op_last)->op_sv;
			SV * const sv = *svp;
			if (SvIOK(sv) && SvIVX(sv) == 0)
			{
			  if (right->op_private & OPpSPLIT_IMPLIM) {
			    /* our own SV, created in ck_split */
			    SvREADONLY_off(sv);
			    sv_setiv(sv, PL_modcount+1);
			  }
			  else {
			    /* SV may belong to someone else */
			    SvREFCNT_dec(sv);
			    *svp = newSViv(PL_modcount+1);
			  }
			}
		    }
		}
	    }
	}
	return o;
    }
    if (!right)
	right = newOP(OP_UNDEF, 0);
    if (right->op_type == OP_READLINE) {
	right->op_flags |= OPf_STACKED;
	return newBINOP(OP_NULL, flags, op_lvalue(scalar(left), OP_SASSIGN),
		scalar(right));
    }
    else {
	o = newBINOP(OP_SASSIGN, flags,
	    scalar(right), op_lvalue(scalar(left), OP_SASSIGN) );
    }
    return o;
}

/*
=for apidoc Am|OP *|newSTATEOP|I32 flags|char *label|OP *o

Constructs a state op (COP).  The state op is normally a C<nextstate> op,
but will be a C<dbstate> op if debugging is enabled for currently-compiled
code.  The state op is populated from C<PL_curcop> (or C<PL_compiling>).
If I<label> is non-null, it supplies the name of a label to attach to
the state op; this function takes ownership of the memory pointed at by
I<label>, and will free it.  I<flags> gives the eight bits of C<op_flags>
for the state op.

If I<o> is null, the state op is returned.  Otherwise the state op is
combined with I<o> into a C<lineseq> list op, which is returned.  I<o>
is consumed by this function and becomes part of the returned op tree.

=cut
*/

OP *
Perl_newSTATEOP(pTHX_ I32 flags, char *label, OP *o)
{
    dVAR;
    const U32 seq = intro_my();
    const U32 utf8 = flags & SVf_UTF8;
    COP *cop;

    flags &= ~SVf_UTF8;

    NewOp(1101, cop, 1, COP);
    if (PERLDB_LINE && CopLINE(PL_curcop) && PL_curstash != PL_debstash) {
	cop->op_type = OP_DBSTATE;
	cop->op_ppaddr = PL_ppaddr[ OP_DBSTATE ];
    }
    else {
	cop->op_type = OP_NEXTSTATE;
	cop->op_ppaddr = PL_ppaddr[ OP_NEXTSTATE ];
    }
    cop->op_flags = (U8)flags;
    CopHINTS_set(cop, PL_hints);
#ifdef NATIVE_HINTS
    cop->op_private |= NATIVE_HINTS;
#endif
#ifdef VMS
    if (VMSISH_HUSHED) cop->op_private |= OPpHUSH_VMSISH;
#endif
    cop->op_next = (OP*)cop;

    cop->cop_seq = seq;
    cop->cop_warnings = DUP_WARNINGS(PL_curcop->cop_warnings);
    CopHINTHASH_set(cop, cophh_copy(CopHINTHASH_get(PL_curcop)));
    if (label) {
	Perl_cop_store_label(aTHX_ cop, label, strlen(label), utf8);

	PL_hints |= HINT_BLOCK_SCOPE;
	/* It seems that we need to defer freeing this pointer, as other parts
	   of the grammar end up wanting to copy it after this op has been
	   created. */
	SAVEFREEPV(label);
    }

    if (PL_parser->preambling != NOLINE) {
        CopLINE_set(cop, PL_parser->preambling);
        PL_parser->copline = NOLINE;
    }
    else if (PL_parser->copline == NOLINE)
        CopLINE_set(cop, CopLINE(PL_curcop));
    else {
	CopLINE_set(cop, PL_parser->copline);
	PL_parser->copline = NOLINE;
    }
#ifdef USE_ITHREADS
    CopFILE_set(cop, CopFILE(PL_curcop));	/* XXX share in a pvtable? */
#else
    CopFILEGV_set(cop, CopFILEGV(PL_curcop));
#endif
    CopSTASH_set(cop, PL_curstash);

    if (cop->op_type == OP_DBSTATE) {
	/* this line can have a breakpoint - store the cop in IV */
	AV *av = CopFILEAVx(PL_curcop);
	if (av) {
	    SV * const * const svp = av_fetch(av, CopLINE(cop), FALSE);
	    if (svp && *svp != &PL_sv_undef ) {
		(void)SvIOK_on(*svp);
		SvIV_set(*svp, PTR2IV(cop));
	    }
	}
    }

    if (flags & OPf_SPECIAL)
	op_null((OP*)cop);
    return op_prepend_elem(OP_LINESEQ, (OP*)cop, o);
}

/*
=for apidoc Am|OP *|newLOGOP|I32 type|I32 flags|OP *first|OP *other

Constructs, checks, and returns a logical (flow control) op.  I<type>
is the opcode.  I<flags> gives the eight bits of C<op_flags>, except
that C<OPf_KIDS> will be set automatically, and, shifted up eight bits,
the eight bits of C<op_private>, except that the bit with value 1 is
automatically set.  I<first> supplies the expression controlling the
flow, and I<other> supplies the side (alternate) chain of ops; they are
consumed by this function and become part of the constructed op tree.

=cut
*/

OP *
Perl_newLOGOP(pTHX_ I32 type, I32 flags, OP *first, OP *other)
{
    dVAR;

    PERL_ARGS_ASSERT_NEWLOGOP;

    return new_logop(type, flags, &first, &other);
}

STATIC OP *
S_search_const(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_SEARCH_CONST;

    switch (o->op_type) {
	case OP_CONST:
	    return o;
	case OP_NULL:
	    if (o->op_flags & OPf_KIDS)
		return search_const(cUNOPo->op_first);
	    break;
	case OP_LEAVE:
	case OP_SCOPE:
	case OP_LINESEQ:
	{
	    OP *kid;
	    if (!(o->op_flags & OPf_KIDS))
		return NULL;
	    kid = cLISTOPo->op_first;
	    do {
		switch (kid->op_type) {
		    case OP_ENTER:
		    case OP_NULL:
		    case OP_NEXTSTATE:
			kid = kid->op_sibling;
			break;
		    default:
			if (kid != cLISTOPo->op_last)
			    return NULL;
			goto last;
		}
	    } while (kid);
	    if (!kid)
		kid = cLISTOPo->op_last;
last:
	    return search_const(kid);
	}
    }

    return NULL;
}

STATIC OP *
S_new_logop(pTHX_ I32 type, I32 flags, OP** firstp, OP** otherp)
{
    dVAR;
    LOGOP *logop;
    OP *o;
    OP *first;
    OP *other;
    OP *cstop = NULL;
    int prepend_not = 0;

    PERL_ARGS_ASSERT_NEW_LOGOP;

    first = *firstp;
    other = *otherp;

    /* [perl #59802]: Warn about things like "return $a or $b", which
       is parsed as "(return $a) or $b" rather than "return ($a or
       $b)".  NB: This also applies to xor, which is why we do it
       here.
     */
    switch (first->op_type) {
    case OP_NEXT:
    case OP_LAST:
    case OP_REDO:
	/* XXX: Perhaps we should emit a stronger warning for these.
	   Even with the high-precedence operator they don't seem to do
	   anything sensible.

	   But until we do, fall through here.
         */
    case OP_RETURN:
    case OP_EXIT:
    case OP_DIE:
    case OP_GOTO:
	/* XXX: Currently we allow people to "shoot themselves in the
	   foot" by explicitly writing "(return $a) or $b".

	   Warn unless we are looking at the result from folding or if
	   the programmer explicitly grouped the operators like this.
	   The former can occur with e.g.

		use constant FEATURE => ( $] >= ... );
		sub { not FEATURE and return or do_stuff(); }
	 */
	if (!first->op_folded && !(first->op_flags & OPf_PARENS))
	    Perl_ck_warner(aTHX_ packWARN(WARN_SYNTAX),
	                   "Possible precedence issue with control flow operator");
	/* XXX: Should we optimze this to "return $a;" (i.e. remove
	   the "or $b" part)?
	*/
	break;
    }

    if (type == OP_XOR)		/* Not short circuit, but here by precedence. */
	return newBINOP(type, flags, scalar(first), scalar(other));

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_LOGOP);

    scalarboolean(first);
    /* optimize AND and OR ops that have NOTs as children */
    if (first->op_type == OP_NOT
	&& (first->op_flags & OPf_KIDS)
	&& ((first->op_flags & OPf_SPECIAL) /* unless ($x) { } */
	    || (other->op_type == OP_NOT))  /* if (!$x && !$y) { } */
	&& !PL_madskills) {
	if (type == OP_AND || type == OP_OR) {
	    if (type == OP_AND)
		type = OP_OR;
	    else
		type = OP_AND;
	    op_null(first);
	    if (other->op_type == OP_NOT) { /* !a AND|OR !b => !(a OR|AND b) */
		op_null(other);
		prepend_not = 1; /* prepend a NOT op later */
	    }
	}
    }
    /* search for a constant op that could let us fold the test */
    if ((cstop = search_const(first))) {
	if (cstop->op_private & OPpCONST_STRICT)
	    no_bareword_allowed(cstop);
	else if ((cstop->op_private & OPpCONST_BARE))
		Perl_ck_warner(aTHX_ packWARN(WARN_BAREWORD), "Bareword found in conditional");
	if ((type == OP_AND &&  SvTRUE(((SVOP*)cstop)->op_sv)) ||
	    (type == OP_OR  && !SvTRUE(((SVOP*)cstop)->op_sv)) ||
	    (type == OP_DOR && !SvOK(((SVOP*)cstop)->op_sv))) {
	    *firstp = NULL;
	    if (other->op_type == OP_CONST)
		other->op_private |= OPpCONST_SHORTCIRCUIT;
	    if (PL_madskills) {
		OP *newop = newUNOP(OP_NULL, 0, other);
		op_getmad(first, newop, '1');
		newop->op_targ = type;	/* set "was" field */
		return newop;
	    }
	    op_free(first);
	    if (other->op_type == OP_LEAVE)
		other = newUNOP(OP_NULL, OPf_SPECIAL, other);
	    else if (other->op_type == OP_MATCH
	          || other->op_type == OP_SUBST
	          || other->op_type == OP_TRANSR
	          || other->op_type == OP_TRANS)
		/* Mark the op as being unbindable with =~ */
		other->op_flags |= OPf_SPECIAL;

	    other->op_folded = 1;
	    return other;
	}
	else {
	    /* check for C<my $x if 0>, or C<my($x,$y) if 0> */
	    const OP *o2 = other;
	    if ( ! (o2->op_type == OP_LIST
		    && (( o2 = cUNOPx(o2)->op_first))
		    && o2->op_type == OP_PUSHMARK
		    && (( o2 = o2->op_sibling)) )
	    )
		o2 = other;
	    if ((o2->op_type == OP_PADSV || o2->op_type == OP_PADAV
			|| o2->op_type == OP_PADHV)
		&& o2->op_private & OPpLVAL_INTRO
		&& !(o2->op_private & OPpPAD_STATE))
	    {
		Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
				 "Deprecated use of my() in false conditional");
	    }

	    *otherp = NULL;
	    if (cstop->op_type == OP_CONST)
		cstop->op_private |= OPpCONST_SHORTCIRCUIT;
	    if (PL_madskills) {
		first = newUNOP(OP_NULL, 0, first);
		op_getmad(other, first, '2');
		first->op_targ = type;	/* set "was" field */
	    }
	    else
		op_free(other);
	    return first;
	}
    }
    else if ((first->op_flags & OPf_KIDS) && type != OP_DOR
	&& ckWARN(WARN_MISC)) /* [#24076] Don't warn for <FH> err FOO. */
    {
	const OP * const k1 = ((UNOP*)first)->op_first;
	const OP * const k2 = k1->op_sibling;
	OPCODE warnop = 0;
	switch (first->op_type)
	{
	case OP_NULL:
	    if (k2 && k2->op_type == OP_READLINE
		  && (k2->op_flags & OPf_STACKED)
		  && ((k1->op_flags & OPf_WANT) == OPf_WANT_SCALAR))
	    {
		warnop = k2->op_type;
	    }
	    break;

	case OP_SASSIGN:
	    if (k1->op_type == OP_READDIR
		  || k1->op_type == OP_GLOB
		  || (k1->op_type == OP_NULL && k1->op_targ == OP_GLOB)
                 || k1->op_type == OP_EACH
                 || k1->op_type == OP_AEACH)
	    {
		warnop = ((k1->op_type == OP_NULL)
			  ? (OPCODE)k1->op_targ : k1->op_type);
	    }
	    break;
	}
	if (warnop) {
	    const line_t oldline = CopLINE(PL_curcop);
            /* This ensures that warnings are reported at the first line
               of the construction, not the last.  */
	    CopLINE_set(PL_curcop, PL_parser->copline);
	    Perl_warner(aTHX_ packWARN(WARN_MISC),
		 "Value of %s%s can be \"0\"; test with defined()",
		 PL_op_desc[warnop],
		 ((warnop == OP_READLINE || warnop == OP_GLOB)
		  ? " construct" : "() operator"));
	    CopLINE_set(PL_curcop, oldline);
	}
    }

    if (!other)
	return first;

    if (type == OP_ANDASSIGN || type == OP_ORASSIGN || type == OP_DORASSIGN)
	other->op_private |= OPpASSIGN_BACKWARDS;  /* other is an OP_SASSIGN */

    NewOp(1101, logop, 1, LOGOP);

    logop->op_type = (OPCODE)type;
    logop->op_ppaddr = PL_ppaddr[type];
    logop->op_first = first;
    logop->op_flags = (U8)(flags | OPf_KIDS);
    logop->op_other = LINKLIST(other);
    logop->op_private = (U8)(1 | (flags >> 8));

    /* establish postfix order */
    logop->op_next = LINKLIST(first);
    first->op_next = (OP*)logop;
    first->op_sibling = other;

    CHECKOP(type,logop);

    o = newUNOP(prepend_not ? OP_NOT : OP_NULL, 0, (OP*)logop);
    other->op_next = o;

    return o;
}

/*
=for apidoc Am|OP *|newCONDOP|I32 flags|OP *first|OP *trueop|OP *falseop

Constructs, checks, and returns a conditional-expression (C<cond_expr>)
op.  I<flags> gives the eight bits of C<op_flags>, except that C<OPf_KIDS>
will be set automatically, and, shifted up eight bits, the eight bits of
C<op_private>, except that the bit with value 1 is automatically set.
I<first> supplies the expression selecting between the two branches,
and I<trueop> and I<falseop> supply the branches; they are consumed by
this function and become part of the constructed op tree.

=cut
*/

OP *
Perl_newCONDOP(pTHX_ I32 flags, OP *first, OP *trueop, OP *falseop)
{
    dVAR;
    LOGOP *logop;
    OP *start;
    OP *o;
    OP *cstop;

    PERL_ARGS_ASSERT_NEWCONDOP;

    if (!falseop)
	return newLOGOP(OP_AND, 0, first, trueop);
    if (!trueop)
	return newLOGOP(OP_OR, 0, first, falseop);

    scalarboolean(first);
    if ((cstop = search_const(first))) {
	/* Left or right arm of the conditional?  */
	const bool left = SvTRUE(((SVOP*)cstop)->op_sv);
	OP *live = left ? trueop : falseop;
	OP *const dead = left ? falseop : trueop;
        if (cstop->op_private & OPpCONST_BARE &&
	    cstop->op_private & OPpCONST_STRICT) {
	    no_bareword_allowed(cstop);
	}
	if (PL_madskills) {
	    /* This is all dead code when PERL_MAD is not defined.  */
	    live = newUNOP(OP_NULL, 0, live);
	    op_getmad(first, live, 'C');
	    op_getmad(dead, live, left ? 'e' : 't');
	} else {
	    op_free(first);
	    op_free(dead);
	}
	if (live->op_type == OP_LEAVE)
	    live = newUNOP(OP_NULL, OPf_SPECIAL, live);
	else if (live->op_type == OP_MATCH || live->op_type == OP_SUBST
	      || live->op_type == OP_TRANS || live->op_type == OP_TRANSR)
	    /* Mark the op as being unbindable with =~ */
	    live->op_flags |= OPf_SPECIAL;
	live->op_folded = 1;
	return live;
    }
    NewOp(1101, logop, 1, LOGOP);
    logop->op_type = OP_COND_EXPR;
    logop->op_ppaddr = PL_ppaddr[OP_COND_EXPR];
    logop->op_first = first;
    logop->op_flags = (U8)(flags | OPf_KIDS);
    logop->op_private = (U8)(1 | (flags >> 8));
    logop->op_other = LINKLIST(trueop);
    logop->op_next = LINKLIST(falseop);

    CHECKOP(OP_COND_EXPR, /* that's logop->op_type */
	    logop);

    /* establish postfix order */
    start = LINKLIST(first);
    first->op_next = (OP*)logop;

    first->op_sibling = trueop;
    trueop->op_sibling = falseop;
    o = newUNOP(OP_NULL, 0, (OP*)logop);

    trueop->op_next = falseop->op_next = o;

    o->op_next = start;
    return o;
}

/*
=for apidoc Am|OP *|newRANGE|I32 flags|OP *left|OP *right

Constructs and returns a C<range> op, with subordinate C<flip> and
C<flop> ops.  I<flags> gives the eight bits of C<op_flags> for the
C<flip> op and, shifted up eight bits, the eight bits of C<op_private>
for both the C<flip> and C<range> ops, except that the bit with value
1 is automatically set.  I<left> and I<right> supply the expressions
controlling the endpoints of the range; they are consumed by this function
and become part of the constructed op tree.

=cut
*/

OP *
Perl_newRANGE(pTHX_ I32 flags, OP *left, OP *right)
{
    dVAR;
    LOGOP *range;
    OP *flip;
    OP *flop;
    OP *leftstart;
    OP *o;

    PERL_ARGS_ASSERT_NEWRANGE;

    NewOp(1101, range, 1, LOGOP);

    range->op_type = OP_RANGE;
    range->op_ppaddr = PL_ppaddr[OP_RANGE];
    range->op_first = left;
    range->op_flags = OPf_KIDS;
    leftstart = LINKLIST(left);
    range->op_other = LINKLIST(right);
    range->op_private = (U8)(1 | (flags >> 8));

    left->op_sibling = right;

    range->op_next = (OP*)range;
    flip = newUNOP(OP_FLIP, flags, (OP*)range);
    flop = newUNOP(OP_FLOP, 0, flip);
    o = newUNOP(OP_NULL, 0, flop);
    LINKLIST(flop);
    range->op_next = leftstart;

    left->op_next = flip;
    right->op_next = flop;

    range->op_targ = pad_alloc(OP_RANGE, SVs_PADMY);
    sv_upgrade(PAD_SV(range->op_targ), SVt_PVNV);
    flip->op_targ = pad_alloc(OP_RANGE, SVs_PADMY);
    sv_upgrade(PAD_SV(flip->op_targ), SVt_PVNV);

    flip->op_private =  left->op_type == OP_CONST ? OPpFLIP_LINENUM : 0;
    flop->op_private = right->op_type == OP_CONST ? OPpFLIP_LINENUM : 0;

    /* check barewords before they might be optimized aways */
    if (flip->op_private && cSVOPx(left)->op_private & OPpCONST_STRICT)
	no_bareword_allowed(left);
    if (flop->op_private && cSVOPx(right)->op_private & OPpCONST_STRICT)
	no_bareword_allowed(right);

    flip->op_next = o;
    if (!flip->op_private || !flop->op_private)
	LINKLIST(o);		/* blow off optimizer unless constant */

    return o;
}

/*
=for apidoc Am|OP *|newLOOPOP|I32 flags|I32 debuggable|OP *expr|OP *block

Constructs, checks, and returns an op tree expressing a loop.  This is
only a loop in the control flow through the op tree; it does not have
the heavyweight loop structure that allows exiting the loop by C<last>
and suchlike.  I<flags> gives the eight bits of C<op_flags> for the
top-level op, except that some bits will be set automatically as required.
I<expr> supplies the expression controlling loop iteration, and I<block>
supplies the body of the loop; they are consumed by this function and
become part of the constructed op tree.  I<debuggable> is currently
unused and should always be 1.

=cut
*/

OP *
Perl_newLOOPOP(pTHX_ I32 flags, I32 debuggable, OP *expr, OP *block)
{
    dVAR;
    OP* listop;
    OP* o;
    const bool once = block && block->op_flags & OPf_SPECIAL &&
		      block->op_type == OP_NULL;

    PERL_UNUSED_ARG(debuggable);

    if (expr) {
	if (once && (
	      (expr->op_type == OP_CONST && !SvTRUE(((SVOP*)expr)->op_sv))
	   || (  expr->op_type == OP_NOT
	      && cUNOPx(expr)->op_first->op_type == OP_CONST
	      && SvTRUE(cSVOPx_sv(cUNOPx(expr)->op_first))
	      )
	   ))
	    /* Return the block now, so that S_new_logop does not try to
	       fold it away. */
	    return block;	/* do {} while 0 does once */
	if (expr->op_type == OP_READLINE
	    || expr->op_type == OP_READDIR
	    || expr->op_type == OP_GLOB
	    || expr->op_type == OP_EACH || expr->op_type == OP_AEACH
	    || (expr->op_type == OP_NULL && expr->op_targ == OP_GLOB)) {
	    expr = newUNOP(OP_DEFINED, 0,
		newASSIGNOP(0, newDEFSVOP(), 0, expr) );
	} else if (expr->op_flags & OPf_KIDS) {
	    const OP * const k1 = ((UNOP*)expr)->op_first;
	    const OP * const k2 = k1 ? k1->op_sibling : NULL;
	    switch (expr->op_type) {
	      case OP_NULL:
		if (k2 && (k2->op_type == OP_READLINE || k2->op_type == OP_READDIR)
		      && (k2->op_flags & OPf_STACKED)
		      && ((k1->op_flags & OPf_WANT) == OPf_WANT_SCALAR))
		    expr = newUNOP(OP_DEFINED, 0, expr);
		break;

	      case OP_SASSIGN:
		if (k1 && (k1->op_type == OP_READDIR
		      || k1->op_type == OP_GLOB
		      || (k1->op_type == OP_NULL && k1->op_targ == OP_GLOB)
                     || k1->op_type == OP_EACH
                     || k1->op_type == OP_AEACH))
		    expr = newUNOP(OP_DEFINED, 0, expr);
		break;
	    }
	}
    }

    /* if block is null, the next op_append_elem() would put UNSTACK, a scalar
     * op, in listop. This is wrong. [perl #27024] */
    if (!block)
	block = newOP(OP_NULL, 0);
    listop = op_append_elem(OP_LINESEQ, block, newOP(OP_UNSTACK, 0));
    o = new_logop(OP_AND, 0, &expr, &listop);

    if (once) {
	ASSUME(listop);
    }

    if (listop)
	((LISTOP*)listop)->op_last->op_next = LINKLIST(o);

    if (once && o != listop)
    {
	assert(cUNOPo->op_first->op_type == OP_AND
	    || cUNOPo->op_first->op_type == OP_OR);
	o->op_next = ((LOGOP*)cUNOPo->op_first)->op_other;
    }

    if (o == listop)
	o = newUNOP(OP_NULL, 0, o);	/* or do {} while 1 loses outer block */

    o->op_flags |= flags;
    o = op_scope(o);
    o->op_flags |= OPf_SPECIAL;	/* suppress POPBLOCK curpm restoration*/
    return o;
}

/*
=for apidoc Am|OP *|newWHILEOP|I32 flags|I32 debuggable|LOOP *loop|OP *expr|OP *block|OP *cont|I32 has_my

Constructs, checks, and returns an op tree expressing a C<while> loop.
This is a heavyweight loop, with structure that allows exiting the loop
by C<last> and suchlike.

I<loop> is an optional preconstructed C<enterloop> op to use in the
loop; if it is null then a suitable op will be constructed automatically.
I<expr> supplies the loop's controlling expression.  I<block> supplies the
main body of the loop, and I<cont> optionally supplies a C<continue> block
that operates as a second half of the body.  All of these optree inputs
are consumed by this function and become part of the constructed op tree.

I<flags> gives the eight bits of C<op_flags> for the C<leaveloop>
op and, shifted up eight bits, the eight bits of C<op_private> for
the C<leaveloop> op, except that (in both cases) some bits will be set
automatically.  I<debuggable> is currently unused and should always be 1.
I<has_my> can be supplied as true to force the
loop body to be enclosed in its own scope.

=cut
*/

OP *
Perl_newWHILEOP(pTHX_ I32 flags, I32 debuggable, LOOP *loop,
	OP *expr, OP *block, OP *cont, I32 has_my)
{
    dVAR;
    OP *redo;
    OP *next = NULL;
    OP *listop;
    OP *o;
    U8 loopflags = 0;

    PERL_UNUSED_ARG(debuggable);

    if (expr) {
	if (expr->op_type == OP_READLINE
         || expr->op_type == OP_READDIR
         || expr->op_type == OP_GLOB
	 || expr->op_type == OP_EACH || expr->op_type == OP_AEACH
		     || (expr->op_type == OP_NULL && expr->op_targ == OP_GLOB)) {
	    expr = newUNOP(OP_DEFINED, 0,
		newASSIGNOP(0, newDEFSVOP(), 0, expr) );
	} else if (expr->op_flags & OPf_KIDS) {
	    const OP * const k1 = ((UNOP*)expr)->op_first;
	    const OP * const k2 = (k1) ? k1->op_sibling : NULL;
	    switch (expr->op_type) {
	      case OP_NULL:
		if (k2 && (k2->op_type == OP_READLINE || k2->op_type == OP_READDIR)
		      && (k2->op_flags & OPf_STACKED)
		      && ((k1->op_flags & OPf_WANT) == OPf_WANT_SCALAR))
		    expr = newUNOP(OP_DEFINED, 0, expr);
		break;

	      case OP_SASSIGN:
		if (k1 && (k1->op_type == OP_READDIR
		      || k1->op_type == OP_GLOB
		      || (k1->op_type == OP_NULL && k1->op_targ == OP_GLOB)
                     || k1->op_type == OP_EACH
                     || k1->op_type == OP_AEACH))
		    expr = newUNOP(OP_DEFINED, 0, expr);
		break;
	    }
	}
    }

    if (!block)
	block = newOP(OP_NULL, 0);
    else if (cont || has_my) {
	block = op_scope(block);
    }

    if (cont) {
	next = LINKLIST(cont);
    }
    if (expr) {
	OP * const unstack = newOP(OP_UNSTACK, 0);
	if (!next)
	    next = unstack;
	cont = op_append_elem(OP_LINESEQ, cont, unstack);
    }

    assert(block);
    listop = op_append_list(OP_LINESEQ, block, cont);
    assert(listop);
    redo = LINKLIST(listop);

    if (expr) {
	scalar(listop);
	o = new_logop(OP_AND, 0, &expr, &listop);
	if (o == expr && o->op_type == OP_CONST && !SvTRUE(cSVOPo->op_sv)) {
	    op_free((OP*)loop);
	    return expr;		/* listop already freed by new_logop */
	}
	if (listop)
	    ((LISTOP*)listop)->op_last->op_next =
		(o == listop ? redo : LINKLIST(o));
    }
    else
	o = listop;

    if (!loop) {
	NewOp(1101,loop,1,LOOP);
	loop->op_type = OP_ENTERLOOP;
	loop->op_ppaddr = PL_ppaddr[OP_ENTERLOOP];
	loop->op_private = 0;
	loop->op_next = (OP*)loop;
    }

    o = newBINOP(OP_LEAVELOOP, 0, (OP*)loop, o);

    loop->op_redoop = redo;
    loop->op_lastop = o;
    o->op_private |= loopflags;

    if (next)
	loop->op_nextop = next;
    else
	loop->op_nextop = o;

    o->op_flags |= flags;
    o->op_private |= (flags >> 8);
    return o;
}

/*
=for apidoc Am|OP *|newFOROP|I32 flags|OP *sv|OP *expr|OP *block|OP *cont

Constructs, checks, and returns an op tree expressing a C<foreach>
loop (iteration through a list of values).  This is a heavyweight loop,
with structure that allows exiting the loop by C<last> and suchlike.

I<sv> optionally supplies the variable that will be aliased to each
item in turn; if null, it defaults to C<$_> (either lexical or global).
I<expr> supplies the list of values to iterate over.  I<block> supplies
the main body of the loop, and I<cont> optionally supplies a C<continue>
block that operates as a second half of the body.  All of these optree
inputs are consumed by this function and become part of the constructed
op tree.

I<flags> gives the eight bits of C<op_flags> for the C<leaveloop>
op and, shifted up eight bits, the eight bits of C<op_private> for
the C<leaveloop> op, except that (in both cases) some bits will be set
automatically.

=cut
*/

OP *
Perl_newFOROP(pTHX_ I32 flags, OP *sv, OP *expr, OP *block, OP *cont)
{
    dVAR;
    LOOP *loop;
    OP *wop;
    PADOFFSET padoff = 0;
    I32 iterflags = 0;
    I32 iterpflags = 0;
    OP *madsv = NULL;

    PERL_ARGS_ASSERT_NEWFOROP;

    if (sv) {
	if (sv->op_type == OP_RV2SV) {	/* symbol table variable */
	    iterpflags = sv->op_private & OPpOUR_INTRO; /* for our $x () */
	    sv->op_type = OP_RV2GV;
	    sv->op_ppaddr = PL_ppaddr[OP_RV2GV];

	    /* The op_type check is needed to prevent a possible segfault
	     * if the loop variable is undeclared and 'strict vars' is in
	     * effect. This is illegal but is nonetheless parsed, so we
	     * may reach this point with an OP_CONST where we're expecting
	     * an OP_GV.
	     */
	    if (cUNOPx(sv)->op_first->op_type == OP_GV
	     && cGVOPx_gv(cUNOPx(sv)->op_first) == PL_defgv)
		iterpflags |= OPpITER_DEF;
	}
	else if (sv->op_type == OP_PADSV) { /* private variable */
	    iterpflags = sv->op_private & OPpLVAL_INTRO; /* for my $x () */
	    padoff = sv->op_targ;
	    if (PL_madskills)
		madsv = sv;
	    else {
		sv->op_targ = 0;
		op_free(sv);
	    }
	    sv = NULL;
	}
	else
	    Perl_croak(aTHX_ "Can't use %s for loop variable", PL_op_desc[sv->op_type]);
	if (padoff) {
	    SV *const namesv = PAD_COMPNAME_SV(padoff);
	    STRLEN len;
	    const char *const name = SvPV_const(namesv, len);

	    if (len == 2 && name[0] == '$' && name[1] == '_')
		iterpflags |= OPpITER_DEF;
	}
    }
    else {
        const PADOFFSET offset = pad_findmy_pvs("$_", 0);
	if (offset == NOT_IN_PAD || PAD_COMPNAME_FLAGS_isOUR(offset)) {
	    sv = newGVOP(OP_GV, 0, PL_defgv);
	}
	else {
	    padoff = offset;
	}
	iterpflags |= OPpITER_DEF;
    }
    if (expr->op_type == OP_RV2AV || expr->op_type == OP_PADAV) {
	expr = op_lvalue(force_list(scalar(ref(expr, OP_ITER))), OP_GREPSTART);
	iterflags |= OPf_STACKED;
    }
    else if (expr->op_type == OP_NULL &&
             (expr->op_flags & OPf_KIDS) &&
             ((BINOP*)expr)->op_first->op_type == OP_FLOP)
    {
	/* Basically turn for($x..$y) into the same as for($x,$y), but we
	 * set the STACKED flag to indicate that these values are to be
	 * treated as min/max values by 'pp_enteriter'.
	 */
	const UNOP* const flip = (UNOP*)((UNOP*)((BINOP*)expr)->op_first)->op_first;
	LOGOP* const range = (LOGOP*) flip->op_first;
	OP* const left  = range->op_first;
	OP* const right = left->op_sibling;
	LISTOP* listop;

	range->op_flags &= ~OPf_KIDS;
	range->op_first = NULL;

	listop = (LISTOP*)newLISTOP(OP_LIST, 0, left, right);
	listop->op_first->op_next = range->op_next;
	left->op_next = range->op_other;
	right->op_next = (OP*)listop;
	listop->op_next = listop->op_first;

#ifdef PERL_MAD
	op_getmad(expr,(OP*)listop,'O');
#else
	op_free(expr);
#endif
	expr = (OP*)(listop);
        op_null(expr);
	iterflags |= OPf_STACKED;
    }
    else {
        expr = op_lvalue(force_list(expr), OP_GREPSTART);
    }

    loop = (LOOP*)list(convert(OP_ENTERITER, iterflags,
			       op_append_elem(OP_LIST, expr, scalar(sv))));
    assert(!loop->op_next);
    /* for my  $x () sets OPpLVAL_INTRO;
     * for our $x () sets OPpOUR_INTRO */
    loop->op_private = (U8)iterpflags;
    if (loop->op_slabbed
     && DIFF(loop, OpSLOT(loop)->opslot_next)
	 < SIZE_TO_PSIZE(sizeof(LOOP)))
    {
	LOOP *tmp;
	NewOp(1234,tmp,1,LOOP);
	Copy(loop,tmp,1,LISTOP);
	S_op_destroy(aTHX_ (OP*)loop);
	loop = tmp;
    }
    else if (!loop->op_slabbed)
	loop = (LOOP*)PerlMemShared_realloc(loop, sizeof(LOOP));
    loop->op_targ = padoff;
    wop = newWHILEOP(flags, 1, loop, newOP(OP_ITER, 0), block, cont, 0);
    if (madsv)
	op_getmad(madsv, (OP*)loop, 'v');
    return wop;
}

/*
=for apidoc Am|OP *|newLOOPEX|I32 type|OP *label

Constructs, checks, and returns a loop-exiting op (such as C<goto>
or C<last>).  I<type> is the opcode.  I<label> supplies the parameter
determining the target of the op; it is consumed by this function and
becomes part of the constructed op tree.

=cut
*/

OP*
Perl_newLOOPEX(pTHX_ I32 type, OP *label)
{
    dVAR;
    OP *o = NULL;

    PERL_ARGS_ASSERT_NEWLOOPEX;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_LOOPEXOP);

    if (type != OP_GOTO) {
	/* "last()" means "last" */
	if (label->op_type == OP_STUB && (label->op_flags & OPf_PARENS)) {
	    o = newOP(type, OPf_SPECIAL);
	}
    }
    else {
	/* Check whether it's going to be a goto &function */
	if (label->op_type == OP_ENTERSUB
		&& !(label->op_flags & OPf_STACKED))
	    label = newUNOP(OP_REFGEN, 0, op_lvalue(label, OP_REFGEN));
    }

    /* Check for a constant argument */
    if (label->op_type == OP_CONST) {
	    SV * const sv = ((SVOP *)label)->op_sv;
	    STRLEN l;
	    const char *s = SvPV_const(sv,l);
	    if (l == strlen(s)) {
		o = newPVOP(type,
			    SvUTF8(((SVOP*)label)->op_sv),
			    savesharedpv(
				SvPV_nolen_const(((SVOP*)label)->op_sv)));
	    }
    }
    
    /* If we have already created an op, we do not need the label. */
    if (o)
#ifdef PERL_MAD
		op_getmad(label,o,'L');
#else
		op_free(label);
#endif
    else o = newUNOP(type, OPf_STACKED, label);

    PL_hints |= HINT_BLOCK_SCOPE;
    return o;
}

/* if the condition is a literal array or hash
   (or @{ ... } etc), make a reference to it.
 */
STATIC OP *
S_ref_array_or_hash(pTHX_ OP *cond)
{
    if (cond
    && (cond->op_type == OP_RV2AV
    ||  cond->op_type == OP_PADAV
    ||  cond->op_type == OP_RV2HV
    ||  cond->op_type == OP_PADHV))

	return newUNOP(OP_REFGEN, 0, op_lvalue(cond, OP_REFGEN));

    else if(cond
    && (cond->op_type == OP_ASLICE
    ||  cond->op_type == OP_KVASLICE
    ||  cond->op_type == OP_HSLICE
    ||  cond->op_type == OP_KVHSLICE)) {

	/* anonlist now needs a list from this op, was previously used in
	 * scalar context */
	cond->op_flags |= ~(OPf_WANT_SCALAR | OPf_REF);
	cond->op_flags |= OPf_WANT_LIST;

	return newANONLIST(op_lvalue(cond, OP_ANONLIST));
    }

    else
	return cond;
}

/* These construct the optree fragments representing given()
   and when() blocks.

   entergiven and enterwhen are LOGOPs; the op_other pointer
   points up to the associated leave op. We need this so we
   can put it in the context and make break/continue work.
   (Also, of course, pp_enterwhen will jump straight to
   op_other if the match fails.)
 */

STATIC OP *
S_newGIVWHENOP(pTHX_ OP *cond, OP *block,
		   I32 enter_opcode, I32 leave_opcode,
		   PADOFFSET entertarg)
{
    dVAR;
    LOGOP *enterop;
    OP *o;

    PERL_ARGS_ASSERT_NEWGIVWHENOP;

    NewOp(1101, enterop, 1, LOGOP);
    enterop->op_type = (Optype)enter_opcode;
    enterop->op_ppaddr = PL_ppaddr[enter_opcode];
    enterop->op_flags =  (U8) OPf_KIDS;
    enterop->op_targ = ((entertarg == NOT_IN_PAD) ? 0 : entertarg);
    enterop->op_private = 0;

    o = newUNOP(leave_opcode, 0, (OP *) enterop);

    if (cond) {
	enterop->op_first = scalar(cond);
	cond->op_sibling = block;

	o->op_next = LINKLIST(cond);
	cond->op_next = (OP *) enterop;
    }
    else {
	/* This is a default {} block */
	enterop->op_first = block;
	enterop->op_flags |= OPf_SPECIAL;
	o      ->op_flags |= OPf_SPECIAL;

	o->op_next = (OP *) enterop;
    }

    CHECKOP(enter_opcode, enterop); /* Currently does nothing, since
    				       entergiven and enterwhen both
    				       use ck_null() */

    enterop->op_next = LINKLIST(block);
    block->op_next = enterop->op_other = o;

    return o;
}

/* Does this look like a boolean operation? For these purposes
   a boolean operation is:
     - a subroutine call [*]
     - a logical connective
     - a comparison operator
     - a filetest operator, with the exception of -s -M -A -C
     - defined(), exists() or eof()
     - /$re/ or $foo =~ /$re/
   
   [*] possibly surprising
 */
STATIC bool
S_looks_like_bool(pTHX_ const OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_LOOKS_LIKE_BOOL;

    switch(o->op_type) {
	case OP_OR:
	case OP_DOR:
	    return looks_like_bool(cLOGOPo->op_first);

	case OP_AND:
	    return (
	    	looks_like_bool(cLOGOPo->op_first)
	     && looks_like_bool(cLOGOPo->op_first->op_sibling));

	case OP_NULL:
	case OP_SCALAR:
	    return (
		o->op_flags & OPf_KIDS
	    && looks_like_bool(cUNOPo->op_first));

	case OP_ENTERSUB:

	case OP_NOT:	case OP_XOR:

	case OP_EQ:	case OP_NE:	case OP_LT:
	case OP_GT:	case OP_LE:	case OP_GE:

	case OP_I_EQ:	case OP_I_NE:	case OP_I_LT:
	case OP_I_GT:	case OP_I_LE:	case OP_I_GE:

	case OP_SEQ:	case OP_SNE:	case OP_SLT:
	case OP_SGT:	case OP_SLE:	case OP_SGE:
	
	case OP_SMARTMATCH:
	
	case OP_FTRREAD:  case OP_FTRWRITE: case OP_FTREXEC:
	case OP_FTEREAD:  case OP_FTEWRITE: case OP_FTEEXEC:
	case OP_FTIS:     case OP_FTEOWNED: case OP_FTROWNED:
	case OP_FTZERO:   case OP_FTSOCK:   case OP_FTCHR:
	case OP_FTBLK:    case OP_FTFILE:   case OP_FTDIR:
	case OP_FTPIPE:   case OP_FTLINK:   case OP_FTSUID:
	case OP_FTSGID:   case OP_FTSVTX:   case OP_FTTTY:
	case OP_FTTEXT:   case OP_FTBINARY:
	
	case OP_DEFINED: case OP_EXISTS:
	case OP_MATCH:	 case OP_EOF:

	case OP_FLOP:

	    return TRUE;
	
	case OP_CONST:
	    /* Detect comparisons that have been optimized away */
	    if (cSVOPo->op_sv == &PL_sv_yes
	    ||  cSVOPo->op_sv == &PL_sv_no)
	    
		return TRUE;
	    else
		return FALSE;

	/* FALL THROUGH */
	default:
	    return FALSE;
    }
}

/*
=for apidoc Am|OP *|newGIVENOP|OP *cond|OP *block|PADOFFSET defsv_off

Constructs, checks, and returns an op tree expressing a C<given> block.
I<cond> supplies the expression that will be locally assigned to a lexical
variable, and I<block> supplies the body of the C<given> construct; they
are consumed by this function and become part of the constructed op tree.
I<defsv_off> is the pad offset of the scalar lexical variable that will
be affected.  If it is 0, the global $_ will be used.

=cut
*/

OP *
Perl_newGIVENOP(pTHX_ OP *cond, OP *block, PADOFFSET defsv_off)
{
    dVAR;
    PERL_ARGS_ASSERT_NEWGIVENOP;
    return newGIVWHENOP(
    	ref_array_or_hash(cond),
    	block,
	OP_ENTERGIVEN, OP_LEAVEGIVEN,
	defsv_off);
}

/*
=for apidoc Am|OP *|newWHENOP|OP *cond|OP *block

Constructs, checks, and returns an op tree expressing a C<when> block.
I<cond> supplies the test expression, and I<block> supplies the block
that will be executed if the test evaluates to true; they are consumed
by this function and become part of the constructed op tree.  I<cond>
will be interpreted DWIMically, often as a comparison against C<$_>,
and may be null to generate a C<default> block.

=cut
*/

OP *
Perl_newWHENOP(pTHX_ OP *cond, OP *block)
{
    const bool cond_llb = (!cond || looks_like_bool(cond));
    OP *cond_op;

    PERL_ARGS_ASSERT_NEWWHENOP;

    if (cond_llb)
	cond_op = cond;
    else {
	cond_op = newBINOP(OP_SMARTMATCH, OPf_SPECIAL,
		newDEFSVOP(),
		scalar(ref_array_or_hash(cond)));
    }
    
    return newGIVWHENOP(cond_op, block, OP_ENTERWHEN, OP_LEAVEWHEN, 0);
}

void
Perl_cv_ckproto_len_flags(pTHX_ const CV *cv, const GV *gv, const char *p,
		    const STRLEN len, const U32 flags)
{
    SV *name = NULL, *msg;
    const char * cvp = SvROK(cv) ? "" : CvPROTO(cv);
    STRLEN clen = CvPROTOLEN(cv), plen = len;

    PERL_ARGS_ASSERT_CV_CKPROTO_LEN_FLAGS;

    if (p == NULL && cvp == NULL)
	return;

    if (!ckWARN_d(WARN_PROTOTYPE))
	return;

    if (p && cvp) {
	p = S_strip_spaces(aTHX_ p, &plen);
	cvp = S_strip_spaces(aTHX_ cvp, &clen);
	if ((flags & SVf_UTF8) == SvUTF8(cv)) {
	    if (plen == clen && memEQ(cvp, p, plen))
		return;
	} else {
	    if (flags & SVf_UTF8) {
		if (bytes_cmp_utf8((const U8 *)cvp, clen, (const U8 *)p, plen) == 0)
		    return;
            }
	    else {
		if (bytes_cmp_utf8((const U8 *)p, plen, (const U8 *)cvp, clen) == 0)
		    return;
	    }
	}
    }

    msg = sv_newmortal();

    if (gv)
    {
	if (isGV(gv))
	    gv_efullname3(name = sv_newmortal(), gv, NULL);
	else if (SvPOK(gv) && *SvPVX((SV *)gv) == '&')
	    name = newSVpvn_flags(SvPVX((SV *)gv)+1, SvCUR(gv)-1, SvUTF8(gv)|SVs_TEMP);
	else name = (SV *)gv;
    }
    sv_setpvs(msg, "Prototype mismatch:");
    if (name)
	Perl_sv_catpvf(aTHX_ msg, " sub %"SVf, SVfARG(name));
    if (cvp)
	Perl_sv_catpvf(aTHX_ msg, " (%"UTF8f")", 
	    UTF8fARG(SvUTF8(cv),clen,cvp)
	);
    else
	sv_catpvs(msg, ": none");
    sv_catpvs(msg, " vs ");
    if (p)
	Perl_sv_catpvf(aTHX_ msg, "(%"UTF8f")", UTF8fARG(flags & SVf_UTF8,len,p));
    else
	sv_catpvs(msg, "none");
    Perl_warner(aTHX_ packWARN(WARN_PROTOTYPE), "%"SVf, SVfARG(msg));
}

static void const_sv_xsub(pTHX_ CV* cv);
static void const_av_xsub(pTHX_ CV* cv);

/*

=head1 Optree Manipulation Functions

=for apidoc cv_const_sv

If C<cv> is a constant sub eligible for inlining, returns the constant
value returned by the sub.  Otherwise, returns NULL.

Constant subs can be created with C<newCONSTSUB> or as described in
L<perlsub/"Constant Functions">.

=cut
*/
SV *
Perl_cv_const_sv(pTHX_ const CV *const cv)
{
    SV *sv;
    PERL_UNUSED_CONTEXT;
    if (!cv)
	return NULL;
    if (!(SvTYPE(cv) == SVt_PVCV || SvTYPE(cv) == SVt_PVFM))
	return NULL;
    sv = CvCONST(cv) ? MUTABLE_SV(CvXSUBANY(cv).any_ptr) : NULL;
    if (sv && SvTYPE(sv) == SVt_PVAV) return NULL;
    return sv;
}

SV *
Perl_cv_const_sv_or_av(pTHX_ const CV * const cv)
{
    PERL_UNUSED_CONTEXT;
    if (!cv)
	return NULL;
    assert (SvTYPE(cv) == SVt_PVCV || SvTYPE(cv) == SVt_PVFM);
    return CvCONST(cv) ? MUTABLE_SV(CvXSUBANY(cv).any_ptr) : NULL;
}

/* op_const_sv:  examine an optree to determine whether it's in-lineable.
 * Can be called in 3 ways:
 *
 * !cv
 * 	look for a single OP_CONST with attached value: return the value
 *
 * cv && CvCLONE(cv) && !CvCONST(cv)
 *
 * 	examine the clone prototype, and if contains only a single
 * 	OP_CONST referencing a pad const, or a single PADSV referencing
 * 	an outer lexical, return a non-zero value to indicate the CV is
 * 	a candidate for "constizing" at clone time
 *
 * cv && CvCONST(cv)
 *
 *	We have just cloned an anon prototype that was marked as a const
 *	candidate. Try to grab the current value, and in the case of
 *	PADSV, ignore it if it has multiple references. In this case we
 *	return a newly created *copy* of the value.
 */

SV *
Perl_op_const_sv(pTHX_ const OP *o, CV *cv)
{
    dVAR;
    SV *sv = NULL;

    if (PL_madskills)
	return NULL;

    if (!o)
	return NULL;

    if (o->op_type == OP_LINESEQ && cLISTOPo->op_first)
	o = cLISTOPo->op_first->op_sibling;

    for (; o; o = o->op_next) {
	const OPCODE type = o->op_type;

	if (sv && o->op_next == o)
	    return sv;
	if (o->op_next != o) {
	    if (type == OP_NEXTSTATE
	     || (type == OP_NULL && !(o->op_flags & OPf_KIDS))
	     || type == OP_PUSHMARK)
		continue;
	    if (type == OP_DBSTATE)
		continue;
	}
	if (type == OP_LEAVESUB || type == OP_RETURN)
	    break;
	if (sv)
	    return NULL;
	if (type == OP_CONST && cSVOPo->op_sv)
	    sv = cSVOPo->op_sv;
	else if (cv && type == OP_CONST) {
	    sv = PAD_BASE_SV(CvPADLIST(cv), o->op_targ);
	    if (!sv)
		return NULL;
	}
	else if (cv && type == OP_PADSV) {
	    if (CvCONST(cv)) { /* newly cloned anon */
		sv = PAD_BASE_SV(CvPADLIST(cv), o->op_targ);
		/* the candidate should have 1 ref from this pad and 1 ref
		 * from the parent */
		if (!sv || SvREFCNT(sv) != 2)
		    return NULL;
		sv = newSVsv(sv);
		SvREADONLY_on(sv);
		return sv;
	    }
	    else {
		if (PAD_COMPNAME_FLAGS(o->op_targ) & SVf_FAKE)
		    sv = &PL_sv_undef; /* an arbitrary non-null value */
	    }
	}
	else {
	    return NULL;
	}
    }
    return sv;
}

static bool
S_already_defined(pTHX_ CV *const cv, OP * const block, OP * const o,
			PADNAME * const name, SV ** const const_svp)
{
    assert (cv);
    assert (o || name);
    assert (const_svp);
    if ((!block
#ifdef PERL_MAD
	 || block->op_type == OP_NULL
#endif
	 )) {
	if (CvFLAGS(PL_compcv)) {
	    /* might have had built-in attrs applied */
	    const bool pureperl = !CvISXSUB(cv) && CvROOT(cv);
	    if (CvLVALUE(PL_compcv) && ! CvLVALUE(cv) && pureperl
	     && ckWARN(WARN_MISC))
	    {
		/* protect against fatal warnings leaking compcv */
		SAVEFREESV(PL_compcv);
		Perl_warner(aTHX_ packWARN(WARN_MISC), "lvalue attribute ignored after the subroutine has been defined");
		SvREFCNT_inc_simple_void_NN(PL_compcv);
	    }
	    CvFLAGS(cv) |=
		(CvFLAGS(PL_compcv) & CVf_BUILTIN_ATTRS
		  & ~(CVf_LVALUE * pureperl));
	}
	return FALSE;
    }

    /* redundant check for speed: */
    if (CvCONST(cv) || ckWARN(WARN_REDEFINE)) {
	const line_t oldline = CopLINE(PL_curcop);
	SV *namesv = o
	    ? cSVOPo->op_sv
	    : sv_2mortal(newSVpvn_utf8(
		PadnamePV(name)+1,PadnameLEN(name)-1, PadnameUTF8(name)
	      ));
	if (PL_parser && PL_parser->copline != NOLINE)
            /* This ensures that warnings are reported at the first
               line of a redefinition, not the last.  */
	    CopLINE_set(PL_curcop, PL_parser->copline);
	/* protect against fatal warnings leaking compcv */
	SAVEFREESV(PL_compcv);
	report_redefined_cv(namesv, cv, const_svp);
	SvREFCNT_inc_simple_void_NN(PL_compcv);
	CopLINE_set(PL_curcop, oldline);
    }
#ifdef PERL_MAD
    if (!PL_minus_c)	/* keep old one around for madskills */
#endif
    {
	/* (PL_madskills unset in used file.) */
	SAVEFREESV(cv);
    }
    return TRUE;
}

CV *
Perl_newMYSUB(pTHX_ I32 floor, OP *o, OP *proto, OP *attrs, OP *block)
{
    dVAR;
    CV **spot;
    SV **svspot;
    const char *ps;
    STRLEN ps_len = 0; /* init it to avoid false uninit warning from icc */
    U32 ps_utf8 = 0;
    CV *cv = NULL;
    CV *compcv = PL_compcv;
    SV *const_sv;
    PADNAME *name;
    PADOFFSET pax = o->op_targ;
    CV *outcv = CvOUTSIDE(PL_compcv);
    CV *clonee = NULL;
    HEK *hek = NULL;
    bool reusable = FALSE;

    PERL_ARGS_ASSERT_NEWMYSUB;

    /* Find the pad slot for storing the new sub.
       We cannot use PL_comppad, as it is the pad owned by the new sub.  We
       need to look in CvOUTSIDE and find the pad belonging to the enclos-
       ing sub.  And then we need to dig deeper if this is a lexical from
       outside, as in:
	   my sub foo; sub { sub foo { } }
     */
   redo:
    name = PadlistNAMESARRAY(CvPADLIST(outcv))[pax];
    if (PadnameOUTER(name) && PARENT_PAD_INDEX(name)) {
	pax = PARENT_PAD_INDEX(name);
	outcv = CvOUTSIDE(outcv);
	assert(outcv);
	goto redo;
    }
    svspot =
	&PadARRAY(PadlistARRAY(CvPADLIST(outcv))
			[CvDEPTH(outcv) ? CvDEPTH(outcv) : 1])[pax];
    spot = (CV **)svspot;

    if (!(PL_parser && PL_parser->error_count))
        move_proto_attr(&proto, &attrs, (GV *)name);

    if (proto) {
	assert(proto->op_type == OP_CONST);
	ps = SvPV_const(((SVOP*)proto)->op_sv, ps_len);
        ps_utf8 = SvUTF8(((SVOP*)proto)->op_sv);
    }
    else
	ps = NULL;

    if (!PL_madskills) {
	if (proto)
	    SAVEFREEOP(proto);
	if (attrs)
	    SAVEFREEOP(attrs);
    }

    if (PL_parser && PL_parser->error_count) {
	op_free(block);
	SvREFCNT_dec(PL_compcv);
	PL_compcv = 0;
	goto done;
    }

    if (CvDEPTH(outcv) && CvCLONE(compcv)) {
	cv = *spot;
	svspot = (SV **)(spot = &clonee);
    }
    else if (PadnameIsSTATE(name) || CvDEPTH(outcv))
	cv = *spot;
    else {
	MAGIC *mg;
	SvUPGRADE(name, SVt_PVMG);
	mg = mg_find(name, PERL_MAGIC_proto);
	assert (SvTYPE(*spot) == SVt_PVCV);
	if (CvNAMED(*spot))
	    hek = CvNAME_HEK(*spot);
	else {
	    CvNAME_HEK_set(*spot, hek =
		share_hek(
		    PadnamePV(name)+1,
		    PadnameLEN(name)-1 * (PadnameUTF8(name) ? -1 : 1), 0
		)
	    );
	}
	if (mg) {
	    assert(mg->mg_obj);
	    cv = (CV *)mg->mg_obj;
	}
	else {
	    sv_magic(name, &PL_sv_undef, PERL_MAGIC_proto, NULL, 0);
	    mg = mg_find(name, PERL_MAGIC_proto);
	}
	spot = (CV **)(svspot = &mg->mg_obj);
    }

    if (!block || !ps || *ps || attrs
	|| (CvFLAGS(compcv) & CVf_BUILTIN_ATTRS)
#ifdef PERL_MAD
	|| block->op_type == OP_NULL
#endif
	)
	const_sv = NULL;
    else
	const_sv = op_const_sv(block, NULL);

    if (cv) {
        const bool exists = CvROOT(cv) || CvXSUB(cv);

        /* if the subroutine doesn't exist and wasn't pre-declared
         * with a prototype, assume it will be AUTOLOADed,
         * skipping the prototype check
         */
        if (exists || SvPOK(cv))
            cv_ckproto_len_flags(cv, (GV *)name, ps, ps_len, ps_utf8);
	/* already defined? */
	if (exists) {
	    if (S_already_defined(aTHX_ cv,block,NULL,name,&const_sv))
		cv = NULL;
	    else {
		if (attrs) goto attrs;
		/* just a "sub foo;" when &foo is already defined */
		SAVEFREESV(compcv);
		goto done;
	    }
	}
	else if (CvDEPTH(outcv) && CvCLONE(compcv)) {
	    cv = NULL;
	    reusable = TRUE;
	}
    }
    if (const_sv) {
	SvREFCNT_inc_simple_void_NN(const_sv);
	SvFLAGS(const_sv) = (SvFLAGS(const_sv) & ~SVs_PADMY) | SVs_PADTMP;
	if (cv) {
	    assert(!CvROOT(cv) && !CvCONST(cv));
	    cv_forget_slab(cv);
	}
	else {
	    cv = MUTABLE_CV(newSV_type(SVt_PVCV));
	    CvFILE_set_from_cop(cv, PL_curcop);
	    CvSTASH_set(cv, PL_curstash);
	    *spot = cv;
	}
	sv_setpvs(MUTABLE_SV(cv), "");  /* prototype is "" */
	CvXSUBANY(cv).any_ptr = const_sv;
	CvXSUB(cv) = const_sv_xsub;
	CvCONST_on(cv);
	CvISXSUB_on(cv);
	if (PL_madskills)
	    goto install_block;
	op_free(block);
	SvREFCNT_dec(compcv);
	PL_compcv = NULL;
	goto setname;
    }
    /* Checking whether outcv is CvOUTSIDE(compcv) is not sufficient to
       determine whether this sub definition is in the same scope as its
       declaration.  If this sub definition is inside an inner named pack-
       age sub (my sub foo; sub bar { sub foo { ... } }), outcv points to
       the package sub.  So check PadnameOUTER(name) too.
     */
    if (outcv == CvOUTSIDE(compcv) && !PadnameOUTER(name)) { 
	assert(!CvWEAKOUTSIDE(compcv));
	SvREFCNT_dec(CvOUTSIDE(compcv));
	CvWEAKOUTSIDE_on(compcv);
    }
    /* XXX else do we have a circular reference? */
    if (cv) {	/* must reuse cv in case stub is referenced elsewhere */
	/* transfer PL_compcv to cv */
	if (block
#ifdef PERL_MAD
                  && block->op_type != OP_NULL
#endif
	) {
	    cv_flags_t preserved_flags =
		CvFLAGS(cv) & (CVf_BUILTIN_ATTRS|CVf_NAMED);
	    PADLIST *const temp_padl = CvPADLIST(cv);
	    CV *const temp_cv = CvOUTSIDE(cv);
	    const cv_flags_t other_flags =
		CvFLAGS(cv) & (CVf_SLABBED|CVf_WEAKOUTSIDE);
	    OP * const cvstart = CvSTART(cv);

	    SvPOK_off(cv);
	    CvFLAGS(cv) =
		CvFLAGS(compcv) | preserved_flags;
	    CvOUTSIDE(cv) = CvOUTSIDE(compcv);
	    CvOUTSIDE_SEQ(cv) = CvOUTSIDE_SEQ(compcv);
	    CvPADLIST(cv) = CvPADLIST(compcv);
	    CvOUTSIDE(compcv) = temp_cv;
	    CvPADLIST(compcv) = temp_padl;
	    CvSTART(cv) = CvSTART(compcv);
	    CvSTART(compcv) = cvstart;
	    CvFLAGS(compcv) &= ~(CVf_SLABBED|CVf_WEAKOUTSIDE);
	    CvFLAGS(compcv) |= other_flags;

	    if (CvFILE(cv) && CvDYNFILE(cv)) {
		Safefree(CvFILE(cv));
	    }

	    /* inner references to compcv must be fixed up ... */
	    pad_fixup_inner_anons(CvPADLIST(cv), compcv, cv);
	    if (PERLDB_INTER)/* Advice debugger on the new sub. */
	      ++PL_sub_generation;
	}
	else {
	    /* Might have had built-in attributes applied -- propagate them. */
	    CvFLAGS(cv) |= (CvFLAGS(compcv) & CVf_BUILTIN_ATTRS);
	}
	/* ... before we throw it away */
	SvREFCNT_dec(compcv);
	PL_compcv = compcv = cv;
    }
    else {
	cv = compcv;
	*spot = cv;
    }
   setname:
    if (!CvNAME_HEK(cv)) {
	CvNAME_HEK_set(cv,
	 hek
	  ? share_hek_hek(hek)
	  : share_hek(PadnamePV(name)+1,
		      PadnameLEN(name)-1 * (PadnameUTF8(name) ? -1 : 1),
		      0)
	);
    }
    if (const_sv) goto clone;

    CvFILE_set_from_cop(cv, PL_curcop);
    CvSTASH_set(cv, PL_curstash);

    if (ps) {
	sv_setpvn(MUTABLE_SV(cv), ps, ps_len);
        if ( ps_utf8 ) SvUTF8_on(MUTABLE_SV(cv));
    }

 install_block:
    if (!block)
	goto attrs;

    /* If we assign an optree to a PVCV, then we've defined a subroutine that
       the debugger could be able to set a breakpoint in, so signal to
       pp_entereval that it should not throw away any saved lines at scope
       exit.  */
       
    PL_breakable_sub_gen++;
    /* This makes sub {}; work as expected.  */
    if (block->op_type == OP_STUB) {
	    OP* const newblock = newSTATEOP(0, NULL, 0);
#ifdef PERL_MAD
	    op_getmad(block,newblock,'B');
#else
	    op_free(block);
#endif
	    block = newblock;
    }
    CvROOT(cv) = CvLVALUE(cv)
		   ? newUNOP(OP_LEAVESUBLV, 0,
			     op_lvalue(scalarseq(block), OP_LEAVESUBLV))
		   : newUNOP(OP_LEAVESUB, 0, scalarseq(block));
    CvROOT(cv)->op_private |= OPpREFCOUNTED;
    OpREFCNT_set(CvROOT(cv), 1);
    /* The cv no longer needs to hold a refcount on the slab, as CvROOT
       itself has a refcount. */
    CvSLABBED_off(cv);
    OpslabREFCNT_dec_padok((OPSLAB *)CvSTART(cv));
    CvSTART(cv) = LINKLIST(CvROOT(cv));
    CvROOT(cv)->op_next = 0;
    CALL_PEEP(CvSTART(cv));
    finalize_optree(CvROOT(cv));
    S_prune_chain_head(aTHX_ &CvSTART(cv));

    /* now that optimizer has done its work, adjust pad values */

    pad_tidy(CvCLONE(cv) ? padtidy_SUBCLONE : padtidy_SUB);

    if (CvCLONE(cv)) {
	assert(!CvCONST(cv));
	if (ps && !*ps && op_const_sv(block, cv))
	    CvCONST_on(cv);
    }

  attrs:
    if (attrs) {
	/* Need to do a C<use attributes $stash_of_cv,\&cv,@attrs>. */
	apply_attrs(PL_curstash, MUTABLE_SV(cv), attrs);
    }

    if (block) {
	if (PERLDB_SUBLINE && PL_curstash != PL_debstash) {
	    SV * const tmpstr = sv_newmortal();
	    GV * const db_postponed = gv_fetchpvs("DB::postponed",
						  GV_ADDMULTI, SVt_PVHV);
	    HV *hv;
	    SV * const sv = Perl_newSVpvf(aTHX_ "%s:%ld-%ld",
					  CopFILE(PL_curcop),
					  (long)PL_subline,
					  (long)CopLINE(PL_curcop));
	    if (HvNAME_HEK(PL_curstash)) {
		sv_sethek(tmpstr, HvNAME_HEK(PL_curstash));
		sv_catpvs(tmpstr, "::");
	    }
	    else sv_setpvs(tmpstr, "__ANON__::");
	    sv_catpvn_flags(tmpstr, PadnamePV(name)+1, PadnameLEN(name)-1,
			    PadnameUTF8(name) ? SV_CATUTF8 : SV_CATBYTES);
	    (void)hv_store(GvHV(PL_DBsub), SvPVX_const(tmpstr),
		    SvUTF8(tmpstr) ? -(I32)SvCUR(tmpstr) : (I32)SvCUR(tmpstr), sv, 0);
	    hv = GvHVn(db_postponed);
	    if (HvTOTALKEYS(hv) > 0 && hv_exists(hv, SvPVX_const(tmpstr), SvUTF8(tmpstr) ? -(I32)SvCUR(tmpstr) : (I32)SvCUR(tmpstr))) {
		CV * const pcv = GvCV(db_postponed);
		if (pcv) {
		    dSP;
		    PUSHMARK(SP);
		    XPUSHs(tmpstr);
		    PUTBACK;
		    call_sv(MUTABLE_SV(pcv), G_DISCARD);
		}
	    }
	}
    }

  clone:
    if (clonee) {
	assert(CvDEPTH(outcv));
	spot = (CV **)
	    &PadARRAY(PadlistARRAY(CvPADLIST(outcv))[CvDEPTH(outcv)])[pax];
	if (reusable) cv_clone_into(clonee, *spot);
	else *spot = cv_clone(clonee);
	SvREFCNT_dec_NN(clonee);
	cv = *spot;
	SvPADMY_on(cv);
    }
    if (CvDEPTH(outcv) && !reusable && PadnameIsSTATE(name)) {
	PADOFFSET depth = CvDEPTH(outcv);
	while (--depth) {
	    SV *oldcv;
	    svspot = &PadARRAY(PadlistARRAY(CvPADLIST(outcv))[depth])[pax];
	    oldcv = *svspot;
	    *svspot = SvREFCNT_inc_simple_NN(cv);
	    SvREFCNT_dec(oldcv);
	}
    }

  done:
    if (PL_parser)
	PL_parser->copline = NOLINE;
    LEAVE_SCOPE(floor);
    if (o) op_free(o);
    return cv;
}

/* _x = extended */
CV *
Perl_newATTRSUB_x(pTHX_ I32 floor, OP *o, OP *proto, OP *attrs,
			    OP *block, bool o_is_gv)
{
    dVAR;
    GV *gv;
    const char *ps;
    STRLEN ps_len = 0; /* init it to avoid false uninit warning from icc */
    U32 ps_utf8 = 0;
    CV *cv = NULL;
    SV *const_sv;
    const bool ec = PL_parser && PL_parser->error_count;
    /* If the subroutine has no body, no attributes, and no builtin attributes
       then it's just a sub declaration, and we may be able to get away with
       storing with a placeholder scalar in the symbol table, rather than a
       full GV and CV.  If anything is present then it will take a full CV to
       store it.  */
    const I32 gv_fetch_flags
	= ec ? GV_NOADD_NOINIT :
	 (block || attrs || (CvFLAGS(PL_compcv) & CVf_BUILTIN_ATTRS)
	   || PL_madskills)
	? GV_ADDMULTI : GV_ADDMULTI | GV_NOINIT;
    STRLEN namlen = 0;
    const char * const name =
	 o ? SvPV_const(o_is_gv ? (SV *)o : cSVOPo->op_sv, namlen) : NULL;
    bool has_name;
    bool name_is_utf8 = o && !o_is_gv && SvUTF8(cSVOPo->op_sv);
#ifdef PERL_DEBUG_READONLY_OPS
    OPSLAB *slab = NULL;
#endif

    if (o_is_gv) {
	gv = (GV*)o;
	o = NULL;
	has_name = TRUE;
    } else if (name) {
	gv = gv_fetchsv(cSVOPo->op_sv, gv_fetch_flags, SVt_PVCV);
	has_name = TRUE;
    } else if (PERLDB_NAMEANON && CopLINE(PL_curcop)) {
	SV * const sv = sv_newmortal();
	Perl_sv_setpvf(aTHX_ sv, "%s[%s:%"IVdf"]",
		       PL_curstash ? "__ANON__" : "__ANON__::__ANON__",
		       CopFILE(PL_curcop), (IV)CopLINE(PL_curcop));
	gv = gv_fetchsv(sv, gv_fetch_flags, SVt_PVCV);
	has_name = TRUE;
    } else if (PL_curstash) {
	gv = gv_fetchpvs("__ANON__", gv_fetch_flags, SVt_PVCV);
	has_name = FALSE;
    } else {
	gv = gv_fetchpvs("__ANON__::__ANON__", gv_fetch_flags, SVt_PVCV);
	has_name = FALSE;
    }

    if (!ec)
        move_proto_attr(&proto, &attrs, gv);

    if (proto) {
	assert(proto->op_type == OP_CONST);
	ps = SvPV_const(((SVOP*)proto)->op_sv, ps_len);
        ps_utf8 = SvUTF8(((SVOP*)proto)->op_sv);
    }
    else
	ps = NULL;

    if (!PL_madskills) {
	if (o)
	    SAVEFREEOP(o);
	if (proto)
	    SAVEFREEOP(proto);
	if (attrs)
	    SAVEFREEOP(attrs);
    }

    if (ec) {
	op_free(block);
	if (name) SvREFCNT_dec(PL_compcv);
	else cv = PL_compcv;
	PL_compcv = 0;
	if (name && block) {
	    const char *s = strrchr(name, ':');
	    s = s ? s+1 : name;
	    if (strEQ(s, "BEGIN")) {
		if (PL_in_eval & EVAL_KEEPERR)
		    Perl_croak_nocontext("BEGIN not safe after errors--compilation aborted");
		else {
                    SV * const errsv = ERRSV;
		    /* force display of errors found but not reported */
		    sv_catpvs(errsv, "BEGIN not safe after errors--compilation aborted");
		    Perl_croak_nocontext("%"SVf, SVfARG(errsv));
		}
	    }
	}
	goto done;
    }

    if (SvTYPE(gv) != SVt_PVGV) {	/* Maybe prototype now, and had at
					   maximum a prototype before. */
	if (SvTYPE(gv) > SVt_NULL) {
	    cv_ckproto_len_flags((const CV *)gv,
				 o ? (const GV *)cSVOPo->op_sv : NULL, ps,
				 ps_len, ps_utf8);
	}
	if (ps) {
	    sv_setpvn(MUTABLE_SV(gv), ps, ps_len);
            if ( ps_utf8 ) SvUTF8_on(MUTABLE_SV(gv));
        }
	else
	    sv_setiv(MUTABLE_SV(gv), -1);

	SvREFCNT_dec(PL_compcv);
	cv = PL_compcv = NULL;
	goto done;
    }

    cv = (!name || GvCVGEN(gv)) ? NULL : GvCV(gv);

    if (!block || !ps || *ps || attrs
	|| (CvFLAGS(PL_compcv) & CVf_BUILTIN_ATTRS)
#ifdef PERL_MAD
	|| block->op_type == OP_NULL
#endif
	)
	const_sv = NULL;
    else
	const_sv = op_const_sv(block, NULL);

    if (cv) {
        const bool exists = CvROOT(cv) || CvXSUB(cv);

        /* if the subroutine doesn't exist and wasn't pre-declared
         * with a prototype, assume it will be AUTOLOADed,
         * skipping the prototype check
         */
        if (exists || SvPOK(cv))
            cv_ckproto_len_flags(cv, gv, ps, ps_len, ps_utf8);
	/* already defined (or promised)? */
	if (exists || GvASSUMECV(gv)) {
	    if (S_already_defined(aTHX_ cv, block, o, NULL, &const_sv))
		cv = NULL;
	    else {
		if (attrs) goto attrs;
		/* just a "sub foo;" when &foo is already defined */
		SAVEFREESV(PL_compcv);
		goto done;
	    }
	}
    }
    if (const_sv) {
	SvREFCNT_inc_simple_void_NN(const_sv);
	SvFLAGS(const_sv) = (SvFLAGS(const_sv) & ~SVs_PADMY) | SVs_PADTMP;
	if (cv) {
	    assert(!CvROOT(cv) && !CvCONST(cv));
	    cv_forget_slab(cv);
	    sv_setpvs(MUTABLE_SV(cv), "");  /* prototype is "" */
	    CvXSUBANY(cv).any_ptr = const_sv;
	    CvXSUB(cv) = const_sv_xsub;
	    CvCONST_on(cv);
	    CvISXSUB_on(cv);
	}
	else {
	    GvCV_set(gv, NULL);
	    cv = newCONSTSUB_flags(
		NULL, name, namlen, name_is_utf8 ? SVf_UTF8 : 0,
		const_sv
	    );
	}
	if (PL_madskills)
	    goto install_block;
	op_free(block);
	SvREFCNT_dec(PL_compcv);
	PL_compcv = NULL;
	goto done;
    }
    if (cv) {				/* must reuse cv if autoloaded */
	/* transfer PL_compcv to cv */
	if (block
#ifdef PERL_MAD
                  && block->op_type != OP_NULL
#endif
	) {
	    cv_flags_t existing_builtin_attrs = CvFLAGS(cv) & CVf_BUILTIN_ATTRS;
	    PADLIST *const temp_av = CvPADLIST(cv);
	    CV *const temp_cv = CvOUTSIDE(cv);
	    const cv_flags_t other_flags =
		CvFLAGS(cv) & (CVf_SLABBED|CVf_WEAKOUTSIDE);
	    OP * const cvstart = CvSTART(cv);

	    CvGV_set(cv,gv);
	    assert(!CvCVGV_RC(cv));
	    assert(CvGV(cv) == gv);

	    SvPOK_off(cv);
	    CvFLAGS(cv) = CvFLAGS(PL_compcv) | existing_builtin_attrs;
	    CvOUTSIDE(cv) = CvOUTSIDE(PL_compcv);
	    CvOUTSIDE_SEQ(cv) = CvOUTSIDE_SEQ(PL_compcv);
	    CvPADLIST(cv) = CvPADLIST(PL_compcv);
	    CvOUTSIDE(PL_compcv) = temp_cv;
	    CvPADLIST(PL_compcv) = temp_av;
	    CvSTART(cv) = CvSTART(PL_compcv);
	    CvSTART(PL_compcv) = cvstart;
	    CvFLAGS(PL_compcv) &= ~(CVf_SLABBED|CVf_WEAKOUTSIDE);
	    CvFLAGS(PL_compcv) |= other_flags;

	    if (CvFILE(cv) && CvDYNFILE(cv)) {
		Safefree(CvFILE(cv));
    }
	    CvFILE_set_from_cop(cv, PL_curcop);
	    CvSTASH_set(cv, PL_curstash);

	    /* inner references to PL_compcv must be fixed up ... */
	    pad_fixup_inner_anons(CvPADLIST(cv), PL_compcv, cv);
	    if (PERLDB_INTER)/* Advice debugger on the new sub. */
	      ++PL_sub_generation;
	}
	else {
	    /* Might have had built-in attributes applied -- propagate them. */
	    CvFLAGS(cv) |= (CvFLAGS(PL_compcv) & CVf_BUILTIN_ATTRS);
	}
	/* ... before we throw it away */
	SvREFCNT_dec(PL_compcv);
	PL_compcv = cv;
    }
    else {
	cv = PL_compcv;
	if (name) {
	    GvCV_set(gv, cv);
	    GvCVGEN(gv) = 0;
	    if (HvENAME_HEK(GvSTASH(gv)))
		/* sub Foo::bar { (shift)+1 } */
		gv_method_changed(gv);
	}
    }
    if (!CvGV(cv)) {
	CvGV_set(cv, gv);
	CvFILE_set_from_cop(cv, PL_curcop);
	CvSTASH_set(cv, PL_curstash);
    }

    if (ps) {
	sv_setpvn(MUTABLE_SV(cv), ps, ps_len);
        if ( ps_utf8 ) SvUTF8_on(MUTABLE_SV(cv));
    }

 install_block:
    if (!block)
	goto attrs;

    /* If we assign an optree to a PVCV, then we've defined a subroutine that
       the debugger could be able to set a breakpoint in, so signal to
       pp_entereval that it should not throw away any saved lines at scope
       exit.  */
       
    PL_breakable_sub_gen++;
    /* This makes sub {}; work as expected.  */
    if (block->op_type == OP_STUB) {
	    OP* const newblock = newSTATEOP(0, NULL, 0);
#ifdef PERL_MAD
	    op_getmad(block,newblock,'B');
#else
	    op_free(block);
#endif
	    block = newblock;
    }
    CvROOT(cv) = CvLVALUE(cv)
		   ? newUNOP(OP_LEAVESUBLV, 0,
			     op_lvalue(scalarseq(block), OP_LEAVESUBLV))
		   : newUNOP(OP_LEAVESUB, 0, scalarseq(block));
    CvROOT(cv)->op_private |= OPpREFCOUNTED;
    OpREFCNT_set(CvROOT(cv), 1);
    /* The cv no longer needs to hold a refcount on the slab, as CvROOT
       itself has a refcount. */
    CvSLABBED_off(cv);
    OpslabREFCNT_dec_padok((OPSLAB *)CvSTART(cv));
#ifdef PERL_DEBUG_READONLY_OPS
    slab = (OPSLAB *)CvSTART(cv);
#endif
    CvSTART(cv) = LINKLIST(CvROOT(cv));
    CvROOT(cv)->op_next = 0;
    CALL_PEEP(CvSTART(cv));
    finalize_optree(CvROOT(cv));
    S_prune_chain_head(aTHX_ &CvSTART(cv));

    /* now that optimizer has done its work, adjust pad values */

    pad_tidy(CvCLONE(cv) ? padtidy_SUBCLONE : padtidy_SUB);

    if (CvCLONE(cv)) {
	assert(!CvCONST(cv));
	if (ps && !*ps && op_const_sv(block, cv))
	    CvCONST_on(cv);
    }

  attrs:
    if (attrs) {
	/* Need to do a C<use attributes $stash_of_cv,\&cv,@attrs>. */
	HV *stash = name && GvSTASH(CvGV(cv)) ? GvSTASH(CvGV(cv)) : PL_curstash;
	if (!name) SAVEFREESV(cv);
	apply_attrs(stash, MUTABLE_SV(cv), attrs);
	if (!name) SvREFCNT_inc_simple_void_NN(cv);
    }

    if (block && has_name) {
	if (PERLDB_SUBLINE && PL_curstash != PL_debstash) {
	    SV * const tmpstr = sv_newmortal();
	    GV * const db_postponed = gv_fetchpvs("DB::postponed",
						  GV_ADDMULTI, SVt_PVHV);
	    HV *hv;
	    SV * const sv = Perl_newSVpvf(aTHX_ "%s:%ld-%ld",
					  CopFILE(PL_curcop),
					  (long)PL_subline,
					  (long)CopLINE(PL_curcop));
	    gv_efullname3(tmpstr, gv, NULL);
	    (void)hv_store(GvHV(PL_DBsub), SvPVX_const(tmpstr),
		    SvUTF8(tmpstr) ? -(I32)SvCUR(tmpstr) : (I32)SvCUR(tmpstr), sv, 0);
	    hv = GvHVn(db_postponed);
	    if (HvTOTALKEYS(hv) > 0 && hv_exists(hv, SvPVX_const(tmpstr), SvUTF8(tmpstr) ? -(I32)SvCUR(tmpstr) : (I32)SvCUR(tmpstr))) {
		CV * const pcv = GvCV(db_postponed);
		if (pcv) {
		    dSP;
		    PUSHMARK(SP);
		    XPUSHs(tmpstr);
		    PUTBACK;
		    call_sv(MUTABLE_SV(pcv), G_DISCARD);
		}
	    }
	}

	if (name && ! (PL_parser && PL_parser->error_count))
	    process_special_blocks(floor, name, gv, cv);
    }

  done:
    if (PL_parser)
	PL_parser->copline = NOLINE;
    LEAVE_SCOPE(floor);
#ifdef PERL_DEBUG_READONLY_OPS
    /* Watch out for BEGIN blocks */
    if (slab && gv && isGV(gv) && GvCV(gv)) Slab_to_ro(slab);
#endif
    return cv;
}

STATIC void
S_process_special_blocks(pTHX_ I32 floor, const char *const fullname,
			 GV *const gv,
			 CV *const cv)
{
    const char *const colon = strrchr(fullname,':');
    const char *const name = colon ? colon + 1 : fullname;

    PERL_ARGS_ASSERT_PROCESS_SPECIAL_BLOCKS;

    if (*name == 'B') {
	if (strEQ(name, "BEGIN")) {
	    const I32 oldscope = PL_scopestack_ix;
            dSP;
	    if (floor) LEAVE_SCOPE(floor);
	    ENTER;
            PUSHSTACKi(PERLSI_REQUIRE);
	    SAVECOPFILE(&PL_compiling);
	    SAVECOPLINE(&PL_compiling);
	    SAVEVPTR(PL_curcop);

	    DEBUG_x( dump_sub(gv) );
	    Perl_av_create_and_push(aTHX_ &PL_beginav, MUTABLE_SV(cv));
	    GvCV_set(gv,0);		/* cv has been hijacked */
	    call_list(oldscope, PL_beginav);

            POPSTACK;
	    LEAVE;
	}
	else
	    return;
    } else {
	if (*name == 'E') {
	    if strEQ(name, "END") {
		DEBUG_x( dump_sub(gv) );
		Perl_av_create_and_unshift_one(aTHX_ &PL_endav, MUTABLE_SV(cv));
	    } else
		return;
	} else if (*name == 'U') {
	    if (strEQ(name, "UNITCHECK")) {
		/* It's never too late to run a unitcheck block */
		Perl_av_create_and_unshift_one(aTHX_ &PL_unitcheckav, MUTABLE_SV(cv));
	    }
	    else
		return;
	} else if (*name == 'C') {
	    if (strEQ(name, "CHECK")) {
		if (PL_main_start)
		    /* diag_listed_as: Too late to run %s block */
		    Perl_ck_warner(aTHX_ packWARN(WARN_VOID),
				   "Too late to run CHECK block");
		Perl_av_create_and_unshift_one(aTHX_ &PL_checkav, MUTABLE_SV(cv));
	    }
	    else
		return;
	} else if (*name == 'I') {
	    if (strEQ(name, "INIT")) {
		if (PL_main_start)
		    /* diag_listed_as: Too late to run %s block */
		    Perl_ck_warner(aTHX_ packWARN(WARN_VOID),
				   "Too late to run INIT block");
		Perl_av_create_and_push(aTHX_ &PL_initav, MUTABLE_SV(cv));
	    }
	    else
		return;
	} else
	    return;
	DEBUG_x( dump_sub(gv) );
	GvCV_set(gv,0);		/* cv has been hijacked */
    }
}

/*
=for apidoc newCONSTSUB

See L</newCONSTSUB_flags>.

=cut
*/

CV *
Perl_newCONSTSUB(pTHX_ HV *stash, const char *name, SV *sv)
{
    return newCONSTSUB_flags(stash, name, name ? strlen(name) : 0, 0, sv);
}

/*
=for apidoc newCONSTSUB_flags

Creates a constant sub equivalent to Perl C<sub FOO () { 123 }> which is
eligible for inlining at compile-time.

Currently, the only useful value for C<flags> is SVf_UTF8.

The newly created subroutine takes ownership of a reference to the passed in
SV.

Passing NULL for SV creates a constant sub equivalent to C<sub BAR () {}>,
which won't be called if used as a destructor, but will suppress the overhead
of a call to C<AUTOLOAD>.  (This form, however, isn't eligible for inlining at
compile time.)

=cut
*/

CV *
Perl_newCONSTSUB_flags(pTHX_ HV *stash, const char *name, STRLEN len,
                             U32 flags, SV *sv)
{
    dVAR;
    CV* cv;
    const char *const file = CopFILE(PL_curcop);

    ENTER;

    if (IN_PERL_RUNTIME) {
	/* at runtime, it's not safe to manipulate PL_curcop: it may be
	 * an op shared between threads. Use a non-shared COP for our
	 * dirty work */
	 SAVEVPTR(PL_curcop);
	 SAVECOMPILEWARNINGS();
	 PL_compiling.cop_warnings = DUP_WARNINGS(PL_curcop->cop_warnings);
	 PL_curcop = &PL_compiling;
    }
    SAVECOPLINE(PL_curcop);
    CopLINE_set(PL_curcop, PL_parser ? PL_parser->copline : NOLINE);

    SAVEHINTS();
    PL_hints &= ~HINT_BLOCK_SCOPE;

    if (stash) {
	SAVEGENERICSV(PL_curstash);
	PL_curstash = (HV *)SvREFCNT_inc_simple_NN(stash);
    }

    /* Protect sv against leakage caused by fatal warnings. */
    if (sv) SAVEFREESV(sv);

    /* file becomes the CvFILE. For an XS, it's usually static storage,
       and so doesn't get free()d.  (It's expected to be from the C pre-
       processor __FILE__ directive). But we need a dynamically allocated one,
       and we need it to get freed.  */
    cv = newXS_len_flags(name, len,
			 sv && SvTYPE(sv) == SVt_PVAV
			     ? const_av_xsub
			     : const_sv_xsub,
			 file ? file : "", "",
			 &sv, XS_DYNAMIC_FILENAME | flags);
    CvXSUBANY(cv).any_ptr = SvREFCNT_inc_simple(sv);
    CvCONST_on(cv);

    LEAVE;

    return cv;
}

CV *
Perl_newXS_flags(pTHX_ const char *name, XSUBADDR_t subaddr,
		 const char *const filename, const char *const proto,
		 U32 flags)
{
    PERL_ARGS_ASSERT_NEWXS_FLAGS;
    return newXS_len_flags(
       name, name ? strlen(name) : 0, subaddr, filename, proto, NULL, flags
    );
}

CV *
Perl_newXS_len_flags(pTHX_ const char *name, STRLEN len,
			   XSUBADDR_t subaddr, const char *const filename,
			   const char *const proto, SV **const_svp,
			   U32 flags)
{
    CV *cv;
    bool interleave = FALSE;

    PERL_ARGS_ASSERT_NEWXS_LEN_FLAGS;

    {
        GV * const gv = gv_fetchpvn(
			    name ? name : PL_curstash ? "__ANON__" : "__ANON__::__ANON__",
			    name ? len : PL_curstash ? sizeof("__ANON__") - 1:
				sizeof("__ANON__::__ANON__") - 1,
			    GV_ADDMULTI | flags, SVt_PVCV);
    
        if (!subaddr)
            Perl_croak(aTHX_ "panic: no address for '%s' in '%s'", name, filename);
    
        if ((cv = (name ? GvCV(gv) : NULL))) {
            if (GvCVGEN(gv)) {
                /* just a cached method */
                SvREFCNT_dec(cv);
                cv = NULL;
            }
            else if (CvROOT(cv) || CvXSUB(cv) || GvASSUMECV(gv)) {
                /* already defined (or promised) */
                /* Redundant check that allows us to avoid creating an SV
                   most of the time: */
                if (CvCONST(cv) || ckWARN(WARN_REDEFINE)) {
                    report_redefined_cv(newSVpvn_flags(
                                         name,len,(flags&SVf_UTF8)|SVs_TEMP
                                        ),
                                        cv, const_svp);
                }
                interleave = TRUE;
                ENTER;
                SAVEFREESV(cv);
                cv = NULL;
            }
        }
    
        if (cv)				/* must reuse cv if autoloaded */
            cv_undef(cv);
        else {
            cv = MUTABLE_CV(newSV_type(SVt_PVCV));
            if (name) {
                GvCV_set(gv,cv);
                GvCVGEN(gv) = 0;
                if (HvENAME_HEK(GvSTASH(gv)))
                    gv_method_changed(gv); /* newXS */
            }
        }
        if (!name)
            CvANON_on(cv);
        CvGV_set(cv, gv);
        (void)gv_fetchfile(filename);
        CvFILE(cv) = (char *)filename; /* NOTE: not copied, as it is expected to be
                                    an external constant string */
        assert(!CvDYNFILE(cv)); /* cv_undef should have turned it off */
        CvISXSUB_on(cv);
        CvXSUB(cv) = subaddr;
    
        if (name)
            process_special_blocks(0, name, gv, cv);
    }

    if (flags & XS_DYNAMIC_FILENAME) {
	CvFILE(cv) = savepv(filename);
	CvDYNFILE_on(cv);
    }
    sv_setpv(MUTABLE_SV(cv), proto);
    if (interleave) LEAVE;
    return cv;
}

CV *
Perl_newSTUB(pTHX_ GV *gv, bool fake)
{
    CV *cv = MUTABLE_CV(newSV_type(SVt_PVCV));
    GV *cvgv;
    PERL_ARGS_ASSERT_NEWSTUB;
    assert(!GvCVu(gv));
    GvCV_set(gv, cv);
    GvCVGEN(gv) = 0;
    if (!fake && HvENAME_HEK(GvSTASH(gv)))
	gv_method_changed(gv);
    if (SvFAKE(gv)) {
	cvgv = gv_fetchsv((SV *)gv, GV_ADDMULTI, SVt_PVCV);
	SvFAKE_off(cvgv);
    }
    else cvgv = gv;
    CvGV_set(cv, cvgv);
    CvFILE_set_from_cop(cv, PL_curcop);
    CvSTASH_set(cv, PL_curstash);
    GvMULTI_on(gv);
    return cv;
}

/*
=for apidoc U||newXS

Used by C<xsubpp> to hook up XSUBs as Perl subs.  I<filename> needs to be
static storage, as it is used directly as CvFILE(), without a copy being made.

=cut
*/

CV *
Perl_newXS(pTHX_ const char *name, XSUBADDR_t subaddr, const char *filename)
{
    PERL_ARGS_ASSERT_NEWXS;
    return newXS_len_flags(
	name, name ? strlen(name) : 0, subaddr, filename, NULL, NULL, 0
    );
}

#ifdef PERL_MAD
OP *
#else
void
#endif
Perl_newFORM(pTHX_ I32 floor, OP *o, OP *block)
{
    dVAR;
    CV *cv;
#ifdef PERL_MAD
    OP* pegop = newOP(OP_NULL, 0);
#endif

    GV *gv;

    if (PL_parser && PL_parser->error_count) {
	op_free(block);
	goto finish;
    }

    gv = o
	? gv_fetchsv(cSVOPo->op_sv, GV_ADD, SVt_PVFM)
	: gv_fetchpvs("STDOUT", GV_ADD|GV_NOTQUAL, SVt_PVFM);

    GvMULTI_on(gv);
    if ((cv = GvFORM(gv))) {
	if (ckWARN(WARN_REDEFINE)) {
	    const line_t oldline = CopLINE(PL_curcop);
	    if (PL_parser && PL_parser->copline != NOLINE)
		CopLINE_set(PL_curcop, PL_parser->copline);
	    if (o) {
		Perl_warner(aTHX_ packWARN(WARN_REDEFINE),
			    "Format %"SVf" redefined", SVfARG(cSVOPo->op_sv));
	    } else {
		/* diag_listed_as: Format %s redefined */
		Perl_warner(aTHX_ packWARN(WARN_REDEFINE),
			    "Format STDOUT redefined");
	    }
	    CopLINE_set(PL_curcop, oldline);
	}
	SvREFCNT_dec(cv);
    }
    cv = PL_compcv;
    GvFORM(gv) = (CV *)SvREFCNT_inc_simple_NN(cv);
    CvGV_set(cv, gv);
    CvFILE_set_from_cop(cv, PL_curcop);


    pad_tidy(padtidy_FORMAT);
    CvROOT(cv) = newUNOP(OP_LEAVEWRITE, 0, scalarseq(block));
    CvROOT(cv)->op_private |= OPpREFCOUNTED;
    OpREFCNT_set(CvROOT(cv), 1);
    CvSTART(cv) = LINKLIST(CvROOT(cv));
    CvROOT(cv)->op_next = 0;
    CALL_PEEP(CvSTART(cv));
    finalize_optree(CvROOT(cv));
    S_prune_chain_head(aTHX_ &CvSTART(cv));
    cv_forget_slab(cv);

  finish:
#ifdef PERL_MAD
    op_getmad(o,pegop,'n');
    op_getmad_weak(block, pegop, 'b');
#else
    op_free(o);
#endif
    if (PL_parser)
	PL_parser->copline = NOLINE;
    LEAVE_SCOPE(floor);
#ifdef PERL_MAD
    return pegop;
#endif
}

OP *
Perl_newANONLIST(pTHX_ OP *o)
{
    return convert(OP_ANONLIST, OPf_SPECIAL, o);
}

OP *
Perl_newANONHASH(pTHX_ OP *o)
{
    return convert(OP_ANONHASH, OPf_SPECIAL, o);
}

OP *
Perl_newANONSUB(pTHX_ I32 floor, OP *proto, OP *block)
{
    return newANONATTRSUB(floor, proto, NULL, block);
}

OP *
Perl_newANONATTRSUB(pTHX_ I32 floor, OP *proto, OP *attrs, OP *block)
{
    return newUNOP(OP_REFGEN, 0,
	newSVOP(OP_ANONCODE, 0,
		MUTABLE_SV(newATTRSUB(floor, 0, proto, attrs, block))));
}

OP *
Perl_oopsAV(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_OOPSAV;

    switch (o->op_type) {
    case OP_PADSV:
    case OP_PADHV:
	o->op_type = OP_PADAV;
	o->op_ppaddr = PL_ppaddr[OP_PADAV];
	return ref(o, OP_RV2AV);

    case OP_RV2SV:
    case OP_RV2HV:
	o->op_type = OP_RV2AV;
	o->op_ppaddr = PL_ppaddr[OP_RV2AV];
	ref(o, OP_RV2AV);
	break;

    default:
	Perl_ck_warner_d(aTHX_ packWARN(WARN_INTERNAL), "oops: oopsAV");
	break;
    }
    return o;
}

OP *
Perl_oopsHV(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_OOPSHV;

    switch (o->op_type) {
    case OP_PADSV:
    case OP_PADAV:
	o->op_type = OP_PADHV;
	o->op_ppaddr = PL_ppaddr[OP_PADHV];
	return ref(o, OP_RV2HV);

    case OP_RV2SV:
    case OP_RV2AV:
	o->op_type = OP_RV2HV;
	o->op_ppaddr = PL_ppaddr[OP_RV2HV];
	ref(o, OP_RV2HV);
	break;

    default:
	Perl_ck_warner_d(aTHX_ packWARN(WARN_INTERNAL), "oops: oopsHV");
	break;
    }
    return o;
}

OP *
Perl_newAVREF(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_NEWAVREF;

    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADAV;
	o->op_ppaddr = PL_ppaddr[OP_PADAV];
	return o;
    }
    else if ((o->op_type == OP_RV2AV || o->op_type == OP_PADAV)) {
	Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
		       "Using an array as a reference is deprecated");
    }
    return newUNOP(OP_RV2AV, 0, scalar(o));
}

OP *
Perl_newGVREF(pTHX_ I32 type, OP *o)
{
    if (type == OP_MAPSTART || type == OP_GREPSTART || type == OP_SORT)
	return newUNOP(OP_NULL, 0, o);
    return ref(newUNOP(OP_RV2GV, OPf_REF, o), type);
}

OP *
Perl_newHVREF(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_NEWHVREF;

    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADHV;
	o->op_ppaddr = PL_ppaddr[OP_PADHV];
	return o;
    }
    else if ((o->op_type == OP_RV2HV || o->op_type == OP_PADHV)) {
	Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
		       "Using a hash as a reference is deprecated");
    }
    return newUNOP(OP_RV2HV, 0, scalar(o));
}

OP *
Perl_newCVREF(pTHX_ I32 flags, OP *o)
{
    if (o->op_type == OP_PADANY) {
	dVAR;
	o->op_type = OP_PADCV;
	o->op_ppaddr = PL_ppaddr[OP_PADCV];
    }
    return newUNOP(OP_RV2CV, flags, scalar(o));
}

OP *
Perl_newSVREF(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_NEWSVREF;

    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADSV;
	o->op_ppaddr = PL_ppaddr[OP_PADSV];
	return o;
    }
    return newUNOP(OP_RV2SV, 0, scalar(o));
}

/* Check routines. See the comments at the top of this file for details
 * on when these are called */

OP *
Perl_ck_anoncode(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_ANONCODE;

    cSVOPo->op_targ = pad_add_anon((CV*)cSVOPo->op_sv, o->op_type);
    if (!PL_madskills)
	cSVOPo->op_sv = NULL;
    return o;
}

static void
S_io_hints(pTHX_ OP *o)
{
    HV * const table =
	PL_hints & HINT_LOCALIZE_HH ? GvHV(PL_hintgv) : NULL;;
    if (table) {
	SV **svp = hv_fetchs(table, "open_IN", FALSE);
	if (svp && *svp) {
	    STRLEN len = 0;
	    const char *d = SvPV_const(*svp, len);
	    const I32 mode = mode_from_discipline(d, len);
	    if (mode & O_BINARY)
		o->op_private |= OPpOPEN_IN_RAW;
	    else if (mode & O_TEXT)
		o->op_private |= OPpOPEN_IN_CRLF;
	}

	svp = hv_fetchs(table, "open_OUT", FALSE);
	if (svp && *svp) {
	    STRLEN len = 0;
	    const char *d = SvPV_const(*svp, len);
	    const I32 mode = mode_from_discipline(d, len);
	    if (mode & O_BINARY)
		o->op_private |= OPpOPEN_OUT_RAW;
	    else if (mode & O_TEXT)
		o->op_private |= OPpOPEN_OUT_CRLF;
	}
    }
}

OP *
Perl_ck_backtick(pTHX_ OP *o)
{
    GV *gv;
    OP *newop = NULL;
    PERL_ARGS_ASSERT_CK_BACKTICK;
    /* qx and `` have a null pushmark; CORE::readpipe has only one kid. */
    if (o->op_flags & OPf_KIDS && cUNOPo->op_first->op_sibling
     && (gv = gv_override("readpipe",8))) {
	newop = S_new_entersubop(aTHX_ gv, cUNOPo->op_first->op_sibling);
	cUNOPo->op_first->op_sibling = NULL;
    }
    else if (!(o->op_flags & OPf_KIDS))
	newop = newUNOP(OP_BACKTICK, 0,	newDEFSVOP());
    if (newop) {
#ifdef PERL_MAD
	op_getmad(o,newop,'O');
#else
	op_free(o);
#endif
	return newop;
    }
    S_io_hints(aTHX_ o);
    return o;
}

OP *
Perl_ck_bitop(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_CK_BITOP;

    o->op_private = (U8)(PL_hints & HINT_INTEGER);
    if (!(o->op_flags & OPf_STACKED) /* Not an assignment */
	    && (o->op_type == OP_BIT_OR
	     || o->op_type == OP_BIT_AND
	     || o->op_type == OP_BIT_XOR))
    {
	const OP * const left = cBINOPo->op_first;
	const OP * const right = left->op_sibling;
	if ((OP_IS_NUMCOMPARE(left->op_type) &&
		(left->op_flags & OPf_PARENS) == 0) ||
	    (OP_IS_NUMCOMPARE(right->op_type) &&
		(right->op_flags & OPf_PARENS) == 0))
	    Perl_ck_warner(aTHX_ packWARN(WARN_PRECEDENCE),
			   "Possible precedence problem on bitwise %c operator",
			   o->op_type == OP_BIT_OR ? '|'
			   : o->op_type == OP_BIT_AND ? '&' : '^'
			   );
    }
    return o;
}

PERL_STATIC_INLINE bool
is_dollar_bracket(pTHX_ const OP * const o)
{
    const OP *kid;
    return o->op_type == OP_RV2SV && o->op_flags & OPf_KIDS
	&& (kid = cUNOPx(o)->op_first)
	&& kid->op_type == OP_GV
	&& strEQ(GvNAME(cGVOPx_gv(kid)), "[");
}

OP *
Perl_ck_cmp(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_CMP;
    if (ckWARN(WARN_SYNTAX)) {
	const OP *kid = cUNOPo->op_first;
	if (kid && (
		(
		   is_dollar_bracket(aTHX_ kid)
		&& kid->op_sibling && kid->op_sibling->op_type == OP_CONST
		)
	     || (  kid->op_type == OP_CONST
		&& (kid = kid->op_sibling) && is_dollar_bracket(aTHX_ kid))
	   ))
	    Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
			"$[ used in %s (did you mean $] ?)", OP_DESC(o));
    }
    return o;
}

OP *
Perl_ck_concat(pTHX_ OP *o)
{
    const OP * const kid = cUNOPo->op_first;

    PERL_ARGS_ASSERT_CK_CONCAT;
    PERL_UNUSED_CONTEXT;

    if (kid->op_type == OP_CONCAT && !(kid->op_private & OPpTARGET_MY) &&
	    !(kUNOP->op_first->op_flags & OPf_MOD))
        o->op_flags |= OPf_STACKED;
    return o;
}

OP *
Perl_ck_spair(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_CK_SPAIR;

    if (o->op_flags & OPf_KIDS) {
	OP* newop;
	OP* kid;
	const OPCODE type = o->op_type;
	o = modkids(ck_fun(o), type);
	kid = cUNOPo->op_first;
	newop = kUNOP->op_first->op_sibling;
	if (newop) {
	    const OPCODE type = newop->op_type;
	    if (newop->op_sibling || !(PL_opargs[type] & OA_RETSCALAR) ||
		    type == OP_PADAV || type == OP_PADHV ||
		    type == OP_RV2AV || type == OP_RV2HV)
		return o;
	}
#ifdef PERL_MAD
	op_getmad(kUNOP->op_first,newop,'K');
#else
	op_free(kUNOP->op_first);
#endif
	kUNOP->op_first = newop;
    }
    /* transforms OP_REFGEN into OP_SREFGEN, OP_CHOP into OP_SCHOP,
     * and OP_CHOMP into OP_SCHOMP */
    o->op_ppaddr = PL_ppaddr[++o->op_type];
    return ck_fun(o);
}

OP *
Perl_ck_delete(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_DELETE;

    o = ck_fun(o);
    o->op_private = 0;
    if (o->op_flags & OPf_KIDS) {
	OP * const kid = cUNOPo->op_first;
	switch (kid->op_type) {
	case OP_ASLICE:
	    o->op_flags |= OPf_SPECIAL;
	    /* FALL THROUGH */
	case OP_HSLICE:
	    o->op_private |= OPpSLICE;
	    break;
	case OP_AELEM:
	    o->op_flags |= OPf_SPECIAL;
	    /* FALL THROUGH */
	case OP_HELEM:
	    break;
	case OP_KVASLICE:
	    Perl_croak(aTHX_ "delete argument is index/value array slice,"
			     " use array slice");
	case OP_KVHSLICE:
	    Perl_croak(aTHX_ "delete argument is key/value hash slice, use"
			     " hash slice");
	default:
	    Perl_croak(aTHX_ "delete argument is not a HASH or ARRAY "
			     "element or slice");
	}
	if (kid->op_private & OPpLVAL_INTRO)
	    o->op_private |= OPpLVAL_INTRO;
	op_null(kid);
    }
    return o;
}

OP *
Perl_ck_eof(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_CK_EOF;

    if (o->op_flags & OPf_KIDS) {
	OP *kid;
	if (cLISTOPo->op_first->op_type == OP_STUB) {
	    OP * const newop
		= newUNOP(o->op_type, OPf_SPECIAL, newGVOP(OP_GV, 0, PL_argvgv));
#ifdef PERL_MAD
	    op_getmad(o,newop,'O');
#else
	    op_free(o);
#endif
	    o = newop;
	}
	o = ck_fun(o);
	kid = cLISTOPo->op_first;
	if (kid->op_type == OP_RV2GV)
	    kid->op_private |= OPpALLOW_FAKE;
    }
    return o;
}

OP *
Perl_ck_eval(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_CK_EVAL;

    PL_hints |= HINT_BLOCK_SCOPE;
    if (o->op_flags & OPf_KIDS) {
	SVOP * const kid = (SVOP*)cUNOPo->op_first;
	assert(kid);

	if (kid->op_type == OP_LINESEQ || kid->op_type == OP_STUB) {
	    LOGOP *enter;
#ifdef PERL_MAD
	    OP* const oldo = o;
#endif

	    cUNOPo->op_first = 0;
#ifndef PERL_MAD
	    op_free(o);
#endif

	    NewOp(1101, enter, 1, LOGOP);
	    enter->op_type = OP_ENTERTRY;
	    enter->op_ppaddr = PL_ppaddr[OP_ENTERTRY];
	    enter->op_private = 0;

	    /* establish postfix order */
	    enter->op_next = (OP*)enter;

	    o = op_prepend_elem(OP_LINESEQ, (OP*)enter, (OP*)kid);
	    o->op_type = OP_LEAVETRY;
	    o->op_ppaddr = PL_ppaddr[OP_LEAVETRY];
	    enter->op_other = o;
	    op_getmad(oldo,o,'O');
	    return o;
	}
	else {
	    scalar((OP*)kid);
	    PL_cv_has_eval = 1;
	}
    }
    else {
	const U8 priv = o->op_private;
#ifdef PERL_MAD
	OP* const oldo = o;
#else
	op_free(o);
#endif
	o = newUNOP(OP_ENTEREVAL, priv <<8, newDEFSVOP());
	op_getmad(oldo,o,'O');
    }
    o->op_targ = (PADOFFSET)PL_hints;
    if (o->op_private & OPpEVAL_BYTES) o->op_targ &= ~HINT_UTF8;
    if ((PL_hints & HINT_LOCALIZE_HH) != 0
     && !(o->op_private & OPpEVAL_COPHH) && GvHV(PL_hintgv)) {
	/* Store a copy of %^H that pp_entereval can pick up. */
	OP *hhop = newSVOP(OP_HINTSEVAL, 0,
			   MUTABLE_SV(hv_copy_hints_hv(GvHV(PL_hintgv))));
	cUNOPo->op_first->op_sibling = hhop;
	o->op_private |= OPpEVAL_HAS_HH;
    }
    if (!(o->op_private & OPpEVAL_BYTES)
	 && FEATURE_UNIEVAL_IS_ENABLED)
	    o->op_private |= OPpEVAL_UNICODE;
    return o;
}

OP *
Perl_ck_exec(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_EXEC;

    if (o->op_flags & OPf_STACKED) {
        OP *kid;
	o = ck_fun(o);
	kid = cUNOPo->op_first->op_sibling;
	if (kid->op_type == OP_RV2GV)
	    op_null(kid);
    }
    else
	o = listkids(o);
    return o;
}

OP *
Perl_ck_exists(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_CK_EXISTS;

    o = ck_fun(o);
    if (o->op_flags & OPf_KIDS) {
	OP * const kid = cUNOPo->op_first;
	if (kid->op_type == OP_ENTERSUB) {
	    (void) ref(kid, o->op_type);
	    if (kid->op_type != OP_RV2CV
			&& !(PL_parser && PL_parser->error_count))
		Perl_croak(aTHX_
			  "exists argument is not a subroutine name");
	    o->op_private |= OPpEXISTS_SUB;
	}
	else if (kid->op_type == OP_AELEM)
	    o->op_flags |= OPf_SPECIAL;
	else if (kid->op_type != OP_HELEM)
	    Perl_croak(aTHX_ "exists argument is not a HASH or ARRAY "
			     "element or a subroutine");
	op_null(kid);
    }
    return o;
}

OP *
Perl_ck_rvconst(pTHX_ OP *o)
{
    dVAR;
    SVOP * const kid = (SVOP*)cUNOPo->op_first;

    PERL_ARGS_ASSERT_CK_RVCONST;

    o->op_private |= (PL_hints & HINT_STRICT_REFS);
    if (o->op_type == OP_RV2CV)
	o->op_private &= ~1;

    if (kid->op_type == OP_CONST) {
	int iscv;
	GV *gv;
	SV * const kidsv = kid->op_sv;

	/* Is it a constant from cv_const_sv()? */
	if (SvROK(kidsv) && SvREADONLY(kidsv)) {
	    SV * const rsv = SvRV(kidsv);
	    const svtype type = SvTYPE(rsv);
            const char *badtype = NULL;

	    switch (o->op_type) {
	    case OP_RV2SV:
		if (type > SVt_PVMG)
		    badtype = "a SCALAR";
		break;
	    case OP_RV2AV:
		if (type != SVt_PVAV)
		    badtype = "an ARRAY";
		break;
	    case OP_RV2HV:
		if (type != SVt_PVHV)
		    badtype = "a HASH";
		break;
	    case OP_RV2CV:
		if (type != SVt_PVCV)
		    badtype = "a CODE";
		break;
	    }
	    if (badtype)
		Perl_croak(aTHX_ "Constant is not %s reference", badtype);
	    return o;
	}
	if (SvTYPE(kidsv) == SVt_PVAV) return o;
	if ((o->op_private & HINT_STRICT_REFS) && (kid->op_private & OPpCONST_BARE)) {
	    const char *badthing;
	    switch (o->op_type) {
	    case OP_RV2SV:
		badthing = "a SCALAR";
		break;
	    case OP_RV2AV:
		badthing = "an ARRAY";
		break;
	    case OP_RV2HV:
		badthing = "a HASH";
		break;
	    default:
		badthing = NULL;
		break;
	    }
	    if (badthing)
		Perl_croak(aTHX_
			   "Can't use bareword (\"%"SVf"\") as %s ref while \"strict refs\" in use",
			   SVfARG(kidsv), badthing);
	}
	/*
	 * This is a little tricky.  We only want to add the symbol if we
	 * didn't add it in the lexer.  Otherwise we get duplicate strict
	 * warnings.  But if we didn't add it in the lexer, we must at
	 * least pretend like we wanted to add it even if it existed before,
	 * or we get possible typo warnings.  OPpCONST_ENTERED says
	 * whether the lexer already added THIS instance of this symbol.
	 */
	iscv = (o->op_type == OP_RV2CV) * 2;
	do {
	    gv = gv_fetchsv(kidsv,
		iscv | !(kid->op_private & OPpCONST_ENTERED),
		iscv
		    ? SVt_PVCV
		    : o->op_type == OP_RV2SV
			? SVt_PV
			: o->op_type == OP_RV2AV
			    ? SVt_PVAV
			    : o->op_type == OP_RV2HV
				? SVt_PVHV
				: SVt_PVGV);
	} while (!gv && !(kid->op_private & OPpCONST_ENTERED) && !iscv++);
	if (gv) {
	    kid->op_type = OP_GV;
	    SvREFCNT_dec(kid->op_sv);
#ifdef USE_ITHREADS
	    /* XXX hack: dependence on sizeof(PADOP) <= sizeof(SVOP) */
	    assert (sizeof(PADOP) <= sizeof(SVOP));
	    kPADOP->op_padix = pad_alloc(OP_GV, SVs_PADTMP);
	    SvREFCNT_dec(PAD_SVl(kPADOP->op_padix));
	    GvIN_PAD_on(gv);
	    PAD_SETSV(kPADOP->op_padix, MUTABLE_SV(SvREFCNT_inc_simple_NN(gv)));
#else
	    kid->op_sv = SvREFCNT_inc_simple_NN(gv);
#endif
	    kid->op_private = 0;
	    kid->op_ppaddr = PL_ppaddr[OP_GV];
	    /* FAKE globs in the symbol table cause weird bugs (#77810) */
	    SvFAKE_off(gv);
	}
    }
    return o;
}

OP *
Perl_ck_ftst(pTHX_ OP *o)
{
    dVAR;
    const I32 type = o->op_type;

    PERL_ARGS_ASSERT_CK_FTST;

    if (o->op_flags & OPf_REF) {
	NOOP;
    }
    else if (o->op_flags & OPf_KIDS && cUNOPo->op_first->op_type != OP_STUB) {
	SVOP * const kid = (SVOP*)cUNOPo->op_first;
	const OPCODE kidtype = kid->op_type;

	if (kidtype == OP_CONST && (kid->op_private & OPpCONST_BARE)
	 && !kid->op_folded) {
	    OP * const newop = newGVOP(type, OPf_REF,
		gv_fetchsv(kid->op_sv, GV_ADD, SVt_PVIO));
#ifdef PERL_MAD
	    op_getmad(o,newop,'O');
#else
	    op_free(o);
#endif
	    return newop;
	}
	if ((PL_hints & HINT_FILETEST_ACCESS) && OP_IS_FILETEST_ACCESS(o->op_type))
	    o->op_private |= OPpFT_ACCESS;
	if (PL_check[kidtype] == Perl_ck_ftst
	        && kidtype != OP_STAT && kidtype != OP_LSTAT) {
	    o->op_private |= OPpFT_STACKED;
	    kid->op_private |= OPpFT_STACKING;
	    if (kidtype == OP_FTTTY && (
		   !(kid->op_private & OPpFT_STACKED)
		|| kid->op_private & OPpFT_AFTER_t
	       ))
		o->op_private |= OPpFT_AFTER_t;
	}
    }
    else {
#ifdef PERL_MAD
	OP* const oldo = o;
#else
	op_free(o);
#endif
	if (type == OP_FTTTY)
	    o = newGVOP(type, OPf_REF, PL_stdingv);
	else
	    o = newUNOP(type, 0, newDEFSVOP());
	op_getmad(oldo,o,'O');
    }
    return o;
}

OP *
Perl_ck_fun(pTHX_ OP *o)
{
    dVAR;
    const int type = o->op_type;
    I32 oa = PL_opargs[type] >> OASHIFT;

    PERL_ARGS_ASSERT_CK_FUN;

    if (o->op_flags & OPf_STACKED) {
	if ((oa & OA_OPTIONAL) && (oa >> 4) && !((oa >> 4) & OA_OPTIONAL))
	    oa &= ~OA_OPTIONAL;
	else
	    return no_fh_allowed(o);
    }

    if (o->op_flags & OPf_KIDS) {
        OP **tokid = &cLISTOPo->op_first;
        OP *kid = cLISTOPo->op_first;
        OP *sibl;
        I32 numargs = 0;
	bool seen_optional = FALSE;

	if (kid->op_type == OP_PUSHMARK ||
	    (kid->op_type == OP_NULL && kid->op_targ == OP_PUSHMARK))
	{
	    tokid = &kid->op_sibling;
	    kid = kid->op_sibling;
	}
	if (kid && kid->op_type == OP_COREARGS) {
	    bool optional = FALSE;
	    while (oa) {
		numargs++;
		if (oa & OA_OPTIONAL) optional = TRUE;
		oa = oa >> 4;
	    }
	    if (optional) o->op_private |= numargs;
	    return o;
	}

	while (oa) {
	    if (oa & OA_OPTIONAL || (oa & 7) == OA_LIST) {
		if (!kid && !seen_optional && PL_opargs[type] & OA_DEFGV)
		    *tokid = kid = newDEFSVOP();
		seen_optional = TRUE;
	    }
	    if (!kid) break;

	    numargs++;
	    sibl = kid->op_sibling;
#ifdef PERL_MAD
	    if (!sibl && kid->op_type == OP_STUB) {
		numargs--;
		break;
	    }
#endif
	    switch (oa & 7) {
	    case OA_SCALAR:
		/* list seen where single (scalar) arg expected? */
		if (numargs == 1 && !(oa >> 4)
		    && kid->op_type == OP_LIST && type != OP_SCALAR)
		{
		    return too_many_arguments_pv(o,PL_op_desc[type], 0);
		}
		if (type != OP_DELETE) scalar(kid);
		break;
	    case OA_LIST:
		if (oa < 16) {
		    kid = 0;
		    continue;
		}
		else
		    list(kid);
		break;
	    case OA_AVREF:
		if ((type == OP_PUSH || type == OP_UNSHIFT)
		    && !kid->op_sibling)
		    Perl_ck_warner(aTHX_ packWARN(WARN_SYNTAX),
				   "Useless use of %s with no values",
				   PL_op_desc[type]);

		if (kid->op_type == OP_CONST &&
		    (kid->op_private & OPpCONST_BARE))
		{
		    OP * const newop = newAVREF(newGVOP(OP_GV, 0,
			gv_fetchsv(((SVOP*)kid)->op_sv, GV_ADD, SVt_PVAV) ));
		    Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
				   "Array @%"SVf" missing the @ in argument %"IVdf" of %s()",
				   SVfARG(((SVOP*)kid)->op_sv), (IV)numargs, PL_op_desc[type]);
#ifdef PERL_MAD
		    op_getmad(kid,newop,'K');
#else
		    op_free(kid);
#endif
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		else if (kid->op_type == OP_CONST
		      && (  !SvROK(cSVOPx_sv(kid)) 
		         || SvTYPE(SvRV(cSVOPx_sv(kid))) != SVt_PVAV  )
		        )
		    bad_type_pv(numargs, "array", PL_op_desc[type], 0, kid);
		/* Defer checks to run-time if we have a scalar arg */
		if (kid->op_type == OP_RV2AV || kid->op_type == OP_PADAV)
		    op_lvalue(kid, type);
		else {
		    scalar(kid);
		    /* diag_listed_as: push on reference is experimental */
		    Perl_ck_warner_d(aTHX_
				     packWARN(WARN_EXPERIMENTAL__AUTODEREF),
				    "%s on reference is experimental",
				     PL_op_desc[type]);
		}
		break;
	    case OA_HVREF:
		if (kid->op_type == OP_CONST &&
		    (kid->op_private & OPpCONST_BARE))
		{
		    OP * const newop = newHVREF(newGVOP(OP_GV, 0,
			gv_fetchsv(((SVOP*)kid)->op_sv, GV_ADD, SVt_PVHV) ));
		    Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
				   "Hash %%%"SVf" missing the %% in argument %"IVdf" of %s()",
				   SVfARG(((SVOP*)kid)->op_sv), (IV)numargs, PL_op_desc[type]);
#ifdef PERL_MAD
		    op_getmad(kid,newop,'K');
#else
		    op_free(kid);
#endif
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		else if (kid->op_type != OP_RV2HV && kid->op_type != OP_PADHV)
		    bad_type_pv(numargs, "hash", PL_op_desc[type], 0, kid);
		op_lvalue(kid, type);
		break;
	    case OA_CVREF:
		{
		    OP * const newop = newUNOP(OP_NULL, 0, kid);
		    kid->op_sibling = 0;
		    newop->op_next = newop;
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		break;
	    case OA_FILEREF:
		if (kid->op_type != OP_GV && kid->op_type != OP_RV2GV) {
		    if (kid->op_type == OP_CONST &&
			(kid->op_private & OPpCONST_BARE))
		    {
			OP * const newop = newGVOP(OP_GV, 0,
			    gv_fetchsv(((SVOP*)kid)->op_sv, GV_ADD, SVt_PVIO));
			if (!(o->op_private & 1) && /* if not unop */
			    kid == cLISTOPo->op_last)
			    cLISTOPo->op_last = newop;
#ifdef PERL_MAD
			op_getmad(kid,newop,'K');
#else
			op_free(kid);
#endif
			kid = newop;
		    }
		    else if (kid->op_type == OP_READLINE) {
			/* neophyte patrol: open(<FH>), close(<FH>) etc. */
			bad_type_pv(numargs, "HANDLE", OP_DESC(o), 0, kid);
		    }
		    else {
			I32 flags = OPf_SPECIAL;
			I32 priv = 0;
			PADOFFSET targ = 0;

			/* is this op a FH constructor? */
			if (is_handle_constructor(o,numargs)) {
                            const char *name = NULL;
			    STRLEN len = 0;
                            U32 name_utf8 = 0;
			    bool want_dollar = TRUE;

			    flags = 0;
			    /* Set a flag to tell rv2gv to vivify
			     * need to "prove" flag does not mean something
			     * else already - NI-S 1999/05/07
			     */
			    priv = OPpDEREF;
			    if (kid->op_type == OP_PADSV) {
				SV *const namesv
				    = PAD_COMPNAME_SV(kid->op_targ);
				name = SvPV_const(namesv, len);
                                name_utf8 = SvUTF8(namesv);
			    }
			    else if (kid->op_type == OP_RV2SV
				     && kUNOP->op_first->op_type == OP_GV)
			    {
				GV * const gv = cGVOPx_gv(kUNOP->op_first);
				name = GvNAME(gv);
				len = GvNAMELEN(gv);
                                name_utf8 = GvNAMEUTF8(gv) ? SVf_UTF8 : 0;
			    }
			    else if (kid->op_type == OP_AELEM
				     || kid->op_type == OP_HELEM)
			    {
				 OP *firstop;
				 OP *op = ((BINOP*)kid)->op_first;
				 name = NULL;
				 if (op) {
				      SV *tmpstr = NULL;
				      const char * const a =
					   kid->op_type == OP_AELEM ?
					   "[]" : "{}";
				      if (((op->op_type == OP_RV2AV) ||
					   (op->op_type == OP_RV2HV)) &&
					  (firstop = ((UNOP*)op)->op_first) &&
					  (firstop->op_type == OP_GV)) {
					   /* packagevar $a[] or $h{} */
					   GV * const gv = cGVOPx_gv(firstop);
					   if (gv)
						tmpstr =
						     Perl_newSVpvf(aTHX_
								   "%s%c...%c",
								   GvNAME(gv),
								   a[0], a[1]);
				      }
				      else if (op->op_type == OP_PADAV
					       || op->op_type == OP_PADHV) {
					   /* lexicalvar $a[] or $h{} */
					   const char * const padname =
						PAD_COMPNAME_PV(op->op_targ);
					   if (padname)
						tmpstr =
						     Perl_newSVpvf(aTHX_
								   "%s%c...%c",
								   padname + 1,
								   a[0], a[1]);
				      }
				      if (tmpstr) {
					   name = SvPV_const(tmpstr, len);
                                           name_utf8 = SvUTF8(tmpstr);
					   sv_2mortal(tmpstr);
				      }
				 }
				 if (!name) {
				      name = "__ANONIO__";
				      len = 10;
				      want_dollar = FALSE;
				 }
				 op_lvalue(kid, type);
			    }
			    if (name) {
				SV *namesv;
				targ = pad_alloc(OP_RV2GV, SVf_READONLY);
				namesv = PAD_SVl(targ);
				if (want_dollar && *name != '$')
				    sv_setpvs(namesv, "$");
				else
				    sv_setpvs(namesv, "");
				sv_catpvn(namesv, name, len);
                                if ( name_utf8 ) SvUTF8_on(namesv);
			    }
			}
			kid->op_sibling = 0;
			kid = newUNOP(OP_RV2GV, flags, scalar(kid));
			kid->op_targ = targ;
			kid->op_private |= priv;
		    }
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		scalar(kid);
		break;
	    case OA_SCALARREF:
		if ((type == OP_UNDEF || type == OP_POS)
		    && numargs == 1 && !(oa >> 4)
		    && kid->op_type == OP_LIST)
		    return too_many_arguments_pv(o,PL_op_desc[type], 0);
		op_lvalue(scalar(kid), type);
		break;
	    }
	    oa >>= 4;
	    tokid = &kid->op_sibling;
	    kid = kid->op_sibling;
	}
#ifdef PERL_MAD
	if (kid && kid->op_type != OP_STUB)
	    return too_many_arguments_pv(o,OP_DESC(o), 0);
	o->op_private |= numargs;
#else
	/* FIXME - should the numargs move as for the PERL_MAD case?  */
	o->op_private |= numargs;
	if (kid)
	    return too_many_arguments_pv(o,OP_DESC(o), 0);
#endif
	listkids(o);
    }
    else if (PL_opargs[type] & OA_DEFGV) {
#ifdef PERL_MAD
	OP *newop = newUNOP(type, 0, newDEFSVOP());
	op_getmad(o,newop,'O');
	return newop;
#else
	/* Ordering of these two is important to keep f_map.t passing.  */
	op_free(o);
	return newUNOP(type, 0, newDEFSVOP());
#endif
    }

    if (oa) {
	while (oa & OA_OPTIONAL)
	    oa >>= 4;
	if (oa && oa != OA_LIST)
	    return too_few_arguments_pv(o,OP_DESC(o), 0);
    }
    return o;
}

OP *
Perl_ck_glob(pTHX_ OP *o)
{
    dVAR;
    GV *gv;

    PERL_ARGS_ASSERT_CK_GLOB;

    o = ck_fun(o);
    if ((o->op_flags & OPf_KIDS) && !cLISTOPo->op_first->op_sibling)
	op_append_elem(OP_GLOB, o, newDEFSVOP()); /* glob() => glob($_) */

    if (!(o->op_flags & OPf_SPECIAL) && (gv = gv_override("glob", 4)))
    {
	/* convert
	 *     glob
	 *       \ null - const(wildcard)
	 * into
	 *     null
	 *       \ enter
	 *            \ list
	 *                 \ mark - glob - rv2cv
	 *                             |        \ gv(CORE::GLOBAL::glob)
	 *                             |
	 *                              \ null - const(wildcard)
	 */
	o->op_flags |= OPf_SPECIAL;
	o->op_targ = pad_alloc(OP_GLOB, SVs_PADTMP);
	o = S_new_entersubop(aTHX_ gv, o);
	o = newUNOP(OP_NULL, 0, o);
	o->op_targ = OP_GLOB; /* hint at what it used to be: eg in newWHILEOP */
	return o;
    }
    else o->op_flags &= ~OPf_SPECIAL;
#if !defined(PERL_EXTERNAL_GLOB)
    if (!PL_globhook) {
	ENTER;
	Perl_load_module(aTHX_ PERL_LOADMOD_NOIMPORT,
			       newSVpvs("File::Glob"), NULL, NULL, NULL);
	LEAVE;
    }
#endif /* !PERL_EXTERNAL_GLOB */
    gv = (GV *)newSV(0);
    gv_init(gv, 0, "", 0, 0);
    gv_IOadd(gv);
    op_append_elem(OP_GLOB, o, newGVOP(OP_GV, 0, gv));
    SvREFCNT_dec_NN(gv); /* newGVOP increased it */
    scalarkids(o);
    return o;
}

OP *
Perl_ck_grep(pTHX_ OP *o)
{
    dVAR;
    LOGOP *gwop;
    OP *kid;
    const OPCODE type = o->op_type == OP_GREPSTART ? OP_GREPWHILE : OP_MAPWHILE;
    PADOFFSET offset;

    PERL_ARGS_ASSERT_CK_GREP;

    o->op_ppaddr = PL_ppaddr[OP_GREPSTART];
    /* don't allocate gwop here, as we may leak it if PL_parser->error_count > 0 */

    if (o->op_flags & OPf_STACKED) {
        kid = cUNOPx(cLISTOPo->op_first->op_sibling)->op_first;
	if (kid->op_type != OP_SCOPE && kid->op_type != OP_LEAVE)
	    return no_fh_allowed(o);
	o->op_flags &= ~OPf_STACKED;
    }
    kid = cLISTOPo->op_first->op_sibling;
    if (type == OP_MAPWHILE)
	list(kid);
    else
	scalar(kid);
    o = ck_fun(o);
    if (PL_parser && PL_parser->error_count)
	return o;
    kid = cLISTOPo->op_first->op_sibling;
    if (kid->op_type != OP_NULL)
	Perl_croak(aTHX_ "panic: ck_grep, type=%u", (unsigned) kid->op_type);
    kid = kUNOP->op_first;

    NewOp(1101, gwop, 1, LOGOP);
    gwop->op_type = type;
    gwop->op_ppaddr = PL_ppaddr[type];
    gwop->op_first = o;
    gwop->op_flags |= OPf_KIDS;
    gwop->op_other = LINKLIST(kid);
    kid->op_next = (OP*)gwop;
    offset = pad_findmy_pvs("$_", 0);
    if (offset == NOT_IN_PAD || PAD_COMPNAME_FLAGS_isOUR(offset)) {
	o->op_private = gwop->op_private = 0;
	gwop->op_targ = pad_alloc(type, SVs_PADTMP);
    }
    else {
	o->op_private = gwop->op_private = OPpGREP_LEX;
	gwop->op_targ = o->op_targ = offset;
    }

    kid = cLISTOPo->op_first->op_sibling;
    for (kid = kid->op_sibling; kid; kid = kid->op_sibling)
	op_lvalue(kid, OP_GREPSTART);

    return (OP*)gwop;
}

OP *
Perl_ck_index(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_INDEX;

    if (o->op_flags & OPf_KIDS) {
	OP *kid = cLISTOPo->op_first->op_sibling;	/* get past pushmark */
	if (kid)
	    kid = kid->op_sibling;			/* get past "big" */
	if (kid && kid->op_type == OP_CONST) {
	    const bool save_taint = TAINT_get;
	    SV *sv = kSVOP->op_sv;
	    if ((!SvPOK(sv) || SvNIOKp(sv)) && SvOK(sv) && !SvROK(sv)) {
		sv = newSV(0);
		sv_copypv(sv, kSVOP->op_sv);
		SvREFCNT_dec_NN(kSVOP->op_sv);
		kSVOP->op_sv = sv;
	    }
	    if (SvOK(sv)) fbm_compile(sv, 0);
	    TAINT_set(save_taint);
#ifdef NO_TAINT_SUPPORT
            PERL_UNUSED_VAR(save_taint);
#endif
	}
    }
    return ck_fun(o);
}

OP *
Perl_ck_lfun(pTHX_ OP *o)
{
    const OPCODE type = o->op_type;

    PERL_ARGS_ASSERT_CK_LFUN;

    return modkids(ck_fun(o), type);
}

OP *
Perl_ck_defined(pTHX_ OP *o)		/* 19990527 MJD */
{
    PERL_ARGS_ASSERT_CK_DEFINED;

    if ((o->op_flags & OPf_KIDS)) {
	switch (cUNOPo->op_first->op_type) {
	case OP_RV2AV:
	case OP_PADAV:
	case OP_AASSIGN:		/* Is this a good idea? */
	    Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
			   "defined(@array) is deprecated");
	    Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
			   "\t(Maybe you should just omit the defined()?)\n");
	break;
	case OP_RV2HV:
	case OP_PADHV:
	    Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
			   "defined(%%hash) is deprecated");
	    Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
			   "\t(Maybe you should just omit the defined()?)\n");
	    break;
	default:
	    /* no warning */
	    break;
	}
    }
    return ck_rfun(o);
}

OP *
Perl_ck_readline(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_READLINE;

    if (o->op_flags & OPf_KIDS) {
	 OP *kid = cLISTOPo->op_first;
	 if (kid->op_type == OP_RV2GV) kid->op_private |= OPpALLOW_FAKE;
    }
    else {
	OP * const newop
	    = newUNOP(OP_READLINE, 0, newGVOP(OP_GV, 0, PL_argvgv));
#ifdef PERL_MAD
	op_getmad(o,newop,'O');
#else
	op_free(o);
#endif
	return newop;
    }
    return o;
}

OP *
Perl_ck_rfun(pTHX_ OP *o)
{
    const OPCODE type = o->op_type;

    PERL_ARGS_ASSERT_CK_RFUN;

    return refkids(ck_fun(o), type);
}

OP *
Perl_ck_listiob(pTHX_ OP *o)
{
    OP *kid;

    PERL_ARGS_ASSERT_CK_LISTIOB;

    kid = cLISTOPo->op_first;
    if (!kid) {
	o = force_list(o);
	kid = cLISTOPo->op_first;
    }
    if (kid->op_type == OP_PUSHMARK)
	kid = kid->op_sibling;
    if (kid && o->op_flags & OPf_STACKED)
	kid = kid->op_sibling;
    else if (kid && !kid->op_sibling) {		/* print HANDLE; */
	if (kid->op_type == OP_CONST && kid->op_private & OPpCONST_BARE
	 && !kid->op_folded) {
	    o->op_flags |= OPf_STACKED;	/* make it a filehandle */
	    kid = newUNOP(OP_RV2GV, OPf_REF, scalar(kid));
	    cLISTOPo->op_first->op_sibling = kid;
	    cLISTOPo->op_last = kid;
	    kid = kid->op_sibling;
	}
    }

    if (!kid)
	op_append_elem(o->op_type, o, newDEFSVOP());

    if (o->op_type == OP_PRTF) return modkids(listkids(o), OP_PRTF);
    return listkids(o);
}

OP *
Perl_ck_smartmatch(pTHX_ OP *o)
{
    dVAR;
    PERL_ARGS_ASSERT_CK_SMARTMATCH;
    if (0 == (o->op_flags & OPf_SPECIAL)) {
	OP *first  = cBINOPo->op_first;
	OP *second = first->op_sibling;
	
	/* Implicitly take a reference to an array or hash */
	first->op_sibling = NULL;
	first = cBINOPo->op_first = ref_array_or_hash(first);
	second = first->op_sibling = ref_array_or_hash(second);
	
	/* Implicitly take a reference to a regular expression */
	if (first->op_type == OP_MATCH) {
	    first->op_type = OP_QR;
	    first->op_ppaddr = PL_ppaddr[OP_QR];
	}
	if (second->op_type == OP_MATCH) {
	    second->op_type = OP_QR;
	    second->op_ppaddr = PL_ppaddr[OP_QR];
        }
    }
    
    return o;
}


OP *
Perl_ck_sassign(pTHX_ OP *o)
{
    dVAR;
    OP * const kid = cLISTOPo->op_first;

    PERL_ARGS_ASSERT_CK_SASSIGN;

    /* has a disposable target? */
    if ((PL_opargs[kid->op_type] & OA_TARGLEX)
	&& !(kid->op_flags & OPf_STACKED)
	/* Cannot steal the second time! */
	&& !(kid->op_private & OPpTARGET_MY)
	/* Keep the full thing for madskills */
	&& !PL_madskills
	)
    {
	OP * const kkid = kid->op_sibling;

	/* Can just relocate the target. */
	if (kkid && kkid->op_type == OP_PADSV
	    && !(kkid->op_private & OPpLVAL_INTRO))
	{
	    kid->op_targ = kkid->op_targ;
	    kkid->op_targ = 0;
	    /* Now we do not need PADSV and SASSIGN. */
	    kid->op_sibling = o->op_sibling;	/* NULL */
	    cLISTOPo->op_first = NULL;
	    op_free(o);
	    op_free(kkid);
	    kid->op_private |= OPpTARGET_MY;	/* Used for context settings */
	    return kid;
	}
    }
    if (kid->op_sibling) {
	OP *kkid = kid->op_sibling;
	/* For state variable assignment, kkid is a list op whose op_last
	   is a padsv. */
	if ((kkid->op_type == OP_PADSV ||
	     (OP_TYPE_IS_OR_WAS(kkid, OP_LIST) &&
	      (kkid = cLISTOPx(kkid)->op_last)->op_type == OP_PADSV
	     )
	    )
		&& (kkid->op_private & OPpLVAL_INTRO)
		&& SvPAD_STATE(*av_fetch(PL_comppad_name, kkid->op_targ, FALSE))) {
	    const PADOFFSET target = kkid->op_targ;
	    OP *const other = newOP(OP_PADSV,
				    kkid->op_flags
				    | ((kkid->op_private & ~OPpLVAL_INTRO) << 8));
	    OP *const first = newOP(OP_NULL, 0);
	    OP *const nullop = newCONDOP(0, first, o, other);
	    OP *const condop = first->op_next;
	    /* hijacking PADSTALE for uninitialized state variables */
	    SvPADSTALE_on(PAD_SVl(target));

	    condop->op_type = OP_ONCE;
	    condop->op_ppaddr = PL_ppaddr[OP_ONCE];
	    condop->op_targ = target;
	    other->op_targ = target;

	    /* Because we change the type of the op here, we will skip the
	       assignment binop->op_last = binop->op_first->op_sibling; at the
	       end of Perl_newBINOP(). So need to do it here. */
	    cBINOPo->op_last = cBINOPo->op_first->op_sibling;

	    return nullop;
	}
    }
    return o;
}

OP *
Perl_ck_match(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_CK_MATCH;

    if (o->op_type != OP_QR && PL_compcv) {
	const PADOFFSET offset = pad_findmy_pvs("$_", 0);
	if (offset != NOT_IN_PAD && !(PAD_COMPNAME_FLAGS_isOUR(offset))) {
	    o->op_targ = offset;
	    o->op_private |= OPpTARGET_MY;
	}
    }
    if (o->op_type == OP_MATCH || o->op_type == OP_QR)
	o->op_private |= OPpRUNTIME;
    return o;
}

OP *
Perl_ck_method(pTHX_ OP *o)
{
    OP * const kid = cUNOPo->op_first;

    PERL_ARGS_ASSERT_CK_METHOD;

    if (kid->op_type == OP_CONST) {
	SV* sv = kSVOP->op_sv;
	const char * const method = SvPVX_const(sv);
	if (!(strchr(method, ':') || strchr(method, '\''))) {
	    OP *cmop;
	    if (!SvIsCOW_shared_hash(sv)) {
		sv = newSVpvn_share(method, SvUTF8(sv) ? -(I32)SvCUR(sv) : (I32)SvCUR(sv), 0);
	    }
	    else {
		kSVOP->op_sv = NULL;
	    }
	    cmop = newSVOP(OP_METHOD_NAMED, 0, sv);
#ifdef PERL_MAD
	    op_getmad(o,cmop,'O');
#else
	    op_free(o);
#endif
	    return cmop;
	}
    }
    return o;
}

OP *
Perl_ck_null(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_NULL;
    PERL_UNUSED_CONTEXT;
    return o;
}

OP *
Perl_ck_open(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_CK_OPEN;

    S_io_hints(aTHX_ o);
    {
	 /* In case of three-arg dup open remove strictness
	  * from the last arg if it is a bareword. */
	 OP * const first = cLISTOPx(o)->op_first; /* The pushmark. */
	 OP * const last  = cLISTOPx(o)->op_last;  /* The bareword. */
	 OP *oa;
	 const char *mode;

	 if ((last->op_type == OP_CONST) &&		/* The bareword. */
	     (last->op_private & OPpCONST_BARE) &&
	     (last->op_private & OPpCONST_STRICT) &&
	     (oa = first->op_sibling) &&		/* The fh. */
	     (oa = oa->op_sibling) &&			/* The mode. */
	     (oa->op_type == OP_CONST) &&
	     SvPOK(((SVOP*)oa)->op_sv) &&
	     (mode = SvPVX_const(((SVOP*)oa)->op_sv)) &&
	     mode[0] == '>' && mode[1] == '&' &&	/* A dup open. */
	     (last == oa->op_sibling))			/* The bareword. */
	      last->op_private &= ~OPpCONST_STRICT;
    }
    return ck_fun(o);
}

OP *
Perl_ck_repeat(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_REPEAT;

    if (cBINOPo->op_first->op_flags & OPf_PARENS) {
	o->op_private |= OPpREPEAT_DOLIST;
	cBINOPo->op_first = force_list(cBINOPo->op_first);
    }
    else
	scalar(o);
    return o;
}

OP *
Perl_ck_require(pTHX_ OP *o)
{
    dVAR;
    GV* gv;

    PERL_ARGS_ASSERT_CK_REQUIRE;

    if (o->op_flags & OPf_KIDS) {	/* Shall we supply missing .pm? */
	SVOP * const kid = (SVOP*)cUNOPo->op_first;

	if (kid->op_type == OP_CONST && (kid->op_private & OPpCONST_BARE)) {
	    SV * const sv = kid->op_sv;
	    U32 was_readonly = SvREADONLY(sv);
	    char *s;
	    STRLEN len;
	    const char *end;

	    if (was_readonly) {
		    SvREADONLY_off(sv);
	    }   
	    if (SvIsCOW(sv)) sv_force_normal_flags(sv, 0);

	    s = SvPVX(sv);
	    len = SvCUR(sv);
	    end = s + len;
	    for (; s < end; s++) {
		if (*s == ':' && s[1] == ':') {
		    *s = '/';
		    Move(s+2, s+1, end - s - 1, char);
		    --end;
		}
	    }
	    SvEND_set(sv, end);
	    sv_catpvs(sv, ".pm");
	    SvFLAGS(sv) |= was_readonly;
	}
    }

    if (!(o->op_flags & OPf_SPECIAL) /* Wasn't written as CORE::require */
	/* handle override, if any */
     && (gv = gv_override("require", 7))) {
	OP *kid, *newop;
	if (o->op_flags & OPf_KIDS) {
	    kid = cUNOPo->op_first;
	    cUNOPo->op_first = NULL;
	}
	else {
	    kid = newDEFSVOP();
	}
#ifndef PERL_MAD
	op_free(o);
#endif
	newop = S_new_entersubop(aTHX_ gv, kid);
	op_getmad(o,newop,'O');
	return newop;
    }

    return scalar(ck_fun(o));
}

OP *
Perl_ck_return(pTHX_ OP *o)
{
    dVAR;
    OP *kid;

    PERL_ARGS_ASSERT_CK_RETURN;

    kid = cLISTOPo->op_first->op_sibling;
    if (CvLVALUE(PL_compcv)) {
	for (; kid; kid = kid->op_sibling)
	    op_lvalue(kid, OP_LEAVESUBLV);
    }

    return o;
}

OP *
Perl_ck_select(pTHX_ OP *o)
{
    dVAR;
    OP* kid;

    PERL_ARGS_ASSERT_CK_SELECT;

    if (o->op_flags & OPf_KIDS) {
	kid = cLISTOPo->op_first->op_sibling;	/* get past pushmark */
	if (kid && kid->op_sibling) {
	    o->op_type = OP_SSELECT;
	    o->op_ppaddr = PL_ppaddr[OP_SSELECT];
	    o = ck_fun(o);
	    return fold_constants(op_integerize(op_std_init(o)));
	}
    }
    o = ck_fun(o);
    kid = cLISTOPo->op_first->op_sibling;    /* get past pushmark */
    if (kid && kid->op_type == OP_RV2GV)
	kid->op_private &= ~HINT_STRICT_REFS;
    return o;
}

OP *
Perl_ck_shift(pTHX_ OP *o)
{
    dVAR;
    const I32 type = o->op_type;

    PERL_ARGS_ASSERT_CK_SHIFT;

    if (!(o->op_flags & OPf_KIDS)) {
	OP *argop;

	if (!CvUNIQUE(PL_compcv)) {
	    o->op_flags |= OPf_SPECIAL;
	    return o;
	}

	argop = newUNOP(OP_RV2AV, 0, scalar(newGVOP(OP_GV, 0, PL_argvgv)));
#ifdef PERL_MAD
	{
	    OP * const oldo = o;
	    o = newUNOP(type, 0, scalar(argop));
	    op_getmad(oldo,o,'O');
	    return o;
	}
#else
	op_free(o);
	return newUNOP(type, 0, scalar(argop));
#endif
    }
    return scalar(ck_fun(o));
}

OP *
Perl_ck_sort(pTHX_ OP *o)
{
    dVAR;
    OP *firstkid;
    OP *kid;
    HV * const hinthv =
	PL_hints & HINT_LOCALIZE_HH ? GvHV(PL_hintgv) : NULL;
    U8 stacked;

    PERL_ARGS_ASSERT_CK_SORT;

    if (hinthv) {
	    SV ** const svp = hv_fetchs(hinthv, "sort", FALSE);
	    if (svp) {
		const I32 sorthints = (I32)SvIV(*svp);
		if ((sorthints & HINT_SORT_QUICKSORT) != 0)
		    o->op_private |= OPpSORT_QSORT;
		if ((sorthints & HINT_SORT_STABLE) != 0)
		    o->op_private |= OPpSORT_STABLE;
	    }
    }

    if (o->op_flags & OPf_STACKED)
	simplify_sort(o);
    firstkid = cLISTOPo->op_first->op_sibling;		/* get past pushmark */

    if ((stacked = o->op_flags & OPf_STACKED)) {	/* may have been cleared */
	OP *kid = cUNOPx(firstkid)->op_first;		/* get past null */

        /* if the first arg is a code block, process it and mark sort as
         * OPf_SPECIAL */
	if (kid->op_type == OP_SCOPE || kid->op_type == OP_LEAVE) {
	    LINKLIST(kid);
	    if (kid->op_type == OP_LEAVE)
		    op_null(kid);			/* wipe out leave */
	    /* Prevent execution from escaping out of the sort block. */
	    kid->op_next = 0;

	    /* provide scalar context for comparison function/block */
	    kid = scalar(firstkid);
	    kid->op_next = kid;
	    o->op_flags |= OPf_SPECIAL;
	}

	firstkid = firstkid->op_sibling;
    }

    for (kid = firstkid; kid; kid = kid->op_sibling) {
	/* provide list context for arguments */
	list(kid);
	if (stacked)
	    op_lvalue(kid, OP_GREPSTART);
    }

    return o;
}

/* for sort { X } ..., where X is one of
 *   $a <=> $b, $b <= $a, $a cmp $b, $b cmp $a
 * elide the second child of the sort (the one containing X),
 * and set these flags as appropriate
	OPpSORT_NUMERIC;
	OPpSORT_INTEGER;
	OPpSORT_DESCEND;
 * Also, check and warn on lexical $a, $b.
 */

STATIC void
S_simplify_sort(pTHX_ OP *o)
{
    dVAR;
    OP *kid = cLISTOPo->op_first->op_sibling;	/* get past pushmark */
    OP *k;
    int descending;
    GV *gv;
    const char *gvname;
    bool have_scopeop;

    PERL_ARGS_ASSERT_SIMPLIFY_SORT;

    kid = kUNOP->op_first;				/* get past null */
    if (!(have_scopeop = kid->op_type == OP_SCOPE)
     && kid->op_type != OP_LEAVE)
	return;
    kid = kLISTOP->op_last;				/* get past scope */
    switch(kid->op_type) {
	case OP_NCMP:
	case OP_I_NCMP:
	case OP_SCMP:
	    if (!have_scopeop) goto padkids;
	    break;
	default:
	    return;
    }
    k = kid;						/* remember this node*/
    if (kBINOP->op_first->op_type != OP_RV2SV
     || kBINOP->op_last ->op_type != OP_RV2SV)
    {
	/*
	   Warn about my($a) or my($b) in a sort block, *if* $a or $b is
	   then used in a comparison.  This catches most, but not
	   all cases.  For instance, it catches
	       sort { my($a); $a <=> $b }
	   but not
	       sort { my($a); $a < $b ? -1 : $a == $b ? 0 : 1; }
	   (although why you'd do that is anyone's guess).
	*/

       padkids:
	if (!ckWARN(WARN_SYNTAX)) return;
	kid = kBINOP->op_first;
	do {
	    if (kid->op_type == OP_PADSV) {
		SV * const name = AvARRAY(PL_comppad_name)[kid->op_targ];
		if (SvCUR(name) == 2 && *SvPVX(name) == '$'
		 && (SvPVX(name)[1] == 'a' || SvPVX(name)[1] == 'b'))
		    /* diag_listed_as: "my %s" used in sort comparison */
		    Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
				     "\"%s %s\" used in sort comparison",
				      SvPAD_STATE(name) ? "state" : "my",
				      SvPVX(name));
	    }
	} while ((kid = kid->op_sibling));
	return;
    }
    kid = kBINOP->op_first;				/* get past cmp */
    if (kUNOP->op_first->op_type != OP_GV)
	return;
    kid = kUNOP->op_first;				/* get past rv2sv */
    gv = kGVOP_gv;
    if (GvSTASH(gv) != PL_curstash)
	return;
    gvname = GvNAME(gv);
    if (*gvname == 'a' && gvname[1] == '\0')
	descending = 0;
    else if (*gvname == 'b' && gvname[1] == '\0')
	descending = 1;
    else
	return;

    kid = k;						/* back to cmp */
    /* already checked above that it is rv2sv */
    kid = kBINOP->op_last;				/* down to 2nd arg */
    if (kUNOP->op_first->op_type != OP_GV)
	return;
    kid = kUNOP->op_first;				/* get past rv2sv */
    gv = kGVOP_gv;
    if (GvSTASH(gv) != PL_curstash)
	return;
    gvname = GvNAME(gv);
    if ( descending
	 ? !(*gvname == 'a' && gvname[1] == '\0')
	 : !(*gvname == 'b' && gvname[1] == '\0'))
	return;
    o->op_flags &= ~(OPf_STACKED | OPf_SPECIAL);
    if (descending)
	o->op_private |= OPpSORT_DESCEND;
    if (k->op_type == OP_NCMP)
	o->op_private |= OPpSORT_NUMERIC;
    if (k->op_type == OP_I_NCMP)
	o->op_private |= OPpSORT_NUMERIC | OPpSORT_INTEGER;
    kid = cLISTOPo->op_first->op_sibling;
    cLISTOPo->op_first->op_sibling = kid->op_sibling; /* bypass old block */
#ifdef PERL_MAD
    op_getmad(kid,o,'S');			      /* then delete it */
#else
    op_free(kid);				      /* then delete it */
#endif
}

OP *
Perl_ck_split(pTHX_ OP *o)
{
    dVAR;
    OP *kid;

    PERL_ARGS_ASSERT_CK_SPLIT;

    if (o->op_flags & OPf_STACKED)
	return no_fh_allowed(o);

    kid = cLISTOPo->op_first;
    if (kid->op_type != OP_NULL)
	Perl_croak(aTHX_ "panic: ck_split, type=%u", (unsigned) kid->op_type);
    kid = kid->op_sibling;
    op_free(cLISTOPo->op_first);
    if (kid)
	cLISTOPo->op_first = kid;
    else {
	cLISTOPo->op_first = kid = newSVOP(OP_CONST, 0, newSVpvs(" "));
	cLISTOPo->op_last = kid; /* There was only one element previously */
    }

    if (kid->op_type != OP_MATCH || kid->op_flags & OPf_STACKED) {
	OP * const sibl = kid->op_sibling;
	kid->op_sibling = 0;
        kid = pmruntime( newPMOP(OP_MATCH, OPf_SPECIAL), kid, 0, 0); /* OPf_SPECIAL is used to trigger split " " behavior */
	if (cLISTOPo->op_first == cLISTOPo->op_last)
	    cLISTOPo->op_last = kid;
	cLISTOPo->op_first = kid;
	kid->op_sibling = sibl;
    }

    kid->op_type = OP_PUSHRE;
    kid->op_ppaddr = PL_ppaddr[OP_PUSHRE];
    scalar(kid);
    if (((PMOP *)kid)->op_pmflags & PMf_GLOBAL) {
      Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP),
		     "Use of /g modifier is meaningless in split");
    }

    if (!kid->op_sibling)
	op_append_elem(OP_SPLIT, o, newDEFSVOP());

    kid = kid->op_sibling;
    scalar(kid);

    if (!kid->op_sibling)
    {
	op_append_elem(OP_SPLIT, o, newSVOP(OP_CONST, 0, newSViv(0)));
	o->op_private |= OPpSPLIT_IMPLIM;
    }
    assert(kid->op_sibling);

    kid = kid->op_sibling;
    scalar(kid);

    if (kid->op_sibling)
	return too_many_arguments_pv(o,OP_DESC(o), 0);

    return o;
}

OP *
Perl_ck_join(pTHX_ OP *o)
{
    const OP * const kid = cLISTOPo->op_first->op_sibling;

    PERL_ARGS_ASSERT_CK_JOIN;

    if (kid && kid->op_type == OP_MATCH) {
	if (ckWARN(WARN_SYNTAX)) {
            const REGEXP *re = PM_GETRE(kPMOP);
            const SV *msg = re
                    ? newSVpvn_flags( RX_PRECOMP_const(re), RX_PRELEN(re),
                                            SVs_TEMP | ( RX_UTF8(re) ? SVf_UTF8 : 0 ) )
                    : newSVpvs_flags( "STRING", SVs_TEMP );
	    Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
			"/%"SVf"/ should probably be written as \"%"SVf"\"",
			SVfARG(msg), SVfARG(msg));
	}
    }
    return ck_fun(o);
}

/*
=for apidoc Am|CV *|rv2cv_op_cv|OP *cvop|U32 flags

Examines an op, which is expected to identify a subroutine at runtime,
and attempts to determine at compile time which subroutine it identifies.
This is normally used during Perl compilation to determine whether
a prototype can be applied to a function call.  I<cvop> is the op
being considered, normally an C<rv2cv> op.  A pointer to the identified
subroutine is returned, if it could be determined statically, and a null
pointer is returned if it was not possible to determine statically.

Currently, the subroutine can be identified statically if the RV that the
C<rv2cv> is to operate on is provided by a suitable C<gv> or C<const> op.
A C<gv> op is suitable if the GV's CV slot is populated.  A C<const> op is
suitable if the constant value must be an RV pointing to a CV.  Details of
this process may change in future versions of Perl.  If the C<rv2cv> op
has the C<OPpENTERSUB_AMPER> flag set then no attempt is made to identify
the subroutine statically: this flag is used to suppress compile-time
magic on a subroutine call, forcing it to use default runtime behaviour.

If I<flags> has the bit C<RV2CVOPCV_MARK_EARLY> set, then the handling
of a GV reference is modified.  If a GV was examined and its CV slot was
found to be empty, then the C<gv> op has the C<OPpEARLY_CV> flag set.
If the op is not optimised away, and the CV slot is later populated with
a subroutine having a prototype, that flag eventually triggers the warning
"called too early to check prototype".

If I<flags> has the bit C<RV2CVOPCV_RETURN_NAME_GV> set, then instead
of returning a pointer to the subroutine it returns a pointer to the
GV giving the most appropriate name for the subroutine in this context.
Normally this is just the C<CvGV> of the subroutine, but for an anonymous
(C<CvANON>) subroutine that is referenced through a GV it will be the
referencing GV.  The resulting C<GV*> is cast to C<CV*> to be returned.
A null pointer is returned as usual if there is no statically-determinable
subroutine.

=cut
*/

/* shared by toke.c:yylex */
CV *
Perl_find_lexical_cv(pTHX_ PADOFFSET off)
{
    PADNAME *name = PAD_COMPNAME(off);
    CV *compcv = PL_compcv;
    while (PadnameOUTER(name)) {
	assert(PARENT_PAD_INDEX(name));
	compcv = CvOUTSIDE(PL_compcv);
	name = PadlistNAMESARRAY(CvPADLIST(compcv))
		[off = PARENT_PAD_INDEX(name)];
    }
    assert(!PadnameIsOUR(name));
    if (!PadnameIsSTATE(name) && SvMAGICAL(name)) {
	MAGIC * mg = mg_find(name, PERL_MAGIC_proto);
	assert(mg);
	assert(mg->mg_obj);
	return (CV *)mg->mg_obj;
    }
    return (CV *)AvARRAY(PadlistARRAY(CvPADLIST(compcv))[1])[off];
}

CV *
Perl_rv2cv_op_cv(pTHX_ OP *cvop, U32 flags)
{
    OP *rvop;
    CV *cv;
    GV *gv;
    PERL_ARGS_ASSERT_RV2CV_OP_CV;
    if (flags & ~(RV2CVOPCV_MARK_EARLY|RV2CVOPCV_RETURN_NAME_GV))
	Perl_croak(aTHX_ "panic: rv2cv_op_cv bad flags %x", (unsigned)flags);
    if (cvop->op_type != OP_RV2CV)
	return NULL;
    if (cvop->op_private & OPpENTERSUB_AMPER)
	return NULL;
    if (!(cvop->op_flags & OPf_KIDS))
	return NULL;
    rvop = cUNOPx(cvop)->op_first;
    switch (rvop->op_type) {
	case OP_GV: {
	    gv = cGVOPx_gv(rvop);
	    cv = GvCVu(gv);
	    if (!cv) {
		if (flags & RV2CVOPCV_MARK_EARLY)
		    rvop->op_private |= OPpEARLY_CV;
		return NULL;
	    }
	} break;
	case OP_CONST: {
	    SV *rv = cSVOPx_sv(rvop);
	    if (!SvROK(rv))
		return NULL;
	    cv = (CV*)SvRV(rv);
	    gv = NULL;
	} break;
	case OP_PADCV: {
	    cv = find_lexical_cv(rvop->op_targ);
	    gv = NULL;
	} break;
	default: {
	    return NULL;
	} break;
    }
    if (SvTYPE((SV*)cv) != SVt_PVCV)
	return NULL;
    if (flags & RV2CVOPCV_RETURN_NAME_GV) {
	if (!CvANON(cv) || !gv)
	    gv = CvGV(cv);
	return (CV*)gv;
    } else {
	return cv;
    }
}

/*
=for apidoc Am|OP *|ck_entersub_args_list|OP *entersubop

Performs the default fixup of the arguments part of an C<entersub>
op tree.  This consists of applying list context to each of the
argument ops.  This is the standard treatment used on a call marked
with C<&>, or a method call, or a call through a subroutine reference,
or any other call where the callee can't be identified at compile time,
or a call where the callee has no prototype.

=cut
*/

OP *
Perl_ck_entersub_args_list(pTHX_ OP *entersubop)
{
    OP *aop;
    PERL_ARGS_ASSERT_CK_ENTERSUB_ARGS_LIST;
    aop = cUNOPx(entersubop)->op_first;
    if (!aop->op_sibling)
	aop = cUNOPx(aop)->op_first;
    for (aop = aop->op_sibling; aop->op_sibling; aop = aop->op_sibling) {
	if (!(PL_madskills && aop->op_type == OP_STUB)) {
	    list(aop);
	    op_lvalue(aop, OP_ENTERSUB);
	}
    }
    return entersubop;
}

/*
=for apidoc Am|OP *|ck_entersub_args_proto|OP *entersubop|GV *namegv|SV *protosv

Performs the fixup of the arguments part of an C<entersub> op tree
based on a subroutine prototype.  This makes various modifications to
the argument ops, from applying context up to inserting C<refgen> ops,
and checking the number and syntactic types of arguments, as directed by
the prototype.  This is the standard treatment used on a subroutine call,
not marked with C<&>, where the callee can be identified at compile time
and has a prototype.

I<protosv> supplies the subroutine prototype to be applied to the call.
It may be a normal defined scalar, of which the string value will be used.
Alternatively, for convenience, it may be a subroutine object (a C<CV*>
that has been cast to C<SV*>) which has a prototype.  The prototype
supplied, in whichever form, does not need to match the actual callee
referenced by the op tree.

If the argument ops disagree with the prototype, for example by having
an unacceptable number of arguments, a valid op tree is returned anyway.
The error is reflected in the parser state, normally resulting in a single
exception at the top level of parsing which covers all the compilation
errors that occurred.  In the error message, the callee is referred to
by the name defined by the I<namegv> parameter.

=cut
*/

OP *
Perl_ck_entersub_args_proto(pTHX_ OP *entersubop, GV *namegv, SV *protosv)
{
    STRLEN proto_len;
    const char *proto, *proto_end;
    OP *aop, *prev, *cvop;
    int optional = 0;
    I32 arg = 0;
    I32 contextclass = 0;
    const char *e = NULL;
    PERL_ARGS_ASSERT_CK_ENTERSUB_ARGS_PROTO;
    if (SvTYPE(protosv) == SVt_PVCV ? !SvPOK(protosv) : !SvOK(protosv))
	Perl_croak(aTHX_ "panic: ck_entersub_args_proto CV with no proto, "
		   "flags=%lx", (unsigned long) SvFLAGS(protosv));
    if (SvTYPE(protosv) == SVt_PVCV)
	 proto = CvPROTO(protosv), proto_len = CvPROTOLEN(protosv);
    else proto = SvPV(protosv, proto_len);
    proto = S_strip_spaces(aTHX_ proto, &proto_len);
    proto_end = proto + proto_len;
    aop = cUNOPx(entersubop)->op_first;
    if (!aop->op_sibling)
	aop = cUNOPx(aop)->op_first;
    prev = aop;
    aop = aop->op_sibling;
    for (cvop = aop; cvop->op_sibling; cvop = cvop->op_sibling) ;
    while (aop != cvop) {
	OP* o3;
	if (PL_madskills && aop->op_type == OP_STUB) {
	    aop = aop->op_sibling;
	    continue;
	}
	if (PL_madskills && aop->op_type == OP_NULL)
	    o3 = ((UNOP*)aop)->op_first;
	else
	    o3 = aop;

	if (proto >= proto_end)
	    return too_many_arguments_sv(entersubop, gv_ename(namegv), 0);

	switch (*proto) {
	    case ';':
		optional = 1;
		proto++;
		continue;
	    case '_':
		/* _ must be at the end */
		if (proto[1] && !strchr(";@%", proto[1]))
		    goto oops;
	    case '$':
		proto++;
		arg++;
		scalar(aop);
		break;
	    case '%':
	    case '@':
		list(aop);
		arg++;
		break;
	    case '&':
		proto++;
		arg++;
		if (o3->op_type != OP_REFGEN && o3->op_type != OP_UNDEF)
		    bad_type_gv(arg,
			    arg == 1 ? "block or sub {}" : "sub {}",
			    namegv, 0, o3);
		break;
	    case '*':
		/* '*' allows any scalar type, including bareword */
		proto++;
		arg++;
		if (o3->op_type == OP_RV2GV)
		    goto wrapref;	/* autoconvert GLOB -> GLOBref */
		else if (o3->op_type == OP_CONST)
		    o3->op_private &= ~OPpCONST_STRICT;
		else if (o3->op_type == OP_ENTERSUB) {
		    /* accidental subroutine, revert to bareword */
		    OP *gvop = ((UNOP*)o3)->op_first;
		    if (gvop && gvop->op_type == OP_NULL) {
			gvop = ((UNOP*)gvop)->op_first;
			if (gvop) {
			    for (; gvop->op_sibling; gvop = gvop->op_sibling)
				;
			    if (gvop &&
				    (gvop->op_private & OPpENTERSUB_NOPAREN) &&
				    (gvop = ((UNOP*)gvop)->op_first) &&
				    gvop->op_type == OP_GV)
			    {
				GV * const gv = cGVOPx_gv(gvop);
				OP * const sibling = aop->op_sibling;
				SV * const n = newSVpvs("");
#ifdef PERL_MAD
				OP * const oldaop = aop;
#else
				op_free(aop);
#endif
				gv_fullname4(n, gv, "", FALSE);
				aop = newSVOP(OP_CONST, 0, n);
				op_getmad(oldaop,aop,'O');
				prev->op_sibling = aop;
				aop->op_sibling = sibling;
			    }
			}
		    }
		}
		scalar(aop);
		break;
	    case '+':
		proto++;
		arg++;
		if (o3->op_type == OP_RV2AV ||
		    o3->op_type == OP_PADAV ||
		    o3->op_type == OP_RV2HV ||
		    o3->op_type == OP_PADHV
		) {
		    goto wrapref;
		}
		scalar(aop);
		break;
	    case '[': case ']':
		goto oops;
		break;
	    case '\\':
		proto++;
		arg++;
	    again:
		switch (*proto++) {
		    case '[':
			if (contextclass++ == 0) {
			    e = strchr(proto, ']');
			    if (!e || e == proto)
				goto oops;
			}
			else
			    goto oops;
			goto again;
			break;
		    case ']':
			if (contextclass) {
			    const char *p = proto;
			    const char *const end = proto;
			    contextclass = 0;
			    while (*--p != '[')
				/* \[$] accepts any scalar lvalue */
				if (*p == '$'
				 && Perl_op_lvalue_flags(aTHX_
				     scalar(o3),
				     OP_READ, /* not entersub */
				     OP_LVALUE_NO_CROAK
				    )) goto wrapref;
			    bad_type_gv(arg, Perl_form(aTHX_ "one of %.*s",
					(int)(end - p), p),
				    namegv, 0, o3);
			} else
			    goto oops;
			break;
		    case '*':
			if (o3->op_type == OP_RV2GV)
			    goto wrapref;
			if (!contextclass)
			    bad_type_gv(arg, "symbol", namegv, 0, o3);
			break;
		    case '&':
			if (o3->op_type == OP_ENTERSUB)
			    goto wrapref;
			if (!contextclass)
			    bad_type_gv(arg, "subroutine entry", namegv, 0,
				    o3);
			break;
		    case '$':
			if (o3->op_type == OP_RV2SV ||
				o3->op_type == OP_PADSV ||
				o3->op_type == OP_HELEM ||
				o3->op_type == OP_AELEM)
			    goto wrapref;
			if (!contextclass) {
			    /* \$ accepts any scalar lvalue */
			    if (Perl_op_lvalue_flags(aTHX_
				    scalar(o3),
				    OP_READ,  /* not entersub */
				    OP_LVALUE_NO_CROAK
			       )) goto wrapref;
			    bad_type_gv(arg, "scalar", namegv, 0, o3);
			}
			break;
		    case '@':
			if (o3->op_type == OP_RV2AV ||
				o3->op_type == OP_PADAV)
			    goto wrapref;
			if (!contextclass)
			    bad_type_gv(arg, "array", namegv, 0, o3);
			break;
		    case '%':
			if (o3->op_type == OP_RV2HV ||
				o3->op_type == OP_PADHV)
			    goto wrapref;
			if (!contextclass)
			    bad_type_gv(arg, "hash", namegv, 0, o3);
			break;
		    wrapref:
			{
			    OP* const kid = aop;
			    OP* const sib = kid->op_sibling;
			    kid->op_sibling = 0;
			    aop = newUNOP(OP_REFGEN, 0, kid);
			    aop->op_sibling = sib;
			    prev->op_sibling = aop;
			}
			if (contextclass && e) {
			    proto = e + 1;
			    contextclass = 0;
			}
			break;
		    default: goto oops;
		}
		if (contextclass)
		    goto again;
		break;
	    case ' ':
		proto++;
		continue;
	    default:
	    oops: {
                SV* const tmpsv = sv_newmortal();
                gv_efullname3(tmpsv, namegv, NULL);
		Perl_croak(aTHX_ "Malformed prototype for %"SVf": %"SVf,
			SVfARG(tmpsv), SVfARG(protosv));
            }
	}

	op_lvalue(aop, OP_ENTERSUB);
	prev = aop;
	aop = aop->op_sibling;
    }
    if (aop == cvop && *proto == '_') {
	/* generate an access to $_ */
	aop = newDEFSVOP();
	aop->op_sibling = prev->op_sibling;
	prev->op_sibling = aop; /* instead of cvop */
    }
    if (!optional && proto_end > proto &&
	(*proto != '@' && *proto != '%' && *proto != ';' && *proto != '_'))
	return too_few_arguments_sv(entersubop, gv_ename(namegv), 0);
    return entersubop;
}

/*
=for apidoc Am|OP *|ck_entersub_args_proto_or_list|OP *entersubop|GV *namegv|SV *protosv

Performs the fixup of the arguments part of an C<entersub> op tree either
based on a subroutine prototype or using default list-context processing.
This is the standard treatment used on a subroutine call, not marked
with C<&>, where the callee can be identified at compile time.

I<protosv> supplies the subroutine prototype to be applied to the call,
or indicates that there is no prototype.  It may be a normal scalar,
in which case if it is defined then the string value will be used
as a prototype, and if it is undefined then there is no prototype.
Alternatively, for convenience, it may be a subroutine object (a C<CV*>
that has been cast to C<SV*>), of which the prototype will be used if it
has one.  The prototype (or lack thereof) supplied, in whichever form,
does not need to match the actual callee referenced by the op tree.

If the argument ops disagree with the prototype, for example by having
an unacceptable number of arguments, a valid op tree is returned anyway.
The error is reflected in the parser state, normally resulting in a single
exception at the top level of parsing which covers all the compilation
errors that occurred.  In the error message, the callee is referred to
by the name defined by the I<namegv> parameter.

=cut
*/

OP *
Perl_ck_entersub_args_proto_or_list(pTHX_ OP *entersubop,
	GV *namegv, SV *protosv)
{
    PERL_ARGS_ASSERT_CK_ENTERSUB_ARGS_PROTO_OR_LIST;
    if (SvTYPE(protosv) == SVt_PVCV ? SvPOK(protosv) : SvOK(protosv))
	return ck_entersub_args_proto(entersubop, namegv, protosv);
    else
	return ck_entersub_args_list(entersubop);
}

OP *
Perl_ck_entersub_args_core(pTHX_ OP *entersubop, GV *namegv, SV *protosv)
{
    int opnum = SvTYPE(protosv) == SVt_PVCV ? 0 : (int)SvUV(protosv);
    OP *aop = cUNOPx(entersubop)->op_first;

    PERL_ARGS_ASSERT_CK_ENTERSUB_ARGS_CORE;

    if (!opnum) {
	OP *cvop;
	if (!aop->op_sibling)
	    aop = cUNOPx(aop)->op_first;
	aop = aop->op_sibling;
	for (cvop = aop; cvop->op_sibling; cvop = cvop->op_sibling) ;
	if (PL_madskills) while (aop != cvop && aop->op_type == OP_STUB) {
	    aop = aop->op_sibling;
	}
	if (aop != cvop)
	    (void)too_many_arguments_pv(entersubop, GvNAME(namegv), 0);
	
	op_free(entersubop);
	switch(GvNAME(namegv)[2]) {
	case 'F': return newSVOP(OP_CONST, 0,
					newSVpv(CopFILE(PL_curcop),0));
	case 'L': return newSVOP(
	                   OP_CONST, 0,
                           Perl_newSVpvf(aTHX_
	                     "%"IVdf, (IV)CopLINE(PL_curcop)
	                   )
	                 );
	case 'P': return newSVOP(OP_CONST, 0,
	                           (PL_curstash
	                             ? newSVhek(HvNAME_HEK(PL_curstash))
	                             : &PL_sv_undef
	                           )
	                        );
	}
	NOT_REACHED;
    }
    else {
	OP *prev, *cvop;
	U32 flags;
#ifdef PERL_MAD
	bool seenarg = FALSE;
#endif
	if (!aop->op_sibling)
	    aop = cUNOPx(aop)->op_first;
	
	prev = aop;
	aop = aop->op_sibling;
	prev->op_sibling = NULL;
	for (cvop = aop;
	     cvop->op_sibling;
	     prev=cvop, cvop = cvop->op_sibling)
#ifdef PERL_MAD
	    if (PL_madskills && cvop->op_sibling
	     && cvop->op_type != OP_STUB) seenarg = TRUE
#endif
	    ;
	prev->op_sibling = NULL;
	flags = OPf_SPECIAL * !(cvop->op_private & OPpENTERSUB_NOPAREN);
	op_free(cvop);
	if (aop == cvop) aop = NULL;
	op_free(entersubop);

	if (opnum == OP_ENTEREVAL
	 && GvNAMELEN(namegv)==9 && strnEQ(GvNAME(namegv), "evalbytes", 9))
	    flags |= OPpEVAL_BYTES <<8;
	
	switch (PL_opargs[opnum] & OA_CLASS_MASK) {
	case OA_UNOP:
	case OA_BASEOP_OR_UNOP:
	case OA_FILESTATOP:
	    return aop ? newUNOP(opnum,flags,aop) : newOP(opnum,flags);
	case OA_BASEOP:
	    if (aop) {
#ifdef PERL_MAD
		if (!PL_madskills || seenarg)
#endif
		    (void)too_many_arguments_pv(aop, GvNAME(namegv), 0);
		op_free(aop);
	    }
	    return opnum == OP_RUNCV
		? newPVOP(OP_RUNCV,0,NULL)
		: newOP(opnum,0);
	default:
	    return convert(opnum,0,aop);
	}
    }
    assert(0);
    return entersubop;
}

/*
=for apidoc Am|void|cv_get_call_checker|CV *cv|Perl_call_checker *ckfun_p|SV **ckobj_p

Retrieves the function that will be used to fix up a call to I<cv>.
Specifically, the function is applied to an C<entersub> op tree for a
subroutine call, not marked with C<&>, where the callee can be identified
at compile time as I<cv>.

The C-level function pointer is returned in I<*ckfun_p>, and an SV
argument for it is returned in I<*ckobj_p>.  The function is intended
to be called in this manner:

    entersubop = (*ckfun_p)(aTHX_ entersubop, namegv, (*ckobj_p));

In this call, I<entersubop> is a pointer to the C<entersub> op,
which may be replaced by the check function, and I<namegv> is a GV
supplying the name that should be used by the check function to refer
to the callee of the C<entersub> op if it needs to emit any diagnostics.
It is permitted to apply the check function in non-standard situations,
such as to a call to a different subroutine or to a method call.

By default, the function is
L<Perl_ck_entersub_args_proto_or_list|/ck_entersub_args_proto_or_list>,
and the SV parameter is I<cv> itself.  This implements standard
prototype processing.  It can be changed, for a particular subroutine,
by L</cv_set_call_checker>.

=cut
*/

void
Perl_cv_get_call_checker(pTHX_ CV *cv, Perl_call_checker *ckfun_p, SV **ckobj_p)
{
    MAGIC *callmg;
    PERL_ARGS_ASSERT_CV_GET_CALL_CHECKER;
    callmg = SvMAGICAL((SV*)cv) ? mg_find((SV*)cv, PERL_MAGIC_checkcall) : NULL;
    if (callmg) {
	*ckfun_p = DPTR2FPTR(Perl_call_checker, callmg->mg_ptr);
	*ckobj_p = callmg->mg_obj;
    } else {
	*ckfun_p = Perl_ck_entersub_args_proto_or_list;
	*ckobj_p = (SV*)cv;
    }
}

/*
=for apidoc Am|void|cv_set_call_checker|CV *cv|Perl_call_checker ckfun|SV *ckobj

Sets the function that will be used to fix up a call to I<cv>.
Specifically, the function is applied to an C<entersub> op tree for a
subroutine call, not marked with C<&>, where the callee can be identified
at compile time as I<cv>.

The C-level function pointer is supplied in I<ckfun>, and an SV argument
for it is supplied in I<ckobj>.  The function should be defined like this:

    STATIC OP * ckfun(pTHX_ OP *op, GV *namegv, SV *ckobj)

It is intended to be called in this manner:

    entersubop = ckfun(aTHX_ entersubop, namegv, ckobj);

In this call, I<entersubop> is a pointer to the C<entersub> op,
which may be replaced by the check function, and I<namegv> is a GV
supplying the name that should be used by the check function to refer
to the callee of the C<entersub> op if it needs to emit any diagnostics.
It is permitted to apply the check function in non-standard situations,
such as to a call to a different subroutine or to a method call.

The current setting for a particular CV can be retrieved by
L</cv_get_call_checker>.

=cut
*/

void
Perl_cv_set_call_checker(pTHX_ CV *cv, Perl_call_checker ckfun, SV *ckobj)
{
    PERL_ARGS_ASSERT_CV_SET_CALL_CHECKER;
    if (ckfun == Perl_ck_entersub_args_proto_or_list && ckobj == (SV*)cv) {
	if (SvMAGICAL((SV*)cv))
	    mg_free_type((SV*)cv, PERL_MAGIC_checkcall);
    } else {
	MAGIC *callmg;
	sv_magic((SV*)cv, &PL_sv_undef, PERL_MAGIC_checkcall, NULL, 0);
	callmg = mg_find((SV*)cv, PERL_MAGIC_checkcall);
	if (callmg->mg_flags & MGf_REFCOUNTED) {
	    SvREFCNT_dec(callmg->mg_obj);
	    callmg->mg_flags &= ~MGf_REFCOUNTED;
	}
	callmg->mg_ptr = FPTR2DPTR(char *, ckfun);
	callmg->mg_obj = ckobj;
	if (ckobj != (SV*)cv) {
	    SvREFCNT_inc_simple_void_NN(ckobj);
	    callmg->mg_flags |= MGf_REFCOUNTED;
	}
	callmg->mg_flags |= MGf_COPY;
    }
}

OP *
Perl_ck_subr(pTHX_ OP *o)
{
    OP *aop, *cvop;
    CV *cv;
    GV *namegv;

    PERL_ARGS_ASSERT_CK_SUBR;

    aop = cUNOPx(o)->op_first;
    if (!aop->op_sibling)
	aop = cUNOPx(aop)->op_first;
    aop = aop->op_sibling;
    for (cvop = aop; cvop->op_sibling; cvop = cvop->op_sibling) ;
    cv = rv2cv_op_cv(cvop, RV2CVOPCV_MARK_EARLY);
    namegv = cv ? (GV*)rv2cv_op_cv(cvop, RV2CVOPCV_RETURN_NAME_GV) : NULL;

    o->op_private &= ~1;
    o->op_private |= OPpENTERSUB_HASTARG;
    o->op_private |= (PL_hints & HINT_STRICT_REFS);
    if (PERLDB_SUB && PL_curstash != PL_debstash)
	o->op_private |= OPpENTERSUB_DB;
    if (cvop->op_type == OP_RV2CV) {
	o->op_private |= (cvop->op_private & OPpENTERSUB_AMPER);
	op_null(cvop);
    } else if (cvop->op_type == OP_METHOD || cvop->op_type == OP_METHOD_NAMED) {
	if (aop->op_type == OP_CONST)
	    aop->op_private &= ~OPpCONST_STRICT;
	else if (aop->op_type == OP_LIST) {
	    OP * const sib = ((UNOP*)aop)->op_first->op_sibling;
	    if (sib && sib->op_type == OP_CONST)
		sib->op_private &= ~OPpCONST_STRICT;
	}
    }

    if (!cv) {
	return ck_entersub_args_list(o);
    } else {
	Perl_call_checker ckfun;
	SV *ckobj;
	cv_get_call_checker(cv, &ckfun, &ckobj);
	if (!namegv) { /* expletive! */
	    /* XXX The call checker API is public.  And it guarantees that
		   a GV will be provided with the right name.  So we have
		   to create a GV.  But it is still not correct, as its
		   stringification will include the package.  What we
		   really need is a new call checker API that accepts a
		   GV or string (or GV or CV). */
	    HEK * const hek = CvNAME_HEK(cv);
	    /* After a syntax error in a lexical sub, the cv that
	       rv2cv_op_cv returns may be a nameless stub. */
	    if (!hek) return ck_entersub_args_list(o);;
	    namegv = (GV *)sv_newmortal();
	    gv_init_pvn(namegv, PL_curstash, HEK_KEY(hek), HEK_LEN(hek),
			SVf_UTF8 * !!HEK_UTF8(hek));
	}
	return ckfun(aTHX_ o, namegv, ckobj);
    }
}

OP *
Perl_ck_svconst(pTHX_ OP *o)
{
    SV * const sv = cSVOPo->op_sv;
    PERL_ARGS_ASSERT_CK_SVCONST;
    PERL_UNUSED_CONTEXT;
#ifdef PERL_OLD_COPY_ON_WRITE
    if (SvIsCOW(sv)) sv_force_normal(sv);
#elif defined(PERL_NEW_COPY_ON_WRITE)
    /* Since the read-only flag may be used to protect a string buffer, we
       cannot do copy-on-write with existing read-only scalars that are not
       already copy-on-write scalars.  To allow $_ = "hello" to do COW with
       that constant, mark the constant as COWable here, if it is not
       already read-only. */
    if (!SvREADONLY(sv) && !SvIsCOW(sv) && SvCANCOW(sv)) {
	SvIsCOW_on(sv);
	CowREFCNT(sv) = 0;
# ifdef PERL_DEBUG_READONLY_COW
	sv_buf_to_ro(sv);
# endif
    }
#endif
    SvREADONLY_on(sv);
    return o;
}

OP *
Perl_ck_trunc(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_TRUNC;

    if (o->op_flags & OPf_KIDS) {
	SVOP *kid = (SVOP*)cUNOPo->op_first;

	if (kid->op_type == OP_NULL)
	    kid = (SVOP*)kid->op_sibling;
	if (kid && kid->op_type == OP_CONST &&
	    (kid->op_private & OPpCONST_BARE) &&
	    !kid->op_folded)
	{
	    o->op_flags |= OPf_SPECIAL;
	    kid->op_private &= ~OPpCONST_STRICT;
	}
    }
    return ck_fun(o);
}

OP *
Perl_ck_substr(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_SUBSTR;

    o = ck_fun(o);
    if ((o->op_flags & OPf_KIDS) && (o->op_private == 4)) {
	OP *kid = cLISTOPo->op_first;

	if (kid->op_type == OP_NULL)
	    kid = kid->op_sibling;
	if (kid)
	    kid->op_flags |= OPf_MOD;

    }
    return o;
}

OP *
Perl_ck_tell(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_TELL;
    o = ck_fun(o);
    if (o->op_flags & OPf_KIDS) {
     OP *kid = cLISTOPo->op_first;
     if (kid->op_type == OP_NULL && kid->op_sibling) kid = kid->op_sibling;
     if (kid->op_type == OP_RV2GV) kid->op_private |= OPpALLOW_FAKE;
    }
    return o;
}

OP *
Perl_ck_each(pTHX_ OP *o)
{
    dVAR;
    OP *kid = o->op_flags & OPf_KIDS ? cUNOPo->op_first : NULL;
    const unsigned orig_type  = o->op_type;
    const unsigned array_type = orig_type == OP_EACH ? OP_AEACH
	                      : orig_type == OP_KEYS ? OP_AKEYS : OP_AVALUES;
    const unsigned ref_type   = orig_type == OP_EACH ? OP_REACH
	                      : orig_type == OP_KEYS ? OP_RKEYS : OP_RVALUES;

    PERL_ARGS_ASSERT_CK_EACH;

    if (kid) {
	switch (kid->op_type) {
	    case OP_PADHV:
	    case OP_RV2HV:
		break;
	    case OP_PADAV:
	    case OP_RV2AV:
		CHANGE_TYPE(o, array_type);
		break;
	    case OP_CONST:
		if (kid->op_private == OPpCONST_BARE
		 || !SvROK(cSVOPx_sv(kid))
		 || (  SvTYPE(SvRV(cSVOPx_sv(kid))) != SVt_PVAV
		    && SvTYPE(SvRV(cSVOPx_sv(kid))) != SVt_PVHV  )
		   )
		    /* we let ck_fun handle it */
		    break;
	    default:
		CHANGE_TYPE(o, ref_type);
		scalar(kid);
	}
    }
    /* if treating as a reference, defer additional checks to runtime */
    if (o->op_type == ref_type) {
	/* diag_listed_as: keys on reference is experimental */
	Perl_ck_warner_d(aTHX_ packWARN(WARN_EXPERIMENTAL__AUTODEREF),
			      "%s is experimental", PL_op_desc[ref_type]);
	return o;
    }
    return ck_fun(o);
}

OP *
Perl_ck_length(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_LENGTH;

    o = ck_fun(o);

    if (ckWARN(WARN_SYNTAX)) {
        const OP *kid = o->op_flags & OPf_KIDS ? cLISTOPo->op_first : NULL;

        if (kid) {
            SV *name = NULL;
            const bool hash = kid->op_type == OP_PADHV
                           || kid->op_type == OP_RV2HV;
            switch (kid->op_type) {
                case OP_PADHV:
                case OP_PADAV:
                case OP_RV2HV:
                case OP_RV2AV:
		    name = S_op_varname(aTHX_ kid);
                    break;
                default:
                    return o;
            }
            if (name)
                Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
                    "length() used on %"SVf" (did you mean \"scalar(%s%"SVf
                    ")\"?)",
                    name, hash ? "keys " : "", name
                );
            else if (hash)
     /* diag_listed_as: length() used on %s (did you mean "scalar(%s)"?) */
                Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
                    "length() used on %%hash (did you mean \"scalar(keys %%hash)\"?)");
            else
     /* diag_listed_as: length() used on %s (did you mean "scalar(%s)"?) */
                Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
                    "length() used on @array (did you mean \"scalar(@array)\"?)");
        }
    }

    return o;
}

/* Check for in place reverse and sort assignments like "@a = reverse @a"
   and modify the optree to make them work inplace */

STATIC void
S_inplace_aassign(pTHX_ OP *o) {

    OP *modop, *modop_pushmark;
    OP *oright;
    OP *oleft, *oleft_pushmark;

    PERL_ARGS_ASSERT_INPLACE_AASSIGN;

    assert((o->op_flags & OPf_WANT) == OPf_WANT_VOID);

    assert(cUNOPo->op_first->op_type == OP_NULL);
    modop_pushmark = cUNOPx(cUNOPo->op_first)->op_first;
    assert(modop_pushmark->op_type == OP_PUSHMARK);
    modop = modop_pushmark->op_sibling;

    if (modop->op_type != OP_SORT && modop->op_type != OP_REVERSE)
	return;

    /* no other operation except sort/reverse */
    if (modop->op_sibling)
	return;

    assert(cUNOPx(modop)->op_first->op_type == OP_PUSHMARK);
    if (!(oright = cUNOPx(modop)->op_first->op_sibling)) return;

    if (modop->op_flags & OPf_STACKED) {
	/* skip sort subroutine/block */
	assert(oright->op_type == OP_NULL);
	oright = oright->op_sibling;
    }

    assert(cUNOPo->op_first->op_sibling->op_type == OP_NULL);
    oleft_pushmark = cUNOPx(cUNOPo->op_first->op_sibling)->op_first;
    assert(oleft_pushmark->op_type == OP_PUSHMARK);
    oleft = oleft_pushmark->op_sibling;

    /* Check the lhs is an array */
    if (!oleft ||
	(oleft->op_type != OP_RV2AV && oleft->op_type != OP_PADAV)
	|| oleft->op_sibling
	|| (oleft->op_private & OPpLVAL_INTRO)
    )
	return;

    /* Only one thing on the rhs */
    if (oright->op_sibling)
	return;

    /* check the array is the same on both sides */
    if (oleft->op_type == OP_RV2AV) {
	if (oright->op_type != OP_RV2AV
	    || !cUNOPx(oright)->op_first
	    || cUNOPx(oright)->op_first->op_type != OP_GV
	    || cUNOPx(oleft )->op_first->op_type != OP_GV
	    || cGVOPx_gv(cUNOPx(oleft)->op_first) !=
	       cGVOPx_gv(cUNOPx(oright)->op_first)
	)
	    return;
    }
    else if (oright->op_type != OP_PADAV
	|| oright->op_targ != oleft->op_targ
    )
	return;

    /* This actually is an inplace assignment */

    modop->op_private |= OPpSORT_INPLACE;

    /* transfer MODishness etc from LHS arg to RHS arg */
    oright->op_flags = oleft->op_flags;

    /* remove the aassign op and the lhs */
    op_null(o);
    op_null(oleft_pushmark);
    if (oleft->op_type == OP_RV2AV && cUNOPx(oleft)->op_first)
	op_null(cUNOPx(oleft)->op_first);
    op_null(oleft);
}



/* mechanism for deferring recursion in rpeep() */

#define MAX_DEFERRED 4

#define DEFER(o) \
  STMT_START { \
    if (defer_ix == (MAX_DEFERRED-1)) { \
        OP **defer = defer_queue[defer_base]; \
        CALL_RPEEP(*defer); \
        S_prune_chain_head(aTHX_ defer); \
	defer_base = (defer_base + 1) % MAX_DEFERRED; \
	defer_ix--; \
    } \
    defer_queue[(defer_base + ++defer_ix) % MAX_DEFERRED] = &(o); \
  } STMT_END

#define IS_AND_OP(o)   (o->op_type == OP_AND)
#define IS_OR_OP(o)    (o->op_type == OP_OR)


STATIC void
S_null_listop_in_list_context(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_NULL_LISTOP_IN_LIST_CONTEXT;

    /* This is an OP_LIST in list context. That means we
     * can ditch the OP_LIST and the OP_PUSHMARK within. */

    op_null(cUNOPo->op_first); /* NULL the pushmark */
    op_null(o); /* NULL the list */
}

/* A peephole optimizer.  We visit the ops in the order they're to execute.
 * See the comments at the top of this file for more details about when
 * peep() is called */

void
Perl_rpeep(pTHX_ OP *o)
{
    dVAR;
    OP* oldop = NULL;
    OP* oldoldop = NULL;
    OP** defer_queue[MAX_DEFERRED]; /* small queue of deferred branches */
    int defer_base = 0;
    int defer_ix = -1;

    if (!o || o->op_opt)
	return;
    ENTER;
    SAVEOP();
    SAVEVPTR(PL_curcop);
    for (;; o = o->op_next) {
	if (o && o->op_opt)
	    o = NULL;
	if (!o) {
	    while (defer_ix >= 0) {
                OP **defer =
                        defer_queue[(defer_base + defer_ix--) % MAX_DEFERRED];
                CALL_RPEEP(*defer);
                S_prune_chain_head(aTHX_ defer);
            }
	    break;
	}

	/* By default, this op has now been optimised. A couple of cases below
	   clear this again.  */
	o->op_opt = 1;
	PL_op = o;


        /* The following will have the OP_LIST and OP_PUSHMARK
         * patched out later IF the OP_LIST is in list context.
         * So in that case, we can set the this OP's op_next
         * to skip to after the OP_PUSHMARK:
         *   a THIS -> b
         *   d list -> e
         *   b   pushmark -> c
         *   c   whatever -> d
         *   e whatever
         * will eventually become:
         *   a THIS -> c
         *   - ex-list -> -
         *   -   ex-pushmark -> -
         *   c   whatever -> e
         *   e whatever
         */
        {
            OP *sibling;
            OP *other_pushmark;
            if (OP_TYPE_IS(o->op_next, OP_PUSHMARK)
                && (sibling = o->op_sibling)
                && sibling->op_type == OP_LIST
                /* This KIDS check is likely superfluous since OP_LIST
                 * would otherwise be an OP_STUB. */
                && sibling->op_flags & OPf_KIDS
                && (sibling->op_flags & OPf_WANT) == OPf_WANT_LIST
                && (other_pushmark = cLISTOPx(sibling)->op_first)
                /* Pointer equality also effectively checks that it's a
                 * pushmark. */
                && other_pushmark == o->op_next)
            {
                o->op_next = other_pushmark->op_next;
                null_listop_in_list_context(sibling);
            }
        }

	switch (o->op_type) {
	case OP_DBSTATE:
	    PL_curcop = ((COP*)o);		/* for warnings */
	    break;
	case OP_NEXTSTATE:
	    PL_curcop = ((COP*)o);		/* for warnings */

	    /* Optimise a "return ..." at the end of a sub to just be "...".
	     * This saves 2 ops. Before:
	     * 1  <;> nextstate(main 1 -e:1) v ->2
	     * 4  <@> return K ->5
	     * 2    <0> pushmark s ->3
	     * -    <1> ex-rv2sv sK/1 ->4
	     * 3      <#> gvsv[*cat] s ->4
	     *
	     * After:
	     * -  <@> return K ->-
	     * -    <0> pushmark s ->2
	     * -    <1> ex-rv2sv sK/1 ->-
	     * 2      <$> gvsv(*cat) s ->3
	     */
	    {
		OP *next = o->op_next;
		OP *sibling = o->op_sibling;
		if (   OP_TYPE_IS(next, OP_PUSHMARK)
		    && OP_TYPE_IS(sibling, OP_RETURN)
		    && OP_TYPE_IS(sibling->op_next, OP_LINESEQ)
		    && OP_TYPE_IS(sibling->op_next->op_next, OP_LEAVESUB)
		    && cUNOPx(sibling)->op_first == next
		    && next->op_sibling && next->op_sibling->op_next
		    && next->op_next
		) {
		    /* Look through the PUSHMARK's siblings for one that
		     * points to the RETURN */
		    OP *top = next->op_sibling;
		    while (top && top->op_next) {
			if (top->op_next == sibling) {
			    top->op_next = sibling->op_next;
			    o->op_next = next->op_next;
			    break;
			}
			top = top->op_sibling;
		    }
		}
	    }

	    /* Optimise 'my $x; my $y;' into 'my ($x, $y);'
             *
	     * This latter form is then suitable for conversion into padrange
	     * later on. Convert:
	     *
	     *   nextstate1 -> padop1 -> nextstate2 -> padop2 -> nextstate3
	     *
	     * into:
	     *
	     *   nextstate1 ->     listop     -> nextstate3
	     *                 /            \
	     *         pushmark -> padop1 -> padop2
	     */
	    if (o->op_next && (
		    o->op_next->op_type == OP_PADSV
		 || o->op_next->op_type == OP_PADAV
		 || o->op_next->op_type == OP_PADHV
		)
		&& !(o->op_next->op_private & ~OPpLVAL_INTRO)
		&& o->op_next->op_next && o->op_next->op_next->op_type == OP_NEXTSTATE
		&& o->op_next->op_next->op_next && (
		    o->op_next->op_next->op_next->op_type == OP_PADSV
		 || o->op_next->op_next->op_next->op_type == OP_PADAV
		 || o->op_next->op_next->op_next->op_type == OP_PADHV
		)
		&& !(o->op_next->op_next->op_next->op_private & ~OPpLVAL_INTRO)
		&& o->op_next->op_next->op_next->op_next && o->op_next->op_next->op_next->op_next->op_type == OP_NEXTSTATE
		&& (!CopLABEL((COP*)o)) /* Don't mess with labels */
		&& (!CopLABEL((COP*)o->op_next->op_next)) /* ... */
	    ) {
		OP *first;
		OP *last;
		OP *newop;

		first = o->op_next;
		last = o->op_next->op_next->op_next;

		newop = newLISTOP(OP_LIST, 0, first, last);
		newop->op_flags |= OPf_PARENS;
		newop->op_flags = (newop->op_flags & ~OPf_WANT) | OPf_WANT_VOID;

		/* Kill nextstate2 between padop1/padop2 */
		op_free(first->op_next);

		first->op_next = last;                /* padop2 */
		first->op_sibling = last;             /* ... */
		o->op_next = cUNOPx(newop)->op_first; /* pushmark */
		o->op_next->op_next = first;          /* padop1 */
		o->op_next->op_sibling = first;       /* ... */
		newop->op_next = last->op_next;       /* nextstate3 */
		newop->op_sibling = last->op_sibling;
		last->op_next = newop;                /* listop */
		last->op_sibling = NULL;
		o->op_sibling = newop;                /* ... */

		newop->op_flags = (newop->op_flags & ~OPf_WANT) | OPf_WANT_VOID;

		/* Ensure pushmark has this flag if padops do */
		if (first->op_flags & OPf_MOD && last->op_flags & OPf_MOD) {
		    o->op_next->op_flags |= OPf_MOD;
		}

		break;
	    }

	    /* Two NEXTSTATEs in a row serve no purpose. Except if they happen
	       to carry two labels. For now, take the easier option, and skip
	       this optimisation if the first NEXTSTATE has a label.  */
	    if (!CopLABEL((COP*)o) && !PERLDB_NOOPT) {
		OP *nextop = o->op_next;
		while (nextop && nextop->op_type == OP_NULL)
		    nextop = nextop->op_next;

		if (nextop && (nextop->op_type == OP_NEXTSTATE)) {
		    COP *firstcop = (COP *)o;
		    COP *secondcop = (COP *)nextop;
		    /* We want the COP pointed to by o (and anything else) to
		       become the next COP down the line.  */
		    cop_free(firstcop);

		    firstcop->op_next = secondcop->op_next;

		    /* Now steal all its pointers, and duplicate the other
		       data.  */
		    firstcop->cop_line = secondcop->cop_line;
#ifdef USE_ITHREADS
		    firstcop->cop_stashoff = secondcop->cop_stashoff;
		    firstcop->cop_file = secondcop->cop_file;
#else
		    firstcop->cop_stash = secondcop->cop_stash;
		    firstcop->cop_filegv = secondcop->cop_filegv;
#endif
		    firstcop->cop_hints = secondcop->cop_hints;
		    firstcop->cop_seq = secondcop->cop_seq;
		    firstcop->cop_warnings = secondcop->cop_warnings;
		    firstcop->cop_hints_hash = secondcop->cop_hints_hash;

#ifdef USE_ITHREADS
		    secondcop->cop_stashoff = 0;
		    secondcop->cop_file = NULL;
#else
		    secondcop->cop_stash = NULL;
		    secondcop->cop_filegv = NULL;
#endif
		    secondcop->cop_warnings = NULL;
		    secondcop->cop_hints_hash = NULL;

		    /* If we use op_null(), and hence leave an ex-COP, some
		       warnings are misreported. For example, the compile-time
		       error in 'use strict; no strict refs;'  */
		    secondcop->op_type = OP_NULL;
		    secondcop->op_ppaddr = PL_ppaddr[OP_NULL];
		}
	    }
	    break;

	case OP_CONCAT:
	    if (o->op_next && o->op_next->op_type == OP_STRINGIFY) {
		if (o->op_next->op_private & OPpTARGET_MY) {
		    if (o->op_flags & OPf_STACKED) /* chained concats */
			break; /* ignore_optimization */
		    else {
			/* assert(PL_opargs[o->op_type] & OA_TARGLEX); */
			o->op_targ = o->op_next->op_targ;
			o->op_next->op_targ = 0;
			o->op_private |= OPpTARGET_MY;
		    }
		}
		op_null(o->op_next);
	    }
	    break;
	case OP_STUB:
	    if ((o->op_flags & OPf_WANT) != OPf_WANT_LIST) {
		break; /* Scalar stub must produce undef.  List stub is noop */
	    }
	    goto nothin;
	case OP_NULL:
	    if (o->op_targ == OP_NEXTSTATE
		|| o->op_targ == OP_DBSTATE)
	    {
		PL_curcop = ((COP*)o);
	    }
	    /* XXX: We avoid setting op_seq here to prevent later calls
	       to rpeep() from mistakenly concluding that optimisation
	       has already occurred. This doesn't fix the real problem,
	       though (See 20010220.007). AMS 20010719 */
	    /* op_seq functionality is now replaced by op_opt */
	    o->op_opt = 0;
	    /* FALL THROUGH */
	case OP_SCALAR:
	case OP_LINESEQ:
	case OP_SCOPE:
	nothin:
	    if (oldop) {
		oldop->op_next = o->op_next;
		o->op_opt = 0;
		continue;
	    }
	    break;

        case OP_PUSHMARK:

            /* Convert a series of PAD ops for my vars plus support into a
             * single padrange op. Basically
             *
             *    pushmark -> pad[ahs]v -> pad[ahs]?v -> ... -> (list) -> rest
             *
             * becomes, depending on circumstances, one of
             *
             *    padrange  ----------------------------------> (list) -> rest
             *    padrange  --------------------------------------------> rest
             *
             * where all the pad indexes are sequential and of the same type
             * (INTRO or not).
             * We convert the pushmark into a padrange op, then skip
             * any other pad ops, and possibly some trailing ops.
             * Note that we don't null() the skipped ops, to make it
             * easier for Deparse to undo this optimisation (and none of
             * the skipped ops are holding any resourses). It also makes
             * it easier for find_uninit_var(), as it can just ignore
             * padrange, and examine the original pad ops.
             */
        {
            OP *p;
            OP *followop = NULL; /* the op that will follow the padrange op */
            U8 count = 0;
            U8 intro = 0;
            PADOFFSET base = 0; /* init only to stop compiler whining */
            U8 gimme       = 0; /* init only to stop compiler whining */
            bool defav = 0;  /* seen (...) = @_ */
            bool reuse = 0;  /* reuse an existing padrange op */

            /* look for a pushmark -> gv[_] -> rv2av */

            {
                GV *gv;
                OP *rv2av, *q;
                p = o->op_next;
                if (   p->op_type == OP_GV
                    && (gv = cGVOPx_gv(p))
                    && GvNAMELEN_get(gv) == 1
                    && *GvNAME_get(gv) == '_'
                    && GvSTASH(gv) == PL_defstash
                    && (rv2av = p->op_next)
                    && rv2av->op_type == OP_RV2AV
                    && !(rv2av->op_flags & OPf_REF)
                    && !(rv2av->op_private & (OPpLVAL_INTRO|OPpMAYBE_LVSUB))
                    && ((rv2av->op_flags & OPf_WANT) == OPf_WANT_LIST)
                    && o->op_sibling == rv2av /* these two for Deparse */
                    && cUNOPx(rv2av)->op_first == p
                ) {
                    q = rv2av->op_next;
                    if (q->op_type == OP_NULL)
                        q = q->op_next;
                    if (q->op_type == OP_PUSHMARK) {
                        defav = 1;
                        p = q;
                    }
                }
            }
            if (!defav) {
                /* To allow Deparse to pessimise this, it needs to be able
                 * to restore the pushmark's original op_next, which it
                 * will assume to be the same as op_sibling. */
                if (o->op_next != o->op_sibling)
                    break;
                p = o;
            }

            /* scan for PAD ops */

            for (p = p->op_next; p; p = p->op_next) {
                if (p->op_type == OP_NULL)
                    continue;

                if ((     p->op_type != OP_PADSV
                       && p->op_type != OP_PADAV
                       && p->op_type != OP_PADHV
                    )
                      /* any private flag other than INTRO? e.g. STATE */
                   || (p->op_private & ~OPpLVAL_INTRO)
                )
                    break;

                /* let $a[N] potentially be optimised into AELEMFAST_LEX
                 * instead */
                if (   p->op_type == OP_PADAV
                    && p->op_next
                    && p->op_next->op_type == OP_CONST
                    && p->op_next->op_next
                    && p->op_next->op_next->op_type == OP_AELEM
                )
                    break;

                /* for 1st padop, note what type it is and the range
                 * start; for the others, check that it's the same type
                 * and that the targs are contiguous */
                if (count == 0) {
                    intro = (p->op_private & OPpLVAL_INTRO);
                    base = p->op_targ;
                    gimme = (p->op_flags & OPf_WANT);
                }
                else {
                    if ((p->op_private & OPpLVAL_INTRO) != intro)
                        break;
                    /* Note that you'd normally  expect targs to be
                     * contiguous in my($a,$b,$c), but that's not the case
                     * when external modules start doing things, e.g.
                     i* Function::Parameters */
                    if (p->op_targ != base + count)
                        break;
                    assert(p->op_targ == base + count);
                    /* all the padops should be in the same context */
                    if (gimme != (p->op_flags & OPf_WANT))
                        break;
                }

                /* for AV, HV, only when we're not flattening */
                if (   p->op_type != OP_PADSV
                    && gimme != OPf_WANT_VOID
                    && !(p->op_flags & OPf_REF)
                )
                    break;

                if (count >= OPpPADRANGE_COUNTMASK)
                    break;

                /* there's a biggest base we can fit into a
                 * SAVEt_CLEARPADRANGE in pp_padrange */
                if (intro && base >
                        (UV_MAX >> (OPpPADRANGE_COUNTSHIFT+SAVE_TIGHT_SHIFT)))
                    break;

                /* Success! We've got another valid pad op to optimise away */
                count++;
                followop = p->op_next;
            }

            if (count < 1)
                break;

            /* pp_padrange in specifically compile-time void context
             * skips pushing a mark and lexicals; in all other contexts
             * (including unknown till runtime) it pushes a mark and the
             * lexicals. We must be very careful then, that the ops we
             * optimise away would have exactly the same effect as the
             * padrange.
             * In particular in void context, we can only optimise to
             * a padrange if see see the complete sequence
             *     pushmark, pad*v, ...., list, nextstate
             * which has the net effect of of leaving the stack empty
             * (for now we leave the nextstate in the execution chain, for
             * its other side-effects).
             */
            assert(followop);
            if (gimme == OPf_WANT_VOID) {
                if (OP_TYPE_IS_OR_WAS(followop, OP_LIST)
                        && gimme == (followop->op_flags & OPf_WANT)
                        && (   followop->op_next->op_type == OP_NEXTSTATE
                            || followop->op_next->op_type == OP_DBSTATE))
                {
                    followop = followop->op_next; /* skip OP_LIST */

                    /* consolidate two successive my(...);'s */

                    if (   oldoldop
                        && oldoldop->op_type == OP_PADRANGE
                        && (oldoldop->op_flags & OPf_WANT) == OPf_WANT_VOID
                        && (oldoldop->op_private & OPpLVAL_INTRO) == intro
                        && !(oldoldop->op_flags & OPf_SPECIAL)
                    ) {
                        U8 old_count;
                        assert(oldoldop->op_next == oldop);
                        assert(   oldop->op_type == OP_NEXTSTATE
                               || oldop->op_type == OP_DBSTATE);
                        assert(oldop->op_next == o);

                        old_count
                            = (oldoldop->op_private & OPpPADRANGE_COUNTMASK);

                       /* Do not assume pad offsets for $c and $d are con-
                          tiguous in
                            my ($a,$b,$c);
                            my ($d,$e,$f);
                        */
                        if (  oldoldop->op_targ + old_count == base
                           && old_count < OPpPADRANGE_COUNTMASK - count) {
                            base = oldoldop->op_targ;
                            count += old_count;
                            reuse = 1;
                        }
                    }

                    /* if there's any immediately following singleton
                     * my var's; then swallow them and the associated
                     * nextstates; i.e.
                     *    my ($a,$b); my $c; my $d;
                     * is treated as
                     *    my ($a,$b,$c,$d);
                     */

                    while (    ((p = followop->op_next))
                            && (  p->op_type == OP_PADSV
                               || p->op_type == OP_PADAV
                               || p->op_type == OP_PADHV)
                            && (p->op_flags & OPf_WANT) == OPf_WANT_VOID
                            && (p->op_private & OPpLVAL_INTRO) == intro
                            && !(p->op_private & ~OPpLVAL_INTRO)
                            && p->op_next
                            && (   p->op_next->op_type == OP_NEXTSTATE
                                || p->op_next->op_type == OP_DBSTATE)
                            && count < OPpPADRANGE_COUNTMASK
                            && base + count == p->op_targ
                    ) {
                        count++;
                        followop = p->op_next;
                    }
                }
                else
                    break;
            }

            if (reuse) {
                assert(oldoldop->op_type == OP_PADRANGE);
                oldoldop->op_next = followop;
                oldoldop->op_private = (intro | count);
                o = oldoldop;
                oldop = NULL;
                oldoldop = NULL;
            }
            else {
                /* Convert the pushmark into a padrange.
                 * To make Deparse easier, we guarantee that a padrange was
                 * *always* formerly a pushmark */
                assert(o->op_type == OP_PUSHMARK);
                o->op_next = followop;
                o->op_type = OP_PADRANGE;
                o->op_ppaddr = PL_ppaddr[OP_PADRANGE];
                o->op_targ = base;
                /* bit 7: INTRO; bit 6..0: count */
                o->op_private = (intro | count);
                o->op_flags = ((o->op_flags & ~(OPf_WANT|OPf_SPECIAL))
                                    | gimme | (defav ? OPf_SPECIAL : 0));
            }
            break;
        }

	case OP_PADAV:
	case OP_GV:
	    if (o->op_type == OP_PADAV || o->op_next->op_type == OP_RV2AV) {
		OP* const pop = (o->op_type == OP_PADAV) ?
			    o->op_next : o->op_next->op_next;
		IV i;
		if (pop && pop->op_type == OP_CONST &&
		    ((PL_op = pop->op_next)) &&
		    pop->op_next->op_type == OP_AELEM &&
		    !(pop->op_next->op_private &
		      (OPpLVAL_INTRO|OPpLVAL_DEFER|OPpDEREF|OPpMAYBE_LVSUB)) &&
		    (i = SvIV(((SVOP*)pop)->op_sv)) >= -128 && i <= 127)
		{
		    GV *gv;
		    if (cSVOPx(pop)->op_private & OPpCONST_STRICT)
			no_bareword_allowed(pop);
		    if (o->op_type == OP_GV)
			op_null(o->op_next);
		    op_null(pop->op_next);
		    op_null(pop);
		    o->op_flags |= pop->op_next->op_flags & OPf_MOD;
		    o->op_next = pop->op_next->op_next;
		    o->op_ppaddr = PL_ppaddr[OP_AELEMFAST];
		    o->op_private = (U8)i;
		    if (o->op_type == OP_GV) {
			gv = cGVOPo_gv;
			GvAVn(gv);
			o->op_type = OP_AELEMFAST;
		    }
		    else
			o->op_type = OP_AELEMFAST_LEX;
		}
		break;
	    }

	    if (o->op_next->op_type == OP_RV2SV) {
		if (!(o->op_next->op_private & OPpDEREF)) {
		    op_null(o->op_next);
		    o->op_private |= o->op_next->op_private & (OPpLVAL_INTRO
							       | OPpOUR_INTRO);
		    o->op_next = o->op_next->op_next;
		    o->op_type = OP_GVSV;
		    o->op_ppaddr = PL_ppaddr[OP_GVSV];
		}
	    }
	    else if (o->op_next->op_type == OP_READLINE
		    && o->op_next->op_next->op_type == OP_CONCAT
		    && (o->op_next->op_next->op_flags & OPf_STACKED))
	    {
		/* Turn "$a .= <FH>" into an OP_RCATLINE. AMS 20010917 */
		o->op_type   = OP_RCATLINE;
		o->op_flags |= OPf_STACKED;
		o->op_ppaddr = PL_ppaddr[OP_RCATLINE];
		op_null(o->op_next->op_next);
		op_null(o->op_next);
	    }

	    break;
        
        {
            OP *fop;
            OP *sop;
            
#define HV_OR_SCALARHV(op)                                   \
    (  (op)->op_type == OP_PADHV || (op)->op_type == OP_RV2HV \
       ? (op)                                                  \
       : (op)->op_type == OP_SCALAR && (op)->op_flags & OPf_KIDS \
       && (  cUNOPx(op)->op_first->op_type == OP_PADHV          \
          || cUNOPx(op)->op_first->op_type == OP_RV2HV)          \
         ? cUNOPx(op)->op_first                                   \
         : NULL)

        case OP_NOT:
            if ((fop = HV_OR_SCALARHV(cUNOP->op_first)))
                fop->op_private |= OPpTRUEBOOL;
            break;

        case OP_AND:
	case OP_OR:
	case OP_DOR:
            fop = cLOGOP->op_first;
            sop = fop->op_sibling;
	    while (cLOGOP->op_other->op_type == OP_NULL)
		cLOGOP->op_other = cLOGOP->op_other->op_next;
	    while (o->op_next && (   o->op_type == o->op_next->op_type
				  || o->op_next->op_type == OP_NULL))
		o->op_next = o->op_next->op_next;

	    /* if we're an OR and our next is a AND in void context, we'll
	       follow it's op_other on short circuit, same for reverse.
	       We can't do this with OP_DOR since if it's true, its return
	       value is the underlying value which must be evaluated
	       by the next op */
	    if (o->op_next &&
	        (
		    (IS_AND_OP(o) && IS_OR_OP(o->op_next))
	         || (IS_OR_OP(o) && IS_AND_OP(o->op_next))
	        )
	        && (o->op_next->op_flags & OPf_WANT) == OPf_WANT_VOID
	    ) {
	        o->op_next = ((LOGOP*)o->op_next)->op_other;
	    }
	    DEFER(cLOGOP->op_other);
          
	    o->op_opt = 1;
            fop = HV_OR_SCALARHV(fop);
            if (sop) sop = HV_OR_SCALARHV(sop);
            if (fop || sop
            ){	
                OP * nop = o;
                OP * lop = o;
                if (!((nop->op_flags & OPf_WANT) == OPf_WANT_VOID)) {
                    while (nop && nop->op_next) {
                        switch (nop->op_next->op_type) {
                            case OP_NOT:
                            case OP_AND:
                            case OP_OR:
                            case OP_DOR:
                                lop = nop = nop->op_next;
                                break;
                            case OP_NULL:
                                nop = nop->op_next;
                                break;
                            default:
                                nop = NULL;
                                break;
                        }
                    }            
                }
                if (fop) {
                    if (  (lop->op_flags & OPf_WANT) == OPf_WANT_VOID
                      || o->op_type == OP_AND  )
                        fop->op_private |= OPpTRUEBOOL;
                    else if (!(lop->op_flags & OPf_WANT))
                        fop->op_private |= OPpMAYBE_TRUEBOOL;
                }
                if (  (lop->op_flags & OPf_WANT) == OPf_WANT_VOID
                   && sop)
                    sop->op_private |= OPpTRUEBOOL;
            }                  
            
	    
	    break;
	
	case OP_COND_EXPR:
	    if ((fop = HV_OR_SCALARHV(cLOGOP->op_first)))
		fop->op_private |= OPpTRUEBOOL;
#undef HV_OR_SCALARHV
	    /* GERONIMO! */
	}    

	case OP_MAPWHILE:
	case OP_GREPWHILE:
	case OP_ANDASSIGN:
	case OP_ORASSIGN:
	case OP_DORASSIGN:
	case OP_RANGE:
	case OP_ONCE:
	    while (cLOGOP->op_other->op_type == OP_NULL)
		cLOGOP->op_other = cLOGOP->op_other->op_next;
	    DEFER(cLOGOP->op_other);
	    break;

	case OP_ENTERLOOP:
	case OP_ENTERITER:
	    while (cLOOP->op_redoop->op_type == OP_NULL)
		cLOOP->op_redoop = cLOOP->op_redoop->op_next;
	    while (cLOOP->op_nextop->op_type == OP_NULL)
		cLOOP->op_nextop = cLOOP->op_nextop->op_next;
	    while (cLOOP->op_lastop->op_type == OP_NULL)
		cLOOP->op_lastop = cLOOP->op_lastop->op_next;
	    /* a while(1) loop doesn't have an op_next that escapes the
	     * loop, so we have to explicitly follow the op_lastop to
	     * process the rest of the code */
	    DEFER(cLOOP->op_lastop);
	    break;

        case OP_ENTERTRY:
	    assert(cLOGOPo->op_other->op_type == OP_LEAVETRY);
	    DEFER(cLOGOPo->op_other);
	    break;

	case OP_SUBST:
	    assert(!(cPMOP->op_pmflags & PMf_ONCE));
	    while (cPMOP->op_pmstashstartu.op_pmreplstart &&
		   cPMOP->op_pmstashstartu.op_pmreplstart->op_type == OP_NULL)
		cPMOP->op_pmstashstartu.op_pmreplstart
		    = cPMOP->op_pmstashstartu.op_pmreplstart->op_next;
	    DEFER(cPMOP->op_pmstashstartu.op_pmreplstart);
	    break;

	case OP_SORT: {
	    OP *oright;

	    if (o->op_flags & OPf_SPECIAL) {
                /* first arg is a code block */
		OP * const nullop = cLISTOP->op_first->op_sibling;
                OP * kid          = cUNOPx(nullop)->op_first;

                assert(nullop->op_type == OP_NULL);
		assert(kid->op_type == OP_SCOPE
		 || (kid->op_type == OP_NULL && kid->op_targ == OP_LEAVE));
                /* since OP_SORT doesn't have a handy op_other-style
                 * field that can point directly to the start of the code
                 * block, store it in the otherwise-unused op_next field
                 * of the top-level OP_NULL. This will be quicker at
                 * run-time, and it will also allow us to remove leading
                 * OP_NULLs by just messing with op_nexts without
                 * altering the basic op_first/op_sibling layout. */
                kid = kLISTOP->op_first;
                assert(
                      (kid->op_type == OP_NULL
                      && (  kid->op_targ == OP_NEXTSTATE
                         || kid->op_targ == OP_DBSTATE  ))
                    || kid->op_type == OP_STUB
                    || kid->op_type == OP_ENTER);
                nullop->op_next = kLISTOP->op_next;
                DEFER(nullop->op_next);
	    }

	    /* check that RHS of sort is a single plain array */
	    oright = cUNOPo->op_first;
	    if (!oright || oright->op_type != OP_PUSHMARK)
		break;

	    if (o->op_private & OPpSORT_INPLACE)
		break;

	    /* reverse sort ... can be optimised.  */
	    if (!cUNOPo->op_sibling) {
		/* Nothing follows us on the list. */
		OP * const reverse = o->op_next;

		if (reverse->op_type == OP_REVERSE &&
		    (reverse->op_flags & OPf_WANT) == OPf_WANT_LIST) {
		    OP * const pushmark = cUNOPx(reverse)->op_first;
		    if (pushmark && (pushmark->op_type == OP_PUSHMARK)
			&& (cUNOPx(pushmark)->op_sibling == o)) {
			/* reverse -> pushmark -> sort */
			o->op_private |= OPpSORT_REVERSE;
			op_null(reverse);
			pushmark->op_next = oright->op_next;
			op_null(oright);
		    }
		}
	    }

	    break;
	}

	case OP_REVERSE: {
	    OP *ourmark, *theirmark, *ourlast, *iter, *expushmark, *rv2av;
	    OP *gvop = NULL;
	    LISTOP *enter, *exlist;

	    if (o->op_private & OPpSORT_INPLACE)
		break;

	    enter = (LISTOP *) o->op_next;
	    if (!enter)
		break;
	    if (enter->op_type == OP_NULL) {
		enter = (LISTOP *) enter->op_next;
		if (!enter)
		    break;
	    }
	    /* for $a (...) will have OP_GV then OP_RV2GV here.
	       for (...) just has an OP_GV.  */
	    if (enter->op_type == OP_GV) {
		gvop = (OP *) enter;
		enter = (LISTOP *) enter->op_next;
		if (!enter)
		    break;
		if (enter->op_type == OP_RV2GV) {
		  enter = (LISTOP *) enter->op_next;
		  if (!enter)
		    break;
		}
	    }

	    if (enter->op_type != OP_ENTERITER)
		break;

	    iter = enter->op_next;
	    if (!iter || iter->op_type != OP_ITER)
		break;
	    
	    expushmark = enter->op_first;
	    if (!expushmark || expushmark->op_type != OP_NULL
		|| expushmark->op_targ != OP_PUSHMARK)
		break;

	    exlist = (LISTOP *) expushmark->op_sibling;
	    if (!exlist || exlist->op_type != OP_NULL
		|| exlist->op_targ != OP_LIST)
		break;

	    if (exlist->op_last != o) {
		/* Mmm. Was expecting to point back to this op.  */
		break;
	    }
	    theirmark = exlist->op_first;
	    if (!theirmark || theirmark->op_type != OP_PUSHMARK)
		break;

	    if (theirmark->op_sibling != o) {
		/* There's something between the mark and the reverse, eg
		   for (1, reverse (...))
		   so no go.  */
		break;
	    }

	    ourmark = ((LISTOP *)o)->op_first;
	    if (!ourmark || ourmark->op_type != OP_PUSHMARK)
		break;

	    ourlast = ((LISTOP *)o)->op_last;
	    if (!ourlast || ourlast->op_next != o)
		break;

	    rv2av = ourmark->op_sibling;
	    if (rv2av && rv2av->op_type == OP_RV2AV && rv2av->op_sibling == 0
		&& rv2av->op_flags == (OPf_WANT_LIST | OPf_KIDS)
		&& enter->op_flags == (OPf_WANT_LIST | OPf_KIDS)) {
		/* We're just reversing a single array.  */
		rv2av->op_flags = OPf_WANT_SCALAR | OPf_KIDS | OPf_REF;
		enter->op_flags |= OPf_STACKED;
	    }

	    /* We don't have control over who points to theirmark, so sacrifice
	       ours.  */
	    theirmark->op_next = ourmark->op_next;
	    theirmark->op_flags = ourmark->op_flags;
	    ourlast->op_next = gvop ? gvop : (OP *) enter;
	    op_null(ourmark);
	    op_null(o);
	    enter->op_private |= OPpITER_REVERSED;
	    iter->op_private |= OPpITER_REVERSED;
	    
	    break;
	}

	case OP_QR:
	case OP_MATCH:
	    if (!(cPMOP->op_pmflags & PMf_ONCE)) {
		assert (!cPMOP->op_pmstashstartu.op_pmreplstart);
	    }
	    break;

	case OP_RUNCV:
	    if (!(o->op_private & OPpOFFBYONE) && !CvCLONE(PL_compcv)) {
		SV *sv;
		if (CvEVAL(PL_compcv)) sv = &PL_sv_undef;
		else {
		    sv = newRV((SV *)PL_compcv);
		    sv_rvweaken(sv);
		    SvREADONLY_on(sv);
		}
		o->op_type = OP_CONST;
		o->op_ppaddr = PL_ppaddr[OP_CONST];
		o->op_flags |= OPf_SPECIAL;
		cSVOPo->op_sv = sv;
	    }
	    break;

	case OP_SASSIGN:
	    if (OP_GIMME(o,0) == G_VOID) {
		OP *right = cBINOP->op_first;
		if (right) {
                    /*   sassign
                    *      RIGHT
                    *      substr
                    *         pushmark
                    *         arg1
                    *         arg2
                    *         ...
                    * becomes
                    *
                    *  ex-sassign
                    *     substr
                    *        pushmark
                    *        RIGHT
                    *        arg1
                    *        arg2
                    *        ...
                    */
		    OP *left = right->op_sibling;
		    if (left->op_type == OP_SUBSTR
			 && (left->op_private & 7) < 4) {
			op_null(o);
			cBINOP->op_first = left;
			right->op_sibling =
			    cBINOPx(left)->op_first->op_sibling;
			cBINOPx(left)->op_first->op_sibling = right;
			left->op_private |= OPpSUBSTR_REPL_FIRST;
			left->op_flags =
			    (o->op_flags & ~OPf_WANT) | OPf_WANT_VOID;
		    }
		}
	    }
	    break;

	case OP_CUSTOM: {
	    Perl_cpeep_t cpeep = 
		XopENTRYCUSTOM(o, xop_peep);
	    if (cpeep)
		cpeep(aTHX_ o, oldop);
	    break;
	}
	    
	}
        /* did we just null the current op? If so, re-process it to handle
         * eliding "empty" ops from the chain */
        if (o->op_type == OP_NULL && oldop && oldop->op_next == o) {
            o->op_opt = 0;
            o = oldop;
        }
        else {
            oldoldop = oldop;
            oldop = o;
        }
    }
    LEAVE;
}

void
Perl_peep(pTHX_ OP *o)
{
    CALL_RPEEP(o);
}

/*
=head1 Custom Operators

=for apidoc Ao||custom_op_xop
Return the XOP structure for a given custom op.  This macro should be
considered internal to OP_NAME and the other access macros: use them instead.
This macro does call a function.  Prior
to 5.19.6, this was implemented as a
function.

=cut
*/

XOPRETANY
Perl_custom_op_get_field(pTHX_ const OP *o, const xop_flags_enum field)
{
    SV *keysv;
    HE *he = NULL;
    XOP *xop;

    static const XOP xop_null = { 0, 0, 0, 0, 0 };

    PERL_ARGS_ASSERT_CUSTOM_OP_GET_FIELD;
    assert(o->op_type == OP_CUSTOM);

    /* This is wrong. It assumes a function pointer can be cast to IV,
     * which isn't guaranteed, but this is what the old custom OP code
     * did. In principle it should be safer to Copy the bytes of the
     * pointer into a PV: since the new interface is hidden behind
     * functions, this can be changed later if necessary.  */
    /* Change custom_op_xop if this ever happens */
    keysv = sv_2mortal(newSViv(PTR2IV(o->op_ppaddr)));

    if (PL_custom_ops)
	he = hv_fetch_ent(PL_custom_ops, keysv, 0, 0);

    /* assume noone will have just registered a desc */
    if (!he && PL_custom_op_names &&
	(he = hv_fetch_ent(PL_custom_op_names, keysv, 0, 0))
    ) {
	const char *pv;
	STRLEN l;

	/* XXX does all this need to be shared mem? */
	Newxz(xop, 1, XOP);
	pv = SvPV(HeVAL(he), l);
	XopENTRY_set(xop, xop_name, savepvn(pv, l));
	if (PL_custom_op_descs &&
	    (he = hv_fetch_ent(PL_custom_op_descs, keysv, 0, 0))
	) {
	    pv = SvPV(HeVAL(he), l);
	    XopENTRY_set(xop, xop_desc, savepvn(pv, l));
	}
	Perl_custom_op_register(aTHX_ o->op_ppaddr, xop);
    }
    else {
	if (!he)
	    xop = (XOP *)&xop_null;
	else
	    xop = INT2PTR(XOP *, SvIV(HeVAL(he)));
    }
    {
	XOPRETANY any;
	if(field == XOPe_xop_ptr) {
	    any.xop_ptr = xop;
	} else {
	    const U32 flags = XopFLAGS(xop);
	    if(flags & field) {
		switch(field) {
		case XOPe_xop_name:
		    any.xop_name = xop->xop_name;
		    break;
		case XOPe_xop_desc:
		    any.xop_desc = xop->xop_desc;
		    break;
		case XOPe_xop_class:
		    any.xop_class = xop->xop_class;
		    break;
		case XOPe_xop_peep:
		    any.xop_peep = xop->xop_peep;
		    break;
		default:
		    NOT_REACHED;
		    break;
		}
	    } else {
		switch(field) {
		case XOPe_xop_name:
		    any.xop_name = XOPd_xop_name;
		    break;
		case XOPe_xop_desc:
		    any.xop_desc = XOPd_xop_desc;
		    break;
		case XOPe_xop_class:
		    any.xop_class = XOPd_xop_class;
		    break;
		case XOPe_xop_peep:
		    any.xop_peep = XOPd_xop_peep;
		    break;
		default:
		    NOT_REACHED;
		    break;
		}
	    }
	}
	return any;
    }
}

/*
=for apidoc Ao||custom_op_register
Register a custom op.  See L<perlguts/"Custom Operators">.

=cut
*/

void
Perl_custom_op_register(pTHX_ Perl_ppaddr_t ppaddr, const XOP *xop)
{
    SV *keysv;

    PERL_ARGS_ASSERT_CUSTOM_OP_REGISTER;

    /* see the comment in custom_op_xop */
    keysv = sv_2mortal(newSViv(PTR2IV(ppaddr)));

    if (!PL_custom_ops)
	PL_custom_ops = newHV();

    if (!hv_store_ent(PL_custom_ops, keysv, newSViv(PTR2IV(xop)), 0))
	Perl_croak(aTHX_ "panic: can't register custom OP %s", xop->xop_name);
}

/*
=head1 Functions in file op.c

=for apidoc core_prototype
This function assigns the prototype of the named core function to C<sv>, or
to a new mortal SV if C<sv> is NULL.  It returns the modified C<sv>, or
NULL if the core function has no prototype.  C<code> is a code as returned
by C<keyword()>.  It must not be equal to 0.

=cut
*/

SV *
Perl_core_prototype(pTHX_ SV *sv, const char *name, const int code,
                          int * const opnum)
{
    int i = 0, n = 0, seen_question = 0, defgv = 0;
    I32 oa;
#define MAX_ARGS_OP ((sizeof(I32) - 1) * 2)
    char str[ MAX_ARGS_OP * 2 + 2 ]; /* One ';', one '\0' */
    bool nullret = FALSE;

    PERL_ARGS_ASSERT_CORE_PROTOTYPE;

    assert (code);

    if (!sv) sv = sv_newmortal();

#define retsetpvs(x,y) sv_setpvs(sv, x); if(opnum) *opnum=(y); return sv

    switch (code < 0 ? -code : code) {
    case KEY_and   : case KEY_chop: case KEY_chomp:
    case KEY_cmp   : case KEY_defined: case KEY_delete: case KEY_exec  :
    case KEY_exists: case KEY_eq     : case KEY_ge    : case KEY_goto  :
    case KEY_grep  : case KEY_gt     : case KEY_last  : case KEY_le    :
    case KEY_lt    : case KEY_map    : case KEY_ne    : case KEY_next  :
    case KEY_or    : case KEY_print  : case KEY_printf: case KEY_qr    :
    case KEY_redo  : case KEY_require: case KEY_return: case KEY_say   :
    case KEY_select: case KEY_sort   : case KEY_split : case KEY_system:
    case KEY_x     : case KEY_xor    :
	if (!opnum) return NULL; nullret = TRUE; goto findopnum;
    case KEY_glob:    retsetpvs("_;", OP_GLOB);
    case KEY_keys:    retsetpvs("+", OP_KEYS);
    case KEY_values:  retsetpvs("+", OP_VALUES);
    case KEY_each:    retsetpvs("+", OP_EACH);
    case KEY_push:    retsetpvs("+@", OP_PUSH);
    case KEY_unshift: retsetpvs("+@", OP_UNSHIFT);
    case KEY_pop:     retsetpvs(";+", OP_POP);
    case KEY_shift:   retsetpvs(";+", OP_SHIFT);
    case KEY_pos:     retsetpvs(";\\[$*]", OP_POS);
    case KEY_splice:
	retsetpvs("+;$$@", OP_SPLICE);
    case KEY___FILE__: case KEY___LINE__: case KEY___PACKAGE__:
	retsetpvs("", 0);
    case KEY_evalbytes:
	name = "entereval"; break;
    case KEY_readpipe:
	name = "backtick";
    }

#undef retsetpvs

  findopnum:
    while (i < MAXO) {	/* The slow way. */
	if (strEQ(name, PL_op_name[i])
	    || strEQ(name, PL_op_desc[i]))
	{
	    if (nullret) { assert(opnum); *opnum = i; return NULL; }
	    goto found;
	}
	i++;
    }
    return NULL;
  found:
    defgv = PL_opargs[i] & OA_DEFGV;
    oa = PL_opargs[i] >> OASHIFT;
    while (oa) {
	if (oa & OA_OPTIONAL && !seen_question && (
	      !defgv || (oa & (OA_OPTIONAL - 1)) == OA_FILEREF
	)) {
	    seen_question = 1;
	    str[n++] = ';';
	}
	if ((oa & (OA_OPTIONAL - 1)) >= OA_AVREF
	    && (oa & (OA_OPTIONAL - 1)) <= OA_SCALARREF
	    /* But globs are already references (kinda) */
	    && (oa & (OA_OPTIONAL - 1)) != OA_FILEREF
	) {
	    str[n++] = '\\';
	}
	if ((oa & (OA_OPTIONAL - 1)) == OA_SCALARREF
	 && !scalar_mod_type(NULL, i)) {
	    str[n++] = '[';
	    str[n++] = '$';
	    str[n++] = '@';
	    str[n++] = '%';
	    if (i == OP_LOCK || i == OP_UNDEF) str[n++] = '&';
	    str[n++] = '*';
	    str[n++] = ']';
	}
	else str[n++] = ("?$@@%&*$")[oa & (OA_OPTIONAL - 1)];
	if (oa & OA_OPTIONAL && defgv && str[n-1] == '$') {
	    str[n-1] = '_'; defgv = 0;
	}
	oa = oa >> 4;
    }
    if (code == -KEY_not || code == -KEY_getprotobynumber) str[n++] = ';';
    str[n++] = '\0';
    sv_setpvn(sv, str, n - 1);
    if (opnum) *opnum = i;
    return sv;
}

OP *
Perl_coresub_op(pTHX_ SV * const coreargssv, const int code,
                      const int opnum)
{
    OP * const argop = newSVOP(OP_COREARGS,0,coreargssv);
    OP *o;

    PERL_ARGS_ASSERT_CORESUB_OP;

    switch(opnum) {
    case 0:
	return op_append_elem(OP_LINESEQ,
	               argop,
	               newSLICEOP(0,
	                          newSVOP(OP_CONST, 0, newSViv(-code % 3)),
	                          newOP(OP_CALLER,0)
	               )
	       );
    case OP_SELECT: /* which represents OP_SSELECT as well */
	if (code)
	    return newCONDOP(
	                 0,
	                 newBINOP(OP_GT, 0,
	                          newAVREF(newGVOP(OP_GV, 0, PL_defgv)),
	                          newSVOP(OP_CONST, 0, newSVuv(1))
	                         ),
	                 coresub_op(newSVuv((UV)OP_SSELECT), 0,
	                            OP_SSELECT),
	                 coresub_op(coreargssv, 0, OP_SELECT)
	           );
	/* FALL THROUGH */
    default:
	switch (PL_opargs[opnum] & OA_CLASS_MASK) {
	case OA_BASEOP:
	    return op_append_elem(
	                OP_LINESEQ, argop,
	                newOP(opnum,
	                      opnum == OP_WANTARRAY || opnum == OP_RUNCV
	                        ? OPpOFFBYONE << 8 : 0)
	           );
	case OA_BASEOP_OR_UNOP:
	    if (opnum == OP_ENTEREVAL) {
		o = newUNOP(OP_ENTEREVAL,OPpEVAL_COPHH<<8,argop);
		if (code == -KEY_evalbytes) o->op_private |= OPpEVAL_BYTES;
	    }
	    else o = newUNOP(opnum,0,argop);
	    if (opnum == OP_CALLER) o->op_private |= OPpOFFBYONE;
	    else {
	  onearg:
	      if (is_handle_constructor(o, 1))
		argop->op_private |= OPpCOREARGS_DEREF1;
	      if (scalar_mod_type(NULL, opnum))
		argop->op_private |= OPpCOREARGS_SCALARMOD;
	    }
	    return o;
	default:
	    o = convert(opnum,OPf_SPECIAL*(opnum == OP_GLOB),argop);
	    if (is_handle_constructor(o, 2))
		argop->op_private |= OPpCOREARGS_DEREF2;
	    if (opnum == OP_SUBSTR) {
		o->op_private |= OPpMAYBE_LVSUB;
		return o;
	    }
	    else goto onearg;
	}
    }
}

void
Perl_report_redefined_cv(pTHX_ const SV *name, const CV *old_cv,
			       SV * const *new_const_svp)
{
    const char *hvname;
    bool is_const = !!CvCONST(old_cv);
    SV *old_const_sv = is_const ? cv_const_sv(old_cv) : NULL;

    PERL_ARGS_ASSERT_REPORT_REDEFINED_CV;

    if (is_const && new_const_svp && old_const_sv == *new_const_svp)
	return;
	/* They are 2 constant subroutines generated from
	   the same constant. This probably means that
	   they are really the "same" proxy subroutine
	   instantiated in 2 places. Most likely this is
	   when a constant is exported twice.  Don't warn.
	*/
    if (
	(ckWARN(WARN_REDEFINE)
	 && !(
		CvGV(old_cv) && GvSTASH(CvGV(old_cv))
	     && HvNAMELEN(GvSTASH(CvGV(old_cv))) == 7
	     && (hvname = HvNAME(GvSTASH(CvGV(old_cv))),
		 strEQ(hvname, "autouse"))
	     )
	)
     || (is_const
	 && ckWARN_d(WARN_REDEFINE)
	 && (!new_const_svp || sv_cmp(old_const_sv, *new_const_svp))
	)
    )
	Perl_warner(aTHX_ packWARN(WARN_REDEFINE),
			  is_const
			    ? "Constant subroutine %"SVf" redefined"
			    : "Subroutine %"SVf" redefined",
			  name);
}

/*
=head1 Hook manipulation

These functions provide convenient and thread-safe means of manipulating
hook variables.

=cut
*/

/*
=for apidoc Am|void|wrap_op_checker|Optype opcode|Perl_check_t new_checker|Perl_check_t *old_checker_p

Puts a C function into the chain of check functions for a specified op
type.  This is the preferred way to manipulate the L</PL_check> array.
I<opcode> specifies which type of op is to be affected.  I<new_checker>
is a pointer to the C function that is to be added to that opcode's
check chain, and I<old_checker_p> points to the storage location where a
pointer to the next function in the chain will be stored.  The value of
I<new_pointer> is written into the L</PL_check> array, while the value
previously stored there is written to I<*old_checker_p>.

The function should be defined like this:

    static OP *new_checker(pTHX_ OP *op) { ... }

It is intended to be called in this manner:

    new_checker(aTHX_ op)

I<old_checker_p> should be defined like this:

    static Perl_check_t old_checker_p;

L</PL_check> is global to an entire process, and a module wishing to
hook op checking may find itself invoked more than once per process,
typically in different threads.  To handle that situation, this function
is idempotent.  The location I<*old_checker_p> must initially (once
per process) contain a null pointer.  A C variable of static duration
(declared at file scope, typically also marked C<static> to give
it internal linkage) will be implicitly initialised appropriately,
if it does not have an explicit initialiser.  This function will only
actually modify the check chain if it finds I<*old_checker_p> to be null.
This function is also thread safe on the small scale.  It uses appropriate
locking to avoid race conditions in accessing L</PL_check>.

When this function is called, the function referenced by I<new_checker>
must be ready to be called, except for I<*old_checker_p> being unfilled.
In a threading situation, I<new_checker> may be called immediately,
even before this function has returned.  I<*old_checker_p> will always
be appropriately set before I<new_checker> is called.  If I<new_checker>
decides not to do anything special with an op that it is given (which
is the usual case for most uses of op check hooking), it must chain the
check function referenced by I<*old_checker_p>.

If you want to influence compilation of calls to a specific subroutine,
then use L</cv_set_call_checker> rather than hooking checking of all
C<entersub> ops.

=cut
*/

void
Perl_wrap_op_checker(pTHX_ Optype opcode,
    Perl_check_t new_checker, Perl_check_t *old_checker_p)
{
    dVAR;

    PERL_ARGS_ASSERT_WRAP_OP_CHECKER;
    if (*old_checker_p) return;
    OP_CHECK_MUTEX_LOCK;
    if (!*old_checker_p) {
	*old_checker_p = PL_check[opcode];
	PL_check[opcode] = new_checker;
    }
    OP_CHECK_MUTEX_UNLOCK;
}

#include "XSUB.h"

/* Efficient sub that returns a constant scalar value. */
static void
const_sv_xsub(pTHX_ CV* cv)
{
    dVAR;
    dXSARGS;
    SV *const sv = MUTABLE_SV(XSANY.any_ptr);
    PERL_UNUSED_ARG(items);
    if (!sv) {
	XSRETURN(0);
    }
    EXTEND(sp, 1);
    ST(0) = sv;
    XSRETURN(1);
}

static void
const_av_xsub(pTHX_ CV* cv)
{
    dVAR;
    dXSARGS;
    AV * const av = MUTABLE_AV(XSANY.any_ptr);
    SP -= items;
    assert(av);
#ifndef DEBUGGING
    if (!av) {
	XSRETURN(0);
    }
#endif
    if (SvRMAGICAL(av))
	Perl_croak(aTHX_ "Magical list constants are not supported");
    if (GIMME_V != G_ARRAY) {
	EXTEND(SP, 1);
	ST(0) = sv_2mortal(newSViv((IV)AvFILLp(av)+1));
	XSRETURN(1);
    }
    EXTEND(SP, AvFILLp(av)+1);
    Copy(AvARRAY(av), &ST(0), AvFILLp(av)+1, SV *);
    XSRETURN(AvFILLp(av)+1);
}

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
