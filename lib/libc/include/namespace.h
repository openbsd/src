/*	$OpenBSD: namespace.h,v 1.6 2015/09/10 18:13:46 guenther Exp $	*/

#ifndef _LIBC_NAMESPACE_H_
#define _LIBC_NAMESPACE_H_

/*
 * Copyright (c) 2015 Philip Guenther <guenther@openbsd.org>
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

/*
 * The goal: calls from inside libc to other libc functions should be via
 * identifiers that are of hidden visibility and--to avoid confusion--are
 * in the reserved namespace.  By doing this these calls are protected
 * from overriding by applications and on many platforms can avoid creation
 * or use of GOT or PLT entries.  I've chosen a prefix of underbar-C-underbar
 * ("_libc_") for this.  These will not be declared directly; instead, the
 * gcc "asm labels" extension will be used rename the function.
 *
 * For syscalls which are cancellation points, such as wait4(), there
 * are identifiers that do not provide cancellation:
 *	_libc_wait4		hidden alias, for use internal to libc only
 *	_thread_sys_wait4	global name, for use outside libc only
 * ...and identifiers that do provide cancellation:
 *	wait4			weak alias, for general use
 *	_libc_wait4_cancel	hidden alias, for use internal to libc only
 * Inside libc, the bare name ("wait4") binds to the version *without*
 * cancellation; the few times where cancellation is desired it can be
 * obtained by calling CANCEL(x) instead of just x.
 *
 * Some other calls need to be wrapped for reasons other than cancellation,
 * such as to provide functionality beyond the underlying syscall (e.g.,
 * setlogin).  For these, there are identifiers for the raw call, without
 * the wrapping:
 *	_libc_setlogin		hidden alias, for use internal to libc only
 *	_thread_sys_setlogin	global name, for use outside libc only
 * ...and identifiers that do provide the libc wrapping:
 *	setlogin		weak alias, for general use
 *	_libc_setlogin_wrap	hidden alias, for use internal to libc only
 * Inside libc, the bare name ("setlogin") binds to the wrapper; when the
 * raw version is necessary it can be obtained by calling HIDDEN(x) instead of
 * just x.
 *
 * For syscalls which are not cancellation points, such as getpid(),
 * the identifiers are just:
 *	_libc_getpid		hidden alias, for use internal to libc only
 *	_thread_sys_getpid	global name, for use outside libc only
 *	getpid			weak alias, for use outside libc only
 *
 * By using gcc's "asm label" extension, we can usually avoid having
 * to type those prefixes in the .h and .c files.  However, for those
 * cases where a non-default binding is necessary we can use these macros
 * to get the desired identifier:
 *
 *   CANCEL(x)
 *	This expands to the internal, hidden name of a cancellation
 *	wrapper: _libc_x_cancel.  ex: CANCEL(fsync)(fd)
 *
 *   WRAP(x)
 *	This expands to the internal, hidden name of a non-cancellation
 *	wrapper: _libc_x_wrap.  ex: WRAP(sigpending)(set)
 *
 *
 * In order to actually set up the desired asm labels, we use these in
 * the internal .h files:
 *   PROTO_NORMAL(x)		Symbols used both internally and externally
 *	This makes gcc convert use of x to use _libc_x instead
 *	ex: PROTO_NORMAL(getpid)
 *
 *   PROTO_STD_DEPRECATED(x)	Standard C symbols that we don't want to use
 * 	This just marks the symbol as deprecated, with no renaming.
 *	ex: PROTO_STD_DEPRECATED(strcpy)
 *
 *   PROTO_DEPRECATED(x)	Symbols not in ISO C that we don't want to use
 * 	This marks the symbol as both weak and deprecated, with no renaming
 *	ex: PROTO_DEPRECATED(creat)
 *
 *   PROTO_CANCEL(x)		Functions that have cancellation wrappers
 *	Like PROTO_NORMAL(x), but also declares _libc_x_cancel
 *	ex: PROTO_CANCEL(wait4)
 *
 *   PROTO_WRAP(x)		Functions that have wrappers for other reasons
 *	This makes gcc convert use of x to use _libc_x_wrap instead.
 *	ex: PROTO_WRAP(setlogin)
 *
 *
 * Finally, to create the expected aliases, we use these in the .c files
 * where the definitions are:
 *   DEF_STRONG(x)		Symbols reserved to or specified by ISO C
 *	This defines x as a strong alias for _libc_x; this must only
 *	be used for symbols that are reserved by the C standard
 *	(or reserved in the external identifier namespace).
 *	Matches with PROTO_NORMAL()
 *	ex: DEF_STRONG(fopen)
 *
 *   DEF_WEAK(x)		Symbols used internally and not in ISO C
 *	This defines x as a weak alias for _libc_x
 *	Matches with PROTO_NORMAL()
 *	ex: DEF_WEAK(lseek)
 *
 *   DEF_CANCEL(x)		Symbols that have a cancellation wrapper
 *	This defines x as a weak alias for _libc_x_cancel.
 *	Matches with PROTO_CANCEL()
 *	ex: DEF_CANCEL(read)
 *
 *   DEF_WRAP(x)
 *	This defines x as a weak alias for _libc_x_wrap.
 *	Matches with PROTO_WRAP()
 *	ex: DEF_WRAP(setlogin)
 *
 *   DEF_SYS(x)
 *	This defines _thread_sys_x as a strong alias for _libc_x.  This should
 *	only be needed for syscalls that have C instead of asm stubs.
 *	Matches with PROTO_NORMAL(), PROTO_CANCEL(), or PROTO_WRAP()
 *	ex: DEF_SYS(pread)
 */

#include <sys/cdefs.h>	/* for __dso_hidden and __{weak,strong}_alias */

#define	HIDDEN(x)		_libc_##x
#define	CANCEL(x)		_libc_##x##_cancel
#define	WRAP(x)			_libc_##x##_wrap
#define	HIDDEN_STRING(x)	"_libc_" __STRING(x)
#define	WRAP_STRING(x)		"_libc_" __STRING(x) "_wrap"

#define	PROTO_NORMAL(x)		__dso_hidden typeof(x) x asm(HIDDEN_STRING(x))
#define	PROTO_STD_DEPRECATED(x)	typeof(x) x __attribute__((deprecated))
#define	PROTO_DEPRECATED(x)	typeof(x) x __attribute__((deprecated, weak))
#define	PROTO_CANCEL(x)		PROTO_NORMAL(x), CANCEL(x)
#define	PROTO_WRAP(x)		__dso_hidden typeof(x) x asm(WRAP_STRING(x))

#define	DEF_STRONG(x)		__strong_alias(x, HIDDEN(x))
#define	DEF_WEAK(x)		__weak_alias(x, HIDDEN(x))
#define	DEF_CANCEL(x)		__weak_alias(x, CANCEL(x))
#define	DEF_WRAP(x)		__weak_alias(x, WRAP(x))
#define	DEF_SYS(x)		__strong_alias(_thread_sys_##x, HIDDEN(x))


/*
 * gcc will generate calls to the functions below.
 * Declare and redirect them here so we always go
 * directly to our hidden aliases.
 */
#include <sys/_types.h>
void	*memcpy(void *__restrict, const void *__restrict, __size_t);
void	*memset(void *, int, __size_t);
void	__stack_smash_handler(const char [], int __attribute__((__unused__)));
PROTO_NORMAL(memcpy);
PROTO_NORMAL(memset);
PROTO_NORMAL(__stack_smash_handler);

#endif  /* _LIBC_NAMESPACE_H_ */

