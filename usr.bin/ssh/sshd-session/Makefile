#	$OpenBSD: Makefile,v 1.7 2026/05/27 13:57:26 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	sshd-session.c auth2-methods.c \
	auth-rhosts.c auth-passwd.c sshpty.c sshlogin.c servconf.c \
	serverloop.c auth.c auth2.c auth-options.c session.c auth2-chall.c \
	groupaccess.c auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c auth2-pubkeyfile.c \
	monitor.c monitor_wrap.c \
	sftp-server.c sftp-common.c sftp-realpath.c srclimit.c
SRCS+=	authfd.c compat.c dns.c fatal.c hostfile.c readpass.c utf8.c uidswap.c
SRCS+=	misc-agent.c
SRCS+=	${SRCS_BASE} ${SRCS_KEX} ${SRCS_KEXS} ${SRCS_KEY} ${SRCS_KEYP} \
	${SRCS_KRL} ${SRCS_PROT} ${SRCS_PKT} ${SRCS_UTL} ${SRCS_PKCS11} \
	${SRCS_SK_CLIENT}

PROG=	sshd-session
RELINK=./${PROG} -V # test it works
BINDIR=	/usr/libexec
BINMODE=511
NOMAN=	1

.include <bsd.own.mk> # for KERBEROS

# Enabling this requires the 'heimdal' package from ports.
KERBEROS5=no

.if (${KERBEROS5:L} == "yes")
CFLAGS+=-DKRB5 -DGSSAPI
CFLAGS+=-I${DESTDIR}/usr/local/heimdal/include -I${DESTDIR}/usr/local/include
SRCS+=  auth-krb5.c auth2-gss.c gss-genr.c gss-serv.c gss-serv-krb5.c
.endif

.include <bsd.prog.mk>

.if (${KERBEROS5:L} == "yes")
LDADD+= -L${DESTDIR}/usr/local/heimdal/lib -L${DESTDIR}/usr/local/lib
LDADD+= -lgssapi -lkrb5 -lasn1
LDADD+= -lwind -lroken -lcom_err -lpthread -lheimbase
DPADD+= ${LIBGSSAPI} ${LIBKRB5}
.endif

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
