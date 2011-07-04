#	$NetBSD: Makefile.inc,v 1.4 2006/09/23 17:39:34 cherry Exp $

#SRCS+=	bswap16.c bswap32.c bswap64.c

SRCS+= flt_rounds.c fpgetround.c fpsetround.c fpgetmask.c fpsetmask.c
#SRCS+=	isinff_ieee754.c isinfd_ieee754.c
#SRCS+=	isnanf_ieee754.c isnand_ieee754.c
