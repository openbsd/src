/*	$OpenBSD: boot.c,v 1.1 2006/07/28 17:12:06 kettenis Exp $	*/

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

#include <lib/libsa/stand.h>
#include <lib/libsa/loadfile.h>

int
main(void)
{
	u_long marks[MARK_MAX];

	cons_init();

	printf("OpenBSD/armish boot\n");

	printf("OIOWTVR: 0x%x\n", *((volatile uint32_t *)0xffffe15c));
	printf("ATUCR: 0x%x\n", *((volatile uint32_t *)0xffffe180));
	printf("ATU_OIOWTVR: 0x%x\n", *((volatile uint32_t *)0xffffe15c));
	printf("ATU_OMWTVR0: 0x%x\n", *((volatile uint32_t *)0xffffe160));
	printf("ATU_OUMWTVR0: 0x%x\n", *((volatile uint32_t *)0xffffe164));
	printf("ATU_OMWTVR1: 0x%x\n", *((volatile uint32_t *)0xffffe168));
	printf("ATU_OUMWTVR1: 0x%x\n", *((volatile uint32_t *)0xffffe16c));
	volatile uint32_t *p = ((volatile uint32_t *)0xffffe180);
	*p = 1<<1;
	printf("ATUCR: 0x%x\n", *((volatile uint32_t *)0xffffe180));

#define L1_S_SHIFT      20
	{
	uint32_t *pde;

        __asm volatile("mrc     p15, 0, %0, c2, c0, 0" : "=r" (pde));

	printf("pde %x\n", pde);
	pde = (uint32_t *)((uint32_t) pde & 0x0fffffff);
	printf("mapping of %x is %x\n", p, pde[(u_int32_t)p >> L1_S_SHIFT]);
	p = (u_int32_t *)0x90000000;
	printf("mapping of %x is %x\n", p, pde[(u_int32_t)p >> L1_S_SHIFT]);
	p = (u_int32_t *)0xa0000000;
	printf("mapping of %x is %x\n", p, pde[(u_int32_t)p >> L1_S_SHIFT]);
	p = (u_int32_t *)0x00000000;
	printf("mapping of %x is %x\n", p, pde[(u_int32_t)p >> L1_S_SHIFT]);

	p = (u_int32_t *)0x90000000;
	pde[(u_int32_t)p >> L1_S_SHIFT] = ((uint32_t)p & 0xfff00000) | 0xc02;
	printf("new mapping of %x is %x\n", p, pde[(u_int32_t)p >> L1_S_SHIFT]);

	}

	marks[MARK_START] = 0;
	if (loadfile("wd2a:/bsd", marks, LOAD_ALL) < 0) {
		printf("loadfile: errno %\n", errno);
		goto err;
	}

	run_loadfile(marks, 0);

 err:
	printf("halted...");
	for (;;) ;
}
