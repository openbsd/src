/*	$OpenBSD: varargs.h,v 1.1 2002/03/11 00:19:28 miod Exp $	*/

#ifndef _MACHINE_VARARGS_H_
#define	_MACHINE_VARARGS_H_

#include <machine/stdarg.h>

#define	__va_ellipsis	...

#define	va_alist	__builtin_va_alist
#define	va_dcl		long __builtin_va_list; __va_ellipsis

#undef	va_start
#define	va_start(ap) \
	((ap) = (va_list)&__builtin_va_alist)

#endif /* !_MACHINE_VARARGS_H */
