#ifndef OHASH_H
#define OHASH_H
/* $OpenBSD: ohash.h,v 1.6 2003/08/01 17:38:33 avsm Exp $ */
/* ex:ts=8 sw=4: 
 */

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Code written for the OpenBSD project.
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
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Open hashing support. 
 * Open hashing was chosen because it is much lighter than other hash
 * techniques, and more efficient in most cases.
 */

struct ohash_info {
	ptrdiff_t key_offset;
	void *data;	/* user data */
	void *(*halloc)(size_t, void *);
	void (*hfree)(void *, size_t, void *);
	void *(*alloc)(size_t, void *);
};

struct _ohash_record;

struct ohash {
	struct _ohash_record 	*t;
	struct ohash_info 	info;
	unsigned int 		size;
	unsigned int 		total;
	unsigned int 		deleted;
};

/* For this to be tweakable, we use small primitives, and leave part of the
 * logic to the client application.  e.g., hashing is left to the client
 * application.  We also provide a simple table entry lookup that yields
 * a hashing table index (opaque) to be used in find/insert/remove.
 * The keys are stored at a known position in the client data.
 */
__BEGIN_DECLS
void ohash_init(struct ohash *, unsigned, struct ohash_info *);
void ohash_delete(struct ohash *);

unsigned int ohash_lookup_string(struct ohash *, const char *, u_int32_t);
unsigned int ohash_lookup_interval(struct ohash *, const char *,
	    const char *, u_int32_t);
unsigned int ohash_lookup_memory(struct ohash *, const char *,
	    size_t, u_int32_t)
		__attribute__ ((__bounded__(__string__,2,3)));
void *ohash_find(struct ohash *, unsigned int);
void *ohash_remove(struct ohash *, unsigned int);
void *ohash_insert(struct ohash *, unsigned int, void *);
void *ohash_first(struct ohash *, unsigned int *);
void *ohash_next(struct ohash *, unsigned int *);
unsigned int ohash_entries(struct ohash *);

void *ohash_create_entry(struct ohash_info *, const char *, const char **);
u_int32_t ohash_interval(const char *, const char **);

unsigned int ohash_qlookupi(struct ohash *, const char *, const char **);
unsigned int ohash_qlookup(struct ohash *, const char *);
__END_DECLS
#endif
