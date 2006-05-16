/*	$OpenBSD: netdev.c,v 1.2 2006/05/16 22:52:09 miod Exp $ */

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
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
 */

#include <sys/param.h>
#include <machine/prom.h>
#include <string.h>

#include "stand.h"
#include "tftpfs.h"

#include "libsa.h"

struct bugdev_softc {
	short	clun;
	short	dlun;
} bugdev_softc[1];

int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	struct bugdev_softc *pp = &bugdev_softc[0];

	pp->clun = (short)bugargs.ctrl_lun;
	pp->dlun = (short)bugargs.dev_lun;

	f->f_devdata = (void *)pp;
	f->f_dev = &devsw[0];
	*file = (char *)fname;
	return (0);
}

#define	NFR_TIMEOUT	5

int
net_strategy(devdata, func, nblk, size, buf, rsize)
	void *devdata;
	int func;
	daddr_t nblk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	struct bugdev_softc *pp = (struct bugdev_softc *)devdata;
	struct mvmeprom_netfread nfr;
	int attempts;

	for (attempts = 0; attempts < 10; attempts++) {
		nfr.ctrl = pp->clun;
		nfr.dev = pp->dlun;
		nfr.status = 0;
		nfr.addr = (u_long)buf;
		nfr.bytes = 0;
		nfr.blk = nblk;
		nfr.timeout = NFR_TIMEOUT;
		mvmeprom_netfread(&nfr);
	
		if (rsize) {
			*rsize = nfr.bytes;
		}

		if (nfr.status == 0)
			return (0);
	}

	return (EIO);
}

int
net_open(struct open_file *f, ...)
{
	va_list ap;
	struct mvmeprom_netfopen nfo;
	struct bugdev_softc *pp = (struct bugdev_softc *)f->f_devdata;
	char *filename;

	va_start(ap, f);
	filename = va_arg(ap, char *);
	va_end(ap);

	nfo.ctrl = pp->clun;
	nfo.dev = pp->dlun;
	nfo.status = 0;
	strlcpy(nfo.filename, filename, sizeof nfo.filename);
	mvmeprom_netfopen(&nfo);
	
#ifdef DEBUG
	printf("tftp open(%s): error %x\n", filename, nfo.status);
#endif
	return (nfo.status);
}

int
net_close(f)
	struct open_file *f;
{
	return (0);
}

int
net_ioctl(f, cmd, data)
	struct open_file *f;
	u_long cmd;
	void *data;
{
	return (EIO);
}
