/*	$OpenBSD: adutil.c,v 1.9 1997/11/06 17:23:09 csapuntz Exp $	*/
/*	$NetBSD: adutil.c,v 1.15 1996/10/13 02:52:07 christos Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1996 Matthias Scheler
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
 *      This product includes software developed by Christian E. Hopps.
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
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/buf.h>
#include <adosfs/adosfs.h>

/*
 * look for anode in the mount's hash table, return locked.
 */
#define AHASH(an) ((an) & (ANODEHASHSZ - 1))

static __inline char	CapitalChar __P((char, int));

struct vnode * 
adosfs_ahashget(mp, an)
	struct mount *mp;
	ino_t an;
{
	struct anodechain *hp;
	struct anode *ap;
	struct proc  *p = curproc;

	hp = &VFSTOADOSFS(mp)->anodetab[AHASH(an)];

start_over:
	for (ap = hp->lh_first; ap != NULL; ap = ap->link.le_next) {
		if (ABLKTOINO(ap->block) != an)
			continue;
		if (ap->flags & ALOCKED) {
			ap->flags |= AWANT;
			tsleep(ap, PINOD, "ahashget", 0);
			goto start_over;
		}
		if (vget(ATOV(ap), LK_EXCLUSIVE, p))
			goto start_over;
		return (ATOV(ap));
	}
	return (NULL);
}

/*
 * insert in hash table and lock
 */
void
adosfs_ainshash(amp, ap)
	struct adosfsmount *amp;
	struct anode *ap;
{
	LIST_INSERT_HEAD(&amp->anodetab[AHASH(ABLKTOINO(ap->block))], ap,
	    link);
	ap->flags |= ALOCKED;
}

void
adosfs_aremhash(ap)
	struct anode *ap;
{
	LIST_REMOVE(ap, link);
}

int
adosfs_getblktype(amp, bp)
	struct adosfsmount *amp;
	struct buf *bp;
{
	if (adoscksum(bp, amp->nwords)) {
#ifdef DIAGNOSTIC
		printf("adosfs: aget: cksum of blk %ld failed\n",
		    bp->b_blkno / amp->secsperblk);
#endif
		return (-1);
	}

	/*
	 * check primary block type
	 */
	if (adoswordn(bp, 0) != BPT_SHORT) {
#ifdef DIAGNOSTIC
		printf("adosfs: aget: bad primary type blk %ld\n",
		    bp->b_blkno / amp->secsperblk);
#endif
		return (-1);
	}

	switch (adoswordn(bp, amp->nwords - 1)) {
	case BST_RDIR:		/* root block */
		return (AROOT);
	case BST_LDIR:		/* hard link to dir */
		return (ALDIR);
	case BST_UDIR:		/* user dir */
		return (ADIR);
	case BST_LFILE:		/* hard link to file */
		return (ALFILE);
	case BST_FILE:		/* file header */
		return (AFILE);
	case BST_SLINK:		/* soft link */
		return (ASLINK);
	}
	return (-1);
}

int
adunixprot(adprot)
	int adprot;
{
	if (adprot & 0xc000ee00) {
		adprot = (adprot & 0xee0e) >> 1;
		return (((adprot & 0x7) << 6) |
			((adprot & 0x700) >> 5) |
			((adprot & 0x7000) >> 12));
	}
	else {
		adprot = (adprot >> 1) & 0x7;
		return((adprot << 6) | (adprot << 3) | adprot);
	}
}

static __inline char
CapitalChar(ch, inter)
	char ch;
	int inter;
{
	if ((ch >= 'a' && ch <= 'z') || 
	    (inter && ch >= '\xe0' && ch <= '\xfe' && ch != '\xf7'))
		return(ch - ('a' - 'A'));
	return(ch);
}

u_int32_t
adoscksum(bp, n)
	struct buf *bp;
	int n;
{
	u_int32_t sum, *lp;
	
	lp = (u_int32_t *)bp->b_data;
	sum = 0;

	while (n--)
		sum += ntohl(*lp++);
	return(sum);
}

int
adoscaseequ(name1, name2, len, inter)
	const char *name1, *name2;
	int len, inter;
{
	while (len-- > 0) 
		if (CapitalChar(*name1++, inter) != 
		    CapitalChar(*name2++, inter))
			return 0;
	
	return 1;
}

int
adoshash(nam, namlen, nelt, inter)
	const char *nam;
	int namlen, nelt, inter;
{
	int val;

	val = namlen;
	while (namlen--)
		val = ((val * 13) + (u_char)CapitalChar(*nam++, inter)) &
		    0x7ff;
	return(val % nelt);
}

#ifdef notyet
/*
 * datestamp is local time, tv is to be UTC
 */
int
dstotv(dsp, tvp)
	struct datestamp *dsp;
	struct timeval *tvp;
{
}

/*
 * tv is UTC, datestamp is to be local time
 */
int
tvtods(tvp, dsp)
	struct timeval *tvp;
	struct datestamp *dsp;
{
}
#endif
