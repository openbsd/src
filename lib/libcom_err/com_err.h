/*	$OpenBSD: com_err.h,v 1.3 1998/05/13 17:53:26 art Exp $	*/

/*-
 * Header file for common error description library.
 *
 * Copyright 1988, Student Information Processing Board of the
 * Massachusetts Institute of Technology.
 *
 * For copyright and distribution info, see the documentation supplied
 * with this package.
 */

#ifndef __COM_ERR_H
#define __COM_ERR_H

typedef int errcode_t;		/* XXX compatibilty with newer version */

#include <stdarg.h>

/* ANSI C -- use prototypes etc */
void com_err __P((const char *, errcode_t, const char *, ...));
void com_err_va __P((const char *, errcode_t, const char *, va_list));
char const *error_message __P((long));
void (*com_err_hook) __P((const char *, errcode_t, const char *, va_list));
void (*set_com_err_hook __P((void (*) (const char *, errcode_t, const char *, va_list))))
    __P((const char *, errcode_t, const char *, va_list));
void (*reset_com_err_hook __P((void)))
    __P((const char *, errcode_t, const char *, va_list));

#endif /* ! defined(__COM_ERR_H) */
