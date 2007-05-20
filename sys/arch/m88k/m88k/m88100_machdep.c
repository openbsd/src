/*	$OpenBSD: m88100_machdep.c,v 1.3 2007/05/20 20:12:31 miod Exp $	*/
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

#include "assym.h"	/* EF_xxx */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/asm_macro.h>
#include <m88k/m88100.h>

/*
 *  Data Access Emulation for M88100 exceptions
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
						printf("[byte %x -> [%x(%c)]\n",
						    dmdx & 0xff, dmax,
						    ISSET(dmtx, DMT_DAS) ? 's' : 'u')
					);
						do_store_byte(dmax, dmdx,
						    dmtx & DMT_DAS);
						break;
					case DMT_HALF:
					DAE_DEBUG(
						printf("[half %x -> [%x(%c)]\n",
						    dmdx & 0xffff, dmax,
						    ISSET(dmtx, DMT_DAS) ? 's' : 'u')
					);
						do_store_half(dmax, dmdx,
						    dmtx & DMT_DAS);
						break;
					case DMT_WORD:
					DAE_DEBUG(
						printf("[word %x -> [%x(%c)]\n",
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
							printf("[no write to r0 done]\n");
						else
							printf("[r%d <- %x]\n", reg, v);
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

/*
 * Routines to patch the kernel code on 88100 systems not affected by
 * the xxx.usr bug.
 */

void
m88100_apply_patches()
{
#ifdef ERRATA__XXX_USR
	if (((get_cpu_pid() & PID_VN) >> VN_SHIFT) > 10) {
		/*
		 * Patch DAE helpers.
		 *	    before		    after
		 *	branch			branch
		 *	NOP			jmp.n r1
		 *	xxx.usr			xxx.usr
		 *	NOP; NOP; NOP
		 *	jmp r1
		 */
		((u_int32_t *)(do_load_word))[1] = 0xf400c401;
		((u_int32_t *)(do_load_half))[1] = 0xf400c401;
		((u_int32_t *)(do_load_byte))[1] = 0xf400c401;
		((u_int32_t *)(do_store_word))[1] = 0xf400c401;
		((u_int32_t *)(do_store_half))[1] = 0xf400c401;
		((u_int32_t *)(do_store_byte))[1] = 0xf400c401;
		((u_int32_t *)(do_xmem_word))[1] = 0xf400c401;
		((u_int32_t *)(do_xmem_byte))[1] = 0xf400c401;
	}
#endif
}
