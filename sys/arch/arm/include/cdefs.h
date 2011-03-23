/*	$OpenBSD: cdefs.h,v 1.3 2011/03/23 16:54:34 pirofti Exp $	*/

#ifndef	_ARM_CDEFS_H_
#define	_ARM_CDEFS_H_

#if defined(lint)
#define __indr_reference(sym,alias)	__lint_equal__(sym,alias)
#define __warn_references(sym,msg)
#define __weak_alias(alias,sym)		__lint_equal__(sym,alias)
#elif defined(__GNUC__) && defined(__STDC__)
#define __weak_alias(alias,sym)					\
	__asm__(".weak " __STRING(alias) " ; " __STRING(alias)	\
	    " = " __STRING(sym))
#define	__warn_references(sym,msg)				\
	__asm__(".section .gnu.warning." __STRING(sym)		\
	    " ; .ascii \"" msg "\" ; .text")
#endif

#endif /* !_ARM_CDEFS_H_ */
