/*	$OpenBSD: pci.h,v 1.1 2019/04/14 10:14:53 jsg Exp $	*/
/*
 * Copyright (c) 2015 Mark Kettenis
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

#ifndef _LINUX_PCI_H
#define _LINUX_PCI_H

#include <sys/types.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
/* sparc64 cpu.h needs time.h and siginfo.h (indirect via param.h) */
#include <sys/param.h>
#include <machine/cpu.h>
#include <uvm/uvm_extern.h>

#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kobject.h>

struct pci_dev;

struct pci_bus {
	pci_chipset_tag_t pc;
	unsigned char	number;
	pcitag_t	*bridgetag;
	struct pci_dev	*self;
};

struct pci_dev {
	struct pci_bus	_bus;
	struct pci_bus	*bus;

	unsigned int	devfn;
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subsystem_vendor;
	uint16_t	subsystem_device;
	uint8_t		revision;

	pci_chipset_tag_t pc;
	pcitag_t	tag;
	struct pci_softc *pci;

	int		irq;
	int		msi_enabled;
	uint8_t		no_64bit_msi;
};
#define PCI_ANY_ID (uint16_t) (~0U)

#define PCI_VENDOR_ID_APPLE	PCI_VENDOR_APPLE
#define PCI_VENDOR_ID_ASUSTEK	PCI_VENDOR_ASUSTEK
#define PCI_VENDOR_ID_ATI	PCI_VENDOR_ATI
#define PCI_VENDOR_ID_DELL	PCI_VENDOR_DELL
#define PCI_VENDOR_ID_HP	PCI_VENDOR_HP
#define PCI_VENDOR_ID_IBM	PCI_VENDOR_IBM
#define PCI_VENDOR_ID_INTEL	PCI_VENDOR_INTEL
#define PCI_VENDOR_ID_SONY	PCI_VENDOR_SONY
#define PCI_VENDOR_ID_VIA	PCI_VENDOR_VIATECH

#define PCI_DEVICE_ID_ATI_RADEON_QY	PCI_PRODUCT_ATI_RADEON_QY

#define PCI_SUBVENDOR_ID_REDHAT_QUMRANET	0x1af4
#define PCI_SUBDEVICE_ID_QEMU			0x1100

#define PCI_DEVFN(slot, func)	((slot) << 3 | (func))
#define PCI_SLOT(devfn)		((devfn) >> 3)
#define PCI_FUNC(devfn)		((devfn) & 0x7)

#define pci_dev_put(x)

#define PCI_EXP_DEVSTA		0x0a
#define PCI_EXP_DEVSTA_TRPND	0x0020
#define PCI_EXP_LNKCAP		0x0c
#define PCI_EXP_LNKCAP_CLKPM	0x00040000
#define PCI_EXP_LNKCTL		0x10
#define PCI_EXP_LNKCTL_HAWD	0x0200
#define PCI_EXP_LNKCTL2		0x30

#define PCI_COMMAND		PCI_COMMAND_STATUS_REG
#define PCI_COMMAND_MEMORY	PCI_COMMAND_MEM_ENABLE

static inline int
pci_read_config_dword(struct pci_dev *pdev, int reg, u32 *val)
{
	*val = pci_conf_read(pdev->pc, pdev->tag, reg);
	return 0;
} 

static inline int
pci_read_config_word(struct pci_dev *pdev, int reg, u16 *val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x2));
	*val = (v >> ((reg & 0x2) * 8));
	return 0;
} 

static inline int
pci_read_config_byte(struct pci_dev *pdev, int reg, u8 *val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x3));
	*val = (v >> ((reg & 0x3) * 8));
	return 0;
} 

static inline int
pci_write_config_dword(struct pci_dev *pdev, int reg, u32 val)
{
	pci_conf_write(pdev->pc, pdev->tag, reg, val);
	return 0;
} 

static inline int
pci_write_config_word(struct pci_dev *pdev, int reg, u16 val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x2));
	v &= ~(0xffff << ((reg & 0x2) * 8));
	v |= (val << ((reg & 0x2) * 8));
	pci_conf_write(pdev->pc, pdev->tag, (reg & ~0x2), v);
	return 0;
} 

static inline int
pci_write_config_byte(struct pci_dev *pdev, int reg, u8 val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x3));
	v &= ~(0xff << ((reg & 0x3) * 8));
	v |= (val << ((reg & 0x3) * 8));
	pci_conf_write(pdev->pc, pdev->tag, (reg & ~0x3), v);
	return 0;
}

static inline int
pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
    int reg, u16 *val)
{
	pcitag_t tag = pci_make_tag(bus->pc, bus->number,
	    PCI_SLOT(devfn), PCI_FUNC(devfn));
	uint32_t v;

	v = pci_conf_read(bus->pc, tag, (reg & ~0x2));
	*val = (v >> ((reg & 0x2) * 8));
	return 0;
}

static inline int
pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
    int reg, u8 *val)
{
	pcitag_t tag = pci_make_tag(bus->pc, bus->number,
	    PCI_SLOT(devfn), PCI_FUNC(devfn));
	uint32_t v;

	v = pci_conf_read(bus->pc, tag, (reg & ~0x3));
	*val = (v >> ((reg & 0x3) * 8));
	return 0;
}

static inline int
pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn,
    int reg, u8 val)
{
	pcitag_t tag = pci_make_tag(bus->pc, bus->number,
	    PCI_SLOT(devfn), PCI_FUNC(devfn));
	uint32_t v;

	v = pci_conf_read(bus->pc, tag, (reg & ~0x3));
	v &= ~(0xff << ((reg & 0x3) * 8));
	v |= (val << ((reg & 0x3) * 8));
	pci_conf_write(bus->pc, tag, (reg & ~0x3), v);
	return 0;
}

static inline int
pci_pcie_cap(struct pci_dev *pdev)
{
	int pos;
	if (!pci_get_capability(pdev->pc, pdev->tag, PCI_CAP_PCIEXPRESS,
	    &pos, NULL))
		return -EINVAL;
	return pos;
}

static inline bool
pci_is_root_bus(struct pci_bus *pbus)
{
	return (pbus->bridgetag == NULL);
}

static inline int
pcie_capability_read_dword(struct pci_dev *pdev, int off, u32 *val)
{
	int pos;
	if (!pci_get_capability(pdev->pc, pdev->tag, PCI_CAP_PCIEXPRESS,
	    &pos, NULL)) {
		*val = 0;
		return -EINVAL;
	}
	*val = pci_conf_read(pdev->pc, pdev->tag, pos + off);
	return 0;
}

#define pci_set_master(x)
#define pci_clear_master(x)

#define pci_save_state(x)
#define pci_restore_state(x)

#define pci_enable_msi(x)	0
#define pci_disable_msi(x)

typedef enum {
	PCI_D0,
	PCI_D1,
	PCI_D2,
	PCI_D3hot,
	PCI_D3cold
} pci_power_t;

enum pci_bus_speed {
	PCIE_SPEED_2_5GT,
	PCIE_SPEED_5_0GT,
	PCIE_SPEED_8_0GT,
	PCIE_SPEED_16_0GT,
	PCI_SPEED_UNKNOWN
};

enum pcie_link_width {
	PCIE_LNK_X1	= 1,
	PCIE_LNK_X2	= 2,
	PCIE_LNK_X4	= 4,
	PCIE_LNK_X8	= 8,
	PCIE_LNK_X12	= 12,
	PCIE_LNK_X16	= 16,
	PCIE_LNK_X32	= 32,
	PCIE_LNK_WIDTH_UNKNOWN	= 0xff
};

enum pci_bus_speed pcie_get_speed_cap(struct pci_dev *);
enum pcie_link_width pcie_get_width_cap(struct pci_dev *);
int pci_resize_resource(struct pci_dev *, int, int);

#define pci_save_state(x)
#define pci_enable_device(x)		0
#define pci_disable_device(x)
#define pci_is_thunderbolt_attached(x) false
#define pci_set_drvdata(x, y)

static inline int
pci_set_power_state(struct pci_dev *dev, int state)
{
	return 0;
}

#if defined(__amd64__) || defined(__i386__)

#define PCI_DMA_BIDIRECTIONAL	0

static inline dma_addr_t
pci_map_page(struct pci_dev *pdev, struct vm_page *page, unsigned long offset, size_t size, int direction)
{
	return VM_PAGE_TO_PHYS(page);
}

static inline void
pci_unmap_page(struct pci_dev *pdev, dma_addr_t dma_address, size_t size, int direction)
{
}

static inline int
pci_dma_mapping_error(struct pci_dev *pdev, dma_addr_t dma_addr)
{
	return 0;
}

#define pci_set_dma_mask(x, y)			0
#define pci_set_consistent_dma_mask(x, y)	0

#endif /* defined(__amd64__) || defined(__i386__) */

#endif
