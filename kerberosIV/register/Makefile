#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.2 1996/02/15 03:56:36 etheisen Exp $

PROG=	register
SRCS=	register.c des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS
#DPADD+=	${LIBKRB} ${LIBCRYPT} ${LIBDES}
DPADD+=	${LIBKRB} ${LIBDES}
#LDADD+=	-lkrb -lcrypt -ldes
LDADD+=	-lkrb -ldes
BINDIR=	/usr/bin
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
