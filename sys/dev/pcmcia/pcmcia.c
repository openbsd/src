/*
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
 *      $Id: pcmcia.c,v 1.1 1996/01/15 00:05:03 hvozda Exp $
 */

/* derived from scsiconf.c writte by Julian Elischer et al */
/* TODO add modload support and loadable lists of devices */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>

#include <dev/pcmcia/pcmcia.h>
#include <dev/pcmcia/pcmciabus.h>
#include <dev/pcmcia/pcmcia_ioctl.h>

#ifdef IBM_WD
#define PCMCIA_DEBUG
#endif
#ifdef PCMCIA_DEBUG
# define PPRINTF(a)	printf a
#else
# define PPRINTF(a)
#endif

static void pcmciadumpcf __P((struct pcmcia_conf *));
static int pcmcia_strcmp __P((char *, char *, int, char *));

void pcmcia_shuthook __P((void *));

static struct pcmcia_adapter pcmcia_drivers[4];
static int      pcmcia_cntrl = 0;
static int      probed = 0;
static struct device **deldevs = NULL;
static int      ndeldevs = 0;

/* I've decided to re-ifdef these.  It makes making a kernel easier
    until I either get config(8) modified to deal somehow
    or figure out a better to way declare the prototypes and
    build up the knowndevs struct.  Stefan may have ideas...
*/

#ifdef PCMCIA_ED
extern struct pcmciadevs pcmcia_ed_devs[];
#endif
#ifdef PCMCIA_COM
extern struct pcmciadevs pcmcia_com_devs[];
#endif
#ifdef PCMCIA_EP
extern struct pcmciadevs pcmcia_ep_devs[];
#endif

static struct pcmciadevs *knowndevs[] = {
#ifdef PCMCIA_ED
	pcmcia_ed_devs,
#endif
#ifdef PCMCIA_COM
	pcmcia_com_devs,
#endif
#ifdef PCMCIA_EP
	pcmcia_ep_devs,
#endif
	NULL
};

#ifdef notyet
static struct pcmciadevs *knowndevs[10] = { NULL };
#define KNOWNSIZE (sizeof(knowndevs) / sizeof(knowndevs[0]))
#endif

#define PCMCIA_SERVICE(a,b,c,d,e)   ((a)->chip_link->pcmcia_service(b,c,\
								(void *) d,e))
#define PCMCIA_MAP_IO(a,b,c,d,e)    ((a)->chip_link->pcmcia_map_io(b,c,d,e))
#define PCMCIA_MAP_INTR(a,b,c,d)    ((a)->chip_link->pcmcia_map_intr(b,c,d))
#define PCMCIA_MAP_MEM(a,b,c,d,e,f) ((a)->chip_link->pcmcia_map_mem(b,c,d,e,f))

#define PCMCIA_BUS_INIT(a,b,c,d,e,f)((a)->bus_link->bus_init((b),(c),(d),(e)\
									,(f)))
#define PCMCIA_BUS_SEARCH(a,b,c,d)  ((a)->bus_link->bus_search((b),(c),(d)))
#define PCMCIA_BUS_PROBE(a,b,c,d,e) ((a)->bus_link->bus_probe((b),(c),(d),(e)))
#define PCMCIA_BUS_CONFIG(a,b,c,d,e)((a)->bus_link->bus_config((b),(c),(d),(e)))
#define PCMCIA_BUS_UNCONFIG(a,b)    ((a)->bus_link->bus_unconfig((b)))

#define SCRATCH_MEM(a)	((a)->scratch_mem)
#define SCRATCH_SIZE(a)	((a)->scratch_memsiz)
#define SCRATCH_INUSE(a)((a)->scratch_inuse)

/*
 * Declarations
 */
struct pcmciadevs *pcmcia_probedev __P((struct pcmcia_link *));
struct pcmciadevs *pcmcia_selectdev __P((char *, char *, char *, char *));
int pcmcia_probe_bus __P((struct pcmcia_link *, int, int,
			  struct pcmcia_conf *));
int pcmciabusmatch __P((struct device *, void *, void *));
void pcmciabusattach __P((struct device *, struct device *, void *));

struct cfdriver pcmciabuscd = {
	NULL, "pcmcia", pcmciabusmatch, pcmciabusattach, DV_DULL,
	sizeof(struct pcmciabus_softc), 1
};

#ifdef notyet
int
pcmcia_add_device(devs)
	struct pcmciadevs *devs;
{
	int i;

	if (devs == NULL)
		return 0;

	for (i = 0; i < KNOWNSIZE; i++)
		if (knowndevs[i] == NULL)
			break;

	if (i == KNOWNSIZE)
		panic("Too many pcmcia devices");

	knowndevs[i] = devs;
	for (; devs->devname != NULL; devs++)
		printf("added %s\n", devs->devname);
	return i;
}
#endif

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

int
pcmciabusmatch(parent, self, aux)
	struct device	*parent;
	void 		*self;
	void		*aux;
{
	struct pcmciabus_softc *sc = (void *)self;
	struct cfdata *cf = sc->sc_dev.dv_cfdata;
	int             i, found = 0;

	PPRINTF(("- pcmciabusmatch\n"));
	if (pcmcia_cntrl <= 0)
		return 0;

	for (i = 0; i < 4; i++)
		if (pcmcia_drivers[i].bus_link) {
			if (PCMCIA_BUS_INIT(&pcmcia_drivers[i], parent, cf,
					    aux, &pcmcia_drivers[i], 0))
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
	int             i, found = 0;

	PPRINTF(("- pcmciabusattach\n"));
	for (i = 0; i < 4; i++)
		if (pcmcia_drivers[i].bus_link) {
			if (PCMCIA_BUS_INIT(&pcmcia_drivers[i], parent, cf,
					    aux, &pcmcia_drivers[i], 1))
				found++;
		}

	printf("\n");

	pcmcia_probe_bus(NULL, sc->sc_dev.dv_unit, -1, NULL);
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
		for (bus = 0; bus < pcmciabuscd.cd_ndevs; bus++)
			if (pcmciabuscd.cd_devs[bus])
				pcmcia_probe_bus(NULL, bus, slot, NULL);
		return 0;
	} else {
		return pcmcia_probe_bus(NULL, bus, slot, NULL);
	}
}

/*
 * Probe the requested pcmcia bus. It must be already set up.
 */
int
pcmcia_probe_bus(link, bus, slot, cf)
	struct pcmcia_link *link;
	int             bus, slot;
	struct pcmcia_conf *cf;
{
	struct pcmciabus_softc *pcmcia;
	int             maxslot, minslot, maxlun, minlun;
	struct pcmciadevs *bestmatch = NULL;
	int             spec_probe = (link != NULL);

	PPRINTF(("- pcmcia_probe_bus\n"));
	if (bus < 0 || bus >= pcmciabuscd.cd_ndevs)
		return ENXIO;
	pcmcia = pcmciabuscd.cd_devs[bus];
	if (!pcmcia)
		return ENXIO;

	if (slot == -1) {
		maxslot = 7;
		minslot = 0;
	} else {
		if (slot < 0 || slot > 7)
			return EINVAL;
		maxslot = minslot = slot;
	}

	for (slot = minslot; slot <= maxslot; slot++) {
		if (link = pcmcia->sc_link[slot]) {
			if (link->devp)
				continue;
		}
		if (pcmcia_drivers[slot >> 1].adapter_softc == NULL)
			continue;

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
			link->adapter = &pcmcia_drivers[slot >> 1];
			link->slot = slot;
		}
		bestmatch = pcmcia_probedev(link);
		/*
		 * We already know what the device is.  We use a
		 * special matching routine which insists that the
		 * cfdata is of the right type rather than putting
		 * more intelligence in individual match routines for
		 * each high-level driver.
		 * We must have the final probe do all of the comparisons,
		 * or we could get stuck in an infinite loop trying the same
		 * device repeatedly.  We use the `fordriver' field of
		 * the pcmcia_link for now, rather than inventing a new
		 * structure just for the config_search().
		 */
		if (link->fordriver == NULL) {
			if (bestmatch)
				link->fordriver = bestmatch->devname;
			else {
				if (!spec_probe) {
					link->device = NULL;
					link->devp = NULL;
					PCMCIA_SERVICE(link->adapter,
						       link, PCMCIA_OP_POWER,
						       0, 0);
				}
			}
		}

		if (spec_probe) {
			if (cf && pcmcia_mapcard(link, -1, cf) != 0)
				link->fordriver = NULL;
		}

		if (link->fordriver != NULL) {
			int             i;
			struct device **delp = deldevs;
			int             found = 0;
			link->device = bestmatch;
			link->flags = (link->flags &
					  ~(PCMCIA_ATTACH_TYPE)) |
					    PCMCIA_REATTACH;
			for (i = 0; i < ndeldevs; i++, delp++) {
				if (*delp &&
				    pcmcia_configure((*delp)->dv_parent, *delp,
						     link)) {
					link->flags = (link->flags &
							  ~PCMCIA_ATTACH_TYPE)
						       | PCMCIA_SLOT_INUSE;
					found = 1;
					*delp = NULL;
					break;
				}
			}
			if (!found) {
				link->flags = (link->flags &
				       ~PCMCIA_ATTACH_TYPE) | PCMCIA_ATTACH;
				if (PCMCIA_BUS_SEARCH(link->adapter,
						      &pcmcia->sc_dev,
						      link, NULL)) {
					link->flags = (link->flags &
							  ~PCMCIA_ATTACH_TYPE)
						       | PCMCIA_SLOT_INUSE;
				} else {
					link->flags &= ~(PCMCIA_ATTACH_TYPE |
							 PCMCIA_SLOT_INUSE);
					link->device = NULL;
					printf(
					    "No matching config entry %s.\n",
					       link->fordriver ? 
					       link->fordriver : "(NULL)");
					if (!spec_probe)
						PCMCIA_SERVICE(link->adapter,
							       link,
							       PCMCIA_OP_POWER,
							       0, 0);
				}
			}
		}
	}
	return 0;
}

/*
 * given a target ask the device what
 * it is, and find the correct driver table
 * entry.
 */
struct pcmciadevs *
pcmcia_probedev(link)
	struct pcmcia_link *link;
{
	struct pcmcia_adapter *pca = link->adapter;
	u_char          scratch[CIS_MAXSIZE];
	char            manu[MAX_CIS_NAMELEN];
	char            model[MAX_CIS_NAMELEN];
	char            add_inf1[MAX_CIS_NAMELEN];
	char            add_inf2[MAX_CIS_NAMELEN];
	int             card_stat;
	int             err;
	int             pow = 0;
	int             slot = link->slot;

	PPRINTF(("- pcmcia_probe_dev\n"));

	printf("%s slot %d:",
	       ((struct device *) link->adapter->adapter_softc)->dv_xname,
	       slot & 1);

	if ((err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_STATUS,
				  &card_stat, 0)) != 0) {
		printf("failed to get status %d\n", err);
		return NULL;
	}

	if ((card_stat & PCMCIA_CARD_PRESENT) == 0) {
		printf(" <slot empty>\n");
		return NULL;
	}

	if (!(card_stat & PCMCIA_POWER)) {
		pow = 1;
		if ((err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_POWER, 10000,
					  PCMCIA_POWER_ON|
					  PCMCIA_POWER_5V)) != 0) {
			printf("failed to turn on power %d\n", err);
			return NULL;
		}
	}

	if (!(link->flags & (PCMCIA_SLOT_INUSE | CARD_IS_MAPPED))) {
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
				      manu, model, add_inf1,
				      add_inf2)) != 0) {
		printf("failed to get cis info %d\n", err);
		goto bad;
	}

	printf(" <%s, %s", manu, model);
	if (add_inf1[0])
		printf(", %s", add_inf1);
	if (add_inf2[0])
		printf(", %s", add_inf2);
	printf(">\n");


	/*
	 * Try make as good a match as possible with
	 * available sub drivers
	 */
	return pcmcia_selectdev(manu, model, add_inf1, add_inf2);
bad:
	if (!pow)
		PCMCIA_SERVICE(pca, link, PCMCIA_OP_POWER, 0, 0);
	return NULL;
}

/*
 * Try make as good a match as possible with
 * available sub drivers
 */
struct pcmciadevs *
pcmcia_selectdev(manu, model, add_inf1, add_inf2)
	char *manu, *model, *add_inf1, *add_inf2;
{
	u_int bestmatches = 0;
	struct pcmciadevs *bestmatch = (struct pcmciadevs *) 0;
	struct pcmciadevs **dlist, *dentry;

	PPRINTF(("- pcmcia_selectdev\n"));
	for (dlist = knowndevs; *dlist; dlist++)
		for (dentry = *dlist; dentry && 
				      dentry->devname != NULL; dentry++) {
			int match = 0;

#ifdef PCMCIA_DEBUG
			dentry->flags |= PC_SHOWME;
#endif
    	    	    	match|=pcmcia_strcmp(dentry->manufacturer,
    	    	    	    	    manu,dentry->flags,"manufacturer")<<6;
    	    	    	match|=pcmcia_strcmp(dentry->model,
    	    	    	    	    model,dentry->flags,"model")<<4;
    	    	    	match|=pcmcia_strcmp(dentry->add_inf1,
    	    	    	    	    add_inf1,dentry->flags,"info1")<<2;
    	    	    	match|=pcmcia_strcmp(dentry->add_inf2,
    	    	    	    	    add_inf2,dentry->flags,"info2");
/* the following was replaced by the wildcard function called above */
#if 0
			if (dentry->flags & PC_SHOWME)
				printf("manufacturer = `%s'-`%s'\n", 
				       dentry->manufacturer ?
				       dentry->manufacturer :
				       "X",
				       manu);
			if (dentry->manufacturer) {
			    if (strcmp(dentry->manufacturer, manu) == 0) {
				match |= 8;
			    } else {
				continue;
			    }
			}

			if (dentry->flags & PC_SHOWME)
				printf("model = `%s'-`%s'\n",
				       dentry->model ? dentry->model :
				       "X",
				       model);
			if (dentry->model)  {
			    if (strcmp(dentry->model, model) == 0) {
				match |= 4;
			    } else {
				continue;
			    }
			}


			if (dentry->flags & PC_SHOWME)
				printf("info1 = `%s'-`%s'\n",
				       dentry->add_inf1 ? dentry->add_inf1 :
				       "X",
				       add_inf1);
			if (dentry->add_inf1) {
			    if (strcmp(dentry->add_inf1, add_inf1) == 0) {
				match |= 2;
			    } else {
				continue;
			    }
			}

			if (dentry->flags & PC_SHOWME)
				printf("info2 = `%s'-`%s'\n", 
				       dentry->add_inf2 ? dentry->add_inf2 :
				       "X",
				       add_inf2);
			if (dentry->add_inf2) {
			    if (strcmp(dentry->add_inf2, add_inf2) == 0) {
				match |= 1;
			    } else {
				continue;
			    }
			}
#endif
#ifdef PCMCIA_DEBUG
			printf("match == %d [%d]\n",match,bestmatches);
#endif

			if(match > bestmatches) {
				bestmatches = match;
				bestmatch = dentry;
			}
		}

	return bestmatch;
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
	char           *devname = (char *) link->fordriver;
	struct pcmciadevs *pcs = link->device;
	struct pcmcia_device *pcd;
	struct pcmcia_adapter *pca = link->adapter;
	struct pcmcia_conf pc_cf;
	u_char          scratch[CIS_MAXSIZE];
	int             mymap = 0;

	PPRINTF(("- pcmcia_configure\n"));

	if (strcmp(devname, cd->cd_name) || !pca)
		return 0;

	if (pcs == NULL)
		pcd = NULL;
	else
		pcd = pcs->dev;

	PPRINTF(("pcmcia_configure: %x\n", pcd));
	if (!(link->flags & CARD_IS_MAPPED)) {
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
			PPRINTF(("pcmcia_configure: calling config\n"));
			if (pcd->pcmcia_config(link, dev, &pc_cf, cf))
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

	PPRINTF(("pcmcia_configure: calling bus probe\n"));
	if (!(PCMCIA_BUS_PROBE(pca, parent, dev, cf, link))) {
		PPRINTF(("pcmcia_configure: bus probe failed\n"));
		goto bad;
	}

	if (pcd && pcd->pcmcia_insert && pcd->pcmcia_insert(link, dev, cf)) {
		PPRINTF(("pcmcia_configure: pcmcia_insert failed\n"));
		goto bad;
	}

	link->shuthook = shutdownhook_establish(pcmcia_shuthook,
						(void *)link);
	return 1;

bad:
	link->devp = NULL;
	if (mymap)
		pcmcia_unmapcard(link);
	printf("pcmcia_configure: configuration error\n");
	return 0;
}

void
pcmcia_shuthook(arg)
void *arg;
{
    struct pcmcia_link *link = (struct pcmcia_link *)arg;
    if (pcmcia_unconfigure(link) == 0) {
	/*
	 * turn off power too.
	 */
	PCMCIA_SERVICE(link->adapter, link, PCMCIA_OP_RESET, 500000, 0);
	PCMCIA_SERVICE(link->adapter, link, PCMCIA_OP_POWER, 0, 0);
    }
}

int
pcmcia_unconfigure(link)
	struct pcmcia_link *link;
{
	int             status;
	int             i, err;
	struct device **delp;
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

	if (link->flags & CARD_IS_MAPPED) {
		if (pcd && pcd->pcmcia_remove) {
			if (pcd->pcmcia_remove(link, link->devp))
				return EBUSY;
		}
		else {
			if (PCMCIA_BUS_UNCONFIG(pca, link))
				return EBUSY;
		}
		if (pcmcia_unmapcard(link) != 0)
			return EBUSY;
	}
	delp = deldevs;
	for (i = 0; delp && *delp && i < ndeldevs; i++, delp++)
		continue;
	if (i >= ndeldevs) {
		int sz = ndeldevs ? (ndeldevs * 2) : 
				    (MINALLOCSIZE / sizeof(void *));
		struct device **ndel = malloc(sz * sizeof(void *),
					      M_DEVBUF, M_NOWAIT);
		if (!ndel) {
			PPRINTF(("pcmcia_delete: creating dev array"));
			return ENOMEM;
		}
		bzero(ndel, sz * sizeof(void *));
		if (ndeldevs) {
			bcopy(deldevs, ndel, ndeldevs * sizeof(void *));
			free(deldevs, M_DEVBUF);
		}
		ndeldevs = sz - 1;
		deldevs = ndel;
		delp = deldevs + i;
	}
	dev = *delp = link->devp;
	link->devp = NULL;
	printf("device %s in pcmcia slot %d detached\n", dev->dv_xname,
	       link->slot);
	shutdownhook_disestablish(link->shuthook);
	link->shuthook = 0;
	return 0;
}

int
pcmcia_mapcard(link, unit, pc_cf)
	struct pcmcia_link *link;
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
	if ((err = PCMCIA_MAP_MEM(pca, link, SCRATCH_MEM(pca),
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

#define GETMEM(x) SCRATCH_MEM(pca)[(pc_cf->cfg_off & \
			            (SCRATCH_SIZE(pca) - 1)) + x]
	if ((pc_cf->cfgtype & DOSRESET)) {
		GETMEM(0) = PCMCIA_SRESET;
		delay(50000);
	}


	PPRINTF(("CMDR %x\n",((pc_cf->cfgtype & CFGENTRYID) ?
			 pc_cf->cfgid |CFGENTRYID:
			 (pc_cf->cfgtype & CFGENTRYMASK)|1)|
		    (pc_cf->irq_level ? PCMCIA_LVLREQ : 0)
	));

	GETMEM(0) = ((pc_cf->cfgtype & CFGENTRYID) ?
			 pc_cf->cfgid |CFGENTRYID:
			 (pc_cf->cfgtype & CFGENTRYMASK)|1)|
		    (pc_cf->irq_level ? PCMCIA_LVLREQ : 0);
	delay(50000);

	if (pc_cf->cfg_regmask & (1 << (PCMCIA_SCR / 2)))
		GETMEM(PCMCIA_SCR) = (link->slot & 1) | 0x10;

#if 0
	DPRINTF(("CCSR %x\n", GETMEM(PCMCIA_CCSR]));
	if (GETMEM(PCMCIA_CCSR] & PCMCIA_POWER_DOWN) {
		GETMEM(PCMCIA_CCSR] &= ~PCMCIA_POWER_DOWN;
		DPRINTF(("CCSR now %x\n", GETMEM(PCMCIA_CCSR]));
	}
#endif

	if ((err = PCMCIA_SERVICE(pca, link, PCMCIA_OP_WAIT,
				  500000, 0)) != 0)
		PPRINTF(("failed to initialize %d\n", err));
error:
	PCMCIA_MAP_MEM(pca, link, 0, 0, 0, PCMCIA_LAST_WIN | PCMCIA_UNMAP);
	if (err != 0) {
		for (i = 0; i < pc_cf->memwin; i++) {
			PCMCIA_MAP_MEM(pca, link,
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
		link->flags &= ~CARD_IS_MAPPED;
		link->iowin = 0;
		link->memwin = 0;
		link->intr = 0;
	} else {
		link->flags |= CARD_IS_MAPPED;
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
		PCMCIA_MAP_MEM(pca, link, 0, 0, 0, (i | PCMCIA_UNMAP));

	for (i = 0; i < link->iowin; i++)
		PCMCIA_MAP_IO(pca, link, 0, 0, (i | PCMCIA_UNMAP));

	PCMCIA_MAP_INTR(pca, link, link->intr, PCMCIA_UNMAP);
	PCMCIA_SERVICE(pca, link, PCMCIA_OP_RESET, 0, 0);
	link->flags &= ~(CARD_IS_MAPPED | PCMCIA_SLOT_INUSE);
	link->iowin = 0;
	link->memwin = 0;
	link->intr = 0;
	return 0;
}


static int
pcmcia_mapcard_and_configure(link, unit, pc_cf)
	struct pcmcia_link *link;
	struct pcmcia_conf *pc_cf;
	int             unit;
{
	int             err;
	int             mymap = 0;

	PPRINTF(("- pcmcia_mapcard_and_configure\n"));
	if (pc_cf->driver_name[0][0]) {
#if 0
		if ((err = pcmcia_mapcard(link, unit, pc_cf)) != 0) {
			return err;
		}
		mymap=1;
#endif
		link->fordriver = pc_cf->driver_name[0];
	} else {
		link->fordriver = NULL;
		pc_cf = NULL;
	}
	pcmcia_probe_bus(link, 0, link->slot, pc_cf);
	if ((link->flags & PCMCIA_SLOT_INUSE) == 0) {
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

	PPRINTF(("- pcmcia_read_cis\n"));
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

		if ((err = PCMCIA_MAP_MEM(pca, link, p, pgoff, size,
					  PCMCIA_MAP_ATTR |
					  PCMCIA_LAST_WIN)) != 0)
			goto error;

		for (i = 0; i < tlen; j++, i++)
			scratch[j] = p[toff + i * 2];

		PCMCIA_MAP_MEM(pca, link, p, 0, size,
			       PCMCIA_LAST_WIN | PCMCIA_UNMAP);
		len -= tlen;
	}
error:
	s = splbio();
	*inuse = 0;
	wakeup((caddr_t) inuse); 
	splx(s);

	return err;
}

/* here we start our pseudodev for controlling the slots */
#define PCMCIABUS_UNIT(a)    (minor(a))
#define PCMCIABUS_SLOT(a)    (a&0x7)
#define PCMCIABUS_CHIPIID(a) (a&0x3)
#define PCMCIABUS_CHIP       0x10
#define PCMCIABUS_BUS        0x20
#define PCMCIABUS_DEVTYPE(a) ((a)&(PCMCIABUS_CHIP|PCMCIABUS_BUS))
static int      busopen = 0;
static int      chipopen[4] = {0, 0, 0, 0};

int
pcmciabusopen(dev, flag, mode, p)
	dev_t           dev;
	int             flag, mode;
	struct proc    *p;
{
	int             unit = PCMCIABUS_UNIT(dev);
	int		chipid, slot;
	struct	pcmcia_link *link;
	struct pcmciabus_softc *pcmcia;

	PPRINTF(("- pcmciabusopen\n"));
	if (pcmcia_cntrl == 0)
		return ENXIO;
	switch (PCMCIABUS_DEVTYPE(unit)) {
	case PCMCIABUS_BUS:
		if (unit != PCMCIABUS_BUS)
			return ENXIO;
		if (busopen)
			return EBUSY;
		busopen = 1;
		break;

	case PCMCIABUS_CHIP:
		chipid = PCMCIABUS_CHIPIID(unit);
		if (chipid > 3)
			return ENXIO;
		if (pcmcia_drivers[chipid].adapter_softc == NULL)
			return ENXIO;

		if (chipopen[chipid])
			return EBUSY;

		chipopen[chipid] = 1;
		break;

	case 0:
		slot = PCMCIABUS_SLOT(unit);
		chipid = slot >> 1;

		if (chipid > 7)
			return ENXIO;

		if (pcmcia_drivers[chipid].adapter_softc == NULL)
			return ENXIO;
		pcmcia = pcmciabuscd.cd_devs[0];
		link = pcmcia->sc_link[slot];

		if (link->flags & PCMCIA_SLOT_OPEN)
			return EBUSY;

		link->flags |= PCMCIA_SLOT_OPEN;
		break;

	default:
		return ENXIO;

	}
	return 0;
}


int
pcmciabusclose(dev)
{
	int unit = PCMCIABUS_UNIT(dev);
	int chipid, slot;
	struct	pcmcia_link *link;
	struct pcmciabus_softc *pcmcia;
	int s;

	PPRINTF(("- pcmciabusclose\n"));
	if (pcmcia_cntrl == 0)
		return ENXIO;
	switch (PCMCIABUS_DEVTYPE(unit)) {
	case PCMCIABUS_BUS:
		busopen = 0;
		break;

	case PCMCIABUS_CHIP:
		chipid = PCMCIABUS_CHIPIID(unit);
		chipopen[chipid] = 0;
		break;

	case 0:
		slot = PCMCIABUS_SLOT(unit);
		pcmcia = pcmciabuscd.cd_devs[0];
		link = pcmcia->sc_link[slot];

		s = splclock();
		link->flags &= ~(PCMCIA_SLOT_OPEN|PCMCIA_SLOT_EVENT);
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
	int             err = 0;
	struct pcmcia_adapter *pca = &pcmcia_drivers[chipid];
	struct pcmcia_link link;
	struct pcmcia_regs *pi = (void *) data;

	PPRINTF(("- pcmciachip_ioctl\n"));
	if (pca->chip_link == NULL || pca->adapter_softc == NULL)
		return ENXIO;

	switch (cmd) {
	case PCMCIAIO_READ_REGS:
		pi->chip = chipid;
		link.adapter = pca;
		link.slot = chipid << 1;
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
	struct pcmcia_adapter *pca = &pcmcia_drivers[slotid >> 1];

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
				pi->status |= ((link->flags & CARD_IS_MAPPED) ?
					       PCMCIA_CARD_IS_MAPPED : 0) |
					((link->flags & PCMCIA_SLOT_INUSE) ?
					 PCMCIA_CARD_INUSE : 0);
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
			if ((status & PCMCIA_CARD_PRESENT) == 0)
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
			if ((status & PCMCIA_CARD_PRESENT) == 0) 
				return ENODEV;

			if (status = pcmcia_read_cis(link, pi->cis_data, 0,
						     CIS_MAXSIZE))
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
			if ((err = PCMCIA_MAP_MEM(pca, link, SCRATCH_MEM(pca),
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
				PCMCIA_MAP_MEM(pca, link, SCRATCH_MEM(pca),
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
pcmciabusioctl(dev, cmd, data, flag, p)
	dev_t           dev;
	int             cmd;
	caddr_t         data;
	int             flag;
	struct proc    *p;
{
	int             unit = PCMCIABUS_UNIT(dev);
	struct pcmciabus_softc *pcmcia;
	struct pcmcia_link *link;

	PPRINTF(("- pcmciabus_ioctl\n"));
	pcmcia = pcmciabuscd.cd_devs[0];
	if (pcmcia_cntrl == 0 || pcmcia == NULL)
		return ENXIO;
	switch (PCMCIABUS_DEVTYPE(unit)) {
#if 0
	case PCMCIABUS_BUS:
		return pcmciabus_ioctl(0, cmd, data);
#endif
	case PCMCIABUS_CHIP:
		return pcmciachip_ioctl(PCMCIABUS_CHIPIID(unit), cmd, data);
	case 0:
		link = pcmcia->sc_link[PCMCIABUS_SLOT(unit)];
		return pcmciaslot_ioctl(link, PCMCIABUS_SLOT(unit), cmd, data);
	default:
		return ENXIO;
	}
}

int
pcmciabusselect(device, rw, p)
	dev_t device;
	int rw;
	struct proc *p;
{
	int s;
	int unit = PCMCIABUS_UNIT(device);
	struct pcmciabus_softc *pcmcia;
	struct pcmcia_link *link;

	PPRINTF(("- pcmciabus_ioctl\n"));
	pcmcia = pcmciabuscd.cd_devs[0];
	
	switch (PCMCIABUS_DEVTYPE(unit)) {
	case 0:
		link = pcmcia->sc_link[PCMCIABUS_SLOT(unit)];
		break;
	case PCMCIABUS_BUS:
	case PCMCIABUS_CHIP:
	default:
		return 0;
	}

	s = splclock();		/* XXX something higher than all devices that can plug in.... */
	switch (rw) {
	case FREAD:
	case FWRITE:
		break;
	case 0:
		if (link->flags & PCMCIA_SLOT_EVENT) {
			link->flags &= ~PCMCIA_SLOT_EVENT;
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
pcmciabusmmap()
{
	return ENXIO;
}

/* pcmcia template string match. A '*' matches any number of characters.
   A NULL template matches all strings.
   return-value 
    0 nomatch 
    1 wildcard match 
    2 excact match
 */
static int
pcmcia_strcmp(templ,val,flags,msg)
    char *templ;
    char *val;
    int flags;
    char *msg;
{
    char *ltempl=NULL,*lval=NULL;

    if (flags & PC_SHOWME)
	    printf("%s = `%s'-`%s'\n", msg, templ ? templ : "X", val);

    if(templ==NULL)
        return 1;
    while(*val) {
        while(*templ=='*') {
            ltempl=++templ;
            lval=val;
        }
        if(*templ==*val) {
            templ++;
            val++;
        } else {
            if(ltempl==NULL)
                return 0;
            val=++lval;
            templ=ltempl;
        }
    }
    if(*templ!=0 && *templ!='*')
        return 0;
    return  ltempl?1:2;
}

#ifdef PCMCIA_DEBUG
static void
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
