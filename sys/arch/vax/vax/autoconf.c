/*	$OpenBSD: autoconf.c,v 1.15 2001/11/06 19:53:17 miod Exp $	*/
/*	$NetBSD: autoconf.c,v 1.45 1999/10/23 14:56:05 ragge Exp $	*/

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/nexus.h>
#include <machine/ioa.h>
#include <machine/ka820.h>
#include <machine/ka750.h>
#include <machine/ka650.h>
#include <machine/clock.h>
#include <machine/rpb.h>

#include "sd.h"
#include "cd.h"

#include <vax/vax/gencons.h>

#include <vax/bi/bireg.h>

void	cpu_dumpconf __P((void));	/* machdep.c */
void	gencnslask __P((void));
void	setroot __P((void));		/* rootfil.c */

struct cpu_dep *dep_call;
int	mastercpu;	/* chief of the system */
struct device *booted_from;

#define MAINBUS	0

void
cpu_configure()
{
	if (config_rootfound("mainbus", NULL) == NULL)
		panic("mainbus not configured");

	setroot();
	/*	
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	swapconf();
	cpu_dumpconf();

	/*
	 * We're ready to start up. Clear CPU cold start flag.
	 */

	cold = 0;

	if (mountroot == NULL) {
		if (B_TYPE(bootdev) >= BDEV_NET) 
			mountroot = nfs_mountroot;
		else
			mountroot = dk_mountroot;
	}

	if (dep_call->cpu_clrf) 
		(*dep_call->cpu_clrf)();
}

int	mainbus_print __P((void *, const char *));
int	mainbus_match __P((struct device *, struct cfdata *, void *));
void	mainbus_attach __P((struct device *, struct device *, void *));

int
mainbus_print(aux, hej)
	void *aux;
	const char *hej;
{
	if (hej)
		printf("nothing at %s", hej);
	return (UNCONF);
}

int
mainbus_match(parent, cf, aux)
	struct	device	*parent;
	struct cfdata *cf;
	void	*aux;
{
	if (cf->cf_unit == 0 &&
	    strcmp(cf->cf_driver->cd_name, "mainbus") == 0)
		return 1; /* First (and only) mainbus */

	return (0);
}

void
mainbus_attach(parent, self, hej)
	struct	device	*parent, *self;
	void	*hej;
{
	printf("\n");

	/*
	 * Hopefully there a master bus?
	 * Maybe should have this as master instead of mainbus.
	 */
	config_found(self, NULL, mainbus_print);

#if VAX53
	/* Kludge: To have two master buses */
	if (vax_boardtype == VAX_BTYP_1303)
		config_found(self, (void *)1, mainbus_print);
#endif

	if (dep_call->cpu_subconf)
		(*dep_call->cpu_subconf)(self);
}

struct	cfattach mainbus_ca = {
	sizeof(struct device), (cfmatch_t) mainbus_match, mainbus_attach
};

struct  cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};


