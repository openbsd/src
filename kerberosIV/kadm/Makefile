#	$OpenBSD: Makefile,v 1.9 1999/03/09 00:01:42 niklas Exp $

.include <bsd.obj.mk>

LIB=	kadm
SRCS=	kadm_err.c kadm_cli_wrap.c kadm_stream.c kadm_supp.c check_password.c
CFLAGS+=-I. -I${.CURDIR}
.if exists(${.CURDIR}/../krb/${__objdir})
CFLAGS+=-I${.CURDIR}/../krb/${__objdir}
.else
CFLAGS+=-I${.CURDIR}/../krb
.endif
LDADD+=	${COM_ERR}
CLEANFILES+=	kadm_err.c kadm_err.h

kadm_err.c kadm_err.h: kadm_err.et
	test -e kadm_err.et || ln -s ${.CURDIR}/kadm_err.et .
	compile_et kadm_err.et
	-test -h kadm_err.et && rm kadm_err.et

beforedepend prereq: kadm_err.h

includes: kadm_err.h
	-cd ${.OBJDIR}; cmp -s kadm_err.h \
	    ${DESTDIR}/usr/include/kerberosIV/kadm_err.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 kadm_err.h \
	    ${DESTDIR}/usr/include/kerberosIV

.include <bsd.lib.mk>
