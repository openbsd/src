/*	$NetBSD: debug.h,v 1.6 1994/10/27 04:17:06 cgd Exp $	*/

#ifdef INTR_DEBUG
#define INTRLOCAL(label) label
#else /* not INTR_DEBUG */
#define INTRLOCAL(label) L/**/label
#endif /* INTR_DEBUG */

#define	BDBTRAP(name) \
	ss ; \
	cmpb	$0,_bdb_exists ; \
	je	1f ; \
	testb	$SEL_RPL_MASK,4(%esp) ; \
	jne	1f ; \
	ss ; \
bdb_/**/name/**/_ljmp: ; \
	ljmp	$0,$0 ; \
1:

	.data
	.globl	_bdb_exists
_bdb_exists:
	.long	0
	.text
