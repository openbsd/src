/*	$OpenBSD: diskprobe.c,v 1.3 1997/10/22 23:34:38 mickey Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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
 *	This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
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

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <stand/boot/bootarg.h>
#include <machine/biosvar.h>
#include "biosdev.h"
#include "libsa.h"

/* These get passed to kernel */
bios_diskinfo_t bios_diskinfo[16];

#if notyet
/* Checksum given buffer */
static u_int32_t
bufsum(buf, len)
	void *buf;
	int len;
{
	u_int32_t sum = 0;
	u_int8_t *p = buf;

	while(len--){
		sum += p[len];
	}
	return(sum);
}

/* Checksum given drive until different */
void
disksum(pos)
	int pos;
{
	u_int32_t sum;
	int len, i;
}
#endif

/* Probe for all BIOS disks */
void
diskprobe()
{
	u_int drive, i = 0, rv;
	struct disklabel label;
	u_int unit, type;

	printf("Probing disks:");

	/* Floppies */
	for(drive = 0; drive < 4; drive++) {
		rv = bios_getinfo(drive, &bios_diskinfo[i]);

		if( (rv & 0x00FF)) continue;
		if(!(rv & 0xFF00)) continue;

		printf(" fd%u", drive);

		/* Fill out best we can - (fd?) */
		bios_diskinfo[i].bsd_dev = MAKEBOOTDEV(2, 0, 0, drive, 0);
#if 0
		disksum(&bios_diskinfo[i]);
#endif
		i++;
	}

	/* Hard disks */
	for(drive = 0x80; drive < 0x88; drive++) {
		rv = bios_getinfo(drive, &bios_diskinfo[i]);

		if( (rv & 0x00FF)) continue;
		if(!(rv & 0xFF00)) continue;

		unit = drive - 0x80;
		printf(" hd%u%s", unit, (bios_diskinfo[i].bios_edd > 0?"+":""));

		/* Try to find the label, to figure out device type */
		if((bios_getdisklabel(drive, &label)) ) {
			printf("*");
			type = 0;	/* XXX let it be IDE */
		} else {
			/* Best guess */
			if (label.d_type == DTYPE_SCSI)
				type = 4;
			else
				type = 0;
		}

		/* Fill out best we can */
		bios_diskinfo[i].bsd_dev = MAKEBOOTDEV(type, 0, 0, unit, 0);
#if 0
		disksum(&bios_diskinfo[i]);
#endif
		i++;
	}

	/* End of list */
	bios_diskinfo[i].bios_number = -1;
	addbootarg(BOOTARG_DISKINFO,
		   (i + 1) * sizeof(bios_diskinfo[0]), bios_diskinfo);

	printf("\n");
}

/* Find info on given BIOS disk */
bios_diskinfo_t *
bios_dklookup(dev)
	register int dev;
{
	register int i;

	for(i = 0; bios_diskinfo[i].bios_number != -1; i++)
		if(bios_diskinfo[i].bios_number == dev)
			return(&bios_diskinfo[i]);

	return(NULL);
}
