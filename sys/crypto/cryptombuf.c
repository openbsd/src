/*      $OpenBSD: cryptombuf.c,v 1.5 2001/11/06 19:53:18 miod Exp $	*/

/*
 * Copyright (c) 1999 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>

#include <uvm/uvm_extern.h>

#include <crypto/cryptodev.h>

int
mbuf2pages(m, np, pp, lp, maxp, nicep)
	struct mbuf *m;
	int *np;
	long *pp;
	int *lp;
	int maxp;
	int *nicep;
{
	int npa = 0, tlen = 0;

	for (; m != NULL; m = m->m_next) {
		vaddr_t va, off;
		paddr_t pa;
		int len;

		if ((len = m->m_len) == 0)
			continue;
		tlen += len;
		va = (vaddr_t)m->m_data;
		off = va & PAGE_MASK;
		va -= off;

next_page:
		if (pmap_extract(pmap_kernel(), va, &pa) == FALSE)
			panic("mbuf2pages: page not mapped");

		pa += off;

		lp[npa] = len;
		pp[npa] = pa;

		if (++npa > maxp)
			return (0);

		if (len + off > PAGE_SIZE) {
			lp[npa - 1] = PAGE_SIZE - off;
			va += PAGE_SIZE;
			len -= PAGE_SIZE;
			goto next_page;
		}
	}
			
	if (nicep) {
		int nice = 1;
		int i;

		/* see if each [pa,len] entry is long-word aligned */
		for (i = 0; i < npa; i++)
			if ((lp[i] & 3) || (pp[i] & 3))
				nice = 0;
		*nicep = nice;
	}

	*np = npa;
	return (tlen);
}
