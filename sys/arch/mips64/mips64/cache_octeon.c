/*
 * Copyright (c) 2010 Takuya ASADA.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 1998-2004 Opsycon AB (www.opsycon.se)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>

#define SYNC() asm volatile("sync\n")
#define SYNCI() \
	asm volatile( \
		".set push\n" \
		".set mips64r2\n" \
		".word 0x041f0000\n" \
		"nop\n" \
		".set pop")

int 
Octeon_ConfigCache(struct cpu_info *ci)
{
	ci->ci_cacheways = 4;
	ci->ci_l1instcachesize = 32 * 1024;
	ci->ci_l1instcacheline = 128;
	ci->ci_l1datacachesize = 16 * 1024;
	ci->ci_l1datacacheline = 128;
	ci->ci_l2size = 128 * 1024;
	ci->ci_l3size = 0;
	return 0;
}

void
Octeon_SyncCache(struct cpu_info *ci)
{
	SYNC();
}

void
Octeon_InvalidateICache(struct cpu_info *ci, vaddr_t addr, size_t len)
{
	/* A SYNCI flushes the entire icache on OCTEON */
	SYNCI();
}

void
Octeon_SyncDCachePage(struct cpu_info *ci, paddr_t addr)
{
}

void
Octeon_HitSyncDCache(struct cpu_info *ci, paddr_t addr, size_t len)
{
}

void
Octeon_HitInvalidateDCache(struct cpu_info *ci, paddr_t addr, size_t len)
{
}

void
Octeon_IOSyncDCache(struct cpu_info *ci, paddr_t addr, size_t len, int how)
{
	switch (how) {
		default:
		case 0:
			break;
		case 1: /* writeback */
		case 2: /* writeback and invalidate */
			SYNC();
			break;
	}
}
