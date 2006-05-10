/* $OpenBSD: dl_prebind.h,v 1.2 2006/05/10 03:26:50 deraadt Exp $ */
/*
 * Copyright (c) 2006 Dale Rahn <drahn@dalerahn.com>
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

#include <sys/exec_elf.h>
#include "resolve.h"
#include "prebind.h"

extern char *_dl_noprebind;
extern char *_dl_prebind_validate;
void	_dl_prebind_pre_resolve(void);
void	_dl_prebind_post_resolve(void);
void	*prebind_load_fd(int fd, const char *name);
void	prebind_load_exe(Elf_Phdr *phdp, elf_object_t *exe_obj);

void	prebind_validate(elf_object_t *req_obj, unsigned int symidx, int flags,
	    const Elf_Sym *ref_sym);
extern char *_dl_prebind_validate; /* XXX */

void	prebind_symcache(elf_object_t *object, int pltflag);
void	prebind_free(elf_object_t *object);

extern struct prebind_footer *footer;
