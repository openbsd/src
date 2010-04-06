/*	$OpenBSD: ip30_machdep.c,v 1.39 2010/04/06 19:02:47 miod Exp $	*/

/*
 * Copyright (c) 2008, 2009 Miodrag Vallat.
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
 * Octane (IP30) specific code.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/tty.h>

#include <mips64/arcbios.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/memconf.h>

#include <uvm/uvm_extern.h>

#include <sgi/sgi/ip30.h>
#include <sgi/xbow/widget.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>
#include <sgi/xbow/xbridgereg.h>	/* BRIDGE_PCI0_MEM_SPACE_BASE */

#include <sgi/xbow/xheartreg.h>
#include <sgi/pci/iocreg.h>

#include <dev/ic/comvar.h>

#ifdef MULTIPROCESSOR
#include <sgi/xbow/xheartreg.h>
#endif

extern char *hw_prod;

extern int	mbprint(void *, const char *);

#ifdef MULTIPROCESSOR
extern int      xheart_intr_establish(int (*)(void *), void *, int, int, 
    const char *, struct intrhand *);
extern void     xheart_intr_set(int);
extern void     xheart_intr_clear(int);
extern void	xheart_setintrmask(int);

extern struct	user *proc0paddr;
#endif

uint32_t ip30_lights_frob(uint32_t, struct trap_frame *);
paddr_t	ip30_widget_short(int16_t, u_int);
paddr_t	ip30_widget_long(int16_t, u_int);
paddr_t	ip30_widget_map(int16_t, u_int, bus_addr_t *, bus_size_t *);
int	ip30_widget_id(int16_t, u_int, uint32_t *);
static u_long ip30_get_ncpusfound(void);

#ifdef DDB
void	ip30_nmi(void);			/* ip30_nmi.S */
void	ip30_nmi_handler(void);
#endif

static	paddr_t ip30_iocbase;

static const paddr_t mpconf =
    PHYS_TO_XKPHYS(MPCONF_BASE, CCA_COHERENT_EXCLWRITE);

static int ip30_cpu_exists(int);

void
ip30_setup()
{
	paddr_t heart;
	int bank;
	uint32_t memcfg;
	uint64_t start, count, end;
	u_long cpuspeed;
#ifdef DDB
	struct ip30_gda *gda;
#endif

	/*
	 * Although being r10k/r12k based, the uncached spaces are
	 * apparently not used in this design.
	 */
	uncached_base = PHYS_TO_XKPHYS(0, CCA_NC);

	/*
	 * Scan for memory. ARCBios reports up to 1GB of memory as available,
	 * and anything after is reported as reserved.
	 */
	heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC);
	for (bank = 0; bank < 8; bank++) {
		memcfg = *(volatile uint32_t *)
		    (heart + HEART_MEMORY_STATUS + bank * sizeof(uint32_t));
#ifdef DEBUG
		bios_printf("memory bank %d: %08x\n", bank, memcfg);
#endif

		if (!ISSET(memcfg, HEART_MEMORY_VALID))
			continue;

		count = ((memcfg & HEART_MEMORY_SIZE_MASK) >>
		    HEART_MEMORY_SIZE_SHIFT) + 1;
		start = (memcfg & HEART_MEMORY_ADDR_MASK) >>
		    HEART_MEMORY_ADDR_SHIFT;

		count <<= HEART_MEMORY_UNIT_SHIFT;
		start <<= HEART_MEMORY_UNIT_SHIFT;

		/* Physical memory starts at 512MB */
		start += IP30_MEMORY_BASE;
		end = start + count;
#ifdef DEBUG
		bios_printf("memory from %p to %p\n",
		    start, end);
#endif

		/*
		 * Add memory not obtained through ARCBios.
		 */
		if (start >= IP30_MEMORY_BASE + IP30_MEMORY_ARCBIOS_LIMIT) {
			/*
			 * XXX Temporary until there is a way to cope with
			 * XXX xbridge ATE shortage.
			 */
			if (end > (2UL << 30)) {
#if 0
				physmem += atop(end - (2UL << 30));
#endif
				end = 2UL << 30;
			}
			if (end <= start)
				continue;

			memrange_register(atop(start), atop(end),
			    0, VM_FREELIST_DEFAULT);
		}
	}


	xbow_widget_base = ip30_widget_short;
	xbow_widget_map = ip30_widget_map;
	xbow_widget_id = ip30_widget_id;

	bootcpu_hwinfo.c0prid = cp0_get_prid();
	bootcpu_hwinfo.c1prid = cp1_get_prid();
	cpuspeed = bios_getenvint("cpufreq");
	if (cpuspeed < 100)
		cpuspeed = 175;		/* reasonable default */
	bootcpu_hwinfo.clock = cpuspeed * 1000000;
	bootcpu_hwinfo.tlbsize = 64;	/* R10000 family */
	bootcpu_hwinfo.type = (bootcpu_hwinfo.c0prid >> 8) & 0xff;

	/*
	 * Initialize the early console parameters.
	 * On Octane, the BRIDGE is always widget 15, and IOC3 is always
	 * mapped in memory space at address 0x500000.
	 *
	 * Also, note that by using a direct widget bus_space, there is
	 * no endianness conversion done on the bus addresses. Which is
	 * exactly what we need, since the IOC3 doesn't need any. Some
	 * may consider this an evil abuse of bus_space knowledge, though.
	 */

	xbow_build_bus_space(&sys_config.console_io, 0, 15);
	sys_config.console_io.bus_base = ip30_widget_long(0, 15) +
	    BRIDGE_PCI0_MEM_SPACE_BASE + 0x500000;

	comconsaddr = IOC3_UARTA_BASE;
	comconsfreq = 22000000 / 3;
	comconsiot = &sys_config.console_io;
	comconsrate = bios_getenvint("dbaud");
	if (comconsrate < 50 || comconsrate > 115200)
		comconsrate = 9600;

#ifdef DDB
	/*
	 * Setup NMI handler.
	 */
	gda = (struct ip30_gda *)PHYS_TO_XKPHYS(GDA_BASE, CCA_CACHED);
	if (gda->magic == GDA_MAGIC)
		gda->nmi_cb = ip30_nmi;
#endif

	/*
	 * Octane and Octane2 can be told apart with a GPIO source bit
	 * in the onboard IOC3.
	 */
	ip30_iocbase = sys_config.console_io.bus_base;
	if (*(volatile uint32_t *)
	    (ip30_iocbase + IOC3_GPPR(IP30_GPIO_CLASSIC)) != 0)
		hw_prod = "Octane";
	else
		hw_prod = "Octane2";

	ncpusfound = ip30_get_ncpusfound();
}

/*
 * Autoconf enumeration
 */

void
ip30_autoconf(struct device *parent)
{
	struct cpu_attach_args caa;
#ifdef MULTIPROCESSOR
	struct cpu_hwinfo hw;
	int cpuid;
#endif

	bzero(&caa, sizeof caa);
	caa.caa_maa.maa_nasid = masternasid;
	caa.caa_maa.maa_name = "cpu";
	caa.caa_hw = &bootcpu_hwinfo;
	config_found(parent, &caa, mbprint);

#ifdef MULTIPROCESSOR
	for (cpuid = 1; cpuid < IP30_MAXCPUS; cpuid++)
		if (ip30_cpu_exists(cpuid)) {
			/*
			 * Attach other processors with the same hardware
			 * information as the boot processor, unless we
			 * can get this information from the MPCONF area;
			 * since Octane processors should be identical
			 * (model, speed and cache), this should be safe.
			 */
			bcopy(&bootcpu_hwinfo, &hw, sizeof(struct cpu_hwinfo));
			hw.c0prid = 
		           *(volatile uint32_t *)(mpconf + MPCONF_PRID(cpuid));
			hw.type = (hw.c0prid >> 8) & 0xff;
			hw.l2size = 1 << *(volatile uint32_t *)
			    (mpconf + MPCONF_SCACHESZ(cpuid));
			caa.caa_hw = &hw;
			config_found(parent, &caa, mbprint);
		}
#endif

	caa.caa_maa.maa_name = "clock";
	config_found(parent, &caa.caa_maa, mbprint);
	caa.caa_maa.maa_name = "xbow";
	config_found(parent, &caa.caa_maa, mbprint);
	caa.caa_maa.maa_name = "power";
	config_found(parent, &caa.caa_maa, mbprint);
}

/*
 * Widget mapping. IP30 only has one processor board node, so the nasid
 * parameter is ignored.
 */

paddr_t
ip30_widget_short(int16_t nasid, u_int widget)
{
	return ((uint64_t)(widget) << 24) | (1ULL << 28) | uncached_base;
}

paddr_t
ip30_widget_long(int16_t nasid, u_int widget)
{
	return ((uint64_t)(widget) << 36) | uncached_base;
}

paddr_t
ip30_widget_map(int16_t nasid, u_int widget, bus_addr_t *offs, bus_size_t *len)
{
	paddr_t base;

	/*
	 * On Octane, the whole widget space is always accessible.
	 */

	base = ip30_widget_long(nasid, widget);
	*len = (1ULL << 36) - *offs;

	return base + *offs;
}

/*
 * Widget enumeration
 */

int
ip30_widget_id(int16_t nasid, u_int widget, uint32_t *wid)
{
	paddr_t linkpa, wpa;

	if (widget != 0)
	{
		if (widget < WIDGET_MIN || widget > WIDGET_MAX)
			return EINVAL;

		linkpa = ip30_widget_short(nasid, 0) + XBOW_WIDGET_LINK(widget);
		if (!ISSET(*(uint32_t *)(linkpa + (WIDGET_LINK_STATUS | 4)),
		    WIDGET_STATUS_ALIVE))
			return ENXIO;	/* not connected */
	}

	wpa = ip30_widget_short(nasid, widget);
	if (wid != NULL)
		*wid = *(uint32_t *)(wpa + (WIDGET_ID | 4));

	return 0;
}

/*
 * Figure out which video widget to use.
 *
 * If we are running with glass console, ConsoleOut will be `video(#)' with
 * the optional number being the number of the video device in the ARCBios
 * component tree.
 *
 * Unfortunately, we do not know how to match an ARCBios component to a
 * given widget (the PROM can... it's just not sharing this with us).
 *
 * So simply walk the available widget space and count video devices.
 */

int
ip30_find_video()
{
	uint widid, head;
	uint32_t id, vendor, product;
	char *p;

	if (strncmp(bios_console, "video", 5) != 0)
		return 0;	/* not graphics console */

	p = bios_console + 5;
	switch (*p) {
	case '(':
		/* 8 widgets max -> single digit */
		p++;
		if (*p == ')')
			head = 0;
		else {
			if (*p < '0' || *p > '9')
				return 0;
			head = *p++ - '0';
			if (*p != ')')
				return 0;
		}
		break;
	case '\0':
		head = 0;
		break;
	default:
		return 0;
	}

	for (widid = WIDGET_MAX; widid >= WIDGET_MIN; widid--) {
		if (ip30_widget_id(0, widid, &id) != 0)
			continue;

		vendor = WIDGET_ID_VENDOR(id);
		product = WIDGET_ID_PRODUCT(id);

		if ((vendor == XBOW_VENDOR_SGI2 &&
		    product == XBOW_PRODUCT_SGI2_ODYSSEY) ||
		    (vendor == XBOW_VENDOR_SGI5 &&
		    product == XBOW_PRODUCT_SGI5_IMPACT) ||
		    (vendor == XBOW_VENDOR_SGI5 &&
		    product == XBOW_PRODUCT_SGI5_KONA)) {
			/* found a video device */
			if (head == 0)
				return widid;
			head--;
		}
	}

	return 0;
}

/*
 * Fun with the lightbar
 */
uint32_t
ip30_lights_frob(uint32_t hwpend, struct trap_frame *cf)
{
	uint32_t gpioold, gpio;

	/* Light bar status: idle - white, user - red, system - both */

	gpio = gpioold = *(volatile uint32_t *)(ip30_iocbase + IOC3_GPDR);
	gpio &= ~((1 << IP30_GPIO_WHITE_LED) | (1 << IP30_GPIO_RED_LED));

	if (cf->sr & SR_KSU_USER)
		gpio |= (1 << IP30_GPIO_RED_LED);
	else {
		gpio |= (1 << IP30_GPIO_WHITE_LED);

		/* XXX SMP check other CPU is unidle */
		if (curproc != curcpu()->ci_schedstate.spc_idleproc)
			gpio |= (1 << IP30_GPIO_RED_LED);
	}

	if (gpio != gpioold)
		*(volatile uint32_t *)(ip30_iocbase + IOC3_GPDR) = gpio;

	return 0;	/* Real clock int handler will claim the interrupt. */
}

static int
ip30_cpu_exists(int cpuid)
{
	uint32_t magic =
	    *(volatile uint32_t *)(mpconf + MPCONF_MAGIC(cpuid));
	return magic == MPCONF_MAGIC_VAL;
}

u_long
ip30_get_ncpusfound(void)
{
	int i;
	int ncpus = 0;

	for (i = 0; i < IP30_MAXCPUS; i++)
		if (ip30_cpu_exists(i))
			ncpus++;

	return ncpus;
}

#ifdef DDB
void
ip30_nmi_handler()
{
	extern int kdb_trap(int, struct trap_frame *);
	extern void stacktrace(struct trap_frame *);
	struct trap_frame *fr0;
	int s;
#ifdef MULTIPROCESSOR
	struct trap_frame *fr1;
	struct cpu_info *ci = curcpu();
#endif

	setsr(getsr() & ~SR_BOOT_EXC_VEC);

	s = splhigh();
#ifdef MULTIPROCESSOR
	ENABLEIPI();

	if (!CPU_IS_PRIMARY(ci)) {
		for (;;) ;
	}
#endif

	printf("NMI\n");

	fr0 = (struct trap_frame *)PHYS_TO_XKPHYS(IP30_MEMORY_BASE + 0x4000,
	    CCA_CACHED);
#ifdef MULTIPROCESSOR
	fr1 = (struct trap_frame *)PHYS_TO_XKPHYS(IP30_MEMORY_BASE + 0x6000,
	    CCA_CACHED);
#endif

#ifdef MULTIPROCESSOR
	printf("cpu #0 traceback\n");
#endif
	stacktrace(fr0);
#ifdef MULTIPROCESSOR
	printf("cpu #1 traceback\n");
	stacktrace(fr1);
#endif

	kdb_trap(-1, fr0);

	splx(s);
	printf("Resetting system...\n");
	boot(RB_USERREQ);
}
#endif

#ifdef MULTIPROCESSOR
void
hw_cpu_boot_secondary(struct cpu_info *ci)
{
	int cpuid =  ci->ci_cpuid;
	vaddr_t kstack;

#ifdef DEBUG
	uint64_t stackaddr =
		*(volatile uint64_t *)(mpconf + MPCONF_STACKADDR(cpuid));
	uint64_t lparam =
		*(volatile uint64_t *)(mpconf + MPCONF_LPARAM(cpuid));
	uint64_t launch =
		*(volatile uint64_t *)(mpconf + MPCONF_LAUNCH(cpuid));
	uint32_t magic =
		*(volatile uint32_t *)(mpconf + MPCONF_MAGIC(cpuid));
	uint32_t prid =
		*(volatile uint32_t *)(mpconf + MPCONF_PRID(cpuid));
	uint32_t physid =
		*(volatile uint32_t *)(mpconf + MPCONF_PHYSID(cpuid));
	uint32_t virtid =
		*(volatile uint32_t *)(mpconf + MPCONF_VIRTID(cpuid));
	uint32_t scachesz =
		*(volatile uint32_t *)(mpconf + MPCONF_SCACHESZ(cpuid));
	uint16_t fanloads =
		*(volatile uint16_t *)(mpconf + MPCONF_FANLOADS(cpuid));
	uint64_t rndvz =
		*(volatile uint64_t *)(mpconf + MPCONF_RNDVZ(cpuid));
	uint64_t rparam =
		*(volatile uint64_t *)(mpconf + MPCONF_RPARAM(cpuid));
	uint32_t idleflag =
		*(volatile uint32_t *)(mpconf + MPCONF_IDLEFLAG(cpuid));

	printf("ci:%p cpuid:%d magic:%x prid:%x physid:%x virtid:%x\n"
	    "scachesz:%u fanloads:%x launch:%llx rndvz:%llx\n"
	    "stackaddr:%llx lparam:%llx rparam:%llx idleflag:%x\n",
	    ci, cpuid, magic, prid, physid, virtid,
	    scachesz, fanloads, launch, rndvz,
	    stackaddr, lparam, rparam, idleflag);
#endif
	kstack = smp_malloc(USPACE);
	if (kstack == NULL)
		panic("unable to allocate idle stack\n");
	bzero((char *)kstack, USPACE);
	ci->ci_curprocpaddr = (void *)kstack;

	*(volatile uint64_t *)(mpconf + MPCONF_STACKADDR(cpuid)) =
	    (uint64_t)(kstack + USPACE);
	*(volatile uint64_t *)(mpconf + MPCONF_LPARAM(cpuid)) =
	    (uint64_t)ci;
	*(volatile uint64_t *)(mpconf + MPCONF_LAUNCH(cpuid)) =
	    (uint64_t)hw_cpu_spinup_trampoline;

	while (!cpuset_isset(&cpus_running, ci))
		;
}

void
hw_cpu_hatch(struct cpu_info *ci)
{
	int s;

	/*
	 * Make sure we can access the extended address space.
	 * Note that r10k and later do not allow XUSEG accesses
	 * from kernel mode unless SR_UX is set.
	 */
	setsr(getsr() | SR_KX | SR_UX);

	tlb_set_page_mask(TLB_PAGE_MASK);
	tlb_set_wired(0);
	tlb_flush(64);
	tlb_set_wired(UPAGES / 2);

	tlb_set_pid(0);

	/*
	 * Turn off bootstrap exception vectors.
	 */
	setsr(getsr() & ~SR_BOOT_EXC_VEC);

	/*
	 * Clear out the I and D caches.
	 */
	Mips10k_ConfigCache(ci);
	Mips_SyncCache(ci);

	cpu_startclock(ci);

	ncpus++;
	cpuset_add(&cpus_running, ci);

	mips64_ipi_init();
	xheart_setintrmask(0);

	spl0();
	(void)updateimask(0);

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
}

int
hw_ipi_intr_establish(int (*func)(void *), u_long cpuid)
{
	return xheart_intr_establish(func, (void *)cpuid, HEART_ISR_IPI(cpuid), 
	    IPL_IPI, NULL, &curcpu()->ci_ipiih);
};

void
hw_ipi_intr_set(u_long cpuid)
{
	xheart_intr_set(HEART_ISR_IPI(cpuid));
}

void
hw_ipi_intr_clear(u_long cpuid)
{
	xheart_intr_clear(HEART_ISR_IPI(cpuid));
}

#endif
