#	$OpenBSD: Makefile.inc,v 1.2 1996/02/29 13:28:27 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.6 1996/02/06 21:22:06 christos Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	bcmp.S bzero.S ffs.S strcat.c strcmp.S strcpy.S strlen.S strncmp.S \
	strncpy.S htonl.S htons.S ntohl.S ntohs.S scanc.S skpc.S locc.S \
	strncasecmp.c
