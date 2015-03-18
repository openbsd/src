/*	$OpenBSD: kcore.h,v 1.3 2015/03/18 20:56:40 miod Exp $	*/
/*	$NetBSD: kcore.h,v 1.1 1996/11/09 22:52:22 pk Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

/*
 * The layout of a kernel core on the dump device is as follows:
 *	a `struct kcore_seg' of type CORE_CPU
 *	a `struct cpu_kcore_hdr'
 *	an array of `cpu_kcore_hdr.nmemseg' phys_ram_seg_t's
 *	an array of `cpu_kcore_hdr.npmegs' PTEs (zero of these on sun4ms).
 */

typedef struct cpu_kcore_hdr {
	int	cputype;		/* CPU type associated with this dump */
	int	nmemseg;		/* # of physical memory segments */
	int	memsegoffset;		/* start of memseg array (relative */
					/*  to the start of this header) */
	int	npmeg;			/* # of PMEGs; [sun4/sun4c] only */
	int	pmegoffset;		/* start of pmeg array (relative */
					/*  to the start of this header) */
	struct	segmap segmap_store[NKREG_MAX*NSEGRG];	/* MMU data... */
} cpu_kcore_hdr_t;
