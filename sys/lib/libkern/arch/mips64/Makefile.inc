#	$OpenBSD: Makefile.inc,v 1.1 2004/08/11 10:29:59 pefo Exp $
#	$NetBSD: Makefile.inc,v 1.4 1996/04/13 01:17:46 cgd Exp $
#
#	There are likely more that we will notice when we go native

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.c memcmp.c memset.c strcmp.c \
	bcmp.S bcopy.S bzero.S strcmp.c strlcat.c strlcpy.c \
	ffs.S htonl.S \
	htons.S strcmp.S strlen.S strncmp.c random.c scanc.c skpc.c strncpy.c \
	strncasecmp.c

