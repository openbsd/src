/* $OpenBSD: check.c,v 1.1 1999/09/27 21:40:03 espie Exp $ */
/*-
 * Copyright (c) 1999 Marc Espie.
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
 *	This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS 
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Simple code for a stand-alone package checker */
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <paths.h>
#include <errno.h>
#include "stand.h"
#include "pgp.h"
#include "gzip.h"
#include "extern.h"

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL	"/dev/null"
#endif

typedef /*@observer@*/char *pchar;

static void 
gzcat(fdin, fdout, envp) 
	int fdin, fdout;
	char *envp[];
{
	pchar argv[2];

	argv[0] = GZCAT;
	argv[1] = NULL;
	if (dup2(fdin, fileno(stdin)) == -1 || 
	    dup2(fdout, fileno(stdout)) == -1 ||
	    execve(GZCAT, argv, envp) == -1)
		exit(errno);
}

static void 
pgpcheck(fd, userid, envp) 
	int fd;
	const char *userid;
	char *envp[];
{
	int fdnull;
	pchar argv[6];

	argv[0] = PGP;
	argv[1] = "+batchmode";
	argv[2] = "-f";

	if (userid) {
		argv[3] = "-u";
		argv[4] = (char *)userid;
		argv[5] = NULL;
	} else
		argv[3] = NULL;

	fdnull = open(_PATH_DEVNULL, O_RDWR);
	if (fdnull == -1 ||
	    dup2(fd, fileno(stdin)) == -1 || 
	    dup2(fdnull, fileno(stdout)) == -1 ||
	    execve(PGP, argv, envp)  == -1)
		exit(errno);
}

static int 
reap(pid)
	pid_t pid;
{
	pid_t result;
	int pstat;

	do {
		result = waitpid(pid, &pstat, 0);
	} while (result == -1 && errno == EINTR);
	return result == -1 ? -1 : pstat;
}

int 
check_signature(file, userid, envp, filename)
	/*@dependent@*/FILE *file;
	const char *userid;	
	char *envp[];
	/*@observer@*/const char *filename;
{
	FILE *file2;
	int c;
	char sign[SIGNSIZE];
	struct mygzip_header h;
	int status;
	int togzcat[2], topgpcheck[2];
	pid_t pgpid, gzcatid;

	status = read_header_and_diagnose(file, &h, sign, filename);
	if (status != 1)
		return PKG_UNSIGNED;

	if (pipe(topgpcheck) == -1) {
		fprintf(stderr, "Error creating pipe\n");
		return PKG_SIGERROR;
	}
	switch(pgpid = fork()) {
	case -1:
		fprintf(stderr, "Error creating pgp process\n");
		return PKG_SIGERROR;
	case 0:
		if (close(topgpcheck[1]) == -1)
			exit(errno);
		pgpcheck(topgpcheck[0], userid, envp);
		/*@notreached@*/
		break;
	default:
		(void)close(topgpcheck[0]);
		break;
	}
	if (write(topgpcheck[1], sign, sizeof(sign)) != sizeof(sign)) {
		fprintf(stderr, "Error writing to pgp pipe\n");
		(void)close(topgpcheck[1]);
		(void)reap(pgpid);
		return PKG_SIGERROR;
	}
	if (pipe(togzcat) == -1) {
		fprintf(stderr, "Error creating pipe\n");
		(void)close(topgpcheck[1]);
		(void)reap(pgpid);
		return PKG_SIGERROR;
	}
	switch (gzcatid=fork()) {
	case -1:
		fprintf(stderr, "Error creating gzcat process\n");
		(void)reap(pgpid);
		return PKG_SIGERROR;
	case 0:
		if (close(togzcat[1]) == -1)
			exit(errno);
		gzcat(togzcat[0], topgpcheck[1], envp);
		/*@notreached@*/
		break;
	default:
		(void)close(topgpcheck[1]);
		(void)close(togzcat[0]);
	}

	file2 = fdopen(togzcat[1], "w");
	if (file2 == NULL) {
		(void)close(togzcat[1]);
		(void)reap(gzcatid);
		(void)reap(pgpid);
		fprintf(stderr, "Error turning fd into FILE *\n");
		return PKG_SIGERROR;
	}

	if (gzip_write_header(file2, &h, NULL) != 1) {
		(void)fclose(file2);
		(void)reap(pgpid);
		(void)reap(gzcatid);
		fprintf(stderr, "Error writing gzip header\n");
		return PKG_SIGERROR;
	}
	while((c = fgetc(file)) != EOF) {
		if (fputc(c, file2) == EOF) {
		 	fprintf(stderr, "Problem writing to zcat\n");
			(void)fclose(file2);
			(void)reap(pgpid);
			(void)reap(gzcatid);
			return PKG_SIGERROR;
		}

	}
	status = PKG_GOODSIG;
	if (fclose(file2) != 0)
		status = PKG_SIGERROR;
	if (reap(gzcatid) != 0)
		status = PKG_SIGERROR;
	if (reap(pgpid) != 0)
		status = PKG_BADSIG;
	return status;
}
