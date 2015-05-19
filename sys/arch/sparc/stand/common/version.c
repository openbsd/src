/*	$OpenBSD: version.c,v 1.12 2015/05/19 20:42:11 miod Exp $	*/
/*	$NetBSD: version.c,v 1.4 1995/09/16 23:20:39 pk Exp $ */

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *	NOTE ANY CHANGES YOU MAKE TO THE BOOTBLOCKS HERE.
 *
 *	1.1
 *	1.2	get it to work with V0 bootproms.
 *	1.3	add oldmon support and network support.
 *	1.4	add cd9660 support
 *
 *	2.0	OpenBSD reorganization.
 *	2.1	Bumped RELOC
 *	2.2	ELF support added.
 *	2.3	Bumped RELOC
 *	2.4	Support for larger kernels and fragmented memory layouts.
 *	2.5	sun4e support
 *	2.6	Support for larger kernels when booting from tape, and avoid
 *		stomping on PROM data below 4MB on sun4c
 *	2.7	/etc/random.seed support
 *	2.8	Fix network boot broken on some PROM by the 2.7 changes
 *	2.9	Cope with DMA transfer size limits of old sun4 PROMs
 *	2.10	Allocate more memory beyond the kernel image on sun4d/sun4m
 *		systems to allow for larger kernel page tables
 *	2.11	Only attempt to load /etc/random.seed after a successful
 *		kernel load, and never if booting from tape. This works
 *		around sun4e PROM 1.6, which apparently gets confused by
 *		open() - close() sequences without any I/O in between.
 */

char *version = "2.11";
