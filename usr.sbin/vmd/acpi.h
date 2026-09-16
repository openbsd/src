/*	$OpenBSD */

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

#ifndef _ACPI_H_
#define _ACPI_H_

#include <dev/acpi/acpireg.h>

struct vm_run_params;

#define VMD_ACPI_EBDA_PTR	0x040E

#define VMD_OEMID		"VMD   "
#define VMD_XSDT_OEM_TABLEID	"VMD XSDT"
#define VMD_MADT_OEM_TABLEID	"VMD MADT"
#define VMD_ASLCOMPILER_ID	"VMD "

/*
 * Keep the payload tables at the bottom of vmd's reserved 0x90000-0xfffff
 * range.  The top of conventional memory is firmware scratch/EBDA space and
 * is not stable across SeaBIOS POST.  The RSDP remains in its own EBDA page
 * for the direct-kernel boot path; firmware boot also copies it to the
 * F-segment through fw_cfg.
 */
#define VMD_XSDT_PADDR		0x90000
#define VMD_MADT_PADDR		0x91000
#define VMD_FADT_PADDR		0x92000
#define VMD_DSDT_PADDR		0x93000
#define VMD_FACS_PADDR		0x94000
#define VMD_DSDT_MAX_SIZE	(VMD_FACS_PADDR - VMD_DSDT_PADDR)
#define VMD_HPET_PADDR		0x95000
#define VMD_RSDP_PADDR		0x9D000

#define VMD_PM1A_EVT_BASE	0xB000
#define VMD_PM1A_EVT_LEN	4
#define VMD_PM1A_CNT_BASE	0xB008
#define VMD_PM1A_CNT_LEN	2
#define VMD_PM_TMR_BASE		0xB00C
#define VMD_PM_TMR_LEN		4
#define VMD_PM1_SLP_TYP_S5	5

#define VMD_FADT_OEM_TABLEID	"VMD FADT"
#define VMD_DSDT_OEM_TABLEID	"VMD DSDT"
#define VMD_HPET_OEM_TABLEID	"VMD HPET"

uint8_t acpi_calculate_checksum(uint8_t *, size_t);
int acpi_load_dsdt(const char *);
void acpi_populate_header(struct acpi_table_header *, uint8_t *);
void acpi_create_facs(paddr_t);
void acpi_create_fadt(paddr_t, paddr_t, paddr_t);
void acpi_create_madt(paddr_t, size_t);
void acpi_create_hpet(paddr_t);
void acpi_create_xsdt(paddr_t, paddr_t *, size_t);
void acpi_create_rsdp(paddr_t, paddr_t);
void acpi_pm1_init(void);
uint8_t vcpu_exit_acpi_pm1(struct vm_run_params *);
uint8_t vcpu_exit_acpi_pm_timer(struct vm_run_params *);
void acpi_init(size_t);

#endif /* !_ACPI_H_ */
