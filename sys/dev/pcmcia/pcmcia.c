/*	$Id: pcmcia.c,v 1.4 1996/05/03 07:59:40 deraadt Exp $	*/
/*
 * Copyright (c) 1996 John T. Kohl.  All rights reserved.
 * Copyright (c) 1994 Stefan Grefen.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 *	This product includes software developed by Stefan Grefen.
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
 *
 */

/* XXX - these next two lines are just "glue" until the confusion over
   pcmcia vs pcmciabus between the framework and sys/conf/files
   gets resolved */
#define pcmciabus_cd pcmcia_cd
#define pcmciabus_ca pcmcia_ca

/* derived from scsiconf.c writte by Julian Elischer et al */
/* TODO add modload support and loadable lists of devices */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/cpu.h>

#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmcia_ioctl.h>

#ifdef IBM_WD
#define PCMCIA_DEBUG
#endif
#ifdef PCMCIA_DEBUG
# define PPRINTF(a)	printf a
#else
# define PPRINTF(a)
#endif

#ifdef PCMCIA_DEBUG
void pcmciadumpcf __P((struct pcmcia_conf *));
#endif

static struct old_devs {
	struct device *dev;
	struct pcmciadevs *pcdev;
} *deldevs;
static int      ndeldevs = 0;

#define PCMCIA_SERVICE(a,b,c,d,e)   ((a)->chip_link->pcmcia_service(b,c,\
								(void *) d,e))
#define PCMCIA_MAP_IO(a,b,c,d,e)    ((a)->chip_link->pcmcia_map_io(b,c,d,e))
#define PCMCIA_MAP_INTR(a,b,c,d)    ((a)->chip_link->pcmcia_map_intr(b,c,d))
/* XXX
 * this is quite broken in the face of various bus mapping stuff...
 * drivers need to cooperate with the pcmcia framework to deal with
 * bus mapped memory.  Whee.
 */
#define PCMCIA_MAP_MEM(a,b,c,d,e,f,g) ((a)->chip_link->pcmcia_map_mem(b,c,d,e,f,g))

#define SCRATCH_MEM(a)	((a)->scratch_memh)
#define SCRATCH_BC(a)	((a)->pa_bc)
#define SCRATCH_SIZE(a)	((a)->scratch_memsiz)
#define SCRATCH_INUSE(a)((a)->scratch_inuse)

/*
 * Declarations
 */
int pcmcia_probedev __P((struct pcmcia_link *, struct pcmcia_cardinfo *));
int pcmcia_probe_bus __P((int, int));
int pcmciabusmatch __P((struct device *, void *, void *));
void pcmciabusattach __P((struct device *, struct device *, void *));
int  pcmcia_mapcard __P((struct pcmcia_link *, int, struct pcmcia_conf *));

int pcmcia_unconfigure __P((struct pcmcia_link *));
int pcmcia_unmapcard __P((struct pcmcia_link *));

int pcmcia_print __P((void *, char *));
int pcmcia_submatch __P((struct device *, void *, void *));
void pcmcia_probe_link __P((struct pcmcia_link *));

struct cfattach pcmcia_ca = {
	sizeof(struct pcmciabus_softc), pcmciabusmatch, pcmciabusattach,
};

struct cfdriver pcmcia_cd = {
	NULL, "pcmcia", DV_DULL, 1
};

#if 0
int
pcmcia_register(adapter_softc, bus_link, chip_link, slot)
	void           *adapter_softc;
	struct pcmciabus_link *bus_link;
	struct pcmcia_funcs *chip_link;
	int             slot;
{
	PPRINTF(("- pcmcia_register\n"));
	if (pcmcia_cntrl == 0)
		bzero(pcmcia_drivers, sizeof(pcmcia_drivers));

	if (pcmcia_cntrl < 4) {
		pcmcia_drivers[slot].adapter_softc = adapter_softc;
		pcmcia_drivers[slot].chip_link = chip_link;
		pcmcia_drivers[slot].bus_link = bus_link;
		pcmcia_cntrl++;
		return 1;
	}
	return 0;
}
#endif

int
pcmciabusmatch(parent, self, aux)
	struct device	*parent;
	void 		*self;
	void		*aux;
{
	struct pcmciabus_softc *sc = (void *)self;
	struct cfdata *cf = sc->sc_dev.dv_cfdata;
	struct pcmciabus_attach_args *pba = aux;
	struct pcmcia_adapter *pca = pba->pba_aux;
	int found = 0;

	PPRINTF(("- pcmciabusmatch %p %p\n", pba, pca));

	if (pca->bus_link) {
		if (PCMCIA_BUS_INIT(pca, parent, cf, aux, pca, 0))
			found++;
	}
	return found != 0;
}

/*
 * The routine called by the adapter boards to get all their
 * devices configured in.
 */
void
pcmciabusattach(parent, self, aux)
	struct device	*parent;
	struct device 	*self;
	void		*aux;
{
	struct pcmciabus_softc *sc = (struct pcmciabus_softc *) self;
	struct cfdata  *cf = self->dv_cfdata;
	struct pcmciabus_attach_args *pba = aux;
	struct pcmcia_adapter *pca = pba->pba_aux;

	PPRINTF(("- pcmciabusattach\n"));
	if (pca->bus_link) {
		PCMCIA_BUS_INIT(pca, parent, cf, aux, pca, 1);
	}
	printf("\n");

	sc->sc_driver = pca;
	sc->sc_bc = pba->pba_bc;
	pcmcia_probe_bus(sc->sc_dev.dv_unit, -1);
}

/*
 * Probe the requested pcmcia bus. It must be already set up.
 * -1 requests all set up pcmcia busses.
 */
int
pcmcia_probe_busses(bus, slot)
	int             bus, slot;
{
	PPRINTF(("- pcmcia_probe_busses\n"));
	if (bus == -1) {
		for (bus = 0; bus < pcmciabus_cd.cd_ndevs; bus++)
			if (pcmciabus_cd.cd_devs[bus])
				pcmcia_probe_bus(bus, slot);
		return 0;
	} else {
		return pcmcia_probe_bus(bus, slot);
	}
}

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

/*
 * Probe the requested pcmcia bus. It must be already set up.
 */
int
pcmcia_probe_bus(bus, slot)
	int             bus, slot;
{
	struct pcmciabus_softc *pcmcia;
	int             maxslot, minslot;
	struct pcmcia_link *link;

	PPRINTF(("- pcmcia_probe_bus\n"));
	if (bus < 0 || bus >= pcmciabus_cd.cd_ndevs)
		return ENXIO;
	pcmcia = pcmciabus_cd.cd_devs[bus];
	if (!pcmcia || pcmcia->sc_driver == NULL) /* bus is not configured */
		return ENXIO;

	if (slot == -1) {
		maxslot = pcmcia->sc_driver->nslots - 1;
		minslot = 0;
	} else {
		if (slot < 0 || slot >= pcmcia->sc_driver->nslots)
			return EINVAL;
		maxslot = minslot = slot;
	}

	for (slot = minslot; slot <= maxslot; slot++) {
		if ((link = pcmcia->sc_link[slot])) {
			if (link->devp)
				continue;
		}

		/*
	         * If we presently don't have a link block
	         * then allocate one
	         */
		if (!link) {
			pcmcia->sc_link[slot] = link =
				malloc(sizeof(*link), M_TEMP, M_NOWAIT);
			if (link == NULL)
				return ENOMEM;
			bzero(link, sizeof(*link));
			link->opennings = 1;
			link->adapter = pcmcia->sc_driver;
			link->bus = pcmcia;
			link->slot = slot;
		}
		(void) pcmcia_probe_link(link);
	}
	return 0;
}

void
pcmcia_probe_link(link)
	struct pcmcia_link *link;
{
	struct pcmcia_cardinfo cardinfo;
	struct pcmcia_attach_args paa;
	struct pcmciabus_softc *pcmcia = link->bus;
	int             i;

	PPRINTF(("- pcmcia_probe_link %p\n", link));
	/*
	 * Set up card and fetch card info.
	 */
	if (pcmcia_probedev(link, &cardinfo) == 0) {
		/* could not fetch its strings, so give up on it. */
		PCMCIA_SERVICE(link->adapter,
			       link, PCMCIA_OP_POWER,
			       0, 0);
		return;
	}

	/*
	 * See if we can reattach a device.
	 */
	CLR(link->flags, PCMCIA_ATTACH_TYPE);
	SET(link->flags, PCMCIA_REATTACH);
	for (i = 0; i < ndeldevs; i++) {
		if (deldevs[i].dev) {
			PPRINTF(("trying device\n"));
			link->device = deldevs[i].pcdev;
			if (pcmcia_configure(deldevs[i].dev->dv_parent,
					     deldevs[i].dev, link)) {
				CLR(link->flags, PCMCIA_ATTACH_TYPE);
				SET(link->flags, PCMCIA_SLOT_INUSE);
				deldevs[i].dev = NULL;
				deldevs[i].pcdev = NULL;
				return;
			}
		}
	}


	paa.paa_cardinfo = &cardinfo;
	paa.paa_link = link;
	paa.paa_aux = NULL;
	paa.paa_bestmatch = 0;
	paa.paa_matchonly = 1;
	CLR(link->flags, PCMCIA_ATTACH_TYPE);
	SET(link->flags, PCMCIA_ATTACH);

	/* Run the config matching routines to find us a good match.
	 * match routines will flag on "matchonly" and fill in stuff
	 * into the link structure, but not return any match.
	 */
	(void) config_found_sm(&pcmcia->sc_dev,
			       &paa,
			       pcmcia_print,
			       pcmcia_submatch);

	if (PCMCIA_BUS_SEARCH(link->adapter,
			      &pcmcia->sc_dev,
			      link, NULL)) {
		CLR(link->flags, PCMCIA_ATTACH_TYPE);
		SET(link->flags, PCMCIA_SLOT_INUSE);
	} else {
		CLR(link->flags, PCMCIA_ATTACH_TYPE|PCMCIA_SLOT_INUSE);
		link->device = NULL;
		printf("%s slot %d: No matching config entry.\n",
		       pcmcia->sc_dev.dv_xname,
		       link->slot);
		PCMCIA_SERVICE(link->adapter,
			       link, PCMCIA_OP_POWER,
			       0, 0);
		link->fordriver = NULL;
	}
	return;
}

/*
 * given a target ask the device what
 * it is, and find the correct driver table
 * entry.
 */
int
pcmcia_probedev(link, cardinfo)
	struct pcmcia_link *link;
	struct pcmcia_cardinfo *cardinfo;
{
	struct pcmcia_adapter *pca = link->adapter;
	u_char          scratch[CIS_MAXSIZE];
	int             card_stat;
	int             err;
	int             pow = 0;
	int             slot = link->slot;

	PPRINTF(("- pcmcia_probe_dev\n"));

	printf("%s slot %d: ", ((struct device *) link->bus)->dv_xname, slot);

	/* turn off power in case it's on, to get a fresh start on things: */
	PCMCIA_SERVICE(pca, link, PCMCIA_OP_POWER, 0, 0);
	if ((err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_STATUS,
				  &card_stat, 0)) != 0) {
		printf("failed to get status %d\n", err);
		return NULL;
	}

	if (ISSET(card_stat, PCMCIA_CARD_PRESENT) == 0) {
		printf("empty\n");
		return NULL;
	}

	if (!ISSET(card_stat, PCMCIA_POWER)) {
		pow = 1;
		if ((err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_POWER, 10000,
					  PCMCIA_POWER_ON|
					  PCMCIA_POWER_5V)) != 0) {
			printf("failed to turn on power %d\n", err);
			return NULL;
		}
	}

	if (!ISSET(link->flags, (PCMCIA_SLOT_INUSE | CARD_IS_MAPPED))) {
		if ((err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_RESET,
					  500000, 0)) != 0) {
			printf("failed to reset %d\n", err);
			PCMCIA_SERVICE(pca, link, PCMCIA_OP_POWER, 0, 0);
			return NULL;
		}
	}

	/*
	 * Ask the device what it is
	 */
	if ((err = pcmcia_read_cis(link, scratch, 0, sizeof(scratch))) != 0) {
		printf("failed to read cis info %d\n", err);
		goto bad;
	}

	if ((err = pcmcia_get_cisver1(link, scratch, sizeof(scratch),
				      cardinfo->manufacturer,
				      cardinfo->model, cardinfo->add_info1,
				      cardinfo->add_info2)) != 0) {
		printf("failed to get cis info %d\n", err);
		goto bad;
	}

	printf("<%s, %s", cardinfo->manufacturer, cardinfo->model);
	if (cardinfo->add_info1[0])
		printf(", %s", cardinfo->add_info1);
	if (cardinfo->add_info2[0])
		printf(", %s", cardinfo->add_info2);
	printf(">\n");

	return 1;
bad:
	if (!pow)
		PCMCIA_SERVICE(pca, link, PCMCIA_OP_POWER, 0, 0);
	return 0;
}

int
pcmcia_configure(parent, self, aux)
	struct device  *parent;
	void 	       *self;
	void           *aux;
{
	struct device  *dev = self;
	struct pcmcia_link *link = aux;
	struct cfdata  *cf = dev->dv_cfdata;
	struct cfdriver *cd = cf->cf_driver;
	struct pcmciadevs *pcs = link->device;
	struct pcmcia_device *pcd;
	struct pcmcia_adapter *pca = link->adapter;
	struct pcmcia_conf pc_cf;
	char *devname = (char *) link->fordriver;
	u_char          scratch[CIS_MAXSIZE];
	int             mymap = 0;

	PPRINTF(("- pcmcia_configure\n"));

	if ((devname && strcmp(devname, cd->cd_name)) || !pca)
		return 0;

	if (link->devp)
		return 0;		/* something else already attached */

	if (pcs == NULL)
		pcd = NULL;
	else
		pcd = pcs->dev;

	PPRINTF(("pcmcia_configure: %p\n", pcd));
	if (!ISSET(link->flags, CARD_IS_MAPPED)) {
		/* read 'suggested' configuration */
		PPRINTF(("pcmcia_configure: calling read cis\n"));
		if (pcmcia_read_cis(link, scratch, 0, sizeof(scratch)) != 0)
			return 0;

		bzero(&pc_cf, sizeof(pc_cf));

		PPRINTF(("pcmcia_configure: calling get cf\n"));
		if (pcmcia_get_cf(link, scratch, sizeof(scratch), -1,
				  &pc_cf) != 0)
			return 0;
#ifdef PCMCIA_DEBUG
		pcmciadumpcf(&pc_cf);
#endif
		/* and modify it (device specific) */
		if (pcd && pcd->pcmcia_config) {
			PPRINTF(("pcmcia_configure: calling config %p %p\n",
				 pcd, pcd->pcmcia_config));
			if ((*pcd->pcmcia_config)(link, dev, &pc_cf, cf))
				return 0;

			if ((pc_cf.cfgtype & CFGENTRYMASK) == CFGENTRYID) {
				PPRINTF(("pcmcia_configure: calling cf2\n"));
				if (pcmcia_get_cf(link, scratch,
						  sizeof(scratch), -2,
						  &pc_cf) != 0)
					return 0;

				PPRINTF(("pcmcia_configure: calling conf2\n"));
				if (pcd->pcmcia_config(link, dev, &pc_cf, cf))
					return 0;
				/* give it a try */
				if(pc_cf.cfgid==0)
				    pc_cf.cfgid=1;
			}
		} else {
			PPRINTF(("pcmcia_configure: calling bus config\n"));
			if (PCMCIA_BUS_CONFIG(pca, link, dev, &pc_cf, cf))
				return 0;
		}
#ifdef PCMCIA_DEBUG
		pcmciadumpcf(&pc_cf);
#endif

		if (pcmcia_mapcard(link, -1, &pc_cf) != 0)
			return 0;

		mymap = 1;
	}
	link->devp = dev;

	PPRINTF(("pcmcia_configure: calling bus attach\n"));
	if (!(PCMCIA_BUS_PROBE(pca, parent, dev, cf, link))) {
		PPRINTF(("pcmcia_configure: bus probe failed\n"));
		goto bad;
	}

	if (pcd && pcd->pcmcia_insert && pcd->pcmcia_insert(link, dev, cf)) {
		PPRINTF(("pcmcia_configure: pcmcia_insert failed\n"));
		goto bad;
	}

	return 1;

bad:
	link->devp = NULL;
	if (mymap)
		pcmcia_unmapcard(link);
	PPRINTF(("pcmcia_configure: configuration error\n"));
	return 0;
}

void
pcmcia_detach(dev, arg)
	struct device *dev;
	void *arg;
{
	struct pcmcia_link *link = arg;

	link->devp = NULL;
	printf("%s: device %s at slot %d detached/really\n",
	       dev->dv_parent->dv_xname,
	       dev->dv_xname, link->slot);
}

int
pcmcia_unconfigure(link)
	struct pcmcia_link *link;
{
	int             i;
	struct device  *dev;
	struct pcmcia_adapter *pca = link->adapter;
	struct pcmcia_device *pcd;

	PPRINTF(("- pcmcia_unconfigure\n"));
	if (link->devp == NULL)
		return ENODEV;

	if (link->device)
		pcd = link->device->dev;
	else
		pcd = NULL;

	if (ISSET(link->flags, CARD_IS_MAPPED)) {
		if (pcd && pcd->pcmcia_remove) {
			if ((*pcd->pcmcia_remove)(link, link->devp))
				return EBUSY;
		}
		else {
			if (PCMCIA_BUS_UNCONFIG(pca, link))
				return EBUSY;
		}
		if (pcmcia_unmapcard(link) != 0)
			return EBUSY;
	}
	if (config_detach(link->devp->dv_cfdata, pcmcia_detach, link)) {
		/* must be retained */
		for (i = 0; deldevs && deldevs[i].dev && i < ndeldevs; i++)
			continue;

		if (i >= ndeldevs) {
			int sz = ndeldevs ? (ndeldevs * 2) : 
				(MINALLOCSIZE / sizeof(deldevs[0]));
			struct old_devs *ndel = malloc(sz * sizeof(deldevs[0]),
						      M_DEVBUF, M_NOWAIT);
			if (!ndel) {
				PPRINTF(("pcmcia_delete: creating dev array"));
				return ENOMEM;
			}
			bzero(ndel, sz * sizeof(ndel[0]));
			if (ndeldevs) {
				bcopy(deldevs, ndel,
				      ndeldevs * sizeof(deldevs[0]));
				free(deldevs, M_DEVBUF);
			}
			ndeldevs = sz - 1;
			deldevs = ndel;
		}
		dev = deldevs[i].dev = link->devp;
		deldevs[i].pcdev = link->device;
		link->devp = NULL;
		TAILQ_REMOVE(&alldevs, dev, dv_list);
		printf("%s: device %s at slot %d detached/retained\n",
		       dev->dv_parent->dv_xname,
		       dev->dv_xname, link->slot);
		/*
		 * Make this node eligible to probe again.
		 * Since we're indirectly allocating state,
		 * this device data will not get trashed later and we
		 * can hold onto it.
		 */
/*		dev->dv_cfdata->cf_fstate = FSTATE_NOTFOUND;*/
	}
	return 0;
}

/*
 * Map the card into I/O and memory space, using the details provided
 * with pc_cf.
 */

int
pcmcia_mapcard(link, unit, pc_cf)
	struct pcmcia_link *link;
	int unit;
	struct pcmcia_conf *pc_cf;
{
	struct pcmcia_adapter *pca = link->adapter;
	int s, i, err;
	PPRINTF(("- pcmcia_mapcard\n"));

	if (pca == NULL)
		return ENXIO;
	s = splbio();

	while (SCRATCH_INUSE(pca))
		sleep((caddr_t) & SCRATCH_INUSE(pca), PZERO - 1);

	SCRATCH_INUSE(pca) = 1;
	splx(s);
	for (i = 0; i < pc_cf->memwin; i++) {
		if ((err = PCMCIA_MAP_MEM(pca, link,
					  pca->pa_bc,
					  (caddr_t) pc_cf->mem[i].start,
					  pc_cf->mem[i].caddr,
					  pc_cf->mem[i].len,
					  (pc_cf->mem[i].flags &
					  (PCMCIA_MAP_16 | PCMCIA_MAP_ATTR)) |
					  i | PCMCIA_PHYSICAL_ADDR)) != 0) {
			PPRINTF(("pcmcia_mapcard: mapmem %d err%d\n", i, err));
			goto error;
		}
	}
	for (i = 0; i < pc_cf->iowin; i++) {
		if ((err = PCMCIA_MAP_IO(pca, link, pc_cf->io[i].start,
					 pc_cf->io[i].len,
					 (pc_cf->io[i].flags & (PCMCIA_MAP_16 |
					 PCMCIA_MAP_8)) | i)) != 0) {
			PPRINTF(("pcmcia_mapcard: mapio %d err %d\n", i, err));
			goto error;
		}
	}

	if ((pc_cf->irq_num & 0xf) > 0) {
		if ((err = PCMCIA_MAP_INTR(pca, link, pc_cf->irq_num & 0xf,
					   0)) != 0) {
			PPRINTF(("pcmcia_mapcard: map_intr %d err %d\n", 
			         pc_cf->irq_num & 0xf, err));
			goto error;
		}
	}
	/* Now we've mapped everything enable it */
	if ((err = PCMCIA_MAP_MEM(pca, link, SCRATCH_BC(pca), SCRATCH_MEM(pca),
	     pc_cf->cfg_off & (~(SCRATCH_SIZE(pca) - 1)), SCRATCH_SIZE(pca),
				  PCMCIA_MAP_ATTR | PCMCIA_LAST_WIN)) != 0) {
		PPRINTF(("pcmcia_mapcard: enable err %d\n", err));
		goto error;
	}

	if ((err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_RESET, -500000,
				  pc_cf->iocard)) != 0) {
		PPRINTF(("failed to reset %d\n", err));
		goto error;
	}

#define GETMEM(x) bus_mem_read_1(pca->scratch_bc, SCRATCH_MEM(pca), \
				 (pc_cf->cfg_off & (SCRATCH_SIZE(pca)-1)) + x)
#define PUTMEM(x,v) \
	bus_mem_write_1(pca->scratch_bc, SCRATCH_MEM(pca), \
			(pc_cf->cfg_off & (SCRATCH_SIZE(pca)-1)) + x, v)

	if (ISSET(pc_cf->cfgtype, DOSRESET)) {
		PUTMEM(0, PCMCIA_SRESET);
		delay(50000);
	}


	PPRINTF(("CMDR %x\n",(ISSET(pc_cf->cfgtype, CFGENTRYID) ?
			 pc_cf->cfgid |CFGENTRYID:
			 (pc_cf->cfgtype & CFGENTRYMASK)|1)|
		    (pc_cf->irq_level ? PCMCIA_LVLREQ : 0)
	));

	PUTMEM(0, (ISSET(pc_cf->cfgtype, CFGENTRYID) ?
		   pc_cf->cfgid |CFGENTRYID:
		   (pc_cf->cfgtype & CFGENTRYMASK)|1)|
	       (pc_cf->irq_level ? PCMCIA_LVLREQ : 0));
	delay(50000);

	if (ISSET(pc_cf->cfg_regmask, (1 << (PCMCIA_SCR / 2))))
		PUTMEM(PCMCIA_SCR, (link->slot & 1) | 0x10);

#if 0
	DPRINTF(("CCSR %x\n", GETMEM(PCMCIA_CCSR]));
	if (ISSET(GETMEM(PCMCIA_CCSR), PCMCIA_POWER_DOWN)) {
		u_char val = GETMEM(PCMCIA_CCSR);
		CLR(val, PCMCIA_POWER_DOWN);
		PUTMEM(PCMCIA_CCSR, var);
		DPRINTF(("CCSR now %x\n", GETMEM(PCMCIA_CCSR)));
	}
#endif


	PPRINTF(("pcmcia_mapcard: about to initialize...\n"));

	if ((err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_WAIT,
				  1000, 0)) != 0) {
		PPRINTF(("failed to initialize %d\n", err));
		err = 0;		/* XXX */
	}
error:
	PCMCIA_MAP_MEM(pca, link, SCRATCH_BC(pca), SCRATCH_MEM(pca), 0,
		       SCRATCH_SIZE(pca), PCMCIA_LAST_WIN | PCMCIA_UNMAP);
	if (err != 0) {
		PPRINTF(("pcmcia_mapcard: unmaping\n"));
		for (i = 0; i < pc_cf->memwin; i++) {
			PCMCIA_MAP_MEM(pca, link,
				       pca->pa_bc,
				       (caddr_t) pc_cf->mem[i].start,
				       pc_cf->mem[i].caddr,
				       pc_cf->mem[i].len,
				       (pc_cf->mem[i].flags & (PCMCIA_MAP_16 |
				       PCMCIA_MAP_ATTR)) | i |
				       PCMCIA_PHYSICAL_ADDR | PCMCIA_UNMAP);
		}
		for (i = 0; i < pc_cf->iowin; i++) {
			PCMCIA_MAP_IO(pca, link, pc_cf->io[i].start,
				      pc_cf->io[i].len,
				      (pc_cf->io[i].flags & (PCMCIA_MAP_16 |
				      PCMCIA_MAP_8)) | i | PCMCIA_UNMAP);
		}
		PCMCIA_MAP_INTR(pca, link, pc_cf->irq_num, PCMCIA_UNMAP);
		CLR(link->flags, CARD_IS_MAPPED);
		link->iowin = 0;
		link->memwin = 0;
		link->intr = 0;
	} else {
		SET(link->flags, CARD_IS_MAPPED);
		link->iowin = pc_cf->iowin;
		link->memwin = pc_cf->memwin;
		link->intr = pc_cf->irq_num;
	}
	s = splbio();
	SCRATCH_INUSE(pca) = 0;
	wakeup((caddr_t) & SCRATCH_INUSE(pca));
	splx(s);
	return err;
}

int
pcmcia_unmapcard(link)
	struct pcmcia_link *link;
{
	int             i;
	struct pcmcia_adapter *pca = link->adapter;
	PPRINTF(("- pcmcia_unmapcard\n"));
	if (!pca)
		return ENODEV;

	for (i = 0; i < link->memwin; i++)
		PCMCIA_MAP_MEM(pca, link, pca->pa_bc, 0, 0, 0,
			       (i | PCMCIA_UNMAP));

	for (i = 0; i < link->iowin; i++)
		PCMCIA_MAP_IO(pca, link, 0, 0, (i | PCMCIA_UNMAP));

	PCMCIA_MAP_INTR(pca, link, link->intr, PCMCIA_UNMAP);
	PCMCIA_SERVICE(pca, link, PCMCIA_OP_RESET, 50000, 0);
	CLR(link->flags, (CARD_IS_MAPPED | PCMCIA_SLOT_INUSE));
	link->iowin = 0;
	link->memwin = 0;
	link->intr = 0;
	return 0;
}


int
pcmcia_mapcard_and_configure(link, unit, pc_cf)
	struct pcmcia_link *link;
	struct pcmcia_conf *pc_cf;
	int             unit;
{
	int             mymap = 0;
	int err;

	PPRINTF(("- pcmcia_mapcard_and_configure\n"));
	if (pc_cf->driver_name[0][0]) {
		if ((err = pcmcia_mapcard(link, unit, pc_cf)) != 0) {
			return err;
		}
		mymap=1;
		link->fordriver = pc_cf->driver_name[0];
	} else
		link->fordriver = NULL;
	
	pcmcia_probe_link(link);

	if (!ISSET(link->flags, PCMCIA_SLOT_INUSE)) {
		if (mymap)
			pcmcia_unmapcard(link);
		return ENODEV;
	}
	return 0;
}


int
pcmcia_read_cis(link, scratch, offs, len)
	struct pcmcia_link *link;
	u_char         *scratch;
	int             offs, len;
{
	struct pcmcia_adapter *pca = link->adapter;
	int             s;
	int             err = 0;
	int             j = 0;
	u_char *p = SCRATCH_MEM(pca);
	int size = SCRATCH_SIZE(pca);
	volatile int *inuse = &SCRATCH_INUSE(pca);

	PPRINTF(("- pcmcia_read_cis: mem %p size %d\n", p, size));
	if (pca == NULL)
		return ENXIO;

	s = splbio();
	while (*inuse)
		sleep((caddr_t) inuse, PZERO - 1);
	*inuse = 1;
	splx(s);

	while (len > 0) {
		int pgoff = offs / size;
		int toff = offs - (pgoff * size);
		int tlen = min(len + toff, size / 2) - toff;
		int i;

		if ((err = PCMCIA_MAP_MEM(pca, link, pca->pa_bc, p, pgoff,
					  size,
					  PCMCIA_MAP_ATTR |
					  PCMCIA_LAST_WIN)) != 0)
			goto error;

		PPRINTF(("- pcmcia_read_cis: mem mapped\n"));

		for (i = 0; i < tlen; j++, i++)
			scratch[j] = p[toff + i * 2];

		PCMCIA_MAP_MEM(pca, link, pca->pa_bc, p, 0, size,
			       PCMCIA_LAST_WIN | PCMCIA_UNMAP);
		len -= tlen;
	}
error:
	s = splbio();
	*inuse = 0;
	wakeup((caddr_t) inuse); 
	splx(s);

	PPRINTF(("- pcmcia_read_cis return %d\n", err));
	return err;
}

/* here we start our pseudodev for controlling the slots */
#define PCMCIABUS_UNIT(a)    (minor(a))
#define PCMCIABUS_SLOT(a)    (a&0x3)	/* per-controller */
#define PCMCIABUS_SLOTID(a)  (a&0xf)	/* system-wide assignment */
#define PCMCIABUS_CHIPNO(a)  ((a&0xf)>>2)
#define PCMCIABUS_CHIPID(a) (a&0x3)
#define PCMCIABUS_CHIP       0x40
#define PCMCIABUS_BUS        0x80
#define PCMCIABUS_BUSID(a)   (a&0x3)
#define PCMCIABUS_DEVTYPE(a) ((a)&(PCMCIABUS_CHIP|PCMCIABUS_BUS))
static int      busopen = 0;
static int      chipopen[4] = {0, 0, 0, 0};

int
pcmciaopen(dev, flag, mode, p)
	dev_t           dev;
	int             flag, mode;
	struct proc    *p;
{
	int             unit = PCMCIABUS_UNIT(dev);
	int		chipid, slot;
	struct	pcmcia_link *link;
	struct pcmciabus_softc *pcmcia;

	PPRINTF(("- pcmciabusopen\n"));
	switch (PCMCIABUS_DEVTYPE(unit)) {
	case PCMCIABUS_BUS:
		if (unit != PCMCIABUS_BUS)
			return ENXIO;
		if (busopen)
			return EBUSY;
		busopen = 1;
		break;

	case PCMCIABUS_CHIP:
		chipid = PCMCIABUS_CHIPID(unit);
		if (chipid < 0 || chipid >= pcmciabus_cd.cd_ndevs)
			return ENXIO;
		pcmcia = pcmciabus_cd.cd_devs[chipid];
		if (pcmcia == NULL || pcmcia->sc_driver == NULL)
			return ENXIO;

		if (chipopen[chipid])
			return EBUSY;

		chipopen[chipid] = 1;
		break;

	case 0:
		slot = PCMCIABUS_SLOT(unit);
		chipid = PCMCIABUS_CHIPNO(unit);

		if (chipid < 0 || chipid >= pcmciabus_cd.cd_ndevs)
			return ENXIO;

		pcmcia = pcmciabus_cd.cd_devs[chipid];
		if (pcmcia == NULL || pcmcia->sc_driver == NULL)
			return ENXIO;
		link = pcmcia->sc_link[slot];
		if (!link)
		 	return ENXIO;

		if (ISSET(link->flags, PCMCIA_SLOT_OPEN))
			return EBUSY;

		SET(link->flags, PCMCIA_SLOT_OPEN);
		break;

	default:
		return ENXIO;

	}
	return 0;
}


int
pcmciaclose(dev)
{
	int unit = PCMCIABUS_UNIT(dev);
	int chipid, slot;
	struct	pcmcia_link *link;
	struct pcmciabus_softc *pcmcia;
	int s;

	PPRINTF(("- pcmciabusclose\n"));
	switch (PCMCIABUS_DEVTYPE(unit)) {
	case PCMCIABUS_BUS:
		busopen = 0;
		break;

	case PCMCIABUS_CHIP:
		chipid = PCMCIABUS_CHIPID(unit);
		chipopen[chipid] = 0;
		break;

	case 0:
		slot = PCMCIABUS_SLOT(unit);
		chipid = PCMCIABUS_CHIPNO(unit);
		pcmcia = pcmciabus_cd.cd_devs[chipid];
		link = pcmcia->sc_link[slot];

		s = splclock();
		CLR(link->flags, (PCMCIA_SLOT_OPEN|PCMCIA_SLOT_EVENT));
		splx(s);
		break;

	default:
		return ENXIO;
	}
	return 0;
}

int
pcmciachip_ioctl(chipid, cmd, data)
	int             chipid, cmd;
	caddr_t         data;
{
	struct pcmciabus_softc *pcmcia = pcmciabus_cd.cd_devs[chipid];
	struct pcmcia_adapter *pca = pcmcia->sc_driver;
	struct pcmcia_link link;
	struct pcmcia_regs *pi = (void *) data;

	PPRINTF(("- pcmciachip_ioctl\n"));
	if (pca->chip_link == NULL || pca->adapter_softc == NULL)
		return ENXIO;

	switch (cmd) {
	case PCMCIAIO_READ_REGS:
		pi->chip = chipid;
		link.adapter = pca;
		link.slot = 0;
		return PCMCIA_SERVICE(pca, &link, PCMCIA_OP_GETREGS,
				      pi->chip_data, 0);
	}
	return ENOTTY;
}

int
pcmciaslot_ioctl(link, slotid, cmd, data)
	struct pcmcia_link *link;
	int             slotid, cmd;
	caddr_t         data;
{
	int             err = 0;
	struct pcmciabus_softc *pcmcia =
	    pcmciabus_cd.cd_devs[PCMCIABUS_CHIPNO(slotid)];
	struct pcmcia_adapter *pca = pcmcia->sc_driver;

	PPRINTF(("- pcmciaslot_ioctl\n"));
	if (link == NULL || pca->chip_link == NULL ||
	    pca->adapter_softc == NULL)
		return ENXIO;

	switch (cmd) {
	case PCMCIAIO_GET_STATUS:
		{
			struct pcmcia_status *pi = (void *) data;
			pi->slot = slotid;
			pi->status = 0;
			err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_STATUS,
					     &pi->status, 0);
			if (!err) {
			    if (ISSET(link->flags, CARD_IS_MAPPED))
				SET(pi->status, PCMCIA_CARD_IS_MAPPED);
			    if (ISSET(link->flags, PCMCIA_SLOT_INUSE))
				SET(pi->status, PCMCIA_CARD_INUSE);
			}
			return err;
		}

	case PCMCIAIO_GET_INFO:
		{
			struct pcmcia_info *pi = (void *) data;
			int             status;

			if ((err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_STATUS,
						  &status, 0)) != 0)
				return err;
			if (!ISSET(status, PCMCIA_CARD_PRESENT))
				return ENODEV;
			pi->slot = slotid;
			return pcmcia_read_cis(link, pi->cis_data, 0,
					       CIS_MAXSIZE);
		}

	case PCMCIAIO_CONFIGURE:
		{
			struct pcmcia_conf *pc_cf = (void *) data;
			return pcmcia_mapcard_and_configure(link, -1, pc_cf);
		}

	case PCMCIAIO_UNCONFIGURE:
		return pcmcia_unconfigure(link);

	case PCMCIAIO_UNMAP:
		if (ISSET(link->flags, PCMCIA_SLOT_INUSE))
			return EBUSY;
		return pcmcia_unmapcard(link);

	case PCMCIAIO_SET_POWER:
		{
			int             pi = *(int *) data;
			pi &= 0x3;
			switch (pi) {
			case PCMCIASIO_POWER_OFF:
				return PCMCIA_SERVICE(pca, link,
						      PCMCIA_OP_POWER, 0, 0);

			case PCMCIASIO_POWER_5V:
			case PCMCIASIO_POWER_3V:
			case PCMCIASIO_POWER_AUTO:
				err = PCMCIA_SERVICE(pca, link,
						     PCMCIA_OP_POWER,
						     10000, pi);
				if (err)
					return err;

				err = PCMCIA_SERVICE(pca, link,
						     PCMCIA_OP_RESET,
						     500000, 0);
				if (err) {
					PPRINTF(("failed to reset %d\n", err));
					PCMCIA_SERVICE(pca, link,
						       PCMCIA_OP_POWER, 0, 0);
					return err;
				}
				return 0;

			default:
				return EINVAL;
			}
		}

	case PCMCIAIO_READ_COR:
		{
			struct pcmcia_info *pi = (void *)data;
			struct pcmcia_conf pc_cf;
			int status,s;

			err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_STATUS, 
					     &status, 0);
			if (err)
				return err;
			if (!ISSET(status, PCMCIA_CARD_PRESENT)) 
				return ENODEV;

			if ((status = pcmcia_read_cis(link, pi->cis_data, 0,
						      CIS_MAXSIZE)))
				return status;

			bzero(&pc_cf, sizeof(pc_cf));
			if (pcmcia_get_cf(link, pi->cis_data,
					  sizeof(pi->cis_data), -1,
					  &pc_cf) != 0 )
				return EIO;

			s=splbio();

			while(SCRATCH_INUSE(pca))
				sleep((caddr_t)&SCRATCH_INUSE(pca), PZERO - 1);

			SCRATCH_INUSE(pca) = 1;
			splx(s);
			if ((err = PCMCIA_MAP_MEM(pca, link,
						  SCRATCH_BC(pca),
						  SCRATCH_MEM(pca),
						  pc_cf.cfg_off &
						  ~(SCRATCH_SIZE(pca)-1),
						  SCRATCH_SIZE(pca),
						  PCMCIA_MAP_ATTR|
						  PCMCIA_LAST_WIN)) == 0) {
				int m, i;
				u_char *d = pi->cis_data,*p;
				p = SCRATCH_MEM(pca)+
				    (pc_cf.cfg_off & (SCRATCH_SIZE(pca)-1));
				for (i = 0, m = 1; i < 32; i++, m <<= 1) {
					if (pc_cf.cfg_regmask & m) {
						*d++ = i;
						*d++ = p[i*2];
					}
				}
				*d++ = 0xff;
				*d++ = 0xff;
				PCMCIA_MAP_MEM(pca, link,
					       SCRATCH_BC(pca),
					       SCRATCH_MEM(pca),
					       0,SCRATCH_SIZE(pca), 
					       PCMCIA_LAST_WIN|PCMCIA_UNMAP);
			} 
			s = splbio();
			SCRATCH_INUSE(pca)=0;
			wakeup((caddr_t)&SCRATCH_INUSE(pca));
			splx(s);
			return err;
		}
	default:
		return ENOTTY;
	}
	return ENOTTY;
}

int
pcmciaioctl(dev, cmd, data, flag, p)
	dev_t           dev;
	int             cmd;
	caddr_t         data;
	int             flag;
	struct proc    *p;
{
	int unit = PCMCIABUS_UNIT(dev);
	int chipid = PCMCIABUS_CHIPNO(unit);
	struct pcmciabus_softc *pcmcia;
	struct pcmcia_link *link;

	PPRINTF(("- pcmciabusioctl\n"));
	if (chipid < 0 || chipid >= pcmciabus_cd.cd_ndevs)
		return ENXIO;

	pcmcia = pcmciabus_cd.cd_devs[chipid];
	if (pcmcia == NULL)
		return ENXIO;

	switch (PCMCIABUS_DEVTYPE(unit)) {
#if 0
	case PCMCIABUS_BUS:
		return pcmciabus_ioctl(PCMCIABUS_BUSID(unit), cmd, data);
#endif
	case PCMCIABUS_CHIP:
		return pcmciachip_ioctl(PCMCIABUS_CHIPID(unit), cmd, data);
	case 0:
		link = pcmcia->sc_link[PCMCIABUS_SLOT(unit)];
		return pcmciaslot_ioctl(link, PCMCIABUS_SLOTID(unit),
					cmd, data);
	default:
		return ENXIO;
	}
}

int
pcmciaselect(device, rw, p)
	dev_t device;
	int rw;
	struct proc *p;
{
	int s;
	int unit = PCMCIABUS_UNIT(device);
	int chipid = PCMCIABUS_CHIPNO(unit);
	struct pcmciabus_softc *pcmcia;
	struct pcmcia_link *link;

	PPRINTF(("- pcmciabus_select\n"));
	pcmcia = pcmciabus_cd.cd_devs[chipid];
	
	switch (PCMCIABUS_DEVTYPE(unit)) {
	case 0:
		link = pcmcia->sc_link[PCMCIABUS_SLOT(unit)];
		break;
	case PCMCIABUS_BUS:
	case PCMCIABUS_CHIP:
	default:
		return 0;
	}

	s = splpcmcia();
	switch (rw) {
	case FREAD:
	case FWRITE:
		break;
	case 0:
		if (ISSET(link->flags, PCMCIA_SLOT_EVENT)) {
			CLR(link->flags, PCMCIA_SLOT_EVENT);
			splx(s);
			return 1;
		}
		selrecord(p, &link->pcmcialink_sel);
		break;
	}
	splx(s);
	return 0;
}

int
pcmciammap()
{
	return ENXIO;
}


#ifdef PCMCIA_DEBUG
void
pcmciadumpcf(cf)
	struct pcmcia_conf * cf;
{
	int             i;
	static char    *ios[] = {
		"auto", "8bit", "16bit", "illegal"
	};
	printf("Driver name %s\n", cf->driver_name[0]);
	printf("CFG offset %x\n", cf->cfg_off);
	printf("IRQ type %s%s\n", cf->irq_level ? "Level " : "", 
				  cf->irq_pulse ? "Pulse" : "");
	printf("IRQ num %x\n", cf->irq_num);
	printf("IRQ mask %x\n", cf->irq_mask);
	printf("CFG type %x %x\n", cf->cfgtype,cf->cfgid);
	printf("Cardtype %s\n", cf->iocard ? "IO" : "MEM");
	for (i = 0; i < cf->iowin; i++) {
		printf("iowin  %x-%x %s\n", cf->io[i].start,
		       cf->io[i].start + cf->io[i].len - 1,
		       ios[(cf->io[i].flags & 
			    (PCMCIA_MAP_8 | PCMCIA_MAP_16)) >> 8]);
	}
	for (i = 0; i < cf->memwin; i++) {
		printf("memwin  (%x)%x-%x %x\n",
		       cf->mem[i].caddr,
		       cf->mem[i].start,
		       cf->mem[i].start + cf->mem[i].len - 1,
		       cf->mem[i].flags);
	}
}
#endif

int
pcmcia_print(aux, pnp)
	void *aux;
	char *pnp;
{
#if 0
	struct pcmcia_attach_args *paa = aux;
	printf(" slot %d", paa->paa_link->slot);
#endif
	return (0);			/* be silent */
}

/*
 * Filter out inappropriate configurations before heading off to
 * the device match routines.
 */
int
pcmcia_submatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct device *self = match;
	struct cfdata *cf = self->dv_cfdata;
	struct pcmcia_attach_args *paa = aux;
	struct pcmcia_link *link = paa->paa_link;

#if 0
	printf("pcmcia_submatch: paa=%p link=%p, cf=%p\n", paa, link, cf);
	delay(2000000);

#endif

	if (cf->cf_loc[6] != -1 && link->slot != cf->cf_loc[6]) {
		printf("slot mismatch: %d cf_loc %d\n", link->slot, cf->cf_loc[6]);
		return 0;
	}

	return ((*cf->cf_attach->ca_match)(parent, match, aux));
}
