/*	$NetBSD: images.h,v 1.2 1995/03/18 12:28:19 cgd Exp $	*/

/* 
 * Copyright (c) 1994 Philip A. Nelson.
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
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  This is the description of the images information that
 *  follows the BSD label on a pc532 disk (first 1k block).
 *
 *  Phil Nelson, Oct 23, 1991
 *
 *
 *  These structures are expected to reside on the first 1K bytes
 *  of a disk.  Their order and structures as follows:
 *
 *	-------------------
 *	| nothing         |
 *	-------------------
 *	| BSD disk_label  |
 *	-------------------
 *	| pc532 boot_info |
 *	-------------------
 */

/* Constants for the header block.  */
#define	IMAGE_MAGIC 0x6ef2b7d5L

#ifndef MAXIMAGES
#define MAXIMAGES 8
#endif

/* This is the header block. */

struct imageinfo {
	long  ii_magic;		    /* The magic number. */
	short ii_boot_partition;    /* The partition that holds the image. */
	short ii_boot_count;	    /* The number of boot entries. (>=1) */
	short ii_boot_used;	    /* The number of boot entries used. */
	short ii_boot_default;      /* The default boot image. */
	struct {
	    long  boot_address;	    /* The byte address of the boot image.
				       This address is relative to start of
				       the boot partition. */
	    long  boot_size;	    /* The size of the boot image in zones. */
	    long  boot_load_adr;    /* Where to load the image, real memory. */
	    long  boot_run_adr;	    /* The jump address to start the image. */
	    char  boot_name[16];    /* A title for the image. */
	} ii_images[MAXIMAGES];
};

