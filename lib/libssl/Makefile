.include <bsd.own.mk>

ECHO= /bin/echo

.if exists(${.OBJDIR}/src-patent)
SUBDIR= crypto-patent ssl-patent
.else
SUBDIR= crypto ssl
.endif

distribution:
	@echo "Installing ${DESTDIR}/etc/ssl/lib/ssleay.cnf"; \
	${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
	   ${.CURDIR}/ssleay.cnf ${DESTDIR}/etc/ssl/lib/ssleay.cnf;

.include <bsd.subdir.mk>

