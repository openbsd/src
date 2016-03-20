/*	$OpenBSD: tib.h,v 1.2 2016/03/20 02:30:28 guenther Exp $	*/
/*
 * Copyright (c) 2011,2014 Philip Guenther <guenther@openbsd.org>
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
 * Thread Information Block (TIB) and Thread Local Storage (TLS) handling
 * (the TCB, Thread Control Block, is part of the TIB)
 */

#ifndef	_TIB_H_
#define	_TIB_H_

#include <sys/types.h>
#include <machine/tcb.h>

#include <stddef.h>


/*
 * This header defines struct tib and at least eight macros:
 *	TLS_VARIANT
 *		Either 1 or 2  (Actually defined by <machine/tcb.h>)
 *
 *	TCB_SET(tcb)
 *		Set the TCB pointer for this thread to 'tcb'
 *
 *	TCB_GET()
 *		Return the TCB pointer for this thread
 *
 *	TCB_TO_TIB(tcb)
 *		Given a TCB pointer, return the matching TIB pointer
 *
 *	TIB_TO_TCB(tib)
 *		Given a TIB pointer, return the matching TCB pointer
 *
 *	TIB_INIT(tib, dtv)
 *		Initializes a TIB for a new thread, using the supplied
 *		value for the dtv pointer
 *
 *	TIB_TO_THREAD(tib)
 *		Given a TIB pointer, return a pointer to the struct pthread
 *
 *	TIB_GET()
 *		Short-hand for TCB_TO_TIB(TCB_GET())
 *
 *	TIB_THREAD()
 *		Returns a pointer to this thread's struct pthread
 *
 *	TIB_EXTRA_ALIGN
 *		On TLS varaint 2 archs, what alignment is sufficient
 *		for the extra space that will be used for struct pthread?
 *
 * The following functions are provided by either ld.so (dynamic) or
 * libc (static) for allocating and freeing a common memory block that
 * will hold both the TIB and the pthread structure:
 *	_dl_allocate_tib(sizeof(struct pthread), flags)
 *		Allocates a combined TIB and pthread memory region.
 *		The first argument is the amount of space to reserve
 *		for the pthread structure; the second argument is
 *		either zero or DAT_UPDATE_CURRENT, the latter meaning
 *		this call is to update/replace the current thread's
 *		TIB.  Returns a pointer to the TIB inside the
 *		allocated block.
 *
 * 	_dl_free_tib(tib, sizeof(struct pthread))
 *		Frees a TIB and pthread block previously allocated
 *		with _dl_allocate_tls().  Must be passed the return
 *		value of that previous call.
 */

/*
 * Regarding <machine/tcb.h>:
 *  - it must define the TLS_VARIANT macro
 *  - if there's a faster way to get or set the TCB pointer for the thread
 *    than the __{get,set}_tcb() syscalls, it should define either or both
 *    the TCB_{GET,SET} macros to do so.
 */


/* If <machine/tcb.h> doesn't provide a better way, then use the default */
#ifdef	TCB_GET
# define TCB_HAVE_MD_GET	1
#else
# define TCB_GET()		__get_tcb()
#endif
#ifdef	TCB_SET
# define TCB_HAVE_MD_SET	1
#else
# define TCB_SET(tcb)		__set_tcb(tcb)
#endif


#if TLS_VARIANT == 1
/*
 * ABI specifies that the static TLS data starts two words after the
 * (notional) thread pointer, with the first of those two words being
 * the TLS dtv pointer.  The other (second) word is reserved for the
 * implementation, so we place the thread's locale there, but we place
 * our thread bits before the TCB, at negative offsets from the
 * TCB pointer.  Ergo, memory is laid out, low to high, as:
 *
 *	[pthread structure]
 *	TIB {
 *		int cancel_flags
 *		int cancel_requested
 *		int errno
 *		void *locale
 *		TCB {
 *			void *dtv
 *			struct pthread *thread
 *		}
 *	}
 *	static TLS data
 */

struct tib {
#ifdef __LP64__
	int	__tib_padding;		/* padding for 8byte alignment */
#endif
	int	tib_cancel_flags;
	int	tib_cancel;
	int	tib_errno;
	void	*tib_locale;
	void	*tib_dtv;		/* internal to the runtime linker */
	void	*tib_thread;
};
#ifdef __LP64__
# define _TIB_PREP(tib)	((void)((tib)->__tib_padding = 0))
#endif

#define	_TIBO_PTHREAD		(- _ALIGN(sizeof(struct pthread)))

#elif TLS_VARIANT == 2
/*
 * ABI specifies that the static TLS data occupies the memory before
 * the TCB pointer, at negative offsets, and that on i386 and amd64
 * the word the TCB points to contains a pointer to itself.  So,
 * we place errno and our thread bits after that.  Memory is laid
 * out, low to high, as:
 *	static TLS data
 *	TIB {
 *		TCB {
 *			self pointer [i386/amd64 only]
 *			void *dtv
 *		}
 *		struct pthread *thread
 *		void *locale
 *		int errno
 *		int cancel_count_flags
 *		int cancel_requested
 *	}
 *	[pthread structure]
 */

struct tib {
#if defined(__i386) || defined(__amd64)
	struct	tib *__tib_self;
# define __tib_tcb __tib_self
#endif
	void	*tib_dtv;		/* internal to the runtime linker */
	void	*tib_thread;
	void	*tib_locale;
	int	tib_errno;
	int	tib_cancel;		/* set to request cancelation */
	int	tib_cancel_flags;
#if defined(__LP64__) || defined(__i386)
	int	__tib_padding;		/* padding for 8byte alignment */
#endif
};

#define	_TIBO_PTHREAD		_ALIGN(sizeof(struct tib))

#if defined(__i386) || defined(__amd64)
# define _TIB_PREP(tib)	\
	((void)((tib)->__tib_self = (tib), (tib)->__tib_padding = 0))
#elif defined(__LP64__)
# define _TIB_PREP(tib)	((void)((tib)->__tib_padding = 0))
#endif

#define	TIB_EXTRA_ALIGN		sizeof(void *)

#else
# error "unknown TLS variant"
#endif

/* nothing to do by default */
#ifndef	_TIB_PREP
# define _TIB_PREP(tib)	((void)0)
#endif

#define	TIB_INIT(tib, dtv, thread)	do {		\
		(tib)->tib_thread	= (thread);	\
		(tib)->tib_locale	= NULL;		\
		(tib)->tib_cancel_flags	= 0;		\
		(tib)->tib_cancel	= 0;		\
		(tib)->tib_dtv		= (dtv);	\
		(tib)->tib_errno	= 0;		\
		_TIB_PREP(tib);				\
	} while (0)

#ifndef	__tib_tcb
# define __tib_tcb		tib_dtv
#endif
#define	_TIBO_TCB		offsetof(struct tib, __tib_tcb)

#define	TCB_TO_TIB(tcb)		((struct tib *)((char *)(tcb) - _TIBO_TCB))
#define	TIB_TO_TCB(tib)		((char *)(tib) + _TIBO_TCB)
#define	TIB_TO_THREAD(tib)	((struct pthread *)(tib)->tib_thread)
#define	TIB_GET()		TCB_TO_TIB(TCB_GET())
#define	TCB_THREAD()		TIB_TO_THREAD(TIB_GET())


__BEGIN_DECLS
void	*_dl_allocate_tib(size_t _extra) __dso_public;
void	_dl_free_tib(void *_tib, size_t _extra) __dso_public;

/* The actual syscalls */
void	*__get_tcb(void);
void	__set_tcb(void *_tcb);
__END_DECLS

#endif /* _TIB_H_ */
