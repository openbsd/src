/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890 
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */
/*
 *
 * HISTORY
 * $Log: misc.s,v $
 * Revision 1.1  1995/10/18 12:32:29  deraadt
 * moved from m88k directory
 *
 * Revision 1.1.1.1  1995/10/18 10:54:27  deraadt
 * initial 88k import; code by nivas and based on mach luna88k
 *
 * Revision 2.3  93/01/26  18:01:25  danner
 * 	Conditionalied "#define ASSEMBLER".
 * 	[93/01/25            jfriedl]
 * 
 * Revision 2.2  92/08/03  17:52:14  jfriedl
 * 	created [danner]
 * 
 */

#ifndef ASSEMBLER
  #define ASSEMBLER
#endif

#include <m88k/asm.h>

LABEL(_ff1)
	jmp.n	r1
	ff1	r2, r2

/*
 *       invalidate_pte(pte)
 *
 *       This function will invalidate specified pte indivisibly
 *       to avoid the write-back of used-bit and/or modify-bit into
 *       that pte.  It also returns the pte found in the table.
 */
LABEL(_invalidate_pte)
        or      r3,r0,r0
        xmem    r3,r2,0
	tb1     0,r0,0
        jmp.n   r1
        or      r2,r3,r0
