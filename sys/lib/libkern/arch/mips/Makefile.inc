#	$OpenBSD: Makefile.inc,v 1.4 1996/05/15 08:24:15 pefo Exp $
#	$NetBSD: Makefile.inc,v 1.4 1996/04/13 01:17:46 cgd Exp $
#
#	There are likely more that we will notice when we go native

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	bcmp.S bcopy.S bzero.S strcat.c strcmp.c strcpy.c ffs.S htonl.S \
	htons.S strcmp.S strlen.S strncmp.c random.c scanc.c skpc.c strncpy.c \
	strncasecmp.c

