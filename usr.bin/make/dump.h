/* $OpenBSD: dump.h,v 1.3 2015/01/23 22:35:57 espie Exp $ */
#ifndef _DUMP_H_
#define _DUMP_H_

/*
 * Copyright (c) 2012 Marc Espie.
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
/* implementation of -p option */
extern void dump_data(void);

/* and of graph debugging options */
extern void post_mortem(void);

struct ohash;
/* utility functions for both var and targ */


/* t = sort_ohash_by_name(h): 
 *	returns a NULL terminated array holding hash entries, sorted by name.
 *	free(t) when done with it.
 */
extern void *sort_ohash_by_name(struct ohash *);
/* t = sort_ohash(h, cmp_f);
 *	returns a NULL terminated array holding hash entries, pass comparison
 *	function.
 *	free(t) when done with it.
 */
extern void *sort_ohash(struct ohash *, int (*)(const void *, const void *));

#endif
