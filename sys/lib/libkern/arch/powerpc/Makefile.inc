#	$OpenBSD: Makefile.inc,v 1.4 1998/09/25 04:20:44 rahnds Exp $
#	$NetBSD: Makefile.inc,v 1.6 1994/10/26 06:39:03 cgd Exp $

CFLAGS	+= -D_KERNEL

SRCS+=	__main.c __eabi.c \
	imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.c \
	bcmp.c bzero.c ffs.c strcat.c strcmp.c strcpy.c strlen.c strncmp.c \
	strncpy.c scanc.c skpc.c locc.c htonl.c htons.c ntohl.c ntohs.c \
	memcpy.c random.c strncasecmp.c
#	bcopy.c htolel.c letohl.c htoles.c letohs.c copystr.c \

#	copyinstr.c copyoutstr.c copystr.c copyin.c copyout.c \
