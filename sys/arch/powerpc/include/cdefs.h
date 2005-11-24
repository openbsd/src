/*	$OpenBSD: cdefs.h,v 1.5 2005/11/24 20:46:47 deraadt Exp $	*/

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	_POWERPC_CDEFS_H_
#define	_POWERPC_CDEFS_H_

#define _C_LABEL(x)	_STRING(_ ## x)

#if defined(lint)
#define __indr_reference(sym,alias)	__lint_equal__(sym,alias)
#define __warn_references(sym,msg)
#define __weak_alias(alias,sym)		__lint_equal__(sym,alias)
#elif defined(__GNUC__) && defined(__STDC__)
#define __weak_alias(alias,sym)				\
	__asm__(".weak " __STRING(alias) " ; "		\
	    __STRING(alias) " = " __STRING(sym))
#define	__warn_references(sym,msg)			\
	__asm__(".section .gnu.warning." __STRING(sym)	\
	    " ; .ascii \"" msg "\" ; .text")
#endif

#endif /* !_POWERPC_CDEFS_H_ */
