/*	$NetBSD: obio.h,v 1.17 1996/12/17 21:11:09 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file defines addresses in Type 1 space for various devices
 * which can be on the motherboard directly.
 *
 * Supposedly these values are constant across the entire sun3 architecture.
 *
 */

#define OBIO_KEYBD_MS     0x000000
#define OBIO_ZS           0x020000
#define OBIO_EEPROM       0x040000
#define OBIO_CLOCK        0x060000
#define OBIO_MEMERR       0x080000
#define OBIO_INTERREG     0x0A0000
#define OBIO_INTEL_ETHER  0x0C0000
#define OBIO_COLOR_MAP    0x0E0000
#define OBIO_EPROM        0x100000
#define OBIO_AMD_ETHER    0x120000
#define OBIO_NCR_SCSI     0x140000
#define OBIO_RESERVED1    0x160000
#define OBIO_RESERVED2    0x180000
#define OBIO_IOX_BUS      0x1A0000
#define OBIO_DES          0x1C0000
#define OBIO_ECCREG       0x1E0000

#define OBIO_KEYBD_MS_SIZE	0x00008
#define OBIO_ZS_SIZE		0x00008
#define OBIO_EEPROM_SIZE	0x00800
#define OBIO_CLOCK_SIZE		0x00020
#define OBIO_MEMERR_SIZE	0x00008
#define OBIO_INTERREG_SIZE	0x00001
#define OBIO_INTEL_ETHER_SIZE	0x00001
#define OBIO_COLOR_MAP_SIZE	0x00400
#define OBIO_EPROM_SIZE		0x10000
#define OBIO_AMD_ETHER_SIZE	0x00004
#define OBIO_NCR_SCSI_SIZE	0x00020
#define OBIO_DES_SIZE		0x00004
#define OBIO_ECCREG_SIZE	0x00100

#ifdef	_KERNEL

caddr_t	obio_alloc __P((int, int));
void	obio_init __P((void));
caddr_t	obio_find_mapping __P((int pa, int size));
caddr_t	obio_vm_alloc __P((int));

/*
 * These are some OBIO devices that need early init calls.
 */
void	zs_init     __P((void));
void	eeprom_init __P((void));
void	intreg_init __P((void));
void	clock_init  __P((void));

#endif	/* _KERNEL */
