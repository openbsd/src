/*	$OpenBSD: ops.h,v 1.1.1.1 1996/09/07 21:40:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * ops.h: Things mostly shared between normal.c, cmdline.c and ops.c
 */

/*
 * Operators
 */
#define NOP 	0				/* no pending operation */
#define DELETE	1
#define YANK	2
#define CHANGE	3
#define LSHIFT	4				/* left shift */
#define RSHIFT	5				/* right shift */
#define FILTER	6
#define TILDE	7				/* switch case */
#define INDENT	8
#define FORMAT	9
#define COLON	10
#define UPPER	11				/* make upper case */
#define LOWER	12				/* make lower case */
#define JOIN	13				/* only for visual mode */
#define GFORMAT 14				/* "gq" */

/*
 * operator characters; the order must correspond to the defines above!
 */
EXTERN char_u *opchars INIT(= (char_u *)"dyc<>!~=Q:UuJq");

/*
 * When a cursor motion command is made, it is marked as being a character or
 * line oriented motion. Then, if an operator is in effect, the operation
 * becomes character or line oriented accordingly.
 *
 * Character motions are marked as being inclusive or not. Most char. motions
 * are inclusive, but some (e.g. 'w') are not.
 *
 * Generally speaking, every command in normal() should either clear any pending
 * operator (with CLEAROP), or set the motion type variable.
 */

/*
 * Motion types
 */
#define MCHAR	0
#define MLINE	1
#define MBLOCK	2

EXTERN int		op_type INIT(= NOP);	/* current pending operator type */
EXTERN int		op_motion_type;			/* type of the current cursor motion */
EXTERN int		op_inclusive;			/* TRUE if char motion is inclusive */
EXTERN int		op_block_mode INIT(= FALSE);
									/* current operator is Visual block mode */
EXTERN colnr_t	op_start_vcol;			/* start col for block mode operator */
EXTERN colnr_t	op_end_vcol;			/* end col for block mode operator */
EXTERN int		op_end_adjusted;		/* backuped op_end one char */
EXTERN long		op_line_count;			/* number of lines from op_start to
											op_end (inclusive) */
EXTERN int		op_empty;				/* op_start and op_end the same */
EXTERN int		op_is_VIsual;			/* opeartor on visual area */

EXTERN int		yankbuffer INIT(= 0);	/* current yank buffer */
