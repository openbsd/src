/*	$OpenBSD: tcfs_fileinfo.h,v 1.2 2000/06/17 17:32:27 provos Exp $	*/
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
/*	Gestione informazioni sui files cifrati
*/


typedef struct {	
		 unsigned long flag;
		 unsigned int end_of_file;
		} tcfs_fileinfo;

#define		MBFLAG	0x00000010
#define		SPFLAG	0x000000e0
#define		GSFLAG	0x00000100

#define	FI_CFLAG(x)	(((x)->flag&MBFLAG)>>4)
#define	FI_SPURE(x)	(((x)->flag&SPFLAG)>>5)
#define	FI_GSHAR(x)	(((x)->flag&GSFLAG)>>8)
#define FI_ENDOF(x)	((x)->end_of_file)

#define	FI_SET_CF(x,y)	((x)->flag=\
			 ((x)->flag & (~MBFLAG))|((y<<4)&MBFLAG))

#define	FI_SET_SP(x,y)	((x)->flag=\
			 ((x)->flag & (~SPFLAG))|((y<<5)&SPFLAG))

#define	FI_SET_GS(x,y)	((x)->flag=\
			 ((x)->flag & (~GSFLAG))|((y<<8)&GSFLAG))

/*	prototipi	*/

tcfs_fileinfo 	tcfs_get_fileinfo(void *);
tcfs_fileinfo 	tcfs_xgetflags(struct vnode *,struct proc *,struct ucred*);
int 		tcfs_set_fileinfo(void *, tcfs_fileinfo *);
int 		tcfs_xsetflags(struct vnode *, struct proc *, struct ucred *, tcfs_fileinfo *);

