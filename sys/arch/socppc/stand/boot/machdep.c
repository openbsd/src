/*	$OpenBSD: machdep.c,v 1.1 2008/05/10 20:06:26 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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

#include "libsa.h"

#define RPR	0xe0000918
#define  RPR_RSTE	0x52535445
#define RCR	0xe000091c
#define  RCR_SWSR	0x00000001
#define  RCR_SWHR	0x00000002

void
machdep(void)
{
	cninit();

{
	extern u_int32_t wdc_base_addr;
	wdc_base_addr = 0xe2000000;
}

}

int
main(void)
{
	extern char __bss_start[], _end[];
	bzero(__bss_start, _end-__bss_start);

	boot(0);
	return 0;
}

void
_rtt(void)
{
	uint32_t v;

	*((volatile uint32_t *)(RPR)) = RPR_RSTE;
	__asm __volatile("eieio");
	*((volatile uint32_t *)(RCR)) = RCR_SWHR;

	printf("RESET FAILED\n");
	for (;;) ;
}
