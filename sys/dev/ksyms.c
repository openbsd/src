/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * kernel symbols special file (masquerades as a ZMAGIC a.out file)
 *
 * TODO: get boot loaders to put symbols on a page boundary so we
 *       can mmap them too (also requires minor change to db_aout.c).
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include <machine/cpu.h>

#include <vm/vm.h>

extern char *esym;				/* end of symbol table */
extern long end;				/* end of kernel */

static struct exec *k1;				/* first page of /dev/ksyms */
static caddr_t symtab = (caddr_t)(&end + 1);	/* start of symbol table */

void	ksymsattach __P((int));
int	ksymsopen __P((dev_t, int, int));
int	ksymsclose __P((dev_t, int, int));
int	ksymsread __P((dev_t, struct uio *, int));

/*ARGSUSED*/
void
ksymsattach(num)
	int num;
{

	if (esym > (char *)&end) {
		/*
		 * If we have a symbol table, fake up a struct exec.
		 * We only fill in the following non-zero entries:
		 *	a_text - fake text sement (struct exec only)
		 *	a_syms - size of symbol table
		 *
		 * We assume __LDPGSZ is a multiple of NBPG (it is)
		 */
		k1 = (struct exec *)malloc(__LDPGSZ, M_TEMP, M_WAITOK);
		bzero(k1, __LDPGSZ);
		N_SETMAGIC(*k1, ZMAGIC, MID_MACHINE, 0);
		k1->a_text = __LDPGSZ;
		k1->a_syms = end;
	}
	return;
}

/*ARGSUSED*/
int
ksymsopen(dev, flag, mode)
	dev_t dev;
	int flag, mode;
{

	/* There are no non-zero minor devices */
	if (minor(dev) != 0)
		return (ENXIO);

	/* This device is read-only */
	if ((flag & FWRITE))
		return (EPERM);
		
	/* Must have symbols at the end of the kernel to work */
	if (esym <= (char *)&end)
		return (ENXIO);

	return (0);
}

/*ARGSUSED*/
int
ksymsclose(dev, flag, mode)
	dev_t dev;
	int flag, mode;
{

	return (0);
}

/*ARGSUSED*/
int
ksymsread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register vm_offset_t v;
	register size_t c, len;
	int error = 0;

#define iov	(uio->uio_iov)
	while (uio->uio_resid > 0 && error == 0) {
		/* Done with this iov?  Fill the next one... */
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("ksymread");
		}

		/* Can't read past size of symbol table... */
		if (uio->uio_offset >= (vm_offset_t)(esym - symtab) +
		    k1->a_text)
			break;

		if (uio->uio_offset < k1->a_text) {
			/*
			 * If they asked for more that a_text,
			 * read the part of k1 first, then the
			 * part of symtab next time throug the loop.
			 */
			if (iov->iov_len + (size_t)uio->uio_offset >
			    k1->a_text)
				len = k1->a_text;
			else
				len = iov->iov_len;

			/* Make offset relative to struct exec */
			v = uio->uio_offset + (vm_offset_t)k1;
			c = min(len, MAXPHYS);
			error = uiomove((caddr_t)v, c, uio);
		} else {
			/* Make offset relative to symtab */
			v = uio->uio_offset - k1->a_text +
			    (vm_offset_t)symtab;
			c = min(iov->iov_len, MAXPHYS);

			/* Don't read past esym, truncate. */
			if (v + c > (vm_offset_t)esym)
				c = (vm_offset_t)esym - v;
			error = uiomove((caddr_t)v, c, uio);
		}
	}
	return (error);
}

/* XXX - can't do mmap until boot loaders make the symbol table page aligned */
#if 0
int
ksymsmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
#define ksyms_btop(x)	((vm_offset_t)(x) >> PGSHIFT
	if (off < 0)
		return (-1);
	if ((unsigned)off >= (unsigned)(esym - symtab) + k1->a_text)
		return (-1);

	if ((unsigned)off < k1->a_text)
		return (ksyms_btop(off + (unsigned)k1));
	else
		return (ksyms_btop(off + (unsigned)symtab - k1->a_text));
}
#endif
