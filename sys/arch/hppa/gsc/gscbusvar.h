/*	$OpenBSD: gscbusvar.h,v 1.1 1998/11/04 17:05:15 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct gscbus_ic {
	enum {gsc_unknown = 0, gsc_lasi, gsc_wax, gsc_asp} gsc_type;
	void *gsc_dv;

	void *(*gsc_intr_establish) __P((struct gscbus_ic *ic, int pri,
					 int (*handler) __P((void *)), void *,
					 const char *));
	void (*gsc_intr_disestablish) __P((void *v));
	int (*gsc_intack) __P((void *v));
};

struct gsc_attach_args {
	struct confargs ga_ca;
#define	ga_name	ga_ca.ca_name
#define	ga_iot	ga_ca.ca_iot
#define	ga_mod	ga_ca.ca_mod
#define	ga_type	ga_ca.ca_type
#define	ga_hpa	ga_ca.ca_hpa
/*#define	ga_pdc_iodc_read	ga_ca.ca_pdc_iodc_read */
	struct gscbus_ic *ga_ic;	/* IC pointer */
}; 

struct gsc_softc {
	struct  device sc_dev;

	bus_space_tag_t sc_iot;
	struct gscbus_ic *sc_ic;
};

#define	gsc_intr_establish(ic,pri,h,c,s) \
	((ic)->gsc_intr_establish((ic), (pri), (h), (c), (s)))
#define	gsc_intr_disestablish(ic,v) \
	(ic)->gsc_intr_disestablish((ic), (v))
#define	gsc_intr_intack(ic,v) \
	((ic)->gsc_intr_disestablish((ic), (v)))
