/*	$OpenBSD: clit.h,v 1.2 2002/03/15 16:41:06 jason Exp $	*/
/*
 * Copyright (c) 2002 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

struct clit {
	const char *cmd;
	const char *help;
	int minargc;
	int maxargc;
	int (*handler)(int argc, char **argv, void *);
	void *arg;
};

char *prompt_add;
	
int cmd_help(int, char **, void *);

void *cmdinit(struct clit *, int);
int cmdloop(void *);
void cmdend(void *);

/*
 * This function must be defined by the calling code. Sorry, but there is
 * no way to pass arguments to it or pass this function in some arguments.
 *
 * Fills in the possible completions into buf. Returns != 0 when there are
 * no possible completions. May whack buf, but the "returned"
 * string should be appended to the string that was in buf.
 */
int cmd_complt(char *buf, size_t buflen);
