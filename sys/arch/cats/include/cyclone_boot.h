/*	$OpenBSD: cyclone_boot.h,v 1.1 2004/02/01 05:12:54 drahn Exp $	*/
/*	$NetBSD: cyclone_boot.h,v 1.1 2001/06/20 22:14:34 chris Exp $	*/

/*
 * Copyright (c) 1997,1998 Mark Brinicombe.
 * Copyright (c) 1997,1998 Causality Limited.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Define the boot structure that is passed to the kernel
 * from the cyclone firmware.
 *
 * The bootloader reserves a page for boot argument info.
 * This page will contain the ebsaboot structure and the
 * kernel argument string.
 */

struct ebsaboot {
	u_int32_t	bt_magic;	/* boot info magic number */
	u_int32_t	bt_vargp;	/* virtual addr of arg page */
	u_int32_t	bt_pargp;	/* physical addr of arg page */
	const char *	bt_args;	/* kernel args string pointer */
	pd_entry_t *	bt_l1;		/* active L1 page table */
	u_int32_t	bt_memstart;	/* start of physical memory */
	u_int32_t	bt_memend;	/* end of physical memory */
	u_int32_t	bt_memavail;	/* start of avail phys memory */
	u_int32_t	bt_fclk;	/* fclk frequency */
	u_int32_t	bt_pciclk;	/* PCI bus frequency */
	u_int32_t	bt_vers;	/* structure version (CATS) */
	u_int32_t	bt_features;	/* feature mask (CATS) */
};

#define BT_MAGIC_NUMBER_EBSA	0x45425341
#define BT_MAGIC_NUMBER_CATS	0x43415453

#define BT_BOOT_VERSION_OLD	0
#define BT_BOOT_VERSION_NEW	1

/* End of cyclone_boot.h */
