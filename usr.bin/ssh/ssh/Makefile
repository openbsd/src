#	$OpenBSD: Makefile,v 1.41 2002/05/15 21:56:39 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh
BINOWN=	root

#BINMODE?=4555

BINDIR=	/usr/bin
MAN=	ssh.1
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c

.include <bsd.own.mk> # for AFS

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV
LDADD+=  -lkrb5 -lasn1 -lcom_err
DPADD+=  ${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR}
.endif # KERBEROS5

.if (${KERBEROS:L} == "yes")
CFLAGS+= -DKRB4 -I${DESTDIR}/usr/include/kerberosIV
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}
.if (${AFS:L} == "yes")
CFLAGS+= -DAFS
LDADD+=  -lkafs
DPADD+=  ${LIBKAFS}
.endif # AFS
.endif # KERBEROS

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lz -ldes
DPADD+=	${LIBCRYPTO} ${LIBZ} ${LIBDES}
