/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board.
 *
 * For copyright info, see mit-sipb-copyright.h.
 */

#include <stdio.h>
#include "com_err.h"
#include "mit-sipb-copyright.h"
#include "error_table.h"
#include "internal.h"

#if !defined(__STDC__) && !defined(STDARG_PROTOTYPES)
#include <varargs.h>
#define VARARGS
#endif

static void
#ifdef __STDC__
    default_com_err_proc (const char *whoami, errcode_t code, const
			  char *fmt, va_list args)
#else
    default_com_err_proc (whoami, code, fmt, args)
    const char *whoami;
    errcode_t code;
    const char *fmt;
    va_list args;
#endif
{
    if (whoami) {
	fputs(whoami, stderr);
	fputs(": ", stderr);
    }
    if (code) {
	fputs(error_message(code), stderr);
	fputs(" ", stderr);
    }
    if (fmt) {
        vfprintf (stderr, fmt, args);
    }
    /* should do this only on a tty in raw mode */
    putc('\r', stderr);
    putc('\n', stderr);
    fflush(stderr);
}

#ifdef __STDC__
typedef void (*errf) (const char *, errcode_t, const char *, va_list);
#else
typedef void (*errf) ();
#endif

errf com_err_hook = default_com_err_proc;

#ifdef __STDC__
void com_err_va (const char *whoami, errcode_t code, const char *fmt,
		 va_list args)
#else
void com_err_va (whoami, code, fmt, args)
    const char *whoami;
    errcode_t code;
    const char *fmt;
    va_list args;
#endif
{
    (*com_err_hook) (whoami, code, fmt, args);
}

#ifndef VARARGS
void com_err (const char *whoami,
	      errcode_t code,
	      const char *fmt, ...)
{
#else
void com_err (va_alist)
    va_dcl
{
    const char *whoami, *fmt;
    errcode_t code;
#endif
    va_list pvar;

    if (!com_err_hook)
	com_err_hook = default_com_err_proc;
#ifdef VARARGS
    va_start (pvar);
    whoami = va_arg (pvar, const char *);
    code = va_arg (pvar, errcode_t);
    fmt = va_arg (pvar, const char *);
#else
    va_start(pvar, fmt);
#endif
    com_err_va (whoami, code, fmt, pvar);
    va_end(pvar);
}

errf set_com_err_hook (new_proc)
    errf new_proc;
{
    errf x = com_err_hook;

    if (new_proc)
	com_err_hook = new_proc;
    else
	com_err_hook = default_com_err_proc;

    return x;
}

errf reset_com_err_hook () {
    errf x = com_err_hook;
    com_err_hook = default_com_err_proc;
    return x;
}
