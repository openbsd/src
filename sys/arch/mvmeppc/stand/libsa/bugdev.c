/*	$OpenBSD: bugdev.c,v 1.5 2011/03/13 00:13:53 deraadt Exp $ */

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
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
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <machine/prom.h>

#include <stand.h>
#include <ufs.h>
#include "rawfs.h"
#include "tftpfs.h"

#include "libsa.h"

int errno;
int bootdev_type = BUGDEV_DISK;

#if 1 
#define md_swap_long(x) ( (((x) >> 24) & 0xff) | (((x) >> 8 ) & 0xff00) | \
			(((x) << 8) & 0xff0000) | (((x) << 24) & 0xff000000))
#else 
#define md_swap_long(x) (x)
#endif 

struct bugdev_softc {
	int	fd;	/* Prom file descriptor */
	u_int	pnum;	/* Partition number */	
	u_int	poff;	/* Partition offset */
	u_int	psize;	/* Partition size */
	short	clun;
	short	dlun;
} bugdev_softc[1];

extern struct fs_ops ufs_file_system;
extern struct fs_ops tftp_file_system;
extern struct fs_ops raw_file_system;

struct fs_ops file_system[1];

struct mvmeprom_dskio tape_ioreq;

#ifdef BUG_DEBUG
unsigned io = 0, mem = 0;
#define PCI_BASE 0x80000000
#define CFC *(unsigned *)(PCI_BASE + 0xcfc)
#define CF8 *(unsigned *)(PCI_BASE + 0xcf8)
#define BUS_SHIFT 16
#define DEVICE_SHIFT 11
#define FNC_SHIFT 8

unsigned
bugpci_make_tag(bus, dev, fnc)
	int bus, dev, fnc;
{
	return (bus << BUS_SHIFT) | (dev << DEVICE_SHIFT) | (fnc << FNC_SHIFT);
}

static unsigned
bugpci_gen_config_reg(tag, offset)
	unsigned tag;
	int offset;
{
	unsigned reg;

	/* config mechanism #2, type 0
	/* standard cf8/cfc config */
	reg =  0x80000000 | tag  | offset;

	return reg;
}

/*#define DEBUG_CONFIG */
unsigned
bugpci_conf_read(bus, dev, func, offset)
	int bus, dev, func, offset;
{

	unsigned data;
	unsigned reg, tag, xx;

	if(offset & 3 || offset < 0 || offset >= 0x100) {
		printf ("bugpci_conf_read: bad reg %x\n", offset);
		return(~0);
	}

        tag = bugpci_make_tag(bus, dev, func);
	reg = bugpci_gen_config_reg(tag, offset);
	reg = md_swap_long(reg);
	/* if invalid tag, return -1 */
	if (reg == 0xffffffff) {
		return 0xffffffff;
	}

	CF8 = reg;
	xx = CF8;
	data = CFC;
	data = md_swap_long(data);

	CF8 = 0;

	return(data);
}

void
bugpci_conf_write(bus, dev, func, offset, data)
	int bus, dev, func, offset;
	unsigned data;
{
	unsigned reg, tag, xx;

        tag = bugpci_make_tag(bus, dev, func);
	reg = bugpci_gen_config_reg(tag, offset);
	reg = md_swap_long(reg);

	/* if invalid tag, return ??? */
	if (reg == 0xffffffff) {
		return;
	}

	CF8 = reg;
	xx = CF8;
	data = md_swap_long(data);
	CFC = data;
	CF8 = 0;
	xx = CF8;
}
#endif 

static u_long
get_long(p)
	const void *p;
{
	const unsigned char *cp = p;
	
	return cp[0] | (cp[1] << 8) | (cp[2] << 16) | (cp[3] << 24);
}

/*
 * Find a valid disklabel.
 */
static int
search_label(devp, off, buf, lp, off0)
	void *devp;
	u_long off;
	char *buf;
	struct disklabel *lp;
	u_long off0;
{
	size_t read;
	struct dos_partition *p;
	int i;
	u_long poff;
	static int recursion;
	
	if (dsk_strategy(devp, F_READ, off, DEV_BSIZE, buf, &read) 
	    || read != DEV_BSIZE)
		return ERDLAB;
	
	if (buf[510] != 0x55 || buf[511] != 0xaa)
		return ERDLAB;

	if (recursion++ <= 1)
		off0 += off;
	for (p = (struct dos_partition *)(buf + DOSPARTOFF), i = 4;
	     --i >= 0; p++) {
		if (p->dp_typ == DOSPTYP_OPENBSD) {
			poff = get_long(&p->dp_start) + off0;
			if (dsk_strategy(devp, F_READ, poff + LABELSECTOR,
				     DEV_BSIZE, buf, &read) == 0
			    && read == DEV_BSIZE) {
				if (!getdisklabel(buf, lp)) {
					recursion--;
					return 0;
				}
			}
			if (dsk_strategy(devp, F_READ, off, DEV_BSIZE, buf, &read)
			    || read != DEV_BSIZE) {
				recursion--;
				return ERDLAB;
			}
		} else if (p->dp_typ == DOSPTYP_EXTEND) {
			poff = get_long(&p->dp_start);
			if (!search_label(devp, poff, buf, lp, off0)) {
				recursion--;
				return 0;
			}
			if (dsk_strategy(devp, F_READ, off, DEV_BSIZE, buf, &read)
			    || read != DEV_BSIZE) {
				recursion--;
				return ERDLAB;
			}
		}
	}
	recursion--;
	return ERDLAB;
}

int dsk_read_disklabel(devp)
	void *devp;
{
	static	char iobuf[MAXBSIZE];
	struct disklabel label;
	int error = 0;
	int i;
	
	register struct bugdev_softc *pp = (struct bugdev_softc *)devp;

#ifdef DEBUG
	printf("dsk_open:\n");
#endif
	/* First try to find a disklabel without MBR partitions */
	if (dsk_strategy(pp, F_READ, LABELSECTOR, DEV_BSIZE, iobuf, &i) != 0
	    || i != DEV_BSIZE
	    || getdisklabel(iobuf, &label)) {
		/* Else try MBR partitions */
		error = search_label(pp, 0, iobuf, &label, 0);
		if (error && error != ERDLAB)
			return (error);
	}
	if (error == ERDLAB) {
		if (pp->pnum)
			/* User specified a partition, but there is none */
			return (error);
		/* No, label, just use complete disk */
		pp->poff = 0;
	} else {
		pp->poff = label.d_partitions[pp->pnum].p_offset;
                pp->psize = label.d_partitions[pp->pnum].p_size;
	}
	return(0);
}


int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	register struct bugdev_softc *pp = &bugdev_softc[0];
	int	error;
	char	*dev, *cp;

	pp->clun = (short)bugargs.ctrl_lun;
	pp->dlun =  (short)bugargs.dev_lun;
	pp->poff = 0;
	pp->psize = 0;
	pp->fd = 0;
	pp->pnum = 0;

	f->f_devdata = (void *)pp;
	
	switch (bootdev_type) {
	case BUGDEV_DISK:
		dev = bugargs.arg_start;
		/*
		 * Extract partition # from boot device string.
		 */
		for (cp = dev; *cp; cp++) /* void */;
		while (*cp != '/' && cp > dev) {
			if (*cp == ':')
				pp->pnum = *(cp+1) - 'a';
			--cp;
		}
		
		error = dsk_read_disklabel(pp);
		if (error)
			return(error);
		
		bcopy(&ufs_file_system, file_system, sizeof file_system[0]);
		break;

	case BUGDEV_NET:
		bcopy(&tftp_file_system, file_system, sizeof file_system[0]);
		break;

	case BUGDEV_TAPE:
		bcopy(&raw_file_system, file_system, sizeof file_system[0]);
		break;
	}
	
	f->f_dev = &devsw[bootdev_type];
	nfsys = 1;
	*file = (char *)fname;
	return (0);
}

/* silly block scale factor */
#define BUG_BLOCK_SIZE 256
#define BUG_SCALE (512/BUG_BLOCK_SIZE)
int
dsk_strategy(devdata, func, dblk, size, buf, rsize)
	void *devdata;
	int func;
	daddr32_t dblk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	struct mvmeprom_dskio dio;
	register struct bugdev_softc *pp = (struct bugdev_softc *)devdata;
	daddr32_t blk = dblk + pp->poff;

	twiddle();

	dio.ctrl_lun = pp->clun;
	dio.dev_lun = pp->dlun;
	dio.status = 0;
	dio.pbuffer = buf;
	dio.blk_num = blk * BUG_SCALE;
	dio.blk_cnt = size / BUG_BLOCK_SIZE; /* assumed size in bytes */
	dio.flag = 0;
	dio.addr_mod = 0;
#ifdef DEBUG
	printf("dsk_strategy: size=%d blk=%d buf=%x\n", size, blk, buf);
	printf("ctrl %d dev %d\n", dio.ctrl_lun, dio.dev_lun);
#endif
	mvmeprom_diskrd(&dio);

	*rsize = dio.blk_cnt * BUG_BLOCK_SIZE;
#ifdef DEBUG
printf("rsize %d status %x\n", *rsize, dio.status);
#endif

	if (dio.status)
		return (EIO);
	return (0);
}

int
dsk_open(f)
	struct open_file *f;
{
#ifdef DEBUG
	register struct bugdev_softc *pp = (struct bugdev_softc *)f->f_devdata;
	printf("dsk_open:\n");
	printf("using mvmebug ctrl %d dev %d\n",
		pp->clun, pp->dlun);
#endif 
	return (0);
}

int
dsk_close(f)
	struct open_file *f;
{
	return (EIO);
}

int
dsk_ioctl(f, cmd, data)
	struct open_file *f;
	u_long cmd;
	void *data;
{
	return (EIO);
}

#define NFR_TIMEOUT	5
/* netboot stuff */
int
net_strategy(devdata, func, nblk, size, buf, rsize)
	void *devdata;
	int func;
	daddr32_t nblk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	int attempts = 0;
   struct mvmeprom_netfread nfr;
	register struct bugdev_softc *pp = (struct bugdev_softc *)devdata;

retry:

   attempts+=1;

   nfr.clun = pp->clun;
	nfr.dlun = pp->dlun;
	nfr.status = 0;
	nfr.addr = buf;
	nfr.bytes = 0;
	nfr.blk = nblk;
	nfr.timeout = NFR_TIMEOUT;
#ifdef DEBUG
	printf("net_strategy: size=%d blk=%d buf=%x\n", size, nblk, buf);
	printf("ctrl %d dev %d\n", nfr.clun, nfr.dlun);
#endif
	mvmeprom_netfread(&nfr);
	
#ifdef BUG_DEBUG
	io = bugpci_conf_read(0, 14, 0, 0x10) & ~0xf;
	mem = bugpci_conf_read(0, 14, 0, 0x14) & ~0xf;

	printf(" IO @ %x\n", io);
	printf("MEM @ %x\n", mem);

#define PRINT_REG(regname, x) printf("%s = 0x%x\n", regname, \
	    md_swap_long(*(unsigned *)(io + x)))
	
        PRINT_REG("CSR0", 0x00);
        PRINT_REG("CSR1", 0x08);
        PRINT_REG("CSR2", 0x10);
        PRINT_REG("CSR3", 0x18);
        PRINT_REG("CSR4", 0x20);
        PRINT_REG("CSR5", 0x28);
        PRINT_REG("CSR6", 0x30);
        PRINT_REG("CSR7", 0x38);
        PRINT_REG("CSR8", 0x40);
        PRINT_REG("CSR9", 0x48);
        PRINT_REG("CSR10", 0x50);
        PRINT_REG("CSR11", 0x58);
        PRINT_REG("CSR12", 0x60);
        PRINT_REG("CSR13", 0x68);
        PRINT_REG("CSR14", 0x70);
        PRINT_REG("CSR15", 0x78);
#endif 
	if (rsize) {
		*rsize = nfr.bytes;
	}
#ifdef DEBUG
	printf("rsize %d status %x\n", *rsize, nfr.status);
#endif

	if (nfr.status)
      if (attempts < 10) 
         goto retry;
      else
		   return (EIO);
	return (0);
}

int
net_open(struct open_file *f, ...)
{
	va_list ap;
	struct mvmeprom_netfopen nfo;
        register struct bugdev_softc *pp = (struct bugdev_softc *)f->f_devdata;
	char *filename;
	short nfoerr = 0;
	va_start(ap, f);
	filename = va_arg(ap, char *);
	va_end(ap);

#ifdef DEBUG
	printf("net_open: using mvmebug ctrl %d dev %d, filename: %s\n",
	    pp->clun, pp->dlun, filename);
#endif
	nfo.clun = pp->clun;
	nfo.dlun = pp->dlun;
	nfo.status = 0;
	strlcpy(nfo.filename, filename, sizeof nfo.filename);
	/* .NETFOPN syscall */
	mvmeprom_netfopen(&nfo);
	
#ifdef DEBUG
	if (nfo.status) {
		nfoerr = nfo.status;
		printf("net_open: ci err = 0x%x, cd err = 0x%x\n",
		    ((nfoerr >> 8) & 0x0F), (nfoerr & 0x0F));
	}
#endif
	return (nfo.status);
}

int
net_close(f)
	struct open_file *f;
{
	return (EIO);
}

int
net_ioctl(f, cmd, data)
	struct open_file *f;
	u_long cmd;
	void *data;
{
	return (EIO);
}

int
tape_open(struct open_file *f, ...)
{
	va_list ap;
	int	error = 0;
	int  	part;
	struct mvmeprom_dskio *ti;
	char *fname;		/* partition number, i.e. "1" */

	va_start(ap, f);
	fname = va_arg(ap, char *);
	va_end(ap);
	
	/*
	 * Set the tape segment number to the one indicated
	 * by the single digit fname passed in above.
	 */
	if ((fname[0] < '0') && (fname[0] > '9')) {
		return ENOENT;
	}
	part = fname[0] - '0';
	fname = NULL;

	/*
	 * Setup our part of the saioreq.
	 * (determines what gets opened)
	 */
	ti = &tape_ioreq;
	bzero((caddr_t)ti, sizeof(*ti));

	ti->ctrl_lun = bugargs.ctrl_lun;
	ti->dev_lun = bugargs.dev_lun;
	ti->status = 0;
	ti->pbuffer = NULL;
	ti->blk_num = part;
	ti->blk_cnt = 0;
	ti->flag = 0;
	ti->addr_mod = 0;

	f->f_devdata = ti;

	return (0);
}

int
tape_close(f)
	struct open_file *f;
{
	struct mvmeprom_dskio *ti;


	ti = f->f_devdata;
	f->f_devdata = NULL;
	return 0;
}

#define MVMEPROM_SCALE (512/MVMEPROM_BLOCK_SIZE)

int
tape_strategy(devdata, flag, dblk, size, buf, rsize)
	void	*devdata;
	int	flag;
	daddr32_t dblk;
	size_t  size;
	void    *buf;
	size_t  *rsize;
{
	struct mvmeprom_dskio *ti;
	int ret;

	ti = devdata;

	if (flag != F_READ)
		return(EROFS);

	ti->status = 0;
	ti->pbuffer = buf;
	/* don't change block #, set in open */
	ti->blk_cnt = size / (512 / MVMEPROM_SCALE);

	ret = mvmeprom_diskrd(ti);

	if (ret != 0)
		return (EIO);

	*rsize = (ti->blk_cnt / MVMEPROM_SCALE) * 512;
	ti->flag |= IGNORE_FILENUM; /* ignore next time */

	return (0);
}

int
tape_ioctl(f, cmd, data)
	struct open_file *f;
	u_long cmd;
	void *data;
{
	return EIO;
}


