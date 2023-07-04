# $OpenBSD: Makefile,v 1.1.1.1 2023/07/04 11:27:13 bluhm Exp $

.if ! (make(clean) || make(cleandir) || make(obj))
NCPU !=		/sbin/sysctl -n hw.ncpu
.endif

REGRESS_TARGETS +=	build
build:
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper obj
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper -j ${NCPU} all

REGRESS_TARGETS +=	test
test:
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper test

.include <bsd.regress.mk>
