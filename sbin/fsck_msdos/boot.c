/*	$OpenBSD: boot.c,v 1.2 1996/06/23 14:30:41 deraadt Exp $	*/
/*	$NetBSD: boot.c,v 1.1 1996/05/14 17:39:28 ws Exp $	*/

/*
 * Copyright (C) 1995 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
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
 *	This product includes software developed by Martin Husemann
 *	and Wolfgang Solfrank.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef lint
static char rcsid[] = "$OpenBSD: boot.c,v 1.2 1996/06/23 14:30:41 deraadt Exp $";
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#include "ext.h"

int
readboot(dosfs, boot)
	int dosfs;
	struct bootblock *boot;
{
	u_char block[DOSBOOTBLOCKSIZE];
	int n;
	
	if ((n = read(dosfs, block, sizeof block)) < (int)sizeof block) {
		if (n < 0)
			perror("could not read boot block");
		else
			pfatal("Short bootblock?");
		return FSFATAL;
	}

	/* decode bios parameter block */
	boot->BytesPerSec = block[11] + (block[12] << 8);
	boot->SecPerClust = block[13];
	boot->ResSectors = block[14] + (block[15] << 8);
	boot->FATs = block[16];
	boot->RootDirEnts = block[17] + (block[18] << 8);
	boot->Sectors = block[19] + (block[20] << 8);
	boot->Media = block[21];
	boot->FATsecs = block[22] + (block[23] << 8);
	boot->SecPerTrack = block[24] + (block[25] << 8);
	boot->Heads = block[26] + (block[27] << 8);
	boot->HiddenSecs = block[28] + (block[29] << 8) + (block[30] << 16) + (block[31] << 24);
	boot->HugeSectors = block[32] + (block[33] << 8) + (block[34] << 16) + (block[35] << 24);
	boot->ClusterOffset = (boot->RootDirEnts * 32 + boot->BytesPerSec - 1)
	    / boot->BytesPerSec
	    + boot->ResSectors
	    + boot->FATs * boot->FATsecs
	    - CLUST_FIRST * boot->SecPerClust;

	if (boot->BytesPerSec % DOSBOOTBLOCKSIZE != 0) {
		pfatal("Invalid sector size: %u\n", boot->BytesPerSec);
		return FSFATAL;
	}
	if (boot->SecPerClust == 0) {
		pfatal("Invalid cluster size: %u\n", boot->SecPerClust);
		return FSFATAL;
	}
	if (boot->Sectors) {
		boot->HugeSectors = 0;
		boot->NumSectors = boot->Sectors;
	} else
		boot->NumSectors = boot->HugeSectors;
	boot->NumClusters = (boot->NumSectors - boot->ClusterOffset) / boot->SecPerClust;
	if (boot->NumClusters >= MAX12BITCLUSTERS)
		boot->Is16BitFat = 1;
	else
		boot->Is16BitFat = 0;

	if (boot->Is16BitFat)
		boot->NumFatEntries = (boot->FATsecs * boot->BytesPerSec) / 2;
	else
		boot->NumFatEntries = (boot->FATsecs * boot->BytesPerSec * 2) / 3;
	if (boot->NumFatEntries < boot->NumClusters) {
		pfatal("FAT size too small, %d entries won't fit into %u sectors\n", 
		       boot->NumClusters, boot->FATsecs);
		return FSFATAL;
	}
	boot->ClusterSize = boot->BytesPerSec * boot->SecPerClust;

	boot->NumFiles = 1;
	boot->NumFree = 0;
	
	return FSOK;
}
