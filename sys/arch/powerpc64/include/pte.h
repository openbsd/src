/*	$OpenBSD: pte.h,v 1.1 2020/05/23 14:49:32 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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

#ifndef _MACHINE_PTE_H

/*
 * Page Table Entry bits that should work for all 64-bit POWER CPUs as
 * well as the PowerPC 970.
 */

struct pte {
	uint64_t pte_hi;
	uint64_t pte_lo;
};

/* High doubleword: */
#define PTE_VALID		0x0000000000000001ULL
#define PTE_HID			0x0000000000000002ULL
#define PTE_WIRED		0x0000000000000004ULL /* SW */
#define PTE_AVPN		0x3fffffffffffff80ULL

/* Low doubleword: */
#define PTE_PP			0x0000000000000003ULL
#define PTE_RO			0x0000000000000003ULL
#define PTE_RW			0x0000000000000002ULL
#define PTE_N			0x0000000000000004ULL
#define PTE_G			0x0000000000000008ULL
#define PTE_M			0x0000000000000010ULL
#define PTE_I			0x0000000000000020ULL
#define PTE_W			0x0000000000000040ULL
#define PTE_CHG			0x0000000000000080ULL
#define PTE_REF			0x0000000000000100ULL
#define PTE_RPGN		0x0ffffffffffff000ULL

#endif
