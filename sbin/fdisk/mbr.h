/*	$OpenBSD: mbr.h,v 1.4 1998/09/14 03:54:35 rahnds Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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
 *    This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _MBR_H
#define _MBR_H

#include "part.h"

/* Various constants */
#define MBR_CODE_SIZE 0x1B8
#define MBR_PART_SIZE	0x10
#define MBR_PART_OFF 0x1BE
#define MBR_NTSER_OFF 0x1B8
#define MBR_SPARE_OFF 0x1BC		/* Spare short, not used */
#define MBR_SIG_OFF 0x1FE


/* MBR type */
typedef struct _mbr_t {
	off_t reloffset;
	off_t offset;
	unsigned char code[MBR_CODE_SIZE];
	unsigned long nt_serial;
	unsigned short spare;
	prt_t part[NDOSPART];
	unsigned short signature;
} mbr_t;

/* Prototypes */
void MBR_print_disk __P((char *));
void MBR_print __P((mbr_t *));
void MBR_parse __P((disk_t *, char *, off_t, off_t, mbr_t *));
void MBR_make __P((mbr_t *, char *));
int MBR_read __P((int, off_t, char *));
int MBR_write __P((int, off_t, char *));

/* Sanity check */
#include <machine/param.h>
#if (DEV_BSIZE != 512)
#error "DEV_BSIZE != 512, somebody better fix!"
#endif

#endif _MBR_H

