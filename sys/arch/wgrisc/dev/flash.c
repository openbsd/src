/*	$OpenBSD: flash.c,v 1.3 1998/10/03 21:18:59 millert Exp $ */

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
 *	Per Fogelstrom for Willowglen Singapore.
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

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/pio.h>

#include <wgrisc/riscbus/riscbus.h>
#include <wgrisc/dev/flashreg.h>
#include <wgrisc/wgrisc/wgrisctype.h>

extern int cputype;

void flattach __P((struct device *, struct device *, void *));
int  flmatch __P((struct device *, void *, void *));

#define	FLUNIT(dev)	((dev & 0xf0) >> 4)
#define	FLPART(dev)	(minor(dev) & 0x0f)
#define MAKEFLDEV(maj, unit, part)      MAKEDISKDEV(maj, unit, part)
         
#define FLLABELDEV(dev) (MAKEFLDEV(major(dev), FLUNIT(dev), RAW_PART))   



/*
 *	Flash cache stuff
 */

#define	MAX_BLKS	64		/* Define size of cache */

#ifdef IS_THERE_A_16BIT_STORE_PROBLEM
struct flctag {
	u_int16_t	next;		/* Pointer to next on list	*/
	u_int16_t	offset;		/* Flash memory block offset	*/
	u_int16_t	age;		/* LSW of time last updated	*/
	u_int16_t	stat;		/* Status			*/
};

struct flcache {
	int		magic;
	u_int16_t	free;		/* List of free blocks		*/
	u_int16_t	nfree;		/* Number of free blocks left	*/
	u_int16_t	lbusy;		/* Last busy block		*/
	u_int16_t	stat;		/* Cache status			*/
	char		*cache;		/* Pointer to cache data area	*/

	struct flctag flcblk[MAX_BLKS];	/* Flash cache block tags	*/
};
#else
struct flctag {
	u_int32_t	next;		/* Pointer to next on list	*/
	u_int32_t	offset;		/* Flash memory block offset	*/
	u_int32_t	age;		/* LSW of time last updated	*/
	u_int32_t	stat;		/* Status			*/
};

struct flcache {
	int		magic;
	u_int32_t	free;		/* List of free blocks		*/
	u_int32_t	nfree;		/* Number of free blocks left	*/
	u_int32_t	lbusy;		/* Last busy block		*/
	u_int32_t	stat;		/* Cache status			*/
	char		*cache;		/* Pointer to cache data area	*/

	struct flctag flcblk[MAX_BLKS];	/* Flash cache block tags	*/
};
#endif

#define	FLC_MAGIC	0xf1cac3e0

#define	BLK_SFREE	0x0001		/* Sanity check blk on free	*/
#define	BLK_SBUSY	0x0002		/* Sanity check blk on busy	*/
#define	BLK_ONFREE	0x0010		/* Block is on free list	*/
#define	BLK_ONBUSY	0x0020		/* Block is on busy list	*/

#define	BLK_FIND	0		/* Serach cmd "find"		*/
#define	BLK_FORCE	1		/* Search cmd "must find"	*/
#define	BLK_LAST	2		/* Search cmd "put last" if found */

/*	Flash memory type descriptor */

struct fltype {
	char	*fl_name;
	int	fl_size;
	int	fl_blksz;
	u_char	fl_manf;
	u_char	fl_type;
};
struct fltype fllist[] = {
	{ "Samsung KM29N16000",	2097152, 4096, 0xec, 0x64 },
	{ NULL },
};

struct flsoftc {
	struct device	sc_dev;
	int		sc_prot;
	size_t		sc_size;
	int		sc_present;
	int		sc_opncnt;
	struct flcache *sc_cache;
	struct fltype *sc_ftype;
	struct disklabel sc_label;
};

struct cfattach fl_ca = {
	sizeof(struct flsoftc), flmatch, flattach
};
struct cfdriver fl_cd = {
	NULL, "fl", DV_DISK, 0	/* Yes! We want is as root device */
};

static void flgetdisklabel __P((dev_t, struct flsoftc *));
static void flinitcache __P((struct flsoftc *));
void flstrategy __P((struct buf *));


int
flmatch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	struct confargs *ca = args;

	if(!BUS_MATCHNAME(ca, "fl"))
		return(0);
	return(1);
}

void
flattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct flsoftc *sc = (struct flsoftc *)self;
	struct fltype *flist;
	int i, manf, type;
	u_int32_t next;
	struct flctag *blkbase;

	switch(cputype) {
	case WGRISC9100:	/* WGRISC9100 can have 4 chips */
		sc->sc_opncnt = 0;
		sc->sc_present = 0;
		sc->sc_ftype = NULL;
		OUT_FL_CTRL(0, 0);	/* All CS lines high */
		for(i = 0; i < 4; i++) {
			OUT_FL_CLE(FL_READID, (1 << i));
			OUT_FL_ALE1(0, (1 << i));
			manf = IN_FL_DATA;
			type = IN_FL_DATA;
			flist = fllist;
			while(flist->fl_name != 0) {
				if(flist->fl_manf == manf &&
				   flist->fl_type == type) {
					sc->sc_present |= 1 << i;
					sc->sc_size += flist->fl_size;
					if(sc->sc_ftype == NULL) {
						sc->sc_ftype = flist;
					}
					else if(sc->sc_ftype == flist) {
					}
/* XXX Protection test type dependent ? */
					OUT_FL_CLE(FL_READSTAT, (1 << i));
					if(!(IN_FL_DATA & FLST_UNPROT)) {
						sc->sc_prot = 1;
					}
					break;
				}
				flist++;
			}
		}
		break;

	default:
		printf("fl: Unknown cputype '%d'", cputype);
	}
	if(sc->sc_ftype != NULL) {
		printf(" %s, %d*%d bytes%s.", sc->sc_ftype->fl_name,
				sc->sc_size / sc->sc_ftype->fl_size,
				sc->sc_ftype->fl_size,
				sc->sc_prot ? " Write protected" : "");
	}
	else {
		printf("WARNING! Flash type not identified!");
	}
	printf("\n");

	/*
	 *	Now set up the sram cache. We usually have 128k sram
	 *	on the board and we use up 25% or 32kb of this for the
	 *	flash cache.
	 */

	sc->sc_cache = (struct flcache *)(RISC_SRAM_START+16);
	blkbase = &sc->sc_cache->flcblk[0];

	if(sc->sc_cache->magic != FLC_MAGIC) {
		printf("fl0: *WARNING* flash cache not initialized!");
		printf(" Initializing to %d blocks.\n", MAX_BLKS-1);

		flinitcache(sc);
	}
	else {	/* Cache initialized. Make sanity check. */
		for(i = 1; i < MAX_BLKS; i++) {
			blkbase[i].stat &= ~(BLK_SFREE | BLK_SBUSY);
		}
		next = sc->sc_cache->free;
		i = 0;
		while(next && (next < MAX_BLKS) && (i < MAX_BLKS)) {
			blkbase[next].stat |= BLK_SFREE;
			next = blkbase[next].next;
			i++;
		}
		i = 0;
		next = blkbase[0].next;
		while(next && (next < MAX_BLKS) && (i < MAX_BLKS)) {
			blkbase[next].stat |= BLK_SBUSY;
			next = blkbase[next].next;
			i++;
		}
		for(i = 1; i < MAX_BLKS; i++) {
			switch(blkbase[i].stat & (BLK_SBUSY|BLK_SFREE|BLK_ONFREE|BLK_ONBUSY)) {
			case BLK_SBUSY|BLK_ONBUSY:
			case BLK_SFREE|BLK_ONFREE:
				break;

			default:
				printf("fl0: cache sanity err blk %d stat %x\n",
					i, blkbase[i].stat);
				break;
			}
		}
	}
}

static void
flinitcache(sc)
	struct flsoftc *sc;
{
	int i;
	struct flctag *blkbase;

	blkbase = &sc->sc_cache->flcblk[0];
	for(i = 1; i < MAX_BLKS; i++) {
		blkbase[i].next = i+1;
		blkbase[i].stat = BLK_ONFREE;
	}
	blkbase[MAX_BLKS-1].next = 0;	/* mark end */
	sc->sc_cache->free = 1;	/* first free */
	sc->sc_cache->nfree = MAX_BLKS-1;
	blkbase[0].next = 0;		/* no busy blocks */
	sc->sc_cache->lbusy = 0;	/* no busy blocks */
	sc->sc_cache->stat = 0;
	sc->sc_cache->cache = (char *)sc->sc_cache + sizeof(struct flcache);
	sc->sc_cache->magic = FLC_MAGIC;
}

/*
 *	Return index to cached block if in cache.
 *	Also execute command.
 */
static int
flblkincache(sc, offs, command)
	struct flsoftc *sc;
	size_t offs;
	int command;
{
	u_int32_t next, prev, offset;
	struct flctag *blkbase;

	offset = offs >> DEV_BSHIFT;
	blkbase = &sc->sc_cache->flcblk[0];
	next = blkbase[0].next;
	prev = 0;

	while(next && (blkbase[next].offset != offset)) {
		prev = next;
		next = blkbase[next].next;
	}
	if(next && (command == BLK_LAST)) {
		if(sc->sc_cache->lbusy != next) {
			blkbase[prev].next = blkbase[next].next;
			blkbase[next].next = 0;
			blkbase[sc->sc_cache->lbusy].next = next;
			sc->sc_cache->lbusy = next;
		}
		return(next);		/* already last */
	}
	if(next || (command != BLK_FORCE))
		return(next);

	/* BLK_FORCE */
	printf("fl0: expected offset %x not found in cache!!\n", offset);
	next = blkbase[0].next;
	while(next) {
		printf("%d/%x ",next, blkbase[next].offset);
		next = blkbase[next].next;
	}
	printf("\n");
	next = sc->sc_cache->free;
	while(next) {
		printf("%d/%x ",next, blkbase[next].offset);
		next = blkbase[next].next;
	}
	printf("\n");
mdbpanic();

	return(0);
}

/*
 *	Return block erase status.
 */
static int
flblkerased(sc, blk, offs, cnt)
	struct flsoftc *sc;
	size_t offs;
	size_t cnt;
{
	int chip;
	u_int32_t blkadr;
	int erased = TRUE;

	chip = 1 << (offs / sc->sc_ftype->fl_size);
	blkadr = offs % sc->sc_ftype->fl_size;

	OUT_FL_CLE(FL_READ1, chip);
	OUT_FL_ALE3(blkadr, chip);
	WAIT_FL_RDY;
	while(cnt--) {
		if(IN_FL_DATA != 0xff)
			erased = FALSE;
	}
	return(erased);
}

static int
flgetblk(sc, blk, offs, cnt)
	struct flsoftc *sc;
	char *blk;
	size_t offs;
	size_t cnt;
{
	int chip;
	u_int32_t blkadr;

	chip = 1 << (offs / sc->sc_ftype->fl_size);
	blkadr = offs % sc->sc_ftype->fl_size;

	OUT_FL_CLE(FL_READ1, chip);
	OUT_FL_ALE3(blkadr, chip);
	WAIT_FL_RDY;
	while(cnt--) {
		*blk++ = IN_FL_DATA;
	}
	return(0);
}

static int
flputblk(sc, blk, offs, cnt)
	struct flsoftc *sc;
	char *blk;
	size_t offs;
	size_t cnt;
{
	int chip;
	u_int32_t blkadr;

	chip = 1 << (offs / sc->sc_ftype->fl_size);
	blkadr = offs % sc->sc_ftype->fl_size;

	OUT_FL_CLE(FL_SEQDI, chip);
	OUT_FL_ALE3(blkadr, chip);
	while(cnt--) {
		OUT_FL_DATA(*blk);
		blk++;
	}
	OUT_FL_CLE(FL_PGPROG, chip);
	WAIT_FL_RDY;
	OUT_FL_CLE(FL_READSTAT, chip);
	if(IN_FL_DATA & FLST_ERROR) {
		return(-1);
	}
	return(0);
}

static int
fleraseblk(sc, offs)
	struct flsoftc *sc;
	size_t offs;
{
	int chip;
	u_int32_t blkadr;

	chip = 1 << (offs / sc->sc_ftype->fl_size);
	blkadr = offs % sc->sc_ftype->fl_size;

	OUT_FL_CLE(FL_BLERASE, chip);
	OUT_FL_ALE2(blkadr, chip);
	OUT_FL_CLE(FL_REERASE, chip);
	WAIT_FL_RDY;
	OUT_FL_CLE(FL_READSTAT, chip);
	if(IN_FL_DATA & FLST_ERROR) {
		return(-1);
	}
	return(0);
}

/*
 *	Get next free block and put it last on busy list.
 */
static int
flgetfree(sc, offs)
	struct flsoftc *sc;
	size_t offs;
{
	u_int32_t blkno;
	struct flctag *blkbase;

	blkbase = &sc->sc_cache->flcblk[0];
	blkno = sc->sc_cache->free;
	if(blkno) {
		sc->sc_cache->free = blkbase[blkno].next;
		sc->sc_cache->nfree--;
		blkbase[blkno].next = 0;
		blkbase[blkno].stat = BLK_ONBUSY;
		blkbase[blkno].offset = offs >> DEV_BSHIFT;
		blkbase[blkno].age = 0; /* XXX ticks! */
		blkbase[sc->sc_cache->lbusy].next = blkno;
		sc->sc_cache->lbusy = blkno;
	}
	return(blkno);
}

/*
 *	Put block from busy list on free list.
 */
static void
flputfree(sc, blkno)
	struct flsoftc *sc;
	u_int32_t blkno;
{
	struct flctag *cblk, *rblk;

	if(blkno) {
		cblk = &sc->sc_cache->flcblk[0];
		rblk = &sc->sc_cache->flcblk[blkno];

		while(cblk->next && cblk->next != blkno) {
			cblk = &sc->sc_cache->flcblk[cblk->next];
		}
		cblk->next = rblk->next;
		if(sc->sc_cache->lbusy == blkno) {
			sc->sc_cache->lbusy = cblk - &sc->sc_cache->flcblk[0];
		}

		rblk->next = sc->sc_cache->free;
		rblk->stat = BLK_ONFREE;
		sc->sc_cache->free = blkno;
		sc->sc_cache->nfree++;
	}
}

/*
 *	Push back a block (4k) to the flash. We first need to get
 *	any used pages not in cache before erasing.
 */

static int
flpushblk(sc, offs)
	struct flsoftc *sc;
	size_t offs;
{
	int i;
	u_int32_t blkno, offset;
	int error = 0;
	char *blk;

printf("fl: pushing block %d to flash.\n", offs);
	offset = offs;
	for(i = 0; i < 8; i++) {
		if(flblkincache(sc, offset, BLK_FIND) == 0) {
			if((blkno = flgetfree(sc, offset)) != 0) {
				blk = &sc->sc_cache->cache[blkno*512];
				flgetblk(sc, blk, offset, 256);
				flgetblk(sc, blk + 256, offset + 256, 256);
			}
			else {
				error = EIO;
			}
		}
		offset += DEV_BSIZE;
	}
	if(error == 0 && fleraseblk(sc, offs))	
		error = EIO;

	if(error == 0) {
		offset = offs;
		for(i = 0; i < 8; i++) {
			blkno = flblkincache(sc, offset, BLK_FORCE);
			blk = &sc->sc_cache->cache[blkno*512];
			if(flputblk(sc, blk, offset, 256))
				error = EIO;
			if(flputblk(sc, blk + 256, offset + 256, 256))
				error = EIO;
			offset += DEV_BSIZE;
			flputfree(sc, blkno);
		}
	}
	return(error);
}


/*
 *	Read a block (512 bytes) either from cache or from flash.
 */
static int
flreadblk(sc, blk, offs)
	struct flsoftc *sc;
	char *blk;
	size_t offs;
{
	int error = 0;
	u_int32_t blkno;

	if((blkno = flblkincache(sc, offs, BLK_FIND)) != 0) {
		bcopy(&sc->sc_cache->cache[blkno*512], blk, 512);
	}
	else {
		error |= flgetblk(sc, blk, offs, 256);
		error |= flgetblk(sc, blk+256, offs+256, 256);
	}
	return(error);
}

/*
 *	Write a block (512 bytes) to cache. If no room in cache
 *	make room by pushing data back into the flash memory.
 */
static int
flwriteblk(sc, blk, offs)
	struct flsoftc *sc;
	char *blk;
	size_t offs;
{
	int error = 0;
	u_int32_t blkno, offset;

		/* In cache? */
	if((blkno = flblkincache(sc, offs, BLK_LAST)) != 0) {
		bcopy(blk, &sc->sc_cache->cache[blkno*512], 512);
	}
		/* Add to cache */
	else {
		if(sc->sc_cache->nfree < 8) {
			/* Push busy blocks to get a free one */
			/* XXX We just pick first busy not in same 4k
			 * XXX as the one we need space for, now */
			blkno = sc->sc_cache->flcblk[0].next;
			do {
				offset = sc->sc_cache->flcblk[blkno].offset;
				offset <<= DEV_BSHIFT;
				offset &= ~4095;
				blkno = sc->sc_cache->flcblk[blkno].next;
			} while(offset == (offs & ~4095));

			error = flpushblk(sc, offset);
		}
		blkno = flgetfree(sc, offs);
		if(blkno) {
			bcopy(blk, &sc->sc_cache->cache[blkno*512], 512);
		}
		else {
			error = EIO;
		}
	}
	return(error);
}


/*ARGSUSED*/
int
flopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct flsoftc *sc;

	if (FLUNIT(dev) >= fl_cd.cd_ndevs || fl_cd.cd_devs[FLUNIT(dev)] == NULL)
		return (ENODEV);
	sc = (struct flsoftc *) fl_cd.cd_devs[FLUNIT(dev)];
	flgetdisklabel(dev, sc);
	sc->sc_opncnt++;
	return(0);
}

/*ARGSUSED*/
int
flclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct flsoftc *sc;
	u_int32_t offset, blkno;

	sc = (struct flsoftc *) fl_cd.cd_devs[FLUNIT(dev)];
	sc->sc_opncnt--;
	if(sc->sc_opncnt == 0) {
		while((blkno = sc->sc_cache->flcblk[0].next) != 0) {
			offset = sc->sc_cache->flcblk[blkno].offset;
			offset <<= DEV_BSHIFT;
			offset &= ~4095;
			flpushblk(sc, offset);
		}
	}
	return(0);
}

/*ARGSUSED*/
int
flioctl(dev, cmd, data, flag, p)
	dev_t   dev;
	u_char *data;
	int     cmd, flag;
	struct proc *p;
{
	int unit = FLUNIT(dev);
	struct flsoftc *sc = (struct flsoftc *) fl_cd.cd_devs[unit];
	struct cpu_disklabel clp;
	struct disklabel lp, *lpp;
	int error = 0;

	switch (cmd) {

	/*
	 *  Very basic disklabel handling.
	 */
	case DIOCGDINFO:
		*(struct disklabel *)data = sc->sc_label;
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if((flag & FWRITE) == 0) {
			error = EBADF;
			break;
		}
		error = setdisklabel(&sc->sc_label,
				     (struct disklabel *)data, 0, &clp);
		if((error == 0) && (cmd == DIOCWDINFO)) {
				error = writedisklabel(FLLABELDEV(dev),
						       flstrategy,
						       &sc->sc_label, &clp);
		}
		break;

	case DIOCWLABEL:
		if((flag & FWRITE) == 0)
			error = EBADF;
		break;

	default:
		error = EINVAL;
		break;
	}
	return(error);
}

void
flstrategy(bp)
	struct buf *bp;
{
	int unit = FLUNIT(bp->b_dev);
	struct flsoftc *sc = (struct flsoftc *) fl_cd.cd_devs[unit];
	int error = 0;
	size_t offs, xfer, cnt;
	caddr_t buf;

	offs = bp->b_blkno << DEV_BSHIFT;	/* Start address */
	buf = bp->b_data;
	bp->b_resid = bp->b_bcount;
	if(offs < sc->sc_size) {
		xfer = bp->b_resid;
		if(offs + xfer > sc->sc_size) {
			xfer = sc->sc_size - offs;
		}
		if(bp->b_flags & B_READ) {
			bp->b_resid -= xfer;
			while(xfer > 0) {
				cnt = (xfer > DEV_BSIZE) ? DEV_BSIZE : xfer;
				flreadblk(sc, buf, offs, cnt);
				xfer -= cnt;
				offs += cnt;
				buf += cnt;
			}
		}
		else {
			while(xfer > 0) {
				cnt = (xfer > DEV_BSIZE) ? DEV_BSIZE : xfer;
				if(flwriteblk(sc, buf, offs, cnt))
					error = EIO;
				xfer -= cnt;
				buf += cnt;
				offs += cnt;
				bp->b_resid -= cnt;
			}
		}
	}
	else if(!(bp->b_flags & B_READ)) {	/* No space for write */
		error = EIO;
	}
	if(error) {
		bp->b_error = error;
		bp->b_flags |= B_ERROR;
	}

	biodone(bp);
}

/*ARGSUSED*/
int
flread(dev, uio, ioflag)
        dev_t dev;
        struct uio *uio;
	int ioflag;
{
	return (physio(flstrategy, NULL, dev, B_READ, minphys, uio));
}

/*ARGSUSED*/
int
flwrite(dev, uio, ioflag)
        dev_t dev;
        struct uio *uio;
        int ioflag;
{
	return (physio(flstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
fldump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	return(ENODEV);
}

int
flsize(dev)
	dev_t dev;
{
	return(0);
}

/*
 *	Create a disklabel from the flash ram info.
 */
static void
flgetdisklabel(dev, sc)
	dev_t dev;
	struct flsoftc *sc;
{
	struct disklabel *lp = &sc->sc_label;
	struct cpu_disklabel clp;
	char *errstring;

	bzero(lp, sizeof(struct disklabel));
	bzero(&clp, sizeof(struct cpu_disklabel));

	lp->d_secsize = 1 << DEV_BSHIFT;
	lp->d_ntracks = 1;
	lp->d_nsectors = sc->sc_size >> DEV_BSHIFT;
	lp->d_ncylinders = 1;
	lp->d_secpercyl = lp->d_nsectors;

	strncpy(lp->d_typename, "FLASH disk", 16);
	lp->d_type = DTYPE_SCSI;		/* XXX This is a fake! */
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = lp->d_nsectors;
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	/*
	 * Call the generic disklabel extraction routine
	 */
	errstring = readdisklabel(FLLABELDEV(dev), flstrategy, lp, &clp, 0);
	if (errstring) {
		printf("%s: %s\n", sc->sc_dev.dv_xname, errstring);
	}
}

