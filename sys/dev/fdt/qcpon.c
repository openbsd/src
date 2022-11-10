/*	$OpenBSD: qcpon.c,v 1.2 2022/11/10 16:20:54 patrick Exp $	*/
/*
 * Copyright (c) 2022 Patrick Wildt <patrick@blueri.se>
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
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/task.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/fdt/spmivar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_power.h>
#include <dev/ofw/fdt.h>

struct qcpon_softc {
	struct device		sc_dev;
	int			sc_node;

	spmi_tag_t		sc_tag;
	int8_t			sc_sid;

	void			*sc_pwrkey_ih;
	int			sc_pwrkey_debounce;
	struct task		sc_powerdown_task;
};

int	qcpon_match(struct device *, void *, void *);
void	qcpon_attach(struct device *, struct device *, void *);

int	qcpon_pwrkey_intr(void *);
void	qcpon_powerdown_task(void *);

const struct cfattach qcpon_ca = {
	sizeof(struct qcpon_softc), qcpon_match, qcpon_attach
};

struct cfdriver qcpon_cd = {
	NULL, "qcpon", DV_DULL
};

int
qcpon_match(struct device *parent, void *match, void *aux)
{
	struct spmi_attach_args *saa = aux;

	return OF_is_compatible(saa->sa_node, "qcom,pm8998-pon");
}

void
qcpon_attach(struct device *parent, struct device *self, void *aux)
{
	struct spmi_attach_args *saa = aux;
	struct qcpon_softc *sc = (struct qcpon_softc *)self;
	int node, reg;

	reg = OF_getpropint(saa->sa_node, "reg", -1);
	if (reg < 0) {
		printf(": can't find registers\n");
		return;
	}

	sc->sc_node = saa->sa_node;
	sc->sc_tag = saa->sa_tag;
	sc->sc_sid = saa->sa_sid;

	task_set(&sc->sc_powerdown_task, qcpon_powerdown_task, sc);

	printf("\n");

	for (node = OF_child(saa->sa_node); node; node = OF_peer(node)) {
		if (OF_is_compatible(node, "qcom,pmk8350-pwrkey")) {
			sc->sc_pwrkey_ih = fdt_intr_establish(node, IPL_BIO,
			    qcpon_pwrkey_intr, sc, sc->sc_dev.dv_xname);
			if (sc->sc_pwrkey_ih == NULL) {
				printf("%s: can't establish interrupt\n",
				    sc->sc_dev.dv_xname);
				continue;
			}
		}
	}
}

int
qcpon_pwrkey_intr(void *arg)
{
	struct qcpon_softc *sc = arg;

	/* Ignore presses, handle releases. */
	sc->sc_pwrkey_debounce = (sc->sc_pwrkey_debounce + 1) % 2;
	if (sc->sc_pwrkey_debounce == 1)
		return 1;

	task_add(systq, &sc->sc_powerdown_task);
	return 1;
}

void
qcpon_powerdown_task(void *arg)
{
	extern int allowpowerdown;

	if (allowpowerdown == 1) {
		allowpowerdown = 0;
		prsignal(initprocess, SIGUSR2);
	}
}
