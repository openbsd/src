/*	$OpenBSD: gdt.h,v 1.10 2004/06/13 21:49:16 niklas Exp $	*/
/*	$NetBSD: gdt.h,v 1.7.10.6 2002/08/19 01:22:36 sommerfeld Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John T. Kohl and Charles M. Hannum.
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

#ifndef _LOCORE

struct cpu_info;
struct pcb;
struct pmap;
union descriptor;

void gdt_alloc_cpu(struct cpu_info *);
int gdt_get_slot(void);
void gdt_init(void);
void gdt_init_cpu(struct cpu_info *);
void gdt_reload_cpu(/* XXX struct cpu_info * */ void);
void ldt_alloc(struct pmap *, union descriptor *, size_t);
void ldt_free(struct pmap *);
int tss_alloc(struct pcb *);
void tss_free(int);
void setgdt(int, void *, size_t, int, int, int, int);
#endif

/*
 * The initial GDT size (as a descriptor count), and the maximum
 * GDT size possible.
 *
 * These are actually not arbitrary.  To start with, they have to be
 * multiples of 512 and at least 512, in order to work with the
 * allocation strategy set forth by gdt_init and gdt_grow.  Then, the
 * max cannot exceed 65536 since the selector field of a descriptor is
 * just 16 bits, and used as free list link.
 */

#define MINGDTSIZ 512
#define MAXGDTSIZ 8192
