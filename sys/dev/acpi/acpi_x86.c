/* $OpenBSD: acpi_x86.c,v 1.15 2022/03/06 15:12:00 deraadt Exp $ */
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
 * Copyright (c) 2005 Jordan Hargrave <jordan@openbsd.org>
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

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/dsdt.h>

#include <machine/apmvar.h>

int
sleep_showstate(void *v, int sleepmode)
{
	struct acpi_softc *sc = v;

	switch (sleepmode) {
	case SLEEP_SUSPEND:
		sc->sc_state = ACPI_STATE_S3;
		break;
	case SLEEP_HIBERNATE:
		sc->sc_state = ACPI_STATE_S4;
		break;
	default:
		return (EOPNOTSUPP);
	}

	if (sc->sc_sleeptype[sc->sc_state].slp_typa == -1 ||
	    sc->sc_sleeptype[sc->sc_state].slp_typb == -1) {
		if (sc->sc_state == ACPI_STATE_S4) {
			sc->sc_state = ACPI_STATE_S5;	/* No S4, use S5 */
			printf("%s: S4 unavailable, using S5\n",
			    sc->sc_dev.dv_xname);
		} else {
			printf("%s: state S%d unavailable\n",
			    sc->sc_dev.dv_xname, sc->sc_state);
			return (EOPNOTSUPP);
		}
	}

	/* 1st suspend AML step: _TTS(tostate) */
	if (aml_node_setval(sc, sc->sc_tts, sc->sc_state) != 0)
		return (EINVAL);
	acpi_indicator(sc, ACPI_SST_WAKING);    /* blink */
	return 0;
}

int
sleep_setstate(void *v)
{
	struct acpi_softc *sc = v;

	/* 2nd suspend AML step: _PTS(tostate) */
	if (aml_node_setval(sc, sc->sc_pts, sc->sc_state) != 0)
		return (EINVAL);
	acpi_indicator(sc, ACPI_SST_WAKING);    /* blink */
	return 0;
}

int
gosleep(void *v)
{
	struct acpi_softc *sc = v;
	int ret;

	acpibtn_enable_psw();   /* enable _LID for wakeup */
	acpi_indicator(sc, ACPI_SST_SLEEPING);

	/* 3rd suspend AML step: _GTS(tostate) */
	aml_node_setval(sc, sc->sc_gts, sc->sc_state);

	/* Clear fixed event status */
	acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0, ACPI_PM1_ALL_STS);

	/* Enable wake GPEs */
	acpi_disable_allgpes(sc);
	acpi_enable_wakegpes(sc, sc->sc_state);

	ret = acpi_sleep_cpu(sc, sc->sc_state);
	acpi_resume_cpu(sc, sc->sc_state);
	sc->sc_state = ACPI_STATE_S0;

	return ret;
}

void
sleep_abort(void *v)
{
	struct acpi_softc *sc = v;

	sc->sc_state = ACPI_STATE_S0;
}

int
sleep_resume(void *v)
{
	struct acpi_softc *sc = v;

	acpibtn_disable_psw();		/* disable _LID for wakeup */

	/* 3rd resume AML step: _TTS(runstate) */
	if (aml_node_setval(sc, sc->sc_tts, sc->sc_state) != 0)
		return (EINVAL);
	acpi_indicator(sc, ACPI_SST_WAKING);    /* blink */
	return 0;
}


static int
checklids(struct acpi_softc *sc)
{
	extern int lid_action;
	int lids;

	lids = acpibtn_numopenlids();
	if (lids == 0 && lid_action != 0)
		return EAGAIN;
	return 0;
}	


int
suspend_finish(void *v)
{
	struct acpi_softc *sc = v;

	acpi_record_event(sc, APM_NORMAL_RESUME);
	acpi_indicator(sc, ACPI_SST_WORKING);

	/* If we woke up but all the lids are closed, go back to sleep */
	return checklids(sc);
}
