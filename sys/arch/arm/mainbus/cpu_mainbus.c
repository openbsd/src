/*	$OpenBSD: cpu_mainbus.c,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*	$NetBSD: cpu_mainbus.c,v 1.3 2002/01/05 22:41:48 chris Exp $	*/

/*
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpu.c
 *
 * Probing and configuration for the master cpu
 *
 * Created      : 10/10/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#if 0
#include <uvm/uvm_extern.h>
#include <machine/io.h>
#include <machine/conf.h>
#endif
#include <machine/cpu.h>
#if 0
#include <arm/cpus.h>
#include <arm/undefined.h>
#endif
#include <arm/mainbus/mainbus.h>

/*
 * Prototypes
 */
static int cpu_mainbus_match (struct device *, void *, void *);
static void cpu_mainbus_attach (struct device *, struct device *, void *);
 
/*
 * int cpumatch(struct device *parent, struct cfdata *cf, void *aux)
 */ 
 
static int
cpu_mainbus_match(struct device *parent, void *vcf, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	struct cfdata *cf = (struct cfdata *)vcf;

	return (strcmp(cf->cf_driver->cd_name, ma->ma_name) == 0);
}

/*
 * void cpusattach(struct device *parent, struct device *dev, void *aux)
 *
 * Attach the main cpu
 */
  
static void
cpu_mainbus_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	cpu_attach(self);
}

struct cfattach cpu_mainbus_ca = {
	sizeof(struct device), cpu_mainbus_match, cpu_mainbus_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};
