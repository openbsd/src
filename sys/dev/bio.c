/*	$OpenBSD: bio.c,v 1.4 2005/04/01 20:02:27 marco Exp $	*/

/*
 * Copyright (c) 2002 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/* A device controller ioctl tunnelling device.  */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <dev/biovar.h>

struct bio_mapping {
	LIST_ENTRY(bio_mapping) bm_link;
	struct device *bm_dev;
	int (*bm_ioctl)(struct device *, u_long, caddr_t);
};

LIST_HEAD(, bio_mapping) bios = LIST_HEAD_INITIALIZER(bios);

struct bio_softc {
	struct device	 sc_dev;
};

void	bioattach(int);
int	bioclose(dev_t, int, int, struct proc *);
int	bioioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	bioopen(dev_t, int, int, struct proc *);

int	bio_delegate_ioctl(struct bio_mapping *, u_long, caddr_t);
struct bio_mapping *bio_lookup(char *);
int	bio_validate(void *);

void
bioattach(nunits)
	int nunits;
{
}

int
bioopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	return (0);
}

int
bioclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	return (0);
}

int
bioioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	char name[16];
	size_t len;
	int error;
	struct bio_locate *locate;
	struct bio_common *common;

	switch (cmd) {
	case BIOCLOCATE:
		locate = (struct bio_locate *)addr;
		error = copyinstr(locate->name, name, 16, &len);
		if (error != 0)
			return (error);
		locate->cookie = bio_lookup(name);
		if (locate->cookie == NULL)
			return (ENOENT);
		break;

	default:
		common = (struct bio_common *)addr;
		if (!bio_validate(common->cookie))
			return (ENOENT);
		return (bio_delegate_ioctl(
		    (struct bio_mapping *)common->cookie, cmd, addr));
	}
	return (0);
}

int
bio_register(dev, ioctl)
	struct device *dev;
	int (*ioctl)(struct device *, u_long, caddr_t);
{
	struct bio_mapping *bm;

	MALLOC(bm, struct bio_mapping *, sizeof *bm, M_DEVBUF, M_NOWAIT);
	if (bm == NULL)
		return (ENOMEM);
	bm->bm_dev = dev;
	bm->bm_ioctl = ioctl;
	LIST_INSERT_HEAD(&bios, bm, bm_link);
	return (0);
}

struct bio_mapping *
bio_lookup(name)
	char *name;
{
	struct bio_mapping *bm;

	for (bm = LIST_FIRST(&bios); bm != NULL; bm = LIST_NEXT(bm, bm_link))
		if (strcmp(name, bm->bm_dev->dv_xname) == 0)
			return (bm);
	return (NULL);
}

int
bio_validate(cookie)
	void *cookie;
{
	struct bio_mapping *bm;

	for (bm = LIST_FIRST(&bios); bm != NULL; bm = LIST_NEXT(bm, bm_link))
		if (bm == cookie)
			return (1);
	return (0);
}

int
bio_delegate_ioctl(bm, cmd, addr)
	struct bio_mapping *bm;
	u_long cmd;
	caddr_t addr;
{
	return (bm->bm_ioctl(bm->bm_dev, cmd, addr));
}
