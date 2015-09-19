/*	$OpenBSD: signal.h,v 1.7 2015/09/19 04:02:21 guenther Exp $	*/
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

#ifndef _LIBC_SIGNAL_H
#define _LIBC_SIGNAL_H

#include_next <signal.h>

__BEGIN_HIDDEN_DECLS
extern sigset_t __sigintr;
__END_HIDDEN_DECLS

#if 0
extern PROTO_NORMAL(sys_siglist);
extern PROTO_NORMAL(sys_signame);
#endif

PROTO_DEPRECATED(bsd_signal);
PROTO_NORMAL(kill);             /* wrap to ban SIGTHR? */
PROTO_DEPRECATED(killpg);
PROTO_DEPRECATED(psignal);
/*PROTO_NORMAL(pthread_sigmask);*/
PROTO_NORMAL(raise);
/*PROTO_WRAP(sigaction);	wrap to hide SIGTHR */
PROTO_NORMAL(sigaddset);
PROTO_NORMAL(sigaltstack);
PROTO_NORMAL(sigblock);
PROTO_NORMAL(sigdelset);
PROTO_NORMAL(sigemptyset);
PROTO_NORMAL(sigfillset);
PROTO_DEPRECATED(siginterrupt);
PROTO_NORMAL(sigismember);
PROTO_NORMAL(signal);
PROTO_DEPRECATED(sigpause);
/*PROTO_NORMAL(sigpending);	wrap to hide SIGTHR */
/*PROTO_WRAP(sigprocmask);	wrap to hide SIGTHR */
PROTO_NORMAL(sigreturn);
PROTO_NORMAL(sigsetmask);
/*PROTO_CANCEL(sigsuspend);	wrap to hide SIGTHR */
PROTO_DEPRECATED(sigvec);

#endif	/* !_LIBC_SIGNAL_H */
