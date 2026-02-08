#	$OpenBSD: Makefile,v 1.82 2026/02/08 15:28:01 dtucker Exp $

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

# Enabling this requires the 'heimdal' package from ports.
KERBEROS5=no

.if (${KERBEROS5:L} == "yes")
CFLAGS+=-DKRB5 -DGSSAPI -I${DESTDIR}/usr/local/heimdal/include
SRCS+=  gss-genr.c
.endif # KERBEROS5

.include <bsd.prog.mk>

.if (${KERBEROS5:L} == "yes")
DPADD+=  ${LIBGSSAPI} ${LIBKRB5}
LDADD+=  -L/usr/local/heimdal/lib -L/usr/local/lib
LDADD+=  -lgssapi -lkrb5 -lasn1
LDADD+=  -lwind -lroken -lcom_err -lpthread -lheimbase -lgssapi
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
