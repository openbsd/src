/* $OpenBSD: locore_c_routines.c,v 1.1.1.1 2004/04/21 15:24:01 aoyama Exp $	*/
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

#include "assym.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>

#include <machine/board.h>		/* m188 bit defines	*/
#include <machine/cmmu.h>		/* DMT_VALID		*/
#include <machine/asm.h>		/* END_OF_VECTOR_LIST, etc.	*/
#include <machine/asm_macro.h>		/* enable/disable interrupts	*/
#include <machine/cpu_number.h>		/* cpu_number()		*/
#include <machine/locore.h>
#include <machine/trap.h>
#ifdef M88100
#include <machine/m88100.h>
#endif

#ifdef DDB
#include <ddb/db_output.h>
#endif /* DDB */

#if defined(DDB) && defined(JEFF_DEBUG)
#define DATA_DEBUG
#endif

#if DDB
#define DEBUG_MSG db_printf
#else
#define DEBUG_MSG printf
#endif /* DDB */

typedef struct {
	unsigned word_one, word_two;
} m88k_exception_vector_area;

extern unsigned int *volatile int_mask_reg[MAX_CPUS]; /* in machdep.c */
extern unsigned master_cpu;      /* in cmmu.c */

/* FORWARDS */
void setlevel(unsigned int);
void vector_init(m88k_exception_vector_area *, unsigned *);

#ifdef M88100

/*
 *  data access emulation for M88100 exceptions
 */

#define DMT_BYTE	1
#define DMT_HALF	2
#define DMT_WORD	4

const struct {
	unsigned char    offset;
	unsigned char    size;
} dmt_en_info[16] = {
	{0, 0}, {3, DMT_BYTE}, {2, DMT_BYTE}, {2, DMT_HALF},
	{1, DMT_BYTE}, {0, 0}, {0, 0}, {0, 0},
	{0, DMT_BYTE}, {0, 0}, {0, 0}, {0, 0},
	{0, DMT_HALF}, {0, 0}, {0, 0}, {0, DMT_WORD}
};

#ifdef DATA_DEBUG
int data_access_emulation_debug = 0;
#define DAE_DEBUG(stuff) \
	do { \
		if (data_access_emulation_debug != 0) { \
			stuff; \
		} \
	} while (0)
#else
#define DAE_DEBUG(stuff)
#endif

void
dae_print(unsigned *eframe)
{
	int x;
	unsigned dmax, dmdx, dmtx;

	if (!ISSET(eframe[EF_DMT0], DMT_VALID))
		return;

	for (x = 0; x < 3; x++) {
		dmtx = eframe[EF_DMT0 + x * 3];
		if (!ISSET(dmtx, DMT_VALID))
			continue;

		dmdx = eframe[EF_DMD0 + x * 3];
		dmax = eframe[EF_DMA0 + x * 3];

		if (ISSET(dmtx, DMT_WRITE))
			printf("[DMT%d=%x: st.%c %x to %x as %d %s %s]\n",
			    x, dmtx, dmtx & DMT_DAS ? 's' : 'u', dmdx, dmax,
			    DMT_ENBITS(dmtx),
			    dmtx & DMT_DOUB1 ? "double": "not double",
			    dmtx & DMT_LOCKBAR ? "xmem": "not xmem");
		else
			printf("[DMT%d=%x: ld.%c r%d <- %x as %d %s %s]\n",
			    x, dmtx, dmtx & DMT_DAS ? 's' : 'u',
			    DMT_DREGBITS(dmtx), dmax, DMT_ENBITS(dmtx),
			    dmtx & DMT_DOUB1 ? "double": "not double",
			    dmtx & DMT_LOCKBAR ? "xmem": "not xmem");
	}
}

void
data_access_emulation(unsigned *eframe)
{
	int x;
	unsigned dmax, dmdx, dmtx;
	unsigned v, reg;

	dmtx = eframe[EF_DMT0];
	if (!ISSET(dmtx, DMT_VALID))
		return;

	for (x = 0; x < 3; x++) {
		dmtx = eframe[EF_DMT0 + x * 3];
		if (!ISSET(dmtx, DMT_VALID) || ISSET(dmtx, DMT_SKIP))
			continue;

		dmdx = eframe[EF_DMD0 + x * 3];
		dmax = eframe[EF_DMA0 + x * 3];

      DAE_DEBUG(
		if (ISSET(dmtx, DMT_WRITE))
			printf("[DMT%d=%x: st.%c %x to %x as %d %s %s]\n",
			    x, dmtx, dmtx & DMT_DAS ? 's' : 'u', dmdx, dmax,
			    DMT_ENBITS(dmtx),
			    dmtx & DMT_DOUB1 ? "double": "not double",
			    dmtx & DMT_LOCKBAR ? "xmem": "not xmem");
		else
			printf("[DMT%d=%x: ld.%c r%d <- %x as %d %s %s]\n",
			    x, dmtx, dmtx & DMT_DAS ? 's' : 'u',
			    DMT_DREGBITS(dmtx), dmax, DMT_ENBITS(dmtx),
			    dmtx & DMT_DOUB1 ? "double": "not double",
			    dmtx & DMT_LOCKBAR ? "xmem": "not xmem")
	);

		dmax += dmt_en_info[DMT_ENBITS(dmtx)].offset;
		reg = DMT_DREGBITS(dmtx);

		if (!ISSET(dmtx, DMT_LOCKBAR)) {
			/* the fault is not during an XMEM */

			if (x == 2 && ISSET(dmtx, DMT_DOUB1)) {
				/* pipeline 2 (earliest stage) for a double */

				if (ISSET(dmtx, DMT_WRITE)) {
					/*
					 * STORE DOUBLE WILL BE REINITIATED
					 * BY rte
					 */
				} else {
					/* EMULATE ld.d INSTRUCTION */
					v = do_load_word(dmax, dmtx & DMT_DAS);
					if (reg != 0)
						eframe[EF_R0 + reg] = v;
					v = do_load_word(dmax ^ 4,
					    dmtx & DMT_DAS);
					if (reg != 31)
						eframe[EF_R0 + reg + 1] = v;
				}
			} else {
				/* not pipeline #2 with a double */
				if (dmtx & DMT_WRITE) {
					switch (dmt_en_info[DMT_ENBITS(dmtx)].size) {
					case DMT_BYTE:
					DAE_DEBUG(
						DEBUG_MSG("[byte %x -> [%x(%c)]\n",
						    dmdx & 0xff, dmax,
						    ISSET(dmtx, DMT_DAS) ? 's' : 'u')
					);
						do_store_byte(dmax, dmdx,
						    dmtx & DMT_DAS);
						break;
					case DMT_HALF:
					DAE_DEBUG(
						DEBUG_MSG("[half %x -> [%x(%c)]\n",
						    dmdx & 0xffff, dmax,
						    ISSET(dmtx, DMT_DAS) ? 's' : 'u')
					);
						do_store_half(dmax, dmdx,
						    dmtx & DMT_DAS);
						break;
					case DMT_WORD:
					DAE_DEBUG(
						DEBUG_MSG("[word %x -> [%x(%c)]\n",
						    dmdx, dmax,
						    ISSET(dmtx, DMT_DAS) ? 's' : 'u')
					);
						do_store_word(dmax, dmdx,
						    dmtx & DMT_DAS);
						break;
					}
				} else {
					/* else it's a read */
					switch (dmt_en_info[DMT_ENBITS(dmtx)].size) {
					case DMT_BYTE:
						v = do_load_byte(dmax,
						    dmtx & DMT_DAS);
						if (!ISSET(dmtx, DMT_SIGNED))
							v &= 0x000000ff;
						break;
					case DMT_HALF:
						v = do_load_half(dmax,
						    dmtx & DMT_DAS);
						if (!ISSET(dmtx, DMT_SIGNED))
							v &= 0x0000ffff;
						break;
					case DMT_WORD:
						v = do_load_word(dmax,
						    dmtx & DMT_DAS);
						break;
					}
					DAE_DEBUG(
						if (reg == 0)
							DEBUG_MSG("[no write to r0 done]\n");
						else
							DEBUG_MSG("[r%d <- %x]\n", reg, v);
					);
					if (reg != 0)
						eframe[EF_R0 + reg] = v;
				}
			}
		} else {
			/* if lockbar is set... it's part of an XMEM */
			/*
			 * According to Motorola's "General Information",
			 * the DMT_DOUB1 bit is never set in this case, as it
			 * should be.
			 * If lockbar is set (as it is if we're here) and if
			 * the write is not set, then it's the same as if DOUB1
			 * was set...
			 */
			if (!ISSET(dmtx, DMT_WRITE)) {
				if (x != 2) {
					/* RERUN xmem WITH DMD(x+1) */
					x++;
					dmdx = eframe[EF_DMD0 + x * 3];
				} else {
					/* RERUN xmem WITH DMD2 */
				}

				if (dmt_en_info[DMT_ENBITS(dmtx)].size ==
				    DMT_WORD) {
					v = do_xmem_word(dmax, dmdx,
					    dmtx & DMT_DAS);
				} else {
					v = do_xmem_byte(dmax, dmdx,
					    dmtx & DMT_DAS);
				}
				if (reg != 0)
					eframe[EF_R0 + reg] = v;
			} else {
				if (x == 0) {
					if (reg != 0)
						eframe[EF_R0 + reg] = dmdx;
					eframe[EF_SFIP] = eframe[EF_SNIP];
					eframe[EF_SNIP] = eframe[EF_SXIP];
					eframe[EF_SXIP] = 0;
					/* xmem RERUN ON rte */
					eframe[EF_DMT0] = 0;
					return;
				}
			}
		}
	}
	eframe[EF_DMT0] = 0;
}
#endif /* M88100 */

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

	for (; num <= SIGSYS_MAX; num++)
		SET_VECTOR(num, sigsys);

	for (; num <= SIGTRAP_MAX; num++)
		SET_VECTOR(num, sigtrap);

	SET_VECTOR(450, syscall_handler);
	SET_VECTOR(504, stepbpt);
	SET_VECTOR(511, userbpt);

	/* GCC will by default produce explicit trap 503 for division by zero */
	SET_VECTOR(503, vector_init_list[T_ZERODIV]);
}

unsigned int luna88k_curspl[MAX_CPUS] = {0, 0, 0, 0};

unsigned int int_mask_val[INT_LEVEL] = {
	INT_MASK_LV0,
	INT_MASK_LV1,
	INT_MASK_LV2,
	INT_MASK_LV3,
	INT_MASK_LV4,
	INT_MASK_LV5,
	INT_MASK_LV6,
	INT_MASK_LV7
};

unsigned int int_set_val[INT_LEVEL] = {
	INT_SET_LV0,
	INT_SET_LV1,
	INT_SET_LV2,
	INT_SET_LV3,
	INT_SET_LV4,
	INT_SET_LV5,
	INT_SET_LV6,
	INT_SET_LV7
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
	unsigned int set_value;
	int cpu = cpu_number();

	set_value = int_set_val[level];

	if (cpu != master_cpu)
		set_value &= INT_SLAVE_MASK;

#if 0
	set_value &= ISR_SOFTINT_EXCEPT_MASK(cpu);
	set_value &= ~blocked_interrupts_mask;
#endif

	*int_mask_reg[cpu] = set_value;
#if 0
	int_mask_shadow[cpu] = set_value;
#endif
	luna88k_curspl[cpu] = level;
}

unsigned
getipl(void)
{
	unsigned curspl;
	m88k_psr_type psr; /* processor status register */

	psr = disable_interrupts_return_psr();
	curspl = luna88k_curspl[cpu_number()];
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
	curspl = luna88k_curspl[cpu_number()];
	setlevel(level);

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
	curspl = luna88k_curspl[cpu_number()];
	if (curspl < level)
		setlevel(level);

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
