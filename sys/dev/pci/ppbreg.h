/*	$OpenBSD: ppbreg.h,v 1.1 1996/04/18 23:48:09 niklas Exp $	*/
/*	$NetBSD: ppbreg.h,v 1.2 1996/03/14 02:35:35 cgd Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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
 */

/*
 * PCI-PCI Bridge chip register definitions and macros.
 * Derived from information found in the ``PCI to PCI Bridge
 * Architecture Specification, Revision 1.0, April 5, 1994.''
 *
 * XXX much is missing.
 */

/*
 * Register offsets
 */
#define	PPB_REG_BASE0		0x10		/* Base Addr Reg. 0 */
#define	PPB_REG_BASE1		0x14		/* Base Addr Reg. 1 */
#define	PPB_REG_BUSINFO		0x18		/* Bus information */
#define	PPB_REG_IOSTATUS	0x1c		/* I/O base+lim & sec stat */
#define	PPB_REG_MEM		0x20		/* Memory base/limit */
#define	PPB_REG_PREFMEM		0x24		/* Pref Mem  base/limit */
#define	PPB_REG_PREFBASE_HI32	0x28		/* Pref Mem base high bits */
#define	PPB_REG_PREFLIM_HI32	0x2c		/* Pref Mem lim high bits */
#define	PPB_REG_IO_HI		0x30		/* I/O base+lim high bits */
#define	PPB_REG_BRIDGECONTROL	PCI_INTERRUPT_REG /* bridge control register */

/*
 * Macros to extract the contents of the "Bus Info" register.
 */
#define	PPB_BUSINFO_PRIMARY(bir)					\
	    ((bir >>  0) & 0xff)
#define	PPB_BUSINFO_SECONDARY(bir)					\
	    ((bir >>  8) & 0xff)
#define	PPB_BUSINFO_SUBORDINATE(bir)					\
	    ((bir >> 16) & 0xff)
#define	PPB_BUSINFO_SECLAT(bir)						\
	    ((bir >> 24) & 0xff)

/*
 * Routine to translate between secondary bus interrupt pin/device number and
 * primary bus interrupt pin number.
 */
#define	PPB_INTERRUPT_SWIZZLE(pin, device)				\
	    ((((pin) + (device) - 1) % 4) + 1)
