/*	$OpenBSD: bios.c,v 1.9 1997/10/18 00:33:11 weingart Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <sys/sysctl.h>

#include <dev/cons.h>

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

#define LMVAS (1024*1024-NBPG)
#define LMVOF NBPG

int gdt_get_slot __P((void));

struct bios_softc {
	struct	device sc_dev;

	bus_space_handle_t bt;
};

int biosprobe __P((struct device *, void *, void *));
void biosattach __P((struct device *, struct device *, void *));
void bios_init __P((bus_space_handle_t));
int bios_print __P((void *, const char *));
static __inline int bios_call __P((u_int cmd, u_int arg));

struct cfattach bios_ca = {
	sizeof(struct bios_softc), biosprobe, biosattach
};

struct cfdriver bios_cd = {
	NULL, "bios", DV_DULL
};

int bios_initted = 0;
int bios_ds, bios_cs16;
bus_space_handle_t bios_lmva;
struct {
	u_int32_t ip;
	u_int16_t cs;
} bios_kentry;
struct BIOS_vars BIOS_vars;
bios_diskinfo_t bios_diskinfo[16];		/* XXX - For now */

static __inline int
bios_call(cmd, arg)
	u_int cmd;
	u_int arg;
{
	int rv;
	__asm volatile ("pushl %1\n\t"
			"pushl %2\n\t"
			"pushl %%ds\n\t"
			"movl  %4, %%ds\n\t"
			"movl  %4, %%es\n\t"
			"movl  %4, %%gs\n\t"
			"movl  %4, %%fs\n\t"
			"lcall %%cs:(%3)\n\t"
			"popl  %%ds\n\t"
			"addl $8, %%esp"
			: "=a" (rv)
			: "id" (cmd), "r" (arg),
			  "r" (&bios_kentry), "r" (bios_ds));
	return rv;
}

void
bios_init(bt)
	bus_space_handle_t bt;
{
	if (bios_initted)
		return;

	if (bus_space_map(bt, LMVOF, LMVAS, 0, &bios_lmva) == 0) {
		extern union descriptor *dynamic_gdt;

		setsegment(&dynamic_gdt[bios_kentry.cs = gdt_get_slot()].sd,
			   (void*)bios_lmva, LMVAS, SDT_MEMERA, SEL_KPL, 1, 0);
		setsegment(&dynamic_gdt[bios_ds = gdt_get_slot()].sd,
			   (void*)bios_lmva, LMVAS, SDT_MEMRWA, SEL_KPL, 1, 0);
		setsegment(&dynamic_gdt[bios_cs16 = gdt_get_slot()].sd,
			   (void*)bios_lmva, LMVAS, SDT_MEMERA, SEL_KPL, 0, 0);

		bios_initted++;
	}
}

int
biosprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct bios_attach_args *bia = aux;
	extern u_int bootapiver; /* locore.s */

	if (bootapiver == 0)
		return 0;

#if 0
	if (!bios_initted) {
		bus_space_handle_t hsp;

		if (bus_space_map(bia->bios_memt, LMVOF, LMVAS, 0, &hsp) != 0)
			return 0;
		bus_space_unmap(bia->bios_memt, hsp, LMVAS);
	}
#endif
	return !bios_cd.cd_ndevs && !strcmp(bia->bios_dev, "bios");
}

void
biosattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct bios_softc *sc = (void *) self;
	struct bios_attach_args *bia = aux;

	u_int8_t *va = ISA_HOLE_VADDR(0xffff0);
	char *p;

	sc->bt = bia->bios_memt;
	/* bios_init(sc->bt); */
	switch (va[14]) {
	default:
	case 0xff: p = "PC";		break;
	case 0xfe: p = "PC/XT";		break;
	case 0xfd: p = "PCjr";		break;
	case 0xfc: p = "AT/286+";	break;
	case 0xfb: p = "PC/XT+";	break;
	case 0xfa: p = "PS/2 25/30";	break;
	case 0xf9: p = "PC Convertible";break;
	case 0xf8: p = "PS/2 386+";	break;
	}
	printf(": %s(%02x) BIOS, date %c%c/%c%c/%c%c\n",
	    p, va[15], va[5], va[6], va[8], va[9], va[11], va[12]);
#ifdef DEBUG
	printf("apminfo: %x, code %x/%x[%x], data %x[%x], entry %x\n",
	    BIOS_vars.bios_apm_detail, BIOS_vars.bios_apm_code32_base,
	    BIOS_vars.bios_apm_code16_base, BIOS_vars.bios_apm_code_len,
	    BIOS_vars.bios_apm_data_base, BIOS_vars.bios_apm_data_len,
	    BIOS_vars.bios_apm_entry);
#endif
#if NAPM > 0
	{
		struct bios_attach_args ba;

		ba.apm_detail = BIOS_vars.bios_apm_detail;
		ba.apm_code32_base = BIOS_vars.bios_apm_code32_base;
		ba.apm_code16_base = BIOS_vars.bios_apm_code16_base;
		ba.apm_code_len = BIOS_vars.bios_apm_code_len;
		ba.apm_data_base = BIOS_vars.bios_apm_data_base;
		ba.apm_data_len = BIOS_vars.bios_apm_data_len;
		ba.apm_entry = BIOS_vars.bios_apm_entry;
		ba.bios_dev = "apm";
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
		printf("%s at %s", ba->bios_dev, pnp);
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
	if (!bios_initted)
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
	extern u_int cnvmem, extmem, bootapiver; /* locore.s */

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	if (bootapiver == 0)
		return EOPNOTSUPP;

	switch (name[0]) {
	case BIOS_DEV:
		return sysctl_rdint(oldp, oldlenp, newp, BIOS_vars.bios_dev);
	case BIOS_GEOMETRY:
		return sysctl_rdint(oldp, oldlenp, newp, BIOS_vars.bios_geometry);
	case BIOS_CNVMEM:
		return sysctl_rdint(oldp, oldlenp, newp, cnvmem);
	case BIOS_EXTMEM:
		return sysctl_rdint(oldp, oldlenp, newp, extmem);
	default:
		return EOPNOTSUPP;
	}
	/* NOTREACHED */
}
