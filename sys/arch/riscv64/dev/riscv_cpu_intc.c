/*
 * Copyright (c) 2020, Mars Li <mengshi.li.mars@gmail.com>
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
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/riscvreg.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include "riscv64/dev/plic.h"
#include "riscv_cpu_intc.h"

struct intrhand {
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
	int ih_irq;			/* IRQ number */
	char *ih_name;
};

struct intrhand* intc_handler[INTC_NIRQS] = {NULL};
struct interrupt_controller intc_ic;

int	riscv_intc_match(struct device *, void *, void *);
void	riscv_intc_attach(struct device *, struct device *, void *);

void	riscv_intc_irq_handler(void *);
void	*riscv_intc_intr_establish(int, int, int (*)(void *),
		void *, char *);
void	riscv_intc_intr_disestablish(void *);


struct cfattach        intc_ca = {
       sizeof (struct device), riscv_intc_match, riscv_intc_attach
};

struct cfdriver intc_cd = {
       NULL, "rv_cpu_intc", DV_DULL
};

int
riscv_intc_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;
	int node = faa->fa_node;
	return (OF_getproplen(node, "interrupt-controller") >= 0 &&
		OF_is_compatible(node, "riscv,cpu-intc"));
}

void
riscv_intc_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;/* should only use fa_node field */

	riscv_init_smask();

	/* hook the intr_handler */
	riscv_set_intr_handler(riscv_intc_irq_handler);

	intc_ic.ic_node = faa->fa_node;
	intc_ic.ic_cookie = &intc_ic;

	/*
	 * only allow install/uninstall handler to/from global vector
	 * by calling riscv_intc_intr_establish/disestablish
	 */
	intc_ic.ic_establish = NULL;
	intc_ic.ic_disestablish = NULL;

	riscv_intr_register_fdt(&intc_ic);

	/*
	 * XXX right time to enable interrupts ??
	 * might need to postpone untile autoconf is finished
	 */
	enable_interrupts();
}


/* global interrupt handler */
void
riscv_intc_irq_handler(void *frame)
{
	int irq;
	struct intrhand *ih;
	struct trapframe *_frame;
        _frame = (struct trapframe*) frame;

	KASSERTMSG(_frame->tf_scause & EXCP_INTR,
		"riscv_cpu_intr: wrong frame passed");

	irq = (_frame->tf_scause & EXCP_MASK);
#ifdef DEBUG_INTC
	printf("irq %d fired\n", irq);
#endif

	ih = intc_handler[irq];
	if (ih->ih_func(frame) == 0)
#ifdef DEBUG_INTC
		printf("fail in handling irq %d %s\n", irq, ih->ih_name);
#else
		;
#endif /* DEBUG_INTC */
}

void *
riscv_intc_intr_establish(int irqno, int dummy_level, int (*func)(void *),
    void *arg, char *name)
{
	int sie;
	struct intrhand *ih;

	if (irqno < 0 || irqno >= INTC_NIRQS)
		panic("intc_intr_establish: bogus irqnumber %d: %s",
		     irqno, name);
	sie = disable_interrupts();

	ih = malloc(sizeof(*ih), M_DEVBUF, M_WAITOK);
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_irq = irqno;
	ih->ih_name = name;

	intc_handler[irqno] = ih;
#ifdef DEBUG_INTC
	printf("\nintc_intr_establish irq %d [%s]\n", irqno, name);
#endif
	restore_interrupts(sie);
	return (ih);
}

void
riscv_intc_intr_disestablish(void *cookie)
{
	int sie;
	struct intrhand *ih = cookie;
	int irqno = ih->ih_irq;
	sie = disable_interrupts();

	intc_handler[irqno] = NULL;
	free(ih, M_DEVBUF, 0);

	restore_interrupts(sie);
}
