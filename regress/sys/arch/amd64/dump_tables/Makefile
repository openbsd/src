# $OpenBSD: Makefile,v 1.1 2019/04/23 02:32:17 guenther Exp $

PROG=	dump_tables
LDADD=	-lkvm
DPADD=	${LIBKVM}
CFLAGS+=-I.

REGRESS_TARGETS=

PMAP_H=	/usr/include/machine/pmap.h
SPMAP_H=struct_pmap.h

dump_tables.o: ${SPMAP_H}

s=[[:space:]]
${SPMAP_H}: ${PMAP_H}
	sed -nE -e '/^struct$s+pmap$s*\{/,/^}/p' \
		-e '/^$s*#$s*define$s+PMAP_DIRECT_(UN)?MAP\(/p' \
	    ${PMAP_H} >$@

CLEANFILES+=${SPMAP_H}

.include <bsd.regress.mk>
