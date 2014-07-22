#	$OpenBSD: Makefile,v 1.8 2014/07/22 17:05:45 deraadt Exp $

PROG=	sliplogin
MAN=	sliplogin.8
MLINKS=	sliplogin.8 slip.hosts.5 \
	sliplogin.8 slip.login.5
FILES=	slip.hosts slip.login
BINOWN=	root
BINGRP=	network
BINMODE=4550

.include <bsd.prog.mk>

distribution:
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		if [ ! -f ${DESTDIR}/etc/sliphome/$$i ]; then \
			${INSTALL} ${INSTALL_COPY} -o root -g wheel ${.CURDIR}/$$i \
				${DESTDIR}/etc/sliphome/$$i; \
		fi; \
	done
