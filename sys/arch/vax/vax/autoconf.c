/*	$OpenBSD: autoconf.c,v 1.38 2014/10/31 10:54:39 jsg Exp $	*/
/*	$NetBSD: autoconf.c,v 1.45 1999/10/23 14:56:05 ragge Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
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
#include <sys/disklabel.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/vmparam.h>
#include <machine/nexus.h>
#include <machine/clock.h>
#include <machine/rpb.h>
#ifdef VAX60
#include <vax/mbus/mbusreg.h>
#endif

#include <dev/cons.h>

#include "led.h"

#include <vax/vax/gencons.h>

void	dumpconf(void);	/* machdep.c */

struct cpu_dep *dep_call;

int	mastercpu;	/* chief of the system */

struct device *bootdv;
int booted_partition;	/* defaults to 0 (aka 'a' partition) */

void
cpu_configure(void)
{
	softintr_init();

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("mainbus not configured");

	/*
	 * We're ready to start up. Clear CPU cold start flag.
	 */
	cold = 0;

	if (dep_call->cpu_clrf) 
		(*dep_call->cpu_clrf)();
}

void
diskconf(void)
{
	if (bootdv == NULL)
		printf("boot device: unknown (rpb %d/%d)\n",
		    rpb.devtyp, rpb.unit);
	else
		printf("boot device: %s\n", bootdv->dv_xname);

	setroot(bootdv, booted_partition, RB_USERREQ);
	dumpconf();
}

int	mainbus_print(void *, const char *);
int	mainbus_match(struct device *, struct cfdata *, void *);
void	mainbus_attach(struct device *, struct device *, void *);

int
mainbus_print(void *aux, const char *hej)
{
	struct mainbus_attach_args *maa = aux;

	if (maa->maa_bustype == VAX_LEDS)
		return (QUIET);

	if (hej) {
		printf("nothing at %s", hej);
	}
	return (UNCONF);
}

int
mainbus_match(struct device *parent, struct cfdata *cf, void *aux)
{
	if (cf->cf_unit == 0 &&
	    strcmp(cf->cf_driver->cd_name, "mainbus") == 0)
		return 1; /* First (and only) mainbus */

	return (0);
}

void
mainbus_attach(struct device *parent, struct device *self, void *hej)
{
	struct mainbus_attach_args maa;

	printf("\n");

	maa.maa_bustype = vax_bustype;
	config_found(self, &maa, mainbus_print);

#if VAX53
	/* These models have both vsbus and ibus */
	if (vax_boardtype == VAX_BTYP_1303) {
		maa.maa_bustype = VAX_VSBUS;
		config_found(self, &maa, mainbus_print);
	}
#endif

#if NLED > 0
	maa.maa_bustype = VAX_LEDS;
	config_found(self, &maa, mainbus_print);
#endif

#if 1 /* boot blocks too old */
        if (rpb.rpb_base == (void *)-1)
                printf("\nWARNING: you must update your boot blocks.\n\n");
#endif

}

struct	cfattach mainbus_ca = {
	sizeof(struct device), (cfmatch_t) mainbus_match, mainbus_attach
};

struct  cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

#include "sd.h"
#include "cd.h"
#include "ra.h"

static int ubtest(void *);
static int jmfr(char *, struct device *, int);
static int booted_qe(struct device *, void *);
static int booted_le(struct device *, void *);
static int booted_ze(struct device *, void *);
static int booted_de(struct device *, void *);
#if NSD > 0 || NCD > 0
static int booted_sd(struct device *, void *);
#endif
#if NRA
static int booted_ra(struct device *, void *);
#endif
#if NRD
static int booted_rd(struct device *, void *);
#endif

int (*devreg[])(struct device *, void *) = {
	booted_qe,
	booted_le,
	booted_ze,
	booted_de,
#if NSD > 0 || NCD > 0
	booted_sd,
#endif
#if NRA
	booted_ra,
#endif
#if NRD
	booted_hd,
#endif
	0,
};

#define	ubreg(x) ((x) & 017777)

void
device_register(struct device *dev, void *aux)
{
	int (**dp)(struct device *, void *) = devreg;

	/* If there's a synthetic RPB, we can't trust it */
	if (rpb.rpb_base == (void *)-1)
		return;

	while (*dp) {
		if ((*dp)(dev, aux)) {
			if (bootdv == NULL)
				bootdv = dev;
			break;
		}
		dp++;
	}
}

/*
 * Simple checks. Return 1 on fail.
 */
int
jmfr(char *n, struct device *dev, int nr)
{
	if (rpb.devtyp != nr)
		return 1;
	return strcmp(n, dev->dv_cfdata->cf_driver->cd_name);
}

#include <arch/vax/qbus/ubavar.h>
int
ubtest(void *aux)
{
	paddr_t p;

	p = kvtophys(((struct uba_attach_args *)aux)->ua_ioh);
	if (rpb.csrphy != p)
		return 1;
	return 0;
}

#if 1 /* NDE */
int
booted_de(struct device *dev, void *aux)
{

	if (jmfr("de", dev, BDEV_DE) || ubtest(aux))
		return 0;

	return 1;
}
#endif /* NDE */

int
booted_le(struct device *dev, void *aux)
{
	if (jmfr("le", dev, BDEV_LE))
		return 0;
	return 1;
}

int
booted_ze(struct device *dev, void *aux)
{
	if (jmfr("ze", dev, BDEV_ZE))
		return 0;
	return 1;
}

#if 1 /* NQE */
int
booted_qe(struct device *dev, void *aux)
{
	if (jmfr("qe", dev, BDEV_QE) || ubtest(aux))
		return 0;

	return 1;
}
#endif /* NQE */

#if NSD > 0 || NCD > 0
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
int
booted_sd(struct device *dev, void *aux)
{
	struct scsi_attach_args *sa = aux;
	struct device *ppdev;

	/* Is this a SCSI device? */
	if (jmfr("sd", dev, BDEV_SD) && jmfr("cd", dev, BDEV_SD) &&
	    jmfr("sd", dev, BDEV_SDN) && jmfr("cd", dev, BDEV_SDN) &&
	    jmfr("sd", dev, BDEV_SDS) && jmfr("cd", dev, BDEV_SDS))
		return 0;

	if (sa->sa_sc_link->target != rpb.unit)
		return 0; /* Wrong unit */

	ppdev = dev->dv_parent->dv_parent;

	/* VS3100 NCR 53C80 (si) & VS4000 NCR 53C94 (asc) */
	if (((jmfr("ncr", ppdev, BDEV_SD) == 0) ||	/* old name */
	    (jmfr("asc", ppdev, BDEV_SD) == 0) ||
	    (jmfr("asc", ppdev, BDEV_SDN) == 0)) &&
	    (ppdev->dv_cfdata->cf_loc[0] == rpb.csrphy))
			return 1;

#ifdef VAX60
	/* VS35x0 (sii) */
	if (jmfr("sii", ppdev, BDEV_SDS) == 0 && rpb.csrphy ==
	    MBUS_SLOT_BASE(ppdev->dv_parent->dv_cfdata->cf_loc[0]))
		return 1;
#endif

	return 0; /* Where did we come from??? */
}
#endif

#if NRA
#include <arch/vax/mscp/mscp.h>
#include <arch/vax/mscp/mscpreg.h>
#include <arch/vax/mscp/mscpvar.h>
int
booted_ra(struct device *dev, void *aux)
{
	struct drive_attach_args *da = aux;
	struct mscp_softc *pdev = (void *)dev->dv_parent;
	paddr_t ioaddr;

	if (jmfr("ra", dev, BDEV_UDA))
		return 0;

	if (da->da_mp->mscp_unit != rpb.unit)
		return 0; /* Wrong unit number */

	ioaddr = kvtophys(pdev->mi_iph); /* Get phys addr of CSR */
	if (rpb.devtyp == BDEV_UDA && rpb.csrphy == ioaddr)
		return 1; /* Did match CSR */

	return 0;
}
#endif
#if NHD
int     
booted_hd(struct device *dev, void *aux)
{
	int *nr = aux; /* XXX - use the correct attach struct */

	if (jmfr("hd", dev, BDEV_RD))
		return 0;

	if (*nr != rpb.unit)
		return 0;

	return 1;
}
#endif

struct nam2blk nam2blk[] = {
	{ "ra",          9 },
	{ "rx",         12 },
	{ "hd",		19 },
	{ "sd",         20 },
	{ "cd",         22 },
	{ "rd",         23 },
	{ "raid",       25 },
	{ "vnd",	18 },
	{ NULL,		-1 }
};
