#	$Id: Makefile,v 1.5 1996/08/31 18:47:39 deraadt Exp $

LIB=	ss
SRCS=	ss_err.c data.c error.c execute_cmd.c help.c invocation.c list_rqs.c \
	listen.c pager.c parse.c prompt.c request_tbl.c requests.c \
	std_rqs.c
CFLAGS+=-I. -I${.CURDIR}/.. -I${.CURDIR} -I${.CURDIR}/../com_err
LDADD+=	${COM_ERR}
CLEANFILES+=	ss_err.c ss_err.h std_rqs.c ss

ss_err.h ss_err.c:	${.CURDIR}/ss_err.et
	test -e ss_err.et || ln -s ${.CURDIR}/ss_err.et .
	cd ${.CURDIR}/../compile_et; ${MAKE}
	${COMPILE_ET} ss_err.et
	-test -h ss_err.et && rm ss_err.et

std_rqs.c:		${.CURDIR}/std_rqs.ct
	test -e std_rqs.ct || ln -s ${.CURDIR}/std_rqs.ct .
	${MK_CMDS} std_rqs.ct
	-test -h std_rqs.ct && rm std_rqs.ct

includes: ss_err.h
	-cd ${.OBJDIR}; \
	    if [ -f ss_err.h ]; then \
		cmp -s ss_err.h ${DESTDIR}/usr/include/ss/ss_err.h || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 ss_err.h \
		${DESTDIR}/usr/include/ss; \
	    else \
		true; \
	    fi

beforedepend:
	test -h ss || ln -s . ss

.include <bsd.lib.mk>
