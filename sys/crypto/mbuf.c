/*      $OpenBSD: mbuf.c,v 1.2 2000/04/20 13:05:30 art Exp $	*/

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
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include <crypto/crypto.h>

int
mbuf2pages(m, np, pp, lp, maxp, nicep)
	struct mbuf *m;
	int *np;
	long *pp;
	int *lp;
	int maxp;
	int *nicep;
{
	struct  mbuf *m0;
	int npa = 0, tlen = 0;

	/* generate a [pa,len] array from an mbuf */
	for (m0 = m; m; m = m->m_next) {
		void *va;
		long pg, npg;
		int len, off;

		if (m->m_len == 0)
			continue;
		len = m->m_len;
		tlen += len;
		va = m->m_data;

		lp[npa] = len;
		pp[npa] = vtophys(va);
		pg = pp[npa] & ~PAGE_MASK;
		off = (long)va & PAGE_MASK;

		while (len + off > PAGE_SIZE) {
			va = va + PAGE_SIZE - off;
			npg = vtophys(va);
			if (npg != pg) {
				/* FUCKED UP condition */
				if (++npa > maxp)
					return (0);
				continue;
			}
			lp[npa] = PAGE_SIZE - off;
			off = 0;

			if (++npa > maxp)
				return (0);

			lp[npa] = len - (PAGE_SIZE - off);
			len -= lp[npa];
			pp[npa] = vtophys(va);
		}

		if (++npa == maxp)
			return (0);
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
