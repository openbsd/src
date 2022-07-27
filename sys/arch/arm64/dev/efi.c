/*	$OpenBSD: efi.c,v 1.13 2022/07/27 21:01:38 kettenis Exp $	*/

/*
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/cpufunc.h>
#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/fpu.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/acpi/efi.h>

#include <dev/clock_subr.h>

/*
 * We need a large address space to allow identity mapping of physical
 * memory on some machines.
 */
#define EFI_SPACE_BITS	48

extern todr_chip_handle_t todr_handle;

extern uint32_t mmap_size;
extern uint32_t mmap_desc_size;
extern uint32_t mmap_desc_ver;

extern EFI_MEMORY_DESCRIPTOR *mmap;

uint64_t efi_acpi_table;
uint64_t efi_smbios_table;

struct efi_softc {
	struct device	sc_dev;
	struct pmap	*sc_pm;
	EFI_RUNTIME_SERVICES *sc_rs;
	u_long		sc_psw;

	struct todr_chip_handle sc_todr;
};

int	efi_match(struct device *, void *, void *);
void	efi_attach(struct device *, struct device *, void *);

const struct cfattach efi_ca = {
	sizeof(struct efi_softc), efi_match, efi_attach
};

struct cfdriver efi_cd = {
	NULL, "efi", DV_DULL
};

void	efi_remap_runtime(struct efi_softc *);
void	efi_enter(struct efi_softc *);
void	efi_leave(struct efi_softc *);
int	efi_gettime(struct todr_chip_handle *, struct timeval *);
int	efi_settime(struct todr_chip_handle *, struct timeval *);

int
efi_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (strcmp(faa->fa_name, "efi") == 0);
}

void
efi_attach(struct device *parent, struct device *self, void *aux)
{
	struct efi_softc *sc = (struct efi_softc *)self;
	struct fdt_attach_args *faa = aux;
	uint64_t system_table;
	bus_space_handle_t ioh;
	EFI_SYSTEM_TABLE *st;
	EFI_TIME time;
	EFI_STATUS status;
	uint16_t major, minor;
	int node, i;

	node = OF_finddevice("/chosen");
	KASSERT(node != -1);

	system_table = OF_getpropint64(node, "openbsd,uefi-system-table", 0);
	KASSERT(system_table);

	if (bus_space_map(faa->fa_iot, system_table, sizeof(EFI_SYSTEM_TABLE),
	    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_CACHEABLE, &ioh)) {
		printf(": can't map system table\n");
		return;
	}

	st = bus_space_vaddr(faa->fa_iot, ioh);
	sc->sc_rs = st->RuntimeServices;

	major = st->Hdr.Revision >> 16;
	minor = st->Hdr.Revision & 0xffff;
	printf(": UEFI %d.%d", major, minor / 10);
	if (minor % 10)
		printf(".%d", minor % 10);
	printf("\n");

	efi_remap_runtime(sc);
	sc->sc_rs = st->RuntimeServices;

	/*
	 * The FirmwareVendor and ConfigurationTable fields have been
	 * converted from a physical pointer to a virtual pointer, so
	 * we have to activate our pmap to access them.
	 */
	efi_enter(sc);
	if (st->FirmwareVendor) {
		printf("%s: ", sc->sc_dev.dv_xname);
		for (i = 0; st->FirmwareVendor[i]; i++)
			printf("%c", st->FirmwareVendor[i]);
		printf(" rev 0x%x\n", st->FirmwareRevision);
	}
	for (i = 0; i < st->NumberOfTableEntries; i++) {
		EFI_CONFIGURATION_TABLE *ct = &st->ConfigurationTable[i];
		static EFI_GUID acpi_guid = EFI_ACPI_20_TABLE_GUID;
		static EFI_GUID smbios_guid = SMBIOS3_TABLE_GUID;

		if (efi_guidcmp(&acpi_guid, &ct->VendorGuid) == 0)
			efi_acpi_table = (uint64_t)ct->VendorTable;
		if (efi_guidcmp(&smbios_guid, &ct->VendorGuid) == 0)
			efi_smbios_table = (uint64_t)ct->VendorTable;
	}
	efi_leave(sc);

	if (efi_smbios_table != 0) {
		struct fdt_reg reg = { .addr = efi_smbios_table };
		struct fdt_attach_args fa;

		fa.fa_name = "smbios";
		fa.fa_iot = faa->fa_iot;
		fa.fa_reg = &reg;
		fa.fa_nreg = 1;
		config_found(self, &fa, NULL);
	}
	
	efi_enter(sc);
	status = sc->sc_rs->GetTime(&time, NULL);
	efi_leave(sc);
	if (status != EFI_SUCCESS)
		return;

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = efi_gettime;
	sc->sc_todr.todr_settime = efi_settime;
	todr_handle = &sc->sc_todr;
}

void
efi_remap_runtime(struct efi_softc *sc)
{
	EFI_MEMORY_DESCRIPTOR *src;
	EFI_MEMORY_DESCRIPTOR *dst;
	EFI_MEMORY_DESCRIPTOR *vmap;
	EFI_PHYSICAL_ADDRESS phys_start = ~0ULL;
	EFI_PHYSICAL_ADDRESS phys_end = 0;
	EFI_VIRTUAL_ADDRESS virt_start;
	EFI_STATUS status;
	vsize_t space;
	int count = 0;
	int i;

	/*
	 * We don't really want some random executable non-OpenBSD
	 * code lying around in kernel space.  So create a separate
	 * pmap and only activate it when we call runtime services.
	 */
	sc->sc_pm = pmap_create();
	sc->sc_pm->pm_privileged = 1;
	sc->sc_pm->have_4_level_pt = 1;

	/*
	 * We need to provide identity mappings for the complete
	 * memory map for the first SetVirtualAddressMap() call.
	 */
	src = mmap;
	for (i = 0; i < mmap_size / mmap_desc_size; i++) {
		if (src->Type != EfiConventionalMemory) {
			vaddr_t va = src->PhysicalStart;
			paddr_t pa = src->PhysicalStart;
			int npages = src->NumberOfPages;
			vm_prot_t prot = PROT_READ | PROT_WRITE | PROT_EXEC;

#ifdef EFI_DEBUG
			printf("type 0x%x pa 0x%llx va 0x%llx pages 0x%llx attr 0x%llx\n",
			    src->Type, src->PhysicalStart,
			    src->VirtualStart, src->NumberOfPages,
			    src->Attribute);
#endif

			/*
			 * Normal memory is expected to be "write
			 * back" cacheable.  Everything else is mapped
			 * as device memory.
			 */
			if ((src->Attribute & EFI_MEMORY_WB) == 0)
				pa |= PMAP_DEVICE;

			if (src->Attribute & EFI_MEMORY_RP)
				prot &= ~PROT_READ;
			if (src->Attribute & EFI_MEMORY_XP)
				prot &= ~PROT_EXEC;
			if (src->Attribute & EFI_MEMORY_RO)
				prot &= ~PROT_WRITE;

			while (npages--) {
				pmap_enter(sc->sc_pm, va, pa, prot,
				   prot | PMAP_WIRED);
				va += PAGE_SIZE;
				pa += PAGE_SIZE;
			}
		}

		if (src->Attribute & EFI_MEMORY_RUNTIME) {
			if (src->Attribute & EFI_MEMORY_WB) {
				phys_start = MIN(phys_start,
				    src->PhysicalStart);
				phys_end = MAX(phys_end, src->PhysicalStart +
				    src->NumberOfPages * PAGE_SIZE);
			}
			count++;
		}

		src = NextMemoryDescriptor(src, mmap_desc_size);
	}

	/* Allocate memory descriptors for new mappings. */
	vmap = km_alloc(round_page(count * mmap_desc_size),
	    &kv_any, &kp_zero, &kd_waitok);

	/*
	 * Pick a random address somewhere in the lower half of the
	 * usable virtual address space.
	 */
	space = 3 * (VM_MAX_ADDRESS - VM_MIN_ADDRESS) / 4;
	virt_start = VM_MIN_ADDRESS +
	    ((vsize_t)arc4random_uniform(space >> PAGE_SHIFT) << PAGE_SHIFT);

	/*
	 * Establish new mappings.  Apparently some EFI code relies on
	 * the offset between code and data remaining the same so pick
	 * virtual addresses for normal memory that meets that
	 * constraint.  Other mappings are simply tagged on to the end
	 * of the last normal memory mapping.
	 */
	src = mmap;
	dst = vmap;
	for (i = 0; i < mmap_size / mmap_desc_size; i++) {
		if (src->Attribute & EFI_MEMORY_RUNTIME) {
			memcpy(dst, src, mmap_desc_size);
			if (dst->Attribute & EFI_MEMORY_WB) {
				dst->VirtualStart = virt_start +
				    (dst->PhysicalStart - phys_start);
			} else {
				dst->VirtualStart = virt_start +
				     (phys_end - phys_start);
				phys_end += dst->NumberOfPages * PAGE_SIZE;
			}
			/* Mask address to make sure it fits in our pmap. */
			dst->VirtualStart &= ((1ULL << EFI_SPACE_BITS) - 1);
			dst = NextMemoryDescriptor(dst, mmap_desc_size);
		}

		src = NextMemoryDescriptor(src, mmap_desc_size);
	}

	efi_enter(sc);
	status = sc->sc_rs->SetVirtualAddressMap(count * mmap_desc_size,
	    mmap_desc_size, mmap_desc_ver, vmap);
	efi_leave(sc);

	/*
	 * If remapping fails, undo the translations.
	 */
	if (status != EFI_SUCCESS) {
		src = mmap;
		dst = vmap;
		for (i = 0; i < mmap_size / mmap_desc_size; i++) {
			if (src->Attribute & EFI_MEMORY_RUNTIME) {
				dst->VirtualStart = src->PhysicalStart;
				dst = NextMemoryDescriptor(dst, mmap_desc_size);
			}
			src = NextMemoryDescriptor(src, mmap_desc_size);
		}
	}

	/*
	 * Remove all mappings from the pmap.
	 */
	src = mmap;
	for (i = 0; i < mmap_size / mmap_desc_size; i++) {
		if (src->Type != EfiConventionalMemory) {
			pmap_remove(sc->sc_pm, src->PhysicalStart,
			    src->PhysicalStart + src->NumberOfPages * PAGE_SIZE);
		}
		src = NextMemoryDescriptor(src, mmap_desc_size);
	}

	/*
	 * Add back the (translated) runtime mappings.
	 */
	src = vmap;
	for (i = 0; i < count; i++) {
		if (src->Attribute & EFI_MEMORY_RUNTIME) {
			vaddr_t va = src->VirtualStart;
			paddr_t pa = src->PhysicalStart;
			int npages = src->NumberOfPages;
			vm_prot_t prot = PROT_READ | PROT_WRITE;

#ifdef EFI_DEBUG
			printf("type 0x%x pa 0x%llx va 0x%llx pages 0x%llx attr 0x%llx\n",
			    src->Type, src->PhysicalStart,
			    src->VirtualStart, src->NumberOfPages,
			    src->Attribute);
#endif

			/*
			 * Normal memory is expected to be "write
			 * back" cacheable.  Everything else is mapped
			 * as device memory.
			 */
			if ((src->Attribute & EFI_MEMORY_WB) == 0)
				pa |= PMAP_DEVICE;

			/*
			 * Only make pages marked as runtime service code
			 * executable.  This violates the standard but it
			 * seems we can get away with it.
			 */
			if (src->Type == EfiRuntimeServicesCode)
				prot |= PROT_EXEC;

			if (src->Attribute & EFI_MEMORY_RP)
				prot &= ~PROT_READ;
			if (src->Attribute & EFI_MEMORY_XP)
				prot &= ~PROT_EXEC;
			if (src->Attribute & EFI_MEMORY_RO)
				prot &= ~PROT_WRITE;

			while (npages--) {
				pmap_enter(sc->sc_pm, va, pa, prot,
				   prot | PMAP_WIRED);
				va += PAGE_SIZE;
				pa += PAGE_SIZE;
			}
		}

		src = NextMemoryDescriptor(src, mmap_desc_size);
	}

	km_free(vmap, round_page(count * mmap_desc_size), &kv_any, &kp_zero);
}

void
efi_enter(struct efi_softc *sc)
{
	struct pmap *pm = sc->sc_pm;
	uint64_t tcr;

	sc->sc_psw = intr_disable();
	WRITE_SPECIALREG(ttbr0_el1, pmap_kernel()->pm_pt0pa);
	__asm volatile("isb");
	tcr = READ_SPECIALREG(tcr_el1);
	tcr &= ~TCR_T0SZ(0x3f);
	tcr |= TCR_T0SZ(64 - EFI_SPACE_BITS);
	WRITE_SPECIALREG(tcr_el1, tcr);
	cpu_setttb(pm->pm_asid, pm->pm_pt0pa);

	fpu_kernel_enter();
}

void
efi_leave(struct efi_softc *sc)
{
	struct pmap *pm = curcpu()->ci_curpm;
	uint64_t tcr;

	fpu_kernel_exit();

	WRITE_SPECIALREG(ttbr0_el1, pmap_kernel()->pm_pt0pa);
	__asm volatile("isb");
	tcr = READ_SPECIALREG(tcr_el1);
	tcr &= ~TCR_T0SZ(0x3f);
	tcr |= TCR_T0SZ(64 - USER_SPACE_BITS);
	WRITE_SPECIALREG(tcr_el1, tcr);
	cpu_setttb(pm->pm_asid, pm->pm_pt0pa);
	intr_restore(sc->sc_psw);
}

int
efi_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct efi_softc *sc = handle->cookie;
	struct clock_ymdhms dt;
	EFI_TIME time;
	EFI_STATUS status;

	efi_enter(sc);
	status = sc->sc_rs->GetTime(&time, NULL);
	efi_leave(sc);
	if (status != EFI_SUCCESS)
		return EIO;

	dt.dt_year = time.Year;
	dt.dt_mon = time.Month;
	dt.dt_day = time.Day;
	dt.dt_hour = time.Hour;
	dt.dt_min = time.Minute;
	dt.dt_sec = time.Second;

	if (dt.dt_sec > 59 || dt.dt_min > 59 || dt.dt_hour > 23 ||
	    dt.dt_day > 31 || dt.dt_day == 0 ||
	    dt.dt_mon > 12 || dt.dt_mon == 0 ||
	    dt.dt_year < POSIX_BASE_YEAR)
		return EINVAL;

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;
	return 0;
}

int
efi_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct efi_softc *sc = handle->cookie;
	struct clock_ymdhms dt;
	EFI_TIME time;
	EFI_STATUS status;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	time.Year = dt.dt_year;
	time.Month = dt.dt_mon;
	time.Day = dt.dt_day;
	time.Hour = dt.dt_hour;
	time.Minute = dt.dt_min;
	time.Second = dt.dt_sec;
	time.Nanosecond = 0;
	time.TimeZone = 0;
	time.Daylight = 0;

	efi_enter(sc);
	status = sc->sc_rs->SetTime(&time);
	efi_leave(sc);
	if (status != EFI_SUCCESS)
		return EIO;
	return 0;
}
