/*	$OpenBSD: subr_autoconf.c,v 1.22 1999/01/11 05:12:23 millert Exp $	*/
/*	$NetBSD: subr_autoconf.c,v 1.21 1996/04/04 06:06:18 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Header: subr_autoconf.c,v 1.12 93/02/01 19:31:48 torek Exp  (LBL)
 *
 *	@(#)subr_autoconf.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <machine/limits.h>
/* Extra stuff from Matthias Drochner <drochner@zelux6.zel.kfa-juelich.de> */
#include <sys/queue.h>

/* Bleh!  Need device_register proto */
#if defined(__alpha__) || defined(hp300)
#include <machine/autoconf.h>
#endif /* __alpha__ || hp300 */

/*
 * Autoconfiguration subroutines.
 */

typedef int (*cond_predicate_t) __P((struct device *, void *));

/*
 * ioconf.c exports exactly two names: cfdata and cfroots.  All system
 * devices and drivers are found via these tables.
 */
extern short cfroots[];

#define	ROOT ((struct device *)NULL)

struct matchinfo {
	cfmatch_t fn;
	struct	device *parent;
	void	*match, *aux;
	int	indirect, pri;
};

struct cftable_head allcftables;

static struct cftable staticcftable = {
	cfdata
};

#ifndef AUTOCONF_VERBOSE
#define AUTOCONF_VERBOSE 0
#endif /* AUTOCONF_VERBOSE */
int autoconf_verbose = AUTOCONF_VERBOSE;	/* trace probe calls */

static char *number __P((char *, int));
static void mapply __P((struct matchinfo *, struct cfdata *));
static int haschild __P((struct device *));
int	detach_devices __P((cond_predicate_t, void *,
	    config_detach_callback_t, void *));
int	dev_matches_cfdata __P((struct device *dev, void *));
int	parentdev_matches_cfdata __P((struct device *dev, void *));


struct devicelist alldevs;		/* list of all devices */
struct evcntlist allevents;		/* list of all event counters */

/*
 * Initialize autoconfiguration data structures.  This occurs before console
 * initialization as that might require use of this subsystem.  Furthermore
 * this means that malloc et al. isn't yet available.
 */
void
config_init()
{

	TAILQ_INIT(&alldevs);
	TAILQ_INIT(&allevents);
	TAILQ_INIT(&allcftables);
	TAILQ_INSERT_TAIL(&allcftables, &staticcftable, list);
}

/*
 * Apply the matching function and choose the best.  This is used
 * a few times and we want to keep the code small.
 */
static void
mapply(m, cf)
	register struct matchinfo *m;
	register struct cfdata *cf;
{
	register int pri;
	void *match;

	if (m->indirect)
		match = config_make_softc(m->parent, cf);
	else
		match = cf;

	if (autoconf_verbose) {
		printf(">>> probing for %s", cf->cf_driver->cd_name);
		if (cf->cf_fstate == FSTATE_STAR)
			printf("*\n");
		else
			printf("%d\n", cf->cf_unit);
	}
	if (m->fn != NULL)
		pri = (*m->fn)(m->parent, match, m->aux);
	else {
	        if (cf->cf_attach->ca_match == NULL) {
			panic("mapply: no match function for '%s' device",
			    cf->cf_driver->cd_name);
		}
		pri = (*cf->cf_attach->ca_match)(m->parent, match, m->aux);
	}
	if (autoconf_verbose)
		printf(">>> %s probe returned %d\n", cf->cf_driver->cd_name,
		    pri);

	if (pri > m->pri) {
		if (m->indirect && m->match)
			free(m->match, M_DEVBUF);
		m->match = match;
		m->pri = pri;
	} else {
		if (m->indirect)
			free(match, M_DEVBUF);
	}
}

/*
 * Iterate over all potential children of some device, calling the given
 * function (default being the child's match function) for each one.
 * Nonzero returns are matches; the highest value returned is considered
 * the best match.  Return the `found child' if we got a match, or NULL
 * otherwise.  The `aux' pointer is simply passed on through.
 *
 * Note that this function is designed so that it can be used to apply
 * an arbitrary function to all potential children (its return value
 * can be ignored).
 */
void *
config_search(fn, parent, aux)
	cfmatch_t fn;
	register struct device *parent;
	void *aux;
{
	register struct cfdata *cf;
	register short *p;
	struct matchinfo m;
	struct cftable *t;

	m.fn = fn;
	m.parent = parent;
	m.match = NULL;
	m.aux = aux;
	m.indirect = parent && parent->dv_cfdata->cf_driver->cd_indirect;
	m.pri = 0;
	for(t = allcftables.tqh_first; t; t = t->list.tqe_next) {
		for (cf = t->tab; cf->cf_driver; cf++) {
			/*
			 * Skip cf if no longer eligible, otherwise scan
			 * through parents for one matching `parent',
			 * and try match function.
			 */
			if (cf->cf_fstate == FSTATE_FOUND)
				continue;
			if (cf->cf_fstate == FSTATE_DNOTFOUND ||
			    cf->cf_fstate == FSTATE_DSTAR)
				continue;
			for (p = cf->cf_parents; *p >= 0; p++)
				if (parent->dv_cfdata == &(t->tab)[*p])
					mapply(&m, cf);
		}
	}
	if (autoconf_verbose) {
		if (m.match)
			printf(">>> %s probe won\n",
			    ((struct cfdata *)m.match)->cf_driver->cd_name);
		else
			printf(">>> no winning probe\n");
	}
	return (m.match);
}

/*
 * Iterate over all potential children of some device, calling the given
 * function for each one.
 *
 * Note that this function is designed so that it can be used to apply
 * an arbitrary function to all potential children (its return value
 * can be ignored).
 */
void
config_scan(fn, parent)
	cfscan_t fn;
	register struct device *parent;
{
	register struct cfdata *cf;
	register short *p;
	void *match;
	int indirect;
	struct cftable *t;

	indirect = parent && parent->dv_cfdata->cf_driver->cd_indirect;
	for (t = allcftables.tqh_first; t; t = t->list.tqe_next) {
		for (cf = t->tab; cf->cf_driver; cf++) {
			/*
			 * Skip cf if no longer eligible, otherwise scan
			 * through parents for one matching `parent',
			 * and try match function.
			 */
			if (cf->cf_fstate == FSTATE_FOUND)
				continue;
			if (cf->cf_fstate == FSTATE_DNOTFOUND ||
			    cf->cf_fstate == FSTATE_DSTAR)
				continue;
			for (p = cf->cf_parents; *p >= 0; p++)
				if (parent->dv_cfdata == &(t->tab)[*p]) {
					match = indirect?
					    config_make_softc(parent, cf) :
					    (void *)cf;
					(*fn)(parent, match);
				}
		}
	}
}

/*
 * Find the given root device.
 * This is much like config_search, but there is no parent.
 */
void *
config_rootsearch(fn, rootname, aux)
	register cfmatch_t fn;
	register char *rootname;
	register void *aux;
{
	register struct cfdata *cf;
	register short *p;
	struct matchinfo m;

	m.fn = fn;
	m.parent = ROOT;
	m.match = NULL;
	m.aux = aux;
	m.indirect = 0;
	m.pri = 0;
	/*
	 * Look at root entries for matching name.  We do not bother
	 * with found-state here since only one root should ever be
	 * searched (and it must be done first).
	 */
	for (p = cfroots; *p >= 0; p++) {
		cf = &cfdata[*p];
		if (strcmp(cf->cf_driver->cd_name, rootname) == 0)
			mapply(&m, cf);
	}
	return (m.match);
}

char *msgs[3] = { "", " not configured\n", " unsupported\n" };

/*
 * The given `aux' argument describes a device that has been found
 * on the given parent, but not necessarily configured.  Locate the
 * configuration data for that device (using the submatch function
 * provided, or using candidates' cd_match configuration driver
 * functions) and attach it, and return true.  If the device was
 * not configured, call the given `print' function and return 0.
 */
struct device *
config_found_sm(parent, aux, print, submatch)
	struct device *parent;
	void *aux;
	cfprint_t print;
	cfmatch_t submatch;
{
	void *match;

	if ((match = config_search(submatch, parent, aux)) != NULL)
		return (config_attach(parent, match, aux, print));
	if (print)
		printf(msgs[(*print)(aux, parent->dv_xname)]);
	return (NULL);
}

/*
 * As above, but for root devices.
 */
struct device *
config_rootfound(rootname, aux)
	char *rootname;
	void *aux;
{
	void *match;

	if ((match = config_rootsearch((cfmatch_t)NULL, rootname, aux)) != NULL)
		return (config_attach(ROOT, match, aux, (cfprint_t)NULL));
	printf("root device %s not configured\n", rootname);
	return (NULL);
}

/* just like sprintf(buf, "%d") except that it works from the end */
static char *
number(ep, n)
	register char *ep;
	register int n;
{

	*--ep = 0;
	while (n >= 10) {
		*--ep = (n % 10) + '0';
		n /= 10;
	}
	*--ep = n + '0';
	return (ep);
}

/*
 * Attach a found device.  Allocates memory for device variables.
 */
struct device *
config_attach(parent, match, aux, print)
	register struct device *parent;
	void *match;
	register void *aux;
	cfprint_t print;
{
	register struct cfdata *cf;
	register struct device *dev;
	register struct cfdriver *cd;
	register struct cfattach *ca;
	struct cftable *t;

	if (parent && parent->dv_cfdata->cf_driver->cd_indirect) {
		dev = match;
		cf = dev->dv_cfdata;
	} else {
		cf = match;
		dev = config_make_softc(parent, cf);
	}

	cd = cf->cf_driver;
	ca = cf->cf_attach;

	cd->cd_devs[dev->dv_unit] = dev;

	/*
	 * If this is a "STAR" device and we used the last unit, prepare for
	 * another one.
	 */
	if (cf->cf_fstate == FSTATE_STAR) {
		if (dev->dv_unit == cf->cf_unit)
			cf->cf_unit++;
	} else
		cf->cf_fstate = FSTATE_FOUND;

	TAILQ_INSERT_TAIL(&alldevs, dev, dv_list);

	if (parent == ROOT)
		printf("%s (root)", dev->dv_xname);
	else {
		printf("%s at %s", dev->dv_xname, parent->dv_xname);
		if (print)
			(void) (*print)(aux, (char *)0);
	}

	/*
	 * Before attaching, clobber any unfound devices that are
	 * otherwise identical, or bump the unit number on all starred
	 * cfdata for this device.
	 */
	for (t = allcftables.tqh_first; t; t = t->list.tqe_next) {
		for (cf = t->tab; cf->cf_driver; cf++)
			if (cf->cf_driver == cd &&
			    cf->cf_unit == dev->dv_unit) {
				if (cf->cf_fstate == FSTATE_NOTFOUND)
					cf->cf_fstate = FSTATE_FOUND;
				if (cf->cf_fstate == FSTATE_STAR)
					cf->cf_unit++;
			}
	}
#if defined(__alpha__) || defined(hp300)
	device_register(dev, aux);
#endif
	(*ca->ca_attach)(parent, dev, aux);
	return (dev);
}

struct device *
config_make_softc(parent, cf)
	struct device *parent;
	struct cfdata *cf;
{
	register struct device *dev;
	register struct cfdriver *cd;
	register struct cfattach *ca;
	register size_t lname, lunit;
	register char *xunit;
	char num[10];

	cd = cf->cf_driver;
	ca = cf->cf_attach;
	if (ca->ca_devsize < sizeof(struct device))
		panic("config_make_softc");

	/* get memory for all device vars */
	dev = (struct device *)malloc(ca->ca_devsize, M_DEVBUF, M_NOWAIT);
	if (!dev)
		panic("config_make_softc: allocation for device softc failed");
	bzero(dev, ca->ca_devsize);
	dev->dv_class = cd->cd_class;
	dev->dv_cfdata = cf;

	/* If this is a STAR device, search for a free unit number */
	if (cf->cf_fstate == FSTATE_STAR) {
		for (dev->dv_unit = cf->cf_starunit1;
		    dev->dv_unit < cf->cf_unit; dev->dv_unit++)
			if (cd->cd_ndevs == 0 ||
			    cd->cd_devs[dev->dv_unit] == NULL)
				break;
	} else
		dev->dv_unit = cf->cf_unit;

	/* compute length of name and decimal expansion of unit number */
	lname = strlen(cd->cd_name);
	xunit = number(&num[sizeof num], dev->dv_unit);
	lunit = &num[sizeof num] - xunit;
	if (lname + lunit >= sizeof(dev->dv_xname))
		panic("config_make_softc: device name too long");

	bcopy(cd->cd_name, dev->dv_xname, lname);
	bcopy(xunit, dev->dv_xname + lname, lunit);
	dev->dv_parent = parent;

	/* put this device in the devices array */
	if (dev->dv_unit >= cd->cd_ndevs) {
		/*
		 * Need to expand the array.
		 */
		int old = cd->cd_ndevs, new;
		void **nsp;

		if (old == 0)
			new = MINALLOCSIZE / sizeof(void *);
		else
			new = old * 2;
		while (new <= dev->dv_unit)
			new *= 2;
		cd->cd_ndevs = new;
		nsp = malloc(new * sizeof(void *), M_DEVBUF, M_NOWAIT);	
		if (nsp == 0)
			panic("config_make_softc: %sing dev array",
			    old != 0 ? "expand" : "creat");
		bzero(nsp + old, (new - old) * sizeof(void *));
		if (old != 0) {
			bcopy(cd->cd_devs, nsp, old * sizeof(void *));
			free(cd->cd_devs, M_DEVBUF);
		}
		cd->cd_devs = nsp;
	}
	if (cd->cd_devs[dev->dv_unit])
		panic("config_make_softc: duplicate %s", dev->dv_xname);

	return (dev);
}

/*
 * Attach an event.  These must come from initially-zero space (see
 * commented-out assignments below), but that occurs naturally for
 * device instance variables.
 */
void
evcnt_attach(dev, name, ev)
	struct device *dev;
	const char *name;
	struct evcnt *ev;
{

#ifdef DIAGNOSTIC
	if (strlen(name) >= sizeof(ev->ev_name))
		panic("evcnt_attach");
#endif
	/* ev->ev_next = NULL; */
	ev->ev_dev = dev;
	/* ev->ev_count = 0; */
	strcpy(ev->ev_name, name);
	TAILQ_INSERT_TAIL(&allevents, ev, ev_list);
}

static int
haschild(dev)
	struct device *dev;
{
	struct device *d;

	for (d = alldevs.tqh_first; d != NULL; d = d->dv_list.tqe_next) {
		if (d->dv_parent == dev)
			return(1);
	}
	return(0);
}

int
detach_devices(cond, condarg, callback, arg)
	cond_predicate_t cond;
	void *condarg;
	config_detach_callback_t callback;
	void *arg;
{
	struct device *d;
	int alldone = 1;

	/*
	 * XXX should use circleq and run around the list backwards
	 * to allow for predicates to match children.
	 */
	d = alldevs.tqh_first;
	while (d != NULL) {
		if ((*cond)(d, condarg)) {
			struct cfdriver *drv = d->dv_cfdata->cf_driver;

			/* device not busy? */
			/* driver's detach routine decides, upper
			   layer (eg bus dependent code) is notified
			   via callback */
#ifdef DEBUG
			printf("trying to detach device %s (%p)\n",
			       d->dv_xname, d);
#endif
			if (!haschild(d) &&
			    d->dv_cfdata->cf_attach->ca_detach &&
			    ((*(d->dv_cfdata->cf_attach->ca_detach))(d)) == 0) {
				int needit, i;
				struct device *help;

				if (callback)
					(*callback)(d, arg);

				/* remove reference in driver's devicelist */
				if ((d->dv_unit >= drv->cd_ndevs) ||
				    (drv->cd_devs[d->dv_unit]!=d))
					panic("bad unit in detach_devices");
				drv->cd_devs[d->dv_unit] = NULL;

				/* driver is not needed anymore? */
				needit = 0;
				for(i = 0; i<drv->cd_ndevs; i++)
					if (drv->cd_devs[i])
						needit = 1;

				if (!needit) {
					/* free devices array (alloc'd
                                           in config_make_softc) */
					free(drv->cd_devs, M_DEVBUF);
					drv->cd_ndevs = 0;
				}

				/* remove entry in global device list */
				help = d->dv_list.tqe_next;
				TAILQ_REMOVE(&alldevs, d, dv_list);
#ifdef DEBUG
				printf("%s removed\n", d->dv_xname);
#endif
				if (d->dv_cfdata->cf_fstate == FSTATE_FOUND)
					d->dv_cfdata->cf_fstate =
					    FSTATE_NOTFOUND;
				/* free memory for dev data (alloc'd
                                   in config_make_softc) */
				free(d, M_DEVBUF);
				d = help;
				continue;
			} else
				alldone = 0;
		}
		d = d->dv_list.tqe_next;
	}
	return (!alldone);
}

int
dev_matches_cfdata(dev, arg)
	struct device *dev;
	void *arg;
{
	struct cfdata *cfdata = arg;
	return(/* device uses same driver ? */
		(dev->dv_cfdata->cf_driver == cfdata->cf_driver)
		/* device instance described by this cfdata? */
		&& ((cfdata->cf_fstate == FSTATE_STAR)
		    || ((cfdata->cf_fstate == FSTATE_FOUND)
		        && (dev->dv_unit == cfdata->cf_unit)))
		);
}

int
parentdev_matches_cfdata(dev, arg)
	struct device *dev;
	void *arg;
{
	return (dev->dv_parent ? dev_matches_cfdata(dev->dv_parent, arg) : 0);
}

int
config_detach(cf, callback, arg)
	struct cfdata *cf;
	config_detach_callback_t callback;
	void *arg;
{
	return (detach_devices(dev_matches_cfdata, cf, callback, arg));
}

int
config_detach_children(cf, callback, arg)
	struct cfdata *cf;
	config_detach_callback_t callback;
	void *arg;
{
	return (detach_devices(parentdev_matches_cfdata, cf, callback, arg));
}

int
attach_loadable(parentname, parentunit, cftable)
	char *parentname;
	int parentunit;
	struct cftable *cftable;
{
	int found = 0;
	struct device *d;

	TAILQ_INSERT_TAIL(&allcftables, cftable, list);

	for(d = alldevs.tqh_first; d != NULL; d = d->dv_list.tqe_next) {
		struct cfdriver *drv = d->dv_cfdata->cf_driver;

		if (strcmp(parentname, drv->cd_name) == NULL &&
		    (parentunit == -1 || parentunit == d->dv_unit)) {
			int s;

			s = splhigh(); /* ??? */
			found |= (*d->dv_cfdata->cf_attach->ca_reprobe)(d,
			    &(cftable->tab[0]));
			splx(s);
		}
	}
	if (!found)
		TAILQ_REMOVE(&allcftables, cftable, list);
	return(found);
}

static int
devcf_intable __P((struct device *, void *));

static int
devcf_intable(dev, arg)
	struct device *dev;
	void *arg;
{
	struct cftable *tbl = arg;
	struct cfdata *cf;

	for(cf = tbl->tab; cf->cf_driver; cf++) {
		if (dev->dv_cfdata == cf)
			return(1);
	}
	return(0);
}

int
detach_loadable(cftable)
	struct cftable *cftable;
{
	if (!detach_devices(devcf_intable, cftable, 0, 0))
		return(0);
	TAILQ_REMOVE(&allcftables, cftable, list);
	return(1);
}
