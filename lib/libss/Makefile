#	$OpenBSD: Makefile,v 1.7 1997/07/07 08:42:39 grr Exp $

LIB=	ss
HDRS=	ss.h ${.OBJDIR}/ss_err.h
SRCS=	ss_err.c data.c error.c execute_cmd.c help.c invocation.c list_rqs.c \
	listen.c pager.c parse.c prompt.c request_tbl.c requests.c \
	std_rqs.c
CFLAGS+=-I. -I${.CURDIR}/.. -I${.CURDIR} -I${.CURDIR}/../com_err
LDADD+=	${COM_ERR}
CLEANFILES+=	ss_err.c ss_err.h std_rqs.c

ss_err.h ss_err.c:	${.CURDIR}/ss_err.et
	test -e ss_err.et || ln -s ${.CURDIR}/ss_err.et .
	compile_et ss_err.et
	-test -h ss_err.et && rm ss_err.et

std_rqs.c:		${.CURDIR}/std_rqs.ct
	test -e std_rqs.ct || ln -s ${.CURDIR}/std_rqs.ct .
	mk_cmds std_rqs.ct
	-test -h std_rqs.ct && rm std_rqs.ct

includes: ss_err.h
	@${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		${DESTDIR}/usr/include/ss; \
	cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/ss/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include/ss"; \
	    echo $$j; \
	    eval "$$j"; \
	done

beforedepend:
	test -h ss || ln -s . ss

cleandir: _SUBDIRUSE clean
	rm -f ss

.include <bsd.lib.mk>
