/*	$OpenBSD: autoconf.c,v 1.5 2007/06/15 01:19:06 deraadt Exp $	*/
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

#include <machine/asm_macro.h>
#include <machine/autoconf.h>
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

int cold = 1;   /* 1 if still booting */

struct device *bootdv;	/* set by device drivers (if found) */

u_int bootdevtype;

#define	BT_CIEN		0x6369656e
#define	BT_DGEN		0x6467656e
#define	BT_HKEN		0x686b656e
#define	BT_INEN		0x696e656e
#define	BT_INSC		0x696e7363
#define	BT_NCSC		0x6e637363

/*
 * called at boot time, configure all devices on the system.
 */
void
cpu_configure()
{
	printf("bootpath: '%s' dev %u unit %u part %u\n",
	    bootargs, bootdev, bootunit, bootpart);

	if (config_rootfound("mainbus", "mainbus") == 0)
		panic("no mainbus found");

	/*
	 * Turn external interrupts on.
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
	 * Skip boot device ``foo(ctrl,dev,lun)'' and filename,
	 * i.e. eat everything until whitespace.
	 */
	p = stws(bootargs);
	while (*p != '\0') {
		if (*p++ == '-')
			switch (*p) {
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
	 * Actually we rely upon the fact that all device strings are
	 * exactly 4 characters long, and appears at the beginning of the
	 * commandline, so we can simply use its numerical value, as a
	 * word, to tell device types apart.
	 */
	bootdevtype = *(int *)bootargs;
}

void
device_register(struct device *dev, void *aux)
{
	if (bootdv != NULL)
		return;

	switch (bootdevtype) {
	case BT_INEN:
		/*
		 * Internal ethernet is le at syscon only, and we do not
		 * care about controller and unit numbers.
		 */
		if (strncmp("le", dev->dv_xname, 2) == 0 &&
		    strncmp("syscon", dev->dv_parent->dv_xname, 6) == 0)
			bootdv = dev;
		break;
	}
}

struct nam2blk nam2blk[] = {
	{ "sd",		4 },
	{ "cd", 	6 },
	{ "rd",		7 },
	{ NULL,		-1 }
};
