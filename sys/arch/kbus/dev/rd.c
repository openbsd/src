/*	$OpenBSD: rd.c,v 1.2 2000/03/03 00:54:49 todd Exp $	*/
/*
 * Copyright (c) 1995 Philip A. Nelson.
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
 *	This product includes software developed by Philip A. Nelson.
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/disk.h>

static int rdmatch(struct device *parent, void *cf, void *aux);
static void rdattach(struct device *parent, struct device *self, void *aux);

struct rdsoftc {
	struct	device sc_dev;		/* generic device glue */
	struct	disk sc_dkdev;		/* generic disk glue */
};

struct cfdriver rdcd = {
	NULL,
	"rd",
	rdmatch,
	rdattach,
	DV_DISK,
	sizeof(struct rdsoftc),
	NULL,
	0
};

void rdstrategy __P((struct buf *));

struct dkdriver rddkdriver = { rdstrategy };

#if !defined(RD_SIZE)
#  define RD_SIZE	0x200000
#endif

u_char ram_disk[RD_SIZE] = "Ramdiskorigin";

static int
rdmatch(parent, cf, aux)
	struct device	*parent;
	void		*cf, *aux;
{
	return(((struct cfdata *)cf)->cf_unit == 0);
}

static void
rdattach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
{
	struct rdsoftc *sc = (struct rdsoftc *)self;

	printf(" addr 0x%x, size 0x%x\n", ram_disk, RD_SIZE);

	/*
	 * Initialize and attach the disk structure.
	 */
	bzero(&sc->sc_dkdev, sizeof(sc->sc_dkdev));
	sc->sc_dkdev.dk_driver = &rddkdriver;
	sc->sc_dkdev.dk_name = sc->sc_dev.dv_xname;
	disk_attach(&sc->sc_dkdev);
}


/* operational routines */

int
rdopen(dev, flags, devtype, p)
	dev_t		dev;
	int		flags, devtype;
	struct proc	*p;
{
	if (minor(dev) == 0)
		  return(0);
	else
		  return(ENXIO);
}

int
rdclose(dev, flags, devtype, p)
	dev_t		dev;
	int		flags, devtype;
	struct proc	*p;
{
	return(0);
}

int
rdioctl(dev, cmd, addr, flag, p)
	dev_t		dev;
	u_long		cmd;
	int		flag;
	caddr_t 	addr;
	struct proc	*p;
{
	return(ENOTTY);
}

int
rdsize(dev)
	dev_t dev;
{
	if (minor(dev) == 0)
		return(RD_SIZE / DEV_BSIZE);
	else
		return(0);
}

int
rddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	return(ENXIO);
}

void
rdstrategy(bp)
	struct buf *bp;
{
	int loc, size;
	char *adr;

	if (minor(bp->b_dev) == 0)
		loc = bp->b_blkno * DEV_BSIZE;
	else {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		return;
	}
	size = bp->b_bcount;
	adr = (char *) bp->b_un.b_addr;
	if (loc + size > sizeof(ram_disk)) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		return;
	}
	if (bp->b_flags & B_READ)
	      bcopy(&ram_disk[loc], adr, size);
	else
	      bcopy(adr, &ram_disk[loc], size);
	biodone(bp);
}

int
rdread(dev, uio)
	dev_t		dev;
	struct uio	*uio;
{
	return(physio(rdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
rdwrite(dev, uio)
	dev_t		dev;
	struct uio	*uio;
{
	return(physio(rdstrategy, NULL, dev, B_WRITE, minphys, uio));
}
