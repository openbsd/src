/*	$OpenBSD: cdefs.h,v 1.1 2004/04/26 12:34:05 miod Exp $ */
/*	$NetBSD: cdefs.h,v 1.2 1995/03/23 20:10:48 jtc Exp $	*/

/*
 * Written by J.T. Conklin <jtc@wimsey.com> 01/17/95.
 * Public domain.
 */

#ifndef	__MACHINE_CDEFS_H__
#define	__MACHINE_CDEFS_H__

#ifdef __STDC__
#define	_C_LABEL(name)		_ ## name
#else
#define	_C_LABEL(name)		_/**/name
#endif

#ifdef __GNUC__
#ifdef __STDC__
#define __indr_reference(sym,alias)	\
	__asm__(".stabs \"_" #alias "\",11,0,0,0");	\
	__asm__(".stabs \"_" #sym "\",1,0,0,0")
#define __warn_references(sym,msg)	\
	__asm__(".stabs \"" msg "\",30,0,0,0");		\
	__asm__(".stabs \"_" #sym "\",1,0,0,0")
#define __weak_alias(alias,sym)				\
	__asm__(".weak _" #alias "; _" #alias "= _" __STRING(sym))
#else
#define __indr_reference(sym,alias)	\
	__asm__(".stabs \"_/**/alias\",11,0,0,0");	\
	__asm__(".stabs \"_/**/sym\",1,0,0,0")
#define __warn_references(sym,msg)	\
	__asm__(".stabs msg,30,0,0,0");			\
	__asm__(".stabs \"_/**/sym\",1,0,0,0")
#define __weak_alias(alias,sym)				\
	__asm__(".weak _/**/alias; _/**/alias = _/**/sym")
#endif
#endif

#endif /* __MACHINE_CDEFS_H__ */
