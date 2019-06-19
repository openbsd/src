/*	$OpenBSD: extern.h,v 1.10 2018/06/17 15:00:29 deraadt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)extern.h	8.2 (Berkeley) 4/28/95
 */

extern time_t now;			/* Current time. */
extern char tbuf[1024];			/* Temp buffer for anybody. */
extern int entries;			/* Number of people. */
extern int lflag;
extern int oflag;
extern int pplan;

int	 demi_print(char *, int);
void	 enter_lastlog(PERSON *);
PERSON	*enter_person(struct passwd *);
void	 enter_where(struct utmp *, PERSON *);
void	 expandusername(char *, char *, char *, size_t);
PERSON	*find_person(char *);
int	 hash(char *);
void	 lflag_print(void);
void	 loginlist(void);
void	 lprint(PERSON *);
int	 match(struct passwd *, char *);
void	 netfinger(char *);
PERSON	*palloc(void);
char	*prphone(char *);
int	 psort(const void *, const void *);
void	 sflag_print(void);
int	 show_text(char *, char *, char *);
PERSON **sort(void);
void	 stimeprint(WHERE *);
void	 userlist(int, char **);
void	 vputc(int);
