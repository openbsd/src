/*	$OpenBSD: cdefs.h,v 1.4 2002/02/19 03:17:45 drahn Exp $	*/
/*	$NetBSD: cdefs.h,v 1.1 1996/09/30 16:34:21 ws Exp $	*/

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	_POWERPC_CDEFS_H_
#define	_POWERPC_CDEFS_H_

#ifdef __STDC__
#define _C_LABEL(x)	_STRING(_ ## x)
#else
#define _C_LABEL(x)	_STRING(_/**/x)
#endif

#define __weak_alias(alias,sym)                                         \
    __asm__(".weak " __STRING(alias) " ; " __STRING(alias) " = " __STRING(sym))
#define	__warn_references(sym,msg)					\
    __asm__(".section .gnu.warning." __STRING(sym) " ; .ascii \"" msg "\" ; .text")

#endif /* !_POWERPC_CDEFS_H_ */
