/*	$OpenBSD: exec.h,v 1.2 1998/07/14 17:21:44 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#ifndef _SA_EXEC_H_
#define _SA_EXEC_H_

#define	MAX_EXEC_NAME	8

#ifdef	EXEC_AOUT
#include <sys/exec_aout.h>
#endif
#ifdef	EXEC_ECOFF
#include <sys/exec_ecoff.h>
#endif
#ifdef	EXEC_ELF
#include <sys/exec_elf.h>
#endif
#ifdef	EXEC_SOM
#include <sys/exec_som.h>
#endif

union x_header {
#ifdef	EXEC_AOUT
	struct exec		x_aout;
#endif
#ifdef	EXEC_ECOFF
	struct ecoff_exechdr	x_ecoff;
#endif
#ifdef	EXEC_ELF
	struct elfhdr		x_elf;
#endif
#ifdef	EXEC_SOM
	struct som_filehdr	x_som;
#endif
};

struct x_param;
struct x_sw {
	char name[MAX_EXEC_NAME];
	/* returns file position to lseek to */
	int (*probe) __P((int, union x_header *));
	/* zero on success */
	int (*load) __P((int, struct x_param *));
};

struct x_param {
	union x_header *xp_hdr;
	const struct x_sw *xp_execsw;
	u_int xp_entry, xp_end;

	struct { u_int addr, size, foff; } text, data, bss, sym, str;
};

extern const struct x_sw execsw[];
void machdep_exec __P((struct x_param *, int, void *));

int aout_probe __P((int, union x_header *));
int aout_load __P((int, struct x_param *));

int elf_probe __P((int, union x_header *));
int elf_load __P((int, struct x_param *));

int ecoff_probe __P((int, union x_header *));
int ecoff_load __P((int, struct x_param *));

int som_probe __P((int, union x_header *));
int som_load __P((int, struct x_param *));

#endif /* _SA_EXEC_H_ */
