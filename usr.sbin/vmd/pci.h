/*	$OpenBSD: pci.h,v 1.7 2017/09/17 23:07:56 pd Exp $	*/

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

#define PCI_MODE1_ENABLE	0x80000000UL
#define PCI_MODE1_ADDRESS_REG	0x0cf8
#define PCI_MODE1_DATA_REG	0x0cfc
#define PCI_CONFIG_MAX_DEV	32
#define PCI_MAX_BARS		6

#define PCI_BAR_TYPE_IO		0x0
#define PCI_BAR_TYPE_MMIO	0x1

#define PCI_MAX_PIC_IRQS	10

typedef int (*pci_cs_fn_t)(int dir, uint8_t reg, uint32_t *data);
typedef int (*pci_iobar_fn_t)(int dir, uint16_t reg, uint32_t *data, uint8_t *,
    void *, uint8_t);
typedef int (*pci_mmiobar_fn_t)(int dir, uint32_t ofs, uint32_t *data);

union pci_dev {
	uint32_t pd_cfg_space[PCI_CONFIG_SPACE_SIZE / 4];

	struct {
		uint16_t pd_vid;
		uint16_t pd_did;
		uint16_t pd_cmd;
		uint16_t pd_status;
		uint8_t pd_rev;
		uint8_t pd_prog_if;
		uint8_t pd_subclass;
		uint8_t pd_class;
		uint8_t pd_cache_size;
		uint8_t pd_lat_timer;
		uint8_t pd_header_type;
		uint8_t pd_bist;
		uint32_t pd_bar[PCI_MAX_BARS];
		uint32_t pd_cardbus_cis;
		uint16_t pd_subsys_vid;
		uint16_t pd_subsys_id;
		uint32_t pd_exp_rom_addr;
		uint8_t pd_cap;
		uint32_t pd_reserved0 : 24;
		uint32_t pd_reserved1;
		uint8_t pd_irq;
		uint8_t pd_int;
		uint8_t pd_min_grant;
		uint8_t pd_max_grant;

		uint8_t pd_bar_ct;
		pci_cs_fn_t pd_csfunc;

		uint8_t pd_bartype[PCI_MAX_BARS];
		uint32_t pd_barsize[PCI_MAX_BARS];
		void *pd_barfunc[PCI_MAX_BARS];
		void *pd_bar_cookie[PCI_MAX_BARS];
	} __packed;
};

struct pci {
	uint8_t pci_dev_ct;
	uint64_t pci_next_mmio_bar;
	uint64_t pci_next_io_bar;
	uint8_t pci_next_pic_irq;
	uint32_t pci_addr_reg;
	uint32_t pci_data_reg;

	union pci_dev pci_devices[PCI_CONFIG_MAX_DEV];
};

void pci_handle_address_reg(struct vm_run_params *);
void pci_handle_data_reg(struct vm_run_params *);
uint8_t pci_handle_io(struct vm_run_params *);
void pci_init(void);
int pci_add_device(uint8_t *, uint16_t, uint16_t, uint8_t, uint8_t, uint16_t,
    uint16_t, uint8_t, pci_cs_fn_t);
int pci_add_bar(uint8_t, uint32_t, void *, void *);
int pci_set_bar_fn(uint8_t, uint8_t, void *, void *);
uint8_t pci_get_dev_irq(uint8_t);
int pci_dump(int);
int pci_restore(int);
