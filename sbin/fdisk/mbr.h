/*	$OpenBSD: mbr.h,v 1.14 2014/03/08 01:54:14 krw Exp $	*/

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

#include <sys/param.h>

#include "part.h"

/* Various constants */
#define MBR_CODE_SIZE 0x1BE
#define MBR_PART_SIZE	0x10
#define MBR_PART_OFF 0x1BE
#define MBR_SIG_OFF 0x1FE


/* MBR type */
struct mbr {
	off_t reloffset;
	off_t offset;
	unsigned char code[MBR_CODE_SIZE];
	struct prt part[NDOSPART];
	unsigned short signature;
};

/* Prototypes */
void MBR_print_disk(char *);
void MBR_print(struct mbr *, char *);
void MBR_parse(struct disk *, char *, off_t, off_t, struct mbr *);
void MBR_make(struct mbr *, char *);
void MBR_init(struct disk *, struct mbr *);
int MBR_read(int, off_t, char *);
int MBR_write(int, off_t, char *);
void MBR_pcopy(struct disk *, struct mbr *);

#endif /* _MBR_H */

