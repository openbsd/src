#	$OpenBSD: Makefile,v 1.2 1997/04/27 20:55:52 millert Exp $

FILES=	call config dial dialcode passwd port sys
NOOBJ=	noobj

install:
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		if [ ! -f ${DESTDIR}/etc/uucp/$$i ]; then \
			${INSTALL} ${INSTALL_COPY} -o root -g wheel -m 644 $$i \
				${DESTDIR}/etc/uucp; \
		fi; \
	done

.include <bsd.prog.mk>
