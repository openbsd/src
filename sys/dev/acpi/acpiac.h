/* $OpenBSD: acpiac.h,v 1.1 2005/12/14 04:15:43 marco Exp $ */
/*
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
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

#ifndef __DEV_ACPI_ACPIAC_H__
#define __DEV_ACPI_ACPIAC_H__

/*
 * _PSR (Power Source)
 * Arguments: none
 * Results  : DWORD status
 */
#define PSR_ONLINE		0x00
#define PSR_OFFLINE		0x01

/*
 * _PCL (Power Consumer List)
 * Arguments: none
 * Results  : LIST of Power Class pointers
 */

#endif /* __DEV_ACPI_ACPIAC_H__ */
