/*	$OpenBSD: lifvar.h,v 1.2 1998/09/29 07:32:26 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)volhdr.h	8.1 (Berkeley) 6/10/93
 */

/*
 * vohldr.h: volume header for "LIF" format volumes
 */

struct	lifvol {
	short	vol_id;
	char	vol_label[6];
	u_int	vol_addr;
	short	vol_oct;
	short	vol_dummy;
	u_int	vol_dirsize;
	short	vol_version;
	short	vol_zero;
	u_int	vol_number;
	u_int	vol_lastvol;
	u_int	vol_length;
	char	vol_toc[6];
	char	vol_dummy1[198];

	u_int	ipl_addr;
	u_int	ipl_size;
	u_int	ipl_entry;

	u_int	vol_dummy2;
};

struct	lifdir {
	char	dir_name[10];
	short	dir_type;
	u_int	dir_addr;
	u_int	dir_length;
	char	dir_toc[6];
	short	dir_flag;
	u_int	dir_implement;
};

struct load {
	int address;
	int count;
};

#define VOL_ID		-32768
#define VOL_OCT		4096
#define DIR_SWAP	0x5243
#define	DIR_FS		0xcd38
#define	DIR_IOMAP	0xcd60
#define	DIR_HPUX	0xcd80
#define	DIR_ISL		0xce00
#define	DIR_PAD		0xcffe
#define	DIR_AUTO	0xcfff
#define	DIR_EST		0xd001
#define	DIR_TYPE	0xe942

#define DIR_FLAG	0x8001	/* dont ask me! */
#define	SECTSIZE	256

#define LIF_NUMDIR	8

#define LIF_VOLSTART	0
#define LIF_VOLSIZE	sizeof(struct lifvol)
#define LIF_DIRSTART	2048
#define LIF_DIRSIZE	(LIF_NUMDIR * sizeof(struct lifdir))
#define LIF_FILESTART	8192

#define btolifs(b)	(((b) + (SECTSIZE - 1)) / SECTSIZE)
#define lifstob(s)	((s) * SECTSIZE)
#define lifstodb(s)	((s) * SECTSIZE / DEV_BSIZE)

#ifdef _STANDALONE
int	lif_open __P((char *path, struct open_file *f));
int	lif_close __P((struct open_file *f));
int	lif_read __P((struct open_file *f, void *buf,
		size_t size, size_t *resid));
int	lif_write __P((struct open_file *f, void *buf,
		size_t size, size_t *resid));
off_t	lif_seek __P((struct open_file *f, off_t offset, int where));
int	lif_stat __P((struct open_file *f, struct stat *sb));
int	lif_readdir __P((struct open_file *f, char *name));
#endif
