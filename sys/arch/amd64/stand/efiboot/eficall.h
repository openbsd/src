/*	$OpenBSD: eficall.h,v 1.1 2015/09/02 01:52:25 yasuoka Exp $	*/

/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
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

#if !defined(__amd64__)

#define	EFI_CALL(_func_, ...) (_func_)(__VA_ARGS__)

#else

extern uint64_t efi_call(int, void *, ...);

#define	_call_0(_func) \
    efi_call(0, (_func))
#define	_call_1(_func, _1) \
    efi_call(1, (_func), (_1))
#define	_call_2(_func, _1, _2) \
    efi_call(2, (_func), (_1), (_2))
#define	_call_3(_func, _1, _2, _3) \
    efi_call(3, (_func), (_1), (_2), (_3))
#define	_call_4(_func, _1, _2, _3, _4) \
    efi_call(4, (_func), (_1), (_2), (_3), (_4))
#define	_call_5(_func, _1, _2, _3, _4, _5) \
    efi_call(5, (_func), (_1), (_2), (_3), (_4), (_5))
#define	_call_6(_func, _1, _2, _3, _4, _5, _6) \
    efi_call(6, (_func), (_1), (_2), (_3), (_4), (_5), (_6))
#define	_call_7(_func, _1, _2, _3, _4, _5, _6, _7) \
    efi_call(7, (_func), (_1), (_2), (_3), (_4), (_5), (_6), (_7))
#define	_call_8(_func, _1, _2, _3, _4, _5, _6, _7, _8) \
    efi_call(8, (_func), (_1), (_2), (_3), (_4), (_5), (_6), (_7), (_8))
#define	_call_9(_func, _1, _2, _3, _4, _5, _6, _7, _8, _9) \
    efi_call(9, (_func), (_1), (_2), (_3), (_4), (_5), (_6), (_7), (_8), (_9))

#define _efi_call_fn(_func, _1, _2, _3, _4, _5, _6, _7, _8, _9, _fn, ...) _fn

#define	EFI_CALL(...)	\
    _efi_call_fn(__VA_ARGS__, _call_9, _call_8, _call_7, _call_6, _call_5, \
	    _call_4, _call_3, _call_2, _call_1, _call_1)(__VA_ARGS__)
#endif
