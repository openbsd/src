/*	$OpenBSD: extern.h,v 1.4 2005/01/07 21:58:14 otto Exp $	*/

/*
 * Copyright (c) 2003 Theo de Raadt <deraadt@openbsd.org>
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

u_short	dkcksum(struct disklabel *);
int	checklabel(struct disklabel *);
double	scale(u_int32_t, char, struct disklabel *);
void	display(FILE *, struct disklabel *, char **, char, int, u_int32_t);
void	display_partition(FILE *, struct disklabel *, char **, int, char);

struct disklabel *readlabel(int);
struct disklabel *makebootarea(char *, struct disklabel *, int);
int	editor(struct disklabel *, int, char *, char *);

int	writelabel(int, char *, struct disklabel *);
extern  char bootarea[], *specname;
extern  int donothing;

#ifdef DOSLABEL
extern  struct dos_partition *dosdp;    /* DOS partition, if found */
#endif
