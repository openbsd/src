/*	$OpenBSD: ofw_machdep.c,v 1.35 2010/04/21 03:03:26 deraadt Exp $	*/
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
#include <sys/timeout.h>

#include <uvm/uvm_extern.h>

#include <machine/powerpc.h>
#include <machine/autoconf.h>

#include <dev/ofw/openfirm.h>

#include <macppc/macppc/ofw_machdep.h>

#include "ukbd.h"
#include "akbd.h"
#include "zstty.h"
#include <dev/usb/ukbdvar.h>
#include <dev/adb/akbdvar.h>
#include <dev/usb/usbdevs.h>

/* XXX, called from asm */
int save_ofw_mapping(void);

void OF_exit(void) __attribute__((__noreturn__));
void OF_boot(char *bootspec) __attribute__((__noreturn__));
void ofw_mem_regions(struct mem_region **memp, struct mem_region **availp);
void ofw_vmon(void);

extern char *hw_prod;

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
ofw_mem_regions(struct mem_region **memp, struct mem_region **availp)
{
	int phandle;
	int nreg, navail;
	int i, j;

	/*
	 * Get memory.
	 */
	phandle = OF_finddevice("/memory");
	if (phandle == -1)
		panic("no memory?");

	nreg = OF_getprop(phandle, "reg", OFmem,
	    sizeof(OFmem[0]) * OFMEM_REGIONS) / sizeof(OFmem[0]);
	navail = OF_getprop(phandle, "available", OFavail,
	    sizeof(OFavail[0]) * OFMEM_REGIONS) / sizeof(OFavail[0]);
	if (nreg <= 0 || navail <= 0)
		panic("no memory?");

	/* Eliminate empty regions. */
	for (i = 0, j = 0; i < nreg; i++) {
		if (OFmem[i].size == 0)
			continue;
		if (i != j) {
			OFmem[j].start = OFmem[i].start;
			OFmem[j].size = OFmem[i].size;
			OFmem[i].start = 0;
			OFmem[i].size = 0;
		}
		j++;
	}

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

int
save_ofw_mapping()
{
	int chosen;
	int stdout, stdin;

	if ((chosen = OF_finddevice("/chosen")) == -1) {
		return 0;
	}

	if (OF_getprop(chosen, "stdin", &stdin, sizeof stdin) != sizeof stdin) {
		return 0;
	}

	OF_stdin = stdin;
	if (OF_getprop(chosen, "stdout", &stdout, sizeof stdout)
	    != sizeof stdout) {
		return 0;
	}

	if (stdout == 0) {
		/* If the screen is to be console, but not active, open it */
		stdout = OF_open("screen");
	}
	OF_stdout = stdout;

	fw = &ofw_firmware;
	fwcall = &fwentry;
	return 0;
}

#include <dev/pci/pcivar.h>
#include <arch/macppc/pci/vgafb_pcivar.h>
static pcitag_t ofw_make_tag( void *cpv, int bus, int dev, int fnc);

/* ARGSUSED */
static pcitag_t
ofw_make_tag(void *cpv, int bus, int dev, int fnc)
{
        return (bus << 16) | (dev << 11) | (fnc << 8);
}

#define       OFW_PCI_PHYS_HI_BUSMASK         0x00ff0000
#define       OFW_PCI_PHYS_HI_BUSSHIFT        16
#define       OFW_PCI_PHYS_HI_DEVICEMASK      0x0000f800
#define       OFW_PCI_PHYS_HI_DEVICESHIFT     11
#define       OFW_PCI_PHYS_HI_FUNCTIONMASK    0x00000700
#define       OFW_PCI_PHYS_HI_FUNCTIONSHIFT   8

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
	if (OF_getprop(stdout_node,  "device_type", type, sizeof(type)) == -1) {
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
	int len, attach = 0;

	stdin_node = OF_instance_to_package(OF_stdin);
	len = OF_getprop(stdin_node, "name", iname, 20);
	iname[len] = 0;
	printf("console in [%s] ", iname);

	/* GRR, apple removed the interface once used for keyboard
	 * detection walk the OFW tree to find keyboards and what type.
	 */

	ofw_recurse_keyboard(OF_peer(0));


	if (ofw_have_kbd == (OFW_HAVE_USBKBD | OFW_HAVE_ADBKBD)) {
		/*
		 * On some machines, such as PowerBook6,8,
		 * the built-in USB Bluetooth device
		 * appears as an USB device.  Prefer
		 * ADB (builtin) keyboard for console
		 * for PowerBook systems.
		 */
		if (strncmp(hw_prod, "PowerBook", 9) ||
		    strncmp(hw_prod, "iBook", 5)) {
			ofw_have_kbd = OFW_HAVE_ADBKBD;
		} else {
			ofw_have_kbd = OFW_HAVE_USBKBD;
		}
		printf("USB and ADB found");
	}
	if (ofw_have_kbd == OFW_HAVE_USBKBD) {
#if NUKBD > 0
		printf(", using USB\n");
		ukbd_cnattach();
		attach = 1;
#endif
	} else if (ofw_have_kbd == OFW_HAVE_ADBKBD) {
#if NAKBD >0
		printf(", using ADB\n");
		akbd_cnattach();
		attach = 1;
#endif
	} 
	if (attach == 0) {
#if NUKBD > 0
		printf(", no keyboard attached, trying usb anyway\n");
		ukbd_cnattach();
#else
		printf(", no keyboard found!\n");
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
			panic(": no address");
		}
	}

	if (OF_getnodebyname(0, "backlight") != 0)
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

	if (cons_backlight_available == 1)
		of_setbrightness(DEFAULT_BRIGHTNESS);
#endif
}

void
of_setbrightness(int brightness)
{

#if NVGAFB_PCI > 0
	if (cons_backlight_available == 0)
		return;

	if (brightness < MIN_BRIGHTNESS)
		brightness = MIN_BRIGHTNESS;
	else if (brightness > MAX_BRIGHTNESS)
		brightness = MAX_BRIGHTNESS;

	cons_brightness = brightness;

	/*
	 * The OF method is called "set-contrast" but affects brightness.
	 * Don't ask.
	 */
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
ofwcnprobe(struct consdev *cd)
{
}

void
ofwcninit(struct consdev *cd)
{
}
void
ofwcnputc(dev_t dev, int c)
{
	char ch = c;

	OF_write(OF_stdout, &ch, 1);
}
int
ofwcngetc(dev_t dev)
{
        unsigned char ch = '\0';
        int l;

        while ((l = OF_read(OF_stdin, &ch, 1)) != 1)
                if (l != -2 && l != 0)
                        return -1;
        return ch;
}

void
ofwcnpollc(dev_t dev, int on)
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
