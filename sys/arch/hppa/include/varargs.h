/*	$OpenBSD: varargs.h,v 1.2 2003/12/08 08:25:17 pvalchev Exp $	*/

#ifndef _MACHINE_VARARGS_H_
#define	_MACHINE_VARARGS_H_

#include <machine/stdarg.h>

#define	__va_ellipsis	...

#define	va_alist	__builtin_va_alist
#define	va_dcl		long __builtin_va_alist; __va_ellipsis

#undef	va_start
#define	va_start(ap) \
	((ap) = (va_list)&__builtin_va_alist)

#endif /* !_MACHINE_VARARGS_H */
