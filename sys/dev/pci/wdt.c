/*-
 * Copyright (c) 1998,1999 Alex Nash
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$OpenBSD: wdt.c,v 1.1 1999/04/28 23:21:04 alex Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <machine/bus.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include "wdt50x.h"
#include "wdt.h"

#if NWDT > 0

struct wdt_softc {
	/* wdt_dev must be the first item in the struct */
	struct device		wdt_dev;

	/* feature set: 0 = none   1 = temp, buzzer, etc. */
	int			features;

	/* unit number (unlikely more than one would be present though) */
	int			unit;

	/* how many processes are in WIOCSCHED */
	unsigned		procs;

	/* watchdog timeout */
	unsigned		timeout_secs;

	/* device access through bus space */
	bus_space_tag_t		iot;
	bus_space_handle_t	ioh;
};

/* externally visible functions */
int wdtprobe __P((struct device *, void *, void *));
void wdtattach __P((struct device *, struct device *, void *));
int wdtopen __P((dev_t, int, int, struct proc *));
int wdtclose __P((dev_t, int, int, struct proc *));
int wdtioctl __P((dev_t, u_long, caddr_t, int, struct proc *));

/* static functions */
static int wdt_is501 __P((struct wdt_softc *wdt));
static void wdt_8254_count __P((struct wdt_softc *wdt, int counter, u_int16_t v));
static void wdt_8254_mode __P((struct wdt_softc *wdt, int counter, int mode));
static void wdt_set_timeout __P((struct wdt_softc *wdt, unsigned seconds));
static void wdt_timeout __P((void *arg));
static void wdt_init_timer __P((struct wdt_softc *wdt));
static void wdt_buzzer_off __P((struct wdt_softc *wdt));
static int wdt_read_temperature __P((struct wdt_softc *wdt));
static int wdt_read_status __P((struct wdt_softc *wdt));
static void wdt_display_status __P((struct wdt_softc *wdt));
static int wdt_get_state __P((struct wdt_softc *wdt, struct wdt_state *state));
static void wdt_shutdown __P((void *arg));
static int wdt_sched __P((struct wdt_softc *wdt, struct proc *p));
static void wdt_timer_disable __P((struct wdt_softc *wdt));
static void wdt_timer_enable __P((struct wdt_softc *wdt, unsigned seconds));
#if WDT_DISABLE_BUZZER
static void wdt_buzzer_disable __P((struct wdt_softc *wdt));
#else
static void wdt_buzzer_enable __P((struct wdt_softc *wdt));
#endif

struct cfattach wdt_ca = {
	sizeof(struct wdt_softc), wdtprobe, wdtattach
};

struct cfdriver wdt_cd = {
	NULL, "wdt", DV_DULL
};

/*
 *	8254 counter mappings
 */
#define WDT_8254_TC_LO		0	/* low 16 bits of timeout counter  */
#define	WDT_8254_TC_HI		1	/* high 16 bits of timeout counter */
#define WDT_8254_BUZZER		2

/*
 *	WDT500/501 ports
 */
#define WDT_8254_BASE		0
#define WDT_8254_CTL		(WDT_8254_BASE + 3)
#define WDT_DISABLE_TIMER	7
#define WDT_ENABLE_TIMER	7

/*
 *	WDT501 specific ports
 */
#define WDT_STATUS_REG		4
#define WDT_START_BUZZER	4
#define WDT_TEMPERATURE		5
#define WDT_STOP_BUZZER		5

#define UNIT(dev)		(minor(dev))

int
wdtprobe (parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *const pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INDCOMPSRC ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_INDCOMPSRC_WDT50x)
		return(0);

	return(1);
}

void
wdtattach (parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wdt_softc *wdt = (struct wdt_softc *)self;
	struct pci_attach_args *const pa = (struct pci_attach_args *)aux;
	int unit;
	bus_size_t iosize;
	bus_addr_t iobase;

	wdt->iot = pa->pa_iot;

	unit = wdt->wdt_dev.dv_unit;

	/* retrieve the I/O region (BAR2) */
	if (pci_io_find(pa->pa_pc, pa->pa_tag, 0x18, &iobase,
	    &iosize) != 0) {
		printf("wdt%d: couldn't find PCI I/O region\n", unit);
		return;
	}

	/* sanity check I/O size */
	if (iosize != (bus_size_t)16) {
		printf("wdt%d: invalid I/O region size\n", unit);
		return;
	}

	/* map I/O region */
	if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &wdt->ioh) != 0) {
		printf("wdt%d: couldn't map PCI I/O region\n", unit);
		return;
	}

	/* initialize the watchdog timer structure */
	wdt->unit  = unit;
	wdt->procs = 0;

	/* check the feature set available */
	if (wdt_is501(wdt))
		wdt->features = 1;
	else
		wdt->features = 0;

	/*
	 * register a callback for system shutdown
	 * (we need to disable the watchdog timer during shutdown)
	 */
	if (shutdownhook_establish(wdt_shutdown, wdt) == NULL)
		return;

	if (wdt->features) {
		/*
		 * turn off the buzzer, it may have been activated
		 * by a previous timeout
		 */
		wdt_buzzer_off(wdt);

#ifdef WDT_DISABLE_BUZZER
		wdt_buzzer_disable(wdt);
#else
		wdt_buzzer_enable(wdt);
#endif
	}

	/* initialize the timer modes and the lower 16-bit counter */
	wdt_init_timer(wdt);

	/*
	 * it appears the timeout queue isn't processed until the
	 * kernel has fully booted, so we set the first timeout
	 * far in advance, and subsequent timeouts at the normal
	 * 30 second interval
	 */
	wdt_timer_enable(wdt, 90/*seconds*/);
	wdt->timeout_secs = 30;

	printf("\n");
	wdt_display_status(wdt);
}

int
wdtopen (dev_t dev, int flags, int fmt, struct proc *p)
{
	if (UNIT(dev) >= wdt_cd.cd_ndevs)
		return(ENXIO);

	return(0);
}

int
wdtclose (dev_t dev, int flags, int fmt, struct proc *p)
{
	return(0);
}

int
wdtioctl (dev_t dev, u_long cmd, caddr_t arg, int flag, struct proc *p)
{
	struct wdt_softc *wdt = wdt_cd.cd_devs[UNIT(dev)];
	int error;

	switch (cmd) {
		case WIOCSCHED:
			error = wdt_sched(wdt, p);
			break;

		case WIOCGETSTATE:
			if (wdt->features)
				error = wdt_get_state(wdt,
					(struct wdt_state *)arg);
			else
				error = ENXIO;
			break;

		default:
			error = ENXIO;
			break;
	}

	return(error);
}

/*
 *	wdt_is501
 *
 *	Returns non-zero if the card is a 501 model.
 */
static int
wdt_is501 (struct wdt_softc *wdt)
{
	/*
	 *	It makes too much sense to detect the card type
	 *	by the device ID, so we have to resort to testing
	 *	the presence of a register to determine the type.
	 */
	int v = bus_space_read_1(wdt->iot, wdt->ioh, WDT_TEMPERATURE);

	/* XXX may not be reliable */
	if (v == 0 || v == 0xFF)
		return(0);

	return(1);
}

/*
 *	wdt_8254_count
 *
 *	Loads the specified counter with the 16-bit value 'v'.
 */
static void
wdt_8254_count (struct wdt_softc *wdt, int counter, u_int16_t v)
{
	bus_space_write_1(wdt->iot, wdt->ioh,
			WDT_8254_BASE + counter, v & 0xFF);
	bus_space_write_1(wdt->iot, wdt->ioh, WDT_8254_BASE + counter, v >> 8);
}

/*
 *	wdt_8254_mode
 *
 *	Sets the mode of the specified counter.
 */
static void
wdt_8254_mode (struct wdt_softc *wdt, int counter, int mode)
{
	bus_space_write_1(wdt->iot, wdt->ioh, WDT_8254_CTL,
		(counter << 6) | 0x30 | (mode << 1));
}

/*
 *	wdt_set_timeout
 *
 *	Load the watchdog timer with the specified number of seconds.
 */
static void
wdt_set_timeout (struct wdt_softc *wdt, unsigned seconds)
{
	/* 8254 has been programmed with a 2ms period */
	u_int16_t v = (u_int16_t)seconds * 50;

	/* disable the timer */
	(void)bus_space_read_1(wdt->iot, wdt->ioh, WDT_DISABLE_TIMER);

	/* load the new timeout count */
	wdt_8254_count(wdt, WDT_8254_TC_HI, v);

	/* enable the timer */
	bus_space_write_1(wdt->iot, wdt->ioh, WDT_ENABLE_TIMER, 0);
}

/*
 *	wdt_timeout
 *
 *	Kernel timeout handler.  This function is called every
 *	wdt->timeout_secs / 2 seconds.  It reloads the watchdog
 *	counters in one of two ways:
 *
 *	   - If there are one or more processes sleeping in a
 *	     WIOCSCHED ioctl(), they are woken up to perform
 *	     the counter reload.
 *	   - If no processes are sleeping in WIOCSCHED, the
 *	     counters are reloaded from here.
 *
 *	Finally, another timeout is scheduled for wdt->timeout_secs
 *	from now.
 */
static void
wdt_timeout (void *arg)
{
	struct wdt_softc *wdt = (struct wdt_softc *)arg;

	/* reload counters from proc in WIOCSCHED ioctl()? */
	if (wdt->procs)
		wakeup(wdt);
	else
		wdt_set_timeout(wdt, wdt->timeout_secs);

	/* schedule another timeout in half the countdown time */
	timeout(wdt_timeout, arg, wdt->timeout_secs * hz / 2);
}

/*
 *	wdt_timer_disable
 *
 *	Disables the watchdog timer and cancels the scheduled (if any)
 *	kernel timeout.
 */
static void
wdt_timer_disable (struct wdt_softc *wdt)
{
	(void)bus_space_read_1(wdt->iot, wdt->ioh, WDT_DISABLE_TIMER);
	untimeout(wdt_timeout, wdt);
}

/*
 *	wdt_timer_enable
 *
 *	Enables the watchdog timer to expire in the specified number
 *	of seconds.  If 'seconds' is outside the range 2-1800, it
 *	is silently clamped to be within range.
 */
static void
wdt_timer_enable (struct wdt_softc *wdt, unsigned seconds)
{
	int s;

	/* clamp range */
	if (seconds < 2)
		seconds = 2;

	if (seconds > 1800)
		seconds = 1800;

	/* block out the timeout handler */
	s = splclock();

	wdt_timer_disable(wdt);
	wdt->timeout_secs = seconds;

	timeout(wdt_timeout, wdt, hz * seconds / 2);
	wdt_set_timeout(wdt, seconds);

	/* re-enable clock interrupts */
	splx(s);
}

/*
 *	wdt_init_timer
 *
 *	Configure the modes for the watchdog counters and initialize
 *	the low 16-bits of the watchdog counter to have a period of
 *	approximately 1/50th of a second.
 */
static void
wdt_init_timer (struct wdt_softc *wdt)
{
	wdt_8254_mode(wdt, WDT_8254_TC_LO, 3);
	wdt_8254_mode(wdt, WDT_8254_TC_HI, 2);
	wdt_8254_count(wdt, WDT_8254_TC_LO, 41666);
}

/*******************************************************************
 *	WDT501 specific functions
 *******************************************************************/

/*
 *	wdt_buzzer_off
 *
 *	Turns the buzzer off.
 */
static void
wdt_buzzer_off (struct wdt_softc *wdt)
{
	bus_space_write_1(wdt->iot, wdt->ioh, WDT_STOP_BUZZER, 0);
}

#ifndef WDT_DISABLE_BUZZER
/*
 *	wdt_buzzer_enable
 *
 *	Enables the buzzer when the watchdog counter expires.
 */
static void
wdt_buzzer_enable (struct wdt_softc *wdt)
{
	bus_space_write_1(wdt->iot, wdt->ioh, WDT_8254_BUZZER, 1);
	wdt_8254_mode(wdt, WDT_8254_BUZZER, 1);
}
#else
/*
 *	wdt_buzzer_disable
 *
 *	Disables the buzzer from sounding when the watchdog counter
 *	expires.
 */
static void
wdt_buzzer_disable (struct wdt_softc *wdt)
{
	wdt_8254_mode(wdt, WDT_8254_BUZZER, 0);
}
#endif

/*
 *	wdt_read_temperature
 *
 *	Returns the temperature (in Fahrenheit) from the board.
 */
static int
wdt_read_temperature (struct wdt_softc *wdt)
{
	unsigned v = bus_space_read_1(wdt->iot, wdt->ioh, WDT_TEMPERATURE);

	return((v * 11) / 15 + 7);
}

/*
 *	wdt_read_status
 *
 *	Returns the status register bits minus the counter refresh
 *	and IRQ generated bits.
 */
static int
wdt_read_status (struct wdt_softc *wdt)
{
	/* mask off counter refresh & IRQ generated bits */
	return(bus_space_read_1(wdt->iot, wdt->ioh, WDT_STATUS_REG) & 0x7E);
}

/*
 *	wdt_display_status
 *
 *	Displays the current timeout, temperature, and power supply
 *	over/undervoltages to the console.
 */
static void
wdt_display_status (struct wdt_softc *wdt)
{
	if (wdt->features) {
		int status = wdt_read_status(wdt);
		int temp   = wdt_read_temperature(wdt);

		printf("wdt%d: WDT501 timeout %d secs, temp %d F",
			   wdt->unit, wdt->timeout_secs, temp);

		/* overvoltage bit is active low */
		if ((status & WDT_SR_PS_OVER) == 0)
			printf(" <PS overvoltage>");

		/* undervoltage bit is active low */
		if ((status & WDT_SR_PS_UNDER) == 0)
			printf(" <PS undervoltage>");
	} else {
		printf("wdt%d: WDT500 timeout %d secs",
			   wdt->unit, wdt->timeout_secs);
	}

	printf("\n");
}

/*
 *	wdt_get_state
 *
 *	Returns the temperature and status bits.
 */
static int
wdt_get_state (struct wdt_softc *wdt, struct wdt_state *state)
{
	state->temperature	= wdt_read_temperature(wdt);
	state->status		= wdt_read_status(wdt);

	return(0);
}

/*
 *	wdt_shutdown
 *
 *	Disables the watchdog timer at system shutdown time.
 */
static void
wdt_shutdown (void *arg)
{
	struct wdt_softc *wdt = (struct wdt_softc *)arg;

	wdt_timer_disable(wdt);
}

/*
 *	wdt_sched
 *
 *	Put the process into an infinite loop in which:
 *
 *	  - The process sleeps, waiting for a wakeup() from the timeout()
 *	    handler.
 *	  - When awakened, the process reloads the watchdog counter and
 *	    repeats the loop.
 *
 *	The only way the loop can be broken is if the process is interrupted
 *	via a signal.
 *
 *	The whole point of this is to cause a watchdog timeout to be
 *	generated if processes are no longer being scheduled.
 */
static int
wdt_sched (struct wdt_softc *wdt, struct proc *p)
{
	int error;
	int s;

	/*
	 * Regardless of the device permissions, you must be
	 * root to do this -- a process which is STOPPED
	 * while in this function can cause a reboot to occur
	 * if the counters aren't reloaded within wdt->timeout_secs
	 * seconds.
	 */
	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return(error);

	/* block out the timeout handler */
	s = splclock();

	/* indicate that we are sleeping */
	++wdt->procs;

	/* loop until the process is signaled */
	while (1) {
		error = tsleep(wdt, PCATCH | PSWP, "wdtsch", 0);

		wdt_set_timeout(wdt, wdt->timeout_secs);

		if (error != 0)
			break;
	}

	/* remove sleeping indication */
	--wdt->procs;

	/* re-enable timeout handler */
	splx(s);

	return(error);
}

#endif /* NWDT > 0 */
