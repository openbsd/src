/*	$OpenBSD: util.h,v 1.3 1996/12/03 01:05:36 downsj Exp $	*/
/*	$NetBSD: util.h,v 1.2 1996/05/16 07:00:22 thorpej Exp $	*/

/*-
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 * Portions Copyright (c) 1996, Jason Downs.  All rights reserved.
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
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <pwd.h>
#include <utmp.h>
#include <termios.h>
#include <sys/ttycom.h>
#include <sys/types.h>
#include <sys/cdefs.h>

/*
 * opendev() specific operation flags.
 */
#define OPENDEV_PART	0x01		/* Try to open the raw partition. */
#define OPENDEV_DRCT	0x02		/* Try to open the device directly. */

__BEGIN_DECLS
void	login __P((struct utmp *));
int	login_tty __P((int));
int	logout __P((const char *));
void	logwtmp __P((const char *, const char *, const char *));
int	opendev __P((char *, int, int, char **));
int	pw_lock __P((int retries));
int	pw_mkdb __P((void));
int	pw_abort __P((void));
void	pw_init __P((void));
void	pw_edit __P((int, const char *));
void	pw_prompt __P((void));
void	pw_copy __P((int, int, struct passwd *));
int	pw_scan __P((char *, struct passwd *, int *));
void	pw_error __P((const char *, int, int));
int	openpty __P((int *, int *, char *, struct termios *,
		     struct winsize *));
pid_t	forkpty __P((int *, char *, struct termios *, struct winsize *));
int	getmaxpartitions __P((void));
int	getrawpartition __P((void));
void	login_fbtab __P((char *, uid_t, gid_t));
char   *readlabelfs __P((char *));
__END_DECLS

#endif /* !_UTIL_H_ */
