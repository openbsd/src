/*	$OpenBSD: cdefs.h,v 1.8 2013/03/28 17:30:45 martynas Exp $	*/

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#define __indr_reference(sym,alias)			\
	__asm__(".stabs \"_" #alias "\",11,0,0,0");	\
	__asm__(".stabs \"_" #sym "\",1,0,0,0")
#define __warn_references(sym,msg)			\
	__asm__(".stabs \"" msg "\",30,0,0,0");		\
	__asm__(".stabs \"_" #sym "\",1,0,0,0")
#define __strong_alias(alias,sym)			\
	__asm__(".global _" #alias "; _" #alias "= _" __STRING(sym))
#define __weak_alias(alias,sym)				\
	__asm__(".weak _" #alias "; _" #alias "= _" __STRING(sym))

#endif /* !_MACHINE_CDEFS_H_ */
