/*     $OpenBSD: ipt.h,v 1.6 1999/02/05 05:58:47 deraadt Exp $     */
/*
 * Copyright (C) 1993-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 * $Id: ipt.h,v 1.6 1999/02/05 05:58:47 deraadt Exp $
 */

#ifndef	__IPT_H__
#define	__IPT_H__

#include <fcntl.h>
#ifdef	__STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif


struct	ipread	{
	int	(*r_open) __P((char *));
	int	(*r_close) __P((void));
	int	(*r_readip) __P((char *, int, char **, int *));
};

extern	void	debug __P((char *, ...));
extern	void	verbose __P((char *, ...));

#endif /* __IPT_H__ */
