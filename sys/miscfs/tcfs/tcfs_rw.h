/*	$OpenBSD: tcfs_rw.h,v 1.3 2000/06/17 20:25:55 provos Exp $	*/
/*
 * Copyright 2000 The TCFS Project at http://tcfs.dia.unisa.it/
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TCFS_RW_H_
#define _TCFS_RW_H_

/* Gestione informazioni sui files cifrati */
#include <miscfs/tcfs/tcfs_fileinfo.h>

typedef struct {
	tcfs_fileinfo	i;
	int off;
	int req;
	int tcfs_op_desc;
	int in_boff;
	int in_foff;
	int out_boff;
	int out_foff;
} tcfs_opinfo;

/* tcfs_opinfo x */

#define ROFF(x)		((x)->off%BLOCKSIZE)
#define LAST(x)		((x)->off+(x)->req-1)
#define BOFF(x)		(((x)->off/BLOCKSIZE)*BLOCKSIZE)
#define FOFF(x)		(LAST((x))+BLOCKSIZE-(LAST((x))%BLOCKSIZE+1))
#define P_BOFF(x)	(((x)->off/SBLOCKSIZE)*SBLOCKSIZE)
#define P_FOFF(x)	(LAST((x))+SBLOCKSIZE-(LAST((x))%SBLOCKSIZE+1))
#define SPURE(x)	((x)->req%SBLOCKSIZE?(SBLOCKSIZE-(x)->req%SBLOCKSIZE):0)

/* int o */
#define D_BOFF(o)	(((o)/BLOCKSIZE)*BLOCKSIZE)
#define D_FOFF(o)	((o)+BLOCKSIZE-((o)%BLOCKSIZE+1))
#define D_PFOFF(o)	((o)+SBLOCKSIZE-((o)%SBLOCKSIZE+1))
#define D_SPURE(o)	((o)%SBLOCKSIZE?(SBLOCKSIZE-(o)%SBLOCKSIZE):0)
#define D_NOBLK(o)	((o)/BLOCKSIZE+(o%BLOCKSIZE?1:0))

#define TCFS_NONE	0
#define TCFS_READ_C1	1
#define TCFS_READ_C2	2
#define TCFS_WRITE_C1	3
#define TCFS_WRITE_C2	4
#define TCFS_WRITE_C3	5
#define TCFS_WRITE_C4	6
#define TCFS_WRITE_C5	7

/* prototypes */

char    *tcfs_new_uio_i(struct uio*,struct uio**,tcfs_opinfo*);
char    *tcfs_new_uio_obs(struct uio*,struct uio**,int off, int ireq);
void    tcfs_dispose_new_uio(struct uio *);
void    dispose_new_uio(struct uio *);
int 	tcfs_ed(struct vnode*, struct proc*, struct ucred *, tcfs_fileinfo *);
tcfs_opinfo tcfs_get_opinfo(void*);

#endif /* _TCFS_RW_H_ */
