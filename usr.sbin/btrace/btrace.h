/*	$OpenBSD: btrace.h,v 1.11 2021/12/07 22:17:03 guenther Exp $ */

/*
 * Copyright (c) 2019 - 2020 Martin Pieuchot <mpi@openbsd.org>
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

#ifndef BTRACE_H
#define BTRACE_H

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

struct dt_evt;
struct bt_arg;
struct bt_var;
struct bt_stmt;

/* btrace.c */
const char *		 ba_name(struct bt_arg *);
long			 ba2long(struct bt_arg *, struct dt_evt *);
const char		*ba2str(struct bt_arg *, struct dt_evt *);
long			 bacmp(struct bt_arg *, struct bt_arg *);

/* ksyms.c */
int			 kelf_open(void);
void			 kelf_close(void);
int			 kelf_snprintsym(char *, size_t, unsigned long);

/* map.c */
struct map;
struct hist;
void			 map_clear(struct map *);
void			 map_delete(struct map *, const char *);
struct bt_arg		*map_get(struct map *, const char *);
struct map		*map_insert(struct map *, const char *, struct bt_arg *,
			    struct dt_evt *);
void			 map_print(struct map *, size_t, const char *);
void			 map_zero(struct map *);
struct hist		*hist_increment(struct hist *, const char *, long);
void			 hist_print(struct hist *, const char *);

#define KLEN	512	/* # of characters in map key, contain a stack trace */
#define STRLEN	64	/* maximum # of bytes to output via str() function */

/* printf.c */
int			 stmt_printf(struct bt_stmt *, struct dt_evt *);

/* syscalls.c */
extern const char	*const syscallnames[];

#endif /* BTRACE_H */
