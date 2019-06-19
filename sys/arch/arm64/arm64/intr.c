/* $OpenBSD: intr.c,v 1.14 2019/05/13 20:55:22 drahn Exp $ */
/*
 * Copyright (c) 2011 Dale Rahn <drahn@openbsd.org>
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
#include <sys/timetc.h>
#include <sys/malloc.h>

#include <dev/clock_subr.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/ofw/openfirm.h>

uint32_t arm_intr_get_parent(int);
uint32_t arm_intr_map_msi(int, uint64_t *);

void *arm_intr_prereg_establish_fdt(void *, int *, int, int (*)(void *),
    void *, char *);
void arm_intr_prereg_disestablish_fdt(void *);

int arm_dflt_splraise(int);
int arm_dflt_spllower(int);
void arm_dflt_splx(int);
void arm_dflt_setipl(int);

void arm_dflt_intr(void *);
void arm_cpu_intr(void *);

#define SI_TO_IRQBIT(x) (1 << (x))
uint32_t arm_smask[NIPL];

struct arm_intr_func arm_intr_func = {
	arm_dflt_splraise,
	arm_dflt_spllower,
	arm_dflt_splx,
	arm_dflt_setipl
};

void (*arm_intr_dispatch)(void *) = arm_dflt_intr;

void
arm_cpu_intr(void *frame)
{
	struct cpu_info	*ci = curcpu();

	ci->ci_idepth++;
	(*arm_intr_dispatch)(frame);
	ci->ci_idepth--;
}
void
arm_dflt_intr(void *frame)
{
	panic("arm_dflt_intr() called");
}

/*
 * Find the interrupt parent by walking up the tree.
 */
uint32_t
arm_intr_get_parent(int node)
{
	uint32_t phandle = 0;

	while (node && !phandle) {
		phandle = OF_getpropint(node, "interrupt-parent", 0);
		node = OF_parent(node);
	}

	return phandle;
}

uint32_t
arm_intr_map_msi(int node, uint64_t *data)
{
	uint64_t msi_base;
	uint32_t phandle = 0;
	uint32_t *cell;
	uint32_t *map;
	uint32_t mask, rid_base, rid;
	int i, len, length, mcells, ncells;

	len = OF_getproplen(node, "msi-map");
	if (len <= 0) {
		while (node && !phandle) {
			phandle = OF_getpropint(node, "msi-parent", 0);
			node = OF_parent(node);
		}

		return phandle;
	}

	map = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "msi-map", map, len);

	mask = OF_getpropint(node, "msi-map-mask", 0xffff);
	rid = *data & mask;

	cell = map;
	ncells = len / sizeof(uint32_t);
	while (ncells > 1) {
		node = OF_getnodebyphandle(cell[1]);
		if (node == 0)
			goto out;

		/*
		 * Some device trees (e.g. those for the Rockchip
		 * RK3399 boards) are missing a #msi-cells property.
		 * Assume the msi-specifier uses a single cell in that
		 * case.
		 */
		mcells = OF_getpropint(node, "#msi-cells", 1);
		if (ncells < mcells + 3)
			goto out;

		rid_base = cell[0];
		length = cell[2 + mcells];
		msi_base = cell[2];
		for (i = 1; i < mcells; i++) {
			msi_base <<= 32;
			msi_base |= cell[2 + i];
		}
		if (rid >= rid_base && rid < rid_base + length) {
			*data = msi_base + (rid - rid_base);
			phandle = cell[1];
			break;
		}

		cell += (3 + mcells);
		ncells -= (3 + mcells);
	}

out:
	free(map, M_TEMP, len);
	return phandle;
}

/*
 * Interrupt pre-registration.
 *
 * To allow device drivers to establish interrupt handlers before all
 * relevant interrupt controllers have been attached, we support
 * pre-registration of interrupt handlers.  For each node in the
 * device tree that has an "interrupt-controller" property, we
 * register a dummy interrupt controller that simply stashes away all
 * relevant details of the interrupt handler being established.
 * Later, when the real interrupt controller registers itself, we
 * establush those interrupt handlers based on that information.
 */

#define MAX_INTERRUPT_CELLS	4

struct intr_prereg {
	LIST_ENTRY(intr_prereg) ip_list;
	uint32_t ip_phandle;
	uint32_t ip_cell[MAX_INTERRUPT_CELLS];

	int ip_level;
	int (*ip_func)(void *);
	void *ip_arg;
	char *ip_name;

	struct interrupt_controller *ip_ic;
	void *ip_ih;
};

LIST_HEAD(, intr_prereg) prereg_interrupts =
	LIST_HEAD_INITIALIZER(prereg_interrupts);

void *
arm_intr_prereg_establish_fdt(void *cookie, int *cell, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct interrupt_controller *ic = cookie;
	struct intr_prereg *ip;
	int i;

	ip = malloc(sizeof(struct intr_prereg), M_DEVBUF, M_ZERO | M_WAITOK);
	ip->ip_phandle = ic->ic_phandle;
	for (i = 0; i < ic->ic_cells; i++)
		ip->ip_cell[i] = cell[i];
	ip->ip_level = level;
	ip->ip_func = func;
	ip->ip_arg = arg;
	ip->ip_name = name;
	LIST_INSERT_HEAD(&prereg_interrupts, ip, ip_list);

	return ip;
}

void
arm_intr_prereg_disestablish_fdt(void *cookie)
{
	struct intr_prereg *ip = cookie;
	struct interrupt_controller *ic = ip->ip_ic;

	if (ip->ip_ic != NULL && ip->ip_ih != NULL)
		ic->ic_disestablish(ip->ip_ih);

	if (ip->ip_ic != NULL)
		LIST_REMOVE(ip, ip_list);

	free(ip, M_DEVBUF, sizeof(*ip));
}

void
arm_intr_init_fdt_recurse(int node)
{
	struct interrupt_controller *ic;

	if (OF_getproplen(node, "interrupt-controller") >= 0) {
		ic = malloc(sizeof(struct interrupt_controller),
		    M_DEVBUF, M_ZERO | M_WAITOK);
		ic->ic_node = node;
		ic->ic_cookie = ic;
		ic->ic_establish = arm_intr_prereg_establish_fdt;
		ic->ic_disestablish = arm_intr_prereg_disestablish_fdt;
		arm_intr_register_fdt(ic);
	}

	for (node = OF_child(node); node; node = OF_peer(node))
		arm_intr_init_fdt_recurse(node);
}

void
arm_intr_init_fdt(void)
{
	int node = OF_peer(0);

	if (node)
		arm_intr_init_fdt_recurse(node);
}

LIST_HEAD(, interrupt_controller) interrupt_controllers =
	LIST_HEAD_INITIALIZER(interrupt_controllers);

void
arm_intr_register_fdt(struct interrupt_controller *ic)
{
	struct intr_prereg *ip, *tip;

	ic->ic_cells = OF_getpropint(ic->ic_node, "#interrupt-cells", 0);
	ic->ic_phandle = OF_getpropint(ic->ic_node, "phandle", 0);
	if (ic->ic_phandle == 0)
		return;
	KASSERT(ic->ic_cells <= MAX_INTERRUPT_CELLS);

	LIST_INSERT_HEAD(&interrupt_controllers, ic, ic_list);

	/* Establish pre-registered interrupt handlers. */
	LIST_FOREACH_SAFE(ip, &prereg_interrupts, ip_list, tip) {
		if (ip->ip_phandle != ic->ic_phandle)
			continue;

		ip->ip_ic = ic;
		ip->ip_ih = ic->ic_establish(ic->ic_cookie, ip->ip_cell,
		    ip->ip_level, ip->ip_func, ip->ip_arg, ip->ip_name);
		if (ip->ip_ih == NULL)
			printf("can't establish interrupt %s\n", ip->ip_name);

		LIST_REMOVE(ip, ip_list);
	}
}

struct arm_intr_handle {
	struct interrupt_controller *ih_ic;
	void *ih_ih;
};

void *
arm_intr_establish_fdt(int node, int level, int (*func)(void *),
    void *cookie, char *name)
{
	return arm_intr_establish_fdt_idx(node, 0, level, func, cookie, name);
}

void *
arm_intr_establish_fdt_idx(int node, int idx, int level, int (*func)(void *),
    void *cookie, char *name)
{
	struct interrupt_controller *ic;
	int i, len, ncells, extended = 1;
	uint32_t *cell, *cells, phandle;
	struct arm_intr_handle *ih;
	void *val = NULL;

	len = OF_getproplen(node, "interrupts-extended");
	if (len <= 0) {
		len = OF_getproplen(node, "interrupts");
		extended = 0;
	}
	if (len <= 0 || (len % sizeof(uint32_t) != 0))
		return NULL;

	/* Old style. */
	if (!extended) {
		phandle = arm_intr_get_parent(node);
		LIST_FOREACH(ic, &interrupt_controllers, ic_list) {
			if (ic->ic_phandle == phandle)
				break;
		}

		if (ic == NULL)
			return NULL;
	}

	cell = cells = malloc(len, M_TEMP, M_WAITOK);
	if (extended)
		OF_getpropintarray(node, "interrupts-extended", cells, len);
	else
		OF_getpropintarray(node, "interrupts", cells, len);
	ncells = len / sizeof(uint32_t);

	for (i = 0; i <= idx && ncells > 0; i++) {
		if (extended) {
			phandle = cell[0];

			LIST_FOREACH(ic, &interrupt_controllers, ic_list) {
				if (ic->ic_phandle == phandle)
					break;
			}

			if (ic == NULL)
				break;

			cell++;
			ncells--;
		}

		if (i == idx && ncells >= ic->ic_cells && ic->ic_establish) {
			val = ic->ic_establish(ic->ic_cookie, cell, level,
			    func, cookie, name);
			break;
		}

		cell += ic->ic_cells;
		ncells -= ic->ic_cells;
	}

	free(cells, M_TEMP, len);

	if (val == NULL)
		return NULL;

	ih = malloc(sizeof(*ih), M_DEVBUF, M_WAITOK);
	ih->ih_ic = ic;
	ih->ih_ih = val;

	return ih;
}

void *
arm_intr_establish_fdt_imap(int node, int *reg, int nreg, int level,
    int (*func)(void *), void *cookie, char *name)
{
	struct interrupt_controller *ic;
	struct arm_intr_handle *ih;
	uint32_t *cell;
	uint32_t map_mask[4], *map;
	int len, acells, ncells;
	void *val = NULL;

	if (nreg != sizeof(map_mask))
		return NULL;

	if (OF_getpropintarray(node, "interrupt-map-mask", map_mask,
	    sizeof(map_mask)) != sizeof(map_mask))
		return NULL;

	len = OF_getproplen(node, "interrupt-map");
	if (len <= 0)
		return NULL;

	map = malloc(len, M_DEVBUF, M_WAITOK);
	OF_getpropintarray(node, "interrupt-map", map, len);

	cell = map;
	ncells = len / sizeof(uint32_t);
	while (ncells > 5) {
		LIST_FOREACH(ic, &interrupt_controllers, ic_list) {
			if (ic->ic_phandle == cell[4])
				break;
		}

		if (ic == NULL)
			break;

		acells = OF_getpropint(ic->ic_node, "#address-cells", 0);
		if (ncells >= (5 + acells + ic->ic_cells) &&
		    (reg[0] & map_mask[0]) == cell[0] &&
		    (reg[1] & map_mask[1]) == cell[1] &&
		    (reg[2] & map_mask[2]) == cell[2] &&
		    (reg[3] & map_mask[3]) == cell[3] &&
		    ic->ic_establish) {
			val = ic->ic_establish(ic->ic_cookie, &cell[5 + acells],
			    level, func, cookie, name);
			break;
		}

		cell += (5 + acells + ic->ic_cells);
		ncells -= (5 + acells + ic->ic_cells);
	}

	if (val == NULL) {
		free(map, M_DEVBUF, len);
		return NULL;
	}

	ih = malloc(sizeof(*ih), M_DEVBUF, M_WAITOK);
	ih->ih_ic = ic;
	ih->ih_ih = val;

	free(map, M_DEVBUF, len);
	return ih;
}

void *
arm_intr_establish_fdt_msi(int node, uint64_t *addr, uint64_t *data,
    int level, int (*func)(void *), void *cookie, char *name)
{
	struct interrupt_controller *ic;
	struct arm_intr_handle *ih;
	uint32_t phandle;
	void *val = NULL;

	phandle = arm_intr_map_msi(node, data);
	LIST_FOREACH(ic, &interrupt_controllers, ic_list) {
		if (ic->ic_phandle == phandle)
			break;
	}

	if (ic == NULL || ic->ic_establish_msi == NULL)
		return NULL;

	val = ic->ic_establish_msi(ic->ic_cookie, addr, data,
	    level, func, cookie, name);

	ih = malloc(sizeof(*ih), M_DEVBUF, M_WAITOK);
	ih->ih_ic = ic;
	ih->ih_ih = val;

	return ih;
}

void
arm_intr_disestablish_fdt(void *cookie)
{
	struct arm_intr_handle *ih = cookie;
	struct interrupt_controller *ic = ih->ih_ic;

	ic->ic_disestablish(ih->ih_ih);
	free(ih, M_DEVBUF, sizeof(*ih));
}

void
arm_intr_enable(void *cookie)
{
	struct arm_intr_handle *ih = cookie;
	struct interrupt_controller *ic = ih->ih_ic;

	KASSERT(ic->ic_enable != NULL);
	ic->ic_enable(ih->ih_ih);
}

void
arm_intr_disable(void *cookie)
{
	struct arm_intr_handle *ih = cookie;
	struct interrupt_controller *ic = ih->ih_ic;

	KASSERT(ic->ic_disable != NULL);
	ic->ic_disable(ih->ih_ih);
}

/*
 * Some interrupt controllers transparently forward interrupts to
 * their parent.  Such interrupt controllers can use this function to
 * delegate the interrupt handler to their parent.
 */
void *
arm_intr_parent_establish_fdt(void *cookie, int *cell, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct interrupt_controller *ic = cookie;
	struct arm_intr_handle *ih;
	uint32_t phandle;
	void *val;

	phandle = arm_intr_get_parent(ic->ic_node);
	LIST_FOREACH(ic, &interrupt_controllers, ic_list) {
		if (ic->ic_phandle == phandle)
			break;
	}
	if (ic == NULL)
		return NULL;

	val = ic->ic_establish(ic->ic_cookie, cell, level, func, arg, name);
	if (val == NULL)
		return NULL;

	ih = malloc(sizeof(*ih), M_DEVBUF, M_WAITOK);
	ih->ih_ic = ic;
	ih->ih_ih = val;

	return ih;
}

void
arm_intr_parent_disestablish_fdt(void *cookie)
{
	struct arm_intr_handle *ih = cookie;
	struct interrupt_controller *ic = ih->ih_ic;

	ic->ic_disestablish(ih->ih_ih);
	free(ih, M_DEVBUF, sizeof(*ih));
}

void
arm_intr_route(void *cookie, int enable, struct cpu_info *ci)
{
	struct arm_intr_handle *ih = cookie;
	struct interrupt_controller *ic = ih->ih_ic;

	if (ic->ic_route)
		ic->ic_route(ih->ih_ih, enable, ci);
}

void
arm_intr_cpu_enable(void)
{
	struct interrupt_controller *ic;

	LIST_FOREACH(ic, &interrupt_controllers, ic_list)
		if (ic->ic_cpu_enable)
			ic->ic_cpu_enable();
}

int
arm_dflt_splraise(int newcpl)
{
	struct cpu_info *ci = curcpu();
	int oldcpl;

	oldcpl = ci->ci_cpl;

	if (newcpl < oldcpl)
		newcpl = oldcpl;

	ci->ci_cpl = newcpl;

	return oldcpl;
}

int
arm_dflt_spllower(int newcpl)
{
	struct cpu_info *ci = curcpu();
	int oldcpl;

	oldcpl = ci->ci_cpl;

	splx(newcpl);

	return oldcpl;
}

void
arm_dflt_splx(int newcpl)
{
	struct cpu_info *ci = curcpu();

	if (ci->ci_ipending & arm_smask[newcpl])
		arm_do_pending_intr(newcpl);
	ci->ci_cpl = newcpl;
}

void
arm_dflt_setipl(int newcpl)
{
	struct cpu_info *ci = curcpu();

	ci->ci_cpl = newcpl;
}

void
arm_do_pending_intr(int pcpl)
{
	struct cpu_info *ci = curcpu();
	int oldirqstate;

	oldirqstate = disable_interrupts();

#define DO_SOFTINT(si, ipl) \
	if ((ci->ci_ipending & arm_smask[pcpl]) &	\
	    SI_TO_IRQBIT(si)) {						\
		ci->ci_ipending &= ~SI_TO_IRQBIT(si);			\
		arm_intr_func.setipl(ipl);				\
		restore_interrupts(oldirqstate);			\
		softintr_dispatch(si);					\
		oldirqstate = disable_interrupts();			\
	}

	do {
		DO_SOFTINT(SIR_TTY, IPL_SOFTTTY);
		DO_SOFTINT(SIR_NET, IPL_SOFTNET);
		DO_SOFTINT(SIR_CLOCK, IPL_SOFTCLOCK);
		DO_SOFTINT(SIR_SOFT, IPL_SOFT);
	} while (ci->ci_ipending & arm_smask[pcpl]);

	/* Don't use splx... we are here already! */
	arm_intr_func.setipl(pcpl);
	restore_interrupts(oldirqstate);
}

void arm_set_intr_handler(int (*raise)(int), int (*lower)(int),
    void (*x)(int), void (*setipl)(int),
	void (*intr_handle)(void *))
{
	arm_intr_func.raise		= raise;
	arm_intr_func.lower		= lower;
	arm_intr_func.x			= x;
	arm_intr_func.setipl		= setipl;
	arm_intr_dispatch		= intr_handle;
}

void
arm_init_smask(void)
{
	static int inited = 0;
	int i;

	if (inited)
		return;
	inited = 1;

	for (i = IPL_NONE; i <= IPL_HIGH; i++)  {
		arm_smask[i] = 0;
		if (i < IPL_SOFT)
			arm_smask[i] |= SI_TO_IRQBIT(SIR_SOFT);
		if (i < IPL_SOFTCLOCK)
			arm_smask[i] |= SI_TO_IRQBIT(SIR_CLOCK);
		if (i < IPL_SOFTNET)
			arm_smask[i] |= SI_TO_IRQBIT(SIR_NET);
		if (i < IPL_SOFTTTY)
			arm_smask[i] |= SI_TO_IRQBIT(SIR_TTY);
	}
}

/* provide functions for asm */
#undef splraise
#undef spllower
#undef splx

int
splraise(int ipl)
{
	return arm_intr_func.raise(ipl);
}

int _spllower(int ipl); /* XXX - called from asm? */
int
_spllower(int ipl)
{
	return arm_intr_func.lower(ipl);
}
int
spllower(int ipl)
{
	return arm_intr_func.lower(ipl);
}

void
splx(int ipl)
{
	arm_intr_func.x(ipl);
}


#ifdef DIAGNOSTIC
void
arm_splassert_check(int wantipl, const char *func)
{
	int oldipl = curcpu()->ci_cpl;

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		arm_intr_func.setipl(wantipl);
	}

	if (wantipl == IPL_NONE && curcpu()->ci_idepth != 0) {
		splassert_fail(-1, curcpu()->ci_idepth, func);
	}
}
#endif

void arm_dflt_delay(u_int usecs);

struct {
	void	(*delay)(u_int);
	void	(*initclocks)(void);
	void	(*setstatclockrate)(int);
	void	(*mpstartclock)(void);
} arm_clock_func = {
	arm_dflt_delay,
	NULL,
	NULL,
	NULL
};

void
arm_clock_register(void (*initclock)(void), void (*delay)(u_int),
    void (*statclock)(int), void(*mpstartclock)(void))
{
	if (arm_clock_func.initclocks)
		return;

	arm_clock_func.initclocks = initclock;
	arm_clock_func.delay = delay;
	arm_clock_func.setstatclockrate = statclock;
	arm_clock_func.mpstartclock = mpstartclock;
}


void
delay(u_int usec)
{
	arm_clock_func.delay(usec);
}

void
cpu_initclocks(void)
{
	if (arm_clock_func.initclocks == NULL)
		panic("initclocks function not initialized yet");

	arm_clock_func.initclocks();
}

void
cpu_startclock(void)
{
	if (arm_clock_func.mpstartclock == NULL)
		panic("startclock function not initialized yet");

	arm_clock_func.mpstartclock();
}

void
arm_dflt_delay(u_int usecs)
{
	int j;
	/* BAH - there is no good way to make this close */
	/* but this isn't supposed to be used after the real clock attaches */
	for (; usecs > 0; usecs--)
		for (j = 100; j > 0; j--)
			;

}

todr_chip_handle_t todr_handle;

/*
 * resettodr:
 *
 *      Reset the time-of-day register with the current time.
 */
void
resettodr(void)
{
	struct timeval rtctime;

	if (time_second == 1)
		return;

	microtime(&rtctime);

	if (todr_handle != NULL &&
	   todr_settime(todr_handle, &rtctime) != 0)
		printf("resettodr: failed to set time\n");
}

void
setstatclockrate(int new)
{
	if (arm_clock_func.setstatclockrate == NULL) {
		panic("arm_clock_func.setstatclockrate not intialized");
	}
	arm_clock_func.setstatclockrate(new);
}

void
intr_barrier(void *ih)
{
	sched_barrier(NULL);
}

/*
 * IPI implementation
 */

void arm_no_send_ipi(struct cpu_info *ci, int id);
void (*intr_send_ipi_func)(struct cpu_info *, int) = arm_no_send_ipi;

void
arm_send_ipi(struct cpu_info *ci, int id)
{
	(*intr_send_ipi_func)(ci, id);
}

void
arm_no_send_ipi(struct cpu_info *ci, int id)
{
	panic("arm_send_ipi() called: no ipi function");
}
