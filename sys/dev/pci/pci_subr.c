/*	$OpenBSD: pci_subr.c,v 1.8 1998/02/03 19:47:13 deraadt Exp $	*/
/*	$NetBSD: pci_subr.c,v 1.19 1996/10/13 01:38:29 christos Exp $	*/

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * PCI autoconfiguration support functions.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#ifdef PCIVERBOSE
#include <dev/pci/pcidevs.h>
#endif

/*
 * Descriptions of known PCI classes and subclasses.
 *
 * Subclasses are described in the same way as classes, but have a
 * NULL subclass pointer.
 */
struct pci_class {
	char		*name;
	int		val;		/* as wide as pci_{,sub}class_t */
	struct pci_class *subclasses;
};

struct pci_class pci_subclass_prehistoric[] = {
	{ "miscellaneous",	PCI_SUBCLASS_PREHISTORIC_MISC,		},
	{ "VGA",		PCI_SUBCLASS_PREHISTORIC_VGA,		},
	{ 0 }
};

struct pci_class pci_subclass_mass_storage[] = {
	{ "SCSI",		PCI_SUBCLASS_MASS_STORAGE_SCSI,		},
	{ "IDE",		PCI_SUBCLASS_MASS_STORAGE_IDE,		},
	{ "floppy",		PCI_SUBCLASS_MASS_STORAGE_FLOPPY,	},
	{ "IPI",		PCI_SUBCLASS_MASS_STORAGE_IPI,		},
	{ "RAID",		PCI_SUBCLASS_MASS_STORAGE_RAID,		},
	{ "miscellaneous",	PCI_SUBCLASS_MASS_STORAGE_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_network[] = {
	{ "ethernet",		PCI_SUBCLASS_NETWORK_ETHERNET,		},
	{ "token ring",		PCI_SUBCLASS_NETWORK_TOKENRING,		},
	{ "FDDI",		PCI_SUBCLASS_NETWORK_FDDI,		},
	{ "ATM",		PCI_SUBCLASS_NETWORK_ATM,		},
	{ "miscellaneous",	PCI_SUBCLASS_NETWORK_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_display[] = {
	{ "VGA",		PCI_SUBCLASS_DISPLAY_VGA,		},
	{ "XGA",		PCI_SUBCLASS_DISPLAY_XGA,		},
	{ "miscellaneous",	PCI_SUBCLASS_DISPLAY_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_multimedia[] = {
	{ "video",		PCI_SUBCLASS_MULTIMEDIA_VIDEO,		},
	{ "audio",		PCI_SUBCLASS_MULTIMEDIA_AUDIO,		},
	{ "miscellaneous",	PCI_SUBCLASS_MULTIMEDIA_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_memory[] = {
	{ "RAM",		PCI_SUBCLASS_MEMORY_RAM,		},
	{ "flash",		PCI_SUBCLASS_MEMORY_FLASH,		},
	{ "miscellaneous",	PCI_SUBCLASS_MEMORY_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_bridge[] = {
	{ "host",		PCI_SUBCLASS_BRIDGE_HOST,		},
	{ "ISA",		PCI_SUBCLASS_BRIDGE_ISA,		},
	{ "EISA",		PCI_SUBCLASS_BRIDGE_EISA,		},
	{ "MicroChannel",	PCI_SUBCLASS_BRIDGE_MC,			},
	{ "PCI",		PCI_SUBCLASS_BRIDGE_PCI,		},
	{ "PCMCIA",		PCI_SUBCLASS_BRIDGE_PCMCIA,		},
	{ "NuBus",		PCI_SUBCLASS_BRIDGE_NUBUS,		},
	{ "CardBus",		PCI_SUBCLASS_BRIDGE_CARDBUS,		},
	{ "miscellaneous",	PCI_SUBCLASS_BRIDGE_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_communications[] = {
	{ "serial",		PCI_SUBCLASS_COMMUNICATIONS_SERIAL,	},
	{ "parallel",		PCI_SUBCLASS_COMMUNICATIONS_PARALLEL,	},
	{ "miscellaneous",	PCI_SUBCLASS_COMMUNICATIONS_MISC,	},
	{ 0 },
};

struct pci_class pci_subclass_system[] = {
	{ "8259 PIC",		PCI_SUBCLASS_SYSTEM_PIC,		},
	{ "8237 DMA",		PCI_SUBCLASS_SYSTEM_DMA,		},
	{ "8254 timer",		PCI_SUBCLASS_SYSTEM_TIMER,		},
	{ "RTC",		PCI_SUBCLASS_SYSTEM_RTC,		},
	{ "miscellaneous",	PCI_SUBCLASS_SYSTEM_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_input[] = {
	{ "keyboard",		PCI_SUBCLASS_INPUT_KEYBOARD,		},
	{ "digitizer",		PCI_SUBCLASS_INPUT_DIGITIZER,		},
	{ "mouse",		PCI_SUBCLASS_INPUT_MOUSE,		},
	{ "miscellaneous",	PCI_SUBCLASS_INPUT_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_dock[] = {
	{ "generic",		PCI_SUBCLASS_DOCK_GENERIC,		},
	{ "miscellaneous",	PCI_SUBCLASS_DOCK_MISC,			},
	{ 0 },
};

struct pci_class pci_subclass_processor[] = {
	{ "386",		PCI_SUBCLASS_PROCESSOR_386,		},
	{ "486",		PCI_SUBCLASS_PROCESSOR_486,		},
	{ "Pentium",		PCI_SUBCLASS_PROCESSOR_PENTIUM,		},
	{ "Alpha",		PCI_SUBCLASS_PROCESSOR_ALPHA,		},
	{ "PowerPC",		PCI_SUBCLASS_PROCESSOR_POWERPC,		},
	{ "Co-processor",	PCI_SUBCLASS_PROCESSOR_COPROC,		},
	{ 0 },
};

struct pci_class pci_subclass_serialbus[] = {
	{ "Firewire",		PCI_SUBCLASS_SERIALBUS_FIREWIRE,	},
	{ "ACCESS.bus",		PCI_SUBCLASS_SERIALBUS_ACCESS,		},
	{ "SSA",		PCI_SUBCLASS_SERIALBUS_SSA,		},
	{ "USB",		PCI_SUBCLASS_SERIALBUS_USB,		},
	{ "Fiber Channel",	PCI_SUBCLASS_SERIALBUS_FIBER,		},
	{ 0 },
};

struct pci_class pci_class[] = {
	{ "prehistoric",	PCI_CLASS_PREHISTORIC,
	    pci_subclass_prehistoric,				},
	{ "mass storage",	PCI_CLASS_MASS_STORAGE,
	    pci_subclass_mass_storage,				},
	{ "network",		PCI_CLASS_NETWORK,
	    pci_subclass_network,				},
	{ "display",		PCI_CLASS_DISPLAY,
	    pci_subclass_display,				},
	{ "multimedia",		PCI_CLASS_MULTIMEDIA,
	    pci_subclass_multimedia,				},
	{ "memory",		PCI_CLASS_MEMORY,
	    pci_subclass_memory,				},
	{ "bridge",		PCI_CLASS_BRIDGE,
	    pci_subclass_bridge,				},
	{ "communications",	PCI_CLASS_COMMUNICATIONS,
	    pci_subclass_communications,			},
	{ "system",		PCI_CLASS_SYSTEM,
	    pci_subclass_system,				},
	{ "input",		PCI_CLASS_INPUT,
	    pci_subclass_input,					},
	{ "dock",		PCI_CLASS_DOCK,
	    pci_subclass_dock,					},
	{ "processor",		PCI_CLASS_PROCESSOR,
	    pci_subclass_processor,				},
	{ "serial bus",		PCI_CLASS_SERIALBUS,
	    pci_subclass_serialbus,				},
	{ "undefined",		PCI_CLASS_UNDEFINED,
	    0,							},
	{ 0 },
};

#ifdef PCIVERBOSE
/*
 * Descriptions of of known vendors and devices ("products").
 */
struct pci_knowndev {
	pci_vendor_id_t		vendor;
	pci_product_id_t	product;
	int			flags;
	char			*vendorname, *productname;
};
#define	PCI_KNOWNDEV_NOPROD	0x01		/* match on vendor only */

#include <dev/pci/pcidevs_data.h>
#endif /* PCIVERBOSE */

void
pci_devinfo(id_reg, class_reg, showclass, cp)
	pcireg_t id_reg, class_reg;
	int showclass;
	char *cp;
{
	pci_vendor_id_t vendor;
	pci_product_id_t product;
	pci_class_t class;
	pci_subclass_t subclass;
	pci_interface_t interface;
	pci_revision_t revision;
	char *vendor_namep, *product_namep;
	struct pci_class *classp, *subclassp;
#ifdef PCIVERBOSE
	struct pci_knowndev *kdp;
	const char *unmatched = "unknown ";
#else
	const char *unmatched = "";
#endif

	vendor = PCI_VENDOR(id_reg);
	product = PCI_PRODUCT(id_reg);

	class = PCI_CLASS(class_reg);
	subclass = PCI_SUBCLASS(class_reg);
	interface = PCI_INTERFACE(class_reg);
	revision = PCI_REVISION(class_reg);

#ifdef PCIVERBOSE
	kdp = pci_knowndevs;
        while (kdp->vendorname != NULL) {	/* all have vendor name */
                if (kdp->vendor == vendor && (kdp->product == product ||
		    (kdp->flags & PCI_KNOWNDEV_NOPROD) != 0))
                        break;
		kdp++;
	}
        if (kdp->vendorname == NULL)
		vendor_namep = product_namep = NULL;
	else {
		vendor_namep = kdp->vendorname;
		product_namep = (kdp->flags & PCI_KNOWNDEV_NOPROD) == 0 ?
		    kdp->productname : NULL;
        }
#else /* PCIVERBOSE */
	vendor_namep = product_namep = NULL;
#endif /* PCIVERBOSE */

	classp = pci_class;
	while (classp->name != NULL) {
		if (class == classp->val)
			break;
		classp++;
	}

	subclassp = (classp->name != NULL) ? classp->subclasses : NULL;
	while (subclassp && subclassp->name != NULL) {
		if (subclass == subclassp->val)
			break;
		subclassp++;
	}

	if (vendor_namep == NULL)
		cp += sprintf(cp, "%svendor 0x%04x product 0x%04x",
		    unmatched, vendor, product);
	else if (product_namep != NULL)
		cp += sprintf(cp, "\"%s %s\"", vendor_namep, product_namep);
	else
		cp += sprintf(cp, "vendor \"%s\", unknown product 0x%x",
		    vendor_namep, product);
	if (showclass && product_namep == NULL) {
		cp += sprintf(cp, " (");
		if (classp->name == NULL)
			cp += sprintf(cp,
			    "unknown class 0x%2x, subclass 0x%02x",
			    class, subclass);
		else {
			cp += sprintf(cp, "class %s, ", classp->name);
			if (subclassp == NULL || subclassp->name == NULL)
				cp += sprintf(cp, "unknown subclass 0x%02x",
				    subclass);
			else
				cp += sprintf(cp, "subclass %s",
				    subclassp->name);
		}
#if 0 /* not very useful */
		cp += sprintf(cp, ", interface 0x%02x", interface);
#endif
		cp += sprintf(cp, ", rev 0x%02x)", revision);
	} else
		cp += sprintf(cp, " rev 0x%02x", revision);
}
