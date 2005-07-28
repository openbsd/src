/*	$OpenBSD: if_pcnreg.h,v 1.1 2005/07/28 01:31:22 brad Exp $	*/
/*	$NetBSD: if_pcnreg.h,v 1.3 2002/09/04 01:36:07 thorpej Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_PCI_IF_PCNREG_H_
#define	_DEV_PCI_IF_PCNREG_H_

/*
 * Register definitions for the AMD PCnet-PCI series of Ethernet
 * chips.
 *
 * These are only the registers that we access directly from PCI
 * space.  Everything else (accessed via the RAP + RDP/BDP) is
 * defined in <dev/ic/lancereg.h>.
 */

/*
 * PCI configuration space.
 */

#define	PCN_PCI_CBIO	(PCI_MAPREG_START + 0x00)
#define	PCN_PCI_CBMEM	(PCI_MAPREG_START + 0x04)

/*
 * I/O map in Word I/O mode.
 */

#define	PCN16_APROM	0x00
#define	PCN16_RDP	0x10
#define	PCN16_RAP	0x12
#define	PCN16_RESET	0x14
#define	PCN16_BDP	0x16

/*
 * I/O map in DWord I/O mode.
 */

#define	PCN32_APROM	0x00
#define	PCN32_RDP	0x10
#define	PCN32_RAP	0x14
#define	PCN32_RESET	0x18
#define	PCN32_BDP	0x1c

#endif /* _DEV_PCI_IF_PCNREG_H_ */
