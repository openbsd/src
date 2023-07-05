# $OpenBSD: Makefile,v 1.3 2023/07/05 21:38:22 bluhm Exp $

.if ! (make(clean) || make(cleandir) || make(obj))
NCPU !=		/sbin/sysctl -n hw.ncpuonline
.endif

REGRESS_SETUP_ONCE +=	build
build:
	# Makefile and Perl tree must exist in obj directory for testing.
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper obj
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper -j ${NCPU} all

REGRESS_SETUP += permissions
permissions:
	# The permissions in CVS tree are not as in the Perl distribution.
	# Match expectations of t/porting/exec-bit.t and
	# cpan/Test-Harness/t/source_tests/source.sh
	awk '{print $$1}' ${BSDSRCDIR}/gnu/usr.bin/perl/MANIFEST |\
	    ( cd ${BSDOBJDIR}/gnu/usr.bin/perl/ && xargs chmod -x )
	grep -v '^#' ${BSDSRCDIR}/gnu/usr.bin/perl/Porting/exec-bit.txt |\
	    ( cd ${BSDOBJDIR}/gnu/usr.bin/perl/ && xargs chmod +x )

REGRESS_TARGETS +=	test
test:
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper test

.include <bsd.regress.mk>
