/*	$NetBSD: control.h,v 1.14 1996/11/20 18:57:06 gwr Exp $	*/

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
 * defines for sun3 control space
 */

#define IDPROM_BASE 0x00000000
#define PGMAP_BASE  0x10000000
#define SEGMAP_BASE 0x20000000
#define CONTEXT_REG 0x30000000
#define SYSTEM_ENAB 0x40000000
#define UDVMA_ENAB  0x50000000
#define BUSERR_REG  0x60000000
#define DIAG_REG    0x70000000

#define CONTROL_ADDR_MASK 0x0FFFFFFC


#define NBSEG    0x20000
#define NPMEG    0x100

#define VAC_CACHE_TAGS    0x80000000
#define VAC_CACHE_DATA    0x90000000
#define VAC_FLUSH_BASE    0xA0000000
#define VAC_FLUSH_CONTEXT 0x1
#define VAC_FLUSH_PAGE    0x2
#define VAC_FLUSH_SEGMENT 0x3

#define CONTEXT_0  0x0
#define CONTEXT_1  0x1
#define CONTEXT_2  0x2
#define CONTEXT_3  0x3
#define CONTEXT_4  0x4
#define CONTEXT_5  0x5
#define CONTEXT_6  0x6
#define CONTEXT_7  0x7
#define CONTEXT_NUM 0x8
#define CONTEXT_MASK 0x7

#define SYSTEM_ENAB_DIAG  0x01
#define SYSTEM_ENAB_FPA   0x02
#define SYSTEM_ENAB_COPY  0x04
#define SYSTEM_ENAB_VIDEO 0x08
#define SYSTEM_ENAB_CACHE 0x10
#define SYSTEM_ENAB_SVDMA 0x20
#define SYSTEM_ENAB_FPP   0x40
#define SYSTEM_ENAB_BOOT  0x80

#include <sys/types.h>

unsigned char get_control_byte __P((char *));
unsigned int get_control_word __P((char *));
void set_control_byte __P((char *, unsigned char));
void set_control_word __P((char *, unsigned int));

int get_context __P((void));
void set_context __P((int));
     
vm_offset_t get_pte __P((vm_offset_t va));
void set_pte __P((vm_offset_t, vm_offset_t));
     
unsigned char get_segmap __P((vm_offset_t));
void set_segmap __P((vm_offset_t va, unsigned char));
void set_segmap_allctx __P((vm_offset_t va, unsigned char));

