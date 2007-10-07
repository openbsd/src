# $OpenBSD: Makefile,v 1.1 2007/10/07 18:12:43 otto Exp $

CC=/usr/local/bin/cc

REGRESS_TARGETS=const001 \
	darray001 \
	enum001 \
	init001 init004 \
	tmpalloc001 \
	shouldfail

.c:
	@echo ${*}
	${CC} ${.CURDIR}/${*}.c && ./a.out

shouldfail:
	@echo ${*}
	if ${CC} ${.CURDIR}/init002.c; then false; else true; fi
	if ${CC} ${.CURDIR}/init003.c; then false; else true; fi
	
clean:
	rm -f a.out

.include <bsd.regress.mk>
