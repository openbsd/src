/*	$OpenBSD: mmio.h,v 1.3 2026/09/16 18:31:37 mlarkin Exp $	*/

/*
 * Copyright (c) 2024 Mike Larkin <mlarkin@openbsd.org>
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

#ifndef _MMIO_H_
#define _MMIO_H_

#include <sys/types.h>
#include <sys/queue.h>

#define MMIO_DIR_READ 0
#define MMIO_DIR_WRITE 1

typedef int (*mmio_dev_fn_t)(uint32_t vcpu_id, int dir, paddr_t addr,
    uint8_t size, uint64_t *data);

struct mmio_dev {
	paddr_t start;
	paddr_t end;

	mmio_dev_fn_t fn;

	SLIST_ENTRY(mmio_dev) dev_next;
};


void mmio_init(void);
mmio_dev_fn_t mmio_find_dev(paddr_t);
int mmio_dev_add(paddr_t, paddr_t, mmio_dev_fn_t);

#endif /* _MMIO_H_ */
