/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 *	$OpenBSD: piixpm.c,v 1.2 2004/09/17 10:18:01 grange Exp $
 *	$FreeBSD: /repoman/r/ncvs/src/sys/i386/i386/mp_clock.c,v 1.19 2004/05/30 20:34:57 phk Exp $
 */

/*-
 * Just when we thought life were beautiful, reality pops its grim face over
 * the edge again:
 *
 * ] 20. ACPI Timer Errata
 * ]
 * ]   Problem: The power management timer may return improper result when
 * ]   read. Although the timer value settles properly after incrementing,
 * ]   while incrementing there is a 3nS window every 69.8nS where the
 * ]   timer value is indeterminate (a 4.2% chance that the data will be
 * ]   incorrect when read). As a result, the ACPI free running count up
 * ]   timer specification is violated due to erroneous reads.  Implication:
 * ]   System hangs due to the "inaccuracy" of the timer when used by
 * ]   software for time critical events and delays.
 * ] 
 * ] Workaround: Read the register twice and compare.
 * ] Status: This will not be fixed in the PIIX4 or PIIX4E.
 *
 * The counter is in other words not latched to the PCI bus clock when
 * read.  Notice the workaround isn't:  We need to read until we have
 * three monotonic samples and then use the middle one, otherwise we are
 * not protected against the fact that the bits can be wrong in two
 * directions.  If we only cared about monosity two reads would be enough.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#ifdef __HAVE_TIMECOUNTER
#include <sys/timetc.h>
#endif

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#define PIIX_PMPTR	0x40		/* PIIX PM address ptr */

#define	PIIX_PMBASE(x)	((x) & 0xffc0)	/* PIIX PM base address */
#define	PIIX_PMSIZE	56		/* PIIX PM space size */

struct piixpm_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

int piixpm_probe(struct device *, void *, void *);
void piixpm_attach(struct device *, struct device *, void *);

#ifdef __HAVE_TIMECOUNTER
u_int piix_get_timecount(struct timecounter *tc);

static u_int piix_freq = 14318182/4;

static struct timecounter piix_timecounter = {
	piix_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffff,		/* counter_mask */
	0,			/* frequency */
	"PIIX",			/* name */
	1000			/* quality */
};
#endif

struct cfattach piixpm_ca = {
	sizeof(struct piixpm_softc), piixpm_probe, piixpm_attach
};

struct cfdriver piixpm_cd = {
	NULL, "piixpm", DV_DULL
};

#if 0
int
sysctl_machdep_piix_freq(SYSCTL_HANDLER_ARGS)
{
	int error;
	u_int freq;

	if (piix_timecounter.tc_frequency == 0)
		return (EOPNOTSUPP);
	freq = piix_freq;
	error = sysctl_handle_int(oidp, &freq, sizeof(freq), req);
	if (error == 0 && req->newptr != NULL) {
		piix_freq = freq;
		piix_timecounter.tc_frequency = piix_freq;
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, piix_freq, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof(u_int), sysctl_machdep_piix_freq, "I", "");
#endif

#ifdef __HAVE_TIMECOUNTER
u_int
piix_get_timecount(struct timecounter *tc)
{
	struct piixpm_softc *sc = (struct piixpm_softc *) tc->tc_priv;
	u_int u1, u2, u3;

	u2 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, 8);
	u3 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, 8);
	do {
		u1 = u2;
		u2 = u3;
		u3 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, 8);
	} while (u1 > u2 || u2 > u3);
	return (u2);
}
#endif

/*
 * XXX - this has to be redone if we ever do real ACPI
 */
int
piixpm_probe(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;
	pcireg_t reg;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_INTEL_82371AB_PMC)
		return (0);

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	if ((reg & PCI_COMMAND_IO_ENABLE) == 0)
		return (0);
	return (1);
}

void
piixpm_attach(struct device *parent, struct device *self, void *aux)
{
	struct piixpm_softc *sc = (struct piixpm_softc *) self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcireg_t reg;

        reg = pci_conf_read(pc, pa->pa_tag, PIIX_PMPTR);
        if (bus_space_map(pa->pa_iot, PIIX_PMBASE(reg), PIIX_PMSIZE,
            0, &sc->sc_ioh)) {
                printf(": can't map i/o space\n");
                return;
        }

	sc->sc_iot = pa->pa_iot;
	printf("\n");
#ifdef __HAVE_TIMECOUNTER
	piix_timecounter.tc_frequency = piix_freq;
	piix_timecounter.tc_priv = sc;
	tc_init(&piix_timecounter);
#endif
}
