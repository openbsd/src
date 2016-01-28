/*	$OpenBSD: file_media.c,v 1.44 2016/01/28 13:01:33 krw Exp $	*/

/*
 * file_media.c -
 *
 * Written by Eryk Vershen
 */

/*
 * Copyright 1997,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>		/* DEV_BSIZE */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dpme.h"
#include "file_media.h"

struct ddmap_ondisk {
    uint8_t	ddBlock[4];
    uint8_t	ddSize[2];
    uint8_t	ddType[2];
};

struct block0_ondisk {
    uint8_t	sbSig[2];
    uint8_t	sbBlkSize[2];
    uint8_t	sbBlkCount[4];
    uint8_t	sbDevType[2];
    uint8_t	sbDevId[2];
    uint8_t	sbData[4];
    uint8_t	sbDrvrCount[2];
    uint8_t	sbDDMap[64];	/* ddmap_ondisk[8] */
    uint8_t	reserved[430];
};

struct dpme_ondisk {
    uint8_t	dpme_signature[2];
    uint8_t	dpme_reserved_1[2];
    uint8_t	dpme_map_entries[4];
    uint8_t	dpme_pblock_start[4];
    uint8_t	dpme_pblocks[4];
    uint8_t	dpme_name[DPISTRLEN];
    uint8_t	dpme_type[DPISTRLEN];
    uint8_t	dpme_lblock_start[4];
    uint8_t	dpme_lblocks[4];
    uint8_t	dpme_flags[4];
    uint8_t	dpme_boot_block[4];
    uint8_t	dpme_boot_bytes[4];
    uint8_t	dpme_load_addr[4];
    uint8_t	dpme_reserved_2[4];
    uint8_t	dpme_goto_addr[4];
    uint8_t	dpme_reserved_3[4];
    uint8_t	dpme_checksum[4];
    uint8_t	dpme_processor_id[16];
    uint8_t	dpme_reserved_4[376];
};

static int	read_block(int, uint64_t, void *);
static int	write_block(int, uint64_t, void *);

static int
read_block(int fd, uint64_t sector, void *address)
{
	ssize_t off;

	off = pread(fd, address, DEV_BSIZE, sector * DEV_BSIZE);
	if (off == DEV_BSIZE)
		return 1;

	if (off == 0)
		fprintf(stderr, "end of file encountered");
	else if (off == -1)
		warn("reading file failed");
	else
		fprintf(stderr, "short read");

	return 0;
}

static int
write_block(int fd, uint64_t sector, void *address)
{
	ssize_t off;

	off = pwrite(fd, address, DEV_BSIZE, sector * DEV_BSIZE);
	if (off == DEV_BSIZE)
		return 1;

	warn("writing to file failed");
	return 0;
}

int
read_block0(int fd, struct block0 *block0)
{
	struct block0_ondisk *block0_ondisk;
	struct ddmap_ondisk ddmap_ondisk;
	int i;

	block0_ondisk = malloc(sizeof(struct block0_ondisk));
	if (block0_ondisk == NULL)
		return 0;

	if (read_block(fd, 0, block0_ondisk) == 0)
		return 0;

	memcpy(&block0->sbSig, block0_ondisk->sbSig,
	    sizeof(block0->sbSig));
	block0->sbSig = betoh16(block0->sbSig);
	memcpy(&block0->sbBlkSize, block0_ondisk->sbBlkSize,
	    sizeof(block0->sbBlkSize));
	block0->sbBlkSize = betoh16(block0->sbBlkSize);
	memcpy(&block0->sbBlkCount, block0_ondisk->sbBlkCount,
	    sizeof(block0->sbBlkCount));
	block0->sbBlkCount = betoh32(block0->sbBlkCount);
	memcpy(&block0->sbDevType, block0_ondisk->sbDevType,
	    sizeof(block0->sbDevType));
	block0->sbDevType = betoh16(block0->sbDevType);
	memcpy(&block0->sbDevId, block0_ondisk->sbDevId,
	    sizeof(block0->sbDevId));
	block0->sbDevId = betoh16(block0->sbDevId);
	memcpy(&block0->sbData, block0_ondisk->sbData,
	    sizeof(block0->sbData));
	block0->sbData = betoh32(block0->sbData);
	memcpy(&block0->sbDrvrCount, block0_ondisk->sbDrvrCount,
	    sizeof(block0->sbDrvrCount));
	block0->sbDrvrCount = betoh16(block0->sbDrvrCount);

	for (i = 0; i < 8; i++) {
		memcpy(&ddmap_ondisk,
		    block0->sbDDMap+i*sizeof(struct ddmap_ondisk),
		    sizeof(ddmap_ondisk));
		memcpy(&block0->sbDDMap[i].ddBlock, &ddmap_ondisk.ddBlock,
		    sizeof(block0->sbDDMap[i].ddBlock));
		block0->sbDDMap[i].ddBlock =
		    betoh32(block0->sbDDMap[i].ddBlock);
		memcpy(&block0->sbDDMap[i].ddSize, &ddmap_ondisk.ddSize,
		    sizeof(block0->sbDDMap[i].ddSize));
		block0->sbDDMap[i].ddSize = betoh16(block0->sbDDMap[i].ddSize);
		memcpy(&block0->sbDDMap[i].ddType, &ddmap_ondisk.ddType,
		    sizeof(block0->sbDDMap[i].ddType));
		block0->sbDDMap[i].ddType = betoh32(block0->sbDDMap[i].ddType);
	}

	free(block0_ondisk);
	return 1;
}

int
write_block0(int fd, struct block0 *block0)
{
	struct block0_ondisk *block0_ondisk;
	struct ddmap_ondisk ddmap_ondisk;
	int i, rslt;
	uint32_t tmp32;
	uint16_t tmp16;

	block0_ondisk = malloc(sizeof(struct block0_ondisk));
	if (block0_ondisk == NULL)
		return 0;

	tmp16 = htobe16(block0->sbSig);
	memcpy(block0_ondisk->sbSig, &tmp16,
	    sizeof(block0_ondisk->sbSig));
	tmp16 = htobe16(block0->sbBlkSize);
	memcpy(block0_ondisk->sbBlkSize, &tmp16,
	    sizeof(block0_ondisk->sbBlkSize));
	tmp32 = htobe32(block0->sbBlkCount);
	memcpy(block0_ondisk->sbBlkCount, &tmp32,
	    sizeof(block0_ondisk->sbBlkCount));
	tmp16 = htobe16(block0->sbDevType);
	memcpy(block0_ondisk->sbDevType, &tmp16,
	    sizeof(block0_ondisk->sbDevType));
	tmp16 = htobe16(block0->sbDevId);
	memcpy(block0_ondisk->sbDevId, &tmp16,
	    sizeof(block0_ondisk->sbDevId));
	tmp32 = htobe32(block0->sbData);
	memcpy(block0_ondisk->sbData, &tmp32,
	    sizeof(block0_ondisk->sbData));
	tmp16 = htobe16(block0->sbDrvrCount);
	memcpy(block0_ondisk->sbDrvrCount, &tmp16,
	    sizeof(block0_ondisk->sbDrvrCount));

	for (i = 0; i < 8; i++) {
		tmp32 = htobe32(block0->sbDDMap[i].ddBlock);
		memcpy(ddmap_ondisk.ddBlock, &tmp32,
		    sizeof(ddmap_ondisk.ddBlock));
		tmp16 = htobe16(block0->sbDDMap[i].ddSize);
		memcpy(&ddmap_ondisk.ddSize, &tmp16,
		    sizeof(ddmap_ondisk.ddSize));
		tmp16 = betoh32(block0->sbDDMap[i].ddType);
		memcpy(&ddmap_ondisk.ddType, &tmp16,
		    sizeof(ddmap_ondisk.ddType));
		memcpy(block0->sbDDMap+i*sizeof(struct ddmap_ondisk),
		    &ddmap_ondisk, sizeof(ddmap_ondisk));
	}

	rslt = write_block(fd, 0, block0_ondisk);
	free(block0_ondisk);
	return rslt;
}

int
read_dpme(int fd, uint64_t sector, struct dpme *dpme)
{
	struct dpme_ondisk *dpme_ondisk;

	dpme_ondisk = malloc(sizeof(struct dpme_ondisk));
	if (dpme_ondisk == NULL)
		return 0;

	if (read_block(fd, sector, dpme_ondisk) == 0)
		return 0;

	memcpy(&dpme->dpme_signature, dpme_ondisk->dpme_signature,
	    sizeof(dpme->dpme_signature));
	memcpy(&dpme->dpme_map_entries, dpme_ondisk->dpme_map_entries,
	    sizeof(dpme->dpme_map_entries));
	memcpy(&dpme->dpme_pblock_start, dpme_ondisk->dpme_pblock_start,
	    sizeof(dpme->dpme_pblock_start));
	memcpy(&dpme->dpme_pblocks, dpme_ondisk->dpme_pblocks,
	    sizeof(dpme->dpme_pblocks));
	memcpy(&dpme->dpme_lblock_start, dpme_ondisk->dpme_lblock_start,
	    sizeof(dpme->dpme_lblock_start));
	memcpy(&dpme->dpme_lblocks, dpme_ondisk->dpme_lblocks,
	    sizeof(dpme->dpme_lblocks));
	memcpy(&dpme->dpme_flags, dpme_ondisk->dpme_flags,
	    sizeof(dpme->dpme_flags));
	memcpy(&dpme->dpme_boot_block, dpme_ondisk->dpme_boot_block,
	    sizeof(dpme->dpme_boot_block));
	memcpy(&dpme->dpme_boot_bytes, dpme_ondisk->dpme_boot_bytes,
	    sizeof(dpme->dpme_boot_bytes));
	memcpy(&dpme->dpme_load_addr, dpme_ondisk->dpme_load_addr,
	    sizeof(dpme->dpme_load_addr));
	memcpy(&dpme->dpme_goto_addr, dpme_ondisk->dpme_goto_addr,
	    sizeof(dpme->dpme_goto_addr));
	memcpy(&dpme->dpme_checksum, dpme_ondisk->dpme_checksum,
	    sizeof(dpme->dpme_checksum));

	dpme->dpme_signature = betoh16(dpme->dpme_signature);
	dpme->dpme_map_entries = betoh32(dpme->dpme_map_entries);
	dpme->dpme_pblock_start = betoh32(dpme->dpme_pblock_start);
	dpme->dpme_pblocks = betoh32(dpme->dpme_pblocks);
	dpme->dpme_lblock_start = betoh32(dpme->dpme_lblock_start);
	dpme->dpme_lblocks = betoh32(dpme->dpme_lblocks);
	dpme->dpme_flags = betoh32(dpme->dpme_flags);
	dpme->dpme_boot_block = betoh32(dpme->dpme_boot_block);
	dpme->dpme_boot_bytes = betoh32(dpme->dpme_boot_bytes);
	dpme->dpme_load_addr = betoh32(dpme->dpme_load_addr);
	dpme->dpme_goto_addr = betoh32(dpme->dpme_goto_addr);
	dpme->dpme_checksum = betoh32(dpme->dpme_checksum);

	memcpy(dpme->dpme_reserved_1, dpme_ondisk->dpme_reserved_1,
	    sizeof(dpme->dpme_reserved_1));
	memcpy(dpme->dpme_reserved_2, dpme_ondisk->dpme_reserved_2,
	    sizeof(dpme->dpme_reserved_2));
	memcpy(dpme->dpme_reserved_3, dpme_ondisk->dpme_reserved_3,
	    sizeof(dpme->dpme_reserved_3));
	memcpy(dpme->dpme_reserved_4, dpme_ondisk->dpme_reserved_4,
	    sizeof(dpme->dpme_reserved_4));

	strlcpy(dpme->dpme_name, dpme_ondisk->dpme_name,
	    sizeof(dpme->dpme_name));
	strlcpy(dpme->dpme_type, dpme_ondisk->dpme_type,
	    sizeof(dpme->dpme_type));
	strlcpy(dpme->dpme_processor_id, dpme_ondisk->dpme_processor_id,
	    sizeof(dpme->dpme_processor_id));

	free(dpme_ondisk);
	return 1;
}

int
write_dpme(int fd, uint64_t sector, struct dpme *dpme)
{
	struct dpme_ondisk *dpme_ondisk;
	int rslt;
	uint32_t tmp32;
	uint16_t tmp16;

	dpme_ondisk = malloc(sizeof(struct dpme_ondisk));
	if (dpme_ondisk == NULL)
		return 0;

	memcpy(dpme_ondisk->dpme_name, dpme->dpme_name,
	    sizeof(dpme_ondisk->dpme_name));
	memcpy(dpme_ondisk->dpme_type, dpme->dpme_type,
	    sizeof(dpme_ondisk->dpme_type));
	memcpy(dpme_ondisk->dpme_processor_id, dpme->dpme_processor_id,
	    sizeof(dpme_ondisk->dpme_processor_id));

	memcpy(dpme_ondisk->dpme_reserved_1, dpme->dpme_reserved_1,
	    sizeof(dpme_ondisk->dpme_reserved_1));
	memcpy(dpme_ondisk->dpme_reserved_2, dpme->dpme_reserved_2,
	    sizeof(dpme_ondisk->dpme_reserved_2));
	memcpy(dpme_ondisk->dpme_reserved_3, dpme->dpme_reserved_3,
	    sizeof(dpme_ondisk->dpme_reserved_3));
	memcpy(dpme_ondisk->dpme_reserved_4, dpme->dpme_reserved_4,
	    sizeof(dpme_ondisk->dpme_reserved_4));

	tmp16 = htobe16(dpme->dpme_signature);
	memcpy(dpme_ondisk->dpme_signature, &tmp16,
	    sizeof(dpme_ondisk->dpme_signature));
	tmp32 = htobe32(dpme->dpme_map_entries);
	memcpy(dpme_ondisk->dpme_map_entries, &tmp32,
	    sizeof(dpme_ondisk->dpme_map_entries));
	tmp32 = htobe32(dpme->dpme_pblock_start);
	memcpy(dpme_ondisk->dpme_pblock_start, &tmp32,
	    sizeof(dpme_ondisk->dpme_pblock_start));
	tmp32 = htobe32(dpme->dpme_pblocks);
	memcpy(dpme_ondisk->dpme_pblocks, &tmp32,
	    sizeof(dpme_ondisk->dpme_pblocks));
	tmp32 = htobe32(dpme->dpme_lblock_start);
	memcpy(dpme_ondisk->dpme_lblock_start, &tmp32,
	    sizeof(dpme_ondisk->dpme_lblock_start));
	tmp32 = betoh32(dpme->dpme_lblocks);
	memcpy(dpme_ondisk->dpme_lblocks, &tmp32,
	    sizeof(dpme_ondisk->dpme_lblocks));
	tmp32 = betoh32(dpme->dpme_flags);
	memcpy(dpme_ondisk->dpme_flags, &tmp32,
	    sizeof(dpme_ondisk->dpme_flags));
	tmp32 = htobe32(dpme->dpme_boot_block);
	memcpy(dpme_ondisk->dpme_boot_block, &tmp32,
	    sizeof(dpme_ondisk->dpme_boot_block));
	tmp32 = htobe32(dpme->dpme_boot_bytes);
	memcpy(dpme_ondisk->dpme_boot_bytes, &tmp32,
	    sizeof(dpme_ondisk->dpme_boot_bytes));
	tmp32 = betoh32(dpme->dpme_load_addr);
	memcpy(dpme_ondisk->dpme_load_addr, &tmp32,
	    sizeof(dpme_ondisk->dpme_load_addr));
	tmp32 = betoh32(dpme->dpme_goto_addr);
	memcpy(dpme_ondisk->dpme_goto_addr, &tmp32,
	    sizeof(dpme_ondisk->dpme_goto_addr));
	tmp32 = betoh32(dpme->dpme_checksum);
	memcpy(dpme_ondisk->dpme_checksum, &tmp32,
	    sizeof(dpme_ondisk->dpme_checksum));

	rslt = write_block(fd, sector, dpme_ondisk);
	free(dpme_ondisk);
	return rslt;
}
