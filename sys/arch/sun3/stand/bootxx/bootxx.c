/*	$NetBSD: bootxx.c,v 1.5 1995/10/13 21:44:57 gwr Exp $ */

/*
 * Copyright (c) 1994 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * This is a generic "first-stage" boot program.
 *
 * Note that this program has absolutely no filesystem knowledge!
 *
 * Instead, this uses a table of disk block numbers that are
 * filled in by the installboot program such that this program
 * can load the "second-stage" boot program.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/exec.h>

#include <machine/mon.h>
#include "stand.h"

/*
 * Boot device is derived from ROM provided information.
 */
#define LOADADDR	0x4000

/* This determines the largest boot program we can load. */
#define MAXBLOCKNUM	64

/*
 * These three names are known by installboot.
 * The block_table contains starting block numbers,
 * in terms of 512-byte blocks.  Each non-zero value
 * will result in a read of block_size bytes.
 */
int     	block_size = 512;	/* default */
int     	block_count = MAXBLOCKNUM;	/* length of table */
daddr_t 	block_table[MAXBLOCKNUM] = { 0 };


main()
{
	struct open_file	f;
	void	(*entry)();
	char	*addr;
	int n, error;

#ifdef DEBUG
	printf("bootxx: open...\n");
#endif
	f.f_flags = F_RAW;
	if (devopen(&f, 0, &addr)) {
		printf("bootxx: open failed\n");
		exit();
	}

	addr = (char*)LOADADDR;
	error = copyboot(&f, addr);
	f.f_dev->dv_close(&f);
	if (!error) {
#ifdef DEBUG
		printf("bootxx: start 0x%x\n", (long)addr);
#endif
		entry = (void (*)())addr;
		(*entry)();
	}
	/* copyboot had a problem... */
	exit();
}

int
copyboot(fp, addr)
	struct open_file	*fp;
	char			*addr;
{
	int	n, i, blknum;
	char *buf;

#ifdef	sparc
	/*
	 * On the sparc, the 2nd stage boot has an a.out header.
	 * On the sun3, (by tradition) the 2nd stage boot programs
	 * have the a.out header stripped off.  (1st stage boot
	 * programs have the header stripped by installboot.)
	 */
	/* XXX - This assumes OMAGIC format! */
	addr -= sizeof(struct exec); /* XXX */
#endif

	/* Need to use a buffer that can be mapped into DVMA space. */
	buf = alloc(block_size);
	if (!buf)
		panic("bootxx: alloc failed");

	for (i = 0; i < block_count; i++) {

		if ((blknum = block_table[i]) == 0)
			break;

#ifdef DEBUG
		printf("bootxx: block # %d = %d\n", i, blknum);
#endif
		if ((fp->f_dev->dv_strategy)(fp->f_devdata, F_READ,
					   blknum, block_size, buf, &n))
		{
			printf("bootxx: read failed\n");
			return -1;
		}
		if (n != block_size) {
			printf("bootxx: short read\n");
			return -1;
		}
		bcopy(buf, addr, block_size);
		addr += block_size;
	}

	return 0;
}

