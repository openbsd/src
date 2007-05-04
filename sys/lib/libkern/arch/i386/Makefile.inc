#	$OpenBSD: Makefile.inc,v 1.9 2007/05/04 22:24:34 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.10 1996/04/13 01:17:41 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.S memcmp.S \
	bcmp.S ffs.S memset.S strcmp.S strlcat.c strlcpy.c \
	strlen.S strncmp.c \
	strncpy.c scanc.S skpc.S htonl.S htons.S ntohl.S ntohs.S \
	strncasecmp.c
