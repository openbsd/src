#	from: @(#)Makefile	5.6 (Berkeley) 9/27/90
#	$Id: Makefile,v 1.2 1995/12/16 22:20:35 tholo Exp $

PROG=	rsh
SRCS=	rsh.c
BINOWN=	root
BINMODE=4555

.include <bsd.own.mk>	# For KERBEROS

.if defined(KERBEROS)
SRCS+=	des_rw.c
.PATH:	${.CURDIR}/../rlogin
CFLAGS+=-DKERBEROS
SRCS+=	krcmd.c kcmd.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
