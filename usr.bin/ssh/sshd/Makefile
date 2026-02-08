#	$OpenBSD: Makefile,v 1.113 2026/02/08 15:28:01 dtucker Exp $

.PATH:		${.CURDIR}/..

SRCS=	sshd.c servconf.c sshpty.c srclimit.c groupaccess.c auth2-methods.c
SRCS+=	dns.c fatal.c compat.c utf8.c authfd.c canohost.c kex-names.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} \
	${SRCS_MAC} ${SRCS_UTL} ${SRCS_SK_CLIENT} ${SRCS_PKCS11_CLIENT}

PROG=	sshd
BINMODE=511
BINDIR=	/usr/sbin
MAN=	sshd.8 sshd_config.5

.include <bsd.prog.mk>

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
