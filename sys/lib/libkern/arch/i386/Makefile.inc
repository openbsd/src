#	$OpenBSD: Makefile.inc,v 1.3 1996/04/19 16:09:44 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.9 1996/03/11 05:32:00 scottr Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	bcmp.S ffs.S memset.S strcat.S strcmp.S strcpy.S strlen.S strncmp.c \
	strncpy.c scanc.S skpc.S locc.S htonl.S htons.S ntohl.S ntohs.S
