#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:45 tholo Exp $

LIB=	kadm
SRCS=	kadm_err.c kadm_cli_wrap.c kadm_stream.c kadm_supp.c
CFLAGS+=-I. -I${.CURDIR}
.if exists(${.CURDIR}/../krb/obj)
CFLAGS+=-I${.CURDIR}/../krb/obj
.else
CFLAGS+=-I${.CURDIR}/../krb
.endif
LDADD+=	${COM_ERR}
CLEANFILES+=	kadm_err.c kadm_err.h

kadm_err.c kadm_err.h: kadm_err.et
	test -e kadm_err.et || ln -s ${.CURDIR}/kadm_err.et .
	${COMPILE_ET} kadm_err.et
	-test -h kadm_err.et && rm kadm_err.et

beforeinstall:
	-cd ${.OBJDIR}; cmp -s kadm_err.h ${DESTDIR}/usr/include/kerberosIV/kadm_err.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 kadm_err.h ${DESTDIR}/usr/include/kerberosIV

.include <bsd.lib.mk>
