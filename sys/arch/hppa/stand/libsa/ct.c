/*	$OpenBSD: ct.c,v 1.1.1.1 1998/06/23 18:46:42 mickey Exp $	*/
/*	$NOWHERE: ct.c,v 2.2 1998/06/22 18:41:34 mickey Exp $	*/

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

/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */

#include "libsa.h"

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/reboot.h>
#include <machine/pdc.h>
#include <machine/iodc.h>
#include <machine/iomod.h>

#include "dev_hppa.h"

int (*ctiodc)();	/* cartridge tape IODC entry point */
int ctcode[IODC_MAXSIZE/sizeof(int)];

/* hp800-specific comments:
 *
 * Tape driver ALWAYS uses "Alternate Boot Device", which is assumed to ALWAYS
 * be the boot device in pagezero (meaning we booted from it).
 *
 * NOTE about skipping file, below:  It's assumed that a read gets 2k (a page).
 * This is done because, even though the cartridge tape has record sizes of 1k,
 * and an EOF takes one record, reads through the IODC must be in 2k chunks,
 * and must start on a 2k-byte boundary.  This means that ANY TAPE FILE TO BE
 * SKIPPED OVER IS GOING TO HAVE TO BE AN ODD NUMBER OF 1 KBYTE RECORDS so the
 * read of the subsequent file can start on a 2k boundary.  If a real error
 * occurs, the record count is reset below, so this isn't a problem.
 */
int	ctbyteno;	/* block number on tape to access next */
int	ctworking;	/* flag:  have we read anything successfully? */

int
#ifdef __STDC__
ctopen(struct open_file *f, ...)
#else
ctopen(f)
	struct open_file *f;
#endif
{
	struct hppa_dev *dp = f->f_devdata;
	int i, ret, part = B_PARTITION(dp->bootdev);

	if (ctiodc == 0) {

		if ((ret = (*pdc)(PDC_IODC, PDC_IODC_READ, pdcbuf, ctdev.pz_hpa,
				  IODC_IO, ctcode, IODC_MAXSIZE)) < 0) {
			printf("ct: device ENTRY_IO Read ret'd %d\n", ret);
			return (EIO);
		} else
			ctdev.pz_iodc_io = ctiodc = (int (*)()) ctcode;
	}

	if (ctiodc != NULL)
		if ((ret = (*ctiodc)(ctdev.pz_hpa, IODC_IO_BOOTIN, ctdev.pz_spa,
				     ctdev.pz_layers, pdcbuf,0, btbuf,0,0)) < 0)
			printf("ct: device rewind ret'd %d\n", ret);

	ctbyteno = 0;
	for (i = part; --i >= 0; ) {
		ctworking = 0;
		for (;;) {
			ret = iodc_rw(btbuf, ctbyteno, IONBPG, F_READ, &ctdev);
			ctbyteno += IONBPG;
			if (ret <= 0)
				break;
			ctworking = 1;
		}
		if (ret < 0 && (ret != -4 || !ctworking)) {
			printf("ct: error %d after %d %d-byte records\n",
				ret, ctbyteno >> IOPGSHIFT, IONBPG);
			ctbyteno = 0;
			ctworking = 0;
			return (EIO);
		}
	}
	ctworking = 0;
	return (0);
}

/*ARGSUSED*/
int
ctclose(f)
	struct open_file *f;
{
	ctbyteno = 0;
	ctworking = 0;

	return 0;
}

int
ctstrategy(devdata, rw, dblk, size, buf, rsize)
	void *devdata;
	int rw;
	daddr_t dblk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	int ret;

	if ((ret = iodc_rw(buf, ctbyteno, size, rw, &ctdev)) < 0) {
		if (ret == -4 && ctworking)
			ret = 0;

		ctworking = 0;
	} else {
		ctworking = 1;
		ctbyteno += ret;
	}

	return (ret);
}
