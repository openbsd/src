/*	$OpenBSD: acpi_machdep.c,v 1.27 2009/11/29 21:21:06 deraadt Exp $	*/
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/conf.h>
#include <machine/acpiapm.h>
#include <i386/isa/isa_machdep.h>

#include <machine/cpufunc.h>
#include <machine/npx.h>

#include <dev/isa/isareg.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>

#include "apm.h"
#include "isa.h"
#include "ioapic.h"
#include "lapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

#if NLAPIC > 0
#include <machine/apicvar.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#endif

#if NAPM > 0
int haveacpibutusingapm;	
#endif

extern u_char acpi_real_mode_resume[], acpi_resume_end[];

extern int acpi_savecpu(void);
extern void intr_calculatemasks(void);

#define ACPI_BIOS_RSDP_WINDOW_BASE        0xe0000
#define ACPI_BIOS_RSDP_WINDOW_SIZE        0x20000

u_int8_t	*acpi_scan(struct acpi_mem_map *, paddr_t, size_t);

int
acpi_map(paddr_t pa, size_t len, struct acpi_mem_map *handle)
{
	paddr_t pgpa = trunc_page(pa);
	paddr_t endpa = round_page(pa + len);
	vaddr_t va = uvm_km_valloc(kernel_map, endpa - pgpa);

	if (va == 0)
		return (ENOMEM);

	handle->baseva = va;
	handle->va = (u_int8_t *)(va + (u_long)(pa & PGOFSET));
	handle->vsize = endpa - pgpa;
	handle->pa = pa;

	do {
		pmap_kenter_pa(va, pgpa, VM_PROT_READ | VM_PROT_WRITE);
		va += NBPG;
		pgpa += NBPG;
	} while (pgpa < endpa);

	return 0;
}

void
acpi_unmap(struct acpi_mem_map *handle)
{
	pmap_kremove(handle->baseva, handle->vsize);
	uvm_km_free(kernel_map, handle->baseva, handle->vsize);
}

u_int8_t *
acpi_scan(struct acpi_mem_map *handle, paddr_t pa, size_t len)
{
	size_t i;
	u_int8_t *ptr;
	struct acpi_rsdp1 *rsdp;

	if (acpi_map(pa, len, handle))
		return (NULL);
	for (ptr = handle->va, i = 0;
	     i < len;
	     ptr += 16, i += 16)
		if (memcmp(ptr, RSDP_SIG, sizeof(RSDP_SIG) - 1) == 0) {
			rsdp = (struct acpi_rsdp1 *)ptr;
			/*
			 * Only checksum whichever portion of the
			 * RSDP that is actually present
			 */
			if (rsdp->revision == 0 &&
			    acpi_checksum(ptr, sizeof(struct acpi_rsdp1)) == 0)
				return (ptr);
			else if (rsdp->revision >= 2 && rsdp->revision <= 3 &&
			    acpi_checksum(ptr, sizeof(struct acpi_rsdp)) == 0)
				return (ptr);
		}
	acpi_unmap(handle);

	return (NULL);
}

int
acpi_probe(struct device *parent, struct cfdata *match,
    struct bios_attach_args *ba)
{
	struct acpi_mem_map handle;
	u_int8_t *ptr;
	paddr_t ebda;
#if NAPM > 0
	extern int apm_attached;
#endif

	/*
	 * First try to find ACPI table entries in the EBDA
	 */
	if (acpi_map(0, NBPG, &handle))
		printf("acpi: failed to map BIOS data area\n");
	else {
		ebda = *(const u_int16_t *)(&handle.va[0x40e]);
		ebda <<= 4;
		acpi_unmap(&handle);

		if (ebda && ebda < IOM_BEGIN) {
			if ((ptr = acpi_scan(&handle, ebda, 1024)))
				goto havebase;
		}
	}

	/*
	 * Next try to find the ACPI table entries in the
	 * BIOS memory
	 */
	if ((ptr = acpi_scan(&handle, ACPI_BIOS_RSDP_WINDOW_BASE,
	    ACPI_BIOS_RSDP_WINDOW_SIZE)))
		goto havebase;

	return (0);

havebase:
	ba->ba_acpipbase = ptr - handle.va + handle.pa;
	acpi_unmap(&handle);
#if NAPM > 0
	if (apm_attached) {
		haveacpibutusingapm = 1;
		return (0);
	}
#endif

	return (1);
}

#ifndef SMALL_KERNEL

void
acpi_attach_machdep(struct acpi_softc *sc)
{
	extern void (*cpuresetfn)(void);

	sc->sc_interrupt = isa_intr_establish(NULL, sc->sc_fadt->sci_int,
	    IST_LEVEL, IPL_TTY, acpi_interrupt, sc, sc->sc_dev.dv_xname);
	acpiapm_open = acpiopen;
	acpiapm_close = acpiclose;
	acpiapm_ioctl = acpiioctl;
	acpiapm_kqfilter = acpikqfilter;
	cpuresetfn = acpi_reset;

	/*
	 * Sanity check before setting up trampoline.
	 * Ensure the trampoline size is < PAGE_SIZE
	 */
	KASSERT(acpi_resume_end - acpi_real_mode_resume < PAGE_SIZE);

	bcopy(acpi_real_mode_resume, (caddr_t)ACPI_TRAMPOLINE,
	    acpi_resume_end - acpi_real_mode_resume);
}

void
acpi_cpu_flush(struct acpi_softc *sc, int state)
{
	/*
	 * Flush write back caches since we'll lose them.
	 */
	if (state > ACPI_STATE_S1)
		wbinvd();
}

int
acpi_sleep_machdep(struct acpi_softc *sc, int state)
{
	if (sc->sc_facs == NULL) {
		printf("%s: acpi_sleep_machdep: no FACS\n", DEVNAME(sc));
		return (ENXIO);
	}

	/* i386 does lazy pmap_activate */
	pmap_activate(curproc);

	/*
	 *
	 * ACPI defines two wakeup vectors. One is used for ACPI 1.0
	 * implementations - it's in the FACS table as wakeup_vector and
	 * indicates a 32-bit physical address containing real-mode wakeup
	 * code.
	 *
	 * The second wakeup vector is in the FACS table as
	 * x_wakeup_vector and indicates a 64-bit physical address
	 * containing protected-mode wakeup code.
	 *
	 */
	sc->sc_facs->wakeup_vector = (u_int32_t)ACPI_TRAMPOLINE;
	if (sc->sc_facs->version == 1)
		sc->sc_facs->x_wakeup_vector = 0;

	/* Copy the current cpu registers into a safe place for resume. */
	if (acpi_savecpu()) {
		npxsave_cpu(curcpu(), 1);
		wbinvd();
		acpi_enter_sleep_state(sc, state);
		panic("%s: acpi_enter_sleep_state failed", DEVNAME(sc));
	}

	/*
	 * On resume, the execution path will actually occur here.
	 * This is because we previously saved the stack location
	 * in acpi_savecpu, and issued a far jmp to the restore
	 * routine in the wakeup code. This means we are
	 * returning to the location immediately following the
	 * last call instruction - after the call to acpi_savecpu.
	 */
	
#if 0
        /* Temporarily disabled for debugging purposes */
        /* Reset the wakeup vector to avoid resuming on reboot */
        sc->sc_facs->wakeup_vector = 0;
#endif	

#if NISA > 0
	isa_defaultirq();
#endif
	intr_calculatemasks();

#if NLAPIC > 0
	lapic_enable();
	lapic_initclocks();
	lapic_set_lvt();
#endif

	npxinit(&cpu_info_primary);

#if NIOAPIC > 0
	ioapic_enable();
#endif
	initrtclock();
	inittodr(time_second);

	return (0);
}

#endif /* ! SMALL_KERNEL */
