/*	$NetBSD: boot.c,v 1.1.1.1 1995/10/13 21:27:30 gwr Exp $	*/

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
#include <sys/reboot.h>
#include <sys/exec.h>

#include <machine/mon.h>

#include "stand.h"
#include "promboot.h"

/*
 * Boot device is derived from ROM provided information.
 */
#define LOADADDR	0x4000

extern int debug;
extern char		*version;

char	line[80];

int block_size = 512;


main()
{
	struct open_file	f;
	struct bootparam *bp;
	void	(*entry)();
	char	*addr;
	int n, error;

	printf(">> NetBSD tapeboot [%s]\n", version);
	prom_get_boot_info();

	/*
	 * Set the tape file number to the next one, because
	 * the boot program is first, then the kernel.
	 */
	bp = *romp->bootParam;
	bp->partNum += 1;

	for (;;) {
		if (prom_boothow & RB_ASKNAME) {
			printf("tapeboot: segment? [%d] ", bp->partNum);
			gets(line);
			if (('0' <= line[0]) && (line[0] <= '9')) {
				bp->partNum = line[0] - '0';
			}
		}
		
		printf("tapeboot: opening segment %d\n", bp->partNum);
		f.f_flags = F_RAW;
		if ((error = devopen(&f, 0, &addr)) != 0) {
			printf("tapeboot: open failed, error=%d\n", error);
			goto ask;
		}

		addr = (char*)LOADADDR;
		error = loadfile(&f, addr);

		printf("tapeboot: close (rewind)...\n");
		f.f_dev->dv_close(&f);
		if (error == 0)
			break;

		printf("tapeboot: load failed, error=%d\n", error);
	ask:
		prom_boothow |= RB_ASKNAME;
	}

	if (debug) {
		printf("Debug mode - enter c to continue...");
		/* This will print "\nAbort at ...\n" */
		asm("	trap #0");
	}

	printf("Starting program at 0x%x\n", (long)addr);
	entry = (void (*)())addr;
	(*entry)();
}

int
loadfile(fp, addr)
	struct open_file	*fp;
	char			*addr;
{
	char *buf;
	int	n, blknum;
	int error = 0;
	/*
	 * Loading a kernel.  It WILL have an a.out header.
	 * XXX - This assumes OMAGIC format!
	 */
	addr -= sizeof(struct exec); /* XXX */

	/* Need to use a buffer that can be mapped into DVMA space. */
	buf = alloc(block_size);
	if (!buf)
		panic("tapeboot: alloc failed");

	printf("tapeboot: loading ... ");
	/* limit program size to < 2MB */
	for (blknum = 0; blknum < 4096; blknum++) {

		error = (fp->f_dev->dv_strategy)(fp->f_devdata, F_READ,
		                blknum, block_size, buf, &n);
		if (error) {
			printf("(error=%d)\n", error);
			return EIO;
		}
		if (n == 0)
			break;	/* end of tape */
		if (n != block_size) {
			printf(" (short read)\n");
			return EIO;
		}

		bcopy(buf, addr, block_size);
		addr += block_size;
	}

	printf("(%d blocks)\n", blknum);
	return 0;
}

