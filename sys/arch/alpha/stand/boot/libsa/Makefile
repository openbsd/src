#	$NetBSD: Makefile,v 1.2 1995/02/16 02:33:03 cgd Exp $

LIB=	sa
SRCS=	alloc.c bzero.c close.c dev.c devopen.c disklabel.c getfile.c \
	gets.c ioctl.c lseek.c open.c printf.c read.c strcmp.c ufs.c \
	write.c bcopy.c filesystem.c strlen.c ntohl.c

.PATH:	../../../../../lib/libsa ../../../../../lib/libkern
NOPIC=nopic
NOPROFILE=noprofile

.include <bsd.lib.mk>

CFLAGS=	-mno-fp-regs -I../../../../.. -I. -DCOMPAT_UFS
