/*	$NetBSD: idprom.c,v 1.10 1995/02/11 20:57:11 gwr Exp $	*/

/*
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine ID PROM - system type and serial number
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/control.h>
#include <machine/idprom.h>
#include <machine/mon.h>

extern long hostid;	/* in kern_sysctl.c */

/*
 * This structure is what this driver is all about.
 * It is copied from control space early in startup.
 */
struct idprom identity_prom;

static int  idprom_match __P((struct device *, void *vcf, void *args));
static void idprom_attach __P((struct device *, struct device *, void *));

struct cfdriver idpromcd = {
	NULL, "idprom", idprom_match, idprom_attach,
	DV_DULL, sizeof(struct device), 0 };

int idprom_match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;

	/* This driver only supports one unit. */
	if (cf->cf_unit != 0)
		return (0);

	return (1);
}

void idprom_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct idprom *idp;
	union {
		long l;
		char c[4];
	} id;

	/*
	 * Construct the hostid from the idprom contents.
	 * This appears to be the way SunOS does it.
	 */
	idp = &identity_prom;
	id.c[0] = idp->idp_machtype;
	id.c[1] = idp->idp_serialnum[0];
	id.c[2] = idp->idp_serialnum[1];
	id.c[3] = idp->idp_serialnum[2];
	hostid = id.l;

	printf(" hostid 0x%x\n", id.l);
}

int idpromopen(dev, oflags, devtype, p)
	dev_t dev;
	int oflags;
	int devtype;
	struct proc *p;
{
	return 0;
}

int idpromclose(dev, fflag, devtype, p)
	dev_t dev;
	int fflag;
	int devtype;
	struct proc *p;
{
	return 0;
}

idpromread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int error, unit, length;

	error = 0;
	while (uio->uio_resid > 0 && error == 0) {
		if (uio->uio_offset >= IDPROM_SIZE)
			break; /* past or at end */
		length = min(uio->uio_resid,
					 (IDPROM_SIZE - (int)uio->uio_offset));
		error = uiomove((caddr_t) &identity_prom, length, uio);
	}
	return error;
}

/*
 * This is called very early during startup to
 * get a copy of the idprom from control space.
 */
int idprom_init()
{
	struct idprom *idp;
	char *src, *dst;
	int len, x, xorsum;

	idp = &identity_prom;
	dst = (char*)idp;
	src = (char*)IDPROM_BASE;
	len = IDPROM_SIZE;
	xorsum = 0;	/* calculated as xor of data */

	do {
		x = get_control_byte(src++);
		*dst++ = x;
		xorsum ^= x;
	} while (--len > 0);

	if (xorsum != 0) {
		mon_printf("idprom_fetch: bad checksum=%d\n", xorsum);
		return xorsum;
	}
	if (idp->idp_format < 1) {
		mon_printf("idprom_fetch: bad version=%d\n", idp->idp_format);
		return -1;
	}
	return 0;
}

void idprom_etheraddr(eaddrp)
	u_char *eaddrp;
{
	u_char *src, *dst;
	int len = 6;

	src = identity_prom.idp_etheraddr;
	dst = eaddrp;

	do *dst++ = *src++;
	while (--len > 0);
}
