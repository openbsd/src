/*    run.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2004, 2005, 2006, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* This file contains the main Perl opcode execution loop. It just
 * calls the pp_foo() function associated with each op, and expects that
 * function to return a pointer to the next op to be executed, or null if
 * it's the end of the sub or program or whatever.
 *
 * There is a similar loop in dump.c, Perl_runops_debug(), which does
 * the same, but also checks for various debug flags each time round the
 * loop.
 *
 * Why this function requires a file all of its own is anybody's guess.
 * DAPM.
 */

#include "EXTERN.h"
#define PERL_IN_RUN_C
#include "perl.h"

/*
 * 'Away now, Shadowfax!  Run, greatheart, run as you have never run before!
 *  Now we are come to the lands where you were foaled, and every stone you
 *  know.  Run now!  Hope is in speed!'                    --Gandalf
 *
 *     [p.600 of _The Lord of the Rings_, III/xi: "The Palantír"]
 */

int
Perl_runops_standard(pTHX)
{
    OP *op = PL_op;
    PERL_DTRACE_PROBE_OP(op);
    while ((PL_op = op = op->op_ppaddr(aTHX))) {
        PERL_DTRACE_PROBE_OP(op);
    }
    PERL_ASYNC_CHECK();

    TAINT_NOT;
    return 0;
}


#ifdef PERL_RC_STACK

/* this is a wrapper for all runops-style functions. It temporarily
 * reifies the stack if necessary, then calls the real runops function
 */
int
Perl_runops_wrap(pTHX)
{
    /* runops loops assume a ref-counted stack. If we have been called via a
     * wrapper (pp_wrap or xs_wrap) with the top half of the stack not
     * reference-counted, or with a non-real stack, temporarily convert it
     * to reference-counted. This is because the si_stack_nonrc_base
     * mechanism only allows a single split in the stack, not multiple
     * stripes.
     * At the end, we revert the stack (or part thereof) to non-refcounted
     * to keep whoever our caller is happy.
     *
     * If what we call croaks, catch it, revert, then rethrow.
     */

    I32 cut;          /* the cut point between refcnted and non-refcnted */
    bool was_real  = cBOOL(AvREAL(PL_curstack));
    I32  old_base  = PL_curstackinfo->si_stack_nonrc_base;

    if (was_real && !old_base) {
        PL_runops(aTHX); /* call the real loop */
        return 0;
    }

    if (was_real) {
        cut = old_base;
        assert(PL_stack_base + cut <= PL_stack_sp + 1);
        PL_curstackinfo->si_stack_nonrc_base = 0;
    }
    else {
        assert(!old_base);
        assert(!AvREIFY(PL_curstack));
        AvREAL_on(PL_curstack);
        /* skip the PL_sv_undef guard at PL_stack_base[0] but still
         * signal adjusting may be needed on return by setting to a
         * non-zero value - even if stack is empty */
        cut = 1;
    }

    if (cut) {
        SV **svp = PL_stack_base + cut;
        while (svp <= PL_stack_sp) {
            SvREFCNT_inc_simple_void(*svp);
            svp++;
        }
    }

    AV * old_curstack = PL_curstack;

    /* run the real loop while catching exceptions */
    dJMPENV;
    int ret;
    JMPENV_PUSH(ret);
    switch (ret) {
    case 0: /* normal return from JMPENV_PUSH */
        cur_env.je_mustcatch = cur_env.je_prev->je_mustcatch;
        PL_runops(aTHX); /* call the real loop */

      revert:
        /* revert stack back its non-ref-counted state */
        assert(AvREAL(PL_curstack));

        if (cut) {
            /* undo the stack reification that took place at the beginning of
             * this function */
            if (UNLIKELY(!was_real))
                AvREAL_off(PL_curstack);

            SSize_t n = PL_stack_sp - (PL_stack_base + cut) + 1;
            if (n > 0) {
                /* we need to decrement the refcount of every SV from cut
                 * upwards; but this may prematurely free them, so
                 * mortalise them instead */
                EXTEND_MORTAL(n);
                for (SSize_t i = 0; i < n; i ++) {
                    SV* sv = PL_stack_base[cut + i];
                    if (sv)
                        PL_tmps_stack[++PL_tmps_ix] = sv;
                }
            }

            I32 sp1 = PL_stack_sp - PL_stack_base + 1;
            PL_curstackinfo->si_stack_nonrc_base =
                                old_base > sp1 ? sp1 : old_base;
        }
        break;

    case 3: /* exception trapped by eval - stack only partially unwound */

        /* if the exception has already unwound to before the current
         * stack, no need to fix it up */
        if (old_curstack == PL_curstack)
            goto revert;
        break;

    default:
        break;
    }

    JMPENV_POP;

    if (ret) {
        JMPENV_JUMP(ret); /* re-throw the exception */
        NOT_REACHED; /* NOTREACHED */
    }

    return 0;
}

#endif

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
