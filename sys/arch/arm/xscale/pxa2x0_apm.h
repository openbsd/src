/*	$OpenBSD: pxa2x0_apm.h,v 1.8 2009/03/27 16:01:37 oga Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@bsdx.de>
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

#ifndef _PXA2X0_APM_H_
#define _PXA2X0_APM_H_

#include <sys/event.h>

#include <machine/bus.h>
#include <machine/apmvar.h>

struct pxa2x0_apm_softc {
	struct	device sc_dev;
	struct	proc *sc_thread;
	struct	rwlock sc_lock;
	struct	klist sc_note;
	int	sc_flags;
	int	sc_wakeon;	/* enabled wakeup sources */
	int	sc_batt_life;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_pm_ioh;
	bus_space_handle_t sc_memctl_ioh;
	int	(*sc_get_event)(struct pxa2x0_apm_softc *, u_int *);
	void	(*sc_power_info)(struct pxa2x0_apm_softc *,
	    struct apm_power_info *);
	void	(*sc_suspend)(struct pxa2x0_apm_softc *);
	int	(*sc_resume)(struct pxa2x0_apm_softc *);
};

void	pxa2x0_apm_attach_sub(struct pxa2x0_apm_softc *);
void	pxa2x0_apm_sleep(struct pxa2x0_apm_softc *);

#define PXA2X0_WAKEUP_POWERON	(1<<0)
#define PXA2X0_WAKEUP_GPIORST	(1<<1)
#define PXA2X0_WAKEUP_SD	(1<<2)
#define PXA2X0_WAKEUP_RC	(1<<3)
#define PXA2X0_WAKEUP_SYNC	(1<<4)
#define PXA2X0_WAKEUP_KEYNS0	(1<<5)
#define PXA2X0_WAKEUP_KEYNS1	(1<<6)
#define PXA2X0_WAKEUP_KEYNS2	(1<<7)
#define PXA2X0_WAKEUP_KEYNS3	(1<<8)
#define PXA2X0_WAKEUP_KEYNS4	(1<<9)
#define PXA2X0_WAKEUP_KEYNS5	(1<<10)
#define PXA2X0_WAKEUP_KEYNS6	(1<<11)
#define PXA2X0_WAKEUP_CF0	(1<<12)
#define PXA2X0_WAKEUP_CF1	(1<<13)
#define PXA2X0_WAKEUP_USBD	(1<<14)
#define PXA2X0_WAKEUP_LOCKSW	(1<<15)
#define PXA2X0_WAKEUP_JACKIN	(1<<16)
#define PXA2X0_WAKEUP_CHRGFULL	(1<<17)
#define PXA2X0_WAKEUP_RTC	(1<<18)

#define PXA2X0_WAKEUP_KEYNS_ALL	(PXA2X0_WAKEUP_KEYNS0|			\
    PXA2X0_WAKEUP_KEYNS1|PXA2X0_WAKEUP_KEYNS2|PXA2X0_WAKEUP_KEYNS3|	\
    PXA2X0_WAKEUP_KEYNS4|PXA2X0_WAKEUP_KEYNS5|PXA2X0_WAKEUP_KEYNS6)

#define PXA2X0_WAKEUP_CF_ALL	(PXA2X0_WAKEUP_CF0|PXA2X0_WAKEUP_CF1)

#define PXA2X0_WAKEUP_ALL	(PXA2X0_WAKEUP_POWERON|			\
    PXA2X0_WAKEUP_GPIORST|PXA2X0_WAKEUP_SD|PXA2X0_WAKEUP_RC|		\
    PXA2X0_WAKEUP_SYNC|PXA2X0_WAKEUP_KEYNS_ALL|PXA2X0_WAKEUP_CF_ALL|	\
    PXA2X0_WAKEUP_USBD|PXA2X0_WAKEUP_LOCKSW|PXA2X0_WAKEUP_JACKIN|	\
    PXA2X0_WAKEUP_CHRGFULL|PXA2X0_WAKEUP_RTC)

void	pxa2x0_wakeup_config(u_int, int);
u_int	pxa2x0_wakeup_status(void);

#endif
