#	$OpenBSD: Makefile.inc,v 1.3 1996/04/21 22:27:55 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.4 1996/04/13 01:17:46 cgd Exp $
#
#	There are likely more that we will notice when we go native

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	bcmp.S bzero.S strcat.c strcmp.c strcpy.c ffs.S strlen.S strncmp.c \
	random.c scanc.c skpc.c strcat.c strcpy.c strncmp.c strncpy.c \
	strncasecmp.c

