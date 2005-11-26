/*	$OpenBSD: cpu_subr.c,v 1.2 2005/11/26 22:40:31 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis
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

#include <sys/param.h>

#include <machine/cpu.h>

void
ppc_mtscomc(u_int32_t val)
{
	int s;

	s = ppc_intr_disable();
	__asm __volatile ("mtspr 276,%0; isync" :: "r" (val));
	ppc_intr_enable(s);
}

void
ppc_mtscomd(u_int32_t val)
{
	int s;

	s = ppc_intr_disable();
	__asm __volatile ("mtspr 277,%0; isync" :: "r" (val));
	ppc_intr_enable(s);
}

u_int64_t
ppc64_mfscomc(void)
{
	u_int64_t ret;
	int s;

	s = ppc_intr_disable();
	__asm __volatile ("mfspr %0,276;"
	    " mr %0+1, %0; srdi %0,%0,32" : "=r" (ret));
	ppc_intr_enable(s);
	return ret;
}

void
ppc64_mtscomc(u_int64_t val)
{
	int s;

	s = ppc_intr_disable();
	__asm __volatile ("sldi %0,%0,32; or %0,%0,%0+1;"
	    " mtspr 276,%0; isync" :: "r" (val));
	ppc_intr_enable(s);
}

u_int64_t
ppc64_mfscomd(void)
{
	u_int64_t ret;
	int s;

	s = ppc_intr_disable();
	__asm __volatile ("mfspr %0,277;"
            " mr %0+1, %0; srdi %0,%0,32" : "=r" (ret));
	ppc_intr_enable(s);
	return ret;
}
