/*	$OpenBSD: wdvar.h,v 1.2 2000/04/10 07:06:16 csapuntz Exp $	*/
/*	$NetBSD: wdvar.h,v 1.3 1998/11/11 19:38:27 bouyer Exp $	*/

/*
 * Copyright (c) 1998 Manuel Bouyer.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* Params needed by the controller to perform an ATA bio */
struct ata_bio {
    volatile u_int16_t flags; /* cmd flags */
#define ATA_NOSLEEP 0x0001 /* Can't sleep */   
#define ATA_POLL    0x0002 /* poll for completion */
#define ATA_ITSDONE 0x0004 /* the transfer is as done as it gets */
#define ATA_SINGLE  0x0008 /* transfer has to be done in single-sector mode */
#define ATA_LBA     0x0010 /* tranfert uses LBA adressing */
#define ATA_READ    0x0020 /* tranfert is a read (otherwise a write) */
#define ATA_CORR    0x0040 /* transfer had a corrected error */
    int multi; /* number of blocks to transfer in multi-mode */
    struct disklabel *lp; /* pointer to drive's label info */
    daddr_t blkno; /* block addr */
    daddr_t blkdone; /* number of blks transfered */
    daddr_t nblks; /* number of block currently transfering */
    int     nbytes; /* number of bytes currently transfering */
    long    bcount; /* total number of bytes */
    char*   databuf; /* data buffer adress */
    volatile int error;
#define NOERROR 0 /* There was no error (r_error invalid) */
#define ERROR   1 /* check r_error */
#define ERR_DF	2 /* Drive fault */
#define ERR_DMA 3 /* DMA error */
#define TIMEOUT 4 /* device timed out */
#define ERR_NODEV 5 /* device bas been detached */
    u_int8_t r_error; /* copy of error register */
    daddr_t badsect[127];    /* 126 plus trailing -1 marker */
    struct wd_softc *wd;
};

/* drive states stored in ata_drive_datas */
#define RECAL          0
#define RECAL_WAIT     1
#define PIOMODE	       2
#define PIOMODE_WAIT   3
#define DMAMODE        4
#define DMAMODE_WAIT   5
#define GEOMETRY       6
#define GEOMETRY_WAIT  7
#define MULTIMODE      8
#define MULTIMODE_WAIT 9
#define READY          10

int wdc_ata_bio __P((struct ata_drive_datas*, struct ata_bio*)); 

void wddone __P((void *));
