/*	$NetBSD: kgdb_glue.c,v 1.2 1996/03/27 22:42:16 mark Exp $	*/

/*
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>

#include <kgdb/kgdb.h>

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/kgdb.h>

int kgdbregs[NREG];

dump(p, l)
	u_char *p;
{
	int i, j, n;
	
	while (l > 0) {
		printf("%08x: ", p);
		n = l > 16 ? 16 : l;
		for (i = 4; --i >= 0;) {
			for (j = 4; --j >= 0;)
				printf(--n >= 0 ? "%02x " : "   ", *p++);
			printf(" ");
		}
		p -= 16;
		n = l > 16 ? 16 : l;
		n = (n + 3) & ~3;
		for (i = 4; --i >= 0;)
			printf((n -= 4) >= 0 ? "%08x " : "", *((long *)p)++);
		printf("\n");
		l -= 16;
	}
}

#define	NDBGSTKS	8	/* number of debugstacks */
#define	SZDBGSTK	512	/* size of one debugstack */
static int debugstack[NDBGSTKS * SZDBGSTK];
static int dbgstkused;

int *
kgdb_find_stack()
{
	int i;
	
	for (i = 0; i < NDBGSTKS; i++)
		if (!(dbgstkused&(1 << i))) {
			dbgstkused |= 1 << i;
			return debugstack + (i + 1) * SZDBGSTK;
		}
	panic("KGDB: no stack");
}

void
kgdb_free_stack(sp)
	int *sp;
{
	int i;
	
	for (i = 0; i < NDBGSTKS; i++)
		if (sp == debugstack + (i + 1) * SZDBGSTK) {
			if (!(dbgstkused&(1 << i)))
				panic("KGDB: free free stack");
			dbgstkused &= ~(1 << i);
			return;
		}
	panic("KGDB: free non-stack");
}

void
kgdbinit()
{
	/* initialize undefined mode & setup trap vector */
	initmode(PSR_UND32_MODE|F32_bit|I32_bit, kgdb_find_stack());
}

void
kgdb_connect(when)
	int when;
{
	boothowto |= RB_KDB;
	if (when == 0)
		printf("waiting for remote GDB\n");
	__asm(".word 0xe6000010");
}

int
kgdb_poll()
{
	return 0;
}

void
kgdb_panic()
{
	kgdbpanic = 1;
	__asm(".word 0xe6000010");
}

int
kgdb_trap_glue(regs)
	int *regs;
{
	int inst;
	int cnt;
	
	inst = fetchinst(regs[PC] - 4);
	switch (inst) {
	default:
		/* unexpected */
#ifdef	__notyet__
		return 0;
#endif
	case 0xe6000011:	/* KGDB installed breakpoint */
		regs[PC] -= 4;
		break;
	case 0xe6000010:	/* breakpoint in kgdb_connect */
		break;
	}
	while (1) {
		kgdbcopy(regs, kgdbregs, sizeof kgdbregs);
		switch (kgdbcmds()) {
		case 1:
			kgdbcopy(kgdbregs, regs, sizeof kgdbregs);
			if ((cnt = singlestep(regs)) < 0)
				panic("singlestep");
			regs[PC] += cnt;
			continue;
		default:
			break;
		}
		break;
	}
	kgdbcopy(kgdbregs, regs, sizeof kgdbregs);
	if (PSR_USER(regs[PSR]) || !PSR_32(regs[PSR]))
		panic("KGDB: invalid mode %x", regs[PSR]);
	return 1;
}

void
kgdbcopy(vs, vd, n)
	void *vs, *vd;
	int n;
{
	char *s = vs, *d = vd;
	long *ls, *ld;
	int ln;
	
	if (((int)s&(sizeof(long)-1)) == ((int)d&(sizeof(long)-1))
	    && n >= 2 * sizeof(long)) {
		while (((int)s&(sizeof(long)-1)) && --n >= 0)
			*d++ = *s++;
		ln = n / sizeof(long);
		n -= ln * sizeof(long);
		ls = (long *)s;
		ld = (long *)d;
		while (--ln >= 0)
			*ld++ = *ls++;
		s = (char *)ls;
		d = (char *)ld;
	}
	while (--n >= 0)
		*d++ = *s++;
}

void
kgdbzero(vd, n)
	void *vd;
	int n;
{
	char *d = vd;
	long *ld;
	int ln;
	
	if (n >= 2 * sizeof(long)) {
		while (--n >= 0) {
			if (!((int)d&sizeof(long)-1))
				break;
			*d++ = 0;
		}
		ln = n / sizeof(long);
		n -= ln * sizeof(long);
		ld = (long *)d;
		while (--ln >= 0)
			*ld++ = 0;
		d = (char *)ld;
	}
	while (--n >= 0)
		*d++ = 0;
}

int
kgdbcmp(vs, vd, n)
	void *vs, *vd;
	int n;
{
	char *s = vs, *d = vd;
	long *ls, *ld;
	int ln;
	
	if (((int)s&(sizeof(long)-1)) == ((int)d&(sizeof(long)-1))
	    && n >= 2 * sizeof(long)) {
		while (--n >= 0) {
			if (!((int)s&(sizeof(long)-1)))
				break;
			if (*d++ != *s++)
				return *--d - *--s;
		}
		ln = n / sizeof(long);
		n -= ln * sizeof(long);
		ls = (long *)s;
		ld = (long *)d;
		while (--ln >= 0)
			if (*ld++ != *ls++) {
				n += ++ln * sizeof(long);
				break;
			}
		s = (char *)ls;
		d = (char *)ld;
	}
	while (--n >= 0)
		if (*d++ != *s++)
			return *--d - *--s;
	return 0;
}
