/* $OpenBSD: bootxx.c,v 1.3 2002/03/14 01:26:47 millert Exp $ */
/* $NetBSD: bootxx.c,v 1.2 1999/10/23 14:40:38 ragge Exp $ */
/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)boot.c	7.15 (Berkeley) 5/4/91
 */

#include "sys/param.h"
#include "sys/reboot.h"
#include "sys/disklabel.h"

#include "lib/libsa/stand.h"
#include "lib/libsa/ufs.h"

#include "../include/pte.h"
#include "../include/sid.h"
#include "../include/mtpr.h"
#include "../include/reg.h"
#include "../include/rpb.h"

#include "../mba/mbareg.h"
#include "../mba/hpreg.h"

#define NRSP 1 /* Kludge */
#define NCMD 1 /* Kludge */

#include "../mscp/mscp.h"
#include "../mscp/mscpreg.h"

#include "vaxstand.h"

struct rom_softc {
	int part;
	int unit;
} rom_softc;

int	romstrategy(void *, int, daddr_t, size_t, void *, size_t *);
int romopen(struct open_file *, int, int, int, int);

struct fs_ops	file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat }
};
int	nfsys = (sizeof(file_system) / sizeof(struct fs_ops));

struct devsw	devsw[] = {
	SADEV("rom", romstrategy, romopen, nullsys, noioctl),
};
int	ndevs = (sizeof(devsw)/sizeof(devsw[0]));

int	command(int cmd, int arg);

/*
 * Boot program... argume passed in r10 and r11 determine whether boot
 * stops to ask for system name and which device boot comes from.
 */

volatile dev_t	devtype, bootdev;
unsigned        opendev, boothowto, bootset, memsz;

struct open_file file;

unsigned *bootregs;
struct	rpb *rpb;
int	vax_cputype;

/*
 * The boot block are used by 11/750, 8200, MicroVAX II/III, VS2000,
 * VS3100/??, VS4000 and VAX6000/???, and only when booting from disk.
 */
Xmain()
{
	int io;
	char *scbb;
	char *new, *bqo;
	char *hej = "/boot";

	vax_cputype = (mfpr(PR_SID) >> 24) & 0xFF;

	/*
	 */ 
	switch (vax_cputype) {
	case VAX_TYP_UV2:
	case VAX_TYP_CVAX:
	case VAX_TYP_RIGEL:
	case VAX_TYP_NVAX:
	case VAX_TYP_MARIAH:
	case VAX_TYP_SOC:
		/*
		 * now relocate rpb/bqo (which are used by ROM-routines)
		 */
		rpb = (void*)XXRPB;
		bcopy ((void*)bootregs[11], rpb, 512);
		rpb->rpb_base = rpb;
		bqo = (void*)(512+(int)rpb);
		bcopy ((void*)rpb->iovec, bqo, rpb->iovecsz);
		rpb->iovec = (int)bqo;
		bootregs[11] = (int)rpb;
		bootdev = rpb->devtyp;
		memsz = rpb->pfncnt << 9;
		break;
	case VAX_8200:
        case VAX_750:
		bootdev = bootregs[10];
		memsz = 0;
		break;
	default:
		asm("halt");
	}

	bootset = getbootdev();
	io = open(hej, 0);

	read(io, (void *)0x10000, 0x10000);
	bcopy((void *) 0x10000, 0, 0xffff);
	hoppabort(32, XXRPB, bootset);
	asm("halt");
}

getbootdev()
{
	int i, adaptor, controller, unit, partition, retval;

	adaptor = controller = unit = partition = 0;

	switch (vax_cputype) {
	case VAX_TYP_UV2:
	case VAX_TYP_CVAX:
	case VAX_TYP_MARIAH:
	case VAX_TYP_RIGEL:
		if (rpb->devtyp == BDEV_SD || rpb->devtyp == BDEV_SDN) {
			unit = rpb->unit / 100;
			controller = (rpb->csrphy & 0x100 ? 1 : 0);
		} else {
			controller = ((rpb->csrphy & 017777) == 0xDC)?1:0;
			unit = rpb->unit;			/* DUC, DUD? */
		}
		break;

	case VAX_TYP_8SS:
	case VAX_TYP_750:
		controller = bootregs[1];
		unit = bootregs[3];
		break;
	}

	switch (B_TYPE(bootdev)) {
	case BDEV_HP:			/* massbuss boot */
		adaptor = (bootregs[1] & 0x6000) >> 17;
		break;

	case BDEV_UDA:		/* UDA50 boot */
		if (vax_cputype == VAX_750)
			adaptor = (bootregs[1] & 0x40000 ? 0 : 1);
		break;

	case BDEV_TK:		/* TK50 boot */
	case BDEV_CNSL:		/* Console storage boot */
	case BDEV_RD:		/* RD/RX on HDC9224 (MV2000) */
	case BDEV_ST:		/* SCSI-tape on NCR5380 (MV2000) */
	case BDEV_SD:		/* SCSI-disk on NCR5380 (3100/76) */
	case BDEV_SDN:		/* SCSI disk on NCR53C94 */	
		break;

	case BDEV_KDB:		/* DSA disk on KDB50 (VAXBI VAXen) */
		bootdev = (bootdev & ~B_TYPEMASK) | BDEV_UDA;
		break;

	default:
		boothowto |= (RB_SINGLE | RB_ASKNAME);
	}
	return MAKEBOOTDEV(bootdev, adaptor, controller, unit, partition);
}

/*
 * Write an extremely limited version of a (us)tar filesystem, suitable
 * for loading secondary-stage boot loader.
 * - Can only load file "boot".
 * - Must be the first file on tape.
 */
int tar_open(char *path, struct open_file *f);
ssize_t tar_read(struct open_file *f, void *buf, size_t size, size_t *resid);

int
tar_open(path, f)
	char *path;
	struct open_file *f;
{
	char *buf = alloc(512);

	bzero(buf, 512);
	romstrategy(0, 0, 8192, 512, buf, 0);
	if (bcmp(buf, "boot", 5) || bcmp(&buf[257], "ustar", 5))
		return EINVAL; /* Not a ustarfs with "boot" first */
	return 0;
}

ssize_t
tar_read(f, buf, size, resid)
	struct open_file *f;
	void *buf;
	size_t size;
	size_t *resid;
{
	romstrategy(0, 0, (8192+512), size, buf, 0);
	*resid = size;
}

struct disklabel lp;
int part_off = 0;		/* offset into partition holding /boot */
char io_buf[DEV_BSIZE];
volatile struct uda {
	struct  mscp_1ca uda_ca;           /* communications area */
	struct  mscp uda_rsp;     /* response packets */
	struct  mscp uda_cmd;     /* command packets */
} uda;
struct udadevice {u_short udaip;u_short udasa;};
volatile struct udadevice *csr;

devopen(f, fname, file)
	struct open_file *f;
	const char    *fname;
	char          **file;
{
	extern char	start;
	char           *msg;
	int		i, err, off;
	char		line[64];
	struct devsw	*dp;

	f->f_dev = &devsw[0];
	*file = (char *)fname;

	/*
	 * On uVAX we need to init [T]MSCP ctlr to be able to use it.
	 */
	if (vax_cputype == VAX_TYP_UV2 || vax_cputype == VAX_TYP_CVAX) {
		switch (bootdev) {
		case BDEV_UDA:	/* MSCP */
		case BDEV_TK:	/* TMSCP */
			csr = (struct udadevice *)rpb->csrphy;

			csr->udaip = 0; /* Start init */
			while((csr->udasa & MP_STEP1) == 0);
			csr->udasa = 0x8000;
			while((csr->udasa & MP_STEP2) == 0);
			csr->udasa = (short)(((u_int)&uda)&0xffff) + 8;
			while((csr->udasa & MP_STEP3) == 0);
			csr->udasa = 0x10;
			while((csr->udasa & MP_STEP4) == 0);
			csr->udasa = 0x0001;

			uda.uda_ca.ca_rspdsc =
			    (int) &uda.uda_rsp.mscp_cmdref;
			uda.uda_ca.ca_cmddsc =
			    (int) &uda.uda_cmd.mscp_cmdref;
			if (bootdev == BDEV_TK)
				uda.uda_cmd.mscp_vcid = 1;
			command(M_OP_SETCTLRC, 0);
			uda.uda_cmd.mscp_unit = rpb->unit;
			command(M_OP_ONLINE, 0);
		}
	}

	/* 
	 * the disklabel _shall_ be at address LABELOFFSET + RELOC in
	 * phys memory now, no need at all to reread it again.
	 * Actually disklabel is only needed when using hp disks,
	 * but it doesn't hurt to always get it.
	 */
	getdisklabel(LABELOFFSET + &start, &lp);

	/* currently only one dev in devsw; this must change if we add more */
	dp = devsw;
	i = 0;
	if(dp != NULL && dp->dv_open != NULL) {
		i = (*dp->dv_open)(f, B_ADAPTOR(bootdev), B_CONTROLLER(bootdev), 
				B_UNIT(bootdev), B_PARTITION(bootdev));
	}

	return i;
}

command(cmd, arg)
{
	volatile int hej;

	uda.uda_cmd.mscp_opcode = cmd;
	uda.uda_cmd.mscp_modifier = arg;

	uda.uda_cmd.mscp_msglen = MSCP_MSGLEN;
	uda.uda_rsp.mscp_msglen = MSCP_MSGLEN;
	uda.uda_ca.ca_rspdsc |= MSCP_OWN|MSCP_INT;
	uda.uda_ca.ca_cmddsc |= MSCP_OWN|MSCP_INT;
	hej = csr->udaip;
	while (uda.uda_ca.ca_rspdsc < 0);

}

int curblock = 0;

romstrategy(sc, func, dblk, size, buf, rsize)
	void    *sc;
	int     func;
	daddr_t dblk;
	size_t	size;
	void    *buf;
	size_t	*rsize;
{
	struct rom_softc *romsc = sc;
	int i;
	int	block = dblk;
	int     nsize = size;

	switch (vax_cputype) {
	/*
	 * case VAX_TYP_UV2:
	 * case VAX_TYP_CVAX:
	 * case VAX_TYP_RIGEL:
	 */
	default:
		switch (B_TYPE(bootdev)) {

		case BDEV_UDA: /* MSCP */
			uda.uda_cmd.mscp_seq.seq_lbn = dblk;
			uda.uda_cmd.mscp_seq.seq_bytecount = size;
			uda.uda_cmd.mscp_seq.seq_buffer = (int)buf;
			uda.uda_cmd.mscp_unit = rpb->unit;
			command(M_OP_READ, 0);
			break;

		case BDEV_TK: /* TMSCP */
			if (dblk < curblock) {
				uda.uda_cmd.mscp_seq.seq_bytecount =
				    curblock - dblk;
				command(M_OP_POS, 12);
			} else {
				uda.uda_cmd.mscp_seq.seq_bytecount =
				    dblk - curblock;
				command(M_OP_POS, 4);
			}
			curblock = size/512 + dblk;
			for (i = 0 ; i < size/512 ; i++) {
				uda.uda_cmd.mscp_seq.seq_lbn = 1;
				uda.uda_cmd.mscp_seq.seq_bytecount = 512;
				uda.uda_cmd.mscp_seq.seq_buffer =
				    (int)buf + i * 512;
				uda.uda_cmd.mscp_unit = rpb->unit;
				command(M_OP_READ, 0);
			}
			break;

		case BDEV_SDN:		/* XXX others too eventually */
		case BDEV_SD:		/* XXX others too eventually */
			if(romsc != NULL) 
				block += lp.d_partitions[romsc->part].p_offset;
		case BDEV_RD:
		case BDEV_ST:

		default:
			romread_uvax(block, size, buf, bootregs);
			break;

		}
		break;

	case VAX_8200:
	case VAX_750:
		if (bootdev != BDEV_HP) {
			while (size > 0) {
				while ((read750(block, bootregs) & 0x01) == 0){
				}
				bcopy(0, buf, 512);
				size -= 512;
				buf += 512;
				block++;
			}
		} else
			hpread(block, size, buf);
		break;
	}

	if (rsize)
		*rsize = nsize;
	return 0;
}

int
romopen(f, adapt, ctlr, unit, part)
	struct open_file *f;
	int adapt, ctlr, unit, part;
{
	rom_softc.unit = unit;
	rom_softc.part = part;
	
	f->f_devdata = (void *)&rom_softc;
	
	return 0;
}

hpread(block, size, buf)
	char           *buf;
{
	volatile struct mba_regs *mr = (void *) bootregs[1];
	volatile struct hp_drv *hd = (void*)&mr->mba_md[bootregs[3]];
	struct disklabel *dp = &lp;
	u_int           pfnum, nsize, mapnr, bn, cn, sn, tn;

	pfnum = (u_int) buf >> PGSHIFT;

	for (mapnr = 0, nsize = size; (nsize + NBPG) > 0; nsize -= NBPG)
		*(int *)&mr->mba_map[mapnr++] = PG_V | pfnum++;
	mr->mba_var = ((u_int) buf & PGOFSET);
	mr->mba_bc = (~size) + 1;
	bn = block;
	cn = bn / dp->d_secpercyl;
	sn = bn % dp->d_secpercyl;
	tn = sn / dp->d_nsectors;
	sn = sn % dp->d_nsectors;
	hd->hp_dc = cn;
	hd->hp_da = (tn << 8) | sn;
	hd->hp_cs1 = HPCS_READ;
	while (mr->mba_sr & MBASR_DTBUSY);
	if (mr->mba_sr & MBACR_ABORT){
		return 1;
	}
	return 0;
}

extern char end[];
static char *top = (char*)end;

void *
alloc(size)
        unsigned size;
{
	void *ut = top;
	top += size;
	return ut;
}

void
free(ptr, size)
        void *ptr;
        unsigned size;
{
}

int
romclose(f)
	struct open_file *f;
{
	return 0;
}
