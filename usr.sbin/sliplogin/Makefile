#	from: @(#)Makefile	5.1 (Berkeley) 5/11/90
#	$Id: Makefile,v 1.3 1997/04/27 20:57:07 millert Exp $

PROG=	sliplogin
MAN=	sliplogin.8
FILES=	slip.hosts slip.login
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>

afterinstall:
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		if [ ! -f ${DESTDIR}/etc/sliphome/$$i ]; then \
			${INSTALL} ${INSTALL_COPY} -o root -g wheel ${.CURDIR}/$$i \
				${DESTDIR}/etc/sliphome/$$i; \
		fi; \
	done
