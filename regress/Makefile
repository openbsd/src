#	$OpenBSD: Makefile,v 1.11 2002/02/18 10:16:30 art Exp $

#
# To run full regression tests you just need two steps:
# 1. make depend
# 2. make regress
#
# A normal "make" is not necessary and will simply do the same thing
# as "regress" (possibly with more bugs). This allows us to include
# compilation as a part of the regression test.
#
# To get a log of test results set the REGRESSLOG make variable to
# a log file, use an absolute path.
# To skip the really slow tests, set the REGRESSSKIPSLOW variable.

SUBDIR+= etc bin include lib libexec sbin sys share usr.bin

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
