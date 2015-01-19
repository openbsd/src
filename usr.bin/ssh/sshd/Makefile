#	$OpenBSD: Makefile,v 1.84 2015/01/19 20:30:24 markus Exp $

.PATH:		${.CURDIR}/..
.include "${.CURDIR}/../Makefile.inc"

PROG=	sshd
BINOWN=	root
BINMODE=555
BINDIR=	/usr/sbin
MAN=	sshd.8 sshd_config.5

SRCS=	sshd.c auth-rhosts.c auth-passwd.c \
	sshpty.c sshlogin.c servconf.c serverloop.c \
	auth.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c \
	monitor_mm.c monitor.c monitor_wrap.c \
	sftp-server.c sftp-common.c \
	roaming_common.c roaming_serv.c sandbox-systrace.c

.if (${OPENSSL:L} == "yes")
SRCS+=	auth-rsa.c auth-rh-rsa.c auth1.c
.endif

.include <bsd.own.mk> # for KERBEROS and AFS

KERBEROS5=no

.if (${KERBEROS5:L} == "yes")
CFLAGS+=-DKRB5 -I${DESTDIR}/usr/include/kerberosV -DGSSAPI
SRCS+=  auth-krb5.c auth2-gss.c gss-serv.c gss-serv-krb5.c
.endif

.include <bsd.prog.mk>

.if (${KERBEROS5:L} == "yes")
LDADD+= -lgssapi -lkrb5 -lasn1
LDADD+= -lwind -lroken -lcom_err -lpthread -lheimbase -lkafs
DPADD+= ${LIBGSSAPI} ${LIBKRB5}
.endif

.if (${OPENSSL:L} == "yes")
LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
.endif

LDADD+=	-lz
DPADD+=	${LIBZ}
