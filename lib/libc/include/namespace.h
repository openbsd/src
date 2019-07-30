/*	$OpenBSD: namespace.h,v 1.12 2018/01/18 08:23:44 guenther Exp $	*/

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
 * sigaction).  For these, there are identifiers for the raw call, without
 * the wrapping:
 *	_libc_sigaction		hidden alias, for use internal to libc only
 *	_thread_sys_sigaction	global name, for use outside libc only
 * ...and identifiers that do provide the libc wrapping:
 *	sigaction		weak alias, for general use
 *	_libc_sigaction_wrap	hidden alias, for use internal to libc only
 * Inside libc, the bare name ("sigaction") binds to the wrapper; when the
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
 *	wrapper: _libc_x_wrap.  ex: WRAP(sigprocmask)(set)
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
 *	Like PROTO_NORMAL(x), but also declares _libc_x_wrap.  Internal
 *	calls that want the wrapper's processing should invoke WRAP(x)(...)
 *	ex: PROTO_WRAP(sigaction)
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
 *	ex: DEF_WRAP(sigaction)
 *
 *   DEF_SYS(x)
 *	This defines _thread_sys_x as a strong alias for _libc_x.  This should
 *	only be needed for syscalls that have C instead of asm stubs.
 *	Matches with PROTO_NORMAL(), PROTO_CANCEL(), or PROTO_WRAP()
 *	ex: DEF_SYS(pread)
 *
 *   MAKE_CLONE(dst, src)	Symbols that are exact clones of other symbols
 *	This declares _libc_dst as being the same type as dst, and makes
 *	_libc_dst a strong, hidden alias for _libc_src.  You still need to
 *	DEF_STRONG(dst) or DEF_WEAK(dst) to alias dst itself
 *	ex: MAKE_CLONE(SHA224Pad, SHA256Pad)
 */

#include <sys/cdefs.h>	/* for __dso_hidden and __{weak,strong}_alias */

#define	__dso_protected		__attribute__((__visibility__("protected")))

#define	HIDDEN(x)		_libc_##x
#define	CANCEL(x)		_libc_##x##_cancel
#define	WRAP(x)			_libc_##x##_wrap
#define	HIDDEN_STRING(x)	"_libc_" __STRING(x)
#define	CANCEL_STRING(x)	"_libc_" __STRING(x) "_cancel"
#define	WRAP_STRING(x)		"_libc_" __STRING(x) "_wrap"

#define	PROTO_NORMAL(x)		__dso_hidden typeof(x) x asm(HIDDEN_STRING(x))
#define	PROTO_STD_DEPRECATED(x)	typeof(x) x __attribute__((deprecated))
#define	PROTO_DEPRECATED(x)	typeof(x) x __attribute__((deprecated, weak))
#define	PROTO_CANCEL(x)		__dso_hidden typeof(x) HIDDEN(x), \
					x asm(CANCEL_STRING(x))
#define	PROTO_WRAP(x)		PROTO_NORMAL(x), WRAP(x)
#define	PROTO_PROTECTED(x)	__dso_protected typeof(x) x

#define	DEF_STRONG(x)		__strong_alias(x, HIDDEN(x))
#define	DEF_WEAK(x)		__weak_alias(x, HIDDEN(x))
#define	DEF_CANCEL(x)		__weak_alias(x, CANCEL(x))
#define	DEF_WRAP(x)		__weak_alias(x, WRAP(x))
#define	DEF_SYS(x)		__strong_alias(_thread_sys_##x, HIDDEN(x))
#ifdef __clang__
#define	DEF_BUILTIN(x)		__asm("")
#define	BUILTIN			__dso_protected
#else
#define	DEF_BUILTIN(x)		DEF_STRONG(x)
#define	BUILTIN
#endif

#define	MAKE_CLONE(dst, src)	__dso_hidden typeof(dst) HIDDEN(dst) \
				__attribute__((alias (HIDDEN_STRING(src))))


/*
 * gcc and clang will generate calls to the functions below.
 * Declare and redirect them here so we always go
 * directly to our hidden aliases.
 */
#include <sys/_types.h>
BUILTIN void	*memmove(void *, const void *, __size_t);
BUILTIN void	*memcpy(void *__restrict, const void *__restrict, __size_t);
BUILTIN void	*memset(void *, int, __size_t);
BUILTIN void	__stack_smash_handler(const char [], int __unused);
#ifndef __clang__
PROTO_NORMAL(memmove);
PROTO_NORMAL(memcpy);
PROTO_NORMAL(memset);
PROTO_NORMAL(__stack_smash_handler);
#endif
#undef	BUILTIN

#endif  /* _LIBC_NAMESPACE_H_ */

