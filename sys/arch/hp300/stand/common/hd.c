/*	$OpenBSD: hd.c,v 1.7 2011/03/13 00:13:52 deraadt Exp $	*/
/*	$NetBSD: rd.c,v 1.11 1996/12/21 21:34:40 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah Hdr: rd.c 1.20 92/12/21
 *
 *	@(#)rd.c	8.1 (Berkeley) 7/15/93
 */

/*
 * CS80/SS80 disk driver
 */
#include <sys/param.h>
#include <sys/disklabel.h>

#include <lib/libsa/stand.h>

#include "samachdep.h"

#include <hp300/dev/hdreg.h>
#include "hpibvar.h"

struct	hd_iocmd hd_ioc;
struct	hd_rscmd hd_rsc;
struct	hd_stat hd_stat;
struct	hd_ssmcmd hd_ssmc;

struct	disklabel hdlabel;

struct	hdminilabel {
	u_short	npart;
	u_long	offset[MAXPARTITIONS];
};

struct	hd_softc {
	int	sc_ctlr;
	int	sc_unit;
	int	sc_part;
	char	sc_retry;
	char	sc_alive;
	short	sc_type;
	struct	hdminilabel sc_pinfo;
} hd_softc[NHPIB][NHD];

#define	HDRETRY		5

struct	hdidentinfo {
	short	ri_hwid;
	short	ri_maxunum;
	int	ri_nblocks;
} hdidentinfo[] = {
	{ HD7946AID,	0,	 108416 },
	{ HD9134DID,	1,	  29088 },
	{ HD9134LID,	1,	   1232 },
	{ HD7912PID,	0,	 128128 },
	{ HD7914PID,	0,	 258048 },
	{ HD7958AID,	0,	 255276 },
	{ HD7957AID,	0,	 159544 },
	{ HD7933HID,	0,	 789958 },
	{ HD9134LID,	1,	  77840 },
	{ HD7936HID,	0,	 600978 },
	{ HD7937HID,	0,	1116102 },
	{ HD7914CTID,	0,	 258048 },
	{ HD7946AID,	0,	 108416 },
	{ HD9134LID,	1,	   1232 },
	{ HD7957BID,	0,	 159894 },
	{ HD7958BID,	0,	 297108 },
	{ HD7959BID,	0,	 594216 },
	{ HD2200AID,	0,	 654948 },
	{ HD2203AID,	0,	1309896 }
};
int numhdidentinfo = sizeof(hdidentinfo) / sizeof(hdidentinfo[0]);

int	hdclose(struct open_file *);
int	hderror(int, int, int);
int	hdgetinfo(struct hd_softc *);
int	hdident(int, int);
int	hdinit(int, int);
int	hdopen(struct open_file *, int, int, int);
void	hdreset(int, int);
int	hdstrategy(void *, int, daddr32_t, size_t, void *, size_t *);

int
hdinit(int ctlr, int unit)
{
	struct hd_softc *rs = &hd_softc[ctlr][unit];

	rs->sc_type = hdident(ctlr, unit);
	if (rs->sc_type < 0)
		return (0);
	rs->sc_alive = 1;
	return (1);
}

void
hdreset(int ctlr, int unit)
{
	u_char stat;

	hd_ssmc.c_unit = C_SUNIT(0);
	hd_ssmc.c_cmd = C_SSM;
	hd_ssmc.c_refm = REF_MASK;
	hd_ssmc.c_fefm = FEF_MASK;
	hd_ssmc.c_aefm = AEF_MASK;
	hd_ssmc.c_iefm = IEF_MASK;
	hpibsend(ctlr, unit, C_CMD, &hd_ssmc, sizeof(hd_ssmc));
	hpibswait(ctlr, unit);
	hpibrecv(ctlr, unit, C_QSTAT, &stat, 1);
}

int
hdident(int ctlr, int unit)
{
	struct cs80_describe desc;
	u_char stat, cmd[3];
	char name[7];
	int id, i;

	id = hpibid(ctlr, unit);
	if ((id & 0x200) == 0)
		return(-1);
	for (i = 0; i < numhdidentinfo; i++)
		if (id == hdidentinfo[i].ri_hwid)
			break;
	if (i == numhdidentinfo)
		return(-1);
	id = i;
	hdreset(ctlr, unit);
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
	/*
	 * Take care of a couple of anomalies:
	 * 1. 7945A and 7946A both return same HW id
	 * 2. 9122S and 9134D both return same HW id
	 * 3. 9122D and 9134L both return same HW id
	 */
	switch (hdidentinfo[id].ri_hwid) {
	case HD7946AID:
		if (bcmp(name, "079450", 6) == 0)
			id = HD7945A;
		else
			id = HD7946A;
		break;

	case HD9134LID:
		if (bcmp(name, "091340", 6) == 0)
			id = HD9134L;
		else
			id = HD9122D;
		break;

	case HD9134DID:
		if (bcmp(name, "091220", 6) == 0)
			id = HD9122S;
		else
			id = HD9134D;
		break;
	}
	return(id);
}

char io_buf[MAXBSIZE];

int
hdgetinfo(struct hd_softc *rs)
{
	struct hdminilabel *pi = &rs->sc_pinfo;
	struct disklabel *lp = &hdlabel;
	char *msg;
	int err, savepart;
	size_t i;

	bzero((caddr_t)lp, sizeof *lp);
	lp->d_secsize = DEV_BSIZE;

	/* Disklabel is always from RAW_PART. */
	savepart = rs->sc_part;
	rs->sc_part = RAW_PART;
	err = hdstrategy(rs, F_READ, LABELSECTOR,
	    lp->d_secsize ? lp->d_secsize : DEV_BSIZE, io_buf, &i);
	rs->sc_part = savepart;

	if (err) {
		printf("hdgetinfo: hdstrategy error %d\n", err);
		return(0);
	}

	msg = getdisklabel(io_buf, lp);
	if (msg) {
		printf("hd(%d,%d,%d): WARNING: %s, ",
		       rs->sc_ctlr, rs->sc_unit, rs->sc_part, msg);
		printf("defining `c' partition as entire disk\n");
		pi->npart = 3;
		pi->offset[0] = pi->offset[1] = -1;
		pi->offset[2] = 0;
	} else {
		pi->npart = lp->d_npartitions;
		for (i = 0; i < pi->npart; i++)
			pi->offset[i] = lp->d_partitions[i].p_size == 0 ?
				-1 : lp->d_partitions[i].p_offset;
	}
	return(1);
}

int
hdopen(struct open_file *f, int ctlr, int unit, int part)
{
	struct hd_softc *rs;

	if (ctlr >= NHPIB || hpibalive(ctlr) == 0)
		return (EADAPT);
	if (unit >= NHD)
		return (ECTLR);
	rs = &hd_softc[ctlr][unit];
	rs->sc_part = part;
	rs->sc_unit = unit;
	rs->sc_ctlr = ctlr;
	if (rs->sc_alive == 0) {
		if (hdinit(ctlr, unit) == 0)
			return (ENXIO);
		if (hdgetinfo(rs) == 0)
			return (ERDLAB);
	}
	if (part != RAW_PART &&     /* always allow RAW_PART to be opened */
	    (part >= rs->sc_pinfo.npart || rs->sc_pinfo.offset[part] == -1))
		return (EPART);
	f->f_devdata = (void *)rs;
	return (0);
}

int
hdclose(struct open_file *f)
{
	struct hd_softc *rs = f->f_devdata;

	/*
	 * Mark the disk `not alive' so that the disklabel
	 * will be re-loaded at next open.
	 */
	bzero(rs, sizeof(struct hd_softc));
	f->f_devdata = NULL;
	return (0);
}

int
hdstrategy(void *devdata, int func, daddr32_t dblk, size_t size, void *v_buf,
    size_t *rsize)
{
	char *buf = v_buf;
	struct hd_softc *rs = devdata;
	int ctlr = rs->sc_ctlr;
	int unit = rs->sc_unit;
	daddr32_t blk;
	char stat;

	if (size == 0)
		return(0);

	/*
	 * Don't do partition translation on the `raw partition'.
	 */
	blk = (dblk + ((rs->sc_part == RAW_PART) ? 0 :
	    rs->sc_pinfo.offset[rs->sc_part]));

	rs->sc_retry = 0;
	hd_ioc.c_unit = C_SUNIT(0);
	hd_ioc.c_volume = C_SVOL(0);
	hd_ioc.c_saddr = C_SADDR;
	hd_ioc.c_hiaddr = 0;
	hd_ioc.c_addr = HDBTOS(blk);
	hd_ioc.c_nop2 = C_NOP;
	hd_ioc.c_slen = C_SLEN;
	hd_ioc.c_len = size;
	hd_ioc.c_cmd = func == F_READ ? C_READ : C_WRITE;
retry:
	hpibsend(ctlr, unit, C_CMD, &hd_ioc.c_unit, sizeof(hd_ioc)-2);
	hpibswait(ctlr, unit);
	hpibgo(ctlr, unit, C_EXEC, buf, size, func);
	hpibswait(ctlr, unit);
	hpibrecv(ctlr, unit, C_QSTAT, &stat, 1);
	if (stat) {
		if (hderror(ctlr, unit, rs->sc_part) == 0)
			return(EIO);
		if (++rs->sc_retry > HDRETRY)
			return(EIO);
		goto retry;
	}
	*rsize = size;

	return(0);
}

int
hderror(int ctlr, int unit, int part)
{
	char stat;

	hd_rsc.c_unit = C_SUNIT(0);
	hd_rsc.c_sram = C_SRAM;
	hd_rsc.c_ram = C_RAM;
	hd_rsc.c_cmd = C_STATUS;
	hpibsend(ctlr, unit, C_CMD, &hd_rsc, sizeof(hd_rsc));
	hpibrecv(ctlr, unit, C_EXEC, &hd_stat, sizeof(hd_stat));
	hpibrecv(ctlr, unit, C_QSTAT, &stat, 1);
	if (stat) {
		printf("hd(%d,%d,0,%d): request status fail %d\n",
		       ctlr, unit, part, stat);
		return(0);
	}
	printf("hd(%d,%d,0,%d) err: vu 0x%x",
	       ctlr, unit, part, hd_stat.c_vu);
	if ((hd_stat.c_aef & AEF_UD) || (hd_stat.c_ief & (IEF_MD|IEF_RD)))
		printf(", block %ld", hd_stat.c_blk);
	printf(", R0x%x F0x%x A0x%x I0x%x\n",
	       hd_stat.c_ref, hd_stat.c_fef, hd_stat.c_aef, hd_stat.c_ief);
	return(1);
}
