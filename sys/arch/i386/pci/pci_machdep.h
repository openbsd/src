/*	$OpenBSD: pci_machdep.h,v 1.2 1996/04/18 19:22:23 niklas Exp $	*/
/*	$NetBSD: pci_machdep.h,v 1.4 1996/03/14 02:37:59 cgd Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Machine-specific definitions for PCI autoconfiguration.
 *
 * See the comments in pci_machdep.c for more explanation.
 */

/*
 * Configuration tag; created from a {bus,device,function} triplet by
 * pci_make_tag(), and passed to pci_conf_read() and pci_conf_write().
 * We could instead always pass the {bus,device,function} triplet to
 * the read and write routines, but this would cause extra overhead.
 *
 * Machines other than PCs are likely to use the equivalent of mode 1
 * tags always.  Mode 2 is historical and deprecated by the Revision
 * 2.0 specification.
 */
typedef union {
	u_long mode1;
	struct {
		u_short port;
		u_char enable;
		u_char forward;
	} mode2;
} pcitag_t;

/*
 * Type of a value read from or written to a configuration register.
 * Always 32 bits.
 */
typedef u_int32_t pcireg_t;

/*
 * PCs which use Configuration Mechanism #2 are limited to 16
 * devices per bus.
 */
#define	PCI_MAX_DEVICE_NUMBER	(pci_mode == 2 ? 16 : 32)

/*
 * Hook for PCI bus attach function to do any necessary machine-specific
 * operations.
 */

#define	pci_md_attach_hook(parent, sc, pba)				\
	do {								\
		if (pba->pba_bus == 0)					\
			printf(": configuration mode %d", pci_mode);	\
	} while (0);

/*
 * Miscellaneous variables and functions.
 */
extern int pci_mode;
extern int pci_mode_detect __P((void));
