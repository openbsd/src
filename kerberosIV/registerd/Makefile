# Copyright (c) 1990 The Regents of the University of California.
# All rights reserved.
#
#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:36 tholo Exp $

PROG=	registerd
SRCS=	registerd.c des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS -I${.CURDIR}/../register
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN=	registerd.8
BINDIR=	/usr/libexec

.include <bsd.prog.mk>
