/*	$OpenBSD: core.h,v 1.2 2002/07/22 01:20:50 art Exp $	*/
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
#include <sys/core.h>
#include <sys/stat.h>

struct pstate;

struct corefile {
	char	       *path;
	struct stat	cfstat;
	struct core    *chdr;		/* core header */
	struct coreseg **segs;
	struct reg     *regs;
	void	       *c_stack;	/* pointer to the top of the stack */
};

int	read_core(const char *, struct pstate *);
void	free_core(struct pstate *);
void	core_printregs(struct corefile *);

ssize_t core_read(struct pstate *, off_t, void *, size_t);
ssize_t core_write(struct pstate *, off_t, void *, size_t);
