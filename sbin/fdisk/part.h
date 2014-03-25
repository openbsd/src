/*	$OpenBSD: part.h,v 1.16 2014/03/25 12:59:03 krw Exp $	*/

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

#ifndef _PART_H
#define _PART_H

struct prt {
	u_int32_t shead, scyl, ssect;
	u_int32_t ehead, ecyl, esect;
	u_int32_t bs;
	u_int32_t ns;
	unsigned char flag;
	unsigned char id;
};

void	PRT_printall(void);
const char *PRT_ascii_id(int);
void PRT_parse(struct disk *, struct dos_partition *, off_t, off_t,
    struct prt *);
void PRT_make(struct prt *, off_t, off_t, struct dos_partition *);
void PRT_print(int, struct prt *, char *);

/* This does CHS -> bs/ns */
void PRT_fix_BN(struct disk *, struct prt *, int);

/* This does bs/ns -> CHS */
void PRT_fix_CHS(struct disk *, struct prt *);

#endif /* _PART_H */
