/*	$OpenBSD: varargs.h,v 1.6 2002/03/23 10:40:27 miod Exp $	*/

#ifndef _M88K_VARARGS_H_
#define _M88K_VARARGS_H_

#define	_VARARGS_H

#include <machine/va-m88k.h>

/* Define va_list from __gnuc_va_list.  */
typedef	__gnuc_va_list	va_list;
typedef	_BSD_VA_LIST_	__gnuc_va_list;

#endif	/* _M88K_VARARGS_H_ */
