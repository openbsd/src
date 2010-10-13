#	$OpenBSD: Makefile,v 1.72 2010/10/13 08:14:22 jsg Exp $

.PATH:		${.CURDIR}/..

PROG=	sshd
BINOWN=	root
BINMODE=555
BINDIR=	/usr/sbin
MAN=	sshd.8 sshd_config.5

SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	sshpty.c sshlogin.c servconf.c serverloop.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c auth2-jpake.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c \
	monitor_mm.c monitor.c monitor_wrap.c \
	kexdhs.c kexgexs.c kexecdhs.c sftp-server.c sftp-common.c \
	roaming_common.c roaming_serv.c

.include <bsd.own.mk> # for KERBEROS and AFS

.if (${KERBEROS5:L} == "yes")
CFLAGS+=-DKRB5 -I${DESTDIR}/usr/include/kerberosV -DGSSAPI
SRCS+=  auth-krb5.c auth2-gss.c gss-serv.c gss-serv-krb5.c
.endif

.include <bsd.prog.mk>

.if (${KERBEROS5:L} == "yes")
LDADD+= -lgssapi -lkrb5 -lkafs
DPADD+= ${LIBGSSAPI} ${LIBKRB5}
.endif

DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
LDADD+=	-lcrypto -lutil -lz

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+= -DLIBWRAP
DPADD+= ${LIBWRAP}
LDADD+= -lwrap
.endif
