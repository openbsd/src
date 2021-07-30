/*	$OpenBSD: dwc2var.h,v 1.22 2021/07/30 18:05:24 mglocker Exp $	*/
/*	$NetBSD: dwc2var.h,v 1.3 2013/10/22 12:57:40 skrll Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nick Hudson
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_DWC2VAR_H_
#define	_DWC2VAR_H_

#include <sys/pool.h>
#include <sys/task.h>

struct dwc2_hsotg;
struct dwc2_qtd;

struct dwc2_xfer {
	struct usbd_xfer xfer;			/* Needs to be first */

	struct dwc2_hcd_urb *urb;

	TAILQ_ENTRY(dwc2_xfer) xnext;		/* list of complete xfers */
	usbd_status intr_status;
};

struct dwc2_pipe {
	struct usbd_pipe pipe;		/* Must be first */

	/* Current transfer */
	void *priv;			/* QH */

	 /* DMA buffer for control endpoint requests */
	struct usb_dma req_dma;
};


#define	DWC2_BUS2SC(bus)	((void *)(bus))
#define	DWC2_PIPE2SC(pipe)	DWC2_BUS2SC((pipe)->device->bus)
#define	DWC2_XFER2SC(xfer)	DWC2_PIPE2SC((xfer)->pipe)
#define	DWC2_DPIPE2SC(d)	DWC2_BUS2SC((d)->pipe.device->bus)

#define	DWC2_XFER2DXFER(x)	(struct dwc2_xfer *)(x)

#define	DWC2_XFER2DPIPE(x)	(struct dwc2_pipe *)(x)->pipe;
#define	DWC2_PIPE2DPIPE(p)	(struct dwc2_pipe *)(p)


typedef struct dwc2_softc {
	struct usbd_bus		sc_bus;

 	bus_space_tag_t		sc_iot;
 	bus_space_handle_t	sc_ioh;
	struct dwc2_core_params *sc_params;
	int			(*sc_set_dma_addr)(struct device *, bus_addr_t, int);

	/*
	 * Private
	 */

	struct dwc2_hsotg *sc_hsotg;

	struct mutex sc_lock;

	bool sc_hcdenabled;
	void *sc_rhc_si;

	struct usbd_xfer *sc_intrxfer;

	struct device *sc_child;	/* /dev/usb# device */

	char sc_vendor[32];		/* vendor string for root hub */

	TAILQ_HEAD(, dwc2_xfer) sc_complete;	/* complete transfers */

	struct pool sc_xferpool;
	struct pool sc_qhpool;
	struct pool sc_qtdpool;

	uint8_t sc_addr;		/* device address */
	uint8_t sc_conf;		/* device configuration */

} dwc2_softc_t;

int		dwc2_init(struct dwc2_softc *);
int		dwc2_intr(void *);
int		dwc2_detach(dwc2_softc_t *, int);

void		dwc2_worker(struct task *, void *);

void		dwc2_host_complete(struct dwc2_hsotg *, struct dwc2_qtd *,
				   int);

static inline void
dwc2_root_intr(dwc2_softc_t *sc)
{

	softintr_schedule(sc->sc_rhc_si);
}

/*
 * XXX Compat
 */
#define DWC2_MAXISOCPACKETS	40	/* XXX: Fix nframes handling */
#define ENOSR			90
#define device_xname(d)		((d)->dv_xname)
#define jiffies			hardclock_ticks
#define mstohz(ms) \
	(__predict_false((ms) >= 0x20000) ? \
	    ((ms +0u) / 1000u) * hz : \
	    ((ms +0u) * hz) / 1000u)
#define msecs_to_jiffies	mstohz
#define IS_ENABLED(option)	(option)

#endif	/* _DWC_OTGVAR_H_ */
