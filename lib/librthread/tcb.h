/*
 * Copyright (c) 2011 Philip Guenther <guenther@openbsd.org>
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

#ifndef _TCB_H_
#define _TCB_H_

/*
 * We define the structure for the TCB, plus three macros:
 *	TCB_THREAD()
 *		Expands to a pointer-to-pthread rvalue that points
 *		to this thread's struct pthread
 *	TCB_ERRNOPTR()
 *		Expands to a pointer-to-int lvalue that points to
 *		this thread's errno
 *	TCB_INIT(tcb, thread, errnoptr)
 *		Expands to a code to initialize the TCB pointed to by
 *		its first argument
 *
 * <machine/tcb.h> must define the TLS_VARIANT macro.  If it defines
 * that to 1, then may also defined the macro
 *	THREAD_ERRNOPTR_OFFSET
 *		Byte offset in struct pthread of the pointer to the
 *		thread's errno
 *
 * By default, we get and set the TCB pointer for a thread using the
 * __get_tcb() and __set_tcb() syscalls.  If there's a faster way to do
 * either of those, <machine/tcb.h> should define the macros
 *	TCB_SET(tcb)
 *		Set the TCB pointer for this thread
 *	TCB_GET()
 *		Return the TCB pointer for this thread
 * If it defines TCB_GET, then it must also define:
 *	TCB_GET_MEMBER(member)
 *		Return the pointer in the 'member' slot in the TCB
 */

#include <machine/tcb.h>

#include <stddef.h>

struct pthread;

void	*__get_tcb(void);
void	__set_tcb(void *);

#ifdef TCB_GET_MEMBER
#define TCB_THREAD()	((struct pthread *)TCB_GET_MEMBER(tcb_thread))
#else
#define TCB_THREAD()	\
		(((struct thread_control_block *)__get_tcb())->tcb_thread)
#endif


#if TLS_VARIANT == 1
/*
 * Small TCB, with TLS data after the TCB.
 * Errno pointer stored in struct pthread
 */

struct thread_control_block {
	void	*tcb_dtv;		/* internal to the runtime linker */
	struct	pthread *tcb_thread;
};

#ifndef THREAD_ERRNOPTR_OFFSET
# define THREAD_ERRNOPTR_OFFSET	offsetof(struct pthread, errno_ptr)
#endif
#define __ERRNOPTR(thread)	\
	(((int **)(thread))[THREAD_ERRNOPTR_OFFSET / sizeof(int *)])
#define TCB_ERRNOPTR()	\
	__ERRNOPTR(TCB_THREAD())
#define	TCB_INIT(tcb, thread, errnoptr)			\
	do {						\
		(tcb)->tcb_dtv = 0;			\
		(tcb)->tcb_thread = (thread);		\
		__ERRNOPTR(thread) = (errnoptr);	\
	} while (0)


#elif TLS_VARIANT == 2
/*
 * Large TCB, with TLS data before the TCB (i.e., negative offsets)
 * Errno pointer stored in the TCB
 */

struct thread_control_block {
	struct	thread_control_block *__tcb_self;
	void	*tcb_dtv;		/* internal to the runtime linker */
	struct	pthread *tcb_thread;
	int	*__tcb_errno;
};

#ifdef TCB_GET_MEMBER
#define TCB_ERRNOPTR()	((int *)TCB_GET_MEMBER(__tcb_errno))
#else
#define TCB_ERRNOPTR()	\
		(((struct thread_control_block *)__get_tcb())->__tcb_errno)
#endif
#define	TCB_INIT(tcb, thread, errnoptr)			\
	do {						\
		(tcb)->__tcb_self = (tcb);		\
		(tcb)->tcb_dtv = 0;			\
		(tcb)->tcb_thread = (thread);		\
		(tcb)->__tcb_errno = (errnoptr);	\
	} while (0)


#else
# error "unknown TLS variant"
#endif


/* If there isn't a better way, use the default */
#ifndef	TCB_SET
#define	TCB_SET(tcb)	__set_tcb(tcb)
#endif

#if 0
void *_rtld_allocate_tls(void *, size_t, size_t);
void _rtld_free_tls(void *, size_t, size_t);
#else
/*
 * XXX Until we have these in ld.so and support __thread, just use
 * malloc/free.  The main thread's TCB cannot be allocated or freed with these.
 */
#define	_rtld_allocate_tls(old, size, align)	malloc(size)
#define	_rtld_free_tls(old, size, align)	free(old)
#endif

#endif /* _TCB_H_ */
