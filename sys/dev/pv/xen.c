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
#include <dev/pv/xenreg.h>
#include <dev/pv/xenvar.h>

struct xen_softc *xen_sc;

int	xen_init_hypercall(struct xen_softc *);
int	xen_getversion(struct xen_softc *);
int	xen_getfeatures(struct xen_softc *);
int	xen_init_info_page(struct xen_softc *);
int	xen_init_cbvec(struct xen_softc *);
int	xen_init_interrupts(struct xen_softc *);

int 	xen_match(struct device *, void *, void *);
void	xen_attach(struct device *, struct device *, void *);
void	xen_deferred(void *);
void	xen_resume(struct device *);
int	xen_activate(struct device *, int);

const struct cfdriver xen_cd = {
	NULL, "xen", DV_DULL
};

const struct cfattach xen_ca = {
	sizeof(struct xen_softc), xen_match, xen_attach, NULL, xen_activate
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

	mountroothook_establish(xen_deferred, sc);
}

void
xen_deferred(void *arg)
{
	struct xen_softc *sc = arg;

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
	if ((version = xen_hypercall(sc, xen_version, 1, XENVER_version)) < 0) {
		printf("%s: failed to fetch version\n", sc->sc_dev.dv_xname);
		return (-1);
	}
	if (xen_hypercallv(sc, xen_version, argc, argv) < 0) {
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
	if (xen_hypercallv(sc, xen_version, argc, argv) < 0) {
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
	if (xen_hypercall(sc, memory_op, 2, XENMEM_add_to_physmap, &xatp)) {
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
	if (xen_hypercall(sc, hvm_op, 2, HVMOP_set_param, &xhp)) {
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
		xen_hypercall(sc, event_channel_op, 2, EVTCHNOP_send, &es);
	}
}

int
xen_intr_establish(evtchn_port_t port, xen_intr_handle_t *xih,
    void (*handler)(void *), void *arg, char *name)
{
	struct xen_softc *sc = xen_sc;
	struct xen_intsrc *xi;
	struct evtchn_alloc_unbound eau;
	struct evtchn_unmask eu;
#if notyet
	struct evtchn_bind_vcpu ebv;
#endif
#ifdef XEN_DEBUG
	struct evtchn_status es;
#endif

	if (port && xen_lookup_intsrc(sc, port)) {
		printf("%s: interrupt handler has already been established "
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
		if (xen_hypercall(sc, event_channel_op, 2,
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
	if (xen_hypercall(sc, event_channel_op, 2, EVTCHNOP_bind_vcpu, &ebv)) {
		printf("%s: failed to bind interrupt on port %u to vcpu%d\n",
		    sc->sc_dev.dv_xname, ebv.port, ebv.vcpu);
	}
#endif

	evcount_attach(&xi->xi_evcnt, name, &sc->sc_irq);

	SLIST_INSERT_HEAD(&sc->sc_intrs, xi, xi_entry);

	if (!cold) {
		eu.port = xi->xi_port;
		if (xen_hypercall(sc, event_channel_op, 2, EVTCHNOP_unmask,
		    &eu) || isset(sc->sc_ipg->evtchn_mask, port))
			printf("%s: unmasking port %u failed\n",
			    sc->sc_dev.dv_xname, port);
	}

#ifdef XEN_DEBUG
	memset(&es, 0, sizeof(es));
	es.dom = DOMID_SELF;
	es.port = xi->xi_port;
	if (xen_hypercall(sc, event_channel_op, 2, EVTCHNOP_status, &es)) {
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

	if (xen_lookup_intsrc(sc, port) != NULL) {
		DPRINTF("%s: failed to lookup an established interrupt handler "
		    "for port %u\n", sc->sc_dev.dv_xname, port);
		return (-1);
	}

	evcount_detach(&xi->xi_evcnt);

	SLIST_REMOVE(&sc->sc_intrs, xi, xen_intsrc, xi_entry);

	setbit((char *)sc->sc_ipg->evtchn_mask, xi->xi_port);
	clrbit((char *)sc->sc_ipg->evtchn_pending[0], xi->xi_port);
	membar_producer();

	if (!xi->xi_noclose) {
		ec.port = xi->xi_port;
		if (xen_hypercall(sc, event_channel_op, 2, EVTCHNOP_close,
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
			if (xen_hypercall(sc, event_channel_op, 2,
			    EVTCHNOP_unmask, &eu) ||
			    isset(sc->sc_ipg->evtchn_mask, xi->xi_port))
				printf("%s: unmasking port %u failed\n",
				    sc->sc_dev.dv_xname, xi->xi_port);
		}
	}
}
