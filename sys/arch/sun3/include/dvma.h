/*	$NetBSD: dvma.h,v 1.3 1996/02/20 22:06:28 gwr Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
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
 * DVMA (Direct Virtual Memory Access - like DMA)
 *
 * The Sun3 MMU is presented to secondary masters using DVMA.
 * Before such devices can access kernel memory, that memory
 * must be mapped into the kernel DVMA space.  All DVMA space
 * is presented as slave-accessible memory for VME and OBIO
 * devices, though not at the same address seen by the CPU.
 *
 * Relevant parts of virtual memory map are:
 *
 * 0FE0.0000  monitor map (devices)
 * 0FF0.0000  DVMA space
 * 0FFE.0000  monitor RAM seg.
 * 0FFF.E000  monitor RAM page
 *
 * Note that while the DVMA harware makes the last 1MB visible
 * for secondary masters, the PROM "owns" the last page of it.
 * Also note that OBIO devices can actually see the last 16MB
 * of kernel virtual space.  That can be mostly ignored, except
 * when calculating the alias address for slave access.
 */

/*
 * To convert an address in DVMA space to a slave address,
 * just use a logical AND with one of the following masks.
 * To convert back, just logical OR with the base address.
 */
#define DVMA_OBIO_SLAVE_BASE 0x0F000000
#define DVMA_OBIO_SLAVE_MASK 0x00FFffff	/* 16MB */

#define DVMA_VME_SLAVE_BASE  0x0FF00000	/*  1MB */
#define DVMA_VME_SLAVE_MASK  0x000Fffff	/*  1MB */


/* DVMA is the last 1MB, but the PROM gets the last page. */
#define	DVMA_SPACE_START	0x0FF00000
#define DVMA_SPACE_END  	0x0FFFE000

/* Allocate/free actual pages of DVMA space. */
caddr_t dvma_malloc(size_t bytes);
void dvma_free(caddr_t addr, size_t bytes);

/* Remap/unmap kernel memory in DVMA space. */
caddr_t dvma_mapin(char *kva, int len);
void dvma_mapout(caddr_t dvma_addr, int len);

/* Convert a kernel DVMA pointer to a slave address. */
long dvma_kvtopa(long kva, int bus);

