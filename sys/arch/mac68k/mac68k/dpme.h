/*	$OpenBSD: dpme.h,v 1.7 2008/03/09 12:03:03 sobrado Exp $	*/
/*	$NetBSD: dpme.h,v 1.8 1997/11/30 04:46:59 briggs Exp $	*/

/*
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>

/*
 * Partition map structure from Inside Macintosh V-579.
 */
struct partmapentry {
	u_int16_t       pmSig;
	u_int16_t       pmSigPad;
	u_int32_t       pmMapBlkCnt;
	u_int32_t       pmPyPartStart;
	u_int32_t       pmPartBlkCnt;
	u_int8_t        pmPartName[32];
	u_int8_t        pmPartType[32];
	u_int32_t       pmLgDataStart;
	u_int32_t       pmDataCnt;
	u_int32_t       pmPartStatus;
	u_int32_t       pmLgBootStart;
	u_int32_t       pmBootSize;
	u_int32_t       pmBootLoad;
	u_int32_t       pmBootLoad2;
	u_int32_t       pmBootEntry;
	u_int32_t       pmBootEntry2;
	u_int32_t       pmBootCksum;
	int8_t          pmProcessor[16];
	u_int8_t        pmBootArgs[128];
	u_int8_t        blockpadding[248];
};

/*
 * Disk Partition Map Entry Magic number.  Valid entries have this
 * in the pmSig field.
 */
#define DPME_MAGIC	0x504d

/*
 * "pmBootArgs" for APPLE_UNIX_SVR2 partition.
 * OpenBSD/mac68k only uses Magic, Cluster, Type, and Flags.
 */
struct blockzeroblock {
	u_int32_t       bzbMagic;
	u_int8_t        bzbCluster;
	u_int8_t        bzbType;
	u_int16_t       bzbBadBlockInode;
	u_int16_t       bzbFlags;
	u_int16_t       bzbReserved;
	u_int32_t       bzbCreationTime;
	u_int32_t       bzbMountTime;
	u_int32_t       bzbUMountTime;
};

#define BZB_MAGIC	0xABADBABE
#define BZB_TYPEFS	1
#define BZB_TYPESWAP	3
#define BZB_ROOTFS	0x8000
#define BZB_USRFS	0x4000
#define BZB_EXFS4	0x4
#define BZB_EXFS5	0x5
#define BZB_EXFS6	0x6

/* MF */
#define PART_UNIX_TYPE		"APPLE_UNIX_SVR2"
#define PART_MAC_TYPE		"APPLE_HFS"
#define PART_SCRATCH		"APPLE_SCRATCH"
#define PART_DRIVER_TYPE	"APPLE_DRIVER"
#define PART_DRIVER43_TYPE	"APPLE_DRIVER43"
#define PART_DRIVERATA_TYPE	"APPLE_DRIVER_ATA"
#define PART_FWB_COMPONENT_TYPE	"FWB DRIVER COMPONENTS"
#define PART_PARTMAP_TYPE	"APPLE_PARTITION_MAP"
