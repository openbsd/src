/*	$OpenBSD: autoconf.c,v 1.11 2007/06/01 19:25:10 deraadt Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/kernel.h>

#include <machine/asm_macro.h>   /* enable/disable interrupts */
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/disklabel.h>
#include <machine/vmparam.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/cons.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */

void	dumpconf(void);
void	get_autoboot_device(void);

int cold = 1;   /* 1 if still booting */

void *bootaddr;
int bootpart;
struct device *bootdv;	/* set by device drivers (if found) */

/*
 * called at boot time, configure all devices on the system.
 */
void
cpu_configure()
{

	if (config_rootfound("mainbus", "mainbus") == 0)
		panic("no mainbus found");

	/*
	 * Turn external interrupts on.
	 *
	 * XXX We have a race here. If we enable interrupts after setroot(),
	 * the kernel dies.
	 */
	set_psr(get_psr() & ~PSR_IND);
	spl0();
	cold = 0;
}

void
diskconf(void)
{
	printf("boot device: %s\n",
	    (bootdv) ? bootdv->dv_xname : "<unknown>");
	setroot(bootdv, 0, RB_USERREQ);
	dumpconf();
}

/*
 * Get 'auto-boot' information from NVRAM
 *
 * XXX Right now we can not handle network boot.
 */
struct autoboot_t
{
	char	cont[16];
	int	targ;
	int	part;
} autoboot;

void
get_autoboot_device(void)
{
	char *value, c;
	int i, len, part;
	extern char *nvram_by_symbol(char *);		/* machdep.c */

	/* Assume default controller is internal spc (spc0) */
	strlcpy(autoboot.cont, "spc0", sizeof(autoboot.cont));

	/* Get boot controler and SCSI target from NVRAM */
	value = nvram_by_symbol("boot_unit");
	if (value != NULL) {
		len = strlen(value);
		if (len == 1) {
			c = value[0];
		} else if (len == 2) {
			if (value[0] == '1') {
				/* External spc (spc1) */
				strlcpy(autoboot.cont, "spc1", sizeof(autoboot.cont));
				c = value[1];
			}
		}

		if ((c >= '0') && (c <= '6'))
			autoboot.targ = 6 - (c - '0');
	}

	/* Get partition number from NVRAM */
	value = nvram_by_symbol("boot_partition");
	if (value != NULL) {
		len = strlen(value);
		part = 0;
		for (i = 0; i < len; i++)
			part = part * 10 + (value[i] - '0');
		autoboot.part = part;
	}
}

void
device_register(struct device *dev, void *aux)
{
        /*
         * scsi: sd,cd  XXX: Can LUNA88K boot from CD-ROM?
         */
        if (strncmp("sd", dev->dv_xname, 2) == 0 ||
            strncmp("cd", dev->dv_xname, 2) == 0) {
		struct scsi_attach_args *sa = aux;
		struct device *spcsc;

		spcsc = dev->dv_parent->dv_parent;

                if (strncmp(autoboot.cont, spcsc->dv_xname, 4) == 0 &&
		    sa->sa_sc_link->target == autoboot.targ &&
		    sa->sa_sc_link->lun == 0) {
                        bootdv = dev;
			bootpart = autoboot.part;
                        return;
                }
        }
}

struct nam2blk nam2blk[] = {
	{ "sd",		4 },
	{ "st",		5 },
	{ "rd",		7 },
	{ NULL,		-1 }
};
