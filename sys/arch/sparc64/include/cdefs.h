/*	$OpenBSD: cdefs.h,v 1.2 2001/08/23 16:10:46 art Exp $	*/
/*	$NetBSD: cdefs.h,v 1.3 1999/03/20 01:40:26 thorpej Exp $	*/

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#define __weak_alias(alias,sym)                                         \
    __asm__(".weak " __STRING(alias) " ; " __STRING(alias) " = " __STRING(sym))
#define __warn_references(sym,msg)                                      \
    __asm__(".section .gnu.warning." __STRING(sym) " ; .ascii \"" msg "\" ; .text")


#endif /* !_MACHINE_CDEFS_H_ */
