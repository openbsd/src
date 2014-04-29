#	$OpenBSD: Makefile,v 1.63 2014/04/29 18:01:49 markus Exp $

.PATH:		${.CURDIR}/..
.include "${.CURDIR}/../Makefile.inc"

PROG=	ssh
BINOWN=	root

#BINMODE?=4555

BINDIR=	/usr/bin
MAN=	ssh.1 ssh_config.5
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect2.c mux.c \
	roaming_common.c roaming_client.c

.include <bsd.own.mk>

.if (${OPENSSL:L} == "yes")
SRCS+=	sshconnect1.c
.endif

KERBEROS5=no

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV -DGSSAPI
.endif # KERBEROS5

.include <bsd.prog.mk>

.if (${KERBEROS5:L} == "yes")
DPADD+=  ${LIBGSSAPI} ${LIBKRB5}
LDADD+=  -lgssapi -lkrb5 -lasn1
LDADD+=  -lwind -lroken -lcom_err -lpthread -lheimbase
.endif # KERBEROS5

.if (${OPENSSL:L} == "yes")
LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
.endif

LDADD+=	-lz
DPADD+=	${LIBZ}
