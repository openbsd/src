#	$OpenBSD: Makefile,v 1.44 2003/04/10 00:17:52 pvalchev Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh
BINOWN=	root

#BINMODE?=4555

BINDIR=	/usr/bin
MAN=	ssh.1 ssh_config.5
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c

.include <bsd.own.mk> # for AFS

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV
LDADD+=  -lkrb5 -lkafs -lkrb -lasn1 -lcom_err
DPADD+=  ${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR}
.endif # KERBEROS5

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lz -ldes
DPADD+=	${LIBCRYPTO} ${LIBZ} ${LIBDES}
