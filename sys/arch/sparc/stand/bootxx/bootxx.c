/*	$OpenBSD: bootxx.c,v 1.3 2002/08/11 23:11:22 art Exp $	*/
/*	$NetBSD: bootxx.c,v 1.2 1997/09/14 19:28:17 pk Exp $	*/

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

#include <sys/param.h>
#include <sys/time.h>
#include <a.out.h>

#include <lib/libsa/stand.h>

#include <sparc/stand/common/promdev.h>

int debug;
int netif_debug;

/*
 * Boot device is derived from ROM provided information.
 */
char		progname[] = "bootxx";
struct open_file	io;

/*
 * The contents of the block_* variables below is set by installboot(8)
 * to hold the the filesystem data of the second-stage boot program
 * (typically `/boot'): filesystem block size, # of filesystem blocks and
 * the block numbers themselves.
 */
#define MAXBLOCKNUM	256	/* enough for a 2MB boot program (bs 8K) */
int32_t			block_size = 0;
int32_t			block_count = MAXBLOCKNUM;
daddr_t			block_table[MAXBLOCKNUM] = { 0 };


void	loadboot(struct open_file *, caddr_t);

int
main()
{
	char	*dummy;
	size_t	n;
	register void (*entry)(caddr_t) = (void (*)(caddr_t))LOADADDR;

	prom_init();
	io.f_flags = F_RAW;
	if (devopen(&io, 0, &dummy)) {
		panic("%s: can't open device", progname);
	}

	(void)loadboot(&io, LOADADDR);
	(io.f_dev->dv_close)(&io);
	(*entry)(cputyp == CPU_SUN4 ? LOADADDR : (caddr_t)promvec);
	_rtt();
}

void
loadboot(f, addr)
	register struct open_file	*f;
	register char			*addr;
{
	register int	i;
	register char	*buf;
	size_t		n;
	daddr_t		blk;

	/*
	 * Allocate a buffer that we can map into DVMA space; only
	 * needed for sun4 architecture, but use it for all machines
	 * to keep code size down as much as possible.
	 */
	buf = alloc(block_size);
	if (buf == NULL)
		panic("%s: alloc failed", progname);

	for (i = 0; i < block_count; i++) {
		if ((blk = block_table[i]) == 0)
			panic("%s: block table corrupt", progname);

#ifdef DEBUG
		printf("%s: block # %d = %d\n", progname, i, blk);
#endif
		if ((f->f_dev->dv_strategy)(f->f_devdata, F_READ,
					    blk, block_size, buf, &n)) {
			panic("%s: read failure", progname);
		}
		bcopy(buf, addr, block_size);
		if (n != block_size)
			panic("%s: short read", progname);
		if (i == 0) {
			register int m = N_GETMAGIC(*(struct exec *)addr);
			if (m == ZMAGIC || m == NMAGIC || m == OMAGIC) {
				/* Move exec header out of the way */
				bcopy(addr, addr - sizeof(struct exec), n);
				addr -= sizeof(struct exec);
			}
		}
		addr += n;
	}

}
