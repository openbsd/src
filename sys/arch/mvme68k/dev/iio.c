/* $Id: iio.c,v 1.1.1.1 1995/10/18 08:51:10 deraadt Exp $ */

/*
 *
 * Copyright (c) 1995 Charles D. Cranor
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
 *      This product includes software developed by Charles D. Cranor.
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
 * peripheral channel controller
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <dev/cons.h>
#include <mvme68k/mvme68k/isr.h>
#include <mvme68k/dev/iio.h>

/*
 * Configuration routines for the internal I/O bus
 */
void iioattach __P((struct device *, struct device *, void *));
int  iiomatch __P((struct device *, void *, void *));

struct iiosoftc {
	struct device	sc_dev;
};

struct cfdriver iiocd = {
	NULL, "iio", iiomatch, iioattach,
	DV_DULL, sizeof(struct iiosoftc), 0
};

int
iiomatch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	return (1);
}

void
iioattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	extern struct cfdata cfdata[];
	extern struct cfdriver pcccd;
	struct cfdata *cf, *pcccf = NULL;

	printf(" addr 0x%x\n", INTIOBASE);

	/*
	 * attach the pcc first!
	 */
	for (cf = cfdata; pcccf==NULL && cf->cf_driver; cf++) {
		if (cf->cf_driver != &pcccd)
			continue;
		pcccf = cf;
	}
	if (!pcccf)
		panic("no pcc device configured");
	config_attach(self, pcccf, NULL, NULL);

	while (config_found(self, NULL, NULL))
		;
}

void
iio_print(cf)
	struct cfdata *cf;
{
	printf(" offset 0x%x", cf->cf_loc[0]);
	if (cf->cf_loc[1] > 0)
		printf(" ipl %d", cf->cf_loc[1]);
}
