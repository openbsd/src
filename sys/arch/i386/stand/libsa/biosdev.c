/*	$OpenBSD: biosdev.c,v 1.38 1997/10/18 00:33:15 weingart Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <machine/tss.h>
#include <machine/biosvar.h>
#include <lib/libsa/saerrno.h>
#include "libsa.h"
#include "biosdev.h"

extern int debug;
extern bios_diskinfo_t bios_diskinfo[];

struct biosdisk {
	bios_diskinfo_t *bios_info;
	int	edd_flags;
	struct disklabel disklabel;
};

/*
 * return a word that represents the max number
 * of sectors and heads for this device
 *
 */
u_int32_t
biosdinfo(dev)
	int dev;
{
	u_int f, rv;
	__asm __volatile (DOINT(0x13) "; setc %b0\n\t"
			  /* form a word with ntrack/nhead/nsect packed */
			  "shll	$16, %1; movw %%cx, %w1"
			  /* "movb %%cl, %b1; andb $0x3f, %b1" */
			  : "=a" (f), "=d" (rv)
			  : "0" (0x800), "1" (dev) : "%ecx", "cc");
	return (f & 0xff)? 0 : rv;
}

/*
 * Probe if given bios disk exists.
 *
 * NOTE: This seems to hang on certain machines.  Use biosdinfo()
 * first, and verify with biosdprobe() IFF biosdinfo() succeeds
 * first.
 *
 * XXX - biosdinfo() and biosdprobe() should be integrated into 1 fcn.
 */
u_int32_t
biosdprobe(dev)
	int dev;
{
	u_int32_t val = 0;

	__asm __volatile (
		DOINT(0x13) "\n\t"
		"setc %b0 \n\t"
		: "=a" (val)
		: "0" (0x1500),
		  "d" (dev)
		: "%ecx", "%edx", "cc");

	return(val & 0xffff);
}

/*
 * reset disk system
 */
static __inline int
biosdreset(dev)
	int dev;
{
	int rv;
	__asm __volatile (DOINT(0x13) "; setc %b0" : "=a" (rv)
			  : "0" (0), "d" (dev) : "%ecx", "cc");
	return (rv & 0xff)? rv >> 8 : 0;
}

/*
 * Read/Write a block from given place using the BIOS.
 */
int
biosd_rw(rw, dev, cyl, head, sect, nsect, buf)
	int rw, dev, cyl, head;
	int sect, nsect;
	void* buf;
{
	int rv;
	BIOS_regs.biosr_es = (u_int32_t)buf >> 4;
	__asm __volatile ("movb %b7, %h1\n\t"
			  "movb %b6, %%dh\n\t"
			  "andl $0xf, %4\n\t"
			  /* cylinder; the highest 2 bits of cyl is in %cl */
			  "xchgb %%ch, %%cl\n\t"
			  "rorb  $2, %%cl\n\t"
			  "orb %b5, %%cl\n\t"
			  "incl %%cx\n\t"
			  DOINT(0x13) "\n\t"
			  "setc %b0"
			  : "=a" (rv)
			  : "0" (nsect), "d" (dev), "c" (cyl),
			    "b" (buf), "m" (sect), "m" (head),
			    "m" ((rw == F_READ)? 2: 3)
			  : "cc", "memory");

	return (rv & 0xff)? rv >> 8 : 0;
}

/*
 * check the features supported by the bios for the drive
 */
static __inline int
EDDcheck (dev)
	int dev;
{
	int rv, bm, sgn;
	__asm __volatile(DOINT(0x13) "; setc %b0"
			 : "=a" (rv), "=c" (bm), "=b" (sgn)
			 : "0" (0x4400), "2" (0x55aa) : "%edx", "cc");
	return ((rv & 0xff) && (sgn & 0xffff) == 0xaa55)? bm & 0xffff : -1;
}

int
EDD_rw(rw, dev, daddr, nblk, buf)
	int rw, dev;
	u_int64_t daddr;
	u_int32_t nblk;
	void *buf;
{
	int rv;
	struct EDD_CB cb;

	cb.edd_len = sizeof(cb);
	cb.edd_nblk = nblk;
	cb.edd_buf = (u_int32_t)buf;
	cb.edd_daddr = daddr;

	__asm __volatile (DOINT(0x13) "; setc %b0" : "=a" (rv)
			  : "0" ((rw == F_READ)? 0x4200: 0x4300),
			    "d" (dev), "S" (&cb) : "%ecx", "cc");
	return (rv & 0xff)? rv >> 8 : 0;
}

char *
bios_getdisklabel(dev, label)
	int dev;
	struct disklabel *label;
{
	char *st, buf[DEV_BSIZE];
	struct dos_mbr mbr;
	int error;

	error = biosd_rw(F_READ, dev, 0, 0, 1, 1, &mbr);
	st = getdisklabel(buf, label);
	return(st);
}

int
biosopen(struct open_file *f, ...)
{
	va_list ap;
	register char	*cp, **file;
	dev_t	maj, unit, part;
	register struct biosdisk *bd;
	daddr_t off = LABELSECTOR;
	u_int8_t *buf;
	int i, biosdev;

	va_start(ap, f);
	cp = *(file = va_arg(ap, char **));
	va_end(ap);

#ifdef BIOS_DEBUG
	if (debug)
		printf("%s\n", cp);
#endif

	f->f_devdata = NULL;
	/* search for device specification */
	cp += 2;
	if (cp[2] != ':') {
		if (cp[3] != ':')
			return ENOENT;
		else
			cp++;
	}

	for (maj = 0; maj < nbdevs && 
	     strncmp(*file, bdevs[maj], cp - *file); maj++);
	if (maj >= nbdevs) {
		printf("Unknown device: ");
		for (cp = *file; *cp != ':'; cp++)
			putchar(*cp);
		putchar('\n');
		return EADAPT;
	}

	/* get unit */
	if ('0' <= *cp && *cp <= '9')
		unit = *cp++ - '0';
	else {
		printf("Bad unit number\n");
		return EUNIT;
	}
	/* get partition */
	if ('a' <= *cp && *cp <= 'p')
		part = *cp++ - 'a';
	else {
		printf("Bad partition id\n");
		return EPART;
	}
		
	cp++;	/* skip ':' */
	if (*cp != 0)
		*file = cp;
	else
		f->f_flags |= F_RAW;

	bd = alloc(sizeof(*bd));
	bzero(bd, sizeof(bd));

	switch (maj) {
	case 0:  /* wd */
	case 4:  /* sd */
	case 17: /* hd */
		biosdev = unit | 0x80;
		if (maj == 17)
			unit = 0;
		maj = 17;
		break;
	case 2:  /* fd */
		biosdev = unit;
		break;
	case 7:  /* mcd */
	case 15: /* scd */
	case 6:  /* cd */
	case 18: /* acd */
#ifdef BIOS_DEBUG
		printf("Booting from CD is not yet supported\n");
#endif
	case 3:  /* wt */
#ifdef BIOS_DEBUG
		if (maj == 3)
			printf("Booting from Wangtek is not supported\n");
#endif
	default:
		free(bd, 0);
		return ENXIO;
	}

	bd->bios_info = diskfind(biosdev);
	if (!bd->bios_info)
		return ENXIO;

	/* Get EDD stuff */
	bd->edd_flags = EDDcheck(biosdev);

	/* maj is fixed later w/ disklabel read */
	bootdev = MAKEBOOTDEV(maj, 0, 0, unit, part);

#ifdef BIOS_DEBUG
	if (debug) {
		printf("BIOS geometry: heads=%u, s/t=%u; EDD=%d\n",
		       BIOSNHEADS(bd->dinfo), BIOSNSECTS(bd->dinfo),
		       bd->edd_flags);
	}
#endif

	if (maj == 17) {	/* hd, wd, sd */
		struct dos_mbr	mbr;

		if ((errno = biosstrategy(bd, F_READ, DOSBBSECTOR,
		    DEV_BSIZE, &mbr, NULL)) != 0) {
#ifdef BIOS_DEBUG
			if (debug)
				printf("cannot read MBR\n");
#endif
			free(bd, 0);
			return errno;
		}

		/* check mbr signature */
		if (mbr.dmbr_sign != DOSMBR_SIGNATURE) {
#ifdef BIOS_DEBUG
			if (debug)
				printf("bad MBR signature\n");
#endif
			free(bd, 0);
			return ERDLAB;
		}

		for (off = 0, i = 0; off == 0 && i < NDOSPART; i++)
			if (mbr.dmbr_parts[i].dp_typ == DOSPTYP_OPENBSD)
				off = mbr.dmbr_parts[i].dp_start + LABELSECTOR;

		/* just in case */
		if (off == 0)
			for (off = 0, i = 0; off == 0 && i < NDOSPART; i++)
				if (mbr.dmbr_parts[i].dp_typ == DOSPTYP_NETBSD)
					off = mbr.dmbr_parts[i].dp_start +
						LABELSECTOR;

		if (off == 0) {
#ifdef BIOS_DEBUG
			if (debug)
				printf("no BSD partition\n");
#endif
			free(bd, 0);
			return ERDLAB;
		}
	}

	buf = alloca(DEV_BSIZE);
#ifdef BIOS_DEBUG
	if (debug)
		printf("loading disklabel @ %u\n", off);
#endif
	/* read disklabel */
	if ((errno = biosstrategy(bd, F_READ, off,
				  DEV_BSIZE, buf, NULL)) != 0) {
#ifdef BIOS_DEBUG
		if (debug)
			printf("failed to read disklabel\n");
#endif
		free(bd, 0);
		return ERDLAB;
	}

	if ((cp = getdisklabel(buf, &bd->disklabel)) != NULL) {
#ifdef BIOS_DEBUG
		if (debug)
			printf("%s\n", cp);
#endif
		free(bd, 0);
		return EUNLAB;
	}

	if (maj == 17) { /* figure out what it's exactly */
		switch (bd->disklabel.d_type) {
		case DTYPE_SCSI:  maj = 4;  break;
		default:          maj = 0;  break;
		}
	}

	/* and again w/ fixed maj */
	bootdev = MAKEBOOTDEV(maj, 0, 0, unit, part);

	f->f_devdata = bd;

	return 0;
}

static __inline const char *
biosdisk_err(error)
	u_int error;
{
	static const u_char errs[] = 
/* ignored	"\x00" "successful completion\0" */
		"\x01" "invalid function/parameter\0"
		"\x02" "address mark not found\0"
		"\x03" "write-protected\0"
		"\x04" "sector not found\0"
		"\x05" "reset failed\0"
		"\x06" "disk changed\0"
		"\x07" "drive parameter activity failed\0"
		"\x08" "DMA overrun\0"
		"\x09" "data boundary error\0"
		"\x0A" "bad sector detected\0"
		"\x0B" "bad track detected\0"
		"\x0C" "invalid media\0"
		"\x0E" "control data address mark detected\0"
		"\x0F" "DMA arbitration level out of range\0"
		"\x10" "uncorrectable CRC or ECC error on read\0"
/* ignored	"\x11" "data ECC corrected\0" */
		"\x20" "controller failure\0"
		"\x31" "no media in drive\0"
		"\x32" "incorrect drive type in CMOS\0"
		"\x40" "seek failed\0"
		"\x80" "operation timed out\0"
		"\xAA" "drive not ready\0"
		"\xB0" "volume not locked in drive\0"
		"\xB1" "volume locked in drive\0"
		"\xB2" "volume not removable\0"
		"\xB3" "volume in use\0"
		"\xB4" "lock count exceeded\0"
		"\xB5" "valid eject request failed\0"
		"\xBB" "undefined error\0"
		"\xCC" "write fault\0"
		"\xE0" "status register error\0"
		"\xFF" "sense operation failed\0"
		"\x00" "\0";
	register const u_char *p = errs;

	while (*p && *p != error)
		while(*p++);

	return ++p;
}

static __inline int
biosdisk_errno(error)
	u_int error;
{
	static const struct biosdisk_errors {
		u_char error;
		u_char errno;
	} tab[] = {
		{ 0x01, EINVAL },
		{ 0x03, EROFS },
		{ 0x08, EINVAL },
		{ 0x09, EINVAL },
		{ 0x0A, EBSE },
		{ 0x0B, EBSE },
		{ 0x0C, ENXIO },
		{ 0x0D, EINVAL },
		{ 0x10, EECC },
		{ 0x20, EHER },	
		{ 0x31, ENXIO },
		{ 0x32, ENXIO },
		{ 0x00, EIO }
	};
	register const struct biosdisk_errors *p;

	if (!error)
		return 0;

	for (p = tab; p->error && p->error != error; p++);

	return p->errno;
}

int
biosstrategy(devdata, rw, blk, size, buf, rsize)
	void *devdata;
	int rw;
	daddr_t blk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	u_int8_t error = 0;
	register struct biosdisk *bd = (struct biosdisk *)devdata;
	register size_t i, nsect, n, spt;

	nsect = (size + DEV_BSIZE-1) / DEV_BSIZE;
#if 0
	if (rsize != NULL)
		blk += bd->disklabel.
			d_partitions[B_PARTITION(bd->bsddev)].p_offset;
#endif

#ifdef BIOS_DEBUG
	if (debug)
		printf("biosstrategy(%p,%s,%u,%u,%p,%p), dev=%x:%x\n"
		       "biosdread:",
		       bd, (rw==F_READ?"reading":"writing"), blk, size,
		       buf, rsize, bd->biosdev, bd->bsddev);
#endif

	/* handle floppies w/ different from drive geometry */
	if (!(bd->bios_info->bios_number & 0x80) && bd->disklabel.d_nsectors != 0)
		spt = bd->disklabel.d_nsectors;
	else
		spt = bd->bios_info->bios_sectors;

	for (i = 0; error == 0 && i < nsect;
	     i += n, blk += n, buf += n * DEV_BSIZE) {
		register int	cyl, hd, sect, j;
		void *bb;

		btochs(blk, cyl, hd, sect, (bd->bios_info->bios_heads), spt);
		if ((sect + (nsect - i)) >= spt)
			n = spt - sect;
		else
			n = nsect - i;
		
		/* use a bounce buffer to not cross 64k DMA boundary */
		if ((((u_int32_t)buf) & ~0xffff) !=
		    (((u_int32_t)buf + n * DEV_BSIZE) & ~0xffff)) {
			/*
			 * XXX we believe that all the io is buffered
			 * by fs routines, so no big reads anyway
			 */
			bb = alloca(n * DEV_BSIZE);
			if (rw != F_READ)
				bcopy (buf, bb, n * DEV_BSIZE);
		} else
			bb = buf;
#ifdef BIOS_DEBUG
		if (debug)
			printf(" (%d,%d,%d,%d)@%p", cyl, hd, sect, n, bb);
#endif
		/* Try to do operation up to 5 times */
		for (error = 1, j = 5; j-- && error;)
			switch (error = biosd_rw(rw, bd->bios_info->bios_number,
						 cyl, hd, sect, n, bb)) {
			case 0x00:	/* No errors */
			case 0x11:	/* ECC corrected */
				error = 0;
				break;

			default:	/* All other errors */
				printf("\nBIOS error 0x%x (%s)\n", error,
				       biosdisk_err(error));
				biosdreset(bd->bios_info->bios_number);
				break;
			}

		if (bb != buf && rw == F_READ)
			bcopy (bb, buf, n * DEV_BSIZE);
	}

#ifdef BIOS_DEBUG
	if (debug) {
		if (error != 0)
			printf("=0x%x(%s)", error, biosdisk_err(error));
		putchar('\n');
	}
#endif

	if (rsize != NULL)
		*rsize = i * DEV_BSIZE;

	return biosdisk_errno(error);
}

int
biosclose(f)
	struct open_file *f;
{
	free(f->f_devdata, 0);
	return 0;
}

int
biosioctl(f, cmd, data)
	struct open_file *f;
	u_long cmd;
	void *data;
{
	return 0;
}

