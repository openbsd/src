/*	$OpenBSD: savelev.c,v 1.2 1998/09/15 05:12:33 pjanzen Exp $	*/
/*	$NetBSD: savelev.c,v 1.4 1997/10/18 20:03:45 christos Exp $	*/

/* savelev.c		 Larn is copyrighted 1986 by Noah Morgan. */
#ifndef lint
static char rcsid[] = "$OpenBSD: savelev.c,v 1.2 1998/09/15 05:12:33 pjanzen Exp $";
#endif				/* not lint */
#include "header.h"
#include "extern.h"

/*
 *	routine to save the present level into storage
 */
void
savelevel()
{
	struct cel *pcel;
	char  *pitem, *pknow, *pmitem;
	short *phitp, *piarg;
	struct cel *pecel;
	pcel = &cell[level * MAXX * MAXY];	/* pointer to this level's
						 * cells */
	pecel = pcel + MAXX * MAXY;	/* pointer to past end of this
					 * level's cells */
	pitem = item[0];
	piarg = iarg[0];
	pknow = know[0];
	pmitem = mitem[0];
	phitp = hitp[0];
	while (pcel < pecel) {
		pcel->mitem = *pmitem++;
		pcel->hitp = *phitp++;
		pcel->item = *pitem++;
		pcel->know = *pknow++;
		pcel++->iarg = *piarg++;
	}
}

/*
 *	routine to restore a level from storage
 */
void
getlevel()
{
	struct cel *pcel;
	char  *pitem, *pknow, *pmitem;
	short *phitp, *piarg;
	struct cel *pecel;
	pcel = &cell[level * MAXX * MAXY];	/* pointer to this level's
						 * cells */
	pecel = pcel + MAXX * MAXY;	/* pointer to past end of this
					 * level's cells */
	pitem = item[0];
	piarg = iarg[0];
	pknow = know[0];
	pmitem = mitem[0];
	phitp = hitp[0];
	while (pcel < pecel) {
		*pmitem++ = pcel->mitem;
		*phitp++ = pcel->hitp;
		*pitem++ = pcel->item;
		*pknow++ = pcel->know;
		*piarg++ = pcel++->iarg;
	}
}
