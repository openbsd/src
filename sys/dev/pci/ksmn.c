/*	$OpenBSD: ksmn.c,v 1.4 2020/01/04 01:34:24 jsg Exp $	*/

/*
 * Copyright (c) 2019 Bryan Steele <brynet@openbsd.org>
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
#include <sys/device.h>
#include <sys/sensors.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/*
 * AMD temperature sensors on Family 17h (and some 15h) must be
 * read from the System Management Unit (SMU) co-processor over
 * the System Management Network (SMN).
 */

#define SMN_17H_ADDR_R 0x60
#define SMN_17H_DATA_R 0x64

/*
  * AMD Family 17h SMU Thermal Registers (THM)
  *
  * 4.2.1, OSRR (Open-Source Register Reference) Guide for Family 17h
  *     [31:21]  Current reported temperature.
  */
#define SMU_17H_THM	0x59800
#define GET_CURTMP(r)	(((r) >> 21) & 0x7ff)

/*
 * Bit 19 set: "Report on -49C to 206C scale range."
 *      clear: "Report on 0C to 225C (255C?) scale range."
 */
#define CURTMP_17H_RANGE_SEL	(1 << 19)
#define CURTMP_17H_RANGE_ADJUST	490

/*
 * Undocumented tCTL offsets gleamed from Linux k10temp driver.
 */
struct curtmp_offset {
	const char *const cpu_model;
	int tctl_offset;
} cpu_model_offsets[] = {
	{ "AMD Ryzen 5 1600X", 200 },
	{ "AMD Ryzen 7 1700X", 200 },
	{ "AMD Ryzen 7 1800X", 200 },
	{ "AMD Ryzen 7 2700X", 100 },
	{ "AMD Ryzen Threadripper 19", 270 }, /* many models */
	{ "AMD Ryzen Threadripper 29", 270 }, /* many models */
	/* ... */
	{ NULL, 0 },
};

struct ksmn_softc {
	struct device		sc_dev;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_pcitag;

	int			sc_tctl_offset;

	struct ksensor		sc_sensor;
	struct ksensordev	sc_sensordev;
};

int	ksmn_match(struct device *, void *, void *);
void	ksmn_attach(struct device *, struct device *, void *);
void	ksmn_refresh(void *);

struct cfattach ksmn_ca = {
	sizeof(struct ksmn_softc), ksmn_match, ksmn_attach
};

struct cfdriver ksmn_cd = {
	NULL, "ksmn", DV_DULL
};

static const struct pci_matchid ksmn_devices[] = {
	{ PCI_VENDOR_AMD, PCI_PRODUCT_AMD_17_RC },
	{ PCI_VENDOR_AMD, PCI_PRODUCT_AMD_17_1X_RC },
	{ PCI_VENDOR_AMD, PCI_PRODUCT_AMD_17_3X_RC }
};

int
ksmn_match(struct device *parent, void *match, void *aux)
{
	/* successful match supersedes pchb(4) */
	return pci_matchbyid((struct pci_attach_args *)aux, ksmn_devices,
	    sizeof(ksmn_devices) / sizeof(ksmn_devices[0])) * 2;
}

void
ksmn_attach(struct device *parent, struct device *self, void *aux)
{
	struct ksmn_softc	*sc = (struct ksmn_softc *)self;
	struct pci_attach_args	*pa = aux;
	struct curtmp_offset	*p;
	extern char		cpu_model[];

	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;

	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	sc->sc_sensor.type = SENSOR_TEMP;
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor);

	/*
	 * Zen/Zen+ CPUs are offset if TDP > 65, otherwise 0.
	 * Zen 2 models appear to have no tCTL offset, so always 0.
	 *
	 * XXX: Does any public documentation exist for this?
	 */
	for (p = cpu_model_offsets; p->cpu_model != NULL; p++) {
		/* match partial string */
		if (!strncmp(cpu_model, p->cpu_model, strlen(p->cpu_model)))
			sc->sc_tctl_offset = p->tctl_offset;
	}

	if (sensor_task_register(sc, ksmn_refresh, 5) == NULL) {
		printf(": unable to register update task\n");
		return;
	}

	sensordev_install(&sc->sc_sensordev);

	printf("\n");
}

void
ksmn_refresh(void *arg)
{
	struct ksmn_softc	*sc = arg;
	struct ksensor		*s = &sc->sc_sensor;
	pcireg_t		reg;
	int 			raw, offset = 0;

	pci_conf_write(sc->sc_pc, sc->sc_pcitag, SMN_17H_ADDR_R,
	    SMU_17H_THM);
	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, SMN_17H_DATA_R);

	raw = GET_CURTMP(reg);
	if ((reg & CURTMP_17H_RANGE_SEL) != 0)
		offset -= CURTMP_17H_RANGE_ADJUST;
	offset -= sc->sc_tctl_offset;
	/* convert to uC */
	offset *= 100000;

	/* convert raw (in steps of 0.125C) to uC, add offset, uC to uK. */
	s->value = ((raw * 125000) + offset) + 273150000;
}
