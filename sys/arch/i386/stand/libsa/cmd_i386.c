/*	$OpenBSD: cmd_i386.c,v 1.21 1998/05/25 19:20:51 mickey Exp $	*/

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

#ifndef _TEST
#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/biosvar.h>
#include <sys/disklabel.h>
#include "disk.h"
#include "debug.h"
#include "biosdev.h"
#include "libsa.h"
#include <cmd.h>


extern const char version[];

static int Xboot __P((void));
static int Xdiskinfo __P((void));
static int Xmemory __P((void));
static int Xregs __P((void));
static int Xcnvmem __P((void));
static int Xextmem __P((void));

/* From gidt.S */
int bootbuf __P((void*, int));

const struct cmd_table cmd_machine[] = {
	{ "boot",     CMDT_CMD, Xboot },
	{ "diskinfo", CMDT_CMD, Xdiskinfo },
	{ "memory",   CMDT_CMD, Xmemory },
	{ "regs",     CMDT_CMD, Xregs },
	{ "cnvmem",   CMDT_CMD, Xcnvmem},
	{ "extmem",   CMDT_CMD, Xextmem},
	{ NULL, 0 }
};


/* Set size of conventional ram */
static int
Xcnvmem()
{
	if (cmd.argc != 2)
		printf("cnvmem %d\n", cnvmem);
	else
		cnvmem = strtol(cmd.argv[1], NULL, 0);

	return 0;
}

/* Set size of extended ram */
static int
Xextmem()
{
	if (cmd.argc != 2)
		printf("extmem %d\n", extmem);
	else
		extmem = strtol(cmd.argv[1], NULL, 0);

	return 0;
}

static int
Xdiskinfo()
{
	dump_diskinfo();

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
	char buf[DEV_BSIZE], *dest = (void*)0x7c00;

	if(cmd.argc != 2) {
		printf("machine boot {fd,hd}<0123>[abcd]\n");
		printf("Where [0123] is the disk number,"
			" and [abcd] is the partition.\n");
		return 0;
	}

	/* Check arg */
	if(cmd.argv[1][0] != 'f' && cmd.argv[1][0] != 'h')
		goto bad;
	if(cmd.argv[1][1] != 'd')
		goto bad;
	if(cmd.argv[1][2] < '0' || cmd.argv[1][2] > '3')
		goto bad;
	if((cmd.argv[1][3] < 'a' || cmd.argv[1][3] > 'd') && cmd.argv[1][3] != '\0')
		goto bad;

	printf("Booting from %s ", cmd.argv[1]);

	dev = (cmd.argv[1][0] == 'h')?0x80:0;
	dev += (cmd.argv[1][2] - '0');
	part = (cmd.argv[1][3] - 'a');

	if (part > 0)
		printf("[%x,%d]\n", dev, part);
	else
		printf("[%x]\n", dev);

	/* Read boot sector from device */
	st = biosd_io(F_READ, dev, 0, 0, 0, 1, buf);
	if(st) goto bad;

	/* Frob boot flag in buffer from HD */
	if((dev & 0x80) && (part > 0)){
		int i, j;

		for(i = 0, j = DOSPARTOFF; i < 4; i++, j += 16)
			if(part == i)
				buf[j] |= 0x80;
			else
				buf[j] &= ~0x80;
	}

	/* Load %dl, ljmp */
	bcopy(buf, dest, DEV_BSIZE);
	bootbuf(dest, dev);

bad:
	printf("Invalid device!\n");
	return 0;
}

static int
Xmemory()
{
	bios_memmap_t *tm = memory_map;
	int count, total = 0;

	for(count = 0; tm[count].type != BIOS_MAP_END; count++){
		printf("Region %d: type %u at 0x%lx for %luKB\n", count,
			tm[count].type, (long)tm[count].addr, (long)tm[count].size);

		if(tm[count].type == BIOS_MAP_FREE)
			total += tm[count].size;
	}

	printf("Low ram: %dKB  High ram: %dKB\n", cnvmem, extmem);
	printf("Total free memory: %dKB\n", total);

	return 0;
}
#endif
