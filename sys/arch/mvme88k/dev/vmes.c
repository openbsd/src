/*	$OpenBSD: vmes.c,v 1.16 2003/12/28 19:44:26 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>

#include <mvme88k/dev/vme.h>

/*
 * The VMES driver deals with D16 transfers on the VME bus. The number
 * of address bits (A16, A24, A32) is irrelevant since the mapping
 * functions will decide how many address bits are relevant.
 */

void vmesattach(struct device *, struct device *, void *);
int  vmesmatch(struct device *, void *, void *);

struct cfattach vmes_ca = {
        sizeof(struct device), vmesmatch, vmesattach
};

struct cfdriver vmes_cd = {
        NULL, "vmes", DV_DULL, 0
};

int vmesscan(struct device *, void *, void *);

int
vmesmatch(parent, cf, args)
	struct device *parent;
	void *cf, *args;
{
	return (1);
}

int
vmesscan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	return (vmescan(parent, child, args, BUS_VMES));
}

void
vmesattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	printf("\n");

	config_search(vmesscan, self, args);
}

/*ARGSUSED*/
int
vmesopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	if (minor(dev) >= vmes_cd.cd_ndevs ||
	    vmes_cd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);
	return (0);
}

/*ARGSUSED*/
int
vmesclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
}

/*ARGSUSED*/
int
vmesioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error = 0;

	switch (cmd) {
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

int
vmesread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct device *sc = (struct device *)vmes_cd.cd_devs[unit];

	return (vmerw(sc->dv_parent, uio, flags, BUS_VMES));
}

int
vmeswrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct device *sc = (struct device *)vmes_cd.cd_devs[unit];

	return (vmerw(sc->dv_parent, uio, flags, BUS_VMES));
}

paddr_t
vmesmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	int unit = minor(dev);
	struct device *sc = (struct device *)vmes_cd.cd_devs[unit];
	void * pa;

	pa = vmepmap(sc->dv_parent, off, NBPG, BUS_VMES);
#ifdef DEBUG
	printf("vmes %llx pa %p\n", off, pa);
#endif
	if (pa == NULL)
		return (-1);
	return (atop(pa));
}

/*
 * Specific D16 access functions
 *
 * D16 cards will trigger bus errors on attempting to read or write more
 * than 16 bits on the bus. Given how the m88k processor works, this means
 * basically that all long (D32) accesses must be carefully taken care of.
 *
 * Since the kernels bcopy() and bzero() routines will use 32 bit accesses
 * for performance, here are specific D16-compatible routines. They expect
 * pointers to be 16-bit aligned.
 */

void
d16_bcopy(const void *src, void *dst, size_t len)
{
	const u_int16_t *s = (const u_int16_t *)src;
	u_int16_t *d = (u_int16_t *)dst;
	size_t l = len;

	l >>= 1;
	while (l-- != 0)
		*d++ = *s++;
	if (len & 1)
		*(u_int8_t *)d = *(u_int8_t *)s;
}

void
d16_bzero(void *dst, size_t len)
{
	u_int16_t *d = (u_int16_t *)dst;
	size_t l = len;

	l >>= 1;
	while (l-- != 0)
		*d++ = 0;
	if (len & 1)
		*(u_int8_t *)d = 0;
}
