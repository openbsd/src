/*	$OpenBSD: db_prof.c,v 1.4 2017/08/11 15:14:23 nayden Exp $	*/

/*
 * Copyright (c) 2016 Martin Pieuchot
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
/*-
 * Copyright (c) 1983, 1992, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/exec_elf.h>
#include <sys/malloc.h>
#include <sys/gmon.h>

#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h> /* for db_write_bytes() */
#include <ddb/db_sym.h>

extern char etext[];

struct prof_probe {
	const char		*pp_name;
	Elf_Sym			*pp_symb;
	SLIST_ENTRY(prof_probe)	 pp_next;
	vaddr_t			 pp_inst;
	int			 pp_on;
};

#define PPTSIZE		PAGE_SIZE
#define	PPTMASK		((PPTSIZE / sizeof(struct prof_probe)) - 1)
#define INSTTOIDX(inst)	((((unsigned long)(inst)) >> 4) & PPTMASK)
SLIST_HEAD(, prof_probe) *pp_table;

extern int db_profile;			/* Allow dynamic profiling */
int db_prof_on;				/* Profiling state On/Off */

vaddr_t db_get_pc(struct trapframe *);
vaddr_t db_get_probe_addr(struct trapframe *);

void db_prof_forall(Elf_Sym *, char *, char *, int, void *);
void db_prof_count(unsigned long, unsigned long);

void
db_prof_init(void)
{
	unsigned long nentries;

	pp_table = malloc(PPTSIZE, M_TEMP, M_NOWAIT|M_ZERO);
	if (pp_table == NULL)
		return;

	db_elf_sym_forall(db_prof_forall, &nentries);
	printf("ddb probe table references %lu entry points\n", nentries);
}

void
db_prof_forall(Elf_Sym *sym, char *name, char *suff, int pre, void *xarg)
{
	Elf_Sym *symb = sym;
	unsigned long *nentries = xarg;
	struct prof_probe *pp;
	vaddr_t inst;

	if (ELF_ST_TYPE(symb->st_info) != STT_FUNC)
		return;

	inst = symb->st_value;
	if (inst < KERNBASE || inst >= (vaddr_t)&etext)
		return;

	if (*((uint8_t *)inst) != SSF_INST)
		return;

	if (strncmp(name, "db_", 3) == 0 || strncmp(name, "trap", 4) == 0)
		return;

#ifdef __i386__
	/* Avoid a recursion in db_write_text(). */
	if (strncmp(name, "pmap_pte", 8) == 0)
		return;
#endif

	pp = malloc(sizeof(struct prof_probe), M_TEMP, M_NOWAIT|M_ZERO);
	if (pp == NULL)
		return;

	pp->pp_name = name;
	pp->pp_inst = inst;
	pp->pp_symb = symb;

	SLIST_INSERT_HEAD(&pp_table[INSTTOIDX(pp->pp_inst)], pp, pp_next);

	(*nentries)++;
}

int
db_prof_enable(void)
{
#if defined(__amd64__) || defined(__i386__)
	struct prof_probe *pp;
	uint8_t patch = BKPT_INST;
	unsigned long s;
	int i;

	if (!db_profile)
		return EPERM;

	if (pp_table == NULL)
		return ENOENT;

	KASSERT(BKPT_SIZE == SSF_SIZE);

	s = intr_disable();
	for (i = 0; i < (PPTSIZE / sizeof(*pp)); i++) {
		SLIST_FOREACH(pp, &pp_table[i], pp_next) {
			pp->pp_on = 1;
			db_write_bytes(pp->pp_inst, BKPT_SIZE, &patch);
		}
	}
	intr_restore(s);

	db_prof_on = 1;

	return 0;
#else
	return ENOENT;
#endif
}

void
db_prof_disable(void)
{
	struct prof_probe *pp;
	uint8_t patch = SSF_INST;
	unsigned long s;
	int i;

	db_prof_on = 0;

	s = intr_disable();
	for (i = 0; i < (PPTSIZE / sizeof(*pp)); i++) {
		SLIST_FOREACH(pp, &pp_table[i], pp_next) {
			db_write_bytes(pp->pp_inst, SSF_SIZE, &patch);
			pp->pp_on = 0;
		}
	}
	intr_restore(s);
}

int
db_prof_hook(struct trapframe *frame)
{
	struct prof_probe *pp;
	vaddr_t pc, inst;

	if (pp_table == NULL)
		return 0;

	pc = db_get_pc(frame);
	inst = db_get_probe_addr(frame);

	SLIST_FOREACH(pp, &pp_table[INSTTOIDX(inst)], pp_next) {
		if (pp->pp_on && pp->pp_inst == inst) {
			if (db_prof_on)
				db_prof_count(pc, inst);
			return 1;
		}
	}

	return 0;
}

/*
 * Equivalent to mcount(), must be called with interrupt disabled.
 */
void
db_prof_count(unsigned long frompc, unsigned long selfpc)
{
	unsigned short *frompcindex;
	struct tostruct *top, *prevtop;
	struct gmonparam *p;
	long toindex;

	if ((p = curcpu()->ci_gmon) == NULL)
		return;

	/*
	 * check that we are profiling
	 * and that we aren't recursively invoked.
	 */
	if (p->state != GMON_PROF_ON)
		return;

	/*
	 * check that frompcindex is a reasonable pc value.
	 * for example:	signal catchers get called from the stack,
	 *		not from text space.  too bad.
	 */
	frompc -= p->lowpc;
	if (frompc > p->textsize)
		return;

#if (HASHFRACTION & (HASHFRACTION - 1)) == 0
	if (p->hashfraction == HASHFRACTION)
		frompcindex =
		    &p->froms[frompc / (HASHFRACTION * sizeof(*p->froms))];
	else
#endif
		frompcindex =
		    &p->froms[frompc / (p->hashfraction * sizeof(*p->froms))];
	toindex = *frompcindex;
	if (toindex == 0) {
		/*
		 *	first time traversing this arc
		 */
		toindex = ++p->tos[0].link;
		if (toindex >= p->tolimit)
			/* halt further profiling */
			goto overflow;

		*frompcindex = toindex;
		top = &p->tos[toindex];
		top->selfpc = selfpc;
		top->count = 1;
		top->link = 0;
		return;
	}
	top = &p->tos[toindex];
	if (top->selfpc == selfpc) {
		/*
		 * arc at front of chain; usual case.
		 */
		top->count++;
		return;
	}
	/*
	 * have to go looking down chain for it.
	 * top points to what we are looking at,
	 * prevtop points to previous top.
	 * we know it is not at the head of the chain.
	 */
	for (; /* return */; ) {
		if (top->link == 0) {
			/*
			 * top is end of the chain and none of the chain
			 * had top->selfpc == selfpc.
			 * so we allocate a new tostruct
			 * and link it to the head of the chain.
			 */
			toindex = ++p->tos[0].link;
			if (toindex >= p->tolimit)
				goto overflow;

			top = &p->tos[toindex];
			top->selfpc = selfpc;
			top->count = 1;
			top->link = *frompcindex;
			*frompcindex = toindex;
			return;
		}
		/*
		 * otherwise, check the next arc on the chain.
		 */
		prevtop = top;
		top = &p->tos[top->link];
		if (top->selfpc == selfpc) {
			/*
			 * there it is.
			 * increment its count
			 * move it to the head of the chain.
			 */
			top->count++;
			toindex = prevtop->link;
			prevtop->link = top->link;
			top->link = *frompcindex;
			*frompcindex = toindex;
			return;
		}
	}

overflow:
	p->state = GMON_PROF_ERROR;
}
