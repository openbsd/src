/*	$NetBSD: idprom.c,v 1.13 1996/11/20 18:56:50 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
	union {
		long l;
		char c[4];
	} hid;

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

	/*
	 * Construct the hostid from the idprom contents.
	 * This appears to be the way SunOS does it.
	 */
	hid.c[0] = idp->idp_machtype;
	hid.c[1] = idp->idp_serialnum[0];
	hid.c[2] = idp->idp_serialnum[1];
	hid.c[3] = idp->idp_serialnum[2];
	hostid = hid.l;

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
