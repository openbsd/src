/*	$OpenBSD: exec.new.c,v 1.2 1998/07/20 18:12:34 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <sys/param.h>
#include <sys/reboot.h>
#ifndef INSECURE
#include <sys/stat.h>
#endif
#include "libsa.h"
#include <lib/libsa/exec.h>

void
exec(path, loadaddr, howto)
	char *path;
	void *loadaddr;
	int howto;
{
	int fd;
	struct stat sb;
	const struct x_sw *sw;
	struct x_param param;
	union x_header hdr;
	register u_char *pa;
	register int save_err;
	u_int sz;

	if ((fd = open(path, 0)) < 0 || fstat(fd, &sb))
		return;

	if (sb.st_mode & 2)
		printf("non-secure file, check permissions!\n");

	if (read(fd, (char *)&hdr, sizeof(hdr)) != sizeof(hdr))
		return;

	errno = 0; /* XXX */
	/* probe obj format */
	for (sw = execsw; sw->probe; sw++)
		if (sw->probe(fd, &hdr))
			break;

	bzero (&param, sizeof(param));
	param.xp_execsw = sw;
	param.xp_hdr = &hdr;

	if (!sw->probe || sw->load(fd, &param)) {
		errno = errno? errno : EFTYPE;
		goto err;
	}
#ifdef	EXEC_DEBUG
	printf("ep=%x, end=%x\n.text=%x,%u,%u\n.data=%x,%u,%u\n"
	       ".bss=%x,%u,%u\n.sym=%x,%u,%u\n.str=%x,%u,%u\n",
	       param.xp_entry, param.xp_end,
	       param.text.addr, param.text.size, param.text.foff,
	       param.data.addr, param.data.size, param.data.foff,
	       param.bss.addr,  param.bss.size,  param.bss.foff,
	       param.sym.addr,  param.sym.size,  param.sym.foff,
	       param.str.addr,  param.str.size,  param.str.foff);
#endif
	pa = loadaddr;

	printf("%u", param.text.size);
	/* read .text + .data + .sym */
	if (lseek(fd, param.text.foff, SEEK_SET) < 0 ||
	    read(fd, pa+param.text.addr, param.text.size) != param.text.size)
		goto err;

	if (param.data.size) {
		printf("+%u", param.data.size);
		if (lseek(fd, param.data.foff, SEEK_SET) <= 0 ||
		    read(fd,pa+param.data.addr,param.data.size) != param.data.size)
			goto err;
	}

	/* .bss */
	printf("+%u", param.bss.size);
	bzero (pa + param.bss.addr, param.bss.size);

	sz = 0;
	if (param.sym.size) {
		printf("+[%u", param.sym.size);
		if (lseek(fd, param.sym.foff, SEEK_SET) <= 0 ||
		    read(fd,pa+param.sym.addr,param.sym.size) != param.sym.size)
			goto err;

		/* .str */
		if (lseek(fd, param.str.foff, SEEK_SET) <= 0)
			goto err;

		pa += param.str.addr;
		sz = param.str.size;

		/* special hack for a.out, where .str size is it's first int */
		if (param.str.foff && sz == 0) {
			if (read(fd, pa, sizeof(u_int)) != sizeof(u_int))
				goto err;
			else {
				sz = param.sym.size = *(u_int*)pa;
				pa += sizeof(u_int);
				sz -= sizeof(u_int);
			}
		}
		if (sz) {
			if (sz && read(fd, pa, sz) != sz)
				goto err;
		}
		printf("+%u]", sz);
	}

	param.xp_end = ((u_int)(pa + sz) + sizeof(int) -1) & ~(sizeof(int) -1);

	printf(" total=0x%x start=0x%x\n", param.xp_end, param.xp_entry);

	/* call the joker */
	machdep_exec(&param, howto, loadaddr);

	/* exec failed */
	errno = ENOEXEC;
	return;
err:
	save_err = errno? errno: EIO;
	close(fd);
	errno = save_err;
}
