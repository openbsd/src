#	$OpenBSD: Makefile.inc,v 1.3 1996/04/21 22:27:58 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.7 1996/04/13 01:17:50 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	bcmp.c ffs.c strcat.c strcmp.c strcpy.c strlen.c strncmp.c strncpy.c \
	scanc.c skpc.c locc.c strncasecmp.c \
	htonl.S htons.S ntohl.S ntohs.S
