/*	$OpenBSD: copy.c,v 1.5 1998/05/11 07:36:25 niklas Exp $ */
/*	$NetBSD: copy.c,v 1.4 1997/02/12 18:00:42 ragge Exp $ */
/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	@(#)boot.c	7.15 (Berkeley) 5/4/91
 */

#include "sys/param.h"
#include "sys/reboot.h"
#include "lib/libsa/stand.h"

#include "vaxstand.h"

#include <sys/exec.h>

char line[100];
volatile u_int devtype, bootdev;
extern	unsigned opendev;

static char 	*progname 	= "copy";
static char 	*iobuf 		= NULL;
static char	*bufp		= NULL;
static int 	bufsize 	= 0;
static int  	partlist[8];

int fill_buffer (void);
int write_disk (void);

Xmain()
{
	int adapt, ctlr, unit, part;
	int res, i, loops;
	char line[64];

	autoconf ();

	printf ("\n");
	printf ("%s: \n", progname);
	printf ("This program will read miniroot from tape/floppy/disk \n");
	printf ("and install this miniroot onto disk.\n");
	printf ("\n");

	res = fill_buffer ();
	if (res < 0) {
		printf ("errors occured during read. Continue at your own risk.\n");
		printf ("Do you want to continue ? [y/n] ");
		gets (line);
		if (*line != 'y' && *line != 'Y') {
			printf ("bye.\n");
			return (-1);
		}
	}

	printf ("\n");
	res = write_disk ();

	printf ("\n");
	printf ("Halt/Reboot the machine NOW.\n");
	for (;;)
		;
	/* NOTREACHED */
}

int 
fill_buffer (void)
{
	char 	devname[64];
	int	numblocks;
	int	blocksize = 512;
	int	bpv = 0;		/* blocks per volume */
	int 	bpt = 8;		/* blocks per transfer */
	struct open_file file;
	char 	*filename;
	int	i, loops;
	int	size, rsize;
	int	res, errors = 0;

again:
	printf("\n");
	printf("Specify the device to read from as xx(N,?), where\n");
	printf("xx is the device-name, ? is file/partition number\n");
	printf("and N is the unit-number, e.g.\n");
	printf("\"mt(0,1)\" for the first TMSCP-tape (TK50),\n");
	printf("\"ra(2,0)\" for the third MSCP-disk/floppy (RX33/RX50)\n");
	printf("\n");
	printf("device to read from ? ");
	gets(devname);

	printf("\n");
	printf("Specify number of blocks to transfer. Usually this is\n");
	printf("sizeof(miniroot) / 512.\n");
	printf("It's safe to transfer more blocks than just the miniroot.\n");
	printf("\n");
	while (1) {
		printf ("number of blocks ? ");
		gets (line);
		if (atoi(line) > 0) {
			if (iobuf && bufsize)
				free (iobuf, bufsize);
			numblocks = atoi (line);
			bufsize = 512 * numblocks;
			iobuf = alloc (bufsize);
			bufp = iobuf;
			if (iobuf == NULL) {
				printf ("cannot allocate this much memory.\n");
				continue;
			}
			break;
		}
		printf ("invalid number %d.\n", atoi(line));
	}

	printf ("\n");
	printf ("If your miniroot is split into volumes, then you must\n");
	printf ("specify the number of blocks per volume.\n");
	printf ("(e.g. 800 blocks per RX50, 2400 blocks per RX33)\n");
	printf ("\n");
	while (1) {
		printf ("number of blocks per volume ? [%d] ", numblocks);
		gets (line);
		if (!*line || atoi(line) > 0) {
			bpv = (atoi(line) > 0 ? atoi(line) : numblocks);
			break;
		}
		printf ("invalid number %d.\n", atoi(line));
	}

	printf ("\n");
	do {
		printf ("Make sure unit %s is online and holds the proper volume.\n", devname);
		printf ("Then type \'g\' to Go or \'a\' to Abort.\n");
		printf ("\n");
		printf ("OK to go on ? [g/a] ");
		gets (line);
		if (*line == 'g' || *line == 'G') {
			printf ("Reading ... ");
			if (devopen (&file, devname, &filename)) {
				printf ("cannot open unit %s.\n", devname);
				goto again;
			}
			loops = bpv / bpt + (bpv % bpt != 0);
			for (i=0; i<loops; i++) {
				twiddle ();
				size = 512 * min (bpt, bpv - (i*bpt));
				res = (*file.f_dev->dv_strategy)
					(file.f_devdata, F_READ,
					(daddr_t)i*bpt, size, bufp, &rsize);
				if (res != 0) {
					printf ("error %d in read.\n", res);
					errors++;
					/* continue ? halt ??? */
				}
				bufp += size;
			}
			numblocks -= bpv;
		}
		if (numblocks > 0) {
			int vn = ((bufp - iobuf) / 512) / bpv;
			printf ("\n");
			printf ("volume #%d done. Now insert volume #%d\n",
				vn - 1, vn);
		}
	} while (numblocks > 0);
	printf ("Reading of miniroot done. (%d blocks read)\n", 
		(bufp - iobuf) / 512);

	return (-errors);
}

int 
write_disk (void) 
{
	char line[64];
	char devname[64];
	struct open_file file;
	char *fname;
	int rsize, res;
	int i, errors = 0;

	printf ("\n");
	printf ("Now specify the device to write miniroot to as xx(N,1)\n");
	printf ("where xx is the drive type and N is the drive number.\n");
	printf ("For example: ra(0,1) refers to MSCP drive #0, b partition\n");
	printf ("\n");
	do {
		printf ("Root disk ? : ");
		gets (devname);
	} while (devopen (&file, devname, &fname));

	/*
	 * next: initialize the partition
	 */
	printf ("Initializing partition ... ");
	bufp = iobuf + (16 * 512);
	for (i=16; i<bufsize/512; i++) {
		twiddle ();
		res = (*file.f_dev->dv_strategy) (file.f_devdata, F_WRITE,
			(daddr_t)i, 512, bufp, &rsize);
		if (res != 0) {
			errors++;
			printf ("error writing block %d.\n");
			printf ("trying to continue ...\n");
		}
		bufp += 512;
	}
	printf ("done.\n");
	printf ("(%d blocks written.)\n", bufsize/512);

	printf ("\n");
	printf ("Halt the machine and reboot from distribution media,\n");
	printf ("giving second partition as part to mount as root. Ex:\n");
	printf (": ra(0,1) for ra disk 0, hp(2,1) for massbuss disk 2\n");
	
	return (-errors);
}

