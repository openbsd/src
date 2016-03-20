/* $OpenBSD: kbind.h,v 1.1 2016/03/20 02:32:39 guenther Exp $ */

/*
 * Copyright (c) 2016 Philip Guenther <guenther@openbsd.org>
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

#include <sys/syscall.h>
#include <machine/pal.h>

#define	MD_DISABLE_KBIND						\
	do {								\
		register long syscall_num __asm("$0") /* v0 */ = SYS_kbind;\
		register void *arg1 __asm("$16") /* a0 */ = NULL;	\
		__asm volatile("call_pal %1" : "+r" (syscall_num)	\
		    : "i" (PAL_OSF1_callsys), "r" (arg1) : "$19", "$20");\
	} while (0)
