#	$OpenBSD: Makefile.inc,v 1.5 1997/11/04 19:08:05 chuck Exp $
#	$NetBSD: Makefile.inc,v 1.10 1996/04/13 01:17:41 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.S \
	bcmp.S ffs.S memset.S strcat.S strcmp.S strcpy.S strlen.S strncmp.c \
	strncpy.c scanc.S skpc.S locc.S htonl.S htons.S ntohl.S ntohs.S \
	strncasecmp.c
