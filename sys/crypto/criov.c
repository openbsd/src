/*      $OpenBSD: criov.c,v 1.8 2001/11/06 19:53:18 miod Exp $	*/

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
iov2pages(uio, np, pp, lp, maxp, nicep)
	struct uio *uio;
	int *np;
	long *pp;
	int *lp;
	int maxp;
	int *nicep;
{
	int npa = 0, tlen = 0;
	int i;

	for (i = 0; i < uio->uio_iovcnt; i++) {
		vaddr_t va, off;
		paddr_t pa;
		int len;

		if ((len = uio->uio_iov[i].iov_len) == 0)
			continue;
		tlen += len;
		va = (vaddr_t)uio->uio_iov[i].iov_base;
		off = va & PAGE_MASK;
		va -= off;

next_page:
		
		if (pmap_extract(pmap_kernel(), va, &pa) == FALSE)
			panic("iov2pages: unmapped pages");

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

void
cuio_copydata(uio, off, len, cp)
	struct uio *uio;
	int off, len;
	caddr_t cp;
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;

	if (off < 0)
		panic("cuio_copydata: off %d < 0", off);
	if (len < 0)
		panic("cuio_copydata: len %d < 0", len);
	while (off > 0) {
		if (iol == 0)
			panic("iov_copydata: empty in skip");
		if (off < iov->iov_len)
			break;
		off -= iov->iov_len;
		iol--;
		iov++;
	}
	while (len > 0) {
		if (iol == 0)
			panic("cuio_copydata: empty");
		count = min(iov->iov_len - off, len);
		bcopy(((caddr_t)iov->iov_base) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		iol--;
		iov++;
	}
}

void
cuio_copyback(uio, off, len, cp)
	struct uio *uio;
	int off, len;
	caddr_t cp;
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;

	if (off < 0)
		panic("cuio_copyback: off %d < 0", off);
	if (len < 0)
		panic("cuio_copyback: len %d < 0", len);
	while (off > 0) {
		if (iol == 0)
			panic("cuio_copyback: empty in skip");
		if (off < iov->iov_len)
			break;
		off -= iov->iov_len;
		iol--;
		iov++;
	}
	while (len > 0) {
		if (iol == 0)
			panic("uio_copyback: empty");
		count = min(iov->iov_len - off, len);
		bcopy(cp, ((caddr_t)iov->iov_base) + off, count);
		len -= count;
		cp += count;
		off = 0;
		iol--;
		iov++;
	}
}
