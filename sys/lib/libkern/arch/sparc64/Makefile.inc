#	$OpenBSD: Makefile.inc,v 1.4 2002/09/06 22:59:58 art Exp $
#	$NetBSD: Makefile.inc,v 1.4 2000/11/01 19:37:20 thorpej Exp $

SRCS+=  __main.c \
	imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	bcmp.c ffs.S \
	memchr.c memcmp.c \
	strcat.c strcmp.c strcpy.c strlen.S strlcpy.c strlcat.c \
	strncasecmp.c strncmp.c strncpy.c \
	scanc.c skpc.c \
	htonl.S htons.S ntohl.S ntohs.S
