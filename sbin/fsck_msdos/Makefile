#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:36:40 deraadt Exp $

PROG=	fsck_msdos
MAN=	fsck_msdos.8
SRCS=	main.c check.c boot.c fat.c dir.c fsutil.c
CFLAGS+= -I${.CURDIR}/../fsck
.PATH: ${.CURDIR}/../fsck

.include <bsd.prog.mk>
