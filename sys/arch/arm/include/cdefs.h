/*	$OpenBSD: cdefs.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*	$NetBSD: cdefs.h,v 1.1 2001/01/10 19:02:05 bjh21 Exp $	*/

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#define __weak_alias(alias,sym)                                         \
    __asm__(".weak " __STRING(alias) " ; " __STRING(alias) " = " __STRING(sym))
#define	__warn_references(sym,msg)					\
    __asm__(".section .gnu.warning." __STRING(sym) " ; .ascii \"" msg "\" ; .text")


#endif /* !_MACHINE_CDEFS_H_ */
