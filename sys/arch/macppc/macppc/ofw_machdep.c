/*	$OpenBSD: ofw_machdep.c,v 1.15 2002/09/15 02:02:44 deraadt Exp $	*/
/*	$NetBSD: ofw_machdep.c,v 1.1 1996/09/30 16:34:50 ws Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/powerpc.h>
#include <machine/autoconf.h>

#include <dev/ofw/openfirm.h>

#include <macppc/macppc/ofw_machdep.h>

#include <ukbd.h>
#include <akbd.h>
#include <zstty.h>
#include <dev/usb/ukbdvar.h>
#include <macppc/dev/akbdvar.h>

/* XXX, called from asm */
int save_ofw_mapping(void);
int restore_ofw_mapping(void);

void OF_exit(void) __attribute__((__noreturn__));
void OF_boot(char *bootspec) __attribute__((__noreturn__));
void ofw_mem_regions(struct mem_region **memp, struct mem_region **availp);
void ofw_vmon(void);

struct firmware ofw_firmware = {
	ofw_mem_regions,
	OF_exit,
	OF_boot,
	ofw_vmon
#ifdef FW_HAS_PUTC
	ofwcnputc;
#endif
};

#define	OFMEM_REGIONS	32
static struct mem_region OFmem[OFMEM_REGIONS + 1], OFavail[OFMEM_REGIONS + 3];

/*
 * This is called during initppc, before the system is really initialized.
 * It shall provide the total and the available regions of RAM.
 * Both lists must have a zero-size entry as terminator.
 * The available regions need not take the kernel into account, but needs
 * to provide space for two additional entry beyond the terminating one.
 */
void
ofw_mem_regions(memp, availp)
	struct mem_region **memp, **availp;
{
	int phandle;

	/*
	 * Get memory.
	 */
	if ((phandle = OF_finddevice("/memory")) == -1 ||
	    OF_getprop(phandle, "reg", OFmem,
	    sizeof OFmem[0] * OFMEM_REGIONS) <= 0 ||
	    OF_getprop(phandle, "available", OFavail,
	    sizeof OFavail[0] * OFMEM_REGIONS) <= 0)
		panic("no memory?");
	*memp = OFmem;
	/* HACK */
	if (OFmem[0].size == 0) {
		*memp = OFavail;
	}
	*availp = OFavail;
}

typedef void (fwcall_f)(int, int);
extern fwcall_f *fwcall;
fwcall_f fwentry;
extern u_int32_t ofmsr;

void
ofw_vmon()
{
	fwcall = &fwentry;
}

int OF_stdout;
int OF_stdin;

/* code to save and create the necessary mappings for BSD to handle
 * the vm-setup for OpenFirmware
 */
static int N_mapping;
static struct {
	vm_offset_t va;
	int len;
	vm_offset_t pa;
	int mode;
} ofw_mapping[256];
int
save_ofw_mapping()
{
	int mmui, mmu;
	int chosen;
	int stdout, stdin;
	if ((chosen = OF_finddevice("/chosen")) == -1) {
		return 0;
	}

	if (OF_getprop(chosen, "stdin", &stdin, sizeof stdin) != sizeof stdin) {
		return 0;
	}
	OF_stdin = stdin;
	if (OF_getprop(chosen, "stdout", &stdout, sizeof stdout) !=
	    sizeof stdout) {
		return 0;
	}
	if (stdout == 0) {
		/* If the screen is to be console, but not active, open it */
		stdout = OF_open("screen");
	}
	OF_stdout = stdout;

	chosen = OF_finddevice("/chosen");

	OF_getprop(chosen, "mmu", &mmui, 4);
	mmu = OF_instance_to_package(mmui);
	bzero(ofw_mapping, sizeof(ofw_mapping));

	N_mapping = OF_getprop(mmu, "translations", ofw_mapping,
	    sizeof(ofw_mapping));
	N_mapping /= sizeof(ofw_mapping[0]);

	fw = &ofw_firmware;
	fwcall = &fwentry;
	return 0;
}

struct pmap ofw_pmap;
int
restore_ofw_mapping()
{
	int i;

	pmap_pinit(&ofw_pmap);

	ofw_pmap.pm_sr[KERNEL_SR] = KERNEL_SEGMENT;

	for (i = 0; i < N_mapping; i++) {
		vm_offset_t pa = ofw_mapping[i].pa;
		vm_offset_t va = ofw_mapping[i].va;
		int size = ofw_mapping[i].len;

		if (va < 0xf8000000)			/* XXX */
			continue;

		while (size > 0) {
			pmap_enter(&ofw_pmap, va, pa, VM_PROT_ALL, PMAP_WIRED);
			pa += NBPG;
			va += NBPG;
			size -= NBPG;
		}
	}
	pmap_update(pmap_kernel());

	return 0;
}

typedef void  (void_f) (void);
extern void_f *pending_int_f;
void ofw_do_pending_int(void);
extern int system_type;

void ofw_intr_init(void);

void
ofrootfound()
{
	int node;
	struct ofprobe probe;

	if (!(node = OF_peer(0)))
		panic("No PROM root");
	probe.phandle = node;
	if (!config_rootfound("ofroot", &probe))
		panic("ofroot not configured");
	if (system_type == OFWMACH) {
		pending_int_f = ofw_do_pending_int;
		ofw_intr_init();
	}
}

void
ofw_intr_establish()
{
	if (system_type == OFWMACH) {
		pending_int_f = ofw_do_pending_int;
		ofw_intr_init();
	}
}

void
ofw_intr_init()
{
	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so imp > (tty | net | bio).
	 */
	/* with openfirmware drivers all levels block clock
	 * (have to block polling)
	 */
	imask[IPL_IMP] = SPL_CLOCK;
	imask[IPL_TTY] = SPL_CLOCK | SINT_TTY;
	imask[IPL_NET] = SPL_CLOCK | SINT_NET;
	imask[IPL_BIO] = SPL_CLOCK;
	imask[IPL_IMP] |= imask[IPL_TTY] | imask[IPL_NET] | imask[IPL_BIO];

	/*
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_TTY] |= imask[IPL_NET] | imask[IPL_BIO];
	imask[IPL_NET] |= imask[IPL_BIO];

	/*
	 * These are pseudo-levels.
	 */
	imask[IPL_NONE] = 0x00000000;
	imask[IPL_HIGH] = 0xffffffff;

}

void
ofw_do_pending_int()
{
	int pcpl;
	int emsr, dmsr;
	static int processing;

	if (processing)
		return;

	processing = 1;
	__asm__ volatile("mfmsr %0" : "=r"(emsr));
	dmsr = emsr & ~PSL_EE;
	__asm__ volatile("mtmsr %0" :: "r"(dmsr));


	pcpl = splhigh();		/* Turn off all */
	if ((ipending & SINT_CLOCK) && ((pcpl & imask[IPL_CLOCK]) == 0)) {
		ipending &= ~SINT_CLOCK;
		softclock();
	}
	if ((ipending & SINT_NET) && ((pcpl & imask[IPL_NET]) == 0) ) {
		extern int netisr;
		int pisr = netisr;
		netisr = 0;
		ipending &= ~SINT_NET;
		softnet(pisr);
	}
	ipending &= pcpl;
	cpl = pcpl;	/* Don't use splx... we are here already! */
	__asm__ volatile("mtmsr %0" :: "r"(emsr));
	processing = 0;
}

#include <dev/pci/pcivar.h>
#include <arch/macppc/pci/vgafb_pcivar.h>
static pcitag_t ofw_make_tag( void *cpv, int bus, int dev, int fnc);

/* ARGSUSED */
static pcitag_t
ofw_make_tag(cpv, bus, dev, fnc)
	void *cpv;
	int bus, dev, fnc;
{
	return (bus << 16) | (dev << 11) | (fnc << 8);
}

#define OFW_PCI_PHYS_HI_BUSMASK         0x00ff0000
#define OFW_PCI_PHYS_HI_BUSSHIFT        16
#define OFW_PCI_PHYS_HI_DEVICEMASK      0x0000f800
#define OFW_PCI_PHYS_HI_DEVICESHIFT     11
#define OFW_PCI_PHYS_HI_FUNCTIONMASK    0x00000700
#define OFW_PCI_PHYS_HI_FUNCTIONSHIFT   8

#define pcibus(x) \
	(((x) & OFW_PCI_PHYS_HI_BUSMASK) >> OFW_PCI_PHYS_HI_BUSSHIFT)
#define pcidev(x) \
	(((x) & OFW_PCI_PHYS_HI_DEVICEMASK) >> OFW_PCI_PHYS_HI_DEVICESHIFT)
#define pcifunc(x) \
	(((x) & OFW_PCI_PHYS_HI_FUNCTIONMASK) >> OFW_PCI_PHYS_HI_FUNCTIONSHIFT)


struct ppc_bus_space ppc_membus;
int cons_displaytype=0;
bus_space_tag_t cons_membus = &ppc_membus;
bus_space_handle_t cons_display_mem_h;
bus_space_handle_t cons_display_ctl_h;
int cons_height, cons_width, cons_linebytes, cons_depth;
int cons_display_ofh;
u_int32_t cons_addr;
int cons_brightness;
int cons_backlight_available;

#include "vgafb_pci.h"

struct usb_kbd_ihandles {
	struct usb_kbd_ihandles *next;
	int ihandle;
};

void of_display_console(void);

void
ofwconprobe()
{
	char type[32];
	int stdout_node;

	stdout_node = OF_instance_to_package(OF_stdout);

	/* handle different types of console */

	bzero(type, sizeof(type));
	if (OF_getprop(stdout_node, "device_type", type, sizeof(type)) == -1) {
		return; /* XXX */
	}
	if (strcmp(type, "display") == 0) {
		of_display_console();
		return;
	}
	if (strcmp(type, "serial") == 0) {
#if NZSTTY > 0
		/* zscnprobe/zscninit do all the required initialization */
		return;
#endif
	}

	OF_stdout = OF_open("screen");
	OF_stdin = OF_open("keyboard");

	/* cross fingers that this works. */
	of_display_console();

	return;
}

#define DEVTREE_UNKNOWN 0
#define DEVTREE_USB	1
#define DEVTREE_ADB	2
#define DEVTREE_HID	3
int ofw_devtree = DEVTREE_UNKNOWN;

#define OFW_HAVE_USBKBD 1
#define OFW_HAVE_ADBKBD 2
int ofw_have_kbd = 0;

void ofw_recurse_keyboard(int pnode);
void ofw_find_keyboard(void);

void
ofw_recurse_keyboard(int pnode)
{
	char name[32];
	int old_devtree;
	int len;
	int node;

	for (node = OF_child(pnode); node != 0; node = OF_peer(node)) {

		len = OF_getprop(node, "name", name, 20);
		if (len == 0)
			continue;
		name[len] = 0;
		if (strcmp(name, "keyboard") == 0) {
			/* found a keyboard node, where is it? */
			if (ofw_devtree == DEVTREE_USB) {
				ofw_have_kbd |= OFW_HAVE_USBKBD;
			} else if (ofw_devtree == DEVTREE_ADB) {
				ofw_have_kbd |= OFW_HAVE_ADBKBD;
			} else {
				/* hid or some other keyboard? igore */
			}
			continue;
		}

		old_devtree = ofw_devtree;

		if (strcmp(name, "adb") == 0) {
			ofw_devtree = DEVTREE_ADB;
		}
		if (strcmp(name, "usb") == 0) {
			ofw_devtree = DEVTREE_USB;
		}

		ofw_recurse_keyboard(node);

		ofw_devtree = old_devtree; /* nest? */
	}
}

void
ofw_find_keyboard()
{
	int stdin_node;
	char iname[32];
	int len;

	stdin_node = OF_instance_to_package(OF_stdin);
	len = OF_getprop(stdin_node, "name", iname, 20);
	iname[len] = 0;
	printf("console in [%s] ", iname);

	/* GRR, apple removed the interface once used for keyboard
	 * detection walk the OFW tree to find keyboards and what type.
	 */

	ofw_recurse_keyboard(OF_peer(0));

	if (ofw_have_kbd == 0) {
		printf("no keyboard found, hoping USB will be present\n");
#if NUKBD > 0
		ukbd_cnattach();
#endif
	}

	if (ofw_have_kbd == (OFW_HAVE_USBKBD|OFW_HAVE_ADBKBD)) {
#if NUKBD > 0
		printf("USB and ADB found, using USB\n");
		ukbd_cnattach();
#else
		ofw_have_kbd = OFW_HAVE_ADBKBD; /* ??? */
#endif
	}
	if (ofw_have_kbd == OFW_HAVE_USBKBD) {
#if NUKBD > 0
		printf("USB found\n");
		ukbd_cnattach();
#endif
	} else if (ofw_have_kbd == OFW_HAVE_ADBKBD) {
#if NAKBD >0
		printf("ADB found\n");
		akbd_cnattach();
#endif
	}
}

void
of_display_console()
{
#if NVGAFB_PCI > 0
	char name[32];
	int len;
	int stdout_node;
	int display_node;
	int err;
	int backlight_control[2];
	u_int32_t memtag, iotag;
	struct ppc_pci_chipset pa;
	struct {
		u_int32_t phys_hi, phys_mid, phys_lo;
		u_int32_t size_hi, size_lo;
	} addr [8];

	pa.pc_make_tag = &ofw_make_tag;

	stdout_node = OF_instance_to_package(OF_stdout);
	len = OF_getprop(stdout_node, "name", name, 20);
	name[len] = 0;
	printf("console out [%s]", name);
	cons_displaytype=1;
	cons_display_ofh = OF_stdout;
	err = OF_getprop(stdout_node, "width", &cons_width, 4);
	if ( err != 4) {
		cons_width = 0;
	}
	err = OF_getprop(stdout_node, "linebytes", &cons_linebytes, 4);
	if ( err != 4) {
		cons_linebytes = cons_width;
	}
	err = OF_getprop(stdout_node, "height", &cons_height, 4);
	if ( err != 4) {
		cons_height = 0;
	}
	err = OF_getprop(stdout_node, "depth", &cons_depth, 4);
	if ( err != 4) {
		cons_depth = 0;
	}
	err = OF_getprop(stdout_node, "address", &cons_addr, 4);
	if ( err != 4) {
		OF_interpret("frame-buffer-adr", 1, &cons_addr);
	}

	ofw_find_keyboard();

	display_node = stdout_node;
	len = OF_getprop(stdout_node, "assigned-addresses", addr, sizeof(addr));
	if (len == -1) {
		display_node = OF_parent(stdout_node);
		len = OF_getprop(display_node, "name", name, 20);
		name[len] = 0;

		printf("using parent %s:", name);
		len = OF_getprop(display_node, "assigned-addresses",
			addr, sizeof(addr));
		if (len < sizeof(addr[0])) {
			panic(": no address\n");
		}
	}
	len = OF_getprop(display_node, "backlight-control",
	    backlight_control, sizeof(backlight_control));
	if (len > 0)
		cons_backlight_available = 1;

	memtag = ofw_make_tag(NULL, pcibus(addr[0].phys_hi),
		pcidev(addr[0].phys_hi),
		pcifunc(addr[0].phys_hi));
	iotag = ofw_make_tag(NULL, pcibus(addr[1].phys_hi),
		pcidev(addr[1].phys_hi),
		pcifunc(addr[1].phys_hi));

#if 1
	printf(": memaddr %x size %x, ", addr[0].phys_lo, addr[0].size_lo);
	printf(": consaddr %x, ", cons_addr);
	printf(": ioaddr %x, size %x", addr[1].phys_lo, addr[1].size_lo);
	printf(": memtag %x, iotag %x", memtag, iotag);
	printf(": width %d linebytes %d height %d depth %d\n",
		cons_width, cons_linebytes, cons_height, cons_depth);
#endif

	{
		int i;

		cons_membus->bus_base = 0x80000000;
		cons_membus->bus_reverse = 1;
#if 0
		err = bus_space_map( cons_membus, cons_addr, addr[0].size_lo,
			0, &cons_display_mem_h);
		printf("mem map err %x",err);
		bus_space_map( cons_membus, addr[1].phys_lo, addr[1].size_lo,
			0, &cons_display_ctl_h);
#endif

		vgafb_pci_console(cons_membus,
			addr[1].phys_lo, addr[1].size_lo,
			cons_membus,
			cons_addr, addr[0].size_lo,
			&pa, pcibus(addr[1].phys_hi), pcidev(addr[1].phys_hi),
			pcifunc(addr[1].phys_hi));

#if 1
		for (i = 0; i < cons_linebytes * cons_height; i++) {
			bus_space_write_1(cons_membus,
				cons_display_mem_h, i, 0);

		}
#endif
	}

	of_setbrightness(DEFAULT_BRIGHTNESS);
#endif
}

void
of_setbrightness(brightness)
	int brightness;
{

#if NVGAFB_PCI > 0
	if (cons_backlight_available == 0)
		return;

	if (brightness < MIN_BRIGHTNESS)
		brightness = MIN_BRIGHTNESS;
	else if (brightness > MAX_BRIGHTNESS)
		brightness = MAX_BRIGHTNESS;

	cons_brightness = brightness;

	/* The OF method is called "set-contrast" but affects brightness. Don't ask. */
	OF_call_method_1("set-contrast", cons_display_ofh, 1, cons_brightness);

	/* XXX this routine should also save the brightness settings in the nvram */
#endif
}

#include <dev/cons.h>

cons_decl(ofw);

/*
 * Console support functions
 */
void
ofwcnprobe(cd)
	struct consdev *cd;
{
	cd->cn_pri = CN_DEAD;
}

void
ofwcninit(cd)
	struct consdev *cd;
{
}

void
ofwcnputc(dev, c)
	dev_t dev;
	int c;
{
	char ch = c;

	OF_write(OF_stdout, &ch, 1);
}
int
ofwcngetc(dev)
	dev_t dev;
{
	unsigned char ch = '\0';
	int l;

	while ((l = OF_read(OF_stdin, &ch, 1)) != 1)
		if (l != -2 && l != 0)
		return -1;
	return ch;
}

void
ofwcnpollc(dev, on)
	dev_t dev;
	int on;
{
}

struct consdev consdev_ofw = {
	ofwcnprobe,
	ofwcninit,
	ofwcngetc,
	ofwcnputc,
	ofwcnpollc,
	NULL,
};

void
ofwconsinit()
{
	struct consdev *cp;
	cp = &consdev_ofw;
	cn_tab = cp;
}
