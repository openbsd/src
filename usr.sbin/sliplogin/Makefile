#	from: @(#)Makefile	5.1 (Berkeley) 5/11/90
#	$Id: Makefile,v 1.2 1996/12/08 22:31:39 tholo Exp $

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
			${INSTALL} ${COPY} -o root -g wheel ${.CURDIR}/$$i \
				${DESTDIR}/etc/sliphome/$$i; \
		fi; \
	done
