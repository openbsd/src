/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>

#include <err.h>
#include <roken.h>

#include <atypes.h>
#include <kafs.h>

#ifdef RCSID
RCSID("$KTH: test-setpag.c,v 1.5 2000/10/03 00:36:21 lha Exp $");
#endif

#ifdef KERBEROS

#if !defined(NGROUPS) && defined(NGROUPS_MAX)
#define NGROUPS NGROUPS_MAX
#endif

static void
print_groups (int ngroups, gid_t groups[NGROUPS])
{
    int i;

    printf ("groups: ");
    for (i = 0; i < ngroups; ++i)
	printf ("%d%s", groups[i], (i < ngroups - 1) ? ", " : "");
    printf ("\n");
}    

int
main(int argc, char **argv)
{
    int ret;
    gid_t groups[NGROUPS];
    int ngroups;
    gid_t pag1, pag2;
    pid_t pid;

    set_progname (argv[0]);

    if (argc != 1)
	errx (1, "Usage: %s", argv[0]);

    if (!k_hasafs ())
	errx (1, "No AFS?");

    ngroups = getgroups (NGROUPS, groups);
    if (ngroups < 0)
	err (1, "getgroups %d", NGROUPS);
    pag1 = groups[1];
    pag2 = groups[2];
    printf ("in parent ");
    print_groups (ngroups, groups);
    pid = fork ();
    if (pid < 0)
	err (1, "fork");
    if (pid == 0) {
	ret = k_setpag ();
	if (ret < 0)
	    err (1, "k_setpag");
	ngroups = getgroups (NGROUPS, groups);
	if (ngroups < 0)
	    err (1, "getgroups %d", NGROUPS);
	printf ("in child ");
	print_groups (ngroups, groups);
	return 0;
    } else {
	int status;

	while(waitpid (pid, &status, WNOHANG | WUNTRACED) != pid)
	    ;
	if (status)
	    return 1;
	ngroups = getgroups (NGROUPS, groups);
	if (ngroups < 0)
	    err (1, "getgroups %d", NGROUPS);
	printf ("in parent ");
	print_groups (ngroups, groups);
	if (groups[1] == pag1 && groups[2] == pag2)
	    return 0;
	else
	    return 1;
    }
}

#else /* !KERBEROS */

int
main (int argc, char **argv)
{
    set_progname (argv[0]);

    errx (1, "no kafs");
}

#endif /* !KERBEROS */
