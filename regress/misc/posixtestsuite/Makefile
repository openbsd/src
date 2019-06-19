#	$OpenBSD: Makefile,v 1.1.1.1 2018/05/03 17:11:54 bluhm Exp $

# The following port must be installed:
#
# posixtestsuite	open POSIX test suite

# Run the conformance tests of the open POSIX test suite.  The
# test suite http://posixtest.sourceforge.net/ is used as a package
# and run as regress.

.if ! (make(clean) || make(cleandir) || make(obj))
.if ! exists(/usr/local/bin/posixtestsuite)
regress:
	@echo Install misc/posixtestsuite port for additional tests.
	@echo SKIPPED
.endif
.endif

regress:
	/usr/local/bin/posixtestsuite

clean:
	rm -rf logfile conformance functional

.include <bsd.regress.mk>
