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

int 	xen_match(struct device *, void *, void *);
void	xen_attach(struct device *, struct device *, void *);
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

void
xen_intr(void)
{
	/* stub */
}
