/*	$OpenBSD: hpux_exec.h,v 1.1 2004/07/09 21:48:21 mickey Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff.  All rights reserved.
 * Copyright (c) 1995 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: hpux_exec.h 1.6 92/01/20$
 *
 *	@(#)hpux_exec.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _HPUX_EXEC_H_
#define _HPUX_EXEC_H_

/*
 * HPUX SOM header format
 */
struct som_exec {
	long	ha_magic;
	long	som_version;
	struct timespec som_time;
	long	som_ep_space;
	long	som_ep_subspace;
	long	som_ep_offset;
	long	som_auxhdr;
	long	som_auxsize;
	long	pad[22];	/* there is more but we do not care */
	long	som_cksum;
};

struct som_aux {
	long	som_flags;
	long	som_length;		/* of the rest */
	long	som_tsize;		/* .text size */
	long	som_tmem;		/* .text address */
	long	som_tfile;		/* .text offset in file */
	long	som_dsize;
	long	som_dmem;
	long	som_dfile;
	long	som_bsize;
	long	som_entry;
	long	som_ldflags;
	long	som_bfill;
};

#define	HPUX_EXEC_HDR_SIZE \
	(sizeof(struct som_exec) + sizeof(struct som_aux))

#define	HPUX_MAGIC(ha)		((ha)->ha_magic & 0xffff)
#define	HPUX_SYSID(ha)		(((ha)->ha_magic >> 16) & 0xffff)

/*
 * Additional values for HPUX_MAGIC()
 */
#define	HPUX_MAGIC_RELOC	0x0106		/* relocatable object */
#define HPUX_MAGIC_DL		0x010d		/* dynamic load library */
#define	HPUX_MAGIC_SHL		0x010e		/* shared library */

#define	HPUX_SOM_V0		85082112
#define	HPUX_SOM_V1		87102412

#define HPUX_LDPGSZ		4096		/* align to this */
#define HPUX_LDPGSHIFT		12		/* log2(HPUX_LDPGSZ) */

int	exec_hpux_makecmds(struct proc *, struct exec_package *);

#endif /* _HPUX_EXEC_H_ */
