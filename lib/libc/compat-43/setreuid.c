/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1991 Regents of the University
 * of California.  All rights reserved.
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
static char *rcsid = "$OpenBSD: setreuid.c,v 1.6 2002/10/30 20:15:29 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <unistd.h>

__warn_references(setreuid, "warning: this program uses setreuid(), which is deprecated.");

int
setreuid(uid_t ruid, uid_t euid)
{
	int error;
	uid_t suid, cur_ruid, cur_euid, cur_suid;

	if (error == (getresuid(&cur_ruid, &cur_euid, &cur_suid)) != 0)
		return (error);

	/*
	 * The saved uid presents a bit of a dilemma, as it did not
	 * appear in 4.3BSD.  We only set the saved uid when the real
	 * uid is specified and either its value would change, or,
	 * where the saved and effective uids are different.
	 */
	if (ruid != (uid_t)-1 && (ruid != cur_ruid ||
	    cur_suid != (euid != (uid_t)-1 ? euid : cur_euid)))
		suid = ruid;
	else
		suid = (uid_t)-1;

	return (setresuid(ruid, euid, suid));
}
