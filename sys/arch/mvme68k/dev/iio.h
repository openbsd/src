/*	$NetBSD: iio.h,v 1.1.1.1 1995/07/25 23:12:11 chuck Exp $	*/
/* $Id: iio.h,v 1.1.1.1 1995/10/18 08:51:10 deraadt Exp $ */

struct iioargs {
	int	ic_addr;
	int	ic_lev;
};

#define IIO_CFLOC_ADDR(cf)	(IIOV(INTIOBASE + (cf)->cf_loc[0]))
#define IIO_CFLOC_LEVEL(cf)	((cf)->cf_loc[1])

/*
 * for the console we need zs phys addr
 */

#define ZS0_PHYS        (INTIOBASE + 0x3000)
#define ZS1_PHYS        (INTIOBASE + 0x3800)

