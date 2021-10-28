/*	$OpenBSD: dt_prov_kprobe.c,v 1.4 2021/10/28 08:47:40 jasper Exp $	*/

/*
 * Copyright (c) 2020 Tom Rollet <tom.rollet@epita.fr>
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
#if defined(DDBPROF) && (defined(__amd64__) || defined(__i386__))

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/atomic.h>
#include <sys/exec_elf.h>

#include <ddb/db_elf.h>
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_interface.h>

#include <dev/dt/dtvar.h>

int dt_prov_kprobe_alloc(struct dt_probe *dtp, struct dt_softc *sc,
    struct dt_pcb_list *plist, struct dtioc_req *dtrq);
int dt_prov_kprobe_hook(struct dt_provider *dtpv, ...);
int dt_prov_kprobe_dealloc(struct dt_probe *dtp, struct dt_softc *sc,
    struct dtioc_req *dtrq);

void	db_prof_count(struct trapframe *frame);
vaddr_t	db_get_probe_addr(struct trapframe *);

struct kprobe_probe {
	struct dt_probe* dtp;
	SLIST_ENTRY(kprobe_probe) kprobe_next;
};

/* Bob Jenkin's public domain 32-bit integer hashing function.
 * Original at https://burtleburtle.net/bob/hash/integer.html.
 */
uint32_t
ptr_hash(uint32_t a) {
	a = (a + 0x7ed55d16) + (a<<12);
	a = (a ^ 0xc761c23c) ^ (a>>19);
	a = (a + 0x165667b1) + (a<<5);
	a = (a + 0xd3a2646c) ^ (a<<9);
	a = (a + 0xfd7046c5) + (a<<3);
	a = (a ^ 0xb55a4f09) ^ (a>>16);
	return a;
}

#define PPTSIZE		PAGE_SIZE * 30
#define	PPTMASK		((PPTSIZE / sizeof(struct kprobe_probe)) - 1)
#define INSTTOIDX(inst)	(ptr_hash(inst) & PPTMASK)

SLIST_HEAD(, kprobe_probe) *dtpf_entry;
SLIST_HEAD(, kprobe_probe) *dtpf_return;
int nb_probes_entry =	0;
int nb_probes_return =	0;

#define DTEVT_PROV_KPROBE (DTEVT_COMMON|DTEVT_FUNCARGS)

#define KPROBE_ENTRY "entry"
#define KPROBE_RETURN "return"

#if defined(__amd64__)
#define KPROBE_RETGUARD_MOV_1 0x4c
#define KPROBE_RETGUARD_MOV_2 0x8b
#define KPROBE_RETGUARD_MOV_3 0x1d

#define KPROBE_RETGUARD_MOV_SIZE 7

#define KPROBE_RETGUARD_XOR_1 0x4c
#define KPROBE_RETGUARD_XOR_2 0x33
#define KPROBE_RETGUARD_XOR_3 0x1c

#define KPROBE_RETGUARD_XOR_SIZE 4

#define RET		0xc3
#define RET_SIZE	1
#elif defined(__i386__)
#define POP_RBP		0x5d
#define POP_RBP_SIZE	1
#endif

struct dt_provider dt_prov_kprobe = {
	.dtpv_name    = "kprobe",
	.dtpv_alloc   = dt_prov_kprobe_alloc,
	.dtpv_enter   = dt_prov_kprobe_hook,
	.dtpv_leave   = NULL,
	.dtpv_dealloc = dt_prov_kprobe_dealloc,
};

extern db_symtab_t db_symtab;
extern char __kutext_end[];
extern int db_prof_on;

/* Initialize all entry and return probes and store them in global arrays */
int
dt_prov_kprobe_init(void)
{
	struct dt_probe *dtp;
	struct kprobe_probe *kprobe_dtp;
	Elf_Sym *symp, *symtab_start, *symtab_end;
	char *strtab, *name;
	vaddr_t inst, limit;
	int nb_sym, nb_probes;

	nb_sym = (db_symtab.end - db_symtab.start) / sizeof (Elf_Sym);
	nb_probes = nb_probes_entry = nb_probes_return = 0;

	dtpf_entry = malloc(PPTSIZE, M_DT, M_NOWAIT|M_ZERO);
	if (dtpf_entry == NULL)
		goto end;

	dtpf_return = malloc(PPTSIZE, M_DT, M_NOWAIT|M_ZERO);
	if (dtpf_return == NULL)
		goto end;

	db_symtab_t *stab = &db_symtab;

	symtab_start = STAB_TO_SYMSTART(stab);
	symtab_end = STAB_TO_SYMEND(stab);

	strtab = db_elf_find_strtab(stab);

	for (symp = symtab_start; symp < symtab_end; symp++) {
		if (ELF_ST_TYPE(symp->st_info) != STT_FUNC)
			continue;

		inst = symp->st_value;
		name = strtab + symp->st_name;
		limit = symp->st_value + symp->st_size;

		/* Filter function that are not mapped in memory */
		if (inst < KERNBASE || inst >= (vaddr_t)&__kutext_end)
			continue;

		/* Remove some function to avoid recursive tracing */
		if (strncmp(name, "dt_", 3) == 0 || strncmp(name, "trap", 4) == 0 ||
		    strncmp(name, "db_", 3) == 0)
			continue;

#if defined(__amd64__)
		/* Find if there is a retguard, if so move the inst pointer to the later 'push rbp' */
		if (*((uint8_t *)inst) != SSF_INST) {
			/* No retguards in i386 */
			if (((uint8_t *)inst)[0] != KPROBE_RETGUARD_MOV_1 ||
				((uint8_t *)inst)[1] != KPROBE_RETGUARD_MOV_2 ||
				((uint8_t *)inst)[2] != KPROBE_RETGUARD_MOV_3 ||
				((uint8_t *)inst)[KPROBE_RETGUARD_MOV_SIZE] != KPROBE_RETGUARD_XOR_1 ||
				((uint8_t *)inst)[KPROBE_RETGUARD_MOV_SIZE + 1] != KPROBE_RETGUARD_XOR_2 ||
				((uint8_t *)inst)[KPROBE_RETGUARD_MOV_SIZE + 2] != KPROBE_RETGUARD_XOR_3 ||
				((uint8_t *)inst)[KPROBE_RETGUARD_MOV_SIZE + KPROBE_RETGUARD_XOR_SIZE] != SSF_INST)
				continue;
			inst = (vaddr_t)&(((uint8_t *)inst)[KPROBE_RETGUARD_MOV_SIZE + KPROBE_RETGUARD_XOR_SIZE]);
		}
#elif defined(__i386__)
		if (*((uint8_t *)inst) != SSF_INST)
			continue;
#endif

		dtp = dt_dev_alloc_probe(name, KPROBE_ENTRY, &dt_prov_kprobe);
		if (dtp == NULL)
			goto end;

		kprobe_dtp = malloc(sizeof(struct kprobe_probe), M_TEMP, M_NOWAIT|M_ZERO);
		if (kprobe_dtp == NULL)
			goto end;
		kprobe_dtp->dtp = dtp;

		dtp->dtp_addr = inst;
		dtp->dtp_nargs = db_ctf_func_numargs(symp);
		dt_dev_register_probe(dtp);

		SLIST_INSERT_HEAD(&dtpf_entry[INSTTOIDX(dtp->dtp_addr)], kprobe_dtp, kprobe_next);

		nb_probes++;
		nb_probes_entry++;

		/*
		 *  Poor method to find the return point
		 *  => we would need a disassembler to find all return points
		 *  For now we start from the end of the function, iterate on
		 *  int3 inserted for retguard until we find a ret
		 */
#if defined(__amd64__)
		if (*(uint8_t *)(limit - 1) != RET)
			continue;
		inst = limit - 1;
#elif defined(__i386__)
		/*
		 * Little temporary hack to find some return probe
		 *   => always int3 after 'pop %rpb; ret'
		 */
		while(*((uint8_t *)inst) == 0xcc)
			(*(uint8_t *)inst) -= 1;
		if (*(uint8_t *)(limit - 2) != POP_RBP)
			continue;
		inst = limit - 2;
#endif

		dtp = dt_dev_alloc_probe(name, KPROBE_RETURN, &dt_prov_kprobe);
		if (dtp == NULL)
			goto end;

		kprobe_dtp = malloc(sizeof(struct kprobe_probe), M_TEMP, M_NOWAIT|M_ZERO);
		if (kprobe_dtp == NULL)
			goto end;
		kprobe_dtp->dtp = dtp;

		dtp->dtp_addr = inst;
		dt_dev_register_probe(dtp);
		SLIST_INSERT_HEAD(&dtpf_return[INSTTOIDX(dtp->dtp_addr)], kprobe_dtp, kprobe_next);
		nb_probes++;
		nb_probes_return++;
	}
end:
	return nb_probes;
}

int
dt_prov_kprobe_alloc(struct dt_probe *dtp, struct dt_softc *sc,
    struct dt_pcb_list *plist, struct dtioc_req *dtrq)
{
	uint8_t patch = BKPT_INST;
	struct dt_pcb *dp;
	unsigned s;

	dp = dt_pcb_alloc(dtp, sc);
	if (dp == NULL)
		return ENOMEM;

	/* Patch only if it's first pcb referencing this probe */
	dtp->dtp_ref++;
	KASSERT(dtp->dtp_ref != 0);

	if (dtp->dtp_ref == 1) {
		s = intr_disable();
		db_write_bytes(dtp->dtp_addr, BKPT_SIZE, &patch);
		intr_restore(s);
	}

	dp->dp_filter = dtrq->dtrq_filter;
	dp->dp_evtflags = dtrq->dtrq_evtflags & DTEVT_PROV_KPROBE;
	TAILQ_INSERT_HEAD(plist, dp, dp_snext);
	return 0;
}

int
dt_prov_kprobe_dealloc(struct dt_probe *dtp, struct dt_softc *sc,
   struct dtioc_req *dtrq)
{
	uint8_t patch;
	int size;
	unsigned s;

	if (strcmp(dtp->dtp_name, KPROBE_ENTRY) == 0) {
		patch = SSF_INST;
		size  = SSF_SIZE;
	} else if (strcmp(dtp->dtp_name, KPROBE_RETURN) == 0) {
#if defined(__amd64__)
		patch = RET;
		size  = RET_SIZE;
#elif defined(__i386__)
		patch = POP_RBP;
		size  = POP_RBP_SIZE;
#endif
	} else
		KASSERT(0 && "Trying to dealloc not yet implemented probe type");

	dtp->dtp_ref--;

	if (dtp->dtp_ref == 0) {
		s = intr_disable();
		db_write_bytes(dtp->dtp_addr, size, &patch);
		intr_restore(s);
	}

	/* Deallocation of PCB is done by dt_pcb_purge when closing the dev */
	return 0;
}

int
dt_prov_kprobe_hook(struct dt_provider *dtpv, ...)
{
	struct dt_probe *dtp;
	struct dt_pcb *dp;
	struct trapframe *tf;
	struct kprobe_probe *kprobe_dtp;
	va_list ap;
	int is_dt_bkpt = 0;
	int error;	/* Return values for return probes*/
	vaddr_t *args, addr;
	size_t argsize;
	register_t retval[2];

	KASSERT(dtpv == &dt_prov_kprobe);

	va_start(ap, dtpv);
	tf = va_arg(ap, struct trapframe*);
	va_end(ap);

	addr = db_get_probe_addr(tf);

	SLIST_FOREACH(kprobe_dtp, &dtpf_entry[INSTTOIDX(addr)], kprobe_next) {
		dtp = kprobe_dtp->dtp;

		if (dtp->dtp_addr != addr)
			continue;

		is_dt_bkpt = 1;
		if (db_prof_on)
			db_prof_count(tf);

		if (!dtp->dtp_recording)
			continue;

		smr_read_enter();
		SMR_SLIST_FOREACH(dp, &dtp->dtp_pcbs, dp_pnext) {
			struct dt_evt *dtev;

			dtev = dt_pcb_ring_get(dp, 0);
			if (dtev == NULL)
				continue;

#if defined(__amd64__)
			args = (vaddr_t *)tf->tf_rdi;
			/* XXX: use CTF to get the number of arguments. */
			argsize = 6;
#elif defined(__i386__)
			/* All args on stack */
			args = (vaddr_t *)(tf->tf_esp + 4);
			argsize = 10;
#endif

			if (ISSET(dp->dp_evtflags, DTEVT_FUNCARGS))
				memcpy(dtev->dtev_args, args, argsize);

			dt_pcb_ring_consume(dp, dtev);
		}
		smr_read_leave();
	}

	if (is_dt_bkpt)
		return is_dt_bkpt;

	SLIST_FOREACH(kprobe_dtp, &dtpf_return[INSTTOIDX(addr)], kprobe_next) {
		dtp = kprobe_dtp->dtp;

		if (dtp->dtp_addr != addr)
			continue;

		is_dt_bkpt = 2;

		if (!dtp->dtp_recording)
			continue;

		smr_read_enter();
		SMR_SLIST_FOREACH(dp, &dtp->dtp_pcbs, dp_pnext) {
			struct dt_evt *dtev;

			dtev = dt_pcb_ring_get(dp, 0);
			if (dtev == NULL)
				continue;

#if defined(__amd64__)
			retval[0] = tf->tf_rax;
			retval[1] = 0;
			error = 0;
#elif defined(__i386)
			retval[0] = tf->tf_eax;
			retval[1] = 0;
			error = 0;
#endif

			dtev->dtev_retval[0] = retval[0];
			dtev->dtev_retval[1] = retval[1];
			dtev->dtev_error = error;

			dt_pcb_ring_consume(dp, dtev);
		}
		smr_read_leave();
	}
	return is_dt_bkpt;
}

/* Function called by ddb to patch all functions without allocating 1 pcb per probe */
void
dt_prov_kprobe_patch_all_entry(void)
{
	uint8_t patch = BKPT_INST;
	struct dt_probe *dtp;
	struct kprobe_probe *kprobe_dtp;
	size_t i;

	for (i = 0; i < PPTMASK; ++i) {
		SLIST_FOREACH(kprobe_dtp, &dtpf_entry[i], kprobe_next) {
			dtp = kprobe_dtp->dtp;
			dtp->dtp_ref++;

			if (dtp->dtp_ref == 1) {
				unsigned s;
				s = intr_disable();
				db_write_bytes(dtp->dtp_addr, BKPT_SIZE, &patch);
				intr_restore(s);
			}
		}
	}
}

/* Function called by ddb to patch all functions without allocating 1 pcb per probe */
void
dt_prov_kprobe_depatch_all_entry(void)
{
	uint8_t patch = SSF_INST;
	struct dt_probe *dtp;
	struct kprobe_probe *kprobe_dtp;
	size_t i;

	for (i = 0; i < PPTMASK; ++i) {
		SLIST_FOREACH(kprobe_dtp, &dtpf_entry[i], kprobe_next) {
			dtp = kprobe_dtp->dtp;
			dtp->dtp_ref--;

			if (dtp->dtp_ref == 0) {
				unsigned s;
				s = intr_disable();
				db_write_bytes(dtp->dtp_addr, SSF_SIZE, &patch);
				intr_restore(s);
			}
		}

	}
}
#endif /* __amd64__ || __i386__ */
