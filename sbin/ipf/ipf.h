/*	$OpenBSD: ipf.h,v 1.4 1996/06/23 14:30:54 deraadt Exp $	*/

/*
 * (C)opyright 1993-1996 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ipf.h	1.11 4/10/96
 */

#define	OPT_REMOVE	0x0001
#define	OPT_DEBUG	0x0002
#define	OPT_OUTQUE	FR_OUTQUE	/* 0x0004 */
#define	OPT_INQUE	FR_INQUE	/* 0x0008 */
#define	OPT_LOG		FR_LOG		/* 0x0010 */
#define	OPT_SHOWLIST	0x0020
#define	OPT_VERBOSE	0x0040
#define	OPT_DONOTHING	0x0080
#define	OPT_HITS	0x100
#define	OPT_BRIEF	0x200
#define OPT_ACCNT	FR_ACCOUNT	/* 0x0800 */
#define	OPT_FRSTATES	FR_KEEPFRAG	/* 0x1000 */
#define	OPT_IPSTATES	FR_KEEPSTATE	/* 0x2000 */
#define	OPT_INACTIVE	FR_INACTIVE	/* 0x4000 */
#define	OPT_SHOWLINENO	0x8000

extern	struct	frentry	*parse();

extern	void	printfr(), binprint(), initparse();

#if defined(__SVR4) || defined(__svr4__)
#define	index	strchr
#define	bzero(a,b)	memset(a, 0, b)
#define	bcopy(a,b,c)	memmove(b,a,c)
#endif

struct	ipopt_names	{
	int	on_value;
	int	on_bit;
	int	on_siz;
	char	*on_name;
};


extern	u_long	hostnum(), optname();
extern	void	printpacket();

