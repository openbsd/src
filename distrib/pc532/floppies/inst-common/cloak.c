/*	$OpenBSD: cloak.c,v 1.3 2000/03/01 22:10:07 todd Exp $	*/
/*
 * Copyright (c) 1995 Matthias Pfaller.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Matthias Pfaller.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <a.out.h>
#include <sys/param.h>

#define SYM_DATA "_input_data"
#define SYM_LEN "_input_len"
void
writex(int fd, void *buf, int cnt, char *prog)
{
	if (write(fd, buf, cnt) != cnt) {
		perror(prog);
		exit(1);
	}
}

main(int argc, char **argv)
{
	int n, data_len = 0;
	static struct exec exec;
	static struct nlist nlist[2];
	static char buf[10240];

	if (argc != 1) {
		fprintf(stderr, "usage: %s <file1 >file2", argv[0]);
		exit(1);
	}
	writex(1, &exec, sizeof(exec), argv[0]);
	while ((n = read(0, buf, sizeof(buf))) > 0) {
		data_len += n;
		writex(1, buf, n, argv[0]);
	}

	memset(buf, 0, sizeof(buf));
	if (ALIGN(data_len) - data_len)
		writex(1, buf, ALIGN(data_len) - data_len, argv[0]);
	writex(1, (void *)&data_len, sizeof(data_len), argv[0]);

	n = sizeof(SYM_DATA) + sizeof(SYM_LEN) + sizeof(n);
	nlist[0].n_un.n_strx  = sizeof(n);
	nlist[0].n_type  = N_TEXT | N_EXT;
	nlist[0].n_value = 0;
	nlist[1].n_un.n_strx  = nlist[0].n_un.n_strx + sizeof(SYM_DATA);
	nlist[1].n_type  = N_TEXT | N_EXT;
	nlist[1].n_value = ALIGN(data_len);
	writex(1, nlist, sizeof(nlist), argv[0]);
	writex(1, &n, sizeof(n), argv[0]);
	writex(1, SYM_DATA, sizeof(SYM_DATA), argv[0]);
	writex(1, SYM_LEN, sizeof(SYM_LEN), argv[0]);

	N_SETMAGIC(exec, OMAGIC, MID_MACHINE, 0);
	exec.a_text   = ALIGN(data_len) + sizeof(data_len);
	exec.a_data   = 0;
	exec.a_bss    = 0;
	exec.a_syms   = sizeof(nlist);
	exec.a_trsize = 0;
	exec.a_drsize = 0;
	lseek(1, (off_t)0, SEEK_SET);
	writex(1, &exec, sizeof(exec), argv[0]);
	exit(0);
}
