/*	$NetBSD: ramd.c,v 1.6 1996/01/07 22:02:06 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/dkbad.h>

/*
 * Misc. defines:
 */
#define	RAMD_CHUNK	(9 * 512)	/* Chunk-size for auto-load	*/
#define	RAMD_NDEV	2		/* Number of devices configured	*/

struct   ramd_info {
	u_long	ramd_size;  /* Size of disk in bytes			*/
	u_long	ramd_flag;  /* see defs below				*/
	dev_t	ramd_dev;   /* device to load from			*/
	u_long	ramd_state; /* runtime state  see defs below		*/
	caddr_t	ramd_addr;  /* Kernel virtual addr			*/
};

/*
 * ramd_flag:
 */
#define	RAMD_LOAD	0x01	/* Auto load when first opened	*/
#define	RAMD_LCOMP	0x02	/* Input is compressed		*/

/*
 * ramd_state:
 */
#define	RAMD_ALLOC	0x01	/* Memory is allocated		*/
#define	RAMD_OPEN	0x02	/* Ramdisk is open		*/
#define	RAMD_INOPEN	0x04	/* Ramdisk is being opened	*/
#define	RAMD_WANTED	0x08	/* Someone is waiting on struct	*/
#define	RAMD_LOADED	0x10	/* Ramdisk is properly loaded	*/

struct ramd_info rd_info[RAMD_NDEV] = {
    {
	1105920,		/*	1Mb in 2160 sectors		*/
	RAMD_LOAD,		/* auto-load this device		*/
	MAKEDISKDEV(2, 0, 1),	/* XXX: This is crap! (720Kb flop)	*/
	0,			/* Will be set at runtime		*/
	NULL 			/* Will be set at runtime		*/
    },
    {
	1474560,		/*	1.44Mb in 2880 sectors		*/
	RAMD_LOAD,		/* auto-load this device		*/
	MAKEDISKDEV(2, 0, 1),	/* XXX: This is crap! (720Kb flop)	*/
	0,			/* Will be set at runtime		*/
	NULL 			/* Will be set at runtime		*/
    }
};

struct read_info {
    struct buf	*bp;		/* buffer for strategy function		*/
    long	nbytes;		/* total number of bytes to read	*/
    long	offset;		/* offset in input medium		*/
    caddr_t	bufp;		/* current output buffer		*/
    caddr_t	ebufp;		/* absolute maximum for bufp		*/
    int		chunk;		/* chunk size on input medium		*/
    int		media_sz;	/* size of input medium			*/
    void	(*strat)();	/* strategy function for read		*/
};

static	struct disk ramd_disks[RAMD_NDEV];	/* XXX Ick. */

/*
 * Autoconfig stuff....
 */
static int	ramdmatch __P((struct device *, struct cfdata *, void *));
static int	ramdprint __P((void *, char *));
static void	ramdattach __P((struct device *, struct device *, void *));

struct cfdriver ramdcd = {
	NULL, "rd", (cfmatch_t)ramdmatch, ramdattach, DV_DULL,
	sizeof(struct device), NULL, 0 };

void	rdstrategy __P((struct buf *));

struct	dkdriver ramddkdriver = { rdstrategy };

static int
ramdmatch(pdp, cfp, auxp)
struct device	*pdp;
struct cfdata	*cfp;
void		*auxp;
{
	if(strcmp("rd", auxp) || (cfp->cf_unit >= RAMD_NDEV))
		return(0);
	return(1);
}

static void
ramdattach(pdp, dp, auxp)
struct device	*pdp, *dp;
void		*auxp;
{
	int	i;
	struct	disk *diskp;

	/*
	 * XXX It's not really clear to me _exactly_ what's going
	 * on here, so this might need to be adjusted.  --thorpej
	 */

	for(i = 0; i < RAMD_NDEV; i++) {
		/*
		 * Initialize and attach the disk structure.
		 */
		diskp = &ramd_disks[i];
		bzero(diskp, sizeof(struct disk));
		if ((diskp->dk_name = malloc(8, M_DEVBUF, M_NOWAIT)) == NULL)
			panic("ramdattach: can't allocate space for name");
		bzero(diskp->dk_name, 8);
		sprintf(diskp->dk_name, "rd%d", i);
		diskp->dk_driver = &ramddkdriver;
		disk_attach(diskp);

		config_found(dp, (void*)i, ramdprint);
	}
}

static int
ramdprint(auxp, pnp)
void	*auxp;
char	*pnp;
{
	return(UNCONF);
}

static int  loaddisk __P((struct  ramd_info *, struct proc *));
static int  ramd_norm_read __P((struct read_info *));
static int  cpy_uncompressed __P((caddr_t, int, struct read_info *));
static int  rd_compressed __P((caddr_t, int, struct read_info *));

int
rdopen(dev, flags, devtype, p)
dev_t		dev;
int		flags, devtype;
struct proc	*p;
{
	struct ramd_info *ri;
	int		 s;
	int		 error = 0;

	if(DISKUNIT(dev) >= RAMD_NDEV)
		return(ENXIO);

	ri = &rd_info[DISKUNIT(dev)];
	if(ri->ramd_state & RAMD_OPEN)
		return(0);

	/*
	 * If someone is busy opening, wait for it to complete.
	 */
	s = splbio();
	while(ri->ramd_state & RAMD_INOPEN) {
		ri->ramd_state |= RAMD_WANTED;
		tsleep((caddr_t)ri, PRIBIO, "rdopen", 0);
	}
	ri->ramd_state |= RAMD_INOPEN;
	splx(s);

	if(!(ri->ramd_state & RAMD_ALLOC)) {
		ri->ramd_addr = malloc(ri->ramd_size, M_DEVBUF, M_WAITOK);
		if(ri->ramd_addr == NULL) {
			error = ENXIO;
			goto done;
		}
		ri->ramd_state |= RAMD_ALLOC;
	}
	if((ri->ramd_flag & RAMD_LOAD) && !(ri->ramd_state & RAMD_LOADED)) {
		error = loaddisk(ri, p);
		if(!error)
			ri->ramd_state |= RAMD_LOADED;
	}
done:
	ri->ramd_state &= ~RAMD_INOPEN;
	if(ri->ramd_state & RAMD_WANTED) {
		ri->ramd_state &= ~RAMD_WANTED;
		wakeup((caddr_t)ri);
	}
	return(error);
}

int
rdclose(dev, flags, devtype, p)
dev_t		dev;
int		flags, devtype;
struct proc	*p;
{
	return(0);
}

int
rdioctl(dev, cmd, addr, flag, p)
dev_t		dev;
u_long		cmd;
int		flag;
caddr_t		addr;
struct proc	*p;
{
	return(ENOTTY);
}

/*
 * no dumps to ram disks thank you.
 */
int
rdsize(dev)
dev_t	dev;
{
   return(-1);
}

void
rdstrategy(bp)
struct buf *bp;
{
	struct ramd_info	*ri;
	long			maxsz, sz;
	char			*datap;

	ri = &rd_info[DISKUNIT(bp->b_dev)];

	maxsz = ri->ramd_size / DEV_BSIZE;
	sz = (bp->b_bcount + DEV_BSIZE - 1) / DEV_BSIZE;
	if (bp->b_blkno < 0 || bp->b_blkno + sz > maxsz) {
		if((bp->b_blkno == maxsz) && (bp->b_flags & B_READ)) {
			/* Hitting EOF */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		sz = maxsz - bp->b_blkno;
		if((sz <= 0) || (bp->b_blkno < 0)) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			goto done;
		}
		bp->b_bcount = sz * DEV_BSIZE;
	}
	datap = (char*)((u_long)ri->ramd_addr + bp->b_blkno * DEV_BSIZE);
	if(bp->b_flags & B_READ)
		bcopy(datap, bp->b_data, bp->b_bcount);
	else bcopy(bp->b_data, datap, bp->b_bcount);
	bp->b_resid = 0;
	biodone(bp);
	return;

done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

int
rdread(dev, uio)
dev_t		dev;
struct uio	*uio;
{
   return (physio(rdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
rdwrite(dev, uio)
dev_t		dev;
struct uio	*uio;
{
   return (physio(rdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

static int
loaddisk(ri, proc)
struct ramd_info	*ri;
struct proc		*proc;
{
	struct buf		buf;
	int			error;
	struct bdevsw		*bdp = &bdevsw[major(ri->ramd_dev)];
	struct disklabel	dl;
	struct read_info	rs;

	/*
	 * Initialize out buffer header:
	 */
	buf.b_actf  = NULL;
	buf.b_rcred = buf.b_wcred = proc->p_ucred;
	buf.b_vnbufs.le_next = NOLIST;
	buf.b_flags = B_BUSY;
	buf.b_dev   = ri->ramd_dev;
	buf.b_error = 0;
	buf.b_proc  = proc;

	/*
	 * Setup read_info:
	 */
	rs.bp       = &buf;
	rs.nbytes   = ri->ramd_size;
	rs.offset   = 0;
	rs.bufp     = ri->ramd_addr;
	rs.ebufp    = ri->ramd_addr + ri->ramd_size;
	rs.chunk    = RAMD_CHUNK;
	rs.media_sz = ri->ramd_size;
	rs.strat    = bdp->d_strategy;

	/*
	 * Open device and try to get some statistics.
	 */
	if(error = bdp->d_open(ri->ramd_dev,FREAD | FNONBLOCK, 0, proc))
		return(error);
	if(bdp->d_ioctl(ri->ramd_dev,DIOCGDINFO,(caddr_t)&dl,FREAD,proc) == 0) {
		/* Read on a cylinder basis */
		rs.chunk    = dl.d_secsize * dl.d_secpercyl;
		rs.media_sz = dl.d_secperunit * dl.d_secsize;
	}

#ifdef notyet
	if(ri->ramd_flag & RAMD_LCOMP)
		error = decompress(cpy_uncompressed, rd_compressed, &rs);
	else
#endif /* notyet */
		error = ramd_norm_read(&rs);

	bdp->d_close(ri->ramd_dev,FREAD | FNONBLOCK, 0, proc);
	return(error);
}

static int
ramd_norm_read(rsp)
struct read_info	*rsp;
{
	long		bytes_left;
	int		done, error;
	struct buf	*bp;
	int		s;
	int		dotc = 0;

	bytes_left = rsp->nbytes;
	bp         = rsp->bp;
	error      = 0;

	while(bytes_left > 0) {
		s = splbio();
		bp->b_flags = B_BUSY | B_PHYS | B_READ;
		splx(s);
		bp->b_blkno  = btodb(rsp->offset);
		bp->b_bcount = rsp->chunk;
		bp->b_data   = rsp->bufp;

		/* Initiate read */
		(*rsp->strat)(bp);

		/* Wait for results	*/
		s = splbio();
		while ((bp->b_flags & B_DONE) == 0)
			tsleep((caddr_t) bp, PRIBIO + 1, "ramd_norm_read", 0);
		if (bp->b_flags & B_ERROR)
			error = (bp->b_error ? bp->b_error : EIO);
		splx(s);

		/* Dot counter */
		printf(".");
		if(!(++dotc % 40))
			printf("\n");

		done = bp->b_bcount - bp->b_resid;
		bytes_left   -= done;
		rsp->offset  += done;
		rsp->bufp    += done;

		if(error || !done)
			break;

		if((rsp->offset == rsp->media_sz) && (bytes_left != 0)) {
			printf("\nInsert next media and hit any key...");
			cngetc();
			printf("\n");
			rsp->offset = 0;
		}
	}
	printf("\n");
	s = splbio();
	splx(s);
	return(error);
}

/*
 * Functions supporting uncompression:
 */
/*
 * Copy from the uncompression buffer to the ramdisk
 */
static int
cpy_uncompressed(buf, nbyte, rsp)
caddr_t			buf;
struct read_info	*rsp;
int			nbyte;
{
	if((rsp->bufp + nbyte) >= rsp->ebufp)
		return(0);
	bcopy(buf, rsp->bufp, nbyte);
	rsp->bufp += nbyte;
	return(0);
}

/*
 * Read a maximum of 'nbyte' bytes into 'buf'.
 */
static int
rd_compressed(buf, nbyte, rsp)
caddr_t			buf;
struct read_info	*rsp;
int			nbyte;
{
	static int	dotc = 0;
	struct buf	*bp;
	       int	nread = 0;
	       int	s;
	       int	done, error;


	error  = 0;
	bp     = rsp->bp;
	nbyte &= ~(DEV_BSIZE - 1);

	while(nbyte > 0) {
		s = splbio();
		bp->b_flags = B_BUSY | B_PHYS | B_READ;
		splx(s);
		bp->b_blkno  = btodb(rsp->offset);
		bp->b_bcount = min(rsp->chunk, nbyte);
		bp->b_data   = buf;

		/* Initiate read */
		(*rsp->strat)(bp);

		/* Wait for results	*/
		s = splbio();
		while ((bp->b_flags & B_DONE) == 0)
			tsleep((caddr_t) bp, PRIBIO + 1, "ramd_norm_read", 0);
		if (bp->b_flags & B_ERROR)
			error = (bp->b_error ? bp->b_error : EIO);
		splx(s);

		/* Dot counter */
		printf(".");
		if(!(++dotc % 40))
			printf("\n");

		done = bp->b_bcount - bp->b_resid;
		nbyte        -= done;
		nread        += done;
		rsp->offset  += done;

		if(error || !done)
			break;

		if(rsp->offset == rsp->media_sz) {
			printf("\nInsert next media and hit any key...");
			if(cngetc() != '\n')
				printf("\n");
			rsp->offset = 0;
		}
	}
	s = splbio();
	splx(s);
	return(nread);
}
