/*	$Id: com_err.h,v 1.1.1.1 1995/12/14 06:52:34 tholo Exp $	*/

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

#include <stdarg.h>

/* ANSI C -- use prototypes etc */
void com_err __P((const char *, long, const char *, ...));
char const *error_message __P((long));
void (*com_err_hook) __P((const char *, long, const char *, va_list));
void (*set_com_err_hook __P((void (*) (const char *, long, const char *, va_list))))
    __P((const char *, long, const char *, va_list));
void (*reset_com_err_hook __P((void)))
    __P((const char *, long, const char *, va_list));

#endif /* ! defined(__COM_ERR_H) */
