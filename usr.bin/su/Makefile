#	from: @(#)Makefile	5.5 (Berkeley) 5/11/90
#	$Id: Makefile,v 1.3 1996/02/15 05:02:28 etheisen Exp $

PROG=	su
BINOWN=	root
BINMODE=4555
#LDADD+= -lcrypt
#DPADD+= ${LIBCRYPT}

.include <bsd.own.mk>

.if defined(SKEY)
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.if defined(KERBEROS)
CFLAGS+=-DKERBEROS
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
