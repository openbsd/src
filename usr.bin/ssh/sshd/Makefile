#	$OpenBSD: Makefile,v 1.39 2001/05/18 14:13:29 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	sshd
BINOWN=	root
BINMODE=555
BINDIR=	/usr/sbin
MAN=	sshd.8
CFLAGS+=-DHAVE_LOGIN_CAP

SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	sshpty.c sshlogin.c servconf.c serverloop.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-skey.c auth-bsdauth.c

.include <bsd.own.mk> # for KERBEROS and AFS

.if (${KERBEROS:L} == "yes")
.if (${AFS:L} == "yes")
CFLAGS+= -DAFS
LDADD+=  -lkafs
DPADD+=  ${LIBKRBAFS}
.endif # AFS
CFLAGS+= -DKRB4 -I${DESTDIR}/usr/include/kerberosIV
SRCS+=	auth-krb4.c
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}
.endif # KERBEROS

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+= -DLIBWRAP
LDADD+= -lwrap
DPADD+= ${LIBWRAP}
.endif

.if (${SKEY:L} == "yes")
CFLAGS+= -DSKEY
LDADD+= -lskey
DPADD+= ${SKEY}
.endif
