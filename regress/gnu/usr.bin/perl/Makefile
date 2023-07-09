# $OpenBSD: Makefile,v 1.5 2023/07/09 13:30:03 bluhm Exp $

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
	# Permissions in CVS src tree are not as in the Perl distribution.
	# Match expectations of cpan/Test-Harness/t/source_handler.t for
	# cpan/Test-Harness/t/source_tests/source*.sh executalbe bits.
	# Remove symlink and fix permissions of copy in obj directory.
	# cpan/autodie/t/chmod.t and cpan/autodie/t/chown.t modify themself,
	# and cpan/autodie/t/utime.t touches touch_me.  Replace symlink.
.for f in \
    cpan/Test-Harness/t/source_tests/source.sh \
    cpan/Test-Harness/t/source_tests/source_args.sh \
    cpan/autodie/t/chmod.t \
    cpan/autodie/t/chown.t \
    cpan/autodie/t/touch_me
	rm -f ${BSDOBJDIR}/gnu/usr.bin/perl/$f
	cp ${BSDSRCDIR}/gnu/usr.bin/perl/$f ${BSDOBJDIR}/gnu/usr.bin/perl/$f
	chmod +x ${BSDOBJDIR}/gnu/usr.bin/perl/$f
.endfor

REGRESS_TARGETS +=	test
test:
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper test

.include <bsd.regress.mk>
