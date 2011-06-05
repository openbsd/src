/*	$OpenBSD: boot.c,v 1.20 2011/06/05 21:49:36 miod Exp $	*/
/*	$NetBSD: boot.c,v 1.10 1997/01/18 01:58:33 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/loadfile.h>

#include <sys/param.h>
#include <sys/exec.h>

#include <machine/rpb.h>
#include <machine/prom.h>
#include <machine/autoconf.h>

char boot_file[128];
char boot_flags[128];

extern char bootprog_name[];

struct bootinfo_v1 bootinfo_v1;

paddr_t ptbr_save;

int debug;

int
main()
{
	char *name, **namep;
	u_int64_t entry;
	int rc;
	u_long marks[MARK_MAX];
#ifdef DEBUG
	struct rpb *r;
	struct mddt *mddtp;
	struct mddt_cluster *memc;
	int i;
#endif

	/* Init prom callback vector. */
	init_prom_calls();

	/* print a banner */
	printf("%s\n", bootprog_name);

	/* switch to OSF pal code. */
	OSFpal();

#ifdef DEBUG
	r = (struct rpb *)HWRPB_ADDR;
	mddtp = (struct mddt *)(HWRPB_ADDR + r->rpb_memdat_off);
	printf("%d memory clusters\n", mddtp->mddt_cluster_cnt);
	for (i = 0; i < mddtp->mddt_cluster_cnt; i++) {
		memc = &mddtp->mddt_clusters[i];
		printf("%d: (%d) %lx-%lx\n", i, memc->mddt_usage,
		    memc->mddt_pfn << PAGE_SHIFT,
		    (memc->mddt_pfn + memc->mddt_pg_cnt) << PAGE_SHIFT);
	}
#endif
	prom_getenv(PROM_E_BOOTED_FILE, boot_file, sizeof(boot_file));
	prom_getenv(PROM_E_BOOTED_OSFLAGS, boot_flags, sizeof(boot_flags));

	if (boot_file[0] != '\0') {
		(void)printf("Boot file: %s %s\n", boot_file, boot_flags);
		name = boot_file;
	} else
		name = "bsd";

	(void)printf("Loading %s...\n", name);
	marks[MARK_START] = 0;
	rc = loadfile(name, marks, LOAD_KERNEL | COUNT_KERNEL);
	(void)printf("\n");
	if (rc != 0)
		goto fail;

	/*
	 * Fill in the bootinfo for the kernel.
	 */
	bzero(&bootinfo_v1, sizeof(bootinfo_v1));
	bootinfo_v1.ssym = marks[MARK_SYM];
	bootinfo_v1.esym = marks[MARK_END];
	bcopy(name, bootinfo_v1.booted_kernel,
	    sizeof(bootinfo_v1.booted_kernel));
	bcopy(boot_flags, bootinfo_v1.boot_flags,
	    sizeof(bootinfo_v1.boot_flags));
	bootinfo_v1.hwrpb = (void *)HWRPB_ADDR;
	bootinfo_v1.hwrpbsize = ((struct rpb *)HWRPB_ADDR)->rpb_size;
	bootinfo_v1.cngetc = NULL;
	bootinfo_v1.cnputc = NULL;
	bootinfo_v1.cnpollc = NULL;

	entry = marks[MARK_START];
	(*(void (*)(u_int64_t, u_int64_t, u_int64_t, void *, u_int64_t,
	    u_int64_t))entry)(0, ptbr_save, BOOTINFO_MAGIC, &bootinfo_v1, 1, 0);

fail:
	halt();
}
