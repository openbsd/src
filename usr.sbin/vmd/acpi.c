/*	$OpenBSD: acpi.c,v 1.4 2026/09/22 20:20:57 mlarkin Exp $ */

/*
 * Copyright (c) 2025 Mike Larkin <mlarkin@openbsd.org>
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

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <machine/i82093reg.h>
#include <machine/i82489reg.h>
#include <sys/types.h>

#include "acpi.h"
#include "atomicio.h"
#include "fw_cfg.h"
#include "hpet.h"
#include "vmd.h"
#include "virtio.h"

struct acpi_pm1 {
	pthread_mutex_t	mtx;
	uint16_t	status;
	uint16_t	enable;
	uint16_t	control;
	struct timespec	timer_start;
};

static struct acpi_pm1 acpi_pm1 = {
	.mtx = PTHREAD_MUTEX_INITIALIZER
};

static void *acpi_dsdt;
static size_t acpi_dsdt_size;

#define VMD_PM1_ENABLE_MASK	(ACPI_PM1_TMR_EN | ACPI_PM1_GBL_EN | \
	ACPI_PM1_PWRBTN_EN | ACPI_PM1_SLPBTN_EN | ACPI_PM1_RTC_EN | \
	ACPI_PM1_PCIEXP_WAKE_DIS)
#define VMD_PM1_CONTROL_MASK	(ACPI_PM1_SCI_EN | ACPI_PM1_BM_RLD | \
	ACPI_PM1_GBL_RLS | ACPI_PM1_SLP_TYPX_MASK | ACPI_PM1_SLP_EN)

void
acpi_pm1_init(void)
{
	mutex_lock(&acpi_pm1.mtx);
	acpi_pm1.status = 0;
	acpi_pm1.enable = 0;

	acpi_pm1.control = ACPI_PM1_SCI_EN;
	if (clock_gettime(CLOCK_MONOTONIC, &acpi_pm1.timer_start) == -1)
		fatal("clock_gettime");
	mutex_unlock(&acpi_pm1.mtx);
}

static uint8_t
acpi_pm1_read_byte(uint16_t port)
{
	if (port >= VMD_PM1A_EVT_BASE &&
	    port < VMD_PM1A_EVT_BASE + sizeof(acpi_pm1.status))
		return acpi_pm1.status >> ((port - VMD_PM1A_EVT_BASE) * 8);
	if (port >= VMD_PM1A_EVT_BASE + sizeof(acpi_pm1.status) &&
	    port < VMD_PM1A_EVT_BASE + VMD_PM1A_EVT_LEN)
		return acpi_pm1.enable >>
		    ((port - VMD_PM1A_EVT_BASE - sizeof(acpi_pm1.status)) * 8);
	if (port >= VMD_PM1A_CNT_BASE &&
	    port < VMD_PM1A_CNT_BASE + VMD_PM1A_CNT_LEN)
		return acpi_pm1.control >> ((port - VMD_PM1A_CNT_BASE) * 8);

	return 0xff;
}

/*
 * 24 bit PM timer running at 3.579545 MHz
 */
static uint32_t
acpi_pm_timer_read(void)
{
	struct timespec now;
	uint64_t sec, nsec, ticks;

	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
		fatal("clock_gettime");

	sec = now.tv_sec - acpi_pm1.timer_start.tv_sec;
	if (now.tv_nsec < acpi_pm1.timer_start.tv_nsec) {
		sec--;
		nsec = 1000000000ULL + now.tv_nsec -
		    acpi_pm1.timer_start.tv_nsec;
	} else {
		nsec = now.tv_nsec - acpi_pm1.timer_start.tv_nsec;
	}
	ticks = sec * ACPI_FREQUENCY +
	    nsec * ACPI_FREQUENCY / 1000000000ULL;

	return ticks & 0x00ffffff;
}

uint8_t
vcpu_exit_acpi_pm_timer(struct vm_run_params *vrp)
{
	struct vm_exit *vei = vrp->vrp_exit;
	uint32_t data = 0, timer;
	uint16_t offset;
	uint8_t i;

	if (vei->vei.vei_dir == VEI_DIR_IN) {
		mutex_lock(&acpi_pm1.mtx);
		timer = acpi_pm_timer_read();
		mutex_unlock(&acpi_pm1.mtx);
		offset = vei->vei.vei_port - VMD_PM_TMR_BASE;
		for (i = 0; i < vei->vei.vei_size && offset + i < 4; i++)
			data |= ((timer >> ((offset + i) * 8)) & 0xff) <<
			    (i * 8);
		set_return_data(vei, data);
	}
	/* discard writes */

	return 0xff;
}

static void
acpi_pm1_write_byte(uint16_t port, uint8_t data)
{
	uint16_t mask;

	if (port >= VMD_PM1A_EVT_BASE &&
	    port < VMD_PM1A_EVT_BASE + sizeof(acpi_pm1.status)) {
		mask = (uint16_t)data << ((port - VMD_PM1A_EVT_BASE) * 8);
		/* PM1 status bits are write-one-to-clear. */
		acpi_pm1.status &= ~(mask & ACPI_PM1_ALL_STS);
	} else if (port >= VMD_PM1A_EVT_BASE + sizeof(acpi_pm1.status) &&
	    port < VMD_PM1A_EVT_BASE + VMD_PM1A_EVT_LEN) {
		mask = 0xffU <<
		    ((port - VMD_PM1A_EVT_BASE - sizeof(acpi_pm1.status)) * 8);
		acpi_pm1.enable = (acpi_pm1.enable & ~mask) |
		    ((uint16_t)data <<
		    ((port - VMD_PM1A_EVT_BASE - sizeof(acpi_pm1.status)) * 8));
		acpi_pm1.enable &= VMD_PM1_ENABLE_MASK;
	} else if (port >= VMD_PM1A_CNT_BASE &&
	    port < VMD_PM1A_CNT_BASE + VMD_PM1A_CNT_LEN) {
		mask = 0xffU << ((port - VMD_PM1A_CNT_BASE) * 8);
		acpi_pm1.control = (acpi_pm1.control & ~mask) |
		    ((uint16_t)data << ((port - VMD_PM1A_CNT_BASE) * 8));
		acpi_pm1.control &= VMD_PM1_CONTROL_MASK;
	}
}

uint8_t
vcpu_exit_acpi_pm1(struct vm_run_params *vrp)
{
	struct vm_exit *vei = vrp->vrp_exit;
	uint32_t data = 0;
	int powerdown = 0;
	uint16_t port;
	uint8_t i;

	mutex_lock(&acpi_pm1.mtx);
	if (vei->vei.vei_dir == VEI_DIR_IN) {
		for (i = 0; i < vei->vei.vei_size; i++)
			data |= (uint32_t)acpi_pm1_read_byte(vei->vei.vei_port + i)
			    << (i * 8);
		set_return_data(vei, data);
	} else {
		port = vei->vei.vei_port;
		for (i = 0; i < vei->vei.vei_size; i++)
			acpi_pm1_write_byte(port + i,
			    (uint8_t)(vei->vei.vei_data >> (i * 8)));
		powerdown = (acpi_pm1.control & ACPI_PM1_SLP_EN) != 0 &&
		    (acpi_pm1.control & ACPI_PM1_SLP_TYPX_MASK) ==
		    ACPI_PM1_SLP_TYPX(VMD_PM1_SLP_TYP_S5);
	}
	mutex_unlock(&acpi_pm1.mtx);

	if (powerdown) {
		log_debug("%s: guest entered ACPI S5", __func__);
		vm_shutdown(VMMCI_SHUTDOWN);
	}

	return 0xff;
}

/*
 * acpi_calculate_checksum
 *
 * calculates a checksum value for the table at 'table' with size 'len'.
 */
uint8_t
acpi_calculate_checksum(uint8_t *tbl, size_t len)
{
	uint8_t cksum;
	size_t i;

	cksum = 0;
	for (i = 0; i < len; i++)
		cksum += tbl[i];

	return (256 - cksum);
}

static void
acpi_verify_checksum(uint8_t *tbl, size_t len)
{
	size_t i;
	uint8_t cksum = 0;

	for (i = 0 ; i < len ; i++)
		cksum += tbl[i];

	if (cksum)
		log_warnx("%s: ACPI table checksum error, got %d", __func__,
		    cksum);
}

/*
 * acpi_load_dsdt
 *
 * Read the DSDT before the vm process restricts filesystem access via
 * pledge. It is copied into guest memory later, after the VM's memory map
 * exists.
 */
int
acpi_load_dsdt(const char *path)
{
	int fd;
	size_t n;
	struct stat sb;
	void *buf;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		log_warn("%s: cannot open %s", __func__, path);
		return (-1);
	}

	if (fstat(fd, &sb) == -1) {
		log_warn("%s: cannot stat %s", __func__, path);
		close(fd);
		return (-1);
	}

	if (sb.st_size <= 0 ||
	    (uint64_t)sb.st_size > VMD_DSDT_MAX_SIZE) {
		log_warnx("%s: unreasonable table size %lld", __func__,
		    (long long)sb.st_size);
		close(fd);
		return (-1);
	}

	buf = malloc((size_t)sb.st_size);
	if (buf == NULL) {
		log_warn("%s: malloc", __func__);
		close(fd);
		return (-1);
	}

	n = atomicio(read, fd, buf, (size_t)sb.st_size);
	close(fd);

	if (n != (size_t)sb.st_size) {
		log_warn("%s: short read from %s", __func__, path);
		free(buf);
		return (-1);
	}

	free(acpi_dsdt);
	acpi_dsdt = buf;
	acpi_dsdt_size = n;
	return (0);
}

/* Copy the preloaded DSDT into guest memory. */
static ssize_t
acpi_install_dsdt(paddr_t pa)
{
	ssize_t size;

	if (acpi_dsdt == NULL)
		return (-1);

	size = (ssize_t)acpi_dsdt_size;
	if (write_mem(pa, acpi_dsdt, acpi_dsdt_size)) {
		log_warnx("%s: could not write DSDT to %lx", __func__, pa);
		size = -1;
	}
	free(acpi_dsdt);
	acpi_dsdt = NULL;
	acpi_dsdt_size = 0;

	return (size);
}

/*
 * acpi_populate_header
 *
 * populates the table header in 'hdr' with default values. The OEM table
 * id is provided in 'oemtableid'.
 *
 * Caller is responsible for later populating the header length, checksum,
 * and signature fields
 */
void
acpi_populate_header(struct acpi_table_header *hdr, uint8_t *oemtableid)
{
	/* Header */
	hdr->revision = 1;
	memcpy(hdr->oemid, VMD_OEMID, 6);
	memcpy(hdr->oemtableid, oemtableid, 8);
	hdr->oemrevision = 1;
	memcpy(hdr->aslcompilerid, VMD_ASLCOMPILER_ID, 4);
	hdr->aslcompilerrevision = 1;
}

void
acpi_create_facs(paddr_t pa)
{
	struct acpi_facs facs;

	memset(&facs, 0, sizeof(facs));
	memcpy(facs.signature, FACS_SIG, sizeof(facs.signature));
	facs.length = sizeof(facs);
	facs.version = 1;

	log_debug("%s: writing FACS to %lx", __func__, pa);
	if (write_mem(pa, &facs, sizeof(facs)))
		log_warnx("%s: could not write FACS", __func__);
}

/*
 * acpi_create_fadt
 *
 * create FADT (Fixed ACPI Description Table) at 'pa' pointing to FACS at
 * 'facs_pa' and DSDT at 'dsdt_pa'
 */
void
acpi_create_fadt(paddr_t pa, paddr_t facs_pa, paddr_t dsdt_pa)
{
	struct acpi_fadt fadt;

	log_debug("%s: creating FADT", __func__);
	memset(&fadt, 0, sizeof(fadt));

	acpi_populate_header(&fadt.hdr, VMD_FADT_OEM_TABLEID);
	fadt.hdr.revision = 6;
	fadt.fadt_minor = 5;

	/* Header */
	memcpy(fadt.hdr_signature, FADT_SIG, 4);
	fadt.hdr.length = sizeof(fadt);

	/* The tables reside below 4GB, so use only the 32-bit pointers. */
	fadt.firmware_ctl = (uint32_t)facs_pa;
	fadt.x_firmware_ctl = 0;
	fadt.dsdt = (uint32_t)dsdt_pa;
	fadt.x_dsdt = 0;

	/* System configuration */
	fadt.pm_profile = FADT_PM_DESKTOP;

	/* SCI interrupt - typically uses IRQ 9 for ACPI */
	fadt.sci_int = 9;

	/* PM register lengths */
	fadt.pm1_evt_len = VMD_PM1A_EVT_LEN;
	fadt.pm1_cnt_len = VMD_PM1A_CNT_LEN;
	fadt.pm_tmr_len = VMD_PM_TMR_LEN;
	fadt.gpe0_blk_len = 0;
	fadt.p_lvl2_lat = 100;
	fadt.p_lvl3_lat = 1000;

	/*
	 * PM1A is retained for non-hardware-reduced ACPI.
	 */
	fadt.pm1a_evt_blk = VMD_PM1A_EVT_BASE;
	fadt.pm1b_evt_blk = 0;
	fadt.pm1a_cnt_blk = VMD_PM1A_CNT_BASE;
	fadt.pm1b_cnt_blk = 0;
	fadt.pm_tmr_blk = VMD_PM_TMR_BASE;
	fadt.gpe0_blk = 0;

	/*
	 * Revision 5 and newer consumers use the extended GAS fields in
	 * preference to the legacy 32-bit port fields. PM1A event and control
	 * are required for non-hardware-reduced ACPI.
	 */
	fadt.x_pm1a_evt_blk.address_space_id = GAS_SYSTEM_IOSPACE;
	fadt.x_pm1a_evt_blk.register_bit_width = VMD_PM1A_EVT_LEN * 8;
	fadt.x_pm1a_evt_blk.access_size = GAS_ACCESS_DWORD;
	fadt.x_pm1a_evt_blk.address = VMD_PM1A_EVT_BASE;
	fadt.x_pm1a_cnt_blk.address_space_id = GAS_SYSTEM_IOSPACE;
	fadt.x_pm1a_cnt_blk.register_bit_width = VMD_PM1A_CNT_LEN * 8;
	fadt.x_pm1a_cnt_blk.access_size = GAS_ACCESS_WORD;
	fadt.x_pm1a_cnt_blk.address = VMD_PM1A_CNT_BASE;
	fadt.x_pm_tmr_blk.address_space_id = GAS_SYSTEM_IOSPACE;
	fadt.x_pm_tmr_blk.register_bit_width = VMD_PM_TMR_LEN * 8;
	fadt.x_pm_tmr_blk.access_size = GAS_ACCESS_DWORD;
	fadt.x_pm_tmr_blk.address = VMD_PM_TMR_BASE;

	/* IAPC Boot Architecture Flags */
	fadt.iapc_boot_arch = FADT_LEGACY_DEVICES;

	/* FADT flags */
	fadt.flags = 0;

	/* Checksum */
	fadt.hdr.checksum = acpi_calculate_checksum((uint8_t *)&fadt, sizeof(fadt));

	log_debug("%s: FADT size %zu", __func__, sizeof(fadt));
	log_debug("%s: DSDT pointer 0x%x / 0x%llx", __func__, fadt.dsdt,
	    (unsigned long long)fadt.x_dsdt);

	acpi_verify_checksum((uint8_t *)&fadt, sizeof(fadt));

	log_debug("%s: writing FADT to %lx", __func__, pa);
	if (write_mem(pa, &fadt, sizeof(fadt)))
		log_warnx("%s: could not write FADT table", __func__);

	log_debug("%s: FADT creation complete", __func__);
}

/*
 * acpi_create_madt
 *
 * create MADT table at 'pa' based on the supplied parameters
 */
void
acpi_create_madt(paddr_t pa, size_t numcpu)
{
	struct acpi_madt *madt;
	struct acpi_madt_lapic *lapic;
	struct acpi_madt_ioapic *ioapic;
	uint8_t *tbl, *b;
	size_t i, sz;

	log_debug("%s: creating MADT", __func__);
	sz = sizeof(*madt) + sizeof(*ioapic) + (sizeof(*lapic) * numcpu);
	log_debug("%s: MADT size %zd", __func__, sz);
	tbl = (uint8_t *)malloc(sz);
	if (tbl == NULL)
		fatal("malloc");

	memset(tbl, 0, sz);

	b = tbl;
	madt = (struct acpi_madt *)b;

	b += sizeof(*madt);
	ioapic = (struct acpi_madt_ioapic *)b;

	b += sizeof(*ioapic);
	lapic = (struct acpi_madt_lapic *)b;

	acpi_populate_header(&madt->hdr, VMD_MADT_OEM_TABLEID);
	madt->local_apic_address = LAPIC_BASE;
	madt->flags = ACPI_APIC_PCAT_COMPAT;

	for (i = 0; i < numcpu; i++) {
		lapic[i].apic_type = ACPI_MADT_LAPIC;
		lapic[i].length = sizeof(lapic[i]);
		lapic[i].acpi_proc_id = i;
		lapic[i].apic_id = i;
		lapic[i].flags = ACPI_PROC_ENABLE;
	}

	ioapic->apic_type = ACPI_MADT_IOAPIC;
	ioapic->length = sizeof(*ioapic);
	ioapic->reserved = 0;
	ioapic->address = IOAPIC_BASE_DEFAULT;
	ioapic->global_int_base = 0;
	ioapic->acpi_ioapic_id = numcpu + 1;

	memcpy(madt->hdr_signature, MADT_SIG, 4);
	madt->hdr.length = sz;
	madt->hdr.checksum = acpi_calculate_checksum(tbl, sz);
	log_debug("%s: computed MADT checksum 0x%x", __func__, madt->hdr.checksum);

	log_debug("%s: writing MADT to %lx", __func__, pa);
	acpi_verify_checksum((uint8_t *)tbl, sz);

	if (write_mem(pa, tbl, sz))
		log_warnx("%s: could not write MADT table", __func__);
	free(tbl);

	log_debug("%s: MADT creation complete", __func__);
}

void
acpi_create_hpet(paddr_t pa)
{
	struct acpi_hpet table;

	memset(&table, 0, sizeof(table));
	acpi_populate_header(&table.hdr, VMD_HPET_OEM_TABLEID);
	memcpy(table.hdr.signature, HPET_SIG, sizeof(table.hdr.signature));
	table.hdr.length = sizeof(table);
	table.hdr.revision = 1;
	table.event_timer_block_id = VMD_HPET_CAP_ID;
	table.base_address.address_space_id = GAS_SYSTEM_MEMORY;
	table.base_address.register_bit_width = 64;
	table.base_address.access_size = GAS_ACCESS_QWORD;
	table.base_address.address = VMD_HPET_BASE;
	table.hpet_number = 0;
	table.main_counter_min_clock_tick = 0x80;
	table.page_protection = 0;
	table.hdr.checksum = acpi_calculate_checksum((uint8_t *)&table,
	    sizeof(table));

	acpi_verify_checksum((uint8_t *)&table, sizeof(table));
	if (write_mem(pa, &table, sizeof(table)) != 0)
		log_warnx("%s: could not write HPET table", __func__);
}

/*
 * acpi_create_xsdt
 *
 * create XSDT table at 'pa' with pointers to the tables provided
 * in 'tables'. The 'numtables' parameter represents the number of
 * tables to include.
 */
void
acpi_create_xsdt(paddr_t pa, paddr_t *tables, size_t numtables)
{
	struct acpi_xsdt *xsdt;
	size_t i, sz;

	log_debug("%s: creating XSDT", __func__);
	sz = sizeof(*xsdt) + ((numtables - 1) * sizeof(paddr_t));
	log_debug("%s: XSDT size %zd", __func__, sz);

	xsdt = (struct acpi_xsdt *)malloc(sz);

	/* XXX unwind properly dont call fatal */
	if (xsdt == NULL)
		fatal("malloc");

	memset(xsdt, 0, sizeof(*xsdt));

	acpi_populate_header(&xsdt->hdr, VMD_XSDT_OEM_TABLEID);
	for (i = 0; i < numtables; i++)
		xsdt->table_offsets[i] = tables[i];

	memcpy(xsdt->hdr_signature, XSDT_SIG, 4);
	xsdt->hdr.length = sz;
	xsdt->hdr.checksum = acpi_calculate_checksum((uint8_t *)xsdt, sz);

	acpi_verify_checksum((uint8_t *)xsdt, sz);

	log_debug("%s: writing XSDT to %lx", __func__, pa);
	if (write_mem(pa, xsdt, sz))
		log_warnx("%s: could not write XSDT table", __func__);

	log_debug("%s: XSDT creation complete", __func__);
	free(xsdt);
}

/*
 * acpi_create_rsdp
 *
 * create RSDP table at 'pa' with a pointer to the XSDT located at 'xsdt_pa'.
 */
void
acpi_create_rsdp(paddr_t pa, paddr_t xsdt_pa)
{
	struct acpi_rsdp rsdp;

	log_debug("%s: creating RSDP", __func__);
	memset(&rsdp, 0, sizeof(rsdp));

	/* RSDP v1 fields */
	memcpy(&rsdp.rsdp_signature, RSDP_SIG, 8);
	memcpy(&rsdp.rsdp_oemid, VMD_OEMID, 6);
	rsdp.rsdp_revision = 2;
	rsdp.rsdp_rsdt = 0;

	/* RSDP v2 fields */
	rsdp.rsdp_length = sizeof(rsdp);
	rsdp.rsdp_xsdt = xsdt_pa;

	/* Checksums */
	rsdp.rsdp_checksum = acpi_calculate_checksum((uint8_t *)&rsdp.rsdp1,
	    sizeof(rsdp.rsdp1));

	rsdp.rsdp_extchecksum = acpi_calculate_checksum((uint8_t *)&rsdp,
	    sizeof(rsdp));

	acpi_verify_checksum((uint8_t *)&rsdp, sizeof(rsdp));
	fw_cfg_add_acpi_rsdp(&rsdp, sizeof(rsdp));

	log_debug("%s: writing RSDP to %lx", __func__, pa);
	if (write_mem(pa, &rsdp, sizeof(rsdp)))
		log_warnx("%s: could not write RSDP table", __func__);

	log_debug("%s: RSDP creation complete", __func__);
}

void
acpi_init(size_t numcpu)
{
	paddr_t tables[2];
	size_t numtables;
	ssize_t dsdt_size;
	uint16_t rsdp_ptr_real;
	int have_dsdt = 0;

	log_debug("%s: initializing acpi tables", __func__);
	rsdp_ptr_real = VMD_RSDP_PADDR >> 4;

	numtables = 0;

	/* Install the preloaded DSDT before creating the FADT. */
	dsdt_size = acpi_install_dsdt(VMD_DSDT_PADDR);
	if (dsdt_size != -1) {
		have_dsdt = 1;
		log_debug("%s: DSDT loaded successfully (%zd bytes)", __func__,
		    dsdt_size);
	} else {
		log_warnx("%s: DSDT not loaded (optional)", __func__);
	}

	/* Create FADT - points to DSDT if available, required for ACPI */
	if (have_dsdt) {
		acpi_create_facs(VMD_FACS_PADDR);
		acpi_create_fadt(VMD_FADT_PADDR, VMD_FACS_PADDR,
		    VMD_DSDT_PADDR);
		tables[numtables++] = VMD_FADT_PADDR;
		log_debug("%s: FADT created pointing to DSDT", __func__);
	}

	acpi_create_madt(VMD_MADT_PADDR, numcpu);
	tables[numtables++] = VMD_MADT_PADDR;
	/* Do not advertise an HPET until vmd provides the device itself. */

	acpi_create_xsdt(VMD_XSDT_PADDR, tables, numtables);
	acpi_create_rsdp(VMD_RSDP_PADDR, VMD_XSDT_PADDR);

	/* EBDA pointer */
	log_debug("%s: writing RSDP pointer 0x%x -> 0x%x", __func__,
	    rsdp_ptr_real, VMD_ACPI_EBDA_PTR);

	if (write_mem(VMD_ACPI_EBDA_PTR , &rsdp_ptr_real, sizeof(rsdp_ptr_real)))
		log_warnx("%s: could not write RSDP pointer to BDA", __func__);

}
