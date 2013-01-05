/*	$OpenBSD: cdefs.h,v 1.5 2013/01/05 11:20:56 miod Exp $ */

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	_M88K_CDEFS_H_
#define	_M88K_CDEFS_H_

#if defined(lint)
#define __indr_reference(sym,alias)	__lint_equal__(sym,alias)
#define __warn_references(sym,msg)
#define __weak_alias(alias,sym)		__lint_equal__(sym,alias)
#elif defined(__GNUC__) && defined(__STDC__)
#define __weak_alias(alias,sym)				\
	__asm__(".weak " __STRING(alias) " ; "		\
	    __STRING(alias) " = " __STRING(sym))
#define __warn_references(sym,msg)			\
	__asm__(".section .gnu.warning." __STRING(sym)	\
	    " ; .ascii \"" msg "\" ; .text")
#endif

#endif /* _M88K_CDEFS_H_ */
