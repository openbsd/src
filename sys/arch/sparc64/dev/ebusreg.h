/*	$NetBSD: ebusreg.h,v 1.1 1999/06/04 13:29:13 mrg Exp $	*/

/*
 * Copyright (c) 1999 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SPARC64_DEV_EBUSREG_H_
#define _SPARC64_DEV_EBUSREG_H_

/*
 * UltraSPARC `ebus'
 *
 * The `ebus' bus is designed to plug traditional PC-ISA devices into
 * an SPARC system with as few costs as possible, without sacrificing
 * to performance.  Typically, it is implemented in the PCIO IC from
 * SME, which also implements a `hme-compatible' PCI network device
 * (`network').  The ebus has 4 DMA channels, similar to the DMA seen
 * in the ESP SCSI DMA.
 *
 * Typical UltraSPARC systems have a NatSemi SuperIO IC to provide
 * serial ports for the keyboard and mouse (`se'), floppy disk
 * controller (`fdthree'), parallel port controller (`bpp') connected
 * to the ebus, and a PCI-IDE controller (connected directly to the
 * PCI bus, of course), as well as a Siemens Nixdorf SAB82532 dual
 * channel serial controller (`su' providing ttya and ttyb), an MK48T59
 * EEPROM/clock controller (also where the idprom, including the
 * ethernet address, is located), the audio system (`SUNW,CS4231', same
 * as other UltraSPARC and some SPARC systems), and other various
 * internal devices found on traditional SPARC systems such as the
 * `power', `flashprom', etc., devices.
 *
 * The ebus uses an interrupt mapping scheme similar to PCI, though
 * the actual structures are different.
 */

/*
 * ebus PROM structures
 */

struct ebus_regs {
	u_int32_t	hi;		/* high bits of physaddr */ 
	u_int32_t	lo;
	u_int32_t	size;
};

#define	EBUS_PADDR_FROM_REG(reg)	((((paddr_t)((reg)->hi)) << 32UL) | ((paddr_t)(reg)->lo))

struct ebus_ranges {
	u_int32_t	child_hi;	/* child high phys addr */
	u_int32_t	child_lo;	/* child low phys addr */
	u_int32_t	phys_hi;	/* parent high phys addr */
	u_int32_t	phys_mid;	/* parent mid phys addr */
	u_int32_t	phys_lo;	/* parent low phys addr */
	u_int32_t	size;
};

struct ebus_interrupt_map {
	u_int32_t	hi;		/* high phys addr mask */
	u_int32_t	lo;		/* low phys addr mask */
	u_int32_t	intr;		/* interrupt mask */
	int32_t		cnode;		/* child node */
	u_int32_t	cintr;		/* child interrupt */
};

struct ebus_interrupt_map_mask {
	u_int32_t	hi;		/* high phys addr */
	u_int32_t	lo;		/* low phys addr */
	u_int32_t	intr;		/* interrupt */
};

#endif /* _SPARC64_DEV_EBUSREG_H_ */
