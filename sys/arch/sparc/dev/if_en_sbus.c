/*	$OpenBSD: if_en_sbus.c,v 1.6 2004/09/29 07:35:11 miod Exp $	*/
/*	$NetBSD: if_en_sbus.c,v 1.4 1997/05/24 20:16:22 pk Exp $	*/

/*
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and
 *	Washington University.
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

/*
 *
 * i f _ e n _ s b u s . c  
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 * started: spring, 1996.
 *
 * SBUS glue for the eni155s card.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sparc/dev/sbusvar.h>

#include <dev/ic/midwayreg.h>
#include <dev/ic/midwayvar.h>


/*
 * local structures
 */

struct en_sbus_softc {
  /* bus independent stuff */
  struct en_softc esc;		/* includes "device" structure */

  /* sbus glue */
  struct sbusdev sc_sd;		/* sbus device */
  struct intrhand sc_ih;	/* interrupt vectoring */
};

/*
 * local defines (SBUS specific stuff)
 */

#define EN_IPL 5

/*
 * prototypes
 */

static	int en_sbus_match(struct device *, void *, void *);
static	void en_sbus_attach(struct device *, struct device *, void *);

/*
 * SBUS autoconfig attachments
 */

struct cfattach en_sbus_ca = {
    sizeof(struct en_sbus_softc), en_sbus_match, en_sbus_attach,
};

/***********************************************************************/

/*
 * autoconfig stuff
 */

static int en_sbus_match(parent, match, aux)

struct device *parent;
void *match;
void *aux;

{
  struct cfdata *cf = match;
  struct confargs *ca = aux;
  register struct romaux *ra = &ca->ca_ra;

  if (strcmp("ENI-155s", ra->ra_name))
    return 0;
  if (ca->ca_bustype == BUS_SBUS)
    return (1);
  
  return 0;
}


static void en_sbus_attach(parent, self, aux)

struct device *parent, *self;
void *aux;

{
  struct en_softc *sc = (void *)self;
  struct en_sbus_softc *scs = (void *)self;
  struct confargs *ca = aux;
  int lcv, iplcode;

  printf("\n");

  if (CPU_ISSUN4M) {
    printf("%s: sun4m DMA not supported yet\n", sc->sc_dev.dv_xname);
    return;
  }

  sc->en_base = (caddr_t) mapiodev(ca->ca_ra.ra_reg, 0, 4*1024*1024);

  if (ca->ca_ra.ra_nintr == 1) {
    sc->ipl = ca->ca_ra.ra_intr[0].int_pri;
  } else {
    printf("%s: claims to be at the following IPLs: ", sc->sc_dev.dv_xname);
    iplcode = 0;
    for (lcv = 0 ; lcv < ca->ca_ra.ra_nintr ; lcv++) {
      printf("%d ", ca->ca_ra.ra_intr[lcv].int_pri);
      if (EN_IPL == ca->ca_ra.ra_intr[lcv].int_pri)
        iplcode = lcv;
    }
    if (!iplcode) {
      printf("%s: can't find the IPL we want (%d)\n", sc->sc_dev.dv_xname,
		EN_IPL);
      return;
    }
    printf("\n%s: we choose IPL %d\n", sc->sc_dev.dv_xname, EN_IPL);
    sc->ipl = iplcode;
  }
  scs->sc_ih.ih_fun = en_intr;
  scs->sc_ih.ih_arg = sc;
  intr_establish(EN_IPL, &scs->sc_ih, IPL_NET, self->dv_xname);

  /*
   * done SBUS specific stuff
   */

  en_attach(sc);

}
