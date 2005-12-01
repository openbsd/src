/*	$OpenBSD: externs2.h,v 1.5 2005/12/01 05:06:40 cloder Exp $	*/
/*	$NetBSD: externs2.h,v 1.2 1995/07/03 21:24:46 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
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

/*
 * main.c
 */
extern	int	xflag;
extern	int	uflag;
extern	int	Cflag;
extern	const	char *libname;
extern	int	pflag;
extern	int	sflag;
extern	int	tflag;
extern	int	Hflag;
extern	int	hflag;
extern	int	Fflag;


/*
 * hash.c
 */
extern	void	inithash(void);
extern	hte_t	*hsearch(const char *, int);
extern	void	forall(void (*)(hte_t *));

/*
 * read.c
 */
extern	const	char **fnames;
extern	type_t	**tlst;
extern	int	csrcfile;

extern	void	readfile(const char *);
extern	void	mkstatic(hte_t *);

/*
 * mem2.c
 */
extern	void	initmem(void);
extern	void	*xalloc(size_t);

/*
 * chk.c
 */
extern	void	inittyp(void);
extern	void	mainused(void);
extern	void	chkname(hte_t *);

/*
 * msg.c
 */
extern	void	msg(int, ...);
extern	const	char *mkpos(pos_t *);

/*
 * emit2.c
 */
extern	void	outlib(const char *);
