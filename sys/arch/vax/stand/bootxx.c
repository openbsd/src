/* $NetBSD: bootxx.c,v 1.4 1995/10/20 13:35:43 ragge Exp $ */
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

#include "../mba/mbareg.h"
#include "../mba/hpreg.h"

#include "../include/pte.h"
#include "../include/sid.h"
#include "../include/mtpr.h"
#include "../include/reg.h"
#include "../include/rpb.h"

#define NRSP 0 /* Kludge */
#define NCMD 0 /* Kludge */
#include "../uba/ubareg.h"
#include "../uba/udareg.h"
#include "../vax/mscp.h"

#include "data.h"
#include "vaxstand.h"

#include <a.out.h>

int             romstrategy(), romopen();
int	command(int, int);

/*
 * Boot program... argume passed in r10 and r11 determine whether boot
 * stops to ask for system name and which device boot comes from.
 */

volatile u_int  devtype, bootdev;
unsigned        opendev, boothowto, bootset;
int             cpu_type, cpunumber;
unsigned *bootregs;
int is_750 = 0, is_mvax = 0, is_tmscp = 0;
struct	rpb *rpb;

main()
{
	int io;
	char *hej = "/boot";

        cpu_type = mfpr(PR_SID);
        cpunumber = (mfpr(PR_SID) >> 24) & 0xFF;

        switch (cpunumber) {

        case VAX_78032:
        case VAX_650:
        {
                int	cpu_sie;        /* sid-extension */

                is_mvax = 1;
                cpu_sie = *((int *) 0x20040004) >> 24;
                cpu_type |= cpu_sie;
		rpb = (struct rpb *)bootregs[11];
		bootdev = rpb->devtyp;

                break;
        }
        case VAX_750:
                is_750 = 1;
		bootdev = bootregs[10];

                break;
        }

	bootset = getbootdev();

	printf("\nhowto 0x%x, bdev 0x%x, booting...\n", boothowto, bootdev);
	io = open(hej, 0);

	if (io >= 0 && io < SOPEN_MAX) {
		copyunix(io);
	} else {
		printf("Boot failed. errno %d (%s)\n", errno, strerror(errno));
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
		printf("Bad format: errno %s\n", strerror(errno));
		return;
	}
	printf("%d", x.a_text);
	if (N_GETMAGIC(x) == ZMAGIC && lseek(io, N_TXTADDR(x), SEEK_SET) == -1)
		goto shread;
	if (read(io, (char *) 0x10000, x.a_text) != x.a_text)
		goto shread;
	addr = (char *) x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC || N_GETMAGIC(x) == NMAGIC)
		while ((int) addr & CLOFSET)
			*addr++ = 0;
	printf("+%d", x.a_data);
	if (read(io, addr + 0x10000, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;
	bcopy((void *) 0x10000, 0, (int) addr);
	printf("+%d", x.a_bss);
	for (i = 0; i < x.a_bss; i++)
		*addr++ = 0;
	for (i = 0; i < 128 * 512; i++)	/* slop */
		*addr++ = 0;
	printf(" start 0x%x\n", x.a_entry);
	hoppabort(x.a_entry, boothowto, bootset);
	(*((int (*) ()) x.a_entry)) ();
	return;
shread:
	printf("Short read\n");
	return;
}

getbootdev()
{
	int	i, major, adaptor, controller, unit, partition;


	switch (cpunumber) {
	case VAX_78032:
	case VAX_650:
		adaptor = 0;
		controller = ((rpb->csrphy & 017777) == 0xDC)?1:0;
		unit = rpb->unit;			/* DUC, DUD? */
		
		break;

	case VAX_750:
		controller = 0;	/* XXX Actually massbuss can be on 3 ctlr's */
		unit = bootregs[3];
		break;
	}

	partition = 0;

	switch (bootdev) {
	case 0:			/* massbuss boot */
		major = 0;	/* hp / ...  */
		adaptor = (bootregs[1] & 0x6000) >> 17;
		break;

	case 17:		/* UDA50 boot */
		major = 9;	/* ra / mscp  */
		if (is_750)
			adaptor = (bootregs[1] & 0x40000 ? 0 : 1);
		break;

	case 18:		/* TK50 boot */
		major = 15;	/* tms / tmscp  */
		is_tmscp = 1;	/* use tape spec in mscp routines */
		break;

	default:
		printf("Unsupported boot device %d, trying anyway.\n", bootdev);
		boothowto |= (RB_SINGLE | RB_ASKNAME);
	}
	return MAKEBOOTDEV(major, adaptor, controller, unit, partition);
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
char io_buf[MAXBSIZE];
volatile struct uda {
	struct  uda1ca uda_ca;           /* communications area */
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
	*file = fname;

	/*
	 * On uVAX we need to init [T]MSCP ctlr to be able to use it.
	 */
	if (is_mvax) {
		switch (bootdev) {
		case 17:	/* MSCP */
		case 18:	/* TMSCP */
			csr = (struct udadevice *)rpb->csrphy;

			csr->udaip = 0; /* Start init */
			while((csr->udasa & UDA_STEP1) == 0);
			csr->udasa = 0x8000;
			while((csr->udasa & UDA_STEP2) == 0);
			csr->udasa = (short)(((u_int)&uda)&0xffff) + 8;
			while((csr->udasa & UDA_STEP3) == 0);
			csr->udasa = 0x10;
			while((csr->udasa & UDA_STEP4) == 0);
			csr->udasa = 0x0001;

			uda.uda_ca.ca_rspdsc =
			    (int) &uda.uda_rsp.mscp_cmdref;
			uda.uda_ca.ca_cmddsc =
			    (int) &uda.uda_cmd.mscp_cmdref;
			if (is_tmscp)
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
	if (!is_tmscp) {
		msg = getdisklabel((void *)LABELOFFSET + RELOC, &lp);
		if (msg) {
			printf("getdisklabel: %s\n", msg);
		}
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

	switch (cpunumber) {

	case VAX_650:
	case VAX_78032:
		switch (bootdev) {

		case 17: /* MSCP */
			uda.uda_cmd.mscp_seq.seq_lbn = dblk;
			uda.uda_cmd.mscp_seq.seq_bytecount = size;
			uda.uda_cmd.mscp_seq.seq_buffer = (int)buf;
			uda.uda_cmd.mscp_unit = rpb->unit;
			command(M_OP_READ, 0);
			break;

		case 18: /* TMSCP */
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

		}
		break;

	case VAX_750:
		if (bootdev) {
			while (size > 0) {
				if ((read750(block, bootregs) & 0x01) == 0)
					return 1;

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
		mr->mba_map[mapnr++] = PG_V | pfnum++;
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
