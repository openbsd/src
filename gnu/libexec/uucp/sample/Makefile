#	$OpenBSD: Makefile,v 1.1 1996/12/08 22:39:32 tholo Exp $

FILES=	call config dial dialcode passwd port sys
NOOBJ=	noobj

install:
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		if [ ! -f ${DESTDIR}/etc/uucp/$$i ]; then \
			${INSTALL} ${COPY} -o root -g wheel -m 644 $$i \
				${DESTDIR}/etc/uucp; \
		fi; \
	done

.include <bsd.prog.mk>
