/*	$OpenBSD: extern.h,v 1.21 2010/11/24 14:15:31 jsing Exp $	*/

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

#define MEG(x)	((x) * 1024LL * (1024 / 512))
#define GIG(x)  (MEG(x) * 1024LL)

u_short	dkcksum(struct disklabel *);
int	checklabel(struct disklabel *);
char	canonical_unit(struct disklabel *, char);
double	scale(u_int64_t, char, struct disklabel *);
void	display(FILE *, struct disklabel *, char, int);
void	display_partition(FILE *, struct disklabel *, int, char);
int	duid_parse(struct disklabel *, char *);

void	readlabel(int);
struct disklabel *makebootarea(char *, struct disklabel *, int);
int	editor(struct disklabel *, int);
void	editor_allocspace(struct disklabel *);
void	mpsave(struct disklabel *);

int	writelabel(int, char *, struct disklabel *);
extern  char bootarea[], *specname, *fstabfile;
extern	char *mountpoints[MAXPARTITIONS];
extern  int aflag, dflag;
extern  int donothing;
extern	int verbose;
extern	char print_unit;
