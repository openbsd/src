/*	$OpenBSD: autoconf.c,v 1.15 2013/10/09 21:28:33 miod Exp $	*/
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

#include <machine/asm_macro.h>
#include <machine/autoconf.h>
#include <machine/board.h>
#include <machine/cpu.h>
#include <machine/vmparam.h>

#ifdef AV530
#include <machine/av530.h>
#endif

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/cons.h>

#include "sd.h"
#include "st.h"

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */

void	dumpconf(void);

int cold = 1;   /* 1 if still booting */

struct device *bootdv;	/* set by device drivers (if found) */

uint32_t bootdevtype;

/* cied */
#define	BT_CIEN		0x6369656e
/* cimd */
/* cird */
/* cisc */
#define	BT_DGEN		0x6467656e
#define	BT_DGSC		0x64677363
/* hada */
#define	BT_HKEN		0x686b656e
#define	BT_INEN		0x696e656e
#define	BT_INSC		0x696e7363
#define	BT_NCSC		0x6e637363
/* nvrd */
/* pefn */
/* vitr */

/*
 * called at boot time, configure all devices on the system.
 */
void
cpu_configure()
{
	printf("bootpath: '%s' dev %u unit %u part %u\n",
	    bootargs, bootdev, bootunit, bootpart);

	softintr_init();

	if (config_rootfound("mainbus", "mainbus") == 0)
		panic("no mainbus found");

	/* NO PROM CALLS FROM NOW ON */

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

/*
 * Parse the commandline.
 *
 * This has two goals: first, turn the necessary options into boothowto
 * flags; second, convert the PROM boot device into the matching OpenBSD
 * driver name.
 */

/* skip end of token and whitespace */
static char *stws(char *);
static char *
stws(char *p)
{
	while (*p != ' ' && *p != '\0')
		p++;

	while (*p == ' ')
		p++;

	return (p);
}

void
cmdline_parse(void)
{
	char *p;
	

	/*
	 * If the boot commandline has been manually entered, it
	 * may end with a '\r' character.
	 */
	for (p = bootargs; *p != '\0'; p++)
		;
	if (p != bootargs)
		if (*--p == '\r')
			*p = '\0';

	/*
	 * Skip boot device ``foo(ctrl,dev,lun)'' and filename,
	 * i.e. eat everything until whitespace.
	 */
	p = stws(bootargs);
	while (*p != '\0') {
		if (*p++ == '-')
			while (*p != ' ' && *p != '\0')
				switch (*p++) {
				case 'a':
					boothowto |= RB_ASKNAME;
					break;
				case 'b':
					boothowto |= RB_KDB;
					break;
				case 'c':
					boothowto |= RB_CONFIG;
					break;
				case 's':
					boothowto |= RB_SINGLE;
					break;
				}
		p = stws(p);
	}

	/*
	 * Now parse the boot device. We are only interested in the
	 * device type, since the PROM has cracked the controller, unit
	 * and partition numbers for us already, and we do not care about
	 * our own filename...
	 *
	 * However, in the sd() or st() cases, we need to figure out the
	 * SCSI controller name (if not the default one) and address, if
	 * provided.
	 */
	if (memcmp(bootargs, "sd", 2) == 0 ||
	    memcmp(bootargs, "st", 2) == 0) {
		bcopy(platform->default_boot, &bootdevtype, sizeof(uint32_t));
		/* search for a controller specification */
	} else
		bcopy(bootargs, &bootdevtype, sizeof(int));
}

void
device_register(struct device *dev, void *aux)
{
	struct confargs *ca = (struct confargs *)aux;
	struct cfdriver *cf = dev->dv_cfdata->cf_driver;

	if (bootdv != NULL)
		return;
/* SCSI -> match bootunit/bootpart as id:lun iff controller matches */
	switch (bootdevtype) {

	/*
	 * Network devices
	 */

	case BT_INEN:
		/*
		 * Internal LANCE Ethernet is le at syscon only, and we do not
		 * care about controller and unit numbers.
		 */
		if (strcmp("le", cf->cd_name) == 0 && strcmp("syscon",
		      dev->dv_parent->dv_cfdata->cf_driver->cd_name) == 0)
			bootdv = dev;
		break;
	case BT_DGEN:
		/*
		 * Internal ILACC Ethernet is le at syscon only, and need to
		 * match the controller address.
		 */
		if (strcmp("le", cf->cd_name) == 0 && strcmp("syscon",
		      dev->dv_parent->dv_cfdata->cf_driver->cd_name) == 0) {
			switch (cpuid) {
#ifdef AV530
			case AVIION_4600_530:
				if ((bootdev == 0 &&
				    ca->ca_paddr == AV530_LAN1) ||
				    (bootdev == 1 &&
				    ca->ca_paddr == AV530_LAN2))
					bootdv = dev;
				break;
#endif
			default:
				break;
			}
		}
		break;

	/*
	 * SCSI controllers
	 */

	case BT_NCSC:
		/*
		 * Internal 53C700 controller is oosiop at syscon only, and
		 * needs to match the controller address, as well as SCSI
		 * unit and lun numbers.
		 */
	    {
		struct scsi_attach_args *sa = aux;
		struct device *grandp;

		if (memcmp(cf->cd_name, bootargs, 2) != 0 ||
		    (strcmp("sd", cf->cd_name) != 0 &&
		     strcmp("st", cf->cd_name) != 0) ||
		    sa->sa_sc_link->target != bootunit ||
		    sa->sa_sc_link->lun != bootpart)
			break;

		grandp = dev->dv_parent->dv_parent;
		if (strcmp("oosiop",
		    grandp->dv_cfdata->cf_driver->cd_name) == 0) {
			bootdv = dev;	/* XXX second controller */
		}
	    }
		break;

	case BT_INSC:
		/*
		 * Internal AIC-6250 controller is oaic at syscon only, and
		 * needs to match the controller address, as well as SCSI
		 * unit and lun numbers.
		 */
		/* XXX TBD */
		break;
	}
}

struct nam2blk nam2blk[] = {
	{ "sd",		4 },
	{ "cd", 	6 },
	{ "rd",		7 },
	{ "vnd",	8 },
	{ NULL,		-1 }
};
