/*	$NetBSD: hp.c,v 1.1 1995/02/13 00:43:59 ragge Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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

 /* All bugs are subject to removal without further notice */
		


/* hp.c - drivrutiner f|r massbussdiskar 940325/ragge */

#include "param.h"
#include "types.h"
#include "fcntl.h"
#include "syslog.h"
#include "disklabel.h"
#include "buf.h"
#include "vax/mba/mbareg.h"
#include "vax/mba/mbavar.h"
#include "vax/mba/hpdefs.h"
#include "hp.h"

struct	mba_device	*hpinfo[NHP];
struct	hp_info		hp_info[NHP];
struct	disklabel	hplabel[NHP];
int hpslave(), hpattach();

char hptypes[]={
	0x22,0
};

struct mba_driver hpdriver={
	hpslave, 0, "hp", hptypes, hpattach, hpinfo
};

hpslave(){
	printf("Hpslave.\n");
	asm("halt");
};

hpopen(){
	printf("hpopen");
	        asm("halt");
};

hpclose(){
	printf("hpclose\n");
        asm("halt");
};

hpioctl(){
	printf("hpioctl\n");
	asm("halt");
}

hpdump(){
	printf("hpdump\n");
        asm("halt");
};

hpsize(){
	printf("hpsize");
        asm("halt");
};



hpattach(mi)
	struct mba_device *mi;
{
	struct mba_drv *md;

/*
 * We check status of the drive first; to see if there is any idea
 * to try to read the label.
 */
	md=&(mi->mi_mba->mba_drv[mi->drive]);
	if(!md->rmcs1&HPCS1_DVA){
		printf(": Drive not available");
		return;
	}
	if(!md->rmds&HPDS_MOL){
		printf(": Drive offline");
		return;
	}
	if (hpinit(mi, 0))
                printf(": offline");
/*        else if (ra_info[unit].ra_state == OPEN) {
                printf(": %s, size = %d sectors",
                    udalabel[unit].d_typename, ra_info[unit].ra_dsize);
*/	
	printf("rmcs1: %x, rmds: %x, rmdt: %x rmsn: %x\n",
		md->rmcs1, md->rmds, md->rmdt, md->rmsn);


/*        asm("halt"); */
/*
        if (MSCP_MID_ECH(1, ra_info[unit].ra_mediaid) == 'X' - '@') {
                printf(": floppy");
                return;
        }
        if (ui->ui_dk >= 0)
                dk_wpms[ui->ui_dk] = (60 * 31 * 256); 
        udaip[ui->ui_ctlr][ui->ui_slave] = ui;

        if (uda_rainit(ui, 0))
                printf(": offline");
        else if (ra_info[unit].ra_state == OPEN) {
                printf(": %s, size = %d sectors",
                    udalabel[unit].d_typename, ra_info[unit].ra_dsize);
        }*/
}


/*
 * Initialise a drive.  If it is not already, bring it on line,
 * and set a timeout on it in case it fails to respond.
 * When on line, read in the pack label.
 */
hpinit(mi, flags)
        struct mba_device *mi;
{
/*        struct uda_softc *sc = &uda_softc[ui->ui_ctlr]; */
	struct disklabel *lp;
	struct hp_info *hp;
/*         struct mscp *mp; */
        int unit = mi->unit;
	char *msg, *readdisklabel();
	int s, i, hpstrategy();
	extern int cold;

        hp = &hp_info[unit];
/*
        if ((ui->ui_flags & UNIT_ONLINE) == 0) {
                mp = mscp_getcp(&sc->sc_mi, MSCP_WAIT);
                mp->mscp_opcode = M_OP_ONLINE;
                mp->mscp_unit = ui->ui_slave;
                mp->mscp_cmdref = (long)&ui->ui_flags;
                *mp->mscp_addr |= MSCP_OWN | MSCP_INT;
                ra->ra_state = WANTOPEN;
                if (!cold)
                        s = spl5();
                i = ((struct udadevice *)ui->ui_addr)->udaip;

                if (cold) {
                        i = todr() + 1000;
                        while ((ui->ui_flags & UNIT_ONLINE) == 0)
                                if (todr() > i)
                                        break;
                } else {
                        timeout(wakeup, (caddr_t)&ui->ui_flags, 10 * hz);
                        sleep((caddr_t)&ui->ui_flags, PSWP + 1);
                        splx(s);
                        untimeout(wakeup, (caddr_t)&ui->ui_flags);
                }
                if (ra->ra_state != OPENRAW) {
                        ra->ra_state = CLOSED;
                        wakeup((caddr_t)ra);
                        return (EIO);
                }
        }
*/
        lp = &hplabel[unit];
        lp->d_secsize = DEV_BSIZE;

        lp->d_secsize = DEV_BSIZE;
        lp->d_secperunit = 15 /*ra->ra_dsize*/;

        if (flags & O_NDELAY)
                return (0);
        hp->hp_state = RDLABEL;
        /*
         * Set up default sizes until we have the label, or longer
         * if there is none.  Set secpercyl, as readdisklabel wants
         * to compute b_cylin (although we do not need it), and set
         * nsectors in case diskerr is called.
         */
        lp->d_secpercyl = 1;
        lp->d_npartitions = 1;
        lp->d_secsize = 512;
/*        lp->d_secperunit = ra->ra_dsize; */
        lp->d_nsectors = 15 /*ra->ra_geom.rg_nsectors*/;
        lp->d_partitions[0].p_size = lp->d_secperunit;
        lp->d_partitions[0].p_offset = 0;

        /*
         * Read pack label.
         */
        if ((msg = readdisklabel(hpminor(unit, 0), hpstrategy, lp)) != NULL) {
                if (cold)
                        printf(": %s", msg);
                else
                        log(LOG_ERR, "hp%d: %s", unit, msg);
/*                ra->ra_state = OPENRAW; */
/*                uda_makefakelabel(ra, lp); */
        } else
/*                ra->ra_state = OPEN; */
/*        wakeup((caddr_t)hp); */
        return (0);
}

/*
 * Queue a transfer request, and if possible, hand it to the controller.
 *
 * This routine is broken into two so that the internal version
 * udastrat1() can be called by the (nonexistent, as yet) bad block
 * revectoring routine.
 */
hpstrategy(bp)
        register struct buf *bp;
{
	register int unit;
	register struct uba_device *ui;
	register struct hp_info *hp;
	struct partition *pp;
	int p;
	daddr_t sz, maxsz;

	/*
	 * Make sure this is a reasonable drive to use.
	 */
/*	bp->b_error = ENXIO;
	goto bad;
*/
	unit = hpunit(bp->b_dev);

        /*
         * If drive is open `raw' or reading label, let it at it.
         */

	if (hp->hp_state < OPEN) {
		hpstrat1(bp);
		return;
	}


/*	if ((unit = udaunit(bp->b_dev)) >= NRA ||
	    (ui = udadinfo[unit]) == NULL || ui->ui_alive == 0 ||
            (ra = &ra_info[unit])->ra_state == CLOSED) {
                bp->b_error = ENXIO;
                goto bad;
        }
*/
        /*
         * If drive is open `raw' or reading label, let it at it.
         */
/*
        if (ra->ra_state < OPEN) {
                udastrat1(bp);
                return;
        }
        p = udapart(bp->b_dev);
        if ((ra->ra_openpart & (1 << p)) == 0) {
                bp->b_error = ENODEV;
                goto bad;
        }
*/
        /*
         * Determine the size of the transfer, and make sure it is
         * within the boundaries of the partition.
         */
/*
        pp = &udalabel[unit].d_partitions[p];
        maxsz = pp->p_size;
        if (pp->p_offset + pp->p_size > ra->ra_dsize)
                maxsz = ra->ra_dsize - pp->p_offset;
        sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;
        if (bp->b_blkno + pp->p_offset <= LABELSECTOR &&
#if LABELSECTOR != 0
            bp->b_blkno + pp->p_offset + sz > LABELSECTOR &&
#endif
            (bp->b_flags & B_READ) == 0 && ra->ra_wlabel == 0) {
                bp->b_error = EROFS;
                goto bad;
        }
        if (bp->b_blkno < 0 || bp->b_blkno + sz > maxsz) {
                /* if exactly at end of disk, return an EOF */
/*
                if (bp->b_blkno == maxsz) {
                        bp->b_resid = bp->b_bcount;
                        biodone(bp);
                        return;
                }
                /* or truncate if part of it fits */
/*
                sz = maxsz - bp->b_blkno;
                if (sz <= 0) {
                        bp->b_error = EINVAL;   /* or hang it up */
/*
                        goto bad;
                }
                bp->b_bcount = sz << DEV_BSHIFT;
        }
        udastrat1(bp);
        return;
*/
bad:
        bp->b_flags |= B_ERROR;
        biodone(bp);
}

/*
 * Work routine for udastrategy.
 */
hpstrat1(bp)
        register struct buf *bp;
{
        register int unit = hpunit(bp->b_dev);
        register struct hp_ctlr *um;
        register struct buf *dp;
        struct hp_device *ui;
/*        int s = spl5(); */

	asm("halt");
        /*
         * Append the buffer to the drive queue, and if it is not
         * already there, the drive to the controller queue.  (However,
         * if the drive queue is marked to be requeued, we must be
         * awaiting an on line or get unit status command; in this
         * case, leave it off the controller queue.)
         */
/*
        um = (ui = udadinfo[unit])->ui_mi;
        dp = &udautab[unit];
        APPEND(bp, dp, av_forw);
        if (dp->b_active == 0 && (ui->ui_flags & UNIT_REQUEUE) == 0) {
                APPEND(dp, &um->um_tab, b_forw);
                dp->b_active++;
        }

        /*
         * Start activity on the controller.  Note that unlike other
         * Unibus drivers, we must always do this, not just when the
         * controller is not active.
         */
/*
        udastart(um);
        splx(s);
*/
}
