/*
 * Copyright (c) 1988, 1993
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: getlogin.c,v 1.4 1998/11/20 11:18:39 d Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <pwd.h>
#include <utmp.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "thread_private.h"

_THREAD_PRIVATE_MUTEX(logname)
static int  logname_valid = 0;
static char logname[MAXLOGNAME + 1];

int	_getlogin __P((char *, size_t));
int	_setlogin __P((const char *));

char *
getlogin()
{
	_THREAD_PRIVATE_KEY(getlogin)
	char * name = (char *)_THREAD_PRIVATE(getlogin, logname, NULL);

	if ((errno = getlogin_r(name, sizeof logname)) != 0)
		return NULL;
	if (*name == '\0') {
		errno = ENOENT;  /* well? */
		return NULL;
	}
	return name;
}

int
getlogin_r(name, namelen)
	char *name;
	size_t namelen;
{
	int logname_size;

	if (name == NULL)
		return EFAULT;

	_THREAD_PRIVATE_MUTEX_LOCK(logname);
	if (!logname_valid) {
		if (_getlogin(logname, sizeof(logname) - 1) < 0) {
			_THREAD_PRIVATE_MUTEX_UNLOCK(logname);
			return errno;
		}
		logname_valid = 1;
		logname[MAXLOGNAME] = '\0';	/* paranoia */
	}
	logname_size = strlen(logname) + 1;
	if (namelen < logname_size)
		return ERANGE;
	memcpy(name, logname, logname_size);
	_THREAD_PRIVATE_MUTEX_UNLOCK(logname);
	return 0;
}

int
setlogin(name)
	const char *name;
{
	int ret;

	_THREAD_PRIVATE_MUTEX_LOCK(logname);
	ret = _setlogin(name);
	if (ret == 0)
		logname_valid = 0;
	_THREAD_PRIVATE_MUTEX_UNLOCK(logname);
	return ret;
}
