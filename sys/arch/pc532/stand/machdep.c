/*	$NetBSD: machdep.c,v 1.2 1994/10/26 08:25:49 cgd Exp $	*/

/* 
 * Copyright (c) 1994 Philip L. Budne.
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
 *	This product includes software developed by Philip L. Budne.
 * 4. The name of Philip L. Budne may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pc532 standalone machdep code
 * Phil Budne, May 10, 1994
 *
 */

#include <sys/types.h>
#include "samachdep.h"

int testing = 0;

void
bzero( char *addr, int len )
{
    while (len-- > 0)
	*addr++ = '\0';
}

/* XXX TEMP; would like to use code more like hp300 scsi.c */

void
scsiinit(void)
{
}

int
scsialive(int ctlr)
{
	return 1;		/* controller always alive! */
}

/* call functions in scsi_hi.c */
#include "so.h"

int
scsi_tt_read(ctlr, slave, buf, len, blk, nblk)
	int ctlr, slave;
	u_char *buf;
	u_int len;
	daddr_t blk;
	u_int nblk;
{
#if 0
printf("scsi_tt_read ctlr %d, slave %d, len %d, blk %d, nblk %d\n",
	ctlr, slave, len, blk, nblk );
#endif
	if (sc_rdwt(DISK_READ, blk, buf, nblk, 1<<slave, 0) == 0)
		return 0;
	return -2;
}

int
scsi_tt_write(ctlr, slave, buf, len, blk, nblk)
	int ctlr, slave;
	u_char *buf;
	u_int len;
	daddr_t blk;
	u_int nblk;
{
#if 0
	if (sc_rdwt(DISK_WRITE, blk, buf, nblk, 1<<slave, 0) == 0)
		return 0;
#endif
	return -2;
}
