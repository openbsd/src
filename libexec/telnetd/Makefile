#	$OpenBSD: Makefile,v 1.19 2003/05/14 06:47:49 mho Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$NetBSD: Makefile,v 1.6 1996/02/24 01:22:12 jtk Exp $

PROG=	telnetd

CFLAGS+=-DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS
CFLAGS+=-DOLD_ENVIRON -DENV_HACK -I${.CURDIR} -Wall -Werror
CFLAGS+= -DHAS_CGETENT -Wno-unused
SRCS=	authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c mini_inetd.c socket.c \
	auth.c encrypt.c genget.c misc.c
DPADD=	${LIBUTIL} ${LIBCURSES}
LDADD+=	-lutil -lcurses 
MAN=	telnetd.8

.include <bsd.own.mk> # for KERBEROS

.if (${KERBEROS5:L} == "yes")
CFLAGS+=-DENCRYPTION -DAUTHENTICATION -I${.CURDIR}/../../lib
DPADD+=	${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBDES} ${LIBCOM_ERR}
LDADD+=	-lkrb5 -lasn1 -lcrypto -ldes -lcom_err
.endif

.include <bsd.prog.mk>
