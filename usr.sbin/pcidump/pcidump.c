/*	$OpenBSD: pcidump.c,v 1.6 2007/10/22 20:54:52 deraadt Exp $	*/

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcidevs_data.h>

#define PCIDEV	"/dev/pci"

__dead void usage(void);
int probe(int, int, int);
void dump(int, int, int);
const char *str2busdevfunc(const char *, int *, int *, int *);
int pci_nfuncs(int, int);
int pci_read(int, int, int, u_int32_t, u_int32_t *);

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-v] [-d pcidev] [dev:bus:func]\n",
	    __progname);
	exit(1);
}

int pcifd;
int verbose = 0;

int
main(int argc, char *argv[])
{
	int nfuncs;
	int bus, dev, func;
	char *pcidev = PCIDEV;
	const char *errstr;
	int c, error = 0;

	while ((c = getopt(argc, argv, "d:v")) != -1) {
		switch (c) {
		case 'd':
			pcidev = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	pcifd = open(pcidev, O_RDONLY, 0777);
	if (pcifd == -1)
		err(1, "%s", pcidev);

	if (argc == 1) {
		errstr = str2busdevfunc(argv[0], &bus, &dev, &func);
		if (errstr != NULL)
			errx(1, "\"%s\": %s", argv[0], errstr);

		nfuncs = pci_nfuncs(bus, dev);
		if (nfuncs == -1 || func > nfuncs)
			error = ENXIO;
		else
			error = probe(bus, dev, func);

		if (error != 0)
			errx(1, "\"%s\": %s", argv[0], strerror(error));
	} else {
		for (bus = 0; bus < 256; bus++) {
			for (dev = 0; dev <= 32; dev++) {
				nfuncs = pci_nfuncs(bus, dev);
				for (func = 0; func <= nfuncs; func++) {
					probe(bus, dev, func);
				}
			}
		}
	}

	return (0);
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

	*bus = strtonum(b, 0, 256, &errstr);
	if (errstr != NULL)
		return (errstr);
	*dev = strtonum(d, 0, 32, &errstr);
	if (errstr != NULL)
		return (errstr);
	*func = strtonum(f, 0, 8, &errstr);
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

	printf("%d:%d:%d: %s %s\n", bus, dev, func,
	    (vendor == NULL) ? "unknown" : vendor,
	    (product == NULL) ? "unknown" : product);

	if (verbose)
		dump(bus, dev, func);

	return (0);
}

void
dump(int bus, int dev, int func)
{
	u_int32_t reg;
	int bar;

	if (pci_read(bus, dev, func, PCI_ID_REG, &reg) != 0)
		warn("unable to read PCI_ID_REG");
	printf("\t0x%04x: Vendor ID: %04x Product ID: %04x\n", PCI_ID_REG,
	    PCI_VENDOR(reg), PCI_PRODUCT(reg));

	if (pci_read(bus, dev, func, PCI_COMMAND_STATUS_REG, &reg) != 0)
		warn("unable to read PCI_COMMAND_STATUS_REG");
	printf("\t0x%04x: Command: %04x Status ID: %04x\n",
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

	for (bar = PCI_MAPREG_START; bar < PCI_MAPREG_END; bar += 0x4) {
		if (pci_read(bus, dev, func, bar, &reg) != 0)
			warn("unable to read PCI_MAPREG 0x%02x", bar);
		printf("\t0x%04x: %08x\n", bar, reg);
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

	if (pci_read(bus, dev, func, PCI_CAPLISTPTR_REG, &reg) != 0)
		warn("unable to read PCI_CAPLISTPTR_REG");
	printf("\t0x%04x: Capabilities Pointer: %02x\n",
	    PCI_CAPLISTPTR_REG, PCI_CAPLIST_PTR(reg));

	if (pci_read(bus, dev, func, 0x38, &reg) != 0)
		warn("unable to read 0x38 (reserved)");
	printf("\t0x%04x: %08x\n", 0x38, reg);

	if (pci_read(bus, dev, func, PCI_INTERRUPT_REG, &reg) != 0)
		warn("unable to read PCI_INTERRUPT_REG");
	printf("\t0x%04x: Interrupt Pin: %02x Line: %02x Min Gnt: %02x"
	    " Max Lat: %02x\n", PCI_INTERRUPT_REG, PCI_INTERRUPT_PIN(reg),
	    PCI_INTERRUPT_LINE(reg), PCI_MIN_GNT(reg), PCI_MAX_LAT(reg));
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
