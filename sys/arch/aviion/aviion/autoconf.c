/*	$OpenBSD: autoconf.c,v 1.17 2015/04/25 21:15:08 miod Exp $	*/
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
#include <machine/prom.h>

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

static struct device *bootctrl;		/* boot controller */
static struct device *bootdv;		/* boot device (if found) */
u_int bootdev = 0;			/* set in locore.S, can't be in .bss */
u_int bootunit = 0;			/* set in locore.S, can't be in .bss */
u_int bootlun = 0;			/* set in locore.S, can't be in .bss */
u_int bootpart = 0;			/* set in locore.S, can't be in .bss */
static uint32_t bootdevtype;		/* boot controller SCM name */
static paddr_t bootctrlpaddr;		/* boot controller address */

/*
 * called at boot time, configure all devices on the system.
 */
void
cpu_configure()
{
	printf("bootpath: '%s' dev %u unit %u lun %u\n",
	    bootargs, bootdev, bootunit, bootlun);

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

/* parse a positive base 10 number */
static u_int strtoi(const char *);
static u_int
strtoi(const char *s)
{
	int c;
	u_int val = 0;

	if (s == NULL || *s == '\0')
		return 0;

	/* skip whitespace */
	do {
		c = *s++;
	} while (c == ' ' || c == '\t');

	for (;;) {
		if (c < '0' || c > '9')
			break;
		val *= 10;
		val += c - '0';
		c = *s++;
	}

	return val;
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
	 *
	 * Note that we will override bootdev at this point. If no boot
	 * controller number or address was provided, bootdev will be set
	 * to zero anyway.
	 */
	if (memcmp(bootargs, "sd", 2) == 0 ||
	    memcmp(bootargs, "st", 2) == 0) {
		/*
		 * Either
		 *   sd(bootdev,bootunit,bootlun)
		 * or
		 *   sd(ctrl(bootdev,id),bootunit,bootlun)
		 * We already know bootdev, bootunit and bootlun.
		 * All we need here is to figure out the controller type
		 * and address.
		 */
		if (bootargs[7] == '(') {
			bcopy(bootargs + 3, &bootdevtype, sizeof(uint32_t));
			bootdev = strtoi(bootargs + 8);
		}
	} else {
		bcopy(bootargs, &bootdevtype, sizeof(int));
		bootdev = strtoi(bootargs + 5);
	}

	/* fill the holes */
	bootctrlpaddr = platform->get_boot_device(&bootdevtype, bootdev);
}

void
device_register(struct device *dev, void *aux)
{
	struct confargs *ca = (struct confargs *)aux;
	struct cfdriver *cf = dev->dv_cfdata->cf_driver;
	struct device *parent = dev->dv_parent;

	if (bootdv != NULL)
		return;

	if (bootctrl == NULL) {
		if (ca->ca_paddr != bootctrlpaddr)
			return;

		switch (bootdevtype) {
		case SCM_INEN:
		case SCM_DGEN:
			if (strcmp("le", cf->cd_name) == 0 &&
			    strcmp("syscon",
			      parent->dv_cfdata->cf_driver->cd_name) == 0)
				bootctrl = dev;
			break;

		case SCM_INSC:
			if (strcmp("oaic", cf->cd_name) == 0 &&
			    strcmp("syscon",
			      parent->dv_cfdata->cf_driver->cd_name) == 0)
				bootctrl = dev;
			break;

		case SCM_NCSC:
			if (strcmp("oosiop", cf->cd_name) == 0 &&
			    strcmp("syscon",
			      parent->dv_cfdata->cf_driver->cd_name) == 0)
				bootctrl = dev;
			break;
		}

		if (bootctrl != NULL && bootctrl->dv_class == DV_IFNET)
			bootdv = bootctrl;
		return;
	}

	if (memcmp(cf->cd_name, bootargs, 2) == 0 &&
	    (strcmp("sd", cf->cd_name) == 0 ||
	     strcmp("st", cf->cd_name) == 0)) {
		struct scsi_attach_args *saa = aux;
		if (saa->sa_sc_link->target == bootunit &&
		    saa->sa_sc_link->lun == bootlun &&
		    parent->dv_parent == bootctrl)
			bootdv = dev;
	}
}

struct nam2blk nam2blk[] = {
	{ "sd",		4 },
	{ "cd", 	6 },
	{ "rd",		7 },
	{ "vnd",	8 },
	{ NULL,		-1 }
};
