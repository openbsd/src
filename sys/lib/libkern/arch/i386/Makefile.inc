#	$OpenBSD: Makefile.inc,v 1.4 1996/04/21 22:27:52 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.10 1996/04/13 01:17:41 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	bcmp.S ffs.S memset.S strcat.S strcmp.S strcpy.S strlen.S strncmp.c \
	strncpy.c scanc.S skpc.S locc.S htonl.S htons.S ntohl.S ntohs.S \
	strncasecmp.c
