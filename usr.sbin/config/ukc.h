/*	$OpenBSD: ukc.h,v 1.2 2000/01/08 23:23:37 d Exp $ */

/*
 * Copyright (c) 1999 Mats O Jansson.  All rights reserved.
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
 *	This product includes software developed by Mats O Jansson.
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

#ifndef _UKC_H
#define _UKC_H

#define P_LOCNAMES	0
#define S_LOCNAMP	1
#define SA_CFROOTS	2
#define I_CFROOTS_SIZE	3
#define I_PV_SIZE	4
#define SA_PV		5
#define P_CFDATA	6
#define P_KERNEL_TEXT	7
#define P_VERSION	8
#define IA_EXTRALOC	9
#define I_NEXTRALOC	10
#define I_UEXTRALOC	11
#define	I_HISTLEN	12
#define	CA_HISTORY	13
#define TZ_TZ		14
#define NLENTRIES	15

#ifdef UKC_MAIN
struct nlist nl[] = {
	{ "_locnames" },
	{ "_locnamp" },
	{ "_cfroots" },
	{ "_cfroots_size" },
	{ "_pv_size" },
	{ "_pv" },
	{ "_cfdata" },
	{ "_kernel_text" },
	{ "_version" },
	{ "_extraloc" },
	{ "_nextraloc" },
	{ "_uextraloc" },
	{ "_userconf_histlen" },
	{ "_userconf_history" },
	{ "_tz" },
};
struct nlist knl[] = {
	{ "_locnames" },
	{ "_locnamp" },
	{ "_cfroots" },
	{ "_cfroots_size" },
	{ "_pv_size" },
	{ "_pv" },
	{ "_cfdata" },
	{ "_kernel_text" },
	{ "_version" },
	{ "_extraloc" },
	{ "_nextraloc" },
	{ "_uextraloc" },
	{ "_userconf_histlen" },
	{ "_userconf_history" },
	{ "_tz" },
};
int	maxdev = 0;
int	totdev = 0;
int	maxlocnames = 0;
int	base = 16;
int	cnt = -1;
int	lines = 18;
int	oldkernel = 0;
#else
extern struct nlist nl[];
extern int maxdev;
extern int totdev;
extern int maxlocnames;
extern int base;
extern int cnt;
extern int lines;
extern int oldkernel;
#endif

struct cfdata *get_cfdata __P((int));
short	      *get_locnamp __P((int));
caddr_t	      *get_locnames __P((int));
int	      *get_extraloc __P((int));

int	more __P(());
void	pnum __P((int));
void	pdevnam __P((short));
void	pdev __P((short));
int	number __P((char *, int *));
int	device __P((char *, int *, short *, short *));
int	attr __P((char *, int *));
void	modify __P((char *, int *));
void	change __P((int));
void	disable __P((int));
void	enable __P((int));
void	show __P((void));
void	common_attr_val __P((short, int *, char));
void	show_attr __P((char *));
void	common_dev __P((char *, int, short, short, char));
void	common_attr __P((char *, int, char));
void	add_read __P((char *, char, char *, int, int *));
void	add __P((char *, int, short, short));

int	config __P(());
void	process_history __P((int, char *));

#define UC_CHANGE 'c'
#define UC_DISABLE 'd'
#define UC_ENABLE 'e'
#define UC_FIND 'f'
#define UC_SHOW 's'

#endif _UTIL_H

