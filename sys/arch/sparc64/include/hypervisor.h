/*	$OpenBSD: hypervisor.h,v 1.12 2009/12/31 11:50:33 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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

/*
 * UltraSPARC Hypervisor API.
 */

/*
 * API versioning
 */

int64_t	hv_api_get_version(uint64_t api_group,
	    uint64_t *major_number, uint64_t *minor_number);

/*
 * Domain services
 */

int64_t hv_mach_desc(paddr_t buffer, psize_t *length);

/*
 * CPU services
 */

void	hv_cpu_yield(void);
int64_t	hv_cpu_qconf(uint64_t queue, uint64_t base, uint64_t nentries);

#define CPU_MONDO_QUEUE		0x3c
#define DEVICE_MONDO_QUEUE	0x3d

int64_t	hv_cpu_mondo_send(uint64_t ncpus, paddr_t cpulist, paddr_t data);
int64_t	hv_cpu_myid(uint64_t *cpuid);

/*
 * MMU services
 */

int64_t	hv_mmu_demap_page(vaddr_t vaddr, uint64_t context, uint64_t flags);
int64_t	hv_mmu_demap_ctx(uint64_t context, uint64_t flags);
int64_t	hv_mmu_demap_all(uint64_t flags);
int64_t	hv_mmu_map_perm_addr(vaddr_t vaddr, uint64_t tte, uint64_t flags);
int64_t	hv_mmu_unmap_perm_addr(vaddr_t vaddr, uint64_t flags);
int64_t	hv_mmu_map_addr(vaddr_t vaddr, uint64_t context, uint64_t tte,
	    uint64_t flags);
int64_t	hv_mmu_unmap_addr(vaddr_t vaddr, uint64_t context, uint64_t flags);

#define MAP_DTLB	0x1
#define MAP_ITLB	0x2

struct tsb_desc {
	uint16_t	td_idxpgsz;
	uint16_t	td_assoc;
	uint32_t	td_size;
	uint32_t	td_ctxidx;
	uint32_t	td_pgsz;
	paddr_t		td_pa;
	uint64_t	td_reserved;
};

int64_t	hv_mmu_tsb_ctx0(uint64_t ntsb, paddr_t tsbptr);
int64_t	hv_mmu_tsb_ctxnon0(uint64_t ntsb, paddr_t tsbptr);

/*
 * Cache and memory services
 */

int64_t	hv_mem_scrub(paddr_t raddr, psize_t length);
int64_t	hv_mem_sync(paddr_t raddr, psize_t length);

/*
 * Device interrupt services
 */

int64_t	hv_intr_devino_to_sysino(uint64_t devhandle, uint64_t devino,
	    uint64_t *sysino);
int64_t	hv_intr_getenabled(uint64_t sysino, uint64_t *intr_enabled);
int64_t	hv_intr_setenabled(uint64_t sysino, uint64_t intr_enabled);
int64_t	hv_intr_getstate(uint64_t sysino, uint64_t *intr_state);
int64_t	hv_intr_setstate(uint64_t sysino, uint64_t intr_state);
int64_t	hv_intr_gettarget(uint64_t sysino, uint64_t *cpuid);
int64_t	hv_intr_settarget(uint64_t sysino, uint64_t cpuid);

#define INTR_DISABLED	0
#define INTR_ENABLED	1

#define INTR_IDLE	0
#define INTR_RECEIVED	1
#define INTR_DELIVERED	2

int64_t	hv_vintr_getcookie(uint64_t devhandle, uint64_t devino,
	    uint64_t *cookie_value);
int64_t	hv_vintr_setcookie(uint64_t devhandle, uint64_t devino,
	    uint64_t cookie_value);
int64_t	hv_vintr_getenabled(uint64_t devhandle, uint64_t devino,
	    uint64_t *intr_enabled);
int64_t	hv_vintr_setenabled(uint64_t devhandle, uint64_t devino,
	    uint64_t intr_enabled);
int64_t	hv_vintr_getstate(uint64_t devhandle, uint64_t devino,
	    uint64_t *intr_state);
int64_t	hv_vintr_setstate(uint64_t devhandle, uint64_t devino,
	    uint64_t intr_state);
int64_t	hv_vintr_gettarget(uint64_t devhandle, uint64_t devino,
	    uint64_t *cpuid);
int64_t	hv_vintr_settarget(uint64_t devhandle, uint64_t devino,
	    uint64_t cpuid);

/*
 * Time of day services
 */

int64_t	hv_tod_get(uint64_t *tod);
int64_t	hv_tod_set(uint64_t tod);

/*
 * Console services
 */

int64_t	hv_cons_getchar(int64_t *ch);
int64_t	hv_cons_putchar(int64_t ch);
int64_t	hv_api_putchar(int64_t ch);

#define CONS_BREAK	-1
#define CONS_HUP	-2

/*
 * Domain state services
 */

int64_t	hv_soft_state_set(uint64_t software_state,
	    paddr_t software_description_ptr);

#define SIS_NORMAL	0x1
#define SIS_TRANSITION	0x2

/*
 * PCI I/O services
 */

int64_t	hv_pci_iommu_map(uint64_t devhandle, uint64_t tsbid,
	    uint64_t nttes, uint64_t io_attributes, paddr_t io_page_list_p,
	    uint64_t *nttes_mapped);
int64_t	hv_pci_iommu_demap(uint64_t devhandle, uint64_t tsbid,
	    uint64_t nttes, uint64_t *nttes_demapped);
int64_t	hv_pci_iommu_getmap(uint64_t devhandle, uint64_t tsbid,
	    uint64_t *io_attributes, paddr_t *r_addr);
int64_t	hv_pci_iommu_getbypass(uint64_t devhandle, paddr_t r_addr,
	    uint64_t io_attributes, uint64_t *io_addr);

int64_t	hv_pci_config_get(uint64_t devhandle, uint64_t pci_device,
            uint64_t pci_config_offset, uint64_t size,
	    uint64_t *error_flag, uint64_t *data);
int64_t	hv_pci_config_put(uint64_t devhandle, uint64_t pci_device,
            uint64_t pci_config_offset, uint64_t size, uint64_t data,
	    uint64_t *error_flag);

#define PCI_MAP_ATTR_READ  0x01		/* From memory */
#define PCI_MAP_ATTR_WRITE 0x02		/* To memory */

/*
 * Logical Domain Channel services
 */

int64_t hv_ldc_tx_qconf(uint64_t ldc_id, paddr_t base_raddr,
	    uint64_t nentries);
int64_t hv_ldc_tx_qinfo(uint64_t ldc_id, paddr_t *base_raddr,
	    uint64_t *nentries);
int64_t hv_ldc_tx_get_state(uint64_t ldc_id, uint64_t *head_offset,
	    uint64_t *tail_offset, uint64_t *channel_state);
int64_t hv_ldc_tx_set_qtail(uint64_t ldc_id, uint64_t tail_offset);
int64_t hv_ldc_rx_qconf(uint64_t ldc_id, paddr_t base_raddr,
	    uint64_t nentries);
int64_t hv_ldc_rx_qinfo(uint64_t ldc_id, paddr_t *base_raddr,
	    uint64_t *nentries);
int64_t hv_ldc_rx_get_state(uint64_t ldc_id, uint64_t *head_offset,
	    uint64_t *tail_offset, uint64_t *channel_state);
int64_t hv_ldc_rx_set_qhead(uint64_t ldc_id, uint64_t head_offset);

#define LDC_CHANNEL_DOWN	0
#define LDC_CHANNEL_UP		1
#define LDC_CHANNEL_RESET	2

int64_t	hv_ldc_set_map_table(uint64_t ldc_id, paddr_t base_raddr,
	    uint64_t nentries);
int64_t	hv_ldc_get_map_table(uint64_t ldc_id, paddr_t *base_raddr,
	    uint64_t *nentries);
int64_t hv_ldc_copy(uint64_t ldc_id, uint64_t flags, uint64_t cookie,
	    paddr_t raddr, psize_t length, psize_t *ret_length);

#define LDC_COPY_IN		0
#define LDC_COPY_OUT		1

int64_t hv_ldc_mapin(uint64_t ldc_id, uint64_t cookie, paddr_t *raddr,
	    uint64_t *perms);
int64_t hv_ldc_unmap(paddr_t raddr, uint64_t *perms);

/*
 * Cryptographic services
 */

int64_t	hv_rng_get_diag_control(void);
int64_t	hv_rng_ctl_read(paddr_t raddr, uint64_t *state, uint64_t *delta);
int64_t	hv_rng_ctl_write(paddr_t raddr, uint64_t state, uint64_t timeout,
	uint64_t *delta);

#define RNG_STATE_UNCONFIGURED	0
#define RNG_STATE_CONFIGURED	1
#define RNG_STATE_HEALTHCHECK	2
#define RNG_STATE_ERROR		3

int64_t	hv_rng_data_read_diag(paddr_t raddr, uint64_t size, uint64_t *delta);
int64_t	hv_rng_data_read(paddr_t raddr, uint64_t *delta);

/*
 * Error codes
 */

#define H_EOK		0
#define H_ENOCPU	1
#define H_ENORADDR	2
#define H_ENOINTR	3
#define H_EBADPGSZ	4
#define H_EBADTSB	5
#define H_EINVAL	6
#define H_EBADTRAP	7
#define H_EBADALIGN	8
#define H_EWOULDBLOCK	9
#define H_ENOACCESS	10
#define H_EIO		11
#define H_ECPUERROR	12
#define H_ENOTSUPPORTED	13
#define H_ENOMAP	14
#define H_ETOOMANY	15
#define H_ECHANNEL	16
