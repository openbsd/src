/*	$OpenBSD: diskprobe.c,v 1.1 1997/10/17 18:46:56 weingart Exp $	*/

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
#include <machine/biosvar.h>
#include "biosdev.h"
#include "libsa.h"


/* These get passed to kernel */
bios_diskinfo_t bios_diskinfo[16];


void
diskprobe()
{
	int drive, i = 0;

	printf("Probing disks:");

	/* Floppies */
	for(drive = 0; drive < 4; drive++){
		u_int32_t p = biosdinfo(drive);

		if(BIOSNSECTS(p) < 2) continue;
		if(p){
			u_int32_t t = biosdprobe(drive);
			if(t & 0x00FF) continue;
			if(!(t & 0xFF00)) continue;

			printf(" fd%d", drive);

			/* Fill out best we can */
			bios_diskinfo[i].bsd_major = 2;		/* fd? */
			bios_diskinfo[i].bios_number = drive;
			bios_diskinfo[i].bios_cylinders = BIOSNTRACKS(p);
			bios_diskinfo[i].bios_heads = BIOSNHEADS(p);
			bios_diskinfo[i].bios_sectors = BIOSNSECTS(p);

			i++;
		}
	}

	/* Hard disks */
	for(drive = 0x80; drive < 0x88; drive++){
		u_int32_t p = biosdinfo(drive);

		if(BIOSNSECTS(p) < 2) continue;
		if(p){
			u_int32_t t = biosdprobe(drive);
			if(t & 0x00FF) continue;
			if(!(t & 0xFF00)) continue;

			printf(" hd%d", drive - 128);

			/* Fill out best we can */
			bios_diskinfo[i].bsd_major = -1;		/* XXX - fill in */
			bios_diskinfo[i].bios_number = drive;
			bios_diskinfo[i].bios_cylinders = BIOSNTRACKS(p);
			bios_diskinfo[i].bios_heads = BIOSNHEADS(p);
			bios_diskinfo[i].bios_sectors = BIOSNSECTS(p);

			i++;
		}
	}

	/* End of list */
	bios_diskinfo[i].bios_number = -1;

	printf("\n");
}

