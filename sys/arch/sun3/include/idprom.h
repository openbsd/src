/*	$NetBSD: idprom.h,v 1.13 1996/12/17 21:11:07 gwr Exp $	*/

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
 * structure/definitions for the 32 byte id prom found in sun3s
 */

struct idprom {
    unsigned char idp_format;
    unsigned char idp_machtype;
    unsigned char idp_etheraddr[6];
    long          idp_date;
    unsigned char idp_serialnum[3];
    unsigned char idp_checksum;
    unsigned char idp_reserved[16];
};

#define IDPROM_VERSION 1
#define IDPROM_SIZE (sizeof(struct idprom))

/* values for cpu_machine_id */

#define CPU_ARCH_MASK  0xf0
#define SUN3_ARCH      0x10
#define SUN3_IMPL_MASK 0x0f
#define SUN3_MACH_160  0x01
#define SUN3_MACH_50   0x02
#define SUN3_MACH_260  0x03
#define SUN3_MACH_110  0x04
#define SUN3_MACH_60   0x07
#define SUN3_MACH_E    0x08

#ifdef	_KERNEL

extern struct idprom identity_prom;
extern u_char cpu_machine_id;

int idprom_init __P((void));
void idprom_etheraddr __P((u_char *));

#endif	/* _KERNEL */
