/*	$OpenBSD: bios.c,v 1.57 2005/11/13 14:23:26 martin Exp $	*/

/*
 * Copyright (c) 1997-2001 Michael Shalayeff
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
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
#include <sys/extent.h>

#include <uvm/uvm_extern.h>
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
#include "pcibios.h"
#include "pci.h"

struct bios_softc {
	struct	device sc_dev;
};

int biosprobe(struct device *, void *, void *);
void biosattach(struct device *, struct device *, void *);
int bios_print(void *, const char *);

struct cfattach bios_ca = {
	sizeof(struct bios_softc), biosprobe, biosattach
};

struct cfdriver bios_cd = {
	NULL, "bios", DV_DULL
};

extern dev_t bootdev;

#if NAPM > 0 || defined(DEBUG)
bios_apminfo_t *apm;
#endif
#if NPCI > 0
bios_pciinfo_t *bios_pciinfo;
#endif
bios_diskinfo_t *bios_diskinfo;
bios_memmap_t  *bios_memmap;
u_int32_t	bios_cksumlen;
struct bios32_entry bios32_entry;
#ifdef MULTIPROCESSOR
void	       *bios_smpinfo;
#endif

bios_diskinfo_t *bios_getdiskinfo(dev_t);

int
biosprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct bios_attach_args *bia = aux;

#ifdef BIOS_DEBUG
	printf("%s%d: boot API ver %x, %x; args %p[%d]\n",
	    bia->bios_dev, bios_cd.cd_ndevs,
	    bootapiver, BOOTARG_APIVER, bootargp, bootargc);
#endif
	/* there could be only one */
	if (bios_cd.cd_ndevs || strcmp(bia->bios_dev, bios_cd.cd_name))
		return 0;

	if (!(bootapiver & BAPIV_VECTOR) || bootargp == NULL)
		return 0;

	return 1;
}

void
biosattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct bios_softc *sc = (struct bios_softc *) self;
#if (NPCI > 0 && NPCIBIOS > 0) || NAPM > 0
	struct bios_attach_args *bia = aux;
#endif
	volatile u_int8_t *va;
	char *str;
	int flags;

	/* remember flags */
	flags = sc->sc_dev.dv_cfdata->cf_flags;

	va = ISA_HOLE_VADDR(0xffff0);
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
	printf(": %s(%02x) BIOS, date %c%c/%c%c/%c%c",
	    str, va[15], va[5], va[6], va[8], va[9], va[11], va[12]);

	/* see if we have BIOS32 extensions */
	if (!(flags & BIOSF_BIOS32)) {
		for (va = ISA_HOLE_VADDR(BIOS32_START);
		     va < (u_int8_t *)ISA_HOLE_VADDR(BIOS32_END); va += 16) {
			bios32_header_t h = (bios32_header_t)va;
			u_int8_t cksum;
			int i;

			if (h->signature != BIOS32_SIGNATURE)
				continue;

			/* verify checksum */
			for (cksum = 0, i = h->length * 16; i--; cksum += va[i])
				;
			if (cksum != 0)
				continue;

			if (h->entry <= BIOS32_START || h->entry >= BIOS32_END)
				continue;

			bios32_entry.segment = GSEL(GCODE_SEL, SEL_KPL);
			bios32_entry.offset = (u_int32_t)ISA_HOLE_VADDR(h->entry);
			printf(", BIOS32 rev. %d @ 0x%lx", h->rev, h->entry);
			break;
		}
	}

	printf("\n");

#if NAPM > 0
	if (apm) {
		struct bios_attach_args ba;
#if defined(DEBUG) || defined(APMDEBUG)
		printf("apminfo: %x, code %x[%x]/%x[%x], data %x[%x], ept %x\n",
		    apm->apm_detail,
		    apm->apm_code32_base, apm->apm_code_len,
		    apm->apm_code16_base, apm->apm_code16_len,
		    apm->apm_data_base, apm->apm_data_len, apm->apm_entry);
#endif
		ba.bios_dev = "apm";
		ba.bios_func = 0x15;
		ba.bios_memt = bia->bios_memt;
		ba.bios_iot = bia->bios_iot;
		ba.bios_apmp = apm;
		config_found(self, &ba, bios_print);
	}
#endif
#if NPCI > 0 && NPCIBIOS > 0
	if (!(flags & BIOSF_PCIBIOS)) {
		struct bios_attach_args ba;

		ba.bios_dev = "pcibios";
		ba.bios_func = 0x1A;
		ba.bios_memt = bia->bios_memt;
		ba.bios_iot = bia->bios_iot;
		config_found(self, &ba, bios_print);
	}
#endif

	/*
	 * now, that we've gave 'em a chance to attach,
	 * scan and map all the proms we can find
	 */
	if (!(flags & BIOSF_PROMSCAN)) {
		volatile u_int8_t *eva;

		for (str = NULL, va = ISA_HOLE_VADDR(0xc0000),
		     eva = ISA_HOLE_VADDR(0xf0000);
		     va < eva; va += 512) {
			extern struct extent *iomem_ex;
			bios_romheader_t romh = (bios_romheader_t)va;
			u_int32_t off, len;
			u_int8_t cksum;
			int i;

			if (romh->signature != 0xaa55)
				continue;

			/*
			 * for this and the next check we probably want
			 * to reserve the page in the extent anyway
			 */
			if (!romh->len || romh->len == 0xff)
				continue;

			len = romh->len * 512;
			if (va + len > eva)
				continue;

			for (cksum = 0, i = len; i--; cksum += va[i])
				;
#ifdef __stinkpad_sucks__
			if (cksum != 0)
				continue;
#endif

			off = 0xc0000 + (va - (u_int8_t *)
			    ISA_HOLE_VADDR(0xc0000));

			if (!str)
				printf("%s: ROM list:",
				    str = sc->sc_dev.dv_xname);
			printf(" 0x%05x/0x%x%s", off, len,
			    cksum? "!" : "");

			if ((i = extent_alloc_region(iomem_ex,
			    (paddr_t)off, len, EX_NOWAIT)))
				printf(":%d", i);

			va += len - 512;
		}
	}

	if (str)
		printf("\n");
}

void
bios_getopt()
{
	bootarg_t *q;

#ifdef BIOS_DEBUG
	printf("bootargv:");
#endif

	for(q = bootargp; q->ba_type != BOOTARG_END; q = q->ba_next) {
		q->ba_next = (bootarg_t *)((caddr_t)q + q->ba_size);
		switch (q->ba_type) {
		case BOOTARG_MEMMAP:
			bios_memmap = (bios_memmap_t *)q->ba_arg;
#ifdef BIOS_DEBUG
			printf(" memmap %p", bios_memmap);
#endif
			break;
		case BOOTARG_DISKINFO:
			bios_diskinfo = (bios_diskinfo_t *)q->ba_arg;
#ifdef BIOS_DEBUG
			printf(" diskinfo %p", bios_diskinfo);
#endif
			break;
#if NAPM > 0 || defined(DEBUG)
		case BOOTARG_APMINFO:
#ifdef BIOS_DEBUG
			printf(" apminfo %p", q->ba_arg);
#endif
			apm = (bios_apminfo_t *)q->ba_arg;
			break;
#endif
		case BOOTARG_CKSUMLEN:
			bios_cksumlen = *(u_int32_t *)q->ba_arg;
#ifdef BIOS_DEBUG
			printf(" cksumlen %d", bios_cksumlen);
#endif
			break;
#if NPCI > 0
		case BOOTARG_PCIINFO:
			bios_pciinfo = (bios_pciinfo_t *)q->ba_arg;
#ifdef BIOS_DEBUG
			printf(" pciinfo %p", bios_pciinfo);
#endif
			break;
#endif
		case BOOTARG_CONSDEV:
			if (q->ba_size >= sizeof(bios_consdev_t))
			{
				bios_consdev_t *cdp = (bios_consdev_t*)q->ba_arg;
#include "com.h"
#include "pccom.h"
#if NCOM + NPCCOM > 0
				extern int comdefaultrate; /* ic/com.c */
				comdefaultrate = cdp->conspeed;
#endif
#ifdef BIOS_DEBUG
				printf(" console 0x%x:%d",
				    cdp->consdev, cdp->conspeed);
#endif
				cnset(cdp->consdev);
			}
			break;
#ifdef MULTIPROCESSOR
		case BOOTARG_SMPINFO:
			bios_smpinfo = q->ba_arg;
			printf(" smpinfo %p", bios_smpinfo);
			break;
#endif

		default:
#ifdef BIOS_DEBUG
			printf(" unsupported arg (%d) %p", q->ba_type,
			    q->ba_arg);
#endif
			break;
		}
	}
	printf("\n");

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
bios32_service(service, e, ei)
	u_int32_t service;
	bios32_entry_t e;
	bios32_entry_info_t ei;
{
	u_long pa, endpa;
	vaddr_t va, sva;
	u_int32_t base, count, off, ent;
	int slot;

	if (bios32_entry.offset == 0)
		return 0;

	base = 0;
	__asm __volatile("lcall *(%4)"
	    : "+a" (service), "+b" (base), "=c" (count), "=d" (off)
	    : "D" (&bios32_entry)
	    : "%esi", "cc", "memory");

	if (service & 0xff)
		return 0;	/* not found */

	ent = base + off;
	if (ent <= BIOS32_START || ent >= BIOS32_END)
		return 0;


	endpa = round_page(BIOS32_END);

	sva = va = uvm_km_valloc(kernel_map, endpa);
	if (va == 0)
		return (0);

	slot = gdt_get_slot();
	setgdt(slot, (caddr_t)va, BIOS32_END, SDT_MEMERA, SEL_KPL, 1, 0);

	for (pa = trunc_page(BIOS32_START),
	     va += trunc_page(BIOS32_START);
	     pa < endpa; pa += NBPG, va += NBPG) {
		pmap_enter(pmap_kernel(), va, pa,
		    VM_PROT_READ | VM_PROT_WRITE,
		    VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);

		/* for all you, broken hearted */
		if (pa >= trunc_page(base)) {
			pmap_enter(pmap_kernel(), sva, pa,
			    VM_PROT_READ | VM_PROT_WRITE,
			    VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
			sva += NBPG;
		}
	}

	e->segment = GSEL(slot, SEL_KPL);
	e->offset = (vaddr_t)ent;

	ei->bei_base = base;
	ei->bei_size = count;
	ei->bei_entry = ent;

	return 1;
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
	bios_diskinfo_t *pdi;
	int biosdev;

	/* all sysctl names at this level except diskinfo are terminal */
	if (namelen != 1 && name[0] != BIOS_DISKINFO)
		return (ENOTDIR);		/* overloaded */

	if (!(bootapiver & BAPIV_VECTOR))
		return EOPNOTSUPP;

	switch (name[0]) {
	case BIOS_DEV:
		if ((pdi = bios_getdiskinfo(bootdev)) == NULL)
			return ENXIO;
		biosdev = pdi->bios_number;
		return sysctl_rdint(oldp, oldlenp, newp, biosdev);
	case BIOS_DISKINFO:
		if (namelen != 2)
			return ENOTDIR;
		if ((pdi = bios_getdiskinfo(name[1])) == NULL)
			return ENXIO;
		return sysctl_rdstruct(oldp, oldlenp, newp, pdi, sizeof(*pdi));
	case BIOS_CKSUMLEN:
		return sysctl_rdint(oldp, oldlenp, newp, bios_cksumlen);
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

	if (bios_diskinfo == NULL)
		return NULL;

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

