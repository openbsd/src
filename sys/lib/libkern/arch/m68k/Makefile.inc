#	$OpenBSD: Makefile.inc,v 1.4 1997/11/04 19:08:08 chuck Exp $
#	$NetBSD: Makefile.inc,v 1.7 1996/04/18 01:53:04 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.c \
	bcmp.S bzero.S ffs.S strcat.S strcmp.S strcpy.S strlen.S strncmp.S \
	strncpy.S htonl.S htons.S ntohl.S ntohs.S scanc.S skpc.S locc.S \
	strncasecmp.c
