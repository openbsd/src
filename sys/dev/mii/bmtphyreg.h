/*	$OpenBSD: bmtphyreg.h,v 1.1 2001/04/11 06:47:31 deraadt Exp $	*/
/*	$NetBSD: bmtphyreg.h,v 1.1 1998/08/10 23:58:39 thorpej Exp $	*/

#ifndef _DEV_MII_BMTPHYREG_H_
#define	_DEV_MII_BMTPHYREG_H_

#define	MII_BMTPHY_100RCVERR	0x12	/* 100base-X rcv error counter */

#define	MII_BMTPHY_FCSCR	0x13	/* 100base-X false carrier counter */

#define	MII_BMTPHY_100DISC	0x14	/* 100base-X rcv error counter */

#define	MII_BMTPHY_TXEQUAL	0x15	/* TX equalizer coefficient */

#define	MII_BMTPHY_TXEQCTRL	0x16	/* TX equalizer control */

#define	MII_BMTPHY_PTEST	0x17	/* PTEST */

#define	MII_BMTPHY_AUXC		0x18	/* Aux control/status */
#define	AUXC_FORCELINK		0x4000	/* */
#define	AUXC_AUTONEG		0x0010	/* */
#define	AUXC_FORCE100		0x0004	/* Force 100 indicator */
#define	AUXC_SP100		0x0002	/* SP100 indicator */
#define AUXC_FDX		0x0001	/* Full duplex indicator */

#define	MII_BMTPHY_AUXS		0x19	/* Aux status summary */
#define	AUXS_SP100		0x0008	/* */
#define	AUXS_LINKSTATUS		0x0004	/* */

#define MII_BMTPHY_AUX2		0x1b	/* aux mode 2 */
#define AUX2_TWOLNKLED		0x0004	/* Two link led mode */

#endif /* _DEV_MII_BMTPHYREG_H_ */
