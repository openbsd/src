#	$OpenBSD: Makefile,v 1.3 2024/05/17 14:42:00 naddy Exp $

.PATH:		${.CURDIR}/..

SRCS=	sshd-session.c auth2-methods.c \
	auth-rhosts.c auth-passwd.c sshpty.c sshlogin.c servconf.c \
	serverloop.c auth.c auth2.c auth-options.c session.c auth2-chall.c \
	groupaccess.c auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c auth2-pubkeyfile.c \
	monitor.c monitor_wrap.c \
	sftp-server.c sftp-common.c sftp-realpath.c sandbox-pledge.c srclimit.c
SRCS+=	authfd.c compat.c dns.c fatal.c hostfile.c readpass.c utf8.c uidswap.c
SRCS+=	${SRCS_BASE} ${SRCS_KEX} ${SRCS_KEXS} ${SRCS_KEY} ${SRCS_KEYP} \
	${SRCS_KRL} ${SRCS_PROT} ${SRCS_PKT} ${SRCS_UTL} ${SRCS_PKCS11} \
	${SRCS_SK_CLIENT}

PROG=	sshd-session
BINDIR=	/usr/libexec
BINMODE=511
NOMAN=	1

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

CLEANFILES+= ${PROG}.tar install.sh

install.sh: Makefile
	echo "set -o errexit" > $@
	echo "${CC} ${LDFLAGS} ${LDSTATIC} -o ${PROG}" \
	    "\`echo " ${OBJS} "| tr ' ' '\\\n' | sort -R\`" ${LDADD} >> $@
	echo "./${PROG} -V # test it works" >> $@
	echo "install -c -s  -o root -g bin -m ${BINMODE} ${PROG} " \
	    "${BINDIR}/${PROG}" >> $@

${PROG}.tar: ${OBJS} install.sh
	tar cf $@ ${OBJS} install.sh

afterinstall: ${PROG}.tar
	install -d -o root -g wheel -m 755 \
	    ${DESTDIR}/usr/share/relink/${BINDIR}/${PROG}
	install -o ${BINOWN} -g ${BINGRP} -m 640 \
	    ${PROG}.tar ${DESTDIR}/usr/share/relink/${BINDIR}/${PROG}/${PROG}.tar
