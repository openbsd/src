/*	$OpenBSD: com_err.h,v 1.2 1996/12/16 03:17:16 downsj Exp $	*/

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

typedef long errcode_t;		/* XXX compatibilty with newer version */

#include <stdarg.h>

/* ANSI C -- use prototypes etc */
void com_err __P((const char *, long, const char *, ...));
void com_err_va __P((const char *, long, const char *, va_list));
char const *error_message __P((long));
void (*com_err_hook) __P((const char *, long, const char *, va_list));
void (*set_com_err_hook __P((void (*) (const char *, long, const char *, va_list))))
    __P((const char *, long, const char *, va_list));
void (*reset_com_err_hook __P((void)))
    __P((const char *, long, const char *, va_list));

#endif /* ! defined(__COM_ERR_H) */
