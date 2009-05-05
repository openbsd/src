/*	$OpenBSD: small.h,v 1.1 2009/05/05 19:35:30 martynas Exp $	*/

/*
 * Copyright (c) 2009 Martynas Venckus <martynas@openbsd.org>
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

extern jmp_buf jabort;
extern char   *mname;
extern char   *home;
extern char   *stype[];

void	settype(int, char **);
void	changetype(int, int);
void	setbinary(int, char **);
void	get(int, char **);
int	getit(int, char **, int, const char *);
void	mabort(int);
void	mget(int, char **);
void	cd(int, char **);
void	disconnect(int, char **);
char   *dotrans(char *);
char   *domap(char *);

