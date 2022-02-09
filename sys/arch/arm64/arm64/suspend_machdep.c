/*	$OpenBSD: suspend_machdep.c,v 1.1 2022/02/09 23:54:55 deraadt Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/acpi/acpivar.h>
#include <dev/acpi/dsdt.h>

#include <machine/apmvar.h>

#include <arm64/dev/acpiiort.h>

void
sleep_clocks(void *v)
{
}

int
sleep_cpu(void *v, int state)
{
	return 0;
}

void
resume_cpu(void *sc, int state)
{
}

#ifdef MULTIPROCESSOR

void
sleep_mp(void)
{
}

void
resume_mp(void)
{
}

int
sleep_showstate(void *v, int sleepmode)
{
	return 0;
}

int
sleep_setstate(void *v)
{
	return 0;
}

void
gosleep(void *v)
{
	// XXX
}

int
sleep_resume(void *v)
{
	return 0;
}

void
display_suspend(void *v)
{
#if 0
#if NWSDISPLAY > 0
	struct acpi_softc *sc = v;

	/*
	 * Temporarily release the lock to prevent the X server from
	 * blocking on setting the display brightness.
	 */
	rw_exit_write(&sc->sc_lck);		/* XXX replace this interlock */
	wsdisplay_suspend();
	rw_enter_write(&sc->sc_lck);
#endif /* NWSDISPLAY > 0 */
#endif
}

void
display_resume(void *v)
{
#if 0
#if NWSDISPLAY > 0
	struct acpi_softc *sc = v;

	rw_exit_write(&sc->sc_lck);		/* XXX replace this interlock */
	wsdisplay_resume();
	rw_enter_write(&sc->sc_lck);
#endif /* NWSDISPLAY > 0 */
#endif
}

void
suspend_finish(void *v)
{
#if 0
	extern int lid_action;

	acpi_record_event(sc, APM_NORMAL_RESUME);
	acpi_indicator(sc, ACPI_SST_WORKING);

	/* XXX won't work, there is no acpi thread on arm64 */

	/* If we woke up but all the lids are closed, go back to sleep */
	if (acpibtn_numopenlids() == 0 && lid_action != 0)
		acpi_addtask(sc, acpi_sleep_task, sc, sc->sc_state);
#endif
}

void
disable_lid_wakeups(void *v)
{
}

#endif /* !SMALL_KERNEL */
