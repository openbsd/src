#	$OpenBSD: Makefile,v 1.3 2001/06/19 16:38:21 millert Exp $

#	BSDI	$From: Makefile,v 1.2 1997/08/08 18:58:22 prb Exp $

PROG=	login_lchpass
SRCS=	login_lchpass.c local_passwd.c pwd_check.c pwd_gensalt.c
MAN=	login_lchpass.8
.PATH:	${.CURDIR}/../../usr.bin/passwd

CFLAGS+=-Wall -Wno-unused
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
