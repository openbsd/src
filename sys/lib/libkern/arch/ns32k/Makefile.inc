#	$OpenBSD: Makefile.inc,v 1.4 1997/11/04 19:08:11 chuck Exp $
#	$NetBSD: Makefile.inc,v 1.7 1996/04/13 01:17:50 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.S \
	bcmp.c ffs.c strcat.c strcmp.c strcpy.c strlen.c strncmp.c strncpy.c \
	scanc.c skpc.c locc.c strncasecmp.c \
	htonl.S htons.S ntohl.S ntohs.S
