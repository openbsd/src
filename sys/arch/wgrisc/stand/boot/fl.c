/*	$OpenBSD: fl.c,v 1.1 1997/07/21 06:58:13 pefo Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdarg.h>

#include <stand.h>
#include <sys/param.h>

int prom_seek __P((int, long, int));
int disk_read __P((int, char *, int, int));
int disk_open __P((char *, int));

struct	fl_softc {
	int	sc_fd;			/* PROM file id */
};

int
flstrategy(devdata, rw, bn, reqcnt, addr, cnt)
	void *devdata;
	int rw;
	daddr_t bn;
	u_int reqcnt;
	char *addr;
	u_int *cnt;	/* out: number of bytes transfered */
{
	struct fl_softc *sc = (struct fl_softc *)devdata;
	int s;
	long offset;

	offset = bn * DEV_BSIZE;

	s = disk_read(sc->sc_fd, addr, offset, reqcnt);
	if (s < 0)
		return (-s);
	*cnt = s;
	return (0);
}

/*
 *  We only deal with flash 0 here. We need to be small.
 */
int
flopen(struct open_file *f, ...)
{
	struct fl_softc *sc;
	int fd;
	static char device[] = "fl(0)";

	fd = disk_open(device, 0);
	if (fd < 0) {
		return (ENXIO);
	}

	sc = alloc(sizeof(struct fl_softc));
	f->f_devdata = (void *)sc;

	sc->sc_fd = fd;

	return (0);
}

int
flclose(f)
	struct open_file *f;
{
#ifdef FANCY
	free(f->f_devdata, sizeof(struct fl_softc));
	f->f_devdata = (void *)0;
#endif
	return (0);
}
