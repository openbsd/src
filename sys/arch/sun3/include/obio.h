/*	$NetBSD: obio.h,v 1.13 1994/12/12 18:59:42 gwr Exp $	*/

/*
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file defines addresses in Type 1 space for various devices
 * which can be on the motherboard directly.
 *
 * Supposedly these values are constant across the entire sun3 architecture.
 *
 */

#define OBIO_KEYBD_MS     0x00000000
#define OBIO_ZS           0x00020000
#define OBIO_EEPROM       0x00040000
#define OBIO_CLOCK        0x00060000
#define OBIO_MEMERR       0x00080000
#define OBIO_INTERREG     0x000A0000
#define OBIO_INTEL_ETHER  0x000C0000
#define OBIO_COLOR_MAP    0x000E0000
#define OBIO_EPROM        0x00100000
#define OBIO_AMD_ETHER    0x00120000
#define OBIO_NCR_SCSI     0x00140000
#define OBIO_RESERVED1    0x00160000
#define OBIO_RESERVED2    0x00180000
#define OBIO_IOX_BUS      0x001A0000
#define OBIO_DES          0x001C0000
#define OBIO_ECCREG       0x001E0000

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
#define OBIO_IO_BUS_SIZE      0x1000000			
#define OBIO_DES_SIZE		0x00004		
#define OBIO_ECCREG_SIZE	0x00100			

caddr_t obio_alloc __P((int, int));
caddr_t obio_vm_alloc __P((int));
caddr_t obio_find_mapping __P((int pa, int size));

