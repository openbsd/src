/* $OpenBSD: sign.c,v 1.2 1999/09/28 21:31:23 espie Exp $ */
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
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <pwd.h>
#include "stand.h"
#include "pgp.h"
#include "gzip.h"
#include "extern.h"

#define SIGN_TEMPLATE "%s %s | %s +batchmode +compress=off -f -s"
#define SIGN2_TEMPLATE "%s %s | %s +batchmode +compress=off -f -u %s -s"
#define COPY_TEMPLATE "%s.sign"

static int
retrieve_signature(filename, sign, userid)
	const char *filename; 
	char sign[];
	const char *userid;
{
	char *buffer;
	FILE *cmd;

	if (userid) {
		buffer = malloc(strlen(GZCAT) + strlen(filename) +
		    strlen(PGP) + strlen(userid) + sizeof(SIGN2_TEMPLATE));
		if (!buffer)
			return 0;
		sprintf(buffer, SIGN2_TEMPLATE, GZCAT, filename, PGP, userid);
    	} else {
		buffer = malloc(strlen(GZCAT) + strlen(filename) +
		    strlen(PGP) + sizeof(SIGN_TEMPLATE));
		if (!buffer)
			return 0;
		sprintf(buffer, SIGN_TEMPLATE, GZCAT, filename, PGP);
	}
	cmd = popen(buffer, "r");
	free(buffer);
	if (!cmd)
		return 0;
	if (fread(sign, 1, SIGNSIZE, cmd) != SIGNSIZE)
		return 0;
	(void)pclose(cmd);
	return 1;
}

static int 
embed_signature_FILE(orig, dest, sign, filename)
	/*@temp@*/FILE *orig;
	/*@temp@*/FILE *dest; 
	const char sign[]; 
	const char *filename;
{
	struct mygzip_header h;
	int c;

	if (read_header_and_diagnose(orig, &h, NULL, filename) == 0)
		return 0;

	if (gzip_write_header(dest, &h, sign) == 0)
		return 0;
	while ((c = fgetc(orig)) != EOF && fputc(c, dest) != EOF)
		;
	if (ferror(dest) != 0) 
		return 0;
	return 1;
}

static int 
embed_signature(filename, copy, sign)
	const char *filename;
	const char *copy; 
	const char sign[];
{
	FILE *orig, *dest;
	int success;
	
	success = 0;
	orig= fopen(filename, "r");
	if (orig) {
		dest = fopen(copy, "w");
		if (dest) {
			success = embed_signature_FILE(orig, dest, sign, filename);
			if (fclose(dest) != 0)
				success = 0;
		}
		if (fclose(orig) != 0)
			success = 0;
	}
	return success;
}

int 
sign(filename, userid, envp)
	const char *filename;
	const char *userid;
	/*@unused@*/char *envp[] __attribute__((unused));
{
	char sign[SIGNSIZE];
	char *copy;
	int result;

	if (retrieve_signature(filename, sign, userid) == 0) {
		fprintf(stderr, "Problem signing %s\n", filename);
		return 0;
	}
	copy = malloc(strlen(filename)+sizeof(COPY_TEMPLATE));
	if (copy == NULL) {
		fprintf(stderr, "Can't allocate memory\n");
		return 0;
	}
	sprintf(copy, COPY_TEMPLATE, filename);
	result = embed_signature(filename, copy, sign);
	if (result == 0) {
		fprintf(stderr, "Can't embed signature in %s\n", filename);
	} else if (unlink(filename) != 0) {
		fprintf(stderr, "Can't unlink original %s\n", filename);
		result = 0;
	} else if (rename(copy, filename) != 0) {
		fprintf(stderr, "Can't rename new file %s\n", copy);
		result = 0;
	}
	free(copy);
	return result;
}

void
handle_passphrase()
{
	pid_t pid;
	int fd[2];
	char *p;

		/* Retrieve the pgp passphrase */
	p = getpass("Enter passphrase:");

		/* somewhat kludgy code to get the passphrase to pgp, see 
		   pgp documentation for the gore
		 */
	if (pipe(fd) != 0)	{
		perror("pkg_sign");
		exit(EXIT_FAILURE);
	}
	switch(pid = fork()) {
	case -1:
		perror("pkg_sign");
		exit(EXIT_FAILURE);
	case 0:
		{
			(void)close(fd[0]);
				/* the child fills the pipe with copies of the passphrase.
				   Expect violent death when father exits.
				 */
			for(;;) {
				char c = '\n';
				(void)write(fd[1], p, strlen(p));
				(void)write(fd[1], &c, 1);
			}
		}
	default:
		{
			char buf[10];

			(void)close(fd[1]);
			(void)sprintf(buf, "%d", fd[0]);
			(void)setenv("PGPPASSFD", buf, 1);
		}
	}
}

