/*	$OpenBSD: ncrreg.h,v 1.4 2000/03/03 00:54:54 todd Exp $ */
/*	$NetBSD: ncrreg.h,v 1.3 1995/11/30 00:58:56 jtc Exp $ */

/*
 * Copyright (c) 1994 Matthias Pfaller.
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
 *	This product includes software developed by Matthias Pfaller.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _NCRREG_H
#define _NCRREG_H

#define PDMA_ADDRESS	((volatile u_char *) 0xffe00000)
#define	NCR5380		((volatile struct ncr5380 *) 0xffd00000)
#define MIN_PHYS	0x20000

struct ncr5380 {
	volatile u_char	regs[8];	/* use only the odd bytes	*/
};

#define	ncr_data	regs[0]	/* Data register		*/
#define	ncr_icom	regs[1]	/* Initiator command register	*/
#define	ncr_mode	regs[2]	/* Mode register		*/
#define	ncr_tcom	regs[3]	/* Target command register	*/
#define	ncr_idstat	regs[4]	/* Bus status register		*/
#define	ncr_dmstat	regs[5]	/* DMA status register		*/
#define	ncr_trcv	regs[6]	/* Target receive register	*/
#define	ncr_ircv	regs[7]	/* Initiator receive register	*/

#define	GET_5380_REG(rnum)	NCR5380->regs[rnum]
#define	SET_5380_REG(rnum,val)	(NCR5380->regs[rnum] = val)
#define scsi_ienable()		intr_enable(IR_SCSI1)
#define scsi_idisable()		intr_disable(IR_SCSI1)
#define	scsi_clr_ipend()	do { \
		int i = GET_5380_REG(NCR5380_IRCV); \
	} while (0)

#endif /* _NCRREG_H */
