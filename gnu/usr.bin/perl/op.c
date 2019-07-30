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

static const char array_passed_to_stat[] = "Array passed to stat will be coerced to a scalar";

/* Used to avoid recursion through the op tree in scalarvoid() and
   op_free()
*/

#define DEFERRED_OP_STEP 100
#define DEFER_OP(o) \
  STMT_START { \
    if (UNLIKELY(defer_ix == (defer_stack_alloc-1))) {    \
        defer_stack_alloc += DEFERRED_OP_STEP; \
        assert(defer_stack_alloc > 0); \
        Renew(defer_stack, defer_stack_alloc, OP *); \
    } \
    defer_stack[++defer_ix] = o; \
  } STMT_END

#define POP_DEFERRED_OP() (defer_ix >= 0 ? defer_stack[defer_ix--] : (OP *)NULL)

/* remove any leading "empty" ops from the op_next chain whose first
 * node's address is stored in op_p. Store the updated address of the
 * first node in op_p.
 */

STATIC void
S_prune_chain_head(OP** op_p)
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
#ifndef WIN32
    /* The context is unused in non-Windows */
    PERL_UNUSED_CONTEXT;
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
    {
	o = (OP*)PerlMemShared_calloc(1, sz);
        goto gotit;
    }

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
	DEBUG_S_warn((aTHX_ "found free op at %p, slab %p", (void*)o, (void*)slab));
	while (o && DIFF(OpSLOT(o), OpSLOT(o)->opslot_next) < sz) {
	    DEBUG_S_warn((aTHX_ "Alas! too small"));
	    o = *(too = &o->op_next);
	    if (o) { DEBUG_S_warn((aTHX_ "found another free op at %p", (void*)o)); }
	}
	if (o) {
	    *too = o->op_next;
	    Zero(o, opsz, I32 *);
	    o->op_slabbed = 1;
	    goto gotit;
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
    DEBUG_S_warn((aTHX_ "allocating op at %p, slab %p", (void*)o, (void*)slab));

  gotit:
#ifdef PERL_OP_PARENT
    /* moresib == 0, op_sibling == 0 implies a solitary unattached op */
    assert(!o->op_moresib);
    assert(!o->op_sibparent);
#endif

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
    DEBUG_S_warn((aTHX_ "free op at %p, recorded in slab %p", (void*)o, (void*)slab));
    OpslabREFCNT_dec_padok(slab);
}

void
Perl_opslab_free_nopad(pTHX_ OPSLAB *slab)
{
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
    OPSLAB *slab2;
    PERL_ARGS_ASSERT_OPSLAB_FREE;
    PERL_UNUSED_CONTEXT;
    DEBUG_S_warn((aTHX_ "freeing slab %p", (void*)slab));
    assert(slab->opslab_refcnt == 1);
    do {
	slab2 = slab->opslab_next;
#ifdef DEBUGGING
	slab->opslab_refcnt = ~(size_t)0;
#endif
#ifdef PERL_DEBUG_READONLY_OPS
	DEBUG_m(PerlIO_printf(Perl_debug_log, "Deallocate slab at %p\n",
					       (void*)slab));
	if (munmap(slab, slab->opslab_size * sizeof(I32 *))) {
	    perror("munmap failed");
	    abort();
	}
#else
	PerlMemShared_free(slab);
#endif
        slab = slab2;
    } while (slab);
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

#define OpTYPE_set(o,type) \
    STMT_START {				\
	o->op_type = (OPCODE)type;		\
	o->op_ppaddr = PL_ppaddr[type];		\
    } STMT_END

STATIC OP *
S_no_fh_allowed(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_NO_FH_ALLOWED;

    yyerror(Perl_form(aTHX_ "Missing comma after first argument to %s function",
		 OP_DESC(o)));
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

STATIC void
S_bad_type_pv(pTHX_ I32 n, const char *t, const OP *o, const OP *kid)
{
    PERL_ARGS_ASSERT_BAD_TYPE_PV;

    yyerror_pv(Perl_form(aTHX_ "Type of arg %d to %s must be %s (not %s)",
		 (int)n, PL_op_desc[(o)->op_type], t, OP_DESC(kid)), 0);
}

/* remove flags var, its unused in all callers, move to to right end since gv
  and kid are always the same */
STATIC void
S_bad_type_gv(pTHX_ I32 n, GV *gv, const OP *kid, const char *t)
{
    SV * const namesv = cv_name((CV *)gv, NULL, 0);
    PERL_ARGS_ASSERT_BAD_TYPE_GV;
 
    yyerror_pv(Perl_form(aTHX_ "Type of arg %d to %"SVf" must be %s (not %s)",
		 (int)n, SVfARG(namesv), t, OP_DESC(kid)), SvUTF8(namesv));
}

STATIC void
S_no_bareword_allowed(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_NO_BAREWORD_ALLOWED;

    qerror(Perl_mess(aTHX_
		     "Bareword \"%"SVf"\" not allowed while \"strict subs\" in use",
		     SVfARG(cSVOPo_sv)));
    o->op_private &= ~OPpCONST_STRICT; /* prevent warning twice about the same OP */
}

/* "register" allocation */

PADOFFSET
Perl_allocmy(pTHX_ const char *const name, const STRLEN len, const U32 flags)
{
    PADOFFSET off;
    const bool is_our = (PL_parser->in_my == KEY_our);

    PERL_ARGS_ASSERT_ALLOCMY;

    if (flags & ~SVf_UTF8)
	Perl_croak(aTHX_ "panic: allocmy illegal flag bits 0x%" UVxf,
		   (UV)flags);

    /* complain about "my $<special_var>" etc etc */
    if (len &&
	!(is_our ||
	  isALPHA(name[1]) ||
	  ((flags & SVf_UTF8) && isIDFIRST_utf8((U8 *)name+1)) ||
	  (name[1] == '_' && len > 2)))
    {
	if (!(flags & SVf_UTF8 && UTF8_IS_START(name[1]))
	 && isASCII(name[1])
	 && (!isPRINT(name[1]) || strchr("\t\n\r\f", name[1]))) {
	    yyerror(Perl_form(aTHX_ "Can't use global %c^%c%.*s in \"%s\"",
			      name[0], toCTRL(name[1]), (int)(len - 2), name + 2,
			      PL_parser->in_my == KEY_state ? "state" : "my"));
	} else {
	    yyerror_pv(Perl_form(aTHX_ "Can't use global %.*s in \"%s\"", (int) len, name,
			      PL_parser->in_my == KEY_state ? "state" : "my"), flags & SVf_UTF8);
	}
    }

    /* allocate a spare slot and store the name in that slot */

    off = pad_add_name_pvn(name, len,
		       (is_our ? padadd_OUR :
		        PL_parser->in_my == KEY_state ? padadd_STATE : 0),
		    PL_parser->in_my_stash,
		    (is_our
		        /* $_ is always in main::, even with our */
			? (PL_curstash && !memEQs(name,len,"$_")
			    ? PL_curstash
			    : PL_defstash)
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
=head1 Optree Manipulation Functions

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
    SSize_t defer_ix = -1;
    SSize_t defer_stack_alloc = 0;
    OP **defer_stack = NULL;

    do {

        /* Though ops may be freed twice, freeing the op after its slab is a
           big no-no. */
        assert(!o || !o->op_slabbed || OpSLAB(o)->opslab_refcnt != ~(size_t)0);
        /* During the forced freeing of ops after compilation failure, kidops
           may be freed before their parents. */
        if (!o || o->op_type == OP_FREED)
            continue;

        type = o->op_type;

        /* an op should only ever acquire op_private flags that we know about.
         * If this fails, you may need to fix something in regen/op_private.
         * Don't bother testing if:
         *   * the op_ppaddr doesn't match the op; someone may have
         *     overridden the op and be doing strange things with it;
         *   * we've errored, as op flags are often left in an
         *     inconsistent state then. Note that an error when
         *     compiling the main program leaves PL_parser NULL, so
         *     we can't spot faults in the main code, only
         *     evaled/required code */
#ifdef DEBUGGING
        if (   o->op_ppaddr == PL_ppaddr[o->op_type]
            && PL_parser
            && !PL_parser->error_count)
        {
            assert(!(o->op_private & ~PL_op_private_valid[type]));
        }
#endif

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
                    continue;
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
                nextkid = OpSIBLING(kid); /* Get before next freeing kid */
                if (!kid || kid->op_type == OP_FREED)
                    /* During the forced freeing of ops after
                       compilation failure, kidops may be freed before
                       their parents. */
                    continue;
                if (!(kid->op_flags & OPf_KIDS))
                    /* If it has no kids, just free it now */
                    op_free(kid);
                else
                    DEFER_OP(kid);
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
    } while ( (o = POP_DEFERRED_OP()) );

    Safefree(defer_stack);
}

/* S_op_clear_gv(): free a GV attached to an OP */

STATIC
#ifdef USE_ITHREADS
void S_op_clear_gv(pTHX_ OP *o, PADOFFSET *ixp)
#else
void S_op_clear_gv(pTHX_ OP *o, SV**svp)
#endif
{

    GV *gv = (o->op_type == OP_GV || o->op_type == OP_GVSV
            || o->op_type == OP_MULTIDEREF)
#ifdef USE_ITHREADS
                && PL_curpad
                ? ((GV*)PAD_SVl(*ixp)) : NULL;
#else
                ? (GV*)(*svp) : NULL;
#endif
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
    if (*ixp > 0) {
        pad_swipe(*ixp, TRUE);
        *ixp = 0;
    }
#else
    SvREFCNT_dec(*svp);
    *svp = NULL;
#endif
    if (still_valid) {
        int try_downgrade = SvREFCNT(gv) == 2;
        SvREFCNT_dec_NN(gv);
        if (try_downgrade)
            gv_try_downgrade(gv);
    }
}


void
Perl_op_clear(pTHX_ OP *o)
{

    dVAR;

    PERL_ARGS_ASSERT_OP_CLEAR;

    switch (o->op_type) {
    case OP_NULL:	/* Was holding old type, if any. */
        /* FALLTHROUGH */
    case OP_ENTERTRY:
    case OP_ENTEREVAL:	/* Was holding hints. */
	o->op_targ = 0;
	break;
    default:
	if (!(o->op_flags & OPf_REF)
	    || (PL_check[o->op_type] != Perl_ck_ftst))
	    break;
	/* FALLTHROUGH */
    case OP_GVSV:
    case OP_GV:
    case OP_AELEMFAST:
#ifdef USE_ITHREADS
            S_op_clear_gv(aTHX_ o, &(cPADOPx(o)->op_padix));
#else
            S_op_clear_gv(aTHX_ o, &(cSVOPx(o)->op_sv));
#endif
	break;
    case OP_METHOD_REDIR:
    case OP_METHOD_REDIR_SUPER:
#ifdef USE_ITHREADS
	if (cMETHOPx(o)->op_rclass_targ) {
	    pad_swipe(cMETHOPx(o)->op_rclass_targ, 1);
	    cMETHOPx(o)->op_rclass_targ = 0;
	}
#else
	SvREFCNT_dec(cMETHOPx(o)->op_rclass_sv);
	cMETHOPx(o)->op_rclass_sv = NULL;
#endif
    case OP_METHOD_NAMED:
    case OP_METHOD_SUPER:
        SvREFCNT_dec(cMETHOPx(o)->op_u.op_meth_sv);
        cMETHOPx(o)->op_u.op_meth_sv = NULL;
#ifdef USE_ITHREADS
        if (o->op_targ) {
            pad_swipe(o->op_targ, 1);
            o->op_targ = 0;
        }
#endif
        break;
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
	/* FALLTHROUGH */
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
	    pad_swipe(cPMOPo->op_pmreplrootu.op_pmtargetoff, TRUE);
	}
#else
	SvREFCNT_dec(MUTABLE_SV(cPMOPo->op_pmreplrootu.op_pmtargetgv));
#endif
	/* FALLTHROUGH */
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

    case OP_MULTIDEREF:
        {
            UNOP_AUX_item *items = cUNOP_AUXo->op_aux;
            UV actions = items->uv;
            bool last = 0;
            bool is_hash = FALSE;

            while (!last) {
                switch (actions & MDEREF_ACTION_MASK) {

                case MDEREF_reload:
                    actions = (++items)->uv;
                    continue;

                case MDEREF_HV_padhv_helem:
                    is_hash = TRUE;
                case MDEREF_AV_padav_aelem:
                    pad_free((++items)->pad_offset);
                    goto do_elem;

                case MDEREF_HV_gvhv_helem:
                    is_hash = TRUE;
                case MDEREF_AV_gvav_aelem:
#ifdef USE_ITHREADS
                    S_op_clear_gv(aTHX_ o, &((++items)->pad_offset));
#else
                    S_op_clear_gv(aTHX_ o, &((++items)->sv));
#endif
                    goto do_elem;

                case MDEREF_HV_gvsv_vivify_rv2hv_helem:
                    is_hash = TRUE;
                case MDEREF_AV_gvsv_vivify_rv2av_aelem:
#ifdef USE_ITHREADS
                    S_op_clear_gv(aTHX_ o, &((++items)->pad_offset));
#else
                    S_op_clear_gv(aTHX_ o, &((++items)->sv));
#endif
                    goto do_vivify_rv2xv_elem;

                case MDEREF_HV_padsv_vivify_rv2hv_helem:
                    is_hash = TRUE;
                case MDEREF_AV_padsv_vivify_rv2av_aelem:
                    pad_free((++items)->pad_offset);
                    goto do_vivify_rv2xv_elem;

                case MDEREF_HV_pop_rv2hv_helem:
                case MDEREF_HV_vivify_rv2hv_helem:
                    is_hash = TRUE;
                do_vivify_rv2xv_elem:
                case MDEREF_AV_pop_rv2av_aelem:
                case MDEREF_AV_vivify_rv2av_aelem:
                do_elem:
                    switch (actions & MDEREF_INDEX_MASK) {
                    case MDEREF_INDEX_none:
                        last = 1;
                        break;
                    case MDEREF_INDEX_const:
                        if (is_hash) {
#ifdef USE_ITHREADS
                            /* see RT #15654 */
                            pad_swipe((++items)->pad_offset, 1);
#else
                            SvREFCNT_dec((++items)->sv);
#endif
                        }
                        else
                            items++;
                        break;
                    case MDEREF_INDEX_padsv:
                        pad_free((++items)->pad_offset);
                        break;
                    case MDEREF_INDEX_gvsv:
#ifdef USE_ITHREADS
                        S_op_clear_gv(aTHX_ o, &((++items)->pad_offset));
#else
                        S_op_clear_gv(aTHX_ o, &((++items)->sv));
#endif
                        break;
                    }

                    if (actions & MDEREF_FLAG_last)
                        last = 1;
                    is_hash = FALSE;

                    break;

                default:
                    assert(0);
                    last = 1;
                    break;

                } /* switch */

                actions >>= MDEREF_SHIFT;
            } /* while */

            /* start of malloc is at op_aux[-1], where the length is
             * stored */
            PerlMemShared_free(cUNOP_AUXo->op_aux - 1);
        }
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
	    kid = OpSIBLING(kid);
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
    op_clear(o);
    o->op_targ = o->op_type;
    OpTYPE_set(o, OP_NULL);
}

void
Perl_op_refcnt_lock(pTHX)
  PERL_TSA_ACQUIRE(PL_op_mutex)
{
#ifdef USE_ITHREADS
    dVAR;
#endif
    PERL_UNUSED_CONTEXT;
    OP_REFCNT_LOCK;
}

void
Perl_op_refcnt_unlock(pTHX)
  PERL_TSA_RELEASE(PL_op_mutex)
{
#ifdef USE_ITHREADS
    dVAR;
#endif
    PERL_UNUSED_CONTEXT;
    OP_REFCNT_UNLOCK;
}


/*
=for apidoc op_sibling_splice

A general function for editing the structure of an existing chain of
op_sibling nodes.  By analogy with the perl-level C<splice()> function, allows
you to delete zero or more sequential nodes, replacing them with zero or
more different nodes.  Performs the necessary op_first/op_last
housekeeping on the parent node and op_sibling manipulation on the
children.  The last deleted node will be marked as as the last node by
updating the op_sibling/op_sibparent or op_moresib field as appropriate.

Note that op_next is not manipulated, and nodes are not freed; that is the
responsibility of the caller.  It also won't create a new list op for an
empty list etc; use higher-level functions like op_append_elem() for that.

C<parent> is the parent node of the sibling chain. It may passed as C<NULL> if
the splicing doesn't affect the first or last op in the chain.

C<start> is the node preceding the first node to be spliced.  Node(s)
following it will be deleted, and ops will be inserted after it.  If it is
C<NULL>, the first node onwards is deleted, and nodes are inserted at the
beginning.

C<del_count> is the number of nodes to delete.  If zero, no nodes are deleted.
If -1 or greater than or equal to the number of remaining kids, all
remaining kids are deleted.

C<insert> is the first of a chain of nodes to be inserted in place of the nodes.
If C<NULL>, no nodes are inserted.

The head of the chain of deleted ops is returned, or C<NULL> if no ops were
deleted.

For example:

    action                    before      after         returns
    ------                    -----       -----         -------

                              P           P
    splice(P, A, 2, X-Y-Z)    |           |             B-C
                              A-B-C-D     A-X-Y-Z-D

                              P           P
    splice(P, NULL, 1, X-Y)   |           |             A
                              A-B-C-D     X-Y-B-C-D

                              P           P
    splice(P, NULL, 3, NULL)  |           |             A-B-C
                              A-B-C-D     D

                              P           P
    splice(P, B, 0, X-Y)      |           |             NULL
                              A-B-C-D     A-B-X-Y-C-D


For lower-level direct manipulation of C<op_sibparent> and C<op_moresib>,
see C<L</OpMORESIB_set>>, C<L</OpLASTSIB_set>>, C<L</OpMAYBESIB_set>>.

=cut
*/

OP *
Perl_op_sibling_splice(OP *parent, OP *start, int del_count, OP* insert)
{
    OP *first;
    OP *rest;
    OP *last_del = NULL;
    OP *last_ins = NULL;

    if (start)
        first = OpSIBLING(start);
    else if (!parent)
        goto no_parent;
    else
        first = cLISTOPx(parent)->op_first;

    assert(del_count >= -1);

    if (del_count && first) {
        last_del = first;
        while (--del_count && OpHAS_SIBLING(last_del))
            last_del = OpSIBLING(last_del);
        rest = OpSIBLING(last_del);
        OpLASTSIB_set(last_del, NULL);
    }
    else
        rest = first;

    if (insert) {
        last_ins = insert;
        while (OpHAS_SIBLING(last_ins))
            last_ins = OpSIBLING(last_ins);
        OpMAYBESIB_set(last_ins, rest, NULL);
    }
    else
        insert = rest;

    if (start) {
        OpMAYBESIB_set(start, insert, NULL);
    }
    else {
        if (!parent)
            goto no_parent;
        cLISTOPx(parent)->op_first = insert;
        if (insert)
            parent->op_flags |= OPf_KIDS;
        else
            parent->op_flags &= ~OPf_KIDS;
    }

    if (!rest) {
        /* update op_last etc */
        U32 type;
        OP *lastop;

        if (!parent)
            goto no_parent;

        /* ought to use OP_CLASS(parent) here, but that can't handle
         * ex-foo OP_NULL ops. Also note that XopENTRYCUSTOM() can't
         * either */
        type = parent->op_type;
        if (type == OP_CUSTOM) {
            dTHX;
            type = XopENTRYCUSTOM(parent, xop_class);
        }
        else {
            if (type == OP_NULL)
                type = parent->op_targ;
            type = PL_opargs[type] & OA_CLASS_MASK;
        }

        lastop = last_ins ? last_ins : start ? start : NULL;
        if (   type == OA_BINOP
            || type == OA_LISTOP
            || type == OA_PMOP
            || type == OA_LOOP
        )
            cLISTOPx(parent)->op_last = lastop;

        if (lastop)
            OpLASTSIB_set(lastop, parent);
    }
    return last_del ? first : NULL;

  no_parent:
    Perl_croak_nocontext("panic: op_sibling_splice(): NULL parent");
}


#ifdef PERL_OP_PARENT

/*
=for apidoc op_parent

Returns the parent OP of C<o>, if it has a parent. Returns C<NULL> otherwise.
This function is only available on perls built with C<-DPERL_OP_PARENT>.

=cut
*/

OP *
Perl_op_parent(OP *o)
{
    PERL_ARGS_ASSERT_OP_PARENT;
    while (OpHAS_SIBLING(o))
        o = OpSIBLING(o);
    return o->op_sibparent;
}

#endif


/* replace the sibling following start with a new UNOP, which becomes
 * the parent of the original sibling; e.g.
 *
 *  op_sibling_newUNOP(P, A, unop-args...)
 *
 *  P              P
 *  |      becomes |
 *  A-B-C          A-U-C
 *                   |
 *                   B
 *
 * where U is the new UNOP.
 *
 * parent and start args are the same as for op_sibling_splice();
 * type and flags args are as newUNOP().
 *
 * Returns the new UNOP.
 */

STATIC OP *
S_op_sibling_newUNOP(pTHX_ OP *parent, OP *start, I32 type, I32 flags)
{
    OP *kid, *newop;

    kid = op_sibling_splice(parent, start, 1, NULL);
    newop = newUNOP(type, flags, kid);
    op_sibling_splice(parent, start, 0, newop);
    return newop;
}


/* lowest-level newLOGOP-style function - just allocates and populates
 * the struct. Higher-level stuff should be done by S_new_logop() /
 * newLOGOP(). This function exists mainly to avoid op_first assignment
 * being spread throughout this file.
 */

STATIC LOGOP *
S_alloc_LOGOP(pTHX_ I32 type, OP *first, OP* other)
{
    dVAR;
    LOGOP *logop;
    OP *kid = first;
    NewOp(1101, logop, 1, LOGOP);
    OpTYPE_set(logop, type);
    logop->op_first = first;
    logop->op_other = other;
    logop->op_flags = OPf_KIDS;
    while (kid && OpHAS_SIBLING(kid))
        kid = OpSIBLING(kid);
    if (kid)
        OpLASTSIB_set(kid, (OP*)logop);
    return logop;
}


/* Contextualizers */

/*
=for apidoc Am|OP *|op_contextualize|OP *o|I32 context

Applies a syntactic context to an op tree representing an expression.
C<o> is the op tree, and C<context> must be C<G_SCALAR>, C<G_ARRAY>,
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
    }
}

/*

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
            OP *sibl = OpSIBLING(kid);
            if (sibl) {
                kid->op_next = LINKLIST(sibl);
                kid = sibl;
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
        for (kid = cLISTOPo->op_first; kid; kid = OpSIBLING(kid))
	    scalar(kid);
    }
    return o;
}

STATIC OP *
S_scalarboolean(pTHX_ OP *o)
{
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
S_op_varname_subscript(pTHX_ const OP *o, int subscript_type)
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
	    return varname(gv, funny, 0, NULL, 0, subscript_type);
	}
	return
	    varname(MUTABLE_GV(PL_compcv), funny, o->op_targ, NULL, 0, subscript_type);
    }
}

static SV *
S_op_varname(pTHX_ const OP *o)
{
    return S_op_varname_subscript(aTHX_ o, 1);
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
    kid = OpSIBLING(kid); /* get past pushmark */
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
	return;
    }

    /* Don't warn if we have a nulled list either. */
    if (kid->op_type == OP_NULL && kid->op_targ == OP_LIST)
        return;

    assert(OpSIBLING(kid));
    name = S_op_varname(aTHX_ OpSIBLING(kid));
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
		    SVfARG(name), lbrack, SVfARG(keysv), rbrack,
		    SVfARG(name), lbrack, SVfARG(keysv), rbrack);
}

OP *
Perl_scalar(pTHX_ OP *o)
{
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
	if (o->op_private & OPpREPEAT_DOLIST) {
	    kid = cLISTOPx(cUNOPo->op_first)->op_first;
	    assert(kid->op_type == OP_PUSHMARK);
	    if (OpHAS_SIBLING(kid) && !OpHAS_SIBLING(OpSIBLING(kid))) {
		op_null(cLISTOPx(cUNOPo->op_first)->op_first);
		o->op_private &=~ OPpREPEAT_DOLIST;
	    }
	}
	break;
    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = OpSIBLING(cUNOPo->op_first); kid; kid = OpSIBLING(kid))
	    scalar(kid);
	break;
	/* FALLTHROUGH */
    case OP_SPLIT:
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
    case OP_NULL:
    default:
	if (o->op_flags & OPf_KIDS) {
	    for (kid = cUNOPo->op_first; kid; kid = OpSIBLING(kid))
		scalar(kid);
	}
	break;
    case OP_LEAVE:
    case OP_LEAVETRY:
	kid = cLISTOPo->op_first;
	scalar(kid);
	kid = OpSIBLING(kid);
    do_kids:
	while (kid) {
	    OP *sib = OpSIBLING(kid);
	    if (sib && kid->op_type != OP_LEAVEWHEN
	     && (  OpHAS_SIBLING(sib) || sib->op_type != OP_NULL
		|| (  sib->op_targ != OP_NEXTSTATE
		   && sib->op_targ != OP_DBSTATE  )))
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
	kid = OpSIBLING(kid); /* get past pushmark */
	assert(OpSIBLING(kid));
	name = S_op_varname(aTHX_ OpSIBLING(kid));
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
			SVfARG(name), lbrack, SVfARG(keysv), rbrack,
			SVfARG(name), lbrack, SVfARG(keysv), rbrack);
    }
    }
    return o;
}

OP *
Perl_scalarvoid(pTHX_ OP *arg)
{
    dVAR;
    OP *kid;
    SV* sv;
    U8 want;
    SSize_t defer_stack_alloc = 0;
    SSize_t defer_ix = -1;
    OP **defer_stack = NULL;
    OP *o = arg;

    PERL_ARGS_ASSERT_SCALARVOID;

    do {
        SV *useless_sv = NULL;
        const char* useless = NULL;

        if (o->op_type == OP_NEXTSTATE
            || o->op_type == OP_DBSTATE
            || (o->op_type == OP_NULL && (o->op_targ == OP_NEXTSTATE
                                          || o->op_targ == OP_DBSTATE)))
            PL_curcop = (COP*)o;                /* for warning below */

        /* assumes no premature commitment */
        want = o->op_flags & OPf_WANT;
        if ((want && want != OPf_WANT_SCALAR)
            || (PL_parser && PL_parser->error_count)
            || o->op_type == OP_RETURN || o->op_type == OP_REQUIRE || o->op_type == OP_LEAVEWHEN)
        {
            continue;
        }

        if ((o->op_private & OPpTARGET_MY)
            && (PL_opargs[o->op_type] & OA_TARGLEX))/* OPp share the meaning */
        {
            /* newASSIGNOP has already applied scalar context, which we
               leave, as if this op is inside SASSIGN.  */
            continue;
        }

        o->op_flags = (o->op_flags & ~OPf_WANT) | OPf_WANT_VOID;

        switch (o->op_type) {
        default:
            if (!(PL_opargs[o->op_type] & OA_FOLDCONST))
                break;
            /* FALLTHROUGH */
        case OP_REPEAT:
            if (o->op_flags & OPf_STACKED)
                break;
            if (o->op_type == OP_REPEAT)
                scalar(cBINOPo->op_first);
            goto func_ops;
        case OP_SUBSTR:
            if (o->op_private == 4)
                break;
            /* FALLTHROUGH */
        case OP_WANTARRAY:
        case OP_GV:
        case OP_SMARTMATCH:
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
        case OP_KVASLICE:
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
            useless = OP_DESC(o);
            break;

        case OP_GVSV:
        case OP_PADSV:
        case OP_PADAV:
        case OP_PADHV:
        case OP_PADANY:
        case OP_AELEM:
        case OP_AELEMFAST:
        case OP_AELEMFAST_LEX:
        case OP_ASLICE:
        case OP_HELEM:
        case OP_HSLICE:
            if (!(o->op_private & (OPpLVAL_INTRO|OPpOUR_INTRO)))
                /* Otherwise it's "Useless use of grep iterator" */
                useless = OP_DESC(o);
            break;

        case OP_SPLIT:
            kid = cLISTOPo->op_first;
            if (kid && kid->op_type == OP_PUSHRE
                && !kid->op_targ
                && !(o->op_flags & OPf_STACKED)
#ifdef USE_ITHREADS
                && !((PMOP*)kid)->op_pmreplrootu.op_pmtargetoff
#else
                && !((PMOP*)kid)->op_pmreplrootu.op_pmtargetgv
#endif
                )
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
                (!OpHAS_SIBLING(o) || OpSIBLING(o)->op_type != OP_READLINE))
                useless = "a variable";
            break;

        case OP_CONST:
            sv = cSVOPo_sv;
            if (cSVOPo->op_private & OPpCONST_STRICT)
                no_bareword_allowed(o);
            else {
                if (ckWARN(WARN_VOID)) {
                    NV nv;
                    /* don't warn on optimised away booleans, eg
                     * use constant Foo, 5; Foo || print; */
                    if (cSVOPo->op_private & OPpCONST_SHORTCIRCUIT)
                        useless = NULL;
                    /* the constants 0 and 1 are permitted as they are
                       conventionally used as dummies in constructs like
                       1 while some_condition_with_side_effects;  */
                    else if (SvNIOK(sv) && ((nv = SvNV(sv)) == 0.0 || nv == 1.0))
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
                        useless_sv = Perl_newSVpvf(aTHX_ "a constant (%"SVf")", SVfARG(sv));
                    }
                    else
                        useless = "a constant (undef)";
                }
            }
            op_null(o);         /* don't execute or even remember it */
            break;

        case OP_POSTINC:
            OpTYPE_set(o, OP_PREINC);  /* pre-increment is faster */
            break;

        case OP_POSTDEC:
            OpTYPE_set(o, OP_PREDEC);  /* pre-decrement is faster */
            break;

        case OP_I_POSTINC:
            OpTYPE_set(o, OP_I_PREINC);        /* pre-increment is faster */
            break;

        case OP_I_POSTDEC:
            OpTYPE_set(o, OP_I_PREDEC);        /* pre-decrement is faster */
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

            if (!refgen || (refgen->op_type != OP_REFGEN
                            && refgen->op_type != OP_SREFGEN))
                break;

            exlist = (LISTOP *)refgen->op_first;
            if (!exlist || exlist->op_type != OP_NULL
                || exlist->op_targ != OP_LIST)
                break;

            if (exlist->op_first->op_type != OP_PUSHMARK
                && exlist->op_first != exlist->op_last)
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
                && (kid->op_flags & OPf_KIDS)) {
                if (o->op_type == OP_AND) {
                    OpTYPE_set(o, OP_OR);
                } else {
                    OpTYPE_set(o, OP_AND);
                }
                op_null(kid);
            }
            /* FALLTHROUGH */

        case OP_DOR:
        case OP_COND_EXPR:
        case OP_ENTERGIVEN:
        case OP_ENTERWHEN:
            for (kid = OpSIBLING(cUNOPo->op_first); kid; kid = OpSIBLING(kid))
                if (!(kid->op_flags & OPf_KIDS))
                    scalarvoid(kid);
                else
                    DEFER_OP(kid);
        break;

        case OP_NULL:
            if (o->op_flags & OPf_STACKED)
                break;
            /* FALLTHROUGH */
        case OP_NEXTSTATE:
        case OP_DBSTATE:
        case OP_ENTERTRY:
        case OP_ENTER:
            if (!(o->op_flags & OPf_KIDS))
                break;
            /* FALLTHROUGH */
        case OP_SCOPE:
        case OP_LEAVE:
        case OP_LEAVETRY:
        case OP_LEAVELOOP:
        case OP_LINESEQ:
        case OP_LEAVEGIVEN:
        case OP_LEAVEWHEN:
        kids:
            for (kid = cLISTOPo->op_first; kid; kid = OpSIBLING(kid))
                if (!(kid->op_flags & OPf_KIDS))
                    scalarvoid(kid);
                else
                    DEFER_OP(kid);
            break;
        case OP_LIST:
            /* If the first kid after pushmark is something that the padrange
               optimisation would reject, then null the list and the pushmark.
            */
            if ((kid = cLISTOPo->op_first)->op_type == OP_PUSHMARK
                && (  !(kid = OpSIBLING(kid))
                      || (  kid->op_type != OP_PADSV
                            && kid->op_type != OP_PADAV
                            && kid->op_type != OP_PADHV)
                      || kid->op_private & ~OPpLVAL_INTRO
                      || !(kid = OpSIBLING(kid))
                      || (  kid->op_type != OP_PADSV
                            && kid->op_type != OP_PADAV
                            && kid->op_type != OP_PADHV)
                      || kid->op_private & ~OPpLVAL_INTRO)
            ) {
                op_null(cUNOPo->op_first); /* NULL the pushmark */
                op_null(o); /* NULL the list */
            }
            goto kids;
        case OP_ENTEREVAL:
            scalarkids(o);
            break;
        case OP_SCALAR:
            scalar(o);
            break;
        }

        if (useless_sv) {
            /* mortalise it, in case warnings are fatal.  */
            Perl_ck_warner(aTHX_ packWARN(WARN_VOID),
                           "Useless use of %"SVf" in void context",
                           SVfARG(sv_2mortal(useless_sv)));
        }
        else if (useless) {
            Perl_ck_warner(aTHX_ packWARN(WARN_VOID),
                           "Useless use of %s in void context",
                           useless);
        }
    } while ( (o = POP_DEFERRED_OP()) );

    Safefree(defer_stack);

    return arg;
}

static OP *
S_listkids(pTHX_ OP *o)
{
    if (o && o->op_flags & OPf_KIDS) {
        OP *kid;
	for (kid = cLISTOPo->op_first; kid; kid = OpSIBLING(kid))
	    list(kid);
    }
    return o;
}

OP *
Perl_list(pTHX_ OP *o)
{
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
	list(cBINOPo->op_first);
	break;
    case OP_REPEAT:
	if (o->op_private & OPpREPEAT_DOLIST
	 && !(o->op_flags & OPf_STACKED))
	{
	    list(cBINOPo->op_first);
	    kid = cBINOPo->op_last;
	    if (kid->op_type == OP_CONST && SvIOK(kSVOP_sv)
	     && SvIVX(kSVOP_sv) == 1)
	    {
		op_null(o); /* repeat */
		op_null(cUNOPx(cBINOPo->op_first)->op_first);/* pushmark */
		/* const (rhs): */
		op_free(op_sibling_splice(o, cBINOPo->op_first, 1, NULL));
	    }
	}
	break;
    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = OpSIBLING(cUNOPo->op_first); kid; kid = OpSIBLING(kid))
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
	listkids(o);
	break;
    case OP_LIST:
	listkids(o);
	if (cLISTOPo->op_first->op_type == OP_PUSHMARK) {
	    op_null(cUNOPo->op_first); /* NULL the pushmark */
	    op_null(o); /* NULL the list */
	}
	break;
    case OP_LEAVE:
    case OP_LEAVETRY:
	kid = cLISTOPo->op_first;
	list(kid);
	kid = OpSIBLING(kid);
    do_kids:
	while (kid) {
	    OP *sib = OpSIBLING(kid);
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
    if (o) {
	const OPCODE type = o->op_type;

	if (type == OP_LINESEQ || type == OP_SCOPE ||
	    type == OP_LEAVE || type == OP_LEAVETRY)
	{
     	    OP *kid, *sib;
	    for (kid = cLISTOPo->op_first; kid; kid = sib) {
		if ((sib = OpSIBLING(kid))
		 && (  OpHAS_SIBLING(sib) || sib->op_type != OP_NULL
		    || (  sib->op_targ != OP_NEXTSTATE
		       && sib->op_targ != OP_DBSTATE  )))
		{
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
        for (kid = cLISTOPo->op_first; kid; kid = OpSIBLING(kid))
	    op_lvalue(kid, type);
    }
    return o;
}


/* for a helem/hslice/kvslice, if its a fixed hash, croak on invalid
 * const fields. Also, convert CONST keys to HEK-in-SVs.
 * rop is the op that retrieves the hash;
 * key_op is the first key
 */

STATIC void
S_check_hash_fields_and_hekify(pTHX_ UNOP *rop, SVOP *key_op)
{
    PADNAME *lexname;
    GV **fields;
    bool check_fields;

    /* find the padsv corresponding to $lex->{} or @{$lex}{} */
    if (rop) {
        if (rop->op_first->op_type == OP_PADSV)
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
    }

    lexname = NULL; /* just to silence compiler warnings */
    fields  = NULL; /* just to silence compiler warnings */

    check_fields =
            rop
         && (lexname = padnamelist_fetch(PL_comppad_name, rop->op_targ),
             SvPAD_TYPED(lexname))
         && (fields = (GV**)hv_fetchs(PadnameTYPE(lexname), "FIELDS", FALSE))
         && isGV(*fields) && GvHV(*fields);

    for (; key_op; key_op = (SVOP*)OpSIBLING(key_op)) {
        SV **svp, *sv;
        if (key_op->op_type != OP_CONST)
            continue;
        svp = cSVOPx_svp(key_op);

        /* make sure it's not a bareword under strict subs */
        if (key_op->op_private & OPpCONST_BARE &&
            key_op->op_private & OPpCONST_STRICT)
        {
            no_bareword_allowed((OP*)key_op);
        }

        /* Make the CONST have a shared SV */
        if (   !SvIsCOW_shared_hash(sv = *svp)
            && SvTYPE(sv) < SVt_PVMG
            && SvOK(sv)
            && !SvROK(sv))
        {
            SSize_t keylen;
            const char * const key = SvPV_const(sv, *(STRLEN*)&keylen);
            SV *nsv = newSVpvn_share(key, SvUTF8(sv) ? -keylen : keylen, 0);
            SvREFCNT_dec_NN(sv);
            *svp = nsv;
        }

        if (   check_fields
            && !hv_fetch_ent(GvHV(*fields), *svp, FALSE, 0))
        {
            Perl_croak(aTHX_ "No such class field \"%"SVf"\" "
                        "in variable %"PNf" of type %"HEKf,
                        SVfARG(*svp), PNfARG(lexname),
                        HEKfARG(HvNAME_HEK(PadnameTYPE(lexname))));
        }
    }
}


/*
=for apidoc finalize_optree

This function finalizes the optree.  Should be called directly after
the complete optree is built.  It does some additional
checking which can't be done in the normal C<ck_>xxx functions and makes
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

#ifdef USE_ITHREADS
/* Relocate sv to the pad for thread safety.
 * Despite being a "constant", the SV is written to,
 * for reference counts, sv_upgrade() etc. */
PERL_STATIC_INLINE void
S_op_relocate_sv(pTHX_ SV** svp, PADOFFSET* targp)
{
    PADOFFSET ix;
    PERL_ARGS_ASSERT_OP_RELOCATE_SV;
    if (!*svp) return;
    ix = pad_alloc(OP_CONST, SVf_READONLY);
    SvREFCNT_dec(PAD_SVl(ix));
    PAD_SETSV(ix, *svp);
    /* XXX I don't know how this isn't readonly already. */
    if (!SvIsCOW(PAD_SVl(ix))) SvREADONLY_on(PAD_SVl(ix));
    *svp = NULL;
    *targp = ix;
}
#endif


STATIC void
S_finalize_op(pTHX_ OP* o)
{
    PERL_ARGS_ASSERT_FINALIZE_OP;


    switch (o->op_type) {
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	PL_curcop = ((COP*)o);		/* for warnings */
	break;
    case OP_EXEC:
        if (OpHAS_SIBLING(o)) {
            OP *sib = OpSIBLING(o);
            if ((  sib->op_type == OP_NEXTSTATE || sib->op_type == OP_DBSTATE)
                && ckWARN(WARN_EXEC)
                && OpHAS_SIBLING(sib))
            {
		    const OPCODE type = OpSIBLING(sib)->op_type;
		    if (type != OP_EXIT && type != OP_WARN && type != OP_DIE) {
			const line_t oldline = CopLINE(PL_curcop);
			CopLINE_set(PL_curcop, CopLINE((COP*)sib));
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
        op_relocate_sv(&cSVOPo->op_sv, &o->op_targ);
#endif
        break;

#ifdef USE_ITHREADS
    /* Relocate all the METHOP's SVs to the pad for thread safety. */
    case OP_METHOD_NAMED:
    case OP_METHOD_SUPER:
    case OP_METHOD_REDIR:
    case OP_METHOD_REDIR_SUPER:
        op_relocate_sv(&cMETHOPx(o)->op_u.op_meth_sv, &o->op_targ);
        break;
#endif

    case OP_HELEM: {
	UNOP *rop;
	SVOP *key_op;
	OP *kid;

	if ((key_op = cSVOPx(((BINOP*)o)->op_last))->op_type != OP_CONST)
	    break;

	rop = (UNOP*)((BINOP*)o)->op_first;

	goto check_keys;

    case OP_HSLICE:
	S_scalar_slice_warning(aTHX_ o);
        /* FALLTHROUGH */

    case OP_KVHSLICE:
        kid = OpSIBLING(cLISTOPo->op_first);
	if (/* I bet there's always a pushmark... */
	    OP_TYPE_ISNT_AND_WASNT_NN(kid, OP_LIST)
	    && OP_TYPE_ISNT_NN(kid, OP_CONST))
        {
	    break;
        }

	key_op = (SVOP*)(kid->op_type == OP_CONST
				? kid
				: OpSIBLING(kLISTOP->op_first));

	rop = (UNOP*)((LISTOP*)o)->op_last;

      check_keys:	
        if (o->op_private & OPpLVAL_INTRO || rop->op_type != OP_RV2HV)
            rop = NULL;
        S_check_hash_fields_and_hekify(aTHX_ rop, key_op);
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

#ifdef DEBUGGING
        /* check that op_last points to the last sibling, and that
         * the last op_sibling/op_sibparent field points back to the
         * parent, and that the only ops with KIDS are those which are
         * entitled to them */
        U32 type = o->op_type;
        U32 family;
        bool has_last;

        if (type == OP_NULL) {
            type = o->op_targ;
            /* ck_glob creates a null UNOP with ex-type GLOB
             * (which is a list op. So pretend it wasn't a listop */
            if (type == OP_GLOB)
                type = OP_NULL;
        }
        family = PL_opargs[type] & OA_CLASS_MASK;

        has_last = (   family == OA_BINOP
                    || family == OA_LISTOP
                    || family == OA_PMOP
                    || family == OA_LOOP
                   );
        assert(  has_last /* has op_first and op_last, or ...
              ... has (or may have) op_first: */
              || family == OA_UNOP
              || family == OA_UNOP_AUX
              || family == OA_LOGOP
              || family == OA_BASEOP_OR_UNOP
              || family == OA_FILESTATOP
              || family == OA_LOOPEXOP
              || family == OA_METHOP
              /* I don't know why SASSIGN is tagged as OA_BASEOP - DAPM */
              || type == OP_SASSIGN
              || type == OP_CUSTOM
              || type == OP_NULL /* new_logop does this */
              );

        for (kid = cUNOPo->op_first; kid; kid = OpSIBLING(kid)) {
#  ifdef PERL_OP_PARENT
            if (!OpHAS_SIBLING(kid)) {
                if (has_last)
                    assert(kid == cLISTOPo->op_last);
                assert(kid->op_sibparent == o);
            }
#  else
            if (has_last && !OpHAS_SIBLING(kid))
                assert(kid == cLISTOPo->op_last);
#  endif
        }
#endif

	for (kid = cUNOPo->op_first; kid; kid = OpSIBLING(kid))
	    finalize_op(kid);
    }
}

/*
=for apidoc Amx|OP *|op_lvalue|OP *o|I32 type

Propagate lvalue ("modifiable") context to an op and its children.
C<type> represents the context type, roughly based on the type of op that
would do the modifying, although C<local()> is represented by C<OP_NULL>,
because it has no op type of its own (it is signalled by a flag on
the lvalue op).

This function detects things that can't be modified, such as C<$x+1>, and
generates errors for them.  For example, C<$x+1 = 2> would cause it to be
called with an op of type C<OP_ADD> and a C<type> argument of C<OP_SASSIGN>.

It also flags things that need to behave specially in an lvalue context,
such as C<$$x = 5> which might have to vivify a reference in C<$x>.

=cut
*/

static void
S_mark_padname_lvalue(pTHX_ PADNAME *pn)
{
    CV *cv = PL_compcv;
    PadnameLVALUE_on(pn);
    while (PadnameOUTER(pn) && PARENT_PAD_INDEX(pn)) {
	cv = CvOUTSIDE(cv);
        /* RT #127786: cv can be NULL due to an eval within the DB package
         * called from an anon sub - anon subs don't have CvOUTSIDE() set
         * unless they contain an eval, but calling eval within DB
         * pretends the eval was done in the caller's scope.
         */
	if (!cv)
            break;
	assert(CvPADLIST(cv));
	pn =
	   PadlistNAMESARRAY(CvPADLIST(cv))[PARENT_PAD_INDEX(pn)];
	assert(PadnameLEN(pn));
	PadnameLVALUE_on(pn);
    }
}

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

static void
S_lvref(pTHX_ OP *o, I32 type)
{
    dVAR;
    OP *kid;
    switch (o->op_type) {
    case OP_COND_EXPR:
	for (kid = OpSIBLING(cUNOPo->op_first); kid;
	     kid = OpSIBLING(kid))
	    S_lvref(aTHX_ kid, type);
	/* FALLTHROUGH */
    case OP_PUSHMARK:
	return;
    case OP_RV2AV:
	if (cUNOPo->op_first->op_type != OP_GV) goto badref;
	o->op_flags |= OPf_STACKED;
	if (o->op_flags & OPf_PARENS) {
	    if (o->op_private & OPpLVAL_INTRO) {
		 yyerror(Perl_form(aTHX_ "Can't modify reference to "
		      "localized parenthesized array in list assignment"));
		return;
	    }
	  slurpy:
            OpTYPE_set(o, OP_LVAVREF);
	    o->op_private &= OPpLVAL_INTRO|OPpPAD_STATE;
	    o->op_flags |= OPf_MOD|OPf_REF;
	    return;
	}
	o->op_private |= OPpLVREF_AV;
	goto checkgv;
    case OP_RV2CV:
	kid = cUNOPo->op_first;
	if (kid->op_type == OP_NULL)
	    kid = cUNOPx(OpSIBLING(kUNOP->op_first))
		->op_first;
	o->op_private = OPpLVREF_CV;
	if (kid->op_type == OP_GV)
	    o->op_flags |= OPf_STACKED;
	else if (kid->op_type == OP_PADCV) {
	    o->op_targ = kid->op_targ;
	    kid->op_targ = 0;
	    op_free(cUNOPo->op_first);
	    cUNOPo->op_first = NULL;
	    o->op_flags &=~ OPf_KIDS;
	}
	else goto badref;
	break;
    case OP_RV2HV:
	if (o->op_flags & OPf_PARENS) {
	  parenhash:
	    yyerror(Perl_form(aTHX_ "Can't modify reference to "
				 "parenthesized hash in list assignment"));
		return;
	}
	o->op_private |= OPpLVREF_HV;
	/* FALLTHROUGH */
    case OP_RV2SV:
      checkgv:
	if (cUNOPo->op_first->op_type != OP_GV) goto badref;
	o->op_flags |= OPf_STACKED;
	break;
    case OP_PADHV:
	if (o->op_flags & OPf_PARENS) goto parenhash;
	o->op_private |= OPpLVREF_HV;
	/* FALLTHROUGH */
    case OP_PADSV:
	PAD_COMPNAME_GEN_set(o->op_targ, PERL_INT_MAX);
	break;
    case OP_PADAV:
	PAD_COMPNAME_GEN_set(o->op_targ, PERL_INT_MAX);
	if (o->op_flags & OPf_PARENS) goto slurpy;
	o->op_private |= OPpLVREF_AV;
	break;
    case OP_AELEM:
    case OP_HELEM:
	o->op_private |= OPpLVREF_ELEM;
	o->op_flags   |= OPf_STACKED;
	break;
    case OP_ASLICE:
    case OP_HSLICE:
        OpTYPE_set(o, OP_LVREFSLICE);
	o->op_private &= OPpLVAL_INTRO|OPpLVREF_ELEM;
	return;
    case OP_NULL:
	if (o->op_flags & OPf_SPECIAL)		/* do BLOCK */
	    goto badref;
	else if (!(o->op_flags & OPf_KIDS))
	    return;
	if (o->op_targ != OP_LIST) {
	    S_lvref(aTHX_ cBINOPo->op_first, type);
	    return;
	}
	/* FALLTHROUGH */
    case OP_LIST:
	for (kid = cLISTOPo->op_first; kid; kid = OpSIBLING(kid)) {
	    assert((kid->op_flags & OPf_WANT) != OPf_WANT_VOID);
	    S_lvref(aTHX_ kid, type);
	}
	return;
    case OP_STUB:
	if (o->op_flags & OPf_PARENS)
	    return;
	/* FALLTHROUGH */
    default:
      badref:
	/* diag_listed_as: Can't modify reference to %s in %s assignment */
	yyerror(Perl_form(aTHX_ "Can't modify reference to %s in %s",
		     o->op_type == OP_NULL && o->op_flags & OPf_SPECIAL
		      ? "do block"
		      : OP_DESC(o),
		     PL_op_desc[type]));
    }
    OpTYPE_set(o, OP_LVREF);
    o->op_private &=
	OPpLVAL_INTRO|OPpLVREF_ELEM|OPpLVREF_TYPE|OPpPAD_STATE;
    if (type == OP_ENTERLOOP)
	o->op_private |= OPpLVREF_ITER;
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
	if ((o->op_flags & OPf_PARENS))
	    break;
	goto nomod;
    case OP_ENTERSUB:
	if ((type == OP_UNDEF || type == OP_REFGEN || type == OP_LOCK) &&
	    !(o->op_flags & OPf_STACKED)) {
            OpTYPE_set(o, OP_RV2CV);		/* entersub => rv2cv */
	    assert(cUNOPo->op_first->op_type == OP_NULL);
	    op_null(((LISTOP*)cUNOPo->op_first)->op_first);/* disable pushmark */
	    break;
	}
	else {				/* lvalue subroutine call */
	    o->op_private |= OPpLVAL_INTRO;
	    PL_modcount = RETURN_UNLIMITED_NUMBER;
	    if (type == OP_GREPSTART || type == OP_ENTERSUB
	     || type == OP_REFGEN    || type == OP_LEAVESUBLV) {
		/* Potential lvalue context: */
		o->op_private |= OPpENTERSUB_INARGS;
		break;
	    }
	    else {                      /* Compile-time error message: */
		OP *kid = cUNOPo->op_first;
		CV *cv;
		GV *gv;
                SV *namesv;

		if (kid->op_type != OP_PUSHMARK) {
		    if (kid->op_type != OP_NULL || kid->op_targ != OP_LIST)
			Perl_croak(aTHX_
				"panic: unexpected lvalue entersub "
				"args: type/targ %ld:%"UVuf,
				(long)kid->op_type, (UV)kid->op_targ);
		    kid = kLISTOP->op_first;
		}
		while (OpHAS_SIBLING(kid))
		    kid = OpSIBLING(kid);
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

		gv = kGVOP_gv;
		cv = isGV(gv)
		    ? GvCV(gv)
		    : SvROK(gv) && SvTYPE(SvRV(gv)) == SVt_PVCV
			? MUTABLE_CV(SvRV(gv))
			: NULL;
		if (!cv)
		    break;
		if (CvLVALUE(cv))
		    break;
                if (flags & OP_LVALUE_NO_CROAK)
                    return NULL;

                namesv = cv_name(cv, NULL, 0);
                yyerror_pv(Perl_form(aTHX_ "Can't modify non-lvalue "
                                     "subroutine call of &%"SVf" in %s",
                                     SVfARG(namesv), PL_op_desc[type]),
                           SvUTF8(namesv));
                return o;
	    }
	}
	/* FALLTHROUGH */
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
		      : OP_DESC(o)),
		     type ? PL_op_desc[type] : "local"));
	return o;

    case OP_PREINC:
    case OP_PREDEC:
    case OP_POW:
    case OP_MULTIPLY:
    case OP_DIVIDE:
    case OP_MODULO:
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

    case OP_REPEAT:
	if (o->op_flags & OPf_STACKED) {
	    PL_modcount++;
	    break;
	}
	if (!(o->op_private & OPpREPEAT_DOLIST))
	    goto nomod;
	else {
	    const I32 mods = PL_modcount;
	    modkids(cBINOPo->op_first, type);
	    if (type != OP_AASSIGN)
		goto nomod;
	    kid = cBINOPo->op_last;
	    if (kid->op_type == OP_CONST && SvIOK(kSVOP_sv)) {
		const IV iv = SvIV(kSVOP_sv);
		if (PL_modcount != RETURN_UNLIMITED_NUMBER)
		    PL_modcount =
			mods + (PL_modcount - mods) * (iv < 0 ? 0 : iv);
	    }
	    else
		PL_modcount = RETURN_UNLIMITED_NUMBER;
	}
	break;

    case OP_COND_EXPR:
	localize = 1;
	for (kid = OpSIBLING(cUNOPo->op_first); kid; kid = OpSIBLING(kid))
	    op_lvalue(kid, type);
	break;

    case OP_RV2AV:
    case OP_RV2HV:
	if (type == OP_REFGEN && o->op_flags & OPf_PARENS) {
           PL_modcount = RETURN_UNLIMITED_NUMBER;
	    return o;		/* Treat \(@foo) like ordinary list. */
	}
	/* FALLTHROUGH */
    case OP_RV2GV:
	if (scalar_mod_type(o, type))
	    goto nomod;
	ref(cUNOPo->op_first, o->op_type);
	/* FALLTHROUGH */
    case OP_ASLICE:
    case OP_HSLICE:
	localize = 1;
	/* FALLTHROUGH */
    case OP_AASSIGN:
	/* Do not apply the lvsub flag for rv2[ah]v in scalar context.  */
	if (type == OP_LEAVESUBLV && (
		(o->op_type != OP_RV2AV && o->op_type != OP_RV2HV)
	     || (o->op_flags & OPf_WANT) != OPf_WANT_SCALAR
	   ))
	    o->op_private |= OPpMAYBE_LVSUB;
	/* FALLTHROUGH */
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
	/* FALLTHROUGH */
    case OP_GV:
	PL_hints |= HINT_BLOCK_SCOPE;
        /* FALLTHROUGH */
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
	/* FALLTHROUGH */
    case OP_PADSV:
	PL_modcount++;
	if (!type) /* local() */
	    Perl_croak(aTHX_ "Can't localize lexical variable %"PNf,
			      PNfARG(PAD_COMPNAME(o->op_targ)));
	if (!(o->op_private & OPpLVAL_INTRO)
	 || (  type != OP_SASSIGN && type != OP_AASSIGN
	    && PadnameIsSTATE(PAD_COMPNAME_SV(o->op_targ))  ))
	    S_mark_padname_lvalue(aTHX_ PAD_COMPNAME_SV(o->op_targ));
	break;

    case OP_PUSHMARK:
	localize = 0;
	break;

    case OP_KEYS:
	if (type != OP_SASSIGN && type != OP_LEAVESUBLV)
	    goto nomod;
	goto lvalue_func;
    case OP_SUBSTR:
	if (o->op_private == 4) /* don't allow 4 arg substr as lvalue */
	    goto nomod;
	/* FALLTHROUGH */
    case OP_POS:
    case OP_VEC:
      lvalue_func:
	if (type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
	if (o->op_flags & OPf_KIDS)
	    op_lvalue(OpSIBLING(cBINOPo->op_first), type);
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
        /* FALLTHROUGH */
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
	/* FALLTHROUGH */
    case OP_LIST:
	localize = 0;
	for (kid = cLISTOPo->op_first; kid; kid = OpSIBLING(kid))
	    /* elements might be in void context because the list is
	       in scalar context or because they are attribute sub calls */
	    if ( (kid->op_flags & OPf_WANT) != OPf_WANT_VOID )
		op_lvalue(kid, type);
	break;

    case OP_COREARGS:
	return o;

    case OP_AND:
    case OP_OR:
	if (type == OP_LEAVESUBLV
	 || !S_vivifies(cLOGOPo->op_first->op_type))
	    op_lvalue(cLOGOPo->op_first, type);
	if (type == OP_LEAVESUBLV
	 || !S_vivifies(OpSIBLING(cLOGOPo->op_first)->op_type))
	    op_lvalue(OpSIBLING(cLOGOPo->op_first), type);
	goto nomod;

    case OP_SREFGEN:
	if (type != OP_AASSIGN && type != OP_SASSIGN
	 && type != OP_ENTERLOOP)
	    goto nomod;
	/* Don’t bother applying lvalue context to the ex-list.  */
	kid = cUNOPx(cUNOPo->op_first)->op_first;
	assert (!OpHAS_SIBLING(kid));
	goto kid_2lvref;
    case OP_REFGEN:
	if (type != OP_AASSIGN) goto nomod;
	kid = cUNOPo->op_first;
      kid_2lvref:
	{
	    const U8 ec = PL_parser ? PL_parser->error_count : 0;
	    S_lvref(aTHX_ kid, type);
	    if (!PL_parser || PL_parser->error_count == ec) {
		if (!FEATURE_REFALIASING_IS_ENABLED)
		    Perl_croak(aTHX_
		       "Experimental aliasing via reference not enabled");
		Perl_ck_warner_d(aTHX_
				 packWARN(WARN_EXPERIMENTAL__REFALIASING),
				"Aliasing via reference is experimental");
	    }
	}
	if (o->op_type == OP_REFGEN)
	    op_null(cUNOPx(cUNOPo->op_first)->op_first); /* pushmark */
	op_null(o);
	return o;

    case OP_SPLIT:
	kid = cLISTOPo->op_first;
	if (kid && kid->op_type == OP_PUSHRE &&
		(  kid->op_targ
		|| o->op_flags & OPf_STACKED
#ifdef USE_ITHREADS
		|| ((PMOP*)kid)->op_pmreplrootu.op_pmtargetoff
#else
		|| ((PMOP*)kid)->op_pmreplrootu.op_pmtargetgv
#endif
	)) {
	    /* This is actually @array = split.  */
	    PL_modcount = RETURN_UNLIMITED_NUMBER;
	    break;
	}
	goto nomod;

    case OP_SCALAR:
	op_lvalue(cUNOPo->op_first, OP_ENTERSUB);
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
	/* FALLTHROUGH */
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
    case OP_NBIT_AND:
    case OP_NBIT_XOR:
    case OP_NBIT_OR:
    case OP_SBIT_AND:
    case OP_SBIT_XOR:
    case OP_SBIT_OR:
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
	/* FALLTHROUGH */
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
        for (kid = cLISTOPo->op_first; kid; kid = OpSIBLING(kid))
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

    if (PL_parser && PL_parser->error_count)
	return o;

    switch (o->op_type) {
    case OP_ENTERSUB:
	if ((type == OP_EXISTS || type == OP_DEFINED) &&
	    !(o->op_flags & OPf_STACKED)) {
            OpTYPE_set(o, OP_RV2CV);             /* entersub => rv2cv */
	    assert(cUNOPo->op_first->op_type == OP_NULL);
	    op_null(((LISTOP*)cUNOPo->op_first)->op_first);	/* disable pushmark */
	    o->op_flags |= OPf_SPECIAL;
	}
	else if (type == OP_RV2SV || type == OP_RV2AV || type == OP_RV2HV){
	    o->op_private |= (type == OP_RV2AV ? OPpDEREF_AV
			      : type == OP_RV2HV ? OPpDEREF_HV
			      : OPpDEREF_SV);
	    o->op_flags |= OPf_MOD;
	}

	break;

    case OP_COND_EXPR:
	for (kid = OpSIBLING(cUNOPo->op_first); kid; kid = OpSIBLING(kid))
	    doref(kid, type, set_op_ref);
	break;
    case OP_RV2SV:
	if (type == OP_DEFINED)
	    o->op_flags |= OPf_SPECIAL;		/* don't create GV */
	doref(cUNOPo->op_first, o->op_type, set_op_ref);
	/* FALLTHROUGH */
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
	/* FALLTHROUGH */
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
	/* FALLTHROUGH */
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
    OP *rop;

    PERL_ARGS_ASSERT_DUP_ATTRLIST;

    /* An attrlist is either a simple OP_CONST or an OP_LIST with kids,
     * where the first kid is OP_PUSHMARK and the remaining ones
     * are OP_CONST.  We need to push the OP_CONST values.
     */
    if (o->op_type == OP_CONST)
	rop = newSVOP(OP_CONST, o->op_flags, SvREFCNT_inc_NN(cSVOPo->op_sv));
    else {
	assert((o->op_type == OP_LIST) && (o->op_flags & OPf_KIDS));
	rop = NULL;
	for (o = cLISTOPo->op_first; o; o = OpSIBLING(o)) {
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
    PERL_ARGS_ASSERT_APPLY_ATTRS;
    {
        SV * const stashsv = newSVhek(HvNAME_HEK(stash));

        /* fake up C<use attributes $pkg,$rv,@attrs> */

#define ATTRSMODULE "attributes"
#define ATTRSMODULE_PM "attributes.pm"

        Perl_load_module(
          aTHX_ PERL_LOADMOD_IMPORT_OPS,
          newSVpvs(ATTRSMODULE),
          NULL,
          op_prepend_elem(OP_LIST,
                          newSVOP(OP_CONST, 0, stashsv),
                          op_prepend_elem(OP_LIST,
                                          newSVOP(OP_CONST, 0,
                                                  newRV(target)),
                                          dup_attrlist(attrs))));
    }
}

STATIC void
S_apply_attrs_my(pTHX_ HV *stash, OP *target, OP *attrs, OP **imopsp)
{
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
    stashsv = newSVhek(HvNAME_HEK(stash));

    arg = newOP(OP_PADSV, 0);
    arg->op_targ = target->op_targ;
    arg = op_prepend_elem(OP_LIST,
		       newSVOP(OP_CONST, 0, stashsv),
		       op_prepend_elem(OP_LIST,
				    newUNOP(OP_REFGEN, 0,
					    arg),
				    dup_attrlist(attrs)));

    /* Fake up a method call to import */
    meth = newSVpvs_share("import");
    imop = op_convert_list(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL|OPf_WANT_VOID,
		   op_append_elem(OP_LIST,
			       op_prepend_elem(OP_LIST, pack, arg),
			       newMETHOP_named(OP_METHOD_NAMED, 0, meth)));

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
        OP * lasto;
        assert(o->op_flags & OPf_KIDS);
        lasto = cLISTOPo->op_first;
        assert(lasto->op_type == OP_PUSHMARK);
        for (o = OpSIBLING(lasto); o; o = OpSIBLING(o)) {
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
                    /* excise new_proto from the list */
                    op_sibling_splice(*attrs, lasto, 1, NULL);
                    o = lasto;
                    continue;
                }
            }
            lasto = o;
        }
        /* If the list is now just the PUSHMARK, scrap the whole thing; otherwise attributes.xs
           would get pulled in with no real need */
        if (!OpHAS_SIBLING(cLISTOPx(*attrs)->op_first)) {
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
    I32 type;
    const bool stately = PL_parser && PL_parser->in_my == KEY_state;

    PERL_ARGS_ASSERT_MY_KID;

    if (!o || (PL_parser && PL_parser->error_count))
	return o;

    type = o->op_type;

    if (type == OP_LIST) {
        OP *kid;
        for (kid = cLISTOPo->op_first; kid; kid = OpSIBLING(kid))
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
	    assert(PL_parser);
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

        assert(PL_parser);
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
                /* excise pushmark */
                op_sibling_splice(rops, NULL, 1, NULL);
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
             desc, SVfARG(name), SVfARG(name));
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
    if (!(right->op_flags & OPf_STACKED) && !right->op_targ && ismatchop) {
        if (left->op_type == OP_PADSV
         && !(left->op_private & OPpLVAL_INTRO))
        {
            right->op_targ = left->op_targ;
            op_free(left);
            o = right;
        }
        else {
            right->op_flags |= OPf_STACKED;
            if (rtype != OP_MATCH && rtype != OP_TRANSR &&
            ! (rtype == OP_TRANS &&
               right->op_private & OPpTRANS_IDENTICAL) &&
	    ! (rtype == OP_SUBST &&
	       (cPMOPx(right)->op_pmflags & PMf_NONDESTRUCT)))
		left = op_lvalue(left, rtype);
	    if (right->op_type == OP_TRANS || right->op_type == OP_TRANSR)
		o = newBINOP(OP_NULL, OPf_STACKED, scalar(left), right);
	    else
		o = op_prepend_elem(rtype, scalar(left), right);
	}
	if (type == OP_NOT)
	    return newUNOP(OP_NOT, 0, scalar(o));
	return o;
    }
    else
	return bind_match(type, left,
		pmruntime(newPMOP(OP_MATCH, 0), right, NULL, 0, 0));
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
            OpTYPE_set(o, OP_LEAVE);
	}
	else if (o->op_type == OP_LINESEQ) {
	    OP *kid;
            OpTYPE_set(o, OP_SCOPE);
	    kid = ((LISTOP*)o)->op_first;
	    if (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE) {
		op_null(kid);

		/* The following deals with things like 'do {1 for 1}' */
		kid = OpSIBLING(kid);
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
	for(; kid; kid = OpSIBLING(kid))
	    if (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE)
		op_null(kid);
    }
    return o;
}

/*
=for apidoc Am|int|block_start|int full

Handles compile-time scope entry.
Arranges for hints to be restored on block
exit and also handles pad sequence numbers to make lexical variables scope
right.  Returns a savestack index for use with C<block_end>.

=cut
*/

int
Perl_block_start(pTHX_ int full)
{
    const int retval = PL_savestack_ix;

    PL_compiling.cop_seq = PL_cop_seqmax;
    COP_SEQMAX_INC;
    pad_block_start(full);
    SAVEHINTS();
    PL_hints &= ~HINT_BLOCK_SCOPE;
    SAVECOMPILEWARNINGS();
    PL_compiling.cop_warnings = DUP_WARNINGS(PL_compiling.cop_warnings);
    SAVEI32(PL_compiling.cop_seq);
    PL_compiling.cop_seq = 0;

    CALL_BLOCK_HOOKS(bhk_start, full);

    return retval;
}

/*
=for apidoc Am|OP *|block_end|I32 floor|OP *seq

Handles compile-time scope exit.  C<floor>
is the savestack index returned by
C<block_start>, and C<seq> is the body of the block.  Returns the block,
possibly modified.

=cut
*/

OP*
Perl_block_end(pTHX_ I32 floor, OP *seq)
{
    const int needblockscope = PL_hints & HINT_BLOCK_SCOPE;
    OP* retval = scalarseq(seq);
    OP *o;

    /* XXX Is the null PL_parser check necessary here? */
    assert(PL_parser); /* Let’s find out under debugging builds.  */
    if (PL_parser && PL_parser->parsed_sub) {
	o = newSTATEOP(0, NULL, NULL);
	op_null(o);
	retval = op_append_elem(OP_LINESEQ, retval, o);
    }

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
	for (;; kid = OpSIBLING(kid)) {
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

void
Perl_newPROG(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_NEWPROG;

    if (PL_in_eval) {
	PERL_CONTEXT *cx;
	I32 i;
	if (PL_eval_root)
		return;
	PL_eval_root = newUNOP(OP_LEAVEEVAL,
			       ((PL_in_eval & EVAL_KEEPERR)
				? OPf_SPECIAL : 0), o);

	cx = CX_CUR();
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
        S_prune_chain_head(&PL_eval_start);
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
        S_prune_chain_head(&PL_main_start);
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
		if (*s && (strchr("@$%", *s) || (!lex && *s == '*'))
		       && *++s
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
	o = op_convert_list(OP_JOIN, 0, op_prepend_elem(OP_LIST, o2, o));
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
    bool is_stringify;
    SV * VOL sv = NULL;
    int ret = 0;
    OP *old_next;
    SV * const oldwarnhook = PL_warnhook;
    SV * const olddiehook  = PL_diehook;
    COP not_compiling;
    U8 oldwarn = PL_dowarn;
    I32 old_cxix;
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
#ifdef USE_LOCALE_CTYPE
	if (IN_LC_COMPILETIME(LC_CTYPE))
	    goto nope;
#endif
        break;
    case OP_SLT:
    case OP_SGT:
    case OP_SLE:
    case OP_SGE:
    case OP_SCMP:
#ifdef USE_LOCALE_COLLATE
	if (IN_LC_COMPILETIME(LC_COLLATE))
	    goto nope;
#endif
        break;
    case OP_SPRINTF:
	/* XXX what about the numeric ops? */
#ifdef USE_LOCALE_NUMERIC
	if (IN_LC_COMPILETIME(LC_NUMERIC))
	    goto nope;
#endif
	break;
    case OP_PACK:
	if (!OpHAS_SIBLING(cLISTOPo->op_first)
	  || OpSIBLING(cLISTOPo->op_first)->op_type != OP_CONST)
	    goto nope;
	{
	    SV * const sv = cSVOPx_sv(OpSIBLING(cLISTOPo->op_first));
	    if (!SvPOK(sv) || SvGMAGICAL(sv)) goto nope;
	    {
		const char *s = SvPVX_const(sv);
		while (s < SvEND(sv)) {
		    if (isALPHA_FOLD_EQ(*s, 'p')) goto nope;
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

    old_cxix = cxstack_ix;
    create_eval_scope(NULL, G_FAKINGEVAL);

    /* Verify that we don't need to save it:  */
    assert(PL_curcop == &PL_compiling);
    StructCopy(&PL_compiling, &not_compiling, COP);
    PL_curcop = &not_compiling;
    /* The above ensures that we run with all the correct hints of the
       currently compiling COP, but that IN_PERL_RUNTIME is true. */
    assert(IN_PERL_RUNTIME);
    PL_warnhook = PERL_WARNHOOK_FATAL;
    PL_diehook  = NULL;
    JMPENV_PUSH(ret);

    /* Effective $^W=1.  */
    if ( ! (PL_dowarn & G_WARN_ALL_MASK))
	PL_dowarn |= G_WARN_ON;

    switch (ret) {
    case 0:
	CALLRUNOPS(aTHX);
	sv = *(PL_stack_sp--);
	if (o->op_targ && sv == PAD_SV(o->op_targ)) {	/* grab pad temp? */
	    pad_swipe(o->op_targ,  FALSE);
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
    PL_dowarn   = oldwarn;
    PL_warnhook = oldwarnhook;
    PL_diehook  = olddiehook;
    PL_curcop = &PL_compiling;

    /* if we croaked, depending on how we croaked the eval scope
     * may or may not have already been popped */
    if (cxstack_ix > old_cxix) {
        assert(cxstack_ix == old_cxix + 1);
        assert(CxTYPE(CX_CUR()) == CXt_EVAL);
        delete_eval_scope();
    }
    if (ret)
	goto nope;

    /* OP_STRINGIFY and constant folding are used to implement qq.
       Here the constant folding is an implementation detail that we
       want to hide.  If the stringify op is itself already marked
       folded, however, then it is actually a folded join.  */
    is_stringify = type == OP_STRINGIFY && !o->op_folded;
    op_free(o);
    assert(sv);
    if (is_stringify)
	SvPADTMP_off(sv);
    else if (!SvIMMORTAL(sv)) {
	SvPADTMP_on(sv);
	SvREADONLY_on(sv);
    }
    newop = newSVOP(OP_CONST, 0, MUTABLE_SV(sv));
    if (!is_stringify) newop->op_folded = 1;
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
    S_prune_chain_head(&curop);
    PL_op = curop;
    Perl_pp_pushmark(aTHX);
    CALLRUNOPS(aTHX);
    PL_op = curop;
    assert (!(curop->op_flags & OPf_SPECIAL));
    assert(curop->op_type == OP_RANGE);
    Perl_pp_anonlist(aTHX);
    PL_tmps_floor = oldtmps_floor;

    OpTYPE_set(o, OP_RV2AV);
    o->op_flags &= ~OPf_REF;	/* treat \(1..2) like an ordinary list */
    o->op_flags |= OPf_PARENS;	/* and flatten \(1..2,3) */
    o->op_opt = 0;		/* needs to be revisited in rpeep() */
    av = (AV *)SvREFCNT_inc_NN(*PL_stack_sp--);

    /* replace subtree with an OP_CONST */
    curop = ((UNOP*)o)->op_first;
    op_sibling_splice(o, NULL, -1, newSVOP(OP_CONST, 0, (SV *)av));
    op_free(curop);

    if (AvFILLp(av) != -1)
	for (svp = AvARRAY(av) + AvFILLp(av); svp >= AvARRAY(av); --svp)
	{
	    SvPADTMP_on(*svp);
	    SvREADONLY_on(*svp);
	}
    LINKLIST(o);
    return list(o);
}

/*
=head1 Optree Manipulation Functions
*/

/* List constructors */

/*
=for apidoc Am|OP *|op_append_elem|I32 optype|OP *first|OP *last

Append an item to the list of ops contained directly within a list-type
op, returning the lengthened list.  C<first> is the list-type op,
and C<last> is the op to append to the list.  C<optype> specifies the
intended opcode for the list.  If C<first> is not already a list of the
right type, it will be upgraded into one.  If either C<first> or C<last>
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

    op_sibling_splice(first, ((LISTOP*)first)->op_last, 0, last);
    first->op_flags |= OPf_KIDS;
    return first;
}

/*
=for apidoc Am|OP *|op_append_list|I32 optype|OP *first|OP *last

Concatenate the lists of ops contained directly within two list-type ops,
returning the combined list.  C<first> and C<last> are the list-type ops
to concatenate.  C<optype> specifies the intended opcode for the list.
If either C<first> or C<last> is not already a list of the right type,
it will be upgraded into one.  If either C<first> or C<last> is null,
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

    OpMORESIB_set(((LISTOP*)first)->op_last, ((LISTOP*)last)->op_first);
    ((LISTOP*)first)->op_last = ((LISTOP*)last)->op_last;
    OpLASTSIB_set(((LISTOP*)first)->op_last, first);
    first->op_flags |= (last->op_flags & OPf_KIDS);

    S_op_destroy(aTHX_ last);

    return first;
}

/*
=for apidoc Am|OP *|op_prepend_elem|I32 optype|OP *first|OP *last

Prepend an item to the list of ops contained directly within a list-type
op, returning the lengthened list.  C<first> is the op to prepend to the
list, and C<last> is the list-type op.  C<optype> specifies the intended
opcode for the list.  If C<last> is not already a list of the right type,
it will be upgraded into one.  If either C<first> or C<last> is null,
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
            /* insert 'first' after pushmark */
            op_sibling_splice(last, cLISTOPx(last)->op_first, 0, first);
            if (!(first->op_flags & OPf_PARENS))
                last->op_flags &= ~OPf_PARENS;
	}
	else
            op_sibling_splice(last, NULL, 0, first);
	last->op_flags |= OPf_KIDS;
	return last;
    }

    return newLISTOP(type, 0, first, last);
}

/*
=for apidoc Am|OP *|op_convert_list|I32 type|I32 flags|OP *o

Converts C<o> into a list op if it is not one already, and then converts it
into the specified C<type>, calling its check function, allocating a target if
it needs one, and folding constants.

A list-type op is usually constructed one kid at a time via C<newLISTOP>,
C<op_prepend_elem> and C<op_append_elem>.  Then finally it is passed to
C<op_convert_list> to make it the right type.

=cut
*/

OP *
Perl_op_convert_list(pTHX_ I32 type, I32 flags, OP *o)
{
    dVAR;
    if (type < 0) type = -type, flags |= OPf_SPECIAL;
    if (!o || o->op_type != OP_LIST)
        o = force_list(o, 0);
    else
    {
	o->op_flags &= ~OPf_WANT;
	o->op_private &= ~OPpLVAL_INTRO;
    }

    if (!(PL_opargs[type] & OA_MARK))
	op_null(cLISTOPo->op_first);
    else {
	OP * const kid2 = OpSIBLING(cLISTOPo->op_first);
	if (kid2 && kid2->op_type == OP_COREARGS) {
	    op_null(cLISTOPo->op_first);
	    kid2->op_private |= OPpCOREARGS_PUSHMARK;
	}
    }

    OpTYPE_set(o, type);
    o->op_flags |= flags;
    if (flags & OPf_FOLDED)
	o->op_folded = 1;

    o = CHECKOP(type, o);
    if (o->op_type != (unsigned)type)
	return o;

    return fold_constants(op_integerize(op_std_init(o)));
}

/* Constructors */


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

/* promote o and any siblings to be a list if its not already; i.e.
 *
 *  o - A - B
 *
 * becomes
 *
 *  list
 *    |
 *  pushmark - o - A - B
 *
 * If nullit it true, the list op is nulled.
 */

static OP *
S_force_list(pTHX_ OP *o, bool nullit)
{
    if (!o || o->op_type != OP_LIST) {
        OP *rest = NULL;
        if (o) {
            /* manually detach any siblings then add them back later */
            rest = OpSIBLING(o);
            OpLASTSIB_set(o, NULL);
        }
	o = newLISTOP(OP_LIST, 0, o, NULL);
        if (rest)
            op_sibling_splice(o, cLISTOPo->op_last, 0, rest);
    }
    if (nullit)
        op_null(o);
    return o;
}

/*
=for apidoc Am|OP *|newLISTOP|I32 type|I32 flags|OP *first|OP *last

Constructs, checks, and returns an op of any list type.  C<type> is
the opcode.  C<flags> gives the eight bits of C<op_flags>, except that
C<OPf_KIDS> will be set automatically if required.  C<first> and C<last>
supply up to two ops to be direct children of the list op; they are
consumed by this function and become part of the constructed op tree.

For most list operators, the check function expects all the kid ops to be
present already, so calling C<newLISTOP(OP_JOIN, ...)> (e.g.) is not
appropriate.  What you want to do in that case is create an op of type
C<OP_LIST>, append more children to it, and then call L</op_convert_list>.
See L</op_convert_list> for more information.


=cut
*/

OP *
Perl_newLISTOP(pTHX_ I32 type, I32 flags, OP *first, OP *last)
{
    dVAR;
    LISTOP *listop;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_LISTOP
	|| type == OP_CUSTOM);

    NewOp(1101, listop, 1, LISTOP);

    OpTYPE_set(listop, type);
    if (first || last)
	flags |= OPf_KIDS;
    listop->op_flags = (U8)flags;

    if (!last && first)
	last = first;
    else if (!first && last)
	first = last;
    else if (first)
	OpMORESIB_set(first, last);
    listop->op_first = first;
    listop->op_last = last;
    if (type == OP_LIST) {
	OP* const pushop = newOP(OP_PUSHMARK, 0);
	OpMORESIB_set(pushop, first);
	listop->op_first = pushop;
	listop->op_flags |= OPf_KIDS;
	if (!last)
	    listop->op_last = pushop;
    }
    if (listop->op_last)
        OpLASTSIB_set(listop->op_last, (OP*)listop);

    return CHECKOP(type, listop);
}

/*
=for apidoc Am|OP *|newOP|I32 type|I32 flags

Constructs, checks, and returns an op of any base type (any type that
has no extra fields).  C<type> is the opcode.  C<flags> gives the
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
    OpTYPE_set(o, type);
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

Constructs, checks, and returns an op of any unary type.  C<type> is
the opcode.  C<flags> gives the eight bits of C<op_flags>, except that
C<OPf_KIDS> will be set automatically if required, and, shifted up eight
bits, the eight bits of C<op_private>, except that the bit with value 1
is automatically set.  C<first> supplies an optional op to be the direct
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
	|| type == OP_CUSTOM
	|| type == OP_NULL );

    if (!first)
	first = newOP(OP_STUB, 0);
    if (PL_opargs[type] & OA_MARK)
	first = force_list(first, 1);

    NewOp(1101, unop, 1, UNOP);
    OpTYPE_set(unop, type);
    unop->op_first = first;
    unop->op_flags = (U8)(flags | OPf_KIDS);
    unop->op_private = (U8)(1 | (flags >> 8));

    if (!OpHAS_SIBLING(first)) /* true unless weird syntax error */
        OpLASTSIB_set(first, (OP*)unop);

    unop = (UNOP*) CHECKOP(type, unop);
    if (unop->op_next)
	return (OP*)unop;

    return fold_constants(op_integerize(op_std_init((OP *) unop)));
}

/*
=for apidoc newUNOP_AUX

Similar to C<newUNOP>, but creates an C<UNOP_AUX> struct instead, with C<op_aux>
initialised to C<aux>

=cut
*/

OP *
Perl_newUNOP_AUX(pTHX_ I32 type, I32 flags, OP *first, UNOP_AUX_item *aux)
{
    dVAR;
    UNOP_AUX *unop;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_UNOP_AUX
        || type == OP_CUSTOM);

    NewOp(1101, unop, 1, UNOP_AUX);
    unop->op_type = (OPCODE)type;
    unop->op_ppaddr = PL_ppaddr[type];
    unop->op_first = first;
    unop->op_flags = (U8)(flags | (first ? OPf_KIDS : 0));
    unop->op_private = (U8)((first ? 1 : 0) | (flags >> 8));
    unop->op_aux = aux;

    if (first && !OpHAS_SIBLING(first)) /* true unless weird syntax error */
        OpLASTSIB_set(first, (OP*)unop);

    unop = (UNOP_AUX*) CHECKOP(type, unop);

    return op_std_init((OP *) unop);
}

/*
=for apidoc Am|OP *|newMETHOP|I32 type|I32 flags|OP *first

Constructs, checks, and returns an op of method type with a method name
evaluated at runtime.  C<type> is the opcode.  C<flags> gives the eight
bits of C<op_flags>, except that C<OPf_KIDS> will be set automatically,
and, shifted up eight bits, the eight bits of C<op_private>, except that
the bit with value 1 is automatically set.  C<dynamic_meth> supplies an
op which evaluates method name; it is consumed by this function and
become part of the constructed op tree.
Supported optypes: C<OP_METHOD>.

=cut
*/

static OP*
S_newMETHOP_internal(pTHX_ I32 type, I32 flags, OP* dynamic_meth, SV* const_meth) {
    dVAR;
    METHOP *methop;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_METHOP
        || type == OP_CUSTOM);

    NewOp(1101, methop, 1, METHOP);
    if (dynamic_meth) {
        if (PL_opargs[type] & OA_MARK) dynamic_meth = force_list(dynamic_meth, 1);
        methop->op_flags = (U8)(flags | OPf_KIDS);
        methop->op_u.op_first = dynamic_meth;
        methop->op_private = (U8)(1 | (flags >> 8));

        if (!OpHAS_SIBLING(dynamic_meth))
            OpLASTSIB_set(dynamic_meth, (OP*)methop);
    }
    else {
        assert(const_meth);
        methop->op_flags = (U8)(flags & ~OPf_KIDS);
        methop->op_u.op_meth_sv = const_meth;
        methop->op_private = (U8)(0 | (flags >> 8));
        methop->op_next = (OP*)methop;
    }

#ifdef USE_ITHREADS
    methop->op_rclass_targ = 0;
#else
    methop->op_rclass_sv = NULL;
#endif

    OpTYPE_set(methop, type);
    return CHECKOP(type, methop);
}

OP *
Perl_newMETHOP (pTHX_ I32 type, I32 flags, OP* dynamic_meth) {
    PERL_ARGS_ASSERT_NEWMETHOP;
    return newMETHOP_internal(type, flags, dynamic_meth, NULL);
}

/*
=for apidoc Am|OP *|newMETHOP_named|I32 type|I32 flags|SV *const_meth

Constructs, checks, and returns an op of method type with a constant
method name.  C<type> is the opcode.  C<flags> gives the eight bits of
C<op_flags>, and, shifted up eight bits, the eight bits of
C<op_private>.  C<const_meth> supplies a constant method name;
it must be a shared COW string.
Supported optypes: C<OP_METHOD_NAMED>.

=cut
*/

OP *
Perl_newMETHOP_named (pTHX_ I32 type, I32 flags, SV* const_meth) {
    PERL_ARGS_ASSERT_NEWMETHOP_NAMED;
    return newMETHOP_internal(type, flags, NULL, const_meth);
}

/*
=for apidoc Am|OP *|newBINOP|I32 type|I32 flags|OP *first|OP *last

Constructs, checks, and returns an op of any binary type.  C<type>
is the opcode.  C<flags> gives the eight bits of C<op_flags>, except
that C<OPf_KIDS> will be set automatically, and, shifted up eight bits,
the eight bits of C<op_private>, except that the bit with value 1 or
2 is automatically set as required.  C<first> and C<last> supply up to
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
	|| type == OP_SASSIGN || type == OP_NULL || type == OP_CUSTOM);

    NewOp(1101, binop, 1, BINOP);

    if (!first)
	first = newOP(OP_NULL, 0);

    OpTYPE_set(binop, type);
    binop->op_first = first;
    binop->op_flags = (U8)(flags | OPf_KIDS);
    if (!last) {
	last = first;
	binop->op_private = (U8)(1 | (flags >> 8));
    }
    else {
	binop->op_private = (U8)(2 | (flags >> 8));
        OpMORESIB_set(first, last);
    }

    if (!OpHAS_SIBLING(last)) /* true unless weird syntax error */
        OpLASTSIB_set(last, (OP*)binop);

    binop->op_last = OpSIBLING(binop->op_first);
    if (binop->op_last)
        OpLASTSIB_set(binop->op_last, (OP*)binop);

    binop = (BINOP*)CHECKOP(type, binop);
    if (binop->op_next || binop->op_type != (OPCODE)type)
	return (OP*)binop;

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
    SV * const tstr = ((SVOP*)expr)->op_sv;
    SV * const rstr =
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
	STRLEN tcount = 0;
	UV rfirst = 1;
	UV rlast = 0;
	IV rdiff;
	STRLEN rcount = 0;
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

/* There is a snag with this code on EBCDIC: scan_const() in toke.c has
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

	    /* now see which range will peter out first, if either. */
	    tdiff = tlast - tfirst;
	    rdiff = rlast - rfirst;
	    tcount += tdiff + 1;
	    rcount += rdiff + 1;

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
			     UVCHR_SKIP(tfirst) < UVCHR_SKIP(rfirst + diff));
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

	Safefree(tsave);
	Safefree(rsave);

	tlen = tcount;
	rlen = rcount;
	if (r < rend)
	    rlen++;
	else if (rlast == 0xffffffff)
	    rlen = 0;

	goto warnins;
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

  warnins:
    if(del && rlen == tlen) {
	Perl_ck_warner(aTHX_ packWARN(WARN_MISC), "Useless use of /d modifier in transliteration operator"); 
    } else if(rlen > tlen && !complement) {
	Perl_ck_warner(aTHX_ packWARN(WARN_MISC), "Replacement list is longer than search list");
    }

    if (grows)
	o->op_private |= OPpTRANS_GROWS;
    op_free(expr);
    op_free(repl);

    return o;
}

/*
=for apidoc Am|OP *|newPMOP|I32 type|I32 flags

Constructs, checks, and returns an op of any pattern matching type.
C<type> is the opcode.  C<flags> gives the eight bits of C<op_flags>
and, shifted up eight bits, the eight bits of C<op_private>.

=cut
*/

OP *
Perl_newPMOP(pTHX_ I32 type, I32 flags)
{
    dVAR;
    PMOP *pmop;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_PMOP
	|| type == OP_CUSTOM);

    NewOp(1101, pmop, 1, PMOP);
    OpTYPE_set(pmop, type);
    pmop->op_flags = (U8)flags;
    pmop->op_private = (U8)(0 | (flags >> 8));
    if (PL_opargs[type] & OA_RETSCALAR)
	scalar((OP *)pmop);

    if (PL_hints & HINT_RE_TAINT)
	pmop->op_pmflags |= PMf_RETAINT;
#ifdef USE_LOCALE_CTYPE
    if (IN_LC_COMPILETIME(LC_CTYPE)) {
	set_regex_charset(&(pmop->op_pmflags), REGEX_LOCALE_CHARSET);
    }
    else
#endif
         if (IN_UNI_8_BIT) {
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

static void
S_set_haseval(pTHX)
{
    PADOFFSET i = 1;
    PL_cv_has_eval = 1;
    /* Any pad names in scope are potentially lvalues.  */
    for (; i < PadnamelistMAXNAMED(PL_comppad_name); i++) {
	PADNAME *pn = PAD_COMPNAME_SV(i);
	if (!pn || !PadnameLEN(pn))
	    continue;
	if (PadnameOUTER(pn) || PadnameIN_SCOPE(pn, PL_cop_seqmax))
	    S_mark_padname_lvalue(aTHX_ pn);
    }
}

/* Given some sort of match op o, and an expression expr containing a
 * pattern, either compile expr into a regex and attach it to o (if it's
 * constant), or convert expr into a runtime regcomp op sequence (if it's
 * not)
 *
 * isreg indicates that the pattern is part of a regex construct, eg
 * $x =~ /pattern/ or split /pattern/, as opposed to $x =~ $pattern or
 * split "pattern", which aren't. In the former case, expr will be a list
 * if the pattern contains more than one term (eg /a$b/).
 *
 * When the pattern has been compiled within a new anon CV (for
 * qr/(?{...})/ ), then floor indicates the savestack level just before
 * the new sub was created
 */

OP *
Perl_pmruntime(pTHX_ OP *o, OP *expr, OP *repl, bool isreg, I32 floor)
{
    PMOP *pm;
    LOGOP *rcop;
    I32 repl_has_vars = 0;
    bool is_trans = (o->op_type == OP_TRANS || o->op_type == OP_TRANSR);
    bool is_compiletime;
    bool has_code;

    PERL_ARGS_ASSERT_PMRUNTIME;

    if (is_trans) {
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
	for (o = cLISTOPx(expr)->op_first; o; o = OpSIBLING(o)) {
	    if (o->op_type == OP_NULL && (o->op_flags & OPf_SPECIAL)) {
		has_code = 1;
		assert(!o->op_next);
		if (UNLIKELY(!OpHAS_SIBLING(o))) {
		    assert(PL_parser && PL_parser->error_count);
		    /* This can happen with qr/ (?{(^{})/.  Just fake up
		       the op we were expecting to see, to avoid crashing
		       elsewhere.  */
		    op_sibling_splice(expr, o, 0,
				      newSVOP(OP_CONST, 0, &PL_sv_no));
		}
		o->op_next = OpSIBLING(o);
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
	for (o = cLISTOPx(expr)->op_first; o; o = OpSIBLING(o)) {

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
		assert(OpHAS_SIBLING(leaveop->op_first));
		o->op_next = OpSIBLING(leaveop->op_first);
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
            S_prune_chain_head(&(o->op_next));
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
		SSize_t i = 0;
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
	    op_free(expr);
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
	     *                         anoncode
	     *                         srefgen
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

	    expr = list(force_list(newUNOP(OP_ENTERSUB, 0, scalar(expr)), 1));
	}

        rcop = S_alloc_LOGOP(aTHX_ OP_REGCOMP, scalar(expr), o);
	rcop->op_flags |=  ((PL_hints & HINT_RE_EVAL) ? OPf_SPECIAL : 0)
			   | (reglist ? OPf_STACKED : 0);
	rcop->op_targ = cv_targ;

	/* /$x/ may cause an eval, since $x might be qr/(?{..})/  */
	if (PL_hints & HINT_RE_EVAL)
	    S_set_haseval(aTHX);

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
             && cUNOPx(curop)->op_first->op_flags & OPf_KIDS)
         {
            OP *sib;
	    OP *kid = cUNOPx(cUNOPx(curop)->op_first)->op_first;
	    if (kid->op_type == OP_NULL && (sib = OpSIBLING(kid))
	     && !OpHAS_SIBLING(sib))
		curop = sib;
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
            rcop = S_alloc_LOGOP(aTHX_ OP_SUBSTCONT, scalar(repl), o);
	    rcop->op_private = 1;

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
embedded SV.  C<type> is the opcode.  C<flags> gives the eight bits
of C<op_flags>.  C<sv> gives the SV to embed in the op; this function
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
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_FILESTATOP
	|| type == OP_CUSTOM);

    NewOp(1101, svop, 1, SVOP);
    OpTYPE_set(svop, type);
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

/*
=for apidoc Am|OP *|newDEFSVOP|

Constructs and returns an op to access C<$_>.

=cut
*/

OP *
Perl_newDEFSVOP(pTHX)
{
	return newSVREF(newGVOP(OP_GV, 0, PL_defgv));
}

#ifdef USE_ITHREADS

/*
=for apidoc Am|OP *|newPADOP|I32 type|I32 flags|SV *sv

Constructs, checks, and returns an op of any type that involves a
reference to a pad element.  C<type> is the opcode.  C<flags> gives the
eight bits of C<op_flags>.  A pad slot is automatically allocated, and
is populated with C<sv>; this function takes ownership of one reference
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
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_FILESTATOP
	|| type == OP_CUSTOM);

    NewOp(1101, padop, 1, PADOP);
    OpTYPE_set(padop, type);
    padop->op_padix =
	pad_alloc(type, isGV(sv) ? SVf_READONLY : SVs_PADTMP);
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
embedded reference to a GV.  C<type> is the opcode.  C<flags> gives the
eight bits of C<op_flags>.  C<gv> identifies the GV that the op should
reference; calling this function does not transfer ownership of any
reference to it.

=cut
*/

OP *
Perl_newGVOP(pTHX_ I32 type, I32 flags, GV *gv)
{
    PERL_ARGS_ASSERT_NEWGVOP;

#ifdef USE_ITHREADS
    return newPADOP(type, flags, SvREFCNT_inc_simple_NN(gv));
#else
    return newSVOP(type, flags, SvREFCNT_inc_simple_NN(gv));
#endif
}

/*
=for apidoc Am|OP *|newPVOP|I32 type|I32 flags|char *pv

Constructs, checks, and returns an op of any type that involves an
embedded C-level pointer (PV).  C<type> is the opcode.  C<flags> gives
the eight bits of C<op_flags>.  C<pv> supplies the C-level pointer, which
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
	|| type == OP_RUNCV || type == OP_CUSTOM
	|| (PL_opargs[type] & OA_CLASS_MASK) == OA_LOOPEXOP);

    NewOp(1101, pvop, 1, PVOP);
    OpTYPE_set(pvop, type);
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

void
Perl_package(pTHX_ OP *o)
{
    SV *const sv = cSVOPo->op_sv;

    PERL_ARGS_ASSERT_PACKAGE;

    SAVEGENERICSV(PL_curstash);
    save_item(PL_curstname);

    PL_curstash = (HV *)SvREFCNT_inc(gv_stashsv(sv, GV_ADD));

    sv_setsv(PL_curstname, sv);

    PL_hints |= HINT_BLOCK_SCOPE;
    PL_parser->copline = NOLINE;

    op_free(o);
}

void
Perl_package_version( pTHX_ OP *v )
{
    U32 savehints = PL_hints;
    PERL_ARGS_ASSERT_PACKAGE_VERSION;
    PL_hints &= ~HINT_STRICT_VARS;
    sv_setsv( GvSV(gv_fetchpvs("VERSION", GV_ADDMULTI, SVt_PV)), cSVOPx(v)->op_sv );
    PL_hints = savehints;
    op_free(v);
}

void
Perl_utilize(pTHX_ int aver, I32 floor, OP *version, OP *idop, OP *arg)
{
    OP *pack;
    OP *imop;
    OP *veop;
    SV *use_version = NULL;

    PERL_ARGS_ASSERT_UTILIZE;

    if (idop->op_type != OP_CONST)
	Perl_croak(aTHX_ "Module name must be constant");

    veop = NULL;

    if (version) {
	SV * const vesv = ((SVOP*)version)->op_sv;

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
	    veop = op_convert_list(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
			    op_append_elem(OP_LIST,
					op_prepend_elem(OP_LIST, pack, version),
					newMETHOP_named(OP_METHOD_NAMED, 0, meth)));
	}
    }

    /* Fake up an import/unimport */
    if (arg && arg->op_type == OP_STUB) {
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

	/* Make copy of idop so we don't free it twice */
	pack = newSVOP(OP_CONST, 0, newSVsv(((SVOP*)idop)->op_sv));

	/* Fake up a method call to import/unimport */
	meth = aver
	    ? newSVpvs_share("import") : newSVpvs_share("unimport");
	imop = op_convert_list(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
		       op_append_elem(OP_LIST,
				   op_prepend_elem(OP_LIST, pack, arg),
				   newMETHOP_named(OP_METHOD_NAMED, 0, meth)
		       ));
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
    COP_SEQMAX_INC; /* Purely for B::*'s benefit */
}

/*
=head1 Embedding Functions

=for apidoc load_module

Loads the module whose name is pointed to by the string part of name.
Note that the actual module name, not its filename, should be given.
Eg, "Foo::Bar" instead of "Foo/Bar.pm".  flags can be any of
C<PERL_LOADMOD_DENY>, C<PERL_LOADMOD_NOIMPORT>, or C<PERL_LOADMOD_IMPORT_OPS>
(or 0 for no flags).  ver, if specified
and not NULL, provides version semantics
similar to C<use Foo::Bar VERSION>.  The optional trailing SV*
arguments can be used to specify arguments to the module's C<import()>
method, similar to C<use Foo::Bar VERSION LIST>.  They must be
terminated with a final C<NULL> pointer.  Note that this list can only
be omitted when the C<PERL_LOADMOD_NOIMPORT> flag has been used.
Otherwise at least a single C<NULL> pointer to designate the default
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

Constructs, checks, and returns an C<lslice> (list slice) op.  C<flags>
gives the eight bits of C<op_flags>, except that C<OPf_KIDS> will
be set automatically, and, shifted up eight bits, the eight bits of
C<op_private>, except that the bit with value 1 or 2 is automatically
set as required.  C<listval> and C<subscript> supply the parameters of
the slice; they are consumed by this function and become part of the
constructed op tree.

=cut
*/

OP *
Perl_newSLICEOP(pTHX_ I32 flags, OP *subscript, OP *listval)
{
    return newBINOP(OP_LSLICE, flags,
	    list(force_list(subscript, 1)),
	    list(force_list(listval,   1)) );
}

#define ASSIGN_LIST   1
#define ASSIGN_REF    2

STATIC I32
S_assignment_type(pTHX_ const OP *o)
{
    unsigned type;
    U8 flags;
    U8 ret;

    if (!o)
	return TRUE;

    if ((o->op_type == OP_NULL) && (o->op_flags & OPf_KIDS))
	o = cUNOPo->op_first;

    flags = o->op_flags;
    type = o->op_type;
    if (type == OP_COND_EXPR) {
        OP * const sib = OpSIBLING(cLOGOPo->op_first);
        const I32 t = assignment_type(sib);
        const I32 f = assignment_type(OpSIBLING(sib));

	if (t == ASSIGN_LIST && f == ASSIGN_LIST)
	    return ASSIGN_LIST;
	if ((t == ASSIGN_LIST) ^ (f == ASSIGN_LIST))
	    yyerror("Assignment to both a list and a scalar");
	return FALSE;
    }

    if (type == OP_SREFGEN)
    {
	OP * const kid = cUNOPx(cUNOPo->op_first)->op_first;
	type = kid->op_type;
	flags |= kid->op_flags;
	if (!(flags & OPf_PARENS)
	  && (kid->op_type == OP_RV2AV || kid->op_type == OP_PADAV ||
	      kid->op_type == OP_RV2HV || kid->op_type == OP_PADHV ))
	    return ASSIGN_REF;
	ret = ASSIGN_REF;
    }
    else ret = 0;

    if (type == OP_LIST &&
	(flags & OPf_WANT) == OPf_WANT_SCALAR &&
	o->op_private & OPpLVAL_INTRO)
	return ret;

    if (type == OP_LIST || flags & OPf_PARENS ||
	type == OP_RV2AV || type == OP_RV2HV ||
	type == OP_ASLICE || type == OP_HSLICE ||
        type == OP_KVASLICE || type == OP_KVHSLICE || type == OP_REFGEN)
	return TRUE;

    if (type == OP_PADAV || type == OP_PADHV)
	return TRUE;

    if (type == OP_RV2SV)
	return ret;

    return ret;
}


/*
=for apidoc Am|OP *|newASSIGNOP|I32 flags|OP *left|I32 optype|OP *right

Constructs, checks, and returns an assignment op.  C<left> and C<right>
supply the parameters of the assignment; they are consumed by this
function and become part of the constructed op tree.

If C<optype> is C<OP_ANDASSIGN>, C<OP_ORASSIGN>, or C<OP_DORASSIGN>, then
a suitable conditional optree is constructed.  If C<optype> is the opcode
of a binary operator, such as C<OP_BIT_OR>, then an op is constructed that
performs the binary operation and assigns the result to the left argument.
Either way, if C<optype> is non-zero then C<flags> has no effect.

If C<optype> is zero, then a plain scalar or list assignment is
constructed.  Which type of assignment it is is automatically determined.
C<flags> gives the eight bits of C<op_flags>, except that C<OPf_KIDS>
will be set automatically, and, shifted up eight bits, the eight bits
of C<op_private>, except that the bit with value 1 or 2 is automatically
set as required.

=cut
*/

OP *
Perl_newASSIGNOP(pTHX_ I32 flags, OP *left, I32 optype, OP *right)
{
    OP *o;
    I32 assign_type;

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

    if ((assign_type = assignment_type(left)) == ASSIGN_LIST) {
	static const char no_list_state[] = "Initialization of state variables"
	    " in list context currently forbidden";
	OP *curop;

	if (left->op_type == OP_ASLICE || left->op_type == OP_HSLICE)
	    left->op_private &= ~ OPpSLICEWARNING;

	PL_modcount = 0;
	left = op_lvalue(left, OP_AASSIGN);
	curop = list(force_list(left, 1));
	o = newBINOP(OP_AASSIGN, flags, list(force_list(right, 1)), curop);
	o->op_private = (U8)(0 | (flags >> 8));

	if (OP_TYPE_IS_OR_WAS(left, OP_LIST))
	{
	    OP* lop = ((LISTOP*)left)->op_first;
	    while (lop) {
		if ((lop->op_type == OP_PADSV ||
		     lop->op_type == OP_PADAV ||
		     lop->op_type == OP_PADHV ||
		     lop->op_type == OP_PADANY)
		  && (lop->op_private & OPpPAD_STATE)
                )
                    yyerror(no_list_state);
		lop = OpSIBLING(lop);
	    }
	}
	else if (  (left->op_private & OPpLVAL_INTRO)
                && (left->op_private & OPpPAD_STATE)
		&& (   left->op_type == OP_PADSV
		    || left->op_type == OP_PADAV
		    || left->op_type == OP_PADHV
		    || left->op_type == OP_PADANY)
        ) {
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

	if (right && right->op_type == OP_SPLIT
	 && !(right->op_flags & OPf_STACKED)) {
	    OP* tmpop = ((LISTOP*)right)->op_first;
	    PMOP * const pm = (PMOP*)tmpop;
	    assert (tmpop && (tmpop->op_type == OP_PUSHRE));
	    if (
#ifdef USE_ITHREADS
		    !pm->op_pmreplrootu.op_pmtargetoff
#else
		    !pm->op_pmreplrootu.op_pmtargetgv
#endif
		 && !pm->op_targ
		) {
		    if (!(left->op_private & OPpLVAL_INTRO) &&
		        ( (left->op_type == OP_RV2AV &&
			  (tmpop=((UNOP*)left)->op_first)->op_type==OP_GV)
		        || left->op_type == OP_PADAV )
			) {
			if (tmpop != (OP *)pm) {
#ifdef USE_ITHREADS
			  pm->op_pmreplrootu.op_pmtargetoff
			    = cPADOPx(tmpop)->op_padix;
			  cPADOPx(tmpop)->op_padix = 0;	/* steal it */
#else
			  pm->op_pmreplrootu.op_pmtargetgv
			    = MUTABLE_GV(cSVOPx(tmpop)->op_sv);
			  cSVOPx(tmpop)->op_sv = NULL;	/* steal it */
#endif
			  right->op_private |=
			    left->op_private & OPpOUR_INTRO;
			}
			else {
			    pm->op_targ = left->op_targ;
			    left->op_targ = 0; /* filch it */
			}
		      detach_split:
			tmpop = cUNOPo->op_first;	/* to list (nulled) */
			tmpop = ((UNOP*)tmpop)->op_first; /* to pushmark */
                        /* detach rest of siblings from o subtree,
                         * and free subtree */
                        op_sibling_splice(cUNOPo->op_first, tmpop, -1, NULL);
			op_free(o);			/* blow off assign */
			right->op_flags &= ~OPf_WANT;
				/* "I don't know and I don't care." */
			return right;
		    }
		    else if (left->op_type == OP_RV2AV
			  || left->op_type == OP_PADAV)
		    {
			/* Detach the array.  */
#ifdef DEBUGGING
			OP * const ary =
#endif
			op_sibling_splice(cBINOPo->op_last,
					  cUNOPx(cBINOPo->op_last)
						->op_first, 1, NULL);
			assert(ary == left);
			/* Attach it to the split.  */
			op_sibling_splice(right, cLISTOPx(right)->op_last,
					  0, left);
			right->op_flags |= OPf_STACKED;
			/* Detach split and expunge aassign as above.  */
			goto detach_split;
		    }
		    else if (PL_modcount < RETURN_UNLIMITED_NUMBER &&
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
	return o;
    }
    if (assign_type == ASSIGN_REF)
	return newBINOP(OP_REFASSIGN, flags, scalar(right), left);
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
If C<label> is non-null, it supplies the name of a label to attach to
the state op; this function takes ownership of the memory pointed at by
C<label>, and will free it.  C<flags> gives the eight bits of C<op_flags>
for the state op.

If C<o> is null, the state op is returned.  Otherwise the state op is
combined with C<o> into a C<lineseq> list op, which is returned.  C<o>
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

    PL_parser->parsed_sub = 0;

    flags &= ~SVf_UTF8;

    NewOp(1101, cop, 1, COP);
    if (PERLDB_LINE && CopLINE(PL_curcop) && PL_curstash != PL_debstash) {
        OpTYPE_set(cop, OP_DBSTATE);
    }
    else {
        OpTYPE_set(cop, OP_NEXTSTATE);
    }
    cop->op_flags = (U8)flags;
    CopHINTS_set(cop, PL_hints);
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

Constructs, checks, and returns a logical (flow control) op.  C<type>
is the opcode.  C<flags> gives the eight bits of C<op_flags>, except
that C<OPf_KIDS> will be set automatically, and, shifted up eight bits,
the eight bits of C<op_private>, except that the bit with value 1 is
automatically set.  C<first> supplies the expression controlling the
flow, and C<other> supplies the side (alternate) chain of ops; they are
consumed by this function and become part of the constructed op tree.

=cut
*/

OP *
Perl_newLOGOP(pTHX_ I32 type, I32 flags, OP *first, OP *other)
{
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
			kid = OpSIBLING(kid);
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

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_LOGOP
	|| type == OP_CUSTOM);

    scalarboolean(first);

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
		    && (( o2 = OpSIBLING(o2))) )
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
	        op_free(other);
	    return first;
	}
    }
    else if ((first->op_flags & OPf_KIDS) && type != OP_DOR
	&& ckWARN(WARN_MISC)) /* [#24076] Don't warn for <FH> err FOO. */
    {
	const OP * const k1 = ((UNOP*)first)->op_first;
	const OP * const k2 = OpSIBLING(k1);
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

    /* optimize AND and OR ops that have NOTs as children */
    if (first->op_type == OP_NOT
        && (first->op_flags & OPf_KIDS)
        && ((first->op_flags & OPf_SPECIAL) /* unless ($x) { } */
            || (other->op_type == OP_NOT))  /* if (!$x && !$y) { } */
        ) {
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

    logop = S_alloc_LOGOP(aTHX_ type, first, LINKLIST(other));
    logop->op_flags |= (U8)flags;
    logop->op_private = (U8)(1 | (flags >> 8));

    /* establish postfix order */
    logop->op_next = LINKLIST(first);
    first->op_next = (OP*)logop;
    assert(!OpHAS_SIBLING(first));
    op_sibling_splice((OP*)logop, first, 0, other);

    CHECKOP(type,logop);

    o = newUNOP(prepend_not ? OP_NOT : OP_NULL,
		PL_opargs[type] & OA_RETSCALAR ? OPf_WANT_SCALAR : 0,
		(OP*)logop);
    other->op_next = o;

    return o;
}

/*
=for apidoc Am|OP *|newCONDOP|I32 flags|OP *first|OP *trueop|OP *falseop

Constructs, checks, and returns a conditional-expression (C<cond_expr>)
op.  C<flags> gives the eight bits of C<op_flags>, except that C<OPf_KIDS>
will be set automatically, and, shifted up eight bits, the eight bits of
C<op_private>, except that the bit with value 1 is automatically set.
C<first> supplies the expression selecting between the two branches,
and C<trueop> and C<falseop> supply the branches; they are consumed by
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
        op_free(first);
        op_free(dead);
	if (live->op_type == OP_LEAVE)
	    live = newUNOP(OP_NULL, OPf_SPECIAL, live);
	else if (live->op_type == OP_MATCH || live->op_type == OP_SUBST
	      || live->op_type == OP_TRANS || live->op_type == OP_TRANSR)
	    /* Mark the op as being unbindable with =~ */
	    live->op_flags |= OPf_SPECIAL;
	live->op_folded = 1;
	return live;
    }
    logop = S_alloc_LOGOP(aTHX_ OP_COND_EXPR, first, LINKLIST(trueop));
    logop->op_flags |= (U8)flags;
    logop->op_private = (U8)(1 | (flags >> 8));
    logop->op_next = LINKLIST(falseop);

    CHECKOP(OP_COND_EXPR, /* that's logop->op_type */
	    logop);

    /* establish postfix order */
    start = LINKLIST(first);
    first->op_next = (OP*)logop;

    /* make first, trueop, falseop siblings */
    op_sibling_splice((OP*)logop, first,  0, trueop);
    op_sibling_splice((OP*)logop, trueop, 0, falseop);

    o = newUNOP(OP_NULL, 0, (OP*)logop);

    trueop->op_next = falseop->op_next = o;

    o->op_next = start;
    return o;
}

/*
=for apidoc Am|OP *|newRANGE|I32 flags|OP *left|OP *right

Constructs and returns a C<range> op, with subordinate C<flip> and
C<flop> ops.  C<flags> gives the eight bits of C<op_flags> for the
C<flip> op and, shifted up eight bits, the eight bits of C<op_private>
for both the C<flip> and C<range> ops, except that the bit with value
1 is automatically set.  C<left> and C<right> supply the expressions
controlling the endpoints of the range; they are consumed by this function
and become part of the constructed op tree.

=cut
*/

OP *
Perl_newRANGE(pTHX_ I32 flags, OP *left, OP *right)
{
    LOGOP *range;
    OP *flip;
    OP *flop;
    OP *leftstart;
    OP *o;

    PERL_ARGS_ASSERT_NEWRANGE;

    range = S_alloc_LOGOP(aTHX_ OP_RANGE, left, LINKLIST(right));
    range->op_flags = OPf_KIDS;
    leftstart = LINKLIST(left);
    range->op_private = (U8)(1 | (flags >> 8));

    /* make left and right siblings */
    op_sibling_splice((OP*)range, left, 0, right);

    range->op_next = (OP*)range;
    flip = newUNOP(OP_FLIP, flags, (OP*)range);
    flop = newUNOP(OP_FLOP, 0, flip);
    o = newUNOP(OP_NULL, 0, flop);
    LINKLIST(flop);
    range->op_next = leftstart;

    left->op_next = flip;
    right->op_next = flop;

    range->op_targ =
	pad_add_name_pvn("$", 1, padadd_NO_DUP_CHECK|padadd_STATE, 0, 0);
    sv_upgrade(PAD_SV(range->op_targ), SVt_PVNV);
    flip->op_targ =
	pad_add_name_pvn("$", 1, padadd_NO_DUP_CHECK|padadd_STATE, 0, 0);;
    sv_upgrade(PAD_SV(flip->op_targ), SVt_PVNV);
    SvPADTMP_on(PAD_SV(flip->op_targ));

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
and suchlike.  C<flags> gives the eight bits of C<op_flags> for the
top-level op, except that some bits will be set automatically as required.
C<expr> supplies the expression controlling loop iteration, and C<block>
supplies the body of the loop; they are consumed by this function and
become part of the constructed op tree.  C<debuggable> is currently
unused and should always be 1.

=cut
*/

OP *
Perl_newLOOPOP(pTHX_ I32 flags, I32 debuggable, OP *expr, OP *block)
{
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
	    const OP * const k2 = k1 ? OpSIBLING(k1) : NULL;
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
    o->op_flags |= OPf_SPECIAL;	/* suppress cx_popblock() curpm restoration*/
    return o;
}

/*
=for apidoc Am|OP *|newWHILEOP|I32 flags|I32 debuggable|LOOP *loop|OP *expr|OP *block|OP *cont|I32 has_my

Constructs, checks, and returns an op tree expressing a C<while> loop.
This is a heavyweight loop, with structure that allows exiting the loop
by C<last> and suchlike.

C<loop> is an optional preconstructed C<enterloop> op to use in the
loop; if it is null then a suitable op will be constructed automatically.
C<expr> supplies the loop's controlling expression.  C<block> supplies the
main body of the loop, and C<cont> optionally supplies a C<continue> block
that operates as a second half of the body.  All of these optree inputs
are consumed by this function and become part of the constructed op tree.

C<flags> gives the eight bits of C<op_flags> for the C<leaveloop>
op and, shifted up eight bits, the eight bits of C<op_private> for
the C<leaveloop> op, except that (in both cases) some bits will be set
automatically.  C<debuggable> is currently unused and should always be 1.
C<has_my> can be supplied as true to force the
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
	    const OP * const k2 = (k1) ? OpSIBLING(k1) : NULL;
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
        OpTYPE_set(loop, OP_ENTERLOOP);
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

C<sv> optionally supplies the variable that will be aliased to each
item in turn; if null, it defaults to C<$_>.
C<expr> supplies the list of values to iterate over.  C<block> supplies
the main body of the loop, and C<cont> optionally supplies a C<continue>
block that operates as a second half of the body.  All of these optree
inputs are consumed by this function and become part of the constructed
op tree.

C<flags> gives the eight bits of C<op_flags> for the C<leaveloop>
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

    PERL_ARGS_ASSERT_NEWFOROP;

    if (sv) {
	if (sv->op_type == OP_RV2SV) {	/* symbol table variable */
	    iterpflags = sv->op_private & OPpOUR_INTRO; /* for our $x () */
            OpTYPE_set(sv, OP_RV2GV);

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
            sv->op_targ = 0;
            op_free(sv);
	    sv = NULL;
	    PAD_COMPNAME_GEN_set(padoff, PERL_INT_MAX);
	}
	else if (sv->op_type == OP_NULL && sv->op_targ == OP_SREFGEN)
	    NOOP;
	else
	    Perl_croak(aTHX_ "Can't use %s for loop variable", PL_op_desc[sv->op_type]);
	if (padoff) {
	    PADNAME * const pn = PAD_COMPNAME(padoff);
	    const char * const name = PadnamePV(pn);

	    if (PadnameLEN(pn) == 2 && name[0] == '$' && name[1] == '_')
		iterpflags |= OPpITER_DEF;
	}
    }
    else {
	sv = newGVOP(OP_GV, 0, PL_defgv);
	iterpflags |= OPpITER_DEF;
    }

    if (expr->op_type == OP_RV2AV || expr->op_type == OP_PADAV) {
	expr = op_lvalue(force_list(scalar(ref(expr, OP_ITER)), 1), OP_GREPSTART);
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
	OP* const right = OpSIBLING(left);
	LISTOP* listop;

	range->op_flags &= ~OPf_KIDS;
        /* detach range's children */
        op_sibling_splice((OP*)range, NULL, -1, NULL);

	listop = (LISTOP*)newLISTOP(OP_LIST, 0, left, right);
	listop->op_first->op_next = range->op_next;
	left->op_next = range->op_other;
	right->op_next = (OP*)listop;
	listop->op_next = listop->op_first;

	op_free(expr);
	expr = (OP*)(listop);
        op_null(expr);
	iterflags |= OPf_STACKED;
    }
    else {
        expr = op_lvalue(force_list(expr, 1), OP_GREPSTART);
    }

    loop = (LOOP*)op_convert_list(OP_ENTERITER, iterflags,
                                  op_append_elem(OP_LIST, list(expr),
                                                 scalar(sv)));
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
#ifdef PERL_OP_PARENT
        assert(loop->op_last->op_sibparent == (OP*)loop);
        OpLASTSIB_set(loop->op_last, (OP*)tmp); /*point back to new parent */
#endif
	S_op_destroy(aTHX_ (OP*)loop);
	loop = tmp;
    }
    else if (!loop->op_slabbed)
    {
	loop = (LOOP*)PerlMemShared_realloc(loop, sizeof(LOOP));
#ifdef PERL_OP_PARENT
        OpLASTSIB_set(loop->op_last, (OP*)loop);
#endif
    }
    loop->op_targ = padoff;
    wop = newWHILEOP(flags, 1, loop, newOP(OP_ITER, 0), block, cont, 0);
    return wop;
}

/*
=for apidoc Am|OP *|newLOOPEX|I32 type|OP *label

Constructs, checks, and returns a loop-exiting op (such as C<goto>
or C<last>).  C<type> is the opcode.  C<label> supplies the parameter
determining the target of the op; it is consumed by this function and
becomes part of the constructed op tree.

=cut
*/

OP*
Perl_newLOOPEX(pTHX_ I32 type, OP *label)
{
    OP *o = NULL;

    PERL_ARGS_ASSERT_NEWLOOPEX;

    assert((PL_opargs[type] & OA_CLASS_MASK) == OA_LOOPEXOP
	|| type == OP_CUSTOM);

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
		op_free(label);
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
	cond->op_flags &= ~(OPf_WANT_SCALAR | OPf_REF);
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
    PERL_UNUSED_ARG(entertarg); /* used to indicate targ of lexical $_ */

    enterop = S_alloc_LOGOP(aTHX_ enter_opcode, block, NULL);
    enterop->op_targ = 0;
    enterop->op_private = 0;

    o = newUNOP(leave_opcode, 0, (OP *) enterop);

    if (cond) {
        /* prepend cond if we have one */
        op_sibling_splice((OP*)enterop, NULL, 0, scalar(cond));

	o->op_next = LINKLIST(cond);
	cond->op_next = (OP *) enterop;
    }
    else {
	/* This is a default {} block */
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
    PERL_ARGS_ASSERT_LOOKS_LIKE_BOOL;

    switch(o->op_type) {
	case OP_OR:
	case OP_DOR:
	    return looks_like_bool(cLOGOPo->op_first);

	case OP_AND:
        {
            OP* sibl = OpSIBLING(cLOGOPo->op_first);
            ASSUME(sibl);
	    return (
	    	looks_like_bool(cLOGOPo->op_first)
	     && looks_like_bool(sibl));
        }

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

	/* FALLTHROUGH */
	default:
	    return FALSE;
    }
}

/*
=for apidoc Am|OP *|newGIVENOP|OP *cond|OP *block|PADOFFSET defsv_off

Constructs, checks, and returns an op tree expressing a C<given> block.
C<cond> supplies the expression that will be locally assigned to a lexical
variable, and C<block> supplies the body of the C<given> construct; they
are consumed by this function and become part of the constructed op tree.
C<defsv_off> must be zero (it used to identity the pad slot of lexical $_).

=cut
*/

OP *
Perl_newGIVENOP(pTHX_ OP *cond, OP *block, PADOFFSET defsv_off)
{
    PERL_ARGS_ASSERT_NEWGIVENOP;
    PERL_UNUSED_ARG(defsv_off);

    assert(!defsv_off);
    return newGIVWHENOP(
    	ref_array_or_hash(cond),
    	block,
	OP_ENTERGIVEN, OP_LEAVEGIVEN,
	0);
}

/*
=for apidoc Am|OP *|newWHENOP|OP *cond|OP *block

Constructs, checks, and returns an op tree expressing a C<when> block.
C<cond> supplies the test expression, and C<block> supplies the block
that will be executed if the test evaluates to true; they are consumed
by this function and become part of the constructed op tree.  C<cond>
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

/* must not conflict with SVf_UTF8 */
#define CV_CKPROTO_CURSTASH	0x1

void
Perl_cv_ckproto_len_flags(pTHX_ const CV *cv, const GV *gv, const char *p,
		    const STRLEN len, const U32 flags)
{
    SV *name = NULL, *msg;
    const char * cvp = SvROK(cv)
			? SvTYPE(SvRV_const(cv)) == SVt_PVCV
			   ? (cv = (const CV *)SvRV_const(cv), CvPROTO(cv))
			   : ""
			: CvPROTO(cv);
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
	else if (flags & CV_CKPROTO_CURSTASH || SvROK(gv)) {
	    name = sv_2mortal(newSVhek(HvNAME_HEK(PL_curstash)));
	    sv_catpvs(name, "::");
	    if (SvROK(gv)) {
		assert (SvTYPE(SvRV_const(gv)) == SVt_PVCV);
		assert (CvNAMED(SvRV_const(gv)));
		sv_cathek(name, CvNAME_HEK(MUTABLE_CV(SvRV_const(gv))));
	    }
	    else sv_catsv(name, (SV *)gv);
	}
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
value returned by the sub.  Otherwise, returns C<NULL>.

Constant subs can be created with C<newCONSTSUB> or as described in
L<perlsub/"Constant Functions">.

=cut
*/
SV *
Perl_cv_const_sv(const CV *const cv)
{
    SV *sv;
    if (!cv)
	return NULL;
    if (!(SvTYPE(cv) == SVt_PVCV || SvTYPE(cv) == SVt_PVFM))
	return NULL;
    sv = CvCONST(cv) ? MUTABLE_SV(CvXSUBANY(cv).any_ptr) : NULL;
    if (sv && SvTYPE(sv) == SVt_PVAV) return NULL;
    return sv;
}

SV *
Perl_cv_const_sv_or_av(const CV * const cv)
{
    if (!cv)
	return NULL;
    if (SvROK(cv)) return SvRV((SV *)cv);
    assert (SvTYPE(cv) == SVt_PVCV || SvTYPE(cv) == SVt_PVFM);
    return CvCONST(cv) ? MUTABLE_SV(CvXSUBANY(cv).any_ptr) : NULL;
}

/* op_const_sv:  examine an optree to determine whether it's in-lineable.
 * Can be called in 2 ways:
 *
 * !allow_lex
 * 	look for a single OP_CONST with attached value: return the value
 *
 * allow_lex && !CvCONST(cv);
 *
 * 	examine the clone prototype, and if contains only a single
 * 	OP_CONST, return the value; or if it contains a single PADSV ref-
 * 	erencing an outer lexical, turn on CvCONST to indicate the CV is
 * 	a candidate for "constizing" at clone time, and return NULL.
 */

static SV *
S_op_const_sv(pTHX_ const OP *o, CV *cv, bool allow_lex)
{
    SV *sv = NULL;
    bool padsv = FALSE;

    assert(o);
    assert(cv);

    for (; o; o = o->op_next) {
	const OPCODE type = o->op_type;

	if (type == OP_NEXTSTATE || type == OP_LINESEQ
	     || type == OP_NULL
	     || type == OP_PUSHMARK)
		continue;
	if (type == OP_DBSTATE)
		continue;
	if (type == OP_LEAVESUB)
	    break;
	if (sv)
	    return NULL;
	if (type == OP_CONST && cSVOPo->op_sv)
	    sv = cSVOPo->op_sv;
	else if (type == OP_UNDEF && !o->op_private) {
	    sv = newSV(0);
	    SAVEFREESV(sv);
	}
	else if (allow_lex && type == OP_PADSV) {
		if (PAD_COMPNAME_FLAGS(o->op_targ) & PADNAMEt_OUTER)
		{
		    sv = &PL_sv_undef; /* an arbitrary non-null value */
		    padsv = TRUE;
		}
		else
		    return NULL;
	}
	else {
	    return NULL;
	}
    }
    if (padsv) {
	CvCONST_on(cv);
	return NULL;
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
    SAVEFREESV(cv);
    return TRUE;
}

CV *
Perl_newMYSUB(pTHX_ I32 floor, OP *o, OP *proto, OP *attrs, OP *block)
{
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
    OP *start = NULL;
#ifdef PERL_DEBUG_READONLY_OPS
    OPSLAB *slab = NULL;
#endif

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
        move_proto_attr(&proto, &attrs, (GV *)PadnameSV(name));

    if (proto) {
	assert(proto->op_type == OP_CONST);
	ps = SvPV_const(((SVOP*)proto)->op_sv, ps_len);
        ps_utf8 = SvUTF8(((SVOP*)proto)->op_sv);
    }
    else
	ps = NULL;

    if (proto)
        SAVEFREEOP(proto);
    if (attrs)
        SAVEFREEOP(attrs);

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
	assert (SvTYPE(*spot) == SVt_PVCV);
	if (CvNAMED(*spot))
	    hek = CvNAME_HEK(*spot);
	else {
            dVAR;
	    U32 hash;
	    PERL_HASH(hash, PadnamePV(name)+1, PadnameLEN(name)-1);
	    CvNAME_HEK_set(*spot, hek =
		share_hek(
		    PadnamePV(name)+1,
		    (PadnameLEN(name)-1) * (PadnameUTF8(name) ? -1 : 1),
		    hash
		)
	    );
	    CvLEXICAL_on(*spot);
	}
	cv = PadnamePROTOCV(name);
	svspot = (SV **)(spot = &PadnamePROTOCV(name));
    }

    if (block) {
	/* This makes sub {}; work as expected.  */
	if (block->op_type == OP_STUB) {
	    const line_t l = PL_parser->copline;
	    op_free(block);
	    block = newSTATEOP(0, NULL, 0);
	    PL_parser->copline = l;
	}
	block = CvLVALUE(compcv)
	     || (cv && CvLVALUE(cv) && !CvROOT(cv) && !CvXSUB(cv))
		   ? newUNOP(OP_LEAVESUBLV, 0,
			     op_lvalue(scalarseq(block), OP_LEAVESUBLV))
		   : newUNOP(OP_LEAVESUB, 0, scalarseq(block));
	start = LINKLIST(block);
	block->op_next = 0;
        if (ps && !*ps && !attrs && !CvLVALUE(compcv))
            const_sv = S_op_const_sv(aTHX_ start, compcv, FALSE);
        else
            const_sv = NULL;
    }
    else
        const_sv = NULL;

    if (cv) {
        const bool exists = CvROOT(cv) || CvXSUB(cv);

        /* if the subroutine doesn't exist and wasn't pre-declared
         * with a prototype, assume it will be AUTOLOADed,
         * skipping the prototype check
         */
        if (exists || SvPOK(cv))
            cv_ckproto_len_flags(cv, (GV *)PadnameSV(name), ps, ps_len,
                                 ps_utf8);
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
	SvFLAGS(const_sv) |= SVs_PADTMP;
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
	PoisonPADLIST(cv);
	CvFLAGS(cv) |= CvMETHOD(compcv);
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
	    CvPADLIST_set(cv, CvPADLIST(compcv));
	    CvOUTSIDE(compcv) = temp_cv;
	    CvPADLIST_set(compcv, temp_padl);
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
    CvLEXICAL_on(cv);
    if (!CvNAME_HEK(cv)) {
	if (hek) (void)share_hek_hek(hek);
	else {
            dVAR;
	    U32 hash;
	    PERL_HASH(hash, PadnamePV(name)+1, PadnameLEN(name)-1);
	    hek = share_hek(PadnamePV(name)+1,
		      (PadnameLEN(name)-1) * (PadnameUTF8(name) ? -1 : 1),
		      hash);
	}
	CvNAME_HEK_set(cv, hek);
    }
    if (const_sv) goto clone;

    CvFILE_set_from_cop(cv, PL_curcop);
    CvSTASH_set(cv, PL_curstash);

    if (ps) {
	sv_setpvn(MUTABLE_SV(cv), ps, ps_len);
        if ( ps_utf8 ) SvUTF8_on(MUTABLE_SV(cv));
    }

    if (!block)
	goto attrs;

    /* If we assign an optree to a PVCV, then we've defined a subroutine that
       the debugger could be able to set a breakpoint in, so signal to
       pp_entereval that it should not throw away any saved lines at scope
       exit.  */
       
    PL_breakable_sub_gen++;
    CvROOT(cv) = block;
    CvROOT(cv)->op_private |= OPpREFCOUNTED;
    OpREFCNT_set(CvROOT(cv), 1);
    /* The cv no longer needs to hold a refcount on the slab, as CvROOT
       itself has a refcount. */
    CvSLABBED_off(cv);
    OpslabREFCNT_dec_padok((OPSLAB *)CvSTART(cv));
#ifdef PERL_DEBUG_READONLY_OPS
    slab = (OPSLAB *)CvSTART(cv);
#endif
    CvSTART(cv) = start;
    CALL_PEEP(start);
    finalize_optree(CvROOT(cv));
    S_prune_chain_head(&CvSTART(cv));

    /* now that optimizer has done its work, adjust pad values */

    pad_tidy(CvCLONE(cv) ? padtidy_SUBCLONE : padtidy_SUB);

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
#ifdef PERL_DEBUG_READONLY_OPS
    if (slab)
	Slab_to_ro(slab);
#endif
    op_free(o);
    return cv;
}

/* _x = extended */
CV *
Perl_newATTRSUB_x(pTHX_ I32 floor, OP *o, OP *proto, OP *attrs,
			    OP *block, bool o_is_gv)
{
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
       full CV.  If anything is present then it will take a full CV to
       store it.  */
    const I32 gv_fetch_flags
	= ec ? GV_NOADD_NOINIT :
        (block || attrs || (CvFLAGS(PL_compcv) & CVf_BUILTIN_ATTRS))
	? GV_ADDMULTI : GV_ADDMULTI | GV_NOINIT;
    STRLEN namlen = 0;
    const char * const name =
	 o ? SvPV_const(o_is_gv ? (SV *)o : cSVOPo->op_sv, namlen) : NULL;
    bool has_name;
    bool name_is_utf8 = o && !o_is_gv && SvUTF8(cSVOPo->op_sv);
    bool evanescent = FALSE;
    OP *start = NULL;
#ifdef PERL_DEBUG_READONLY_OPS
    OPSLAB *slab = NULL;
#endif

    if (o_is_gv) {
	gv = (GV*)o;
	o = NULL;
	has_name = TRUE;
    } else if (name) {
	/* Try to optimise and avoid creating a GV.  Instead, the CV’s name
	   hek and CvSTASH pointer together can imply the GV.  If the name
	   contains a package name, then GvSTASH(CvGV(cv)) may differ from
	   CvSTASH, so forego the optimisation if we find any.
	   Also, we may be called from load_module at run time, so
	   PL_curstash (which sets CvSTASH) may not point to the stash the
	   sub is stored in.  */
	const I32 flags =
	   ec ? GV_NOADD_NOINIT
	      :   PL_curstash != CopSTASH(PL_curcop)
	       || memchr(name, ':', namlen) || memchr(name, '\'', namlen)
		    ? gv_fetch_flags
		    : GV_ADDMULTI | GV_NOINIT | GV_NOTQUAL;
	gv = gv_fetchsv(cSVOPo->op_sv, flags, SVt_PVCV);
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
    if (!ec) {
        if (isGV(gv)) {
            move_proto_attr(&proto, &attrs, gv);
        } else {
            assert(cSVOPo);
            move_proto_attr(&proto, &attrs, (GV *)cSVOPo->op_sv);
        }
    }

    if (proto) {
	assert(proto->op_type == OP_CONST);
	ps = SvPV_const(((SVOP*)proto)->op_sv, ps_len);
        ps_utf8 = SvUTF8(((SVOP*)proto)->op_sv);
    }
    else
	ps = NULL;

    if (o)
        SAVEFREEOP(o);
    if (proto)
        SAVEFREEOP(proto);
    if (attrs)
        SAVEFREEOP(attrs);

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

    if (!block && SvTYPE(gv) != SVt_PVGV) {
      /* If we are not defining a new sub and the existing one is not a
         full GV + CV... */
      if (attrs || (CvFLAGS(PL_compcv) & CVf_BUILTIN_ATTRS)) {
	/* We are applying attributes to an existing sub, so we need it
	   upgraded if it is a constant.  */
	if (SvROK(gv) && SvTYPE(SvRV(gv)) != SVt_PVCV)
	    gv_init_pvn(gv, PL_curstash, name, namlen,
			SVf_UTF8 * name_is_utf8);
      }
      else {			/* Maybe prototype now, and had at maximum
				   a prototype or const/sub ref before.  */
	if (SvTYPE(gv) > SVt_NULL) {
	    cv_ckproto_len_flags((const CV *)gv,
				 o ? (const GV *)cSVOPo->op_sv : NULL, ps,
				 ps_len, ps_utf8);
	}
	if (!SvROK(gv)) {
	  if (ps) {
	    sv_setpvn(MUTABLE_SV(gv), ps, ps_len);
            if ( ps_utf8 ) SvUTF8_on(MUTABLE_SV(gv));
          }
	  else
	    sv_setiv(MUTABLE_SV(gv), -1);
	}

	SvREFCNT_dec(PL_compcv);
	cv = PL_compcv = NULL;
	goto done;
      }
    }

    cv = (!name || (isGV(gv) && GvCVGEN(gv)))
	? NULL
	: isGV(gv)
	    ? GvCV(gv)
	    : SvROK(gv) && SvTYPE(SvRV(gv)) == SVt_PVCV
		? (CV *)SvRV(gv)
		: NULL;

    if (block) {
	assert(PL_parser);
	/* This makes sub {}; work as expected.  */
	if (block->op_type == OP_STUB) {
	    const line_t l = PL_parser->copline;
	    op_free(block);
	    block = newSTATEOP(0, NULL, 0);
	    PL_parser->copline = l;
	}
	block = CvLVALUE(PL_compcv)
	     || (cv && CvLVALUE(cv) && !CvROOT(cv) && !CvXSUB(cv)
		    && (!isGV(gv) || !GvASSUMECV(gv)))
		   ? newUNOP(OP_LEAVESUBLV, 0,
			     op_lvalue(scalarseq(block), OP_LEAVESUBLV))
		   : newUNOP(OP_LEAVESUB, 0, scalarseq(block));
	start = LINKLIST(block);
	block->op_next = 0;
        if (ps && !*ps && !attrs && !CvLVALUE(PL_compcv))
            const_sv =
                S_op_const_sv(aTHX_ start, PL_compcv,
                                        cBOOL(CvCLONE(PL_compcv)));
        else
            const_sv = NULL;
    }
    else
        const_sv = NULL;

    if (SvPOK(gv) || (SvROK(gv) && SvTYPE(SvRV(gv)) != SVt_PVCV)) {
	cv_ckproto_len_flags((const CV *)gv,
			     o ? (const GV *)cSVOPo->op_sv : NULL, ps,
			     ps_len, ps_utf8|CV_CKPROTO_CURSTASH);
	if (SvROK(gv)) {
	    /* All the other code for sub redefinition warnings expects the
	       clobbered sub to be a CV.  Instead of making all those code
	       paths more complex, just inline the RV version here.  */
	    const line_t oldline = CopLINE(PL_curcop);
	    assert(IN_PERL_COMPILETIME);
	    if (PL_parser && PL_parser->copline != NOLINE)
		/* This ensures that warnings are reported at the first
		   line of a redefinition, not the last.  */
		CopLINE_set(PL_curcop, PL_parser->copline);
	    /* protect against fatal warnings leaking compcv */
	    SAVEFREESV(PL_compcv);

	    if (ckWARN(WARN_REDEFINE)
	     || (  ckWARN_d(WARN_REDEFINE)
		&& (  !const_sv || SvRV(gv) == const_sv
		   || sv_cmp(SvRV(gv), const_sv)  )))
		Perl_warner(aTHX_ packWARN(WARN_REDEFINE),
			  "Constant subroutine %"SVf" redefined",
			  SVfARG(cSVOPo->op_sv));

	    SvREFCNT_inc_simple_void_NN(PL_compcv);
	    CopLINE_set(PL_curcop, oldline);
	    SvREFCNT_dec(SvRV(gv));
	}
    }

    if (cv) {
        const bool exists = CvROOT(cv) || CvXSUB(cv);

        /* if the subroutine doesn't exist and wasn't pre-declared
         * with a prototype, assume it will be AUTOLOADed,
         * skipping the prototype check
         */
        if (exists || SvPOK(cv))
            cv_ckproto_len_flags(cv, gv, ps, ps_len, ps_utf8);
	/* already defined (or promised)? */
	if (exists || (isGV(gv) && GvASSUMECV(gv))) {
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
	SvFLAGS(const_sv) |= SVs_PADTMP;
	if (cv) {
	    assert(!CvROOT(cv) && !CvCONST(cv));
	    cv_forget_slab(cv);
	    sv_setpvs(MUTABLE_SV(cv), "");  /* prototype is "" */
	    CvXSUBANY(cv).any_ptr = const_sv;
	    CvXSUB(cv) = const_sv_xsub;
	    CvCONST_on(cv);
	    CvISXSUB_on(cv);
	    PoisonPADLIST(cv);
	    CvFLAGS(cv) |= CvMETHOD(PL_compcv);
	}
	else {
	    if (isGV(gv) || CvMETHOD(PL_compcv)) {
		if (name && isGV(gv))
		    GvCV_set(gv, NULL);
		cv = newCONSTSUB_flags(
		    NULL, name, namlen, name_is_utf8 ? SVf_UTF8 : 0,
		    const_sv
		);
		CvFLAGS(cv) |= CvMETHOD(PL_compcv);
	    }
	    else {
		if (!SvROK(gv)) {
		    SV_CHECK_THINKFIRST_COW_DROP((SV *)gv);
		    prepare_SV_for_RV((SV *)gv);
		    SvOK_off((SV *)gv);
		    SvROK_on(gv);
		}
		SvRV_set(gv, const_sv);
	    }
	}
	op_free(block);
	SvREFCNT_dec(PL_compcv);
	PL_compcv = NULL;
	goto done;
    }
    if (cv) {				/* must reuse cv if autoloaded */
	/* transfer PL_compcv to cv */
	if (block
	) {
	    cv_flags_t existing_builtin_attrs = CvFLAGS(cv) & CVf_BUILTIN_ATTRS;
	    PADLIST *const temp_av = CvPADLIST(cv);
	    CV *const temp_cv = CvOUTSIDE(cv);
	    const cv_flags_t other_flags =
		CvFLAGS(cv) & (CVf_SLABBED|CVf_WEAKOUTSIDE);
	    OP * const cvstart = CvSTART(cv);

	    if (isGV(gv)) {
		CvGV_set(cv,gv);
		assert(!CvCVGV_RC(cv));
		assert(CvGV(cv) == gv);
	    }
	    else {
		dVAR;
		U32 hash;
		PERL_HASH(hash, name, namlen);
		CvNAME_HEK_set(cv,
			       share_hek(name,
					 name_is_utf8
					    ? -(SSize_t)namlen
					    :  (SSize_t)namlen,
					 hash));
	    }

	    SvPOK_off(cv);
	    CvFLAGS(cv) = CvFLAGS(PL_compcv) | existing_builtin_attrs
					     | CvNAMED(cv);
	    CvOUTSIDE(cv) = CvOUTSIDE(PL_compcv);
	    CvOUTSIDE_SEQ(cv) = CvOUTSIDE_SEQ(PL_compcv);
	    CvPADLIST_set(cv,CvPADLIST(PL_compcv));
	    CvOUTSIDE(PL_compcv) = temp_cv;
	    CvPADLIST_set(PL_compcv, temp_av);
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
	if (name && isGV(gv)) {
	    GvCV_set(gv, cv);
	    GvCVGEN(gv) = 0;
	    if (HvENAME_HEK(GvSTASH(gv)))
		/* sub Foo::bar { (shift)+1 } */
		gv_method_changed(gv);
	}
	else if (name) {
	    if (!SvROK(gv)) {
		SV_CHECK_THINKFIRST_COW_DROP((SV *)gv);
		prepare_SV_for_RV((SV *)gv);
		SvOK_off((SV *)gv);
		SvROK_on(gv);
	    }
	    SvRV_set(gv, (SV *)cv);
	}
    }
    if (!CvHASGV(cv)) {
	if (isGV(gv)) CvGV_set(cv, gv);
	else {
            dVAR;
	    U32 hash;
	    PERL_HASH(hash, name, namlen);
	    CvNAME_HEK_set(cv, share_hek(name,
					 name_is_utf8
					    ? -(SSize_t)namlen
					    :  (SSize_t)namlen,
					 hash));
	}
	CvFILE_set_from_cop(cv, PL_curcop);
	CvSTASH_set(cv, PL_curstash);
    }

    if (ps) {
	sv_setpvn(MUTABLE_SV(cv), ps, ps_len);
        if ( ps_utf8 ) SvUTF8_on(MUTABLE_SV(cv));
    }

    if (!block)
	goto attrs;

    /* If we assign an optree to a PVCV, then we've defined a subroutine that
       the debugger could be able to set a breakpoint in, so signal to
       pp_entereval that it should not throw away any saved lines at scope
       exit.  */
       
    PL_breakable_sub_gen++;
    CvROOT(cv) = block;
    CvROOT(cv)->op_private |= OPpREFCOUNTED;
    OpREFCNT_set(CvROOT(cv), 1);
    /* The cv no longer needs to hold a refcount on the slab, as CvROOT
       itself has a refcount. */
    CvSLABBED_off(cv);
    OpslabREFCNT_dec_padok((OPSLAB *)CvSTART(cv));
#ifdef PERL_DEBUG_READONLY_OPS
    slab = (OPSLAB *)CvSTART(cv);
#endif
    CvSTART(cv) = start;
    CALL_PEEP(start);
    finalize_optree(CvROOT(cv));
    S_prune_chain_head(&CvSTART(cv));

    /* now that optimizer has done its work, adjust pad values */

    pad_tidy(CvCLONE(cv) ? padtidy_SUBCLONE : padtidy_SUB);

  attrs:
    if (attrs) {
	/* Need to do a C<use attributes $stash_of_cv,\&cv,@attrs>. */
	HV *stash = name && !CvNAMED(cv) && GvSTASH(CvGV(cv))
			? GvSTASH(CvGV(cv))
			: PL_curstash;
	if (!name) SAVEFREESV(cv);
	apply_attrs(stash, MUTABLE_SV(cv), attrs);
	if (!name) SvREFCNT_inc_simple_void_NN(cv);
    }

    if (block && has_name) {
	if (PERLDB_SUBLINE && PL_curstash != PL_debstash) {
	    SV * const tmpstr = cv_name(cv,NULL,0);
	    GV * const db_postponed = gv_fetchpvs("DB::postponed",
						  GV_ADDMULTI, SVt_PVHV);
	    HV *hv;
	    SV * const sv = Perl_newSVpvf(aTHX_ "%s:%ld-%ld",
					  CopFILE(PL_curcop),
					  (long)PL_subline,
					  (long)CopLINE(PL_curcop));
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

        if (name) {
            if (PL_parser && PL_parser->error_count)
                clear_special_blocks(name, gv, cv);
            else
                evanescent =
                    process_special_blocks(floor, name, gv, cv);
        }
    }

  done:
    if (PL_parser)
	PL_parser->copline = NOLINE;
    LEAVE_SCOPE(floor);
    if (!evanescent) {
#ifdef PERL_DEBUG_READONLY_OPS
      if (slab)
	Slab_to_ro(slab);
#endif
      if (cv && name && CvOUTSIDE(cv) && !CvEVAL(CvOUTSIDE(cv)))
	pad_add_weakref(cv);
    }
    return cv;
}

STATIC void
S_clear_special_blocks(pTHX_ const char *const fullname,
                       GV *const gv, CV *const cv) {
    const char *colon;
    const char *name;

    PERL_ARGS_ASSERT_CLEAR_SPECIAL_BLOCKS;

    colon = strrchr(fullname,':');
    name = colon ? colon + 1 : fullname;

    if ((*name == 'B' && strEQ(name, "BEGIN"))
        || (*name == 'E' && strEQ(name, "END"))
        || (*name == 'U' && strEQ(name, "UNITCHECK"))
        || (*name == 'C' && strEQ(name, "CHECK"))
        || (*name == 'I' && strEQ(name, "INIT"))) {
        if (!isGV(gv)) {
            (void)CvGV(cv);
            assert(isGV(gv));
        }
        GvCV_set(gv, NULL);
        SvREFCNT_dec_NN(MUTABLE_SV(cv));
    }
}

/* Returns true if the sub has been freed.  */
STATIC bool
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
            (void)CvGV(cv);
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
	    return !PL_savebegin;
	}
	else
	    return FALSE;
    } else {
	if (*name == 'E') {
	    if strEQ(name, "END") {
		DEBUG_x( dump_sub(gv) );
		Perl_av_create_and_unshift_one(aTHX_ &PL_endav, MUTABLE_SV(cv));
	    } else
		return FALSE;
	} else if (*name == 'U') {
	    if (strEQ(name, "UNITCHECK")) {
		/* It's never too late to run a unitcheck block */
		Perl_av_create_and_unshift_one(aTHX_ &PL_unitcheckav, MUTABLE_SV(cv));
	    }
	    else
		return FALSE;
	} else if (*name == 'C') {
	    if (strEQ(name, "CHECK")) {
		if (PL_main_start)
		    /* diag_listed_as: Too late to run %s block */
		    Perl_ck_warner(aTHX_ packWARN(WARN_VOID),
				   "Too late to run CHECK block");
		Perl_av_create_and_unshift_one(aTHX_ &PL_checkav, MUTABLE_SV(cv));
	    }
	    else
		return FALSE;
	} else if (*name == 'I') {
	    if (strEQ(name, "INIT")) {
		if (PL_main_start)
		    /* diag_listed_as: Too late to run %s block */
		    Perl_ck_warner(aTHX_ packWARN(WARN_VOID),
				   "Too late to run INIT block");
		Perl_av_create_and_push(aTHX_ &PL_initav, MUTABLE_SV(cv));
	    }
	    else
		return FALSE;
	} else
	    return FALSE;
	DEBUG_x( dump_sub(gv) );
	(void)CvGV(cv);
	GvCV_set(gv,0);		/* cv has been hijacked */
	return FALSE;
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

Creates a constant sub equivalent to Perl S<C<sub FOO () { 123 }>> which is
eligible for inlining at compile-time.

Currently, the only useful value for C<flags> is C<SVf_UTF8>.

The newly created subroutine takes ownership of a reference to the passed in
SV.

Passing C<NULL> for SV creates a constant sub equivalent to S<C<sub BAR () {}>>,
which won't be called if used as a destructor, but will suppress the overhead
of a call to C<AUTOLOAD>.  (This form, however, isn't eligible for inlining at
compile time.)

=cut
*/

CV *
Perl_newCONSTSUB_flags(pTHX_ HV *stash, const char *name, STRLEN len,
                             U32 flags, SV *sv)
{
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

/*
=for apidoc U||newXS

Used by C<xsubpp> to hook up XSUBs as Perl subs.  C<filename> needs to be
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
Perl_newXS_deffile(pTHX_ const char *name, XSUBADDR_t subaddr)
{
    PERL_ARGS_ASSERT_NEWXS_DEFFILE;
    return newXS_len_flags(
        name, strlen(name), subaddr, NULL, NULL, NULL, 0
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

        CvGV_set(cv, gv);
        if(filename) {
            /* XSUBs can't be perl lang/perl5db.pl debugged
            if (PERLDB_LINE_OR_SAVESRC)
                (void)gv_fetchfile(filename); */
            assert(!CvDYNFILE(cv)); /* cv_undef should have turned it off */
            if (flags & XS_DYNAMIC_FILENAME) {
                CvDYNFILE_on(cv);
                CvFILE(cv) = savepv(filename);
            } else {
            /* NOTE: not copied, as it is expected to be an external constant string */
                CvFILE(cv) = (char *)filename;
            }
        } else {
            assert((flags & XS_DYNAMIC_FILENAME) == 0 && PL_xsubfilename);
            CvFILE(cv) = (char*)PL_xsubfilename;
        }
        CvISXSUB_on(cv);
        CvXSUB(cv) = subaddr;
#ifndef PERL_IMPLICIT_CONTEXT
        CvHSCXT(cv) = &PL_stack_sp;
#else
        PoisonPADLIST(cv);
#endif

        if (name)
            process_special_blocks(0, name, gv, cv);
        else
            CvANON_on(cv);
    } /* <- not a conditional branch */


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

void
Perl_newFORM(pTHX_ I32 floor, OP *o, OP *block)
{
    CV *cv;

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
    S_prune_chain_head(&CvSTART(cv));
    cv_forget_slab(cv);

  finish:
    op_free(o);
    if (PL_parser)
	PL_parser->copline = NOLINE;
    LEAVE_SCOPE(floor);
    PL_compiling.cop_seq = 0;
}

OP *
Perl_newANONLIST(pTHX_ OP *o)
{
    return op_convert_list(OP_ANONLIST, OPf_SPECIAL, o);
}

OP *
Perl_newANONHASH(pTHX_ OP *o)
{
    return op_convert_list(OP_ANONHASH, OPf_SPECIAL, o);
}

OP *
Perl_newANONSUB(pTHX_ I32 floor, OP *proto, OP *block)
{
    return newANONATTRSUB(floor, proto, NULL, block);
}

OP *
Perl_newANONATTRSUB(pTHX_ I32 floor, OP *proto, OP *attrs, OP *block)
{
    SV * const cv = MUTABLE_SV(newATTRSUB(floor, 0, proto, attrs, block));
    OP * anoncode = 
	newSVOP(OP_ANONCODE, 0,
		cv);
    if (CvANONCONST(cv))
	anoncode = newUNOP(OP_ANONCONST, 0,
			   op_convert_list(OP_ENTERSUB,
					   OPf_STACKED|OPf_WANT_SCALAR,
					   anoncode));
    return newUNOP(OP_REFGEN, 0, anoncode);
}

OP *
Perl_oopsAV(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_OOPSAV;

    switch (o->op_type) {
    case OP_PADSV:
    case OP_PADHV:
        OpTYPE_set(o, OP_PADAV);
	return ref(o, OP_RV2AV);

    case OP_RV2SV:
    case OP_RV2HV:
        OpTYPE_set(o, OP_RV2AV);
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
        OpTYPE_set(o, OP_PADHV);
	return ref(o, OP_RV2HV);

    case OP_RV2SV:
    case OP_RV2AV:
        OpTYPE_set(o, OP_RV2HV);
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
        OpTYPE_set(o, OP_PADAV);
	return o;
    }
    else if ((o->op_type == OP_RV2AV || o->op_type == OP_PADAV)) {
	Perl_croak(aTHX_ "Can't use an array as a reference");
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
        OpTYPE_set(o, OP_PADHV);
	return o;
    }
    else if ((o->op_type == OP_RV2HV || o->op_type == OP_PADHV)) {
	Perl_croak(aTHX_ "Can't use a hash as a reference");
    }
    return newUNOP(OP_RV2HV, 0, scalar(o));
}

OP *
Perl_newCVREF(pTHX_ I32 flags, OP *o)
{
    if (o->op_type == OP_PADANY) {
	dVAR;
        OpTYPE_set(o, OP_PADCV);
    }
    return newUNOP(OP_RV2CV, flags, scalar(o));
}

OP *
Perl_newSVREF(pTHX_ OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_NEWSVREF;

    if (o->op_type == OP_PADANY) {
        OpTYPE_set(o, OP_PADSV);
        scalar(o);
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
    cSVOPo->op_sv = NULL;
    return o;
}

static void
S_io_hints(pTHX_ OP *o)
{
#if O_BINARY != 0 || O_TEXT != 0
    HV * const table =
	PL_hints & HINT_LOCALIZE_HH ? GvHV(PL_hintgv) : NULL;;
    if (table) {
	SV **svp = hv_fetchs(table, "open_IN", FALSE);
	if (svp && *svp) {
	    STRLEN len = 0;
	    const char *d = SvPV_const(*svp, len);
	    const I32 mode = mode_from_discipline(d, len);
            /* bit-and:ing with zero O_BINARY or O_TEXT would be useless. */
#  if O_BINARY != 0
	    if (mode & O_BINARY)
		o->op_private |= OPpOPEN_IN_RAW;
#  endif
#  if O_TEXT != 0
	    if (mode & O_TEXT)
		o->op_private |= OPpOPEN_IN_CRLF;
#  endif
	}

	svp = hv_fetchs(table, "open_OUT", FALSE);
	if (svp && *svp) {
	    STRLEN len = 0;
	    const char *d = SvPV_const(*svp, len);
	    const I32 mode = mode_from_discipline(d, len);
            /* bit-and:ing with zero O_BINARY or O_TEXT would be useless. */
#  if O_BINARY != 0
	    if (mode & O_BINARY)
		o->op_private |= OPpOPEN_OUT_RAW;
#  endif
#  if O_TEXT != 0
	    if (mode & O_TEXT)
		o->op_private |= OPpOPEN_OUT_CRLF;
#  endif
	}
    }
#else
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(o);
#endif
}

OP *
Perl_ck_backtick(pTHX_ OP *o)
{
    GV *gv;
    OP *newop = NULL;
    OP *sibl;
    PERL_ARGS_ASSERT_CK_BACKTICK;
    /* qx and `` have a null pushmark; CORE::readpipe has only one kid. */
    if (o->op_flags & OPf_KIDS && (sibl = OpSIBLING(cUNOPo->op_first))
     && (gv = gv_override("readpipe",8)))
    {
        /* detach rest of siblings from o and its first child */
        op_sibling_splice(o, cUNOPo->op_first, -1, NULL);
	newop = S_new_entersubop(aTHX_ gv, sibl);
    }
    else if (!(o->op_flags & OPf_KIDS))
	newop = newUNOP(OP_BACKTICK, 0,	newDEFSVOP());
    if (newop) {
	op_free(o);
	return newop;
    }
    S_io_hints(aTHX_ o);
    return o;
}

OP *
Perl_ck_bitop(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_BITOP;

    o->op_private = (U8)(PL_hints & HINT_INTEGER);

    if (o->op_type == OP_NBIT_OR     || o->op_type == OP_SBIT_OR
     || o->op_type == OP_NBIT_XOR    || o->op_type == OP_SBIT_XOR
     || o->op_type == OP_NBIT_AND    || o->op_type == OP_SBIT_AND
     || o->op_type == OP_NCOMPLEMENT || o->op_type == OP_SCOMPLEMENT)
	Perl_ck_warner_d(aTHX_ packWARN(WARN_EXPERIMENTAL__BITWISE),
			      "The bitwise feature is experimental");
    if (!(o->op_flags & OPf_STACKED) /* Not an assignment */
	    && OP_IS_INFIX_BIT(o->op_type))
    {
	const OP * const left = cBINOPo->op_first;
	const OP * const right = OpSIBLING(left);
	if ((OP_IS_NUMCOMPARE(left->op_type) &&
		(left->op_flags & OPf_PARENS) == 0) ||
	    (OP_IS_NUMCOMPARE(right->op_type) &&
		(right->op_flags & OPf_PARENS) == 0))
	    Perl_ck_warner(aTHX_ packWARN(WARN_PRECEDENCE),
			  "Possible precedence problem on bitwise %s operator",
			   o->op_type ==  OP_BIT_OR
			 ||o->op_type == OP_NBIT_OR  ? "|"
			:  o->op_type ==  OP_BIT_AND
			 ||o->op_type == OP_NBIT_AND ? "&"
			:  o->op_type ==  OP_BIT_XOR
			 ||o->op_type == OP_NBIT_XOR ? "^"
			:  o->op_type == OP_SBIT_OR  ? "|."
			:  o->op_type == OP_SBIT_AND ? "&." : "^."
			   );
    }
    return o;
}

PERL_STATIC_INLINE bool
is_dollar_bracket(pTHX_ const OP * const o)
{
    const OP *kid;
    PERL_UNUSED_CONTEXT;
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
	if (kid &&
            (
		(   is_dollar_bracket(aTHX_ kid)
                 && OpSIBLING(kid) && OpSIBLING(kid)->op_type == OP_CONST
		)
	     || (   kid->op_type == OP_CONST
		 && (kid = OpSIBLING(kid)) && is_dollar_bracket(aTHX_ kid)
                )
	   )
        )
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
        OP* kidkid;
	const OPCODE type = o->op_type;
	o = modkids(ck_fun(o), type);
	kid    = cUNOPo->op_first;
	kidkid = kUNOP->op_first;
	newop = OpSIBLING(kidkid);
	if (newop) {
	    const OPCODE type = newop->op_type;
	    if (OpHAS_SIBLING(newop))
		return o;
	    if (o->op_type == OP_REFGEN
	     && (  type == OP_RV2CV
		|| (  !(newop->op_flags & OPf_PARENS)
		   && (  type == OP_RV2AV || type == OP_PADAV
		      || type == OP_RV2HV || type == OP_PADHV))))
	    	NOOP; /* OK (allow srefgen for \@a and \%h) */
	    else if (OP_GIMME(newop,0) != G_SCALAR)
		return o;
	}
        /* excise first sibling */
        op_sibling_splice(kid, NULL, 1, NULL);
	op_free(kidkid);
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
	    /* FALLTHROUGH */
	case OP_HSLICE:
	    o->op_private |= OPpSLICE;
	    break;
	case OP_AELEM:
	    o->op_flags |= OPf_SPECIAL;
	    /* FALLTHROUGH */
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
    PERL_ARGS_ASSERT_CK_EOF;

    if (o->op_flags & OPf_KIDS) {
	OP *kid;
	if (cLISTOPo->op_first->op_type == OP_STUB) {
	    OP * const newop
		= newUNOP(o->op_type, OPf_SPECIAL, newGVOP(OP_GV, 0, PL_argvgv));
	    op_free(o);
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

	if (o->op_type == OP_ENTERTRY) {
	    LOGOP *enter;

            /* cut whole sibling chain free from o */
            op_sibling_splice(o, NULL, -1, NULL);
	    op_free(o);

            enter = S_alloc_LOGOP(aTHX_ OP_ENTERTRY, NULL, NULL);

	    /* establish postfix order */
	    enter->op_next = (OP*)enter;

	    o = op_prepend_elem(OP_LINESEQ, (OP*)enter, (OP*)kid);
            OpTYPE_set(o, OP_LEAVETRY);
	    enter->op_other = o;
	    return o;
	}
	else {
	    scalar((OP*)kid);
	    S_set_haseval(aTHX);
	}
    }
    else {
	const U8 priv = o->op_private;
	op_free(o);
        /* the newUNOP will recursively call ck_eval(), which will handle
         * all the stuff at the end of this function, like adding
         * OP_HINTSEVAL
         */
	return newUNOP(OP_ENTEREVAL, priv <<8, newDEFSVOP());
    }
    o->op_targ = (PADOFFSET)PL_hints;
    if (o->op_private & OPpEVAL_BYTES) o->op_targ &= ~HINT_UTF8;
    if ((PL_hints & HINT_LOCALIZE_HH) != 0
     && !(o->op_private & OPpEVAL_COPHH) && GvHV(PL_hintgv)) {
	/* Store a copy of %^H that pp_entereval can pick up. */
	OP *hhop = newSVOP(OP_HINTSEVAL, 0,
			   MUTABLE_SV(hv_copy_hints_hv(GvHV(PL_hintgv))));
        /* append hhop to only child  */
        op_sibling_splice(o, cUNOPo->op_first, 0, hhop);

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
	kid = OpSIBLING(cUNOPo->op_first);
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

    if (kid->op_type == OP_CONST) {
	int iscv;
	GV *gv;
	SV * const kidsv = kid->op_sv;

	/* Is it a constant from cv_const_sv()? */
	if ((SvROK(kidsv) || isGV_with_GP(kidsv)) && SvREADONLY(kidsv)) {
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
	iscv = o->op_type == OP_RV2CV ? GV_NOEXPAND|GV_ADDMULTI : 0;
	gv = gv_fetchsv(kidsv,
		o->op_type == OP_RV2CV
			&& o->op_private & OPpMAY_RETURN_CONSTANT
		    ? GV_NOEXPAND
		    : iscv | !(kid->op_private & OPpCONST_ENTERED),
		iscv
		    ? SVt_PVCV
		    : o->op_type == OP_RV2SV
			? SVt_PV
			: o->op_type == OP_RV2AV
			    ? SVt_PVAV
			    : o->op_type == OP_RV2HV
				? SVt_PVHV
				: SVt_PVGV);
	if (gv) {
	    if (!isGV(gv)) {
		assert(iscv);
		assert(SvROK(gv));
		if (!(o->op_private & OPpMAY_RETURN_CONSTANT)
		  && SvTYPE(SvRV(gv)) != SVt_PVCV)
		    gv_fetchsv(kidsv, GV_ADDMULTI, SVt_PVCV);
	    }
            OpTYPE_set(kid, OP_GV);
	    SvREFCNT_dec(kid->op_sv);
#ifdef USE_ITHREADS
	    /* XXX hack: dependence on sizeof(PADOP) <= sizeof(SVOP) */
	    STATIC_ASSERT_STMT(sizeof(PADOP) <= sizeof(SVOP));
	    kPADOP->op_padix = pad_alloc(OP_GV, SVf_READONLY);
	    SvREFCNT_dec(PAD_SVl(kPADOP->op_padix));
	    PAD_SETSV(kPADOP->op_padix, MUTABLE_SV(SvREFCNT_inc_simple_NN(gv)));
#else
	    kid->op_sv = SvREFCNT_inc_simple_NN(gv);
#endif
	    kid->op_private = 0;
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
	    op_free(o);
	    return newop;
	}

        if ((kidtype == OP_RV2AV || kidtype == OP_PADAV) && ckWARN(WARN_SYNTAX)) {
            SV *name = S_op_varname_subscript(aTHX_ (OP*)kid, 2);
            if (name) {
                /* diag_listed_as: Array passed to stat will be coerced to a scalar%s */
                Perl_warner(aTHX_ packWARN(WARN_SYNTAX), "%s (did you want stat %" SVf "?)",
                            array_passed_to_stat, name);
            }
            else {
                /* diag_listed_as: Array passed to stat will be coerced to a scalar%s */
                Perl_warner(aTHX_ packWARN(WARN_SYNTAX), "%s", array_passed_to_stat);
            }
       }
	scalar((OP *) kid);
	if ((PL_hints & HINT_FILETEST_ACCESS) && OP_IS_FILETEST_ACCESS(o->op_type))
	    o->op_private |= OPpFT_ACCESS;
	if (type != OP_STAT && type != OP_LSTAT
            && PL_check[kidtype] == Perl_ck_ftst
            && kidtype != OP_STAT && kidtype != OP_LSTAT
        ) {
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
	op_free(o);
	if (type == OP_FTTTY)
	    o = newGVOP(type, OPf_REF, PL_stdingv);
	else
	    o = newUNOP(type, 0, newDEFSVOP());
    }
    return o;
}

OP *
Perl_ck_fun(pTHX_ OP *o)
{
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
        OP *prev_kid = NULL;
        OP *kid = cLISTOPo->op_first;
        I32 numargs = 0;
	bool seen_optional = FALSE;

	if (kid->op_type == OP_PUSHMARK ||
	    (kid->op_type == OP_NULL && kid->op_targ == OP_PUSHMARK))
	{
	    prev_kid = kid;
	    kid = OpSIBLING(kid);
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
		if (!kid && !seen_optional && PL_opargs[type] & OA_DEFGV) {
		    kid = newDEFSVOP();
                    /* append kid to chain */
                    op_sibling_splice(o, prev_kid, 0, kid);
                }
		seen_optional = TRUE;
	    }
	    if (!kid) break;

	    numargs++;
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
		    && !OpHAS_SIBLING(kid))
		    Perl_ck_warner(aTHX_ packWARN(WARN_SYNTAX),
				   "Useless use of %s with no values",
				   PL_op_desc[type]);

		if (kid->op_type == OP_CONST
		      && (  !SvROK(cSVOPx_sv(kid)) 
		         || SvTYPE(SvRV(cSVOPx_sv(kid))) != SVt_PVAV  )
		        )
		    bad_type_pv(numargs, "array", o, kid);
		else if (kid->op_type != OP_RV2AV && kid->op_type != OP_PADAV) {
                    yyerror_pv(Perl_form(aTHX_ "Experimental %s on scalar is now forbidden",
                                         PL_op_desc[type]), 0);
		}
                else {
                    op_lvalue(kid, type);
                }
		break;
	    case OA_HVREF:
		if (kid->op_type != OP_RV2HV && kid->op_type != OP_PADHV)
		    bad_type_pv(numargs, "hash", o, kid);
		op_lvalue(kid, type);
		break;
	    case OA_CVREF:
		{
                    /* replace kid with newop in chain */
		    OP * const newop =
                        S_op_sibling_newUNOP(aTHX_ o, prev_kid, OP_NULL, 0);
		    newop->op_next = newop;
		    kid = newop;
		}
		break;
	    case OA_FILEREF:
		if (kid->op_type != OP_GV && kid->op_type != OP_RV2GV) {
		    if (kid->op_type == OP_CONST &&
			(kid->op_private & OPpCONST_BARE))
		    {
			OP * const newop = newGVOP(OP_GV, 0,
			    gv_fetchsv(((SVOP*)kid)->op_sv, GV_ADD, SVt_PVIO));
                        /* replace kid with newop in chain */
                        op_sibling_splice(o, prev_kid, 1, newop);
			op_free(kid);
			kid = newop;
		    }
		    else if (kid->op_type == OP_READLINE) {
			/* neophyte patrol: open(<FH>), close(<FH>) etc. */
			bad_type_pv(numargs, "HANDLE", o, kid);
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
				PADNAME * const pn
				    = PAD_COMPNAME_SV(kid->op_targ);
				name = PadnamePV (pn);
				len  = PadnameLEN(pn);
				name_utf8 = PadnameUTF8(pn);
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
                        scalar(kid);
                        kid = S_op_sibling_newUNOP(aTHX_ o, prev_kid,
                                    OP_RV2GV, flags);
                        kid->op_targ = targ;
                        kid->op_private |= priv;
		    }
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
	    prev_kid = kid;
	    kid = OpSIBLING(kid);
	}
	/* FIXME - should the numargs or-ing move after the too many
         * arguments check? */
	o->op_private |= numargs;
	if (kid)
	    return too_many_arguments_pv(o,OP_DESC(o), 0);
	listkids(o);
    }
    else if (PL_opargs[type] & OA_DEFGV) {
	/* Ordering of these two is important to keep f_map.t passing.  */
	op_free(o);
	return newUNOP(type, 0, newDEFSVOP());
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
    GV *gv;

    PERL_ARGS_ASSERT_CK_GLOB;

    o = ck_fun(o);
    if ((o->op_flags & OPf_KIDS) && !OpHAS_SIBLING(cLISTOPo->op_first))
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
    LOGOP *gwop;
    OP *kid;
    const OPCODE type = o->op_type == OP_GREPSTART ? OP_GREPWHILE : OP_MAPWHILE;

    PERL_ARGS_ASSERT_CK_GREP;

    /* don't allocate gwop here, as we may leak it if PL_parser->error_count > 0 */

    if (o->op_flags & OPf_STACKED) {
	kid = cUNOPx(OpSIBLING(cLISTOPo->op_first))->op_first;
	if (kid->op_type != OP_SCOPE && kid->op_type != OP_LEAVE)
	    return no_fh_allowed(o);
	o->op_flags &= ~OPf_STACKED;
    }
    kid = OpSIBLING(cLISTOPo->op_first);
    if (type == OP_MAPWHILE)
	list(kid);
    else
	scalar(kid);
    o = ck_fun(o);
    if (PL_parser && PL_parser->error_count)
	return o;
    kid = OpSIBLING(cLISTOPo->op_first);
    if (kid->op_type != OP_NULL)
	Perl_croak(aTHX_ "panic: ck_grep, type=%u", (unsigned) kid->op_type);
    kid = kUNOP->op_first;

    gwop = S_alloc_LOGOP(aTHX_ type, o, LINKLIST(kid));
    kid->op_next = (OP*)gwop;
    o->op_private = gwop->op_private = 0;
    gwop->op_targ = pad_alloc(type, SVs_PADTMP);

    kid = OpSIBLING(cLISTOPo->op_first);
    for (kid = OpSIBLING(kid); kid; kid = OpSIBLING(kid))
	op_lvalue(kid, OP_GREPSTART);

    return (OP*)gwop;
}

OP *
Perl_ck_index(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_INDEX;

    if (o->op_flags & OPf_KIDS) {
	OP *kid = OpSIBLING(cLISTOPo->op_first);	/* get past pushmark */
	if (kid)
	    kid = OpSIBLING(kid);			/* get past "big" */
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
	    Perl_croak(aTHX_ "Can't use 'defined(@array)'"
			     " (Maybe you should just omit the defined()?)");
	break;
	case OP_RV2HV:
	case OP_PADHV:
	    Perl_croak(aTHX_ "Can't use 'defined(%%hash)'"
			     " (Maybe you should just omit the defined()?)");
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
	op_free(o);
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
	o = force_list(o, 1);
	kid = cLISTOPo->op_first;
    }
    if (kid->op_type == OP_PUSHMARK)
	kid = OpSIBLING(kid);
    if (kid && o->op_flags & OPf_STACKED)
	kid = OpSIBLING(kid);
    else if (kid && !OpHAS_SIBLING(kid)) {		/* print HANDLE; */
	if (kid->op_type == OP_CONST && kid->op_private & OPpCONST_BARE
	 && !kid->op_folded) {
	    o->op_flags |= OPf_STACKED;	/* make it a filehandle */
            scalar(kid);
            /* replace old const op with new OP_RV2GV parent */
            kid = S_op_sibling_newUNOP(aTHX_ o, cLISTOPo->op_first,
                                        OP_RV2GV, OPf_REF);
            kid = OpSIBLING(kid);
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
	OP *second = OpSIBLING(first);
	
	/* Implicitly take a reference to an array or hash */

        /* remove the original two siblings, then add back the
         * (possibly different) first and second sibs.
         */
        op_sibling_splice(o, NULL, 1, NULL);
        op_sibling_splice(o, NULL, 1, NULL);
	first  = ref_array_or_hash(first);
	second = ref_array_or_hash(second);
        op_sibling_splice(o, NULL, 0, second);
        op_sibling_splice(o, NULL, 0, first);
	
	/* Implicitly take a reference to a regular expression */
	if (first->op_type == OP_MATCH) {
            OpTYPE_set(first, OP_QR);
	}
	if (second->op_type == OP_MATCH) {
            OpTYPE_set(second, OP_QR);
        }
    }
    
    return o;
}


static OP *
S_maybe_targlex(pTHX_ OP *o)
{
    OP * const kid = cLISTOPo->op_first;
    /* has a disposable target? */
    if ((PL_opargs[kid->op_type] & OA_TARGLEX)
	&& !(kid->op_flags & OPf_STACKED)
	/* Cannot steal the second time! */
	&& !(kid->op_private & OPpTARGET_MY)
	)
    {
	OP * const kkid = OpSIBLING(kid);

	/* Can just relocate the target. */
	if (kkid && kkid->op_type == OP_PADSV
	    && (!(kkid->op_private & OPpLVAL_INTRO)
	       || kkid->op_private & OPpPAD_STATE))
	{
	    kid->op_targ = kkid->op_targ;
	    kkid->op_targ = 0;
	    /* Now we do not need PADSV and SASSIGN.
	     * Detach kid and free the rest. */
	    op_sibling_splice(o, NULL, 1, NULL);
	    op_free(o);
	    kid->op_private |= OPpTARGET_MY;	/* Used for context settings */
	    return kid;
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

    if (OpHAS_SIBLING(kid)) {
	OP *kkid = OpSIBLING(kid);
	/* For state variable assignment with attributes, kkid is a list op
	   whose op_last is a padsv. */
	if ((kkid->op_type == OP_PADSV ||
	     (OP_TYPE_IS_OR_WAS(kkid, OP_LIST) &&
	      (kkid = cLISTOPx(kkid)->op_last)->op_type == OP_PADSV
	     )
	    )
		&& (kkid->op_private & (OPpLVAL_INTRO|OPpPAD_STATE))
		    == (OPpLVAL_INTRO|OPpPAD_STATE)) {
	    const PADOFFSET target = kkid->op_targ;
	    OP *const other = newOP(OP_PADSV,
				    kkid->op_flags
				    | ((kkid->op_private & ~OPpLVAL_INTRO) << 8));
	    OP *const first = newOP(OP_NULL, 0);
	    OP *const nullop =
		newCONDOP(0, first, o, other);
	    /* XXX targlex disabled for now; see ticket #124160
		newCONDOP(0, first, S_maybe_targlex(aTHX_ o), other);
	     */
	    OP *const condop = first->op_next;

            OpTYPE_set(condop, OP_ONCE);
	    other->op_targ = target;
	    nullop->op_flags |= OPf_WANT_SCALAR;

	    /* Store the initializedness of state vars in a separate
	       pad entry.  */
	    condop->op_targ =
	      pad_add_name_pvn("$",1,padadd_NO_DUP_CHECK|padadd_STATE,0,0);
	    /* hijacking PADSTALE for uninitialized state variables */
	    SvPADSTALE_on(PAD_SVl(condop->op_targ));

	    return nullop;
	}
    }
    return S_maybe_targlex(aTHX_ o);
}

OP *
Perl_ck_match(pTHX_ OP *o)
{
    PERL_UNUSED_CONTEXT;
    PERL_ARGS_ASSERT_CK_MATCH;

    if (o->op_type == OP_MATCH || o->op_type == OP_QR)
	o->op_private |= OPpRUNTIME;
    return o;
}

OP *
Perl_ck_method(pTHX_ OP *o)
{
    SV *sv, *methsv, *rclass;
    const char* method;
    char* compatptr;
    int utf8;
    STRLEN len, nsplit = 0, i;
    OP* new_op;
    OP * const kid = cUNOPo->op_first;

    PERL_ARGS_ASSERT_CK_METHOD;
    if (kid->op_type != OP_CONST) return o;

    sv = kSVOP->op_sv;

    /* replace ' with :: */
    while ((compatptr = strchr(SvPVX(sv), '\''))) {
        *compatptr = ':';
        sv_insert(sv, compatptr - SvPVX_const(sv), 0, ":", 1);
    }

    method = SvPVX_const(sv);
    len = SvCUR(sv);
    utf8 = SvUTF8(sv) ? -1 : 1;

    for (i = len - 1; i > 0; --i) if (method[i] == ':') {
        nsplit = i+1;
        break;
    }

    methsv = newSVpvn_share(method+nsplit, utf8*(len - nsplit), 0);

    if (!nsplit) { /* $proto->method() */
        op_free(o);
        return newMETHOP_named(OP_METHOD_NAMED, 0, methsv);
    }

    if (nsplit == 7 && memEQ(method, "SUPER::", nsplit)) { /* $proto->SUPER::method() */
        op_free(o);
        return newMETHOP_named(OP_METHOD_SUPER, 0, methsv);
    }

    /* $proto->MyClass::method() and $proto->MyClass::SUPER::method() */
    if (nsplit >= 9 && strnEQ(method+nsplit-9, "::SUPER::", 9)) {
        rclass = newSVpvn_share(method, utf8*(nsplit-9), 0);
        new_op = newMETHOP_named(OP_METHOD_REDIR_SUPER, 0, methsv);
    } else {
        rclass = newSVpvn_share(method, utf8*(nsplit-2), 0);
        new_op = newMETHOP_named(OP_METHOD_REDIR, 0, methsv);
    }
#ifdef USE_ITHREADS
    op_relocate_sv(&rclass, &cMETHOPx(new_op)->op_rclass_targ);
#else
    cMETHOPx(new_op)->op_rclass_sv = rclass;
#endif
    op_free(o);
    return new_op;
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
	     (oa = OpSIBLING(first)) &&		/* The fh. */
	     (oa = OpSIBLING(oa)) &&			/* The mode. */
	     (oa->op_type == OP_CONST) &&
	     SvPOK(((SVOP*)oa)->op_sv) &&
	     (mode = SvPVX_const(((SVOP*)oa)->op_sv)) &&
	     mode[0] == '>' && mode[1] == '&' &&	/* A dup open. */
	     (last == OpSIBLING(oa)))			/* The bareword. */
	      last->op_private &= ~OPpCONST_STRICT;
    }
    return ck_fun(o);
}

OP *
Perl_ck_prototype(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_PROTOTYPE;
    if (!(o->op_flags & OPf_KIDS)) {
	op_free(o);
	return newUNOP(OP_PROTOTYPE, 0, newDEFSVOP());
    }
    return o;
}

OP *
Perl_ck_refassign(pTHX_ OP *o)
{
    OP * const right = cLISTOPo->op_first;
    OP * const left = OpSIBLING(right);
    OP *varop = cUNOPx(cUNOPx(left)->op_first)->op_first;
    bool stacked = 0;

    PERL_ARGS_ASSERT_CK_REFASSIGN;
    assert (left);
    assert (left->op_type == OP_SREFGEN);

    o->op_private = 0;
    /* we use OPpPAD_STATE in refassign to mean either of those things,
     * and the code assumes the two flags occupy the same bit position
     * in the various ops below */
    assert(OPpPAD_STATE == OPpOUR_INTRO);

    switch (varop->op_type) {
    case OP_PADAV:
	o->op_private |= OPpLVREF_AV;
	goto settarg;
    case OP_PADHV:
	o->op_private |= OPpLVREF_HV;
        /* FALLTHROUGH */
    case OP_PADSV:
      settarg:
        o->op_private |= (varop->op_private & (OPpLVAL_INTRO|OPpPAD_STATE));
	o->op_targ = varop->op_targ;
	varop->op_targ = 0;
	PAD_COMPNAME_GEN_set(o->op_targ, PERL_INT_MAX);
	break;

    case OP_RV2AV:
	o->op_private |= OPpLVREF_AV;
	goto checkgv;
        NOT_REACHED; /* NOTREACHED */
    case OP_RV2HV:
	o->op_private |= OPpLVREF_HV;
        /* FALLTHROUGH */
    case OP_RV2SV:
      checkgv:
        o->op_private |= (varop->op_private & (OPpLVAL_INTRO|OPpOUR_INTRO));
	if (cUNOPx(varop)->op_first->op_type != OP_GV) goto bad;
      detach_and_stack:
	/* Point varop to its GV kid, detached.  */
	varop = op_sibling_splice(varop, NULL, -1, NULL);
	stacked = TRUE;
	break;
    case OP_RV2CV: {
	OP * const kidparent =
	    OpSIBLING(cUNOPx(cUNOPx(varop)->op_first)->op_first);
	OP * const kid = cUNOPx(kidparent)->op_first;
	o->op_private |= OPpLVREF_CV;
	if (kid->op_type == OP_GV) {
	    varop = kidparent;
	    goto detach_and_stack;
	}
	if (kid->op_type != OP_PADCV)	goto bad;
	o->op_targ = kid->op_targ;
	kid->op_targ = 0;
	break;
    }
    case OP_AELEM:
    case OP_HELEM:
        o->op_private |= (varop->op_private & OPpLVAL_INTRO);
	o->op_private |= OPpLVREF_ELEM;
	op_null(varop);
	stacked = TRUE;
	/* Detach varop.  */
	op_sibling_splice(cUNOPx(left)->op_first, NULL, -1, NULL);
	break;
    default:
      bad:
	/* diag_listed_as: Can't modify reference to %s in %s assignment */
	yyerror(Perl_form(aTHX_ "Can't modify reference to %s in scalar "
				"assignment",
				 OP_DESC(varop)));
	return o;
    }
    if (!FEATURE_REFALIASING_IS_ENABLED)
	Perl_croak(aTHX_
		  "Experimental aliasing via reference not enabled");
    Perl_ck_warner_d(aTHX_
		     packWARN(WARN_EXPERIMENTAL__REFALIASING),
		    "Aliasing via reference is experimental");
    if (stacked) {
	o->op_flags |= OPf_STACKED;
	op_sibling_splice(o, right, 1, varop);
    }
    else {
	o->op_flags &=~ OPf_STACKED;
	op_sibling_splice(o, right, 1, NULL);
    }
    op_free(left);
    return o;
}

OP *
Perl_ck_repeat(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CK_REPEAT;

    if (cBINOPo->op_first->op_flags & OPf_PARENS) {
        OP* kids;
	o->op_private |= OPpREPEAT_DOLIST;
        kids = op_sibling_splice(o, NULL, 1, NULL); /* detach first kid */
        kids = force_list(kids, 1); /* promote it to a list */
        op_sibling_splice(o, NULL, 0, kids); /* and add back */
    }
    else
	scalar(o);
    return o;
}

OP *
Perl_ck_require(pTHX_ OP *o)
{
    GV* gv;

    PERL_ARGS_ASSERT_CK_REQUIRE;

    if (o->op_flags & OPf_KIDS) {	/* Shall we supply missing .pm? */
	SVOP * const kid = (SVOP*)cUNOPo->op_first;
	HEK *hek;
	U32 hash;
	char *s;
	STRLEN len;
	if (kid->op_type == OP_CONST) {
	  SV * const sv = kid->op_sv;
	  U32 const was_readonly = SvREADONLY(sv);
	  if (kid->op_private & OPpCONST_BARE) {
            dVAR;
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
	    PERL_HASH(hash, SvPVX(sv), SvCUR(sv));
	    hek = share_hek(SvPVX(sv),
			    (SSize_t)SvCUR(sv) * (SvUTF8(sv) ? -1 : 1),
			    hash);
	    sv_sethek(sv, hek);
	    unshare_hek(hek);
	    SvFLAGS(sv) |= was_readonly;
	  }
	  else if (SvPOK(sv) && !SvNIOK(sv) && !SvGMAGICAL(sv)
		&& !SvVOK(sv)) {
	    s = SvPV(sv, len);
	    if (SvREFCNT(sv) > 1) {
		kid->op_sv = newSVpvn_share(
		    s, SvUTF8(sv) ? -(SSize_t)len : (SSize_t)len, 0);
		SvREFCNT_dec_NN(sv);
	    }
	    else {
                dVAR;
		if (was_readonly) SvREADONLY_off(sv);
		PERL_HASH(hash, s, len);
		hek = share_hek(s,
				SvUTF8(sv) ? -(SSize_t)len : (SSize_t)len,
				hash);
		sv_sethek(sv, hek);
		unshare_hek(hek);
		SvFLAGS(sv) |= was_readonly;
	    }
	  }
	}
    }

    if (!(o->op_flags & OPf_SPECIAL) /* Wasn't written as CORE::require */
	/* handle override, if any */
     && (gv = gv_override("require", 7))) {
	OP *kid, *newop;
	if (o->op_flags & OPf_KIDS) {
	    kid = cUNOPo->op_first;
            op_sibling_splice(o, NULL, -1, NULL);
	}
	else {
	    kid = newDEFSVOP();
	}
	op_free(o);
	newop = S_new_entersubop(aTHX_ gv, kid);
	return newop;
    }

    return ck_fun(o);
}

OP *
Perl_ck_return(pTHX_ OP *o)
{
    OP *kid;

    PERL_ARGS_ASSERT_CK_RETURN;

    kid = OpSIBLING(cLISTOPo->op_first);
    if (CvLVALUE(PL_compcv)) {
	for (; kid; kid = OpSIBLING(kid))
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
        kid = OpSIBLING(cLISTOPo->op_first);     /* get past pushmark */
        if (kid && OpHAS_SIBLING(kid)) {
            OpTYPE_set(o, OP_SSELECT);
	    o = ck_fun(o);
	    return fold_constants(op_integerize(op_std_init(o)));
	}
    }
    o = ck_fun(o);
    kid = OpSIBLING(cLISTOPo->op_first);    /* get past pushmark */
    if (kid && kid->op_type == OP_RV2GV)
	kid->op_private &= ~HINT_STRICT_REFS;
    return o;
}

OP *
Perl_ck_shift(pTHX_ OP *o)
{
    const I32 type = o->op_type;

    PERL_ARGS_ASSERT_CK_SHIFT;

    if (!(o->op_flags & OPf_KIDS)) {
	OP *argop;

	if (!CvUNIQUE(PL_compcv)) {
	    o->op_flags |= OPf_SPECIAL;
	    return o;
	}

	argop = newUNOP(OP_RV2AV, 0, scalar(newGVOP(OP_GV, 0, PL_argvgv)));
	op_free(o);
	return newUNOP(type, 0, scalar(argop));
    }
    return scalar(ck_fun(o));
}

OP *
Perl_ck_sort(pTHX_ OP *o)
{
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
    firstkid = OpSIBLING(cLISTOPo->op_first);		/* get past pushmark */

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
	else if (kid->op_type == OP_CONST
	      && kid->op_private & OPpCONST_BARE) {
	    char tmpbuf[256];
	    STRLEN len;
	    PADOFFSET off;
	    const char * const name = SvPV(kSVOP_sv, len);
	    *tmpbuf = '&';
	    assert (len < 256);
	    Copy(name, tmpbuf+1, len, char);
	    off = pad_findmy_pvn(tmpbuf, len+1, SvUTF8(kSVOP_sv));
	    if (off != NOT_IN_PAD) {
		if (PAD_COMPNAME_FLAGS_isOUR(off)) {
		    SV * const fq =
			newSVhek(HvNAME_HEK(PAD_COMPNAME_OURSTASH(off)));
		    sv_catpvs(fq, "::");
		    sv_catsv(fq, kSVOP_sv);
		    SvREFCNT_dec_NN(kSVOP_sv);
		    kSVOP->op_sv = fq;
		}
		else {
		    OP * const padop = newOP(OP_PADCV, 0);
		    padop->op_targ = off;
                    /* replace the const op with the pad op */
                    op_sibling_splice(firstkid, NULL, 1, padop);
		    op_free(kid);
		}
	    }
	}

	firstkid = OpSIBLING(firstkid);
    }

    for (kid = firstkid; kid; kid = OpSIBLING(kid)) {
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
    OP *kid = OpSIBLING(cLISTOPo->op_first);	/* get past pushmark */
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
		PADNAME * const name = PAD_COMPNAME(kid->op_targ);
		if (PadnameLEN(name) == 2 && *PadnamePV(name) == '$'
		 && (  PadnamePV(name)[1] == 'a'
		    || PadnamePV(name)[1] == 'b'  ))
		    /* diag_listed_as: "my %s" used in sort comparison */
		    Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
				     "\"%s %s\" used in sort comparison",
				      PadnameIsSTATE(name)
					? "state"
					: "my",
				      PadnamePV(name));
	    }
	} while ((kid = OpSIBLING(kid)));
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
    kid = OpSIBLING(cLISTOPo->op_first);
    /* cut out and delete old block (second sibling) */
    op_sibling_splice(o, cLISTOPo->op_first, 1, NULL);
    op_free(kid);
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
    /* delete leading NULL node, then add a CONST if no other nodes */
    op_sibling_splice(o, NULL, 1,
	OpHAS_SIBLING(kid) ? NULL : newSVOP(OP_CONST, 0, newSVpvs(" ")));
    op_free(kid);
    kid = cLISTOPo->op_first;

    if (kid->op_type != OP_MATCH || kid->op_flags & OPf_STACKED) {
        /* remove kid, and replace with new optree */
        op_sibling_splice(o, NULL, 1, NULL);
        /* OPf_SPECIAL is used to trigger split " " behavior */
        kid = pmruntime( newPMOP(OP_MATCH, OPf_SPECIAL), kid, NULL, 0, 0);
        op_sibling_splice(o, NULL, 0, kid);
    }
    OpTYPE_set(kid, OP_PUSHRE);
    /* target implies @ary=..., so wipe it */
    kid->op_targ = 0;
    scalar(kid);
    if (((PMOP *)kid)->op_pmflags & PMf_GLOBAL) {
      Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP),
		     "Use of /g modifier is meaningless in split");
    }

    if (!OpHAS_SIBLING(kid))
	op_append_elem(OP_SPLIT, o, newDEFSVOP());

    kid = OpSIBLING(kid);
    assert(kid);
    scalar(kid);

    if (!OpHAS_SIBLING(kid))
    {
	op_append_elem(OP_SPLIT, o, newSVOP(OP_CONST, 0, newSViv(0)));
	o->op_private |= OPpSPLIT_IMPLIM;
    }
    assert(OpHAS_SIBLING(kid));

    kid = OpSIBLING(kid);
    scalar(kid);

    if (OpHAS_SIBLING(kid))
	return too_many_arguments_pv(o,OP_DESC(o), 0);

    return o;
}

OP *
Perl_ck_stringify(pTHX_ OP *o)
{
    OP * const kid = OpSIBLING(cUNOPo->op_first);
    PERL_ARGS_ASSERT_CK_STRINGIFY;
    if ((   kid->op_type == OP_JOIN || kid->op_type == OP_QUOTEMETA
         || kid->op_type == OP_LC   || kid->op_type == OP_LCFIRST
         || kid->op_type == OP_UC   || kid->op_type == OP_UCFIRST)
	&& !OpHAS_SIBLING(kid)) /* syntax errs can leave extra children */
    {
	op_sibling_splice(o, cUNOPo->op_first, -1, NULL);
	op_free(o);
	return kid;
    }
    return ck_fun(o);
}
	
OP *
Perl_ck_join(pTHX_ OP *o)
{
    OP * const kid = OpSIBLING(cLISTOPo->op_first);

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
    if (kid
     && (kid->op_type == OP_CONST /* an innocent, unsuspicious separator */
	|| (kid->op_type == OP_PADSV && !(kid->op_private & OPpLVAL_INTRO))
	|| (  kid->op_type==OP_RV2SV && kUNOP->op_first->op_type == OP_GV
	   && !(kid->op_private & (OPpLVAL_INTRO|OPpOUR_INTRO)))))
    {
	const OP * const bairn = OpSIBLING(kid); /* the list */
	if (bairn && !OpHAS_SIBLING(bairn) /* single-item list */
	 && OP_GIMME(bairn,0) == G_SCALAR)
	{
	    OP * const ret = op_convert_list(OP_STRINGIFY, OPf_FOLDED,
				     op_sibling_splice(o, kid, 1, NULL));
	    op_free(o);
	    return ret;
	}
    }

    return ck_fun(o);
}

/*
=for apidoc Am|CV *|rv2cv_op_cv|OP *cvop|U32 flags

Examines an op, which is expected to identify a subroutine at runtime,
and attempts to determine at compile time which subroutine it identifies.
This is normally used during Perl compilation to determine whether
a prototype can be applied to a function call.  C<cvop> is the op
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

If C<flags> has the bit C<RV2CVOPCV_MARK_EARLY> set, then the handling
of a GV reference is modified.  If a GV was examined and its CV slot was
found to be empty, then the C<gv> op has the C<OPpEARLY_CV> flag set.
If the op is not optimised away, and the CV slot is later populated with
a subroutine having a prototype, that flag eventually triggers the warning
"called too early to check prototype".

If C<flags> has the bit C<RV2CVOPCV_RETURN_NAME_GV> set, then instead
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
	compcv = CvOUTSIDE(compcv);
	name = PadlistNAMESARRAY(CvPADLIST(compcv))
		[off = PARENT_PAD_INDEX(name)];
    }
    assert(!PadnameIsOUR(name));
    if (!PadnameIsSTATE(name) && PadnamePROTOCV(name)) {
	return PadnamePROTOCV(name);
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
    if (flags & ~RV2CVOPCV_FLAG_MASK)
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
	    if (!isGV(gv)) {
		if (SvROK(gv) && SvTYPE(SvRV(gv)) == SVt_PVCV) {
		    cv = MUTABLE_CV(SvRV(gv));
		    gv = NULL;
		    break;
		}
		if (flags & RV2CVOPCV_RETURN_STUB)
		    return (CV *)gv;
		else return NULL;
	    }
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
	} NOT_REACHED; /* NOTREACHED */
    }
    if (SvTYPE((SV*)cv) != SVt_PVCV)
	return NULL;
    if (flags & (RV2CVOPCV_RETURN_NAME_GV|RV2CVOPCV_MAYBE_NAME_GV)) {
	if ((!CvANON(cv) || !gv) && !CvLEXICAL(cv)
	 && ((flags & RV2CVOPCV_RETURN_NAME_GV) || !CvNAMED(cv)))
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
    if (!OpHAS_SIBLING(aop))
	aop = cUNOPx(aop)->op_first;
    for (aop = OpSIBLING(aop); OpHAS_SIBLING(aop); aop = OpSIBLING(aop)) {
        /* skip the extra attributes->import() call implicitly added in
         * something like foo(my $x : bar)
         */
        if (   aop->op_type == OP_ENTERSUB
            && (aop->op_flags & OPf_WANT) == OPf_WANT_VOID
        )
            continue;
        list(aop);
        op_lvalue(aop, OP_ENTERSUB);
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

C<protosv> supplies the subroutine prototype to be applied to the call.
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
by the name defined by the C<namegv> parameter.

=cut
*/

OP *
Perl_ck_entersub_args_proto(pTHX_ OP *entersubop, GV *namegv, SV *protosv)
{
    STRLEN proto_len;
    const char *proto, *proto_end;
    OP *aop, *prev, *cvop, *parent;
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
    parent = entersubop;
    aop = cUNOPx(entersubop)->op_first;
    if (!OpHAS_SIBLING(aop)) {
        parent = aop;
	aop = cUNOPx(aop)->op_first;
    }
    prev = aop;
    aop = OpSIBLING(aop);
    for (cvop = aop; OpHAS_SIBLING(cvop); cvop = OpSIBLING(cvop)) ;
    while (aop != cvop) {
	OP* o3 = aop;

	if (proto >= proto_end)
	{
	    SV * const namesv = cv_name((CV *)namegv, NULL, 0);
	    yyerror_pv(Perl_form(aTHX_ "Too many arguments for %"SVf,
					SVfARG(namesv)), SvUTF8(namesv));
	    return entersubop;
	}

	switch (*proto) {
	    case ';':
		optional = 1;
		proto++;
		continue;
	    case '_':
		/* _ must be at the end */
		if (proto[1] && !strchr(";@%", proto[1]))
		    goto oops;
                /* FALLTHROUGH */
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
		if (    o3->op_type != OP_UNDEF
                    && (o3->op_type != OP_SREFGEN
                        || (  cUNOPx(cUNOPx(o3)->op_first)->op_first->op_type
                                != OP_ANONCODE
                            && cUNOPx(cUNOPx(o3)->op_first)->op_first->op_type
                                != OP_RV2CV)))
		    bad_type_gv(arg, namegv, o3,
			    arg == 1 ? "block or sub {}" : "sub {}");
		break;
	    case '*':
		/* '*' allows any scalar type, including bareword */
		proto++;
		arg++;
		if (o3->op_type == OP_RV2GV)
		    goto wrapref;	/* autoconvert GLOB -> GLOBref */
		else if (o3->op_type == OP_CONST)
		    o3->op_private &= ~OPpCONST_STRICT;
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
			    bad_type_gv(arg, namegv, o3,
				    Perl_form(aTHX_ "one of %.*s",(int)(end - p), p));
			} else
			    goto oops;
			break;
		    case '*':
			if (o3->op_type == OP_RV2GV)
			    goto wrapref;
			if (!contextclass)
			    bad_type_gv(arg, namegv, o3, "symbol");
			break;
		    case '&':
			if (o3->op_type == OP_ENTERSUB
			 && !(o3->op_flags & OPf_STACKED))
			    goto wrapref;
			if (!contextclass)
			    bad_type_gv(arg, namegv, o3, "subroutine");
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
			    bad_type_gv(arg, namegv, o3, "scalar");
			}
			break;
		    case '@':
			if (o3->op_type == OP_RV2AV ||
				o3->op_type == OP_PADAV)
			{
			    o3->op_flags &=~ OPf_PARENS;
			    goto wrapref;
			}
			if (!contextclass)
			    bad_type_gv(arg, namegv, o3, "array");
			break;
		    case '%':
			if (o3->op_type == OP_RV2HV ||
				o3->op_type == OP_PADHV)
			{
			    o3->op_flags &=~ OPf_PARENS;
			    goto wrapref;
			}
			if (!contextclass)
			    bad_type_gv(arg, namegv, o3, "hash");
			break;
		    wrapref:
                            aop = S_op_sibling_newUNOP(aTHX_ parent, prev,
                                                OP_REFGEN, 0);
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
		Perl_croak(aTHX_ "Malformed prototype for %"SVf": %"SVf,
				  SVfARG(cv_name((CV *)namegv, NULL, 0)),
				  SVfARG(protosv));
            }
	}

	op_lvalue(aop, OP_ENTERSUB);
	prev = aop;
	aop = OpSIBLING(aop);
    }
    if (aop == cvop && *proto == '_') {
	/* generate an access to $_ */
        op_sibling_splice(parent, prev, 0, newDEFSVOP());
    }
    if (!optional && proto_end > proto &&
	(*proto != '@' && *proto != '%' && *proto != ';' && *proto != '_'))
    {
	SV * const namesv = cv_name((CV *)namegv, NULL, 0);
	yyerror_pv(Perl_form(aTHX_ "Not enough arguments for %"SVf,
				    SVfARG(namesv)), SvUTF8(namesv));
    }
    return entersubop;
}

/*
=for apidoc Am|OP *|ck_entersub_args_proto_or_list|OP *entersubop|GV *namegv|SV *protosv

Performs the fixup of the arguments part of an C<entersub> op tree either
based on a subroutine prototype or using default list-context processing.
This is the standard treatment used on a subroutine call, not marked
with C<&>, where the callee can be identified at compile time.

C<protosv> supplies the subroutine prototype to be applied to the call,
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
by the name defined by the C<namegv> parameter.

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
	if (!OpHAS_SIBLING(aop))
	    aop = cUNOPx(aop)->op_first;
	aop = OpSIBLING(aop);
	for (cvop = aop; OpSIBLING(cvop); cvop = OpSIBLING(cvop)) ;
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
	NOT_REACHED; /* NOTREACHED */
    }
    else {
	OP *prev, *cvop, *first, *parent;
	U32 flags = 0;

        parent = entersubop;
        if (!OpHAS_SIBLING(aop)) {
            parent = aop;
	    aop = cUNOPx(aop)->op_first;
        }
	
	first = prev = aop;
	aop = OpSIBLING(aop);
        /* find last sibling */
	for (cvop = aop;
	     OpHAS_SIBLING(cvop);
	     prev = cvop, cvop = OpSIBLING(cvop))
	    ;
        if (!(cvop->op_private & OPpENTERSUB_NOPAREN)
            /* Usually, OPf_SPECIAL on an op with no args means that it had
             * parens, but these have their own meaning for that flag: */
            && opnum != OP_VALUES && opnum != OP_KEYS && opnum != OP_EACH
            && opnum != OP_DELETE && opnum != OP_EXISTS)
                flags |= OPf_SPECIAL;
        /* excise cvop from end of sibling chain */
        op_sibling_splice(parent, prev, 1, NULL);
	op_free(cvop);
	if (aop == cvop) aop = NULL;

        /* detach remaining siblings from the first sibling, then
         * dispose of original optree */

        if (aop)
            op_sibling_splice(parent, first, -1, NULL);
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
		    (void)too_many_arguments_pv(aop, GvNAME(namegv), 0);
		op_free(aop);
	    }
	    return opnum == OP_RUNCV
		? newPVOP(OP_RUNCV,0,NULL)
		: newOP(opnum,0);
	default:
	    return op_convert_list(opnum,0,aop);
	}
    }
    NOT_REACHED; /* NOTREACHED */
    return entersubop;
}

/*
=for apidoc Am|void|cv_get_call_checker|CV *cv|Perl_call_checker *ckfun_p|SV **ckobj_p

Retrieves the function that will be used to fix up a call to C<cv>.
Specifically, the function is applied to an C<entersub> op tree for a
subroutine call, not marked with C<&>, where the callee can be identified
at compile time as C<cv>.

The C-level function pointer is returned in C<*ckfun_p>, and an SV
argument for it is returned in C<*ckobj_p>.  The function is intended
to be called in this manner:

 entersubop = (*ckfun_p)(aTHX_ entersubop, namegv, (*ckobj_p));

In this call, C<entersubop> is a pointer to the C<entersub> op,
which may be replaced by the check function, and C<namegv> is a GV
supplying the name that should be used by the check function to refer
to the callee of the C<entersub> op if it needs to emit any diagnostics.
It is permitted to apply the check function in non-standard situations,
such as to a call to a different subroutine or to a method call.

By default, the function is
L<Perl_ck_entersub_args_proto_or_list|/ck_entersub_args_proto_or_list>,
and the SV parameter is C<cv> itself.  This implements standard
prototype processing.  It can be changed, for a particular subroutine,
by L</cv_set_call_checker>.

=cut
*/

static void
S_cv_get_call_checker(CV *cv, Perl_call_checker *ckfun_p, SV **ckobj_p,
		      U8 *flagsp)
{
    MAGIC *callmg;
    callmg = SvMAGICAL((SV*)cv) ? mg_find((SV*)cv, PERL_MAGIC_checkcall) : NULL;
    if (callmg) {
	*ckfun_p = DPTR2FPTR(Perl_call_checker, callmg->mg_ptr);
	*ckobj_p = callmg->mg_obj;
	if (flagsp) *flagsp = callmg->mg_flags;
    } else {
	*ckfun_p = Perl_ck_entersub_args_proto_or_list;
	*ckobj_p = (SV*)cv;
	if (flagsp) *flagsp = 0;
    }
}

void
Perl_cv_get_call_checker(pTHX_ CV *cv, Perl_call_checker *ckfun_p, SV **ckobj_p)
{
    PERL_ARGS_ASSERT_CV_GET_CALL_CHECKER;
    PERL_UNUSED_CONTEXT;
    S_cv_get_call_checker(cv, ckfun_p, ckobj_p, NULL);
}

/*
=for apidoc Am|void|cv_set_call_checker_flags|CV *cv|Perl_call_checker ckfun|SV *ckobj|U32 flags

Sets the function that will be used to fix up a call to C<cv>.
Specifically, the function is applied to an C<entersub> op tree for a
subroutine call, not marked with C<&>, where the callee can be identified
at compile time as C<cv>.

The C-level function pointer is supplied in C<ckfun>, and an SV argument
for it is supplied in C<ckobj>.  The function should be defined like this:

    STATIC OP * ckfun(pTHX_ OP *op, GV *namegv, SV *ckobj)

It is intended to be called in this manner:

    entersubop = ckfun(aTHX_ entersubop, namegv, ckobj);

In this call, C<entersubop> is a pointer to the C<entersub> op,
which may be replaced by the check function, and C<namegv> supplies
the name that should be used by the check function to refer
to the callee of the C<entersub> op if it needs to emit any diagnostics.
It is permitted to apply the check function in non-standard situations,
such as to a call to a different subroutine or to a method call.

C<namegv> may not actually be a GV.  For efficiency, perl may pass a
CV or other SV instead.  Whatever is passed can be used as the first
argument to L</cv_name>.  You can force perl to pass a GV by including
C<CALL_CHECKER_REQUIRE_GV> in the C<flags>.

The current setting for a particular CV can be retrieved by
L</cv_get_call_checker>.

=for apidoc Am|void|cv_set_call_checker|CV *cv|Perl_call_checker ckfun|SV *ckobj

The original form of L</cv_set_call_checker_flags>, which passes it the
C<CALL_CHECKER_REQUIRE_GV> flag for backward-compatibility.

=cut
*/

void
Perl_cv_set_call_checker(pTHX_ CV *cv, Perl_call_checker ckfun, SV *ckobj)
{
    PERL_ARGS_ASSERT_CV_SET_CALL_CHECKER;
    cv_set_call_checker_flags(cv, ckfun, ckobj, CALL_CHECKER_REQUIRE_GV);
}

void
Perl_cv_set_call_checker_flags(pTHX_ CV *cv, Perl_call_checker ckfun,
				     SV *ckobj, U32 flags)
{
    PERL_ARGS_ASSERT_CV_SET_CALL_CHECKER_FLAGS;
    if (ckfun == Perl_ck_entersub_args_proto_or_list && ckobj == (SV*)cv) {
	if (SvMAGICAL((SV*)cv))
	    mg_free_type((SV*)cv, PERL_MAGIC_checkcall);
    } else {
	MAGIC *callmg;
	sv_magic((SV*)cv, &PL_sv_undef, PERL_MAGIC_checkcall, NULL, 0);
	callmg = mg_find((SV*)cv, PERL_MAGIC_checkcall);
	assert(callmg);
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
	callmg->mg_flags = (callmg->mg_flags &~ MGf_REQUIRE_GV)
			 | (U8)(flags & MGf_REQUIRE_GV) | MGf_COPY;
    }
}

static void
S_entersub_alloc_targ(pTHX_ OP * const o)
{
    o->op_targ = pad_alloc(OP_ENTERSUB, SVs_PADTMP);
    o->op_private |= OPpENTERSUB_HASTARG;
}

OP *
Perl_ck_subr(pTHX_ OP *o)
{
    OP *aop, *cvop;
    CV *cv;
    GV *namegv;
    SV **const_class = NULL;

    PERL_ARGS_ASSERT_CK_SUBR;

    aop = cUNOPx(o)->op_first;
    if (!OpHAS_SIBLING(aop))
	aop = cUNOPx(aop)->op_first;
    aop = OpSIBLING(aop);
    for (cvop = aop; OpHAS_SIBLING(cvop); cvop = OpSIBLING(cvop)) ;
    cv = rv2cv_op_cv(cvop, RV2CVOPCV_MARK_EARLY);
    namegv = cv ? (GV*)rv2cv_op_cv(cvop, RV2CVOPCV_MAYBE_NAME_GV) : NULL;

    o->op_private &= ~1;
    o->op_private |= (PL_hints & HINT_STRICT_REFS);
    if (PERLDB_SUB && PL_curstash != PL_debstash)
	o->op_private |= OPpENTERSUB_DB;
    switch (cvop->op_type) {
	case OP_RV2CV:
	    o->op_private |= (cvop->op_private & OPpENTERSUB_AMPER);
	    op_null(cvop);
	    break;
	case OP_METHOD:
	case OP_METHOD_NAMED:
	case OP_METHOD_SUPER:
	case OP_METHOD_REDIR:
	case OP_METHOD_REDIR_SUPER:
	    if (aop->op_type == OP_CONST) {
		aop->op_private &= ~OPpCONST_STRICT;
		const_class = &cSVOPx(aop)->op_sv;
	    }
	    else if (aop->op_type == OP_LIST) {
		OP * const sib = OpSIBLING(((UNOP*)aop)->op_first);
		if (sib && sib->op_type == OP_CONST) {
		    sib->op_private &= ~OPpCONST_STRICT;
		    const_class = &cSVOPx(sib)->op_sv;
		}
	    }
	    /* make class name a shared cow string to speedup method calls */
	    /* constant string might be replaced with object, f.e. bigint */
	    if (const_class && SvPOK(*const_class)) {
		STRLEN len;
		const char* str = SvPV(*const_class, len);
		if (len) {
		    SV* const shared = newSVpvn_share(
			str, SvUTF8(*const_class)
                                    ? -(SSize_t)len : (SSize_t)len,
                        0
		    );
                    if (SvREADONLY(*const_class))
                        SvREADONLY_on(shared);
		    SvREFCNT_dec(*const_class);
		    *const_class = shared;
		}
	    }
	    break;
    }

    if (!cv) {
	S_entersub_alloc_targ(aTHX_ o);
	return ck_entersub_args_list(o);
    } else {
	Perl_call_checker ckfun;
	SV *ckobj;
	U8 flags;
	S_cv_get_call_checker(cv, &ckfun, &ckobj, &flags);
	if (CvISXSUB(cv) || !CvROOT(cv))
	    S_entersub_alloc_targ(aTHX_ o);
	if (!namegv) {
	    /* The original call checker API guarantees that a GV will be
	       be provided with the right name.  So, if the old API was
	       used (or the REQUIRE_GV flag was passed), we have to reify
	       the CV’s GV, unless this is an anonymous sub.  This is not
	       ideal for lexical subs, as its stringification will include
	       the package.  But it is the best we can do.  */
	    if (flags & MGf_REQUIRE_GV) {
		if (!CvANON(cv) && (!CvNAMED(cv) || CvNAME_HEK(cv)))
		    namegv = CvGV(cv);
	    }
	    else namegv = MUTABLE_GV(cv);
	    /* After a syntax error in a lexical sub, the cv that
	       rv2cv_op_cv returns may be a nameless stub. */
	    if (!namegv) return ck_entersub_args_list(o);

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
#ifdef PERL_COPY_ON_WRITE
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
	    kid = (SVOP*)OpSIBLING(kid);
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
	    kid = OpSIBLING(kid);
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
     if (kid->op_type == OP_NULL && OpHAS_SIBLING(kid)) kid = OpSIBLING(kid);
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

    PERL_ARGS_ASSERT_CK_EACH;

    if (kid) {
	switch (kid->op_type) {
	    case OP_PADHV:
	    case OP_RV2HV:
		break;
	    case OP_PADAV:
	    case OP_RV2AV:
                OpTYPE_set(o, orig_type == OP_EACH ? OP_AEACH
                            : orig_type == OP_KEYS ? OP_AKEYS
                            :                        OP_AVALUES);
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
                Perl_croak_nocontext(
                    "Experimental %s on scalar is now forbidden",
                    PL_op_desc[orig_type]);
                break;
	}
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
                    SVfARG(name), hash ? "keys " : "", SVfARG(name)
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



/* 
   ---------------------------------------------------------
 
   Common vars in list assignment

   There now follows some enums and static functions for detecting
   common variables in list assignments. Here is a little essay I wrote
   for myself when trying to get my head around this. DAPM.

   ----

   First some random observations:
   
   * If a lexical var is an alias of something else, e.g.
       for my $x ($lex, $pkg, $a[0]) {...}
     then the act of aliasing will increase the reference count of the SV
   
   * If a package var is an alias of something else, it may still have a
     reference count of 1, depending on how the alias was created, e.g.
     in *a = *b, $a may have a refcount of 1 since the GP is shared
     with a single GvSV pointer to the SV. So If it's an alias of another
     package var, then RC may be 1; if it's an alias of another scalar, e.g.
     a lexical var or an array element, then it will have RC > 1.
   
   * There are many ways to create a package alias; ultimately, XS code
     may quite legally do GvSV(gv) = SvREFCNT_inc(sv) for example, so
     run-time tracing mechanisms are unlikely to be able to catch all cases.
   
   * When the LHS is all my declarations, the same vars can't appear directly
     on the RHS, but they can indirectly via closures, aliasing and lvalue
     subs. But those techniques all involve an increase in the lexical
     scalar's ref count.
   
   * When the LHS is all lexical vars (but not necessarily my declarations),
     it is possible for the same lexicals to appear directly on the RHS, and
     without an increased ref count, since the stack isn't refcounted.
     This case can be detected at compile time by scanning for common lex
     vars with PL_generation.
   
   * lvalue subs defeat common var detection, but they do at least
     return vars with a temporary ref count increment. Also, you can't
     tell at compile time whether a sub call is lvalue.
   
    
   So...
         
   A: There are a few circumstances where there definitely can't be any
     commonality:
   
       LHS empty:  () = (...);
       RHS empty:  (....) = ();
       RHS contains only constants or other 'can't possibly be shared'
           elements (e.g. ops that return PADTMPs):  (...) = (1,2, length)
           i.e. they only contain ops not marked as dangerous, whose children
           are also not dangerous;
       LHS ditto;
       LHS contains a single scalar element: e.g. ($x) = (....); because
           after $x has been modified, it won't be used again on the RHS;
       RHS contains a single element with no aggregate on LHS: e.g.
           ($a,$b,$c)  = ($x); again, once $a has been modified, its value
           won't be used again.
   
   B: If LHS are all 'my' lexical var declarations (or safe ops, which
     we can ignore):
   
       my ($a, $b, @c) = ...;
   
       Due to closure and goto tricks, these vars may already have content.
       For the same reason, an element on the RHS may be a lexical or package
       alias of one of the vars on the left, or share common elements, for
       example:
   
           my ($x,$y) = f(); # $x and $y on both sides
           sub f : lvalue { ($x,$y) = (1,2); $y, $x }
   
       and
   
           my $ra = f();
           my @a = @$ra;  # elements of @a on both sides
           sub f { @a = 1..4; \@a }
   
   
       First, just consider scalar vars on LHS:
   
           RHS is safe only if (A), or in addition,
               * contains only lexical *scalar* vars, where neither side's
                 lexicals have been flagged as aliases 
   
           If RHS is not safe, then it's always legal to check LHS vars for
           RC==1, since the only RHS aliases will always be associated
           with an RC bump.
   
           Note that in particular, RHS is not safe if:
   
               * it contains package scalar vars; e.g.:
   
                   f();
                   my ($x, $y) = (2, $x_alias);
                   sub f { $x = 1; *x_alias = \$x; }
   
               * It contains other general elements, such as flattened or
               * spliced or single array or hash elements, e.g.
   
                   f();
                   my ($x,$y) = @a; # or $a[0] or @a{@b} etc 
   
                   sub f {
                       ($x, $y) = (1,2);
                       use feature 'refaliasing';
                       \($a[0], $a[1]) = \($y,$x);
                   }
   
                 It doesn't matter if the array/hash is lexical or package.
   
               * it contains a function call that happens to be an lvalue
                 sub which returns one or more of the above, e.g.
   
                   f();
                   my ($x,$y) = f();
   
                   sub f : lvalue {
                       ($x, $y) = (1,2);
                       *x1 = \$x;
                       $y, $x1;
                   }
   
                   (so a sub call on the RHS should be treated the same
                   as having a package var on the RHS).
   
               * any other "dangerous" thing, such an op or built-in that
                 returns one of the above, e.g. pp_preinc
   
   
           If RHS is not safe, what we can do however is at compile time flag
           that the LHS are all my declarations, and at run time check whether
           all the LHS have RC == 1, and if so skip the full scan.
   
       Now consider array and hash vars on LHS: e.g. my (...,@a) = ...;
   
           Here the issue is whether there can be elements of @a on the RHS
           which will get prematurely freed when @a is cleared prior to
           assignment. This is only a problem if the aliasing mechanism
           is one which doesn't increase the refcount - only if RC == 1
           will the RHS element be prematurely freed.
   
           Because the array/hash is being INTROed, it or its elements
           can't directly appear on the RHS:
   
               my (@a) = ($a[0], @a, etc) # NOT POSSIBLE
   
           but can indirectly, e.g.:
   
               my $r = f();
               my (@a) = @$r;
               sub f { @a = 1..3; \@a }
   
           So if the RHS isn't safe as defined by (A), we must always
           mortalise and bump the ref count of any remaining RHS elements
           when assigning to a non-empty LHS aggregate.
   
           Lexical scalars on the RHS aren't safe if they've been involved in
           aliasing, e.g.
   
               use feature 'refaliasing';
   
               f();
               \(my $lex) = \$pkg;
               my @a = ($lex,3); # equivalent to ($a[0],3)
   
               sub f {
                   @a = (1,2);
                   \$pkg = \$a[0];
               }
   
           Similarly with lexical arrays and hashes on the RHS:
   
               f();
               my @b;
               my @a = (@b);
   
               sub f {
                   @a = (1,2);
                   \$b[0] = \$a[1];
                   \$b[1] = \$a[0];
               }
   
   
   
   C: As (B), but in addition the LHS may contain non-intro lexicals, e.g.
       my $a; ($a, my $b) = (....);
   
       The difference between (B) and (C) is that it is now physically
       possible for the LHS vars to appear on the RHS too, where they
       are not reference counted; but in this case, the compile-time
       PL_generation sweep will detect such common vars.
   
       So the rules for (C) differ from (B) in that if common vars are
       detected, the runtime "test RC==1" optimisation can no longer be used,
       and a full mark and sweep is required
   
   D: As (C), but in addition the LHS may contain package vars.
   
       Since package vars can be aliased without a corresponding refcount
       increase, all bets are off. It's only safe if (A). E.g.
   
           my ($x, $y) = (1,2);
   
           for $x_alias ($x) {
               ($x_alias, $y) = (3, $x); # whoops
           }
   
       Ditto for LHS aggregate package vars.
   
   E: Any other dangerous ops on LHS, e.g.
           (f(), $a[0], @$r) = (...);
   
       this is similar to (E) in that all bets are off. In addition, it's
       impossible to determine at compile time whether the LHS
       contains a scalar or an aggregate, e.g.
   
           sub f : lvalue { @a }
           (f()) = 1..3;

* ---------------------------------------------------------
*/


/* A set of bit flags returned by S_aassign_scan(). Each flag indicates
 * that at least one of the things flagged was seen.
 */

enum {
    AAS_MY_SCALAR       = 0x001, /* my $scalar */
    AAS_MY_AGG          = 0x002, /* aggregate: my @array or my %hash */
    AAS_LEX_SCALAR      = 0x004, /* $lexical */
    AAS_LEX_AGG         = 0x008, /* @lexical or %lexical aggregate */
    AAS_LEX_SCALAR_COMM = 0x010, /* $lexical seen on both sides */
    AAS_PKG_SCALAR      = 0x020, /* $scalar (where $scalar is pkg var) */
    AAS_PKG_AGG         = 0x040, /* package @array or %hash aggregate */
    AAS_DANGEROUS       = 0x080, /* an op (other than the above)
                                         that's flagged OA_DANGEROUS */
    AAS_SAFE_SCALAR     = 0x100, /* produces at least one scalar SV that's
                                        not in any of the categories above */
    AAS_DEFAV           = 0x200  /* contains just a single '@_' on RHS */
};



/* helper function for S_aassign_scan().
 * check a PAD-related op for commonality and/or set its generation number.
 * Returns a boolean indicating whether its shared */

static bool
S_aassign_padcheck(pTHX_ OP* o, bool rhs)
{
    if (PAD_COMPNAME_GEN(o->op_targ) == PERL_INT_MAX)
        /* lexical used in aliasing */
        return TRUE;

    if (rhs)
        return cBOOL(PAD_COMPNAME_GEN(o->op_targ) == (STRLEN)PL_generation);
    else
        PAD_COMPNAME_GEN_set(o->op_targ, PL_generation);

    return FALSE;
}


/*
  Helper function for OPpASSIGN_COMMON* detection in rpeep().
  It scans the left or right hand subtree of the aassign op, and returns a
  set of flags indicating what sorts of things it found there.
  'rhs' indicates whether we're scanning the LHS or RHS. If the former, we
  set PL_generation on lexical vars; if the latter, we see if
  PL_generation matches.
  'top' indicates whether we're recursing or at the top level.
  'scalars_p' is a pointer to a counter of the number of scalar SVs seen.
  This fn will increment it by the number seen. It's not intended to
  be an accurate count (especially as many ops can push a variable
  number of SVs onto the stack); rather it's used as to test whether there
  can be at most 1 SV pushed; so it's only meanings are "0, 1, many".
*/

static int
S_aassign_scan(pTHX_ OP* o, bool rhs, bool top, int *scalars_p)
{
    int flags = 0;
    bool kid_top = FALSE;

    /* first, look for a solitary @_ on the RHS */
    if (   rhs
        && top
        && (o->op_flags & OPf_KIDS)
        && OP_TYPE_IS_OR_WAS(o, OP_LIST)
    ) {
        OP *kid = cUNOPo->op_first;
        if (   (   kid->op_type == OP_PUSHMARK
                || kid->op_type == OP_PADRANGE) /* ex-pushmark */
            && ((kid = OpSIBLING(kid)))
            && !OpHAS_SIBLING(kid)
            && kid->op_type == OP_RV2AV
            && !(kid->op_flags & OPf_REF)
            && !(kid->op_private & (OPpLVAL_INTRO|OPpMAYBE_LVSUB))
            && ((kid->op_flags & OPf_WANT) == OPf_WANT_LIST)
            && ((kid = cUNOPx(kid)->op_first))
            && kid->op_type == OP_GV
            && cGVOPx_gv(kid) == PL_defgv
        )
            flags |= AAS_DEFAV;
    }

    switch (o->op_type) {
    case OP_GVSV:
        (*scalars_p)++;
        return AAS_PKG_SCALAR;

    case OP_PADAV:
    case OP_PADHV:
        (*scalars_p) += 2;
        if (top && (o->op_flags & OPf_REF))
            return (o->op_private & OPpLVAL_INTRO)
                ? AAS_MY_AGG : AAS_LEX_AGG;
        return AAS_DANGEROUS;

    case OP_PADSV:
        {
            int comm = S_aassign_padcheck(aTHX_ o, rhs)
                        ?  AAS_LEX_SCALAR_COMM : 0;
            (*scalars_p)++;
            return (o->op_private & OPpLVAL_INTRO)
                ? (AAS_MY_SCALAR|comm) : (AAS_LEX_SCALAR|comm);
        }

    case OP_RV2AV:
    case OP_RV2HV:
        (*scalars_p) += 2;
        if (cUNOPx(o)->op_first->op_type != OP_GV)
            return AAS_DANGEROUS; /* @{expr}, %{expr} */
        /* @pkg, %pkg */
        if (top && (o->op_flags & OPf_REF))
            return AAS_PKG_AGG;
        return AAS_DANGEROUS;

    case OP_RV2SV:
        (*scalars_p)++;
        if (cUNOPx(o)->op_first->op_type != OP_GV) {
            (*scalars_p) += 2;
            return AAS_DANGEROUS; /* ${expr} */
        }
        return AAS_PKG_SCALAR; /* $pkg */

    case OP_SPLIT:
        if (cLISTOPo->op_first->op_type == OP_PUSHRE) {
            /* "@foo = split... " optimises away the aassign and stores its
             * destination array in the OP_PUSHRE that precedes it.
             * A flattened array is always dangerous.
             */
            (*scalars_p) += 2;
            return AAS_DANGEROUS;
        }
        break;

    case OP_UNDEF:
        /* undef counts as a scalar on the RHS:
         *   (undef, $x) = ...;         # only 1 scalar on LHS: always safe
         *   ($x, $y)    = (undef, $x); # 2 scalars on RHS: unsafe
         */
        if (rhs)
            (*scalars_p)++;
        flags = AAS_SAFE_SCALAR;
        break;

    case OP_PUSHMARK:
    case OP_STUB:
        /* these are all no-ops; they don't push a potentially common SV
         * onto the stack, so they are neither AAS_DANGEROUS nor
         * AAS_SAFE_SCALAR */
        return 0;

    case OP_PADRANGE: /* Ignore padrange; checking its siblings is enough */
        break;

    case OP_NULL:
    case OP_LIST:
        /* these do nothing but may have children; but their children
         * should also be treated as top-level */
        kid_top = top;
        break;

    default:
        if (PL_opargs[o->op_type] & OA_DANGEROUS) {
            (*scalars_p) += 2;
            flags = AAS_DANGEROUS;
            break;
        }

        if (   (PL_opargs[o->op_type] & OA_TARGLEX)
            && (o->op_private & OPpTARGET_MY))
        {
            (*scalars_p)++;
            return S_aassign_padcheck(aTHX_ o, rhs)
                ? AAS_LEX_SCALAR_COMM : AAS_LEX_SCALAR;
        }

        /* if its an unrecognised, non-dangerous op, assume that it
         * it the cause of at least one safe scalar */
        (*scalars_p)++;
        flags = AAS_SAFE_SCALAR;
        break;
    }

    if (o->op_flags & OPf_KIDS) {
        OP *kid;
        for (kid = cUNOPo->op_first; kid; kid = OpSIBLING(kid))
            flags |= S_aassign_scan(aTHX_ kid, rhs, kid_top, scalars_p);
    }
    return flags;
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
    modop = OpSIBLING(modop_pushmark);

    if (modop->op_type != OP_SORT && modop->op_type != OP_REVERSE)
	return;

    /* no other operation except sort/reverse */
    if (OpHAS_SIBLING(modop))
	return;

    assert(cUNOPx(modop)->op_first->op_type == OP_PUSHMARK);
    if (!(oright = OpSIBLING(cUNOPx(modop)->op_first))) return;

    if (modop->op_flags & OPf_STACKED) {
	/* skip sort subroutine/block */
	assert(oright->op_type == OP_NULL);
	oright = OpSIBLING(oright);
    }

    assert(OpSIBLING(cUNOPo->op_first)->op_type == OP_NULL);
    oleft_pushmark = cUNOPx(OpSIBLING(cUNOPo->op_first))->op_first;
    assert(oleft_pushmark->op_type == OP_PUSHMARK);
    oleft = OpSIBLING(oleft_pushmark);

    /* Check the lhs is an array */
    if (!oleft ||
	(oleft->op_type != OP_RV2AV && oleft->op_type != OP_PADAV)
	|| OpHAS_SIBLING(oleft)
	|| (oleft->op_private & OPpLVAL_INTRO)
    )
	return;

    /* Only one thing on the rhs */
    if (OpHAS_SIBLING(oright))
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



/* S_maybe_multideref(): given an op_next chain of ops beginning at 'start'
 * that potentially represent a series of one or more aggregate derefs
 * (such as $a->[1]{$key}), examine the chain, and if appropriate, convert
 * the whole chain to a single OP_MULTIDEREF op (maybe with a few
 * additional ops left in too).
 *
 * The caller will have already verified that the first few ops in the
 * chain following 'start' indicate a multideref candidate, and will have
 * set 'orig_o' to the point further on in the chain where the first index
 * expression (if any) begins.  'orig_action' specifies what type of
 * beginning has already been determined by the ops between start..orig_o
 * (e.g.  $lex_ary[], $pkg_ary->{}, expr->[], etc).
 *
 * 'hints' contains any hints flags that need adding (currently just
 * OPpHINT_STRICT_REFS) as found in any rv2av/hv skipped by the caller.
 */

STATIC void
S_maybe_multideref(pTHX_ OP *start, OP *orig_o, UV orig_action, U8 hints)
{
    dVAR;
    int pass;
    UNOP_AUX_item *arg_buf = NULL;
    bool reset_start_targ  = FALSE; /* start->op_targ needs zeroing */
    int index_skip         = -1;    /* don't output index arg on this action */

    /* similar to regex compiling, do two passes; the first pass
     * determines whether the op chain is convertible and calculates the
     * buffer size; the second pass populates the buffer and makes any
     * changes necessary to ops (such as moving consts to the pad on
     * threaded builds).
     *
     * NB: for things like Coverity, note that both passes take the same
     * path through the logic tree (except for 'if (pass)' bits), since
     * both passes are following the same op_next chain; and in
     * particular, if it would return early on the second pass, it would
     * already have returned early on the first pass.
     */
    for (pass = 0; pass < 2; pass++) {
        OP *o                = orig_o;
        UV action            = orig_action;
        OP *first_elem_op    = NULL;  /* first seen aelem/helem */
        OP *top_op           = NULL;  /* highest [ah]elem/exists/del/rv2[ah]v */
        int action_count     = 0;     /* number of actions seen so far */
        int action_ix        = 0;     /* action_count % (actions per IV) */
        bool next_is_hash    = FALSE; /* is the next lookup to be a hash? */
        bool is_last         = FALSE; /* no more derefs to follow */
        bool maybe_aelemfast = FALSE; /* we can replace with aelemfast? */
        UNOP_AUX_item *arg     = arg_buf;
        UNOP_AUX_item *action_ptr = arg_buf;

        if (pass)
            action_ptr->uv = 0;
        arg++;

        switch (action) {
        case MDEREF_HV_gvsv_vivify_rv2hv_helem:
        case MDEREF_HV_gvhv_helem:
            next_is_hash = TRUE;
            /* FALLTHROUGH */
        case MDEREF_AV_gvsv_vivify_rv2av_aelem:
        case MDEREF_AV_gvav_aelem:
            if (pass) {
#ifdef USE_ITHREADS
                arg->pad_offset = cPADOPx(start)->op_padix;
                /* stop it being swiped when nulled */
                cPADOPx(start)->op_padix = 0;
#else
                arg->sv = cSVOPx(start)->op_sv;
                cSVOPx(start)->op_sv = NULL;
#endif
            }
            arg++;
            break;

        case MDEREF_HV_padhv_helem:
        case MDEREF_HV_padsv_vivify_rv2hv_helem:
            next_is_hash = TRUE;
            /* FALLTHROUGH */
        case MDEREF_AV_padav_aelem:
        case MDEREF_AV_padsv_vivify_rv2av_aelem:
            if (pass) {
                arg->pad_offset = start->op_targ;
                /* we skip setting op_targ = 0 for now, since the intact
                 * OP_PADXV is needed by S_check_hash_fields_and_hekify */
                reset_start_targ = TRUE;
            }
            arg++;
            break;

        case MDEREF_HV_pop_rv2hv_helem:
            next_is_hash = TRUE;
            /* FALLTHROUGH */
        case MDEREF_AV_pop_rv2av_aelem:
            break;

        default:
            NOT_REACHED; /* NOTREACHED */
            return;
        }

        while (!is_last) {
            /* look for another (rv2av/hv; get index;
             * aelem/helem/exists/delele) sequence */

            OP *kid;
            bool is_deref;
            bool ok;
            UV index_type = MDEREF_INDEX_none;

            if (action_count) {
                /* if this is not the first lookup, consume the rv2av/hv  */

                /* for N levels of aggregate lookup, we normally expect
                 * that the first N-1 [ah]elem ops will be flagged as
                 * /DEREF (so they autovivifiy if necessary), and the last
                 * lookup op not to be.
                 * For other things (like @{$h{k1}{k2}}) extra scope or
                 * leave ops can appear, so abandon the effort in that
                 * case */
                if (o->op_type != OP_RV2AV && o->op_type != OP_RV2HV)
                    return;

                /* rv2av or rv2hv sKR/1 */

                ASSUME(!(o->op_flags & ~(OPf_WANT|OPf_KIDS|OPf_PARENS
                                            |OPf_REF|OPf_MOD|OPf_SPECIAL)));
                if (o->op_flags != (OPf_WANT_SCALAR|OPf_KIDS|OPf_REF))
                    return;

                /* at this point, we wouldn't expect any of these
                 * possible private flags:
                 * OPpMAYBE_LVSUB, OPpOUR_INTRO, OPpLVAL_INTRO
                 * OPpTRUEBOOL, OPpMAYBE_TRUEBOOL (rv2hv only)
                 */
                ASSUME(!(o->op_private &
                    ~(OPpHINT_STRICT_REFS|OPpARG1_MASK|OPpSLICEWARNING)));

                hints = (o->op_private & OPpHINT_STRICT_REFS);

                /* make sure the type of the previous /DEREF matches the
                 * type of the next lookup */
                ASSUME(o->op_type == (next_is_hash ? OP_RV2HV : OP_RV2AV));
                top_op = o;

                action = next_is_hash
                            ? MDEREF_HV_vivify_rv2hv_helem
                            : MDEREF_AV_vivify_rv2av_aelem;
                o = o->op_next;
            }

            /* if this is the second pass, and we're at the depth where
             * previously we encountered a non-simple index expression,
             * stop processing the index at this point */
            if (action_count != index_skip) {

                /* look for one or more simple ops that return an array
                 * index or hash key */

                switch (o->op_type) {
                case OP_PADSV:
                    /* it may be a lexical var index */
                    ASSUME(!(o->op_flags & ~(OPf_WANT|OPf_PARENS
                                            |OPf_REF|OPf_MOD|OPf_SPECIAL)));
                    ASSUME(!(o->op_private &
                            ~(OPpPAD_STATE|OPpDEREF|OPpLVAL_INTRO)));

                    if (   OP_GIMME(o,0) == G_SCALAR
                        && !(o->op_flags & (OPf_REF|OPf_MOD))
                        && o->op_private == 0)
                    {
                        if (pass)
                            arg->pad_offset = o->op_targ;
                        arg++;
                        index_type = MDEREF_INDEX_padsv;
                        o = o->op_next;
                    }
                    break;

                case OP_CONST:
                    if (next_is_hash) {
                        /* it's a constant hash index */
                        if (!(SvFLAGS(cSVOPo_sv) & (SVf_IOK|SVf_NOK|SVf_POK)))
                            /* "use constant foo => FOO; $h{+foo}" for
                             * some weird FOO, can leave you with constants
                             * that aren't simple strings. It's not worth
                             * the extra hassle for those edge cases */
                            break;

                        if (pass) {
                            UNOP *rop = NULL;
                            OP * helem_op = o->op_next;

                            ASSUME(   helem_op->op_type == OP_HELEM
                                   || helem_op->op_type == OP_NULL);
                            if (helem_op->op_type == OP_HELEM) {
                                rop = (UNOP*)(((BINOP*)helem_op)->op_first);
                                if (   helem_op->op_private & OPpLVAL_INTRO
                                    || rop->op_type != OP_RV2HV
                                )
                                    rop = NULL;
                            }
                            S_check_hash_fields_and_hekify(aTHX_ rop, cSVOPo);

#ifdef USE_ITHREADS
                            /* Relocate sv to the pad for thread safety */
                            op_relocate_sv(&cSVOPo->op_sv, &o->op_targ);
                            arg->pad_offset = o->op_targ;
                            o->op_targ = 0;
#else
                            arg->sv = cSVOPx_sv(o);
#endif
                        }
                    }
                    else {
                        /* it's a constant array index */
                        IV iv;
                        SV *ix_sv = cSVOPo->op_sv;
                        if (!SvIOK(ix_sv))
                            break;
                        iv = SvIV(ix_sv);

                        if (   action_count == 0
                            && iv >= -128
                            && iv <= 127
                            && (   action == MDEREF_AV_padav_aelem
                                || action == MDEREF_AV_gvav_aelem)
                        )
                            maybe_aelemfast = TRUE;

                        if (pass) {
                            arg->iv = iv;
                            SvREFCNT_dec_NN(cSVOPo->op_sv);
                        }
                    }
                    if (pass)
                        /* we've taken ownership of the SV */
                        cSVOPo->op_sv = NULL;
                    arg++;
                    index_type = MDEREF_INDEX_const;
                    o = o->op_next;
                    break;

                case OP_GV:
                    /* it may be a package var index */

                    ASSUME(!(o->op_flags & ~(OPf_WANT|OPf_SPECIAL)));
                    ASSUME(!(o->op_private & ~(OPpEARLY_CV)));
                    if (  (o->op_flags &~ OPf_SPECIAL) != OPf_WANT_SCALAR
                        || o->op_private != 0
                    )
                        break;

                    kid = o->op_next;
                    if (kid->op_type != OP_RV2SV)
                        break;

                    ASSUME(!(kid->op_flags &
                            ~(OPf_WANT|OPf_KIDS|OPf_MOD|OPf_REF
                             |OPf_SPECIAL|OPf_PARENS)));
                    ASSUME(!(kid->op_private &
                                    ~(OPpARG1_MASK
                                     |OPpHINT_STRICT_REFS|OPpOUR_INTRO
                                     |OPpDEREF|OPpLVAL_INTRO)));
                    if(   (kid->op_flags &~ OPf_PARENS)
                            != (OPf_WANT_SCALAR|OPf_KIDS)
                       || (kid->op_private & ~(OPpARG1_MASK|HINT_STRICT_REFS))
                    )
                        break;

                    if (pass) {
#ifdef USE_ITHREADS
                        arg->pad_offset = cPADOPx(o)->op_padix;
                        /* stop it being swiped when nulled */
                        cPADOPx(o)->op_padix = 0;
#else
                        arg->sv = cSVOPx(o)->op_sv;
                        cSVOPo->op_sv = NULL;
#endif
                    }
                    arg++;
                    index_type = MDEREF_INDEX_gvsv;
                    o = kid->op_next;
                    break;

                } /* switch */
            } /* action_count != index_skip */

            action |= index_type;


            /* at this point we have either:
             *   * detected what looks like a simple index expression,
             *     and expect the next op to be an [ah]elem, or
             *     an nulled  [ah]elem followed by a delete or exists;
             *  * found a more complex expression, so something other
             *    than the above follows.
             */

            /* possibly an optimised away [ah]elem (where op_next is
             * exists or delete) */
            if (o->op_type == OP_NULL)
                o = o->op_next;

            /* at this point we're looking for an OP_AELEM, OP_HELEM,
             * OP_EXISTS or OP_DELETE */

            /* if something like arybase (a.k.a $[ ) is in scope,
             * abandon optimisation attempt */
            if (  (o->op_type == OP_AELEM || o->op_type == OP_HELEM)
               && PL_check[o->op_type] != Perl_ck_null)
                return;

            if (   o->op_type != OP_AELEM
                || (o->op_private &
		      (OPpLVAL_INTRO|OPpLVAL_DEFER|OPpDEREF|OPpMAYBE_LVSUB))
                )
                maybe_aelemfast = FALSE;

            /* look for aelem/helem/exists/delete. If it's not the last elem
             * lookup, it *must* have OPpDEREF_AV/HV, but not many other
             * flags; if it's the last, then it mustn't have
             * OPpDEREF_AV/HV, but may have lots of other flags, like
             * OPpLVAL_INTRO etc
             */

            if (   index_type == MDEREF_INDEX_none
                || (   o->op_type != OP_AELEM  && o->op_type != OP_HELEM
                    && o->op_type != OP_EXISTS && o->op_type != OP_DELETE)
            )
                ok = FALSE;
            else {
                /* we have aelem/helem/exists/delete with valid simple index */

                is_deref =    (o->op_type == OP_AELEM || o->op_type == OP_HELEM)
                           && (   (o->op_private & OPpDEREF) == OPpDEREF_AV
                               || (o->op_private & OPpDEREF) == OPpDEREF_HV);

                if (is_deref) {
                    ASSUME(!(o->op_flags &
                                 ~(OPf_WANT|OPf_KIDS|OPf_MOD|OPf_PARENS)));
                    ASSUME(!(o->op_private & ~(OPpARG2_MASK|OPpDEREF)));

                    ok =    (o->op_flags &~ OPf_PARENS)
                               == (OPf_WANT_SCALAR|OPf_KIDS|OPf_MOD)
                         && !(o->op_private & ~(OPpDEREF|OPpARG2_MASK));
                }
                else if (o->op_type == OP_EXISTS) {
                    ASSUME(!(o->op_flags & ~(OPf_WANT|OPf_KIDS|OPf_PARENS
                                |OPf_REF|OPf_MOD|OPf_SPECIAL)));
                    ASSUME(!(o->op_private & ~(OPpARG1_MASK|OPpEXISTS_SUB)));
                    ok =  !(o->op_private & ~OPpARG1_MASK);
                }
                else if (o->op_type == OP_DELETE) {
                    ASSUME(!(o->op_flags & ~(OPf_WANT|OPf_KIDS|OPf_PARENS
                                |OPf_REF|OPf_MOD|OPf_SPECIAL)));
                    ASSUME(!(o->op_private &
                                    ~(OPpARG1_MASK|OPpSLICE|OPpLVAL_INTRO)));
                    /* don't handle slices or 'local delete'; the latter
                     * is fairly rare, and has a complex runtime */
                    ok =  !(o->op_private & ~OPpARG1_MASK);
                    if (OP_TYPE_IS_OR_WAS(cUNOPo->op_first, OP_AELEM))
                        /* skip handling run-tome error */
                        ok = (ok && cBOOL(o->op_flags & OPf_SPECIAL));
                }
                else {
                    ASSUME(o->op_type == OP_AELEM || o->op_type == OP_HELEM);
                    ASSUME(!(o->op_flags & ~(OPf_WANT|OPf_KIDS|OPf_MOD
                                            |OPf_PARENS|OPf_REF|OPf_SPECIAL)));
                    ASSUME(!(o->op_private & ~(OPpARG2_MASK|OPpMAYBE_LVSUB
                                    |OPpLVAL_DEFER|OPpDEREF|OPpLVAL_INTRO)));
                    ok = (o->op_private & OPpDEREF) != OPpDEREF_SV;
                }
            }

            if (ok) {
                if (!first_elem_op)
                    first_elem_op = o;
                top_op = o;
                if (is_deref) {
                    next_is_hash = cBOOL((o->op_private & OPpDEREF) == OPpDEREF_HV);
                    o = o->op_next;
                }
                else {
                    is_last = TRUE;
                    action |= MDEREF_FLAG_last;
                }
            }
            else {
                /* at this point we have something that started
                 * promisingly enough (with rv2av or whatever), but failed
                 * to find a simple index followed by an
                 * aelem/helem/exists/delete. If this is the first action,
                 * give up; but if we've already seen at least one
                 * aelem/helem, then keep them and add a new action with
                 * MDEREF_INDEX_none, which causes it to do the vivify
                 * from the end of the previous lookup, and do the deref,
                 * but stop at that point. So $a[0][expr] will do one
                 * av_fetch, vivify and deref, then continue executing at
                 * expr */
                if (!action_count)
                    return;
                is_last = TRUE;
                index_skip = action_count;
                action |= MDEREF_FLAG_last;
            }

            if (pass)
                action_ptr->uv |= (action << (action_ix * MDEREF_SHIFT));
            action_ix++;
            action_count++;
            /* if there's no space for the next action, create a new slot
             * for it *before* we start adding args for that action */
            if ((action_ix + 1) * MDEREF_SHIFT > UVSIZE*8) {
                action_ptr = arg;
                if (pass)
                    arg->uv = 0;
                arg++;
                action_ix = 0;
            }
        } /* while !is_last */

        /* success! */

        if (pass) {
            OP *mderef;
            OP *p, *q;

            mderef = newUNOP_AUX(OP_MULTIDEREF, 0, NULL, arg_buf);
            if (index_skip == -1) {
                mderef->op_flags = o->op_flags
                        & (OPf_WANT|OPf_MOD|(next_is_hash ? OPf_SPECIAL : 0));
                if (o->op_type == OP_EXISTS)
                    mderef->op_private = OPpMULTIDEREF_EXISTS;
                else if (o->op_type == OP_DELETE)
                    mderef->op_private = OPpMULTIDEREF_DELETE;
                else
                    mderef->op_private = o->op_private
                        & (OPpMAYBE_LVSUB|OPpLVAL_DEFER|OPpLVAL_INTRO);
            }
            /* accumulate strictness from every level (although I don't think
             * they can actually vary) */
            mderef->op_private |= hints;

            /* integrate the new multideref op into the optree and the
             * op_next chain.
             *
             * In general an op like aelem or helem has two child
             * sub-trees: the aggregate expression (a_expr) and the
             * index expression (i_expr):
             *
             *     aelem
             *       |
             *     a_expr - i_expr
             *
             * The a_expr returns an AV or HV, while the i-expr returns an
             * index. In general a multideref replaces most or all of a
             * multi-level tree, e.g.
             *
             *     exists
             *       |
             *     ex-aelem
             *       |
             *     rv2av  - i_expr1
             *       |
             *     helem
             *       |
             *     rv2hv  - i_expr2
             *       |
             *     aelem
             *       |
             *     a_expr - i_expr3
             *
             * With multideref, all the i_exprs will be simple vars or
             * constants, except that i_expr1 may be arbitrary in the case
             * of MDEREF_INDEX_none.
             *
             * The bottom-most a_expr will be either:
             *   1) a simple var (so padXv or gv+rv2Xv);
             *   2) a simple scalar var dereferenced (e.g. $r->[0]):
             *      so a simple var with an extra rv2Xv;
             *   3) or an arbitrary expression.
             *
             * 'start', the first op in the execution chain, will point to
             *   1),2): the padXv or gv op;
             *   3):    the rv2Xv which forms the last op in the a_expr
             *          execution chain, and the top-most op in the a_expr
             *          subtree.
             *
             * For all cases, the 'start' node is no longer required,
             * but we can't free it since one or more external nodes
             * may point to it. E.g. consider
             *     $h{foo} = $a ? $b : $c
             * Here, both the op_next and op_other branches of the
             * cond_expr point to the gv[*h] of the hash expression, so
             * we can't free the 'start' op.
             *
             * For expr->[...], we need to save the subtree containing the
             * expression; for the other cases, we just need to save the
             * start node.
             * So in all cases, we null the start op and keep it around by
             * making it the child of the multideref op; for the expr->
             * case, the expr will be a subtree of the start node.
             *
             * So in the simple 1,2 case the  optree above changes to
             *
             *     ex-exists
             *       |
             *     multideref
             *       |
             *     ex-gv (or ex-padxv)
             *
             *  with the op_next chain being
             *
             *  -> ex-gv -> multideref -> op-following-ex-exists ->
             *
             *  In the 3 case, we have
             *
             *     ex-exists
             *       |
             *     multideref
             *       |
             *     ex-rv2xv
             *       |
             *    rest-of-a_expr
             *      subtree
             *
             *  and
             *
             *  -> rest-of-a_expr subtree ->
             *    ex-rv2xv -> multideref -> op-following-ex-exists ->
             *
             *
             * Where the last i_expr is non-simple (i.e. MDEREF_INDEX_none,
             * e.g. $a[0]{foo}[$x+1], the next rv2xv is nulled and the
             * multideref attached as the child, e.g.
             *
             *     exists
             *       |
             *     ex-aelem
             *       |
             *     ex-rv2av  - i_expr1
             *       |
             *     multideref
             *       |
             *     ex-whatever
             *
             */

            /* if we free this op, don't free the pad entry */
            if (reset_start_targ)
                start->op_targ = 0;


            /* Cut the bit we need to save out of the tree and attach to
             * the multideref op, then free the rest of the tree */

            /* find parent of node to be detached (for use by splice) */
            p = first_elem_op;
            if (   orig_action == MDEREF_AV_pop_rv2av_aelem
                || orig_action == MDEREF_HV_pop_rv2hv_helem)
            {
                /* there is an arbitrary expression preceding us, e.g.
                 * expr->[..]? so we need to save the 'expr' subtree */
                if (p->op_type == OP_EXISTS || p->op_type == OP_DELETE)
                    p = cUNOPx(p)->op_first;
                ASSUME(   start->op_type == OP_RV2AV
                       || start->op_type == OP_RV2HV);
            }
            else {
                /* either a padXv or rv2Xv+gv, maybe with an ex-Xelem
                 * above for exists/delete. */
                while (   (p->op_flags & OPf_KIDS)
                       && cUNOPx(p)->op_first != start
                )
                    p = cUNOPx(p)->op_first;
            }
            ASSUME(cUNOPx(p)->op_first == start);

            /* detach from main tree, and re-attach under the multideref */
            op_sibling_splice(mderef, NULL, 0,
                    op_sibling_splice(p, NULL, 1, NULL));
            op_null(start);

            start->op_next = mderef;

            mderef->op_next = index_skip == -1 ? o->op_next : o;

            /* excise and free the original tree, and replace with
             * the multideref op */
            p = op_sibling_splice(top_op, NULL, -1, mderef);
            while (p) {
                q = OpSIBLING(p);
                op_free(p);
                p = q;
            }
            op_null(top_op);
        }
        else {
            Size_t size = arg - arg_buf;

            if (maybe_aelemfast && action_count == 1)
                return;

            arg_buf = (UNOP_AUX_item*)PerlMemShared_malloc(
                                sizeof(UNOP_AUX_item) * (size + 1));
            /* for dumping etc: store the length in a hidden first slot;
             * we set the op_aux pointer to the second slot */
            arg_buf->uv = size;
            arg_buf++;
        }
    } /* for (pass = ...) */
}



/* mechanism for deferring recursion in rpeep() */

#define MAX_DEFERRED 4

#define DEFER(o) \
  STMT_START { \
    if (defer_ix == (MAX_DEFERRED-1)) { \
        OP **defer = defer_queue[defer_base]; \
        CALL_RPEEP(*defer); \
        S_prune_chain_head(defer); \
	defer_base = (defer_base + 1) % MAX_DEFERRED; \
	defer_ix--; \
    } \
    defer_queue[(defer_base + ++defer_ix) % MAX_DEFERRED] = &(o); \
  } STMT_END

#define IS_AND_OP(o)   (o->op_type == OP_AND)
#define IS_OR_OP(o)    (o->op_type == OP_OR)


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
    OP *fop;
    OP *sop;

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
                S_prune_chain_head(defer);
            }
	    break;
	}

      redo:

        /* oldoldop -> oldop -> o should be a chain of 3 adjacent ops */
        assert(!oldoldop || oldoldop->op_next == oldop);
        assert(!oldop    || oldop->op_next    == o);

	/* By default, this op has now been optimised. A couple of cases below
	   clear this again.  */
	o->op_opt = 1;
	PL_op = o;

        /* look for a series of 1 or more aggregate derefs, e.g.
         *   $a[1]{foo}[$i]{$k}
         * and replace with a single OP_MULTIDEREF op.
         * Each index must be either a const, or a simple variable,
         *
         * First, look for likely combinations of starting ops,
         * corresponding to (global and lexical variants of)
         *     $a[...]   $h{...}
         *     $r->[...] $r->{...}
         *     (preceding expression)->[...]
         *     (preceding expression)->{...}
         * and if so, call maybe_multideref() to do a full inspection
         * of the op chain and if appropriate, replace with an
         * OP_MULTIDEREF
         */
        {
            UV action;
            OP *o2 = o;
            U8 hints = 0;

            switch (o2->op_type) {
            case OP_GV:
                /* $pkg[..]   :   gv[*pkg]
                 * $pkg->[...]:   gv[*pkg]; rv2sv sKM/DREFAV */

                /* Fail if there are new op flag combinations that we're
                 * not aware of, rather than:
                 *  * silently failing to optimise, or
                 *  * silently optimising the flag away.
                 * If this ASSUME starts failing, examine what new flag
                 * has been added to the op, and decide whether the
                 * optimisation should still occur with that flag, then
                 * update the code accordingly. This applies to all the
                 * other ASSUMEs in the block of code too.
                 */
                ASSUME(!(o2->op_flags &
                            ~(OPf_WANT|OPf_MOD|OPf_PARENS|OPf_SPECIAL)));
                ASSUME(!(o2->op_private & ~OPpEARLY_CV));

                o2 = o2->op_next;

                if (o2->op_type == OP_RV2AV) {
                    action = MDEREF_AV_gvav_aelem;
                    goto do_deref;
                }

                if (o2->op_type == OP_RV2HV) {
                    action = MDEREF_HV_gvhv_helem;
                    goto do_deref;
                }

                if (o2->op_type != OP_RV2SV)
                    break;

                /* at this point we've seen gv,rv2sv, so the only valid
                 * construct left is $pkg->[] or $pkg->{} */

                ASSUME(!(o2->op_flags & OPf_STACKED));
                if ((o2->op_flags & (OPf_WANT|OPf_REF|OPf_MOD|OPf_SPECIAL))
                            != (OPf_WANT_SCALAR|OPf_MOD))
                    break;

                ASSUME(!(o2->op_private & ~(OPpARG1_MASK|HINT_STRICT_REFS
                                    |OPpOUR_INTRO|OPpDEREF|OPpLVAL_INTRO)));
                if (o2->op_private & (OPpOUR_INTRO|OPpLVAL_INTRO))
                    break;
                if (   (o2->op_private & OPpDEREF) != OPpDEREF_AV
                    && (o2->op_private & OPpDEREF) != OPpDEREF_HV)
                    break;

                o2 = o2->op_next;
                if (o2->op_type == OP_RV2AV) {
                    action = MDEREF_AV_gvsv_vivify_rv2av_aelem;
                    goto do_deref;
                }
                if (o2->op_type == OP_RV2HV) {
                    action = MDEREF_HV_gvsv_vivify_rv2hv_helem;
                    goto do_deref;
                }
                break;

            case OP_PADSV:
                /* $lex->[...]: padsv[$lex] sM/DREFAV */

                ASSUME(!(o2->op_flags &
                    ~(OPf_WANT|OPf_PARENS|OPf_REF|OPf_MOD|OPf_SPECIAL)));
                if ((o2->op_flags &
                        (OPf_WANT|OPf_REF|OPf_MOD|OPf_SPECIAL))
                     != (OPf_WANT_SCALAR|OPf_MOD))
                    break;

                ASSUME(!(o2->op_private &
                                ~(OPpPAD_STATE|OPpDEREF|OPpLVAL_INTRO)));
                /* skip if state or intro, or not a deref */
                if (      o2->op_private != OPpDEREF_AV
                       && o2->op_private != OPpDEREF_HV)
                    break;

                o2 = o2->op_next;
                if (o2->op_type == OP_RV2AV) {
                    action = MDEREF_AV_padsv_vivify_rv2av_aelem;
                    goto do_deref;
                }
                if (o2->op_type == OP_RV2HV) {
                    action = MDEREF_HV_padsv_vivify_rv2hv_helem;
                    goto do_deref;
                }
                break;

            case OP_PADAV:
            case OP_PADHV:
                /*    $lex[..]:  padav[@lex:1,2] sR *
                 * or $lex{..}:  padhv[%lex:1,2] sR */
                ASSUME(!(o2->op_flags & ~(OPf_WANT|OPf_MOD|OPf_PARENS|
                                            OPf_REF|OPf_SPECIAL)));
                if ((o2->op_flags &
                        (OPf_WANT|OPf_REF|OPf_MOD|OPf_SPECIAL))
                     != (OPf_WANT_SCALAR|OPf_REF))
                    break;
                if (o2->op_flags != (OPf_WANT_SCALAR|OPf_REF))
                    break;
                /* OPf_PARENS isn't currently used in this case;
                 * if that changes, let us know! */
                ASSUME(!(o2->op_flags & OPf_PARENS));

                /* at this point, we wouldn't expect any of the remaining
                 * possible private flags:
                 * OPpPAD_STATE, OPpLVAL_INTRO, OPpTRUEBOOL,
                 * OPpMAYBE_TRUEBOOL, OPpMAYBE_LVSUB
                 *
                 * OPpSLICEWARNING shouldn't affect runtime
                 */
                ASSUME(!(o2->op_private & ~(OPpSLICEWARNING)));

                action = o2->op_type == OP_PADAV
                            ? MDEREF_AV_padav_aelem
                            : MDEREF_HV_padhv_helem;
                o2 = o2->op_next;
                S_maybe_multideref(aTHX_ o, o2, action, 0);
                break;


            case OP_RV2AV:
            case OP_RV2HV:
                action = o2->op_type == OP_RV2AV
                            ? MDEREF_AV_pop_rv2av_aelem
                            : MDEREF_HV_pop_rv2hv_helem;
                /* FALLTHROUGH */
            do_deref:
                /* (expr)->[...]:  rv2av sKR/1;
                 * (expr)->{...}:  rv2hv sKR/1; */

                ASSUME(o2->op_type == OP_RV2AV || o2->op_type == OP_RV2HV);

                ASSUME(!(o2->op_flags & ~(OPf_WANT|OPf_KIDS|OPf_PARENS
                                |OPf_REF|OPf_MOD|OPf_STACKED|OPf_SPECIAL)));
                if (o2->op_flags != (OPf_WANT_SCALAR|OPf_KIDS|OPf_REF))
                    break;

                /* at this point, we wouldn't expect any of these
                 * possible private flags:
                 * OPpMAYBE_LVSUB, OPpLVAL_INTRO
                 * OPpTRUEBOOL, OPpMAYBE_TRUEBOOL, (rv2hv only)
                 */
                ASSUME(!(o2->op_private &
                    ~(OPpHINT_STRICT_REFS|OPpARG1_MASK|OPpSLICEWARNING
                     |OPpOUR_INTRO)));
                hints |= (o2->op_private & OPpHINT_STRICT_REFS);

                o2 = o2->op_next;

                S_maybe_multideref(aTHX_ o, o2, action, hints);
                break;

            default:
                break;
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
		OP *sibling = OpSIBLING(o);
		if (   OP_TYPE_IS(next, OP_PUSHMARK)
		    && OP_TYPE_IS(sibling, OP_RETURN)
		    && OP_TYPE_IS(sibling->op_next, OP_LINESEQ)
		    && ( OP_TYPE_IS(sibling->op_next->op_next, OP_LEAVESUB)
		       ||OP_TYPE_IS(sibling->op_next->op_next,
				    OP_LEAVESUBLV))
		    && cUNOPx(sibling)->op_first == next
		    && OpHAS_SIBLING(next) && OpSIBLING(next)->op_next
		    && next->op_next
		) {
		    /* Look through the PUSHMARK's siblings for one that
		     * points to the RETURN */
		    OP *top = OpSIBLING(next);
		    while (top && top->op_next) {
			if (top->op_next == sibling) {
			    top->op_next = sibling->op_next;
			    o->op_next = next->op_next;
			    break;
			}
			top = OpSIBLING(top);
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
		OP *pad1, *ns2, *pad2, *ns3, *newop, *newpm;

		pad1 =    o->op_next;
		ns2  = pad1->op_next;
		pad2 =  ns2->op_next;
		ns3  = pad2->op_next;

                /* we assume here that the op_next chain is the same as
                 * the op_sibling chain */
                assert(OpSIBLING(o)    == pad1);
                assert(OpSIBLING(pad1) == ns2);
                assert(OpSIBLING(ns2)  == pad2);
                assert(OpSIBLING(pad2) == ns3);

                /* excise and delete ns2 */
                op_sibling_splice(NULL, pad1, 1, NULL);
                op_free(ns2);

                /* excise pad1 and pad2 */
                op_sibling_splice(NULL, o, 2, NULL);

                /* create new listop, with children consisting of:
                 * a new pushmark, pad1, pad2. */
		newop = newLISTOP(OP_LIST, 0, pad1, pad2);
		newop->op_flags |= OPf_PARENS;
		newop->op_flags = (newop->op_flags & ~OPf_WANT) | OPf_WANT_VOID;

                /* insert newop between o and ns3 */
                op_sibling_splice(NULL, o, 0, newop);

                /*fixup op_next chain */
                newpm = cUNOPx(newop)->op_first; /* pushmark */
		o    ->op_next = newpm;
		newpm->op_next = pad1;
		pad1 ->op_next = pad2;
		pad2 ->op_next = newop; /* listop */
		newop->op_next = ns3;

		/* Ensure pushmark has this flag if padops do */
		if (pad1->op_flags & OPf_MOD && pad2->op_flags & OPf_MOD) {
		    newpm->op_flags |= OPf_MOD;
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
		    op_null(o);
		    if (oldop)
			oldop->op_next = nextop;
                    o = nextop;
		    /* Skip (old)oldop assignment since the current oldop's
		       op_next already points to the next op.  */
		    goto redo;
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
	    /* FALLTHROUGH */
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

            /* Given
                 5 repeat/DOLIST
                 3   ex-list
                 1     pushmark
                 2     scalar or const
                 4   const[0]
               convert repeat into a stub with no kids.
             */
            if (o->op_next->op_type == OP_CONST
             || (  o->op_next->op_type == OP_PADSV
                && !(o->op_next->op_private & OPpLVAL_INTRO))
             || (  o->op_next->op_type == OP_GV
                && o->op_next->op_next->op_type == OP_RV2SV
                && !(o->op_next->op_next->op_private
                        & (OPpLVAL_INTRO|OPpOUR_INTRO))))
            {
                const OP *kid = o->op_next->op_next;
                if (o->op_next->op_type == OP_GV)
                   kid = kid->op_next;
                /* kid is now the ex-list.  */
                if (kid->op_type == OP_NULL
                 && (kid = kid->op_next)->op_type == OP_CONST
                    /* kid is now the repeat count.  */
                 && kid->op_next->op_type == OP_REPEAT
                 && kid->op_next->op_private & OPpREPEAT_DOLIST
                 && (kid->op_next->op_flags & OPf_WANT) == OPf_WANT_LIST
                 && SvIOK(kSVOP_sv) && SvIVX(kSVOP_sv) == 0)
                {
                    o = kid->op_next; /* repeat */
                    assert(oldop);
                    oldop->op_next = o;
                    op_free(cBINOPo->op_first);
                    op_free(cBINOPo->op_last );
                    o->op_flags &=~ OPf_KIDS;
                    /* stub is a baseop; repeat is a binop */
                    STATIC_ASSERT_STMT(sizeof(OP) <= sizeof(BINOP));
                    OpTYPE_set(o, OP_STUB);
                    o->op_private = 0;
                    break;
                }
            }

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
            bool gvoid = 0;     /* init only to stop compiler whining */
            bool defav = 0;  /* seen (...) = @_ */
            bool reuse = 0;  /* reuse an existing padrange op */

            /* look for a pushmark -> gv[_] -> rv2av */

            {
                OP *rv2av, *q;
                p = o->op_next;
                if (   p->op_type == OP_GV
                    && cGVOPx_gv(p) == PL_defgv
                    && (rv2av = p->op_next)
                    && rv2av->op_type == OP_RV2AV
                    && !(rv2av->op_flags & OPf_REF)
                    && !(rv2av->op_private & (OPpLVAL_INTRO|OPpMAYBE_LVSUB))
                    && ((rv2av->op_flags & OPf_WANT) == OPf_WANT_LIST)
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
                    gvoid = OP_GIMME(p,0) == G_VOID;
                }
                else {
                    if ((p->op_private & OPpLVAL_INTRO) != intro)
                        break;
                    /* Note that you'd normally  expect targs to be
                     * contiguous in my($a,$b,$c), but that's not the case
                     * when external modules start doing things, e.g.
                     * Function::Parameters */
                    if (p->op_targ != base + count)
                        break;
                    assert(p->op_targ == base + count);
                    /* Either all the padops or none of the padops should
                       be in void context.  Since we only do the optimisa-
                       tion for av/hv when the aggregate itself is pushed
                       on to the stack (one item), there is no need to dis-
                       tinguish list from scalar context.  */
                    if (gvoid != (OP_GIMME(p,0) == G_VOID))
                        break;
                }

                /* for AV, HV, only when we're not flattening */
                if (   p->op_type != OP_PADSV
                    && !gvoid
                    && !(p->op_flags & OPf_REF)
                )
                    break;

                if (count >= OPpPADRANGE_COUNTMASK)
                    break;

                /* there's a biggest base we can fit into a
                 * SAVEt_CLEARPADRANGE in pp_padrange.
                 * (The sizeof() stuff will be constant-folded, and is
                 * intended to avoid getting "comparison is always false"
                 * compiler warnings. See the comments above
                 * MEM_WRAP_CHECK for more explanation on why we do this
                 * in a weird way to avoid compiler warnings.)
                 */
                if (   intro
                    && (8*sizeof(base) >
                        8*sizeof(UV)-OPpPADRANGE_COUNTSHIFT-SAVE_TIGHT_SHIFT
                        ? base
                        : (UV_MAX >> (OPpPADRANGE_COUNTSHIFT+SAVE_TIGHT_SHIFT))
                        ) >
                        (UV_MAX >> (OPpPADRANGE_COUNTSHIFT+SAVE_TIGHT_SHIFT))
                )
                    break;

                /* Success! We've got another valid pad op to optimise away */
                count++;
                followop = p->op_next;
            }

            if (count < 1 || (count == 1 && !defav))
                break;

            /* pp_padrange in specifically compile-time void context
             * skips pushing a mark and lexicals; in all other contexts
             * (including unknown till runtime) it pushes a mark and the
             * lexicals. We must be very careful then, that the ops we
             * optimise away would have exactly the same effect as the
             * padrange.
             * In particular in void context, we can only optimise to
             * a padrange if we see the complete sequence
             *     pushmark, pad*v, ...., list
             * which has the net effect of leaving the markstack as it
             * was.  Not pushing onto the stack (whereas padsv does touch
             * the stack) makes no difference in void context.
             */
            assert(followop);
            if (gvoid) {
                if (followop->op_type == OP_LIST
                        && OP_GIMME(followop,0) == G_VOID
                   )
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
                OpTYPE_set(o, OP_PADRANGE);
                o->op_targ = base;
                /* bit 7: INTRO; bit 6..0: count */
                o->op_private = (intro | count);
                o->op_flags = ((o->op_flags & ~(OPf_WANT|OPf_SPECIAL))
                              | gvoid * OPf_WANT_VOID
                              | (defav ? OPf_SPECIAL : 0));
            }
            break;
        }

	case OP_PADAV:
	case OP_PADSV:
	case OP_PADHV:
	/* Skip over state($x) in void context.  */
	if (oldop && o->op_private == (OPpPAD_STATE|OPpLVAL_INTRO)
	 && (o->op_flags & OPf_WANT) == OPf_WANT_VOID)
	{
	    oldop->op_next = o->op_next;
	    goto redo_nextstate;
	}
	if (o->op_type != OP_PADAV)
	    break;
	/* FALLTHROUGH */
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
		if (o->op_type != OP_GV)
		    break;
	    }

	    /* Remove $foo from the op_next chain in void context.  */
	    if (oldop
	     && (  o->op_next->op_type == OP_RV2SV
		|| o->op_next->op_type == OP_RV2AV
		|| o->op_next->op_type == OP_RV2HV  )
	     && (o->op_next->op_flags & OPf_WANT) == OPf_WANT_VOID
	     && !(o->op_next->op_private & OPpLVAL_INTRO))
	    {
		oldop->op_next = o->op_next->op_next;
		/* Reprocess the previous op if it is a nextstate, to
		   allow double-nextstate optimisation.  */
	      redo_nextstate:
		if (oldop->op_type == OP_NEXTSTATE) {
		    oldop->op_opt = 0;
		    o = oldop;
		    oldop = oldoldop;
		    oldoldop = NULL;
		    goto redo;
		}
		o = oldop->op_next;
                goto redo;
	    }
	    else if (o->op_next->op_type == OP_RV2SV) {
		if (!(o->op_next->op_private & OPpDEREF)) {
		    op_null(o->op_next);
		    o->op_private |= o->op_next->op_private & (OPpLVAL_INTRO
							       | OPpOUR_INTRO);
		    o->op_next = o->op_next->op_next;
                    OpTYPE_set(o, OP_GVSV);
		}
	    }
	    else if (o->op_next->op_type == OP_READLINE
		    && o->op_next->op_next->op_type == OP_CONCAT
		    && (o->op_next->op_next->op_flags & OPf_STACKED))
	    {
		/* Turn "$a .= <FH>" into an OP_RCATLINE. AMS 20010917 */
                OpTYPE_set(o, OP_RCATLINE);
		o->op_flags |= OPf_STACKED;
		op_null(o->op_next->op_next);
		op_null(o->op_next);
	    }

	    break;
        
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
            sop = OpSIBLING(fop);
	    while (cLOGOP->op_other->op_type == OP_NULL)
		cLOGOP->op_other = cLOGOP->op_other->op_next;
	    while (o->op_next && (   o->op_type == o->op_next->op_type
				  || o->op_next->op_type == OP_NULL))
		o->op_next = o->op_next->op_next;

	    /* If we're an OR and our next is an AND in void context, we'll
	       follow its op_other on short circuit, same for reverse.
	       We can't do this with OP_DOR since if it's true, its return
	       value is the underlying value which must be evaluated
	       by the next op. */
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
	    /* GERONIMO! */ /* FALLTHROUGH */

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
                OP * const nullop = OpSIBLING(cLISTOP->op_first);
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
	    if (!OpHAS_SIBLING(cUNOPo)) {
		/* Nothing follows us on the list. */
		OP * const reverse = o->op_next;

		if (reverse->op_type == OP_REVERSE &&
		    (reverse->op_flags & OPf_WANT) == OPf_WANT_LIST) {
		    OP * const pushmark = cUNOPx(reverse)->op_first;
		    if (pushmark && (pushmark->op_type == OP_PUSHMARK)
			&& (OpSIBLING(cUNOPx(pushmark)) == o)) {
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

	    exlist = (LISTOP *) OpSIBLING(expushmark);
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

	    if (OpSIBLING(theirmark) != o) {
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

	    rv2av = OpSIBLING(ourmark);
	    if (rv2av && rv2av->op_type == OP_RV2AV && !OpHAS_SIBLING(rv2av)
		&& rv2av->op_flags == (OPf_WANT_LIST | OPf_KIDS)) {
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

            oldoldop = NULL;
            oldop    = ourlast;
            o        = oldop->op_next;
            goto redo;
	    
	    break;
	}

	case OP_QR:
	case OP_MATCH:
	    if (!(cPMOP->op_pmflags & PMf_ONCE)) {
		assert (!cPMOP->op_pmstashstartu.op_pmreplstart);
	    }
	    break;

	case OP_RUNCV:
	    if (!(o->op_private & OPpOFFBYONE) && !CvCLONE(PL_compcv)
	     && (!CvANON(PL_compcv) || (!PL_cv_has_eval && !PL_perldb)))
	    {
		SV *sv;
		if (CvEVAL(PL_compcv)) sv = &PL_sv_undef;
		else {
		    sv = newRV((SV *)PL_compcv);
		    sv_rvweaken(sv);
		    SvREADONLY_on(sv);
		}
                OpTYPE_set(o, OP_CONST);
		o->op_flags |= OPf_SPECIAL;
		cSVOPo->op_sv = sv;
	    }
	    break;

	case OP_SASSIGN:
	    if (OP_GIMME(o,0) == G_VOID
	     || (  o->op_next->op_type == OP_LINESEQ
		&& (  o->op_next->op_next->op_type == OP_LEAVESUB
		   || (  o->op_next->op_next->op_type == OP_RETURN
		      && !CvLVALUE(PL_compcv)))))
	    {
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
		    OP *left = OpSIBLING(right);
		    if (left->op_type == OP_SUBSTR
			 && (left->op_private & 7) < 4) {
			op_null(o);
                        /* cut out right */
                        op_sibling_splice(o, NULL, 1, NULL);
                        /* and insert it as second child of OP_SUBSTR */
                        op_sibling_splice(left, cBINOPx(left)->op_first, 0,
                                    right);
			left->op_private |= OPpSUBSTR_REPL_FIRST;
			left->op_flags =
			    (o->op_flags & ~OPf_WANT) | OPf_WANT_VOID;
		    }
		}
	    }
	    break;

	case OP_AASSIGN: {
            int l, r, lr, lscalars, rscalars;

            /* handle common vars detection, e.g. ($a,$b) = ($b,$a).
               Note that we do this now rather than in newASSIGNOP(),
               since only by now are aliased lexicals flagged as such

               See the essay "Common vars in list assignment" above for
               the full details of the rationale behind all the conditions
               below.

               PL_generation sorcery:
               To detect whether there are common vars, the global var
               PL_generation is incremented for each assign op we scan.
               Then we run through all the lexical variables on the LHS,
               of the assignment, setting a spare slot in each of them to
               PL_generation.  Then we scan the RHS, and if any lexicals
               already have that value, we know we've got commonality.
               Also, if the generation number is already set to
               PERL_INT_MAX, then the variable is involved in aliasing, so
               we also have potential commonality in that case.
             */

            PL_generation++;
            /* scan LHS */
            lscalars = 0;
            l = S_aassign_scan(aTHX_ cLISTOPo->op_last,  FALSE, 1, &lscalars);
            /* scan RHS */
            rscalars = 0;
            r = S_aassign_scan(aTHX_ cLISTOPo->op_first, TRUE, 1, &rscalars);
            lr = (l|r);


            /* After looking for things which are *always* safe, this main
             * if/else chain selects primarily based on the type of the
             * LHS, gradually working its way down from the more dangerous
             * to the more restrictive and thus safer cases */

            if (   !l                      /* () = ....; */
                || !r                      /* .... = (); */
                || !(l & ~AAS_SAFE_SCALAR) /* (undef, pos()) = ...; */
                || !(r & ~AAS_SAFE_SCALAR) /* ... = (1,2,length,undef); */
                || (lscalars < 2)          /* ($x, undef) = ... */
            ) {
                NOOP; /* always safe */
            }
            else if (l & AAS_DANGEROUS) {
                /* always dangerous */
                o->op_private |= OPpASSIGN_COMMON_SCALAR;
                o->op_private |= OPpASSIGN_COMMON_AGG;
            }
            else if (l & (AAS_PKG_SCALAR|AAS_PKG_AGG)) {
                /* package vars are always dangerous - too many
                 * aliasing possibilities */
                if (l & AAS_PKG_SCALAR)
                    o->op_private |= OPpASSIGN_COMMON_SCALAR;
                if (l & AAS_PKG_AGG)
                    o->op_private |= OPpASSIGN_COMMON_AGG;
            }
            else if (l & ( AAS_MY_SCALAR|AAS_MY_AGG
                          |AAS_LEX_SCALAR|AAS_LEX_AGG))
            {
                /* LHS contains only lexicals and safe ops */

                if (l & (AAS_MY_AGG|AAS_LEX_AGG))
                    o->op_private |= OPpASSIGN_COMMON_AGG;

                if (l & (AAS_MY_SCALAR|AAS_LEX_SCALAR)) {
                    if (lr & AAS_LEX_SCALAR_COMM)
                        o->op_private |= OPpASSIGN_COMMON_SCALAR;
                    else if (   !(l & AAS_LEX_SCALAR)
                             && (r & AAS_DEFAV))
                    {
                        /* falsely mark
                         *    my (...) = @_
                         * as scalar-safe for performance reasons.
                         * (it will still have been marked _AGG if necessary */
                        NOOP;
                    }
                    else if (r  & (AAS_PKG_SCALAR|AAS_PKG_AGG|AAS_DANGEROUS))
                        o->op_private |= OPpASSIGN_COMMON_RC1;
                }
            }

            /* ... = ($x)
             * may have to handle aggregate on LHS, but we can't
             * have common scalars. */
            if (rscalars < 2)
                o->op_private &=
                        ~(OPpASSIGN_COMMON_SCALAR|OPpASSIGN_COMMON_RC1);

	    break;
        }

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
considered internal to C<OP_NAME> and the other access macros: use them instead.
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
		    NOT_REACHED; /* NOTREACHED */
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
		    NOT_REACHED; /* NOTREACHED */
		    break;
		}
	    }
	}
        /* On some platforms (HP-UX, IA64) gcc emits a warning for this function:
         * op.c: In function 'Perl_custom_op_get_field':
         * op.c:...: warning: 'any.xop_name' may be used uninitialized in this function [-Wmaybe-uninitialized]
         * This is because on those platforms (with -DEBUGGING) NOT_REACHED
         * expands to assert(0), which expands to ((0) ? (void)0 :
         * __assert(...)), and gcc doesn't know that __assert can never return. */
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

=for apidoc core_prototype

This function assigns the prototype of the named core function to C<sv>, or
to a new mortal SV if C<sv> is C<NULL>.  It returns the modified C<sv>, or
C<NULL> if the core function has no prototype.  C<code> is a code as returned
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
    case KEY_keys:    retsetpvs("\\[%@]", OP_KEYS);
    case KEY_values:  retsetpvs("\\[%@]", OP_VALUES);
    case KEY_each:    retsetpvs("\\[%@]", OP_EACH);
    case KEY_push:    retsetpvs("\\@@", OP_PUSH);
    case KEY_unshift: retsetpvs("\\@@", OP_UNSHIFT);
    case KEY_pop:     retsetpvs(";\\@", OP_POP);
    case KEY_shift:   retsetpvs(";\\@", OP_SHIFT);
    case KEY_pos:     retsetpvs(";\\[$*]", OP_POS);
    case KEY_splice:
	retsetpvs("\\@;$$@", OP_SPLICE);
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
	/* FALLTHROUGH */
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
	    o = op_convert_list(opnum,OPf_SPECIAL*(opnum == OP_GLOB),argop);
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
			  SVfARG(name));
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
C<opcode> specifies which type of op is to be affected.  C<new_checker>
is a pointer to the C function that is to be added to that opcode's
check chain, and C<old_checker_p> points to the storage location where a
pointer to the next function in the chain will be stored.  The value of
C<new_pointer> is written into the L</PL_check> array, while the value
previously stored there is written to C<*old_checker_p>.

The function should be defined like this:

    static OP *new_checker(pTHX_ OP *op) { ... }

It is intended to be called in this manner:

    new_checker(aTHX_ op)

C<old_checker_p> should be defined like this:

    static Perl_check_t old_checker_p;

L</PL_check> is global to an entire process, and a module wishing to
hook op checking may find itself invoked more than once per process,
typically in different threads.  To handle that situation, this function
is idempotent.  The location C<*old_checker_p> must initially (once
per process) contain a null pointer.  A C variable of static duration
(declared at file scope, typically also marked C<static> to give
it internal linkage) will be implicitly initialised appropriately,
if it does not have an explicit initialiser.  This function will only
actually modify the check chain if it finds C<*old_checker_p> to be null.
This function is also thread safe on the small scale.  It uses appropriate
locking to avoid race conditions in accessing L</PL_check>.

When this function is called, the function referenced by C<new_checker>
must be ready to be called, except for C<*old_checker_p> being unfilled.
In a threading situation, C<new_checker> may be called immediately,
even before this function has returned.  C<*old_checker_p> will always
be appropriately set before C<new_checker> is called.  If C<new_checker>
decides not to do anything special with an op that it is given (which
is the usual case for most uses of op check hooking), it must chain the
check function referenced by C<*old_checker_p>.

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

    PERL_UNUSED_CONTEXT;
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
 * ex: set ts=8 sts=4 sw=4 et:
 */
