/* $OpenBSD: bootxx.c,v 1.8 1998/05/13 07:30:21 niklas Exp $ */
/* $NetBSD: bootxx.c,v 1.11 1997/06/08 17:49:17 ragge Exp $ */
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

#include "../uba/ubareg.h"
#include "../uba/udareg.h"

#include "../mscp/mscp.h"
#include "../mscp/mscpreg.h"

#include "data.h"
#include "vaxstand.h"

#include <sys/exec.h>

int     romstrategy(), romopen();
int	command(int, int);

/*
 * Boot program... argume passed in r10 and r11 determine whether boot
 * stops to ask for system name and which device boot comes from.
 */

volatile u_int  devtype, bootdev;
unsigned        opendev, boothowto, bootset, memsz;

extern unsigned *bootregs;
extern struct	rpb *rpb;

/*
 * The boot block are used by 11/750, 8200, MicroVAX II/III, VS2000,
 * VS3100/??, VS4000 and VAX6000/???, and only when booting from disk.
 */
Xmain()
{
	int io;
	char *scbb;
	char *new;
	char *hej = "/boot";

        switch (vax_cputype) {

        case VAX_TYP_UV2:
        case VAX_TYP_CVAX:
	case VAX_TYP_RIGEL:
	case VAX_TYP_NVAX:
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
		printf("unknown cpu type %d\nRegister dump:\n", vax_cputype);
		for (io = 0; io < 16; io++)
			printf("r%d 0x%x\n", io, bootregs[io]);
		asm("halt");
        }

	bootset = getbootdev();

	printf("\nhowto 0x%x, bdev 0x%x, booting...", boothowto, bootdev);
	io = open(hej, 0);

	if (io >= 0 && io < SOPEN_MAX) {
		copyunix(io);
	} else {
		printf("Boot failed, saerrno %d\n", errno);
	}
}

/* ARGSUSED */
copyunix(aio)
{
	struct exec     x;
	register int    io = aio, i;
	char           *addr;

	i = read(io, (char *) &x, sizeof(x));
	if (i != sizeof(x) || N_BADMAG(x)) {
		printf("Bad format\n");
		return;
	}

	if (N_GETMAGIC(x) == ZMAGIC && lseek(io, N_TXTADDR(x), SEEK_SET) == -1)
		goto shread;
	if (read(io, (char *) 0x10000, x.a_text) != x.a_text)
		goto shread;
	addr = (char *) x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC || N_GETMAGIC(x) == NMAGIC)
		while ((int) addr & CLOFSET)
			*addr++ = 0;

	if (read(io, addr + 0x10000, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;
	bcopy((void *) 0x10000, 0, (int) addr);

	for (i = 0; i < x.a_bss; i++)
		*addr++ = 0;
	for (i = 0; i < 128 * 512; i++)	/* slop */
		*addr++ = 0;
	printf("done. (%d+%d)\n", x.a_text + x.a_data, x.a_bss);
	hoppabort(x.a_entry, boothowto, bootset);
	(*((int (*) ()) x.a_entry)) ();
	return;
shread:
	printf("Short read\n");
	return;
}

getbootdev()
{
	int i, adaptor, controller, unit, partition, retval;

	adaptor = controller = unit = partition = 0;

	switch (vax_cputype) {
	case VAX_TYP_UV2:
	case VAX_TYP_CVAX:
		adaptor = 0;
		controller = ((rpb->csrphy & 017777) == 0xDC)?1:0;
		unit = rpb->unit;			/* DUC, DUD? */
		
		break;

	case VAX_TYP_RIGEL:
		unit = rpb->unit;
		if (unit > 99)
			unit /= 100;		/* DKB300 is target 3 */
		break;


	case VAX_TYP_8SS:
	case VAX_TYP_750:
		controller = 0;	/* XXX Actually massbuss can be on 3 ctlr's */
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
		controller = 0; /* They are always on ctlr 0 */
		break;

	case BDEV_ST:		/* SCSI-tape on NCR5380 (MV2000) */
	case BDEV_SD:		/* SCSI-disk on NCR5380 (3100/76) */
		/*
		 * No standalone routines for SCSI support yet.
		 * Use rom-routines instead!
		 */
		break;

	default:
		printf("Unsupported boot device %d, trying anyway.\n", bootdev);
		boothowto |= (RB_SINGLE | RB_ASKNAME);
	}
	return MAKEBOOTDEV(bootdev, adaptor, controller, unit, partition);
}

struct devsw    devsw[] = {
	SADEV("rom", romstrategy,nullsys,nullsys, noioctl),
};

int             ndevs = (sizeof(devsw) / sizeof(devsw[0]));

struct fs_ops   file_system[] = {
	{ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat}
};

int             nfsys = (sizeof(file_system) / sizeof(struct fs_ops));

struct disklabel lp;
int part_off = 0;		/* offset into partition holding /boot */
volatile struct uda {
	struct  mscp_1ca uda_ca;           /* communications area */
	struct  mscp uda_rsp;     /* response packets */
	struct  mscp uda_cmd;     /* command packets */
} uda;
volatile struct udadevice *csr;

devopen(f, fname, file)
	struct open_file *f;
	const char    *fname;
	char          **file;
{
	char           *msg;
	int		i, err, off;
	char		line[64];

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
	if ((bootdev != BDEV_TK) && (bootdev != BDEV_CNSL)) {
		msg = getdisklabel((void *)LABELOFFSET + RELOC, &lp);
		if (msg)
			printf("getdisklabel: %s\n", msg);
	}
	return 0;
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
	char    *buf;
	int     size, *rsize;
{
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
		switch (bootdev) {

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
		case BDEV_RD:
		case BDEV_ST:
		case BDEV_SD:

		default:
			romread_uvax(block, size, buf, bootregs);
			break;

		}
		break;

	case VAX_8200:
	case VAX_750:
		if (bootdev != BDEV_HP) {
			while (size > 0) {
				while ((read750(block, bootregs) & 0x01) == 0)
					printf("Retrying read bn# %d\n", block);
				bcopy(0, buf, 512);
				size -= 512;
				buf += 512;
				block++;
			}
		} else
			hpread(block, size, buf);
		break;
	}

	*rsize = nsize;
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
