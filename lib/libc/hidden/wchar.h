/*	$OpenBSD: wchar.h,v 1.2 2015/09/10 18:13:46 guenther Exp $	*/
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

#ifndef _LIBC_WCHAR_H_
#define _LIBC_WCHAR_H_

#include_next <wchar.h>

/*PROTO_NORMAL(btowc);*/
PROTO_NORMAL(fgetwc);
PROTO_NORMAL(fgetws);
PROTO_NORMAL(fputwc);
PROTO_NORMAL(fputws);
PROTO_NORMAL(fwide);
PROTO_NORMAL(fwprintf);
PROTO_NORMAL(fwscanf);
PROTO_NORMAL(getwc);
PROTO_NORMAL(getwchar);
/*PROTO_NORMAL(mbrlen);*/
/*PROTO_NORMAL(mbrtowc);*/
/*PROTO_NORMAL(mbsinit);*/
/*PROTO_NORMAL(mbsnrtowcs);*/
/*PROTO_NORMAL(mbsrtowcs);*/
PROTO_NORMAL(open_wmemstream);
PROTO_NORMAL(putwc);
PROTO_NORMAL(putwchar);
PROTO_NORMAL(swprintf);
PROTO_NORMAL(swscanf);
PROTO_NORMAL(ungetwc);
PROTO_NORMAL(vfwprintf);
PROTO_NORMAL(vfwscanf);
PROTO_NORMAL(vswprintf);
PROTO_NORMAL(vswscanf);
PROTO_NORMAL(vwprintf);
PROTO_NORMAL(vwscanf);
/*PROTO_NORMAL(wcrtomb);*/
/*PROTO_NORMAL(wcscasecmp);*/
/*PROTO_NORMAL(wcscat);*/
/*PROTO_NORMAL(wcschr);*/
/*PROTO_NORMAL(wcscmp);*/
/*PROTO_NORMAL(wcscoll);*/
/*PROTO_NORMAL(wcscpy);*/
/*PROTO_NORMAL(wcscspn);*/
/*PROTO_NORMAL(wcsdup);*/
/*PROTO_NORMAL(wcsftime);*/
/*PROTO_NORMAL(wcslcat);*/
/*PROTO_NORMAL(wcslcpy);*/
/*PROTO_NORMAL(wcslen);*/
/*PROTO_NORMAL(wcsncasecmp);*/
/*PROTO_NORMAL(wcsncat);*/
/*PROTO_NORMAL(wcsncmp);*/
/*PROTO_NORMAL(wcsncpy);*/
/*PROTO_NORMAL(wcsnrtombs);*/
/*PROTO_NORMAL(wcspbrk);*/
/*PROTO_NORMAL(wcsrchr);*/
/*PROTO_NORMAL(wcsrtombs);*/
/*PROTO_NORMAL(wcsspn);*/
/*PROTO_NORMAL(wcsstr);*/
/*PROTO_NORMAL(wcstod);*/
/*PROTO_NORMAL(wcstof);*/
/*PROTO_NORMAL(wcstok);*/
/*PROTO_NORMAL(wcstol);*/
/*PROTO_NORMAL(wcstold);*/
/*PROTO_NORMAL(wcstoll);*/
/*PROTO_NORMAL(wcstoul);*/
/*PROTO_NORMAL(wcstoull);*/
/*PROTO_NORMAL(wcswcs);*/
/*PROTO_NORMAL(wcswidth);*/
/*PROTO_NORMAL(wcsxfrm);*/
/*PROTO_NORMAL(wctob);*/
/*PROTO_NORMAL(wcwidth);*/
/*PROTO_NORMAL(wmemchr);*/
/*PROTO_NORMAL(wmemcmp);*/
/*PROTO_NORMAL(wmemcpy);*/
/*PROTO_NORMAL(wmemmove);*/
/*PROTO_NORMAL(wmemset);*/
PROTO_NORMAL(wprintf);
PROTO_NORMAL(wscanf);

#endif /* !_LIBC_WCHAR_H_ */
