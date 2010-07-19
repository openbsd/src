#ifndef MEMORY_H
#define MEMORY_H

/* $OpenBSD: memory.h,v 1.7 2010/07/19 19:46:44 espie Exp $ */

/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)nonints.h	8.3 (Berkeley) 3/19/94
 */
extern void *emalloc(size_t);
extern char *estrdup(const char *);
extern void *erealloc(void *, size_t);
extern void *ecalloc(size_t, size_t);
extern void *emult_realloc(void *, size_t, size_t);
extern int eunlink(const char *);
extern void esetenv(const char *, const char *);

/* efree(x) works when x==NULL. STDC behavior, may need some different
 * definition for cross-builds on deficient systems */
#define efree	free

extern void *hash_alloc(size_t, void *);
extern void hash_free(void *, size_t, void *);
extern void *element_alloc(size_t, void *);

struct ohash;
/* free_hash(o): free a ohash structure, where each element can be free'd. */
extern void free_hash(struct ohash *);

#endif	/* MEMORY_H */
