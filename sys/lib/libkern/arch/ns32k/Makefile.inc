#	$OpenBSD: Makefile.inc,v 1.5 2000/06/16 19:56:55 millert Exp $
#	$NetBSD: Makefile.inc,v 1.7 1996/04/13 01:17:50 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.S memcmp.c \
	bcmp.c ffs.c strcat.c strcmp.c strcpy.c strlen.c strncmp.c strncpy.c \
	scanc.c skpc.c locc.c strncasecmp.c \
	htonl.S htons.S ntohl.S ntohs.S
