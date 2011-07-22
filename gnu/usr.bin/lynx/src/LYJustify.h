/*
 * $LynxId: LYJustify.h,v 1.7 2009/04/07 00:24:15 tom Exp $
 *
 * Justification for lynx - implemented by Vlad Harchev <hvv@hippo.ru>
 * 11 July 1999
 */

#ifndef LYJUSTIFY_H
#define LYJUSTIFY_H

#include <HTUtils.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef EXP_JUSTIFY_ELTS
    extern BOOL can_justify_here;
    extern BOOL can_justify_here_saved;

    extern BOOL can_justify_this_line;
    extern int wait_for_this_stacked_elt;
    extern BOOL form_in_htext;

/* this is the element with SGML_EMPTY content, so it won't get on the stack,
 * so we can't trap it with wait_for_this_stacked_elt
 */
    extern BOOL in_DT;

/*disabled by default*/
/*#define DEBUG_JUSTIFY*/
#ifdef DEBUG_JUSTIFY
    extern BOOL can_justify_stack_depth;	/* can be 0 or 1 if all code is correct */

#  define CAN_JUSTIFY_STACK_INC ++can_justify_stack_depth;\
	assert(can_justify_stack_depth < 2 && can_justify_stack_depth >=0 );
#  define CAN_JUSTIFY_STACK_DEC --can_justify_stack_depth;\
	assert(can_justify_stack_depth < 2 && can_justify_stack_depth >=0 );
#else
#  define CAN_JUSTIFY_STACK_INC	/* nothing */
#  define CAN_JUSTIFY_STACK_DEC	/* nothing */
#endif

#define CAN_JUSTIFY_PUSH(x) can_justify_here_saved=can_justify_here;\
	can_justify_here=(x); CAN_JUSTIFY_STACK_INC
#define CAN_JUSTIFY_POP can_justify_here=can_justify_here_saved;\
	CAN_JUSTIFY_STACK_INC
#define CAN_JUSTIFY_SET(x) can_justify_here=(x);

/*
 * This is used to indicate that starting from the current offset in current
 * line justification can take place (in order the gap between some prefix and
 * the word not to be enlarged.
 * For example, when forming OL,
 *     1.21 foo
 * 	   ^justification can start here so that gap between 1.21 and "foo"
 *	   will not be enlarged.
 * This is a macro (that uses 'me').
 */
#define CAN_JUSTIFY_START  mark_justify_start_position(me->text);
#define CANT_JUSTIFY_THIS_LINE can_justify_this_line = FALSE
#define EMIT_IFDEF_EXP_JUSTIFY_ELTS(x) x
    /*defined in order not to wrap single line of code  into #ifdef/#endif */

    extern void ht_justify_cleanup(void);
    extern void mark_justify_start_position(void *text);

#else				/* ! EXP_JUSTIFY_ELTS */
/*
 * define empty macros so that they can be used without wrapping them in
 * #ifdef EXP_JUSTIFY_ELTS/#endif
 */
#define CAN_JUSTIFY_PUSH(x)
#define CAN_JUSTIFY_POP
#define CAN_JUSTIFY_SET(x)
#define CAN_JUSTIFY_START
#define CANT_JUSTIFY_THIS_LINE
#define EMIT_IFDEF_EXP_JUSTIFY_ELTS(x)
#endif				/* EXP_JUSTIFY_ELTS */
#define CAN_JUSTIFY_PUSH_F CAN_JUSTIFY_PUSH(FALSE)
#ifdef __cplusplus
}
#endif
#endif				/* LYJUSTIFY_H */
