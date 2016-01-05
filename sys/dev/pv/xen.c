/*	$OpenBSD: xen.c,v 1.22 2016/01/05 13:47:28 mikeb Exp $	*/

/*
 * Copyright (c) 2015 Mike Belopuhov
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
#include <sys/atomic.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>

#include <uvm/uvm_extern.h>

#include <machine/i82489var.h>

#include <dev/pv/pvvar.h>
#include <dev/pv/pvreg.h>
#include <dev/pv/xenreg.h>
#include <dev/pv/xenvar.h>

struct xen_softc *xen_sc;

int	xen_init_hypercall(struct xen_softc *);
int	xen_getversion(struct xen_softc *);
int	xen_getfeatures(struct xen_softc *);
int	xen_init_info_page(struct xen_softc *);
int	xen_init_cbvec(struct xen_softc *);
int	xen_init_interrupts(struct xen_softc *);
int	xen_init_grant_tables(struct xen_softc *);
struct xen_gntent *
	xen_grant_table_grow(struct xen_softc *);
int	xen_grant_table_alloc(struct xen_softc *, grant_ref_t *);
void	xen_grant_table_free(struct xen_softc *, grant_ref_t);
int	xen_grant_table_enter(struct xen_softc *, grant_ref_t, paddr_t, int);
void	xen_grant_table_remove(struct xen_softc *, grant_ref_t);
void	xen_disable_emulated_devices(struct xen_softc *);

int 	xen_match(struct device *, void *, void *);
void	xen_attach(struct device *, struct device *, void *);
void	xen_deferred(struct device *);
void	xen_resume(struct device *);
int	xen_activate(struct device *, int);
int	xen_probe_devices(struct xen_softc *);

void	xen_bus_dma_init(struct xen_softc *);
int	xen_bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	xen_bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	xen_bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
	    struct proc *, int);
int	xen_bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t, struct mbuf *,
	    int);
void	xen_bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);

int	xs_attach(struct xen_softc *);

struct cfdriver xen_cd = {
	NULL, "xen", DV_DULL
};

const struct cfattach xen_ca = {
	sizeof(struct xen_softc), xen_match, xen_attach, NULL, xen_activate
};

struct bus_dma_tag xen_bus_dma_tag = {
	NULL,
	xen_bus_dmamap_create,
	xen_bus_dmamap_destroy,
	xen_bus_dmamap_load,
	xen_bus_dmamap_load_mbuf,
	NULL,
	NULL,
	xen_bus_dmamap_unload,
	_bus_dmamap_sync,
	_bus_dmamem_alloc,
	NULL,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	NULL,
};

int
xen_match(struct device *parent, void *match, void *aux)
{
	struct pv_attach_args *pva = aux;
	struct pvbus_hv *hv = &pva->pva_hv[PVBUS_XEN];

	if (hv->hv_base == 0)
		return (0);

	return (1);
}

void
xen_attach(struct device *parent, struct device *self, void *aux)
{
	struct pv_attach_args *pva = (struct pv_attach_args *)aux;
	struct pvbus_hv *hv = &pva->pva_hv[PVBUS_XEN];
	struct xen_softc *sc = (struct xen_softc *)self;

	sc->sc_base = hv->hv_base;

	printf("\n");

	if (xen_init_hypercall(sc))
		return;

	/* Wire it up to the global */
	xen_sc = sc;

	if (xen_getversion(sc))
		return;
	if (xen_getfeatures(sc))
		return;

	if (xen_init_info_page(sc))
		return;

	xen_init_cbvec(sc);

	if (xen_init_interrupts(sc))
		return;

	if (xen_init_grant_tables(sc))
		return;

	if (xs_attach(sc))
		return;

	xen_disable_emulated_devices(sc);

	xen_probe_devices(sc);

	config_mountroot(self, xen_deferred);
}

void
xen_deferred(struct device *self)
{
	struct xen_softc *sc = (struct xen_softc *)self;

	if (!sc->sc_cbvec) {
		DPRINTF("%s: callback vector hasn't been established\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	xen_intr_enable();
}

void
xen_resume(struct device *self)
{
}

int
xen_activate(struct device *self, int act)
{
	int rv = 0;

	switch (act) {
	case DVACT_RESUME:
		xen_resume(self);
		break;
	}
	return (rv);
}

int
xen_init_hypercall(struct xen_softc *sc)
{
	extern void *xen_hypercall_page;
	uint32_t regs[4];
	paddr_t pa;

	/* Get hypercall page configuration MSR */
	CPUID(sc->sc_base + CPUID_OFFSET_XEN_HYPERCALL,
	    regs[0], regs[1], regs[2], regs[3]);

	/* We don't support more than one hypercall page */
	if (regs[0] != 1) {
		printf("%s: requested %d hypercall pages\n",
		    sc->sc_dev.dv_xname, regs[0]);
		return (-1);
	}

	sc->sc_hc = &xen_hypercall_page;

	if (!pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_hc, &pa)) {
		printf("%s: hypercall page PA extraction failed\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	wrmsr(regs[1], pa);

	DPRINTF("%s: hypercall page at va %p pa %#lx\n", sc->sc_dev.dv_xname,
	    sc->sc_hc, pa);

	return (0);
}

int
xen_hypercall(struct xen_softc *sc, int op, int argc, ...)
{
	va_list ap;
	ulong argv[5];
	int i;

	if (argc < 0 || argc > 5)
		return (-1);
	va_start(ap, argc);
	for (i = 0; i < argc; i++)
		argv[i] = (ulong)va_arg(ap, ulong);
	return (xen_hypercallv(sc, op, argc, argv));
}

int
xen_hypercallv(struct xen_softc *sc, int op, int argc, ulong *argv)
{
	ulong hcall;
	int rv = 0;

	hcall = (ulong)sc->sc_hc + op * 32;

#if defined(XEN_DEBUG) && disabled
	{
		int i;

		printf("hypercall %d", op);
		if (argc > 0) {
			printf(", args {");
			for (i = 0; i < argc; i++)
				printf(" %#lx", argv[i]);
			printf(" }\n");
		} else
			printf("\n");
	}
#endif

	switch (argc) {
	case 0: {
		HYPERCALL_RES1;
		__asm__ volatile (			\
			  HYPERCALL_LABEL		\
			: HYPERCALL_OUT1		\
			: HYPERCALL_PTR(hcall)		\
			: HYPERCALL_CLOBBER		\
		);
		HYPERCALL_RET(rv);
		break;
	}
	case 1: {
		HYPERCALL_RES1; HYPERCALL_RES2;
		HYPERCALL_ARG1(argv[0]);
		__asm__ volatile (			\
			  HYPERCALL_LABEL		\
			: HYPERCALL_OUT1 HYPERCALL_OUT2	\
			: HYPERCALL_IN1			\
			, HYPERCALL_PTR(hcall)		\
			: HYPERCALL_CLOBBER		\
		);
		HYPERCALL_RET(rv);
		break;
	}
	case 2: {
		HYPERCALL_RES1; HYPERCALL_RES2; HYPERCALL_RES3;
		HYPERCALL_ARG1(argv[0]); HYPERCALL_ARG2(argv[1]);
		__asm__ volatile (			\
			  HYPERCALL_LABEL		\
			: HYPERCALL_OUT1 HYPERCALL_OUT2	\
			  HYPERCALL_OUT3		\
			: HYPERCALL_IN1	HYPERCALL_IN2	\
			, HYPERCALL_PTR(hcall)		\
			: HYPERCALL_CLOBBER		\
		);
		HYPERCALL_RET(rv);
		break;
	}
	case 3: {
		HYPERCALL_RES1; HYPERCALL_RES2; HYPERCALL_RES3;
		HYPERCALL_RES4;
		HYPERCALL_ARG1(argv[0]); HYPERCALL_ARG2(argv[1]);
		HYPERCALL_ARG3(argv[2]);
		__asm__ volatile (			\
			  HYPERCALL_LABEL		\
			: HYPERCALL_OUT1 HYPERCALL_OUT2	\
			  HYPERCALL_OUT3 HYPERCALL_OUT4	\
			: HYPERCALL_IN1	HYPERCALL_IN2	\
			  HYPERCALL_IN3			\
			, HYPERCALL_PTR(hcall)		\
			: HYPERCALL_CLOBBER		\
		);
		HYPERCALL_RET(rv);
		break;
	}
	case 4: {
		HYPERCALL_RES1; HYPERCALL_RES2; HYPERCALL_RES3;
		HYPERCALL_RES4; HYPERCALL_RES5;
		HYPERCALL_ARG1(argv[0]); HYPERCALL_ARG2(argv[1]);
		HYPERCALL_ARG3(argv[2]); HYPERCALL_ARG4(argv[3]);
		__asm__ volatile (			\
			  HYPERCALL_LABEL		\
			: HYPERCALL_OUT1 HYPERCALL_OUT2	\
			  HYPERCALL_OUT3 HYPERCALL_OUT4	\
			  HYPERCALL_OUT5		\
			: HYPERCALL_IN1	HYPERCALL_IN2	\
			  HYPERCALL_IN3	HYPERCALL_IN4	\
			, HYPERCALL_PTR(hcall)		\
			: HYPERCALL_CLOBBER		\
		);
		HYPERCALL_RET(rv);
		break;
	}
	case 5: {
		HYPERCALL_RES1; HYPERCALL_RES2; HYPERCALL_RES3;
		HYPERCALL_RES4; HYPERCALL_RES5; HYPERCALL_RES6;
		HYPERCALL_ARG1(argv[0]); HYPERCALL_ARG2(argv[1]);
		HYPERCALL_ARG3(argv[2]); HYPERCALL_ARG4(argv[3]);
		HYPERCALL_ARG5(argv[4]);
		__asm__ volatile (			\
			  HYPERCALL_LABEL		\
			: HYPERCALL_OUT1 HYPERCALL_OUT2	\
			  HYPERCALL_OUT3 HYPERCALL_OUT4	\
			  HYPERCALL_OUT5 HYPERCALL_OUT6	\
			: HYPERCALL_IN1	HYPERCALL_IN2	\
			  HYPERCALL_IN3	HYPERCALL_IN4	\
			  HYPERCALL_IN5			\
			, HYPERCALL_PTR(hcall)		\
			: HYPERCALL_CLOBBER		\
		);
		HYPERCALL_RET(rv);
		break;
	}
	default:
		DPRINTF("%s: wrong number of arguments: %d\n", __func__, argc);
		rv = -1;
		break;
	}
	return (rv);
}

int
xen_getversion(struct xen_softc *sc)
{
	char buf[16];
	int version;
	ulong argv[2] = { XENVER_extraversion, (ulong)&buf[0] };
	int argc = 2;

	memset(buf, 0, sizeof(buf));
	if ((version = xen_hypercall(sc, XC_VERSION, 1, XENVER_version)) < 0) {
		printf("%s: failed to fetch version\n", sc->sc_dev.dv_xname);
		return (-1);
	}
	if (xen_hypercallv(sc, XC_VERSION, argc, argv) < 0) {
		printf("%s: failed to fetch extended version\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	printf("%s: version %d.%d%s\n", sc->sc_dev.dv_xname,
	    version >> 16, version & 0xffff, buf);
	return (0);
}

int
xen_getfeatures(struct xen_softc *sc)
{
	struct xen_feature_info xfi;
	ulong argv[2] = { XENVER_get_features, (ulong)&xfi };
	int argc = 2;

	memset(&xfi, 0, sizeof(xfi));
	if (xen_hypercallv(sc, XC_VERSION, argc, argv) < 0) {
		printf("%s: failed to fetch features\n", sc->sc_dev.dv_xname);
		return (-1);
	}
	sc->sc_features = xfi.submap;
	printf("%s: features %b\n", sc->sc_dev.dv_xname, sc->sc_features,
	    "\20\014DOM0\013PIRQ\012PVCLOCK\011CBVEC\010GNTFLAGS\007HMA"
	    "\006PTUPD\005PAE4G\004SUPERVISOR\003AUTOPMAP\002WDT\001WPT");
	return (0);
}

#ifdef XEN_DEBUG
void
xen_print_info_page(void)
{
	struct xen_softc *sc = xen_sc;
	struct shared_info *s = sc->sc_ipg;
	struct vcpu_info *v;
	int i;

	membar_sync();
	for (i = 0; i < XEN_LEGACY_MAX_VCPUS; i++) {
		v = &s->vcpu_info[i];
		if (!v->evtchn_upcall_pending && !v->evtchn_upcall_mask &&
		    !v->evtchn_pending_sel && !v->time.version &&
		    !v->time.tsc_timestamp && !v->time.system_time &&
		    !v->time.tsc_to_system_mul && !v->time.tsc_shift)
			continue;
		printf("vcpu%d:\n"
		    "   upcall_pending=%02x upcall_mask=%02x pending_sel=%#lx\n"
		    "   time version=%u tsc=%llu system=%llu\n"
		    "   time mul=%u shift=%d\n"
		    , i, v->evtchn_upcall_pending, v->evtchn_upcall_mask,
		    v->evtchn_pending_sel, v->time.version,
		    v->time.tsc_timestamp, v->time.system_time,
		    v->time.tsc_to_system_mul, v->time.tsc_shift);
	}
	printf("pending events: ");
	for (i = 0; i < nitems(s->evtchn_pending); i++) {
		if (s->evtchn_pending[i] == 0)
			continue;
		printf(" %d:%#lx", i, s->evtchn_pending[i]);
	}
	printf("\nmasked events: ");
	for (i = 0; i < nitems(s->evtchn_mask); i++) {
		if (s->evtchn_mask[i] == 0xffffffffffffffffULL)
			continue;
		printf(" %d:%#lx", i, s->evtchn_mask[i]);
	}
	printf("\nwc ver=%u sec=%u nsec=%u\n", s->wc_version, s->wc_sec,
	    s->wc_nsec);
	printf("arch maxpfn=%lu framelist=%lu nmi=%lu\n", s->arch.max_pfn,
	    s->arch.pfn_to_mfn_frame_list, s->arch.nmi_reason);
}
#endif	/* XEN_DEBUG */

int
xen_init_info_page(struct xen_softc *sc)
{
	struct xen_add_to_physmap xatp;
	paddr_t pa;

	sc->sc_ipg = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_ipg == NULL) {
		printf("%s: failed to allocate shared info page\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	if (!pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_ipg, &pa)) {
		printf("%s: shared info page PA extraction failed\n",
		    sc->sc_dev.dv_xname);
		free(sc->sc_ipg, M_DEVBUF, PAGE_SIZE);
		return (-1);
	}
	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = XENMAPSPACE_shared_info;
	xatp.gpfn = atop(pa);
	if (xen_hypercall(sc, XC_MEMORY, 2, XENMEM_add_to_physmap, &xatp)) {
		printf("%s: failed to register shared info page\n",
		    sc->sc_dev.dv_xname);
		free(sc->sc_ipg, M_DEVBUF, PAGE_SIZE);
		return (-1);
	}
	DPRINTF("%s: shared info page at va %p pa %#lx\n", sc->sc_dev.dv_xname,
	    sc->sc_ipg, pa);
	return (0);
}

int
xen_init_cbvec(struct xen_softc *sc)
{
	struct xen_hvm_param xhp;

	if ((sc->sc_features & XENFEAT_CBVEC) == 0)
		return (ENOENT);

	xhp.domid = DOMID_SELF;
	xhp.index = HVM_PARAM_CALLBACK_IRQ;
	xhp.value = HVM_CALLBACK_VECTOR(LAPIC_XEN_VECTOR);
	if (xen_hypercall(sc, XC_HVM, 2, HVMOP_set_param, &xhp)) {
		/* Will retry with the xspd(4) PCI interrupt */
		return (ENOENT);
	}
	DPRINTF("%s: registered callback IDT vector %d\n",
	    sc->sc_dev.dv_xname, LAPIC_XEN_VECTOR);

	sc->sc_cbvec = 1;

	return (0);
}

int
xen_init_interrupts(struct xen_softc *sc)
{
	int i;

	sc->sc_irq = LAPIC_XEN_VECTOR;
	evcount_attach(&sc->sc_evcnt, sc->sc_dev.dv_xname, &sc->sc_irq);

	/*
	 * Clear all pending events and mask all interrupts
	 */
	for (i = 0; i < nitems(sc->sc_ipg->evtchn_pending); i++) {
		sc->sc_ipg->evtchn_pending[i] = 0;
		sc->sc_ipg->evtchn_mask[i] = ~0UL;
		membar_producer();
	}

	SLIST_INIT(&sc->sc_intrs);

	return (0);
}

static inline struct xen_intsrc *
xen_lookup_intsrc(struct xen_softc *sc, evtchn_port_t port)
{
	struct xen_intsrc *xi;

	SLIST_FOREACH(xi, &sc->sc_intrs, xi_entry)
		if (xi->xi_port == port)
			break;
	return (xi);
}

void
xen_intr_ack(void)
{
	struct xen_softc *sc = xen_sc;
	struct shared_info *s = sc->sc_ipg;
	struct cpu_info *ci = curcpu();
	struct vcpu_info *v = &s->vcpu_info[CPU_INFO_UNIT(ci)];

	v->evtchn_upcall_pending = 0;
}

void
xen_intr(void)
{
	struct xen_softc *sc = xen_sc;
	struct xen_intsrc *xi;
	struct shared_info *s = sc->sc_ipg;
	struct cpu_info *ci = curcpu();
	struct vcpu_info *v = &s->vcpu_info[CPU_INFO_UNIT(ci)];
	ulong pending, selector;
	int port, bit, row;

	sc->sc_evcnt.ec_count++;

	v->evtchn_upcall_pending = 0;
	selector = atomic_swap_ulong(&v->evtchn_pending_sel, 0);

	for (row = 0; selector > 0; selector >>= 1, row++) {
		if ((selector & 1) == 0)
			continue;
		pending = sc->sc_ipg->evtchn_pending[row] &
		    ~(sc->sc_ipg->evtchn_mask[row]);
		for (bit = 0; pending > 0; pending >>= 1, bit++) {
			if ((pending & 1) == 0)
				continue;
			sc->sc_ipg->evtchn_pending[row] &= ~(1 << bit);
			membar_producer();
			port = (row * LONG_BIT) + bit;
			if ((xi = xen_lookup_intsrc(sc, port)) == NULL)
				continue;
			xi->xi_evcnt.ec_count++;
			if (xi->xi_handler)
				xi->xi_handler(xi->xi_arg);
		}
	}
}

void
xen_intr_signal(xen_intr_handle_t xih)
{
	struct xen_softc *sc = xen_sc;
	struct xen_intsrc *xi;
	struct evtchn_send es;

	if ((xi = xen_lookup_intsrc(sc, (evtchn_port_t)xih)) != NULL) {
		es.port = xi->xi_port;
		xen_hypercall(sc, XC_EVTCHN, 2, EVTCHNOP_send, &es);
	}
}

int
xen_intr_establish(evtchn_port_t port, xen_intr_handle_t *xih,
    void (*handler)(void *), void *arg, char *name)
{
	struct xen_softc *sc = xen_sc;
	struct xen_intsrc *xi;
	struct evtchn_alloc_unbound eau;
#if notyet
	struct evtchn_bind_vcpu ebv;
#endif
#if defined(XEN_DEBUG) && disabled
	struct evtchn_status es;
#endif

	if (port && xen_lookup_intsrc(sc, port)) {
		DPRINTF("%s: interrupt handler has already been established "
		    "for port %u\n", sc->sc_dev.dv_xname, port);
		return (-1);
	}

	xi = malloc(sizeof(*xi), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (xi == NULL)
		return (-1);

	xi->xi_handler = handler;
	xi->xi_arg = arg;
	xi->xi_port = (evtchn_port_t)*xih;

	if (port == 0) {
		/* We're being asked to allocate a new event port */
		memset(&eau, 0, sizeof(eau));
		eau.dom = DOMID_SELF;
		if (xen_hypercall(sc, XC_EVTCHN, 2,
		    EVTCHNOP_alloc_unbound, &eau) != 0) {
			DPRINTF("%s: failed to allocate new event port\n",
			    sc->sc_dev.dv_xname);
			free(xi, M_DEVBUF, sizeof(*xi));
			return (-1);
		}
		*xih = xi->xi_port = eau.port;
	} else {
		*xih = xi->xi_port = port;
		/*
		 * The Event Channel API didn't open this port, so it is not
		 * responsible for closing it automatically on unbind.
		 */
		xi->xi_noclose = 1;
	}

#ifdef notyet
	/* Bind interrupt to VCPU#0 */
	memset(&ebv, 0, sizeof(ebv));
	ebv.port = xi->xi_port;
	ebv.vcpu = 0;
	if (xen_hypercall(sc, XC_EVTCHN, 2, EVTCHNOP_bind_vcpu, &ebv)) {
		printf("%s: failed to bind interrupt on port %u to vcpu%d\n",
		    sc->sc_dev.dv_xname, ebv.port, ebv.vcpu);
	}
#endif

	evcount_attach(&xi->xi_evcnt, name, &sc->sc_irq);

	SLIST_INSERT_HEAD(&sc->sc_intrs, xi, xi_entry);

	/* Mask the event port */
	setbit((char *)&sc->sc_ipg->evtchn_mask[0], xi->xi_port);

#if defined(XEN_DEBUG) && disabled
	memset(&es, 0, sizeof(es));
	es.dom = DOMID_SELF;
	es.port = xi->xi_port;
	if (xen_hypercall(sc, XC_EVTCHN, 2, EVTCHNOP_status, &es)) {
		printf("%s: failed to obtain status for port %d\n",
		    sc->sc_dev.dv_xname, es.port);
	}
	printf("%s: port %u bound to vcpu%u",
	    sc->sc_dev.dv_xname, es.port, es.vcpu);
	if (es.status == EVTCHNSTAT_interdomain)
		printf(": domain %d port %u\n", es.u.interdomain.dom,
		    es.u.interdomain.port);
	else if (es.status == EVTCHNSTAT_unbound)
		printf(": domain %d\n", es.u.unbound.dom);
	else if (es.status == EVTCHNSTAT_pirq)
		printf(": pirq %u\n", es.u.pirq);
	else if (es.status == EVTCHNSTAT_virq)
		printf(": virq %u\n", es.u.virq);
	else
		printf("\n");
#endif

	return (0);
}

int
xen_intr_disestablish(xen_intr_handle_t xih)
{
	struct xen_softc *sc = xen_sc;
	evtchn_port_t port = (evtchn_port_t)xih;
	struct evtchn_close ec;
	struct xen_intsrc *xi;

	if ((xi = xen_lookup_intsrc(sc, port)) == NULL)
		return (-1);

	evcount_detach(&xi->xi_evcnt);

	SLIST_REMOVE(&sc->sc_intrs, xi, xen_intsrc, xi_entry);

	setbit((char *)&sc->sc_ipg->evtchn_mask[0], xi->xi_port);
	clrbit((char *)&sc->sc_ipg->evtchn_pending[0], xi->xi_port);
	membar_producer();

	if (!xi->xi_noclose) {
		ec.port = xi->xi_port;
		if (xen_hypercall(sc, XC_EVTCHN, 2, EVTCHNOP_close,
		    &ec)) {
			DPRINTF("%s: failed to close event port %u\n",
			    sc->sc_dev.dv_xname, xi->xi_port);
		}
	}

	free(xi, M_DEVBUF, sizeof(*xi));
	return (0);
}

void
xen_intr_enable(void)
{
	struct xen_softc *sc = xen_sc;
	struct xen_intsrc *xi;
	struct evtchn_unmask eu;

	SLIST_FOREACH(xi, &sc->sc_intrs, xi_entry) {
		if (!xi->xi_masked) {
			eu.port = xi->xi_port;
			if (xen_hypercall(sc, XC_EVTCHN, 2,
			    EVTCHNOP_unmask, &eu) ||
			    isset(sc->sc_ipg->evtchn_mask, xi->xi_port))
				printf("%s: unmasking port %u failed\n",
				    sc->sc_dev.dv_xname, xi->xi_port);
		}
	}
}

void
xen_intr_mask(xen_intr_handle_t xih)
{
	struct xen_softc *sc = xen_sc;
	evtchn_port_t port = (evtchn_port_t)xih;
	struct xen_intsrc *xi;

	if ((xi = xen_lookup_intsrc(sc, port)) != NULL) {
		xi->xi_masked = 1;
		setbit((char *)&sc->sc_ipg->evtchn_mask[0], xi->xi_port);
		membar_producer();
	}
}

int
xen_intr_unmask(xen_intr_handle_t xih)
{
	struct xen_softc *sc = xen_sc;
	evtchn_port_t port = (evtchn_port_t)xih;
	struct xen_intsrc *xi;
	struct evtchn_unmask eu;

	if ((xi = xen_lookup_intsrc(sc, port)) != NULL) {
		xi->xi_masked = 0;
		if (!isset(sc->sc_ipg->evtchn_mask, xi->xi_port))
			return (0);
		eu.port = xi->xi_port;
		return (xen_hypercall(sc, XC_EVTCHN, 2, EVTCHNOP_unmask, &eu));
	}
	return (0);
}

int
xen_init_grant_tables(struct xen_softc *sc)
{
	struct gnttab_query_size gqs;
	struct gnttab_get_version ggv;
	struct gnttab_set_version gsv;
	int i;

	gqs.dom = DOMID_SELF;
	if (xen_hypercall(sc, XC_GNTTAB, 3, GNTTABOP_query_size, &gqs, 1)) {
		printf("%s: failed the query for grant table pages\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	if (gqs.nr_frames == 0 || gqs.nr_frames > gqs.max_nr_frames) {
		printf("%s: invalid number of grant table pages: %u/%u\n",
		    sc->sc_dev.dv_xname, gqs.nr_frames, gqs.max_nr_frames);
		return (-1);
	}

	gsv.version = 2;
	ggv.dom = DOMID_SELF;
	if (xen_hypercall(sc, XC_GNTTAB, 3, GNTTABOP_set_version, &gsv, 1) ||
	    xen_hypercall(sc, XC_GNTTAB, 3, GNTTABOP_get_version, &ggv, 1) ||
	    ggv.version != 2) {
		printf("%s: failed to set grant tables API version\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}

	SLIST_INIT(&sc->sc_gnts);

	for (i = 0; i < gqs.max_nr_frames; i++)
		if (xen_grant_table_grow(sc) == NULL)
			break;

	DPRINTF("%s: grant table frames allocated %u/%u\n",
	    sc->sc_dev.dv_xname, sc->sc_gntcnt, gqs.max_nr_frames);

	xen_bus_dma_init(sc);

	return (0);
}

struct xen_gntent *
xen_grant_table_grow(struct xen_softc *sc)
{
	struct xen_add_to_physmap xatp;
	struct xen_gntent *ge;
	paddr_t pa;

	ge = malloc(sizeof(*ge), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (ge == NULL) {
		printf("%s: failed to allocate a grant table entry\n",
		    sc->sc_dev.dv_xname);
		return (NULL);
	}
	ge->ge_table = km_alloc(PAGE_SIZE, &kv_any, &kp_zero,
	    &kd_nowait);
	if (ge->ge_table == NULL) {
		free(ge, M_DEVBUF, sizeof(*ge));
		return (NULL);
	}
	if (!pmap_extract(pmap_kernel(), (vaddr_t)ge->ge_table, &pa)) {
		printf("%s: grant table page PA extraction failed\n",
		    sc->sc_dev.dv_xname);
		km_free(ge->ge_table, PAGE_SIZE, &kv_any, &kp_zero);
		free(ge, M_DEVBUF, sizeof(*ge));
		return (NULL);
	}
	xatp.domid = DOMID_SELF;
	xatp.idx = sc->sc_gntcnt;
	xatp.space = XENMAPSPACE_grant_table;
	xatp.gpfn = atop(pa);
	if (xen_hypercall(sc, XC_MEMORY, 2, XENMEM_add_to_physmap, &xatp)) {
		printf("%s: failed to add a grant table page\n",
		    sc->sc_dev.dv_xname);
		km_free(ge->ge_table, PAGE_SIZE, &kv_any, &kp_zero);
		free(ge, M_DEVBUF, sizeof(*ge));
		return (NULL);
	}
	ge->ge_start = sc->sc_gntcnt * GNTTAB_NEPG;
	/* First page has 8 reserved entries */
	ge->ge_reserved = ge->ge_start == 0 ? GNTTAB_NR_RESERVED_ENTRIES : 0;
	ge->ge_free = GNTTAB_NEPG - ge->ge_reserved;
	ge->ge_next = ge->ge_reserved ? ge->ge_reserved + 1 : 0;
	mtx_init(&ge->ge_mtx, IPL_NET);
	SLIST_INSERT_HEAD(&sc->sc_gnts, ge, ge_entry);

	sc->sc_gntcnt++;

	return (ge);
}

int
xen_grant_table_alloc(struct xen_softc *sc, grant_ref_t *ref)
{
	struct xen_gntent *ge;
	int i;

	SLIST_FOREACH(ge, &sc->sc_gnts, ge_entry) {
		if (!ge->ge_free)
			continue;
		mtx_enter(&ge->ge_mtx);
		for (i = ge->ge_next;
		     /* Math works here because GNTTAB_NEPG is a power of 2 */
		     i != ((ge->ge_next + GNTTAB_NEPG - 1) & (GNTTAB_NEPG - 1));
		     i++) {
			if (i == GNTTAB_NEPG)
				i = 0;
			if (ge->ge_reserved && i < ge->ge_reserved)
				continue;
			if (ge->ge_table[i].hdr.flags != GTF_invalid &&
			    ge->ge_table[i].full_page.frame != 0)
				continue;
			*ref = ge->ge_start + i;
			/* XXX Mark as taken */
			ge->ge_table[i].full_page.frame = 0xffffffff;
			if ((ge->ge_next = i + 1) == GNTTAB_NEPG)
				ge->ge_next = ge->ge_reserved + 1;
			ge->ge_free--;
			mtx_leave(&ge->ge_mtx);
			return (0);
		}
		mtx_leave(&ge->ge_mtx);
	}

	/* We're out of entries */
	return (-1);
}

void
xen_grant_table_free(struct xen_softc *sc, grant_ref_t ref)
{
	struct xen_gntent *ge;

	SLIST_FOREACH(ge, &sc->sc_gnts, ge_entry) {
		if (ref < ge->ge_start || ref > ge->ge_start + GNTTAB_NEPG)
			continue;
		ref -= ge->ge_start;
		mtx_enter(&ge->ge_mtx);
		if (ge->ge_table[ref].hdr.flags != GTF_invalid) {
			mtx_leave(&ge->ge_mtx);
			return;
		}
		ge->ge_table[ref].full_page.frame = 0;
		ge->ge_next = ref;
		ge->ge_free++;
		mtx_leave(&ge->ge_mtx);
	}
}

int
xen_grant_table_enter(struct xen_softc *sc, grant_ref_t ref, paddr_t pa,
    int flags)
{
	struct xen_gntent *ge;

	SLIST_FOREACH(ge, &sc->sc_gnts, ge_entry) {
		if (ref < ge->ge_start || ref > ge->ge_start + GNTTAB_NEPG)
			continue;
		ref -= ge->ge_start;
		mtx_enter(&ge->ge_mtx);
		ge->ge_table[ref].full_page.frame = atop(pa);
		ge->ge_table[ref].full_page.hdr.domid = 0;
		membar_producer();
		ge->ge_table[ref].full_page.hdr.flags =
		    GTF_permit_access | flags;
		mtx_leave(&ge->ge_mtx);
		return (0);
	}
	return (ENOBUFS);
}

void
xen_grant_table_remove(struct xen_softc *sc, grant_ref_t ref)
{
	struct xen_gntent *ge;
	uint32_t flags, *ptr;

	SLIST_FOREACH(ge, &sc->sc_gnts, ge_entry) {
		if (ref < ge->ge_start || ref > ge->ge_start + GNTTAB_NEPG)
			continue;
		ref -= ge->ge_start;

		mtx_enter(&ge->ge_mtx);
		/* Invalidate the grant reference */
		ptr = (uint32_t *)&ge->ge_table[ref].hdr;
		flags = (ge->ge_table[ref].hdr.flags &
		    ~(GTF_reading | GTF_writing));
		while (atomic_cas_uint(ptr, flags, 0) != flags)
			CPU_BUSY_CYCLE();
		ge->ge_table[ref].full_page.frame = 0xffffffff;
		mtx_leave(&ge->ge_mtx);
		break;
	}
}

void
xen_bus_dma_init(struct xen_softc *sc)
{
	xen_bus_dma_tag._cookie = sc;
}

int
xen_bus_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	struct xen_softc *sc = t->_cookie;
	struct xen_gntmap *gm;
	int i, error;

	if (maxsegsz < PAGE_SIZE)
		return (EINVAL);

	/* Allocate a dma map structure */
	error = _bus_dmamap_create(t, size, nsegments, maxsegsz, boundary,
	    flags, dmamp);
	if (error)
		return (error);
	/* ALlocate an array of grant table pa<->ref maps */
	gm = mallocarray(nsegments, sizeof(struct xen_gntmap), M_DEVBUF,
	    M_ZERO | ((flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK));
	if (gm == NULL) {
		_bus_dmamap_destroy(t, *dmamp);
		*dmamp = NULL;
		return (ENOMEM);
	}
	/* Wire it to the dma map */
	(*dmamp)->_dm_cookie = gm;
	/* Claim references from the grant table */
	for (i = 0; i < (*dmamp)->_dm_segcnt; i++) {
		if (xen_grant_table_alloc(sc, &gm[i].gm_ref)) {
			xen_bus_dmamap_destroy(t, *dmamp);
			*dmamp = NULL;
			return (ENOBUFS);
		}
	}
	return (0);
}

void
xen_bus_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct xen_softc *sc = t->_cookie;
	struct xen_gntmap *gm;
	int i;

	gm = map->_dm_cookie;
	for (i = 0; i < map->_dm_segcnt; i++) {
		if (gm[i].gm_ref == 0)
			continue;
		xen_grant_table_free(sc, gm[i].gm_ref);
	}
	free(gm, M_DEVBUF, map->_dm_segcnt * sizeof(struct xen_gntmap));
	_bus_dmamap_destroy(t, map);
}

int
xen_bus_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	struct xen_softc *sc = t->_cookie;
	struct xen_gntmap *gm = map->_dm_cookie;
	int i, error;

	error = _bus_dmamap_load(t, map, buf, buflen, p, flags);
	if (error)
		return (error);
	for (i = 0; i < map->dm_nsegs; i++) {
		error = xen_grant_table_enter(sc, gm[i].gm_ref,
		    map->dm_segs[i].ds_addr, flags & BUS_DMA_WRITE ?
		    GTF_readonly : 0);
		if (error) {
			xen_bus_dmamap_unload(t, map);
			return (error);
		}
		gm[i].gm_paddr = map->dm_segs[i].ds_addr;
		map->dm_segs[i].ds_offset = map->dm_segs[i].ds_addr &
		    PAGE_MASK;
		map->dm_segs[i].ds_addr = gm[i].gm_ref;
	}
	return (0);
}

int
xen_bus_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *m0,
    int flags)
{
	struct xen_softc *sc = t->_cookie;
	struct xen_gntmap *gm = map->_dm_cookie;
	int i, error;

	error = _bus_dmamap_load_mbuf(t, map, m0, flags);
	if (error)
		return (error);
	for (i = 0; i < map->dm_nsegs; i++) {
		error = xen_grant_table_enter(sc, gm[i].gm_ref,
		    map->dm_segs[i].ds_addr, flags & BUS_DMA_WRITE ?
		    GTF_readonly : 0);
		if (error) {
			xen_bus_dmamap_unload(t, map);
			return (error);
		}
		gm[i].gm_paddr = map->dm_segs[i].ds_addr;
		map->dm_segs[i].ds_offset = map->dm_segs[i].ds_addr &
		    PAGE_MASK;
		map->dm_segs[i].ds_addr = gm[i].gm_ref;
	}
	return (0);
}

void
xen_bus_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct xen_softc *sc = t->_cookie;
	struct xen_gntmap *gm = map->_dm_cookie;
	int i;

	for (i = 0; i < map->dm_nsegs; i++) {
		if (gm[i].gm_paddr == 0)
			continue;
		xen_grant_table_remove(sc, gm[i].gm_ref);
		map->dm_segs[i].ds_addr = gm[i].gm_paddr;
		gm[i].gm_paddr = 0;
	}
	_bus_dmamap_unload(t, map);
}

static int
xen_attach_print(void *aux, const char *name)
{
	struct xen_attach_args *xa = aux;

	if (name)
		printf("\"%s\" at %s: %s", xa->xa_name, name, xa->xa_node);

	return (UNCONF);
}

int
xen_probe_devices(struct xen_softc *sc)
{
	struct xen_attach_args xa;
	struct xs_transaction xst;
	struct iovec *iovp1, *iovp2, *iovp3;
	int i, j, error = 0, iov1_cnt, iov2_cnt, iov3_cnt;
	char path[128];

	memset(&xst, 0, sizeof(xst));
	xst.xst_id = 0;
	xst.xst_sc = sc->sc_xs;
	xst.xst_flags |= XST_POLL;

	if ((error = xs_cmd(&xst, XS_LIST, "device", &iovp1,
	    &iov1_cnt)) != 0)
		return (error);

	for (i = 0; i < iov1_cnt; i++) {
		if (strcmp("suspend", (char *)iovp1[i].iov_base) == 0)
			continue;
		snprintf(path, sizeof(path), "device/%s",
		    (char *)iovp1[i].iov_base);
		if ((error = xs_cmd(&xst, XS_LIST, path, &iovp2,
		    &iov2_cnt)) != 0) {
			xs_resfree(&xst, iovp1, iov1_cnt);
			return (error);
		}
		for (j = 0; j < iov2_cnt; j++) {
			xa.xa_parent = sc;
			xa.xa_dmat = &xen_bus_dma_tag;
			strlcpy(xa.xa_name, (char *)iovp1[i].iov_base,
			    sizeof(xa.xa_name));
			snprintf(xa.xa_node, sizeof(xa.xa_node), "device/%s/%s",
			    (char *)iovp1[i].iov_base,
			    (char *)iovp2[j].iov_base);
			snprintf(path, sizeof(path), "%s/backend", xa.xa_node);
			if (!xs_cmd(&xst, XS_READ, path, &iovp3, &iov3_cnt)) {
				strlcpy(xa.xa_backend, (char *)iovp3->iov_base,
				    sizeof(xa.xa_backend));
				xs_resfree(&xst, iovp3, iov3_cnt);
			}
			config_found((struct device *)sc, &xa,
			    xen_attach_print);
		}
		xs_resfree(&xst, iovp2, iov2_cnt);
	}

	return (error);
}

#include <machine/pio.h>

#define	XMI_PORT		0x10
#define XMI_MAGIC		0x49d2
#define XMI_UNPLUG_IDE		0x01
#define XMI_UNPLUG_NIC		0x02
#define XMI_UNPLUG_IDESEC	0x04

int xen_disable_pv_ide, xen_disable_pv_idesec, xen_disable_pv_nic;

void
xen_disable_emulated_devices(struct xen_softc *sc)
{
#if defined(__i386__) || defined(__amd64__)
	ushort unplug = 0;

	if (inw(XMI_PORT) != XMI_MAGIC) {
		printf("%s: no magic!\n", sc->sc_dev.dv_xname);
		return;
	}
	if (xen_disable_pv_ide)
		unplug |= XMI_UNPLUG_IDE;
	if (xen_disable_pv_idesec)
		unplug |= XMI_UNPLUG_IDESEC;
	if (xen_disable_pv_nic)
		unplug |= XMI_UNPLUG_NIC;
	if (unplug) {
		outw(XMI_PORT, unplug);
		DPRINTF("%s: disabled emulated devices\n", sc->sc_dev.dv_xname);
	}
#endif	/* __i386__ || __amd64__ */
}
