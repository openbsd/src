/*	$OpenBSD: machdep.c,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/reboot.h>
#include "libsa.h"
#include <machine/iomod.h>
#include <machine/pdc.h>

#include "dev_hppa64.h"

extern struct	stable_storage sstor;	/* contents of Stable Storage */
int howto;
dev_t bootdev;

void
machdep()
{
	pdc_init();
#ifdef notyet
	debug_init();
#endif
	cninit();

#ifdef PDCDEBUG
	if (debug) {
		int i;

		printf("SSTOR:\n");
		printf("pri_boot=");	DEVPATH_PRINT(&sstor.ss_pri_boot);
		printf("alt_boot=");	DEVPATH_PRINT(&sstor.ss_alt_boot);
		printf("console =");	DEVPATH_PRINT(&sstor.ss_console);
		printf("keyboard=");	DEVPATH_PRINT(&sstor.ss_keyboard);
		printf("mem=%d, fn=%s, osver=%d\nos={",
		       sstor.ss_fast_size, sstor.ss_filenames,
		       sstor.ss_os_version);
		for (i = 0; i < sizeof(sstor.ss_os); i++)
			printf ("%x%c", sstor.ss_os[i], (i%8)? ',' : '\n');

		printf("}\nPAGE0:\n");
		printf("ivec=%x, pf=%p[%u], toc=%p[%u], rndz=%p, clk/10ms=%u\n",
		       PAGE0->ivec_special, PAGE0->ivec_mempf,
		       PAGE0->ivec_mempflen, PAGE0->ivec_toc,
		       PAGE0->ivec_toclen, PAGE0->ivec_rendz,
		       PAGE0->mem_10msec);
		printf ("mem: cont=%u, phys=%u, pdc_spa=%u, resv=%u, free=%x\n"
			"cpu_hpa=%x, pdc=%p, imm_hpa=%p[%u,%u], soft=%u\n",
		       PAGE0->memc_cont, PAGE0->memc_phsize, PAGE0->memc_adsize,
		       PAGE0->memc_resv, PAGE0->mem_free, PAGE0->mem_hpa,
		       PAGE0->mem_pdc, PAGE0->imm_hpa, PAGE0->imm_spa_size,
		       PAGE0->imm_max_mem, PAGE0->imm_soft_boot);

		printf("console:  ");	PZDEV_PRINT(&PAGE0->mem_cons);
		printf("boot:     ");	PZDEV_PRINT(&PAGE0->mem_boot);
		printf("keyboard: ");	PZDEV_PRINT(&PAGE0->mem_kbd);
	}
#endif
}
