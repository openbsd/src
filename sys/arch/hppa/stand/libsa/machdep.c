/*	$OpenBSD: machdep.c,v 1.2 1998/09/29 07:23:48 mickey Exp $	*/
/*	$NOWHERE: machdep.c,v 2.0 1998/06/17 20:49:17 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include "libsa.h"
#include <machine/iomod.h>
#include <machine/pdc.h>
#include <machine/lifvar.h>

#include "dev_hppa.h"

extern struct	stable_storage sstor;	/* contents of Stable Storage */
int howto, bootdev;

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
		printf("SSTOR:\n");
		printf("pri_boot=");	DEVPATH_PRINT(&sstor.ss_pri_boot);
		printf("alt_boot=");	DEVPATH_PRINT(&sstor.ss_alt_boot);
		printf("console =");	DEVPATH_PRINT(&sstor.ss_console);
		printf("keyboard=");	DEVPATH_PRINT(&sstor.ss_keyboard);
		printf("mem=%d, fn=%s, osver=%d\n"
		       "os={%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,\n"
		           "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x}\n",
		       sstor.ss_fast_size, sstor.ss_filenames,
		       sstor.ss_os_version,
		       sstor.ss_os[0], sstor.ss_os[1], sstor.ss_os[2],
		       sstor.ss_os[3], sstor.ss_os[4], sstor.ss_os[5],
		       sstor.ss_os[6], sstor.ss_os[7], sstor.ss_os[8],
		       sstor.ss_os[9], sstor.ss_os[10], sstor.ss_os[11],
		       sstor.ss_os[12], sstor.ss_os[13], sstor.ss_os[14],
		       sstor.ss_os[15], sstor.ss_os[16], sstor.ss_os[17],
		       sstor.ss_os[18], sstor.ss_os[19], sstor.ss_os[20],
		       sstor.ss_os[21]);

		printf("PAGE0:\n");
		printf("ivec=%x, pf=%p[%u], toc=%p[%u], rendz=%p\n"
		       "mem: cont=%u, phys=%u, pdc_spa=%u, resv=%u, free=%x\n"
		       "cpu_hpa=%p, pdc=%p, imm_hpa=%p[%u,%u]\n"
		       "soft=%u, tic/10ms=%u\n",
		       PAGE0->ivec_special, PAGE0->ivec_mempf,
		       PAGE0->ivec_mempflen, PAGE0->ivec_toc,
		       PAGE0->ivec_toclen, PAGE0->ivec_rendz,
		       PAGE0->memc_cont, PAGE0->memc_phsize, PAGE0->memc_adsize,
		       PAGE0->memc_resv, PAGE0->mem_free, PAGE0->mem_hpa,
		       PAGE0->mem_pdc, PAGE0->imm_hpa, PAGE0->imm_spa_size,
		       PAGE0->imm_max_mem, PAGE0->imm_soft_boot,
		       PAGE0->mem_10msec);
		printf("console:  ");	PZDEV_PRINT(&PAGE0->mem_cons);
		printf("boot:     ");	PZDEV_PRINT(&PAGE0->mem_boot);
		printf("keyboard: ");	PZDEV_PRINT(&PAGE0->mem_kbd);
	}
#endif
}
