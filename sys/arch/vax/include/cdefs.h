/*	$OpenBSD: cdefs.h,v 1.9 2013/07/05 21:10:50 miod Exp $	*/

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#define __warn_references(sym,msg)				\
	__asm__(".section .gnu.warning." __STRING(sym)		\
	    " ; .ascii \"" msg "\" ; .text")
#define __strong_alias(alias,sym)			\
	__asm__(".global " __STRING(alias) " ; " __STRING(alias)	\
	    " = " __STRING(sym))
#define __weak_alias(alias,sym)					\
	__asm__(".weak " __STRING(alias) " ; " __STRING(alias)	\
	    " = " __STRING(sym))

#endif /* !_MACHINE_CDEFS_H_ */
