#	$OpenBSD: Makefile,v 1.81 2020/01/25 23:02:14 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh.c readconf.c clientloop.c sshtty.c sshconnect.c sshconnect2.c mux.c
SRCS+=	authfd.c compat.c dns.c fatal.c hostfile.c readpass.c utf8.c
SRCS+=	${SRCS_BASE} ${SRCS_KEX} ${SRCS_KEXC} ${SRCS_KEY} ${SRCS_KEYP} \
	${SRCS_KRL} ${SRCS_PROT} ${SRCS_PKT} ${SRCS_UTL} ${SRCS_PKCS11} \
	${SRCS_SK_CLIENT}

PROG=	ssh

BINDIR=	/usr/bin
MAN=	ssh.1 ssh_config.5

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
LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
.endif

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.if (${ZLIB:L} == "yes")
LDADD+=	-lz
DPADD+=	${LIBZ}
.endif
