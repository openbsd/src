/*	$OpenBSD: compat_util.h,v 1.7 2002/03/14 01:26:49 millert Exp $	*/
/*	$NetBSD: compat_util.h,v 1.1 1995/06/24 20:16:05 christos Exp $	*/

/*
 * Copyright (c) 1994 Christos Zoulas
 * Copyright (c) 1995 Frank van der Linden
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef	_COMPAT_UTIL_H_
#define	_COMPAT_UTIL_H_

#include <uvm/uvm_extern.h>
#include <sys/exec.h>

struct emul;
/* struct proc; */

caddr_t stackgap_init(struct emul *);
void    *stackgap_alloc(caddr_t *, size_t);

struct emul_flags_xtab {
        unsigned long omask;
        unsigned long oval;
        unsigned long nval;
};

int emul_find(struct proc *, caddr_t *, const char *, char *,
		   char **, int);

unsigned long emul_flags_translate(const struct emul_flags_xtab *tab,
		   unsigned long in, unsigned long *leftover);

#define CHECK_ALT_EXIST(p, sgp, root, path) \
    emul_find(p, sgp, root, path, &(path), 0)

#define CHECK_ALT_CREAT(p, sgp, root, path) \
    emul_find(p, sgp, root, path, &(path), 1)

#endif /* !_COMPAT_UTIL_H_ */
