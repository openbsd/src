/*
 * Copyright (c) 2009 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#include <sys/types.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include "compat.h"
#include "logging.h"

#ifdef HAVE_BSM_AUDIT
# include "bsm_audit.h"
#endif

void
#ifdef __STDC__
audit_success(char **exec_args)
#else
audit_success(exec_args)
    const char **exec_args;
#endif
{
#ifdef HAVE_BSM_AUDIT
    bsm_audit_success(exec_args);
#endif
}

void
#ifdef __STDC__
audit_failure(char **exec_args, char const *const fmt, ...)
#else
audit_failure(exec_args, fmt, va_alist)
    const char **exec_args;
    char const *const fmt;
    va_dcl
#endif
{
    va_list ap;

#ifdef __STDC__
    va_start(ap, fmt);
#else
    va_start(ap);
#endif
#ifdef HAVE_BSM_AUDIT
    bsm_audit_failure(exec_args, fmt, ap);
#endif
    va_end(ap);
}
