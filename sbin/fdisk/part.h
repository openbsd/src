/*	$OpenBSD: part.h,v 1.29 2021/07/18 21:40:13 krw Exp $	*/

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

#ifndef _PART_H
#define _PART_H

struct prt {
	uint64_t	prt_bs;
	uint64_t	prt_ns;
	uint32_t	prt_shead, prt_scyl, prt_ssect;
	uint32_t	prt_ehead, prt_ecyl, prt_esect;
	unsigned char	prt_flag;
	unsigned char	prt_id;
};

void		 PRT_printall(void);
void		 PRT_parse(const struct dos_partition *, const uint64_t,
    const uint64_t, struct prt *);
void		 PRT_make(const struct prt *,const uint64_t, const uint64_t,
    struct dos_partition *);
void		 PRT_print(const int, const struct prt *, const char *);
char		*PRT_uuid_to_typename(const struct uuid *);
int		 PRT_uuid_to_type(const struct uuid *);
struct uuid	*PRT_type_to_uuid(const int);
int		 PRT_protected_guid(const struct uuid *);

/* This does CHS -> bs/ns */
void PRT_fix_BN(struct prt *, const int);

/* This does bs/ns -> CHS */
void PRT_fix_CHS(struct prt *);

#endif /* _PART_H */
