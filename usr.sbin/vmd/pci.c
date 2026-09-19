/*	$OpenBSD: pci.c,v 1.42 2026/09/19 17:21:52 dv Exp $	*/

/*
 * Copyright (c) 2015 Mike Larkin <mlarkin@openbsd.org>
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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/vmm/vmm.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "vmd.h"
#include "pci.h"
#include "atomicio.h"
#include "mmio.h"

struct pci pci;

extern struct vmd_vm *current_vm;

#define PCI_MSI_ADDR_BASE		0xfee00000ULL
#define PCI_MSI_ADDR_MASK		0xfff00000ULL
#define PCI_MSI_ADDR_DEST_SHIFT		12
#define PCI_MSI_ADDR_DEST_MASK		0xff
#define PCI_MSI_ADDR_DESTMODE		(1ULL << 2)
#define PCI_MSI_DATA_VECTOR_MASK	0xff
#define PCI_MSI_DATA_DELIVERY_SHIFT	8
#define PCI_MSI_DATA_DELIVERY_MASK	0x7
#define PCI_MSI_DELIVERY_FIXED		0

struct pci_msi_cap {
	uint8_t pmc_id;
	uint8_t pmc_next;
	uint16_t pmc_control;
	uint32_t pmc_address;
	uint32_t pmc_address_hi;
	uint16_t pmc_data;
	uint16_t pmc_reserved;
} __packed;

struct pci_msix_cap {
	uint8_t pmc_id;
	uint8_t pmc_next;
	uint16_t pmc_control;
	uint32_t pmc_table;
	uint32_t pmc_pba;
} __packed;

static int pci_msix_mmio(uint32_t, int, uint32_t, uint8_t, uint64_t *,
    void *);
static void pci_msi_deliver(uint64_t, uint32_t);
static int pci_msi_enabled(struct pci_dev *);
static int pci_msix_enabled(struct pci_dev *);
static void pci_config_write(struct pci_dev *, uint8_t, uint8_t, uint8_t,
    uint32_t);
static void pci_msix_drain(struct pci_dev *);

/* PIC IRQs, assigned to devices in order */
const uint8_t pci_pic_irqs[PCI_MAX_PIC_IRQS] = {3, 5, 6, 7, 9, 10, 11, 12,
    14, 15};

/*
 * pci_add_bar
 *
 * Adds a BAR for the PCI device 'id'. On access, 'barfn' will be
 * called, and passed 'cookie' as an identifier.
 *
 * BARs are fixed size, meaning all I/O BARs requested have the
 * same size and all MMIO BARs have the same size.
 *
 * Parameters:
 *  id: PCI device to add the BAR to (local count, eg if id == 4,
 *      this BAR is to be added to the VM's 5th PCI device)
 *  type: type of the BAR to add (PCI_MAPREG_TYPE_xxx)
 *  barfn: callback function invoked on BAR access
 *  cookie: cookie passed to barfn on access
 *
 * Returns the index of the BAR if added successfully, -1 otherwise.
 */
int
pci_add_bar(uint8_t id, uint32_t type, void *barfn, void *cookie)
{
	uint8_t bar_reg_idx, bar_ct;

	/* Check id */
	if (id >= pci.pci_dev_ct)
		return (-1);

	/* Can only add PCI_MAX_BARS BARs to any device */
	bar_ct = pci.pci_devices[id].pd_bar_ct;
	if (bar_ct >= PCI_MAX_BARS)
		return (-1);

	/* Compute BAR address and add */
	bar_reg_idx = (PCI_MAPREG_START + (bar_ct * 4)) / 4;
	if (type == PCI_MAPREG_TYPE_MEM) {
		if (pci.pci_next_mmio_bar >= PCI_MMIO_BAR_END)
			return (-1);

		pci.pci_devices[id].pd_cfg_space[bar_reg_idx] =
		    PCI_MAPREG_MEM_ADDR(pci.pci_next_mmio_bar);
		pci.pci_next_mmio_bar += VM_PCI_MMIO_BAR_SIZE;
		pci.pci_devices[id].pd_barfunc[bar_ct] = barfn;
		pci.pci_devices[id].pd_bar_cookie[bar_ct] = cookie;
		pci.pci_devices[id].pd_bartype[bar_ct] = PCI_BAR_TYPE_MMIO;
		pci.pci_devices[id].pd_barsize[bar_ct] = VM_PCI_MMIO_BAR_SIZE;
		pci.pci_devices[id].pd_bar_ct++;
	}
#ifdef __amd64__
	else if (type == PCI_MAPREG_TYPE_IO) {
		if (pci.pci_next_io_bar >= VM_PCI_IO_BAR_END)
			return (-1);

		pci.pci_devices[id].pd_cfg_space[bar_reg_idx] =
		    PCI_MAPREG_IO_ADDR(pci.pci_next_io_bar) |
		    PCI_MAPREG_TYPE_IO;
		pci.pci_next_io_bar += VM_PCI_IO_BAR_SIZE;
		pci.pci_devices[id].pd_barfunc[bar_ct] = barfn;
		pci.pci_devices[id].pd_bar_cookie[bar_ct] = cookie;
		DPRINTF("adding pci bar cookie for dev %d bar %d = %p", id,
		    bar_ct, cookie);
		pci.pci_devices[id].pd_bartype[bar_ct] = PCI_BAR_TYPE_IO;
		pci.pci_devices[id].pd_barsize[bar_ct] = VM_PCI_IO_BAR_SIZE;
		pci.pci_devices[id].pd_bar_ct++;
	}
#endif /* __amd64__ */

	return ((int)bar_ct);
}

int
pci_set_bar_fn(uint8_t id, uint8_t bar_ct, void *barfn, void *cookie)
{
	/* Check id */
	if (id >= pci.pci_dev_ct)
		return (1);

	if (bar_ct >= PCI_MAX_BARS)
		return (1);

	pci.pci_devices[id].pd_barfunc[bar_ct] = barfn;
	pci.pci_devices[id].pd_bar_cookie[bar_ct] = cookie;

	return (0);
}

/*
 * pci_get_dev_irq
 *
 * Returns the IRQ for the specified PCI device
 *
 * Parameters:
 *  id: PCI device id to return IRQ for
 *
 * Return values:
 *  The IRQ for the device, or 0xff if no device IRQ assigned
 */
uint8_t
pci_get_dev_irq(uint8_t id)
{
	if (pci.pci_devices[id].pd_int)
		return pci.pci_devices[id].pd_irq;
	else
		return 0xFF;
}

/*
 * pci_add_device
 *
 * Adds a PCI device to the guest VM defined by the supplied parameters.
 *
 * Parameters:
 *  id: the new PCI device ID (0 .. PCI_CONFIG_MAX_DEV)
 *  vid: PCI VID of the new device
 *  pid: PCI PID of the new device
 *  class: PCI 'class' of the new device
 *  subclass: PCI 'subclass' of the new device
 *  subsys_vid: subsystem VID of the new device
 *  subsys_id: subsystem ID of the new device
 *  rev_id: revision id
 *  irq_needed: 1 if an IRQ should be assigned to this PCI device, 0 otherwise
 *  csfunc: PCI config space callback function when the guest VM accesses
 *      CS of this PCI device
 *
 * Return values:
 *  0: the PCI device was added successfully. The PCI device ID is in 'id'.
 *  1: the PCI device addition failed.
 */
int
pci_add_device(uint8_t *id, uint16_t vid, uint16_t pid, uint8_t class,
    uint8_t subclass, uint16_t subsys_vid, uint16_t subsys_id,
    uint8_t rev_id, uint8_t irq_needed, pci_cs_fn_t csfunc)
{
	int ret;

	/* Exceeded max devices? */
	if (pci.pci_dev_ct >= PCI_CONFIG_MAX_DEV)
		return (1);

	/* Exceeded max IRQs? */
	/* XXX we could share IRQs ... */
	if (pci.pci_next_pic_irq >= PCI_MAX_PIC_IRQS && irq_needed)
		return (1);

	*id = pci.pci_dev_ct;
	ret = pthread_mutex_init(&pci.pci_devices[*id].pd_mtx, NULL);
	if (ret != 0) {
		errno = ret;
		log_warn("can't initialize PCI device mutex");
		return (1);
	}

	pci.pci_devices[*id].pd_vid = vid;
	pci.pci_devices[*id].pd_did = pid;
	pci.pci_devices[*id].pd_rev = rev_id;
	pci.pci_devices[*id].pd_class = class;
	pci.pci_devices[*id].pd_subclass = subclass;
	pci.pci_devices[*id].pd_subsys_vid = subsys_vid;
	pci.pci_devices[*id].pd_subsys_id = subsys_id;

	pci.pci_devices[*id].pd_csfunc = csfunc;

	if (irq_needed) {
		pci.pci_devices[*id].pd_irq =
		    pci_pic_irqs[pci.pci_next_pic_irq];
		pci.pci_devices[*id].pd_int = 1;
		pci.pci_next_pic_irq++;
		DPRINTF("assigned irq %d to pci dev %d",
		    pci.pci_devices[*id].pd_irq, *id);
		intr_toggle_el(current_vm, pci.pci_devices[*id].pd_irq, 1);
	}

	pci.pci_dev_ct++;

	return (0);
}

int
pci_add_capability(uint8_t id, struct pci_cap *cap)
{
	uint8_t cid;
	struct pci_dev *dev = NULL;

	if (id >= pci.pci_dev_ct)
		return (-1);
	dev = &pci.pci_devices[id];

	if (dev->pd_cap_ct >= PCI_MAX_CAPS)
		return (-1);
	cid = dev->pd_cap_ct;

	memcpy(&dev->pd_caps[cid], cap, sizeof(dev->pd_caps[0]));

	/* Update the linkage. */
	if (cid > 0)
		dev->pd_caps[cid - 1].pc_next = (sizeof(struct pci_cap) * cid) +
		    offsetof(struct pci_dev, pd_caps);

	dev->pd_cap_ct++;
	dev->pd_cap = offsetof(struct pci_dev, pd_caps);
	dev->pd_status |= (PCI_STATUS_CAPLIST_SUPPORT >> 16);

	return (cid);
}

int
pci_add_msi_capability(uint8_t id)
{
	struct pci_dev *dev;
	struct pci_cap storage;
	struct pci_msi_cap cap;
	int cid;

	if (id >= pci.pci_dev_ct)
		return (-1);
	dev = &pci.pci_devices[id];
	if (dev->pd_msi_cap != 0)
		return (-1);

	memset(&cap, 0, sizeof(cap));
	cap.pmc_id = PCI_CAP_MSI;
	/* A 64-bit address keeps the capability layout uniform. */
	cap.pmc_control = PCI_MSI_MC_C64 >> 16;
	memset(&storage, 0, sizeof(storage));
	memcpy(&storage, &cap, sizeof(cap));
	cid = pci_add_capability(id, &storage);
	if (cid == -1)
		return (-1);
	dev->pd_msi_cap = offsetof(struct pci_dev, pd_caps) +
	    cid * sizeof(struct pci_cap);

	return (0);
}

int
pci_add_msix_capability(uint8_t id, uint16_t nvec)
{
	struct pci_dev *dev;
	struct pci_cap storage;
	struct pci_msix_cap cap;
	int bar, cid;

	if (id >= pci.pci_dev_ct || nvec == 0 ||
	    nvec > PCI_MSIX_MAX_VECTORS)
		return (-1);
	dev = &pci.pci_devices[id];
	if (dev->pd_msix_cap != 0)
		return (-1);

	bar = pci_add_bar(id, PCI_MAPREG_TYPE_MEM, pci_msix_mmio, dev);
	if (bar == -1)
		return (-1);

	memset(&cap, 0, sizeof(cap));
	cap.pmc_id = PCI_CAP_MSIX;
	cap.pmc_control = nvec - 1;
	cap.pmc_table = PCI_MSIX_TABLE_OFFSET | bar;
	cap.pmc_pba = PCI_MSIX_PBA_OFFSET | bar;
	memset(&storage, 0, sizeof(storage));
	memcpy(&storage, &cap, sizeof(cap));
	cid = pci_add_capability(id, &storage);
	if (cid == -1)
		return (-1);

	dev->pd_msix_cap = offsetof(struct pci_dev, pd_caps) +
	    cid * sizeof(struct pci_cap);
	dev->pd_msix_nvec = nvec;
	/* MSI-X vectors begin masked until the guest programs the table. */
	for (cid = 0; cid < nvec; cid++)
		dev->pd_msix_table[cid].pme_vector_control =
		    PCI_MSIX_VC_MASK;

	return (0);
}

static int
pci_msi_enabled(struct pci_dev *dev)
{
	uint16_t control;

	if (dev->pd_msi_cap == 0)
		return (0);
	memcpy(&control, (uint8_t *)dev->pd_cfg_space + dev->pd_msi_cap + 2,
	    sizeof(control));
	return ((control & (PCI_MSI_MC_MSIE >> 16)) != 0);
}

static int
pci_msix_enabled(struct pci_dev *dev)
{
	uint16_t control;

	if (dev->pd_msix_cap == 0)
		return (0);
	memcpy(&control, (uint8_t *)dev->pd_cfg_space + dev->pd_msix_cap + 2,
	    sizeof(control));
	return ((control & (PCI_MSIX_MC_MSIXE >> 16)) != 0);
}

static void
pci_msi_deliver(uint64_t address, uint32_t data)
{
	uint32_t dest;
	uint8_t delivery, vector;

	if ((address & PCI_MSI_ADDR_MASK) != PCI_MSI_ADDR_BASE ||
	    (address >> 32) != 0) {
		log_debug("%s: unsupported MSI address 0x%llx", __func__,
		    address);
		return;
	}
	if (address & PCI_MSI_ADDR_DESTMODE) {
		log_debug("%s: logical MSI destination unsupported", __func__);
		return;
	}

	delivery = (data >> PCI_MSI_DATA_DELIVERY_SHIFT) &
	    PCI_MSI_DATA_DELIVERY_MASK;
	if (delivery != PCI_MSI_DELIVERY_FIXED) {
		log_debug("%s: MSI delivery mode %u unsupported", __func__,
		    delivery);
		return;
	}

	dest = (address >> PCI_MSI_ADDR_DEST_SHIFT) &
	    PCI_MSI_ADDR_DEST_MASK;
	vector = data & PCI_MSI_DATA_VECTOR_MASK;
	vcpu_assert_vector(current_vm->vm_fd, dest, vector);
}

void
pci_assert_irq(uint8_t id, uint16_t msix_vector)
{
	struct pci_dev *dev;
	uint64_t address = 0;
	uint32_t data = 0;
	uint16_t control;
	int legacy = 0, deliver = 0;

	if (id >= pci.pci_dev_ct)
		return;
	dev = &pci.pci_devices[id];

	pthread_mutex_lock(&dev->pd_mtx);
	if (pci_msix_enabled(dev)) {
		memcpy(&control, (uint8_t *)dev->pd_cfg_space +
		    dev->pd_msix_cap + 2, sizeof(control));
		if (msix_vector == UINT16_MAX ||
		    msix_vector >= dev->pd_msix_nvec) {
			log_debug("%s: PCI device %u has no MSI-X vector",
			    __func__, id);
		} else if ((control & (PCI_MSIX_MC_FM >> 16)) ||
		    (dev->pd_msix_table[msix_vector].pme_vector_control &
		    PCI_MSIX_VC_MASK)) {
			dev->pd_msix_pba |= 1ULL << msix_vector;
		} else {
			address = dev->pd_msix_table[msix_vector].pme_addr;
			data = dev->pd_msix_table[msix_vector].pme_data;
			deliver = 1;
		}
	} else if (pci_msi_enabled(dev)) {
		memcpy(&address, (uint8_t *)dev->pd_cfg_space +
		    dev->pd_msi_cap + PCI_MSI_MA, sizeof(address));
		memcpy(&control, (uint8_t *)dev->pd_cfg_space +
		    dev->pd_msi_cap + PCI_MSI_MD64, sizeof(control));
		data = control;
		deliver = 1;
	} else
		legacy = 1;
	pthread_mutex_unlock(&dev->pd_mtx);

	if (deliver)
		pci_msi_deliver(address, data);
	else if (legacy && dev->pd_int)
		vcpu_assert_irq(current_vm->vm_fd, 0, dev->pd_irq);
}

void
pci_deassert_irq(uint8_t id)
{
	struct pci_dev *dev;
	int legacy;

	if (id >= pci.pci_dev_ct)
		return;
	dev = &pci.pci_devices[id];

	pthread_mutex_lock(&dev->pd_mtx);
	legacy = !pci_msix_enabled(dev) && !pci_msi_enabled(dev);
	pthread_mutex_unlock(&dev->pd_mtx);

	if (legacy && dev->pd_int)
		vcpu_deassert_irq(current_vm->vm_fd, 0, dev->pd_irq);
}

static void
pci_msix_drain(struct pci_dev *dev)
{
	uint64_t address;
	uint32_t data;
	uint16_t control;
	int i, deliver;

	for (i = 0; i < dev->pd_msix_nvec; i++) {
		deliver = 0;
		pthread_mutex_lock(&dev->pd_mtx);
		memcpy(&control, (uint8_t *)dev->pd_cfg_space +
		    dev->pd_msix_cap + 2, sizeof(control));
		if ((control & (PCI_MSIX_MC_MSIXE >> 16)) &&
		    !(control & (PCI_MSIX_MC_FM >> 16)) &&
		    !(dev->pd_msix_table[i].pme_vector_control &
		    PCI_MSIX_VC_MASK) && (dev->pd_msix_pba & (1ULL << i))) {
			dev->pd_msix_pba &= ~(1ULL << i);
			address = dev->pd_msix_table[i].pme_addr;
			data = dev->pd_msix_table[i].pme_data;
			deliver = 1;
		}
		pthread_mutex_unlock(&dev->pd_mtx);
		if (deliver)
			pci_msi_deliver(address, data);
	}
}

static int
pci_msix_mmio(uint32_t vcpu_id, int dir, uint32_t ofs, uint8_t size,
    uint64_t *data, void *cookie)
{
	struct pci_dev *dev = cookie;
	struct pci_msix_entry *entry;
	uint32_t reg = 0;
	uint16_t vector;
	int drain = 0;

	(void)vcpu_id;
	(void)size;
	if (ofs < dev->pd_msix_nvec * sizeof(*entry)) {
		vector = ofs / sizeof(*entry);
		if ((ofs & 3) != 0)
			return (EINVAL);
		entry = &dev->pd_msix_table[vector];

		pthread_mutex_lock(&dev->pd_mtx);
		switch (ofs & 0xf) {
		case PCI_MSIX_MA(0):
			reg = entry->pme_addr;
			if (dir == MMIO_DIR_WRITE) {
				entry->pme_addr &= 0xffffffff00000000ULL;
				entry->pme_addr |= (uint32_t)*data;
			}
			break;
		case PCI_MSIX_MAU32(0):
			reg = entry->pme_addr >> 32;
			if (dir == MMIO_DIR_WRITE) {
				entry->pme_addr &= 0x00000000ffffffffULL;
				entry->pme_addr |= (uint64_t)(uint32_t)*data << 32;
			}
			break;
		case PCI_MSIX_MD(0):
			reg = entry->pme_data;
			if (dir == MMIO_DIR_WRITE)
				entry->pme_data = (uint32_t)*data;
			break;
		case PCI_MSIX_VC(0):
			reg = entry->pme_vector_control;
			if (dir == MMIO_DIR_WRITE) {
				entry->pme_vector_control =
				    (uint32_t)*data & PCI_MSIX_VC_MASK;
				if (!(entry->pme_vector_control & PCI_MSIX_VC_MASK))
					drain = 1;
			}
			break;
		}
		if (dir == MMIO_DIR_READ) {
			*data &= 0xffffffff00000000ULL;
			*data |= reg;
		}
		pthread_mutex_unlock(&dev->pd_mtx);
		if (drain)
			pci_msix_drain(dev);
		return (0);
	}

	if (ofs == PCI_MSIX_PBA_OFFSET || ofs == PCI_MSIX_PBA_OFFSET + 4) {
		pthread_mutex_lock(&dev->pd_mtx);
		if (dir == MMIO_DIR_READ) {
			reg = ofs == PCI_MSIX_PBA_OFFSET ? dev->pd_msix_pba :
			    dev->pd_msix_pba >> 32;
			*data &= 0xffffffff00000000ULL;
			*data |= reg;
		}
		pthread_mutex_unlock(&dev->pd_mtx);
		return (0);
	}

	if (dir == MMIO_DIR_READ)
		*data = UINT64_MAX;
	return (0);
}

/*
 * pci_init
 *
 * Initializes the PCI subsystem for the VM by adding a PCI host bridge
 * as the first PCI device.
 */
void
pci_init(void)
{
	uint8_t id;

	memset(&pci, 0, sizeof(pci));

	/* Check if changes to struct pci_dev create an invalid config space. */
	CTASSERT(sizeof(pci.pci_devices[0].pd_cfg_space) <= 256);

	pci.pci_next_mmio_bar = PCI_MMIO_BAR_BASE;
#ifdef __amd64__
	pci.pci_next_io_bar = VM_PCI_IO_BAR_BASE;
#endif /* __amd64__ */

	if (pci_add_device(&id, PCI_VENDOR_OPENBSD, PCI_PRODUCT_OPENBSD_PCHB,
	    PCI_CLASS_BRIDGE, PCI_SUBCLASS_BRIDGE_HOST,
	    PCI_VENDOR_OPENBSD, 0, 0, 0, NULL)) {
		log_warnx("can't add PCI host bridge");
		return;
	}
}

#ifdef __amd64__
int
pci_handle_mmio(uint32_t vcpu_id, int dir, paddr_t addr, uint8_t size,
    uint64_t *data)
{
	struct pci_dev *dev;
	pci_mmiobar_fn_t fn;
	uint64_t base;
	int i, j;

	for (i = 0; i < pci.pci_dev_ct; i++) {
		dev = &pci.pci_devices[i];
		for (j = 0; j < dev->pd_bar_ct; j++) {
			if (dev->pd_bartype[j] != PCI_BAR_TYPE_MMIO)
				continue;
			base = PCI_MAPREG_MEM_ADDR(dev->pd_bar[j]);
			if (addr < base || addr >= base + dev->pd_barsize[j])
				continue;
			fn = (pci_mmiobar_fn_t)dev->pd_barfunc[j];
			if (fn == NULL)
				break;
			return (fn(vcpu_id, dir, addr - base, size, data,
			    dev->pd_bar_cookie[j]));
		}
	}

	if (dir == MMIO_DIR_READ)
		*data = UINT64_MAX;
	return (0);
}

static void
pci_config_write(struct pci_dev *dev, uint8_t reg, uint8_t ofs, uint8_t sz,
    uint32_t data)
{
	uint32_t old, val, mask, fixed;
	uint8_t baridx;
	int drain = 0;

	if (sz != 1 && sz != 2 && sz != 4)
		return;
	if (ofs + sz > 4)
		return;

	pthread_mutex_lock(&dev->pd_mtx);
	old = dev->pd_cfg_space[reg];
	if (sz == 4)
		mask = UINT32_MAX;
	else
		mask = ((1U << (sz * 8)) - 1) << (ofs * 8);
	val = (old & ~mask) | ((data << (ofs * 8)) & mask);

	if (reg >= PCI_MAPREG_START / 4 && reg < PCI_MAPREG_END / 4) {
		baridx = reg - PCI_MAPREG_START / 4;
		if (baridx >= dev->pd_bar_ct)
			val = 0;
		else if (sz == 4 && data == UINT32_MAX) {
			val = ~(dev->pd_barsize[baridx] - 1);
			if (dev->pd_bartype[baridx] == PCI_BAR_TYPE_IO)
				val = (val & PCI_MAPREG_IO_ADDR_MASK) |
				    PCI_MAPREG_TYPE_IO;
			else
				val &= PCI_MAPREG_MEM_ADDR_MASK;
		} else if (dev->pd_bartype[baridx] == PCI_BAR_TYPE_IO) {
			val &= ~(dev->pd_barsize[baridx] - 1);
			val |= PCI_MAPREG_TYPE_IO;
		} else {
			val &= ~(dev->pd_barsize[baridx] - 1);
			val &= PCI_MAPREG_MEM_ADDR_MASK;
		}
	}

	if (dev->pd_msi_cap != 0 && reg == dev->pd_msi_cap / 4) {
		fixed = old & ~PCI_MSI_MC_MSIE;
		val = fixed | (val & PCI_MSI_MC_MSIE);
		if (val & PCI_MSI_MC_MSIE) {
			if (dev->pd_msix_cap != 0)
				dev->pd_cfg_space[dev->pd_msix_cap / 4] &=
				    ~PCI_MSIX_MC_MSIXE;
		}
	} else if (dev->pd_msi_cap != 0 &&
	    reg == (dev->pd_msi_cap + PCI_MSI_MD64) / 4) {
		val &= 0xffff;
	}

	if (dev->pd_msix_cap != 0 && reg == dev->pd_msix_cap / 4) {
		fixed = old & ~(PCI_MSIX_MC_MSIXE | PCI_MSIX_MC_FM);
		val = fixed | (val & (PCI_MSIX_MC_MSIXE | PCI_MSIX_MC_FM));
		if (val & PCI_MSIX_MC_MSIXE) {
			if (dev->pd_msi_cap != 0)
				dev->pd_cfg_space[dev->pd_msi_cap / 4] &=
				    ~PCI_MSI_MC_MSIE;
			drain = (val & PCI_MSIX_MC_FM) == 0;
		}
	} else if (dev->pd_msix_cap != 0 &&
	    (reg == (dev->pd_msix_cap + PCI_MSIX_TABLE) / 4 ||
	    reg == (dev->pd_msix_cap + 8) / 4))
		val = old;

	dev->pd_cfg_space[reg] = val;
	pthread_mutex_unlock(&dev->pd_mtx);

	if (drain)
		pci_msix_drain(dev);
}

void
pci_handle_address_reg(struct vm_run_params *vrp)
{
	struct vm_exit *vei = vrp->vrp_exit;

	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * The guest wrote to the address register.
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		get_input_data(vei, &pci.pci_addr_reg);
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * The guest read the address register
		 */
		set_return_data(vei, pci.pci_addr_reg);
	}
}

uint8_t
pci_handle_io(struct vm_run_params *vrp)
{
	int i, j;
	uint16_t reg, b_hi, b_lo;
	pci_iobar_fn_t fn = NULL;
	void *cookie = NULL;
	uint8_t intr = 0xFF, irq = 0xFF, dir, sz;
	struct vm_exit *vei = vrp->vrp_exit;

	reg = vei->vei.vei_port;
	dir = vei->vei.vei_dir;
	sz = vei->vei.vei_size;

	for (i = 0 ; i < pci.pci_dev_ct; i++) {
		for (j = 0 ; j < pci.pci_devices[i].pd_bar_ct; j++) {
			if (pci.pci_devices[i].pd_bartype[j] != PCI_BAR_TYPE_IO)
				continue;
			b_lo = PCI_MAPREG_IO_ADDR(pci.pci_devices[i].pd_bar[j]);
			b_hi = b_lo + VM_PCI_IO_BAR_SIZE;
			if (reg >= b_lo && reg < b_hi) {
				fn = pci.pci_devices[i].pd_barfunc[j];
				reg = reg - b_lo;
				cookie = pci.pci_devices[i].pd_bar_cookie[j];
				irq = pci.pci_devices[i].pd_irq;
				goto found;
			}
		}
	}
found:
	if (fn == NULL) {
		DPRINTF("no pci i/o function for reg 0x%llx (dir=%d guest "
		    "%%rip=0x%llx)", (uint64_t)reg, dir,
		    vei->vrs.vrs_gprs[VCPU_REGS_RIP]);
		/* Reads from undefined ports return 0xFF */
		if (dir == VEI_DIR_IN)
			set_return_data(vei, 0xFFFFFFFF);
		return (0xFF);
	}

	if (fn(dir, reg, &vei->vei.vei_data, &intr, cookie, sz))
		log_warnx("pci i/o access function failed");
	if (intr != 0xFF)
		intr = irq;

	return (intr);
}

void
pci_handle_data_reg(struct vm_run_params *vrp)
{
	struct vm_exit *vei = vrp->vrp_exit;
	struct pci_dev *pd = NULL;
	uint8_t b, d, f, o, cfgidx, ofs, sz;
	uint32_t data = 0;
	int ret;
	pci_cs_fn_t csfunc;

	/* abort if the address register is wack */
	if (!(pci.pci_addr_reg & PCI_MODE1_ENABLE)) {
		/* if read, return FFs */
		if (vei->vei.vei_dir == VEI_DIR_IN)
			set_return_data(vei, 0xFFFFFFFF);
		log_warnx("invalid address register during pci read: "
		    "0x%llx", (uint64_t)pci.pci_addr_reg);
		return;
	}

	/* I/Os to 0xCFC..0xCFF are permitted */
	ofs = vei->vei.vei_port - 0xCFC;
	sz = vei->vei.vei_size;

	b = (pci.pci_addr_reg >> 16) & 0xff;
	d = (pci.pci_addr_reg >> 11) & 0x1f;
	f = (pci.pci_addr_reg >> 8) & 0x7;
	o = (pci.pci_addr_reg & 0xfc);

	if (b != 0 || f != 0 || d >= pci.pci_dev_ct || ofs + sz > 4) {
		/* Device out of range. Return 0xFF's if a read. */
		DPRINTF("%s: invalid pci device access (%u)", __func__, d);
		if (vei->vei.vei_dir == VEI_DIR_IN)
			set_return_data(vei, 0xFFFFFFFF);
		return;
	}
	pd = &pci.pci_devices[d];

	cfgidx = (o / 4);
	if (cfgidx >= nitems(pd->pd_cfg_space)) {
		DPRINTF("%s: out of range config space access", __func__);
		if (vei->vei.vei_dir == VEI_DIR_IN)
			set_return_data(vei, 0xFFFFFFFF);
		return;
	}

	csfunc = pd->pd_csfunc;
	if (csfunc != NULL) {
		ret = csfunc(vei->vei.vei_dir, cfgidx, &vei->vei.vei_data);
		if (ret)
			log_warnx("cfg space access function failed for "
			    "pci device %d", d);
		return;
	}

	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * The guest wrote to the config space location denoted by the current
	 * value in the address register.
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		/*
		 * Discard writes to "option rom base address" as none of our
		 * emulated devices have PCI option roms.
		 */
		if (o != PCI_EXROMADDR_0)
			pci_config_write(pd, cfgidx, ofs, sz,
			    vei->vei.vei_data);
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * The guest read from the config space location determined by
		 * the current value in the address register.
		 */
		pthread_mutex_lock(&pd->pd_mtx);
		data = pd->pd_cfg_space[cfgidx];
		pthread_mutex_unlock(&pd->pd_mtx);
		set_return_data(vei, data >> (ofs * 8));
	}
}
#endif /* __amd64__ */

/*
 * Find the first PCI device based on PCI Subsystem ID
 * (e.g. PCI_PRODUCT_VIRTIO_BLOCK).
 *
 * Returns the PCI device id of the first matching device, if found.
 * Otherwise, returns -1.
 */
int
pci_find_first_device(uint16_t subsys_id)
{
	int i;

	for (i = 0; i < pci.pci_dev_ct; i++)
		if (pci.pci_devices[i].pd_subsys_id == subsys_id)
			return (i);
	return (-1);
}

/*
 * Retrieve the subsystem identifier for a PCI device if found, otherwise 0.
 */
uint16_t
pci_get_subsys_id(uint8_t pci_id)
{
	if (pci_id >= pci.pci_dev_ct)
		return (0);
	else
		return (pci.pci_devices[pci_id].pd_subsys_id);
}
