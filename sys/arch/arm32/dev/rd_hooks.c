/* $NetBSD: rd_hooks.c,v 1.5 1996/03/28 21:14:13 mark Exp $ */

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
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

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>

#include <dev/ramdisk.h>

#ifndef RAMDISK_SIZE
#define RAMDISK_SIZE 0
#endif

/*extern int boothowto;*/
extern u_int ramdisc_size;
struct rd_conf *bootrd = NULL;

int load_ramdisc_from_floppy __P((struct rd_conf *rd, dev_t dev));

/*
 * This is called during autoconfig.
 */
int
rd_match_hook(parent, self, aux)
	struct device	*parent;
	void	*self;
	void	*aux;
{
	return (1);
}

void
rd_attach_hook(unit, rd)
	int unit;
	struct rd_conf *rd;
{
	if (unit == 0) {
		if (ramdisc_size == 0 && RAMDISK_SIZE)
			ramdisc_size = (RAMDISK_SIZE * 1024);

		if (ramdisc_size != 0) {
			rd->rd_size = round_page(ramdisc_size);
			rd->rd_addr = (caddr_t)kmem_alloc(kernel_map, ramdisc_size);
			rd->rd_type = RD_KMEM_FIXED;
			bootrd = rd;
		}
	}
	printf("rd%d: allocated %dK (%d blocks)\n", unit, rd->rd_size / 1024, rd->rd_size / DEV_BSIZE);
}


/*
 * This is called during open (i.e. mountroot)
 */

void
rd_open_hook(unit, rd)
	int unit;
	struct rd_conf *rd;
{
/* I use the ramdisc for other testing ... */
#if 0
	if (unit == 0) {
		/* The root ramdisk only works single-user. */
		boothowto |= RB_SINGLE;
	}
#endif
}
