/*	$OpenBSD: stdarg.h,v 1.5 2001/08/26 14:31:07 miod Exp $	*/

/* This file has local changes by MOTOROLA
Thu Sep  9 09:06:29 CDT 1993 Dale Rahn (drahn@pacific)
	* (gstdarg.h, gvarargs.h) C-Front requires all builtins to
	be defined.  This is to insert these definitions if
	__cplusplus is defined but not using the G++ compiler.
 */
/* stdarg.h for GNU.
   Note that the type used in va_arg is supposed to match the
   actual type **after default promotions**.
   Thus, va_arg (..., short) is not valid.  */

#ifndef _STDARG_H
#ifndef _ANSI_STDARG_H_
#ifndef __need___va_list
#define _STDARG_H
#define _ANSI_STDARG_H_
#endif /* not __need___va_list */
#undef __need___va_list

#ifndef __GNUC__
/* Use the system's macros with the system's compiler.
   This is relevant only when building GCC with some other compiler.  */
#include <stdarg.h>
#else
#include <machine/va-m88k.h>

#ifdef _STDARG_H
/* Define va_list, if desired, from __gnuc_va_list. */
/* We deliberately do not define va_list when called from
   stdio.h, because ANSI C says that stdio.h is not supposed to define
   va_list.  stdio.h needs to have access to that data type, 
   but must not use that name.  It should use the name __gnuc_va_list,
   which is safe because it is reserved for the implementation.  */

#ifdef _BSD_VA_LIST_
#undef _BSD_VA_LIST_
#define _BSD_VA_LIST_	__gnuc_va_list
#endif /* _BSD_VA_LIST_ */

#if !defined (_VA_LIST_)
#define _VA_LIST_
typedef __gnuc_va_list va_list;
#endif /* not _VA_LIST_ */

#endif /* _STDARG_H */

#endif /* __GNUC__ */
#endif /* not _ANSI_STDARG_H_ */
#endif /* not _STDARG_H */
