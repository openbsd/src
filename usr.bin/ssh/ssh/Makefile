#	$OpenBSD: Makefile,v 1.31 2001/06/26 16:15:26 dugsong Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh
BINOWN=	root

BINMODE?=4555

BINDIR=	/usr/bin
MAN=	ssh.1
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c

.include <bsd.own.mk> # for AFS

.if (${KERBEROS:L} == "yes")
CFLAGS+= -DKRB4 -I${DESTDIR}/usr/include/kerberosIV
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}

CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV
LDADD+=  -lkrb5 -lasn1
DPADD+=  ${LIBKRB5} ${LIBASN1}

.if (${AFS:L} == "yes")
CFLAGS+= -DAFS
LDADD+=  -lkafs
DPADD+=  ${LIBKRBAFS}
.endif # AFS
.endif # KERBEROS

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lz
DPADD+=	${LIBCRYPTO} ${LIBZ}
