/*	$OpenBSD: autoconf.c,v 1.48 2011/10/09 17:01:34 miod Exp $	*/
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
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/kernel.h>

#include <uvm/uvm.h>

#include <machine/asm_macro.h>   /* enable/disable interrupts */
#include <machine/autoconf.h>
#include <machine/bugio.h>
#include <machine/cpu.h>
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
int	get_target(int *, int *, int *);

int cold = 1;   /* 1 if still booting */

paddr_t bootaddr;
int bootpart, bootbus;
struct device *bootdv;	/* set by device drivers (if found) */

/*
 * called at boot time, configure all devices on the system.
 */
void
cpu_configure()
{
	extern void cpu_hatch_secondary_processors(void *);
	softintr_init();

	if (config_rootfound("mainbus", "mainbus") == 0)
		panic("no mainbus found");


	/*
	 * Finally switch to the real console driver,
	 * and say goodbye to the BUG!
	 */
	cn_tab = NULL;
	cninit();

#ifdef MULTIPROCESSOR
	/*
	 * Spin up the other processors, but do not give them work to
	 * do yet.
	 */
	cpu_hatch_secondary_processors(NULL);
#endif

	/* NO BUG CALLS FROM NOW ON */

	/*
	 * Switch to our final trap vectors, and unmap whatever is below
	 * the kernel.
	 */
	set_vbr(kernel_vbr);
	pmap_kremove(0, (vsize_t)kernel_vbr);
	pmap_update(pmap_kernel());

	cold = 0;

	/*
	 * Turn external interrupts on.
	 */
	set_psr(get_psr() & ~PSR_IND);
	spl0();
}

void
diskconf(void)
{
	printf("boot device: %s\n",
	    (bootdv) ? bootdv->dv_xname : "<unknown>");

	setroot(bootdv, bootpart, RB_USERREQ);
	dumpconf();
}

void
device_register(struct device *dev, void *aux)
{
	if (bootpart == -1) /* ignore flag from controller driver? */
		return;

	/*
	 * scsi: sd,cd
	 */
	if (strcmp("cd", dev->dv_cfdata->cf_driver->cd_name) == 0 ||
	    strcmp("sd", dev->dv_cfdata->cf_driver->cd_name) == 0) {
		struct scsi_attach_args *sa = aux;
		int target, bus, lun;

		if (get_target(&target, &bus, &lun) != 0)
			return;
    
		/* make sure we are on the expected scsibus */
		if (bootbus != bus)
			return;

		if (sa->sa_sc_link->target == target &&
		    sa->sa_sc_link->lun == lun) {
			bootdv = dev;
			return;
		}
	}

	/*
	 * ethernet: ie,le
	 */
	else if (strcmp("ie", dev->dv_cfdata->cf_driver->cd_name) == 0 ||
	    strcmp("le", dev->dv_cfdata->cf_driver->cd_name) == 0) {
		struct confargs *ca = aux;

		if (ca->ca_paddr == bootaddr) {
			bootdv = dev;
			return;
		}
	}
}

/*
 * Returns the ID of the SCSI disk based on Motorola's CLUN/DLUN stuff
 * bootdev == CLUN << 8 | DLUN.
 * This handles SBC SCSI and MVME32[78].
 */
int
get_target(int *target, int *bus, int *lun)
{
	extern int bootdev;

	switch (bootdev >> 8) {
	/* built-in controller */
	case 0x00:
	/* MVME327 */
	case 0x02:
	case 0x03:
		*bus = 0;
		*target = (bootdev & 0x70) >> 4;
		*lun = (bootdev & 0x07);
		return (0);
	/* MVME328 */
	case 0x06:
	case 0x07:
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
		*bus = (bootdev & 0x40) >> 6;
		*target = (bootdev & 0x38) >> 3;
		*lun = (bootdev & 0x07);
		return (0);
	default:
		return (ENODEV);
	}
}

struct nam2blk nam2blk[] = {
	{ "sd",		4 },
	{ "cd",		6 },
	{ "rd",		7 },
	{ "vnd",	8 },
	{ NULL,		-1 }
};
