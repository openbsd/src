/*	$OpenBSD: pcidump.c,v 1.46 2017/08/31 12:03:02 otto Exp $	*/

/*
 * Copyright (c) 2006, 2007 David Gwynne <loki@animata.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/pciio.h>

#include <stdio.h>	/* need NULL for dev/pci/ headers */

#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcidevs_data.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define PCIDEV	"/dev/pci"

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

__dead void usage(void);
void scanpcidomain(void);
int probe(int, int, int);
void dump(int, int, int);
void hexdump(int, int, int, int);
const char *str2busdevfunc(const char *, int *, int *, int *);
int pci_nfuncs(int, int);
int pci_read(int, int, int, u_int32_t, u_int32_t *);
int pci_readmask(int, int, int, u_int32_t, u_int32_t *);
void dump_caplist(int, int, int, u_int8_t);
void dump_pci_powerstate(int, int, int, uint8_t);
void dump_pcie_linkspeed(int, int, int, uint8_t);
void print_pcie_ls(uint8_t);
int dump_rom(int, int, int);
int dump_vga_bios(void);

void	dump_type0(int bus, int dev, int func);
void	dump_type1(int bus, int dev, int func);
void	dump_type2(int bus, int dev, int func);

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-v] [-x | -xx | -xxx] [-d pcidev] [bus:dev:func]\n"
	    "       %s -r file [-d pcidev] bus:dev:func\n",
	    __progname, __progname);
	exit(1);
}

int pcifd;
int romfd;
int verbose = 0;
int hex = 0;
int size = 64;

const char *pci_capnames[] = {
	"Reserved",
	"Power Management",
	"AGP",
	"Vital Product Data (VPD)",
	"Slot Identification",
	"Message Signalled Interrupts (MSI)",
	"CompactPCI Hot Swap",
	"PCI-X",
	"AMD LDT/HT",
	"Vendor Specific",
	"Debug Port",
	"CompactPCI Central Resource Control",
	"PCI Hot-Plug",
	"PCI-PCI",
	"AGP8",
	"Secure",
	"PCI Express",
	"Extended Message Signalled Interrupts (MSI-X)",
	"SATA",
	"PCI Advanced Features"
};

const char *pci_enhanced_capnames[] = {
	"Unknown",
	"Advanced Error Reporting",
	"Virtual Channel Capability",
	"Device Serial Number",
	"Power Budgeting",
	"Root Complex Link Declaration",
	"Root Complex Internal Link Control",
	"Root Complex Event Collector",
	"Multi-Function VC Capability",
	"Virtual Channel Capability",
	"Root Complex/Root Bridge",
	"Vendor-Specific",
	"Config Access",
	"Access Control Services",
	"Alternate Routing ID",
	"Address Translation Services",
	"Single Root I/O Virtualization",
	"Multi Root I/O Virtualization",
	"Multicast",
	"Page Request Interface",
	"Reserved for AMD",
	"Resizable BAR",
	"Dynamic Power Allocation",
	"TPH Requester",
	"Latency Tolerance Reporting",
	"Secondary PCIe Capability",
	"Protocol Multiplexing",
	"Process Address Space ID",
	"LN Requester",
	"Downstream Port Containment",
	"L1 PM",
	"Precision Time Measurement",
};

int
main(int argc, char *argv[])
{
	int nfuncs;
	int bus, dev, func;
	char pcidev[PATH_MAX] = PCIDEV;
	char *romfile = NULL;
	const char *errstr;
	int c, error = 0, dumpall = 1, domid = 0;

	while ((c = getopt(argc, argv, "d:r:vx")) != -1) {
		switch (c) {
		case 'd':
			strlcpy(pcidev, optarg, sizeof(pcidev));
			dumpall = 0;
			break;
		case 'r':
			romfile = optarg;
			dumpall = 0;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'x':
			hex++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1 || (romfile && argc != 1))
		usage();

	if (romfile) {
		romfd = open(romfile, O_WRONLY|O_CREAT|O_TRUNC, 0777);
		if (romfd == -1)
			err(1, "%s", romfile);
	}

	if (hex > 1)
		size = 256;
	if (hex > 2)
		size = 4096;

	if (argc == 1)
		dumpall = 0;

	if (dumpall == 0) {
		pcifd = open(pcidev, O_RDONLY, 0777);
		if (pcifd == -1)
			err(1, "%s", pcidev);
	} else {
		for (;;) {
			snprintf(pcidev, 16, "/dev/pci%d", domid++);
			pcifd = open(pcidev, O_RDONLY, 0777);
			if (pcifd == -1) {
				if (errno == ENXIO || errno == ENOENT) {
					return 0;
				} else {
					err(1, "%s", pcidev);
				}
			}
			printf("Domain %s:\n", pcidev);
			scanpcidomain();
			close(pcifd);
		}
	}

	if (argc == 1) {
		errstr = str2busdevfunc(argv[0], &bus, &dev, &func);
		if (errstr != NULL)
			errx(1, "\"%s\": %s", argv[0], errstr);

		nfuncs = pci_nfuncs(bus, dev);
		if (nfuncs == -1 || func > nfuncs)
			error = ENXIO;
		else if (romfile)
			error = dump_rom(bus, dev, func);
		else
			error = probe(bus, dev, func);

		if (error != 0)
			errc(1, error, "\"%s\"", argv[0]);
	} else {
		printf("Domain %s:\n", pcidev);
		scanpcidomain();
	}

	return (0);
}

void
scanpcidomain(void)
{
	int nfuncs;
	int bus, dev, func;

	for (bus = 0; bus < 256; bus++) {
		for (dev = 0; dev < 32; dev++) {
			nfuncs = pci_nfuncs(bus, dev);
			for (func = 0; func < nfuncs; func++) {
				probe(bus, dev, func);
			}
		}
	}
}

const char *
str2busdevfunc(const char *string, int *bus, int *dev, int *func)
{
	const char *errstr;
	char b[80], *d, *f;

	strlcpy(b, string, sizeof(b));

	d = strchr(b, ':');
	if (d == NULL)
		return("device not specified");
	*d++ = '\0';

	f = strchr(d, ':');
	if (f == NULL)
		return("function not specified");
	*f++ = '\0';

	*bus = strtonum(b, 0, 255, &errstr);
	if (errstr != NULL)
		return (errstr);
	*dev = strtonum(d, 0, 31, &errstr);
	if (errstr != NULL)
		return (errstr);
	*func = strtonum(f, 0, 7, &errstr);
	if (errstr != NULL)
		return (errstr);

	return (NULL);
}

int
probe(int bus, int dev, int func)
{
	u_int32_t id_reg;
	const struct pci_known_vendor *pkv;
	const struct pci_known_product *pkp;
	const char *vendor = NULL, *product = NULL;

	if (pci_read(bus, dev, func, PCI_ID_REG, &id_reg) != 0)
		return (errno);

	if (PCI_VENDOR(id_reg) == PCI_VENDOR_INVALID ||
	    PCI_VENDOR(id_reg) == 0)
		return (ENXIO);

	for (pkv = pci_known_vendors; pkv->vendorname != NULL; pkv++) {
		if (pkv->vendor == PCI_VENDOR(id_reg)) {
			vendor = pkv->vendorname;
			break;
		}
	}

	if (vendor != NULL) {
		for (pkp = pci_known_products; pkp->productname != NULL; pkp++)
		if (pkp->vendor == PCI_VENDOR(id_reg) &&
		    pkp->product == PCI_PRODUCT(id_reg)) {
			product = pkp->productname;
			break;
		}
	}

	printf(" %d:%d:%d: %s %s\n", bus, dev, func,
	    (vendor == NULL) ? "unknown" : vendor,
	    (product == NULL) ? "unknown" : product);

	if (verbose)
		dump(bus, dev, func);
	if (hex > 0)
		hexdump(bus, dev, func, size);

	return (0);
}

void
dump_pci_powerstate(int bus, int dev, int func, uint8_t ptr)
{
	u_int32_t pmcsr;

	if (pci_read(bus, dev, func, ptr + PCI_PMCSR, &pmcsr) != 0)
		return;

	printf("\t	State: D%d", pmcsr & PCI_PMCSR_STATE_MASK);
	if (pmcsr & PCI_PMCSR_PME_EN)
		printf(" PME# enabled");
	if (pmcsr & PCI_PMCSR_PME_STATUS)
		printf(" PME# asserted");
	printf("\n");
}

void
print_pcie_ls(uint8_t speed)
{
	if (speed & 4)
		printf("8.0");
	else if (speed & 2)
		printf("5.0");
	else if (speed & 1)
		printf("2.5");
	else
		printf("unknown (%d)", speed);
}

void
dump_pcie_linkspeed(int bus, int dev, int func, uint8_t ptr)
{
	u_int32_t lcap, sreg, lcap2 = 0, xcap;
	u_int8_t cwidth, cspeed, swidth, sspeed;

	if (pci_read(bus, dev, func, ptr + PCI_PCIE_XCAP, &xcap) != 0)
		return;

	if (PCI_PCIE_XCAP_VER(xcap) >= 2) {
		if (pci_read(bus, dev, func, ptr + PCI_PCIE_LCAP2, &lcap2) != 0)
			lcap2 = 0;
		else
			cspeed = (lcap2 & 0x0e) >> 1;
	}

	if (pci_read(bus, dev, func, ptr + PCI_PCIE_LCAP, &lcap) != 0)
		return;
	if (lcap2 == 0)
		cspeed = lcap & 0x0f;

	if (pci_read(bus, dev, func, ptr + PCI_PCIE_LCSR, &sreg) != 0)
		return;
	sreg = sreg >> 16;

	cwidth = (lcap >> 4) & 0x3f;
	if (cwidth == 0)
		return;

	swidth = (sreg >> 4) & 0x3f;
	sspeed = sreg & 0x0f;

	printf("\t        Link Speed: ");
	print_pcie_ls(sspeed);
	printf(" / ");
	print_pcie_ls(cspeed);

	printf(" GT/s Link Width: x%d / x%d\n", swidth, cwidth);
}

void
dump_pcie_enhanced_caplist(int bus, int dev, int func)
{
	u_int32_t reg;
	u_int32_t capidx;
	u_int16_t ptr;
	u_int16_t ecap;

	ptr = PCI_PCIE_ECAP;

	do {
		if (pci_read(bus, dev, func, ptr, &reg) != 0)
			return;

		if (PCI_PCIE_ECAP_ID(reg) == 0xffff &&
		    PCI_PCIE_ECAP_NEXT(reg) == PCI_PCIE_ECAP_LAST)
			return;

		ecap = PCI_PCIE_ECAP_ID(reg);
		if (ecap >= nitems(pci_enhanced_capnames))
			capidx = 0;
		else
			capidx = ecap;

		printf("\t0x%04x: Enhanced Capability 0x%02x: ", ptr, ecap);
		printf("%s\n", pci_enhanced_capnames[capidx]);

		ptr = PCI_PCIE_ECAP_NEXT(reg);

	} while (ptr != PCI_PCIE_ECAP_LAST);
}

void
dump_caplist(int bus, int dev, int func, u_int8_t ptr)
{
	u_int32_t reg;
	u_int8_t cap;

	if (pci_read(bus, dev, func, PCI_COMMAND_STATUS_REG, &reg) != 0)
		return;
	if (!(reg & PCI_STATUS_CAPLIST_SUPPORT))
		return;

	if (pci_read(bus, dev, func, ptr, &reg) != 0)
		return;
	ptr = PCI_CAPLIST_PTR(reg);
	while (ptr != 0) {
		if (pci_read(bus, dev, func, ptr, &reg) != 0)
			return;
		cap = PCI_CAPLIST_CAP(reg);
		printf("\t0x%04x: Capability 0x%02x: ", ptr, cap);
		if (cap >= nitems(pci_capnames))
			cap = 0;
		printf("%s\n", pci_capnames[cap]);
		if (cap == PCI_CAP_PWRMGMT)
			dump_pci_powerstate(bus, dev, func, ptr);
		if (cap == PCI_CAP_PCIEXPRESS) {
			dump_pcie_linkspeed(bus, dev, func, ptr);
			dump_pcie_enhanced_caplist(bus, dev, func);
		}
		ptr = PCI_CAPLIST_NEXT(reg);
	}
}

void
dump_type0(int bus, int dev, int func)
{
	const char *memtype;
	u_int64_t mem;
	u_int64_t mask;
	u_int32_t reg, reg1;
	int bar;

	for (bar = PCI_MAPREG_START; bar < PCI_MAPREG_END; bar += 0x4) {
		if (pci_read(bus, dev, func, bar, &reg) != 0 ||
		    pci_readmask(bus, dev, func, bar, &reg1) != 0)
			warn("unable to read PCI_MAPREG 0x%02x", bar);

		printf("\t0x%04x: BAR ", bar);

		if (reg == 0 && reg1 == 0) {
			printf("empty (%08x)\n", reg);
			continue;
		}

		switch (PCI_MAPREG_TYPE(reg)) {
		case PCI_MAPREG_TYPE_MEM:
			printf("mem ");
			if (PCI_MAPREG_MEM_PREFETCHABLE(reg))
				printf("prefetchable ");

			memtype = "32bit 1m";
			switch (PCI_MAPREG_MEM_TYPE(reg)) {
			case PCI_MAPREG_MEM_TYPE_32BIT:
				memtype = "32bit";
			case PCI_MAPREG_MEM_TYPE_32BIT_1M:
				printf("%s ", memtype);

				printf("addr: 0x%08x/0x%08x\n",
				    PCI_MAPREG_MEM_ADDR(reg),
				    PCI_MAPREG_MEM_SIZE(reg1));

				break;
			case PCI_MAPREG_MEM_TYPE_64BIT:
				mem = reg;
				mask = reg1;
				bar += 0x04;
				if (pci_read(bus, dev, func, bar, &reg) != 0 ||
				    pci_readmask(bus, dev, func, bar, &reg1) != 0)
					warn("unable to read 0x%02x", bar);

				mem |= (u_int64_t)reg << 32;
				mask |= (u_int64_t)reg1 << 32;

				printf("64bit addr: 0x%016llx/0x%08llx\n",
				    PCI_MAPREG_MEM64_ADDR(mem),
				    PCI_MAPREG_MEM64_SIZE(mask));

				break;
			}
			break;

		case PCI_MAPREG_TYPE_IO:
			printf("io addr: 0x%08x/0x%04x\n",
			    PCI_MAPREG_IO_ADDR(reg),
			    PCI_MAPREG_IO_SIZE(reg1));
			break;
		}
	}

	if (pci_read(bus, dev, func, PCI_CARDBUS_CIS_REG, &reg) != 0)
		warn("unable to read PCI_CARDBUS_CIS_REG");
	printf("\t0x%04x: Cardbus CIS: %08x\n", PCI_CARDBUS_CIS_REG, reg);

	if (pci_read(bus, dev, func, PCI_SUBSYS_ID_REG, &reg) != 0)
		warn("unable to read PCI_SUBSYS_ID_REG");
	printf("\t0x%04x: Subsystem Vendor ID: %04x Product ID: %04x\n",
	    PCI_SUBSYS_ID_REG, PCI_VENDOR(reg), PCI_PRODUCT(reg));

	if (pci_read(bus, dev, func, PCI_ROM_REG, &reg) != 0)
		warn("unable to read PCI_ROM_REG");
	printf("\t0x%04x: Expansion ROM Base Address: %08x\n",
	    PCI_ROM_REG, reg);

	if (pci_read(bus, dev, func, 0x38, &reg) != 0)
		warn("unable to read 0x38 (reserved)");
	printf("\t0x%04x: %08x\n", 0x38, reg);

	if (pci_read(bus, dev, func, PCI_INTERRUPT_REG, &reg) != 0)
		warn("unable to read PCI_INTERRUPT_REG");
	printf("\t0x%04x: Interrupt Pin: %02x Line: %02x Min Gnt: %02x"
	    " Max Lat: %02x\n", PCI_INTERRUPT_REG, PCI_INTERRUPT_PIN(reg),
	    PCI_INTERRUPT_LINE(reg), PCI_MIN_GNT(reg), PCI_MAX_LAT(reg));
}

void
dump_type1(int bus, int dev, int func)
{
	u_int32_t reg;
	int bar;

	for (bar = PCI_MAPREG_START; bar < PCI_MAPREG_PPB_END; bar += 0x4) {
		if (pci_read(bus, dev, func, bar, &reg) != 0)
			warn("unable to read PCI_MAPREG 0x%02x", bar);
		printf("\t0x%04x: %08x\n", bar, reg);
	}

	if (pci_read(bus, dev, func, PCI_PRIBUS_1, &reg) != 0)
		warn("unable to read PCI_PRIBUS_1");
	printf("\t0x%04x: Primary Bus: %d Secondary Bus: %d "
	    "Subordinate Bus: %d \n\t        Secondary Latency Timer: %02x\n",
	    PCI_PRIBUS_1, (reg >> 0) & 0xff, (reg >> 8) & 0xff,
	    (reg >> 16) & 0xff, (reg >> 24) & 0xff);

	if (pci_read(bus, dev, func, PCI_IOBASEL_1, &reg) != 0)
		warn("unable to read PCI_IOBASEL_1");
	printf("\t0x%04x: I/O Base: %02x I/O Limit: %02x "
	    "Secondary Status: %04x\n", PCI_IOBASEL_1, (reg >> 0 ) & 0xff,
	    (reg >> 8) & 0xff, (reg >> 16) & 0xffff);

	if (pci_read(bus, dev, func, PCI_MEMBASE_1, &reg) != 0)
		warn("unable to read PCI_MEMBASE_1");
	printf("\t0x%04x: Memory Base: %04x Memory Limit: %04x\n",
	    PCI_MEMBASE_1, (reg >> 0) & 0xffff, (reg >> 16) & 0xffff);

	if (pci_read(bus, dev, func, PCI_PMBASEL_1, &reg) != 0)
		warn("unable to read PCI_PMBASEL_1");
	printf("\t0x%04x: Prefetch Memory Base: %04x "
	    "Prefetch Memory Limit: %04x\n", PCI_PMBASEL_1,
	    (reg >> 0) & 0xffff, (reg >> 16) & 0xffff);

#undef PCI_PMBASEH_1
#define PCI_PMBASEH_1	0x28
	if (pci_read(bus, dev, func, PCI_PMBASEH_1, &reg) != 0)
		warn("unable to read PCI_PMBASEH_1");
	printf("\t0x%04x: Prefetch Memory Base Upper 32 Bits: %08x\n",
	    PCI_PMBASEH_1, reg);

#undef PCI_PMLIMITH_1
#define PCI_PMLIMITH_1	0x2c
	if (pci_read(bus, dev, func, PCI_PMLIMITH_1, &reg) != 0)
		warn("unable to read PCI_PMLIMITH_1");
	printf("\t0x%04x: Prefetch Memory Limit Upper 32 Bits: %08x\n",
	    PCI_PMLIMITH_1, reg);

#undef PCI_IOBASEH_1
#define PCI_IOBASEH_1	0x30
	if (pci_read(bus, dev, func, PCI_IOBASEH_1, &reg) != 0)
		warn("unable to read PCI_IOBASEH_1");
	printf("\t0x%04x: I/O Base Upper 16 Bits: %04x "
	    "I/O Limit Upper 16 Bits: %04x\n", PCI_IOBASEH_1,
	    (reg >> 0) & 0xffff, (reg >> 16) & 0xffff);

#define PCI_PPB_ROM_REG		0x38
	if (pci_read(bus, dev, func, PCI_PPB_ROM_REG, &reg) != 0)
		warn("unable to read PCI_PPB_ROM_REG");
	printf("\t0x%04x: Expansion ROM Base Address: %08x\n",
	    PCI_PPB_ROM_REG, reg);

	if (pci_read(bus, dev, func, PCI_INTERRUPT_REG, &reg) != 0)
		warn("unable to read PCI_INTERRUPT_REG");
	printf("\t0x%04x: Interrupt Pin: %02x Line: %02x "
	    "Bridge Control: %04x\n",
	    PCI_INTERRUPT_REG, PCI_INTERRUPT_PIN(reg),
	    PCI_INTERRUPT_LINE(reg), reg >> 16);
}

void
dump_type2(int bus, int dev, int func)
{
	u_int32_t reg;

	if (pci_read(bus, dev, func, PCI_MAPREG_START, &reg) != 0)
		warn("unable to read PCI_MAPREG\n");
	printf("\t0x%04x: Cardbus Control Registers Base Address: %08x\n",
	    PCI_MAPREG_START, reg);

	if (pci_read(bus, dev, func, PCI_PRIBUS_2, &reg) != 0)
		warn("unable to read PCI_PRIBUS_2");
	printf("\t0x%04x: Primary Bus: %d Cardbus Bus: %d "
	    "Subordinate Bus: %d \n\t        Cardbus Latency Timer: %02x\n",
	    PCI_PRIBUS_2, (reg >> 0) & 0xff, (reg >> 8) & 0xff,
	    (reg >> 16) & 0xff, (reg >> 24) & 0xff);

	if (pci_read(bus, dev, func, PCI_MEMBASE0_2, &reg) != 0)
		warn("unable to read PCI_MEMBASE0_2\n");
	printf("\t0x%04x: Memory Base 0: %08x\n", PCI_MEMBASE0_2, reg);

	if (pci_read(bus, dev, func, PCI_MEMLIMIT0_2, &reg) != 0)
		warn("unable to read PCI_MEMLIMIT0_2\n");
	printf("\t0x%04x: Memory Limit 0: %08x\n", PCI_MEMLIMIT0_2, reg);

	if (pci_read(bus, dev, func, PCI_MEMBASE1_2, &reg) != 0)
		warn("unable to read PCI_MEMBASE1_2\n");
	printf("\t0x%04x: Memory Base 1: %08x\n", PCI_MEMBASE1_2, reg);

	if (pci_read(bus, dev, func, PCI_MEMLIMIT1_2, &reg) != 0)
		warn("unable to read PCI_MEMLIMIT1_2\n");
	printf("\t0x%04x: Memory Limit 1: %08x\n", PCI_MEMLIMIT1_2, reg);

	if (pci_read(bus, dev, func, PCI_IOBASE0_2, &reg) != 0)
		warn("unable to read PCI_IOBASE0_2\n");
	printf("\t0x%04x: I/O Base 0: %08x\n", PCI_IOBASE0_2, reg);

	if (pci_read(bus, dev, func, PCI_IOLIMIT0_2, &reg) != 0)
		warn("unable to read PCI_IOLIMIT0_2\n");
	printf("\t0x%04x: I/O Limit 0: %08x\n", PCI_IOLIMIT0_2, reg);

	if (pci_read(bus, dev, func, PCI_IOBASE1_2, &reg) != 0)
		warn("unable to read PCI_IOBASE1_2\n");
	printf("\t0x%04x: I/O Base 1: %08x\n", PCI_IOBASE1_2, reg);

	if (pci_read(bus, dev, func, PCI_IOLIMIT1_2, &reg) != 0)
		warn("unable to read PCI_IOLIMIT1_2\n");
	printf("\t0x%04x: I/O Limit 1: %08x\n", PCI_IOLIMIT1_2, reg);

	if (pci_read(bus, dev, func, PCI_INTERRUPT_REG, &reg) != 0)
		warn("unable to read PCI_INTERRUPT_REG");
	printf("\t0x%04x: Interrupt Pin: %02x Line: %02x "
	    "Bridge Control: %04x\n",
	    PCI_INTERRUPT_REG, PCI_INTERRUPT_PIN(reg),
	    PCI_INTERRUPT_LINE(reg), reg >> 16);

	if (pci_read(bus, dev, func, PCI_SUBVEND_2, &reg) != 0)
		warn("unable to read PCI_SUBVEND_2");
	printf("\t0x%04x: Subsystem Vendor ID: %04x Product ID: %04x\n",
	    PCI_SUBVEND_2, PCI_VENDOR(reg), PCI_PRODUCT(reg));

	if (pci_read(bus, dev, func, PCI_PCCARDIF_2, &reg) != 0)
		warn("unable to read PCI_PCCARDIF_2\n");
	printf("\t0x%04x: 16-bit Legacy Mode Base Address: %08x\n",
	    PCI_PCCARDIF_2, reg);
}

void
dump(int bus, int dev, int func)
{
	u_int32_t reg;
	u_int8_t capptr = PCI_CAPLISTPTR_REG;

	if (pci_read(bus, dev, func, PCI_ID_REG, &reg) != 0)
		warn("unable to read PCI_ID_REG");
	printf("\t0x%04x: Vendor ID: %04x Product ID: %04x\n", PCI_ID_REG,
	    PCI_VENDOR(reg), PCI_PRODUCT(reg));

	if (pci_read(bus, dev, func, PCI_COMMAND_STATUS_REG, &reg) != 0)
		warn("unable to read PCI_COMMAND_STATUS_REG");
	printf("\t0x%04x: Command: %04x Status: %04x\n",
	    PCI_COMMAND_STATUS_REG, reg & 0xffff, (reg  >> 16) & 0xffff);

	if (pci_read(bus, dev, func, PCI_CLASS_REG, &reg) != 0)
		warn("unable to read PCI_CLASS_REG");
	printf("\t0x%04x: Class: %02x Subclass: %02x Interface: %02x "
	    "Revision: %02x\n", PCI_CLASS_REG, PCI_CLASS(reg),
	    PCI_SUBCLASS(reg), PCI_INTERFACE(reg), PCI_REVISION(reg));

	if (pci_read(bus, dev, func, PCI_BHLC_REG, &reg) != 0)
		warn("unable to read PCI_BHLC_REG");
	printf("\t0x%04x: BIST: %02x Header Type: %02x Latency Timer: %02x "
	    "Cache Line Size: %02x\n", PCI_BHLC_REG, PCI_BIST(reg),
	    PCI_HDRTYPE(reg), PCI_LATTIMER(reg), PCI_CACHELINE(reg));

	switch (PCI_HDRTYPE_TYPE(reg)) {
	case 2:
		dump_type2(bus, dev, func);
		capptr = PCI_CARDBUS_CAPLISTPTR_REG;
		break;
	case 1:
		dump_type1(bus, dev, func);
		break;
	case 0:
		dump_type0(bus, dev, func);
		break;
	default:
		break;
	}
	dump_caplist(bus, dev, func, capptr);
}

void
hexdump(int bus, int dev, int func, int size)
{
	u_int32_t reg;
	int i;

	for (i = 0; i < size; i += 4) {
		if (pci_read(bus, dev, func, i, &reg) != 0) {
			if (errno == EINVAL)
				return;
			warn("unable to read 0x%02x", i);
		}

		if ((i % 16) == 0)
			printf("\t0x%04x:", i);
		printf(" %08x", reg);

		if ((i % 16) == 12)
			printf("\n");
	}
}

int
pci_nfuncs(int bus, int dev)
{
	u_int32_t hdr;

	if (pci_read(bus, dev, 0, PCI_BHLC_REG, &hdr) != 0)
		return (-1);

	return (PCI_HDRTYPE_MULTIFN(hdr) ? 8 : 1);
}

int
pci_read(int bus, int dev, int func, u_int32_t reg, u_int32_t *val)
{
	struct pci_io io;
	int rv;

	bzero(&io, sizeof(io));
	io.pi_sel.pc_bus = bus;
	io.pi_sel.pc_dev = dev;
	io.pi_sel.pc_func = func;
	io.pi_reg = reg;
	io.pi_width = 4;

	rv = ioctl(pcifd, PCIOCREAD, &io);
	if (rv != 0)
		return (rv);

	*val = io.pi_data;

	return (0);
}

int
pci_readmask(int bus, int dev, int func, u_int32_t reg, u_int32_t *val)
{
	struct pci_io io;
	int rv;

	bzero(&io, sizeof(io));
	io.pi_sel.pc_bus = bus;
	io.pi_sel.pc_dev = dev;
	io.pi_sel.pc_func = func;
	io.pi_reg = reg;
	io.pi_width = 4;

	rv = ioctl(pcifd, PCIOCREADMASK, &io);
	if (rv != 0)
		return (rv);

	*val = io.pi_data;

	return (0);
}

int
dump_rom(int bus, int dev, int func)
{
	struct pci_rom rom;
	u_int32_t cr, addr;

	if (pci_read(bus, dev, func, PCI_ROM_REG, &addr) != 0 ||
	    pci_read(bus, dev, func, PCI_CLASS_REG, &cr) != 0)
		return (errno);

	if (addr == 0 && PCI_CLASS(cr) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(cr) == PCI_SUBCLASS_DISPLAY_VGA)
		return dump_vga_bios();

	bzero(&rom, sizeof(rom));
	rom.pr_sel.pc_bus = bus;
	rom.pr_sel.pc_dev = dev;
	rom.pr_sel.pc_func = func;
	if (ioctl(pcifd, PCIOCGETROMLEN, &rom))
		return (errno);

	rom.pr_rom = malloc(rom.pr_romlen);
	if (rom.pr_rom == NULL)
		return (ENOMEM);

	if (ioctl(pcifd, PCIOCGETROM, &rom))
		return (errno);

	if (write(romfd, rom.pr_rom, rom.pr_romlen) == -1)
		return (errno);

	return (0);
}

#define VGA_BIOS_ADDR	0xc0000
#define VGA_BIOS_LEN	0x10000

int
dump_vga_bios(void)
{
#if defined(__amd64__) || defined(__i386__)
	void *bios;
	int fd;

	fd = open(_PATH_MEM, O_RDONLY, 0777);
	if (fd == -1)
		err(1, "%s", _PATH_MEM);

	bios = malloc(VGA_BIOS_LEN);
	if (bios == NULL)
		return (ENOMEM);

	if (pread(fd, bios, VGA_BIOS_LEN, VGA_BIOS_ADDR) == -1)
		err(1, "%s", _PATH_MEM);

	if (write(romfd, bios, VGA_BIOS_LEN) == -1) {
		free(bios);
		return (errno);
	}

	free(bios);

	return (0);
#else
	return (ENODEV);
#endif
}
