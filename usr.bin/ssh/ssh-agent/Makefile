#	$OpenBSD: Makefile,v 1.40 2024/04/04 16:00:51 semarie Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-agent.c ${SRCS_PKCS11_CLIENT}
SRCS+=	compat.c fatal.c readpass.c utf8.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_UTL}
SRCS+=	${SRCS_SK_CLIENT}

PROG=	ssh-agent
BINOWN=	root
BINGRP=	_sshagnt
BINMODE=2511

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

# The random relink kit, used on OpenBSD by /etc/rc

CLEANFILES+= ssh-agent.tar install.sh

install.sh: Makefile
	echo "set -o errexit" > $@
	echo "${CC} ${LDFLAGS} ${LDSTATIC} -o ${PROG}" \
	    "\`echo " ${OBJS} "| tr ' ' '\\\n' | sort -R\`" ${LDADD} >> $@
	echo "./${PROG} /usr/bin/true" >> $@
	echo "install -c -s -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ${PROG} " \
	    "${BINDIR}/${PROG}" >> $@

${PROG}.tar: ${OBJS} install.sh
	tar cf $@ ${OBJS} install.sh

afterinstall: ${PROG}.tar
	install -d -o root -g wheel -m 755 \
	    ${DESTDIR}/usr/share/relink/${BINDIR}/${PROG}
	install -o root -g bin -m 640 \
	    ${PROG}.tar ${DESTDIR}/usr/share/relink/${BINDIR}/${PROG}/${PROG}.tar
