#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:33 tholo Exp $

PROG=	register
SRCS=	register.c des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBCRYPT} ${LIBDES}
LDADD+=	-lkrb -lcrypt -ldes
BINDIR=	/usr/bin
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
