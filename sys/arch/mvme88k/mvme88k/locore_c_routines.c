/* $OpenBSD: locore_c_routines.c,v 1.43 2004/09/30 21:48:57 miod Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/asm.h>		/* END_OF_VECTOR_LIST, etc.	*/
#include <machine/asm_macro.h>		/* enable/disable interrupts	*/
#include <machine/cpu_number.h>		/* cpu_number()		*/
#include <machine/locore.h>
#include <machine/prom.h>
#include <machine/trap.h>

#ifdef MVME188
#include <machine/mvme188.h>
#endif

typedef struct {
	unsigned word_one, word_two;
} m88k_exception_vector_area;

extern unsigned int *volatile int_mask_reg[MAX_CPUS]; /* in machdep.c */
extern unsigned master_cpu;      /* in cmmu.c */

void setlevel(unsigned int);
void vector_init(m88k_exception_vector_area *, unsigned *);

#define SIGSYS_MAX	501
#define SIGTRAP_MAX	510

#define EMPTY_BR	0xc0000000	/* empty "br" instruction */
#define NO_OP 		0xf4005800	/* "or r0, r0, r0" */

#define BRANCH(FROM, TO) \
	(EMPTY_BR | ((unsigned)(TO) - (unsigned)(FROM)) >> 2)

#define SET_VECTOR(NUM, VALUE) \
	do { \
		vector[NUM].word_one = NO_OP; \
		vector[NUM].word_two = BRANCH(&vector[NUM].word_two, VALUE); \
	} while (0)

/*
 * vector_init(vector, vector_init_list)
 *
 * This routine sets up the m88k vector table for the running processor.
 * It is called with a very little stack, and interrupts disabled,
 * so don't call any other functions!
 *	XXX clean this - nivas
 */
void
vector_init(m88k_exception_vector_area *vector, unsigned *vector_init_list)
{
	unsigned num;
	unsigned vec;
	extern void bugtrap(void);
	extern void m88110_bugtrap(void);

	for (num = 0; (vec = vector_init_list[num]) != END_OF_VECTOR_LIST;
	    num++) {
		if (vec != UNKNOWN_HANDLER)
			SET_VECTOR(num, vec);
	}

	/* Save BUG vector */
	bugvec[0] = vector[MVMEPROM_VECTOR].word_one;
	bugvec[1] = vector[MVMEPROM_VECTOR].word_two;

#ifdef M88110
	if (CPU_IS88110) {
		for (; num <= SIGSYS_MAX; num++)
			SET_VECTOR(num, m88110_sigsys);

		for (; num <= SIGTRAP_MAX; num++)
			SET_VECTOR(num, m88110_sigtrap);

		SET_VECTOR(450, m88110_syscall_handler);
		SET_VECTOR(MVMEPROM_VECTOR, m88110_bugtrap);
		SET_VECTOR(504, m88110_stepbpt);
		SET_VECTOR(511, m88110_userbpt);
	}
#endif
#ifdef M88100
	if (CPU_IS88100) {
		for (; num <= SIGSYS_MAX; num++)
			SET_VECTOR(num, sigsys);

		for (; num <= SIGTRAP_MAX; num++)
			SET_VECTOR(num, sigtrap);

		SET_VECTOR(450, syscall_handler);
		SET_VECTOR(MVMEPROM_VECTOR, bugtrap);
		SET_VECTOR(504, stepbpt);
		SET_VECTOR(511, userbpt);
	}
#endif

	/* GCC will by default produce explicit trap 503 for division by zero */
	SET_VECTOR(503, vector_init_list[T_ZERODIV]);

	/* Save new BUG vector */
	sysbugvec[0] = vector[MVMEPROM_VECTOR].word_one;
	sysbugvec[1] = vector[MVMEPROM_VECTOR].word_two;
}

#ifdef MVME188
#if 0
unsigned int int_mask_shadow[MAX_CPUS] = {0, 0, 0, 0};
unsigned int blocked_interrupts_mask;
#endif
unsigned int m188_curspl[MAX_CPUS] = {0, 0, 0, 0};

unsigned int int_mask_val[INT_LEVEL] = {
	MASK_LVL_0,
	MASK_LVL_1,
	MASK_LVL_2,
	MASK_LVL_3,
	MASK_LVL_4,
	MASK_LVL_5,
	MASK_LVL_6,
	MASK_LVL_7
};

/*
 * return next safe spl to reenable interrupts.
 */
unsigned int
safe_level(mask, curlevel)
	unsigned mask;
	unsigned curlevel;
{
	int i;

	for (i = curlevel; i < 8; i++)
		if (!(int_mask_val[i] & mask))
			return i;

	panic("safe_level: no safe level for mask 0x%08x level %d found",
	       mask, curlevel);
	/* NOTREACHED */
}

void
setlevel(unsigned int level)
{
	unsigned int mask;
	int cpu = cpu_number();

	mask = int_mask_val[level];

	if (cpu != master_cpu)
		mask &= SLAVE_MASK;

#if 0
	mask &= ISR_SOFTINT_EXCEPT_MASK(cpu);
	mask &= ~blocked_interrupts_mask;
#endif

	*int_mask_reg[cpu] = mask;
#if 0
	int_mask_shadow[cpu] = mask;
#endif
	m188_curspl[cpu] = level;
}

#endif  /* MVME188 */

unsigned
getipl(void)
{
	unsigned curspl;
	m88k_psr_type psr; /* processor status register */

	psr = disable_interrupts_return_psr();
	switch (brdtyp) {
#ifdef MVME188
	case BRD_188:
		curspl = m188_curspl[cpu_number()];
		break;
#endif /* MVME188 */
#if defined(MVME187) || defined(MVME197)
	case BRD_187:
	case BRD_8120:
	case BRD_197:
		curspl = *md.intr_mask & 0x07;
		break;
#endif /* defined(MVME187) || defined(MVME197) */
	}
	set_psr(psr);
	return curspl;
}

unsigned
setipl(unsigned level)
{
	unsigned curspl;
	m88k_psr_type psr; /* processor status register */

#ifdef DEBUG
	if (level > 7) {
		printf("setipl: invoked with invalid level %x\n", level);
		level &= 0x07;	/* and pray it will work */
	}
#endif

	psr = disable_interrupts_return_psr();
	switch (brdtyp) {
#ifdef MVME188
	case BRD_188:
		curspl = m188_curspl[cpu_number()];
		setlevel(level);
		break;
#endif /* MVME188 */
#if defined(MVME187) || defined(MVME197)
	case BRD_187:
	case BRD_8120:
	case BRD_197:
		curspl = *md.intr_mask & 0x07;
		*md.intr_mask = level;
		break;
#endif /* defined(MVME187) || defined(MVME197) */
	}

	flush_pipeline();

	/* The flush pipeline is required to make sure the above write gets
	 * through the data pipe and to the hardware; otherwise, the next
	 * bunch of instructions could execute at the wrong spl protection
	 */
	set_psr(psr);
	return curspl;
}

unsigned
raiseipl(unsigned level)
{
	unsigned curspl;
	m88k_psr_type psr; /* processor status register */

#ifdef DEBUG
	if (level > 7) {
		printf("raiseipl: invoked with invalid level %x\n", level);
		level &= 0x07;	/* and pray it will work */
	}
#endif

	psr = disable_interrupts_return_psr();
	switch (brdtyp) {
#ifdef MVME188
	case BRD_188:
		curspl = m188_curspl[cpu_number()];
		if (curspl < level)
			setlevel(level);
		break;
#endif /* MVME188 */
#if defined(MVME187) || defined(MVME197)
	case BRD_187:
	case BRD_8120:
	case BRD_197:
		curspl = *md.intr_mask & 0x07;
		if (curspl < level)
			*md.intr_mask = level;
		break;
#endif /* defined(MVME187) || defined(MVME197) */
	}

	flush_pipeline();

	/* The flush pipeline is required to make sure the above write gets
	 * through the data pipe and to the hardware; otherwise, the next
	 * bunch of instructions could execute at the wrong spl protection
	 */
	set_psr(psr);
	return curspl;
}

/* XXX Utterly bogus */
#if NCPUS > 1
#include <sys/simplelock.h>
void
simple_lock_init(lkp)
	struct simplelock *volatile lkp;
{
	lkp->lock_data = 0;
}

int
test_and_set(lock)
	int *volatile lock;
{
#if 0
	int oldlock = *lock;
	if (*lock == 0) {
		*lock = 1;
		return 0;
	}
#else
	return *lock;
	*lock = 1;
	return 0;
#endif
}
#endif
