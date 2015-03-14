/*	$OpenBSD: mbr.h,v 1.19 2015/03/14 15:21:53 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MBR_H
#define _MBR_H

struct mbr {
	off_t reloffset;
	off_t offset;
	unsigned char code[DOSPARTOFF];
	struct prt part[NDOSPART];
	u_int16_t signature;
};

void MBR_print_disk(char *);
void MBR_print(struct mbr *, char *);
void MBR_parse(struct disk *, struct dos_mbr *, off_t, off_t, struct mbr *);
void MBR_make(struct mbr *, struct dos_mbr *);
void MBR_init(struct disk *, struct mbr *);
int MBR_read(int, off_t, struct dos_mbr *);
int MBR_write(int, off_t, struct dos_mbr *);
void MBR_pcopy(struct disk *, struct mbr *);
char *MBR_readsector(int, off_t);
int MBR_writesector(int, char *, off_t);
void MBR_zapgpt(int, struct dos_mbr *, uint64_t);

#endif /* _MBR_H */
