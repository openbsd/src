/*	$OpenBSD: kbus.c,v 1.2 1999/01/11 05:11:27 millert Exp $	*/
/*	$NetBSD: kbus.c,v 1.23 1996/11/20 18:56:56 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/kbus.h>
#include <machine/ioasic.h>

static int  kbus_match __P((struct device *, void *, void *));
static void kbus_attach __P((struct device *, struct device *, void *));
static int ioasic_intr __P((void *arg));

#if 0
static int  kbus_print __P((void *, const char *parentname));
static int  kbus_submatch __P((struct device *, void *, void *));
#endif

struct cfattach kbus_ca = {
	sizeof(struct device), kbus_match, kbus_attach
};

struct cfdriver kbus_cd = {
	NULL, "kbus", DV_DULL
};

/* A mapped page for IOASIC.  */
struct ioasic_reg *ioasic;

static struct intrhand ioasic_intrhand = {ioasic_intr};

static int
kbus_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;

	if (ca->ca_bustype != BUS_KBUS)
		return (0);
	return(1);
}

static void
kbus_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	if (!ioasic)
	  {
	    ioasic = (struct ioasic_reg *)
	      bus_mapin (BUS_KBUS, IOASIC_ADDR, IOASIC_SIZE);
	    if (!ioasic)
	      panic ("Cannot map IOASIC");
	    intr_establish (INTR_IOASIC, 0, &ioasic_intrhand);
	  }

	printf("\n");

	config_search (bus_scan, self, aux);
}

static int
ioasic_intr (arg)
     void *arg;
{
  unsigned int val = ioasic->ioasic_ir;
  printf ("int 131, val = 0x%x\n", val);
  return 0;
}

