#	$OpenBSD: Makefile,v 1.67 2017/04/30 23:17:37 djm Exp $

.PATH:		${.CURDIR}/..
.include "${.CURDIR}/../Makefile.inc"

PROG=	ssh
BINOWN=	root

#BINMODE?=4555

BINDIR=	/usr/bin
MAN=	ssh.1 ssh_config.5

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect2.c mux.c

.include <bsd.own.mk>

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
