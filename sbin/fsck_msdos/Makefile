#	$OpenBSD: Makefile,v 1.3 1997/03/02 05:25:50 millert Exp $
#	$NetBSD: Makefile,v 1.4 1997/01/03 14:32:47 ws Exp $

PROG=	fsck_msdos
MAN=	fsck_msdos.8
SRCS=	main.c check.c boot.c fat.c dir.c fsutil.c
CFLAGS+= -I${.CURDIR}/../fsck
.PATH: ${.CURDIR}/../fsck

.include <bsd.prog.mk>
