/*
 * Header file for common error description library.
 *
 * Copyright 1988, Student Information Processing Board of the
 * Massachusetts Institute of Technology.
 *
 * For copyright and distribution info, see the documentation supplied
 * with this package.
 */

#ifndef __COM_ERR_H

typedef long errcode_t;

#ifdef __STDC__
#include <stdarg.h>

/* ANSI C -- use prototypes etc */
void com_err (const char *, long, const char *, ...);
void com_err_va (const char *whoami, errcode_t code, const char *fmt,
		 va_list args);
char const *error_message (long);
extern void (*com_err_hook) (const char *, long, const char *, va_list);
void (*set_com_err_hook (void (*) (const char *, long, const char *, va_list)))
    (const char *, long, const char *, va_list);
void (*reset_com_err_hook (void)) (const char *, long, const char *, va_list);
int init_error_table(const char * const *msgs, int base, int count);
#else
/* no prototypes */
void com_err ();
void com_err_va ();
char *error_message ();
extern void (*com_err_hook) ();
void (*set_com_err_hook ()) ();
void (*reset_com_err_hook ()) ();
int init_error_table();
#endif

#define __COM_ERR_H
#endif /* ! defined(__COM_ERR_H) */
