/*	$OpenBSD: uk.c,v 1.15 2010/07/01 05:11:18 krw Exp $	*/
/*	$NetBSD: uk.c,v 1.15 1996/03/17 00:59:57 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 * Dummy driver for a device we can't identify.
 * Originally by Julian Elischer (julian@tfs.com)
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#define	UKUNIT(z)	(minor(z))

struct uk_softc {
	struct device		sc_dev;
	struct scsi_link	*sc_link; /* all the inter level info */
};

int	ukmatch(struct device *, void *, void *);
void	ukattach(struct device *, struct device *, void *);

struct cfattach uk_ca = {
	sizeof(struct uk_softc), ukmatch, ukattach
};

struct cfdriver uk_cd = {
	NULL, "uk", DV_DULL
};

int
ukmatch(struct device *parent, void *match, void *aux)
{
	return (1);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
void
ukattach(struct device *parent, struct device *self, void *aux)
{
	struct uk_softc			*uk = (void *)self;
	struct scsi_attach_args		*sa = aux;
	struct scsi_link		*sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("ukattach: "));

	/* Store information needed to contact our base driver */
	uk->sc_link = sc_link;
	sc_link->device_softc = uk;
	sc_link->openings = 1;

	printf("\n");
}

/*
 * open the device.
 */
int
ukopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	int				unit;
	struct uk_softc			*uk;
	struct scsi_link		*sc_link;

	unit = UKUNIT(dev);
	if (unit >= uk_cd.cd_ndevs)
		return (ENXIO);

	uk = uk_cd.cd_devs[unit];
	if (uk == NULL)
		return (ENXIO);

	sc_link = uk->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("ukopen: dev=0x%x (unit %d (of %d))\n",
	    dev, unit, uk_cd.cd_ndevs));

	/* Only allow one at a time */
	if (sc_link->flags & SDEV_OPEN)
		return (EBUSY);

	sc_link->flags |= SDEV_OPEN;

	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));

	return (0);
}

/*
 * close the device.. only called if we are the LAST
 * occurrence of an open device
 */
int
ukclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct uk_softc			*uk = uk_cd.cd_devs[UKUNIT(dev)];

	SC_DEBUG(uk->sc_link, SDEV_DB1, ("closing\n"));
	uk->sc_link->flags &= ~SDEV_OPEN;

	return (0);
}

/*
 * Perform special action on behalf of the user
 * Only does generic scsi ioctls.
 */
int
ukioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct uk_softc			*uk = uk_cd.cd_devs[UKUNIT(dev)];

	return (scsi_do_ioctl(uk->sc_link, cmd, addr, flag));
}
