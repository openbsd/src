/*	$OpenBSD: acpi_machdep.c,v 1.69 2015/03/14 03:38:46 jsg Exp $	*/
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
#include <sys/memrange.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/reboot.h>
#include <sys/hibernate.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/biosvar.h>
#include <machine/isa_machdep.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuvar.h>

#include <dev/isa/isareg.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>

#include "isa.h"
#include "ioapic.h"
#include "lapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

#if NLAPIC > 0
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#endif

extern u_char acpi_real_mode_resume[], acpi_resume_end[];
extern u_char acpi_tramp_data_start[], acpi_tramp_data_end[];
extern u_int32_t acpi_pdirpa;
extern paddr_t tramp_pdirpa;

extern int acpi_savecpu(void) __returns_twice;

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
	handle->va = (u_int8_t *)(va + (pa & PGOFSET));
	handle->vsize = endpa - pgpa;
	handle->pa = pa;

	do {
		pmap_kenter_pa(va, pgpa, PROT_READ | PROT_WRITE);
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
	for (ptr = handle->va, i = 0; i < len; ptr += 16, i += 16) {
		/* is there a valid signature? */
		if (memcmp(ptr, RSDP_SIG, sizeof(RSDP_SIG) - 1))
			continue;

		/* is the checksum valid? */
		if (acpi_checksum(ptr, sizeof(struct acpi_rsdp1)) != 0)
			continue;

		/* check the extended checksum as needed */
		rsdp = (struct acpi_rsdp1 *)ptr;
		if (rsdp->revision == 0)
			return (ptr);
		else if (rsdp->revision >= 2 && rsdp->revision <= 4)
			if (acpi_checksum(ptr, sizeof(struct acpi_rsdp)) == 0)
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

	return (1);
}

/*
 * Acquire the global lock.  If busy, set the pending bit.  The caller
 * will wait for notification from the BIOS that the lock is available
 * and then attempt to acquire it again.
 */
int
acpi_acquire_glk(uint32_t *lock)
{
	uint32_t	new, old;

	do {
		old = *lock;
		new = (old & ~GL_BIT_PENDING) | GL_BIT_OWNED;
		if ((old & GL_BIT_OWNED) != 0)
			new |= GL_BIT_PENDING;
	} while (atomic_cas_uint(lock, old, new) != old);

	return ((new & GL_BIT_PENDING) == 0);
}

/*
 * Release the global lock, returning whether there is a waiter pending.
 * If the BIOS set the pending bit, OSPM must notify the BIOS when it
 * releases the lock.
 */
int
acpi_release_glk(uint32_t *lock)
{
	uint32_t	new, old;

	do {
		old = *lock;
		new = old & ~(GL_BIT_PENDING | GL_BIT_OWNED);
	} while (atomic_cas_uint(lock, old, new) != old);

	return ((old & GL_BIT_PENDING) != 0);
}

#ifndef SMALL_KERNEL

void
acpi_attach_machdep(struct acpi_softc *sc)
{
	extern void (*cpuresetfn)(void);

	sc->sc_interrupt = isa_intr_establish(NULL, sc->sc_fadt->sci_int,
	    IST_LEVEL, IPL_TTY, acpi_interrupt, sc, sc->sc_dev.dv_xname);
	cpuresetfn = acpi_reset;

	/*
	 * Sanity check before setting up trampoline.
	 * Ensure the trampoline size is < PAGE_SIZE
	 */
	KASSERT(acpi_resume_end - acpi_real_mode_resume < PAGE_SIZE);

	/* Map ACPI tramp code and data pages RW for copy */
	pmap_kenter_pa(ACPI_TRAMPOLINE, ACPI_TRAMPOLINE,
	    PROT_READ | PROT_WRITE);
	pmap_kenter_pa(ACPI_TRAMP_DATA, ACPI_TRAMP_DATA,
	    PROT_READ | PROT_WRITE);

	memcpy((caddr_t)ACPI_TRAMPOLINE, acpi_real_mode_resume,
	    acpi_resume_end - acpi_real_mode_resume);
	memcpy((caddr_t)ACPI_TRAMP_DATA, acpi_tramp_data_start,
	    acpi_tramp_data_end - acpi_tramp_data_start);

	acpi_pdirpa = tramp_pdirpa;

	/* Unmap, will be remapped in acpi_sleep_cpu */
	pmap_kremove(ACPI_TRAMPOLINE, PAGE_SIZE);
	pmap_kremove(ACPI_TRAMP_DATA, PAGE_SIZE);
}

void
acpi_sleep_clocks(struct acpi_softc *sc, int state)
{
	rtcstop();

#if NLAPIC > 0
	lapic_disable();
#endif
}

/*
 * We repair the interrupt hardware so that any events which occur
 * will cause the least number of unexpected side effects.  We re-start
 * the clocks early because we will soon run AML whigh might do DELAY.
 */ 
void
acpi_resume_clocks(struct acpi_softc *sc)
{
#if NISA > 0
	i8259_default_setup();
#endif
	intr_calculatemasks(curcpu());

#if NIOAPIC > 0
	ioapic_enable();
#endif

#if NLAPIC > 0
	lapic_enable();
	if (initclock_func == lapic_initclocks)
		lapic_startclock();
	lapic_set_lvt();
#endif

	i8254_startclock();
	if (initclock_func == i8254_initclocks)
		rtcstart();		/* in i8254 mode, rtc is profclock */
}

/*
 * This function may not have local variables due to a bug between
 * acpi_savecpu() and the resume path.
 */
int
acpi_sleep_cpu(struct acpi_softc *sc, int state)
{
	/*
	 * ACPI defines two wakeup vectors. One is used for ACPI 1.0
	 * implementations - it's in the FACS table as wakeup_vector and
	 * indicates a 32-bit physical address containing real-mode wakeup
	 * code.
	 *
	 * The second wakeup vector is in the FACS table as
	 * x_wakeup_vector and indicates a 64-bit physical address
	 * containing protected-mode wakeup code.
	 */
	sc->sc_facs->wakeup_vector = (u_int32_t)ACPI_TRAMPOLINE;
	if (sc->sc_facs->length > 32 && sc->sc_facs->version >= 1)
		sc->sc_facs->x_wakeup_vector = 0;

	/* Map trampoline and data page */
	pmap_kenter_pa(ACPI_TRAMPOLINE, ACPI_TRAMPOLINE, PROT_READ | PROT_EXEC);
	pmap_kenter_pa(ACPI_TRAMP_DATA, ACPI_TRAMP_DATA,
		PROT_READ | PROT_WRITE);

	/*
	 * Copy the current cpu registers into a safe place for resume.
	 * acpi_savecpu actually returns twice - once in the suspend
	 * path and once in the resume path (see setjmp(3)).
	 */
	if (acpi_savecpu()) {
		/* Suspend path */
		fpusave_cpu(curcpu(), 1);
		wbinvd();

#ifdef HIBERNATE
		if (state == ACPI_STATE_S4) {
			if (hibernate_suspend()) {
				printf("%s: hibernate_suspend failed",
				    DEVNAME(sc));
				hibernate_free();
				return (ECANCELED);
			}
		}
#endif

		/*
		 * XXX
		 * Flag to disk drivers that they should "power down" the disk
		 * when we get to DVACT_POWERDOWN.
		 */
		boothowto |= RB_POWERDOWN;
		config_suspend_all(DVACT_POWERDOWN);
		boothowto &= ~RB_POWERDOWN;

		acpi_sleep_pm(sc, state);
		printf("%s: acpi_sleep_pm failed", DEVNAME(sc));
		return (ECANCELED);
	}
	/* Resume path */

	/* Reset the vectors */
	sc->sc_facs->wakeup_vector = 0;
	if (sc->sc_facs->length > 32 && sc->sc_facs->version >= 1)
		sc->sc_facs->x_wakeup_vector = 0;

	pmap_kremove(ACPI_TRAMPOLINE, PAGE_SIZE);
	pmap_kremove(ACPI_TRAMP_DATA, PAGE_SIZE);

	return (0);
}

void
acpi_resume_cpu(struct acpi_softc *sc)
{
	fpuinit(&cpu_info_primary);

	cpu_init(&cpu_info_primary);

	/* Re-initialise memory range handling on BSP */
	if (mem_range_softc.mr_op != NULL)
		mem_range_softc.mr_op->initAP(&mem_range_softc);
}

#ifdef MULTIPROCESSOR
void
acpi_sleep_mp(void)
{
	int i;

	sched_stop_secondary_cpus();
	KASSERT(CPU_IS_PRIMARY(curcpu()));

	/* 
	 * Wait for cpus to halt so we know their FPU state has been
	 * saved and their caches have been written back.
	 */
	x86_broadcast_ipi(X86_IPI_HALT);
	for (i = 0; i < ncpus; i++) {
		struct cpu_info *ci = cpu_info[i];

		if (CPU_IS_PRIMARY(ci))
			continue;
		while (ci->ci_flags & CPUF_RUNNING)
			;
	}
}

void
acpi_resume_mp(void)
{
	void	cpu_start_secondary(struct cpu_info *ci);
	struct cpu_info *ci;
	struct proc *p;
	struct pcb *pcb;
	struct trapframe *tf;
	struct switchframe *sf;
	int i;

	/* XXX refactor with matching code in cpu.c */

	for (i = 0; i < MAXCPUS; i++) {
		ci = cpu_info[i];
		if (ci == NULL)
			continue;
		if (ci->ci_idle_pcb == NULL)
			continue;
		if (ci->ci_flags & (CPUF_BSP|CPUF_SP|CPUF_PRIMARY))
			continue;
		KASSERT((ci->ci_flags & CPUF_RUNNING) == 0);

		p = ci->ci_schedstate.spc_idleproc;
		pcb = &p->p_addr->u_pcb;

		tf = (struct trapframe *)pcb->pcb_kstack - 1;
		sf = (struct switchframe *)tf - 1;
		sf->sf_r12 = (u_int64_t)sched_idle;
		sf->sf_r13 = (u_int64_t)ci;
		sf->sf_rip = (u_int64_t)proc_trampoline;
		pcb->pcb_rsp = (u_int64_t)sf;
		pcb->pcb_rbp = 0;

		ci->ci_idepth = 0;
		ci->ci_handled_intr_level = IPL_NONE;

		ci->ci_flags &= ~CPUF_PRESENT;
		cpu_start_secondary(ci);
	}

	cpu_boot_secondary_processors();
	sched_start_secondary_cpus();
}
#endif /* MULTIPROCESSOR */

#endif /* ! SMALL_KERNEL */
