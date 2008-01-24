# $OpenBSD: Makefile,v 1.5 2008/01/24 18:16:47 stefan Exp $

CC=/usr/local/bin/cc

REGRESS_TARGETS=\
	arith001 \
	const001 \
	darray001 \
	enum001 \
	init001 init004 \
	mustpass0000 mustpass0001 \
	switch003 \
	tmpalloc001 \
	shouldfail

.c:
	@echo ${*}
	${CC} ${.CURDIR}/${*}.c -o ${*}.out && ${.CURDIR}/${*}.out

shouldfail:
	@echo ${*}
	if ${CC} ${.CURDIR}/init002.c; then false; else true; fi
	if ${CC} ${.CURDIR}/init003.c; then false; else true; fi
	if ${CC} ${.CURDIR}/switch001.c; then false; else true; fi
	if ${CC} ${.CURDIR}/switch002.c; then false; else true; fi
	if ${CC} ${.CURDIR}/mustfail0000.c; then false; else true; fi
	
clean:
	rm -f *.out

.include <bsd.regress.mk>
