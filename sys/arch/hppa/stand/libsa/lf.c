/*	$OpenBSD: lf.c,v 1.1 1998/07/30 15:59:32 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
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

#include "libsa.h"
#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/param.h>

#include "dev_hppa.h"

int
#ifdef __STDC__
lfopen(struct open_file *f, ...)
#else
lfopen(f, va_alist)
	struct open_file *f;
#endif
{
	extern dev_t bootdev;
	struct hppa_dev *dp;
	struct pz_device *pzd;

	if (!(pzd = pdc_findev(-1, PCL_NET_MASK|PCL_SEQU)))
		return ENXIO;

	if (!(dp = alloc(sizeof(struct hppa_dev)))) {
#ifdef	DEBUG
		printf("lfopen: no mem\n");
#endif
		return ENODEV;
	}

	bzero (dp, sizeof (struct hppa_dev));
	dp->pz_dev = pzd;
	dp->bootdev = bootdev;
	dp->last_blk = 0;
	dp->last_read = 0;
	f->f_devdata = dp;

	return 0;
}

int
lfclose(f)
	struct open_file *f;
{
	free(f->f_devdata, sizeof(struct hppa_dev));
	f->f_devdata = NULL;
	return 0;
}

