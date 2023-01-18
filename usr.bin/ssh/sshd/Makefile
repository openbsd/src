#	$OpenBSD: Makefile,v 1.108 2023/01/18 20:56:36 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	sshd.c auth-rhosts.c auth-passwd.c sshpty.c sshlogin.c servconf.c \
	serverloop.c auth.c auth2.c auth-options.c session.c auth2-chall.c \
	groupaccess.c auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c auth2-pubkeyfile.c \
	monitor.c monitor_wrap.c \
	sftp-server.c sftp-common.c sftp-realpath.c sandbox-pledge.c srclimit.c
SRCS+=	authfd.c compat.c dns.c fatal.c hostfile.c readpass.c utf8.c uidswap.c
SRCS+=	${SRCS_BASE} ${SRCS_KEX} ${SRCS_KEXS} ${SRCS_KEY} ${SRCS_KEYP} \
	${SRCS_KRL} ${SRCS_PROT} ${SRCS_PKT} ${SRCS_UTL} ${SRCS_PKCS11} \
	${SRCS_SK_CLIENT}

PROG=	sshd
BINMODE=511
BINDIR=	/usr/sbin
MAN=	sshd.8 sshd_config.5

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
LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
.endif

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.if (${ZLIB:L} == "yes")
LDADD+=	-lz
DPADD+=	${LIBZ}
.endif

# The random relink kit, used on OpenBSD by /etc/rc

Makefile.relink: ${.CURDIR}/../Makefile.inc ${.CURDIR}/Makefile
	# XXX assume a concatenation of these is OK
	cat ${.CURDIR}/../Makefile.inc ${.CURDIR}/Makefile > Makefile.relink

sshd.tar: ${OBJS} Makefile.relink
	tar cf $@ ${OBJS} Makefile.relink

afterinstall: sshd.tar
	install -d -o root -g wheel -m 755 \
	    ${DESTDIR}/usr/share/relink/usr/sbin/sshd
	install -o ${BINOWN} -g ${BINGRP} -m 640 \
	    sshd.tar ${DESTDIR}/usr/share/relink/usr/sbin/sshd/sshd.tar

relink:
	cc -o sshd `echo ${OBJS} | tr ' ' '\n' | sort -R` ${LDADD}
	./sshd -V && install -o root -g wheel -m ${BINMODE} sshd /usr/sbin/sshd
