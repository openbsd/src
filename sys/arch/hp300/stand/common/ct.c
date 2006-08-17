/*	$OpenBSD: ct.c,v 1.5 2006/08/17 06:31:10 miod Exp $	*/
/*	$NetBSD: ct.c,v 1.9 1996/10/14 07:29:57 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)ct.c	8.1 (Berkeley) 7/15/93
 */

/*
 * CS80 tape driver
 */
#include <sys/param.h>

#include <hp300/dev/ctreg.h>

#include <lib/libsa/stand.h>

#include "samachdep.h"
#include "hpibvar.h"

struct	ct_iocmd ct_ioc;
struct	ct_rscmd ct_rsc;
struct	ct_stat ct_stat;
struct	ct_ssmcmd ct_ssmc;

struct	ct_softc {
	int	sc_ctlr;
	int	sc_unit;
	char	sc_retry;
	char	sc_alive;
	short	sc_punit;
	int	sc_blkno;
} ct_softc[NHPIB][NCT];

#define	CTRETRY		5
#define	MTFSF		10
#define	MTREW		11

int	ctclose(struct open_file *);
int	cterror(int, int);
int	ctident(int, int);
int	ctinit(int, int);
int	ctopen(struct open_file *, int, int, int);
int	ctpunit(int, int, int *);
int	ctstrategy(void *, int, daddr_t, size_t, void *, size_t *);

char ctio_buf[MAXBSIZE];

struct	ctinfo {
	short	hwid;
	short	punit;
} ctinfo[] = {
	{ CT7946ID,	1 },
	{ CT7912PID,	1 },
	{ CT7914PID,	1 },
	{ CT9144ID,	0 },
	{ CT9145ID,	0 }
};
int	nctinfo = sizeof(ctinfo) / sizeof(ctinfo[0]);

int
ctinit(int ctlr, int unit)
{
	struct ct_softc *rs = &ct_softc[ctlr][unit];
	u_char stat;

	if (hpibrecv(ctlr, unit, C_QSTAT, &stat, 1) != 1 || stat)
		return (0);
	if (ctident(ctlr, unit) < 0)
		return (0);
	bzero(&ct_ssmc, sizeof(ct_ssmc));
	ct_ssmc.unit = C_SUNIT(rs->sc_punit);
	ct_ssmc.cmd = C_SSM;
	ct_ssmc.fefm = FEF_MASK;
	ct_ssmc.refm = REF_MASK;
	ct_ssmc.aefm = AEF_MASK;
	ct_ssmc.iefm = IEF_MASK;
	hpibsend(ctlr, unit, C_CMD, &ct_ssmc, sizeof(ct_ssmc));
	hpibswait(ctlr, unit);
	hpibrecv(ctlr, unit, C_QSTAT, &stat, 1);
	rs->sc_alive = 1;
	return (1);
}

int
ctident(int ctlr, int unit)
{
	struct cs80_describe desc;
	u_char stat, cmd[3];
	char name[7];
	int id, i;

	id = hpibid(ctlr, unit);
	if ((id & 0x200) == 0)
		return(-1);
	for (i = 0; i < nctinfo; i++)
		if (id == ctinfo[i].hwid)
			break;
	if (i == nctinfo)
		return(-1);
	ct_softc[ctlr][unit].sc_punit = ctinfo[i].punit;
	id = i;

	/*
	 * Collect device description.
	 * Right now we only need this to differentiate 7945 from 7946.
	 * Note that we always issue the describe command to unit 0.
	 */
	cmd[0] = C_SUNIT(0);
	cmd[1] = C_SVOL(0);
	cmd[2] = C_DESC;
	hpibsend(ctlr, unit, C_CMD, cmd, sizeof(cmd));
	hpibrecv(ctlr, unit, C_EXEC, &desc, sizeof(desc));
	hpibrecv(ctlr, unit, C_QSTAT, &stat, sizeof(stat));
	bzero(name, sizeof(name));
	if (!stat) {
		int n = desc.d_name;
		for (i = 5; i >= 0; i--) {
			name[i] = (n & 0xf) + '0';
			n >>= 4;
		}
	}
	switch (ctinfo[id].hwid) {
	case CT7946ID:
		if (bcmp(name, "079450", 6) == 0)
			id = -1;		/* not really a 7946 */
		break;
	default:
		break;
	}
	return(id);
}

int
ctpunit(int ctlr, int slave, int *punit)
{
	struct ct_softc *rs;

	if (ctlr >= NHPIB || hpibalive(ctlr) == 0)
		return(EADAPT);
	if (slave >= NCT)
		return(ECTLR);
	rs = &ct_softc[ctlr][slave];

	if (rs->sc_alive == 0)
		return(ENXIO);

	*punit = rs->sc_punit;
	return (0);
}

int
ctopen(struct open_file *f, int ctlr, int unit, int part)
{
	struct ct_softc *rs;
	int skip;
	size_t resid;

	if (ctlr >= NHPIB || hpibalive(ctlr) == 0)
		return(EADAPT);
	if (unit >= NCT)
		return(ECTLR);
	rs = &ct_softc[ctlr][unit];
	rs->sc_blkno = 0;
	rs->sc_unit = unit;
	rs->sc_ctlr = ctlr;
	if (rs->sc_alive == 0)
		if (ctinit(ctlr, unit) == 0)
			return(ENXIO);
	f->f_devdata = (void *)rs;
	ctstrategy(f->f_devdata, MTREW, 0, 0, ctio_buf, &resid);
	skip = part;
	while (skip--)
		ctstrategy(f->f_devdata, MTFSF, 0, 0, ctio_buf, &resid);
	return(0);
}

int
ctclose(struct open_file *f)
{
	size_t resid;

	ctstrategy(f->f_devdata, MTREW, 0, 0, ctio_buf, &resid);
	return (0);
}

int
ctstrategy(void *devdata, int func, daddr_t dblk, size_t size, void *v_buf,
    size_t *rsize)
{
	struct ct_softc *rs = devdata;
	char *buf = v_buf;
	int ctlr = rs->sc_ctlr;
	int unit = rs->sc_unit;
	char stat;

	if (size == 0 && (func == F_READ || func == F_WRITE))
		return(0);

	rs->sc_retry = 0;
	bzero(&ct_ioc, sizeof(ct_ioc));
	ct_ioc.unit = C_SUNIT(rs->sc_punit);
	ct_ioc.saddr = C_SADDR;
	ct_ioc.nop2 = C_NOP;
	ct_ioc.slen = C_SLEN;
	ct_ioc.nop3 = C_NOP;
top:
	if (func == F_READ) {
		ct_ioc.cmd = C_READ;
		ct_ioc.addr = rs->sc_blkno;
		ct_ioc.len = size;
	}
	else if (func == F_WRITE) {
		ct_ioc.cmd = C_WRITE;
		ct_ioc.addr = rs->sc_blkno;
		ct_ioc.len = size;
	}
	else if (func == MTFSF) {
		ct_ioc.cmd = C_READ;
		ct_ioc.addr = rs->sc_blkno;
		ct_ioc.len = size = MAXBSIZE;
	}
	else {
		ct_ioc.cmd = C_READ;
		ct_ioc.addr = 0;
		ct_ioc.len = 0;
		rs->sc_blkno = 0;
		size = 0;
	}
retry:
	hpibsend(ctlr, unit, C_CMD, &ct_ioc, sizeof(ct_ioc));
	if (func != MTREW) {
		hpibswait(ctlr, unit);
		hpibgo(ctlr, unit, C_EXEC, buf, size,
			func != F_WRITE ? F_READ : F_WRITE);
		hpibswait(ctlr, unit);
	} else {
		while (hpibswait(ctlr, unit) < 0)
			;
	}
	hpibrecv(ctlr, unit, C_QSTAT, &stat, 1);
	if (stat) {
		stat = cterror(ctlr, unit);
		if (stat == 0)
			return (-1);
		if (stat == 2)
			return (0);
		if (++rs->sc_retry > CTRETRY)
			return (-1);
		goto retry;
	}
	rs->sc_blkno += CTBTOK(size);
	if (func == MTFSF)
		goto top;
	*rsize = size;

	return(0);
}

int
cterror(int ctlr, int unit)
{
	struct ct_softc *rs = &ct_softc[ctlr][unit];
	char stat;

	bzero(&ct_rsc, sizeof(ct_rsc));
	bzero(&ct_stat, sizeof(ct_stat));
	ct_rsc.unit = C_SUNIT(rs->sc_punit);
	ct_rsc.cmd = C_STATUS;
	hpibsend(ctlr, unit, C_CMD, &ct_rsc, sizeof(ct_rsc));
	hpibrecv(ctlr, unit, C_EXEC, &ct_stat, sizeof(ct_stat));
	hpibrecv(ctlr, unit, C_QSTAT, &stat, 1);
	if (stat) {
		printf("ct%d: request status fail %d\n", unit, stat);
		return(0);
	}
	if (ct_stat.c_aef & AEF_EOF) {
		/* 9145 drives don't increment block number at EOF */
		if ((ct_stat.c_blk - rs->sc_blkno) == 0)
			rs->sc_blkno++;
		else
			rs->sc_blkno = ct_stat.c_blk;
		return (2);
	}
	printf("ct%d err: vu 0x%x, pend 0x%x, bn%ld", unit,
		ct_stat.c_vu, ct_stat.c_pend, ct_stat.c_blk);
	printf(", R 0x%x F 0x%x A 0x%x I 0x%x\n", ct_stat.c_ref,
		ct_stat.c_fef, ct_stat.c_aef, ct_stat.c_ief);
	return (1);
}
