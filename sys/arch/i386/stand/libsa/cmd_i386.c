/*	$OpenBSD: cmd_i386.c,v 1.13 1997/10/20 14:56:09 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff, Tobias Weingartner
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
#include <sys/disklabel.h>
#include <machine/biosvar.h>
#include "debug.h"
#include "biosdev.h"
#include "libsa.h"
#include <cmd.h>

static int Xdiskinfo __P((void));
static int Xregs __P((void));
static int Xboot __P((void));
static int Xmemory __P((void));

/* From gidt.S */
int bootbuf __P((int, int));

/* From probedisk.c */
extern bios_diskinfo_t bios_diskinfo[];

/* From probemem.c */
extern bios_memmap_t *memory_map;

const struct cmd_table cmd_machine[] = {
	{ "diskinfo", CMDT_CMD, Xdiskinfo },
	{ "regs",     CMDT_CMD, Xregs },
	{ "boot",     CMDT_CMD, Xboot },
	{ "memory",   CMDT_CMD, Xmemory },
	{ NULL, 0 }
};

static int
Xdiskinfo()
{
	int i;

	printf("Disk\tBIOS#\t    BSD#\tCylinders\tHeads\tSectors\n");
	for(i = 0; bios_diskinfo[i].bios_number != -1 && i < 10; i++){
		int d = bios_diskinfo[i].bios_number;

		printf("%cd%d\t 0x%x\t0x%x\t %s%d   \t%d\t%d\n",
			(d & 0x80)?'h':'f', (d & 0x80)?d - 128:d, d,
			bios_diskinfo[i].bsd_dev,
			(bios_diskinfo[i].bios_cylinders < 100)?"  ":" ",
			bios_diskinfo[i].bios_cylinders,
			bios_diskinfo[i].bios_heads,
			bios_diskinfo[i].bios_sectors);
	}

	return 0;
}

static int
Xregs()
{
	DUMP_REGS;
	return 0;
}

static int
Xboot()
{
	int dev, part, st;
	char *buf = (void *)0x7c00;

	if(cmd.argc != 2) {
		printf("machine boot {fd,hd}[0123][abcd]\n");
		return 0;
	}

	/* Check arg */
	if(cmd.argv[1][0] != 'f' && cmd.argv[1][0] != 'h')
		goto bad;
	if(cmd.argv[1][1] != 'd')
		goto bad;
	if(cmd.argv[1][2] < '0' || cmd.argv[1][2] > '3')
		goto bad;
	if(cmd.argv[1][3] < 'a' || cmd.argv[1][3] > 'd')
		goto bad;

	printf("Booting from %s ", cmd.argv[1]);

	dev = (cmd.argv[1][0] == 'h')?0x80:0;
	dev += (cmd.argv[1][2] - '0');
	part = (cmd.argv[1][3] - 'a');

	printf("[%x,%d]\n", dev, part);

	/* Read boot sector from device */
	st = biosd_io(F_READ, dev, 0, 0, 1, 1, buf);
	if(st) goto bad;

	/* Frob boot flag in buffer from HD */
	if(dev & 0x80){
		int i, j;

		for(i = 0, j = DOSPARTOFF; i < 4; i++, j += 16)
			if(part == i)
				buf[j] = 0x80;
			else
				buf[j] = 0x00;
	}

	printf("%x %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);

	/* Load %dl, ljmp */
	bootbuf(dev, part);

bad:
	printf("Invalid device!\n");
	return 0;
}

int
Xmemory()
{
	bios_memmap_t *tm = memory_map;
	int total = 0;

	printf ("Map:");
	for(; tm->type != BIOS_MAP_END; tm++){
		printf(" [%luK]@0x%lx", (long)tm->size, (long)tm->addr);
		if(tm->type == BIOS_MAP_FREE)
			total += tm->size;
	}

	printf("\nTotal: %uK, Low: %uK, High: %uK\n", total, cnvmem, extmem);

	return 0;
}
