/*	$OpenBSD: core.c,v 1.2 2002/07/22 01:20:50 art Exp $	*/
/*
 * Copyright (c) 2002 Jean-Francois Brousseau <krapht@secureops.com>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <err.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "core.h"
#include "pmdb.h"

int
read_core(const char *path, struct pstate *ps)
{
	struct corefile *cf;
	void *core_map;
	off_t c_off;
	int i, cfd;

	cf = (struct corefile *)malloc(sizeof(*cf));
	if (cf == NULL)
		err(1, "malloc");

	cfd = open(path, O_RDONLY, 0600);
	if (cfd < 0)
		err(1, "open() failed on core file");

	if (fstat(cfd, &(cf->cfstat)) < 0)
		err(1, "fstat() failed on core");

	if (cf->cfstat.st_mtimespec.tv_sec < ps->exec_stat.st_mtimespec.tv_sec)
		warnx("executable is more recent than core file!");

	core_map = mmap(NULL, cf->cfstat.st_size, PROT_READ, MAP_SHARED,
	    cfd, 0);
	if (core_map == MAP_FAILED)
		err(1, "mmap() failed on core");

	cf->chdr = (struct core *)core_map;
	c_off = cf->chdr->c_hdrsize;
	if (CORE_GETMAGIC(*(cf->chdr)) != COREMAGIC)
		errx(1, "hey, that's not a core file");

	printf("Core file generated from '%s' by signal %d (SIG%s)\n",
	    cf->chdr->c_name, cf->chdr->c_signo,
	    sys_signame[cf->chdr->c_signo]);

#ifdef DEBUG
	printf("Core: text=0x%lx, data=0x%lx, stack=0x%lx\n",
	    cf->chdr->c_tsize, cf->chdr->c_dsize, cf->chdr->c_ssize);
#endif

	cf->segs = (struct coreseg **)calloc(cf->chdr->c_nseg,
	    sizeof(cf->segs));
	if (cf->segs == NULL)
		err(1, "calloc");

	for (i = 0; i < cf->chdr->c_nseg; i++) {
		cf->segs[i] = (struct coreseg *)(core_map + c_off);
		if (CORE_GETMAGIC(*(cf->segs[i])) != CORESEGMAGIC)
			errx(1, "invalid segment hdr for segment %d", i);

		if (CORE_GETFLAG(*(cf->segs[i])) & CORE_CPU) {
			cf->regs = (struct reg *)
			    ((long) cf->segs[i] + cf->chdr->c_seghdrsize);
		}

		if (CORE_GETFLAG(*(cf->segs[i])) & CORE_STACK)
			cf->c_stack = cf->segs[i] + cf->chdr->c_seghdrsize;

		c_off += cf->chdr->c_seghdrsize + cf->segs[i]->c_size;

#ifdef DEBUG
		(void)printf("seg[%d]: midmag=0x%lx  addr=0x%lx  size=0x%lx\n",
		    i, cf->segs[i]->c_midmag, cf->segs[i]->c_addr,
		    cf->segs[i]->c_size);
#endif
	}

	cf->path = (char *)path;
	ps->ps_flags |= PSF_CORE;
	ps->ps_core = cf;

	return (0);
}

void
free_core(struct pstate *ps)
{
	struct corefile *cf = ps->ps_core;

	if (cf == NULL)
		return;

	if (cf->segs != NULL) {
		free(cf->segs);
		cf->segs = NULL;
	}
}

void
core_printregs(struct corefile *cf)
{
	reg *rg;
	int i;

	rg = (reg *)cf->regs;
	for (i = 0; i < md_def.nregs; i++)
		printf("%s:\t0x%.*lx\n", md_def.md_reg_names[i],
		    (int)(sizeof(reg) * 2), (long) rg[i]);
}


ssize_t
core_read(struct pstate *ps, off_t from, void *to, size_t size)
{
	int i;
	size_t read;
	void *fp;
	struct coreseg *cs;

	for (i = 0; i < ps->ps_core->chdr->c_nseg; i++) {
		cs = ps->ps_core->segs[i];
		if ((from >= cs->c_addr) && (from < (cs->c_addr + cs->c_size))) {
			read = size;
			fp = cs + sizeof(*cs) + (from - cs->c_addr);
			memcpy(to, fp, read);
			return read;
		}
	}

	return (-1);
}


ssize_t
core_write(struct pstate *ps, off_t to, void *from, size_t size)
{
	int i;
	size_t written;
	void *fp;
	struct coreseg *cs;

	for (i = 0; i < ps->ps_core->chdr->c_nseg; i++) {
		cs = ps->ps_core->segs[i];
		if ((to > cs->c_addr) && (to < (cs->c_addr + cs->c_size))) {
			written = size;
			fp = cs + sizeof(*cs) + (to - cs->c_addr);
			memcpy(fp, from, written);
			return written;
		}
	}

	return (-1);
}
