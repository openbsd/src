/*	$OpenBSD: chpass.h,v 1.7 2002/07/31 22:08:42 millert Exp $	*/
/*	$NetBSD: chpass.h,v 1.4 1996/05/15 21:50:44 jtc Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)chpass.h	8.4 (Berkeley) 4/2/94
 */

struct passwd;

typedef struct _entry {
	char *prompt;
	int (*func)(), restricted, len;
	char *except, *save;
} ENTRY;

/* Field numbers. */
#define	E_BPHONE	8
#define	E_HPHONE	9
#define	E_LOCATE	10
#define	E_NAME		7
#define	E_SHELL		12

/* edit() return values. */
#define	EDIT_OK		0
#define	EDIT_NOCHANGE	1
#define	EDIT_ERROR	-1

extern ENTRY list[];
extern uid_t uid;

int	 atot(char *, time_t *);
void	 display(char *, int, struct passwd *);
int	 edit(char *, struct passwd *);
char    *ok_shell(char *);
int	 p_change(char *, struct passwd *, ENTRY *);
int	 p_class(char *, struct passwd *, ENTRY *);
int	 p_expire(char *, struct passwd *, ENTRY *);
int	 p_gecos(char *, struct passwd *, ENTRY *);
int	 p_gid(char *, struct passwd *, ENTRY *);
int	 p_hdir(char *, struct passwd *, ENTRY *);
int	 p_login(char *, struct passwd *, ENTRY *);
int	 p_login(char *, struct passwd *, ENTRY *);
int	 p_passwd(char *, struct passwd *, ENTRY *);
int	 p_shell(char *, struct passwd *, ENTRY *);
int	 p_uid(char *, struct passwd *, ENTRY *);
char    *ttoa(char *, size_t, time_t);
int	 verify(char *, struct passwd *);
struct passwd
	*ypgetpwnam(char *);
struct passwd
	*ypgetpwuid(uid_t uid);
int	_yp_check(char **);
int	pw_yp(struct passwd *, uid_t);
