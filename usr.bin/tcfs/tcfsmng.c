/*	$OpenBSD: tcfsmng.c,v 1.3 2000/06/19 22:42:28 aaron Exp $	*/

/*
 * Copyright 2000 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#include <stdio.h>
#include <stdlib.h>

extern int adduser_main(int argc, char **argv);
extern int rmuser_main(int argc, char **argv);
extern int addgroup_main(int argc, char **argv);
extern int rmgroup_main(int argc, char **argv);

struct subprg {
	char *name;
	int (*function)(int, char **);
};

struct subprg subcmds[] = {
	{"adduser", adduser_main},
	{"rmuser", rmuser_main},
	{"addgroup", addgroup_main},
	{"rmgroup", rmgroup_main}
};

void
usage(char *name)
{
	int i;
	fprintf(stderr, "Usage: %s <subcmd> [arguments]\n", name);

	fprintf(stderr, "Possible sub commands:");
	for (i = sizeof(subcmds)/sizeof(struct subprg) - 1; i >= 0; i--)
		fprintf(stderr, " %s", subcmds[i].name);
	fprintf(stderr, "\n");
}


int 
main (int argc, char **argv)
{
	int i;

	if (argc < 2) {
		usage(argv[0]);
		exit(1);
	}

	for (i = sizeof(subcmds)/sizeof(struct subprg) - 1; i >= 0; i--) {
		if (!strcmp(argv[1], subcmds[i].name))
			return (*subcmds[i].function)(argc - 1, argv + 1);
	}

	fprintf(stderr, "%s: unknown command %s\n\n", argv[0], argv[1]);
	usage(argv[0]);
	exit(1);
}
