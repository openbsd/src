/*	$OpenBSD: boot.c,v 1.2 1999/06/21 23:35:41 deraadt Exp $	*/
/*	$NetBSD: boot.c,v 1.2 1997/09/14 19:27:21 pk Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <a.out.h>
#include <lib/libsa/stand.h>

#include <sparc/stand/common/promdev.h>

void copyunix __P((int, char *));
void promsyms __P((int, struct exec *));
int debug;
int netif_debug;

/*
 * Boot device is derived from ROM provided information.
 */
#define	DEFAULT_KERNEL	"bsd"

extern char		*version;
unsigned long		esym;
char			*strtab;
int			strtablen;
char			fbuf[80], dbuf[128];

typedef void (*entry_t)__P((caddr_t, int, int, int, long, long));

void	loadfile __P((int, caddr_t));

main()
{
	int	io;
	char	*file;

	prom_init();

	printf(">> OpenBSD BOOT %s\n", version);

	file = prom_bootfile;
	if (file == 0 || *file == 0)
		file = DEFAULT_KERNEL;

	for (;;) {
		if (prom_boothow & RB_ASKNAME) {
			printf("device[%s]: ", prom_bootdevice);
			gets(dbuf);
			if (dbuf[0])
				prom_bootdevice = dbuf;
			printf("boot: ");
			gets(fbuf);
			if (fbuf[0])
				file = fbuf;
		}
		if ((io = open(file, 0)) >= 0)
			break;
		printf("open: %s: %s\n", file, strerror(errno));
		prom_boothow |= RB_ASKNAME;
	}

	printf("Booting %s @ 0x%x\n", file, LOADADDR);
	loadfile(io, LOADADDR);

	_rtt();
}

void
loadfile(io, addr)
	register int	io;
	register caddr_t addr;
{
	register entry_t entry = (entry_t)LOADADDR;
	struct exec x;
	int i;

	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) ||
	    N_BADMAG(x)) {
		printf("Bad format\n");
		return;
	}
	printf("%d", x.a_text);
	if (N_GETMAGIC(x) == ZMAGIC) {
		entry = (entry_t)(addr+sizeof(struct exec));
		addr += sizeof(struct exec);
	}
	if (read(io, (char *)addr, x.a_text) != x.a_text)
		goto shread;
	addr += x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC || N_GETMAGIC(x) == NMAGIC)
		while ((int)addr & __LDPGSZ)
			*addr++ = 0;
	printf("+%d", x.a_data);
	if (read(io, addr, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;
	printf("+%d", x.a_bss);
	for (i = x.a_bss; i ; --i)
		*addr++ = 0;
	if (x.a_syms != 0) {
		bcopy(&x.a_syms, addr, sizeof(x.a_syms));
		addr += sizeof(x.a_syms);
		printf("+[%d", x.a_syms);
		if (read(io, addr, x.a_syms) != x.a_syms)
			goto shread;
		addr += x.a_syms;

		if (read(io, &strtablen, sizeof(int)) != sizeof(int))
			goto shread;

		bcopy(&strtablen, addr, sizeof(int));
		if (i = strtablen) {
			i -= sizeof(int);
			addr += sizeof(int);
			if (read(io, addr, i) != i)
			    goto shread;
			addr += i;
		}
		printf("+%d]", i);
		esym = ((u_int)x.a_entry - (u_int)LOADADDR) +
			(((int)addr + sizeof(int) - 1) & ~(sizeof(int) - 1));
#if 0
		/*
		 * The FORTH word `loadsyms' is mentioned in the
		 * "Openboot command reference" book, but it seems it has
		 * not been implemented on at least one machine..
		 */
		promsyms(io, &x);
#endif
	}
	printf("=0x%x\n", addr);
	close(io);

	/* Note: args 2-4 not used due to conflicts with SunOS loaders */
	(*entry)(cputyp == CPU_SUN4 ? LOADADDR : (caddr_t)promvec,
		 0, 0, 0, esym, DDB_MAGIC1);
	return;

shread:
	printf("boot: short read\n");
	return;
}

#if 0
struct syms {
	u_int32_t	value;
	u_int32_t	index;
};

void
sort(syms, n)
	struct syms *syms;
	int n;
{
	register struct syms *sj;
	register int i, j, k;
	register u_int32_t value, index;

	/* Insertion sort.  This is O(n^2), but so what? */
	for (i = 1; i < n; i++) {
		/* save i'th entry */
		value = syms[i].value;
		index = syms[i].index;
		/* find j such that i'th entry goes before j'th */
		for (j = 0, sj = syms; j < i; j++, sj++)
			if (value < sj->value)
				break;
		/* slide up any additional entries */
		for (k = 0; k < (i - j); k++) {
			sj[k+1].value = sj[k].value;
			sj[k+1].index = sj[k].index;
		}
		sj->value = value;
		sj->index = index;
	}
}

void
promsyms(fd, hp)
	int fd;
	struct exec *hp;
{
	int i, n, strtablen;
	char *str, *p, *cp, buf[128];
	struct syms *syms;

	lseek(fd, sizeof(*hp)+hp->a_text+hp->a_data, SEEK_SET);
	n = hp->a_syms/sizeof(struct nlist);
	if (n == 0)
		return;
	syms = (struct syms *)alloc(n * sizeof(struct syms));

	printf("+[%x+", hp->a_syms);
	for (i = 0; i < n; i++) {
		struct nlist nlist;

		if (read(fd, &nlist, sizeof(nlist)) != sizeof(nlist)) {
			printf("promsyms: read failed\n");
			return;
		}
		syms[i].value = nlist.n_value;
		syms[i].index = nlist.n_un.n_strx - sizeof(strtablen);
	}

	sort(syms, n);

	if (read(fd, &strtablen, sizeof(strtablen)) != sizeof(strtablen)) {
		printf("promsym: read failed (strtablen)\n");
		return;
	}
	if (strtablen < sizeof(strtablen)) {
		printf("promsym: string table corrupted\n");
		return;
	}
	strtablen -= sizeof(strtablen);
	str = (char *)alloc(strtablen);

	printf("%x]", strtablen);
	if (read(fd, str, strtablen) != strtablen) {
		printf("promsym: read failed (strtab)\n");
		return;
	}

	sprintf(buf, "%x %d %x loadsyms", syms, n, str);
	(promvec->pv_fortheval.v2_eval)(buf);
}
#endif
