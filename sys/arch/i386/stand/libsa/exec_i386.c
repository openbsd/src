/*	$OpenBSD: exec_i386.c,v 1.47 2018/12/10 16:52:02 jsing Exp $	*/

/*
 * Copyright (c) 1997-1998 Michael Shalayeff
 * Copyright (c) 1997 Tobias Weingartner
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <dev/cons.h>
#include <lib/libsa/loadfile.h>
#include <machine/biosvar.h>
#include <machine/specialreg.h>
#include <stand/boot/bootarg.h>

#include "cmd.h"
#include "disk.h"
#include "libsa.h"

#ifdef SOFTRAID
#include <dev/softraidvar.h>
#include <lib/libsa/softraid.h>
#include "softraid_i386.h"
#endif

#ifdef EFIBOOT
#include "efiboot.h"
#endif

typedef void (*startfuncp)(int, int, int, int, int, int, int, int)
    __attribute__ ((noreturn));

void ucode_load(void);
extern struct cmd_state cmd;

char *bootmac = NULL;

void
run_loadfile(u_long *marks, int howto)
{
	u_long entry;
#ifdef EXEC_DEBUG
	extern int debug;
#endif
	dev_t bootdev = bootdev_dip->bootdev;
	size_t ac = BOOTARG_LEN;
	caddr_t av = (caddr_t)BOOTARG_OFF;
	bios_consdev_t cd;
	extern int com_speed; /* from bioscons.c */
	extern int com_addr;
	bios_ddb_t ddb;
	extern int db_console;
	bios_bootduid_t bootduid;
#ifdef SOFTRAID
	bios_bootsr_t bootsr;
	struct sr_boot_volume *bv;
#endif

#ifdef EFIBOOT
	if ((av = alloc(ac)) == NULL)
		panic("alloc for bootarg");
	efi_makebootargs();
#endif
	if (sa_cleanup != NULL)
		(*sa_cleanup)();

	cd.consdev = cn_tab->cn_dev;
	cd.conspeed = com_speed;
	cd.consaddr = com_addr;
	cd.consfreq = 0;
	addbootarg(BOOTARG_CONSDEV, sizeof(cd), &cd);

	if (bootmac != NULL)
		addbootarg(BOOTARG_BOOTMAC, sizeof(bios_bootmac_t), bootmac);

	if (db_console != -1) {
		ddb.db_console = db_console;
		addbootarg(BOOTARG_DDB, sizeof(ddb), &ddb);
	}

	bcopy(bootdev_dip->disklabel.d_uid, &bootduid.duid, sizeof(bootduid));
	addbootarg(BOOTARG_BOOTDUID, sizeof(bootduid), &bootduid);

	ucode_load();

#ifdef SOFTRAID
	if (bootdev_dip->sr_vol != NULL) {
		bv = bootdev_dip->sr_vol;
		bzero(&bootsr, sizeof(bootsr));
		bcopy(&bv->sbv_uuid, &bootsr.uuid, sizeof(bootsr.uuid));
		if (bv->sbv_maskkey != NULL)
			bcopy(bv->sbv_maskkey, &bootsr.maskkey,
			    sizeof(bootsr.maskkey));
		addbootarg(BOOTARG_BOOTSR, sizeof(bios_bootsr_t), &bootsr);
		explicit_bzero(&bootsr, sizeof(bootsr));
	}

	sr_clear_keys();
#endif

	/* Pass memory map to the kernel */
	mem_pass();

#ifdef __amd64__
	/*
	 * This code may be used both for 64bit and 32bit.  Make sure the
	 * bootarg is 32bit always on even on amd64.
	 */
	makebootargs32(av, &ac);
#else
	makebootargs(av, &ac);
#endif

	entry = marks[MARK_ENTRY] & 0x0fffffff;

	printf("entry point at 0x%x\n", (int)entry);

#if defined(EFIBOOT)
	efi_cleanup();
#endif
#if defined(EFIBOOT) && defined(__amd64__)
	(*run_i386)((u_long)run_i386, entry, howto, bootdev, BOOTARG_APIVER,
	    marks[MARK_END], extmem, cnvmem, ac, (intptr_t)av);
#else
	/* stack and the gung is ok at this point, so, no need for asm setup */
	(*(startfuncp)entry)(howto, bootdev, BOOTARG_APIVER, marks[MARK_END],
	    extmem, cnvmem, ac, (int)av);
	/* not reached */
#endif
}

void
ucode_load(void)
{
	uint32_t model, family, stepping;
	uint32_t dummy, signature;
	uint32_t vendor[4];
	bios_ucode_t uc;
	struct stat sb;
	char path[128];
	size_t buflen;
	char *buf;
	int fd;

	CPUID(0, dummy, vendor[0], vendor[2], vendor[1]);
	vendor[3] = 0; /* NULL-terminate */
	if (strcmp((char *)vendor, "GenuineIntel") != 0)
		return;

	CPUID(1, signature, dummy, dummy, dummy);
	family = (signature >> 8) & 0x0f;
	model = (signature >> 4) & 0x0f;
	if (family == 0x6 || family == 0xf) {
		family += (signature >> 20) & 0xff;
		model += ((signature >> 16) & 0x0f) << 4;
	}
	stepping = (signature >> 0) & 0x0f;

	snprintf(path, sizeof(path), "%s:/etc/firmware/intel/%02x-%02x-%02x",
	    cmd.bootdev, family, model, stepping);

	fd = open(path, 0);
	if (fd == -1)
		return;

	if (fstat(fd, &sb) == -1)
		return;

	buflen = sb.st_size;
	if (buflen > 128*1024) {
		printf("ucode too large\n");
		return;
	}

	buf = (char *)(1*1024*1024);

	if (read(fd, buf, buflen) != buflen) {
		close(fd);
		return;
	}

	uc.uc_addr = (uint64_t)buf;
	uc.uc_size = (uint64_t)buflen;
	addbootarg(BOOTARG_UCODE, sizeof(uc), &uc);

	close(fd);
}
