# $OpenBSD: Makefile,v 1.2 2023/07/04 19:01:46 anton Exp $

.if ! (make(clean) || make(cleandir) || make(obj))
NCPU !=		/sbin/sysctl -n hw.ncpuonline
.endif

REGRESS_TARGETS +=	build
build:
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper obj
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper -j ${NCPU} all

REGRESS_TARGETS +=	test
test:
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper test

.include <bsd.regress.mk>
