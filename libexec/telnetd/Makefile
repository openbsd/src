#	$OpenBSD: Makefile,v 1.17 2002/03/05 00:09:32 deraadt Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$NetBSD: Makefile,v 1.6 1996/02/24 01:22:12 jtk Exp $

PROG=	telnetd

CFLAGS+=-DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS
CFLAGS+=-DOLD_ENVIRON -DENV_HACK -I${.CURDIR} -Wall -Werror
SRCS=	authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c mini_inetd.c socket.c
DPADD=	${LIBUTIL} ${LIBCURSES} ${LIBTELNET}
LDADD+=	-lutil -lcurses -ltelnet
MAN=	telnetd.8

.include <bsd.own.mk> # for KERBEROS

.if (${KERBEROS5:L} == "yes")
LDADD+=	-lkrb5 -lasn1 -lcrypto -lcom_err -lkafs
.endif

.if (${KERBEROS:L} == "yes")
CFLAGS+=-DENCRYPTION -DAUTHENTICATION -DKRB4 -I${.CURDIR}/../../lib
LDADD+= -lkrb -lcrypto -ldes
DPADD+= ${LIBDES} ${LIBKRB}
.endif

.include <bsd.prog.mk>
