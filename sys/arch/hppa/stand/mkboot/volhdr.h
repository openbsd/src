/*	$OpenBSD: volhdr.h,v 1.1.1.1 1998/06/23 18:46:43 mickey Exp $	*/
/*	$NetBSD: volhdr.h,v 1.4 1994/10/26 07:28:08 cgd Exp $	*/

/*
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
#define	DIR_TYPE	-5822
#define	DIR_ISL		-12800
#define DIR_FLAG	0x8001	/* dont ask me! */
#define	SECTSIZE	256
