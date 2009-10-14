/*	$OpenBSD: ip27_machdep.c,v 1.22 2009/10/14 20:21:16 miod Exp $	*/

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
 * Origin 200 / Origin 2000 / Onyx 2 (IP27), as well as
 * Origin 300 / Onyx 300 / Origin 350 / Onyx 350 / Onyx 4 / Origin 3000 /
 * Fuel / Tezro (IP35) specific code.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/reboot.h>
#include <sys/tty.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/memconf.h>
#include <machine/mnode.h>

#include <uvm/uvm_extern.h>

#include <sgi/sgi/ip27.h>
#include <sgi/xbow/hub.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbridgereg.h>

#include <dev/ic/comvar.h>

extern char *hw_prod;

extern void (*md_halt)(int);

paddr_t	ip27_widget_short(int16_t, u_int);
paddr_t	ip27_widget_long(int16_t, u_int);
paddr_t	ip27_widget_map(int16_t, u_int,bus_addr_t *, bus_size_t *);
int	ip27_widget_id(int16_t, u_int, uint32_t *);

void	ip27_halt(int);

unsigned int xbow_long_shift = 29;

static paddr_t io_base;
static gda_t *gda;
static int ip35 = 0;
static uint maxnodes;

int	ip27_hub_intr_register(int, int, int *);
int	ip27_hub_intr_establish(int (*)(void *), void *, int, int,
	    const char *);
void	ip27_hub_intr_disestablish(int);
intrmask_t ip27_hub_intr_handler(intrmask_t, struct trap_frame *);
void	ip27_hub_intr_makemasks(void);
void	ip27_hub_do_pending_int(int);

void	ip27_attach_node(struct device *, int16_t);
int	ip27_print(void *, const char *);
void	ip27_nmi(void *);

void
ip27_setup()
{
	size_t gsz;
	uint node;
	uint32_t ctrl;
	uint64_t synergy0_0;
	int brick;
	nmi_t *nmi;

	uncached_base = PHYS_TO_XKPHYS_UNCACHED(0, SP_NC);
	io_base = PHYS_TO_XKPHYS_UNCACHED(0, SP_IO);

	ip35 = sys_config.system_type == SGI_O300;

	if (ip35) {
		/*
		 * Get brick model type.
		 * We need to access the Synergy registers through the remote
		 * HUB interface, local access is protected on some models.
		 * Synergy0 register #0 is a 16 bits identification register.
		 */
		synergy0_0 = IP27_RHSPEC_L(0, HSPEC_SYNERGY(0, 0));
		brick = (synergy0_0 & 0xf000) >> 12;
		switch (brick) {
		case 0x02:	/* Chimera */
			hw_prod = "Origin 350";
			break;
		case 0x04:	/* Asterix */
			hw_prod = "Fuel";
			break;
		case 0x08:	/* Speedo2 */
			hw_prod = "Origin 300";
			break;
		default:
		    {
			static char unknown_model[20];
			snprintf(unknown_model, sizeof unknown_model,
			    "Unknown IP35 type %x", brick);
			hw_prod = unknown_model;
		    }
			break;
		}
	} else {
		/*
		 * XXX need to look for Sn00 type in LBOOT space to tell
		 * XXX Origin 2000 and Origin 200 apart.
		 */
		hw_prod = "Origin 200";
	}

	xbow_widget_base = ip27_widget_short;
	xbow_widget_map = ip27_widget_map;
	xbow_widget_id = ip27_widget_id;

	md_halt = ip27_halt;

	/*
	 * Figure out as early as possibly whether we are running in M
	 * or N mode.
	 */

	kl_init(ip35);
	if (kl_n_mode != 0)
		xbow_long_shift = 28;

	/*
	 * Get a grip on the global data area, and figure out how many
	 * theoretical nodes are available.
	 */

	gda = IP27_GDA(0);
	gsz = IP27_GDA_SIZE(0);
	if (gda->magic != GDA_MAGIC || gda->ver < 2) {
		masternasid = 0;
		maxnodes = 0;
	} else {
		masternasid = gda->masternasid;
		maxnodes = (gsz - offsetof(gda_t, nasid)) / sizeof(int16_t);
		if (maxnodes > GDA_MAXNODES)
			maxnodes = GDA_MAXNODES;
		/* in M mode, there can't be more than 64 nodes anyway */
		if (kl_n_mode == 0 && maxnodes > 64)
			maxnodes = 64;
	}

	/*
	 * Scan all nodes configurations to find out CPU and memory
	 * information, starting with the master node.
	 */

	kl_scan_config(masternasid);
	for (node = 0; node < maxnodes; node++) {
		if (gda->nasid[node] < 0)
			continue;
		if (gda->nasid[node] == masternasid)
			continue;
		kl_scan_config(gda->nasid[node]);
	}
	kl_scan_done();

	/*
	 * Initialize the early console parameters.
	 * This assumes it is either on IOC3 or IOC4, accessible through
	 * a widget small window.
	 *
	 * Since IOC3 and IOC4 use different clocks, we need to tell them
	 * apart early. We rely on the serial port offset within the IOC
	 * space.
	 */

	xbow_build_bus_space(&sys_config.console_io, 0,
	    8 /* whatever nonzero */);
	/* Constrain to the correct window */
	sys_config.console_io.bus_base =
	    kl_get_console_base() & 0xffffffffff000000UL;

	comconsaddr = kl_get_console_base() & 0x0000000000ffffffUL;
	if ((comconsaddr & 0xfff) < 0x380) {
		/* IOC3 */
		comconsfreq = 22000000 / 3;
		bios_printf("IOC3 style console\n");
	} else {
		/* IOC4 */
		/*
		 * IOC4 clocks are derived from the PCI clock, so we need to
		 * figure out whether this is an 66MHz or a 33MHz bus.
		 * Note that this assumes the IOC4 is connected to a Bridge
		 * or PIC widget, and that even if this is a PIC widget,
		 * the common widget register space can be correctly read
		 * with non-doubleword aligned word reads.
		 */
		comconsfreq = 66666667;
		bios_printf("IOC4 style console\n");
		ctrl = *(volatile uint32_t *)
		    ((sys_config.console_io.bus_base + WIDGET_CONTROL) | 4);
		switch (ctrl & BRIDGE_WIDGET_CONTROL_SPEED_MASK) {
		default:
			bios_printf("WARNING! UNRECOGNIZED IOC4 SPEED\n"
			    "ASSUMING 66MHZ\n");
			break;
		case BRIDGE_WIDGET_CONTROL_SPEED_66MHZ:
			break;
		case BRIDGE_WIDGET_CONTROL_SPEED_33MHZ:
			comconsfreq >>= 1;
			break;
		}
	}
	comconsiot = &sys_config.console_io;

	/*
	 * Force widget interrupts to run through us, unless a
	 * better interrupt master widget is found.
	 */

	xbow_intr_widget_intr_register = ip27_hub_intr_register;
	xbow_intr_widget_intr_establish = ip27_hub_intr_establish;
	xbow_intr_widget_intr_disestablish = ip27_hub_intr_disestablish;

	set_intr(INTPRI_XBOWMUX, CR_INT_0, ip27_hub_intr_handler);
	register_pending_int_handler(ip27_hub_do_pending_int);

	/*
	 * Disable all hardware interrupts.
	 */

	IP27_LHUB_S(HUBPI_CPU0_IMR0, 0);
	IP27_LHUB_S(HUBPI_CPU0_IMR1, 0);
	IP27_LHUB_S(HUBPI_CPU1_IMR0, 0);
	IP27_LHUB_S(HUBPI_CPU1_IMR1, 0);
	(void)IP27_LHUB_L(HUBPI_IR0);
	(void)IP27_LHUB_L(HUBPI_IR1);
	/* XXX do the other two cpus on IP35 */

	/*
	 * Setup NMI handler.
	 */
	nmi = IP27_NMI(0);
	nmi->magic = NMI_MAGIC;
	nmi->cb = (vaddr_t)ip27_nmi;
	nmi->cb_complement = ~nmi->cb;
	nmi->cb_arg = 0;

	/*
	 * Set up Node 0's HUB.
	 */
	IP27_LHUB_S(HUBPI_REGION_PRESENT, 0xffffffffffffffff);
	IP27_LHUB_S(HUBPI_CALIAS_SIZE, PI_CALIAS_SIZE_0);
}

/*
 * Autoconf enumeration
 */

void
ip27_autoconf(struct device *parent)
{
	struct confargs nca;
	uint node;

	/*
	 * Attach the CPU we are running on early; other processors,
	 * if any, will get attached as they are discovered.
	 */

	bzero(&nca, sizeof nca);
	nca.ca_nasid = masternasid;
	nca.ca_name = "cpu";
	config_found(parent, &nca, ip27_print);
	nca.ca_name = "clock";
	config_found(parent, &nca, ip27_print);

	/*
	 * Now attach all nodes' I/O devices.
	 */

	ip27_attach_node(parent, masternasid);
	for (node = 0; node < maxnodes; node++) {
		if (gda->nasid[node] < 0)
			continue;
		if (gda->nasid[node] == masternasid)
			continue;
		ip27_attach_node(parent, gda->nasid[node]);
	}
}

void
ip27_attach_node(struct device *parent, int16_t nasid)
{
	struct confargs nca;

	bzero(&nca, sizeof nca);
	nca.ca_name = "xbow";
	nca.ca_nasid = nasid;
	config_found(parent, &nca, ip27_print);
}

int
ip27_print(void *aux, const char *pnp)
{
	struct confargs *ca = aux;

	printf(" nasid %d", ca->ca_nasid);

	return UNCONF;
}

/*
 * Widget mapping.
 */

paddr_t
ip27_widget_short(int16_t nasid, u_int widget)
{
	/*
	 * A hardware bug on the PI side of the Hub chip (at least in
	 * earlier versions) causes accesses to the short window #0
	 * to be unreliable.
	 * The PROM implements a workaround by remapping it to
	 * big window #6 (the last programmable big window).
	 */
	if (widget == 0)
		return ip27_widget_long(nasid, IOTTE_SWIN0);

	return ((uint64_t)(widget) << 24) |
	    ((uint64_t)(nasid) << kl_n_shift) | io_base;
}

paddr_t
ip27_widget_long(int16_t nasid, u_int window)
{
	return ((uint64_t)(window + 1) << xbow_long_shift) |
	    ((uint64_t)(nasid) << kl_n_shift) | io_base;
}

paddr_t
ip27_widget_map(int16_t nasid, u_int widget, bus_addr_t *offs, bus_size_t *len)
{
	uint tte, avail_tte;
	uint64_t iotte;
	paddr_t delta, start, end;
	int s;

	/*
	 * On Origin systems, we can only have partial views of the widget
	 * address space, due to the addressing scheme limiting each node's
	 * address space to 31 to 33 bits.
	 *
	 * The largest window is 256MB or 512MB large, depending on the
	 * mode the system is in (M/N).
	 */

	/*
	 * Round the requested range to a large window boundary.
	 */

	start = *offs;
	end = start + *len;

	start = (start >> xbow_long_shift);
	end = (end + (1 << xbow_long_shift) - 1) >> xbow_long_shift;

	/*
	 * See if an existing IOTTE covers part of the mapping we are asking
	 * for.  If so, reuse it and truncate the caller's range.
	 */

	s = splhigh();	/* XXX or disable interrupts completely? */

	avail_tte = IOTTE_MAX;
	for (tte = 0; tte < IOTTE_MAX; tte++) {
		if (tte == IOTTE_SWIN0)
			continue;

		iotte = IP27_RHUB_L(nasid, HUBIOBASE + HUBIO_IOTTE(tte));
		if (IOTTE_WIDGET(iotte) == 0) {
			if (avail_tte == IOTTE_MAX)
				avail_tte = tte;
			continue;
		}
		if (IOTTE_WIDGET(iotte) != widget)
			continue;

		if (IOTTE_OFFSET(iotte) < start ||
		    (IOTTE_OFFSET(iotte) + 1) >= end)
			continue;

		/*
		 * We found a matching IOTTE (an exact match if we asked for
		 * less than the large window size, a partial match otherwise).
		 * Reuse it (since we never unmap IOTTE at this point, there
		 * is no need to maintain a reference count).
		 */
		break;
	}

	/*
	 * If we found an unused IOTTE while searching above, program it
	 * to map the beginning of the requested range.
	 */

	if (tte == IOTTE_MAX && avail_tte != IOTTE_MAX) {
		tte = avail_tte;

		/* XXX I don't understand why it's not device space. */
		iotte = IOTTE(IOTTE_SPACE_MEMORY, widget, start);
		IP27_RHUB_S(nasid, HUBIOBASE + HUBIO_IOTTE(tte), iotte);
		(void)IP27_RHUB_L(nasid, HUBIOBASE + HUBIO_IOTTE(tte));
	}

	splx(s);

	if (tte != IOTTE_MAX) {
		delta = *offs - (start << xbow_long_shift);
		/* *offs unmodified */
		*len = (1 << xbow_long_shift) - delta;

		return ip27_widget_long(nasid, tte) + delta;
	}

	return 0UL;
}

/*
 * Widget enumeration
 */

int
ip27_widget_id(int16_t nasid, u_int widget, uint32_t *wid)
{
	paddr_t wpa;
	uint32_t id;

	if (widget != 0)
	{
		if (widget < WIDGET_MIN || widget > WIDGET_MAX)
			return EINVAL;
	}

	wpa = ip27_widget_short(nasid, widget);
	if (guarded_read_4(wpa + (WIDGET_ID | 4), &id) != 0)
		return ENXIO;

	if (wid != NULL)
		*wid = id;

	return 0;
}

/*
 * Reboot code
 */

void
ip27_halt(int howto)
{
	uint32_t promop;
	uint node;
	uint64_t nibase, action;

	/*
	 * Even if ARCBios TLB and exception vectors are restored,
	 * returning to ARCBios doesn't work.
	 *
	 * So, instead, send a reset through the network interface
	 * of the Hub space.  Although there seems to be a way to tell
	 * the PROM which action we want it to take afterwards, it
	 * always reboots for me...
	 */

	if (howto & RB_HALT) {
#if 0
		if (howto & RB_POWERDOWN)
			promop = GDA_PROMOP_HALT;
		else
			promop = GDA_PROMOP_EIM;
#else
		if (howto & RB_POWERDOWN)
			printf("Software powerdown not supported, "
			    "please switch off power manually.\n");
		for (;;) ;
		/* NOTREACHED */
#endif
	} else
		promop = GDA_PROMOP_REBOOT;

	promop |= GDA_PROMOP_MAGIC | GDA_PROMOP_NO_DIAGS |
	    GDA_PROMOP_NO_MEMINIT;

#if 0
	/*
	 * That's what one would expect, based on the gda layout...
	 */
	gda->promop = promop;
#else
	/*
	 * ...but the magic location is in a different castle.
	 * And it's not even the same between IP27 and IP35.
	 * Laugh, everyone! It's what SGI wants us to.
	 */
	if (ip35)
		IP27_LHUB_S(HUBLBBASE_IP35 + 0x8010, promop);
	else
		IP27_LHUB_S(HUBPIBASE + 0x418, promop);
#endif

	if (ip35) {
		nibase = HUBNIBASE_IP35;
		action = NI_RESET_LOCAL_IP35 | NI_RESET_ACTION_IP35;
	} else {
		nibase = HUBNIBASE_IP27;
		action = NI_RESET_LOCAL_IP27 | NI_RESET_ACTION_IP27;
	}

	/*
	 * Reset all other nodes, if present.
	 */

	for (node = 0; node < maxnodes; node++) {
		if (gda->nasid[node] < 0)
			continue;
		if (gda->nasid[node] == masternasid)
			continue;
		IP27_RHUB_S(gda->nasid[node],
		    nibase + HUBNI_RESET_ENABLE, NI_RESET_ENABLE);
		IP27_RHUB_S(gda->nasid[node],
		    nibase + HUBNI_RESET, action);
	}
	IP27_LHUB_S(nibase + HUBNI_RESET_ENABLE, NI_RESET_ENABLE);
	IP27_LHUB_S(nibase + HUBNI_RESET, action);
}

/*
 * Local HUB interrupt handling routines
 */

uint64_t ip27_hub_intrmask;

/*
 * Find a suitable interrupt bit for the given interrupt.
 */
int
ip27_hub_intr_register(int widget, int level, int *intrbit)
{
	int bit;

	/*
	 * All interrupts will be serviced at hardware level 0,
	 * so the `level' argument can be ignored.
	 * On HUB, the low 7 bits of the level 0 interrupt register
	 * are reserved.
	 */
	for (bit = SPL_CLOCK - 1; bit >= 7; bit--)
		if ((ip27_hub_intrmask & (1 << bit)) == 0)
			break;

	if (bit < 7)
		return EINVAL;

	*intrbit = bit;
	return 0;
}

/*
 * Register an interrupt handler for a given source, and enable it.
 */
int
ip27_hub_intr_establish(int (*func)(void *), void *arg, int intrbit,
    int level, const char *name)
{
	struct intrhand *ih;

#ifdef DIAGNOSTIC
	if (intrbit < 0 || intrbit >= SPL_CLOCK)
		return EINVAL;
#endif

	/*
	 * Widget interrupts are not supposed to be shared - the interrupt
	 * mask is large enough for all widgets.
	 */
	if (intrhand[intrbit] != NULL)
		return EEXIST;

	ih = malloc(sizeof(*ih), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return ENOMEM;

	ih->ih_next = NULL;
	ih->ih_fun = func;
	ih->ih_arg = arg;
	ih->ih_level = level;
	ih->ih_irq = intrbit;
	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_level,
		    &evcount_intr);
	intrhand[intrbit] = ih;

	ip27_hub_intrmask |= 1UL << intrbit;
	ip27_hub_intr_makemasks();

	/* XXX this assumes we run on cpu0 */
	IP27_LHUB_S(HUBPI_CPU0_IMR0,
	    IP27_LHUB_L(HUBPI_CPU0_IMR0) | (1UL << intrbit));
	(void)IP27_LHUB_L(HUBPI_IR0);

	return 0;
}

void
ip27_hub_intr_disestablish(int intrbit)
{
	struct intrhand *ih;
	int s;

#ifdef DIAGNOSTIC
	if (intrbit < 0 || intrbit >= SPL_CLOCK)
		return;
#endif

	s = splhigh();

	if ((ih = intrhand[intrbit]) == NULL) {
		splx(s);
		return;
	}

	/* XXX this assumes we run on cpu0 */
	IP27_LHUB_S(HUBPI_CPU0_IMR0,
	    IP27_LHUB_L(HUBPI_CPU0_IMR0) & ~(1UL << intrbit));
	(void)IP27_LHUB_L(HUBPI_IR0);

	intrhand[intrbit] = NULL;

	ip27_hub_intrmask &= ~(1UL << intrbit);
	ip27_hub_intr_makemasks();

	free(ih, M_DEVBUF);

	splx(s);
}

/*
 * Recompute interrupt masks.
 */
void
ip27_hub_intr_makemasks()
{
	int irq, level;
	struct intrhand *q;
	intrmask_t intrlevel[INTMASKSIZE];

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < INTMASKSIZE; irq++) {
		int levels = 0;
		for (q = intrhand[irq]; q; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = IPL_NONE; level < NIPLS; level++) {
		int irqs = 0;
		for (irq = 0; irq < INTMASKSIZE; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1 << irq;
		if (level != IPL_NONE)
			irqs |= SINT_ALLMASK;
		imask[level] = irqs;
	}

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so vm > (tty | net | bio).
	 *
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_NET] |= imask[IPL_BIO];
	imask[IPL_TTY] |= imask[IPL_NET];
	imask[IPL_VM] |= imask[IPL_TTY];
	imask[IPL_CLOCK] |= imask[IPL_VM] | SPL_CLOCKMASK;

	/*
	 * These are pseudo-levels.
	 */
	imask[IPL_NONE] = 0;
	imask[IPL_HIGH] = -1;

	if(CPU_IS_PRIMARY(curcpu()))
		hw_setintrmask(0);
}

void
ip27_hub_do_pending_int(int newcpl)
{
	struct cpu_info *ci = curcpu();

	/* Update masks to new cpl. Order highly important! */
	__asm__ (" .set noreorder\n");
	ci->ci_cpl = newcpl;
	__asm__ (" sync\n .set reorder\n");
	if (CPU_IS_PRIMARY(ci))
		hw_setintrmask(newcpl);
	/* If we still have softints pending trigger processing. */
	if (ci->ci_ipending & SINT_ALLMASK & ~newcpl)
		setsoftintr0();
}

intrmask_t
ip27_hub_intr_handler(intrmask_t hwpend, struct trap_frame *frame)
{
	uint64_t imr, isr;
	int icpl;
	int bit;
	intrmask_t mask;
	struct intrhand *ih;
	int rc;
	struct cpu_info *ci = curcpu();

	/* XXX this assumes we run on cpu0 */
	isr = IP27_LHUB_L(HUBPI_IR0);
	imr = IP27_LHUB_L(HUBPI_CPU0_IMR0);

	isr &= imr;
	if (isr == 0)
		return 0;	/* not for us */

	/*
	 * Mask all pending interrupts.
	 */
	IP27_LHUB_S(HUBPI_CPU0_IMR0, imr & ~isr);
	(void)IP27_LHUB_L(HUBPI_IR0);

	/*
	 * If interrupts are spl-masked, mark them as pending only.
	 */
	if ((mask = isr & frame->cpl) != 0) {
		atomic_setbits_int(&ci->ci_ipending, mask);
		isr &= ~mask;
		imr &= ~mask;
	}

	/*
	 * Now process unmasked interrupts.
	 */
	if (isr != 0) {
		atomic_clearbits_int(&ci->ci_ipending, isr);

		__asm__ (" .set noreorder\n");
		icpl = ci->ci_cpl;
		__asm__ (" sync\n .set reorder\n");

		/* XXX Rework this to dispatch in decreasing levels */
		for (bit = SPL_CLOCK - 1, mask = 1 << bit; bit >= 7;
		    bit--, mask >>= 1) {
			if ((isr & mask) == 0)
				continue;

			rc = 0;
			for (ih = intrhand[bit]; ih != NULL; ih = ih->ih_next) {
				splraise(imask[ih->ih_level]);
				ih->frame = frame;
				if ((*ih->ih_fun)(ih->ih_arg) != 0) {
					rc = 1;
					ih->ih_count.ec_count++;
				}
			}
			if (rc == 0)
				printf("spurious interrupt, source %d\n", bit);

			if ((isr ^= mask) == 0)
				break;
		}

		/*
		 * Reenable interrupts which have been serviced.
		 */
		IP27_LHUB_S(HUBPI_CPU0_IMR0, imr);
		(void)IP27_LHUB_L(HUBPI_IR0);
		
		__asm__ (" .set noreorder\n");
		ci->ci_cpl = icpl;
		__asm__ (" sync\n .set reorder\n");
	}

	return CR_INT_0;
}

void
hw_setintrmask(intrmask_t m)
{
	IP27_LHUB_S(HUBPI_CPU0_IMR0, ip27_hub_intrmask & ~((uint64_t)m));
	(void)IP27_LHUB_L(HUBPI_IR0);
}

void
ip27_nmi(void *arg)
{
	vaddr_t regs_offs;
	register_t *regs, epc;
	struct trap_frame nmi_frame;
	extern int kdb_trap(int, struct trap_frame *);

	/*
	 * Build a ddb frame from the registers saved in the NMI KREGS
	 * area.
	 */

	if (ip35)
		regs_offs = IP35_NMI_KREGS_BASE;	/* XXX assumes cpu0 */
	else
		regs_offs = IP27_NMI_KREGS_BASE;	/* XXX assumes cpu0 */
	regs = IP27_UNCAC_ADDR(register_t *, 0, regs_offs);

	memset(&nmi_frame, 0xff, sizeof nmi_frame);
	
	/* general registers */
	memcpy(&nmi_frame.zero, regs, 32 * sizeof(register_t));
	regs += 32;
	nmi_frame.sr = *regs++;		/* COP_0_STATUS_REG */
	nmi_frame.cause = *regs++;	/* COP_0_CAUSE_REG */
	nmi_frame.pc = *regs++;
	nmi_frame.badvaddr = *regs++;	/* COP_0_BAD_VADDR */
	epc = *regs++;			/* COP_0_EXC_PC */
	regs++;				/* COP_0_CACHE_ERR */
	regs++;				/* NMI COP_0_STATUS_REG */

	setsr(getsr() & ~SR_BOOT_EXC_VEC);
	printf("NMI, PC = %p RA = %p SR = %08x EPC = %p\n",
	    nmi_frame.pc, nmi_frame.ra, nmi_frame.sr, epc);
#ifdef DDB
	(void)kdb_trap(-1, &nmi_frame);
#endif
	printf("Resetting system...\n");
	boot(RB_USERREQ);
}
