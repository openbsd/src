#	$OpenBSD: Makefile,v 1.60 2003/08/22 13:22:27 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	sshd
BINOWN=	root
BINMODE=555
BINDIR=	/usr/sbin
MAN=	sshd.8 sshd_config.5
CFLAGS+=-DHAVE_LOGIN_CAP -DBSD_AUTH

SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	sshpty.c sshlogin.c servconf.c serverloop.c uidswap.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-skey.c auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c \
	monitor_mm.c monitor.c monitor_wrap.c monitor_fdpass.c \
	kexdhs.c kexgexs.c

.include <bsd.own.mk> # for KERBEROS and AFS

.if (${KERBEROS5:L} == "yes")
CFLAGS+=-DKRB5 -I${DESTDIR}/usr/include/kerberosV
SRCS+=  auth-krb5.c

LDADD+= -lkrb5 
DPADD+= ${LIBKRB5}

SRCS+= auth2-gss.c gss-serv.c gss-serv-krb5.c
CFLAGS+= -DGSSAPI
LDADD+= -lgssapi
DPADD+= ${LIBGSSAPI}
.endif # KERBEROS5

.include <bsd.prog.mk>

DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ} ${LIBDES}
LDADD+=	-lcrypto -lutil -lz -ldes

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+= -DLIBWRAP
DPADD+= ${LIBWRAP}
LDADD+= -lwrap
.endif

#.if (${SKEY:L} == "yes")
#CFLAGS+= -DSKEY
#DPADD+= ${SKEY}
#LDADD+= -lskey
#.endif
