#	$OpenBSD: Makefile,v 1.8 2000/09/03 18:41:16 espie Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$NetBSD: Makefile,v 1.6 1996/02/24 01:22:12 jtk Exp $

PROG=	telnetd

CFLAGS+=-DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS
CFLAGS+=-DOLD_ENVIRON -DENV_HACK -I${.CURDIR} -Wall -Wno-unused
CFLAGS+=-D_USE_OLD_CURSES_
SRCS=	authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c
DPADD=	${LIBUTIL} ${LIBOLDCURSES} ${LIBTELNET}
LDADD+=	-lutil -locurses -ltelnet
MAN=	telnetd.8

.include <bsd.own.mk> # for KERBEROS

.if (${KERBEROS:L} == "yes")
CFLAGS+=-DENCRYPTION -DAUTHENTICATION -DKRB4 -I${.CURDIR}/../../lib
LDADD+= -lkrb -ldes
DPADD+= ${LIBDES} ${LIBKRB}
.endif

.include <bsd.prog.mk>
