/*	$OpenBSD: subr_autoconf.c,v 1.32 2002/10/06 23:12:31 art Exp $	*/
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
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/systm.h>
/* Extra stuff from Matthias Drochner <drochner@zelux6.zel.kfa-juelich.de> */
#include <sys/queue.h>

/*
 * Autoconfiguration subroutines.
 */

typedef int (*cond_predicate_t)(struct device *, void *);

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

static char *number(char *, int);
static void mapply(struct matchinfo *, struct cfdata *);

struct deferred_config {
	TAILQ_ENTRY(deferred_config) dc_queue;
	struct device *dc_dev;
	void (*dc_func)(struct device *);
};

TAILQ_HEAD(, deferred_config) deferred_config_queue;

void config_process_deferred_children(struct device *);

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
	TAILQ_INIT(&deferred_config_queue);
	TAILQ_INIT(&alldevs);
	TAILQ_INIT(&allevents);
	TAILQ_INIT(&allcftables);
	TAILQ_INSERT_TAIL(&allcftables, &staticcftable, list);
}

/*
 * Apply the matching function and choose the best.  This is used
 * a few times and we want to keep the code small.
 */
void
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
char *
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
	device_ref(dev);

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
#ifdef __HAVE_DEVICE_REGISTER
	device_register(dev, aux);
#endif
	(*ca->ca_attach)(parent, dev, aux);
	config_process_deferred_children(dev);
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
	dev->dv_flags = DVF_ACTIVE;	/* always initially active */

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

	dev->dv_ref = 1;

	return (dev);
}

/*
 * Detach a device.  Optionally forced (e.g. because of hardware
 * removal) and quiet.  Returns zero if successful, non-zero
 * (an error code) otherwise.
 *
 * Note that this code wants to be run from a process context, so
 * that the detach can sleep to allow processes which have a device
 * open to run and unwind their stacks.
 */
int
config_detach(dev, flags)
	struct device *dev;
	int flags;
{
	struct cfdata *cf;
	struct cfattach *ca;
	struct cfdriver *cd;
#ifdef DIAGNOSTIC
	struct device *d;
#endif
	int rv = 0, i;

	cf = dev->dv_cfdata;
#ifdef DIAGNOSTIC
	if (cf->cf_fstate != FSTATE_FOUND && cf->cf_fstate != FSTATE_STAR)
		panic("config_detach: bad device fstate");
#endif
	ca = cf->cf_attach;
	cd = cf->cf_driver;

	/*
	 * Ensure the device is deactivated.  If the device doesn't
	 * have an activation entry point, we allow DVF_ACTIVE to
	 * remain set.  Otherwise, if DVF_ACTIVE is still set, the
	 * device is busy, and the detach fails.
	 */
	if (ca->ca_activate != NULL)
		rv = config_deactivate(dev);

	/*
	 * Try to detach the device.  If that's not possible, then
	 * we either panic() (for the forced but failed case), or
	 * return an error.
	 */
	if (rv == 0) {
		if (ca->ca_detach != NULL)
			rv = (*ca->ca_detach)(dev, flags);
		else
			rv = EOPNOTSUPP;
	}
	if (rv != 0) {
		if ((flags & DETACH_FORCE) == 0)
			return (rv);
		else
			panic("config_detach: forced detach of %s failed (%d)",
			    dev->dv_xname, rv);
	}

	/*
	 * The device has now been successfully detached.
	 */

#ifdef DIAGNOSTIC
	/*
	 * Sanity: If you're successfully detached, you should have no
	 * children.  (Note that because children must be attached
	 * after parents, we only need to search the latter part of
	 * the list.)
	 */
	for (d = TAILQ_NEXT(dev, dv_list); d != NULL;
	     d = TAILQ_NEXT(d, dv_list)) {
		if (d->dv_parent == dev)
			panic("config_detach: detached device has children");
	}
#endif

	/*
	 * Mark cfdata to show that the unit can be reused, if possible.
	 * Note that we can only re-use a starred unit number if the unit
	 * being detached had the last assigned unit number.
	 */
	for (cf = cfdata; cf->cf_driver; cf++) {
		if (cf->cf_driver == cd) {
			if (cf->cf_fstate == FSTATE_FOUND &&
			    cf->cf_unit == dev->dv_unit)
				cf->cf_fstate = FSTATE_NOTFOUND;
			if (cf->cf_fstate == FSTATE_STAR &&
			    cf->cf_unit == dev->dv_unit + 1)
				cf->cf_unit--;
		}
	}

	/*
	 * Unlink from device list.
	 */
	TAILQ_REMOVE(&alldevs, dev, dv_list);
	device_unref(dev);

	/*
	 * Remove from cfdriver's array, tell the world, and free softc.
	 */
	cd->cd_devs[dev->dv_unit] = NULL;
	if ((flags & DETACH_QUIET) == 0)
		printf("%s detached\n", dev->dv_xname);

	device_unref(dev);
	/*
	 * If the device now has no units in use, deallocate its softc array.
	 */
	for (i = 0; i < cd->cd_ndevs; i++)
		if (cd->cd_devs[i] != NULL)
			break;
	if (i == cd->cd_ndevs) {		/* nothing found; deallocate */
		free(cd->cd_devs, M_DEVBUF);
		cd->cd_devs = NULL;
		cd->cd_ndevs = 0;
	}

	/*
	 * Return success.
	 */
	return (0);
}

int
config_activate(dev)
	struct device *dev;
{
	struct cfattach *ca = dev->dv_cfdata->cf_attach;
	int rv = 0, oflags = dev->dv_flags;

	if (ca->ca_activate == NULL)
		return (EOPNOTSUPP);

	if ((dev->dv_flags & DVF_ACTIVE) == 0) {
		dev->dv_flags |= DVF_ACTIVE;
		rv = (*ca->ca_activate)(dev, DVACT_ACTIVATE);
		if (rv)
			dev->dv_flags = oflags;
	}
	return (rv);
}

int
config_deactivate(dev)
	struct device *dev;
{
	struct cfattach *ca = dev->dv_cfdata->cf_attach;
	int rv = 0, oflags = dev->dv_flags;

	if (ca->ca_activate == NULL)
		return (EOPNOTSUPP);

	if (dev->dv_flags & DVF_ACTIVE) {
		dev->dv_flags &= ~DVF_ACTIVE;
		rv = (*ca->ca_activate)(dev, DVACT_DEACTIVATE);
		if (rv)
			dev->dv_flags = oflags;
	}
	return (rv);
}

/*
 * Defer the configuration of the specified device until all
 * of its parent's devices have been attached.
 */
void
config_defer(dev, func)
	struct device *dev;
	void (*func)(struct device *);
{
	struct deferred_config *dc;

	if (dev->dv_parent == NULL)
		panic("config_defer: can't defer config of a root device");

#ifdef DIAGNOSTIC
	for (dc = TAILQ_FIRST(&deferred_config_queue); dc != NULL;
	     dc = TAILQ_NEXT(dc, dc_queue)) {
		if (dc->dc_dev == dev)
			panic("config_defer: deferred twice");
	}
#endif

	if ((dc = malloc(sizeof(*dc), M_DEVBUF, M_NOWAIT)) == NULL)
		panic("config_defer: can't allocate defer structure");

	dc->dc_dev = dev;
	dc->dc_func = func;
	TAILQ_INSERT_TAIL(&deferred_config_queue, dc, dc_queue);
}

/*
 * Process the deferred configuration queue for a device.
 */
void
config_process_deferred_children(parent)
	struct device *parent;
{
	struct deferred_config *dc, *ndc;

	for (dc = TAILQ_FIRST(&deferred_config_queue);
	     dc != NULL; dc = ndc) {
		ndc = TAILQ_NEXT(dc, dc_queue);
		if (dc->dc_dev->dv_parent == parent) {
			TAILQ_REMOVE(&deferred_config_queue, dc, dc_queue);
			(*dc->dc_func)(dc->dc_dev);
			free(dc, M_DEVBUF);
		}
	}
}

int
config_detach_children(parent, flags)
	struct device *parent;
	int flags;
{
	struct device *dev, *next_dev;
	int  rv = 0;

	/* The config_detach routine may sleep, meaning devices
	   may be added to the queue. However, all devices will
	   be added to the tail of the queue, the queue won't
	   be re-organized, and the subtree of parent here should be locked
	   for purposes of adding/removing children.
	*/
	for (dev  = TAILQ_FIRST(&alldevs);
	     dev != NULL; dev = next_dev) {
		next_dev = TAILQ_NEXT(dev, dv_list);
		if (dev->dv_parent == parent &&
		    (rv = config_detach(dev, flags)))
			return (rv);
	}

	return  (rv);
}

int
config_activate_children(parent, act)
	struct device *parent;
	enum devact act;
{
	struct device *dev, *next_dev;
	int  rv = 0;

	/* The config_deactivate routine may sleep, meaning devices
	   may be added to the queue. However, all devices will
	   be added to the tail of the queue, the queue won't
	   be re-organized, and the subtree of parent here should be locked
	   for purposes of adding/removing children.
	*/
	for (dev = TAILQ_FIRST(&alldevs);
	     dev != NULL; dev = next_dev) {
		next_dev = TAILQ_NEXT(dev, dv_list);
		if (dev->dv_parent == parent) {
			switch (act) {
			case DVACT_ACTIVATE:
				rv = config_activate(dev);
				break;
			case DVACT_DEACTIVATE:
				rv = config_deactivate(dev);
				break;
			default:
#ifdef DIAGNOSTIC
				printf ("config_activate_children: shouldn't get here");
#endif
				rv = EOPNOTSUPP;
				break;

			}
						
			if (rv)
				break;
		}
	}

	return  (rv);
}

/* 
 * Lookup a device in the cfdriver device array.  Does not return a
 * device if it is not active.
 *
 * Increments ref count on the device by one, reflecting the
 * new reference created on the stack.
 *
 * Context: process only 
 */
struct device *
device_lookup(cd, unit)
	struct cfdriver *cd;
	int unit;
{
	struct device *dv = NULL;

	if (unit >= 0 && unit < cd->cd_ndevs)
		dv = (struct device *)(cd->cd_devs[unit]);
	
	if (!dv)
		return (NULL);

	if (!(dv->dv_flags & DVF_ACTIVE))
		dv = NULL;

	if (dv != NULL)
		device_ref(dv);

	return (dv);
}


/*
 * Increments the ref count on the device structure. The device
 * structure is freed when the ref count hits 0.
 *
 * Context: process or interrupt
 */
void
device_ref(dv)
	struct device *dv;
{
	dv->dv_ref++;
}

/*
 * Decrement the ref count on the device structure.
 *
 * free's the structure when the ref count hits zero and calls the zeroref
 * function.
 *
 * Context: process or interrupt
 */
void
device_unref(dv)
	struct device *dv;
{
	dv->dv_ref--;
	if (dv->dv_ref == 0) {
		if (dv->dv_cfdata->cf_attach->ca_zeroref)
			(*dv->dv_cfdata->cf_attach->ca_zeroref)(dv);
		
		free(dv, M_DEVBUF);
	}
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
