#	$OpenBSD: Makefile,v 1.5 1998/05/15 02:22:43 art Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$NetBSD: Makefile,v 1.6 1996/02/24 01:22:12 jtk Exp $

PROG=	telnetd

CFLAGS+=-DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS
CFLAGS+=-DOLD_ENVIRON -DENV_HACK -I${.CURDIR} -Wall -Wno-unused
SRCS=	authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c
DPADD=	${LIBUTIL} ${LIBTERM} ${LIBTELNET}
LDADD+=	-lutil -ltermcap -ltelnet
MAN=	telnetd.8

.include <bsd.own.mk> # for KERBEROS

.if (${KERBEROS} == "yes")
CFLAGS+=-DENCRYPTION -DAUTHENTICATION -DKRB4 -I${.CURDIR}/../../lib
LDADD+= -lkrb -ldes
DPADD+= ${LIBDES} ${LIBKRB}
.endif

.include <bsd.prog.mk>
