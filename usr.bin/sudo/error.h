/*
 * Copyright (c) 2004 Todd C. Miller <Todd.Miller@courtesan.com>
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

#ifndef _SUDO_ERROR_H_
#define	_SUDO_ERROR_H_

#ifdef __STDC__
# include <stdarg.h>
void	error(int, const char *, ...) __attribute__((__noreturn__));
void	errorx(int, const char *, ...) __attribute__((__noreturn__));
void	warning(const char *, ...);
void	warningx(const char *, ...);
#else
# include <varargs.h>
void	error() __attribute__((__noreturn__));
void	errorx() __attribute__((__noreturn__));
void	warning();
void	warningx();
#endif /* __STDC__ */

#endif /* _SUDO_ERROR_H_ */
