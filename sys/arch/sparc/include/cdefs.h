/*	$OpenBSD: cdefs.h,v 1.7 2002/02/14 04:23:33 fgsch Exp $	*/
/*	$NetBSD: cdefs.h,v 1.3 1996/12/27 20:51:31 pk Exp $	*/

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#ifdef __STDC__
#define _C_LABEL(x)	_STRING(_ ## x)
#else
#define _C_LABEL(x)	_STRING(_/**/x)
#endif

#ifdef __GNUC__
#ifdef __STDC__
#define __indr_reference(sym,alias)	\
	__asm__(".stabs \"_" #alias "\",11,0,0,0");	\
	__asm__(".stabs \"_" #sym "\",1,0,0,0")
#define __warn_references(sym,msg)	\
	__asm__(".stabs \"" msg "\",30,0,0,0");		\
	__asm__(".stabs \"_" #sym "\",1,0,0,0")
#define __weak_alias(alias,sym)		\
	__asm__(".weak _" #alias "; _" #alias "= _" __STRING(sym))
#else
#define __indr_reference(sym,alias)	\
	__asm__(".stabs \"_/**/alias\",11,0,0,0");	\
	__asm__(".stabs \"_/**/sym\",1,0,0,0")
#define __warn_references(sym,msg)	\
	__asm__(".stabs msg,30,0,0,0");			\
	__asm__(".stabs \"_/**/sym\",1,0,0,0")
#define __weak_alias(alias,sym)		\
	__asm__(".weak _/**/alias; _/**/alias = _/**/sym")
#endif
#else
#define __indr_reference(sym,alias)
#define __warn_references(sym,msg)
#define __weak_alias(alias,sym)
#endif

#endif /* !_MACHINE_CDEFS_H_ */
