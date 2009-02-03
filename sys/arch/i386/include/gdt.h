/*	$OpenBSD: gdt.h,v 1.12 2009/02/03 11:24:19 mikeb Exp $	*/
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
 * Maximum GDT size.  It cannot exceed 65536 since the selector field of
 * a descriptor is just 16 bits, and used as free list link.
 */

#define MAXGDTSIZ 65536
