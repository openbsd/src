/*	$OpenBSD: gscbusvar.h,v 1.14 2010/11/28 20:09:40 miod Exp $	*/

/*
 * Copyright (c) 1998-2004 Michael Shalayeff
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

struct gscbus_ic {
	enum {gsc_unknown = 0, gsc_lasi, gsc_wax, gsc_asp} gsc_type;
	void *gsc_dv;
	volatile void *gsc_base;
};

struct gsc_attach_args {
	struct confargs ga_ca;
#define	ga_name		ga_ca.ca_name
#define	ga_iot		ga_ca.ca_iot
#define	ga_dp		ga_ca.ca_dp
#define	ga_type		ga_ca.ca_type
#define	ga_hpa		ga_ca.ca_hpa
#define	ga_hpamask	ga_ca.ca_hpamask
#define	ga_dmatag	ga_ca.ca_dmatag
#define	ga_irq		ga_ca.ca_irq
#define	ga_pdc_iodc_read	ga_ca.ca_pdc_iodc_read
	struct gscbus_ic *ga_ic;	/* IC pointer */
}; 

struct gsc_softc {
	struct  device sc_dev;
	void *sc_ih;

	bus_space_tag_t sc_iot;
	struct gscbus_ic *sc_ic;
};

void *gsc_intr_establish(struct gsc_softc *sc, int irq, int pri,
    int (*handler)(void *v), void *arg, const char *name);
void gsc_intr_disestablish(struct gsc_softc *sc, void *v);
int gsc_intr(void *);

int gscprint(void *, const char *);
