/*	$OpenBSD: cmd.c,v 1.2 1997/09/29 23:33:32 mickey Exp $	*/

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
 *    This product includes software developed by Tobias Weingartner.
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

#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include "disk.h"
#include "misc.h"
#include "user.h"
#include "part.h"
#include "cmd.h"


int
Xinit(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	char buf[DEV_BSIZE];

	/* Copy MBR */
	MBR_make(tt, buf);
	MBR_parse(buf, mbr);

	/* Tell em we did something */
	printf("In memory copy is initialized.\n");
	printf("Use 'write' to update disk.\n");

	return(CMD_DIRTY);
}

int
Xdisk(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{

	/* Print out disk info */
	DISK_printmetrics(disk);

	/* Ask for new info */
	if(ask_yn("Change geometry?")){
		disk->bios->cylinders = ask_num("Cylinders", ASK_DEC, 1, 1, 1024);
		disk->bios->heads = ask_num("Heads", ASK_DEC, 1, 1, 256);
		disk->bios->sectors = ask_num("Sectors", ASK_DEC, 1, 1, 64);

		disk->bios->size = disk->bios->cylinders * disk->bios->heads
			* disk->bios->sectors;
	}

	return(CMD_CONT);
}

int
Xedit(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int num;
	prt_t *pp;

	if(!isdigit(cmd->args[0])){
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return(CMD_CONT);
	}
	num = atoi(cmd->args);

	if(num < 0 || num > 3){
		printf("Invalid partition number.\n");
		return(CMD_CONT);
	}

	/* Print out current table entry */
	pp = &mbr->part[num];
	PRT_print(0, NULL);
	PRT_print(num, pp);

	/* Ask for partition type */
	pp->id = ask_num("Partition id", ASK_HEX, pp->id, 0, 0xFF);

	/* Unused, so just zero out */
	if(pp->id == DOSPTYP_UNUSED){
		memset(pp, 0, sizeof(*pp));
		printf("Partiton %d cleared.\n", num);
		return(CMD_DIRTY);
	}

	/* Change table entry */
	if(ask_yn("Do you wish to edit in CHS mode?")){
		int maxcyl, maxhead, maxsect;

		/* Shorter */
		maxcyl = disk->bios->cylinders - 1;
		maxhead = disk->bios->heads - 1;
		maxsect = disk->bios->sectors;

		/* Get data */
		pp->scyl = ask_num("Starting cylinder", ASK_DEC, pp->scyl, 0, maxcyl);
		pp->shead = ask_num("Starting head", ASK_DEC, pp->shead, 0, maxhead);
		pp->ssect = ask_num("Starting sector", ASK_DEC, pp->ssect, 1, maxsect);
		pp->ecyl = ask_num("Ending cylinder", ASK_DEC, pp->ecyl, 0, maxcyl);
		pp->ehead = ask_num("Ending head", ASK_DEC, pp->ehead, 0, maxhead);
		pp->esect = ask_num("Ending sector", ASK_DEC, pp->esect, 1, maxsect);

		/* Fix up off/size values */
		PRT_fix_BN(disk, pp);
	}else{
		int start, size;


		/* Get data */
		start = disk->bios->size;
		pp->bs = ask_num("Partition offset", ASK_DEC, pp->bs, 0, start);

		size = disk->bios->size - pp->bs;
		pp->ns = ask_num("Partition size", ASK_DEC, pp->ns, 1, size);

		/* Fix up CHS values */
		PRT_fix_CHS(disk, pp);
	}

	return(CMD_DIRTY);
}

int
Xselect(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int off;
	int num;

	if(!isdigit(cmd->args[0])){
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return(CMD_CONT);
	}

	num = atoi(cmd->args);
	off = mbr->part[num].bs;

	/* Sanity checks */
	if(mbr->part[num].id != DOSPTYP_EXTEND){
		printf("Partition %d is not an extended partition.\n", num);
		return(CMD_CONT);
	}
	if(!off){
		printf("Loop to offset 0!  Not selected.\n");
		return(CMD_CONT);
	}else{
		printf("Selected extended partition %d\n", num);
		printf("New MBR at offset %d.\n", off);
	}

	/* Recursion is beautifull! */
	USER_modify(disk, tt, off);

	return(CMD_CONT);
}

int
Xprint(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{

	MBR_print(mbr);

	return(CMD_CONT);
}

int
Xwrite(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	char mbr_buf[DEV_BSIZE];
	int fd;

	if(ask_yn("Are you sure?")){
		printf("Writing MBR at offset %d.\n", offset);

		fd = DISK_open(disk->name, O_RDWR);
		MBR_make(mbr, mbr_buf);
		MBR_write(fd, offset, mbr_buf);
		close(fd);

		return(CMD_CLEAN);
	}

	return(CMD_CONT);
}

int
Xexit(cmd, disk, r, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *r;
	mbr_t *tt;
	int offset;
{

	/* Nothing to do here */
	return(CMD_EXIT);
}

int
Xquit(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{

	if(ask_yn("You really want to quit?"))
		exit(0);

	return(CMD_CONT);
}

int
Xhelp(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	cmd_table_t *cmd_table = cmd->table;
	int i;

	/* Hmm, print out cmd_table here... */
	for(i = 0; cmd_table[i].cmd != NULL; i++){
		printf("\t%s\t\t%s\n", cmd_table[i].cmd, cmd_table[i].help);
	}

	return(CMD_CONT);
}

int
Xupdate(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{

	/* Update code */
	memcpy(mbr->code, tt->code, MBR_CODE_SIZE);

	printf("Machine code updated.\n");

	return(CMD_DIRTY);
}

int
Xflag(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int i, num = -1;

	/* Parse partition table entry number */
	if(!isdigit(cmd->args[0])){
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return(CMD_CONT);
	}
	num = atoi(cmd->args);

	if(num < 0 || num > 3){
		printf("Invalid partition number.\n");
		return(CMD_CONT);
	}

	/* Set active flag */
	for(i = 0; i < 4; i++){
		if(i == num) mbr->part[i].flag = DOSACTIVE;
		else mbr->part[i].flag = 0x00;
	}

	printf("Partition %d marked active.\n", num);

	return(CMD_DIRTY);
}

