/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * an abstract heap implementation
 */

/* $arla: heap.h,v 1.3 2003/01/10 12:38:42 lha Exp $ */

#ifndef _ARLAUTIL_HEAP_H
#define _ARLAUTIL_HEAP_H 1

#include "bool.h"

typedef int (*heap_cmp_fn)(const void *, const void *);

typedef unsigned heap_ptr;

struct heap_element {
    const void *data;
    heap_ptr *ptr;
};

typedef struct heap_element heap_element;

struct heap {
    heap_cmp_fn cmp;
    unsigned max_sz;
    unsigned sz;
    heap_element *data;
};

typedef struct heap Heap;

Heap *heap_new (unsigned sz, heap_cmp_fn cmp);

int
heap_insert (Heap *h, const void *data, heap_ptr *ptr);

const void *
heap_head (Heap *h);

void
heap_remove_head (Heap *h);

int
heap_remove (Heap *h, heap_ptr ptr);

void
heap_delete (Heap *h);

Bool
heap_verify (Heap *h);

#endif /* _ARLAUTIL_HEAP_H */
