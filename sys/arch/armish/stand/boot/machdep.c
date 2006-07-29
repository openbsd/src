/*	$OpenBSD: machdep.c,v 1.2 2006/07/29 16:08:20 kettenis Exp $	*/

/*
 * Copyright (c) 2006 Mark Kettenis
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

#include <sys/types.h>
#include <arm/pte.h>

#include "libsa.h"

#define L1_IDX(va)	(((uint32_t)(va)) >> L1_S_SHIFT)

#define ATU_OIOWTVR	0xffffe15c
#define ATU_ATUCR	0xffffe180
#define ATU_PCSR	0xffffe184

#define ATUCR_OUT_EN	(1U << 1)

#define	PCSR_RIB	(1U << 5)
#define	PCSR_RPB	(1U << 4)

void
machdep(void)
{
	uint32_t *pde;
	uint32_t va;

	/*
	 * Clean up the mess that RedBoot left us in, amd make sure we
	 * can access the PCI bus.
	 */

	*((volatile uint32_t *)(ATU_ATUCR)) = ATUCR_OUT_EN;

        __asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (pde));
	pde = (uint32_t *)((uint32_t)pde & 0x0fffffff);

	va = *((volatile uint32_t *)(ATU_OIOWTVR));
	pde[L1_IDX(va)] = (va & L1_ADDR_BITS) | L1_S_AP(AP_KRWUR) | L1_TYPE_S;

	/* Start timer */
	__asm volatile ("mcr p6, 0, %0, c3, c1, 0" :: "r" (0xffffffff));
	__asm volatile ("mcr p6, 0, %0, c1, c1, 0" :: "r" (0x00000032));

	cninit();
}

int
main(void)
{
	boot(0);
	return 0;
}

void
_rtt(void)
{
	*((volatile uint32_t *)(ATU_PCSR)) = PCSR_RIB | PCSR_RPB;

	printf("RESET FAILED\n");
	for (;;) ;
}
