/*	$OpenBSD: bios.c,v 1.11 1997/10/22 23:37:11 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
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

/* #define BIOS_DEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/reboot.h>

#include <vm/vm.h>
#include <sys/sysctl.h>

#include <dev/cons.h>
#include <stand/boot/bootarg.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/cpufunc.h>
#include <machine/conf.h>
#include <machine/gdt.h>
#include <machine/pcb.h>
#include <machine/biosvar.h>
#include <machine/apmvar.h>

#include <dev/isa/isareg.h>
#include <i386/isa/isa_machdep.h>

#include "apm.h"

struct bios_softc {
	struct	device sc_dev;
};

int biosprobe __P((struct device *, void *, void *));
void biosattach __P((struct device *, struct device *, void *));
int bios_print __P((void *, const char *));

struct cfattach bios_ca = {
	sizeof(struct bios_softc), biosprobe, biosattach
};

struct cfdriver bios_cd = {
	NULL, "bios", DV_DULL
};

extern bus_addr_t bootargv;
extern int bootargc;
extern u_int bootapiver; /* locore.s */
extern dev_t bootdev;

bios_diskinfo_t *bios_diskinfo;

bios_diskinfo_t *bios_getdiskinfo __P((dev_t));

int
biosprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct bios_attach_args *bia = aux;
	bus_space_handle_t hsp;
	int error;

#ifdef BIOS_DEBUG
	printf("%s%d: boot API ver %x, %x; args %p[%d]\n",
	       bia->bios_dev, bios_cd.cd_ndevs,
	       bootapiver, BOOT_APIVER, bootargv, bootargc);
#endif
	/* there could be only one */
	if (bios_cd.cd_ndevs || strcmp(bia->bios_dev, bios_cd.cd_name))
		return 0;

	if (bootapiver < BOOT_APIVER || bootargv == NULL)
		return 0;

	if ((error = bus_space_map(bia->bios_memt,
				   bootargv, bootargc, 1, &hsp)) != 0) {
#ifdef DEBUG
		printf("bios0: bus_space_map() == %d\n", error);
#endif
		return 0;
	}
	bus_space_unmap(bia->bios_memt, hsp, bootargc);

	return 1;
}

void
biosattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct bios_softc *sc = (void *) self;
	struct bios_attach_args *bia = aux;
#if NAPM > 0 || defined(DEBUG)
	bios_apminfo_t *apm;
#endif
	u_int8_t *va = ISA_HOLE_VADDR(0xffff0);
	char *str;
	bus_space_handle_t hsp;
	bootarg_t *p, *q;

	if (bus_space_map(bia->bios_memt, bootargv, bootargc, 1, &hsp) != 0) {
#ifdef DEBUG
		panic("getbootargs: can't map low memory");
#endif
		return;
	}

	switch (va[14]) {
	default:
	case 0xff: str = "PC";		break;
	case 0xfe: str = "PC/XT";	break;
	case 0xfd: str = "PCjr";	break;
	case 0xfc: str = "AT/286+";	break;
	case 0xfb: str = "PC/XT+";	break;
	case 0xfa: str = "PS/2 25/30";	break;
	case 0xf9: str = "PC Convertible";break;
	case 0xf8: str = "PS/2 386+";	break;
	}
	printf(": %s(%02x) BIOS, date %c%c/%c%c/%c%c\n",
	       str, va[15], va[5], va[6], va[8], va[9], va[11], va[12]);

	printf("%s:", sc->sc_dev.dv_xname);
	p = (bootarg_t *)hsp;
	for(q = p; q->ba_type != BOOTARG_END; q = q->ba_next) {
		q->ba_next = (bootarg_t *)((caddr_t)q + q->ba_size);
		switch (q->ba_type) {
		case BOOTARG_MEMMAP:
			printf(" memmap");
			break;
		case BOOTARG_DISKINFO:
			printf(" diskinfo");
			bios_diskinfo = (bios_diskinfo_t *)q->ba_arg;
			break;
		case BOOTARG_APMINFO:
			printf(" apminfo");
			apm = (bios_apminfo_t *)q->ba_arg;
			break;
		default:
		}
	}
	printf("\n");

#ifdef DEBUG
	printf("apminfo: %x, code %x/%x[%x], data %x[%x], entry %x\n",
	       apm->apm_detail, apm->apm_code32_base,
	       apm->apm_code16_base, apm->apm_code_len,
	       apm->apm_data_base, apm->apm_data_len, apm->apm_entry);
#endif
#if NAPM > 0
	{
		struct bios_attach_args ba;

		ba.bios_apmp = apm;
		ba.bios_dev = "apm";
		ba.bios_func = 0x15;
		config_found(self, &ba, bios_print);
	}
#endif
}

int
bios_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct bios_attach_args *ba = aux;

	if (pnp)
		printf("%s at %s function 0x%x",
		       ba->bios_dev, pnp, ba->bios_func);
	return (UNCONF);
}

int
biosopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct bios_softc *sc = bios_cd.cd_devs[0];

	if (minor(dev))
		return (ENXIO);

	(void)sc;

	return 0;
}

int
biosclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct bios_softc *sc = bios_cd.cd_devs[0];

	if (minor(dev))
		return (ENXIO);

	(void)sc;

	return 0;
}

int
biosioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct bios_softc *sc = bios_cd.cd_devs[0];

	if (minor(dev))
		return (ENXIO);

	switch (cmd) {
	default:
		return ENXIO;
	}

	(void)sc;

	return 0;
}

void
bioscnprobe(cn)
	struct consdev *cn;
{
#if 0
	bios_init(I386_BUS_SPACE_MEM); /* XXX */
	if (!bios_cd.cd_ndevs)
		return;

	if (0 && bios_call(BOOTC_CHECK, NULL))
		return;

	cn->cn_pri = CN_NORMAL;
	cn->cn_dev = makedev(48, 0);
#endif
}

void
bioscninit(cn)
	struct consdev *cn;
{

}

void
bioscnputc(dev, ch)
	dev_t dev;
	int ch;
{

}

int
bioscngetc(dev)
	dev_t dev;
{
	return -1;
}

void
bioscnpollc(dev, on)
	dev_t dev;
	int on;
{
}

int
bios_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	extern u_int cnvmem, extmem; /* locore.s */
	bios_diskinfo_t *pdi;
	int biosdev;

	/* all sysctl names at this level are terminal */
	if (namelen != 1 && name[0] != BIOS_DISKINFO)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case BIOS_DEV:
		if (bootapiver < BOOT_APIVER)
			return EOPNOTSUPP;
		if ((pdi = bios_getdiskinfo(bootdev)) == NULL)
			return ENXIO;
		biosdev = pdi->bios_number;
		return sysctl_rdint(oldp, oldlenp, newp, biosdev);
	case BIOS_DISKINFO:
		if (namelen != 2)
			return ENOTDIR;
		if (bootapiver < BOOT_APIVER)
			return EOPNOTSUPP;
		if ((pdi = bios_getdiskinfo(name[1])) == NULL)
			return ENXIO;
		return sysctl_rdstruct(oldp, oldlenp, newp,
					pdi, sizeof(*bios_diskinfo));
	case BIOS_CNVMEM:
		return sysctl_rdint(oldp, oldlenp, newp, cnvmem);
	case BIOS_EXTMEM:
		return sysctl_rdint(oldp, oldlenp, newp, extmem);
	default:
		return EOPNOTSUPP;
	}
	/* NOTREACHED */
}

bios_diskinfo_t *
bios_getdiskinfo(dev)
	dev_t dev;
{
	bios_diskinfo_t *pdi;

	for (pdi = bios_diskinfo; pdi->bios_number != -1; pdi++) {
		if ((dev & B_MAGICMASK) == B_DEVMAGIC) { /* search by bootdev */
			if (pdi->bsd_dev == dev)
				break;
		} else {
			if (pdi->bios_number == dev)
				break;
		}
	}

	if (pdi->bios_number == -1)
		return NULL;
	else
		return pdi;
}

