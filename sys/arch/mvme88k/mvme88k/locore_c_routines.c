/* $OpenBSD: locore_c_routines.c,v 1.5 1999/02/09 06:36:28 smurph Exp $	*/
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
/*
 * HISTORY
 *****************************************************************RCS**/
/* This file created by Omron Corporation, 1990. */

#include <machine/m88100.h> 		/* DMT_VALID		*/
#include <assym.s>			 /* EF_NREGS, etc.	*/
#include <machine/asm.h>		 /* END_OF_VECTOR_LIST, etc. */
#ifdef DDB
  #include <ddb/db_output.h>		 /* db_printf() 	*/
#endif /* DDB */


#if defined(DDB) && defined(JEFF_DEBUG)
# define DATA_DEBUG  1
#endif


#if DDB
#  define DEBUG_MSG db_printf
#else
#  define DEBUG_MSG printf
#endif /* DDB */

/*
 *  data access emulation for M88100 exceptions
 */
#define DMT_BYTE	1
#define DMT_HALF	2
#define DMT_WORD	4

static struct
{
    unsigned char 	offset;
    unsigned char 	size;
} dmt_en_info[16] =
{
    {0, 0}, {3, DMT_BYTE}, {2, DMT_BYTE}, {2, DMT_HALF},
    {1, DMT_BYTE}, {0, 0}, {0, 0}, {0, 0},
    {0, DMT_BYTE}, {0, 0}, {0, 0}, {0, 0},
    {0, DMT_HALF}, {0, 0}, {0, 0}, {0, DMT_WORD}
};

#if DATA_DEBUG
   int data_access_emulation_debug = 0;
   static char *bytes[] =
   {
	    "____", "___x", "__x_", "__xx",
	    "_x__", "_x_x", "_xx_", "_xxx",
	    "x___", "x__x", "x_x_", "x_xx",
	    "xx__", "xx_x", "xxx_", "xxxx",
   };
  #define DAE_DEBUG(stuff) {						\
	if ((data_access_emulation_debug != 0) && (			\
	    data_access_emulation_debug == 0xffffffff)) { stuff ;}   }
#else
  #define DAE_DEBUG(stuff)
#endif

void data_access_emulation(unsigned *eframe)
{
    register int x;
    register struct dmt_reg *dmtx;
    register unsigned  dmax, dmdx;
    register unsigned  v, reg;

    if (!(eframe[EF_DMT0] & DMT_VALID))
	return;

    for (x = 0; x < 3; x++)
    {
	dmtx = (struct dmt_reg *)&eframe[EF_DMT0+x*3];

	if (!dmtx->dmt_valid)
		continue;

	dmdx = eframe[EF_DMD0+x*3];
	dmax = eframe[EF_DMA0+x*3];

	DAE_DEBUG
	(
	  if (dmtx->dmt_write)
	    DEBUG_MSG("[DMT%d=%x: st.%c %x to %x as [%s] %s %s]\n",
	      x, eframe[EF_DMT0+x*3], dmtx->dmt_das ? 's' : 'u',
	      dmdx, dmax, bytes[dmtx->dmt_en], 
	      dmtx->dmt_doub1 ? "double": "not double",
	      dmtx->dmt_lockbar ? "xmem": "not xmem");
	  else
	    DEBUG_MSG("[DMT%d=%x: ld.%c r%d<-%x as [%s] %s %s]\n",
	      x, eframe[EF_DMT0+x*3], dmtx->dmt_das ? 's' : 'u',
	      dmtx->dmt_dreg, dmax, bytes[dmtx->dmt_en], 
	      dmtx->dmt_doub1 ? "double": "not double",
	      dmtx->dmt_lockbar ? "xmem": "not xmem");
	)

	dmax += dmt_en_info[dmtx->dmt_en].offset;
	reg = dmtx->dmt_dreg;

	if ( ! dmtx->dmt_lockbar)
	{
	    /* the fault is not during an XMEM */

	    if (x == 2 && dmtx->dmt_doub1)
	    {
		/* pipeline 2 (earliest stage) for a double */

		if (dmtx->dmt_write)
		{
		    /* STORE DOUBLE WILL BE RE-INITIATED BY rte */
		}
		else
		{
		    /* EMULATE ld.d INSTRUCTION */
			v = do_load_word(dmax, dmtx->dmt_das);
			if (reg != 0)
			    eframe[EF_R0 + reg] = v;
			v = do_load_word(dmax ^ 4, dmtx->dmt_das);
			if (reg != 31)
			    eframe[EF_R0 + reg + 1] = v;
		}
	    }
	    else	 /* not pipeline #2 with a double */
	    {
		if (dmtx->dmt_write) switch (dmt_en_info[dmtx->dmt_en].size)
		{
		  case DMT_BYTE:
		    DAE_DEBUG(DEBUG_MSG("[byte %x -> [%x(%c)]\n",
			dmdx & 0xff, dmax, dmtx->dmt_das ? 's' : 'u'))
		    do_store_byte(dmax, dmdx, dmtx->dmt_das);
		    break;
		  case DMT_HALF:
		    DAE_DEBUG(DEBUG_MSG("[half %x -> [%x(%c)]\n",
			dmdx & 0xffff, dmax, dmtx->dmt_das ? 's' : 'u'))
		    do_store_half(dmax, dmdx, dmtx->dmt_das);
		    break;
		  case DMT_WORD:
		    DAE_DEBUG(DEBUG_MSG("[word %x -> [%x(%c)]\n",
			dmdx, dmax, dmtx->dmt_das ? 's' : 'u'))
		    do_store_word(dmax, dmdx, dmtx->dmt_das);
		    break;
		}
		else	/* else it's a read */
		{
		    switch (dmt_en_info[dmtx->dmt_en].size)
		    {
		      case DMT_BYTE:
			v = do_load_byte(dmax, dmtx->dmt_das);
			if (!dmtx->dmt_signed)
			    v &= 0x000000ff;
			break;
		      case DMT_HALF:
			v = do_load_half(dmax, dmtx->dmt_das);
			if (!dmtx->dmt_signed)
			    v &= 0x0000ffff;
			break;
		      case DMT_WORD:
		      default: /* 'default' just to shut up lint */
			v = do_load_word(dmax, dmtx->dmt_das);
			break;
		    }
	   	    if (reg == 0) {
			DAE_DEBUG(DEBUG_MSG("[no write to r0 done]\n"));
		    }
		    else
		    {
			DAE_DEBUG(DEBUG_MSG("[r%d <- %x]\n",
				reg, v));
			eframe[EF_R0 + reg] = v;
		    }
		}
	    }
	}
	else /* if lockbar is set... it's part of an XMEM */
	{
	    /*
	     * According to Motorola's "General Information",
	     * the dmt_doub1 bit is never set in this case, as it should be.
	     * They call this "general information" - I call it a f*cking bug!
	     *
	     * Anyway, if lockbar is set (as it is if we're here) and if
	     * the write is not set, then it's the same as if doub1
	     * was set...
	     */
	    if ( ! dmtx->dmt_write)
	    {
		if (x != 2)
		{
		    /* RERUN xmem WITH DMD(x+1) */
		    x++;
		    dmdx = eframe[EF_DMD0 + x*3];
		}
		else
		{
		    /* RERUN xmem WITH DMD2 */
		}

		if (dmt_en_info[dmtx->dmt_en].size == DMT_WORD)
		    v = do_xmem_word(dmax, dmdx, dmtx->dmt_das);
		else
		    v = do_xmem_byte(dmax, dmdx, dmtx->dmt_das);
		eframe[EF_R0 + reg] = v;
	    }
	    else
	    {
		if (x == 0)
		{
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
 ***********************************************************************
 ***********************************************************************
 */
#define SIGSYS_MAX      501
#define SIGTRAP_MAX     511

#define EMPTY_BR	0xC0000000U      /* empty "br" instruction */
#define NO_OP 		0xf4005800U      /* "or r0, r0, r0" */

typedef struct
{
	unsigned word_one,
		 word_two;
} m88k_exception_vector_area;

#define BRANCH(FROM, TO) (EMPTY_BR | ((unsigned)(TO) - (unsigned)(FROM)) >> 2)

#define SET_VECTOR(NUM, to, VALUE) {                                       \
	unsigned _NUM = (unsigned)(NUM);                                   \
	unsigned _VALUE = (unsigned)(VALUE);                               \
	vector[_NUM].word_one = NO_OP; 	                                   \
	vector[_NUM].word_two = BRANCH(&vector[_NUM].word_two, _VALUE);    \
}

/*
 * vector_init(vector, vector_init_list)
 *
 * This routine sets up the m88k vector table for the running processor.
 * It is called with a very little stack, and interrupts disabled,
 * so don't call any other functions!
 *	XXX clean this - nivas
 */
void vector_init(
	m88k_exception_vector_area *vector,
	unsigned *vector_init_list)
{
    register unsigned num;
    register unsigned vec;
    extern void sigsys(), sigtrap(), stepbpt(), userbpt();
    extern void syscall_handler();

    for (num = 0; (vec = vector_init_list[num]) != END_OF_VECTOR_LIST; num++)
    {
	if (vec != PREDEFINED_BY_ROM)
	    SET_VECTOR(num, to, vec);
    }
    while (num < 496)
	SET_VECTOR(num++, to, sigsys);
    num++; /* skip 496, BUG ROM vector */

    SET_VECTOR(450, to, syscall_handler);

    while (num <= SIGSYS_MAX)
	SET_VECTOR(num++, to, sigsys);

    while (num <= SIGTRAP_MAX)
	SET_VECTOR(num++, to, sigtrap);

    SET_VECTOR(504, to, stepbpt);
    SET_VECTOR(511, to, userbpt);
#if 0
    vector[496].word_one = 496 * 4;
    vector[497].word_two = 497 * 4;
#endif
}
